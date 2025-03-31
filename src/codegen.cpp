#include "codegen.h"
#include <iostream>
#include <sstream>
#include <string>
#include <cmath>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>
#include "Debugdefine.h"

namespace llvmpy
{

//===----------------------------------------------------------------------===//
// PyCodeGenError 实现
//===----------------------------------------------------------------------===//

std::string PyCodeGenError::formatError() const
{
    std::stringstream ss;
    ss << (isTypeErr() ? "Type error" : "Code generation error");
    if (line >= 0) ss << " at line " << line;
    if (column >= 0) ss << ", column " << column;
    ss << ": " << what();
    return ss.str();
}

//===----------------------------------------------------------------------===//
// PyScope 实现
//===----------------------------------------------------------------------===//

bool PyScope::hasVariable(const std::string& name) const
{
    return variables.find(name) != variables.end() || (parent && parent->hasVariable(name));
}

llvm::Value* PyScope::getVariable(const std::string& name)
{
    auto it = variables.find(name);
    if (it != variables.end())
    {
        return it->second;
    }
    if (parent)
    {
        return parent->getVariable(name);
    }
    return nullptr;
}

void PyScope::setVariable(const std::string& name, llvm::Value* value, ObjectType* type)
{
    variables[name] = value;
    if (type)
    {
        variableTypes[name] = type;
    }
}

ObjectType* PyScope::getVariableType(const std::string& name)
{
    auto it = variableTypes.find(name);
    if (it != variableTypes.end())
    {
        return it->second;
    }
    if (parent)
    {
        return parent->getVariableType(name);
    }
    return nullptr;
}

//===----------------------------------------------------------------------===//
// PySymbolTable 实现
//===----------------------------------------------------------------------===//

PySymbolTable::PySymbolTable()
{
    // 创建全局作用域
    pushScope();
}

PyScope* PySymbolTable::currentScope()
{
    if (scopes.empty())
    {
        pushScope();  // 确保总是有作用域
    }
    return scopes.top().get();
}

void PySymbolTable::pushScope()
{
    PyScope* parent = scopes.empty() ? nullptr : scopes.top().get();
    scopes.push(std::make_unique<PyScope>(parent));
}

void PySymbolTable::popScope()
{
    if (scopes.size() > 1)
    {  // 保留全局作用域
        scopes.pop();
    }
}

bool PySymbolTable::hasVariable(const std::string& name) const
{
    if (scopes.empty()) return false;
    return scopes.top()->hasVariable(name);
}

llvm::Value* PySymbolTable::getVariable(const std::string& name)
{
    if (scopes.empty()) return nullptr;
    return scopes.top()->getVariable(name);
}

void PySymbolTable::setVariable(const std::string& name, llvm::Value* value, ObjectType* type)
{
    if (scopes.empty()) pushScope();
    scopes.top()->setVariable(name, value, type);
}

ObjectType* PySymbolTable::getVariableType(const std::string& name)
{
    if (scopes.empty()) return nullptr;
    return scopes.top()->getVariableType(name);
}

//===----------------------------------------------------------------------===//
// PyCodeGen 静态成员和初始化
//===----------------------------------------------------------------------===//

PyCodeGenRegistry<ASTKind, PyNodeHandlerFunc> PyCodeGen::nodeHandlers;
PyCodeGenRegistry<ASTKind, PyExprHandlerFunc> PyCodeGen::exprHandlers;
PyCodeGenRegistry<ASTKind, PyStmtHandlerFunc> PyCodeGen::stmtHandlers;
PyCodeGenRegistry<char, PyBinOpHandlerFunc> PyCodeGen::binOpHandlers;
bool PyCodeGen::isInitialized = false;

void PyCodeGen::initializeHandlers()
{
    if (isInitialized) return;

    // 注册表达式处理器
    exprHandlers.registerHandler(ASTKind::NumberExpr, [](PyCodeGen& cg, ExprAST* expr)
                                 {
        auto* numExpr = static_cast<NumberExprAST*>(expr);
        cg.visit(numExpr);
        return cg.getLastExprValue(); });

    exprHandlers.registerHandler(ASTKind::StringExpr, [](PyCodeGen& cg, ExprAST* expr)
                                 {
        auto* strExpr = static_cast<StringExprAST*>(expr);
        cg.visit(strExpr);
        return cg.getLastExprValue(); });

    exprHandlers.registerHandler(ASTKind::BoolExpr, [](PyCodeGen& cg, ExprAST* expr)
                                 {
        auto* boolExpr = static_cast<BoolExprAST*>(expr);
        cg.visit(boolExpr);
        return cg.getLastExprValue(); });

    exprHandlers.registerHandler(ASTKind::NoneExpr, [](PyCodeGen& cg, ExprAST* expr)
                                 {
        auto* noneExpr = static_cast<NoneExprAST*>(expr);
        cg.visit(noneExpr);
        return cg.getLastExprValue(); });

    exprHandlers.registerHandler(ASTKind::VariableExpr, [](PyCodeGen& cg, ExprAST* expr)
                                 {
        auto* varExpr = static_cast<VariableExprAST*>(expr);
        cg.visit(varExpr);
        return cg.getLastExprValue(); });

    exprHandlers.registerHandler(ASTKind::BinaryExpr, [](PyCodeGen& cg, ExprAST* expr)
                                 {
        auto* binExpr = static_cast<BinaryExprAST*>(expr);
        cg.visit(binExpr);
        return cg.getLastExprValue(); });

    exprHandlers.registerHandler(ASTKind::UnaryExpr, [](PyCodeGen& cg, ExprAST* expr)
                                 {
        auto* unExpr = static_cast<UnaryExprAST*>(expr);
        cg.visit(unExpr);
        return cg.getLastExprValue(); });

    exprHandlers.registerHandler(ASTKind::CallExpr, [](PyCodeGen& cg, ExprAST* expr)
                                 {
        auto* callExpr = static_cast<CallExprAST*>(expr);
        cg.visit(callExpr);
        return cg.getLastExprValue(); });

    exprHandlers.registerHandler(ASTKind::ListExpr, [](PyCodeGen& cg, ExprAST* expr)
                                 {
        auto* listExpr = static_cast<ListExprAST*>(expr);
        cg.visit(listExpr);
        return cg.getLastExprValue(); });

    exprHandlers.registerHandler(ASTKind::IndexExpr, [](PyCodeGen& cg, ExprAST* expr)
                                 {
        auto* indexExpr = static_cast<IndexExprAST*>(expr);
        cg.visit(indexExpr);
        return cg.getLastExprValue(); });

    // 注册语句处理器
    stmtHandlers.registerHandler(ASTKind::ExprStmt, [](PyCodeGen& cg, StmtAST* stmt)
                                 {
        auto* exprStmt = static_cast<ExprStmtAST*>(stmt);
        cg.visit(exprStmt); });

    stmtHandlers.registerHandler(ASTKind::ReturnStmt, [](PyCodeGen& cg, StmtAST* stmt)
                                 {
        auto* retStmt = static_cast<ReturnStmtAST*>(stmt);
        cg.visit(retStmt); });

    stmtHandlers.registerHandler(ASTKind::IfStmt, [](PyCodeGen& cg, StmtAST* stmt)
                                 {
        auto* ifStmt = static_cast<IfStmtAST*>(stmt);
        cg.visit(ifStmt); });

    stmtHandlers.registerHandler(ASTKind::WhileStmt, [](PyCodeGen& cg, StmtAST* stmt)
                                 {
        auto* whileStmt = static_cast<WhileStmtAST*>(stmt);
        cg.visit(whileStmt); });

    stmtHandlers.registerHandler(ASTKind::PrintStmt, [](PyCodeGen& cg, StmtAST* stmt)
                                 {
        auto* printStmt = static_cast<PrintStmtAST*>(stmt);
        cg.visit(printStmt); });

    stmtHandlers.registerHandler(ASTKind::AssignStmt, [](PyCodeGen& cg, StmtAST* stmt)
                                 {
        auto* assignStmt = static_cast<AssignStmtAST*>(stmt);
        cg.visit(assignStmt); });

    stmtHandlers.registerHandler(ASTKind::PassStmt, [](PyCodeGen& cg, StmtAST* stmt)
                                 {
        auto* passStmt = static_cast<PassStmtAST*>(stmt);
        cg.visit(passStmt); });

    stmtHandlers.registerHandler(ASTKind::ImportStmt, [](PyCodeGen& cg, StmtAST* stmt)
                                 {
        auto* importStmt = static_cast<ImportStmtAST*>(stmt);
        cg.visit(importStmt); });

    stmtHandlers.registerHandler(ASTKind::ClassStmt, [](PyCodeGen& cg, StmtAST* stmt)
                                 {
        auto* classStmt = static_cast<ClassStmtAST*>(stmt);
        cg.visit(classStmt); });

    stmtHandlers.registerHandler(ASTKind::IndexAssignStmt, [](PyCodeGen& cg, StmtAST* stmt)
                                 {
        auto* idxAssignStmt = static_cast<IndexAssignStmtAST*>(stmt);
        cg.visit(idxAssignStmt); });

    // 注册二元操作符处理器
    binOpHandlers.registerHandler('+', [](PyCodeGen& cg, llvm::Value* L, llvm::Value* R, char op)
                                  {
        // 使用TypeOperations系统来处理加法
        auto typeL = cg.getLastExprType() ? cg.getLastExprType()->getObjectType() : nullptr;
        auto typeR = cg.getLastExprType() ? cg.getLastExprType()->getObjectType() : nullptr;
        return cg.generateBinaryOperation(op, L, R, typeL, typeR); });

    // 注册对函数和模块的处理
    nodeHandlers.registerHandler(ASTKind::Function, [](PyCodeGen& cg, ASTNode* node)
                                 {
        auto* func = static_cast<FunctionAST*>(node);
        cg.visit(func);
        return cg.getCurrentFunction(); });

    nodeHandlers.registerHandler(ASTKind::Module, [](PyCodeGen& cg, ASTNode* node)
                                 {
        auto* module = static_cast<ModuleAST*>(node);
        cg.visit(module);
        return static_cast<llvm::Value*>(nullptr); });

    isInitialized = true;
}

//===----------------------------------------------------------------------===//
// PyCodeGen 构造与辅助方法
//===----------------------------------------------------------------------===//

PyCodeGen::PyCodeGen()
    : context(std::make_unique<llvm::LLVMContext>()),
      module(std::make_unique<llvm::Module>("llvmpy_module", *context)),
      builder(std::make_unique<llvm::IRBuilder<>>(*context)),
      currentFunction(nullptr),
      currentReturnType(nullptr),
      lastExprValue(nullptr),
      lastExprType(nullptr),
      inReturnStmt(false),
      savedBlock(nullptr)
{
    // 确保注册表已初始化
    ensureInitialized();

    // 确保类型系统已初始化
    TypeRegistry::getInstance().ensureBasicTypesRegistered();

    // 初始化类型操作注册表
    TypeOperationRegistry::getInstance();

    // 创建运行时支持
    runtime = std::make_unique<ObjectRuntime>(module.get(), builder.get());

    // 初始化管理器
    typeSafetyManager = std::make_unique<TypeSafetyManager>();
    lifecycleManager = std::make_unique<CodeGenLifecycleManager>();
    operationManager = std::make_unique<CodeGenOperationManager>();
}

PyCodeGen::PyCodeGen(llvm::Module* mod, llvm::IRBuilder<>* b, llvm::LLVMContext* ctx, ObjectRuntime* rt)
    : currentFunction(nullptr),
      currentReturnType(nullptr),
      lastExprValue(nullptr),
      lastExprType(nullptr),
      inReturnStmt(false),
      savedBlock(nullptr)
{
    // 正确处理Context - 创建新的而不是直接赋值
    if (ctx)
    {
        context = std::make_unique<llvm::LLVMContext>();
        // 不能直接赋值LLVMContext，它没有拷贝构造函数
    }

    // 设置模块
    if (mod)
    {
        module = std::unique_ptr<llvm::Module>(mod);
    }
    else if (rt)
    {
        module = std::unique_ptr<llvm::Module>(rt->getModule());
    }
    else
    {
        // 如果没有提供模块和运行时，创建新的
        if (!context)
        {
            context = std::make_unique<llvm::LLVMContext>();
        }
        module = std::make_unique<llvm::Module>("llvmpy_module", getContext());
    }

    // 设置Builder
    if (b)
    {
        builder = std::unique_ptr<llvm::IRBuilder<>>(b);
    }
    else if (rt)
    {
        builder = std::unique_ptr<llvm::IRBuilder<>>(rt->getBuilder());
    }
    else
    {
        // 如果没有提供builder和运行时，创建新的
        if (!context)
        {
            context = std::make_unique<llvm::LLVMContext>();
        }
        builder = std::make_unique<llvm::IRBuilder<>>(getContext());
    }

    // 设置运行时
    if (rt)
    {
        runtime.reset(rt);
    }
    else if (module && builder)
    {
        runtime = std::make_unique<ObjectRuntime>(module.get(), builder.get());
    }

    // 确保注册表已初始化
    ensureInitialized();

    // 确保类型系统已初始化
    TypeRegistry::getInstance().ensureBasicTypesRegistered();

    // 初始化类型操作注册表
    TypeOperationRegistry::getInstance();

    // 初始化管理器
    typeSafetyManager = std::make_unique<TypeSafetyManager>();
    lifecycleManager = std::make_unique<CodeGenLifecycleManager>();
    operationManager = std::make_unique<CodeGenOperationManager>();
}

void PyCodeGen::ensureInitialized()
{
    if (!isInitialized)
    {
        initializeHandlers();
    }
}

llvm::Value* PyCodeGen::logError(const std::string& message, int line, int column)
{
    throw PyCodeGenError(message, line, column);
    return nullptr;
}

llvm::Value* PyCodeGen::logTypeError(const std::string& message, int line, int column)
{
    throw PyCodeGenError(message, line, column, true);
    return nullptr;
}

llvm::Function* PyCodeGen::getOrCreateExternalFunction(
    const std::string& name,
    llvm::Type* returnType,
    std::vector<llvm::Type*> paramTypes,
    bool isVarArg)
{
    // 先从缓存查找
    auto it = externalFunctions.find(name);
    if (it != externalFunctions.end())
    {
        return it->second;
    }

    // 检查模块中是否已存在
    llvm::Function* func = module->getFunction(name);

    if (!func)
    {
        // 创建函数类型
        llvm::FunctionType* funcType = llvm::FunctionType::get(
            returnType, paramTypes, isVarArg);

        // 创建函数声明
        func = llvm::Function::Create(
            funcType, llvm::Function::ExternalLinkage, name, module.get());

        // 缓存函数
        externalFunctions[name] = func;
    }
    else
    {
        // 已存在但不在缓存中，加入缓存
        externalFunctions[name] = func;

        // 验证返回类型和参数类型是否匹配
        llvm::FunctionType* existingType = func->getFunctionType();

        // 简单检查返回类型是否匹配
        if (existingType->getReturnType() != returnType)
        {
            std::cerr << "Warning: Return type mismatch for function " << name << std::endl;
        }

        // 检查参数数量
        if (existingType->getNumParams() != paramTypes.size())
        {
            std::cerr << "Warning: Parameter count mismatch for function " << name << std::endl;
        }
    }
    debugFunctionReuse(name, func);
    return func;
}

//===----------------------------------------------------------------------===//
// 循环控制与基本块管理
//===----------------------------------------------------------------------===//

void PyCodeGen::pushLoopBlocks(llvm::BasicBlock* condBlock, llvm::BasicBlock* afterBlock)
{
    loopStack.push({condBlock, afterBlock});
}

void PyCodeGen::popLoopBlocks()
{
    if (!loopStack.empty())
    {
        loopStack.pop();
    }
}

PyCodeGen::LoopInfo* PyCodeGen::getCurrentLoop()
{
    if (loopStack.empty()) return nullptr;
    return &loopStack.top();
}

llvm::BasicBlock* PyCodeGen::createBasicBlock(const std::string& name, llvm::Function* parent)
{
    if (!parent)
    {
        parent = currentFunction;
    }
    if (!parent)
    {
        return logError("Cannot create basic block outside of function"), nullptr;
    }
    return llvm::BasicBlock::Create(getContext(), name, parent);
}

//===----------------------------------------------------------------------===//
// 临时对象管理
//===----------------------------------------------------------------------===//

void PyCodeGen::addTempObject(llvm::Value* obj, ObjectType* type)
{
    if (obj && type && type->hasFeature("reference"))
    {
        tempObjects.push_back({obj, type});
    }
}

void PyCodeGen::releaseTempObjects()
{
    for (auto& [value, type] : tempObjects)
    {
        if (value && type && type->hasFeature("reference"))
        {
            // 使用运行时减少引用计数
            runtime->decRef(value);
        }
    }
    clearTempObjects();
}

void PyCodeGen::clearTempObjects()
{
    tempObjects.clear();
}

//===----------------------------------------------------------------------===//
// 类型处理
//===----------------------------------------------------------------------===//

llvm::Type* PyCodeGen::getLLVMType(ObjectType* type)
{
    if (!type) return nullptr;

    // 使用正确的方法检查是否为原始类型
    if (type->getCategory() == ObjectType::Primitive)
    {
        switch (type->getTypeId())
        {
            case PY_TYPE_INT:
                return llvm::Type::getInt32Ty(getContext());
            case PY_TYPE_DOUBLE:
                return llvm::Type::getDoubleTy(getContext());
            case PY_TYPE_BOOL:
                return llvm::Type::getInt1Ty(getContext());
            case PY_TYPE_NONE:
                return llvm::Type::getVoidTy(getContext());
            default:
                break;
        }
    }

    // 引用类型
    if (type->hasFeature("reference"))
    {
        return llvm::PointerType::get(getContext(), 0);
    }

    // 默认情况
    return llvm::PointerType::get(getContext(), 0);
}
// 修复 generateModule 方法，避免 'imports' 成员问题
bool PyCodeGen::generateModule(ModuleAST* mod, const std::string& filename)
{
    if (!mod) return false;

    // 设置模块名
    const std::string& moduleName = mod->getModuleName();
    if (module)
    {
        module->setModuleIdentifier(moduleName);
    }

    // 处理顶层语句
    const auto& stmts = mod->getTopLevelStmts();
    for (const auto& stmt : stmts)
    {
        handleStmt(const_cast<StmtAST*>(stmt.get()));
    }

    // 处理函数定义
    const auto& funcs = mod->getFunctions();
    for (const auto& func : funcs)
    {
        handleNode(const_cast<FunctionAST*>(func.get()));
    }

    // 验证模块
    std::string errorInfo;
    llvm::raw_string_ostream errorStream(errorInfo);
    if (llvm::verifyModule(*module, &errorStream))
    {
        std::cerr << "Error in module: " << errorInfo << std::endl;
        return false;
    }

    // 打印模块IR
    if (!filename.empty())
    {
        std::error_code EC;
        llvm::raw_fd_ostream dest(filename, EC);
        if (EC)
        {
            std::cerr << "Could not open file: " << EC.message() << std::endl;
            return false;
        }
        module->print(dest, nullptr);
    }

    return true;
}
//===----------------------------------------------------------------------===//
// AST节点处理接口
//===----------------------------------------------------------------------===//

llvm::Value* PyCodeGen::handleNode(ASTNode* node)
{
    if (!node) return nullptr;

    PyNodeHandlerFunc handler = nodeHandlers.getHandler(node->kind());
    if (handler)
    {
        return handler(*this, node);
    }

    return logError("No handler for node kind: " + std::to_string(static_cast<int>(node->kind())));
}

llvm::Value* PyCodeGen::handleExpr(ExprAST* expr)
{
    if (!expr) return nullptr;

    PyExprHandlerFunc handler = exprHandlers.getHandler(expr->kind());
    if (handler)
    {
        return handler(*this, expr);
    }

    return logError("No handler for expression kind: " + std::to_string(static_cast<int>(expr->kind())));
}

void PyCodeGen::handleStmt(StmtAST* stmt)
{
    if (!stmt) return;

    PyStmtHandlerFunc handler = stmtHandlers.getHandler(stmt->kind());
    if (handler)
    {
        handler(*this, stmt);
    }
    else
    {
        logError("No handler for statement kind: " + std::to_string(static_cast<int>(stmt->kind())));
    }
}

llvm::Value* PyCodeGen::handleBinOp(char op, llvm::Value* L, llvm::Value* R, ObjectType* leftType, ObjectType* rightType)
{
    if (!L || !R)
    {
        return logError("Invalid operands for binary operation");
    }

    if (!leftType || !rightType)
    {
        return logError("Missing type information for binary operation");
    }

    // 使用TypeOperations系统处理二元操作
    return generateBinaryOperation(op, L, R, leftType, rightType);
}

llvm::Value* PyCodeGen::handleUnaryOp(char op, llvm::Value* operand, ObjectType* operandType)
{
    if (!operand)
    {
        return logError("Invalid operand for unary operation");
    }

    if (!operandType)
    {
        return logError("Missing type information for unary operation");
    }

    // 使用TypeOperations系统处理一元操作
    return generateUnaryOperation(op, operand, operandType);
}

//===----------------------------------------------------------------------===//
// 类型安全检查方法
//===----------------------------------------------------------------------===//

bool PyCodeGen::validateExprType(ExprAST* expr, int expectedTypeId)
{
    if (!expr) return false;

    auto typePtr = expr->getType();
    if (!typePtr) return false;

    ObjectType* objType = typePtr->getObjectType();
    if (!objType) return false;

    int runtimeTypeId = ObjectLifecycleManager::getObjectTypeId(objType);

    // 直接匹配
    if (runtimeTypeId == expectedTypeId) return true;

    // 数值类型兼容性检查
    if ((runtimeTypeId == PY_TYPE_INT || runtimeTypeId == PY_TYPE_DOUBLE) && (expectedTypeId == PY_TYPE_INT || expectedTypeId == PY_TYPE_DOUBLE))
    {
        return true;
    }

    // 使用TypeOperations系统检查类型兼容性
    return TypeOperationRegistry::getInstance().areTypesCompatible(runtimeTypeId, expectedTypeId);
}

bool PyCodeGen::validateIndexOperation(ExprAST* target, ExprAST* index)
{
    if (!target || !index) return false;

    auto targetTypePtr = target->getType();
    auto indexTypePtr = index->getType();

    if (!targetTypePtr || !indexTypePtr) return false;

    ObjectType* targetType = targetTypePtr->getObjectType();
    ObjectType* indexType = indexTypePtr->getObjectType();

    if (!targetType || !indexType) return false;

    // 调试信息 - 输出目标类型的详细信息
    std::cerr << "DEBUG: Index operation - Target type: " << targetType->getName()
              << ", TypeID: " << targetType->getTypeId()
              << ", Is container: " << TypeFeatureChecker::hasFeature(targetType, "container")
              << ", Is sequence: " << TypeFeatureChecker::hasFeature(targetType, "sequence") << std::endl;

    // 特殊处理 - 如果类型是any但变量名以list开头或是列表字面量，将其视为列表
    bool forceTreatAsList = false;

    // 添加对TypeID的检查 - 如果TypeID是LIST但名称不是，也视为列表
    if (targetType->getTypeId() == PY_TYPE_LIST)
    {
        std::cerr << "DEBUG: TypeID is LIST (5) - allowing indexing operation" << std::endl;
        forceTreatAsList = true;
    }

    if (targetType->getName() == "any" || targetType->getTypeId() == PY_TYPE_ANY)
    {
        // 检查是否来自列表字面量
        if (target->kind() == ASTKind::ListExpr)
        {
            std::cerr << "DEBUG: Treating list literal as a valid container" << std::endl;
            forceTreatAsList = true;
        }

        // 检查变量名（如果是变量）
        const VariableExprAST* varExpr = dynamic_cast<const VariableExprAST*>(target);
        if (varExpr)
        {
            std::string varName = varExpr->getName();
            // 检查符号表中是否有更准确的类型信息
            ObjectType* symbolType = getVariableType(varName);
            if (symbolType && (symbolType->getTypeId() == PY_TYPE_LIST || TypeFeatureChecker::hasFeature(symbolType, "container")))
            {
                std::cerr << "DEBUG: Found better type for " << varName << " in symbol table" << std::endl;
                forceTreatAsList = true;

                // 将符号表中的类型信息保存到TypeRegistry中，供其他地方使用
                TypeRegistry::getInstance().registerSymbolType(varName, symbolType);
            }
            else if (varName.find("list") != std::string::npos || varName.find("array") != std::string::npos || varName.find("lst") != std::string::npos)
            {
                std::cerr << "DEBUG: Variable name suggests this is a list: " << varName << std::endl;
                forceTreatAsList = true;
            }
        }
    }

    // 检查目标是否可索引 - 修改条件判断，考虑forceTreatAsList标志
    if (!TypeFeatureChecker::hasFeature(targetType, "container") && !TypeFeatureChecker::hasFeature(targetType, "sequence") && !forceTreatAsList)  // 关键修改：添加forceTreatAsList检查
    {
        // 特殊错误 - 整数不能索引
        if (targetType->getName() == "int")
        {
            logTypeError("'int' object is not subscriptable",
                         target->line.value_or(-1),
                         target->column.value_or(-1));
        }
        else
        {
            logTypeError("'" + targetType->getName() + "' object is not subscriptable",
                         target->line.value_or(-1),
                         target->column.value_or(-1));
        }
        return false;
    }

    // 检查特定容器类型的索引类型 - 同样考虑forceTreatAsList标志
    if (dynamic_cast<ListType*>(targetType) || forceTreatAsList ||  // 关键修改：添加forceTreatAsList条件
        targetType->getTypeId() == PY_TYPE_LIST)                    // 关键修改：明确检查LIST类型ID
    {
        if (indexType->getTypeId() != PY_TYPE_INT)  // 修改：使用类型ID而不是名称
        {
            logTypeError("list indices must be integers, not '" + indexType->getName() + "'",
                         index->line.value_or(-1),
                         index->column.value_or(-1));
            return false;
        }
    }
    else if (dynamic_cast<DictType*>(targetType))
    {
        auto dictType = dynamic_cast<DictType*>(targetType);
        if (dictType)
        {
            const ObjectType* keyType = dictType->getKeyType();
            if (!indexType->canAssignTo(const_cast<ObjectType*>(keyType)))
            {
                logTypeError("dict key must be of type '" + keyType->getName() + "', not '" + indexType->getName() + "'",
                             index->line.value_or(-1),
                             index->column.value_or(-1));
                return false;
            }
        }
    }
    else if (targetType->getTypeId() == PY_TYPE_STRING)
    {
        if (indexType->getTypeId() != PY_TYPE_INT)  // 修改：使用类型ID而不是名称
        {
            logTypeError("string indices must be integers, not '" + indexType->getName() + "'",
                         index->line.value_or(-1),
                         index->column.value_or(-1));
            return false;
        }
    }

    return true;
}

bool PyCodeGen::validateAssignment(const std::string& varName, ExprAST* value)
{
    if (!value) return false;

    ObjectType* existingType = getVariableType(varName);
    auto valueTypePtr = value->getType();

    if (!valueTypePtr) return false;

    ObjectType* valueType = valueTypePtr->getObjectType();
    if (!valueType) return false;

    // 如果变量不存在，任何类型都可以赋值
    if (!existingType) return true;

    // 检查类型兼容性
    if (!valueType->canAssignTo(existingType))
    {
        logTypeError("Cannot assign '" + valueType->getName() + "' to variable of type '" + existingType->getName() + "'",
                     value->line.value_or(-1),
                     value->column.value_or(-1));
        return false;
    }

    return true;
}

llvm::Value* PyCodeGen::generateTypeCheck(llvm::Value* obj, int expectedTypeId)
{
    if (!obj || !obj->getType()->isPointerTy())
    {
        return builder->getInt1(false);
    }

    // 使用运行时的类型检查功能
    return runtime->isInstance(obj, expectedTypeId);
}

llvm::Value* PyCodeGen::generateTypeError(llvm::Value* obj, int expectedTypeId)
{
    if (!obj) return nullptr;

    // 获取对象的类型ID
    llvm::Value* typeId = runtime->getTypeIdFromObject(obj);

    // 调用运行时的类型错误函数
    llvm::Function* typeErrorFunc = getOrCreateExternalFunction(
        "py_raise_type_error",
        llvm::Type::getVoidTy(getContext()),
        {llvm::Type::getInt32Ty(getContext()),
         llvm::Type::getInt32Ty(getContext())});

    return builder->CreateCall(typeErrorFunc, {builder->getInt32(expectedTypeId), typeId});
}

//===----------------------------------------------------------------------===//
// 对象生命周期管理方法
//===----------------------------------------------------------------------===//
// 在ObjectLifecycleManager类实现中添加：

void ObjectLifecycleManager::attachTypeMetadata(llvm::Value* value, int typeId)
{
    if (!value || !llvm::isa<llvm::Instruction>(value))
        return;

    // 使用内置的元数据设施
    llvm::Instruction* inst = llvm::cast<llvm::Instruction>(value);
    llvm::LLVMContext& ctx = inst->getContext();

    // 创建元数据节点
    llvm::MDNode* typeNode = llvm::MDNode::get(
        ctx,
        {llvm::ConstantAsMetadata::get(
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), typeId))});

    // 附加元数据到指令
    inst->setMetadata("py_type_id", typeNode);

    // 如果是指针且指向容器类型，标记为引用类型
    if (typeId >= PY_TYPE_LIST && typeId <= PY_TYPE_SET || typeId == PY_TYPE_DICT || typeId == PY_TYPE_TUPLE || typeId == PY_TYPE_MAP)
    {
        llvm::MDNode* refNode = llvm::MDNode::get(
            ctx, {llvm::ConstantAsMetadata::get(
                     llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx), 1))});
        inst->setMetadata("py_is_reference", refNode);
    }
}

