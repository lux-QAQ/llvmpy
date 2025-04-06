#include "CodeGen/CodeGenModule.h"
#include "CodeGen/CodeGenType.h"
#include "CodeGen/CodeGenRuntime.h"
#include "CodeGen/CodeGenStmt.h"  // 添加这一行，确保CodeGenStmt完整定义
#include "TypeOperations.h"
#include "ObjectLifecycle.h"
#include "TypeIDs.h"
#include <iostream>
#include <sstream>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>

namespace llvmpy
{

// 生成完整模块
bool CodeGenModule::generateModule(ModuleAST* module)
{
    if (!module)
    {
        return false;
    }

    // 设置当前模块
    setCurrentModule(module);

    // 初始化模块
    if (!moduleInitialized)
    {
        // 添加运行时函数声明
        addRuntimeFunctions();

        // 创建模块初始化函数
        createModuleInitFunction();

        moduleInitialized = true;
    }

    // 处理模块中的函数定义
    for (auto& func : module->getFunctions())
    {
        if (!handleFunctionDef(func.get()))
        {
            return false;
        }
    }

    // 处理全局语句（仅在main函数中执行的语句）
    if (!module->getStatements().empty())
    {
        // 创建main函数
        std::vector<std::shared_ptr<PyType>> noParams;
        llvm::FunctionType* mainFnType = createFunctionType(PyType::getInt(), noParams);
        // TODO实现带参数的main函数
        llvm::Function* mainFn = codeGen.getOrCreateExternalFunction(
                "main",
                llvm::Type::getInt32Ty(codeGen.getContext()),  // 返回类型为 int32
                {},                                            // 无参数
                false);

        // 创建函数入口块
        llvm::BasicBlock* entryBB = createFunctionEntry(mainFn);
        codeGen.getBuilder().SetInsertPoint(entryBB);

        // 设置当前函数
        codeGen.setCurrentFunction(mainFn);

        // 为main函数设置返回类型
        auto intType = TypeRegistry::getInstance().getType("int");
        codeGen.setCurrentReturnType(intType);

        // 处理模块中的语句
        auto* stmtGen = codeGen.getStmtGen();
        for (auto& stmt : module->getStatements())
        {
            stmtGen->handleStmt(stmt.get());
        }

        // 如果最后一条语句不是返回语句，添加默认返回值0
        if (!codeGen.getBuilder().GetInsertBlock()->getTerminator())
        {
            // 创建返回值
            llvm::Value* retVal = llvm::ConstantInt::get(
                    llvm::Type::getInt32Ty(codeGen.getContext()), 0);

            // 创建返回指令
            codeGen.getBuilder().CreateRet(retVal);
        }

        // 验证main函数
        std::string errorInfo;
        llvm::raw_string_ostream errorStream(errorInfo);
        if (llvm::verifyFunction(*mainFn, &errorStream))
        {
            std::cerr << "Main function verification failed: " << errorStream.str() << std::endl;
            return false;
        }
    }

    // 验证整个模块
    return true;
}

// 创建模块初始化函数
llvm::Function* CodeGenModule::createModuleInitFunction()
{
    // 创建模块初始化函数类型
    llvm::FunctionType* moduleInitType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(codeGen.getContext()), false);

    // 创建模块初始化函数
    llvm::Function* moduleInitFn = llvm::Function::Create(
            moduleInitType,
            llvm::Function::InternalLinkage,
            "__module_init",
            codeGen.getModule());

    // 创建函数入口块
    llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(
            codeGen.getContext(), "entry", moduleInitFn);

    // 保存当前插入点
    llvm::BasicBlock* savedBlock = codeGen.getBuilder().GetInsertBlock();
    llvm::Function* savedFunction = codeGen.getCurrentFunction();
    ObjectType* savedReturnType = codeGen.getCurrentReturnType();

    // 设置插入点到初始化函数
    codeGen.getBuilder().SetInsertPoint(entryBB);
    codeGen.setCurrentFunction(moduleInitFn);
    codeGen.setCurrentReturnType(nullptr);

    // 这里可以添加模块级初始化代码，比如设置全局变量等

    // 创建返回指令
    codeGen.getBuilder().CreateRetVoid();

    // 恢复之前的插入点
    if (savedBlock)
    {
        codeGen.getBuilder().SetInsertPoint(savedBlock);
    }
    codeGen.setCurrentFunction(savedFunction);
    codeGen.setCurrentReturnType(savedReturnType);

    return moduleInitFn;
}

