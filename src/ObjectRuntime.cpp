#include "ObjectRuntime.h"
#include <string>
#include "TypeIDs.h"
namespace llvmpy {

/* ObjectRuntime::ObjectRuntime(llvm::Module* mod, llvm::IRBuilder<>* b)
    : module(mod), builder(b), context(mod->getContext()) {
} */

void ObjectRuntime::initialize() {
    // 创建运行时类型
    createRuntimeTypes();
    
    // 声明运行时函数
    declareRuntimeFunctions();
}

llvm::Value* ObjectRuntime::createList(llvm::Value* size, ObjectType* elemType) {
    // 获取列表创建函数
    std::string funcName = "py_create_list";
    llvm::Function* createFunc = runtimeFuncs[funcName];
    
    if (!createFunc) {
        // 声明创建列表的运行时函数
        llvm::StructType* listType = getListStructType(elemType);
        llvm::Type* listPtrType = llvm::PointerType::get(listType, 0);
        
        std::vector<llvm::Type*> argTypes = {
            llvm::Type::getInt32Ty(context),  // size
            llvm::Type::getInt32Ty(context)   // elemTypeId
        };
        
        createFunc = getRuntimeFunction("py_create_list", 
                                      listPtrType, 
                                      argTypes);
        
        runtimeFuncs["py_create_list"] = createFunc;
    }
    
    // 添加元素类型ID
    llvm::Value* elemTypeId = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(context), 
        elemType->getTypeId());
        
    return builder->CreateCall(createFunc, {size, elemTypeId}, "new_list");
}

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
            
        setListElement(list, index, values[i]);
    }
    
    return list;
}
int ObjectRuntime::mapTypeIdToRuntime(const ObjectType* type) {
    if (!type) return PY_TYPE_NONE;
    
    // 使用统一的映射函数
    return mapToRuntimeTypeId(type->getTypeId());
}
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
    llvm::Value* listObj = builder->CreateBitCast(
        list, 
        llvm::PointerType::get(getPyObjectStructType(), 0),
        "list_as_pyobj");
    
    // 调用获取元素函数
    return builder->CreateCall(getItemFunc, {listObj, index}, "list_elem");
}

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
    llvm::Value* listObj = builder->CreateBitCast(
        list, 
        llvm::PointerType::get(getPyObjectStructType(), 0),
        "list_as_pyobj");
        
    llvm::Value* valueObj = builder->CreateBitCast(
        value, 
        llvm::PointerType::get(getPyObjectStructType(), 0),
        "value_as_pyobj");
    
    // 调用设置元素函数
    builder->CreateCall(setItemFunc, {listObj, index, valueObj});
}

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
    llvm::Value* listObj = builder->CreateBitCast(
        list, 
        llvm::PointerType::get(getPyObjectStructType(), 0),
        "list_as_pyobj");
    
    // 调用获取长度函数
    return builder->CreateCall(lenFunc, {listObj}, "list_len");
}

llvm::Value* ObjectRuntime::copyObject(llvm::Value* obj, ObjectType* type) {
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
    llvm::Value* objPtr = builder->CreateBitCast(
        obj, 
        llvm::PointerType::get(getPyObjectStructType(), 0),
        "obj_as_pyobj");
        
    // 获取类型ID
    llvm::Value* typeId = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(context), 
        type->getTypeId());
    
    // 调用复制函数
    return builder->CreateCall(copyFunc, {objPtr, typeId}, "obj_copy");
}

llvm::Value* ObjectRuntime::incRef(llvm::Value* obj) {
    // 获取引用计数增加函数
    llvm::Function* incRefFunc = runtimeFuncs["py_incref"];
    
    if (!incRefFunc) {
        // 声明引用计数增加函数
        llvm::StructType* pyObjType = getPyObjectStructType();
        llvm::Type* pyObjPtrType = llvm::PointerType::get(pyObjType, 0);
        
        std::vector<llvm::Type*> argTypes = {pyObjPtrType};
        
        incRefFunc = getRuntimeFunction("py_incref", 
                                      llvm::Type::getVoidTy(context), 
                                      argTypes);
                                      
        runtimeFuncs["py_incref"] = incRefFunc;
    }
    
    // 确保对象是PyObject*类型
    llvm::Value* objPtr = builder->CreateBitCast(
        obj, 
        llvm::PointerType::get(getPyObjectStructType(), 0),
        "obj_as_pyobj");
    
    // 调用增加引用计数函数
    return builder->CreateCall(incRefFunc, {objPtr});
}

