#include "codegen.h"
#include <iostream>
#include <sstream>
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

llvm::Value* PyCodeGen::generateBinaryOperation(char op, llvm::Value* L, llvm::Value* R,
                                                ObjectType* leftType, ObjectType* rightType)
{
    if (!L || !R || !leftType || !rightType)
    {
        return logError("Invalid operands for binary operation");
    }

    // 获取类型ID
    int leftTypeId = leftType->getTypeId();
    int rightTypeId = rightType->getTypeId();

    // 获取类型操作注册表
    auto& registry = TypeOperationRegistry::getInstance();

    // 查找二元操作描述符
    BinaryOpDescriptor* descriptor = registry.getBinaryOpDescriptor(op, leftTypeId, rightTypeId);

    // 如果找不到直接匹配的描述符，尝试找到可行的类型转换路径
    if (!descriptor)
    {
        auto path = registry.findOperablePath(op, leftTypeId, rightTypeId);

        // 如果找到了路径，进行类型转换
        if (path.first != leftTypeId || path.second != rightTypeId)
        {
            // 左操作数转换
            if (path.first != leftTypeId)
            {
                auto convDesc = registry.getTypeConversionDescriptor(leftTypeId, path.first);
                if (convDesc)
                {
                    // 获取目标类型
                    ObjectType* targetType = TypeRegistry::getInstance().getType(std::to_string(path.first));
                    if (targetType)
                    {
                        L = generateTypeConversion(L, leftType, targetType);
                        leftType = targetType;
                        leftTypeId = path.first;
                    }
                }
            }

            // 右操作数转换
            if (path.second != rightTypeId)
            {
                auto convDesc = registry.getTypeConversionDescriptor(rightTypeId, path.second);
                if (convDesc)
                {
                    // 获取目标类型
                    ObjectType* targetType = TypeRegistry::getInstance().getType(std::to_string(path.second));
                    if (targetType)
                    {
                        R = generateTypeConversion(R, rightType, targetType);
                        rightType = targetType;
                        rightTypeId = path.second;
                    }
                }
            }

            // 使用转换后的类型重新查找描述符
            descriptor = registry.getBinaryOpDescriptor(op, leftTypeId, rightTypeId);
        }
    }

    // 特别处理索引操作 - 对解决 "Expected type 5, got 1" 错误很重要
    if (op == '[')
    {
        // 检查目标是否是可索引类型
        if (leftTypeId == PY_TYPE_INT)
        {
            return logTypeError("Cannot index an integer value"), nullptr;
        }

        // 处理列表索引操作
        if (leftTypeId == PY_TYPE_LIST)
        {
            // 检查索引类型是否为整数
            if (rightTypeId != PY_TYPE_INT)
            {
                return logTypeError("List indices must be integers"), nullptr;
            }

            // 获取运行时的索引操作函数
            llvm::Function* getItemFunc = getOrCreateExternalFunction(
                "py_list_get_item",
                llvm::PointerType::get(getContext(), 0),
                {
                    llvm::PointerType::get(getContext(), 0),  // list
                    llvm::Type::getInt32Ty(getContext())      // index
                });

            // 确保索引是整数值
            llvm::Value* indexValue = R;
            if (R->getType()->isPointerTy())
            {
                // 如果是对象指针，需要提取整数值
                llvm::Function* extractIntFunc = getOrCreateExternalFunction(
                    "py_extract_int",
                    llvm::Type::getInt32Ty(getContext()),
                    {llvm::PointerType::get(getContext(), 0)});
                indexValue = builder->CreateCall(extractIntFunc, {R}, "idxtmp");
            }
            else if (!R->getType()->isIntegerTy(32))
            {
                // 如果不是i32类型的整数，进行类型转换
                indexValue = builder->CreateIntCast(
                    R, llvm::Type::getInt32Ty(getContext()), true, "idxtmp");
            }

            // 调用列表索引函数
            return builder->CreateCall(getItemFunc, {L, indexValue}, "listitem");
        }

        // 处理字典索引操作
        else if (leftTypeId == PY_TYPE_DICT)
        {
            llvm::Function* getDictItemFunc = getOrCreateExternalFunction(
                "py_dict_get_item",
                llvm::PointerType::get(getContext(), 0),
                {
                    llvm::PointerType::get(getContext(), 0),  // dict
                    llvm::PointerType::get(getContext(), 0)   // key
                });

            // 调用字典索引函数
            return builder->CreateCall(getDictItemFunc, {L, R}, "dictitem");
        }

        // 处理字符串索引操作
        else if (leftTypeId == PY_TYPE_STRING)
        {
            // 检查索引类型是否为整数
            if (rightTypeId != PY_TYPE_INT)
            {
                return logTypeError("String indices must be integers"), nullptr;
            }

            llvm::Function* getCharFunc = getOrCreateExternalFunction(
                "py_string_get_char",
                llvm::PointerType::get(getContext(), 0),
                {
                    llvm::PointerType::get(getContext(), 0),  // string
                    llvm::Type::getInt32Ty(getContext())      // index
                });

            // 确保索引是整数值
            llvm::Value* indexValue = R;
            if (R->getType()->isPointerTy())
            {
                llvm::Function* extractIntFunc = getOrCreateExternalFunction(
                    "py_extract_int",
                    llvm::Type::getInt32Ty(getContext()),
                    {llvm::PointerType::get(getContext(), 0)});
                indexValue = builder->CreateCall(extractIntFunc, {R}, "idxtmp");
            }
            else if (!R->getType()->isIntegerTy(32))
            {
                indexValue = builder->CreateIntCast(
                    R, llvm::Type::getInt32Ty(getContext()), true, "idxtmp");
            }

            // 调用字符串索引函数
            return builder->CreateCall(getCharFunc, {L, indexValue}, "charitem");
        }

        // 不支持的索引操作类型
        else
        {
            return logTypeError("Type '" + leftType->getName() + "' is not subscriptable"), nullptr;
        }
    }

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
                {llvm::PointerType::get(getContext(), 0),
                 llvm::PointerType::get(getContext(), 0)});

            // 确保操作数是指针类型
            if (!L->getType()->isPointerTy())
            {
                L = ObjectLifecycleManager::createObject(*this, L, leftTypeId);
            }

            if (!R->getType()->isPointerTy())
            {
                R = ObjectLifecycleManager::createObject(*this, R, rightTypeId);
            }

            // 调用运行时函数
            return builder->CreateCall(runtimeFunc, {L, R}, "binop");
        }
        // 如果定义了自定义实现函数
        else if (descriptor->customImpl)
        {
            return descriptor->customImpl(*this, L, R);
        }
    }

    // 针对基本运算符的内置实现
    if (leftTypeId == PY_TYPE_INT && rightTypeId == PY_TYPE_INT)
    {
        // 整数运算
        switch (op)
        {
            case '+':
                return builder->CreateAdd(L, R, "addtmp");
            case '-':
                return builder->CreateSub(L, R, "subtmp");
            case '*':
                return builder->CreateMul(L, R, "multmp");
            case '/':
                return builder->CreateSDiv(L, R, "divtmp");
            case '%':
                return builder->CreateSRem(L, R, "modtmp");
            case '<':
                return builder->CreateICmpSLT(L, R, "cmptmp");
            case '>':
                return builder->CreateICmpSGT(L, R, "cmptmp");
            case '=':
                return builder->CreateICmpEQ(L, R, "cmptmp");
            case '!':
                return builder->CreateICmpNE(L, R, "cmptmp");
            default:
                break;
        }
    }
    else if ((leftTypeId == PY_TYPE_DOUBLE || leftTypeId == PY_TYPE_INT) && (rightTypeId == PY_TYPE_DOUBLE || rightTypeId == PY_TYPE_INT))
    {
        // 浮点数运算 - 需要确保操作数都是浮点数
        if (leftTypeId == PY_TYPE_INT)
        {
            L = builder->CreateSIToFP(L, llvm::Type::getDoubleTy(getContext()), "inttofp");
        }

        if (rightTypeId == PY_TYPE_INT)
        {
            R = builder->CreateSIToFP(R, llvm::Type::getDoubleTy(getContext()), "inttofp");
        }

        switch (op)
        {
            case '+':
                return builder->CreateFAdd(L, R, "addtmp");
            case '-':
                return builder->CreateFSub(L, R, "subtmp");
            case '*':
                return builder->CreateFMul(L, R, "multmp");
            case '/':
                return builder->CreateFDiv(L, R, "divtmp");
            case '<':
                return builder->CreateFCmpOLT(L, R, "cmptmp");
            case '>':
                return builder->CreateFCmpOGT(L, R, "cmptmp");
            case '=':
                return builder->CreateFCmpOEQ(L, R, "cmptmp");
            case '!':
                return builder->CreateFCmpONE(L, R, "cmptmp");
            default:
                break;
        }
    }

    // 如果所有方法都无法处理，报告错误
    return logError("Unsupported binary operation '" + std::string(1, op) + "' between types '" + leftType->getName() + "' and '" + rightType->getName() + "'");
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

