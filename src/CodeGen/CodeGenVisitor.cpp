#include "CodeGen/CodeGenVisitor.h"
#include "CodeGen/CodeGenBase.h"
#include "CodeGen/CodeGenType.h"
#include "CodeGen/CodeGenExpr.h"
#include "CodeGen/CodeGenModule.h"
#include "CodeGen/CodeGenRuntime.h"
#include "CodeGen/PyCodeGen.h"
#include "TypeOperations.h"
#include "ObjectLifecycle.h"
#include "TypeIDs.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>

#include <vector>     // 用于 std::vector

#include <set>

namespace llvmpy
{

CodeGenVisitor::CodeGenVisitor(CodeGenBase& cg)
    : codeGen(cg)
{
    // 确保类型特性检查器已初始化
    TypeFeatureChecker::registerBuiltinFeatureChecks();
}




// 通用ASTNode 访问方法

void CodeGenVisitor::visit(ASTNode* node) {
    if (!node) return;
    
    switch (node->kind()) {
        case ASTKind::NumberExpr:
            visit(static_cast<NumberExprAST*>(node));
            break;
        case ASTKind::StringExpr:
            visit(static_cast<StringExprAST*>(node));
            break;
        case ASTKind::BoolExpr:
            visit(static_cast<BoolExprAST*>(node));
            break;
        case ASTKind::NoneExpr:
            visit(static_cast<NoneExprAST*>(node));
            break;
        case ASTKind::VariableExpr:
            visit(static_cast<VariableExprAST*>(node));
            break;
        case ASTKind::BinaryExpr:
            visit(static_cast<BinaryExprAST*>(node));
            break;
        case ASTKind::UnaryExpr:
            visit(static_cast<UnaryExprAST*>(node));
            break;
        case ASTKind::CallExpr:
            visit(static_cast<CallExprAST*>(node));
            break;
        case ASTKind::ListExpr:
            visit(static_cast<ListExprAST*>(node));
            break;
        case ASTKind::IndexExpr:
            visit(static_cast<IndexExprAST*>(node));
            break;
        case ASTKind::Function:
            visit(static_cast<FunctionAST*>(node));
            break;
        case ASTKind::ExprStmt:
            visit(static_cast<ExprStmtAST*>(node));
            break;
        case ASTKind::ReturnStmt:
            visit(static_cast<ReturnStmtAST*>(node));
            break;
        case ASTKind::IfStmt:
            visit(static_cast<IfStmtAST*>(node));
            break;
        case ASTKind::WhileStmt:
            visit(static_cast<WhileStmtAST*>(node));
            break;
        case ASTKind::AssignStmt:
            visit(static_cast<AssignStmtAST*>(node));
            break;
        case ASTKind::IndexAssignStmt:
            visit(static_cast<IndexAssignStmtAST*>(node));
            break;
        case ASTKind::PrintStmt:
            visit(static_cast<PrintStmtAST*>(node));
            break;
        case ASTKind::PassStmt:
            visit(static_cast<PassStmtAST*>(node));
            break;
        case ASTKind::ImportStmt:
            visit(static_cast<ImportStmtAST*>(node));
            break;
        case ASTKind::ClassStmt:
            visit(static_cast<ClassStmtAST*>(node));
            break;
        case ASTKind::Module:
            visit(static_cast<ModuleAST*>(node));
            break;
        default:
            codeGen.logError("不支持的 AST 节点类型", 
                         node->line ? *node->line : 0, 
                         node->column ? *node->column : 0);
            break;
    }
}
// 通用语句访问方法
void CodeGenVisitor::visit(StmtAST* stmt) {
    if (!stmt) return;
    
    // 根据语句类型分派到对应的特定访问方法
    switch (stmt->kind()) {
        case ASTKind::ExprStmt:
            visit(static_cast<ExprStmtAST*>(stmt));
            break;
        case ASTKind::ReturnStmt:
            visit(static_cast<ReturnStmtAST*>(stmt));
            break;
        case ASTKind::IfStmt:
            visit(static_cast<IfStmtAST*>(stmt));
            break;
        case ASTKind::WhileStmt:
            visit(static_cast<WhileStmtAST*>(stmt));
            break;
        case ASTKind::AssignStmt:
            visit(static_cast<AssignStmtAST*>(stmt));
            break;
        case ASTKind::IndexAssignStmt:
            visit(static_cast<IndexAssignStmtAST*>(stmt));
            break;
        case ASTKind::PrintStmt:
            visit(static_cast<PrintStmtAST*>(stmt));
            break;
        case ASTKind::PassStmt:
            visit(static_cast<PassStmtAST*>(stmt));
            break;
        case ASTKind::ImportStmt:
            visit(static_cast<ImportStmtAST*>(stmt));
            break;
        case ASTKind::ClassStmt:
            visit(static_cast<ClassStmtAST*>(stmt));
            break;
        default:
            codeGen.logError("不支持的语句类型", 
                         stmt->line ? *stmt->line : 0, 
                         stmt->column ? *stmt->column : 0);
            break;
    }
}

// 处理表达式语句
void CodeGenVisitor::visit(ExprStmtAST* node) {
    auto* exprGen = codeGen.getExprGen();
    auto* runtime = codeGen.getRuntimeGen();
    
    // 如果存在表达式，生成表达式代码
    if (node->getExpr()) {
        llvm::Value* exprValue = exprGen->handleExpr(node->getExpr());
        
        // 表达式语句的值通常不使用，但可能有副作用
        if (exprValue) {
            codeGen.setLastExprValue(exprValue);
            
            // 设置表达式类型（用于后续可能的类型检查或推导）
            auto* typeGen = codeGen.getTypeGen();
            std::shared_ptr<PyType> exprType = typeGen->inferExprType(node->getExpr());
            codeGen.setLastExprType(exprType);
        }
    }
    
    // 清理可能产生的临时对象，确保正确管理引用计数
    runtime->cleanupTemporaryObjects();
}

// 处理 pass 语句
void CodeGenVisitor::visit(PassStmtAST* node) {
    // pass 语句不生成任何代码，相当于 NOP
    // 但在某些情况下可能需要更新变量上下文
    auto& updateContext = codeGen.getVariableUpdateContext();
    if (updateContext.inLoopContext()) {
        // 如果在循环中，确保应用任何挂起的变量更新
        updateContext.applyPendingUpdates();
    }
}
// 处理导入语句
void CodeGenVisitor::visit(ImportStmtAST* node) {
    auto& builder = codeGen.getBuilder();
    auto* runtime = codeGen.getRuntimeGen();
    auto* typeGen = codeGen.getTypeGen();
    auto& updateContext = codeGen.getVariableUpdateContext();
    
    // 获取模块名和别名
    const std::string& moduleName = node->getModuleName();
    const std::string& alias = node->getAlias();
    
    // 创建全局字符串常量用于模块名
    llvm::Value* moduleNameValue = builder.CreateGlobalString(moduleName, "module_name");
    
    // 获取导入模块函数
    llvm::Function* importModuleFunc = codeGen.getOrCreateExternalFunction(
        "py_import_module", 
        llvm::PointerType::get(codeGen.getContext(), 0),
        {llvm::PointerType::get(codeGen.getContext(), 0)}
    );
    
    // 调用函数导入模块
    llvm::Value* moduleObj = builder.CreateCall(importModuleFunc, {moduleNameValue}, "module_obj");
    
    // 确定模块在当前作用域中的名称
    std::string moduleVarName = alias.empty() ? moduleName : alias;
    
    // 获取模块类型
    std::shared_ptr<PyType> moduleType = typeGen->getModuleType(moduleName);
    
    // 将模块对象添加到符号表
    codeGen.getSymbolTable().setVariable(
        moduleVarName, 
        moduleObj, 
        moduleType->getObjectType()
    );
    
    // 更新变量类型上下文
    updateContext.setVariableType(moduleVarName, moduleType);
    
    // 清理临时对象
    runtime->cleanupTemporaryObjects();
}



// 处理类定义语句
void CodeGenVisitor::visit(ClassStmtAST* node) {
    auto& builder = codeGen.getBuilder();
    auto* runtime = codeGen.getRuntimeGen();
    auto* stmtGen = codeGen.getStmtGen();
    auto* typeGen = codeGen.getTypeGen();
    auto& updateContext = codeGen.getVariableUpdateContext();
    
    // 获取类名和基类
    const std::string& className = node->getClassName();
    const std::vector<std::string>& baseClasses = node->getBaseClasses();
    
    // 创建类名字符串
    llvm::Value* classNameValue = builder.CreateGlobalString(className, "class_name");
    
    // 创建基类列表
    llvm::Value* baseClassesArray = nullptr;
    if (!baseClasses.empty()) {
        // 获取创建字符串列表函数
        llvm::Function* createStringListFunc = codeGen.getOrCreateExternalFunction(
            "py_create_string_list", 
            llvm::PointerType::get(codeGen.getContext(), 0),
            {llvm::Type::getInt32Ty(codeGen.getContext())}
        );
        
        // 创建基类名称列表
        baseClassesArray = builder.CreateCall(
            createStringListFunc, 
            {llvm::ConstantInt::get(llvm::Type::getInt32Ty(codeGen.getContext()), baseClasses.size())},
            "base_classes"
        );
        
        // 填充基类名称
        llvm::Function* setListStringItemFunc = codeGen.getOrCreateExternalFunction(
            "py_list_set_string_item",
            llvm::Type::getVoidTy(codeGen.getContext()),
            {
                llvm::PointerType::get(codeGen.getContext(), 0),
                llvm::Type::getInt32Ty(codeGen.getContext()),
                llvm::PointerType::get(codeGen.getContext(), 0)
            }
        );
        
        for (size_t i = 0; i < baseClasses.size(); ++i) {
            llvm::Value* baseClassName = builder.CreateGlobalString(baseClasses[i], "base_class");
            builder.CreateCall(
                setListStringItemFunc,
                {
                    baseClassesArray,
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(codeGen.getContext()), i),
                    baseClassName
                }
            );
        }
    }
    
