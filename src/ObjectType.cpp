#include "ObjectType.h"
#include <sstream>
#include <llvm/IR/DerivedTypes.h>   // 添加这行，提供StructType, PointerType等定义
#include <llvm/IR/Type.h>           // 添加这行，提供Type类的完整定义
#include <llvm/IR/LLVMContext.h>    // 添加这行，确保LLVMContext定义完整
#include "TypeIDs.h"    
#include "TypeOperations.h"

namespace llvmpy {

// 静态成员初始化
std::unordered_map<std::string, std::unordered_map<std::string, bool>> 
    ObjectType::typeFeatures;

// ObjectType实现
ObjectType::ObjectType(const std::string& name, TypeCategory category) 
    : name(name), category(category) {}

bool ObjectType::canAssignTo(const ObjectType* other) const {
    // 相同类型可以直接赋值
    if (getTypeSignature() == other->getTypeSignature()) {
        return true;
    }
    
    // 是否可以转换决定是否可以赋值
    return canConvertTo(other);
}

bool ObjectType::canConvertTo(const ObjectType* other) const {
    // 相同类型可以直接转换
    if (getTypeSignature() == other->getTypeSignature()) {
        return true;
    }
    
    // 特殊处理 any 类型 - any 可以转换为任何类型，任何类型也可以转换为 any
    if (getName() == "any" || other->getName() == "any" || 
        getTypeId() == PY_TYPE_ANY || other->getTypeId() == PY_TYPE_ANY) {
        return true;
    }
    
    // 使用 TypeOperationRegistry 检查更复杂的类型兼容性
    auto& registry = TypeOperationRegistry::getInstance();
    if (registry.areTypesCompatible(getTypeId(), other->getTypeId())) {
        return true;
    }
    
    return false;  // 默认情况下不能转换
}

std::string ObjectType::getTypeSignature() const {
    return getName();
}

bool ObjectType::hasFeature(const std::string& feature) const {
    auto typeIt = typeFeatures.find(name);

    if (typeIt != typeFeatures.end()) {
        auto featureIt = typeIt->second.find(feature);
        if (featureIt != typeIt->second.end()) {
            return featureIt->second;
        }
    }
    return false;
}

void ObjectType::registerFeature(const std::string& typeName, 
                                const std::string& feature,
                                bool value) {
    typeFeatures[typeName][feature] = value;
}

// PrimitiveType实现
PrimitiveType::PrimitiveType(const std::string& name) 
    : ObjectType(name, Primitive) {}

    llvm::Type* PrimitiveType::getLLVMType(llvm::LLVMContext& context) const {
        if (name == "int") {
            return llvm::Type::getInt32Ty(context);
        } else if (name == "double") {
            return llvm::Type::getDoubleTy(context);
        } else if (name == "bool") {
            return llvm::Type::getInt1Ty(context);
        } else if (name == "void") {
            return llvm::Type::getVoidTy(context);
        } else if (name == "string") {
            return llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context));
        } else if (name == "any" || name == "object") {
            // any类型用指针表示，可以容纳任意类型的值
            return llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context));
        }
        
        // 默认返回指针类型，确保安全
        return llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context));
    }

    int PrimitiveType::getTypeId() const {
        if (name == "int") return PY_TYPE_INT;       // 1
        if (name == "double") return PY_TYPE_DOUBLE; // 2
        if (name == "bool") return PY_TYPE_BOOL;     // 3
        if (name == "string") return PY_TYPE_STRING; // 4
        if (name == "void") return PY_TYPE_NONE;     // 0
        if (name == "any") return PY_TYPE_ANY;       // 7 - 修复这里
        if (name == "object") return PY_TYPE_ANY;    // 也使用7，确保一致性
        return PY_TYPE_NONE; // 默认返回 NONE 而不是-1
    }