llvm::Value* PyCodeGen::generateTypeConversion(llvm::Value* value, ObjectType* fromType, ObjectType* toType)
{
    if (!value || !fromType || !toType)
    {
        return logError("Invalid parameters for type conversion");
    }

    // 如果类型相同，不需要转换
    if (fromType->getTypeId() == toType->getTypeId())
    {
        return value;
    }

    // 获取类型操作注册表
    auto& registry = TypeOperationRegistry::getInstance();

    // 查找类型转换描述符
    TypeConversionDescriptor* descriptor = registry.getTypeConversionDescriptor(
        fromType->getTypeId(), toType->getTypeId());

    // 如果找到了转换描述符，使用它处理转换
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
            if (!value->getType()->isPointerTy())
            {
                value = ObjectLifecycleManager::createObject(*this, value, fromType->getTypeId());
            }

            // 调用运行时函数
            return builder->CreateCall(runtimeFunc, {value}, "typeconv");
        }
        // 如果定义了自定义实现函数
        else if (descriptor->customImpl)
        {
            return descriptor->customImpl(*this, value);
        }
    }

    // 内置的基本类型转换
    int fromTypeId = fromType->getTypeId();
    int toTypeId = toType->getTypeId();

    // 整数到浮点数转换
    if (fromTypeId == PY_TYPE_INT && toTypeId == PY_TYPE_DOUBLE)
    {
        if (value->getType()->isIntegerTy())
        {
            // 直接从LLVM整数类型转换到LLVM浮点类型
            return builder->CreateSIToFP(value, llvm::Type::getDoubleTy(getContext()), "inttofp");
        }
        else if (value->getType()->isPointerTy())
        {
            // 从整数对象提取值，然后转换
            llvm::Function* extractIntFunc = getOrCreateExternalFunction(
                "py_extract_int",
                llvm::Type::getInt32Ty(getContext()),
                {llvm::PointerType::get(getContext(), 0)});

            llvm::Value* intValue = builder->CreateCall(extractIntFunc, {value}, "intval");
            llvm::Value* doubleValue = builder->CreateSIToFP(
                intValue, llvm::Type::getDoubleTy(getContext()), "inttofp");

            // 创建浮点数对象
            return createDoubleObject(doubleValue);
        }
    }
    // 浮点数到整数转换
    else if (fromTypeId == PY_TYPE_DOUBLE && toTypeId == PY_TYPE_INT)
    {
        if (value->getType()->isDoubleTy())
        {
            // 直接从LLVM浮点类型转换到LLVM整数类型
            return builder->CreateFPToSI(value, llvm::Type::getInt32Ty(getContext()), "fptoint");
        }
        else if (value->getType()->isPointerTy())
        {
            // 从浮点数对象提取值，然后转换
            llvm::Function* extractDoubleFunc = getOrCreateExternalFunction(
                "py_extract_double",
                llvm::Type::getDoubleTy(getContext()),
                {llvm::PointerType::get(getContext(), 0)});

            llvm::Value* doubleValue = builder->CreateCall(extractDoubleFunc, {value}, "doubleval");
            llvm::Value* intValue = builder->CreateFPToSI(
                doubleValue, llvm::Type::getInt32Ty(getContext()), "fptoint");

            // 创建整数对象
            return createIntObject(intValue);
        }
    }
    // 整数到布尔值转换
    else if (fromTypeId == PY_TYPE_INT && toTypeId == PY_TYPE_BOOL)
    {
        if (value->getType()->isIntegerTy())
        {
            // 整数不为零则为true
            return builder->CreateICmpNE(
                value, llvm::ConstantInt::get(value->getType(), 0), "inttobool");
        }
        else if (value->getType()->isPointerTy())
        {
            // 从整数对象提取值，然后转换
            llvm::Function* extractIntFunc = getOrCreateExternalFunction(
                "py_extract_int",
                llvm::Type::getInt32Ty(getContext()),
                {llvm::PointerType::get(getContext(), 0)});

            llvm::Value* intValue = builder->CreateCall(extractIntFunc, {value}, "intval");
            llvm::Value* boolValue = builder->CreateICmpNE(
                intValue, llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), 0), "inttobool");

            // 创建布尔对象
            return createBoolObject(boolValue);
        }
    }
    // 浮点数到布尔值转换
    else if (fromTypeId == PY_TYPE_DOUBLE && toTypeId == PY_TYPE_BOOL)
    {
        if (value->getType()->isDoubleTy())
        {
            // 浮点数不为零则为true
            return builder->CreateFCmpONE(
                value, llvm::ConstantFP::get(value->getType(), 0.0), "doubletobool");
        }
        else if (value->getType()->isPointerTy())
        {
            // 从浮点数对象提取值，然后转换
            llvm::Function* extractDoubleFunc = getOrCreateExternalFunction(
                "py_extract_double",
                llvm::Type::getDoubleTy(getContext()),
                {llvm::PointerType::get(getContext(), 0)});

            llvm::Value* doubleValue = builder->CreateCall(extractDoubleFunc, {value}, "doubleval");
            llvm::Value* boolValue = builder->CreateFCmpONE(
                doubleValue, llvm::ConstantFP::get(llvm::Type::getDoubleTy(getContext()), 0.0), "doubletobool");

            // 创建布尔对象
            return createBoolObject(boolValue);
        }
    }

    // 如果没有已知的转换路径，调用通用运行时转换函数
    llvm::Function* convertFunc = getOrCreateExternalFunction(
        "py_convert_type",
        llvm::PointerType::get(getContext(), 0),
        {llvm::PointerType::get(getContext(), 0),
         llvm::Type::getInt32Ty(getContext())});

    // 确保值是对象指针
    if (!value->getType()->isPointerTy())
    {
        value = ObjectLifecycleManager::createObject(*this, value, fromType->getTypeId());
    }

    // 创建目标类型ID常量
    llvm::Value* targetTypeId = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(getContext()), toType->getTypeId());

    // 调用通用转换函数
    return builder->CreateCall(convertFunc, {value, targetTypeId}, "typeconv");
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