// 增强的getTypeIdFromMetadata实现
int ObjectLifecycleManager::getTypeIdFromMetadata(llvm::Value* value)
{
    if (!value)
        return -1;

    // 处理指令元数据
    if (llvm::Instruction* inst = llvm::dyn_cast<llvm::Instruction>(value))
    {
        llvm::MDNode* typeMD = inst->getMetadata("py_type_id");
        if (typeMD && typeMD->getNumOperands() > 0)
        {
            if (llvm::ConstantAsMetadata* constMD = llvm::dyn_cast<llvm::ConstantAsMetadata>(
                    typeMD->getOperand(0)))
            {
                if (llvm::ConstantInt* constInt = llvm::dyn_cast<llvm::ConstantInt>(
                        constMD->getValue()))
                {
                    return constInt->getSExtValue();
                }
            }
        }

        // 检查是否有容器类型标记
        llvm::MDNode* containerMD = inst->getMetadata("py_container_type");
        if (containerMD && containerMD->getNumOperands() > 0)
        {
            if (llvm::ConstantAsMetadata* constMD = llvm::dyn_cast<llvm::ConstantAsMetadata>(
                    containerMD->getOperand(0)))
            {
                if (llvm::ConstantInt* constInt = llvm::dyn_cast<llvm::ConstantInt>(
                        constMD->getValue()))
                {
                    int containerBase = constInt->getSExtValue();
                    // 根据容器基类返回类型ID
                    switch (containerBase)
                    {
                        case 1:
                            return PY_TYPE_LIST_BASE;
                        case 2:
                            return PY_TYPE_DICT_BASE;
                        case 3:
                            return PY_TYPE_FUNC_BASE;
                        default:
                            return PY_TYPE_ANY;
                    }
                }
            }
        }

        // 检查是否有指针类型标记
        llvm::MDNode* ptrMD = inst->getMetadata("py_ptr_type");
        if (ptrMD && ptrMD->getNumOperands() > 0)
        {
            if (llvm::ConstantAsMetadata* constMD = llvm::dyn_cast<llvm::ConstantAsMetadata>(
                    ptrMD->getOperand(0)))
            {
                if (llvm::ConstantInt* constInt = llvm::dyn_cast<llvm::ConstantInt>(
                        constMD->getValue()))
                {
                    int pointeeTypeId = constInt->getSExtValue();
                    // 根据指向的类型返回指针类型ID
                    switch (pointeeTypeId)
                    {
                        case PY_TYPE_INT:
                            return PY_TYPE_PTR_INT;
                        case PY_TYPE_DOUBLE:
                            return PY_TYPE_PTR_DOUBLE;
                        default:
                            return PY_TYPE_PTR;
                    }
                }
            }
        }

        // 检查引用类型标记
        llvm::MDNode* refMD = inst->getMetadata("py_is_reference");
        if (refMD && refMD->getNumOperands() > 0)
        {
            // 如果有引用标记但没有具体类型ID，返回ANY类型
            return PY_TYPE_ANY;
        }
    }

    // 从值类型推断
    if (value->getType()->isDoubleTy())
        return PY_TYPE_DOUBLE;
    if (value->getType()->isIntegerTy(32))
        return PY_TYPE_INT;
    if (value->getType()->isIntegerTy(1))
        return PY_TYPE_BOOL;

    // 检查是否为指针类型
    if (value->getType()->isPointerTy())
    {
        // 尝试推断指针指向的类型
        std::string typeName = value->getName().str();
        if (typeName.find("int") != std::string::npos)
            return PY_TYPE_PTR_INT;
        if (typeName.find("double") != std::string::npos || typeName.find("float") != std::string::npos)
            return PY_TYPE_PTR_DOUBLE;
        if (typeName.find("list") != std::string::npos || typeName.find("arr") != std::string::npos)
            return PY_TYPE_LIST;
        if (typeName.find("dict") != std::string::npos || typeName.find("map") != std::string::npos)
            return PY_TYPE_DICT;

        // 默认指针类型
        return PY_TYPE_PTR;
    }

    // 默认返回-1表示未知
    return -1;
}

// 新增：检查值是否为容器类型
bool ObjectLifecycleManager::isContainerType(llvm::Value* value)
{
    if (!value)
        return false;

    int typeId = getTypeIdFromMetadata(value);
    if (typeId != -1)
    {
        return (typeId >= PY_TYPE_LIST && typeId <= PY_TYPE_SET) || typeId == PY_TYPE_DICT || typeId == PY_TYPE_TUPLE;
    }

    // 尝试从名称推断
    if (value->hasName())
    {
        llvm::StringRef name = value->getName();
        return name.contains("list") || name.contains("dict") || name.contains("array") || name.contains("map");
    }

    return false;
}

// 新增：确保值被视为容器
llvm::Value* ObjectLifecycleManager::ensureContainer(PyCodeGen& codegen, llvm::Value* value, int containerTypeId)
{
    if (!value)
        return nullptr;

    // 如果已经是指针类型，检查并标记元数据
    if (value->getType()->isPointerTy())
    {
        attachTypeMetadata(value, containerTypeId);
        return value;
    }

    // 否则，创建新的容器对象
    llvm::Function* createFunc = nullptr;

    switch (containerTypeId)
    {
        case PY_TYPE_LIST:
            createFunc = codegen.getOrCreateExternalFunction(
                "py_create_list",
                llvm::PointerType::get(codegen.getContext(), 0),
                {llvm::Type::getInt32Ty(codegen.getContext()),
                 llvm::Type::getInt32Ty(codegen.getContext())});

            return codegen.builder->CreateCall(
                createFunc,
                {llvm::ConstantInt::get(llvm::Type::getInt32Ty(codegen.getContext()), 0),             // 初始容量
                 llvm::ConstantInt::get(llvm::Type::getInt32Ty(codegen.getContext()), PY_TYPE_ANY)},  // 元素类型
                "new_list");

        case PY_TYPE_DICT:
            createFunc = codegen.getOrCreateExternalFunction(
                "py_create_dict",
                llvm::PointerType::get(codegen.getContext(), 0),
                {llvm::Type::getInt32Ty(codegen.getContext()),
                 llvm::Type::getInt32Ty(codegen.getContext())});

            return codegen.builder->CreateCall(
                createFunc,
                {llvm::ConstantInt::get(llvm::Type::getInt32Ty(codegen.getContext()), 8),                // 初始容量
                 llvm::ConstantInt::get(llvm::Type::getInt32Ty(codegen.getContext()), PY_TYPE_STRING)},  // 键类型
                "new_dict");

        default:
            return nullptr;
    }
}

llvm::Value* PyCodeGen::handleExpressionValue(llvm::Value* value, ExprAST* expr,
                                              bool isReturnValue, bool isAssignTarget, bool isParameter)
{
    if (!value || !expr) return value;

    auto typePtr = expr->getType();
    if (!typePtr) return value;

    ObjectType* type = typePtr->getObjectType();
    if (!type) return value;

    // 确定对象来源
    ObjectLifecycleManager::ObjectSource source;

    switch (expr->kind())
    {
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
            source = ObjectLifecycleManager::ObjectSource::LOCAL_VARIABLE;
            break;
    }

    // 确定对象目标
    ObjectLifecycleManager::ObjectDestination dest;
    if (isReturnValue)
    {
        dest = ObjectLifecycleManager::ObjectDestination::RETURN;
    }
    else if (isAssignTarget)
    {
        dest = ObjectLifecycleManager::ObjectDestination::ASSIGNMENT;
    }
    else if (isParameter)
    {
        dest = ObjectLifecycleManager::ObjectDestination::PARAMETER;
    }
    else
    {
        dest = ObjectLifecycleManager::ObjectDestination::TEMPORARY;
    }

    // 根据对象来源和目标调整值
    return ObjectLifecycleManager::adjustObject(*this, value, type, source, dest);
}

llvm::Value* PyCodeGen::prepareReturnValue(llvm::Value* value, ObjectType* returnType, ExprAST* expr)
{
    if (!value || !returnType || !expr) return value;

    // 获取表达式类型
    auto exprTypePtr = expr->getType();
    if (!exprTypePtr) return value;

    // 根据需要做类型转换
    if (exprTypePtr->getObjectType()->getTypeId() != returnType->getTypeId())
    {
        value = generateTypeConversion(value, exprTypePtr->getObjectType(), returnType);
    }

    // 根据对象来源判断是否需要增加引用计数
    auto source = CodeGenLifecycleManager::determineObjectSource(expr);

    if (ObjectLifecycleManager::needsIncRef(returnType, source,
                                            ObjectLifecycleManager::ObjectDestination::RETURN))
    {
        // 增加引用计数
        runtime->incRef(value);
    }

    return value;
}

/* llvm::Value* PyCodeGen::prepareAssignmentTarget(llvm::Value* value, ObjectType* targetType, ExprAST* expr)
{
    if (!value || !targetType || !expr) return value;

    // 获取表达式类型
    auto exprTypePtr = expr->getType();
    if (!exprTypePtr) return value;

    // 根据需要做类型转换
    if (exprTypePtr->getObjectType()->getTypeId() != targetType->getTypeId())
    {
        value = generateTypeConversion(value, exprTypePtr->getObjectType(), targetType);
    }

    // 根据对象来源判断是否需要增加引用计数
    auto source = CodeGenLifecycleManager::determineObjectSource(expr);

    if (ObjectLifecycleManager::needsIncRef(targetType, source,
                                            ObjectLifecycleManager::ObjectDestination::ASSIGNMENT))
    {
        // 增加引用计数
        runtime->incRef(value);
    }

    return value;
} */

// 修复 prepareParameter 方法，避免 '=' 操作符问题
llvm::Value* PyCodeGen::prepareParameter(llvm::Value* value, ObjectType* paramType, ExprAST* expr)
{
    if (!value || !paramType || !expr) return value;

    // 获取表达式类型
    auto exprTypePtr = expr->getType();
    if (!exprTypePtr) return value;

    ObjectType* exprType = exprTypePtr->getObjectType();

    // 如果类型不同，尝试类型转换
    if (exprType->getTypeId() != paramType->getTypeId())
    {
        value = generateTypeConversion(value, exprType, paramType);
        if (!value) return nullptr;
    }

    // 确定对象来源和目标
    auto source = CodeGenLifecycleManager::determineObjectSource(expr);

    // 决定是否需要增加引用计数
    if (ObjectLifecycleManager::needsIncRef(paramType, source,
                                            ObjectLifecycleManager::ObjectDestination::PARAMETER))
    {
        llvm::Function* incRefFunc = getOrCreateExternalFunction(
            "py_incref",
            llvm::Type::getVoidTy(getContext()),
            {llvm::PointerType::get(getContext(), 0)});

        builder->CreateCall(incRefFunc, {value});
    }

    return value;
}
//===----------------------------------------------------------------------===//
// 操作代码生成方法
//===----------------------------------------------------------------------===//

#define DEBUG_LOG(msg) \
    std::cerr << "DEBUG: " << msg << std::endl;