// ListType实现
llvm::Type* ListType::getLLVMType(llvm::LLVMContext& context) const {
    // 检查是否已经创建过此类型
    std::string typeName = getName();
    if (llvm::StructType::getTypeByName(context, typeName)) {
        return llvm::PointerType::getUnqual(llvm::StructType::getTypeByName(context, typeName));
    }

    // 创建PyListObject结构类型的指针
    llvm::StructType* listType = llvm::StructType::create(context, typeName);
    
    // PyListObject结构: {PyObject, length, capacity, elemType*, data}
    llvm::StructType* pyObjectType = llvm::StructType::getTypeByName(context, "PyObject");
    if (!pyObjectType) {
        pyObjectType = llvm::StructType::create(context, "PyObject");
        std::vector<llvm::Type*> pyObjectFields = {
            llvm::Type::getInt32Ty(context),   // refcount
            llvm::Type::getInt32Ty(context)    // typeId
        };
        pyObjectType->setBody(pyObjectFields);
    }
    
    std::vector<llvm::Type*> fields = {
        pyObjectType,                       // PyObject头
        llvm::Type::getInt32Ty(context),    // length
        llvm::Type::getInt32Ty(context),    // capacity
        llvm::Type::getInt32Ty(context),    // elemTypeId
        llvm::PointerType::getUnqual(
            llvm::PointerType::getUnqual(pyObjectType)) // data - PyObject**
    };
    
    listType->setBody(fields);
    return llvm::PointerType::getUnqual(listType);
}

int ListType::getTypeId() const {
    // 确保始终返回PY_TYPE_LIST，而不是基于元素类型计算
    return PY_TYPE_LIST; // 5
}

std::string ListType::getTypeSignature() const {
    return "list<" + elementType->getTypeSignature() + ">";
}

// DictType实现
DictType::DictType(ObjectType* keyType, ObjectType* valueType)
    : ObjectType("dict<" + keyType->getName() + "," + valueType->getName() + ">", 
                Container),
      keyType(keyType), valueType(valueType) {}

      llvm::Type* DictType::getLLVMType(llvm::LLVMContext& context) const {
        // 检查是否已经创建过此类型
        std::string typeName = getName();
        if (llvm::StructType::getTypeByName(context, typeName)) {
            return llvm::PointerType::getUnqual(llvm::StructType::getTypeByName(context, typeName));
        }
        
        // 创建PyDictObject结构类型的指针
        llvm::StructType* dictType = llvm::StructType::create(context, typeName);
        
        // PyDictObject结构: {PyObject, size, capacity, keyTypeId, entries*}
        llvm::StructType* pyObjectType = llvm::StructType::getTypeByName(context, "PyObject");
        if (!pyObjectType) {
            pyObjectType = llvm::StructType::create(context, "PyObject");
            std::vector<llvm::Type*> pyObjectFields = {
                llvm::Type::getInt32Ty(context),   // refcount
                llvm::Type::getInt32Ty(context)    // typeId
            };
            pyObjectType->setBody(pyObjectFields);
        }
        
        std::vector<llvm::Type*> fields = {
            pyObjectType,                       // PyObject头
            llvm::Type::getInt32Ty(context),    // size
            llvm::Type::getInt32Ty(context),    // capacity
            llvm::Type::getInt32Ty(context),    // keyTypeId
            llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context))  // entries - 不透明指针
        };
        
        dictType->setBody(fields);
        return llvm::PointerType::getUnqual(dictType);
    }

int DictType::getTypeId() const {
    return PY_TYPE_DICT; // 6
}

std::string DictType::getTypeSignature() const {
    return "dict<" + keyType->getTypeSignature() + "," + 
           valueType->getTypeSignature() + ">";
}

// FunctionType实现
FunctionType::FunctionType(ObjectType* returnType, 
                         std::vector<ObjectType*> paramTypes)
    : ObjectType("func", Function), 
      returnType(returnType), paramTypes(paramTypes) {}

      llvm::Type* FunctionType::getLLVMType(llvm::LLVMContext& context) const {
        std::vector<llvm::Type*> llvmParamTypes;
        for (auto paramType : paramTypes) {
            llvmParamTypes.push_back(paramType->getLLVMType(context));
        }
        
        llvm::Type* llvmReturnType = returnType->getLLVMType(context);
        
        return llvm::PointerType::getUnqual(
            llvm::FunctionType::get(llvmReturnType, llvmParamTypes, false));
    }

