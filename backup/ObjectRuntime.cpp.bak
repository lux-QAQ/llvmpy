#include "ObjectRuntime.h"
#include "CodeGen/codegen.h"
#include "TypeOperations.h"
#include "ObjectLifecycle.h"
#include "TypeIDs.h"
#include "ast.h"
#include <iostream>
#include <string>
class PyCodeGen;  // 前向声明
namespace llvmpy {

// 构造函数
ObjectRuntime::ObjectRuntime(llvm::Module* mod, llvm::IRBuilder<>* b)
    : module(mod), builder(b), context(mod->getContext()) {
    // 创建操作代码生成器
    opCodeGen = std::make_unique<OperationCodeGenerator>();
    
    // 创建操作结果处理器
    resultHandler = std::make_unique<OperationResultHandler>();
    
    // 创建生命周期管理器
    lifecycleManager = std::make_unique<ObjectLifecycleManager>();
}

// 初始化运行时
void ObjectRuntime::initialize() {
    // 创建运行时类型
    createRuntimeTypes();
    
    // 声明运行时函数
    declareRuntimeFunctions();
    
    // 注册类型操作
    registerTypeOperations();
    
    // 确保类型注册表已初始化
    TypeRegistry::getInstance().ensureBasicTypesRegistered();
    
    // 注册类型特性检查
    TypeFeatureChecker::registerBuiltinFeatureChecks();
}

//===----------------------------------------------------------------------===//
// 对象创建操作
//===----------------------------------------------------------------------===//
std::unique_ptr<PyCodeGen> ObjectRuntime::asPyCodeGen() {
    // 创建一个 PyCodeGen 对象，传递必要的组件和 this 指针
    return std::make_unique<PyCodeGen>(module, builder, &context, this);
}
// 创建整数对象
llvm::Value* ObjectRuntime::createIntObject(llvm::Value* value) {
    // 使用 asPyCodeGen 获取兼容的 PyCodeGen 对象
    auto gen = asPyCodeGen();
    return ObjectFactory::createInt(*gen, value);
}

// 创建浮点数对象
llvm::Value* ObjectRuntime::createDoubleObject(llvm::Value* value) {
    auto gen = asPyCodeGen();
    return ObjectFactory::createDouble(*gen, value);
}

// 创建布尔对象
llvm::Value* ObjectRuntime::createBoolObject(llvm::Value* value) {
    auto gen = asPyCodeGen();
    return ObjectFactory::createBool(*gen, value);
}

// 创建字符串对象
llvm::Value* ObjectRuntime::createStringObject(llvm::Value* value) {
    auto gen = asPyCodeGen();
    return ObjectFactory::createString(*gen, value);
}

// 创建列表对象
llvm::Value* ObjectRuntime::createList(llvm::Value* size, ObjectType* elemType) {
    // 获取列表创建函数
    std::string funcName = "py_create_list";
    llvm::Function* createFunc = runtimeFuncs[funcName];
    
    if (!createFunc) {
        // 声明创建列表的运行时函数
        llvm::StructType* pyObjType = getPyObjectStructType();
        llvm::Type* pyObjPtrType = llvm::PointerType::get(pyObjType, 0);
        
        std::vector<llvm::Type*> argTypes = {
            llvm::Type::getInt32Ty(context),  // size
            llvm::Type::getInt32Ty(context)   // elemTypeId
        };
        
        createFunc = getRuntimeFunction("py_create_list", 
                                        pyObjPtrType, 
                                        argTypes);
        
        runtimeFuncs["py_create_list"] = createFunc;
    }
    
    // 添加元素类型ID - 使用统一的映射函数
    int elemTypeID = mapTypeIdToRuntime(elemType);
    llvm::Value* elemTypeIdValue = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(context), 
        elemTypeID);
    
    // 调用创建函数
    llvm::Value* list = builder->CreateCall(createFunc, {size, elemTypeIdValue}, "new_list");
    
    // 跟踪创建的对象，以便自动管理生命周期
    trackObject(list);
    
    return list;
}

// 使用初始值创建列表
llvm::Value* ObjectRuntime::createListWithValues(
    std::vector<llvm::Value*> values, ObjectType* elemType) {
    
    // 创建指定大小的列表
    llvm::Value* size = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(context), 
        values.size());
        
    llvm::Value* list = createList(size, elemType);
    
    // 填充列表元素
    for (size_t i = 0; i < values.size(); i++) {
        llvm::Value* index = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(context), i);
        
        // 准备元素值 - 确保类型正确
        llvm::Value* preparedValue = prepareArgument(
            values[i], 
            elemType, // 假设元素已经是正确类型
            elemType
        );
            
        setListElement(list, index, preparedValue);
    }
    
    return list;
}

