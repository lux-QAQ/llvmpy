#include "ObjectType.h"
#include <sstream>
#include <llvm/IR/DerivedTypes.h>   // 添加这行，提供StructType, PointerType等定义
#include <llvm/IR/Type.h>           // 添加这行，提供Type类的完整定义
#include <llvm/IR/LLVMContext.h>    // 添加这行，确保LLVMContext定义完整
#include "TypeIDs.h"    

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
    // 基础实现：只有相同类型才能转换
    return getTypeSignature() == other->getTypeSignature();
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
        if (name == "int") return 1;
        if (name == "double") return 2;
        if (name == "bool") return 3;
        if (name == "string") return 4;
        if (name == "void") return 0;
        if (name == "any") return 5;
        if (name == "object") return 6;
        return -1; // 未知类型
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
    return 100 + elementType->getTypeId(); // 列表类型起始ID为100
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
    return 200 + keyType->getTypeId() * 100 + valueType->getTypeId();
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
    return 300 + returnType->getTypeId(); // 函数类型ID从300开始
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
}

ObjectType* TypeRegistry::getType(const std::string& name) {
    // 先在已注册类型中查找
    auto it = namedTypes.find(name);
    if (it != namedTypes.end()) {
        return it->second;
    }
    
    // 尝试使用类型创建器
    auto creatorIt = typeCreators.find(name);
    if (creatorIt != typeCreators.end()) {
        ObjectType* newType = creatorIt->second(name);
        registerType(name, newType);
        return newType;
    }
    
    // 如果找不到，对于非空名称的请求，尝试查找any或object类型
    if (!name.empty()) {
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
    if (name == "any" || name.empty()) {
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
}

} // namespace llvmpy