int getBaseTypeId(int typeId)
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

void PyCodeGen::visit(NumberExprAST* expr)
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

void PyCodeGen::visit(StringExprAST* expr)
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

void PyCodeGen::visit(BoolExprAST* expr)
{
    bool value = expr->getValue();

    // 创建布尔值
    lastExprValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(getContext()), value);
    lastExprType = PyType::getBool();
}

void PyCodeGen::visit(NoneExprAST* expr)
{
    // 创建None对象
    llvm::Function* getNoneFunc = getOrCreateExternalFunction(
        "py_get_none",
        llvm::PointerType::get(getContext(), 0),
        {});

    lastExprValue = builder->CreateCall(getNoneFunc, {}, "none");
    lastExprType = PyType::getAny();  // None的类型用Any表示
}

void PyCodeGen::visit(VariableExprAST* expr)
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

void PyCodeGen::visit(BinaryExprAST* expr)
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

void PyCodeGen::visit(UnaryExprAST* expr)
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

void PyCodeGen::visit(CallExprAST* expr)
{
    // 获取函数名
    const std::string& callee = expr->getCallee();

    // 获取函数
    llvm::Function* calleeF = getModule()->getFunction(callee);
    if (!calleeF)
    {
        logError("Unknown function referenced: " + callee,
                 expr->line.value_or(-1), expr->column.value_or(-1));
        return;
    }

    // 检查参数数量
    const auto& args = expr->getArgs();
    if (calleeF->arg_size() != args.size())
    {
        logError("Incorrect number of arguments passed: expected " + std::to_string(calleeF->arg_size()) + ", got " + std::to_string(args.size()),
                 expr->line.value_or(-1), expr->column.value_or(-1));
        return;
    }

    // 处理参数并生成函数调用
    std::vector<llvm::Value*> argsValues;

    // 获取函数类型信息
    ObjectType* funcType = nullptr;
    FunctionType* funcTypeObj = nullptr;

    // 尝试从TypeRegistry获取函数类型
    auto& typeRegistry = TypeRegistry::getInstance();
    auto functionName = calleeF->getName().str();
    auto funcTypeIter = typeRegistry.getFunctionType(nullptr, {});  // 临时获取函数类型
    if (funcTypeIter)
    {
        funcType = funcTypeIter;
        funcTypeObj = dynamic_cast<FunctionType*>(funcType);
    }

    // 处理每个参数
    for (unsigned i = 0, e = args.size(); i != e; ++i)
    {
        // 生成参数表达式代码
        llvm::Value* argValue = handleExpr(const_cast<ExprAST*>(args[i].get()));
        if (!argValue) return;  // 处理参数出错

        // 获取参数类型
        auto argTypePtr = args[i]->getType();
        if (!argTypePtr)
        {
            logError("Cannot determine type of argument " + std::to_string(i),
                     args[i]->line.value_or(-1), args[i]->column.value_or(-1));
            return;
        }

        ObjectType* paramType = nullptr;
        if (funcTypeObj && i < funcTypeObj->getParamTypes().size())
        {
            paramType = const_cast<ObjectType*>(funcTypeObj->getParamTypes()[i]);
        }

        // 如果有参数类型信息，检查类型兼容性并进行必要的转换
        if (paramType)
        {
            ObjectType* argType = argTypePtr->getObjectType();
            if (!argType->canAssignTo(paramType))
            {
                // 尝试类型转换
                argValue = generateTypeConversion(argValue, argType, paramType);
                if (!argValue)
                {
                    logTypeError("Cannot convert argument " + std::to_string(i) + " from " + argType->getName() + " to " + paramType->getName(),
                                 args[i]->line.value_or(-1), args[i]->column.value_or(-1));
                    return;
                }
            }

            // 为参数准备对象生命周期管理
            argValue = prepareParameter(argValue, paramType, const_cast<ExprAST*>(args[i].get()));
        }

        argsValues.push_back(argValue);
    }

    // 创建函数调用
    lastExprValue = builder->CreateCall(calleeF, argsValues, "calltmp");

    // 确定返回类型
    if (funcTypeObj)
    {
        // 如果有函数类型信息，使用其返回类型
        ObjectType* returnType = const_cast<ObjectType*>(funcTypeObj->getReturnType());
        lastExprType = std::make_shared<PyType>(returnType);
    }
    else
    {
        // 否则根据LLVM函数的返回类型推断
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
            lastExprType = PyType::getAny();  // 默认指针类型为Any
        }
        else
        {
            lastExprType = PyType::getAny();  // 未知类型默认为Any
        }
    }

    // 管理返回值的生命周期
    if (lastExprType->getObjectType()->isReference())
    {
        // 如果返回值是引用类型，需要管理其生命周期
        if (lastExprValue->getType()->isPointerTy())
        {
            addTempObject(lastExprValue, lastExprType->getObjectType());
        }
    }
}