llvm::Value* PyCodeGen::generateBinaryOperation(char op, llvm::Value* L, llvm::Value* R,
                                                ObjectType* leftType, ObjectType* rightType)
{
    // 1. 参数检查
    if (!L || !R || !leftType || !rightType)
    {
        return logError("Invalid operands for binary operation");
    }

    // 获取类型ID
    int leftTypeId = leftType->getTypeId();
    int rightTypeId = rightType->getTypeId();

    // 2. 处理any类型（PY_TYPE_ANY）- 优先尝试使用另一个操作数的类型
    if (leftTypeId == PY_TYPE_ANY && rightTypeId != PY_TYPE_ANY)
    {
        DEBUG_LOG("处理left为any类型: " << leftTypeId << ", right为: " << rightTypeId);
        // 左边是any，尝试转换使用右边的类型
        leftTypeId = rightTypeId;
        leftType = rightType;
    }
    else if (rightTypeId == PY_TYPE_ANY && leftTypeId != PY_TYPE_ANY)
    {
        DEBUG_LOG("处理right为any类型: " << rightTypeId << ", left为: " << leftTypeId);
        // 右边是any，尝试转换使用左边的类型
        rightTypeId = leftTypeId;
        rightType = leftType;
    }

    // 3. 确保操作数是正确的Python对象指针
    L = ensurePythonObject(L, leftType);
    R = ensurePythonObject(R, rightType);

    if (!L || !R)
    {
        return logError("Failed to convert operands to Python objects");
    }

    // 4. 查询类型操作注册表，获取最佳操作路径
    auto& registry = TypeOperationRegistry::getInstance();
    auto opPath = registry.findOperablePath(op, leftTypeId, rightTypeId);

    // 5. 如果需要类型转换，执行转换
    if (opPath.first != leftTypeId)
    {
        ObjectType* targetType = TypeRegistry::getInstance().getType(std::to_string(opPath.first));
        if (targetType)
        {
            DEBUG_LOG("将左操作数从类型 " << leftTypeId << " 转换为类型 " << opPath.first);
            L = generateTypeConversion(L, leftType, targetType);
            leftType = targetType;
            leftTypeId = opPath.first;
        }
    }

    if (opPath.second != rightTypeId)
    {
        ObjectType* targetType = TypeRegistry::getInstance().getType(std::to_string(opPath.second));
        if (targetType)
        {
            DEBUG_LOG("将右操作数从类型 " << rightTypeId << " 转换为类型 " << opPath.second);
            R = generateTypeConversion(R, rightType, targetType);
            rightType = targetType;
            rightTypeId = opPath.second;
        }
    }

    // 6. 获取操作描述符
    BinaryOpDescriptor* descriptor = registry.getBinaryOpDescriptor(op, leftTypeId, rightTypeId);

    // 7. 执行操作
    llvm::Value* result = nullptr;

    // 特殊处理索引操作
    if (op == '[')
    {
        result = handleIndexOperation(L, R, leftType, rightType);
    }
    // 使用注册的运行时函数
    else if (descriptor && !descriptor->runtimeFunction.empty())
    {
        DEBUG_LOG("使用运行时函数: " << descriptor->runtimeFunction);
        // 获取运行时函数
        llvm::Function* runtimeFunc = getOrCreateExternalFunction(
            descriptor->runtimeFunction,
            llvm::PointerType::get(getContext(), 0),
            {llvm::PointerType::get(getContext(), 0), llvm::PointerType::get(getContext(), 0)},
            false);

        // 调用函数
        result = builder->CreateCall(runtimeFunc, {L, R}, "binop_result");
    }
    // 使用自定义实现
    else if (descriptor && descriptor->customImpl)
    {
        DEBUG_LOG("使用自定义实现");
        result = descriptor->customImpl(*this, L, R);
    }
    // 回退到通用对象操作
    else
    {
        // 根据操作符选择函数名
        std::string funcName = "py_object_";
        switch (op)
        {
            case '+':
                funcName += "add";
                break;
            case '-':
                funcName += "subtract";
                break;
            case '*':
                funcName += "multiply";
                break;
            case '/':
                funcName += "divide";
                break;
            case '%':
                funcName += "modulo";
                break;
            case '<':
            case '>':
            case '=':
            case '!':
                funcName = "py_object_compare";
                break;
            default:
                return logError("Unsupported binary operator");
        }

        // 特殊处理比较操作
        if (op == '<' || op == '>' || op == '=' || op == '!')
        {
            // 获取比较函数
            llvm::Function* compareFunc = getOrCreateExternalFunction(
                funcName,
                llvm::PointerType::get(getContext(), 0),
                {llvm::PointerType::get(getContext(), 0),
                 llvm::PointerType::get(getContext(), 0),
                 llvm::Type::getInt32Ty(getContext())},
                false);

            // 映射操作符到比较码
            int compareOp;
            switch (op)
            {
                case '<':
                    compareOp = 2;
                    break;  // PY_CMP_LT
                case '>':
                    compareOp = 4;
                    break;  // PY_CMP_GT
                case '=':
                    compareOp = 0;
                    break;  // PY_CMP_EQ
                case '!':
                    compareOp = 1;
                    break;  // PY_CMP_NE
                default:
                    compareOp = 0;
            }

            // 调用比较函数
            result = builder->CreateCall(
                compareFunc,
                {L,
                 R,
                 llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), compareOp)},
                "compare_result");
        }
        else
        {
            // 获取通用操作函数
            llvm::Function* opFunc = getOrCreateExternalFunction(
                funcName,
                llvm::PointerType::get(getContext(), 0),
                {llvm::PointerType::get(getContext(), 0),
                 llvm::PointerType::get(getContext(), 0)},
                false);

            // 调用函数
            result = builder->CreateCall(opFunc, {L, R}, "op_result");
        }
    }

    // 8. 处理可能的NULL结果
    if (!result)
    {
        DEBUG_LOG("生成默认结果(0)");
        // 创建默认值(0)
        result = builder->CreateCall(
            getOrCreateExternalFunction("py_create_int",
                                        llvm::PointerType::get(getContext(), 0),
                                        {llvm::Type::getInt32Ty(getContext())},
                                        false),
            {llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), 0)},
            "default_result");
    }

    // 9. 跟踪生成的对象用于生命周期管理
    trackObject(result);

    return result;
}
llvm::Value* PyCodeGen::ensurePythonObject(llvm::Value* value, ObjectType* type)
{
    if (!value || !type)
    {
        return nullptr;
    }

    int typeId = type->getTypeId();
    bool isPointer = value->getType()->isPointerTy();
    bool isPrimitive = type->getCategory() == ObjectType::Primitive;
    bool isContainer = type->getCategory() == ObjectType::Container;

    // 首先检查元数据类型信息
    int metadataTypeId = ObjectLifecycleManager::getTypeIdFromMetadata(value);
    if (metadataTypeId != -1 && isPointer)
    {
        DEBUG_LOG("从元数据检测到类型ID: " << metadataTypeId);

        // 如果是二元运算结果，需要特殊处理
        if (value->getName().starts_with("binop_result"))
        {
            if (metadataTypeId == PY_TYPE_DOUBLE)
            {
                DEBUG_LOG("二元操作结果是浮点类型，正确提取浮点值");
                llvm::Value* extractedValue = builder->CreateCall(
                    getOrCreateExternalFunction("py_extract_double",
                                                llvm::Type::getDoubleTy(getContext()),
                                                {llvm::PointerType::get(getContext(), 0)},
                                                false),
                    {value},
                    "extracted_double");

                return builder->CreateCall(
                    getOrCreateExternalFunction("py_create_double",
                                                llvm::PointerType::get(getContext(), 0),
                                                {llvm::Type::getDoubleTy(getContext())},
                                                false),
                    {extractedValue},
                    "double_from_binop");
            }
            else if (metadataTypeId == PY_TYPE_INT)
            {
                DEBUG_LOG("二元操作结果是整数类型，正确提取整数值");
                llvm::Value* extractedValue = builder->CreateCall(
                    getOrCreateExternalFunction("py_extract_int",
                                                llvm::Type::getInt32Ty(getContext()),
                                                {llvm::PointerType::get(getContext(), 0)},
                                                false),
                    {value},
                    "extracted_int");

                return builder->CreateCall(
                    getOrCreateExternalFunction("py_create_int",
                                                llvm::PointerType::get(getContext(), 0),
                                                {llvm::Type::getInt32Ty(getContext())},
                                                false),
                    {extractedValue},
                    "int_from_binop");
            }
            else if (metadataTypeId == PY_TYPE_LIST)
            {
                DEBUG_LOG("二元操作结果是列表类型，直接返回");
                // 列表类型直接返回，不需要提取
                return value;
            }
        }

        // 检查索引操作结果 (来自列表的元素访问)
        if (value->getName().starts_with("list_item") || value->getName().starts_with("index_"))
        {
            DEBUG_LOG("处理列表索引访问结果，元数据类型: " << metadataTypeId);

            // 根据元数据类型决定如何处理列表元素
            if (metadataTypeId == PY_TYPE_INT)
            {
                return value;  // 一般情况下直接返回值
            }
            else if (metadataTypeId == PY_TYPE_DOUBLE)
            {
                return value;  // 直接返回值
            }
            else if (metadataTypeId == PY_TYPE_STRING)
            {
                return value;  // 直接返回值
            }
            else
            {
                // 其他类型元素直接返回
                return value;
            }
        }

        // 如果请求的类型和元数据类型匹配或是ANY，直接返回原值
        if (typeId == PY_TYPE_ANY || typeId == metadataTypeId)
        {
            DEBUG_LOG("类型匹配或为ANY类型，直接使用元数据类型");
            return value;
        }

        // 需要类型转换
        DEBUG_LOG("基于元数据进行类型转换: 从类型 " << metadataTypeId << " 到类型 " << typeId);
        std::string convFunc;

        if (metadataTypeId == PY_TYPE_DOUBLE && typeId == PY_TYPE_INT)
        {
            convFunc = "py_convert_double_to_int";
        }
        else if (metadataTypeId == PY_TYPE_INT && typeId == PY_TYPE_DOUBLE)
        {
            convFunc = "py_convert_int_to_double";
        }
        else if (metadataTypeId == PY_TYPE_LIST && typeId == PY_TYPE_LIST)
        {
            // 同为列表类型，但可能元素类型不同，需要复制
            return builder->CreateCall(
                getOrCreateExternalFunction("py_list_copy",
                                            llvm::PointerType::get(getContext(), 0),
                                            {llvm::PointerType::get(getContext(), 0)},
                                            false),
                {value},
                "copied_list");
        }
        else
        {
            // 使用通用转换函数
            convFunc = "py_convert_any_to_" + std::to_string(typeId);
        }

        // 调用转换函数
        llvm::Value* convertedValue = builder->CreateCall(
            getOrCreateExternalFunction(convFunc,
                                        llvm::PointerType::get(getContext(), 0),
                                        {llvm::PointerType::get(getContext(), 0)},
                                        false),
            {value},
            "metadata_converted_value");

        // 确保转换后的值有正确的类型元数据
        ObjectLifecycleManager::attachTypeMetadata(convertedValue, typeId);
        return convertedValue;
    }

    // 处理容器类型 (列表、字典等)
    if (isContainer)
    {
        DEBUG_LOG("处理容器类型: " << typeId);

        // 检查是否已经是列表类型且值是指针
        if (typeId == PY_TYPE_LIST && isPointer)
        {
            // 如果值已经是列表指针，检查名称是否表明这是列表
            if (value->getName().starts_with("list") || value->getName().starts_with("array") || value->getName().contains("list"))
            {
                DEBUG_LOG("值已经是列表类型指针，直接返回");
                return value;
            }

            // 尝试加载指针指向的值
            if (llvm::AllocaInst* alloca = llvm::dyn_cast<llvm::AllocaInst>(value))
            {
                DEBUG_LOG("加载分配的列表指针");
                llvm::Value* loadedValue = builder->CreateLoad(
                    llvm::PointerType::get(getContext(), 0),
                    value,
                    "loaded_list");

                // 确保加载的值有正确的元数据
                ObjectLifecycleManager::attachTypeMetadata(loadedValue, PY_TYPE_LIST);
                return loadedValue;
            }

            // 可能是从另一个容器获取的值，直接返回
            return value;
        }
        else if (typeId == PY_TYPE_DICT && isPointer)
        {
            // 字典类型的处理类似于列表
            if (value->getName().starts_with("dict") || value->getName().contains("dict") || value->getName().contains("map"))
            {
                DEBUG_LOG("值已经是字典类型指针，直接返回");
                return value;
            }

            // 尝试加载指针指向的值
            if (llvm::AllocaInst* alloca = llvm::dyn_cast<llvm::AllocaInst>(value))
            {
                DEBUG_LOG("加载分配的字典指针");
                llvm::Value* loadedValue = builder->CreateLoad(
                    llvm::PointerType::get(getContext(), 0),
                    value,
                    "loaded_dict");

                // 确保加载的值有正确的元数据
                ObjectLifecycleManager::attachTypeMetadata(loadedValue, PY_TYPE_DICT);
                return loadedValue;
            }

            return value;
        }

        // 特殊情况：值不是指针，但请求容器类型(可能是列表、字典创建)
        if (!isPointer && typeId == PY_TYPE_LIST)
        {
            DEBUG_LOG("非指针值请求列表类型，尝试创建新列表");

            // 获取列表元素类型
            int elemTypeId = PY_TYPE_ANY;  // 默认为ANY

            // 尝试从类型信息获取更具体的元素类型
            if (ListType* listType = dynamic_cast<ListType*>(type))
            {
                if (listType->getElementType())
                {
                    elemTypeId = listType->getElementType()->getTypeId();
                }
            }

            // 创建空列表
            llvm::Value* emptyList = builder->CreateCall(
                getOrCreateExternalFunction("py_create_list",
                                            llvm::PointerType::get(getContext(), 0),
                                            {llvm::Type::getInt32Ty(getContext()),
                                             llvm::Type::getInt32Ty(getContext())},
                                            false),
                {llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), 0),
                 llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), elemTypeId)},
                "empty_list");

            // 附加类型元数据
            ObjectLifecycleManager::attachTypeMetadata(emptyList, PY_TYPE_LIST);
            return emptyList;
        }

        if (!isPointer && typeId == PY_TYPE_DICT)
        {
            DEBUG_LOG("非指针值请求字典类型，尝试创建新字典");

            // 获取字典键值类型
            int keyTypeId = PY_TYPE_STRING;  // 默认键类型是字符串

            // 创建空字典
            llvm::Value* emptyDict = builder->CreateCall(
                getOrCreateExternalFunction("py_create_dict",
                                            llvm::PointerType::get(getContext(), 0),
                                            {llvm::Type::getInt32Ty(getContext()),
                                             llvm::Type::getInt32Ty(getContext())},
                                            false),
                {llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), 8),  // 初始容量
                 llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), keyTypeId)},
                "empty_dict");

            // 附加类型元数据
            ObjectLifecycleManager::attachTypeMetadata(emptyDict, PY_TYPE_DICT);
            return emptyDict;
        }
    }

    // 如果值已经是指向Python对象的指针，直接返回
    if (isPointer && !isPrimitive)
    {
        // 可能是一个已有的Python对象指针，确保它有正确的类型元数据
        ObjectLifecycleManager::attachTypeMetadata(value, typeId);
        return value;
    }

    // 检查指针的实际分配类型
    llvm::Type* allocatedType = nullptr;
    if (isPointer)
    {
        if (llvm::AllocaInst* alloca = llvm::dyn_cast<llvm::AllocaInst>(value))
        {
            allocatedType = alloca->getAllocatedType();
            DEBUG_LOG("检测到AllocaInst，分配的类型: " << (allocatedType->isDoubleTy() ? "double" : allocatedType->isIntegerTy(32) ? "int32"
                                                                                                : allocatedType->isIntegerTy(1)    ? "bool"
                                                                                                                                   : "其他"));
        }
        else
        {
            DEBUG_LOG("指针不是AllocaInst: " << value->getName().str());

            // 检查是否可能是列表项
            if (value->getName().starts_with("list_item") || value->getName().starts_with("index_") || value->getName().contains("item"))
            {
                DEBUG_LOG("可能是列表项的访问，使用运行时类型检查");

                // 使用py_get_object_type_id检查对象类型
                llvm::Function* getTypeFunc = getOrCreateExternalFunction(
                    "py_get_object_type_id",
                    llvm::Type::getInt32Ty(getContext()),
                    {llvm::PointerType::get(getContext(), 0)},
                    false);

                llvm::Value* objTypeId = builder->CreateCall(getTypeFunc, {value}, "obj_type_id");

                // 创建条件分支处理不同类型
                llvm::Value* isBasicType = builder->CreateICmpULE(
                    objTypeId,
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), PY_TYPE_STRING),
                    "is_basic_type");

                llvm::BasicBlock* currentBB = builder->GetInsertBlock();
                llvm::Function* parentFunc = currentBB->getParent();

                llvm::BasicBlock* extractBB = llvm::BasicBlock::Create(getContext(), "extract_value", parentFunc);
                llvm::BasicBlock* directBB = llvm::BasicBlock::Create(getContext(), "direct_value", parentFunc);
                llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(getContext(), "merge", parentFunc);

                builder->CreateCondBr(isBasicType, extractBB, directBB);

                // 基本类型处理：尝试提取值
                builder->SetInsertPoint(extractBB);

                // 检查具体类型并提取相应值
                std::vector<std::pair<int, std::string>> typeChecks = {
                    {PY_TYPE_INT, "py_extract_int"},
                    {PY_TYPE_DOUBLE, "py_extract_double"},
                    {PY_TYPE_BOOL, "py_extract_bool"}};

                // 存储创建的所有基本块，避免使用getBasicBlockList()
                std::vector<llvm::BasicBlock*> typeCheckBlocks;
                std::vector<llvm::Value*> extractedObjects;

                llvm::BasicBlock* nextBB = extractBB;

                for (size_t i = 0; i < typeChecks.size(); i++)
                {
                    auto [checkTypeId, extractFunc] = typeChecks[i];

                    llvm::BasicBlock* checkBB = llvm::BasicBlock::Create(
                        getContext(),
                        "check_" + std::to_string(checkTypeId),
                        parentFunc);
                    typeCheckBlocks.push_back(checkBB);

                    // 在当前基本块创建条件分支
                    builder->SetInsertPoint(nextBB);

                    llvm::Value* isType = builder->CreateICmpEQ(
                        objTypeId,
                        llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), checkTypeId),
                        "is_type_" + std::to_string(checkTypeId));

                    // 如果不是最后一个检查，创建下一个检查基本块
                    llvm::BasicBlock* nextCheckBB = nullptr;
                    if (i < typeChecks.size() - 1)
                    {
                        nextCheckBB = llvm::BasicBlock::Create(getContext(), "next_check_" + std::to_string(i), parentFunc);
                        typeCheckBlocks.push_back(nextCheckBB);
                    }
                    else
                    {
                        nextCheckBB = directBB;
                    }

                    builder->CreateCondBr(isType, checkBB, nextCheckBB);

                    // 设置下一个迭代要使用的基本块
                    nextBB = nextCheckBB;

                    // 在检查块中生成提取值的代码
                    builder->SetInsertPoint(checkBB);

                    llvm::Value* extractedValue = nullptr;
                    llvm::Value* objValue = nullptr;

                    if (checkTypeId == PY_TYPE_INT)
                    {
                        extractedValue = builder->CreateCall(
                            getOrCreateExternalFunction(extractFunc,
                                                        llvm::Type::getInt32Ty(getContext()),
                                                        {llvm::PointerType::get(getContext(), 0)},
                                                        false),
                            {value},
                            "extracted_int");

                        objValue = builder->CreateCall(
                            getOrCreateExternalFunction("py_create_int",
                                                        llvm::PointerType::get(getContext(), 0),
                                                        {llvm::Type::getInt32Ty(getContext())},
                                                        false),
                            {extractedValue},
                            "int_from_extract");

                        ObjectLifecycleManager::attachTypeMetadata(objValue, PY_TYPE_INT);
                    }
                    else if (checkTypeId == PY_TYPE_DOUBLE)
                    {
                        extractedValue = builder->CreateCall(
                            getOrCreateExternalFunction(extractFunc,
                                                        llvm::Type::getDoubleTy(getContext()),
                                                        {llvm::PointerType::get(getContext(), 0)},
                                                        false),
                            {value},
                            "extracted_double");

                        objValue = builder->CreateCall(
                            getOrCreateExternalFunction("py_create_double",
                                                        llvm::PointerType::get(getContext(), 0),
                                                        {llvm::Type::getDoubleTy(getContext())},
                                                        false),
                            {extractedValue},
                            "double_from_extract");

                        ObjectLifecycleManager::attachTypeMetadata(objValue, PY_TYPE_DOUBLE);
                    }
                    else if (checkTypeId == PY_TYPE_BOOL)
                    {
                        extractedValue = builder->CreateCall(
                            getOrCreateExternalFunction(extractFunc,
                                                        llvm::Type::getInt1Ty(getContext()),
                                                        {llvm::PointerType::get(getContext(), 0)},
                                                        false),
                            {value},
                            "extracted_bool");

                        objValue = builder->CreateCall(
                            getOrCreateExternalFunction("py_create_bool",
                                                        llvm::PointerType::get(getContext(), 0),
                                                        {llvm::Type::getInt1Ty(getContext())},
                                                        false),
                            {extractedValue},
                            "bool_from_extract");

                        ObjectLifecycleManager::attachTypeMetadata(objValue, PY_TYPE_BOOL);
                    }

                    extractedObjects.push_back(objValue);
                    builder->CreateBr(mergeBB);
                }

                // 直接返回值
                builder->SetInsertPoint(directBB);
                // 假设值已经是有效的Python对象
                ObjectLifecycleManager::attachTypeMetadata(value, PY_TYPE_ANY);
                builder->CreateBr(mergeBB);

                // 合并处理结果
                builder->SetInsertPoint(mergeBB);
                llvm::PHINode* result = builder->CreatePHI(
                    llvm::PointerType::get(getContext(), 0),
                    typeChecks.size() + 1,
                    "result");

                // 添加结果分支，使用之前保存的基本块和值
                for (size_t i = 0; i < typeChecks.size(); i++)
                {
                    // 检查块是按顺序创建的，每个check类型的基本块索引是 i*2
                    if (i < extractedObjects.size() && extractedObjects[i])
                    {
                        result->addIncoming(extractedObjects[i], typeCheckBlocks[i * 2]);
                    }
                }

                // 添加直接分支
                result->addIncoming(value, directBB);

                return result;
            }
            else
            {
                // 尝试从指针对象推断类型
                llvm::Function* getTypeFunc = getOrCreateExternalFunction(
                    "py_get_object_type_id",
                    llvm::Type::getInt32Ty(getContext()),
                    {llvm::PointerType::get(getContext(), 0)},
                    false);

                llvm::Value* objTypeId = builder->CreateCall(getTypeFunc, {value}, "obj_type_id");

                // 创建条件分支处理不同类型
                llvm::Value* isDouble = builder->CreateICmpEQ(
                    objTypeId,
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), PY_TYPE_DOUBLE),
                    "is_double");

                llvm::BasicBlock* currentBB = builder->GetInsertBlock();
                llvm::Function* parentFunc = currentBB->getParent();

                llvm::BasicBlock* doubleBB = llvm::BasicBlock::Create(getContext(), "handle_double", parentFunc);
                llvm::BasicBlock* defaultBB = llvm::BasicBlock::Create(getContext(), "handle_default", parentFunc);
                llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(getContext(), "merge", parentFunc);

                builder->CreateCondBr(isDouble, doubleBB, defaultBB);

                // 处理浮点值
                builder->SetInsertPoint(doubleBB);
                llvm::Value* doubleVal = builder->CreateCall(
                    getOrCreateExternalFunction("py_extract_double",
                                                llvm::Type::getDoubleTy(getContext()),
                                                {llvm::PointerType::get(getContext(), 0)},
                                                false),
                    {value},
                    "extracted_double");

                llvm::Value* doubleObj = builder->CreateCall(
                    getOrCreateExternalFunction("py_create_double",
                                                llvm::PointerType::get(getContext(), 0),
                                                {llvm::Type::getDoubleTy(getContext())},
                                                false),
                    {doubleVal},
                    "double_obj");

                // 添加类型元数据
                ObjectLifecycleManager::attachTypeMetadata(doubleObj, PY_TYPE_DOUBLE);
                builder->CreateBr(mergeBB);

                // 处理默认情况 - 假设为整数
                builder->SetInsertPoint(defaultBB);

                // 添加对列表类型的检查
                llvm::Value* isList = builder->CreateICmpEQ(
                    objTypeId,
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), PY_TYPE_LIST),
                    "is_list");

                llvm::BasicBlock* listBB = llvm::BasicBlock::Create(getContext(), "handle_list", parentFunc);
                llvm::BasicBlock* intBB = llvm::BasicBlock::Create(getContext(), "handle_int", parentFunc);

                builder->CreateCondBr(isList, listBB, intBB);

                // 处理列表情况
                builder->SetInsertPoint(listBB);
                // 列表直接返回，无需提取值
                ObjectLifecycleManager::attachTypeMetadata(value, PY_TYPE_LIST);
                builder->CreateBr(mergeBB);

                // 处理整数情况
                builder->SetInsertPoint(intBB);
                llvm::Value* intVal = builder->CreateCall(
                    getOrCreateExternalFunction("py_extract_int",
                                                llvm::Type::getInt32Ty(getContext()),
                                                {llvm::PointerType::get(getContext(), 0)},
                                                false),
                    {value},
                    "extracted_int");

                llvm::Value* intObj = builder->CreateCall(
                    getOrCreateExternalFunction("py_create_int",
                                                llvm::PointerType::get(getContext(), 0),
                                                {llvm::Type::getInt32Ty(getContext())},
                                                false),
                    {intVal},
                    "int_obj");

                // 添加类型元数据
                ObjectLifecycleManager::attachTypeMetadata(intObj, PY_TYPE_INT);
                builder->CreateBr(mergeBB);

                // 合并结果
                builder->SetInsertPoint(mergeBB);
                llvm::PHINode* result = builder->CreatePHI(
                    llvm::PointerType::get(getContext(), 0), 3, "result");
                result->addIncoming(doubleObj, doubleBB);
                result->addIncoming(value, listBB);  // 列表直接使用原始值
                result->addIncoming(intObj, intBB);

                // 添加元数据到整体结果
                if (typeId != PY_TYPE_ANY)
                {
                    ObjectLifecycleManager::attachTypeMetadata(result, typeId);
                }

                return result;
            }
        }
    }

    // 处理ANY类型 - 根据实际分配的类型处理
    if (typeId == PY_TYPE_ANY)
    {
        // 指针且有分配类型信息
        if (isPointer && allocatedType)
        {
            if (allocatedType->isDoubleTy())
            {
                DEBUG_LOG("ANY类型处理：指针指向double，正确加载");
                // 正确加载double值
                llvm::Value* loadedValue = builder->CreateLoad(llvm::Type::getDoubleTy(getContext()), value, "any_double_val");

                // 创建浮点对象
                llvm::Value* result = builder->CreateCall(
                    getOrCreateExternalFunction("py_create_double",
                                                llvm::PointerType::get(getContext(), 0),
                                                {llvm::Type::getDoubleTy(getContext())},
                                                false),
                    {loadedValue},
                    "any_double_obj");

                ObjectLifecycleManager::attachTypeMetadata(result, PY_TYPE_DOUBLE);
                return result;
            }
            else if (allocatedType->isIntegerTy(32))
            {
                DEBUG_LOG("ANY类型处理：指针指向int32，正确加载");
                // 正确加载int值
                llvm::Value* loadedValue = builder->CreateLoad(llvm::Type::getInt32Ty(getContext()), value, "any_int_val");

                // 创建整数对象
                llvm::Value* result = builder->CreateCall(
                    getOrCreateExternalFunction("py_create_int",
                                                llvm::PointerType::get(getContext(), 0),
                                                {llvm::Type::getInt32Ty(getContext())},
                                                false),
                    {loadedValue},
                    "any_int_obj");

                ObjectLifecycleManager::attachTypeMetadata(result, PY_TYPE_INT);
                return result;
            }
            else if (allocatedType->isIntegerTy(1))
            {
                DEBUG_LOG("ANY类型处理：指针指向bool，正确加载");
                // 正确加载bool值
                llvm::Value* loadedValue = builder->CreateLoad(llvm::Type::getInt1Ty(getContext()), value, "any_bool_val");

                // 创建布尔对象
                llvm::Value* result = builder->CreateCall(
                    getOrCreateExternalFunction("py_create_bool",
                                                llvm::PointerType::get(getContext(), 0),
                                                {llvm::Type::getInt1Ty(getContext())},
                                                false),
                    {loadedValue},
                    "any_bool_obj");

                ObjectLifecycleManager::attachTypeMetadata(result, PY_TYPE_BOOL);
                return result;
            }
            else if (allocatedType->isPointerTy())
            {
                DEBUG_LOG("ANY类型处理：指针指向另一个指针（可能是字符串或对象）");
                // 指向指针类型 - 可能是字符串或其他对象引用
                llvm::Value* loadedValue = builder->CreateLoad(llvm::PointerType::get(getContext(), 0), value, "any_ptr_val");

                // 尝试获取加载值的类型元数据
                int loadedTypeId = ObjectLifecycleManager::getTypeIdFromMetadata(loadedValue);
                if (loadedTypeId != -1)
                {
                    DEBUG_LOG("从加载的指针值获取到类型元数据: " << loadedTypeId);
                    // 已有元数据，确保保留
                    return loadedValue;
                }

                // 假设是已经是Python对象指针，直接返回
                return loadedValue;
            }
            else
            {
                DEBUG_LOG("ANY类型处理：未知分配类型，使用默认加载");
            }
        }

        // 指针但没有分配类型信息 - 尝试使用i32加载（保留原有行为作为备选）
        if (isPointer)
        {
            // 检查是否可能是列表类型
            if (value->getName().starts_with("list") || value->getName().contains("list") || value->getName().contains("array"))
            {
                DEBUG_LOG("ANY类型处理：指针可能是列表类型");
                ObjectLifecycleManager::attachTypeMetadata(value, PY_TYPE_LIST);
                return value;
            }

            DEBUG_LOG("ANY类型处理：无类型信息的指针，尝试作为int32加载");
            // 作为int加载，这是一种回退策略
            llvm::Value* loadedValue = builder->CreateLoad(llvm::Type::getInt32Ty(getContext()), value, "any_fallback_int");

            // 创建整数对象
            llvm::Value* result = builder->CreateCall(
                getOrCreateExternalFunction("py_create_int",
                                            llvm::PointerType::get(getContext(), 0),
                                            {llvm::Type::getInt32Ty(getContext())},
                                            false),
                {loadedValue},
                "any_int_fallback");

            ObjectLifecycleManager::attachTypeMetadata(result, PY_TYPE_INT);
            return result;
        }

        // 非指针值处理
        if (value->getType()->isIntegerTy())
        {
            DEBUG_LOG("ANY类型处理：整数字面量");
            llvm::Value* result = builder->CreateCall(
                getOrCreateExternalFunction("py_create_int",
                                            llvm::PointerType::get(getContext(), 0),
                                            {llvm::Type::getInt32Ty(getContext())},
                                            false),
                {value},
                "any_direct_int");

            ObjectLifecycleManager::attachTypeMetadata(result, PY_TYPE_INT);
            return result;
        }

        if (value->getType()->isFloatingPointTy())
        {
            DEBUG_LOG("ANY类型处理：浮点字面量");
            llvm::Value* result = builder->CreateCall(
                getOrCreateExternalFunction("py_create_double",
                                            llvm::PointerType::get(getContext(), 0),
                                            {llvm::Type::getDoubleTy(getContext())},
                                            false),
                {value},
                "any_direct_double");

            ObjectLifecycleManager::attachTypeMetadata(result, PY_TYPE_DOUBLE);
            return result;
        }

        // 最后的回退策略 - 尝试转为i32
        DEBUG_LOG("ANY类型处理：未知类型，尝试转为int");
        if (!value->getType()->isIntegerTy(32))
        {
            if (value->getType()->isIntegerTy())
            {
                value = builder->CreateIntCast(value, llvm::Type::getInt32Ty(getContext()), true, "int_to_i32");
            }
            else
            {
                // 试图作为指针处理，假设它是某种Python对象
                return value;
            }
        }

        llvm::Value* result = builder->CreateCall(
            getOrCreateExternalFunction("py_create_int",
                                        llvm::PointerType::get(getContext(), 0),
                                        {llvm::Type::getInt32Ty(getContext())},
                                        false),
            {value},
            "any_ultimate_fallback");

        ObjectLifecycleManager::attachTypeMetadata(result, PY_TYPE_INT);
        return result;
    }

    // 处理基本类型（整数、浮点等）
    if (isPrimitive)
    {
        // 处理字面量常量
        if (!isPointer && llvm::isa<llvm::Constant>(value))
        {
            switch (typeId)
            {
                case PY_TYPE_INT:
                {
                    DEBUG_LOG("处理整数字面量");
                    if (!value->getType()->isIntegerTy(32))
                    {
                        value = builder->CreateIntCast(value, llvm::Type::getInt32Ty(getContext()), true, "to_i32");
                    }

                    llvm::Value* result = builder->CreateCall(
                        getOrCreateExternalFunction("py_create_int",
                                                    llvm::PointerType::get(getContext(), 0),
                                                    {llvm::Type::getInt32Ty(getContext())},
                                                    false),
                        {value},
                        "int_obj");

                    ObjectLifecycleManager::attachTypeMetadata(result, PY_TYPE_INT);
                    return result;
                }

                case PY_TYPE_DOUBLE:
                {
                    DEBUG_LOG("处理浮点字面量");
                    if (!value->getType()->isDoubleTy())
                    {
                        if (value->getType()->isFloatingPointTy())
                        {
                            value = builder->CreateFPCast(value, llvm::Type::getDoubleTy(getContext()), "to_double");
                        }
                        else if (value->getType()->isIntegerTy())
                        {
                            value = builder->CreateSIToFP(value, llvm::Type::getDoubleTy(getContext()), "int_to_double");
                        }
                    }

                    llvm::Value* result = builder->CreateCall(
                        getOrCreateExternalFunction("py_create_double",
                                                    llvm::PointerType::get(getContext(), 0),
                                                    {llvm::Type::getDoubleTy(getContext())},
                                                    false),
                        {value},
                        "double_obj");

                    ObjectLifecycleManager::attachTypeMetadata(result, PY_TYPE_DOUBLE);
                    return result;
                }

                case PY_TYPE_BOOL:
                {
                    DEBUG_LOG("处理布尔字面量");
                    if (!value->getType()->isIntegerTy(1))
                    {
                        value = builder->CreateICmpNE(value,
                                                      llvm::Constant::getNullValue(value->getType()),
                                                      "to_bool");
                    }

                    llvm::Value* result = builder->CreateCall(
                        getOrCreateExternalFunction("py_create_bool",
                                                    llvm::PointerType::get(getContext(), 0),
                                                    {llvm::Type::getInt1Ty(getContext())},
                                                    false),
                        {value},
                        "bool_obj");

                    ObjectLifecycleManager::attachTypeMetadata(result, PY_TYPE_BOOL);
                    return result;
                }

                case PY_TYPE_STRING:
                {
                    DEBUG_LOG("处理字符串字面量");
                    // 字符串字面量应该已经是指针类型
                    llvm::Value* result = builder->CreateCall(
                        getOrCreateExternalFunction("py_create_string",
                                                    llvm::PointerType::get(getContext(), 0),
                                                    {llvm::PointerType::get(getContext(), 0)},
                                                    false),
                        {value},
                        "string_obj");

                    ObjectLifecycleManager::attachTypeMetadata(result, PY_TYPE_STRING);
                    return result;
                }

                default:
                    DEBUG_LOG("未知字面量类型: " << typeId);
                    break;
            }
        }

        // 处理基本类型变量（从内存加载）
        if (isPointer)
        {
            llvm::Type* loadType = nullptr;
            std::string funcName;

            // 根据要求的类型或分配类型确定加载类型
            if (allocatedType)
            {
                if (allocatedType->isDoubleTy() && (typeId == PY_TYPE_DOUBLE || typeId == PY_TYPE_ANY))
                {
                    loadType = llvm::Type::getDoubleTy(getContext());
                    funcName = "py_create_double";
                }
                else if (allocatedType->isIntegerTy(32) && (typeId == PY_TYPE_INT || typeId == PY_TYPE_ANY))
                {
                    loadType = llvm::Type::getInt32Ty(getContext());
                    funcName = "py_create_int";
                }
                else if (allocatedType->isIntegerTy(1) && (typeId == PY_TYPE_BOOL || typeId == PY_TYPE_ANY))
                {
                    loadType = llvm::Type::getInt1Ty(getContext());
                    funcName = "py_create_bool";
                }
                else if (allocatedType->isPointerTy() && (typeId == PY_TYPE_STRING || typeId == PY_TYPE_ANY))
                {
                    loadType = llvm::PointerType::get(getContext(), 0);
                    funcName = "py_create_string";
                }
            }

            if (!loadType)
            {
                // 如果未确定加载类型，根据请求类型确定
                switch (typeId)
                {
                    case PY_TYPE_INT:
                        loadType = llvm::Type::getInt32Ty(getContext());
                        funcName = "py_create_int";
                        break;
                    case PY_TYPE_DOUBLE:
                        loadType = llvm::Type::getDoubleTy(getContext());
                        funcName = "py_create_double";
                        break;
                    case PY_TYPE_BOOL:
                        loadType = llvm::Type::getInt1Ty(getContext());
                        funcName = "py_create_bool";
                        break;
                    case PY_TYPE_STRING:
                        loadType = llvm::PointerType::get(getContext(), 0);
                        funcName = "py_create_string";
                        break;
                    default:
                        // 默认尝试作为整数处理
                        loadType = llvm::Type::getInt32Ty(getContext());
                        funcName = "py_create_int";
                        break;
                }
            }

            // 加载值并创建对象
            DEBUG_LOG("加载基本类型: " << (loadType->isDoubleTy() ? "double" : loadType->isIntegerTy(32) ? "int32"
                                                                           : loadType->isIntegerTy(1)    ? "bool"
                                                                                                         : "其他"));

            llvm::Value* loadedValue = builder->CreateLoad(loadType, value, "loaded_primitive");

            llvm::Value* result = builder->CreateCall(
                getOrCreateExternalFunction(funcName,
                                            llvm::PointerType::get(getContext(), 0),
                                            {loadType},
                                            false),
                {loadedValue},
                "primitive_obj");

            ObjectLifecycleManager::attachTypeMetadata(result, typeId);
            return result;
        }
    }

    // 任何未处理的情况，返回原值并附加类型元数据
    DEBUG_LOG("使用原始值 (类型ID: " << typeId << ")");
    if (isPointer)
    {
        ObjectLifecycleManager::attachTypeMetadata(value, typeId);
    }
    return value;
}

// 辅助函数：处理索引操作
llvm::Value* PyCodeGen::handleIndexOperation(llvm::Value* target, llvm::Value* index,
                                             ObjectType* targetType, ObjectType* indexType)
{
    // 基本参数验证
    if (!target || !index || !targetType || !indexType)
    {
        return logTypeError("Invalid parameters for index operation");
    }

    // 获取目标类型和索引类型ID
    int targetTypeId = targetType->getTypeId();
    int indexTypeId = indexType->getTypeId();

    DEBUG_LOG("索引操作: 目标类型ID=" << targetTypeId << ", 索引类型ID=" << indexTypeId);

    // 检查目标是否可索引
    if (targetTypeId == PY_TYPE_INT || targetTypeId == PY_TYPE_DOUBLE || targetTypeId == PY_TYPE_BOOL)
    {
        return logTypeError("目标对象类型 '" + targetType->getName() + "' 不支持索引操作");
    }

    // 确保目标是Python对象指针
    bool isTargetPointer = target->getType()->isPointerTy();
    if (!isTargetPointer)
    {
        DEBUG_LOG("目标不是指针类型，尝试转换为Python对象");
        target = ensurePythonObject(target, targetType);
        if (!target)
        {
            return logTypeError("无法将目标转换为Python对象");
        }
    }

    // 处理列表索引
    if (targetTypeId == PY_TYPE_LIST)
    {
        DEBUG_LOG("处理列表索引操作");

        // 验证索引类型
        if (indexTypeId != PY_TYPE_INT && indexTypeId != PY_TYPE_ANY)
        {
            return logTypeError("列表索引必须是整数类型");
        }

        // 提取索引值 - 确保是整数类型
        llvm::Value* indexValue = index;
        if (index->getType()->isPointerTy())
        {
            DEBUG_LOG("索引是指针类型，提取整数值");
            indexValue = builder->CreateCall(
                getOrCreateExternalFunction("py_extract_int",
                                            llvm::Type::getInt32Ty(getContext()),
                                            {llvm::PointerType::get(getContext(), 0)},
                                            false),
                {index},
                "extracted_index");
        }
        else if (!index->getType()->isIntegerTy(32))
        {
            DEBUG_LOG("索引不是i32类型，转换为i32");
            indexValue = builder->CreateIntCast(index, llvm::Type::getInt32Ty(getContext()), true, "index_to_i32");
        }

        // 获取列表元素类型ID
        int elemTypeId = PY_TYPE_ANY;  // 默认为ANY
        if (ListType* listType = dynamic_cast<ListType*>(targetType))
        {
            if (listType->getElementType())
            {
                elemTypeId = listType->getElementType()->getTypeId();
                DEBUG_LOG("列表元素类型ID: " << elemTypeId);
            }
        }

        // 获取列表元素
        llvm::Value* result = builder->CreateCall(
            getOrCreateExternalFunction("py_list_get_item",
                                        llvm::PointerType::get(getContext(), 0),
                                        {llvm::PointerType::get(getContext(), 0),
                                         llvm::Type::getInt32Ty(getContext())},
                                        false),
            {target, indexValue},
            "list_item");

        // 为结果添加类型元数据 - 这是关键步骤
        ObjectLifecycleManager::attachTypeMetadata(result, elemTypeId);

        // 跟踪对象生命周期
        trackObject(result);

        DEBUG_LOG("列表索引操作完成，返回元素（类型ID: " << elemTypeId << "）");
        return result;
    }

    // 处理字典索引
    else if (targetTypeId == PY_TYPE_DICT)
    {
        DEBUG_LOG("处理字典索引操作");

        // 确保索引是Python对象
        if (!index->getType()->isPointerTy())
        {
            DEBUG_LOG("索引不是指针类型，转换为Python对象");
            index = ensurePythonObject(index, indexType);
            if (!index)
            {
                return logTypeError("无法将索引转换为Python对象");
            }
        }

        // 获取字典值类型ID
        int valueTypeId = PY_TYPE_ANY;  // 默认为ANY
        if (DictType* dictType = dynamic_cast<DictType*>(targetType))
        {
            if (dictType->getValueType())
            {
                valueTypeId = dictType->getValueType()->getTypeId();
                DEBUG_LOG("字典值类型ID: " << valueTypeId);
            }
        }

        // 获取字典元素
        llvm::Value* result = builder->CreateCall(
            getOrCreateExternalFunction("py_dict_get_item",
                                        llvm::PointerType::get(getContext(), 0),
                                        {llvm::PointerType::get(getContext(), 0),
                                         llvm::PointerType::get(getContext(), 0)},
                                        false),
            {target, index},
            "dict_item");

        // 为结果添加类型元数据
        ObjectLifecycleManager::attachTypeMetadata(result, valueTypeId);

        // 跟踪对象生命周期
        trackObject(result);

        DEBUG_LOG("字典索引操作完成，返回元素（类型ID: " << valueTypeId << "）");
        return result;
    }

    // 处理字符串索引
    else if (targetTypeId == PY_TYPE_STRING)
    {
        DEBUG_LOG("处理字符串索引操作");

        // 验证索引类型
        if (indexTypeId != PY_TYPE_INT && indexTypeId != PY_TYPE_ANY)
        {
            return logTypeError("字符串索引必须是整数类型");
        }

        // 提取索引值 - 确保是整数类型
        llvm::Value* indexValue = index;
        if (index->getType()->isPointerTy())
        {
            DEBUG_LOG("索引是指针类型，提取整数值");
            indexValue = builder->CreateCall(
                getOrCreateExternalFunction("py_extract_int",
                                            llvm::Type::getInt32Ty(getContext()),
                                            {llvm::PointerType::get(getContext(), 0)},
                                            false),
                {index},
                "extracted_index");
        }
        else if (!index->getType()->isIntegerTy(32))
        {
            DEBUG_LOG("索引不是i32类型，转换为i32");
            indexValue = builder->CreateIntCast(index, llvm::Type::getInt32Ty(getContext()), true, "index_to_i32");
        }

        // 获取字符串字符
        llvm::Value* result = builder->CreateCall(
            getOrCreateExternalFunction("py_string_get_char",
                                        llvm::PointerType::get(getContext(), 0),
                                        {llvm::PointerType::get(getContext(), 0),
                                         llvm::Type::getInt32Ty(getContext())},
                                        false),
            {target, indexValue},
            "string_char");

        // 为结果添加类型元数据 - 字符串字符仍然是字符串类型
        ObjectLifecycleManager::attachTypeMetadata(result, PY_TYPE_STRING);

        // 跟踪对象生命周期
        trackObject(result);

        DEBUG_LOG("字符串索引操作完成，返回字符");
        return result;
    }

    // 处理元组索引 (如果支持)
    else if (targetTypeId == PY_TYPE_TUPLE)
    {
        DEBUG_LOG("处理元组索引操作");

        // 验证索引类型
        if (indexTypeId != PY_TYPE_INT && indexTypeId != PY_TYPE_ANY)
        {
            return logTypeError("元组索引必须是整数类型");
        }

        // 提取索引值
        llvm::Value* indexValue = index;
        if (index->getType()->isPointerTy())
        {
            indexValue = builder->CreateCall(
                getOrCreateExternalFunction("py_extract_int",
                                            llvm::Type::getInt32Ty(getContext()),
                                            {llvm::PointerType::get(getContext(), 0)},
                                            false),
                {index},
                "extracted_index");
        }

        // 获取元组元素（假设有py_tuple_get_item函数）
        llvm::Value* result = builder->CreateCall(
            getOrCreateExternalFunction("py_tuple_get_item",
                                        llvm::PointerType::get(getContext(), 0),
                                        {llvm::PointerType::get(getContext(), 0),
                                         llvm::Type::getInt32Ty(getContext())},
                                        false),
            {target, indexValue},
            "tuple_item");

        // 为结果添加通用类型元数据
        ObjectLifecycleManager::attachTypeMetadata(result, PY_TYPE_ANY);

        // 跟踪对象生命周期
        trackObject(result);

        return result;
    }

    // 处理ANY类型（尝试运行时类型检查）
    // 处理ANY类型（尝试运行时类型检查）
    else if (targetTypeId == PY_TYPE_ANY)
    {
        DEBUG_LOG("处理ANY类型的索引操作，尝试运行时类型检查");

        // 获取目标对象的运行时类型
        llvm::Value* runtimeTypeId = builder->CreateCall(
            getOrCreateExternalFunction("py_get_object_type_id",
                                        llvm::Type::getInt32Ty(getContext()),
                                        {llvm::PointerType::get(getContext(), 0)},
                                        false),
            {target},
            "runtime_type_id");

        // 创建基本块进行类型分支
        llvm::BasicBlock* currentBB = builder->GetInsertBlock();
        llvm::Function* parentFunc = currentBB->getParent();

        // 类型检查基本块
        llvm::BasicBlock* isListBB = llvm::BasicBlock::Create(getContext(), "is_list", parentFunc);
        llvm::BasicBlock* isDictBB = llvm::BasicBlock::Create(getContext(), "is_dict", parentFunc);
        llvm::BasicBlock* isStringBB = llvm::BasicBlock::Create(getContext(), "is_string", parentFunc);
        llvm::BasicBlock* errorBB = llvm::BasicBlock::Create(getContext(), "not_indexable", parentFunc);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(getContext(), "merge", parentFunc);

        // 检查是否为列表
        llvm::Value* isList = builder->CreateICmpEQ(
            runtimeTypeId,
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), PY_TYPE_LIST),
            "is_list");

        builder->CreateCondBr(isList, isListBB, isDictBB);

        // 列表处理块
        builder->SetInsertPoint(isListBB);
        // 获取正确的类型实例
        ObjectType* anyType = TypeRegistry::getInstance().getType("any");
        ListType* listType = TypeRegistry::getInstance().getListType(anyType);
        llvm::Value* listResult = handleIndexOperation(target, index, listType, indexType);
        builder->CreateBr(mergeBB);

        // 字典处理块
        builder->SetInsertPoint(isDictBB);
        // 检查是否为字典
        llvm::Value* isDict = builder->CreateICmpEQ(
            runtimeTypeId,
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), PY_TYPE_DICT),
            "is_dict");
        builder->CreateCondBr(isDict, isDictBB, isStringBB);

        // 获取正确的字典类型实例
        DictType* dictType = TypeRegistry::getInstance().getDictType(anyType, anyType);
        llvm::Value* dictResult = handleIndexOperation(target, index, dictType, indexType);
        builder->CreateBr(mergeBB);

        // 字符串处理块
        builder->SetInsertPoint(isStringBB);
        // 检查是否为字符串
        llvm::Value* isString = builder->CreateICmpEQ(
            runtimeTypeId,
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), PY_TYPE_STRING),
            "is_string");
        builder->CreateCondBr(isString, isStringBB, errorBB);

        // 获取正确的字符串类型实例
        ObjectType* stringType = TypeRegistry::getInstance().getType("string");
        llvm::Value* stringResult = handleIndexOperation(target, index, stringType, indexType);
        builder->CreateBr(mergeBB);

        // 错误处理块
        builder->SetInsertPoint(errorBB);
        llvm::Value* errorResult = logTypeError("运行时类型不支持索引操作");
        builder->CreateBr(mergeBB);

        // 合并结果
        builder->SetInsertPoint(mergeBB);
        llvm::PHINode* result = builder->CreatePHI(
            llvm::PointerType::get(getContext(), 0), 4, "index_result");
        result->addIncoming(listResult, isListBB);
        result->addIncoming(dictResult, isDictBB);
        result->addIncoming(stringResult, isStringBB);
        result->addIncoming(errorResult, errorBB);

        // 确保结果有类型元数据（ANY类型）
        ObjectLifecycleManager::attachTypeMetadata(result, PY_TYPE_ANY);
        return result;
    }

    // 不支持索引的其他类型
    DEBUG_LOG("类型 '" << targetType->getName() << "' 不支持索引操作");
    return logTypeError("类型 '" + targetType->getName() + "' 不支持索引操作");
}