int FunctionType::getTypeId() const {
    return  PY_TYPE_FUNC; // 8
}

std::string FunctionType::getTypeSignature() const {
    std::stringstream ss;
    ss << "func<" << returnType->getTypeSignature() << "(";
    
    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (i > 0) ss << ",";
        ss << paramTypes[i]->getTypeSignature();
    }
    
    ss << ")>";
    return ss.str();
}

// TypeRegistry实现
TypeRegistry& TypeRegistry::getInstance() {
    static TypeRegistry instance;
    return instance;
}

TypeRegistry::TypeRegistry() {
    registerBuiltinTypes();
}

TypeRegistry::~TypeRegistry() {
    // 清理所有注册的类型
    for (auto& pair : namedTypes) {
        delete pair.second;
    }
}

void TypeRegistry::registerBuiltinTypes() {
    // 注册原始类型
    registerType("int", new PrimitiveType("int"));
    registerType("double", new PrimitiveType("double"));
    registerType("bool", new PrimitiveType("bool"));
    registerType("void", new PrimitiveType("void"));
    registerType("string", new PrimitiveType("string"));
    
    // 注册通用类型 - 用于类型推断失败时的默认类型
    registerType("any", new PrimitiveType("any"));
    registerType("object", new PrimitiveType("object"));
    
    // 注册类型创建器
    registerTypeCreator<PrimitiveType>("int", 
        [](const std::string& name) { return new PrimitiveType(name); });
    registerTypeCreator<PrimitiveType>("double", 
        [](const std::string& name) { return new PrimitiveType(name); });
    registerTypeCreator<PrimitiveType>("bool", 
        [](const std::string& name) { return new PrimitiveType(name); });
    registerTypeCreator<PrimitiveType>("void", 
        [](const std::string& name) { return new PrimitiveType(name); });
    registerTypeCreator<PrimitiveType>("string", 
        [](const std::string& name) { return new PrimitiveType(name); });
    registerTypeCreator<PrimitiveType>("any", 
        [](const std::string& name) { return new PrimitiveType(name); });
    registerTypeCreator<PrimitiveType>("object", 
        [](const std::string& name) { return new PrimitiveType(name); });
    
    // 注册特性
    ObjectType::registerFeature("int", "numeric", true);
    ObjectType::registerFeature("double", "numeric", true);
    ObjectType::registerFeature("bool", "bool", true);
    
    ObjectType::registerFeature("string", "sequence", true);
    ObjectType::registerFeature("string", "reference", true);
    
    // 注册any类型的特性 - 可以与任何类型兼容
    ObjectType::registerFeature("any", "numeric", true);
    ObjectType::registerFeature("any", "sequence", true);
    ObjectType::registerFeature("any", "reference", true);
    ObjectType::registerFeature("any", "container", true);
    ObjectType::registerFeature("any", "mutable", true);
    ObjectType::registerFeature("any", "bool", true);
    
    // object类型也作为一种通用类型
    ObjectType::registerFeature("object", "reference", true);
    
    ObjectType::registerFeature("list", "container", true);
    ObjectType::registerFeature("list", "sequence", true);
    ObjectType::registerFeature("list", "mutable", true);
    ObjectType::registerFeature("list", "reference", true);
    
    ObjectType::registerFeature("dict", "container", true);
    ObjectType::registerFeature("dict", "mapping", true);
    ObjectType::registerFeature("dict", "mutable", true);
    ObjectType::registerFeature("dict", "reference", true);

    // 注册容器基础类型
    auto listBaseType = new PrimitiveType("list_base");
    registerType("list_base", listBaseType);
    typeIdMap[PY_TYPE_LIST_BASE] = listBaseType;
    // 使用静态方法注册特性
    ObjectType::registerFeature("list_base", "container", true);
    ObjectType::registerFeature("list_base", "sequence", true);
    ObjectType::registerFeature("list_base", "reference", true);
    
    auto dictBaseType = new PrimitiveType("dict_base");
    registerType("dict_base", dictBaseType);
    typeIdMap[PY_TYPE_DICT_BASE] = dictBaseType;
    // 使用静态方法注册特性
    ObjectType::registerFeature("dict_base", "container", true);
    ObjectType::registerFeature("dict_base", "mapping", true);
    ObjectType::registerFeature("dict_base", "reference", true);
    
    auto funcBaseType = new PrimitiveType("func_base");
    registerType("func_base", funcBaseType);
    typeIdMap[PY_TYPE_FUNC_BASE] = funcBaseType;
    
    // 注册指针类型
    auto ptrType = new PrimitiveType("ptr");
    registerType("ptr", ptrType);
    typeIdMap[PY_TYPE_PTR] = ptrType;
    // 使用静态方法注册特性
    ObjectType::registerFeature("ptr", "reference", true);
    
    auto ptrIntType = new PrimitiveType("ptr_int");
    registerType("ptr_int", ptrIntType);
    typeIdMap[PY_TYPE_PTR_INT] = ptrIntType;
    // 使用静态方法注册特性
    ObjectType::registerFeature("ptr_int", "reference", true);
    
    auto ptrDoubleType = new PrimitiveType("ptr_double");
    registerType("ptr_double", ptrDoubleType);
    typeIdMap[PY_TYPE_PTR_DOUBLE] = ptrDoubleType;
    // 使用静态方法注册特性
    ObjectType::registerFeature("ptr_double", "reference", true);
    
    // 注册类型创建器 - 容器基础类型
    registerTypeCreator<PrimitiveType>("list_base", 
        [](const std::string& name) { return new PrimitiveType(name); });
    registerTypeCreator<PrimitiveType>("dict_base", 
        [](const std::string& name) { return new PrimitiveType(name); });
    registerTypeCreator<PrimitiveType>("func_base", 
        [](const std::string& name) { return new PrimitiveType(name); });
    
    // 注册类型创建器 - 指针类型
    registerTypeCreator<PrimitiveType>("ptr", 
        [](const std::string& name) { return new PrimitiveType(name); });
    registerTypeCreator<PrimitiveType>("ptr_int", 
        [](const std::string& name) { return new PrimitiveType(name); });
    registerTypeCreator<PrimitiveType>("ptr_double", 
        [](const std::string& name) { return new PrimitiveType(name); });

    // 注册函数类型

    // 添加函数类型的特性
    ObjectType::registerFeature("func_base", "callable", true);
    ObjectType::registerFeature("func_base", "reference", true);
    
    // 添加通用函数类型特性
    ObjectType::registerFeature("func", "callable", true);
    ObjectType::registerFeature("func", "reference", true);
    ObjectType::registerFeature("func", "returns_callable", true);
    ObjectType::registerFeature("func", "returns_self", true);
    // 为函数复杂调用方式打的补丁 , 主要是object类型会影响类型分析
        ObjectType::registerFeature("object", "callable", true);     // 新增
    ObjectType::registerFeature("object", "returns_callable", true); // 新增
    ObjectType::registerFeature("object", "returns_self", true);  // 新增
    
    
    
}