// 创建字典对象
llvm::Value* ObjectRuntime::createDict(ObjectType* keyType, ObjectType* valueType) {
    // 获取字典创建函数
    llvm::Function* createFunc = runtimeFuncs["py_create_dict"];
    
    if (!createFunc) {
        // 声明字典创建函数
        llvm::StructType* pyObjType = getPyObjectStructType();
        llvm::Type* pyObjPtrType = llvm::PointerType::get(pyObjType, 0);
            
        std::vector<llvm::Type*> argTypes = {
            llvm::Type::getInt32Ty(context),  // initialCapacity
            llvm::Type::getInt32Ty(context)   // keyTypeId
        };
        
        createFunc = getRuntimeFunction("py_create_dict", 
                                        pyObjPtrType, 
                                        argTypes);
                                       
        runtimeFuncs["py_create_dict"] = createFunc;
    }
    
    // 初始容量为8
    llvm::Value* capacity = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(context), 8);
        
    // 获取键类型ID - 使用统一的映射函数
    int keyTypeID = mapTypeIdToRuntime(keyType);
    llvm::Value* keyTypeIdValue = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(context), 
        keyTypeID);
        
    // 调用创建函数
    llvm::Value* dict = builder->CreateCall(createFunc, {capacity, keyTypeIdValue}, "new_dict");
    
    // 跟踪创建的对象，以便自动管理生命周期
    trackObject(dict);
    
    return dict;
}

// 创建通用对象
llvm::Value* ObjectRuntime::createObject(llvm::Value* value, ObjectType* type) {
    if (!type) {
        std::cerr << "Error: Attempt to create object with null type" << std::endl;
        return nullptr;
    }
    
    int typeId = type->getTypeId();
    
    // 使用类型ID选择正确的创建方法
    switch (typeId) {
        case PY_TYPE_INT:
            return createIntObject(value);
            
        case PY_TYPE_DOUBLE:
            return createDoubleObject(value);
            
        case PY_TYPE_BOOL:
            return createBoolObject(value);
            
        case PY_TYPE_STRING:
            return createStringObject(value);
            
        case PY_TYPE_LIST:
            if (llvm::ConstantInt* constSize = llvm::dyn_cast<llvm::ConstantInt>(value)) {
                // 如果size是常量，创建空列表
                return createList(value, 
                    // 获取列表元素类型，如果是ListType
                    dynamic_cast<ListType*>(type) ? 
                        const_cast<ObjectType*>(dynamic_cast<ListType*>(type)->getElementType()) : 
                        TypeRegistry::getInstance().getType("object")
                );
            }
            
        default:
            if (typeId >= PY_TYPE_LIST_BASE && typeId < PY_TYPE_DICT_BASE) {
                // 复合列表类型
                ListType* listType = dynamic_cast<ListType*>(type);
                if (listType) {
                    return createList(
                        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), // 空列表
                        const_cast<ObjectType*>(listType->getElementType())
                    );
                }
            }
            else if (typeId >= PY_TYPE_DICT_BASE) {
                // 复合字典类型
                DictType* dictType = dynamic_cast<DictType*>(type);
                if (dictType) {
                    return createDict(
                        const_cast<ObjectType*>(dictType->getKeyType()),
                        const_cast<ObjectType*>(dictType->getValueType())
                    );
                }
            }
            
            // 未知类型，返回空指针
            std::cerr << "Unknown type ID in createObject: " << typeId << std::endl;
            return llvm::ConstantPointerNull::get(
                llvm::PointerType::get(context, 0));
    }
}

//===----------------------------------------------------------------------===//
// 容器操作
//===----------------------------------------------------------------------===//

// 获取列表元素
llvm::Value* ObjectRuntime::getListElement(llvm::Value* list, llvm::Value* index) {
    // 获取列表元素访问函数
    llvm::Function* getItemFunc = runtimeFuncs["py_list_get_item"];
    
    if (!getItemFunc) {
        // 声明获取列表元素的函数
        llvm::StructType* pyObjType = getPyObjectStructType();
        llvm::Type* pyObjPtrType = llvm::PointerType::get(pyObjType, 0);
        
        std::vector<llvm::Type*> argTypes = {
            pyObjPtrType,                      // list
            llvm::Type::getInt32Ty(context)    // index
        };
        
        // 返回值类型是通用对象指针(会根据列表元素类型在运行时进行转换)
        getItemFunc = getRuntimeFunction("py_list_get_item", 
                                         pyObjPtrType, 
                                         argTypes);
                                       
        runtimeFuncs["py_list_get_item"] = getItemFunc;
    }
    
    // 确保列表是PyObject*类型
    llvm::Value* listObj = ensurePyObjectPtr(list);
    
    // 调用获取元素函数
    llvm::Value* element = builder->CreateCall(getItemFunc, {listObj, index}, "list_elem");
    
    // 不需要手动跟踪，因为这是从列表获取的已存在对象
    return element;
}

// 设置列表元素
void ObjectRuntime::setListElement(llvm::Value* list, llvm::Value* index, llvm::Value* value) {
    // 获取列表元素设置函数
    llvm::Function* setItemFunc = runtimeFuncs["py_list_set_item"];
    
    if (!setItemFunc) {
        // 声明设置列表元素的函数
        llvm::StructType* pyObjType = getPyObjectStructType();
        llvm::Type* pyObjPtrType = llvm::PointerType::get(pyObjType, 0);
        
        std::vector<llvm::Type*> argTypes = {
            pyObjPtrType,                      // list
            llvm::Type::getInt32Ty(context),   // index
            pyObjPtrType                       // value
        };
        
        setItemFunc = getRuntimeFunction("py_list_set_item", 
                                         llvm::Type::getVoidTy(context), 
                                         argTypes);
                                       
        runtimeFuncs["py_list_set_item"] = setItemFunc;
    }
    
    // 确保列表和值是PyObject*类型
    llvm::Value* listObj = ensurePyObjectPtr(list);
    llvm::Value* valueObj = ensurePyObjectPtr(value);
    
    // 调用设置元素函数 - 会自动增加valueObj的引用计数
    builder->CreateCall(setItemFunc, {listObj, index, valueObj});
}