    // 获取创建类的函数
    llvm::Function* createClassFunc = codeGen.getOrCreateExternalFunction(
        "py_create_class", 
        llvm::PointerType::get(codeGen.getContext(), 0),
        {
            llvm::PointerType::get(codeGen.getContext(), 0),
            llvm::PointerType::get(codeGen.getContext(), 0)
        }
    );
    
    // 调用函数创建类
    llvm::Value* classObj = builder.CreateCall(
        createClassFunc, 
        {
            classNameValue, 
            baseClassesArray ? baseClassesArray : llvm::ConstantPointerNull::get(
                llvm::PointerType::get(codeGen.getContext(), 0))
        }, 
        "class_obj"
    );
    
    // 创建新作用域来处理类体
    stmtGen->beginScope();
    
    // 为self添加一个占位符
    llvm::Function* getSelfFunc = codeGen.getOrCreateExternalFunction(
        "py_get_self_default",
        llvm::PointerType::get(codeGen.getContext(), 0),
        {}
    );
    
    llvm::Value* self = builder.CreateCall(getSelfFunc, {}, "self");
    
    // 添加self到当前作用域
    codeGen.getSymbolTable().setVariable(
        "self",
        self,
        typeGen->getClassInstanceType(className)->getObjectType()
    );
    
    // 更新变量类型上下文
    updateContext.setVariableType(
        "self",
        typeGen->getClassInstanceType(className)
    );
    