llvm::Value* ObjectRuntime::decRef(llvm::Value* obj) {
    // 获取引用计数减少函数
    llvm::Function* decRefFunc = runtimeFuncs["py_decref"];
    
    if (!decRefFunc) {
        // 声明引用计数减少函数
        llvm::StructType* pyObjType = getPyObjectStructType();
        llvm::Type* pyObjPtrType = llvm::PointerType::get(pyObjType, 0);
        
        std::vector<llvm::Type*> argTypes = {pyObjPtrType};
        
        decRefFunc = getRuntimeFunction("py_decref", 
                                      llvm::Type::getVoidTy(context), 
                                      argTypes);
                                      
        runtimeFuncs["py_decref"] = decRefFunc;
    }
    
    // 确保对象是PyObject*类型
    llvm::Value* objPtr = builder->CreateBitCast(
        obj, 
        llvm::PointerType::get(getPyObjectStructType(), 0),
        "obj_as_pyobj");
    
    // 调用减少引用计数函数
    return builder->CreateCall(decRefFunc, {objPtr});
}

llvm::Value* ObjectRuntime::prepareReturnValue(llvm::Value* value, ObjectType* type) {
    // 对于引用类型，增加引用计数或创建副本
    if (type->isReference() || type->hasFeature("mutable")) {
        // 创建对象的副本，确保函数返回后对象仍然有效
        return copyObject(value, type);
    }
    
    // 对于值类型，直接返回
    return value;
}

void ObjectRuntime::setupCleanupForFunction() {
    // 在函数返回前，对所有追踪的临时对象减少引用计数
    for (auto obj : trackedObjects) {
        decRef(obj);
    }
    
    // 清空追踪列表
    trackedObjects.clear();
}

// 获取或创建PyObject结构类型
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

// 获取或声明运行时函数
llvm::Function* ObjectRuntime::getRuntimeFunction(
    const std::string& name,
    llvm::Type* returnType,
    std::vector<llvm::Type*> argTypes) {
    
    // 检查函数是否已存在
    llvm::Function* func = module->getFunction(name);
    if (func) {
        return func;
    }
    
    // 创建函数类型
    llvm::FunctionType* funcType = llvm::FunctionType::get(
        returnType, argTypes, false);
    
    // 创建函数声明
    func = llvm::Function::Create(
        funcType,
        llvm::Function::ExternalLinkage,
        name,
        module);
    
    return func;
}
// 获取字典项
llvm::Value* ObjectRuntime::getDictItem(llvm::Value* dict, llvm::Value* key) {
    if (!dict || !key) return nullptr;
    
    // 获取字典项获取函数
    llvm::Function* getDictItemFunc = getRuntimeFunction(
        "py_dict_get_item",
        llvm::PointerType::get(context, 0),
        {llvm::PointerType::get(context, 0), llvm::PointerType::get(context, 0)}
    );
    
    // 调用函数获取字典项
    return builder->CreateCall(getDictItemFunc, {dict, key}, "dict_item");
}