// 获取列表长度
llvm::Value* ObjectRuntime::getListLength(llvm::Value* list) {
    // 获取列表长度函数
    llvm::Function* lenFunc = runtimeFuncs["py_list_len"];
    
    if (!lenFunc) {
        // 声明获取列表长度的函数
        llvm::StructType* pyObjType = getPyObjectStructType();
        llvm::Type* pyObjPtrType = llvm::PointerType::get(pyObjType, 0);
        
        std::vector<llvm::Type*> argTypes = {pyObjPtrType};
        
        lenFunc = getRuntimeFunction("py_list_len", 
                                     llvm::Type::getInt32Ty(context), 
                                     argTypes);
                                   
        runtimeFuncs["py_list_len"] = lenFunc;
    }
    
    // 确保列表是PyObject*类型
    llvm::Value* listObj = ensurePyObjectPtr(list);
    
    // 调用获取长度函数
    return builder->CreateCall(lenFunc, {listObj}, "list_len");
}

// 向列表追加元素
llvm::Value* ObjectRuntime::appendToList(llvm::Value* list, llvm::Value* value) {
    // 获取列表追加函数
    llvm::Function* appendFunc = runtimeFuncs["py_list_append"];
    
    if (!appendFunc) {
        // 声明列表追加函数
        llvm::StructType* pyObjType = getPyObjectStructType();
        llvm::Type* pyObjPtrType = llvm::PointerType::get(pyObjType, 0);
        
        std::vector<llvm::Type*> argTypes = {
            pyObjPtrType,  // list
            pyObjPtrType   // item
        };
        
        appendFunc = getRuntimeFunction("py_list_append", 
                                        pyObjPtrType, 
                                        argTypes);
                                      
        runtimeFuncs["py_list_append"] = appendFunc;
    }
    
    // 确保列表和值是PyObject*类型
    llvm::Value* listObj = ensurePyObjectPtr(list);
    llvm::Value* valueObj = ensurePyObjectPtr(value);
    
    // 调用追加函数 - 会自动增加valueObj的引用计数
    return builder->CreateCall(appendFunc, {listObj, valueObj}, "append_result");
}

// 获取字典项
llvm::Value* ObjectRuntime::getDictItem(llvm::Value* dict, llvm::Value* key) {
    if (!dict || !key) return nullptr;
    
    // 获取字典项获取函数
    llvm::Function* getDictItemFunc = runtimeFuncs["py_dict_get_item"];
    
    if (!getDictItemFunc) {
        // 声明字典项获取函数
        llvm::StructType* pyObjType = getPyObjectStructType();
        llvm::Type* pyObjPtrType = llvm::PointerType::get(pyObjType, 0);
        
        std::vector<llvm::Type*> argTypes = {
            pyObjPtrType,  // dict
            pyObjPtrType   // key
        };
        
        getDictItemFunc = getRuntimeFunction("py_dict_get_item", 
                                            pyObjPtrType, 
                                            argTypes);
                                            
        runtimeFuncs["py_dict_get_item"] = getDictItemFunc;
    }
    
    // 确保字典和键是PyObject*类型
    llvm::Value* dictObj = ensurePyObjectPtr(dict);
    llvm::Value* keyObj = ensurePyObjectPtr(key);
    
    // 调用获取函数
    return builder->CreateCall(getDictItemFunc, {dictObj, keyObj}, "dict_item");
}

// 设置字典项
void ObjectRuntime::setDictItem(llvm::Value* dict, llvm::Value* key, llvm::Value* value) {
    if (!dict || !key || !value) return;
    
    // 获取字典项设置函数
    llvm::Function* setDictItemFunc = runtimeFuncs["py_dict_set_item"];
    
    if (!setDictItemFunc) {
        // 声明字典项设置函数
        llvm::StructType* pyObjType = getPyObjectStructType();
        llvm::Type* pyObjPtrType = llvm::PointerType::get(pyObjType, 0);
        
        std::vector<llvm::Type*> argTypes = {
            pyObjPtrType,  // dict
            pyObjPtrType,  // key
            pyObjPtrType   // value
        };
        
        setDictItemFunc = getRuntimeFunction("py_dict_set_item", 
                                            llvm::Type::getVoidTy(context), 
                                            argTypes);
                                            
        runtimeFuncs["py_dict_set_item"] = setDictItemFunc;
    }
    
    // 确保字典、键和值是PyObject*类型
    llvm::Value* dictObj = ensurePyObjectPtr(dict);
    llvm::Value* keyObj = ensurePyObjectPtr(key);
    llvm::Value* valueObj = ensurePyObjectPtr(value);
    
    // 调用设置函数 - 会自动增加keyObj和valueObj的引用计数
    builder->CreateCall(setDictItemFunc, {dictObj, keyObj, valueObj});
}