    // 处理类方法
    for (auto& method : node->getMethods()) {
        if (method) {
            // 处理方法定义
            stmtGen->handleMethod(method.get(), classObj);
        }
    }
    
    // 处理类体中的语句
    for (auto& stmt : node->getBody()) {
        if (stmt) {
            visit(stmt.get());
        }
    }
    
    // 结束类作用域
    stmtGen->endScope();
    
    // 将类对象添加到当前作用域
    codeGen.getSymbolTable().setVariable(
        className,
        classObj,
        typeGen->getClassType(className)->getObjectType()
    );
    
    // 更新变量类型上下文
    updateContext.setVariableType(
        className,
        typeGen->getClassType(className)
    );
    
    // 清理临时对象
    runtime->cleanupTemporaryObjects();
}

// 处理模块
void CodeGenVisitor::visit(ModuleAST* node) {
    auto* moduleGen = codeGen.getModuleGen();
    auto& updateContext = codeGen.getVariableUpdateContext();
    
    // 设置当前模块
    moduleGen->setCurrentModule(node);
    
    // 创建或获取main函数（如果已存在）
    llvm::Function* mainFunc = codeGen.getModule()->getFunction("main");
    if (!mainFunc) {
        // 创建main函数类型：返回PyObject*，不带参数
        llvm::FunctionType* mainFuncType = llvm::FunctionType::get(
            llvm::PointerType::get(codeGen.getContext(), 0),
            false
        );
        
        // 创建main函数
        mainFunc = llvm::Function::Create(
            mainFuncType,
            llvm::Function::ExternalLinkage,
            "main",
            codeGen.getModule()
        );
    }
    
    // 设置当前函数为main
    llvm::Function* savedFunction = codeGen.getCurrentFunction();
    codeGen.setCurrentFunction(mainFunc);
    
    // 创建函数入口基本块
    llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(
        codeGen.getContext(),
        "entry",
        mainFunc
    );
    
    // 设置插入点到入口块
    codeGen.getBuilder().SetInsertPoint(entryBlock);
    
    // 清理可能存在的循环变量上下文
    updateContext.clearLoopVariables();
    
    // 处理模块中的函数定义
    for (auto& func : node->getFunctions()) {
        if (func) {
            visit(func.get());
        }
    }
    
    // 处理模块级语句
    for (auto& stmt : node->getStatements()) {
        if (stmt) {
            visit(stmt.get());
        }
    }
    
    // 如果main函数没有返回语句，添加默认返回值0
    if (!codeGen.getBuilder().GetInsertBlock()->getTerminator()) {
        // 创建整数对象并返回
        llvm::Function* createIntFunc = codeGen.getOrCreateExternalFunction(
            "py_create_int",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {llvm::Type::getInt32Ty(codeGen.getContext())}
        );
        
        llvm::Value* intObj = codeGen.getBuilder().CreateCall(
            createIntFunc, 
            {llvm::ConstantInt::get(llvm::Type::getInt32Ty(codeGen.getContext()), 0)},
            "int_obj"
        );
        
        codeGen.getBuilder().CreateRet(intObj);
    }
    
    // 恢复之前的函数上下文
    codeGen.setCurrentFunction(savedFunction);
}




// 优化 FunctionAST 处理
void CodeGenVisitor::visit(FunctionAST* node)
{
    auto* moduleGen = codeGen.getModuleGen();
    auto& updateContext = codeGen.getVariableUpdateContext();

    // 清理之前可能残留的循环变量上下文
    updateContext.clearLoopVariables();

    llvm::Function* function = moduleGen->handleFunctionDef(node);
    if (!function) return;

    // 设置函数为当前函数并处理函数体
    codeGen.setCurrentFunction(function);

    // 获取函数返回类型并设置
    std::shared_ptr<PyType> returnType = moduleGen->resolveReturnType(node);
    if (returnType && returnType->getObjectType())
    {
        codeGen.setCurrentReturnType(returnType->getObjectType());

        // 记录返回类型，方便变量更新系统使用
        updateContext.setVariableType("return", returnType);
    }

    // 创建入口基本块
    llvm::BasicBlock* entryBlock = moduleGen->createFunctionEntry(function);

    // 处理函数参数类型注册
    unsigned argIdx = 0;
    for (auto& arg : function->args())
    {
        if (argIdx < node->params.size())
        {
            std::shared_ptr<PyType> paramType = node->params[argIdx].resolvedType;
            if (paramType)
            {
                updateContext.setVariableType(arg.getName().str(), paramType);
            }
        }
        argIdx++;
    }

    // 处理函数体中的语句
    for (auto& stmt : node->body)
    {
        visit(stmt.get());
    }

    // 确保函数有一个返回语句
    if (!builder_has_terminator(entryBlock))
    {
        // 对于非void函数，添加默认返回值
        if (returnType && !returnType->isVoid())
        {
            // 生成适当类型的默认返回值
            llvm::Value* defaultReturn = generateDefaultValue(codeGen, returnType);
            codeGen.getBuilder().CreateRet(defaultReturn);
        }
        else
        {
            codeGen.getBuilder().CreateRetVoid();
        }
    }

    // 清理函数
    moduleGen->cleanupFunction();
}