// 设置字典项
void ObjectRuntime::setDictItem(llvm::Value* dict, llvm::Value* key, llvm::Value* value) {
    if (!dict || !key || !value) return;
    
    // 获取字典项设置函数
    llvm::Function* setDictItemFunc = getRuntimeFunction(
        "py_dict_set_item",
        llvm::Type::getVoidTy(context),
        {llvm::PointerType::get(context, 0), 
         llvm::PointerType::get(context, 0),
         llvm::PointerType::get(context, 0)}
    );
    
    // 调用函数设置字典项
    builder->CreateCall(setDictItemFunc, {dict, key, value});
}
// 创建运行时类型
void ObjectRuntime::createRuntimeTypes() {
    // 创建基本的PyObject类型
    getPyObjectStructType();
    
    // 创建字典类型结构
    llvm::StructType* dictType = llvm::StructType::create(context, "PyDictObject");
    
    std::vector<llvm::Type*> dictFields = {
        getPyObjectStructType(),               // PyObject头部
        llvm::Type::getInt32Ty(context),       // 条目数量
        llvm::Type::getInt32Ty(context),       // 容量
        llvm::PointerType::get(               
            llvm::Type::getInt8Ty(context), 0) // 哈希表指针(实际更复杂)
    };
    
    dictType->setBody(dictFields);
    runtimeTypes["PyDictObject"] = dictType;
}

// 添加字典创建方法
llvm::Value* ObjectRuntime::createDict(ObjectType* keyType, ObjectType* valueType) {
    // 获取字典创建函数
    llvm::Function* createFunc = runtimeFuncs["py_create_dict"];
    
    if (!createFunc) {
        // 声明字典创建函数
        llvm::Type* dictPtrType = llvm::PointerType::get(
            getPyObjectStructType(), 0);
            
        std::vector<llvm::Type*> argTypes = {
            llvm::Type::getInt32Ty(context),  // initialCapacity
            llvm::Type::getInt32Ty(context)   // keyTypeId
        };
        
        createFunc = getRuntimeFunction("py_create_dict", 
                                       dictPtrType, 
                                       argTypes);
                                       
        runtimeFuncs["py_create_dict"] = createFunc;
    }
    
    // 初始容量为8
    llvm::Value* capacity = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(context), 8);
        
    // 获取键类型ID
    llvm::Value* keyTypeId = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(context), 
        mapTypeIdToRuntime(keyType));
        
    return builder->CreateCall(createFunc, {capacity, keyTypeId}, "new_dict");
}

// 添加向列表追加元素方法
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
    llvm::Value* listObj = builder->CreateBitCast(
        list, 
        llvm::PointerType::get(getPyObjectStructType(), 0),
        "list_as_pyobj");
        
    llvm::Value* valueObj = builder->CreateBitCast(
        value, 
        llvm::PointerType::get(getPyObjectStructType(), 0),
        "value_as_pyobj");
    
    // 调用追加函数
    return builder->CreateCall(appendFunc, {listObj, valueObj}, "append_result");
}

// 添加对象类型转换方法
llvm::Value* ObjectRuntime::convertObject(
    llvm::Value* obj, ObjectType* fromType, ObjectType* toType) {
    
    if (TypeFeatureChecker::isNumeric(fromType) && 
        TypeFeatureChecker::isNumeric(toType)) {
        
        // 如果是从int转到double
        if (fromType->getName() == "int" && toType->getName() == "double") {
            llvm::Function* convertFunc = runtimeFuncs["py_convert_int_to_double"];
            
            if (!convertFunc) {
                // 声明int到double的转换函数
                llvm::Type* pyObjPtrType = llvm::PointerType::get(
                    getPyObjectStructType(), 0);
                    
                std::vector<llvm::Type*> argTypes = {pyObjPtrType};
                
                convertFunc = getRuntimeFunction("py_convert_int_to_double", 
                                              pyObjPtrType, 
                                              argTypes);
                                              
                runtimeFuncs["py_convert_int_to_double"] = convertFunc;
            }
            
            // 确保对象是PyObject*类型
            llvm::Value* objAsPtr = builder->CreateBitCast(
                obj, 
                llvm::PointerType::get(getPyObjectStructType(), 0),
                "obj_as_pyobj");
                
            return builder->CreateCall(convertFunc, {objAsPtr}, "int_to_double");
        }
        
        // 如果是从double转到int
        if (fromType->getName() == "double" && toType->getName() == "int") {
            llvm::Function* convertFunc = runtimeFuncs["py_convert_double_to_int"];
            
            if (!convertFunc) {
                // 声明double到int的转换函数
                llvm::Type* pyObjPtrType = llvm::PointerType::get(
                    getPyObjectStructType(), 0);
                    
                std::vector<llvm::Type*> argTypes = {pyObjPtrType};
                
                convertFunc = getRuntimeFunction("py_convert_double_to_int", 
                                              pyObjPtrType, 
                                              argTypes);
                                              
                runtimeFuncs["py_convert_double_to_int"] = convertFunc;
            }
            
            // 确保对象是PyObject*类型
            llvm::Value* objAsPtr = builder->CreateBitCast(
                obj, 
                llvm::PointerType::get(getPyObjectStructType(), 0),
                "obj_as_pyobj");
                
            return builder->CreateCall(convertFunc, {objAsPtr}, "double_to_int");
        }
    }
    
    // 其他类型转换... TODO
    // 如果不能直接转换，返回原对象
    return obj;
}