// 获取字典键列表
llvm::Value* ObjectRuntime::getDictKeys(llvm::Value* dict) {
    if (!dict) return nullptr;
    
    // 获取字典键函数
    llvm::Function* getDictKeysFunc = runtimeFuncs["py_dict_keys"];
    
    if (!getDictKeysFunc) {
        // 声明字典键函数
        llvm::StructType* pyObjType = getPyObjectStructType();
        llvm::Type* pyObjPtrType = llvm::PointerType::get(pyObjType, 0);
        
        std::vector<llvm::Type*> argTypes = {pyObjPtrType}; // dict
        
        getDictKeysFunc = getRuntimeFunction("py_dict_keys", 
                                            pyObjPtrType, 
                                            argTypes);
                                            
        runtimeFuncs["py_dict_keys"] = getDictKeysFunc;
    }
    
    // 确保字典是PyObject*类型
    llvm::Value* dictObj = ensurePyObjectPtr(dict);
    
    // 调用获取键函数 - 返回一个新的列表对象
    llvm::Value* keysList = builder->CreateCall(getDictKeysFunc, {dictObj}, "dict_keys");
    
    // 跟踪新创建的列表对象
    trackObject(keysList);
    
    return keysList;
}

//===----------------------------------------------------------------------===//
// 类型操作
//===----------------------------------------------------------------------===//

// 类型转换
llvm::Value* ObjectRuntime::convertValue(llvm::Value* value, ObjectType* fromType, ObjectType* toType) {
    // 如果类型相同，不需要转换
    if (fromType == toType || !value) {
        return value;
    }
    
    int fromTypeId = fromType->getTypeId();
    int toTypeId = toType->getTypeId();
    
        // 使用 asPyCodeGen 获取兼容的 PyCodeGen 对象
        auto gen = asPyCodeGen();
        return OperationCodeGenerator::handleTypeConversion(
            *gen, value, fromTypeId, toTypeId);
}

// 执行二元操作
llvm::Value* ObjectRuntime::performBinaryOp(
    char op, llvm::Value* left, llvm::Value* right, 
    ObjectType* leftType, ObjectType* rightType) {
    
    if (!left || !right || !leftType || !rightType) {
        return nullptr;
    }
    
    int leftTypeId = leftType->getTypeId();
    int rightTypeId = rightType->getTypeId();
    
        // 使用 asPyCodeGen 获取兼容的 PyCodeGen 对象
        auto gen = asPyCodeGen();
        llvm::Value* result = OperationCodeGenerator::handleBinaryOp(
            *gen, op, left, right, leftTypeId, rightTypeId);
        
    
    // 获取操作结果的类型ID
    int resultTypeId = TypeOperationRegistry::getInstance().getResultTypeId(
        leftTypeId, rightTypeId, op);
    
    // 跟踪操作结果对象
    if (result && result->getType()->isPointerTy()) {
        trackObject(result);
    }
    
    return result;
}

// 执行一元操作
llvm::Value* ObjectRuntime::performUnaryOp(
    char op, llvm::Value* operand, ObjectType* type) {
    
    if (!operand || !type) {
        return nullptr;
    }
    
    int typeId = type->getTypeId();
    
    // 使用OperationCodeGenerator处理一元操作
       // 使用 asPyCodeGen 获取兼容的 PyCodeGen 对象
       auto gen = asPyCodeGen();
       llvm::Value* result = OperationCodeGenerator::handleUnaryOp(
           *gen, op, operand, typeId);
    
    // 跟踪操作结果对象
    if (result && result->getType()->isPointerTy()) {
        trackObject(result);
    }
    
    return result;
}

// 类型检查
llvm::Value* ObjectRuntime::checkType(llvm::Value* obj, ObjectType* expectedType) {
    if (!obj || !expectedType) {
        return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
    }
    
    // 使用统一的类型ID
    int expectedTypeId = mapTypeIdToRuntime(expectedType);
    
    // 生成类型检查代码
    return generateTypeCheck(obj, expectedTypeId);
}

// 实例检查
llvm::Value* ObjectRuntime::isInstance(llvm::Value* obj, int typeId) {
    if (!obj) {
        return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
    }
    
    // 生成类型检查代码
    return generateTypeCheck(obj, typeId);
}

//===----------------------------------------------------------------------===//
// 对象生命周期管理
//===----------------------------------------------------------------------===//

// 增加引用计数
llvm::Value* ObjectRuntime::incRef(llvm::Value* obj) {
    if (!obj) return nullptr;
    
    // 获取增加引用计数函数
    llvm::Function* incRefFunc = runtimeFuncs["py_incref"];
    
    if (!incRefFunc) {
        // 声明增加引用计数函数
        llvm::StructType* pyObjType = getPyObjectStructType();
        llvm::Type* pyObjPtrType = llvm::PointerType::get(pyObjType, 0);
        
        std::vector<llvm::Type*> argTypes = {pyObjPtrType};
        
        incRefFunc = getRuntimeFunction("py_incref", 
                                       llvm::Type::getVoidTy(context), 
                                       argTypes);
                                      
        runtimeFuncs["py_incref"] = incRefFunc;
    }
    
    // 确保对象是PyObject*类型
    llvm::Value* objPtr = ensurePyObjectPtr(obj);
    
    // 调用增加引用计数函数
    builder->CreateCall(incRefFunc, {objPtr});
    
    return obj;
}