// 优化赋值语句处理
void CodeGenVisitor::visit(AssignStmtAST* node)
{
    auto* typeGen = codeGen.getTypeGen();
    auto* exprGen = codeGen.getExprGen();
    auto* runtime = codeGen.getRuntimeGen();
    auto& updateContext = codeGen.getVariableUpdateContext();

    // 获取变量名和新值表达式
    const std::string& varName = node->getName();
    const ExprAST* valueExpr = node->getValue();

    // 获取变量类型和值类型
    ObjectType* varType = codeGen.getSymbolTable().getVariableType(varName);
    std::shared_ptr<PyType> valueType = typeGen->inferExprType(valueExpr);

    // 验证赋值类型兼容性
    if (!typeGen->validateAssignment(varName, valueExpr))
    {
        codeGen.logTypeError("Type error: Cannot assign " + valueType->toString() + " to variable '" + varName + "'",
                             node->line ? *node->line : 0,
                             node->column ? *node->column : 0);
        return;
    }

    // 生成值表达式的代码
    llvm::Value* valueIR = exprGen->handleExpr(valueExpr);
    if (!valueIR) return;

    // 准备用于赋值的值
    PyCodeGen* pyCodeGen = codeGen.asPyCodeGen();
    if (pyCodeGen && codeGen.getSymbolTable().hasVariable(varName))
    {
        valueIR = pyCodeGen->prepareAssignmentTarget(valueIR, varType, valueExpr);
        if (!valueIR) return;
    }

    // 使用变量更新系统处理变量更新
    if (codeGen.getSymbolTable().hasVariable(varName))
    {
        // 更新变量类型记录
        updateContext.setVariableType(varName, valueType);

        // 使用策略模式更新变量
        codeGen.getSymbolTable().updateVariable(codeGen, varName, valueIR, varType, valueType);
    }
    else
    {
        // 第一次赋值，设置变量
        codeGen.getSymbolTable().setVariable(varName, valueIR, valueType->getObjectType());
        updateContext.setVariableType(varName, valueType);

        // 处理引用计数
        if (valueType && valueType->isReference())
        {
            runtime->incRef(valueIR);
        }
    }

    // 标记最后计算的表达式
    codeGen.setLastExprValue(valueIR);
    codeGen.setLastExprType(valueType);

    // 清理临时对象
    runtime->cleanupTemporaryObjects();
}

// 优化 WhileStmt 处理 - 已经有很好的优化，进一步增强
void CodeGenVisitor::visit(WhileStmtAST* node)
{
    auto* stmtGen = codeGen.getStmtGen();
    auto& updateContext = codeGen.getVariableUpdateContext();
    auto* typeGen = codeGen.getTypeGen();
    auto& builder = codeGen.getBuilder();

    // 获取当前函数
    llvm::Function* func = codeGen.getCurrentFunction();
    if (!func) return;

    // 创建必要的基本块
    llvm::BasicBlock* entryBlock = builder.GetInsertBlock();  // 保存入口块
    llvm::BasicBlock* condBlock = llvm::BasicBlock::Create(codeGen.getContext(), "while.cond", func);
    llvm::BasicBlock* bodyBlock = llvm::BasicBlock::Create(codeGen.getContext(), "while.body", func);
    llvm::BasicBlock* afterBlock = llvm::BasicBlock::Create(codeGen.getContext(), "while.end", func);

    // 注册循环块，处理break和continue
    codeGen.pushLoopBlocks(condBlock, afterBlock);

    // 设置变量更新上下文
    updateContext.setLoopContext(condBlock, afterBlock);

    // 保存初始状态的所有变量
    auto initialState = codeGen.getSymbolTable().captureState();

    // 跳转到条件块
    builder.CreateBr(condBlock);

    // 生成条件块代码
    builder.SetInsertPoint(condBlock);

    // 为使用的变量创建PHI节点
    updateContext.createPhiNodesForCurrentLoop(codeGen);

    // 分析条件表达式中使用的变量，确保它们有PHI节点
    analyzeExpressionForLoopVars(node->getCondition(), updateContext, condBlock, entryBlock);

    // 生成条件表达式的代码
    auto* exprGen = codeGen.getExprGen();
    llvm::Value* condValue = stmtGen->handleCondition(node->getCondition());

    if (!condValue)
    {
        // 错误处理时清理上下文
        updateContext.clearLoopContext();
        codeGen.popLoopBlocks();
        return;
    }

    // 创建条件分支
    builder.CreateCondBr(condValue, bodyBlock, afterBlock);

    // 生成循环体代码
    builder.SetInsertPoint(bodyBlock);
    stmtGen->beginScope();

    // 处理循环体中的每个语句
    for (auto& stmt : node->getBody())
    {
        visit(stmt.get());

        // 如果当前块已有终结器，跳出处理
        if (builder.GetInsertBlock()->getTerminator()) break;
    }

    // 结束作用域
    stmtGen->endScope();

    // 应用所有挂起的变量更新 - 确保PHI节点正确更新
    updateContext.applyPendingUpdates();

    // 如果循环体没有终结器，添加到条件块的跳转
    if (!builder.GetInsertBlock()->getTerminator())
    {
        builder.CreateBr(condBlock);
    }

    // 处理嵌套循环变量更新
    updateContext.mergeNestedLoopUpdates();

    // 设置插入点到循环后块
    builder.SetInsertPoint(afterBlock);

    // 计算循环可能修改的变量
    auto finalState = codeGen.getSymbolTable().captureState();
    auto modifiedVars = codeGen.getSymbolTable().getModifiedVars(initialState);

    // 清理循环上下文
    updateContext.clearLoopContext();
    codeGen.popLoopBlocks();
}