void TypeFeatureChecker::registerBuiltinFeatureChecks()
{
    // 容器检查
    // 容器检查 - 增强对PY_TYPE_LIST的支持
    // 容器检查
    registerFeatureCheck("container", [](const ObjectType* type) -> bool
                         {
    if (!type) return false;
    
    int typeId = type->getTypeId();
    return type->hasFeature("container") || 
           dynamic_cast<const ListType*>(type) || 
           dynamic_cast<const DictType*>(type) ||
           typeId == PY_TYPE_LIST || 
           typeId == PY_TYPE_DICT ||
           (typeId >= PY_TYPE_LIST_BASE && typeId < PY_TYPE_DICT_BASE) ||
           (typeId >= PY_TYPE_DICT_BASE && typeId < PY_TYPE_FUNC_BASE); });

    // 在 registerBuiltinFeatureChecks 方法中添加
    // 可索引类型检查
    registerFeatureCheck("indexable", [](const ObjectType* type) -> bool
                         {
if (!type) return false;

int typeId = type->getTypeId();
return type->hasFeature("indexable") ||
       dynamic_cast<const ListType*>(type) ||
       typeId == PY_TYPE_STRING ||
       typeId == PY_TYPE_LIST ||
       typeId == PY_TYPE_DICT ||
       (typeId >= PY_TYPE_LIST_BASE && typeId < PY_TYPE_DICT_BASE) ||
       (typeId >= PY_TYPE_DICT_BASE && typeId < PY_TYPE_FUNC_BASE); });

    // 引用类型检查
    // 引用类型检查
    registerFeatureCheck("reference", [](const ObjectType* type) -> bool
                         {
    if (!type) return false;
    
    int typeId = type->getTypeId();
    return type->hasFeature("reference") ||
           type->getCategory() == ObjectType::Reference ||
           typeId == PY_TYPE_STRING ||
           typeId == PY_TYPE_LIST ||
           typeId == PY_TYPE_DICT ||
           (typeId >= PY_TYPE_LIST_BASE && typeId < PY_TYPE_FUNC_BASE) ||
           (typeId >= PY_TYPE_PTR); });

    // 序列类型检查
    // 序列类型检查 - 增强对PY_TYPE_LIST的支持
    // 序列类型检查 - 增强对LIST相关类型的支持
    registerFeatureCheck("sequence", [](const ObjectType* type) -> bool
                         {
        if (!type) return false;
        
        int typeId = type->getTypeId();
        return type->hasFeature("sequence") ||
               dynamic_cast<const ListType*>(type) ||
               typeId == PY_TYPE_STRING ||
               typeId == PY_TYPE_LIST ||
               (typeId >= PY_TYPE_LIST_BASE && typeId < PY_TYPE_DICT_BASE); });

    // 映射类型检查
    registerFeatureCheck("mapping", [](const ObjectType* type)
                         { return dynamic_cast<const DictType*>(type); });

    // 数值类型检查
    registerFeatureCheck("numeric", [](const ObjectType* type)
                         {
            const std::string& name = type->getName();
            return type->getTypeId() == PY_TYPE_INT || type->getTypeId() == PY_TYPE_DOUBLE; });

    // 可变类型检查
    registerFeatureCheck("mutable", [](const ObjectType* type)
                         { return dynamic_cast<const ListType*>(type) || dynamic_cast<const DictType*>(type); });
}