// 减少引用计数
llvm::Value* ObjectRuntime::decRef(llvm::Value* obj) {
    if (!obj) return nullptr;
    
    // 获取减少引用计数函数
    llvm::Function* decRefFunc = runtimeFuncs["py_decref"];
    
    if (!decRefFunc) {
        // 声明减少引用计数函数
        llvm::StructType* pyObjType = getPyObjectStructType();
        llvm::Type* pyObjPtrType = llvm::PointerType::get(pyObjType, 0);
        
        std::vector<llvm::Type*> argTypes = {pyObjPtrType};
        
        decRefFunc = getRuntimeFunction("py_decref", 
                                       llvm::Type::getVoidTy(context), 
                                       argTypes);
                                      
        runtimeFuncs["py_decref"] = decRefFunc;
    }
    
    // 确保对象是PyObject*类型
    llvm::Value* objPtr = ensurePyObjectPtr(obj);
    
    // 调用减少引用计数函数
    builder->CreateCall(decRefFunc, {objPtr});
    
    return obj;
}

// 复制对象
llvm::Value* ObjectRuntime::copyObject(llvm::Value* obj, ObjectType* type) {
    if (!obj || !type) return obj;
    
    // 获取对象复制函数
    llvm::Function* copyFunc = runtimeFuncs["py_object_copy"];
    
    if (!copyFunc) {
        // 声明对象复制函数
        llvm::StructType* pyObjType = getPyObjectStructType();
        llvm::Type* pyObjPtrType = llvm::PointerType::get(pyObjType, 0);
        
        std::vector<llvm::Type*> argTypes = {
            pyObjPtrType,                      // object
            llvm::Type::getInt32Ty(context)    // typeId
        };
        
        copyFunc = getRuntimeFunction("py_object_copy", 
                                     pyObjPtrType, 
                                     argTypes);
                                    
        runtimeFuncs["py_object_copy"] = copyFunc;
    }
    
    // 确保对象是PyObject*类型
    llvm::Value* objPtr = ensurePyObjectPtr(obj);
    
    // 使用统一的类型ID映射
    int typeId = mapTypeIdToRuntime(type);
    
    // 创建类型ID常量
    llvm::Value* typeIdValue = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(context), typeId);
    
    // 调用复制函数
    llvm::Value* copyResult = builder->CreateCall(copyFunc, {objPtr, typeIdValue}, "obj_copy");
    
    // 跟踪新创建的对象
    trackObject(copyResult);
    
    return copyResult;
}

// 跟踪对象
void ObjectRuntime::trackObject(llvm::Value* obj) {
    if (!obj) return;
    
    // 只跟踪指针类型对象
    if (obj->getType()->isPointerTy()) {
        trackedObjects.push_back(obj);
    }
}

// 清除追踪的对象
void ObjectRuntime::clearTrackedObjects() {
    // 对所有追踪的对象减少引用计数
    for (auto obj : trackedObjects) {
        decRef(obj);
    }
    
    // 清空追踪列表
    trackedObjects.clear();
}

// 设置函数清理
void ObjectRuntime::setupCleanupForFunction() {
    // 在函数返回前，对所有追踪的临时对象减少引用计数
    clearTrackedObjects();
}




// 准备返回值
llvm::Value* ObjectRuntime::prepareReturnValue(llvm::Value* value, ObjectType* type) {
    if (!value || !type) return value;
    
    // 使用ObjectLifecycleManager决定如何处理返回值
    bool needsCopy = ObjectLifecycleManager::needsCopy(
        type, 
        ObjectLifecycleManager::ObjectSource::LOCAL_VARIABLE,
        ObjectLifecycleManager::ObjectDestination::RETURN
    );
    
    if (needsCopy && type->isReference()) {
        // 对于引用类型，需要复制以确保返回后仍然有效
        return copyObject(value, type);
    }
    
    // 对于值类型或不需要复制的情况，直接返回
    return value;
}

// 准备参数
llvm::Value* ObjectRuntime::prepareArgument(llvm::Value* value, ObjectType* fromType, ObjectType* paramType) {
    if (!value || !fromType || !paramType) return value;
    
    // 如果类型相同，不需要额外处理
    if (fromType->getTypeId() == paramType->getTypeId()) {
        return value;
    }
    
    // 需要类型转换
    if (TypeOperationRegistry::getInstance().areTypesCompatible(
            fromType->getTypeId(), paramType->getTypeId())) {
        value = convertValue(value, fromType, paramType);
    }
    
    // 使用ObjectLifecycleManager决定如何处理参数
    bool needsIncRef = ObjectLifecycleManager::needsIncRef(
        paramType,
        ObjectLifecycleManager::ObjectSource::LOCAL_VARIABLE,
        ObjectLifecycleManager::ObjectDestination::PARAMETER
    );
    
    if (needsIncRef && paramType->isReference()) {
        // 增加引用计数，因为参数将在被调用函数中使用
        incRef(value);
    }
    
    return value;
}

