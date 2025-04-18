#include "CodeGen/CodeGenRuntime.h"
#include "CodeGen/CodeGenBase.h"
#include "CodeGen/CodeGenType.h"
#include "ObjectType.h"
#include "CodeGen/PyCodeGen.h"
#include "TypeOperations.h"
#include "ObjectLifecycle.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/MDBuilder.h>
#include <iostream>

namespace llvmpy
{

//===----------------------------------------------------------------------===//
// 构造函数与初始化
//===----------------------------------------------------------------------===//

/* CodeGenRuntime::CodeGenRuntime(CodeGenBase& cg, ObjectRuntime* rt)
    : codeGen(cg), runtime(rt) {
} */

//===----------------------------------------------------------------------===//
// 运行时函数管理
//===----------------------------------------------------------------------===//



llvm::Value* CodeGenRuntime::createFunctionObject(llvm::Function* llvmFunc, ObjectType* funcObjectType) {
    // --- 输入验证 ---
    if (!llvmFunc) {
        codeGen.logError("Cannot create function object from null LLVM function.");
        return nullptr;
    }
    // 确保传入的是 FunctionType
    if (!funcObjectType || funcObjectType->getCategory() != ObjectType::Function) {
        codeGen.logError("Cannot create function object: provided ObjectType is null or not a FunctionType.");
        return nullptr;
    }

    auto& context = codeGen.getContext();
    auto& builder = codeGen.getBuilder();

    // --- 1. 定义 C 运行时函数 py_create_function 的 LLVM 签名 ---
    // 签名: PyObject* py_create_function(void* func_ptr, int signature_type_id)
    llvm::Type* pyObjectType = llvm::PointerType::get(context, 0); // PyObject*
    llvm::Type* voidPtrType = llvm::PointerType::getUnqual(context); // void*
    llvm::Type* int32Type = llvm::Type::getInt32Ty(context);       // int

    // --- 2. 获取 C 运行时函数的 LLVM 类型 ---
    llvm::FunctionType* runtimeFuncType = llvm::FunctionType::get(
        pyObjectType,       // 返回类型: PyObject*
        {voidPtrType, int32Type}, // 参数类型: void*, int
        false               // 是否可变参数: 否
    );

    // --- 3. 获取 C 运行时函数的 LLVM 声明 ---
    // 使用 getRuntimeFunction 获取或创建外部函数声明
    llvm::Function* pyCreateFunc = getRuntimeFunction(
        "py_create_function", // C 函数的名称
        runtimeFuncType->getReturnType(),
        {runtimeFuncType->getParamType(0), runtimeFuncType->getParamType(1)}
    );

    if (!pyCreateFunc) {
        // getRuntimeFunction 内部应该已经处理了查找和创建
        // 如果仍然失败，可能是链接问题或名称冲突
        codeGen.logError("Failed to get or create runtime function 'py_create_function'. Linker error likely.");
        return nullptr;
    }

    // --- 4. 准备调用参数 ---
    // 参数1: 将 llvm::Function* 转换为 void*
    llvm::Value* funcPtrVoid = builder.CreateBitCast(llvmFunc, voidPtrType, "func_ptr_void");

    // 参数2: 从 ObjectType 获取类型 ID
    // 这里依赖 ObjectType::getTypeId() 返回代表函数签名的 int ID
    int signatureTypeId = funcObjectType->getTypeId();
    llvm::Value* typeIdVal = llvm::ConstantInt::get(int32Type, signatureTypeId, /*isSigned=*/true);

    // --- 5. 创建对 C 运行时函数的调用 ---
    llvm::Value* pyFuncObj = builder.CreateCall(pyCreateFunc, {funcPtrVoid, typeIdVal}, "new_py_func_obj");

    // --- 6. 返回创建的 Python 函数对象 (llvm::Value*) ---
    // 注意：py_create_function 返回的对象引用计数为 1，
    // CodeGenStmt::handleFunctionDefStmt 中 setVariable 可能需要处理 incRef (如果符号表持有引用)。
    return pyFuncObj;
}

llvm::Function* CodeGenRuntime::getRuntimeFunction(
        const std::string& name,
        llvm::Type* returnType,
        std::vector<llvm::Type*> paramTypes)
{
    // 检查函数是否已缓存
    auto it = runtimeFunctions.find(name);
    if (it != runtimeFunctions.end())
    {
        return it->second;
    }

    // 检查模块中是否已存在此函数
    llvm::Function* func = codeGen.getModule()->getFunction(name);
    if (func)
    {
        runtimeFunctions[name] = func;
        return func;
    }

    // 创建新的函数声明
    llvm::FunctionType* funcType = llvm::FunctionType::get(returnType, paramTypes, false);
    func = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            name,
            codeGen.getModule());