ObjectType* TypeRegistry::getTypeById(int typeId)
{
    // 确保基本类型已注册
    ensureBasicTypesRegistered();

    // 先检查是否在ID映射中
    auto it = typeIdMap.find(typeId);
    if (it != typeIdMap.end()) {
        return it->second;
    }

    // 如果没找到，扫描所有已注册类型
    // 扫描命名类型
    for (const auto& pair : namedTypes) {
        if (pair.second && pair.second->getTypeId() == typeId) {
            // 找到匹配的类型，添加到映射中以加速后续查找
            typeIdMap[typeId] = pair.second;
            return pair.second;
        }
    }

    // 扫描列表类型
    for (const auto& pair : listTypes) {
        if (pair.second && pair.second->getTypeId() == typeId) {
            typeIdMap[typeId] = pair.second;
            return pair.second;
        }
    }

    // 扫描字典类型
    for (const auto& pair : dictTypes) {
        if (pair.second && pair.second->getTypeId() == typeId) {
            typeIdMap[typeId] = pair.second;
            return pair.second;
        }
    }

    // 扫描函数类型
    for (const auto& pair : functionTypes) {
        if (pair.second && pair.second->getTypeId() == typeId) {
            typeIdMap[typeId] = pair.second;
            return pair.second;
        }
    }

    // 根据基本类型ID查找
    switch (typeId) {
        case PY_TYPE_NONE:
            return getType("none");
        case PY_TYPE_INT:
            return getType("int");
        case PY_TYPE_DOUBLE:
            return getType("double");
        case PY_TYPE_BOOL:
            return getType("bool");
        case PY_TYPE_STRING:
            return getType("string");
        case PY_TYPE_LIST:
            return getType("list");
        case PY_TYPE_DICT:
            return getType("dict");
        case PY_TYPE_ANY:
            return getType("any");
        default:
            // 未找到对应类型，返回any类型作为备选
            return getType("any");
    }
}