// 补全 prepareAssignmentTarget 方法，为赋值目标做准备
/* llvm::Value* PyCodeGen::prepareAssignmentTarget(llvm::Value* value, ObjectType* targetType, ExprAST* expr) {
    if (!value || !targetType || !expr) return value;
    
    // 获取表达式来源
    ObjectLifecycleManager::ObjectSource source;
    
    switch (expr->kind()) {
        case ASTKind::NumberExpr:
        case ASTKind::StringExpr:
        case ASTKind::BoolExpr:
        case ASTKind::NoneExpr:
        case ASTKind::ListExpr:
            source = ObjectLifecycleManager::ObjectSource::LITERAL;
            break;
        case ASTKind::BinaryExpr:
            source = ObjectLifecycleManager::ObjectSource::BINARY_OP;
            break;
        case ASTKind::UnaryExpr:
            source = ObjectLifecycleManager::ObjectSource::UNARY_OP;
            break;
        case ASTKind::CallExpr:
            source = ObjectLifecycleManager::ObjectSource::FUNCTION_RETURN;
            break;
        case ASTKind::VariableExpr:
            source = ObjectLifecycleManager::ObjectSource::LOCAL_VARIABLE;
            break;
        case ASTKind::IndexExpr:
            source = ObjectLifecycleManager::ObjectSource::INDEX_ACCESS;
            break;
        default:
            source = ObjectLifecycleManager::ObjectSource::BINARY_OP;
            break;
    }
    
    // 检查是否需要复制对象
    if (ObjectLifecycleManager::needsCopy(targetType, source, 
                                       ObjectLifecycleManager::ObjectDestination::ASSIGNMENT)) {
        // 复制对象
        llvm::Function* copyFunc = getOrCreateExternalFunction(
            "py_object_copy",
            llvm::PointerType::get(getContext(), 0),
            {
                llvm::PointerType::get(getContext(), 0),
                llvm::Type::getInt32Ty(getContext())
            }
        );
        
        // 获取对象类型ID
        int typeId = ObjectLifecycleManager::getObjectTypeId(targetType);
        llvm::Value* typeIdValue = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(getContext()), typeId);
        
        return builder->CreateCall(copyFunc, {value, typeIdValue}, "copy");
    }
    else if (ObjectLifecycleManager::needsIncRef(targetType, source, 
                                             ObjectLifecycleManager::ObjectDestination::ASSIGNMENT)) {
        // 增加引用计数
        llvm::Function* incRefFunc = getOrCreateExternalFunction(
            "py_incref",
            llvm::Type::getVoidTy(getContext()),
            {llvm::PointerType::get(getContext(), 0)}
        );
        
        builder->CreateCall(incRefFunc, {value});
    }
    
    return value;
} */
//===----------------------------------------------------------------------===//
// 运行时类型信息
//===----------------------------------------------------------------------===//

// 获取PyObject结构类型
llvm::StructType* ObjectRuntime::getPyObjectStructType() {
    auto it = runtimeTypes.find("PyObject");
    if (it != runtimeTypes.end()) {
        return it->second;
    }
    
    // 创建PyObject结构类型
    llvm::StructType* pyObjType = llvm::StructType::create(context, "PyObject");
    
    // 定义结构体字段: {refcount, typeid}
    std::vector<llvm::Type*> fields = {
        llvm::Type::getInt32Ty(context),   // 引用计数
        llvm::Type::getInt32Ty(context)    // 类型ID
    };
    
    pyObjType->setBody(fields);
    
    // 存储在缓存中
    runtimeTypes["PyObject"] = pyObjType;
    
    return pyObjType;
}

// 获取列表结构类型
llvm::StructType* ObjectRuntime::getListStructType(ObjectType* elemType) {
    if (!elemType) {
        elemType = TypeRegistry::getInstance().getType("object");
    }
    
    std::string typeName = "PyListOf" + elemType->getName();
    
    auto it = runtimeTypes.find(typeName);
    if (it != runtimeTypes.end()) {
        return it->second;
    }
    
    // 创建列表结构类型
    llvm::StructType* listType = llvm::StructType::create(context, typeName);
    
    // 基础结构：{PyObject header, length, capacity, data_ptr}
    llvm::StructType* pyObjType = getPyObjectStructType();
    
    // 获取元素LLVM类型
    llvm::Type* elemLLVMType = elemType->getLLVMType(context);
    
    std::vector<llvm::Type*> fields = {
        pyObjType,                                 // PyObject头部
        llvm::Type::getInt32Ty(context),          // 列表长度
        llvm::Type::getInt32Ty(context),          // 列表容量
        llvm::PointerType::get(elemLLVMType, 0)   // 数据指针
    };
    
    listType->setBody(fields);
    
    // 存储在缓存中
    runtimeTypes[typeName] = listType;
    
    return listType;
}

// 获取字典结构类型
llvm::StructType* ObjectRuntime::getDictStructType() {
    auto it = runtimeTypes.find("PyDictObject");
    if (it != runtimeTypes.end()) {
        return it->second;
    }
    
    // 创建字典结构类型
    llvm::StructType* dictType = llvm::StructType::create(context, "PyDictObject");
    
    // 基础结构：{PyObject header, count, capacity, entries}
    llvm::StructType* pyObjType = getPyObjectStructType();
    
    std::vector<llvm::Type*> fields = {
        pyObjType,                                 // PyObject头部
        llvm::Type::getInt32Ty(context),          // 条目数量
        llvm::Type::getInt32Ty(context),          // 容量
        llvm::PointerType::get(                   // 哈希表指针
            llvm::Type::getInt8Ty(context), 0)
    };
    
    dictType->setBody(fields);
    
    // 存储在缓存中
    runtimeTypes["PyDictObject"] = dictType;
    
    return dictType;
}

// 映射类型ID到运行时
int ObjectRuntime::mapTypeIdToRuntime(const ObjectType* type) {
    if (!type) return PY_TYPE_NONE;
    
    // 使用TypeIDs.h中定义的映射函数
    return mapToRuntimeTypeId(type->getTypeId());
}