    // 缓存并返回
    runtimeFunctions[name] = func;
    return func;
}

//===----------------------------------------------------------------------===//
// 对象生命周期管理
//===----------------------------------------------------------------------===//

void CodeGenRuntime::markObjectSource(
        llvm::Value* obj,
        ObjectLifecycleManager::ObjectSource source)
{
    if (obj)
    {
        // 记录对象的来源信息，用于生命周期管理
        objectSources[obj] = source;
    }
}

ObjectLifecycleManager::ObjectSource CodeGenRuntime::getObjectSource(llvm::Value* obj)
{
    auto it = objectSources.find(obj);
    if (it != objectSources.end())
    {
        return it->second;
    }

    // 默认认为是临时对象
    return ObjectLifecycleManager::ObjectSource::TEMPORARY;
}

ObjectLifecycleManager::ObjectSource CodeGenRuntime::determineExprSource(const ExprAST* expr)
{
    if (!expr)
    {
        return ObjectLifecycleManager::ObjectSource::TEMPORARY;
    }

    switch (expr->kind())
    {
        case ASTKind::NumberExpr:
        case ASTKind::StringExpr:
        case ASTKind::BoolExpr:
        case ASTKind::NoneExpr:
        case ASTKind::ListExpr:
            return ObjectLifecycleManager::ObjectSource::LITERAL;

        case ASTKind::BinaryExpr:
            return ObjectLifecycleManager::ObjectSource::BINARY_OP;

        case ASTKind::UnaryExpr:
            return ObjectLifecycleManager::ObjectSource::UNARY_OP;

        case ASTKind::CallExpr:
            return ObjectLifecycleManager::ObjectSource::FUNCTION_RETURN;

        case ASTKind::VariableExpr:
            return ObjectLifecycleManager::ObjectSource::LOCAL_VARIABLE;

        case ASTKind::IndexExpr:
            return ObjectLifecycleManager::ObjectSource::INDEX_ACCESS;

        default:
            return ObjectLifecycleManager::ObjectSource::TEMPORARY;
    }
}

//===----------------------------------------------------------------------===//
// 对象操作
//===----------------------------------------------------------------------===//

llvm::Value* CodeGenRuntime::incRef(llvm::Value* obj)
{
    // 创建对象引用计数增加调用
    if (!obj)
    {
        return nullptr;
    }

    // 获取incref函数
    llvm::Function* incRefFunc = getRuntimeFunction(
            "py_incref",
            llvm::Type::getVoidTy(codeGen.getContext()),
            {llvm::PointerType::get(codeGen.getContext(), 0)});

    // 调用incref
    return codeGen.getBuilder().CreateCall(incRefFunc, {obj});
}

llvm::Value* CodeGenRuntime::decRef(llvm::Value* obj)
{
    // 创建对象引用计数减少调用
    if (!obj)
    {
        return nullptr;
    }

    // 获取decref函数
    llvm::Function* decRefFunc = getRuntimeFunction(
            "py_decref",
            llvm::Type::getVoidTy(codeGen.getContext()),
            {llvm::PointerType::get(codeGen.getContext(), 0)});

    // 调用decref
    return codeGen.getBuilder().CreateCall(decRefFunc, {obj});
}