ObjectType* TypeRegistry::getType(const std::string& name) {
    // 标准化类型名称，避免大小写或空格导致的不匹配
    std::string normalizedName = name;
    std::transform(normalizedName.begin(), normalizedName.end(), normalizedName.begin(), ::tolower);
    
    // 特殊处理list类型 - 确保不会返回any类型
    if (normalizedName == "list") {
        // 检查是否已存在list类型
        auto listIt = namedTypes.find("list");
        if (listIt != namedTypes.end()) {
            return listIt->second;
        }
        
        // 创建空列表类型
        ObjectType* anyType = getType("any");
        ListType* listType = new ListType(anyType);
        registerType("list", listType);
        return listType;
    }
    
    // 先在已注册类型中查找
    auto it = namedTypes.find(normalizedName);
    if (it != namedTypes.end()) {
        return it->second;
    }
    
    // 尝试使用类型创建器
    auto creatorIt = typeCreators.find(normalizedName);
    if (creatorIt != typeCreators.end()) {
        ObjectType* newType = creatorIt->second(normalizedName);
        registerType(normalizedName, newType);
        return newType;
    }
    
    // 如果找不到，对于非空名称的请求，尝试查找any或object类型
    if (!normalizedName.empty()) {
        auto anyType = namedTypes.find("any");
        if (anyType != namedTypes.end()) {
            return anyType->second;
        }
        
        auto objectType = namedTypes.find("object");
        if (objectType != namedTypes.end()) {
            return objectType->second;
        }
    }
    
    // 极端情况：如果any和object都没有找到，创建一个
    if (normalizedName == "any" || normalizedName.empty()) {
        PrimitiveType* anyType = new PrimitiveType("any");
        registerType("any", anyType);
        return anyType;
    }
    
    return nullptr;
}
// 添加 ListType 构造函数实现
ListType::ListType(ObjectType* elemType) 
    : ObjectType("list<" + elemType->getName() + ">", Container), elementType(elemType) {
    // 注册列表特性
    static bool registered = false;
    if (!registered) {
        registerFeature("list<>", "sequence", true);
        registerFeature("list<>", "container", true);
        registerFeature("list<>", "mutable", true);
        registerFeature("list<>", "reference", true);
        registered = true;
    }
}
ListType* TypeRegistry::getListType(ObjectType* elemType) {
    std::string signature = "list<" + elemType->getName() + ">";
    
    auto it = namedTypes.find(signature);
    if (it != namedTypes.end()) {
        return static_cast<ListType*>(it->second);
    }
    
    // 创建新的列表类型
    ListType* listType = new ListType(elemType);
    registerType(signature, listType);
    listTypes[signature] = listType;
    
    return listType;
}

DictType* TypeRegistry::getDictType(ObjectType* keyType, ObjectType* valueType) {
    std::string signature = "dict<" + keyType->getName() + "," + 
                          valueType->getName() + ">";
    
    auto it = namedTypes.find(signature);
    if (it != namedTypes.end()) {
        return static_cast<DictType*>(it->second);
    }
    
    // 创建新的字典类型
    DictType* dictType = new DictType(keyType, valueType);
    registerType(signature, dictType);
    dictTypes[signature] = dictType;
    
    return dictType;
}

FunctionType* TypeRegistry::getFunctionType(ObjectType* returnType, 
                                         std::vector<ObjectType*> paramTypes) {
    // 构建函数签名
    std::stringstream ss;
    ss << "func<" << returnType->getName() << "(";
    
    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (i > 0) ss << ",";
        ss << paramTypes[i]->getName();
    }
    
    ss << ")>";
    std::string signature = ss.str();
    
    auto it = namedTypes.find(signature);
    if (it != namedTypes.end()) {
        return static_cast<FunctionType*>(it->second);
    }
    
    // 创建新的函数类型
    FunctionType* funcType = new FunctionType(returnType, paramTypes);
    registerType(signature, funcType);
    functionTypes[signature] = funcType;
    
    return funcType;
}