// 优化返回语句处理
void CodeGenVisitor::visit(ReturnStmtAST* node)
{
    codeGen.setInReturnStmt(true);

    auto& builder = codeGen.getBuilder();
    auto* runtime = codeGen.getRuntimeGen();
    auto* typeGen = codeGen.getTypeGen();
    auto& updateContext = codeGen.getVariableUpdateContext();

    // 获取函数返回类型
    ObjectType* returnTypeObj = codeGen.getCurrentReturnType();
    std::shared_ptr<PyType> returnType = returnTypeObj ? std::make_shared<PyType>(returnTypeObj) : PyType::getAny();

    // 处理无返回值情况
    if (!node->getValue())
    {
        if (returnTypeObj && !returnType->isVoid() && !returnType->isAny())
        {
            codeGen.logTypeError("Function must return a value of type " + returnType->toString(),
                                 node->line ? *node->line : 0,
                                 node->column ? *node->column : 0);
        }

        builder.CreateRetVoid();
        codeGen.setInReturnStmt(false);
        return;
    }

    // 生成返回值表达式代码
    auto* exprGen = codeGen.getExprGen();
    llvm::Value* returnValue = exprGen->handleExpr(node->getValue());

    if (!returnValue)
    {
        codeGen.setInReturnStmt(false);
        return;
    }

    // 获取表达式类型并进行验证
    std::shared_ptr<PyType> exprType = typeGen->inferExprType(node->getValue());

    // 验证返回类型兼容性
    if (returnTypeObj && !TypeSafetyChecker::isTypeCompatible(exprType, returnType))
    {
        codeGen.logTypeError("Cannot return " + exprType->toString() + " from function expecting " + returnType->toString(),
                             node->line ? *node->line : 0,
                             node->column ? *node->column : 0);
    }

    // 准备返回值 - 进行必要的类型转换和生命周期管理
    llvm::Value* preparedValue = runtime->prepareReturnValue(
            returnValue, exprType, returnType);

    // 创建返回指令
    builder.CreateRet(preparedValue);

    // 重置返回语句标记
    codeGen.setInReturnStmt(false);
}

// 优化打印语句处理
void CodeGenVisitor::visit(PrintStmtAST* node)
{
    auto& builder = codeGen.getBuilder();
    auto* exprGen = codeGen.getExprGen();
    auto* typeGen = codeGen.getTypeGen();
    auto* runtime = codeGen.getRuntimeGen();

    // 生成表达式代码
    llvm::Value* exprValue = exprGen->handleExpr(node->getValue());
    if (!exprValue) return;

    // 获取表达式类型
    std::shared_ptr<PyType> exprType = typeGen->inferExprType(node->getValue());

    // 使用类型操作系统来处理打印
    if (exprType->isReference())
    {
        // 对于引用类型，需要转换为字符串
        llvm::Function* toStringFunc = codeGen.getOrCreateExternalFunction(
                "py_convert_to_string",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0)});

        llvm::Value* strObj = builder.CreateCall(toStringFunc, {exprValue}, "str_obj");

        // 提取字符串指针
        llvm::Function* extractStrFunc = codeGen.getOrCreateExternalFunction(
                "py_extract_string",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0)});

        llvm::Value* strPtr = builder.CreateCall(extractStrFunc, {strObj}, "str_ptr");

        // 打印字符串
        llvm::Function* printStrFunc = codeGen.getOrCreateExternalFunction(
                "py_print_string",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0)});

        builder.CreateCall(printStrFunc, {strPtr});

        // 减少临时字符串对象的引用计数
        runtime->decRef(strObj);
    }
    else if (exprType->isInt())
    {
        // 直接打印整数
        llvm::Function* printIntFunc = codeGen.getOrCreateExternalFunction(
                "py_print_int",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::Type::getInt32Ty(codeGen.getContext())});

        // 提取整数值
        llvm::Function* extractIntFunc = codeGen.getOrCreateExternalFunction(
                "py_extract_int",
                llvm::Type::getInt32Ty(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0)});

        llvm::Value* intVal = builder.CreateCall(extractIntFunc, {exprValue});
        builder.CreateCall(printIntFunc, {intVal});
    }
    else if (exprType->isDouble())
    {
        // 直接打印浮点数
        llvm::Function* printDoubleFunc = codeGen.getOrCreateExternalFunction(
                "py_print_double",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::Type::getDoubleTy(codeGen.getContext())});

        // 提取浮点值
        llvm::Function* extractDoubleFunc = codeGen.getOrCreateExternalFunction(
                "py_extract_double",
                llvm::Type::getDoubleTy(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0)});

        llvm::Value* doubleVal = builder.CreateCall(extractDoubleFunc, {exprValue});
        builder.CreateCall(printDoubleFunc, {doubleVal});
    }
    else
    {
        // 默认转为字符串处理
        llvm::Function* toStringFunc = codeGen.getOrCreateExternalFunction(
                "py_convert_to_string",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0)});

        llvm::Value* strObj = builder.CreateCall(toStringFunc, {exprValue}, "str_obj");

        // 提取字符串指针
        llvm::Function* extractStrFunc = codeGen.getOrCreateExternalFunction(
                "py_extract_string",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0)});

        llvm::Value* strPtr = builder.CreateCall(extractStrFunc, {strObj}, "str_ptr");

        // 打印字符串
        llvm::Function* printStrFunc = codeGen.getOrCreateExternalFunction(
                "py_print_string",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0)});

        builder.CreateCall(printStrFunc, {strPtr});

        // 减少临时字符串对象的引用计数
        runtime->decRef(strObj);
    }

    // 清理临时对象
    runtime->cleanupTemporaryObjects();
}