// 类型检查方法实现
llvm::Value* ObjectRuntime::checkType(llvm::Value* obj, ObjectType* expectedType) {
    llvm::Type* i1Ty = llvm::Type::getInt1Ty(context);
    llvm::Type* i32Ty = llvm::Type::getInt32Ty(context);
    llvm::Type* objPtrTy = llvm::PointerType::get(getPyObjectStructType(), 0);
    
    // 确保对象是PyObject*类型
    llvm::Value* objPtr = builder->CreateBitCast(obj, objPtrTy, "obj_ptr");
    
    // 从PyObject头获取类型ID
    llvm::Value* typeIdPtr = builder->CreateStructGEP(getPyObjectStructType(), objPtr, 1, "type_id_ptr");
    llvm::Value* actualTypeId = builder->CreateLoad(i32Ty, typeIdPtr, "actual_type_id");
    
    // 创建期望的类型ID
    llvm::Value* expectedTypeId = llvm::ConstantInt::get(
        i32Ty, mapTypeIdToRuntime(expectedType));
    
    // 比较类型ID
    return builder->CreateICmpEQ(actualTypeId, expectedTypeId, "type_check");
}

// 将未跟踪的对象添加到跟踪列表
void ObjectRuntime::trackObject(llvm::Value* obj) {
    trackedObjects.push_back(obj);
}

/* // 注册类型特性检查函数初始化
void ObjectRuntime::registerFeatureChecks() {
    // 注册通用特性检查
    TypeFeatureChecker::registerFeatureCheck(
        "container", 
        [](const ObjectType* type) {
            return dynamic_cast<const ListType*>(type) || 
                   dynamic_cast<const DictType*>(type);
        });
    
    TypeFeatureChecker::registerFeatureCheck(
        "reference", 
        [](const ObjectType* type) {
            return type->getCategory() == ObjectType::Reference || 
                   type->getCategory() == ObjectType::Container || 
                   type->getName() == "string";
        });
} */



// 声明所有运行时函数
void ObjectRuntime::declareRuntimeFunctions() {
    // 列表创建和操作函数已在各个方法中按需创建
    
    // 获取常用类型
    llvm::StructType* pyObjType = getPyObjectStructType();
    llvm::Type* pyObjPtrType = llvm::PointerType::get(pyObjType, 0);
    
    // 创建字典函数
    {
        std::vector<llvm::Type*> argTypes = {
            llvm::Type::getInt32Ty(context),  // initialCapacity
            llvm::Type::getInt32Ty(context)   // keyTypeId
        };
        
        runtimeFuncs["py_create_dict"] = getRuntimeFunction(
            "py_create_dict", pyObjPtrType, argTypes);
    }
    
    // 字典操作函数
    {
        std::vector<llvm::Type*> argTypes = {
            pyObjPtrType,  // dict
            pyObjPtrType   // key
        };
        
        runtimeFuncs["py_dict_get_item"] = getRuntimeFunction(
            "py_dict_get_item", pyObjPtrType, argTypes);
    }
    
    {
        std::vector<llvm::Type*> argTypes = {
            pyObjPtrType,  // dict
            pyObjPtrType,  // key
            pyObjPtrType   // value
        };
        
        runtimeFuncs["py_dict_set_item"] = getRuntimeFunction(
            "py_dict_set_item", llvm::Type::getVoidTy(context), argTypes);
    }
}

} // namespace llvmpy