llvm::Value* CodeGenRuntime::copyObject(llvm::Value* obj, std::shared_ptr<PyType> type)
{
    // 创建对象的深拷贝
    if (!obj || !type)
    {
        return obj;
    }

    // 获取对象类型ID
    int typeId = type->getObjectType()->getTypeId();

    // 获取copy函数
    llvm::Function* copyFunc = getRuntimeFunction(
            "py_object_copy",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {llvm::PointerType::get(codeGen.getContext(), 0),
             llvm::Type::getInt32Ty(codeGen.getContext())});

    // 创建typeId常量
    llvm::Value* typeIdValue = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(codeGen.getContext()), typeId);

    // 调用copy函数
    return codeGen.getBuilder().CreateCall(copyFunc, {obj, typeIdValue}, "obj_copy");
}

//===----------------------------------------------------------------------===//
// 类型转换与准备
//===----------------------------------------------------------------------===//

// 在 CodeGenRuntime 类中添加方法声明
llvm::Value* CodeGenRuntime::createList(llvm::Value* size, ObjectType* elemType)
{
/*     if (runtime)
    {
        return runtime->createList(size, elemType);
    } 不在需要委托*/

    // 如果没有runtime，使用内置实现
    llvm::Function* createListFunc = getRuntimeFunction(
            "py_create_list",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {llvm::Type::getInt32Ty(codeGen.getContext()),
             llvm::Type::getInt32Ty(codeGen.getContext())});

    // 获取元素类型ID
    int elemTypeId = OperationCodeGenerator::getTypeId(elemType);
    llvm::Value* elemTypeIdValue = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(codeGen.getContext()),
            elemTypeId);

    return codeGen.getBuilder().CreateCall(
            createListFunc, {size, elemTypeIdValue}, "list_obj");
}

llvm::Value* CodeGenRuntime::getListElement(llvm::Value* list, llvm::Value* index)
{
    /* if (runtime)
    {
        return runtime->getListElement(list, index);
    } */

    // 如果没有runtime，使用内置实现
    llvm::Function* getItemFunc = getRuntimeFunction(
            "py_list_get_item",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {llvm::PointerType::get(codeGen.getContext(), 0),
             llvm::Type::getInt32Ty(codeGen.getContext())});

    return codeGen.getBuilder().CreateCall(getItemFunc, {list, index}, "list_item");
}

void CodeGenRuntime::setListElement(llvm::Value* list, llvm::Value* index, llvm::Value* value)
{
   /*  if (runtime)
    {
        runtime->setListElement(list, index, value);
        return;
    } */

    // 如果没有runtime，使用内置实现
    llvm::Function* setItemFunc = getRuntimeFunction(
            "py_list_set_item",
            llvm::Type::getVoidTy(codeGen.getContext()),
            {llvm::PointerType::get(codeGen.getContext(), 0),
             llvm::Type::getInt32Ty(codeGen.getContext()),
             llvm::PointerType::get(codeGen.getContext(), 0)});

    codeGen.getBuilder().CreateCall(setItemFunc, {list, index, value});
}

