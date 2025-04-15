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
// 生成完整模块
bool CodeGenModule::generateModule(ModuleAST* module, bool isEntryPoint)
{
    if (!module)
    {
        return false;
    }

    // 设置当前模块
    setCurrentModule(module);

    // 初始化模块 - 确保运行时函数声明等只执行一次
    if (!moduleInitialized)
    {
        // 添加运行时函数声明
        addRuntimeFunctions();

        // --- 推荐：使用全局构造函数进行运行时初始化 ---
        createAndRegisterRuntimeInitializer(); // 这个对于所有模块都是必要的

        moduleInitialized = true;
    }

    // 处理模块中的函数定义 (对所有模块都执行)
    for (auto& func : module->getFunctions())
    {
        if (!handleFunctionDef(func.get()))
        {
            return false;
        }
    }

    // --- 根据是否是入口点，决定如何处理全局语句 ---
    if (isEntryPoint)
    {
        // --- 生成 main 函数作为入口点 ---
        llvm::FunctionType* mainFnType = llvm::FunctionType::get(
                llvm::PointerType::get(codeGen.getContext(), 0), // 返回 PyObject*
                false);                                          // 无参数

        llvm::Function* mainFn = codeGen.getOrCreateExternalFunction(
                "main",
                mainFnType->getReturnType(),
                {},
                false);

        // 创建函数入口块
        llvm::BasicBlock* entryBB = nullptr;
        if (mainFn->empty()) {
            entryBB = createFunctionEntry(mainFn);
        } else {
            entryBB = &mainFn->getEntryBlock();
        }
        codeGen.getBuilder().SetInsertPoint(entryBB, entryBB->getFirstInsertionPt());

        // 设置当前函数上下文
        llvm::Function* savedFunction = codeGen.getCurrentFunction();
        ObjectType* savedReturnType = codeGen.getCurrentReturnType();
        codeGen.setCurrentFunction(mainFn);
        auto anyObjectType = PyType::getAny()->getObjectType();
        codeGen.setCurrentReturnType(anyObjectType);

        // --- 处理全局语句 ---
        auto* stmtGen = codeGen.getStmtGen();
        // 确保插入点在 main 函数的当前末尾
         if (!mainFn->empty() && mainFn->back().getTerminator() == nullptr) {
             codeGen.getBuilder().SetInsertPoint(&mainFn->back());
         } else if (mainFn->empty()) {
             codeGen.getBuilder().SetInsertPoint(entryBB);
         } else {
             // 如果已有终止符，可能需要创建新块，但这里简化处理
             codeGen.getBuilder().SetInsertPoint(&mainFn->back());
         }

        for (auto& stmt : module->getStatements())
        {
            stmtGen->handleStmt(stmt.get());
        }

        // --- 确保 main 函数总是有返回语句 ---
        if (!codeGen.getBuilder().GetInsertBlock() || !codeGen.getBuilder().GetInsertBlock()->getTerminator())
        {
            llvm::Function* createIntFunc = codeGen.getOrCreateExternalFunction(
                    "py_create_int",
                    llvm::PointerType::get(codeGen.getContext(), 0),
                    {llvm::Type::getInt32Ty(codeGen.getContext())});

            llvm::Value* retValObj = codeGen.getBuilder().CreateCall(
                    createIntFunc,
                    {llvm::ConstantInt::get(llvm::Type::getInt32Ty(codeGen.getContext()), 0)},
                    "ret_int_obj");

            codeGen.getBuilder().CreateRet(retValObj);
        }

        // 恢复之前的函数上下文
        codeGen.setCurrentFunction(savedFunction);
        codeGen.setCurrentReturnType(savedReturnType);

        // 验证main函数
        std::string errorInfo;
        llvm::raw_string_ostream errorStream(errorInfo);
        if (llvm::verifyFunction(*mainFn, &errorStream))
        {
            std::cerr << "Main function verification failed: " << errorStream.str() << std::endl;
            return false;
        }
    }
    else // 非入口点模块
    {
        // TODO: 处理非入口点模块的全局语句
        // 方案1: 忽略全局语句 (最简单，但功能受限)
        // 方案2: 将全局语句放入模块初始化函数 __module_init_<module_name>
        //        并考虑如何触发这个初始化 (例如，通过全局构造函数，或在导入时显式调用)
        //        这需要修改 createModuleInitFunction 并可能扩展 createAndRegisterRuntimeInitializer
        if (!module->getStatements().empty()) {
             codeGen.logWarning("非入口点模块中的全局语句当前未被执行", 0, 0);
        }
    }

    return true;
}



// --- 新增：创建并注册运行时初始化函数 ---
void CodeGenModule::createAndRegisterRuntimeInitializer()
{
    llvm::LLVMContext& context = codeGen.getContext();
    llvm::Module* module = codeGen.getModule();

    // 1. 创建初始化函数 __llvmpy_runtime_init
    llvm::FunctionType* initFuncType = llvm::FunctionType::get(llvm::Type::getVoidTy(context), false);
    llvm::Function* initFunc = llvm::Function::Create(
        initFuncType,
        llvm::Function::InternalLinkage, // 或者 PrivateLinkage
        "__llvmpy_runtime_init",
        module
    );

    // 创建入口块
    llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(context, "entry", initFunc);
    llvm::IRBuilder<> builder(entryBB); // 临时 builder

    // 获取 py_initialize_builtin_type_methods
    llvm::Function* runtimeInitCore = codeGen.getOrCreateExternalFunction(
        "py_initialize_builtin_type_methods",
        llvm::Type::getVoidTy(context),
        {}
    );

    // 在 __llvmpy_runtime_init 中调用核心初始化函数
    builder.CreateCall(runtimeInitCore);
    builder.CreateRetVoid(); // 添加返回

    // 2. 注册到 llvm.global_ctors
    // 定义构造函数记录的类型: { i32, void ()*, i8* }
    llvm::StructType* ctorEntryType = llvm::StructType::get(
        context,
        { llvm::Type::getInt32Ty(context),   // 优先级
          llvm::PointerType::get(initFuncType, 0), // 函数指针
          llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0) // 数据指针 (通常为 null)
        }
    );

    // 创建构造函数记录实例
    llvm::Constant* ctorEntry = llvm::ConstantStruct::get(
        ctorEntryType,
        { llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 65535), // 优先级 (65535 是默认)
          initFunc, // 初始化函数
          llvm::ConstantPointerNull::get(llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)) // 数据指针为 null
        }
    );

    // 创建包含此记录的数组
    llvm::ArrayType* arrayType = llvm::ArrayType::get(ctorEntryType, 1);
    llvm::Constant* ctorsArray = llvm::ConstantArray::get(arrayType, {ctorEntry});

    // 创建全局变量 @llvm.global_ctors
    new llvm::GlobalVariable(
        *module,
        arrayType,
        false, // isConstant = false
        llvm::GlobalValue::AppendingLinkage, // 必须是 AppendingLinkage
        ctorsArray, // 初始化值
        "llvm.global_ctors" // 名称
    );
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
    codeGen.getVariableUpdateContext().clearLoopVariables();
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