// 跟踪对象以进行生命周期管理
void PyCodeGen::trackObject(llvm::Value* obj)
{
    if (obj && runtime)
    {
        runtime->trackObject(obj);
    }
}

llvm::Value* PyCodeGen::generateUnaryOperation(char op, llvm::Value* operand, ObjectType* operandType)
{
    if (!operand || !operandType)
    {
        return logError("Invalid operand for unary operation");
    }

    // 获取类型ID
    int typeId = operandType->getTypeId();

    // 获取类型操作注册表
    auto& registry = TypeOperationRegistry::getInstance();

    // 查找一元操作描述符
    UnaryOpDescriptor* descriptor = registry.getUnaryOpDescriptor(op, typeId);

    // 如果找到了操作描述符，使用它处理操作
    if (descriptor)
    {
        // 如果定义了运行时函数
        if (!descriptor->customImpl && !descriptor->runtimeFunction.empty())
        {
            // 获取运行时函数
            llvm::Function* runtimeFunc = getOrCreateExternalFunction(
                descriptor->runtimeFunction,
                llvm::PointerType::get(getContext(), 0),
                {llvm::PointerType::get(getContext(), 0)});

            // 确保操作数是指针类型
            if (!operand->getType()->isPointerTy())
            {
                operand = ObjectLifecycleManager::createObject(*this, operand, typeId);
            }

            // 调用运行时函数
            return builder->CreateCall(runtimeFunc, {operand}, "unaryop");
        }
        // 如果定义了自定义实现函数
        else if (descriptor->customImpl)
        {
            return descriptor->customImpl(*this, operand);
        }
    }

    // 针对基本运算符的内置实现
    if (typeId == PY_TYPE_INT)
    {
        // 整数一元运算
        switch (op)
        {
            case '-':
                return builder->CreateNeg(operand, "negtmp");
            case '!':
            {
                // 逻辑非：将整数与0比较
                return builder->CreateICmpEQ(
                    operand, llvm::ConstantInt::get(operand->getType(), 0), "lnottmp");
            }
            default:
                break;
        }
    }
    else if (typeId == PY_TYPE_DOUBLE)
    {
        // 浮点数一元运算
        switch (op)
        {
            case '-':
                return builder->CreateFNeg(operand, "negtmp");
            case '!':
            {
                // 逻辑非：将浮点数与0比较
                return builder->CreateFCmpOEQ(
                    operand, llvm::ConstantFP::get(operand->getType(), 0.0), "lnottmp");
            }
            default:
                break;
        }
    }
    else if (typeId == PY_TYPE_BOOL)
    {
        // 布尔一元运算
        switch (op)
        {
            case '!':
                return builder->CreateNot(operand, "nottmp");
            default:
                break;
        }
    }

    // 如果所有方法都无法处理，报告错误
    return logError("Unsupported unary operation '" + std::string(1, op) + "' for type '" + operandType->getName() + "'");
}

llvm::Value* PyCodeGen::generateIndexOperation(llvm::Value* target, llvm::Value* index,
                                               ObjectType* targetType, ObjectType* indexType)
{
    if (!target || !index || !targetType)
    {
        return logError("Invalid parameters for index operation");
    }

    // 获取目标类型ID
    int targetTypeId = targetType->getTypeId();

    // 首先检查目标是否可索引 - 这是解决 "Expected type 5, got 1" 错误的关键
    if (targetTypeId == PY_TYPE_INT)
    {
        return logTypeError("Cannot index an integer value - expected a list, dictionary, or string"), nullptr;
    }

    if (!TypeFeatureChecker::hasFeature(targetType, "container") && !TypeFeatureChecker::hasFeature(targetType, "sequence"))
    {
        return logTypeError("Type '" + targetType->getName() + "' does not support indexing"), nullptr;
    }

    // 列表索引操作
    if (dynamic_cast<ListType*>(targetType))
    {
        // 检查索引类型
        if (!indexType || indexType->getTypeId() != PY_TYPE_INT)
        {
            return logTypeError("List indices must be integers"), nullptr;
        }

        // 获取索引操作函数
        llvm::Function* getItemFunc = getOrCreateExternalFunction(
            "py_list_get_item",
            llvm::PointerType::get(getContext(), 0),
            {
                llvm::PointerType::get(getContext(), 0),  // list
                llvm::Type::getInt32Ty(getContext())      // index
            });

        // 确保索引是整数值
        llvm::Value* indexValue = index;
        if (index->getType()->isPointerTy())
        {
            // 如果是对象指针，需要提取整数值
            llvm::Function* extractIntFunc = getOrCreateExternalFunction(
                "py_extract_int",
                llvm::Type::getInt32Ty(getContext()),
                {llvm::PointerType::get(getContext(), 0)});
            indexValue = builder->CreateCall(extractIntFunc, {index}, "idxtmp");
        }
        else if (!index->getType()->isIntegerTy(32))
        {
            // 如果不是i32类型的整数，进行类型转换
            indexValue = builder->CreateIntCast(
                index, llvm::Type::getInt32Ty(getContext()), true, "idxtmp");
        }

        // 调用列表索引函数
        return builder->CreateCall(getItemFunc, {target, indexValue}, "listitem");
    }
    // 字典索引操作
    else if (dynamic_cast<DictType*>(targetType))
    {
        // 获取字典索引操作函数
        llvm::Function* getDictItemFunc = getOrCreateExternalFunction(
            "py_dict_get_item",
            llvm::PointerType::get(getContext(), 0),
            {
                llvm::PointerType::get(getContext(), 0),  // dict
                llvm::PointerType::get(getContext(), 0)   // key
            });

        // 确保键是对象指针类型
        llvm::Value* keyValue = index;
        if (!index->getType()->isPointerTy())
        {
            // 如果不是对象指针，创建对象
            keyValue = ObjectLifecycleManager::createObject(*this, index, indexType->getTypeId());
        }

        // 调用字典索引函数
        return builder->CreateCall(getDictItemFunc, {target, keyValue}, "dictitem");
    }
    // 字符串索引操作
    else if (targetTypeId == PY_TYPE_STRING)
    {
        // 检查索引类型
        if (!indexType || indexType->getTypeId() != PY_TYPE_INT)
        {
            return logTypeError("String indices must be integers"), nullptr;
        }

        // 获取字符串索引操作函数
        llvm::Function* getCharFunc = getOrCreateExternalFunction(
            "py_string_get_char",
            llvm::PointerType::get(getContext(), 0),
            {
                llvm::PointerType::get(getContext(), 0),  // string
                llvm::Type::getInt32Ty(getContext())      // index
            });

        // 确保索引是整数值
        llvm::Value* indexValue = index;
        if (index->getType()->isPointerTy())
        {
            llvm::Function* extractIntFunc = getOrCreateExternalFunction(
                "py_extract_int",
                llvm::Type::getInt32Ty(getContext()),
                {llvm::PointerType::get(getContext(), 0)});
            indexValue = builder->CreateCall(extractIntFunc, {index}, "idxtmp");
        }
        else if (!index->getType()->isIntegerTy(32))
        {
            indexValue = builder->CreateIntCast(
                index, llvm::Type::getInt32Ty(getContext()), true, "idxtmp");
        }

        // 调用字符串索引函数
        return builder->CreateCall(getCharFunc, {target, indexValue}, "charitem");
    }

    // 如果无法处理，报告错误
    return logError("Unsupported indexing operation for type '" + targetType->getName() + "'");
}

// 类型转换函数改进
llvm::Value* PyCodeGen::generateTypeConversion(llvm::Value* value, ObjectType* fromType, ObjectType* toType)
{
    if (!value || !fromType || !toType)
    {
        return value;  // 返回原值而不是nullptr，以提高鲁棒性
    }

    // 如果类型相同，不需要转换
    if (fromType->getTypeId() == toType->getTypeId())
    {
        return value;
    }

    int fromTypeId = fromType->getTypeId();
    int toTypeId = toType->getTypeId();

    DEBUG_LOG("执行类型转换: 从类型 " << fromTypeId << " 到类型 " << toTypeId);

    // 查询类型转换描述符
    auto& registry = TypeOperationRegistry::getInstance();
    TypeConversionDescriptor* descriptor = registry.getTypeConversionDescriptor(fromTypeId, toTypeId);

    // 如果找到转换描述符，使用它
    if (descriptor)
    {
        if (!descriptor->runtimeFunction.empty())
        {
            DEBUG_LOG("使用运行时转换函数: " << descriptor->runtimeFunction);

            // 确保值是指针类型
            if (!value->getType()->isPointerTy())
            {
                value = ensurePythonObject(value, fromType);
            }

            // 调用运行时转换函数
            llvm::Function* convFunc = getOrCreateExternalFunction(
                descriptor->runtimeFunction,
                llvm::PointerType::get(getContext(), 0),
                {llvm::PointerType::get(getContext(), 0)},
                false);

            return builder->CreateCall(convFunc, {value}, "conv_result");
        }

        if (descriptor->customImpl)
        {
            DEBUG_LOG("使用自定义转换实现");
            return descriptor->customImpl(*this, value);
        }
    }

    // 处理特殊情况: any类型转换
    if (fromTypeId == PY_TYPE_ANY)
    {
        DEBUG_LOG("从any类型转换到具体类型: " << toTypeId);

        std::string convFuncName;
        switch (toTypeId)
        {
            case PY_TYPE_INT:
                convFuncName = "py_convert_any_to_int";
                break;
            case PY_TYPE_DOUBLE:
                convFuncName = "py_convert_any_to_double";
                break;
            case PY_TYPE_BOOL:
                convFuncName = "py_convert_any_to_bool";
                break;
            case PY_TYPE_STRING:
                convFuncName = "py_convert_any_to_string";
                break;
            default:
                // 对于其他类型，使用本地转换
                if (value->getType()->isPointerTy())
                {
                    return value;  // 已经是指针，直接返回
                }
                // 否则包装为目标类型
                return ensurePythonObject(value, toType);
        }

        // 如果有专门的any转换函数，使用它
        if (!convFuncName.empty())
        {
            if (!value->getType()->isPointerTy())
            {
                value = ensurePythonObject(value, fromType);
            }

            llvm::Function* convFunc = getOrCreateExternalFunction(
                convFuncName,
                llvm::PointerType::get(getContext(), 0),
                {llvm::PointerType::get(getContext(), 0)},
                false);

            return builder->CreateCall(convFunc, {value}, "any_conv_result");
        }
    }

    // 处理基本类型间的LLVM级转换
    if (value->getType()->isIntegerTy() && toTypeId == PY_TYPE_DOUBLE)
    {
        DEBUG_LOG("执行LLVM int到double转换");
        llvm::Value* fpVal = builder->CreateSIToFP(
            value,
            llvm::Type::getDoubleTy(getContext()),
            "int_to_fp");

        return builder->CreateCall(
            getOrCreateExternalFunction("py_create_double",
                                        llvm::PointerType::get(getContext(), 0),
                                        {llvm::Type::getDoubleTy(getContext())},
                                        false),
            {fpVal},
            "fp_obj");
    }

    if (value->getType()->isDoubleTy() && toTypeId == PY_TYPE_INT)
    {
        DEBUG_LOG("执行LLVM double到int转换");
        llvm::Value* intVal = builder->CreateFPToSI(
            value,
            llvm::Type::getInt32Ty(getContext()),
            "fp_to_int");

        return builder->CreateCall(
            getOrCreateExternalFunction("py_create_int",
                                        llvm::PointerType::get(getContext(), 0),
                                        {llvm::Type::getInt32Ty(getContext())},
                                        false),
            {intVal},
            "int_obj");
    }

    // 如果没有找到更好的转换方法，尝试通用对象转换
    DEBUG_LOG("执行通用对象转换");
    if (!value->getType()->isPointerTy())
    {
        value = ensurePythonObject(value, fromType);
    }

    // 使用通用的对象转换函数 py_object_convert
    llvm::Function* generalConvFunc = getOrCreateExternalFunction(
        "py_object_convert",
        llvm::PointerType::get(getContext(), 0),
        {llvm::PointerType::get(getContext(), 0),
         llvm::Type::getInt32Ty(getContext())},
        false);

    return builder->CreateCall(
        generalConvFunc,
        {value,
         llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), toTypeId)},
        "general_conv");
}

llvm::Value* PyCodeGen::createDefaultValue(ObjectType* type)
{
    if (!type)
    {
        DEBUG_LOG("警告：尝试为空类型创建默认值，返回nullptr");
        return nullptr;
    }

    DEBUG_LOG("为类型 '" << type->getName() << "' 创建默认值，类型ID: " << type->getTypeId());

    // 根据类型ID选择合适的默认值创建方法
    switch (type->getTypeId())
    {
        case PY_TYPE_INT:
            return llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), 0);

        case PY_TYPE_DOUBLE:
            return llvm::ConstantFP::get(llvm::Type::getDoubleTy(getContext()), 0.0);

        case PY_TYPE_BOOL:
            return llvm::ConstantInt::get(llvm::Type::getInt1Ty(getContext()), 0);

        case PY_TYPE_STRING:
        {
            // 创建空字符串
            llvm::Function* createStrFunc = getOrCreateExternalFunction(
                "py_create_string",
                llvm::PointerType::get(getContext(), 0),
                {llvm::PointerType::get(llvm::Type::getInt8Ty(getContext()), 0)});

            llvm::Value* emptyStr = builder->CreateGlobalStringPtr("", "empty_str");
            llvm::Value* result = builder->CreateCall(createStrFunc, {emptyStr}, "default_string");

            // 附加类型元数据
            ObjectLifecycleManager::attachTypeMetadata(result, PY_TYPE_STRING);
            return result;
        }

        case PY_TYPE_LIST:
        {
            // 创建空列表
            llvm::Function* createListFunc = getOrCreateExternalFunction(
                "py_create_list",
                llvm::PointerType::get(getContext(), 0),
                {llvm::Type::getInt32Ty(getContext())});

            llvm::Value* result = builder->CreateCall(
                createListFunc,
                {llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), 0)},
                "default_list");

            // 附加类型元数据
            ObjectLifecycleManager::attachTypeMetadata(result, PY_TYPE_LIST);

            // 如果是类型化列表，尝试添加元素类型信息
            if (ListType* listType = dynamic_cast<ListType*>(type))
            {
                if (auto elemType = listType->getElementType())
                {
                    int elemTypeId = elemType->getTypeId();
                    // 这里可以添加元素类型的元数据，如果运行时支持的话
                    DEBUG_LOG("列表元素类型: " << elemType->getName() << " (ID: " << elemTypeId << ")");
                }
            }

            return result;
        }

        case PY_TYPE_DICT:
        {
            // 创建空字典
            llvm::Function* createDictFunc = getOrCreateExternalFunction(
                "py_create_dict",
                llvm::PointerType::get(getContext(), 0),
                {});

            llvm::Value* result = builder->CreateCall(createDictFunc, {}, "default_dict");

            // 附加类型元数据
            ObjectLifecycleManager::attachTypeMetadata(result, PY_TYPE_DICT);

            // 如果是类型化字典，尝试添加键值类型信息
            if (DictType* dictType = dynamic_cast<DictType*>(type))
            {
                if (auto keyType = dictType->getKeyType())
                {
                    int keyTypeId = keyType->getTypeId();
                    DEBUG_LOG("字典键类型: " << keyType->getName() << " (ID: " << keyTypeId << ")");
                }
                if (auto valueType = dictType->getValueType())
                {
                    int valueTypeId = valueType->getTypeId();
                    DEBUG_LOG("字典值类型: " << valueType->getName() << " (ID: " << valueTypeId << ")");
                }
            }

            return result;
        }

        case PY_TYPE_NONE:
        {
            // 获取None对象
            llvm::Function* getNoneFunc = getOrCreateExternalFunction(
                "py_get_none",
                llvm::PointerType::get(getContext(), 0),
                {});

            if (getNoneFunc)
            {
                llvm::Value* result = builder->CreateCall(getNoneFunc, {}, "none_value");
                ObjectLifecycleManager::attachTypeMetadata(result, PY_TYPE_NONE);
                return result;
            }
            else
            {
                DEBUG_LOG("警告：找不到py_get_none函数，使用null指针作为None值");
                return llvm::ConstantPointerNull::get(llvm::PointerType::get(getContext(), 0));
            }
        }

        default:
            // 对于其他类型（如自定义类或未知类型）// 目前还没有上实现先注释了
            /* if (type->getTypeId() > PY_TYPE_DICT) {
                // 对于对象类型，尝试使用默认构造函数
                if (type->getCategory() == ObjectType::Category::Class) {
                    DEBUG_LOG("尝试为类类型 '" << type->getName() << "' 创建默认实例");
                    
                    // 获取类名
                    std::string className = type->getName();
                    
                    // 创建类实例
                    llvm::Function* createObjFunc = getOrCreateExternalFunction(
                        "py_create_object",
                        llvm::PointerType::get(getContext(), 0),
                        {llvm::Type::getInt32Ty(getContext())}
                    );
                    
                    if (createObjFunc) {
                        llvm::Value* result = builder->CreateCall(
                            createObjFunc, 
                            {llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), type->getTypeId())},
                            "default_object"
                        );
                        
                        // 附加类型元数据
                        ObjectLifecycleManager::attachTypeMetadata(result, type->getTypeId());
                        return result;
                    }
                }
            } */

            // 如果是引用类型但没有特殊处理，返回null指针
            if (type->isReference())
            {
                DEBUG_LOG("为引用类型 '" << type->getName() << "' 返回null指针默认值");
                return llvm::ConstantPointerNull::get(llvm::PointerType::get(getContext(), 0));
            }

            // 对于未识别的非引用类型，返回整数0
            DEBUG_LOG("警告：未知类型 '" << type->getName() << "'，返回int默认值0");
            return llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), 0);
    }
}

//===----------------------------------------------------------------------===//
// namespace PyCodeGenHelper
//===----------------------------------------------------------------------===//

namespace PyCodeGenHelper
{
// 已有的方法
llvm::Value* convertToDouble(PyCodeGen& codegen, llvm::Value* value);
std::vector<ObjectType*> getFunctionParamTypes(const FunctionAST* func);

// 补全缺失的方法
llvm::Value* convertToInt(PyCodeGen& codegen, llvm::Value* value)
{
    if (!value) return nullptr;

    if (value->getType()->isIntegerTy(32))
    {
        return value;
    }
    else if (value->getType()->isDoubleTy())
    {
        return codegen.getBuilder().CreateFPToSI(value,
                                                 llvm::Type::getInt32Ty(codegen.getContext()), "doubletoint");
    }
    else if (value->getType()->isIntegerTy(1))
    {
        return codegen.getBuilder().CreateZExt(value,
                                               llvm::Type::getInt32Ty(codegen.getContext()), "booltoint");
    }
    else if (value->getType()->isPointerTy())
    {
        // 使用运行时函数提取整数值
        llvm::Function* extractFunc = codegen.getOrCreateExternalFunction(
            "py_extract_int",
            llvm::Type::getInt32Ty(codegen.getContext()),
            {llvm::PointerType::get(codegen.getContext(), 0)});
        return codegen.getBuilder().CreateCall(extractFunc, {value}, "extractint");
    }

    // 未知类型
    codegen.logError("Cannot convert value to int");
    return nullptr;
}

llvm::Value* convertToBool(PyCodeGen& codegen, llvm::Value* value)
{
    if (!value) return nullptr;

    if (value->getType()->isIntegerTy(1))
    {
        return value;
    }
    else if (value->getType()->isIntegerTy())
    {
        return codegen.getBuilder().CreateICmpNE(value,
                                                 llvm::ConstantInt::get(value->getType(), 0), "inttobool");
    }
    else if (value->getType()->isDoubleTy())
    {
        return codegen.getBuilder().CreateFCmpONE(value,
                                                  llvm::ConstantFP::get(value->getType(), 0.0), "doubletobool");
    }
    else if (value->getType()->isPointerTy())
    {
        // 使用运行时函数提取布尔值
        llvm::Function* toBoolFunc = codegen.getOrCreateExternalFunction(
            "py_object_to_bool",
            llvm::Type::getInt1Ty(codegen.getContext()),
            {llvm::PointerType::get(codegen.getContext(), 0)});
        return codegen.getBuilder().CreateCall(toBoolFunc, {value}, "tobool");
    }

    // 未知类型
    codegen.logError("Cannot convert value to bool");
    return nullptr;
}

llvm::Value* deepCopyValue(PyCodeGen& codegen, llvm::Value* value, ObjectType* type)
{
    if (!value || !type) return value;

    // 只有引用类型需要深拷贝
    if (!type->isReference()) return value;

    // 使用运行时函数复制对象
    llvm::Function* copyFunc = codegen.getOrCreateExternalFunction(
        "py_object_deep_copy",
        llvm::PointerType::get(codegen.getContext(), 0),
        {llvm::PointerType::get(codegen.getContext(), 0),
         llvm::Type::getInt32Ty(codegen.getContext())});

    // 确保值是指针类型
    if (!value->getType()->isPointerTy())
    {
        value = ObjectLifecycleManager::createObject(codegen, value, type->getTypeId());
    }

    llvm::Value* typeId = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(codegen.getContext()), type->getTypeId());

    return codegen.getBuilder().CreateCall(copyFunc, {value, typeId}, "deepcopy");
}

llvm::Value* createLocalVariable(PyCodeGen& codegen, const std::string& name,
                                 ObjectType* type, llvm::Value* initValue)
{
    if (!type) return nullptr;

    // 获取变量在当前函数中的存储
    llvm::Function* currentFunc = codegen.getCurrentFunction();
    if (!currentFunc)
    {
        codegen.logError("Cannot create local variable outside of function");
        return nullptr;
    }

    llvm::IRBuilder<>& builder = codegen.getBuilder();

    // 创建在函数入口处的alloca指令
    llvm::BasicBlock& entryBlock = currentFunc->getEntryBlock();
    llvm::IRBuilder<> tempBuilder(&entryBlock, entryBlock.begin());

    // 为变量分配空间
    llvm::AllocaInst* alloca = nullptr;

    if (type->isReference())
    {
        // 对于引用类型，分配指针
        alloca = tempBuilder.CreateAlloca(
            llvm::PointerType::get(codegen.getContext(), 0),
            nullptr, name);
    }
    else
    {
        // 对于值类型，直接分配对应类型
        llvm::Type* llvmType = codegen.getLLVMType(type);
        alloca = tempBuilder.CreateAlloca(llvmType, nullptr, name);
    }

    // 如果提供了初始值，进行赋值
    if (initValue)
    {
        // 可能需要类型转换
        if (type->isReference() && !initValue->getType()->isPointerTy())
        {
            initValue = ObjectLifecycleManager::createObject(codegen, initValue, type->getTypeId());
        }
        builder.CreateStore(initValue, alloca);
    }

    // 注册变量到符号表
    codegen.setVariable(name, alloca, type);

    return alloca;
}

llvm::GlobalVariable* createGlobalVariable(PyCodeGen& codegen, const std::string& name,
                                           ObjectType* type, llvm::Constant* initValue)
{
    if (!type) return nullptr;

    llvm::Module* module = codegen.getModule();

    // 检查是否已存在同名全局变量
    if (module->getNamedGlobal(name))
    {
        codegen.logError("Global variable '" + name + "' already exists");
        return nullptr;
    }

    // 确定LLVM类型
    llvm::Type* llvmType = nullptr;
    if (type->isReference())
    {
        // 对于引用类型，使用指针
        llvmType = llvm::PointerType::get(codegen.getContext(), 0);
    }
    else
    {
        // 对于值类型，直接使用对应类型
        llvmType = codegen.getLLVMType(type);
    }

    // 创建默认初始值
    if (!initValue)
    {
        if (type->getTypeId() == PY_TYPE_INT)
        {
            initValue = llvm::ConstantInt::get(llvm::Type::getInt32Ty(codegen.getContext()), 0);
        }
        else if (type->getTypeId() == PY_TYPE_DOUBLE)
        {
            initValue = llvm::ConstantFP::get(llvm::Type::getDoubleTy(codegen.getContext()), 0.0);
        }
        else if (type->getTypeId() == PY_TYPE_BOOL)
        {
            initValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(codegen.getContext()), 0);
        }
        else if (type->isReference())
        {
            initValue = llvm::ConstantPointerNull::get(
                llvm::PointerType::get(codegen.getContext(), 0));
        }
        else
        {
            // 其他类型可能需要特殊处理
            codegen.logError("Cannot create default initializer for type " + type->getName());
            return nullptr;
        }
    }

    // 创建全局变量
    llvm::GlobalVariable* globalVar = new llvm::GlobalVariable(
        *module, llvmType, false, llvm::GlobalValue::ExternalLinkage,
        initValue, name);

    // 注册到符号表
    codegen.setVariable(name, globalVar, type);

    return globalVar;
}

int getTypeIdFromObjectType(ObjectType* type)
{
    if (!type) return PY_TYPE_NONE;
    return type->getTypeId();
}

int getRuntimeTypeId(ObjectType* type)
{
    if (!type) return PY_TYPE_NONE;
    return mapToRuntimeTypeId(type->getTypeId());
}

/* int getBaseTypeId(int typeId)
{
    // 返回类型ID的基本类型
    // 例如，列表的基本类型是列表，元素类型被忽略
    switch (typeId)
    {
        case PY_TYPE_INT:
        case PY_TYPE_DOUBLE:
        case PY_TYPE_BOOL:
        case PY_TYPE_STRING:
        case PY_TYPE_NONE:
            return typeId;

        // 容器类型返回其本身的ID
        case PY_TYPE_LIST:
        case PY_TYPE_DICT:
            return typeId;

        default:
            // 默认返回原类型
            return typeId;
    }
} */

// 获取基本类型ID，忽略指针信息
inline int getBaseTypeId(int typeId)
{
    if (typeId >= 400) return typeId % 100;
    if (typeId >= 100 && typeId < 400)
    {
        // 容器基础类型处理
        if (typeId >= PY_TYPE_LIST_BASE && typeId < PY_TYPE_DICT_BASE) return PY_TYPE_LIST;
        if (typeId >= PY_TYPE_DICT_BASE && typeId < PY_TYPE_FUNC_BASE) return PY_TYPE_DICT;
        if (typeId >= PY_TYPE_FUNC_BASE && typeId < 400) return PY_TYPE_FUNC;
    }
    return typeId;
}