// 从对象获取类型ID
llvm::Value* ObjectRuntime::getTypeIdFromObject(llvm::Value* obj) {
    if (!obj) return nullptr;
    
    // 确保对象是PyObject*类型
    llvm::Value* objPtr = ensurePyObjectPtr(obj);
    
    // 获取对象的类型ID字段（第二个字段，索引为1）
    // PyObject 结构为 {refcount, typeid}
    std::vector<llvm::Value*> indices = {
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 1)
    };
    
    // 创建 GEP 指令获取 typeID 字段地址
    llvm::Value* typeIDPtr = builder->CreateGEP(
        getPyObjectStructType(),
        objPtr,
        indices,
        "typeid_ptr"
    );
    
    // 加载类型ID值
    return builder->CreateLoad(
        llvm::Type::getInt32Ty(context),
        typeIDPtr, 
        "typeid_value"
    );
}

// 获取类型ID对应的名称
std::string ObjectRuntime::getTypeNameForId(int typeId) {
    // 使用基本类型ID（去除高位标识）
    int baseTypeId = getBaseTypeId(typeId);
    
    switch (baseTypeId) {
        case PY_TYPE_NONE:
            return "None";
        case PY_TYPE_INT:
            return "int";
        case PY_TYPE_DOUBLE:
            return "float";
        case PY_TYPE_BOOL:
            return "bool";
        case PY_TYPE_STRING:
            return "str";
        case PY_TYPE_LIST:
            return "list";
        case PY_TYPE_DICT:
            return "dict";
        default:
            if (typeId >= PY_TYPE_LIST_BASE && typeId < PY_TYPE_DICT_BASE) {
                return "list";
            } else if (typeId >= PY_TYPE_DICT_BASE && typeId < PY_TYPE_FUNC_BASE) {
                return "dict";
            } else if (typeId >= PY_TYPE_FUNC_BASE) {
                return "function";
            }
            return "unknown";
    }
}

// 确保值是PyObject指针类型
llvm::Value* ObjectRuntime::ensurePyObjectPtr(llvm::Value* value) {
    if (!value) return nullptr;
    
    // 如果已经是指针类型，直接返回
    if (value->getType()->isPointerTy()) {
        // 检查是否需要位类型转换
        llvm::PointerType* expectedType = llvm::PointerType::get(
            getPyObjectStructType(), 0);
            
        if (value->getType() != expectedType) {
            // 进行位类型转换，不改变内存布局
            return builder->CreateBitCast(value, expectedType, "obj_cast");
        }
        return value;
    }
    
    // 如果是基本类型，需要创建相应的对象
    if (value->getType()->isIntegerTy(32)) {
        // 整数类型
        return createIntObject(value);
    } else if (value->getType()->isDoubleTy()) {
        // 浮点类型
        return createDoubleObject(value);
    } else if (value->getType()->isIntegerTy(1)) {
        // 布尔类型
        return createBoolObject(value);
    }
    
    // 不支持的类型，发出警告并返回null
    std::cerr << "Warning: Cannot convert value of unknown type to PyObject*" << std::endl;
    return llvm::ConstantPointerNull::get(
        llvm::PointerType::get(getPyObjectStructType(), 0));
}

// 生成类型检查代码
llvm::Value* ObjectRuntime::generateTypeCheck(llvm::Value* obj, int expectedTypeId) {
    if (!obj) return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
    
    // 获取对象的类型ID
    llvm::Value* actualTypeId = getTypeIdFromObject(obj);
    if (!actualTypeId) {
        return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
    }
    
    // 创建类型ID常量
    llvm::Value* expectedTypeIdValue = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(context), expectedTypeId);
    
    // 比较类型ID是否匹配
    return builder->CreateICmpEQ(actualTypeId, expectedTypeIdValue, "type_check");
}

// 获取运行时函数
llvm::Function* ObjectRuntime::getRuntimeFunction(
    const std::string& name,
    llvm::Type* returnType,
    std::vector<llvm::Type*> argTypes) {
    
    // 检查函数是否已经声明
    llvm::Function* func = module->getFunction(name);
    if (func) {
        return func;
    }
    
    // 创建函数类型
    llvm::FunctionType* funcType = llvm::FunctionType::get(
        returnType,
        argTypes,
        false); // 不是可变参数函数
    
    // 声明函数
    func = llvm::Function::Create(
        funcType,
        llvm::Function::ExternalLinkage,
        name,
        module
    );
    
    // 返回新声明的函数
    return func;
}

// 创建运行时类型
void ObjectRuntime::createRuntimeTypes() {
    // 基本对象类型已在 getPyObjectStructType 中创建
    
    // 确保基本结构类型已创建
    getPyObjectStructType();
    
    // 创建列表和字典类型
    getListStructType(TypeRegistry::getInstance().getType("object"));
    getDictStructType();
    
    // 可以在这里添加其他需要预创建的类型
}