// 添加运行时库函数声明
void CodeGenModule::addRuntimeFunctions()
{
    // 处理打印函数
    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_print_int",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::Type::getInt32Ty(codeGen.getContext())},
                false);
    }

    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_print_double",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::Type::getDoubleTy(codeGen.getContext())},
                false);
    }

    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_print_string",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0)},
                false);
    }

    // 处理对象创建函数
    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_create_int",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::Type::getInt32Ty(codeGen.getContext())},
                false);
    }

    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_create_double",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::Type::getDoubleTy(codeGen.getContext())},
                false);
    }

    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_create_bool",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::Type::getInt1Ty(codeGen.getContext())},
                false);
    }

    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_create_string",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0)},
                false);
    }

    // 处理对象操作函数
    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_object_add",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::PointerType::get(codeGen.getContext(), 0)},
                false);
    }

    // 索引操作函数
    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_object_index",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::PointerType::get(codeGen.getContext(), 0)},
                false);
    }

    // 引用计数管理函数
    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_incref",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0)},
                false);

        codeGen.getOrCreateExternalFunction(
                "py_decref",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0)},
                false);
    }

    // 类型检查函数
    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_check_type",
                llvm::Type::getInt1Ty(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::Type::getInt32Ty(codeGen.getContext())},
                false);
    }

    // 获取 None 对象函数
    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_get_none",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {},
                false);
    }
}

// 处理函数定义
llvm::Function* CodeGenModule::handleFunctionDef(FunctionAST* funcAST)
{
    if (!funcAST)
    {
        return nullptr;
    }

    // 解析函数返回类型
    std::shared_ptr<PyType> returnType = resolveReturnType(funcAST);

    // 解析函数参数类型
    std::vector<std::shared_ptr<PyType>> paramTypes;
    funcAST->resolveParamTypes();
    for (auto& param : funcAST->params)
    {
        if (param.resolvedType)
        {
            paramTypes.push_back(param.resolvedType);
        }
        else
        {
            // 如果参数类型未解析，使用 Any 类型
            paramTypes.push_back(PyType::getAny());
        }
    }

    // 创建函数类型
    llvm::FunctionType* fnType = createFunctionType(returnType, paramTypes);

    // 创建函数
    std::string funcName = funcAST->name;
    std::vector<llvm::Type*> llvmParamTypes;
    for (auto& paramType : paramTypes)
    {
        llvmParamTypes.push_back(llvm::PointerType::get(codeGen.getContext(), 0));  // Python 对象指针
    }

    // 获取返回类型
    llvm::Type* llvmReturnType = llvm::PointerType::get(codeGen.getContext(), 0);
    if (returnType->isVoid())
    {
        llvmReturnType = llvm::Type::getVoidTy(codeGen.getContext());
    }

    llvm::Function* function = codeGen.getOrCreateExternalFunction(
            funcName,
            llvmReturnType,
            llvmParamTypes,
            false);

    // 处理函数参数
    handleFunctionParams(function, funcAST->params, paramTypes);

    // 创建函数入口基本块
    llvm::BasicBlock* entryBB = createFunctionEntry(function);
    codeGen.getBuilder().SetInsertPoint(entryBB);

    // 保存当前函数上下文
    llvm::Function* savedFunction = codeGen.getCurrentFunction();
    ObjectType* savedReturnType = codeGen.getCurrentReturnType();

    // 设置当前函数上下文
    codeGen.setCurrentFunction(function);
    codeGen.setCurrentReturnType(returnType->getObjectType());

    // 添加函数引用
    std::vector<ObjectType*> paramObjectTypes;
    for (auto& type : paramTypes)
    {
        paramObjectTypes.push_back(type->getObjectType());
    }
    addFunctionReference(funcName, function, returnType->getObjectType(), paramObjectTypes);

    // 处理函数体
    auto* stmtGen = codeGen.getStmtGen();
    stmtGen->beginScope();

    for (auto& stmt : funcAST->body)
    {
        stmtGen->handleStmt(stmt.get());
    }

    // 如果函数没有显式的返回语句，添加默认返回值 None
    if (!codeGen.getBuilder().GetInsertBlock()->getTerminator())
    {
        // 获取None对象
        llvm::Function* getNoneFn = codeGen.getOrCreateExternalFunction(
                "py_get_none",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {});

        llvm::Value* noneValue = codeGen.getBuilder().CreateCall(getNoneFn);

        // 返回None
        codeGen.getBuilder().CreateRet(noneValue);
    }

    // 结束函数作用域
    stmtGen->endScope();

    // 清理函数资源
    cleanupFunction();

    // 恢复函数上下文
    codeGen.setCurrentFunction(savedFunction);
    codeGen.setCurrentReturnType(savedReturnType);

    // 验证函数
    std::string errorInfo;
    llvm::raw_string_ostream errorStream(errorInfo);
    if (llvm::verifyFunction(*function, &errorStream))
    {
        std::cerr << "Function " << funcName << " verification failed: "
                  << errorStream.str() << std::endl;
        function->eraseFromParent();
        return nullptr;
    }

    return function;
}