llvm::Value* CodeGenRuntime::createDict(ObjectType* keyType, ObjectType* valueType)
{
  /*   if (runtime)
    {
        // 如果存在外部 ObjectRuntime，则委托给它
        return runtime->createDict(keyType, valueType);
    } */

    // --- Fallback: Call C runtime function directly ---
    // 1. 获取 C 运行时函数 py_create_dict 的声明
    llvm::Function* createFunc = getRuntimeFunction(
            "py_create_dict",
            llvm::PointerType::get(codeGen.getContext(), 0), // Return type: PyObject*
            {llvm::Type::getInt32Ty(codeGen.getContext()), // Param 1: initialCapacity (int)
             llvm::Type::getInt32Ty(codeGen.getContext())}  // Param 2: keyTypeId (int)
    );

    // 2. 准备参数
    //    - initialCapacity: 使用一个默认值，例如 8
    llvm::Value* initialCapacity = llvm::ConstantInt::get(llvm::Type::getInt32Ty(codeGen.getContext()), 8);
    //    - keyTypeId: 从传入的 keyType 获取
    //      (需要 OperationCodeGenerator::getTypeId 或类似方法)
    int keyTypeIdInt = OperationCodeGenerator::getTypeId(keyType); // 假设有这个辅助函数
    llvm::Value* keyTypeIdValue = llvm::ConstantInt::get(llvm::Type::getInt32Ty(codeGen.getContext()), keyTypeIdInt);

    // 3. 调用 C 运行时函数
    llvm::Value* dictObj = codeGen.getBuilder().CreateCall(createFunc, {initialCapacity, keyTypeIdValue}, "dict_obj");

    // 4. （可选）标记对象来源，如果需要生命周期管理
    // markObjectSource(dictObj, ObjectLifecycleManager::ObjectSource::LITERAL);

    return dictObj; // 返回创建的字典对象

    // --- Remove the original error path ---
    // codeGen.logError("External ObjectRuntime required for createDict");
    // return nullptr;
}

llvm::Value* CodeGenRuntime::getDictItem(llvm::Value* dict, llvm::Value* key)
{
   /*  if (runtime)
    {
        return runtime->getDictItem(dict, key);
    } */
    llvm::Function* getFunc = getRuntimeFunction(
            "py_dict_get_item",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {llvm::PointerType::get(codeGen.getContext(), 0), llvm::PointerType::get(codeGen.getContext(), 0)});
    return codeGen.getBuilder().CreateCall(getFunc, {dict, key}, "dict_item");
}

void CodeGenRuntime::setDictItem(llvm::Value* dict, llvm::Value* key, llvm::Value* value)
{
   /*  if (runtime)
    {
        runtime->setDictItem(dict, key, value);
        return;
    } */
    llvm::Function* setFunc = getRuntimeFunction(
            "py_dict_set_item",
            llvm::Type::getVoidTy(codeGen.getContext()),  // 假设 set 不返回值
            {llvm::PointerType::get(codeGen.getContext(), 0), llvm::PointerType::get(codeGen.getContext(), 0), llvm::PointerType::get(codeGen.getContext(), 0)});
    codeGen.getBuilder().CreateCall(setFunc, {dict, key, value});
}

llvm::Value* CodeGenRuntime::prepareReturnValue(
        llvm::Value* value,
        std::shared_ptr<PyType> valueType,
        std::shared_ptr<PyType> returnType)
{
    if (!value)
    {
        return nullptr;
    }

    // 如果返回类型未指定或为Any，直接使用值类型
    if (!returnType || returnType->isAny())
    {
        return value;
    }

    // 如果值类型和返回类型相同，无需转换
    if (valueType && valueType->equals(*returnType))
    {
        return value;
    }

    // 使用类型操作系统进行转换
    auto* typeGen = codeGen.getTypeGen();
    ObjectType* fromType = valueType ? valueType->getObjectType() : nullptr;
    ObjectType* toType = returnType->getObjectType();

    if (fromType && toType)
    {
        // 检查类型兼容性
        auto& typeRegistry = TypeOperationRegistry::getInstance();
        int fromTypeId = OperationCodeGenerator::getTypeId(fromType);
        int toTypeId = OperationCodeGenerator::getTypeId(toType);

        // 如果类型兼容，获取合适的转换
        if (typeRegistry.areTypesCompatible(fromTypeId, toTypeId))
        {
            TypeConversionDescriptor* convDesc =
                    typeRegistry.getTypeConversionDescriptor(fromTypeId, toTypeId);

            if (convDesc)
            {
                // 使用TypeOperations执行转换
                return OperationCodeGenerator::handleTypeConversion(
                        static_cast<PyCodeGen&>(codeGen), value, fromTypeId, toTypeId);
            }
        }
    }

    // 如果没有找到转换路径，默认返回原值
    return value;
}