// 索引赋值语句处理优化
void CodeGenVisitor::visit(IndexAssignStmtAST* node)
{
    auto* typeGen = codeGen.getTypeGen();
    auto* exprGen = codeGen.getExprGen();
    auto* runtime = codeGen.getRuntimeGen();
    auto& builder = codeGen.getBuilder();

    // 生成目标表达式（被索引的容器）
    llvm::Value* targetValue = exprGen->handleExpr(node->getTarget());
    if (!targetValue) return;

    // 生成索引表达式
    llvm::Value* indexValue = exprGen->handleExpr(node->getIndex());
    if (!indexValue) return;

    // 生成值表达式
    llvm::Value* valueExpr = exprGen->handleExpr(node->getValue());
    if (!valueExpr) return;

    // 获取类型信息
    std::shared_ptr<PyType> targetType = typeGen->inferExprType(node->getTarget());
    std::shared_ptr<PyType> indexType = typeGen->inferExprType(node->getIndex());
    std::shared_ptr<PyType> valueType = typeGen->inferExprType(node->getValue());

    // 验证索引操作
    if (!TypeSafetyChecker::checkIndexOperation(targetType, indexType))
    {
        codeGen.logTypeError("Invalid index type " + indexType->toString() + " for container type " + targetType->toString(),
                             node->line ? *node->line : 0,
                             node->column ? *node->column : 0);
        return;
    }

    // 根据容器类型处理索引赋值
    if (targetType->isList())
    {
        // 处理列表赋值
        // 获取列表元素类型
        std::shared_ptr<PyType> elemType = PyType::getListElementType(targetType);

        // 验证值类型兼容性
        if (!TypeSafetyChecker::checkAssignmentCompatibility(elemType, valueType))
        {
            codeGen.logTypeError("Cannot assign " + valueType->toString() + " to list element of type " + elemType->toString(),
                                 node->line ? *node->line : 0,
                                 node->column ? *node->column : 0);
            return;
        }

        // 准备索引值 - 确保是整数
        llvm::Value* preparedIndex = OperationCodeGenerator::prepareIndexValue(
                codeGen, indexValue, OperationCodeGenerator::getTypeId(indexType->getObjectType()));

        // 获取设置元素函数
        llvm::Function* setItemFunc = codeGen.getOrCreateExternalFunction(
                "py_list_set_item",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::Type::getInt32Ty(codeGen.getContext()),
                 llvm::PointerType::get(codeGen.getContext(), 0)});

        // 对于引用类型的值，增加引用计数
        if (valueType->isReference())
        {
            runtime->incRef(valueExpr);
        }

        // 设置列表元素
        builder.CreateCall(setItemFunc, {targetValue, preparedIndex, valueExpr});
    }
    else if (targetType->isDict())
    {
        // 处理字典赋值
        // 获取字典值类型
        std::shared_ptr<PyType> valueTypeFromDict = PyType::getDictValueType(targetType);

        // 验证值类型兼容性
        if (!TypeSafetyChecker::checkAssignmentCompatibility(valueTypeFromDict, valueType))
        {
            codeGen.logTypeError("Cannot assign " + valueType->toString() + " to dictionary value of type " + valueTypeFromDict->toString(),
                                 node->line ? *node->line : 0,
                                 node->column ? *node->column : 0);
            return;
        }

        // 获取设置项函数
        llvm::Function* setItemFunc = codeGen.getOrCreateExternalFunction(
                "py_dict_set_item",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::PointerType::get(codeGen.getContext(), 0)});

        // 对于引用类型的值，增加引用计数
        if (valueType->isReference())
        {
            runtime->incRef(valueExpr);
        }

        // 设置字典项
        builder.CreateCall(setItemFunc, {targetValue, indexValue, valueExpr});
    }
    else
    {
        // 使用通用索引设置操作
        llvm::Function* setItemFunc = codeGen.getOrCreateExternalFunction(
                "py_object_set_index",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::PointerType::get(codeGen.getContext(), 0)});

        // 对于引用类型的值，增加引用计数
        if (valueType->isReference())
        {
            runtime->incRef(valueExpr);
        }

        // 设置通用索引
        builder.CreateCall(setItemFunc, {targetValue, indexValue, valueExpr});
    }

    // 清理临时对象
    runtime->cleanupTemporaryObjects();
}