llvm::Value* generateTypeCheckCode(PyCodeGen& codegen, llvm::Value* obj, int expectedTypeId)
{
    if (!obj || !obj->getType()->isPointerTy())
    {
        codegen.logError("Invalid object for type check");
        return nullptr;
    }

    llvm::Function* typeCheckFunc = codegen.getOrCreateExternalFunction(
        "py_check_type",
        llvm::Type::getInt1Ty(codegen.getContext()),
        {llvm::PointerType::get(codegen.getContext(), 0),
         llvm::Type::getInt32Ty(codegen.getContext())});

    llvm::Value* typeIdVal = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(codegen.getContext()), expectedTypeId);

    return codegen.getBuilder().CreateCall(typeCheckFunc, {obj, typeIdVal}, "typecheck");
}

llvm::Value* generateTypeErrorCode(PyCodeGen& codegen, llvm::Value* obj, int expectedTypeId,
                                   const std::string& message)
{
    llvm::Function* typeErrorFunc = codegen.getOrCreateExternalFunction(
        "py_type_error",
        llvm::Type::getVoidTy(codegen.getContext()),
        {llvm::PointerType::get(codegen.getContext(), 0),
         llvm::Type::getInt32Ty(codegen.getContext())});

    llvm::Value* typeIdVal = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(codegen.getContext()), expectedTypeId);

    return codegen.getBuilder().CreateCall(typeErrorFunc, {obj, typeIdVal});
}

llvm::Value* generateListIndexTypeCheck(PyCodeGen& codegen, llvm::Value* target, llvm::Value* index)
{
    if (!target || !index) return nullptr;

    // 确保target是列表
    llvm::Value* isList = generateTypeCheckCode(codegen, target, PY_TYPE_LIST);

    // 创建条件分支
    llvm::Function* currentFunc = codegen.getCurrentFunction();
    llvm::BasicBlock* errorBlock = llvm::BasicBlock::Create(
        codegen.getContext(), "list_type_error", currentFunc);
    llvm::BasicBlock* indexCheckBlock = llvm::BasicBlock::Create(
        codegen.getContext(), "index_check", currentFunc);
    llvm::BasicBlock* continueBlock = llvm::BasicBlock::Create(
        codegen.getContext(), "continue", currentFunc);

    codegen.getBuilder().CreateCondBr(isList, indexCheckBlock, errorBlock);

    // 错误处理块
    codegen.getBuilder().SetInsertPoint(errorBlock);
    generateTypeErrorCode(codegen, target, PY_TYPE_LIST, "Expected list object for indexing");
    codegen.getBuilder().CreateBr(continueBlock);

    // 索引类型检查块
    codegen.getBuilder().SetInsertPoint(indexCheckBlock);

    // 检查索引是否为整数
    llvm::Value* isIntIndex = nullptr;
    if (index->getType()->isIntegerTy(32))
    {
        isIntIndex = llvm::ConstantInt::getTrue(codegen.getContext());
    }
    else if (index->getType()->isPointerTy())
    {
        isIntIndex = generateTypeCheckCode(codegen, index, PY_TYPE_INT);
    }
    else
    {
        isIntIndex = llvm::ConstantInt::getFalse(codegen.getContext());
    }

    llvm::BasicBlock* indexErrorBlock = llvm::BasicBlock::Create(
        codegen.getContext(), "index_type_error", currentFunc);

    codegen.getBuilder().CreateCondBr(isIntIndex, continueBlock, indexErrorBlock);

    // 索引错误处理块
    codegen.getBuilder().SetInsertPoint(indexErrorBlock);
    generateTypeErrorCode(codegen, index, PY_TYPE_INT, "List indices must be integers");
    codegen.getBuilder().CreateBr(continueBlock);

    // 继续执行块
    codegen.getBuilder().SetInsertPoint(continueBlock);

    // 返回PHI节点以获取合并后的值 - 使用PHINode而不是PHI
    llvm::PHINode* result = codegen.getBuilder().CreatePHI(
        llvm::Type::getInt1Ty(codegen.getContext()), 3, "typecheck_result");
    result->addIncoming(llvm::ConstantInt::getFalse(codegen.getContext()), errorBlock);
    result->addIncoming(llvm::ConstantInt::getTrue(codegen.getContext()), indexCheckBlock);
    result->addIncoming(llvm::ConstantInt::getFalse(codegen.getContext()), indexErrorBlock);

    return result;
}

llvm::Value* generateIncRefCode(PyCodeGen& codegen, llvm::Value* obj)
{
    if (!obj || !obj->getType()->isPointerTy())
    {
        return nullptr;  // 不是对象指针，不需要增加引用计数
    }

    // 调用运行时函数增加引用计数
    llvm::Function* incRefFunc = codegen.getOrCreateExternalFunction(
        "py_incref",
        llvm::Type::getVoidTy(codegen.getContext()),
        {llvm::PointerType::get(codegen.getContext(), 0)});

    return codegen.getBuilder().CreateCall(incRefFunc, {obj});
}

llvm::Value* generateDecRefCode(PyCodeGen& codegen, llvm::Value* obj)
{
    if (!obj || !obj->getType()->isPointerTy())
    {
        return nullptr;  // 不是对象指针，不需要减少引用计数
    }

    // 调用运行时函数减少引用计数
    llvm::Function* decRefFunc = codegen.getOrCreateExternalFunction(
        "py_decref",
        llvm::Type::getVoidTy(codegen.getContext()),
        {llvm::PointerType::get(codegen.getContext(), 0)});

    return codegen.getBuilder().CreateCall(decRefFunc, {obj});
}

llvm::Value* generateCopyObjectCode(PyCodeGen& codegen, llvm::Value* obj, ObjectType* type)
{
    if (!obj || !type) return obj;

    // 对于非引用类型，不需要复制
    if (!type->isReference()) return obj;

    // 确保对象是指针类型
    if (!obj->getType()->isPointerTy())
    {
        obj = ObjectLifecycleManager::createObject(codegen, obj, type->getTypeId());
    }

    // 调用运行时函数复制对象
    llvm::Function* copyFunc = codegen.getOrCreateExternalFunction(
        "py_object_copy",
        llvm::PointerType::get(codegen.getContext(), 0),
        {llvm::PointerType::get(codegen.getContext(), 0),
         llvm::Type::getInt32Ty(codegen.getContext())});

    llvm::Value* typeIdVal = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(codegen.getContext()), type->getTypeId());

    return codegen.getBuilder().CreateCall(copyFunc, {obj, typeIdVal}, "objcopy");
}

// 获取LLVM类型
llvm::Type* getLLVMType(llvm::LLVMContext& context, ObjectType* type)
{
    if (!type) return nullptr;

    // 处理基本类型
    switch (type->getTypeId())
    {
        case PY_TYPE_INT:
            return llvm::Type::getInt32Ty(context);
        case PY_TYPE_DOUBLE:
            return llvm::Type::getDoubleTy(context);
        case PY_TYPE_BOOL:
            return llvm::Type::getInt1Ty(context);
        case PY_TYPE_STRING:
        case PY_TYPE_LIST:
        case PY_TYPE_DICT:
        case PY_TYPE_ANY:
            // 引用类型和容器使用指针
            return llvm::PointerType::get(context, 0);
        case PY_TYPE_NONE:
            return llvm::Type::getVoidTy(context);
        default:
            // 默认使用指针
            return llvm::PointerType::get(context, 0);
    }
}
}  // namespace PyCodeGenHelper

//===----------------------------------------------------------------------===//
// 表达式节点访问实现
//===----------------------------------------------------------------------===//

void llvmpy::PyCodeGen::visit(NumberExprAST* expr)
{
    double value = expr->getValue();

    // 创建值
    if (value == (int)value)
    {
        // 整数值
        int intValue = (int)value;
        lastExprValue = llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), intValue);
        lastExprType = PyType::getInt();
    }
    else
    {
        // 浮点数值
        lastExprValue = llvm::ConstantFP::get(llvm::Type::getDoubleTy(getContext()), value);
        lastExprType = PyType::getDouble();
    }
}

void llvmpy::PyCodeGen::visit(StringExprAST* expr)
{
    const std::string& value = expr->getValue();

    // 创建字符串常量
    llvm::Value* stringPtr = builder->CreateGlobalStringPtr(value, "str");

    // 调用运行时函数创建字符串对象
    lastExprValue = createStringObject(stringPtr);
    lastExprType = PyType::getString();

    // 标记为临时对象，需要管理生命周期
    addTempObject(lastExprValue, lastExprType->getObjectType());
}

void llvmpy::PyCodeGen::visit(BoolExprAST* expr)
{
    bool value = expr->getValue();

    // 创建布尔值
    lastExprValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(getContext()), value);
    lastExprType = PyType::getBool();
}

void llvmpy::PyCodeGen::visit(NoneExprAST* expr)
{
    // 创建None对象
    llvm::Function* getNoneFunc = getOrCreateExternalFunction(
        "py_get_none",
        llvm::PointerType::get(getContext(), 0),
        {});

    lastExprValue = builder->CreateCall(getNoneFunc, {}, "none");
    lastExprType = PyType::getAny();  // None的类型用Any表示
}

void llvmpy::PyCodeGen::visit(VariableExprAST* expr)
{
    std::string name = expr->getName();

    // 从符号表查找变量
    llvm::Value* value = getVariable(name);
    if (!value)
    {
        logError("Unknown variable name: " + name, expr->line.value_or(-1), expr->column.value_or(-1));
        return;
    }

    // 获取变量类型
    ObjectType* varType = getVariableType(name);
    if (varType)
    {
        lastExprType = std::make_shared<PyType>(varType);
    }
    else
    {
        lastExprType = PyType::getAny();  // 默认为Any类型
    }

    lastExprValue = value;
}

void llvmpy::PyCodeGen::visit(BinaryExprAST* expr)
{
    // 处理左操作数
    auto* lhs = static_cast<const ExprAST*>(expr->getLHS());
    llvm::Value* L = handleExpr(const_cast<ExprAST*>(lhs));
    if (!L) return;  // 错误已经被记录

    // 获取左操作数类型
    auto lhsTypePtr = lhs->getType();
    if (!lhsTypePtr)
    {
        logError("Cannot determine left operand type", expr->line.value_or(-1), expr->column.value_or(-1));
        return;
    }
    ObjectType* leftType = lhsTypePtr->getObjectType();

    // 获取操作符
    char op = expr->getOp();

    // 短路逻辑运算的特殊处理 ('&&' 和 '||')
    if (op == '&' || op == '|')
    {  // 使用 & 表示 &&, | 表示 ||
        // 创建短路计算需要的基本块
        llvm::Function* currentFunc = builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* rhsBlock = createBasicBlock("rhseval", currentFunc);
        llvm::BasicBlock* mergeBlock = createBasicBlock("merge", currentFunc);

        // 将左操作数转换为布尔值
        llvm::Value* lhsCond;
        if (L->getType()->isIntegerTy(1))
        {
            lhsCond = L;
        }
        else if (L->getType()->isIntegerTy() || L->getType()->isDoubleTy())
        {
            // 数值转布尔: 非零为真
            if (L->getType()->isIntegerTy())
            {
                lhsCond = builder->CreateICmpNE(L,
                                                llvm::ConstantInt::get(L->getType(), 0), "tobool");
            }
            else
            {
                lhsCond = builder->CreateFCmpONE(L,
                                                 llvm::ConstantFP::get(L->getType(), 0.0), "tobool");
            }
        }
        else if (L->getType()->isPointerTy())
        {
            // 对象转布尔: 调用运行时函数
            llvm::Function* toBoolFunc = getOrCreateExternalFunction(
                "py_object_to_bool",
                llvm::Type::getInt1Ty(getContext()),
                {llvm::PointerType::get(getContext(), 0)});
            lhsCond = builder->CreateCall(toBoolFunc, {L}, "tobool");
        }
        else
        {
            logError("Cannot convert left operand to boolean",
                     expr->line.value_or(-1), expr->column.value_or(-1));
            return;
        }

        // AND: 左值为false时无需计算右值
        // OR: 左值为true时无需计算右值
        llvm::BasicBlock* shortCircuitBlock;
        if (op == '&')
        {  // AND
            // 当左值为false时短路
            builder->CreateCondBr(lhsCond, rhsBlock, mergeBlock);
            shortCircuitBlock = mergeBlock;
        }
        else
        {  // OR
            // 当左值为true时短路
            builder->CreateCondBr(lhsCond, mergeBlock, rhsBlock);
            shortCircuitBlock = mergeBlock;
        }

        // 在短路基本块中设置结果
        builder->SetInsertPoint(shortCircuitBlock);
        llvm::PHINode* phi = builder->CreatePHI(llvm::Type::getInt1Ty(getContext()), 2, "logicresult");
        phi->addIncoming(op == '&' ? llvm::ConstantInt::getFalse(getContext()) : llvm::ConstantInt::getTrue(getContext()),
                         builder->GetInsertBlock());

        // 计算右操作数
        builder->SetInsertPoint(rhsBlock);
        auto* rhs = static_cast<const ExprAST*>(expr->getRHS());
        llvm::Value* R = handleExpr(const_cast<ExprAST*>(rhs));
        if (!R)
        {
            // 错误处理
            phi->removeIncomingValue(builder->GetInsertBlock());
            return;
        }

        // 获取右操作数类型
        auto rhsTypePtr = rhs->getType();
        if (!rhsTypePtr)
        {
            logError("Cannot determine right operand type",
                     expr->line.value_or(-1), expr->column.value_or(-1));
            phi->removeIncomingValue(builder->GetInsertBlock());
            return;
        }

        // 将右操作数转换为布尔值
        llvm::Value* rhsCond;
        if (R->getType()->isIntegerTy(1))
        {
            rhsCond = R;
        }
        else if (R->getType()->isIntegerTy() || R->getType()->isDoubleTy())
        {
            if (R->getType()->isIntegerTy())
            {
                rhsCond = builder->CreateICmpNE(R,
                                                llvm::ConstantInt::get(R->getType(), 0), "tobool");
            }
            else
            {
                rhsCond = builder->CreateFCmpONE(R,
                                                 llvm::ConstantFP::get(R->getType(), 0.0), "tobool");
            }
        }
        else if (R->getType()->isPointerTy())
        {
            llvm::Function* toBoolFunc = getOrCreateExternalFunction(
                "py_object_to_bool",
                llvm::Type::getInt1Ty(getContext()),
                {llvm::PointerType::get(getContext(), 0)});
            rhsCond = builder->CreateCall(toBoolFunc, {R}, "tobool");
        }
        else
        {
            logError("Cannot convert right operand to boolean",
                     expr->line.value_or(-1), expr->column.value_or(-1));
            phi->removeIncomingValue(builder->GetInsertBlock());
            return;
        }

        // 右操作数计算完成后跳转到合并块
        builder->CreateBr(mergeBlock);

        // 更新PHI节点
        phi->addIncoming(rhsCond, rhsBlock);

        // 设置插入点到合并块，继续生成代码
        builder->SetInsertPoint(mergeBlock);

        // 设置最终结果
        lastExprValue = phi;
        lastExprType = PyType::getBool();
        return;
    }

    // 处理数组索引操作符 '['
    if (op == '[')
    {
        auto* rhs = static_cast<const ExprAST*>(expr->getRHS());
        llvm::Value* R = handleExpr(const_cast<ExprAST*>(rhs));
        if (!R) return;

        auto rhsTypePtr = rhs->getType();
        if (!rhsTypePtr)
        {
            logError("Cannot determine index type",
                     expr->line.value_or(-1), expr->column.value_or(-1));
            return;
        }
        ObjectType* indexType = rhsTypePtr->getObjectType();

        // 验证索引操作安全性 - 防止 "Expected type 5, got 1" 错误
        if (!validateIndexOperation(const_cast<ExprAST*>(lhs), const_cast<ExprAST*>(rhs)))
        {
            return;
        }

        // 生成索引操作代码
        lastExprValue = generateIndexOperation(L, R, leftType, indexType);

        // 确定返回类型
        if (dynamic_cast<ListType*>(leftType))
        {
            // 如果是列表，返回元素类型
            auto listType = dynamic_cast<ListType*>(leftType);
            if (listType)
            {
                const ObjectType* elemType = listType->getElementType();
                lastExprType = std::make_shared<PyType>(const_cast<ObjectType*>(elemType));
            }
        }
        else if (dynamic_cast<DictType*>(leftType))
        {
            // 如果是字典，返回值类型
            auto dictType = dynamic_cast<DictType*>(leftType);
            if (dictType)
            {
                const ObjectType* valueType = dictType->getValueType();
                lastExprType = std::make_shared<PyType>(const_cast<ObjectType*>(valueType));
            }
        }
        else if (leftType->getTypeId() == PY_TYPE_STRING)
        {
            // 如果是字符串，返回字符串类型（单个字符也是字符串）
            lastExprType = PyType::getString();
        }
        else
        {
            // 其它情况使用Any类型
            lastExprType = PyType::getAny();
        }

        return;
    }

    // 正常二元操作
    auto* rhs = static_cast<const ExprAST*>(expr->getRHS());
    llvm::Value* R = handleExpr(const_cast<ExprAST*>(rhs));
    if (!R) return;

    // 获取右操作数类型
    auto rhsTypePtr = rhs->getType();
    if (!rhsTypePtr)
    {
        logError("Cannot determine right operand type",
                 expr->line.value_or(-1), expr->column.value_or(-1));
        return;
    }
    ObjectType* rightType = rhsTypePtr->getObjectType();

    // 使用 TypeOperations 系统处理二元操作
    lastExprValue = generateBinaryOperation(op, L, R, leftType, rightType);

    // 确定结果类型
    if (!lastExprValue)
    {
        // 如果操作失败，返回Any类型
        lastExprType = PyType::getAny();
        return;
    }

    // 根据操作符和操作数类型推断结果类型
    if (op == '+' || op == '-' || op == '*' || op == '/' || op == '%')
    {
        // 算术运算符
        if (leftType->getTypeId() == PY_TYPE_DOUBLE || rightType->getTypeId() == PY_TYPE_DOUBLE)
        {
            lastExprType = PyType::getDouble();
        }
        else if (leftType->getTypeId() == PY_TYPE_INT && rightType->getTypeId() == PY_TYPE_INT)
        {
            lastExprType = PyType::getInt();
        }
        else if (leftType->getTypeId() == PY_TYPE_STRING && op == '+')
        {
            // 字符串连接
            lastExprType = PyType::getString();
        }
        else
        {
            // 默认使用左操作数类型
            lastExprType = std::make_shared<PyType>(leftType);
        }
    }
    else if (op == '<' || op == '>' || op == '=' || op == '!')
    {
        // 比较运算符
        lastExprType = PyType::getBool();
    }
    else
    {
        // 其它情况使用左操作数类型
        lastExprType = std::make_shared<PyType>(leftType);
    }

    // 管理临时对象生命周期
    if (leftType->isReference() || rightType->isReference())
    {
        if (lastExprValue->getType()->isPointerTy())
        {
            // 标记为临时对象，需要在适当的时候释放
            addTempObject(lastExprValue, lastExprType->getObjectType());
        }
    }
}

void llvmpy::PyCodeGen::visit(UnaryExprAST* expr)
{
    // 获取操作数
    auto* operandExpr = static_cast<const ExprAST*>(expr->getOperand());
    llvm::Value* operand = handleExpr(const_cast<ExprAST*>(operandExpr));
    if (!operand) return;  // 错误已经被记录

    // 获取操作数类型
    auto operandTypePtr = operandExpr->getType();
    if (!operandTypePtr)
    {
        logError("Cannot determine operand type",
                 expr->line.value_or(-1), expr->column.value_or(-1));
        return;
    }
    ObjectType* operandType = operandTypePtr->getObjectType();

    // 获取操作符
    char op = expr->getOpCode();

    // 使用 TypeOperations 系统处理一元操作
    lastExprValue = generateUnaryOperation(op, operand, operandType);

    // 确定结果类型
    if (!lastExprValue)
    {
        // 如果操作失败，返回Any类型
        lastExprType = PyType::getAny();
        return;
    }

    // 根据操作符和操作数类型推断结果类型
    if (op == '-')
    {
        // 数值求反
        if (operandType->getTypeId() == PY_TYPE_DOUBLE)
        {
            lastExprType = PyType::getDouble();
        }
        else if (operandType->getTypeId() == PY_TYPE_INT)
        {
            lastExprType = PyType::getInt();
        }
        else
        {
            lastExprType = std::make_shared<PyType>(operandType);
        }
    }
    else if (op == '!')
    {
        // 逻辑求反
        lastExprType = PyType::getBool();
    }
    else
    {
        // 其它情况使用操作数类型
        lastExprType = std::make_shared<PyType>(operandType);
    }

    // 管理临时对象生命周期
    if (operandType->isReference())
    {
        if (lastExprValue->getType()->isPointerTy())
        {
            // 标记为临时对象，需要在适当的时候释放
            addTempObject(lastExprValue, lastExprType->getObjectType());
        }
    }
}

void llvmpy::PyCodeGen::visit(CallExprAST* expr)
{
    if (!expr)
    {
        logError("尝试处理空函数调用AST");
        lastExprValue = nullptr;
        lastExprType = PyType::getAny();
        return;
    }

    DEBUG_LOG("处理函数调用: " << expr->getCallee());

    // 1. 获取函数名和参数
    const std::string& callee = expr->getCallee();
    const auto& args = expr->getArgs();

    // 2. 获取函数
    llvm::Function* calleeF = getModule()->getFunction(callee);
    if (!calleeF)
    {
        logError("引用了未知函数: " + callee,
                 expr->line.value_or(-1), expr->column.value_or(-1));
        lastExprValue = nullptr;
        lastExprType = PyType::getAny();
        return;
    }

    // 3. 检查参数数量
    if (calleeF->arg_size() != args.size())
    {
        logError("传递的参数数量不正确: 预期 " + std::to_string(calleeF->arg_size()) + ", 得到 " + std::to_string(args.size()),
                 expr->line.value_or(-1), expr->column.value_or(-1));
        lastExprValue = nullptr;
        lastExprType = PyType::getAny();
        return;
    }

    // 4. 获取函数类型信息
    FunctionType* funcType = nullptr;
    ObjectType* returnType = nullptr;
    std::vector<ObjectType*> paramTypes;

    // 从TypeRegistry获取函数类型信息
    auto& typeRegistry = TypeRegistry::getInstance();
    auto functionName = calleeF->getName().str();

    // 尝试获取函数类型
    try
    {
        funcType = typeRegistry.getFunctionType(functionName);
        if (funcType)
        {
            returnType = const_cast<ObjectType*>(funcType->getReturnType());
            paramTypes = const_cast<std::vector<ObjectType*>&>(funcType->getParamTypes());

            DEBUG_LOG("找到函数类型信息: " << functionName << ", 返回类型: " << returnType->getName() << ", 参数数量: " << paramTypes.size());
        }
        else
        {
            DEBUG_LOG("未找到函数类型信息，将尝试根据LLVM类型推断");
        }
    }
    catch (const std::exception& e)
    {
        DEBUG_LOG("获取函数类型时出错: " << e.what());
    }

    // 5. 处理参数
    std::vector<llvm::Value*> argsValues;

    for (unsigned i = 0; i < args.size(); ++i)
    {
        // 获取参数表达式
        if (!args[i])
        {
            logError("函数 '" + callee + "' 的第 " + std::to_string(i + 1) + " 个参数为空",
                     expr->line.value_or(-1), expr->column.value_or(-1));
            lastExprValue = nullptr;
            lastExprType = PyType::getAny();
            return;
        }

        // 生成参数表达式代码
        llvm::Value* argValue = handleExpr(const_cast<ExprAST*>(args[i].get()));
        if (!argValue)
        {
            logError("无法为参数 " + std::to_string(i + 1) + " 生成代码",
                     args[i]->line.value_or(-1), args[i]->column.value_or(-1));
            lastExprValue = nullptr;
            lastExprType = PyType::getAny();
            return;
        }

        // 获取参数类型
        auto argTypePtr = args[i]->getType();
        if (!argTypePtr)
        {
            logError("无法确定参数 " + std::to_string(i + 1) + " 的类型",
                     args[i]->line.value_or(-1), args[i]->column.value_or(-1));
            lastExprValue = nullptr;
            lastExprType = PyType::getAny();
            return;
        }

        ObjectType* argType = argTypePtr->getObjectType();
        ObjectType* paramType = (i < paramTypes.size()) ? paramTypes[i] : nullptr;

        // 如果有参数类型信息，检查类型兼容性并转换
        if (paramType)
        {
            DEBUG_LOG("参数 " << (i + 1) << " 类型: " << argType->getName() << ", 期望类型: " << paramType->getName());

            // 类型不兼容，尝试转换
            if (!argType->canAssignTo(paramType))
            {
                DEBUG_LOG("参数 " << (i + 1) << " 类型不兼容，尝试转换");

                // 尝试类型转换
                llvm::Value* convertedValue = generateTypeConversion(argValue, argType, paramType);
                if (!convertedValue)
                {
                    logTypeError("无法将参数 " + std::to_string(i + 1) + " 从 " + argType->getName() + " 转换为 " + paramType->getName(),
                                 args[i]->line.value_or(-1), args[i]->column.value_or(-1));

                    // 继续使用原始值，让函数调用尝试执行
                    convertedValue = argValue;
                }
                argValue = convertedValue;
            }

            // 参数准备 - 处理引用计数和生命周期
            if (paramType->isReference() || paramType->getCategory() == ObjectType::Container)
            {
                // 如果参数需要作为引用传递
                if (!argValue->getType()->isPointerTy())
                {
                    // 将非指针值包装为对象指针
                    argValue = ensurePythonObject(argValue, paramType);
                }

                // 增加引用计数（如果需要）
                if (ObjectLifecycleManager::needsIncRef(paramType,
                                                        ObjectLifecycleManager::ObjectSource::PARAMETER,
                                                        ObjectLifecycleManager::ObjectDestination::PARAMETER))
                {
                    llvm::Function* incRefFunc = getOrCreateExternalFunction(
                        "py_incref", llvm::Type::getVoidTy(getContext()),
                        {llvm::PointerType::get(getContext(), 0)});

                    if (incRefFunc)
                    {
                        builder->CreateCall(incRefFunc, {argValue});
                        DEBUG_LOG("为参数 " << (i + 1) << " 增加引用计数");
                    }
                }
            }
        }
        else
        {
            // 无类型信息，使用一般规则处理参数
            DEBUG_LOG("无参数 " << (i + 1) << " 的类型信息，使用默认处理");

            // 确保参数类型与函数参数匹配
            llvm::Type* paramLLVMType = calleeF->getFunctionType()->getParamType(i);

            // 类型不匹配，尝试转换
            if (argValue->getType() != paramLLVMType)
            {
                if (paramLLVMType->isPointerTy() && !argValue->getType()->isPointerTy())
                {
                    // 将基本类型转换为对象指针
                    argValue = ensurePythonObject(argValue, argType);
                    DEBUG_LOG("将参数 " << (i + 1) << " 转换为对象指针");
                }
                else if (paramLLVMType->isIntegerTy(32) && argValue->getType()->isDoubleTy())
                {
                    // 浮点数转整数
                    argValue = builder->CreateFPToSI(argValue, builder->getInt32Ty(), "fptosiarg");
                    DEBUG_LOG("将参数 " << (i + 1) << " 从浮点数转换为整数");
                }
                else if (paramLLVMType->isDoubleTy() && argValue->getType()->isIntegerTy())
                {
                    // 整数转浮点数
                    argValue = builder->CreateSIToFP(argValue, builder->getDoubleTy(), "sitofparg");
                    DEBUG_LOG("将参数 " << (i + 1) << " 从整数转换为浮点数");
                }
                // 其他转换...
            }
        }

        // 添加到参数列表
        argsValues.push_back(argValue);
    }

    // 6. 创建函数调用
    llvm::Value* callResult = builder->CreateCall(calleeF, argsValues, "calltmp");
    DEBUG_LOG("创建函数调用: " << callee);

    // 7. 处理返回值和类型元数据
    if (returnType)
    {
        // 根据已知函数类型设置返回值类型
        lastExprType = std::make_shared<PyType>(returnType);

        // 为对象类型的返回值添加类型元数据
        if (returnType->isReference() || returnType->getCategory() == ObjectType::Container || returnType->getTypeId() >= PY_TYPE_LIST)
        {
            if (callResult->getType()->isPointerTy())
            {
                // 附加类型元数据到返回值
                ObjectLifecycleManager::attachTypeMetadata(callResult, returnType->getTypeId());
                DEBUG_LOG("为返回值附加类型元数据: " << returnType->getTypeId());
            }
        }
    }
    else
    {
        // 根据LLVM返回类型推断类型
        llvm::Type* returnTy = calleeF->getReturnType();

        if (returnTy->isVoidTy())
        {
            lastExprType = PyType::getVoid();
        }
        else if (returnTy->isIntegerTy(32))
        {
            lastExprType = PyType::getInt();
        }
        else if (returnTy->isDoubleTy())
        {
            lastExprType = PyType::getDouble();
        }
        else if (returnTy->isIntegerTy(1))
        {
            lastExprType = PyType::getBool();
        }
        else if (returnTy->isPointerTy())
        {
            // 尝试从返回值获取类型元数据
            if (callResult)
            {
                int typeId = ObjectLifecycleManager::getTypeIdFromMetadata(callResult);
                if (typeId > 0)
                {
                    ObjectType* derivedType = TypeRegistry::getInstance().getTypeById(typeId);
                    if (derivedType)
                    {
                        lastExprType = std::make_shared<PyType>(derivedType);
                        DEBUG_LOG("从元数据推断返回类型: " << derivedType->getName());
                    }
                    else
                    {
                        lastExprType = PyType::getAny();
                        DEBUG_LOG("从元数据获取到typeId但无法解析: " << typeId);
                    }
                }
                else
                {
                    lastExprType = PyType::getAny();
                    DEBUG_LOG("返回值没有类型元数据，使用Any类型");
                }
            }
            else
            {
                lastExprType = PyType::getAny();
            }
        }
        else
        {
            lastExprType = PyType::getAny();
            DEBUG_LOG("未知返回类型，使用Any类型");
        }
    }

    // 8. 处理引用类型返回值的生命周期
    if (lastExprType->getObjectType()->isReference() || lastExprType->getObjectType()->getCategory() == ObjectType::Container || lastExprType->getObjectType()->getTypeId() >= PY_TYPE_LIST)
    {
        if (callResult->getType()->isPointerTy())
        {
            // 跟踪对象供后续清理
            addTempObject(callResult, lastExprType->getObjectType());
            DEBUG_LOG("将返回的对象标记为临时对象以便后续清理");
        }
    }

    lastExprValue = callResult;
    DEBUG_LOG("函数调用完成: " << callee << ", 返回类型: " << lastExprType->getObjectType()->getName());
}