llvm::Value* CodeGenRuntime::prepareArgument(
        llvm::Value* value,
        std::shared_ptr<PyType> valueType,
        std::shared_ptr<PyType> paramType)
{
    if (!value)
    {
        return nullptr;
    }

    // 特殊情况：传递给Any类型参数，不需要转换
    if (paramType && paramType->isAny())
    {
        // 确保增加引用计数
        incRef(value);
        return value;
    }

    // 如果值类型和参数类型相同，增加引用计数后返回
    if (valueType && paramType && valueType->equals(*paramType))
    {
        // 确保增加引用计数
        incRef(value);
        return value;
    }

    // 尝试进行类型转换
    auto* typeGen = codeGen.getTypeGen();
    ObjectType* fromType = valueType ? valueType->getObjectType() : nullptr;
    ObjectType* toType = paramType ? paramType->getObjectType() : nullptr;

    if (fromType && toType)
    {
        // 检查类型兼容性
        auto& typeRegistry = TypeOperationRegistry::getInstance();
        int fromTypeId = OperationCodeGenerator::getTypeId(fromType);
        int toTypeId = OperationCodeGenerator::getTypeId(toType);

        // 如果类型兼容，获取合适的转换
        if (typeRegistry.areTypesCompatible(fromTypeId, toTypeId))
        {
            TypeConversionDescriptor* convDesc =
                    typeRegistry.getTypeConversionDescriptor(fromTypeId, toTypeId);

            if (convDesc)
            {
                // 使用TypeOperations执行转换
                llvm::Value* result = OperationCodeGenerator::handleTypeConversion(
                        static_cast<PyCodeGen&>(codeGen), value, fromTypeId, toTypeId);

                // 转换后的对象已经是新对象，不需要增加引用计数
                return result;
            }
        }
    }

    // 如果无法转换，增加引用计数后返回原值
    incRef(value);
    return value;
}

llvm::Value* CodeGenRuntime::prepareAssignmentTarget(
        llvm::Value* value,
        std::shared_ptr<PyType> targetType,
        std::shared_ptr<PyType> valueType)
{
    if (!value)
    {
        return nullptr;
    }

    // 特殊情况：赋值给Any类型，不需要转换
    if (targetType && targetType->isAny())
    {
        // 确保增加引用计数
        incRef(value);
        return value;
    }

    // 如果值类型和目标类型相同，增加引用计数后返回
    if (valueType && targetType && valueType->equals(*targetType))
    {
        // 确保增加引用计数
        incRef(value);
        return value;
    }

    // 尝试进行类型转换
    auto* typeGen = codeGen.getTypeGen();
    ObjectType* fromType = valueType ? valueType->getObjectType() : nullptr;
    ObjectType* toType = targetType ? targetType->getObjectType() : nullptr;

    if (fromType && toType)
    {
        // 检查类型兼容性
        auto& typeRegistry = TypeOperationRegistry::getInstance();
        int fromTypeId = OperationCodeGenerator::getTypeId(fromType);
        int toTypeId = OperationCodeGenerator::getTypeId(toType);

        // 如果类型兼容，获取合适的转换
        if (typeRegistry.areTypesCompatible(fromTypeId, toTypeId))
        {
            TypeConversionDescriptor* convDesc =
                    typeRegistry.getTypeConversionDescriptor(fromTypeId, toTypeId);

            if (convDesc)
            {
                // 使用TypeOperations执行转换
                llvm::Value* result = OperationCodeGenerator::handleTypeConversion(
                        static_cast<PyCodeGen&>(codeGen), value, fromTypeId, toTypeId);

                // 转换后的对象已经是新对象，不需要增加引用计数
                return result;
            }
        }
    }

    // 如果无法转换，增加引用计数后返回原值
    incRef(value);
    return value;
}