void PyCodeGen::visit(ListExprAST* expr)
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
void PyCodeGen::visit(IndexExprAST* expr)
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

void PyCodeGen::visit(ExprStmtAST* stmt)
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

void PyCodeGen::visit(ReturnStmtAST* stmt)
{
    // 标记我们正在处理return语句
    inReturnStmt = true;

    // 获取返回值表达式
    auto* expr = static_cast<const ExprAST*>(stmt->getValue());

    // 确保我们在函数内部
    if (!currentFunction)
    {
        logError("Return statement outside of function",
                 stmt->line.value_or(-1), stmt->column.value_or(-1));
        inReturnStmt = false;
        return;
    }

    // 获取函数的返回类型
    if (!currentReturnType)
    {
        logError("Cannot determine function return type",
                 stmt->line.value_or(-1), stmt->column.value_or(-1));
        inReturnStmt = false;
        return;
    }

    // 处理 void 返回
    if (currentReturnType->getTypeId() == PY_TYPE_NONE)
    {
        if (expr)
        {
            logError("Cannot return a value from function with void return type",
                     stmt->line.value_or(-1), stmt->column.value_or(-1));
        }
        builder->CreateRetVoid();
        inReturnStmt = false;
        return;
    }

    // 确保有返回值表达式
    if (!expr)
    {
        logError("Return statement requires an expression for non-void function",
                 stmt->line.value_or(-1), stmt->column.value_or(-1));
        inReturnStmt = false;
        return;
    }

    // 生成返回值代码
    llvm::Value* retValue = handleExpr(const_cast<ExprAST*>(expr));
    if (!retValue)
    {
        inReturnStmt = false;
        return;  // 错误已经被记录
    }

    // 获取表达式类型
    auto exprTypePtr = expr->getType();
    if (!exprTypePtr)
    {
        logError("Cannot determine expression type",
                 expr->line.value_or(-1), expr->column.value_or(-1));
        inReturnStmt = false;
        return;
    }

    // 类型转换，如果需要
    ObjectType* exprType = exprTypePtr->getObjectType();
    if (!exprType->canAssignTo(currentReturnType))
    {
        retValue = generateTypeConversion(retValue, exprType, currentReturnType);
        if (!retValue)
        {
            logTypeError("Cannot convert return value from " + exprType->getName() + " to " + currentReturnType->getName(),
                         expr->line.value_or(-1), expr->column.value_or(-1));
            inReturnStmt = false;
            return;
        }
    }

    // 准备返回值（处理生命周期）
    retValue = prepareReturnValue(retValue, currentReturnType, const_cast<ExprAST*>(expr));

    // 创建返回指令
    builder->CreateRet(retValue);

    // 释放临时对象
    releaseTempObjects();

    // 重置标记
    inReturnStmt = false;
}