void llvmpy::PyCodeGen::visit(ListExprAST* expr)
{
    const auto& elements = expr->getElements();

    // 确定列表元素类型
    ObjectType* elemType = nullptr;
    if (!elements.empty())
    {
        // 根据第一个元素推断类型
        auto firstElemTypePtr = elements[0]->getType();
        if (firstElemTypePtr)
        {
            elemType = firstElemTypePtr->getObjectType();
        }

        // 检查所有元素类型是否兼容
        for (size_t i = 1; i < elements.size(); ++i)
        {
            auto elemTypePtr = elements[i]->getType();
            if (elemTypePtr)
            {
                ObjectType* thisElemType = elemTypePtr->getObjectType();
                if (!thisElemType->canAssignTo(elemType))
                {
                    // 尝试找到更通用的类型
                    if (elemType->canAssignTo(thisElemType))
                    {
                        elemType = thisElemType;
                    }
                    else if (elemType->getTypeId() == PY_TYPE_INT && thisElemType->getTypeId() == PY_TYPE_DOUBLE)
                    {
                        // Int和Double情况下使用Double
                        elemType = thisElemType;
                    }
                    else
                    {
                        // 如果找不到兼容类型，使用Any
                        elemType = TypeRegistry::getInstance().getType("any");
                        break;
                    }
                }
            }
        }
    }

    // 如果无法确定元素类型，默认为Any
    if (!elemType)
    {
        elemType = TypeRegistry::getInstance().getType("any");
    }

    // 创建列表大小常量
    llvm::Value* size = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(getContext()), elements.size());

    // 创建列表对象
    llvm::Value* listObj = runtime->createList(size, elemType);

    // 提前获取并缓存常用函数，避免重复创建带后缀的函数声明
    llvm::Function* createIntFunc = nullptr;
    llvm::Function* createDoubleFunc = nullptr;
    llvm::Function* createBoolFunc = nullptr;
    llvm::Function* setItemFunc = getOrCreateExternalFunction(
        "py_list_set_item",
        llvm::Type::getVoidTy(getContext()),
        {
            llvm::PointerType::get(getContext(), 0),  // list
            llvm::Type::getInt32Ty(getContext()),     // index
            llvm::PointerType::get(getContext(), 0)   // value
        });

    // 根据元素类型预获取创建函数
    if (elemType->getTypeId() == PY_TYPE_INT)
    {
        createIntFunc = getOrCreateExternalFunction(
            "py_create_int",
            llvm::PointerType::get(getContext(), 0),
            {llvm::Type::getInt32Ty(getContext())});
    }
    else if (elemType->getTypeId() == PY_TYPE_DOUBLE)
    {
        createDoubleFunc = getOrCreateExternalFunction(
            "py_create_double",
            llvm::PointerType::get(getContext(), 0),
            {llvm::Type::getDoubleTy(getContext())});
    }
    else if (elemType->getTypeId() == PY_TYPE_BOOL)
    {
        createBoolFunc = getOrCreateExternalFunction(
            "py_create_bool",
            llvm::PointerType::get(getContext(), 0),
            {llvm::Type::getInt1Ty(getContext())});
    }

    // 处理列表元素
    for (size_t i = 0; i < elements.size(); ++i)
    {
        // 生成元素值
        llvm::Value* elemValue = handleExpr(const_cast<ExprAST*>(elements[i].get()));
        if (!elemValue) return;  // 元素处理出错

        // 获取元素类型
        auto elemTypePtr = elements[i]->getType();
        if (!elemTypePtr)
        {
            logError("Cannot determine type of list element " + std::to_string(i),
                     elements[i]->line.value_or(-1), elements[i]->column.value_or(-1));
            return;
        }
        ObjectType* thisElemType = elemTypePtr->getObjectType();

        // 如果需要，进行类型转换
        if (thisElemType->getTypeId() != elemType->getTypeId())
        {
            elemValue = generateTypeConversion(elemValue, thisElemType, elemType);
            if (!elemValue)
            {
                logTypeError("Cannot convert list element " + std::to_string(i) + " from " + thisElemType->getName() + " to " + elemType->getName(),
                             elements[i]->line.value_or(-1), elements[i]->column.value_or(-1));
                return;
            }
        }

        // 确保元素值是指针类型
        if (!elemValue->getType()->isPointerTy())
        {
            // 根据类型使用预获取的函数，避免创建新函数
            if (elemType->getTypeId() == PY_TYPE_INT)
            {
                if (!createIntFunc)
                {
                    createIntFunc = getOrCreateExternalFunction(
                        "py_create_int",
                        llvm::PointerType::get(getContext(), 0),
                        {llvm::Type::getInt32Ty(getContext())});
                }
                elemValue = builder->CreateCall(createIntFunc, {elemValue}, "int_obj");
            }
            else if (elemType->getTypeId() == PY_TYPE_DOUBLE)
            {
                if (!createDoubleFunc)
                {
                    createDoubleFunc = getOrCreateExternalFunction(
                        "py_create_double",
                        llvm::PointerType::get(getContext(), 0),
                        {llvm::Type::getDoubleTy(getContext())});
                }
                elemValue = builder->CreateCall(createDoubleFunc, {elemValue}, "double_obj");
            }
            else if (elemType->getTypeId() == PY_TYPE_BOOL)
            {
                if (!createBoolFunc)
                {
                    createBoolFunc = getOrCreateExternalFunction(
                        "py_create_bool",
                        llvm::PointerType::get(getContext(), 0),
                        {llvm::Type::getInt1Ty(getContext())});
                }
                elemValue = builder->CreateCall(createBoolFunc, {elemValue}, "bool_obj");
            }
            else
            {
                // 其他类型使用通用方法
                elemValue = ObjectLifecycleManager::createObject(*this, elemValue, elemType->getTypeId());
            }
        }

        // 创建索引常量
        llvm::Value* index = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(getContext()), i);

        // 调用设置列表元素函数
        builder->CreateCall(setItemFunc, {listObj, index, elemValue});
    }

    // 设置结果
    lastExprValue = listObj;

    // 创建列表类型对象
    ListType* listType = TypeRegistry::getInstance().getListType(elemType);
    lastExprType = std::make_shared<PyType>(listType);

    // 标记为需要管理的对象
    addTempObject(listObj, listType);
}
void llvmpy::PyCodeGen::visit(IndexExprAST* expr)
{
    // 获取目标和索引表达式
    auto* target = static_cast<const ExprAST*>(expr->getTarget());
    auto* index = static_cast<const ExprAST*>(expr->getIndex());

    // 生成目标和索引的代码
    llvm::Value* targetValue = handleExpr(const_cast<ExprAST*>(target));
    if (!targetValue) return;  // 错误已经被记录

    llvm::Value* indexValue = handleExpr(const_cast<ExprAST*>(index));
    if (!indexValue) return;  // 错误已经被记录

    // 获取目标和索引的类型
    auto targetTypePtr = target->getType();
    auto indexTypePtr = index->getType();

    if (!targetTypePtr)
    {
        logError("Cannot determine target type for indexing",
                 expr->line.value_or(-1), expr->column.value_or(-1));
        return;
    }

    if (!indexTypePtr)
    {
        logError("Cannot determine index type",
                 expr->line.value_or(-1), expr->column.value_or(-1));
        return;
    }

    ObjectType* targetType = targetTypePtr->getObjectType();
    ObjectType* indexType = indexTypePtr->getObjectType();

    // 调试输出
    std::cerr << "DEBUG: IndexExpr - Target type: " << targetType->getName()
              << ", TypeID: " << targetType->getTypeId() << std::endl;

    // 检查符号表中是否有更准确的类型信息
    bool hasMoreAccurateType = false;
    std::string varName;

    if (const VariableExprAST* varExpr = dynamic_cast<const VariableExprAST*>(target))
    {
        varName = varExpr->getName();
        ObjectType* symbolType = getVariableType(varName);

        // 如果在符号表中找到了类型，并且是容器或序列类型，则使用这个更准确的类型
        if (symbolType && (symbolType->getTypeId() == PY_TYPE_LIST || TypeFeatureChecker::hasFeature(symbolType, "container") || TypeFeatureChecker::hasFeature(symbolType, "sequence")))
        {
            std::cerr << "DEBUG: Using symbol table type for " << varName
                      << ": " << symbolType->getName()
                      << ", TypeID: " << symbolType->getTypeId() << std::endl;

            targetType = symbolType;  // 使用符号表中的类型替换
            hasMoreAccurateType = true;
        }
    }

    // 检查目标值是否为变量地址 (AllocaInst)，如果是则需要加载实际指针
    if (auto allocaInst = llvm::dyn_cast<llvm::AllocaInst>(targetValue))
    {
        auto allocatedType = allocaInst->getAllocatedType();
        std::string allocTypeStr;
        llvm::raw_string_ostream allocRso(allocTypeStr);
        allocatedType->print(allocRso);

        std::cerr << "DEBUG: Target is AllocaInst, allocated type: " << allocTypeStr << std::endl;

        // 如果分配的类型是指针，需要加载其中的值
        if (allocatedType->isPointerTy())
        {
            std::cerr << "DEBUG: Loading actual list pointer from variable "
                      << (varName.empty() ? "target" : varName) << std::endl;
            targetValue = builder->CreateLoad(allocatedType, targetValue, "loaded_target_ptr");
        }
    }

    // 显示当前操作的对象信息
    std::cerr << "DEBUG: Index operation - Target type: " << targetType->getName()
              << ", TypeID: " << targetType->getTypeId()
              << ", Is container: " << TypeFeatureChecker::hasFeature(targetType, "container")
              << ", Is sequence: " << TypeFeatureChecker::hasFeature(targetType, "sequence") << std::endl;

    // 验证索引操作
    if (!validateIndexOperation(const_cast<ExprAST*>(target), const_cast<ExprAST*>(index)))
    {
        return;
    }

    // 如果在符号表找到了更准确的类型，通知用户
    if (hasMoreAccurateType)
    {
        std::cerr << "DEBUG: Found better type for "
                  << (!varName.empty() ? varName : "target")
                  << " in symbol table" << std::endl;
    }

    // 判断目标类型
    bool isList = (targetType->getTypeId() == PY_TYPE_LIST) || dynamic_cast<ListType*>(targetType);
    bool isDict = (targetType->getTypeId() == PY_TYPE_DICT) || dynamic_cast<DictType*>(targetType);
    bool isString = (targetType->getTypeId() == PY_TYPE_STRING) || (targetType->getName() == "string");

    // 如果符号表表明这是一个列表，强制标记为列表
    if (hasMoreAccurateType && targetType->getTypeId() == PY_TYPE_LIST)
    {
        isList = true;
        isDict = false;
        isString = false;
    }

    // 根据目标类型执行不同的索引操作
    if (isList)
    {
        std::cerr << "DEBUG: Generating list index operation" << std::endl;

        // 检查索引类型
        if (indexType->getTypeId() != PY_TYPE_INT)
        {
            logTypeError("List indices must be integers",
                         index->line.value_or(-1), index->column.value_or(-1));
            return;
        }

        // 使用列表索引操作函数
        llvm::Function* getItemFunc = getOrCreateExternalFunction(
            "py_list_get_item",
            llvm::PointerType::get(getContext(), 0),
            {
                llvm::PointerType::get(getContext(), 0),  // list
                llvm::Type::getInt32Ty(getContext())      // index
            });

        // 确保索引是整数值
        llvm::Value* indexIntValue = indexValue;
        if (indexValue->getType()->isPointerTy())
        {
            // 如果是对象指针，需要提取整数值
            llvm::Function* extractIntFunc = getOrCreateExternalFunction(
                "py_extract_int",
                llvm::Type::getInt32Ty(getContext()),
                {llvm::PointerType::get(getContext(), 0)});

            indexIntValue = builder->CreateCall(extractIntFunc, {indexValue}, "idxtmp");
        }
        else if (!indexValue->getType()->isIntegerTy(32))
        {
            // 如果不是i32类型的整数，进行类型转换
            indexIntValue = builder->CreateIntCast(
                indexValue, llvm::Type::getInt32Ty(getContext()), true, "idxtmp");
        }

        // 调用列表索引函数
        lastExprValue = builder->CreateCall(getItemFunc, {targetValue, indexIntValue}, "listitem");

        // 处理列表元素类型
        auto listType = dynamic_cast<ListType*>(targetType);
        if (listType)
        {
            const ObjectType* elemType = listType->getElementType();
            lastExprType = std::make_shared<PyType>(const_cast<ObjectType*>(elemType));

            // 如果元素是基本类型，从返回的对象中提取值
            int elemTypeId = elemType->getTypeId();
            if (elemTypeId == PY_TYPE_INT)
            {
                llvm::Function* extractFunc = getOrCreateExternalFunction(
                    "py_extract_int",
                    llvm::Type::getInt32Ty(getContext()),
                    {llvm::PointerType::get(getContext(), 0)});

                // 创建调用指令 - 从对象中提取基本类型值
                lastExprValue = builder->CreateCall(extractFunc, {lastExprValue}, "loadint");
            }
            else if (elemTypeId == PY_TYPE_DOUBLE)
            {
                llvm::Function* extractFunc = getOrCreateExternalFunction(
                    "py_extract_double",
                    llvm::Type::getDoubleTy(getContext()),
                    {llvm::PointerType::get(getContext(), 0)});

                lastExprValue = builder->CreateCall(extractFunc, {lastExprValue}, "loaddouble");
            }
            else if (elemTypeId == PY_TYPE_BOOL)
            {
                llvm::Function* extractFunc = getOrCreateExternalFunction(
                    "py_extract_bool",
                    llvm::Type::getInt1Ty(getContext()),
                    {llvm::PointerType::get(getContext(), 0)});

                lastExprValue = builder->CreateCall(extractFunc, {lastExprValue}, "loadbool");
            }
        }
        else
        {
            // 如果没有具体的列表类型信息，使用Any类型
            lastExprType = PyType::getAny();
        }
    }
    else if (isDict)
    {
        std::cerr << "DEBUG: Generating dictionary index operation" << std::endl;
        // 字典索引操作
        llvm::Function* getDictItemFunc = getOrCreateExternalFunction(
            "py_dict_get_item",
            llvm::PointerType::get(getContext(), 0),
            {
                llvm::PointerType::get(getContext(), 0),  // dict
                llvm::PointerType::get(getContext(), 0)   // key
            });

        // 确保键是对象指针类型
        llvm::Value* keyValue = indexValue;
        if (!indexValue->getType()->isPointerTy())
        {
            // 如果不是对象指针，创建对象
            keyValue = ObjectLifecycleManager::createObject(*this, indexValue, indexType->getTypeId());
        }

        lastExprValue = builder->CreateCall(getDictItemFunc, {targetValue, keyValue}, "dictitem");

        // 确定字典值类型
        auto dictType = dynamic_cast<DictType*>(targetType);
        if (dictType)
        {
            const ObjectType* valueType = dictType->getValueType();
            lastExprType = std::make_shared<PyType>(const_cast<ObjectType*>(valueType));
        }
        else
        {
            lastExprType = PyType::getAny();
        }
    }
    else if (isString)
    {
        std::cerr << "DEBUG: Generating string index operation" << std::endl;
        // 字符串索引操作
        if (indexType->getTypeId() != PY_TYPE_INT)
        {
            logTypeError("String indices must be integers",
                         index->line.value_or(-1), index->column.value_or(-1));
            return;
        }

        llvm::Function* getCharFunc = getOrCreateExternalFunction(
            "py_string_get_char",
            llvm::PointerType::get(getContext(), 0),
            {
                llvm::PointerType::get(getContext(), 0),  // string
                llvm::Type::getInt32Ty(getContext())      // index
            });

        // 确保索引是整数值
        llvm::Value* indexIntValue = indexValue;
        if (indexValue->getType()->isPointerTy())
        {
            llvm::Function* extractIntFunc = getOrCreateExternalFunction(
                "py_extract_int",
                llvm::Type::getInt32Ty(getContext()),
                {llvm::PointerType::get(getContext(), 0)});
            indexIntValue = builder->CreateCall(extractIntFunc, {indexValue}, "idxtmp");
        }
        else if (!indexValue->getType()->isIntegerTy(32))
        {
            indexIntValue = builder->CreateIntCast(
                indexValue, llvm::Type::getInt32Ty(getContext()), true, "idxtmp");
        }

        lastExprValue = builder->CreateCall(getCharFunc, {targetValue, indexIntValue}, "charitem");
        lastExprType = PyType::getString();
    }
    else
    {
        // 不支持的索引操作类型
        logError("Unsupported indexing operation for type '" + targetType->getName() + "'",
                 expr->line.value_or(-1), expr->column.value_or(-1));
        return;
    }

    // 管理临时对象生命周期
    if (lastExprType->getObjectType()->isReference())
    {
        if (lastExprValue->getType()->isPointerTy())
        {
            addTempObject(lastExprValue, lastExprType->getObjectType());
        }
    }
}

void llvmpy::PyCodeGen::visit(ExprStmtAST* stmt)
{
    // 获取表达式
    auto* expr = static_cast<const ExprAST*>(stmt->getExpr());
    if (!expr) return;

    // 生成表达式代码
    llvm::Value* exprValue = handleExpr(const_cast<ExprAST*>(expr));

    // 表达式语句不需要保留结果值
    lastExprValue = nullptr;
    lastExprType = nullptr;

    // 如果表达式结果是对象引用，可能需要释放
    if (exprValue && expr->getType() && expr->getType()->getObjectType()->isReference())
    {
        // 当前作用域的临时对象会在适当的时候释放
    }
}

void llvmpy::PyCodeGen::visit(ReturnStmtAST* stmt)
{
    // 空指针检查
    if (!stmt)
    {
        logError("尝试处理空的return语句");
        return;
    }

    DEBUG_LOG("处理return语句");

    // 标记我们正在处理return语句
    inReturnStmt = true;

    // 获取返回值表达式
    auto* expr = static_cast<const ExprAST*>(stmt->getValue());

    // 确保我们在函数内部
    if (!currentFunction)
    {
        logError("函数外部的return语句",
                 stmt->line.value_or(-1), stmt->column.value_or(-1));
        inReturnStmt = false;
        return;
    }

    // 获取函数的返回类型
    if (!currentReturnType)
    {
        logError("无法确定函数返回类型",
                 stmt->line.value_or(-1), stmt->column.value_or(-1));
        inReturnStmt = false;
        return;
    }

    DEBUG_LOG("函数返回类型: " << currentReturnType->getName() << ", 类型ID: " << currentReturnType->getTypeId());

    // 处理 void/None 返回
    if (currentReturnType->getTypeId() == PY_TYPE_NONE)
    {
        if (expr)
        {
            logError("void返回类型的函数不能返回值",
                     stmt->line.value_or(-1), stmt->column.value_or(-1));
        }
        builder->CreateRetVoid();
        DEBUG_LOG("创建void返回");
        inReturnStmt = false;
        return;
    }

    // 确保有返回值表达式
    if (!expr)
    {
        // 处理非void函数的无返回值情况 - 创建默认返回值
        llvm::Value* defaultValue = createDefaultValue(currentReturnType);
        if (defaultValue)
        {
            DEBUG_LOG("为缺少的返回表达式创建默认值: " << currentReturnType->getName());

            // 确保返回值类型匹配函数声明的返回类型
            llvm::Type* returnLLVMType = currentFunction->getReturnType();
            if (defaultValue->getType() != returnLLVMType)
            {
                // 需要加载或转换值
                if (defaultValue->getType()->isPointerTy() && returnLLVMType->isIntegerTy(32))
                {
                    defaultValue = builder->CreateLoad(builder->getInt32Ty(), defaultValue, "load_ret_val");
                    DEBUG_LOG("从指针加载整数返回值");
                }
                else if (defaultValue->getType()->isPointerTy() && returnLLVMType->isDoubleTy())
                {
                    defaultValue = builder->CreateLoad(builder->getDoubleTy(), defaultValue, "load_ret_val");
                    DEBUG_LOG("从指针加载浮点返回值");
                }
                else if (defaultValue->getType()->isPointerTy() && returnLLVMType->isIntegerTy(1))
                {
                    defaultValue = builder->CreateLoad(builder->getInt1Ty(), defaultValue, "load_ret_val");
                    DEBUG_LOG("从指针加载布尔返回值");
                }
                // 其他类型转换...
            }

            // 为返回值附加类型元数据
            ObjectLifecycleManager::attachTypeMetadata(defaultValue, currentReturnType->getTypeId());
            builder->CreateRet(defaultValue);
        }
        else
        {
            logError("非void函数需要return表达式",
                     stmt->line.value_or(-1), stmt->column.value_or(-1));
            // 防止编译器崩溃，仍然创建一个返回指令
            builder->CreateRetVoid();
            DEBUG_LOG("创建应急void返回以避免崩溃");
        }
        inReturnStmt = false;
        return;
    }

    // 检查是否是直接返回参数的情况（如 return a）
    bool isParameterReturn = false;
    std::string paramName;

    if (auto varExpr = dynamic_cast<const VariableExprAST*>(expr))
    {
        paramName = varExpr->getName();

        // 检查是否为函数参数
        for (auto& arg : currentFunction->args())
        {
            if (arg.getName() == paramName)
            {
                isParameterReturn = true;  // 添加这行，设置标志
                DEBUG_LOG("检测到直接返回参数: " << paramName);
                break;
            }
        }
    }

    // 获取表达式类型
    auto exprTypePtr = expr->getType();
    if (!exprTypePtr)
    {
        DEBUG_LOG("无法确定表达式类型，使用Any类型");
        exprTypePtr = PyType::getAny();
    }

    // 获取具体对象类型
    ObjectType* exprType = exprTypePtr->getObjectType();

    DEBUG_LOG("返回表达式类型: " << exprType->getName());

    // 生成返回值代码
    llvm::Value* retValue = handleExpr(const_cast<ExprAST*>(expr));
    if (!retValue)
    {
        DEBUG_LOG("返回表达式生成失败，使用默认返回值");
        // 表达式生成失败，使用默认值来避免崩溃
        llvm::Value* defaultValue = createDefaultValue(currentReturnType);
        if (defaultValue)
        {
            builder->CreateRet(defaultValue);
        }
        else
        {
            builder->CreateRetVoid();
        }
        inReturnStmt = false;
        return;
    }

    // 如果是直接返回参数，并且返回类型是Any或者返回类型与参数类型不匹配
    if (isParameterReturn && (currentReturnType->getTypeId() == PY_TYPE_ANY || exprType->getTypeId() != currentReturnType->getTypeId()))
    {
        DEBUG_LOG("参数直接返回场景: 保留原始类型");

        // 获取或创建类型保留函数
        llvm::Function* preserveTypeFunc = getOrCreateExternalFunction(
            "py_convert_any_preserve_type",
            llvm::PointerType::get(getContext(), 0),
            {llvm::PointerType::get(getContext(), 0)});

        // 调用类型保留函数
        retValue = builder->CreateCall(preserveTypeFunc, {retValue}, "preserve_type_result");

        // 检查是否是容器类型
        bool isContainer = TypeFeatureChecker::isContainer(exprType) || exprType->getTypeId() == PY_TYPE_LIST || exprType->getTypeId() == PY_TYPE_DICT;

        // 为容器类型设置适当的类型元数据
        if (isContainer)
        {
            ObjectLifecycleManager::attachTypeMetadata(retValue, exprType->getTypeId());
            DEBUG_LOG("为容器类型返回值附加类型元数据: " << exprType->getTypeId());
        }
    }
    // 对于非参数直接返回的情况，或者类型明确匹配的情况
    else if (!isParameterReturn && !exprType->canAssignTo(currentReturnType))
    {
        DEBUG_LOG("需要进行类型转换: " << exprType->getName() << " -> " << currentReturnType->getName());

        // 使用类型操作系统执行智能转换
        llvm::Value* convertedValue = generateTypeConversion(retValue, exprType, currentReturnType);

        // 仅当转换成功时更新返回值
        if (convertedValue)
        {
            retValue = convertedValue;
            DEBUG_LOG("类型转换成功");
        }
    }

    // 准备返回值（处理生命周期）
    llvm::Value* preparedValue = prepareReturnValue(retValue, currentReturnType, const_cast<ExprAST*>(expr));
    if (!preparedValue)
    {
        DEBUG_LOG("准备返回值失败，使用原始值");
        preparedValue = retValue;
    }

    // 附加类型元数据，确保在运行时能识别正确类型
    if (currentReturnType->isReference() || TypeFeatureChecker::isContainer(currentReturnType) || currentReturnType->getTypeId() >= PY_TYPE_LIST)
    {
        if (preparedValue->getType()->isPointerTy())
        {
            // 对于容器类型，优先使用表达式的类型ID
            int typeId = isParameterReturn ? exprType->getTypeId() : currentReturnType->getTypeId();
            ObjectLifecycleManager::attachTypeMetadata(preparedValue, typeId);
            DEBUG_LOG("为返回值附加类型元数据: " << typeId);
        }
    }

    // 确保返回值类型匹配函数的返回类型
    llvm::Type* functionReturnType = currentFunction->getReturnType();
    if (preparedValue->getType() != functionReturnType)
    {
        DEBUG_LOG("返回值类型不匹配，需要进行调整");
        DEBUG_LOG("预期类型: " << functionReturnType->getTypeID() << ", 实际类型: " << preparedValue->getType()->getTypeID());

        // 处理基本类型
        if (functionReturnType->isIntegerTy(32) && preparedValue->getType()->isPointerTy())
        {
            // 从指针提取整数
            if (currentReturnType->getTypeId() == PY_TYPE_INT)
            {
                preparedValue = builder->CreateLoad(builder->getInt32Ty(), preparedValue, "load_int_ret");
                DEBUG_LOG("从指针加载整数返回值");
            }
            else
            {
                // 调用辅助函数获取整数，保留原始类型的整数提取
                llvm::Function* extractIntFunc = getOrCreateExternalFunction(
                    isParameterReturn ? "py_extract_preserve_int" : "py_extract_int",
                    builder->getInt32Ty(),
                    {llvm::PointerType::get(getContext(), 0)});

                preparedValue = builder->CreateCall(extractIntFunc, {preparedValue}, "extract_int_ret");
                DEBUG_LOG("从对象提取整数返回值");
            }
        }
        else if (functionReturnType->isDoubleTy() && preparedValue->getType()->isPointerTy())
        {
            // 从指针提取浮点数
            if (currentReturnType->getTypeId() == PY_TYPE_DOUBLE)
            {
                preparedValue = builder->CreateLoad(builder->getDoubleTy(), preparedValue, "load_double_ret");
                DEBUG_LOG("从指针加载浮点返回值");
            }
            else
            {
                // 调用辅助函数获取浮点数
                llvm::Function* extractDoubleFunc = getOrCreateExternalFunction(
                    "py_extract_double",
                    builder->getDoubleTy(),
                    {llvm::PointerType::get(getContext(), 0)});

                preparedValue = builder->CreateCall(extractDoubleFunc, {preparedValue}, "extract_double_ret");
                DEBUG_LOG("从对象提取浮点返回值");
            }
        }
        else if (functionReturnType->isIntegerTy(1) && preparedValue->getType()->isPointerTy())
        {
            // 从指针提取布尔值
            if (currentReturnType->getTypeId() == PY_TYPE_BOOL)
            {
                preparedValue = builder->CreateLoad(builder->getInt1Ty(), preparedValue, "load_bool_ret");
                DEBUG_LOG("从指针加载布尔返回值");
            }
            else
            {
                // 调用辅助函数获取布尔值
                llvm::Function* extractBoolFunc = getOrCreateExternalFunction(
                    "py_extract_bool",
                    builder->getInt1Ty(),
                    {llvm::PointerType::get(getContext(), 0)});

                preparedValue = builder->CreateCall(extractBoolFunc, {preparedValue}, "extract_bool_ret");
                DEBUG_LOG("从对象提取布尔返回值");
            }
        }
        else if (functionReturnType->isPointerTy() && !preparedValue->getType()->isPointerTy())
        {
            // 基本类型需要包装为指针类型
            llvm::Value* wrappedValue = nullptr;

            if (preparedValue->getType()->isIntegerTy(32))
            {
                wrappedValue = builder->CreateCall(
                    getOrCreateExternalFunction("py_create_int",
                                                llvm::PointerType::get(getContext(), 0),
                                                {builder->getInt32Ty()}),
                    {preparedValue}, "wrap_int");
            }
            else if (preparedValue->getType()->isDoubleTy())
            {
                wrappedValue = builder->CreateCall(
                    getOrCreateExternalFunction("py_create_double",
                                                llvm::PointerType::get(getContext(), 0),
                                                {builder->getDoubleTy()}),
                    {preparedValue}, "wrap_double");
            }
            else if (preparedValue->getType()->isIntegerTy(1))
            {
                wrappedValue = builder->CreateCall(
                    getOrCreateExternalFunction("py_create_bool",
                                                llvm::PointerType::get(getContext(), 0),
                                                {builder->getInt1Ty()}),
                    {preparedValue}, "wrap_bool");
            }

            if (wrappedValue)
            {
                preparedValue = wrappedValue;
                DEBUG_LOG("将基本类型包装为指针类型");
            }
        }
    }

    // 创建返回指令
    builder->CreateRet(preparedValue);
    DEBUG_LOG("创建return指令，返回类型: " << (isParameterReturn ? exprType->getName() : currentReturnType->getName()));

    // 释放临时对象
    releaseTempObjects();
    DEBUG_LOG("清理临时对象");

    // 重置标记
    inReturnStmt = false;
}