// 优化条件语句处理
void CodeGenVisitor::visit(IfStmtAST* node)
{
    auto* stmtGen = codeGen.getStmtGen();
    auto* typeGen = codeGen.getTypeGen();
    auto& builder = codeGen.getBuilder();

    // 生成条件表达式代码
    llvm::Value* condValue = stmtGen->handleCondition(node->getCondition());
    if (!condValue) return;

    // 获取当前函数
    llvm::Function* func = codeGen.getCurrentFunction();
    if (!func) return;

    // 创建必要的基本块
    llvm::BasicBlock* thenBlock = llvm::BasicBlock::Create(codeGen.getContext(), "if.then", func);
    llvm::BasicBlock* elseBlock = nullptr;
    llvm::BasicBlock* afterBlock = nullptr;

    // 处理有else分支的情况
    if (!node->getElseBody().empty())
    {
        elseBlock = llvm::BasicBlock::Create(codeGen.getContext(), "if.else", func);
        afterBlock = llvm::BasicBlock::Create(codeGen.getContext(), "if.end", func);
        builder.CreateCondBr(condValue, thenBlock, elseBlock);
    }
    else
    {
        afterBlock = llvm::BasicBlock::Create(codeGen.getContext(), "if.end", func);
        builder.CreateCondBr(condValue, thenBlock, afterBlock);
    }

    // 捕获当前变量状态
    auto beforeIfState = codeGen.getSymbolTable().captureState();

    // 生成then块代码
    builder.SetInsertPoint(thenBlock);
    stmtGen->beginScope();

    for (auto& stmt : node->getThenBody())
    {
        visit(stmt.get());
        // 如果当前块已有终结器，跳出处理
        if (builder.GetInsertBlock()->getTerminator()) break;
    }

    // 捕获then分支执行后的变量状态
    auto thenState = codeGen.getSymbolTable().captureState();
    auto thenModifiedVars = codeGen.getSymbolTable().getModifiedVars(beforeIfState);

    stmtGen->endScope();

    // 如果then块没有终结器，添加到after块的跳转
    bool thenFallsThrough = !builder.GetInsertBlock()->getTerminator();
    if (thenFallsThrough)
    {
        builder.CreateBr(afterBlock);
    }

    // 生成else块代码（如果有）
    std::map<std::string, llvm::Value*> elseState;
    std::map<std::string, llvm::Value*> elseModifiedVars;
    bool elseFallsThrough = false;

    if (elseBlock)
    {
        builder.SetInsertPoint(elseBlock);
        stmtGen->beginScope();

        for (auto& stmt : node->getElseBody())
        {
            visit(stmt.get());
            // 如果当前块已有终结器，跳出处理
            if (builder.GetInsertBlock()->getTerminator()) break;
        }

        // 捕获else分支执行后的变量状态
        elseState = codeGen.getSymbolTable().captureState();
        elseModifiedVars = codeGen.getSymbolTable().getModifiedVars(beforeIfState);

        stmtGen->endScope();

        // 如果else块没有终结器，添加到after块的跳转
        elseFallsThrough = !builder.GetInsertBlock()->getTerminator();
        if (elseFallsThrough)
        {
            builder.CreateBr(afterBlock);
        }
    }

    // 设置插入点到after块
    if ((thenFallsThrough || elseFallsThrough) && !afterBlock->getParent())
    {
        // 使用公共API添加基本块
        func->insert(func->end(), afterBlock);
    }

    builder.SetInsertPoint(afterBlock);

    // 处理分支修改的变量，创建PHI节点处理变量合并
    if (thenFallsThrough && (elseFallsThrough || !elseBlock))
    {
        // 如果both分支都有落入afterBlock的可能，需要为修改的变量创建PHI节点
        std::set<std::string> modifiedVarNames;

        // 收集所有被修改的变量名
        for (const auto& pair : thenModifiedVars)
        {
            modifiedVarNames.insert(pair.first);
        }

        for (const auto& pair : elseModifiedVars)
        {
            modifiedVarNames.insert(pair.first);
        }

        // 为每个被修改的变量创建PHI节点
        for (const std::string& varName : modifiedVarNames)
        {
            // 只处理普通变量，不处理函数参数
            if (codeGen.getSymbolTable().hasVariable(varName))
            {
                llvm::Value* beforeValue = beforeIfState[varName];
                llvm::Value* thenValue = thenState[varName];
                llvm::Value* elseValue = elseBlock ? elseState[varName] : beforeValue;

                // 如果值确实发生了变化
                if ((thenValue != beforeValue) || (elseValue != beforeValue))
                {
                    llvm::PHINode* phi = builder.CreatePHI(beforeValue->getType(), 2, varName + ".phi");

                    // 添加then分支的incoming值
                    if (thenFallsThrough)
                    {
                        phi->addIncoming(thenValue, thenBlock);
                    }

                    // 添加else分支的incoming值
                    if (elseBlock)
                    {
                        if (elseFallsThrough)
                        {
                            phi->addIncoming(elseValue, elseBlock);
                        }
                    }
                    else
                    {
                        phi->addIncoming(beforeValue, builder.GetInsertBlock()->getPrevNode());
                    }

                    // 更新变量值为PHI节点
                    codeGen.getSymbolTable().setVariable(varName, phi,
                                                         codeGen.getSymbolTable().getVariableType(varName));
                }
            }
        }
    }
}

// 辅助函数：检查基本块是否有终结器指令
bool CodeGenVisitor::builder_has_terminator(llvm::BasicBlock* block)
{
    return block && block->getTerminator();
}