void PyCodeGen::visit(IfStmtAST* stmt)
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
void PyCodeGen::debugFunctionReuse(const std::string& name, llvm::Function* func)
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
void PyCodeGen::visit(WhileStmtAST* stmt)
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

void PyCodeGen::visit(PrintStmtAST* stmt)
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
void PyCodeGen::visit(AssignStmtAST* stmt)
{
    // 获取变量名和赋值表达式
    const std::string& name = stmt->getName();
    auto* expr = static_cast<const ExprAST*>(stmt->getValue());

    if (name.empty())
    {
        logError("Assignment has no target variable",
                 stmt->line.value_or(-1), stmt->column.value_or(-1));
        return;
    }

    if (!expr)
    {
        logError("Assignment has no value expression",
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

    ObjectType* exprType = typePtr->getObjectType();

    // 检查变量是否已存在
    llvm::Value* varPtr = getVariable(name);
    bool isNewVariable = !varPtr;

    if (isNewVariable)
    {
        // 创建新变量
        if (exprType->isReference())
        {
            // 为引用类型创建指针变量
            varPtr = PyCodeGenHelper::createLocalVariable(*this, name, exprType, nullptr);
        }
        else
        {
            // 为值类型创建直接存储
            varPtr = PyCodeGenHelper::createLocalVariable(*this, name, exprType, nullptr);
        }
    }
    else
    {
        // 获取变量的类型
        ObjectType* varType = getVariableType(name);

        // 验证类型兼容性
        if (varType && !exprType->canAssignTo(varType))
        {
            logTypeError("Cannot assign value of type '" + exprType->getName() + "' to variable '" + name + "' of type '" + varType->getName() + "'",
                         stmt->line.value_or(-1), stmt->column.value_or(-1));
            return;
        }

        // 如果需要，执行类型转换
        if (varType && exprType->getTypeId() != varType->getTypeId())
        {
            value = generateTypeConversion(value, exprType, varType);
            if (!value) return;  // 转换失败
        }
    }

    // 准备赋值
    if (isNewVariable)
    {
        // 为新变量设置值和类型
        setVariable(name, varPtr, exprType);

        // 将类型信息注册到TypeRegistry，用于类型推断
        TypeRegistry::getInstance().registerSymbolType(name, exprType);
    }
    else
    {
        // 变量已存在，更新符号表中的类型信息
        ObjectType* updatedType = getVariableType(name);
        if (updatedType)
        {
            // 更新TypeRegistry中的类型信息
            TypeRegistry::getInstance().registerSymbolType(name, updatedType);
        }
    }

    // 生成储存指令
    if (exprType->isReference())
    {
        // 对于引用类型，需要存储对象指针
        if (value->getType()->isPointerTy())
        {
            // 执行引用计数管理
            llvm::Function* incRefFunc = getOrCreateExternalFunction(
                "py_incref",
                llvm::Type::getVoidTy(getContext()),
                {llvm::PointerType::get(getContext(), 0)});

            // 增加新值的引用计数
            builder->CreateCall(incRefFunc, {value});

            // 如果变量已经存在，需要减少旧值的引用计数
            if (!isNewVariable)
            {
                llvm::Value* oldValue = builder->CreateLoad(
                    llvm::PointerType::get(getContext(), 0), varPtr, "oldval");

                llvm::Function* decRefFunc = getOrCreateExternalFunction(
                    "py_decref",
                    llvm::Type::getVoidTy(getContext()),
                    {llvm::PointerType::get(getContext(), 0)});

                builder->CreateCall(decRefFunc, {oldValue});
            }

            // 存储新值
            builder->CreateStore(value, varPtr);
        }
        else
        {
            logError("Expected pointer type for reference assignment",
                     stmt->line.value_or(-1), stmt->column.value_or(-1));
            return;
        }
    }
    else
    {
        // 对于值类型，直接存储
        builder->CreateStore(value, varPtr);
    }

    // 清理临时对象
    releaseTempObjects();
}

void PyCodeGen::visit(IndexAssignStmtAST* stmt) {
    // 获取左侧索引表达式和右侧值表达式
    const ExprAST* targetExpr = stmt->getTarget();
    const ExprAST* valueExpr = stmt->getValue();
    
    // 增加调试信息
    std::cerr << "DEBUG: IndexAssignStmt - targetExpr kind: " 
              << (targetExpr ? static_cast<int>(targetExpr->kind()) : -1) << std::endl;
    
    if (!targetExpr || !valueExpr) {
        logError("Invalid index assignment statement", 
                 stmt->line.value_or(-1), stmt->column.value_or(-1));
        return;
    }
    
    // 直接获取目标和索引
    // 索引赋值的目标通常有两种形式：
    // 1. 直接是一个变量，索引在stmt中单独存储
    // 2. 已经是一个IndexExprAST
    
    const ExprAST* index = stmt->getIndex();
    if (index) {
        // 如果索引直接可用，使用它
        performIndexAssignment(targetExpr, index, valueExpr, stmt);
        return;
    }
    
    // 否则，尝试从targetExpr提取目标和索引（如果它是IndexExprAST）
    const IndexExprAST* indexExpr = dynamic_cast<const IndexExprAST*>(targetExpr);
    if (indexExpr) {
        const ExprAST* target = indexExpr->getTarget();
        const ExprAST* index = indexExpr->getIndex();
        
        if (!target || !index) {
            logError("Invalid index expression in assignment", 
                     targetExpr->line.value_or(-1), targetExpr->column.value_or(-1));
            return;
        }
        
        performIndexAssignment(target, index, valueExpr, stmt);
        return;
    }
    
    // 目标既不是有效的IndexExprAST，也没有单独提供index
    std::string typeInfo = "unknown";
    if (targetExpr->getType()) {
        typeInfo = targetExpr->getType()->getObjectType()->getName();
    }
    
    logError("Target of index assignment must be an index expression, got: " + 
            typeInfo + " (kind: " + std::to_string(static_cast<int>(targetExpr->kind())) + ")", 
            targetExpr->line.value_or(-1), targetExpr->column.value_or(-1));
}

void PyCodeGen::performIndexAssignment(const ExprAST* target, const ExprAST* index,
                                       const ExprAST* valueExpr, const StmtAST* stmt)
{
    // 生成目标和索引的代码
    llvm::Value* targetValue = handleExpr(const_cast<ExprAST*>(target));
    if (!targetValue) return;

    llvm::Value* indexValue = handleExpr(const_cast<ExprAST*>(index));
    if (!indexValue) return;

    // 生成右侧表达式的代码
    llvm::Value* value = handleExpr(const_cast<ExprAST*>(valueExpr));
    if (!value) return;

    // 获取类型信息
    auto targetTypePtr = target->getType();
    auto indexTypePtr = index->getType();
    auto valueTypePtr = valueExpr->getType();

    if (!targetTypePtr || !indexTypePtr || !valueTypePtr)
    {
        logError("Cannot determine types for index assignment",
                 stmt->line.value_or(-1), stmt->column.value_or(-1));
        return;
    }

    ObjectType* targetType = targetTypePtr->getObjectType();
    ObjectType* indexType = indexTypePtr->getObjectType();
    ObjectType* valueType = valueTypePtr->getObjectType();

    // 调试输出
    std::cerr << "DEBUG: IndexAssign - Target type: " << targetType->getName()
              << ", TypeID: " << targetType->getTypeId()
              << ", Index type: " << indexType->getName()
              << ", Value type: " << valueType->getName() << std::endl;

    // 检查符号表中是否有更准确的类型信息
    bool hasMoreAccurateType = false;
    std::string varName;

    if (const VariableExprAST* varExpr = dynamic_cast<const VariableExprAST*>(target))
    {
        varName = varExpr->getName();
        ObjectType* symbolType = getVariableType(varName);

        if (symbolType && (symbolType->getTypeId() == PY_TYPE_LIST || TypeFeatureChecker::hasFeature(symbolType, "container") || TypeFeatureChecker::hasFeature(symbolType, "sequence")))
        {
            std::cerr << "DEBUG: Using symbol table type for " << varName
                      << ": " << symbolType->getName()
                      << ", TypeID: " << symbolType->getTypeId() << std::endl;

            targetType = symbolType;  // 使用符号表中的更准确类型
            hasMoreAccurateType = true;
        }
    }

    // 检查目标是否为变量地址，如果是则需要加载实际指针
    if (auto allocaInst = llvm::dyn_cast<llvm::AllocaInst>(targetValue))
    {
        auto allocatedType = allocaInst->getAllocatedType();
        std::string allocTypeStr;
        llvm::raw_string_ostream allocRso(allocTypeStr);
        allocatedType->print(allocRso);

        std::cerr << "DEBUG: Target is AllocaInst, allocated type: " << allocTypeStr << std::endl;

        // 如果分配的类型是指针，加载其中的值
        if (allocatedType->isPointerTy())
        {
            std::cerr << "DEBUG: Loading actual list pointer from variable "
                      << (varName.empty() ? "target" : varName) << std::endl;
            targetValue = builder->CreateLoad(allocatedType, targetValue, "loaded_target_ptr");
        }
    }

    // 验证索引操作
    if (!validateIndexOperation(const_cast<ExprAST*>(target), const_cast<ExprAST*>(index)))
    {
        return;
    }

    // 确保类型一致
    if (hasMoreAccurateType)
    {
        std::cerr << "DEBUG: Applied better type from symbol table for assignment operation" << std::endl;
    }

    // 根据目标类型执行不同的索引赋值操作
    bool isList = (targetType->getTypeId() == PY_TYPE_LIST) || dynamic_cast<ListType*>(targetType);
    bool isDict = (targetType->getTypeId() == PY_TYPE_DICT) || dynamic_cast<DictType*>(targetType);
    bool isString = (targetType->getTypeId() == PY_TYPE_STRING) || (targetType->getName() == "string");

    // 强制应用符号表中的类型信息
    if (hasMoreAccurateType && targetType->getTypeId() == PY_TYPE_LIST)
    {
        isList = true;
        isDict = false;
        isString = false;
    }

    if (isList)
    {
        std::cerr << "DEBUG: Generating list index assignment operation" << std::endl;

        // 验证索引和值类型
        if (indexType->getTypeId() != PY_TYPE_INT)
        {
            logTypeError("List indices must be integers",
                         index->line.value_or(-1), index->column.value_or(-1));
            return;
        }

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
            // 类型转换
            indexIntValue = builder->CreateIntCast(
                indexValue, llvm::Type::getInt32Ty(getContext()), true, "idxtmp");
        }

        // 检查值是否需要转换为对象
        llvm::Value* valueObj = value;
        if (!value->getType()->isPointerTy())
        {
            // 如果不是对象指针，根据值类型创建对象
            // 修改: 直接使用getOrCreateExternalFunction而不是通过ObjectLifecycleManager创建
            llvm::Function* createIntFunc = getOrCreateExternalFunction(
                "py_create_int",  // 关键: 使用固定名称，不添加后缀
                llvm::PointerType::get(getContext(), 0),
                {llvm::Type::getInt32Ty(getContext())});
                
            valueObj = builder->CreateCall(createIntFunc, {value}, "int_obj");
        }

        // 获取列表元素类型并验证值是否兼容
        ObjectType* elemType = nullptr;
        if (auto listType = dynamic_cast<ListType*>(targetType))
        {
            elemType = const_cast<ObjectType*>(listType->getElementType());

            // 验证值类型与元素类型的兼容性
            if (elemType && !valueType->canAssignTo(elemType))
            {
                logTypeError("Cannot assign " + valueType->getName() + " to list of " + elemType->getName(),
                             valueExpr->line.value_or(-1), valueExpr->column.value_or(-1));
                return;
            }
        }

        // 调用列表设置元素函数
        llvm::Function* setItemFunc = getOrCreateExternalFunction(
            "py_list_set_item",
            llvm::Type::getVoidTy(getContext()),
            {
                llvm::PointerType::get(getContext(), 0),  // list
                llvm::Type::getInt32Ty(getContext()),     // index
                llvm::PointerType::get(getContext(), 0)   // value
            });

        builder->CreateCall(setItemFunc, {targetValue, indexIntValue, valueObj});
    }
    else if (isDict)
    {
        std::cerr << "DEBUG: Generating dictionary index assignment operation" << std::endl;

        // 确保键是对象指针
        llvm::Value* keyObj = indexValue;
        if (!indexValue->getType()->isPointerTy())
        {
            keyObj = ObjectLifecycleManager::createObject(*this, indexValue, indexType->getTypeId());
        }

        // 确保值是对象指针
        llvm::Value* valueObj = value;
        if (!value->getType()->isPointerTy())
        {
            valueObj = ObjectLifecycleManager::createObject(*this, value, valueType->getTypeId());
        }

        // 调用字典设置项函数
        llvm::Function* setItemFunc = getOrCreateExternalFunction(
            "py_dict_set_item",
            llvm::Type::getVoidTy(getContext()),
            {
                llvm::PointerType::get(getContext(), 0),  // dict
                llvm::PointerType::get(getContext(), 0),  // key
                llvm::PointerType::get(getContext(), 0)   // value
            });

        builder->CreateCall(setItemFunc, {targetValue, keyObj, valueObj});
    }
    else if (isString)
    {
        // 字符串不支持索引赋值
        logError("String objects do not support item assignment",
                 stmt->line.value_or(-1), stmt->column.value_or(-1));
        return;
    }
    else
    {
        // 不支持的目标类型
        logError("Cannot assign to index of type " + targetType->getName(),
                 stmt->line.value_or(-1), stmt->column.value_or(-1));
        return;
    }

    // 成功执行索引赋值后，可能需要释放临时对象
    // 如果系统支持对象生命周期管理，可以添加临时对象清理代码
}
void PyCodeGen::visit(PassStmtAST* stmt)
{
    // pass语句不做任何事情
}

void PyCodeGen::visit(ImportStmtAST* stmt)
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

void PyCodeGen::visit(ClassStmtAST* stmt)
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

void PyCodeGen::visit(FunctionAST* func)
{
    // 获取函数名和参数
    const std::string& name = func->getName();
    const auto& params = func->getParams();

    // 获取或创建函数类型
    std::vector<ObjectType*> paramTypes;
    for (const auto& param : params)
    {
        ObjectType* paramType = nullptr;
        if (!param.typeName.empty())
        {
            paramType = TypeRegistry::getInstance().getType(param.typeName);
        }
        if (!paramType)
        {
            paramType = TypeRegistry::getInstance().getType("any");
        }
        paramTypes.push_back(paramType);
    }

    // 获取返回类型
    ObjectType* returnType = nullptr;
    if (!func->getReturnTypeName().empty())
    {
        returnType = TypeRegistry::getInstance().getType(func->getReturnTypeName());
    }
    if (!returnType)
    {
        // 如果未指定返回类型，尝试推断
        auto inferredType = func->inferReturnType();
        if (inferredType)
        {
            returnType = inferredType->getObjectType();
        }
        else
        {
            returnType = TypeRegistry::getInstance().getType("none");
        }
    }

    // 创建函数类型
    FunctionType* funcType = TypeRegistry::getInstance().getFunctionType(
        returnType, paramTypes);

    // 创建函数参数类型
    std::vector<llvm::Type*> llvmParamTypes;
    for (auto paramType : paramTypes)
    {
        llvm::Type* llvmType = PyCodeGenHelper::getLLVMType(getContext(), paramType);
        llvmParamTypes.push_back(llvmType);
    }

    // 创建函数类型
    llvm::Type* returnLLVMType = PyCodeGenHelper::getLLVMType(getContext(), returnType);
    llvm::FunctionType* llvmFuncType = llvm::FunctionType::get(
        returnLLVMType, llvmParamTypes, false);

    // 创建函数
    llvm::Function* llvmFunc = llvm::Function::Create(
        llvmFuncType, llvm::Function::ExternalLinkage, name, getModule());

    // 设置参数名称
    unsigned idx = 0;
    for (auto& arg : llvmFunc->args())
    {
        if (idx < params.size())
        {
            arg.setName(params[idx].name);
        }
        idx++;
    }

    // 创建入口基本块
    llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(getContext(), "entry", llvmFunc);
    builder->SetInsertPoint(entryBlock);

    // 保存之前的函数和返回类型
    llvm::Function* prevFunc = currentFunction;
    ObjectType* prevRetType = currentReturnType;
    llvm::BasicBlock* prevBlock = savedBlock;

    // 设置当前函数和返回类型
    currentFunction = llvmFunc;
    currentReturnType = returnType;

    // 创建函数作用域
    pushScope();

    // 分配参数
    idx = 0;
    for (auto& arg : llvmFunc->args())
    {
        if (idx < params.size())
        {
            const std::string& paramName = params[idx].name;
            ObjectType* paramType = paramTypes[idx];

            // 为参数创建局部变量
            llvm::Value* paramPtr = PyCodeGenHelper::createLocalVariable(
                *this, paramName, paramType, &arg);

            // 记录在符号表中
            setVariable(paramName, paramPtr, paramType);
        }
        idx++;
    }

    // 生成函数体
    for (const auto& stmt : func->getBody())
    {
        handleStmt(const_cast<StmtAST*>(stmt.get()));
    }

    // 检查函数是否有返回语句
    if (!builder->GetInsertBlock()->getTerminator())
    {
        // 如果没有返回语句，根据返回类型添加默认返回
        if (returnType->getTypeId() == PY_TYPE_NONE)
        {
            builder->CreateRetVoid();
        }
        else
        {
            // 创建默认返回值
            llvm::Value* defaultReturn = nullptr;

            switch (returnType->getTypeId())
            {
                case PY_TYPE_INT:
                    defaultReturn = llvm::ConstantInt::get(llvm::Type::getInt32Ty(getContext()), 0);
                    break;
                case PY_TYPE_DOUBLE:
                    defaultReturn = llvm::ConstantFP::get(llvm::Type::getDoubleTy(getContext()), 0.0);
                    break;
                case PY_TYPE_BOOL:
                    defaultReturn = llvm::ConstantInt::get(llvm::Type::getInt1Ty(getContext()), 0);
                    break;
                default:
                    // 对于引用类型，返回None
                    if (returnType->isReference())
                    {
                        llvm::Function* getNoneFunc = getOrCreateExternalFunction(
                            "py_get_none",
                            llvm::PointerType::get(getContext(), 0),
                            {});
                        defaultReturn = builder->CreateCall(getNoneFunc, {}, "defretval");
                    }
                    else
                    {
                        defaultReturn = llvm::Constant::getNullValue(returnLLVMType);
                    }
                    break;
            }

            if (defaultReturn)
            {
                builder->CreateRet(defaultReturn);
            }
            else
            {
                // 如果无法创建默认返回值，使用void返回
                builder->CreateRetVoid();
            }
        }
    }

    // 结束函数作用域
    popScope();

    // 恢复之前的函数和返回类型
    currentFunction = prevFunc;
    currentReturnType = prevRetType;
    savedBlock = prevBlock;

    // 验证函数
    if (llvm::verifyFunction(*llvmFunc, &llvm::errs()))
    {
        llvmFunc->eraseFromParent();
        logError("Function verification failed: " + name,
                 func->line.value_or(-1), func->column.value_or(-1));
    }
}

void PyCodeGen::visit(ModuleAST* module)
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