// 创建函数类型
llvm::FunctionType* CodeGenModule::createFunctionType(
        std::shared_ptr<PyType> returnType,
        const std::vector<std::shared_ptr<PyType>>& paramTypes)
{
    // 获取返回类型的LLVM表示
    llvm::Type* llvmReturnType;
    if (returnType->isVoid())
    {
        llvmReturnType = llvm::Type::getVoidTy(codeGen.getContext());
    }
    else
    {
        // 对于Python类型，统一使用PyObject指针
        llvmReturnType = llvm::PointerType::get(codeGen.getContext(), 0);
    }

    // 获取参数类型的LLVM表示
    std::vector<llvm::Type*> llvmParamTypes;
    for (auto& paramType : paramTypes)
    {
        // 对于Python类型，统一使用PyObject指针
        llvmParamTypes.push_back(llvm::PointerType::get(codeGen.getContext(), 0));
    }

    // 创建函数类型
    return llvm::FunctionType::get(llvmReturnType, llvmParamTypes, false);
}

// 处理函数参数
void CodeGenModule::handleFunctionParams(
        llvm::Function* function,
        const std::vector<ParamAST>& params,
        std::vector<std::shared_ptr<PyType>>& paramTypes)
{
    // 命名函数参数
    size_t idx = 0;
    for (auto& arg : function->args())
    {
        if (idx < params.size())
        {
            arg.setName(params[idx].name);

            // 在当前作用域中添加参数变量
            llvm::Value* paramValue = &arg;
            codeGen.getSymbolTable().setVariable(
                    params[idx].name,
                    paramValue,
                    paramTypes[idx]->getObjectType());

            idx++;
        }
    }
}

// 解析函数返回类型
std::shared_ptr<PyType> CodeGenModule::resolveReturnType(FunctionAST* funcAST)
{
    // 如果已经解析，直接返回
    if (funcAST->returnTypeResolved)
    {
        return funcAST->getReturnType();
    }

    // 如果有明确的返回类型注解，使用注解类型
    if (!funcAST->returnTypeName.empty())
    {
        // 解析类型名称
        std::shared_ptr<PyType> returnType = PyType::fromString(funcAST->returnTypeName);
        funcAST->cachedReturnType = returnType;
        funcAST->returnTypeResolved = true;
        return returnType;
    }

    // 否则尝试推断返回类型
    std::shared_ptr<PyType> inferredType = funcAST->inferReturnType();
    if (inferredType)
    {
        funcAST->cachedReturnType = inferredType;
        funcAST->returnTypeResolved = true;
        return inferredType;
    }

    // 如果无法推断，默认为Any类型
    funcAST->cachedReturnType = PyType::getAny();
    funcAST->returnTypeResolved = true;
    return funcAST->cachedReturnType;
}

// 创建函数入口基本块
llvm::BasicBlock* CodeGenModule::createFunctionEntry(llvm::Function* function)
{
    return llvm::BasicBlock::Create(codeGen.getContext(), "entry", function);
}

// 处理函数返回 ？这里实际上需要使用类型推导？
void CodeGenModule::handleFunctionReturn(
        llvm::Value* returnValue,
        std::shared_ptr<PyType> returnType)
{
    auto* runtime = codeGen.getRuntimeGen();

    // 准备返回值 - 确保类型匹配（添加第三个参数，值类型与返回类型相同）
    llvm::Value* preparedValue = runtime->prepareReturnValue(
            returnValue,
            returnType,   // 值类型
            returnType);  // 函数返回类型

    // 创建返回指令
    codeGen.getBuilder().CreateRet(preparedValue);
}

/* // 另一个可能的修复方案
void CodeGenModule::handleFunctionReturn(
    llvm::Value* returnValue, 
    std::shared_ptr<PyType> returnType) {
    
    auto* runtime = codeGen.getRuntimeGen();
    auto* typeGen = codeGen.getTypeGen();
    
    // 获取实际返回值的类型（可能需要从返回值中推导）
    std::shared_ptr<PyType> valueType = returnType; // 这里假设两者相同，实际可能需要推导
    
    // 准备返回值 - 确保类型匹配
    llvm::Value* preparedValue = runtime->prepareReturnValue(
        returnValue, 
        valueType,
        returnType);
    
    // 创建返回指令
    codeGen.getBuilder().CreateRet(preparedValue);
} */
// 清理函数资源
void CodeGenModule::cleanupFunction()
{
    // 释放函数中的临时对象
    codeGen.releaseTempObjects();
}

// 添加函数引用
void CodeGenModule::addFunctionReference(
        const std::string& name,
        llvm::Function* function,
        ObjectType* returnType,
        const std::vector<ObjectType*>& paramTypes,
        bool isExternal)
{
    // 创建函数定义信息
    FunctionDefInfo info;
    info.name = name;
    info.function = function;
    info.returnType = returnType;
    info.paramTypes = paramTypes;
    info.isExternal = isExternal;

    // 添加到函数定义映射
    functionDefs[name] = info;
}

// 查找函数引用
FunctionDefInfo* CodeGenModule::getFunctionInfo(const std::string& name)
{
    auto it = functionDefs.find(name);
    if (it != functionDefs.end())
    {
        return &it->second;
    }
    return nullptr;
}

}  // namespace llvmpy