// 声明运行时函数
void ObjectRuntime::declareRuntimeFunctions() {
    // PyObject 指针类型
    llvm::Type* pyObjPtrType = llvm::PointerType::get(getPyObjectStructType(), 0);
    
    // 基本类型
    llvm::Type* voidTy = llvm::Type::getVoidTy(context);
    llvm::Type* boolTy = llvm::Type::getInt1Ty(context);
    llvm::Type* intTy = llvm::Type::getInt32Ty(context);
    llvm::Type* doubleTy = llvm::Type::getDoubleTy(context);
    llvm::Type* charPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
    
    // 对象创建函数
    getRuntimeFunction("py_create_int", pyObjPtrType, {intTy});
    getRuntimeFunction("py_create_double", pyObjPtrType, {doubleTy});
    getRuntimeFunction("py_create_bool", pyObjPtrType, {boolTy});
    getRuntimeFunction("py_create_string", pyObjPtrType, {charPtrTy});
    getRuntimeFunction("py_create_list", pyObjPtrType, {intTy, intTy});
    getRuntimeFunction("py_create_dict", pyObjPtrType, {intTy, intTy});
    
    // 对象操作函数
    getRuntimeFunction("py_object_add", pyObjPtrType, {pyObjPtrType, pyObjPtrType});
    getRuntimeFunction("py_object_subtract", pyObjPtrType, {pyObjPtrType, pyObjPtrType});
    getRuntimeFunction("py_object_multiply", pyObjPtrType, {pyObjPtrType, pyObjPtrType});
    getRuntimeFunction("py_object_divide", pyObjPtrType, {pyObjPtrType, pyObjPtrType});
    getRuntimeFunction("py_object_modulo", pyObjPtrType, {pyObjPtrType, pyObjPtrType});
    getRuntimeFunction("py_object_negate", pyObjPtrType, {pyObjPtrType});
    getRuntimeFunction("py_object_not", pyObjPtrType, {pyObjPtrType});
    
    // 类型转换函数
    getRuntimeFunction("py_convert_int_to_double", pyObjPtrType, {pyObjPtrType});
    getRuntimeFunction("py_convert_double_to_int", pyObjPtrType, {pyObjPtrType});
    getRuntimeFunction("py_convert_to_bool", pyObjPtrType, {pyObjPtrType});
    getRuntimeFunction("py_convert_to_string", pyObjPtrType, {pyObjPtrType});
    
    // 容器操作函数
    getRuntimeFunction("py_list_get_item", pyObjPtrType, {pyObjPtrType, intTy});
    getRuntimeFunction("py_list_set_item", voidTy, {pyObjPtrType, intTy, pyObjPtrType});
    getRuntimeFunction("py_list_append", pyObjPtrType, {pyObjPtrType, pyObjPtrType});
    getRuntimeFunction("py_list_len", intTy, {pyObjPtrType});
    getRuntimeFunction("py_dict_get_item", pyObjPtrType, {pyObjPtrType, pyObjPtrType});
    getRuntimeFunction("py_dict_set_item", voidTy, {pyObjPtrType, pyObjPtrType, pyObjPtrType});
    getRuntimeFunction("py_dict_keys", pyObjPtrType, {pyObjPtrType});
    
    // 引用计数管理函数
    getRuntimeFunction("py_incref", voidTy, {pyObjPtrType});
    getRuntimeFunction("py_decref", voidTy, {pyObjPtrType});
    
    // 对象生命周期管理函数
    getRuntimeFunction("py_object_copy", pyObjPtrType, {pyObjPtrType, intTy});
    
    // 类型检查和比较函数
    getRuntimeFunction("py_check_type", boolTy, {pyObjPtrType, intTy});
    getRuntimeFunction("py_compare_eq", boolTy, {pyObjPtrType, pyObjPtrType});
    getRuntimeFunction("py_compare_ne", boolTy, {pyObjPtrType, pyObjPtrType});
    getRuntimeFunction("py_compare_lt", boolTy, {pyObjPtrType, pyObjPtrType});
    getRuntimeFunction("py_compare_le", boolTy, {pyObjPtrType, pyObjPtrType});
    getRuntimeFunction("py_compare_gt", boolTy, {pyObjPtrType, pyObjPtrType});
    getRuntimeFunction("py_compare_ge", boolTy, {pyObjPtrType, pyObjPtrType});
    
    // 值提取函数
    getRuntimeFunction("py_extract_int", intTy, {pyObjPtrType});
    getRuntimeFunction("py_extract_double", doubleTy, {pyObjPtrType});
    getRuntimeFunction("py_extract_bool", boolTy, {pyObjPtrType});
}

// 注册类型操作
void ObjectRuntime::registerTypeOperations() {
    // 获取类型操作注册表的实例
    auto& registry = TypeOperationRegistry::getInstance();
    
    // 注册特殊的、需要自定义实现的操作
    
    // 例如，注册直接使用LLVM指令执行的整数加法
    registry.registerBinaryOp(
        '+', PY_TYPE_INT, PY_TYPE_INT, PY_TYPE_INT, 
        "py_object_add", false,
        [this](PyCodeGen& gen, llvm::Value* left, llvm::Value* right) -> llvm::Value* {
            // 确保操作数是整数类型
            if (left->getType()->isPointerTy()) {
                llvm::Function* extractFunc = getRuntimeFunction(
                    "py_extract_int", 
                    llvm::Type::getInt32Ty(context), 
                    {llvm::PointerType::get(context, 0)}
                );
                left = builder->CreateCall(extractFunc, {left}, "extract_left");
            }
            
            if (right->getType()->isPointerTy()) {
                llvm::Function* extractFunc = getRuntimeFunction(
                    "py_extract_int", 
                    llvm::Type::getInt32Ty(context), 
                    {llvm::PointerType::get(context, 0)}
                );
                right = builder->CreateCall(extractFunc, {right}, "extract_right");
            }
            
            // 执行LLVM加法指令
            llvm::Value* result = builder->CreateAdd(left, right, "int_add");
            
            // 创建新的整数对象
            return createIntObject(result);
        }
    );
    
    // 可以注册更多自定义操作...
}
}
    
 