ObjectType* TypeRegistry::parseTypeFromString(const std::string& typeStr) {
    // 处理简单类型
    auto basicType = getType(typeStr);
    if (basicType) {
        return basicType;
    }
    
    // 处理列表类型: list<T>
    if (typeStr.substr(0, 5) == "list<" && 
        typeStr[typeStr.length()-1] == '>') {
        
        std::string elemTypeName = typeStr.substr(5, typeStr.length() - 6);
        ObjectType* elemType = parseTypeFromString(elemTypeName);
        
        if (elemType) {
            return getListType(elemType);
        }
    }
    
    // 处理字典类型: dict<K,V>
    if (typeStr.substr(0, 5) == "dict<" && 
        typeStr[typeStr.length()-1] == '>') {
        
        std::string innerTypes = typeStr.substr(5, typeStr.length() - 6);
        size_t commaPos = innerTypes.find(',');
        
        if (commaPos != std::string::npos) {
            std::string keyTypeName = innerTypes.substr(0, commaPos);
            std::string valueTypeName = innerTypes.substr(commaPos + 1);
            
            ObjectType* keyType = parseTypeFromString(keyTypeName);
            ObjectType* valueType = parseTypeFromString(valueTypeName);
            
            if (keyType && valueType) {
                return getDictType(keyType, valueType);
            }
        }
    }
    
    // 未识别的类型，默认返回double
    return getType("double");
}
ObjectType* TypeRegistry::getSymbolType(const std::string& name) const {
    auto it = symbolTypes.find(name);
    if (it != symbolTypes.end()) {
        return it->second;
    }
    return nullptr;
}
bool TypeRegistry::canConvert(ObjectType* from, ObjectType* to) {
    if (!from || !to) return false;
    
    // 相同类型可以直接转换
    if (from->getTypeSignature() == to->getTypeSignature()) {
        return true;
    }
    
        // any类型可以转换为任何类型，任何类型也可以转换为any
        if (from->getName() == "any" || to->getName() == "any") {
            return true;
        }
    // 数值类型之间可以相互转换
    if (TypeFeatureChecker::isNumeric(from) && TypeFeatureChecker::isNumeric(to)) {
        return true;
    }
        // 布尔类型可以与数值类型互相转换
        if ((from->getName() == "bool" && TypeFeatureChecker::isNumeric(to)) ||
        (to->getName() == "bool" && TypeFeatureChecker::isNumeric(from))) {
        return true;
    }
    // 容器类型的转换 - 如果元素类型匹配或可以转换
    if (auto fromList = dynamic_cast<ListType*>(from)) {
        if (auto toList = dynamic_cast<ListType*>(to)) {
            return canConvert(
                const_cast<ObjectType*>(fromList->getElementType()),
                const_cast<ObjectType*>(toList->getElementType()));
        }
    }
    
    // 字典类型的转换
    if (auto fromDict = dynamic_cast<DictType*>(from)) {
        if (auto toDict = dynamic_cast<DictType*>(to)) {
            return canConvert(
                const_cast<ObjectType*>(fromDict->getKeyType()),
                const_cast<ObjectType*>(toDict->getKeyType())) &&
                canConvert(
                    const_cast<ObjectType*>(fromDict->getValueType()),
                    const_cast<ObjectType*>(toDict->getValueType()));
        }
    }
    
    return false;
}

void TypeRegistry::registerType(const std::string& name, ObjectType* type) {
    // 检查是否已注册
    auto it = namedTypes.find(name);
    if (it != namedTypes.end()) {
        // 类型已存在，不重复注册
        return;
    }
    
    namedTypes[name] = type;
        // 同时更新类型ID映射
        typeIdMap[type->getTypeId()] = type;
}

FunctionType* TypeRegistry::getFunctionType(const std::string& functionName)
{
    // 在函数类型映射中查找
    auto it = functionTypes.find(functionName);
    if (it != functionTypes.end()) {
        return it->second;
    }
    
    // 未找到，返回nullptr
    return nullptr;
}

} // namespace llvmpy