//===----------------------------------------------------------------------===//
// 临时对象管理
//===----------------------------------------------------------------------===//

void CodeGenRuntime::trackTempObject(llvm::Value* obj)
{
    if (obj)
    {
        codeGen.addTempObject(obj, nullptr);
    }
}

void CodeGenRuntime::cleanupTemporaryObjects()
{
    // 清理当前函数中的临时对象
    codeGen.releaseTempObjects();
}

void CodeGenRuntime::releaseTrackedObjects()
{
    // 释放所有跟踪的对象
    codeGen.clearTempObjects();
}

//===----------------------------------------------------------------------===//
// 对象元数据
//===----------------------------------------------------------------------===//

void CodeGenRuntime::attachTypeMetadata(llvm::Value* value, int typeId)
{
    if (!value) return;

    // 使用LLVM元数据来附加类型信息
    llvm::MDBuilder mdBuilder(codeGen.getContext());
    llvm::MDNode* typeMD = llvm::MDNode::get(
            codeGen.getContext(),
            llvm::ConstantAsMetadata::get(
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(codeGen.getContext()), typeId)));

    // 如果值是指令，直接附加元数据
    if (llvm::Instruction* inst = llvm::dyn_cast<llvm::Instruction>(value))
    {
        inst->setMetadata("py.type", typeMD);
    }
    // 否则，创建一个位移操作来承载元数据
    else
    {
        auto& builder = codeGen.getBuilder();

        // 创建一个无操作的位移指令，用于附加元数据
        llvm::Value* nop = builder.CreateBitCast(value, value->getType(), "nop_for_metadata");
        if (llvm::Instruction* nopInst = llvm::dyn_cast<llvm::Instruction>(nop))
        {
            nopInst->setMetadata("py.type", typeMD);
        }
    }
}

int CodeGenRuntime::getTypeIdFromMetadata(llvm::Value* value)
{
    if (!value) return -1;

    // 从指令中获取类型元数据
    if (llvm::Instruction* inst = llvm::dyn_cast<llvm::Instruction>(value))
    {
        llvm::MDNode* typeMD = inst->getMetadata("py.type");
        if (typeMD)
        {
            llvm::ConstantAsMetadata* typeIdMD = llvm::dyn_cast<llvm::ConstantAsMetadata>(
                    typeMD->getOperand(0));
            if (typeIdMD)
            {
                llvm::ConstantInt* typeIdConst = llvm::dyn_cast<llvm::ConstantInt>(
                        typeIdMD->getValue());
                if (typeIdConst)
                {
                    return typeIdConst->getSExtValue();
                }
            }
        }
    }

    return -1;
}

//===----------------------------------------------------------------------===//
// 类型检查
//===----------------------------------------------------------------------===//

bool CodeGenRuntime::needsLifecycleManagement(std::shared_ptr<PyType> type)
{
    if (!type)
    {
        return false;
    }

    // 检查类型是否需要生命周期管理
    ObjectType* objType = type->getObjectType();
    if (!objType)
    {
        return false;
    }

    // 引用类型需要生命周期管理
    return objType->isReference();
}

bool CodeGenRuntime::isTemporaryObject(llvm::Value* value)
{
    if (!value)
    {
        return false;
    }

    // 检查是否是函数调用结果
    if (llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(value))
    {
        // 调用结果通常是临时的
        return true;
    }

    // 检查是否是运算结果
    if (llvm::BinaryOperator* binOp = llvm::dyn_cast<llvm::BinaryOperator>(value))
    {
        return true;
    }

    // 检查显式标记的临时对象
    auto it = objectSources.find(value);
    if (it != objectSources.end())
    {
        auto source = it->second;
        return (source == ObjectLifecycleManager::ObjectSource::BINARY_OP || source == ObjectLifecycleManager::ObjectSource::UNARY_OP || source == ObjectLifecycleManager::ObjectSource::FUNCTION_RETURN);
    }

    return false;
}

}  // namespace llvmpy