// 辅助函数：为循环生成默认值
llvm::Value* CodeGenVisitor::generateDefaultValue(CodeGenBase& codeGen, std::shared_ptr<PyType> type)
{
    if (!type) return nullptr;

    auto& builder = codeGen.getBuilder();
    auto& context = codeGen.getContext();

    if (type->isInt())
    {
        // 创建默认整数 0
        llvm::Function* createIntFunc = codeGen.getOrCreateExternalFunction(
                "py_create_int",
                llvm::PointerType::get(context, 0),
                {llvm::Type::getInt32Ty(context)});
        return builder.CreateCall(createIntFunc, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0)});
    }
    else if (type->isDouble())
    {
        // 创建默认浮点数 0.0
        llvm::Function* createDoubleFunc = codeGen.getOrCreateExternalFunction(
                "py_create_double",
                llvm::PointerType::get(context, 0),
                {llvm::Type::getDoubleTy(context)});
        return builder.CreateCall(createDoubleFunc, {llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), 0.0)});
    }
    else if (type->isBool())
    {
        // 创建默认布尔值 False
        llvm::Function* createBoolFunc = codeGen.getOrCreateExternalFunction(
                "py_create_bool",
                llvm::PointerType::get(context, 0),
                {llvm::Type::getInt1Ty(context)});
        return builder.CreateCall(createBoolFunc, {llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0)});
    }
    else if (type->isString())
    {
        // 创建默认空字符串
        llvm::Function* createStringFunc = codeGen.getOrCreateExternalFunction(
                "py_create_string",
                llvm::PointerType::get(context, 0),
                {llvm::PointerType::get(context, 0)});
        return builder.CreateCall(createStringFunc, {builder.CreateGlobalString("")});
    }
    else if (type->isList())
    {
        // 创建默认空列表
        llvm::Function* createListFunc = codeGen.getOrCreateExternalFunction(
                "py_create_list",
                llvm::PointerType::get(context, 0),
                {llvm::Type::getInt32Ty(context), llvm::Type::getInt32Ty(context)});

        // 获取元素类型ID
        std::shared_ptr<PyType> elemType = PyType::getListElementType(type);
        int elemTypeId = elemType ? OperationCodeGenerator::getTypeId(elemType->getObjectType()) : PY_TYPE_ANY;

        return builder.CreateCall(createListFunc, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
                                                   llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), elemTypeId)});
    }
    else
    {
        // 对于其他类型或Any，返回None
        llvm::Function* getNoneFunc = codeGen.getOrCreateExternalFunction(
                "py_get_none",
                llvm::PointerType::get(context, 0),
                {});
        return builder.CreateCall(getNoneFunc);
    }
}

// 分析表达式中使用的变量，确保在循环中创建PHI节点
void CodeGenVisitor::analyzeExpressionForLoopVars(
        const ExprAST* expr,
        VariableUpdateContext& updateContext,
        llvm::BasicBlock* loopHeader,
        llvm::BasicBlock* entryBlock)
{
    if (!expr) return;

    if (expr->kind() == ASTKind::VariableExpr)
    {
        // 处理变量引用
        const VariableExprAST* varExpr = static_cast<const VariableExprAST*>(expr);
        const std::string& varName = varExpr->getName();

        // 如果变量已在符号表中但尚未在循环上下文中注册
        if (codeGen.getSymbolTable().hasVariable(varName) && !updateContext.isVariableInLoop(varName))
        {
            // 获取变量当前值
            llvm::Value* value = codeGen.getSymbolTable().getVariable(varName);
            if (!value) return;

            // 创建PHI节点
            auto& builder = codeGen.getBuilder();
            llvm::IRBuilder<> tempBuilder(loopHeader, loopHeader->begin());
            llvm::PHINode* phi = tempBuilder.CreatePHI(value->getType(), 2, varName + ".phi");

            // 添加初始值
            phi->addIncoming(value, entryBlock);

            // 注册为循环变量
            updateContext.registerLoopVariable(varName, phi);

            // 更新符号表中的变量值为PHI节点
            codeGen.getSymbolTable().setVariable(varName, phi,
                                                 codeGen.getSymbolTable().getVariableType(varName));
        }
    }

    // 递归处理二元表达式
    if (expr->kind() == ASTKind::BinaryExpr)
    {
        const BinaryExprAST* binExpr = static_cast<const BinaryExprAST*>(expr);
        analyzeExpressionForLoopVars(binExpr->getLHS(), updateContext, loopHeader, entryBlock);
        analyzeExpressionForLoopVars(binExpr->getRHS(), updateContext, loopHeader, entryBlock);
    }

    // 递归处理一元表达式
    if (expr->kind() == ASTKind::UnaryExpr)
    {
        const UnaryExprAST* unaryExpr = static_cast<const UnaryExprAST*>(expr);
        analyzeExpressionForLoopVars(unaryExpr->getOperand(), updateContext, loopHeader, entryBlock);
    }

    // 递归处理调用表达式的参数
    if (expr->kind() == ASTKind::CallExpr)
    {
        const CallExprAST* callExpr = static_cast<const CallExprAST*>(expr);
        for (const auto& arg : callExpr->getArgs())
        {
            analyzeExpressionForLoopVars(arg.get(), updateContext, loopHeader, entryBlock);
        }
    }

    // 递归处理索引表达式
    if (expr->kind() == ASTKind::IndexExpr)
    {
        const IndexExprAST* indexExpr = static_cast<const IndexExprAST*>(expr);
        analyzeExpressionForLoopVars(indexExpr->getTarget(), updateContext, loopHeader, entryBlock);
        analyzeExpressionForLoopVars(indexExpr->getIndex(), updateContext, loopHeader, entryBlock);
    }
}

}  // namespace llvmpy