void llvmpy::PyCodeGen::visit(IfStmtAST* stmt)
{
    auto* condExpr = static_cast<const ExprAST*>(stmt->getCondition());
    if (!condExpr)
    {
        logError("If statement has no condition",
                 stmt->line.value_or(-1), stmt->column.value_or(-1));
        return;
    }

    // 生成条件表达式
    llvm::Value* condValue = handleExpr(const_cast<ExprAST*>(condExpr));
    if (!condValue) return;  // 错误已经被记录

    // 确保条件是布尔值
    if (!condValue->getType()->isIntegerTy(1))
    {
        condValue = PyCodeGenHelper::convertToBool(*this, condValue);
        if (!condValue) return;  // 错误已经被记录
    }

    // 创建基本块
    llvm::Function* function = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* thenBlock = createBasicBlock("then", function);
    llvm::BasicBlock* elseBlock = createBasicBlock("else", function);
    llvm::BasicBlock* mergeBlock = createBasicBlock("ifcont", function);

    // 创建条件分支
    builder->CreateCondBr(condValue, thenBlock, elseBlock);

    // 生成 then 块代码
    builder->SetInsertPoint(thenBlock);

    // 为 then 块创建作用域
    pushScope();

    // 处理 then 块的语句
    for (const auto& stmt : stmt->getThenBody())
    {
        handleStmt(const_cast<StmtAST*>(stmt.get()));
    }

    // 确保基本块有终结符
    if (!builder->GetInsertBlock()->getTerminator())
    {
        builder->CreateBr(mergeBlock);
    }

    // 结束 then 块作用域
    popScope();

    // 生成 else 块代码
    builder->SetInsertPoint(elseBlock);

    // 为 else 块创建作用域
    pushScope();

    // 处理 else 块的语句
    for (const auto& stmt : stmt->getElseBody())
    {
        handleStmt(const_cast<StmtAST*>(stmt.get()));
    }

    // 确保基本块有终结符
    if (!builder->GetInsertBlock()->getTerminator())
    {
        builder->CreateBr(mergeBlock);
    }

    // 结束 else 块作用域
    popScope();

    // 设置插入点到合并块
    builder->SetInsertPoint(mergeBlock);
}
// 添加到PyCodeGen类的适当位置
void llvmpy::PyCodeGen::debugFunctionReuse(const std::string& name, llvm::Function* func)
{
    static std::unordered_map<std::string, llvm::Function*> seenFunctions;

    auto it = seenFunctions.find(name);
    if (it != seenFunctions.end())
    {
        if (it->second != func)
        {
            std::cerr << "WARNING: Function " << name << " has multiple instances!" << std::endl;
        }
    }
    else
    {
        seenFunctions[name] = func;
        std::cerr << "DEBUG: First use of function " << name << std::endl;
    }
}
void llvmpy::PyCodeGen::visit(WhileStmtAST* stmt)
{
    auto* condExpr = static_cast<const ExprAST*>(stmt->getCondition());
    if (!condExpr)
    {
        logError("While statement has no condition",
                 stmt->line.value_or(-1), stmt->column.value_or(-1));
        return;
    }

    // 创建基本块
    llvm::Function* function = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* condBlock = createBasicBlock("whilecond", function);
    llvm::BasicBlock* loopBlock = createBasicBlock("whileloop", function);
    llvm::BasicBlock* afterBlock = createBasicBlock("whileend", function);

    // 跳转到条件块
    builder->CreateBr(condBlock);

    // 记录循环块，用于break/continue语句
    pushLoopBlocks(condBlock, afterBlock);

    // 生成条件判断代码
    builder->SetInsertPoint(condBlock);

    // 生成条件表达式
    llvm::Value* condValue = handleExpr(const_cast<ExprAST*>(condExpr));
    if (!condValue)
    {
        popLoopBlocks();
        return;  // 错误已经被记录
    }

    // 确保条件是布尔值
    if (!condValue->getType()->isIntegerTy(1))
    {
        condValue = PyCodeGenHelper::convertToBool(*this, condValue);
        if (!condValue)
        {
            popLoopBlocks();
            return;  // 错误已经被记录
        }
    }

    // 创建条件分支
    builder->CreateCondBr(condValue, loopBlock, afterBlock);

    // 生成循环体代码
    builder->SetInsertPoint(loopBlock);

    // 为循环体创建作用域
    pushScope();

    // 处理循环体语句
    for (const auto& bodyStmt : stmt->getBody())
    {
        handleStmt(const_cast<StmtAST*>(bodyStmt.get()));
    }

    // 结束循环体作用域
    popScope();

    // 如果当前块没有终结符，创建到条件块的分支
    if (!builder->GetInsertBlock()->getTerminator())
    {
        builder->CreateBr(condBlock);
    }

    // 移除循环块记录
    popLoopBlocks();

    // 设置插入点到循环后的块
    builder->SetInsertPoint(afterBlock);
}

void llvmpy::PyCodeGen::visit(PrintStmtAST* stmt)
{
    auto* expr = static_cast<const ExprAST*>(stmt->getValue());
    if (!expr)
    {
        logError("Print statement has no expression",
                 stmt->line.value_or(-1), stmt->column.value_or(-1));
        return;
    }

    // 生成表达式值
    llvm::Value* value = handleExpr(const_cast<ExprAST*>(expr));
    if (!value) return;  // 错误已经被记录

    // 获取表达式类型
    auto typePtr = expr->getType();
    if (!typePtr)
    {
        logError("Cannot determine expression type",
                 expr->line.value_or(-1), expr->column.value_or(-1));
        return;
    }

    ObjectType* type = typePtr->getObjectType();

    // 调试信息
    std::string llvmTypeStr;
    llvm::raw_string_ostream rso(llvmTypeStr);
    value->getType()->print(rso);

    std::cerr << "DEBUG: Print statement - Value LLVM type: " << llvmTypeStr
              << ", Python type ID: " << type->getTypeId()
              << ", Type name: " << type->getName()
              << ", isReference: " << (type->isReference() ? "true" : "false") << std::endl;

    // 关键修复：检测栈变量并适当处理它们
    bool isStackBasicType = false;
    llvm::Type* actualType = nullptr;

    if (auto allocaInst = llvm::dyn_cast<llvm::AllocaInst>(value))
    {
        std::cerr << "DEBUG: Value is AllocaInst" << std::endl;

        // 获取分配的类型
        auto allocatedType = allocaInst->getAllocatedType();
        std::string allocTypeStr;
        llvm::raw_string_ostream allocRso(allocTypeStr);
        allocatedType->print(allocRso);
        std::cerr << "DEBUG: Allocated type: " << allocTypeStr << std::endl;

        // 检查是否是基本类型的栈变量
        isStackBasicType = allocatedType->isIntegerTy() || allocatedType->isDoubleTy() || allocatedType->isFloatTy() || allocatedType->isIntegerTy(1);
        actualType = allocatedType;

        std::cerr << "DEBUG: isStackBasicType=" << (isStackBasicType ? "true" : "false") << std::endl;

        // 如果是基本类型的栈变量，加载其值
        if (isStackBasicType)
        {
            value = builder->CreateLoad(allocatedType, value, "loaded_val");
        }
        // 如果是指针类型的栈变量(如对象引用)
        else if (allocatedType->isPointerTy())
        {
            std::cerr << "DEBUG: Checking pointer element type" << std::endl;
            // 检查指针类型是否为PyObject*
            if (type->isReference())
            {
                value = builder->CreateLoad(allocatedType, value, "loaded_ptr");
            }
        }
    }

    // 根据实际类型选择正确的打印函数
    if (value->getType()->isIntegerTy(32) || (isStackBasicType && actualType && actualType->isIntegerTy(32)))
    {
        std::cerr << "DEBUG: Using py_print_int for integer value" << std::endl;
        llvm::Function* printFunc = getOrCreateExternalFunction(
            "py_print_int",
            llvm::Type::getVoidTy(getContext()),
            {llvm::Type::getInt32Ty(getContext())});
        builder->CreateCall(printFunc, {value});
    }
    else if (value->getType()->isDoubleTy() || (isStackBasicType && actualType && actualType->isDoubleTy()))
    {
        std::cerr << "DEBUG: Using py_print_double for double value" << std::endl;
        llvm::Function* printFunc = getOrCreateExternalFunction(
            "py_print_double",
            llvm::Type::getVoidTy(getContext()),
            {llvm::Type::getDoubleTy(getContext())});
        builder->CreateCall(printFunc, {value});
    }
    else if (value->getType()->isIntegerTy(1) || (isStackBasicType && actualType && actualType->isIntegerTy(1)))
    {
        std::cerr << "DEBUG: Using py_print_bool for boolean value" << std::endl;
        llvm::Function* printFunc = getOrCreateExternalFunction(
            "py_print_bool",
            llvm::Type::getVoidTy(getContext()),
            {llvm::Type::getInt1Ty(getContext())});
        builder->CreateCall(printFunc, {value});
    }
    else if (value->getType()->isPointerTy())
    {
        // 尝试进一步判断指针类型
        if (type->getTypeId() == llvmpy::PY_TYPE_STRING)
        {
            std::cerr << "DEBUG: Using py_print_string for string value" << std::endl;
            llvm::Function* printFunc = getOrCreateExternalFunction(
                "py_print_string",
                llvm::Type::getVoidTy(getContext()),
                {llvm::PointerType::get(getContext(), 0)});
            builder->CreateCall(printFunc, {value});
        }
        else
        {
            std::cerr << "DEBUG: Using fallback printing path with py_print_object" << std::endl;
            llvm::Function* printFunc = getOrCreateExternalFunction(
                "py_print_object",
                llvm::Type::getVoidTy(getContext()),
                {llvm::PointerType::get(getContext(), 0)});
            builder->CreateCall(printFunc, {value});
        }
    }
    else
    {
        std::cerr << "WARNING: Unsupported value type for print statement, attempting to convert to object" << std::endl;
        // 尝试将值转换为对象再打印
        llvm::Value* objValue = ObjectLifecycleManager::createObject(*this, value, type->getTypeId());

        llvm::Function* printFunc = getOrCreateExternalFunction(
            "py_print_object",
            llvm::Type::getVoidTy(getContext()),
            {llvm::PointerType::get(getContext(), 0)});

        builder->CreateCall(printFunc, {objValue});
    }
}
// 完成 AssignStmtAST 的 visit 方法实现
void llvmpy::PyCodeGen::visit(AssignStmtAST* stmt)
{
    // 1. 获取基本信息
    const std::string& name = stmt->getName();
    auto* expr = static_cast<const ExprAST*>(stmt->getValue());

    if (name.empty() || !expr)
    {
        logError("Invalid assignment statement",
                 stmt->line.value_or(-1), stmt->column.value_or(-1));
        return;
    }

    // 2. 生成表达式值
    llvm::Value* value = handleExpr(const_cast<ExprAST*>(expr));
    if (!value) return;

    // 3. 获取表达式类型和信息
    auto typePtr = expr->getType();
    if (!typePtr)
    {
        logError("Cannot determine expression type",
                 expr->line.value_or(-1), expr->column.value_or(-1));
        return;
    }

    ObjectType* exprType = typePtr->getObjectType();

    // 关键修改：确保容器类型始终被视为引用类型
    bool exprIsReference = exprType->isReference() || exprType->getCategory() == ObjectType::Container || exprType->getTypeId() >= PY_TYPE_LIST;
    int exprTypeId = exprType->getTypeId();

    // 4. 检查变量是否已存在，获取变量存储位置和类型
    llvm::Value* varPtr = getVariable(name);
    bool isNewVariable = !varPtr;
    ObjectType* varType = isNewVariable ? nullptr : getVariableType(name);
    // 【新增部分 - 处理复合赋值】
    // 检查是否是复合赋值产生的二元表达式
    if (!isNewVariable && dynamic_cast<const BinaryExprAST*>(expr) != nullptr)
    {
        auto* binExpr = dynamic_cast<const BinaryExprAST*>(expr);

        // 确认左操作数是同一个变量的引用
        if (auto* varExpr = dynamic_cast<const VariableExprAST*>(binExpr->getLHS()))
        {
            if (varExpr->getName() == name)  // 确认是同名变量(如 a = a + 1)
            {
                DEBUG_LOG("处理复合赋值: " << name << " " << binExpr->getOp() << "= ...");

                // 1. 获取变量当前值
                llvm::Value* currentValue = nullptr;

                if (varType->isReference() || varType->getCategory() == ObjectType::Container || varType->getTypeId() >= PY_TYPE_LIST)
                {
                    // 引用类型：加载指针
                    currentValue = builder->CreateLoad(
                        llvm::PointerType::get(getContext(), 0), varPtr, "current_val");
                }
                else
                {
                    // 基本类型：直接加载值
                    llvm::Type* loadType = nullptr;
                    switch (varType->getTypeId())
                    {
                        case PY_TYPE_INT:
                            loadType = llvm::Type::getInt32Ty(getContext());
                            break;
                        case PY_TYPE_DOUBLE:
                            loadType = llvm::Type::getDoubleTy(getContext());
                            break;
                        case PY_TYPE_BOOL:
                            loadType = llvm::Type::getInt1Ty(getContext());
                            break;
                        default:
                            loadType = llvm::PointerType::get(getContext(), 0);
                            break;
                    }
                    currentValue = builder->CreateLoad(loadType, varPtr, "current_val");
                }

                // 2. 设置表达式左边的值（绕过AST，直接设置值）
                lastExprValue = currentValue;
                lastExprType = std::make_shared<PyType>(varType);
            }
        }
    }
    // 【新增部分结束】

    // 同样确保变量类型如果是容器，也被视为引用类型
    bool varIsReference = varType && (varType->isReference() || varType->getCategory() == ObjectType::Container || varType->getTypeId() >= PY_TYPE_LIST);
    int varTypeId = varType ? varType->getTypeId() : -1;

    DEBUG_LOG("赋值: 变量 '" << name << "' "
                             << (isNewVariable ? "新创建" : "已存在")
                             << ", 表达式类型: " << exprType->getName()
                             << ", 表达式是引用: " << (exprIsReference ? "是" : "否"));

    // 5. 变量类型决策 - 确定最终的变量类型和存储方式
    if (isNewVariable)
    {
        // 5.1. 创建新变量

        // 对于ANY类型表达式，尝试用更具体的类型
        if (exprTypeId == PY_TYPE_ANY && !exprIsReference)
        {
            // 根据实际值类型推断更具体的类型
            if (value->getType()->isIntegerTy(32))
            {
                varType = TypeRegistry::getInstance().getType("int");
                varIsReference = false;
                DEBUG_LOG("ANY值具体化为int类型");
            }
            else if (value->getType()->isDoubleTy())
            {
                varType = TypeRegistry::getInstance().getType("double");
                varIsReference = false;
                DEBUG_LOG("ANY值具体化为double类型");
            }
            else if (value->getType()->isIntegerTy(1))
            {
                varType = TypeRegistry::getInstance().getType("bool");
                varIsReference = false;
                DEBUG_LOG("ANY值具体化为bool类型");
            }
            else if (value->getType()->isPointerTy())
            {
                // 保持ANY类型但是引用
                varType = exprType;
                varIsReference = true;
                DEBUG_LOG("ANY值以引用方式存储");
            }
            else
            {
                varType = exprType;
                varIsReference = exprIsReference;
                DEBUG_LOG("保持ANY类型不变");
            }
        }
        else
        {
            // 使用表达式的类型，但确保容器类型总是引用
            varType = exprType;
            varIsReference = exprIsReference;

            // 确保列表、字典等容器类型始终是引用类型
            if (varType->getCategory() == ObjectType::Container || varType->getTypeId() >= PY_TYPE_LIST)
            {
                varIsReference = true;
                DEBUG_LOG("确保容器类型作为引用存储");
            }
        }

        // 创建变量存储 - 根据是否是引用类型决定分配类型
        if (varIsReference)
        {
            // 引用类型分配为指针
            varPtr = builder->CreateAlloca(
                llvm::PointerType::get(getContext(), 0),
                nullptr, name);
            DEBUG_LOG("为引用类型分配指针存储");
        }
        else
        {
            // 基本类型根据实际类型分配
            llvm::Type* allocaType = nullptr;

            switch (varType->getTypeId())
            {
                case PY_TYPE_INT:
                    allocaType = llvm::Type::getInt32Ty(getContext());
                    break;
                case PY_TYPE_DOUBLE:
                    allocaType = llvm::Type::getDoubleTy(getContext());
                    break;
                case PY_TYPE_BOOL:
                    allocaType = llvm::Type::getInt1Ty(getContext());
                    break;
                default:
                    // 其他类型默认作为引用处理
                    allocaType = llvm::PointerType::get(getContext(), 0);
                    varIsReference = true;
                    DEBUG_LOG("无法识别的类型作为引用处理");
                    break;
            }

            varPtr = builder->CreateAlloca(allocaType, nullptr, name);
            DEBUG_LOG("为基本类型分配存储: " << varType->getName());
        }

        // 注册到符号表
        setVariable(name, varPtr, varType);
        DEBUG_LOG("创建新变量: '" << name << "', 类型: "
                                  << varType->getName() << ", 是引用: "
                                  << (varIsReference ? "是" : "否"));
    }
    else
    {
        // 5.2. 更新现有变量，需检查类型兼容性

        // 特殊处理ANY类型变量
        if (varTypeId == PY_TYPE_ANY)
        {
            // ANY类型可以接受任何值，但存储形式取决于之前的存储方式
            DEBUG_LOG("为ANY类型变量赋值，保持存储形式: "
                      << (varIsReference ? "引用" : "值"));
        }
        // 检查类型兼容性
        else if (!exprType->canAssignTo(varType))
        {
            logTypeError("Cannot assign value of type '" + exprType->getName() + "' to variable '" + name + "' of type '" + varType->getName() + "'",
                         stmt->line.value_or(-1), stmt->column.value_or(-1));
            return;
        }

        DEBUG_LOG("为已存在变量赋值: '" << name << "', 变量类型: "
                                        << varType->getName() << ", 变量是引用: "
                                        << (varIsReference ? "是" : "否"));
    }

    // 6. 值类型转换 - 确保值与最终变量类型匹配
    if (exprTypeId != varType->getTypeId() && varTypeId != PY_TYPE_ANY)
    {
        DEBUG_LOG("需要类型转换: 从 " << exprType->getName() << " 到 " << varType->getName());

        // 使用类型系统执行转换
        auto& registry = TypeOperationRegistry::getInstance();
        TypeConversionDescriptor* convDesc = registry.getTypeConversionDescriptor(exprTypeId, varType->getTypeId());

        if (convDesc && !convDesc->runtimeFunction.empty())
        {
            // 使用运行时转换函数
            DEBUG_LOG("使用运行时转换函数: " << convDesc->runtimeFunction);

            // 确保值是指针类型
            if (!value->getType()->isPointerTy())
            {
                value = ensurePythonObject(value, exprType);
            }

            llvm::Function* convFunc = getOrCreateExternalFunction(
                convDesc->runtimeFunction,
                llvm::PointerType::get(getContext(), 0),
                {llvm::PointerType::get(getContext(), 0)});

            value = builder->CreateCall(convFunc, {value}, "conv_result");

            // 确保转换后的值带有正确的类型元数据
            ObjectLifecycleManager::attachTypeMetadata(value, varType->getTypeId());
        }
        else if (convDesc && convDesc->customImpl)
        {
            // 使用自定义转换实现
            DEBUG_LOG("使用自定义类型转换处理");
            value = convDesc->customImpl(*this, value);
            if (!value) return;

            // 添加类型元数据
            ObjectLifecycleManager::attachTypeMetadata(value, varType->getTypeId());
        }
        else
        {
            // 尝试通用转换方法
            value = generateTypeConversion(value, exprType, varType);
            if (!value)
            {
                logTypeError("无法将 " + exprType->getName() + " 转换为 " + varType->getName(),
                             stmt->line.value_or(-1), stmt->column.value_or(-1));
                return;
            }
        }
    }

    // 7. 生成存储指令 - 根据变量存储方式处理
    if (varIsReference)
    {
        // 7.1 引用类型变量 (任何Python对象，包括容器)

        // 确保值是指针类型
        if (!value->getType()->isPointerTy())
        {
            DEBUG_LOG("将非指针值包装为对象指针");
            value = ensurePythonObject(value, varType);
            if (!value)
            {
                logError("无法将值转换为对象指针",
                         stmt->line.value_or(-1), stmt->column.value_or(-1));
                return;
            }
        }

        // 增加新值的引用计数
        llvm::Function* incRefFunc = getOrCreateExternalFunction(
            "py_incref", llvm::Type::getVoidTy(getContext()),
            {llvm::PointerType::get(getContext(), 0)});
        builder->CreateCall(incRefFunc, {value});

        // 为旧值减少引用计数 (若非新变量)
        if (!isNewVariable)
        {
            llvm::Value* oldValue = builder->CreateLoad(
                llvm::PointerType::get(getContext(), 0), varPtr, "oldval");

            llvm::Function* decRefFunc = getOrCreateExternalFunction(
                "py_decref", llvm::Type::getVoidTy(getContext()),
                {llvm::PointerType::get(getContext(), 0)});

            // 添加非空检查
            llvm::Value* isNonNull = builder->CreateICmpNE(
                oldValue,
                llvm::ConstantPointerNull::get(llvm::PointerType::get(getContext(), 0)),
                "is_nonnull");

            llvm::BasicBlock* decRefBlock = llvm::BasicBlock::Create(getContext(), "decref", builder->GetInsertBlock()->getParent());
            llvm::BasicBlock* continueBlock = llvm::BasicBlock::Create(getContext(), "continue", builder->GetInsertBlock()->getParent());

            builder->CreateCondBr(isNonNull, decRefBlock, continueBlock);

            builder->SetInsertPoint(decRefBlock);
            builder->CreateCall(decRefFunc, {oldValue});
            builder->CreateBr(continueBlock);

            builder->SetInsertPoint(continueBlock);
        }

        // 存储新对象指针
        builder->CreateStore(value, varPtr);
    }
    else
    {
        // 7.2 基本类型变量 (int, double, bool)

        // 从对象中提取基本值（如果是指针）
        if (value->getType()->isPointerTy())
        {
            DEBUG_LOG("从Python对象提取基本值");

            std::string extractFunc;
            llvm::Type* basicType;

            switch (varType->getTypeId())
            {
                case PY_TYPE_INT:
                    extractFunc = "py_extract_int";
                    basicType = llvm::Type::getInt32Ty(getContext());
                    break;
                case PY_TYPE_DOUBLE:
                    extractFunc = "py_extract_double";
                    basicType = llvm::Type::getDoubleTy(getContext());
                    break;
                case PY_TYPE_BOOL:
                    extractFunc = "py_extract_bool";
                    basicType = llvm::Type::getInt1Ty(getContext());
                    break;
                default:
                    logTypeError("不支持的基本类型赋值",
                                 stmt->line.value_or(-1), stmt->column.value_or(-1));
                    return;
            }

            llvm::Function* extractFunc_ = getOrCreateExternalFunction(
                extractFunc, basicType, {llvm::PointerType::get(getContext(), 0)});
            value = builder->CreateCall(extractFunc_, {value}, "extracted_value");
        }

        // 确保类型精确匹配
        if (varType->getTypeId() == PY_TYPE_INT && !value->getType()->isIntegerTy(32))
        {
            value = builder->CreateIntCast(value, llvm::Type::getInt32Ty(getContext()),
                                           true, "to_i32_for_store");
        }
        else if (varType->getTypeId() == PY_TYPE_DOUBLE && !value->getType()->isDoubleTy())
        {
            if (value->getType()->isIntegerTy())
            {
                value = builder->CreateSIToFP(value, llvm::Type::getDoubleTy(getContext()),
                                              "int_to_double_for_store");
            }
            else if (value->getType()->isFloatTy())
            {
                value = builder->CreateFPExt(value, llvm::Type::getDoubleTy(getContext()),
                                             "float_to_double_for_store");
            }
        }
        else if (varType->getTypeId() == PY_TYPE_BOOL && !value->getType()->isIntegerTy(1))
        {
            if (value->getType()->isIntegerTy())
            {
                value = builder->CreateICmpNE(value,
                                              llvm::ConstantInt::get(value->getType(), 0), "to_bool_for_store");
            }
            else if (value->getType()->isFloatingPointTy())
            {
                value = builder->CreateFCmpONE(value,
                                               llvm::ConstantFP::get(value->getType(), 0.0), "float_to_bool_for_store");
            }
        }

        // 存储基本值
        builder->CreateStore(value, varPtr);
    }

    // 8. 清理临时对象
    releaseTempObjects();

    DEBUG_LOG("赋值完成: 变量 '" << name << "'");
}

void PyCodeGen::visit(IndexAssignStmtAST* stmt)
{
    // 获取左侧索引表达式和右侧值表达式
    const ExprAST* targetExpr = stmt->getTarget();
    const ExprAST* valueExpr = stmt->getValue();

    // 增加调试信息
    std::cerr << "DEBUG: IndexAssignStmt - targetExpr kind: "
              << (targetExpr ? static_cast<int>(targetExpr->kind()) : -1) << std::endl;

    if (!targetExpr || !valueExpr)
    {
        logError("Invalid index assignment statement",
                 stmt->line.value_or(-1), stmt->column.value_or(-1));
        return;
    }

    // 直接获取目标和索引
    // 索引赋值的目标通常有两种形式：
    // 1. 直接是一个变量，索引在stmt中单独存储
    // 2. 已经是一个IndexExprAST

    const ExprAST* index = stmt->getIndex();
    if (index)
    {
        // 如果索引直接可用，使用它
        performIndexAssignment(targetExpr, index, valueExpr, stmt);
        return;
    }

    // 否则，尝试从targetExpr提取目标和索引（如果它是IndexExprAST）
    const IndexExprAST* indexExpr = dynamic_cast<const IndexExprAST*>(targetExpr);
    if (indexExpr)
    {
        const ExprAST* target = indexExpr->getTarget();
        const ExprAST* index = indexExpr->getIndex();

        if (!target || !index)
        {
            logError("Invalid index expression in assignment",
                     targetExpr->line.value_or(-1), targetExpr->column.value_or(-1));
            return;
        }

        performIndexAssignment(target, index, valueExpr, stmt);
        return;
    }

    // 目标既不是有效的IndexExprAST，也没有单独提供index
    std::string typeInfo = "unknown";
    if (targetExpr->getType())
    {
        typeInfo = targetExpr->getType()->getObjectType()->getName();
    }

    logError("Target of index assignment must be an index expression, got: " + typeInfo + " (kind: " + std::to_string(static_cast<int>(targetExpr->kind())) + ")",
             targetExpr->line.value_or(-1), targetExpr->column.value_or(-1));
}

void llvmpy::PyCodeGen::performIndexAssignment(const ExprAST* target, const ExprAST* index,
                                               const ExprAST* valueExpr, const StmtAST* stmt)
{
    // 1. 生成目标、索引和值的代码
    llvm::Value* targetValue = handleExpr(const_cast<ExprAST*>(target));
    if (!targetValue) return;

    llvm::Value* indexValue = handleExpr(const_cast<ExprAST*>(index));
    if (!indexValue) return;

    llvm::Value* value = handleExpr(const_cast<ExprAST*>(valueExpr));
    if (!value) return;

    // 2. 获取类型信息
    auto targetTypePtr = target->getType();
    auto indexTypePtr = index->getType();
    auto valueTypePtr = valueExpr->getType();

    if (!targetTypePtr || !indexTypePtr || !valueTypePtr)
    {
        logError("无法确定索引赋值的类型信息",
                 stmt->line.value_or(-1), stmt->column.value_or(-1));
        return;
    }

    ObjectType* targetType = targetTypePtr->getObjectType();
    ObjectType* indexType = indexTypePtr->getObjectType();
    ObjectType* valueType = valueTypePtr->getObjectType();

    // 调试输出
    DEBUG_LOG("索引赋值 - 目标类型: " << targetType->getName()
                                      << ", 类型ID: " << targetType->getTypeId()
                                      << ", 索引类型: " << indexType->getName()
                                      << ", 值类型: " << valueType->getName());

    // 3. 检查符号表中是否有更准确的类型信息
    bool hasMoreAccurateType = false;
    std::string varName;

    if (const VariableExprAST* varExpr = dynamic_cast<const VariableExprAST*>(target))
    {
        varName = varExpr->getName();
        ObjectType* symbolType = getVariableType(varName);

        if (symbolType && (symbolType->getTypeId() == PY_TYPE_LIST || symbolType->getCategory() == ObjectType::Container || TypeFeatureChecker::hasFeature(symbolType, "container") || TypeFeatureChecker::hasFeature(symbolType, "sequence")))
        {
            DEBUG_LOG("使用符号表中'" << varName
                                      << "'的更精确类型: " << symbolType->getName()
                                      << ", 类型ID: " << symbolType->getTypeId());

            targetType = symbolType;  // 使用符号表中的更准确类型
            hasMoreAccurateType = true;
        }
    }

    // 4. 如果目标是变量地址，加载实际指针
    if (auto allocaInst = llvm::dyn_cast<llvm::AllocaInst>(targetValue))
    {
        auto allocatedType = allocaInst->getAllocatedType();
        std::string allocTypeStr;
        llvm::raw_string_ostream allocRso(allocTypeStr);
        allocatedType->print(allocRso);

        DEBUG_LOG("目标是分配指令，分配的类型: " << allocTypeStr);

        // 如果分配的类型是指针，加载实际的容器指针
        if (allocatedType->isPointerTy())
        {
            DEBUG_LOG("从变量 " << (varName.empty() ? "target" : varName) << " 加载实际容器指针");
            targetValue = builder->CreateLoad(allocatedType, targetValue, "loaded_container_ptr");

            // 确保加载的值具有正确的类型元数据
            if (hasMoreAccurateType)
            {
                ObjectLifecycleManager::attachTypeMetadata(targetValue, targetType->getTypeId());
            }
        }
    }

    // 5. 验证索引操作
    if (!validateIndexOperation(const_cast<ExprAST*>(target), const_cast<ExprAST*>(index)))
    {
        return;
    }

    // 6. 根据目标类型执行不同的索引赋值操作
    // 明确检测类型 - 不依赖于dynamic_cast
    bool isList = (targetType->getTypeId() == PY_TYPE_LIST || targetType->getName().find("list") != std::string::npos);
    bool isDict = (targetType->getTypeId() == PY_TYPE_DICT || targetType->getName().find("dict") != std::string::npos);
    bool isString = (targetType->getTypeId() == PY_TYPE_STRING || targetType->getName() == "string");

    // 强制应用符号表中的类型信息
    if (hasMoreAccurateType && targetType->getTypeId() == PY_TYPE_LIST)
    {
        isList = true;
        isDict = false;
        isString = false;
    }

    // 7. 处理列表索引赋值
    // 7. 处理列表索引赋值
    if (isList)
    {
        DEBUG_LOG("生成列表索引赋值操作");

        // 7.1 验证索引类型
        if (indexType->getTypeId() != PY_TYPE_INT && indexType->getTypeId() != PY_TYPE_ANY)
        {
            logTypeError("列表索引必须是整数类型，得到: " + indexType->getName(),
                         index->line.value_or(-1), index->column.value_or(-1));
            return;
        }

        // 7.2 确保索引是整数值
        llvm::Value* indexIntValue = indexValue;
        if (indexValue->getType()->isPointerTy())
        {
            // 从对象指针提取整数值
            llvm::Function* extractIntFunc = getOrCreateExternalFunction(
                "py_extract_int",
                llvm::Type::getInt32Ty(getContext()),
                {llvm::PointerType::get(getContext(), 0)});

            indexIntValue = builder->CreateCall(extractIntFunc, {indexValue}, "extracted_idx");
        }
        else if (!indexValue->getType()->isIntegerTy(32))
        {
            // 类型转换为i32
            indexIntValue = builder->CreateIntCast(
                indexValue, llvm::Type::getInt32Ty(getContext()), true, "idx_to_i32");
        }

        // 7.3 确保值是对象指针 - 这里是关键修复！
        llvm::Value* valueObj = nullptr;

        // 检测栈上基本类型变量，如int, double, bool
        if (auto allocaInst = llvm::dyn_cast<llvm::AllocaInst>(value))
        {
            llvm::Type* allocatedType = allocaInst->getAllocatedType();
            DEBUG_LOG("值是栈变量，分配类型: " << (allocatedType->isIntegerTy(32) ? "int32" : allocatedType->isDoubleTy() ? "double"
                                                                                          : allocatedType->isIntegerTy(1) ? "bool"
                                                                                                                          : "其他"));

            if (allocatedType->isIntegerTy(32))  // int
            {
                // 从栈变量加载整数值
                llvm::Value* loadedInt = builder->CreateLoad(allocatedType, value, "loaded_int_val");

                // 创建Python整数对象
                valueObj = builder->CreateCall(
                    getOrCreateExternalFunction(
                        "py_create_int",
                        llvm::PointerType::get(getContext(), 0),
                        {llvm::Type::getInt32Ty(getContext())}),
                    {loadedInt},
                    "int_obj_for_list_assign");

                // 附加类型元数据
                ObjectLifecycleManager::attachTypeMetadata(valueObj, PY_TYPE_INT);
                DEBUG_LOG("从栈变量创建整数对象");
            }
            else if (allocatedType->isDoubleTy())  // double
            {
                llvm::Value* loadedDouble = builder->CreateLoad(allocatedType, value, "loaded_double_val");

                valueObj = builder->CreateCall(
                    getOrCreateExternalFunction(
                        "py_create_double",
                        llvm::PointerType::get(getContext(), 0),
                        {llvm::Type::getDoubleTy(getContext())}),
                    {loadedDouble},
                    "double_obj_for_list_assign");

                ObjectLifecycleManager::attachTypeMetadata(valueObj, PY_TYPE_DOUBLE);
                DEBUG_LOG("从栈变量创建浮点对象");
            }
            else if (allocatedType->isIntegerTy(1))  // bool
            {
                llvm::Value* loadedBool = builder->CreateLoad(allocatedType, value, "loaded_bool_val");

                valueObj = builder->CreateCall(
                    getOrCreateExternalFunction(
                        "py_create_bool",
                        llvm::PointerType::get(getContext(), 0),
                        {llvm::Type::getInt1Ty(getContext())}),
                    {loadedBool},
                    "bool_obj_for_list_assign");

                ObjectLifecycleManager::attachTypeMetadata(valueObj, PY_TYPE_BOOL);
                DEBUG_LOG("从栈变量创建布尔对象");
            }
            else if (allocatedType->isPointerTy())  // 如果是指针类型的栈变量(可能已经是对象指针)
            {
                llvm::Value* loadedPtr = builder->CreateLoad(allocatedType, value, "loaded_ptr_val");
                valueObj = loadedPtr;  // 直接使用加载的指针
                DEBUG_LOG("从栈变量加载对象指针");
            }
        }
        // 直接字面量或已经是对象指针的情况
        else if (value->getType()->isPointerTy())
        {
            // 已经是指针，可能是对象指针
            valueObj = value;
            DEBUG_LOG("值已经是对象指针");
        }
        else if (value->getType()->isIntegerTy(32))  // int字面量
        {
            valueObj = builder->CreateCall(
                getOrCreateExternalFunction(
                    "py_create_int",
                    llvm::PointerType::get(getContext(), 0),
                    {llvm::Type::getInt32Ty(getContext())}),
                {value},
                "int_obj_from_literal");

            ObjectLifecycleManager::attachTypeMetadata(valueObj, PY_TYPE_INT);
            DEBUG_LOG("从整数字面量创建对象");
        }
        else if (value->getType()->isDoubleTy())  // double字面量
        {
            valueObj = builder->CreateCall(
                getOrCreateExternalFunction(
                    "py_create_double",
                    llvm::PointerType::get(getContext(), 0),
                    {llvm::Type::getDoubleTy(getContext())}),
                {value},
                "double_obj_from_literal");

            ObjectLifecycleManager::attachTypeMetadata(valueObj, PY_TYPE_DOUBLE);
            DEBUG_LOG("从浮点字面量创建对象");
        }
        else if (value->getType()->isIntegerTy(1))  // bool字面量
        {
            valueObj = builder->CreateCall(
                getOrCreateExternalFunction(
                    "py_create_bool",
                    llvm::PointerType::get(getContext(), 0),
                    {llvm::Type::getInt1Ty(getContext())}),
                {value},
                "bool_obj_from_literal");

            ObjectLifecycleManager::attachTypeMetadata(valueObj, PY_TYPE_BOOL);
            DEBUG_LOG("从布尔字面量创建对象");
        }
        else
        {
            // 如果以上情况都不是，尝试通用转换
            valueObj = ensurePythonObject(value, valueType);
            DEBUG_LOG("使用通用方法确保对象指针");
        }

        // 检查是否成功创建/获取对象指针
        if (!valueObj)
        {
            logTypeError("无法将值类型 " + valueType->getName() + " 转换为对象指针",
                         valueExpr->line.value_or(-1), valueExpr->column.value_or(-1));
            return;
        }

        // 7.4 验证元素类型与值类型兼容性
        ListType* listType = dynamic_cast<ListType*>(targetType);
        if (listType && listType->getElementType())
        {
            ObjectType* elemType = const_cast<ObjectType*>(listType->getElementType());

            // 验证值类型与元素类型的兼容性
            if (!valueType->canAssignTo(elemType))
            {
                logTypeError("无法将 " + valueType->getName() + " 类型的值赋给列表类型 " + elemType->getName() + " 的元素",
                             valueExpr->line.value_or(-1), valueExpr->column.value_or(-1));
                return;
            }
        }

        // 7.5 增加新值的引用计数
        llvm::Function* incRefFunc = getOrCreateExternalFunction(
            "py_incref",
            llvm::Type::getVoidTy(getContext()),
            {llvm::PointerType::get(getContext(), 0)});

        builder->CreateCall(incRefFunc, {valueObj});
        DEBUG_LOG("增加对象引用计数");

        // 7.6 调用列表设置元素函数
        llvm::Function* setItemFunc = getOrCreateExternalFunction(
            "py_list_set_item",
            llvm::Type::getVoidTy(getContext()),
            {
                llvm::PointerType::get(getContext(), 0),  // list
                llvm::Type::getInt32Ty(getContext()),     // index
                llvm::PointerType::get(getContext(), 0)   // value
            });

        builder->CreateCall(setItemFunc, {targetValue, indexIntValue, valueObj});
        DEBUG_LOG("列表索引赋值操作完成");
    }
    // 8. 处理字典索引赋值
    else if (isDict)
    {
        DEBUG_LOG("生成字典索引赋值操作");

        // 8.1 确保键是对象指针
        llvm::Value* keyObj = indexValue;
        if (!indexValue->getType()->isPointerTy())
        {
            DEBUG_LOG("将键值转换为对象指针");
            // 根据索引值类型选择合适的转换函数
            keyObj = ensurePythonObject(indexValue, indexType);

            if (!keyObj)
            {
                logTypeError("无法将索引值转换为对象指针",
                             index->line.value_or(-1), index->column.value_or(-1));
                return;
            }
        }

        // 8.2 确保值是对象指针
        llvm::Value* valueObj = value;
        if (!value->getType()->isPointerTy())
        {
            DEBUG_LOG("将值转换为对象指针");
            valueObj = ensurePythonObject(value, valueType);

            if (!valueObj)
            {
                logTypeError("无法将值转换为对象指针",
                             valueExpr->line.value_or(-1), valueExpr->column.value_or(-1));
                return;
            }
        }

        // 8.3 验证键值类型兼容性
        // 获取字典键值类型
        if (DictType* dictType = dynamic_cast<DictType*>(targetType))
        {
            ObjectType* keyType = const_cast<ObjectType*>(dictType->getKeyType());
            ObjectType* valType = const_cast<ObjectType*>(dictType->getValueType());

            // 验证类型兼容性
            if (keyType && !indexType->canAssignTo(keyType))
            {
                logTypeError("无法将 " + indexType->getName() + " 类型的键赋给字典键类型 " + keyType->getName(),
                             index->line.value_or(-1), index->column.value_or(-1));
                return;
            }

            if (valType && !valueType->canAssignTo(valType))
            {
                logTypeError("无法将 " + valueType->getName() + " 类型的值赋给字典值类型 " + valType->getName(),
                             valueExpr->line.value_or(-1), valueExpr->column.value_or(-1));
                return;
            }
        }

        // 8.4 增加键和值的引用计数
        llvm::Function* incRefFunc = getOrCreateExternalFunction(
            "py_incref",
            llvm::Type::getVoidTy(getContext()),
            {llvm::PointerType::get(getContext(), 0)});

        builder->CreateCall(incRefFunc, {keyObj});
        builder->CreateCall(incRefFunc, {valueObj});

        // 8.5 调用字典设置项函数
        llvm::Function* setItemFunc = getOrCreateExternalFunction(
            "py_dict_set_item",
            llvm::Type::getVoidTy(getContext()),
            {
                llvm::PointerType::get(getContext(), 0),  // dict
                llvm::PointerType::get(getContext(), 0),  // key
                llvm::PointerType::get(getContext(), 0)   // value
            });

        builder->CreateCall(setItemFunc, {targetValue, keyObj, valueObj});

        DEBUG_LOG("字典索引赋值操作完成");
    }
    // 9. 处理字符串索引赋值 (一般不允许)
    else if (isString)
    {
        // 字符串不支持索引赋值
        logError("字符串对象不支持索引赋值",
                 stmt->line.value_or(-1), stmt->column.value_or(-1));
        return;
    }
    // 10. 处理其他未知类型
    else
    {
        // 尝试识别可能的容器类型
        int typeId = targetType->getTypeId();
        bool isContainer = (targetType->getCategory() == ObjectType::Container) || (typeId >= PY_TYPE_LIST && typeId <= PY_TYPE_SET) || targetType->hasFeature("container") || targetType->hasFeature("sequence");

        if (isContainer)
        {
            DEBUG_LOG("尝试通用容器索引赋值: " << targetType->getName());

            // 确保索引和值都是对象指针
            llvm::Value* idxObj = ensurePythonObject(indexValue, indexType);
            llvm::Value* valObj = ensurePythonObject(value, valueType);

            // 增加引用计数
            llvm::Function* incRefFunc = getOrCreateExternalFunction(
                "py_incref", llvm::Type::getVoidTy(getContext()),
                {llvm::PointerType::get(getContext(), 0)});

            builder->CreateCall(incRefFunc, {valObj});

            // 尝试通用容器赋值函数
            llvm::Function* setItemFunc = getOrCreateExternalFunction(
                "py_container_set_item",
                llvm::Type::getVoidTy(getContext()),
                {
                    llvm::PointerType::get(getContext(), 0),  // container
                    llvm::PointerType::get(getContext(), 0),  // index
                    llvm::PointerType::get(getContext(), 0)   // value
                });

            builder->CreateCall(setItemFunc, {targetValue, idxObj, valObj});
        }
        else
        {
            // 目标不支持索引赋值
            logError("目标类型 " + targetType->getName() + " 不支持索引赋值",
                     stmt->line.value_or(-1), stmt->column.value_or(-1));
            return;
        }
    }

    // 11. 清理临时对象
    releaseTempObjects();
}
void llvmpy::PyCodeGen::visit(PassStmtAST* stmt)
{
    // pass语句不做任何事情
}

void llvmpy::PyCodeGen::visit(ImportStmtAST* stmt)
{
    // 获取模块名称和别名
    const std::string& moduleName = stmt->getModuleName();
    const std::string& alias = stmt->getAlias();

    // 调用运行时函数导入模块
    llvm::Function* importModuleFunc = getOrCreateExternalFunction(
        "py_import_module",
        llvm::PointerType::get(getContext(), 0),
        {llvm::PointerType::get(llvm::Type::getInt8Ty(getContext()), 0)});

    // 创建模块名字符串常量
    llvm::Value* moduleNameStr = builder->CreateGlobalStringPtr(moduleName, "modname");

    // 调用导入函数
    llvm::Value* moduleObj = builder->CreateCall(importModuleFunc, {moduleNameStr}, "module");

    // 将模块对象存储到使用的名称中（原名或别名）
    std::string varName = alias.empty() ? moduleName : alias;

    // 创建模块变量
    llvm::Value* varPtr = PyCodeGenHelper::createLocalVariable(
        *this, varName, TypeRegistry::getInstance().getType("module"), moduleObj);

    // 将变量添加到当前作用域
    setVariable(varName, varPtr, TypeRegistry::getInstance().getType("module"));
}

void llvmpy::PyCodeGen::visit(ClassStmtAST* stmt)
{
    // 获取类名和基类
    const std::string& className = stmt->getClassName();
    const std::vector<std::string>& baseClasses = stmt->getBaseClasses();

    // 创建类对象
    llvm::Function* createClassFunc = getOrCreateExternalFunction(
        "py_create_class",
        llvm::PointerType::get(getContext(), 0),
        {llvm::PointerType::get(llvm::Type::getInt8Ty(getContext()), 0)});

    // 创建类名字符串常量
    llvm::Value* classNameStr = builder->CreateGlobalStringPtr(className, "classname");

    // 调用创建类函数
    llvm::Value* classObj = builder->CreateCall(createClassFunc, {classNameStr}, "class");

    // 如果有基类，添加继承关系
    if (!baseClasses.empty())
    {
        llvm::Function* addBaseClassFunc = getOrCreateExternalFunction(
            "py_add_base_class",
            llvm::Type::getVoidTy(getContext()),
            {
                llvm::PointerType::get(getContext(), 0),  // class object
                llvm::PointerType::get(getContext(), 0)   // base class object
            });

        for (const auto& baseName : baseClasses)
        {
            // 获取基类对象
            llvm::Value* baseClassVar = getVariable(baseName);
            if (!baseClassVar)
            {
                logError("Unknown base class: " + baseName,
                         stmt->line.value_or(-1), stmt->column.value_or(-1));
                continue;
            }

            // 如果变量是指针类型，加载其值
            // 使用新的LLVM不透明指针API
            if (baseClassVar->getType()->isPointerTy())
            {
                // 检查是否是指向指针的指针
                llvm::Type* loadedType = nullptr;

                if (auto allocaInst = llvm::dyn_cast<llvm::AllocaInst>(baseClassVar))
                {
                    loadedType = allocaInst->getAllocatedType();
                    if (loadedType->isPointerTy())
                    {
                        baseClassVar = builder->CreateLoad(
                            llvm::PointerType::get(getContext(), 0), baseClassVar, "baseclass");
                    }
                }
                else if (auto globalVar = llvm::dyn_cast<llvm::GlobalVariable>(baseClassVar))
                {
                    loadedType = globalVar->getValueType();
                    if (loadedType->isPointerTy())
                    {
                        baseClassVar = builder->CreateLoad(
                            llvm::PointerType::get(getContext(), 0), baseClassVar, "baseclass");
                    }
                }
            }

            // 添加基类
            builder->CreateCall(addBaseClassFunc, {classObj, baseClassVar});
        }
    }

    // 创建类作用域
    pushScope();

    // 处理类体内的语句
    for (const auto& bodyStmt : stmt->getBody())
    {
        handleStmt(const_cast<StmtAST*>(bodyStmt.get()));
    }

    // 处理类方法
    for (const auto& method : stmt->getMethods())
    {
        // 生成方法函数
        handleNode(const_cast<FunctionAST*>(method.get()));

        // 获取生成的函数
        std::string methodName = method->getName();
        llvm::Function* methodFunc = getModule()->getFunction(methodName);

        if (methodFunc)
        {
            // 添加方法到类
            llvm::Function* addMethodFunc = getOrCreateExternalFunction(
                "py_add_method",
                llvm::Type::getVoidTy(getContext()),
                {
                    llvm::PointerType::get(getContext(), 0),                         // class object
                    llvm::PointerType::get(llvm::Type::getInt8Ty(getContext()), 0),  // method name
                    llvm::PointerType::get(methodFunc->getType(), 0)                 // method function
                });

            // 创建方法名字符串常量
            llvm::Value* methodNameStr = builder->CreateGlobalStringPtr(methodName, "methname");

            // 获取函数指针
            llvm::Value* methodFuncPtr = builder->CreatePointerCast(
                methodFunc, llvm::PointerType::get(methodFunc->getType(), 0), "methfunc");

            // 添加方法到类
            builder->CreateCall(addMethodFunc, {classObj, methodNameStr, methodFuncPtr});
        }
    }

    // 结束类作用域
    popScope();

    // 将类对象存储到当前作用域
    llvm::Value* varPtr = PyCodeGenHelper::createLocalVariable(
        *this, className, TypeRegistry::getInstance().getType("class"), classObj);

    // 将变量添加到当前作用域
    setVariable(className, varPtr, TypeRegistry::getInstance().getType("class"));
}

void llvmpy::PyCodeGen::visit(FunctionAST* func)
{
    if (!func)
    {
        logError("尝试处理空函数AST");
        return;
    }

    DEBUG_LOG("开始生成函数: " << func->name);

    // 1. 获取函数名和参数
    const std::string& name = func->getName();
    const auto& params = func->getParams();

    // 2. 获取或创建函数类型
    std::vector<ObjectType*> paramTypes;
    for (const auto& param : params)
    {
        ObjectType* paramType = nullptr;
        if (!param.typeName.empty())
        {
            paramType = TypeRegistry::getInstance().getType(param.typeName);
            DEBUG_LOG("参数 '" << param.name << "' 声明类型: " << param.typeName);
        }
        if (!paramType)
        {
            paramType = TypeRegistry::getInstance().getType("any");
            DEBUG_LOG("参数 '" << param.name << "' 未声明类型，使用any类型");
        }
        paramTypes.push_back(paramType);
    }

    // 3. 获取返回类型，优先使用显式声明，其次推断
    ObjectType* returnType = nullptr;
    if (!func->getReturnTypeName().empty())
    {
        returnType = TypeRegistry::getInstance().getType(func->getReturnTypeName());
        DEBUG_LOG("函数 '" << name << "' 声明返回类型: " << func->getReturnTypeName());
    }

    if (!returnType)
    {
        // 尝试推断返回类型
        auto inferredType = func->inferReturnType();
        if (inferredType && inferredType->getObjectType())
        {
            returnType = inferredType->getObjectType();
            DEBUG_LOG("函数 '" << name << "' 推断返回类型: " << returnType->getName());
        }
        else
        {
            // 默认为None类型
            returnType = TypeRegistry::getInstance().getType("none");
            DEBUG_LOG("函数 '" << name << "' 无法推断返回类型，使用默认类型: none");
        }
    }

    // 4. 创建函数类型和LLVM函数类型
    FunctionType* funcType = TypeRegistry::getInstance().getFunctionType(
        returnType, paramTypes);

    std::vector<llvm::Type*> llvmParamTypes;
    for (auto paramType : paramTypes)
    {
        llvm::Type* llvmType = PyCodeGenHelper::getLLVMType(getContext(), paramType);
        llvmParamTypes.push_back(llvmType);
    }

    llvm::Type* returnLLVMType = PyCodeGenHelper::getLLVMType(getContext(), returnType);
    llvm::FunctionType* llvmFuncType = llvm::FunctionType::get(
        returnLLVMType, llvmParamTypes, false);

    // 5. 检查函数是否已存在，或创建新函数
    llvm::Function* llvmFunc = getModule()->getFunction(name);
    if (llvmFunc)
    {
        // 函数已存在，验证类型是否兼容
        if (llvmFunc->getFunctionType() != llvmFuncType)
        {
            logError("函数 '" + name + "' 重定义，类型不匹配",
                     func->line.value_or(-1), func->column.value_or(-1));
            return;
        }
        DEBUG_LOG("函数 '" << name << "' 已经存在，跳过定义");
    }
    else
    {
        // 创建新函数
        llvmFunc = llvm::Function::Create(
            llvmFuncType, llvm::Function::ExternalLinkage, name, getModule());

        DEBUG_LOG("创建新函数 '" << name << "'");

        // 记录到外部函数缓存以避免重复创建
        debugFunctionReuse(name, llvmFunc);
    }

    // 6. 设置参数名称
    unsigned idx = 0;
    for (auto& arg : llvmFunc->args())
    {
        if (idx < params.size())
        {
            arg.setName(params[idx].name);
        }
        idx++;
    }

    // 7. 创建入口基本块
    llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(getContext(), "entry", llvmFunc);
    builder->SetInsertPoint(entryBlock);

    // 8. 保存之前的函数上下文
    llvm::Function* prevFunc = currentFunction;
    ObjectType* prevRetType = currentReturnType;
    llvm::BasicBlock* prevBlock = savedBlock;

    // 9. 设置当前函数上下文
    currentFunction = llvmFunc;
    currentReturnType = returnType;
    savedBlock = builder->GetInsertBlock();

    // 10. 创建函数作用域
    pushScope();

    // 11. 分配参数
    idx = 0;
    for (auto& arg : llvmFunc->args())
    {
        if (idx < params.size())
        {
            const std::string& paramName = params[idx].name;
            ObjectType* paramType = paramTypes[idx];

            // 为参数创建局部变量
            llvm::Value* paramPtr = nullptr;

            // 根据类型决定分配方式
            if (paramType->isReference() || paramType->getCategory() == ObjectType::Container || paramType->getTypeId() >= PY_TYPE_LIST)
            {
                // 引用类型参数
                paramPtr = builder->CreateAlloca(
                    llvm::PointerType::get(getContext(), 0), nullptr, paramName);

                // 存储参数值到分配的内存
                builder->CreateStore(&arg, paramPtr);

                DEBUG_LOG("创建引用类型参数 '" << paramName << "', 类型: " << paramType->getName());
            }
            else
            {
                // 基本类型参数
                llvm::Type* allocaType = nullptr;

                switch (paramType->getTypeId())
                {
                    case PY_TYPE_INT:
                        allocaType = llvm::Type::getInt32Ty(getContext());
                        break;
                    case PY_TYPE_DOUBLE:
                        allocaType = llvm::Type::getDoubleTy(getContext());
                        break;
                    case PY_TYPE_BOOL:
                        allocaType = llvm::Type::getInt1Ty(getContext());
                        break;
                    default:
                        // 未知类型，使用指针
                        allocaType = llvm::PointerType::get(getContext(), 0);
                        break;
                }

                paramPtr = builder->CreateAlloca(allocaType, nullptr, paramName);

                // 存储参数值到分配的内存
                builder->CreateStore(&arg, paramPtr);

                DEBUG_LOG("创建基本类型参数 '" << paramName << "', 类型: " << paramType->getName());
            }

            // 记录到符号表
            if (paramPtr)
            {
                setVariable(paramName, paramPtr, paramType);
            }
            else
            {
                logError("无法为参数 '" + paramName + "' 创建内存空间",
                         func->line.value_or(-1), func->column.value_or(-1));
            }
        }
        idx++;
    }

    // 12. 生成函数体
    for (const auto& stmt : func->getBody())
    {
        if (stmt)
        {
            handleStmt(const_cast<StmtAST*>(stmt.get()));
        }
        else
        {
            DEBUG_LOG("函数体中发现空语句，已跳过");
        }
    }

    // 13. 检查是否缺少终结指令，添加默认返回
    if (!builder->GetInsertBlock()->getTerminator())
    {
        DEBUG_LOG("函数 '" << name << "' 没有明确的返回语句，添加默认返回");

        // 根据返回类型生成默认返回值
        if (returnType->getTypeId() == PY_TYPE_NONE)
        {
            builder->CreateRetVoid();
            DEBUG_LOG("插入默认void返回");
        }
        else
        {
            // 创建默认返回值
            llvm::Value* defaultReturn = createDefaultValue(returnType);

            if (defaultReturn)
            {
                // 为返回值附加类型元数据
                if (returnType->getTypeId() != PY_TYPE_NONE)
                {
                    ObjectLifecycleManager::attachTypeMetadata(defaultReturn, returnType->getTypeId());
                }

                builder->CreateRet(defaultReturn);
                DEBUG_LOG("插入默认返回值，类型: " << returnType->getName()
                                                   << ", 类型ID: " << returnType->getTypeId());
            }
            else
            {
                // 无法创建默认返回值，使用空返回
                builder->CreateRetVoid();
                DEBUG_LOG("无法创建默认返回值，插入void返回");
            }
        }
    }

    // 14. 结束函数作用域
    popScope();

    // 15. 恢复之前的函数上下文
    currentFunction = prevFunc;
    currentReturnType = prevRetType;
    savedBlock = prevBlock;

    // 16. 验证函数
    std::string verifyError;
    llvm::raw_string_ostream errStream(verifyError);
    if (llvm::verifyFunction(*llvmFunc, &errStream))
    {
        logError("函数验证失败: " + name + " - " + verifyError,
                 func->line.value_or(-1), func->column.value_or(-1));

        // 删除无效函数
        llvmFunc->eraseFromParent();
    }
    else
    {
        DEBUG_LOG("函数 '" << name << "' 生成成功");
    }
}

void llvmpy::PyCodeGen::visit(ModuleAST* module)
{
    // 获取模块名
    const std::string& moduleName = module->getModuleName();

    // 设置模块名称
    getModule()->setModuleIdentifier(moduleName);

    // 处理顶层语句
    for (const auto& stmt : module->getTopLevelStmts())
    {
        handleStmt(const_cast<StmtAST*>(stmt.get()));
    }

    // 处理函数定义
    for (const auto& func : module->getFunctions())
    {
        handleNode(const_cast<FunctionAST*>(func.get()));
    }
}

}  // namespace llvmpy