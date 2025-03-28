#include "codegen.h"
#include <iostream>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/Casting.h>:
#include "Debugdefine.h"

using namespace llvm;

namespace llvmpy
{

//===----------------------------------------------------------------------===//
// Type 实现
//===----------------------------------------------------------------------===//

std::shared_ptr<Type> Type::get(TypeKind k)
{
    return std::make_shared<Type>(k);
}

std::shared_ptr<Type> Type::getInt()
{
    return get(Int);
}
std::shared_ptr<Type> Type::getDouble()
{
    return get(Double);
}
std::shared_ptr<Type> Type::getString()
{
    return get(String);
}
std::shared_ptr<Type> Type::getBool()
{
    return get(Bool);
}
std::shared_ptr<Type> Type::getVoid()
{
    return get(Void);
}
std::shared_ptr<Type> Type::getUnknown()
{
    return get(Unknown);
}

llvm::Type* Type::getLLVMType(llvm::LLVMContext& context, TypeKind kind)
{
    switch (kind)
    {
        case Int:
            return llvm::Type::getInt32Ty(context);
        case Double:
            return llvm::Type::getDoubleTy(context);
        case Bool:
            return llvm::Type::getInt1Ty(context);
        case String:
            return llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
        case Void:
            return llvm::Type::getVoidTy(context);
        case Unknown:
            return llvm::Type::getDoubleTy(context);  // 默认为double
    }
    return llvm::Type::getDoubleTy(context);  // 默认为double
}

//===----------------------------------------------------------------------===//
// TypeRegistry 实现
//===----------------------------------------------------------------------===//

std::unordered_map<std::string, std::shared_ptr<Type>> TypeRegistry::typeMap;
bool TypeRegistry::initialized = false;

void TypeRegistry::initialize()
{
    if (initialized) return;

    // 注册基本类型
    registerType("int", Type::getInt());
    registerType("double", Type::getDouble());
    registerType("float", Type::getDouble());  // float 同样映射到 double
    registerType("string", Type::getString());
    registerType("bool", Type::getBool());
    registerType("void", Type::getVoid());

    initialized = true;
}

void TypeRegistry::registerType(const std::string& name, std::shared_ptr<Type> type)
{
    typeMap[name] = type;
}

std::shared_ptr<Type> TypeRegistry::getType(const std::string& name)
{
    ensureInitialized();

    auto it = typeMap.find(name);
    if (it != typeMap.end())
    {
        return it->second;
    }

    // 没找到类型，返回未知类型
    return Type::getUnknown();
}

void TypeRegistry::ensureInitialized()
{
    if (!initialized)
    {
        initialize();
    }
}

//===----------------------------------------------------------------------===//
// CodeGen 静态成员初始化和函数注册
//===----------------------------------------------------------------------===//

std::unordered_map<ASTKind, CodeGen::ASTNodeHandler> CodeGen::nodeHandlers;
std::unordered_map<ASTKind, CodeGen::ExprHandler> CodeGen::exprHandlers;
std::unordered_map<ASTKind, CodeGen::StmtHandler> CodeGen::stmtHandlers;
std::unordered_map<char, std::function<llvm::Value*(CodeGen&, llvm::Value*, llvm::Value*)>> CodeGen::binOpHandlers;

void CodeGen::registerNodeHandler(ASTKind kind, ASTNodeHandler handler)
{
    nodeHandlers[kind] = std::move(handler);
}

void CodeGen::registerExprHandler(ASTKind kind, ExprHandler handler)
{
    exprHandlers[kind] = std::move(handler);
}

void CodeGen::registerStmtHandler(ASTKind kind, StmtHandler handler)
{
    stmtHandlers[kind] = std::move(handler);
}

void CodeGen::registerBinOpHandler(char op, std::function<llvm::Value*(CodeGen&, llvm::Value*, llvm::Value*)> handler)
{
    binOpHandlers[op] = std::move(handler);
}

void CodeGen::initializeRegistries()
{
    // 确保类型注册表已初始化
    TypeRegistry::ensureInitialized();

    // 注册索引赋值语句处理函数
    registerStmtHandler(ASTKind::IndexAssignStmt, [](CodeGen& gen, StmtAST* stmt)
                        { gen.codegenIndexAssignStmt(static_cast<IndexAssignStmtAST*>(stmt)); });
    // 注册索引赋值语句处理函数
    registerStmtHandler(ASTKind::IndexAssignStmt, [](CodeGen& gen, StmtAST* stmt)
                        { gen.codegenIndexAssignStmt(static_cast<IndexAssignStmtAST*>(stmt)); });
    registerExprHandler(ASTKind::ListExpr, [](CodeGen& gen, ExprAST* expr)
                        { return gen.codegenListExpr(static_cast<ListExprAST*>(expr)); });

    registerExprHandler(ASTKind::IndexExpr, [](CodeGen& gen, ExprAST* expr)
                        { return gen.codegenIndexExpr(static_cast<IndexExprAST*>(expr)); });
    // 注册表达式处理函数
    registerExprHandler(ASTKind::ListExpr, [](CodeGen& gen, ExprAST* expr)
                        { return gen.codegenListExpr(static_cast<ListExprAST*>(expr)); });

    registerExprHandler(ASTKind::IndexExpr, [](CodeGen& gen, ExprAST* expr)
                        { return gen.codegenIndexExpr(static_cast<IndexExprAST*>(expr)); });

    registerExprHandler(ASTKind::NumberExpr, [](CodeGen& gen, ExprAST* expr)
                        { return gen.codegenNumberExpr(static_cast<NumberExprAST*>(expr)); });

    registerExprHandler(ASTKind::VariableExpr, [](CodeGen& gen, ExprAST* expr)
                        { return gen.codegenVariableExpr(static_cast<VariableExprAST*>(expr)); });

    registerExprHandler(ASTKind::BinaryExpr, [](CodeGen& gen, ExprAST* expr)
                        { return gen.codegenBinaryExpr(static_cast<BinaryExprAST*>(expr)); });

    registerExprHandler(ASTKind::CallExpr, [](CodeGen& gen, ExprAST* expr)
                        { return gen.codegenCallExpr(static_cast<CallExprAST*>(expr)); });

    registerExprHandler(ASTKind::UnaryExpr, [](CodeGen& gen, ExprAST* expr)
                        { return gen.codegenUnaryExpr(static_cast<UnaryExprAST*>(expr)); });

    // 注册语句处理函数
    registerStmtHandler(ASTKind::ExprStmt, [](CodeGen& gen, StmtAST* stmt)
                        { gen.codegenExprStmt(static_cast<ExprStmtAST*>(stmt)); });

    registerStmtHandler(ASTKind::ReturnStmt, [](CodeGen& gen, StmtAST* stmt)
                        { gen.codegenReturnStmt(static_cast<ReturnStmtAST*>(stmt)); });

    registerStmtHandler(ASTKind::IfStmt, [](CodeGen& gen, StmtAST* stmt)
                        { gen.codegenIfStmt(static_cast<IfStmtAST*>(stmt)); });

    registerStmtHandler(ASTKind::WhileStmt, [](CodeGen& gen, StmtAST* stmt)
                        { gen.codegenWhileStmt(static_cast<WhileStmtAST*>(stmt)); });

    registerStmtHandler(ASTKind::PrintStmt, [](CodeGen& gen, StmtAST* stmt)
                        { gen.codegenPrintStmt(static_cast<PrintStmtAST*>(stmt)); });

    registerStmtHandler(ASTKind::AssignStmt, [](CodeGen& gen, StmtAST* stmt)
                        { gen.codegenAssignStmt(static_cast<AssignStmtAST*>(stmt)); });

    registerStmtHandler(ASTKind::PassStmt, [](CodeGen& gen, StmtAST* stmt)
                        { gen.codegenPassStmt(static_cast<PassStmtAST*>(stmt)); });

    registerStmtHandler(ASTKind::ImportStmt, [](CodeGen& gen, StmtAST* stmt)
                        { gen.codegenImportStmt(static_cast<ImportStmtAST*>(stmt)); });

    registerStmtHandler(ASTKind::ClassStmt, [](CodeGen& gen, StmtAST* stmt)
                        { gen.codegenClassStmt(static_cast<ClassStmtAST*>(stmt)); });

    // 注册节点处理函数
    registerNodeHandler(ASTKind::Function, [](CodeGen& gen, ASTNode* node)
                        { return gen.codegenFunction(static_cast<FunctionAST*>(node)); });

    registerNodeHandler(ASTKind::Module, [](CodeGen& gen, ASTNode* node)
                        {
        gen.generateModule(static_cast<ModuleAST*>(node));
        return nullptr; });

    // 注册二元操作符处理函数
    registerBinOpHandler('+', [](CodeGen& gen, Value* L, Value* R)
                         { return gen.getBuilder().CreateFAdd(L, R, "addtmp"); });

    registerBinOpHandler('-', [](CodeGen& gen, Value* L, Value* R)
                         { return gen.getBuilder().CreateFSub(L, R, "subtmp"); });

    registerBinOpHandler('*', [](CodeGen& gen, Value* L, Value* R)
                         { return gen.getBuilder().CreateFMul(L, R, "multmp"); });

    registerBinOpHandler('/', [](CodeGen& gen, Value* L, Value* R)
                         { return gen.getBuilder().CreateFDiv(L, R, "divtmp"); });

    registerBinOpHandler('<', [](CodeGen& gen, Value* L, Value* R)
                         {
        Value* cmp = gen.getBuilder().CreateFCmpULT(L, R, "cmptmp");
        return gen.getBuilder().CreateUIToFP(cmp, Type::getLLVMType(gen.getContext(), Type::Double), "booltmp"); });

    registerBinOpHandler('>', [](CodeGen& gen, Value* L, Value* R)
                         {
        Value* cmp = gen.getBuilder().CreateFCmpUGT(L, R, "cmptmp");
        return gen.getBuilder().CreateUIToFP(cmp, Type::getLLVMType(gen.getContext(), Type::Double), "booltmp"); });

    registerBinOpHandler('e', [](CodeGen& gen, Value* L, Value* R) {  // ==
        Value* cmp = gen.getBuilder().CreateFCmpUEQ(L, R, "cmptmp");
        return gen.getBuilder().CreateUIToFP(cmp, Type::getLLVMType(gen.getContext(), Type::Double), "booltmp");
    });

    registerBinOpHandler('n', [](CodeGen& gen, Value* L, Value* R) {  // !=
        Value* cmp = gen.getBuilder().CreateFCmpUNE(L, R, "cmptmp");
        return gen.getBuilder().CreateUIToFP(cmp, Type::getLLVMType(gen.getContext(), Type::Double), "booltmp");
    });

    // 注册比较操作符
    registerBinOpHandler('l', [](CodeGen& gen, Value* L, Value* R) {  // <=
        Value* cmp = gen.getBuilder().CreateFCmpULE(L, R, "cmptmp");
        return gen.getBuilder().CreateUIToFP(cmp, Type::getLLVMType(gen.getContext(), Type::Double), "booltmp");
    });

    registerBinOpHandler('g', [](CodeGen& gen, Value* L, Value* R) {  // >=
        Value* cmp = gen.getBuilder().CreateFCmpUGE(L, R, "cmptmp");
        return gen.getBuilder().CreateUIToFP(cmp, Type::getLLVMType(gen.getContext(), Type::Double), "booltmp");
    });

    // 添加其他操作符，如乘方、整除等
    registerBinOpHandler('^', [](CodeGen& gen, Value* L, Value* R) {  // 乘方
        // 使用llvm.pow.f64内建函数
        Function* powFunc = Intrinsic::getDeclaration(
            gen.getModule(),
            Intrinsic::pow,
            Type::getLLVMType(gen.getContext(), Type::Double));
        std::vector<Value*> args = {L, R};
        return gen.getBuilder().CreateCall(powFunc, args, "powtmp");
    });

    registerBinOpHandler('%', [](CodeGen& gen, Value* L, Value* R) {  // 取模
        return gen.getBuilder().CreateFRem(L, R, "modtmp");
    });
}

void CodeGen::ensureInitialized()
{
    static bool initialized = false;
    if (!initialized)
    {
        initializeRegistries();
        initialized = true;
    }
}

//===----------------------------------------------------------------------===//
// CodeGen 构造函数和基础方法
//===----------------------------------------------------------------------===//

CodeGen::CodeGen()
{
    context = std::make_unique<LLVMContext>();
    module = std::make_unique<Module>("llvmpy", *context);
    builder = std::make_unique<IRBuilder<>>(*context);
    currentFunction = nullptr;
    currentReturnType = nullptr;
    lastExprValue = nullptr;

    // 确保注册表已初始化
    ensureInitialized();
}

Value* CodeGen::logErrorV(const std::string& message)
{
    std::cerr << "[CodeGen Error] " << message << std::endl;
    return nullptr;
}

llvm::Type* CodeGen::getTypeFromString(const std::string& typeStr)
{
    auto type = TypeRegistry::getType(typeStr);
    return getLLVMType(type);
}

llvm::Type* CodeGen::getLLVMType(std::shared_ptr<Type> type)
{
    return Type::getLLVMType(*context, type->kind);
}

Value* CodeGen::createStringConstant(const std::string& value)
{
    return builder->CreateGlobalStringPtr(value, "str");
}

Function* CodeGen::getPrintfFunction()
{
    Function* printfFunc = module->getFunction("printf");
    if (!printfFunc)
    {
        // 创建printf函数类型：int printf(char*, ...)
        std::vector<llvm::Type*> argTypes;
        argTypes.push_back(Type::getLLVMType(*context, Type::String));
        FunctionType* printfType = FunctionType::get(
            Type::getLLVMType(*context, Type::Int),
            argTypes,
            true  // 可变参数
        );

        // 声明printf函数
        printfFunc = Function::Create(
            printfType,
            Function::ExternalLinkage,
            "printf",
            module.get());
    }
    return printfFunc;
}

Function* CodeGen::getMallocFunction()
{
    Function* mallocFunc = module->getFunction("malloc");
    if (!mallocFunc)
    {
        // 创建malloc函数类型：void* malloc(size_t)
        std::vector<llvm::Type*> argTypes;
        argTypes.push_back(Type::getLLVMType(*context, Type::Int));
        FunctionType* mallocType = FunctionType::get(
            Type::getLLVMType(*context, Type::String),  // 作为void*
            argTypes,
            false);

        // 声明malloc函数
        mallocFunc = Function::Create(
            mallocType,
            Function::ExternalLinkage,
            "malloc",
            module.get());
    }
    return mallocFunc;
}

Function* CodeGen::getFreeFunction()
{
    Function* freeFunc = module->getFunction("free");
    if (!freeFunc)
    {
        // 创建free函数类型：void free(void*)
        std::vector<llvm::Type*> argTypes;
        argTypes.push_back(Type::getLLVMType(*context, Type::String));  // 作为void*
        FunctionType* freeType = FunctionType::get(
            Type::getLLVMType(*context, Type::Void),
            argTypes,
            false);

        // 声明free函数
        freeFunc = Function::Create(
            freeType,
            Function::ExternalLinkage,
            "free",
            module.get());
    }
    return freeFunc;
}

//===----------------------------------------------------------------------===//
// AST节点处理调度
//===----------------------------------------------------------------------===//

llvm::Value* CodeGen::handleNode(ASTNode* node)
{
    if (!node) return nullptr;

    auto it = nodeHandlers.find(node->kind());
    if (it != nodeHandlers.end())
    {
        return it->second(*this, node);
    }

    logErrorV("No handler registered for AST node kind: " + std::to_string(static_cast<int>(node->kind())));
    return nullptr;
}

llvm::Value* CodeGen::handleExpr(ExprAST* expr)
{
    if (!expr) return nullptr;

    std::cout << "Handling expression of kind: " << static_cast<int>(expr->kind()) << std::endl;

    auto it = exprHandlers.find(expr->kind());
    if (it != exprHandlers.end())
    {
        lastExprValue = it->second(*this, expr);
        return lastExprValue;
    }

    logErrorV("No handler registered for expression kind: " + std::to_string(static_cast<int>(expr->kind())));
    return nullptr;
}

void CodeGen::handleStmt(StmtAST* stmt)
{
    if (!stmt) return;

    auto it = stmtHandlers.find(stmt->kind());
    if (it != stmtHandlers.end())
    {
        it->second(*this, stmt);
        return;
    }

    logErrorV("No handler registered for statement kind: " + std::to_string(static_cast<int>(stmt->kind())));
}

//===----------------------------------------------------------------------===//
// 表达式和语句代码生成函数
//===----------------------------------------------------------------------===//

llvm::Value* CodeGen::codegenExpr(const ExprAST* expr)
{
    if (!expr) return nullptr;
    return handleExpr(const_cast<ExprAST*>(expr));
}

void CodeGen::codegenStmt(const StmtAST* stmt)
{
    if (!stmt) return;
    handleStmt(const_cast<StmtAST*>(stmt));
}

void CodeGen::generateModule(ModuleAST* moduleAst)
{
    // 先生成所有函数定义
    for (auto& func : moduleAst->getFunctions())
    {
        codegenFunction(func.get());
    }

    // 如果有顶层语句，创建主函数包含它们
    if (!moduleAst->getTopLevelStmts().empty())
    {
        // 创建主函数
        FunctionType* mainType = FunctionType::get(
            Type::getLLVMType(*context, Type::Int),
            false);
        Function* mainFunc = Function::Create(
            mainType,
            Function::ExternalLinkage,
            "main",
            module.get());

        // 创建入口块
        BasicBlock* entryBB = BasicBlock::Create(*context, "entry", mainFunc);
        builder->SetInsertPoint(entryBB);

        // 保存当前函数上下文
        auto savedFunc = currentFunction;
        auto savedRetTy = currentReturnType;

        // 设置当前函数为main
        currentFunction = mainFunc;
        currentReturnType = Type::getLLVMType(*context, Type::Int);

        // 生成所有顶层语句的代码
        for (auto& stmt : moduleAst->getTopLevelStmts())
        {
            codegenStmt(stmt.get());
        }

        // 如果最后一个基本块没有终止符，添加返回语句
        if (!builder->GetInsertBlock()->getTerminator())
        {
            builder->CreateRet(ConstantInt::get(Type::getLLVMType(*context, Type::Int), 0));
        }

        // 验证函数
        verifyFunction(*mainFunc);

        // 恢复函数上下文
        currentFunction = savedFunc;
        currentReturnType = savedRetTy;
    }
}

llvm::Value* CodeGen::codegenNumberExpr(const NumberExprAST* expr)
{
    return ConstantFP::get(*context, APFloat(expr->getValue()));
}

// 修复变量表达式代码生成
// 修复变量表达式代码生成
llvm::Value* CodeGen::codegenVariableExpr(const VariableExprAST* expr)
{
    // 查找变量
    auto it = namedValues.find(expr->getName());
    if (it == namedValues.end())
    {
        return logErrorV("Unknown variable name: " + expr->getName());
    }

    // 获取变量地址
    llvm::Value* varPtr = it->second;

    // 检查变量类型，如果是列表结构体，直接返回地址不要加载
    llvm::Type* varType = varPtr->getType();

    // 确保是指针类型
    if (!varType->isPointerTy())
    {
        return logErrorV("Variable is not a pointer type");
    }

    // 定义列表结构体类型 (与codegenListExpr和codegenIndexExpr中使用的相同)
    llvm::StructType* listStructType = llvm::StructType::get(*context,
                                                             {Type::getLLVMType(*context, Type::Int),
                                                              llvm::PointerType::get(Type::getLLVMType(*context, Type::Double), 0)});

    // 使用启发式方法：尝试获取变量在创建时使用的结构布局
    // 如果变量的分配操作是为了存储listStructType，则认为它是一个列表
    if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(varPtr))
    {
        if (allocaInst->getAllocatedType() == listStructType)
        {
            // 列表是结构体类型，返回其地址
            return varPtr;
        }
    }

    // 正常变量，加载并返回值
    return builder->CreateLoad(Type::getLLVMType(*context, Type::Double), varPtr, expr->getName());
}

llvm::Value* CodeGen::codegenBinaryExpr(const BinaryExprAST* expr)
{
    // 生成左右操作数
    Value* L = codegenExpr(expr->getLHS());
    Value* R = codegenExpr(expr->getRHS());
    if (!L || !R) return nullptr;

    // 查找并调用操作符处理函数
    auto it = binOpHandlers.find(expr->getOp());
    if (it != binOpHandlers.end())
    {
        return it->second(*this, L, R);
    }

    return logErrorV("Invalid binary operator: " + std::string(1, expr->getOp()));
}

llvm::Value* CodeGen::codegenCallExpr(const CallExprAST* expr)
{
    // 查找函数
    Function* callee = module->getFunction(expr->getCallee());
    if (!callee)
    {
        return logErrorV("Unknown function referenced: " + expr->getCallee());
    }

    // 检查参数个数
    if (callee->arg_size() != expr->getArgs().size())
    {
        return logErrorV("Incorrect number of arguments passed");
    }

    // 生成所有参数
    std::vector<Value*> args;
    for (const auto& arg : expr->getArgs())
    {
        Value* argValue = codegenExpr(arg.get());
        if (!argValue) return nullptr;
        args.push_back(argValue);
    }

    // 创建函数调用
    return builder->CreateCall(callee, args, "calltmp");
}

llvm::Value* CodeGen::codegenUnaryExpr(const UnaryExprAST* expr)
{
    // 生成操作数
    Value* operand = codegenExpr(expr->getOperand());
    if (!operand) return nullptr;

    // 目前只处理负号
    if (expr->getOpCode() == '-')
    {
        return builder->CreateFNeg(operand, "negtmp");
    }

    return logErrorV("Invalid unary operator");
}

void CodeGen::codegenExprStmt(const ExprStmtAST* stmt)
{
    // 简单地生成表达式，但忽略其值
    codegenExpr(stmt->getExpr());
}

void CodeGen::codegenReturnStmt(const ReturnStmtAST* stmt)
{
    // 生成返回值
    Value* returnValue = nullptr;

    if (stmt->getValue())
    {
        returnValue = codegenExpr(stmt->getValue());
        if (!returnValue) return;
    }

    // 创建返回指令
    if (currentReturnType->isVoidTy())
    {
        builder->CreateRetVoid();
    }
    else
    {
        builder->CreateRet(returnValue);
    }
}

// 在codegenIfStmt函数中修改
void CodeGen::codegenIfStmt(const IfStmtAST* stmt)
{
    // 生成条件表达式
    Value* condValue = codegenExpr(stmt->getCondition());
    if (!condValue) return;

    // 将条件表达式转换为布尔值
    condValue = builder->CreateFCmpONE(
        condValue,
        ConstantFP::get(*context, APFloat(0.0)),
        "ifcond");

    // 创建所需的基本块
    Function* theFunction = builder->GetInsertBlock()->getParent();
    BasicBlock* thenBB = BasicBlock::Create(*context, "then", theFunction);
    BasicBlock* elseBB = BasicBlock::Create(*context, "else");
    BasicBlock* mergeBB = BasicBlock::Create(*context, "ifcont");

    // 创建条件分支
    builder->CreateCondBr(condValue, thenBB, elseBB);

    // 生成then分支代码
    builder->SetInsertPoint(thenBB);
    for (const auto& thenStmt : stmt->getThenBody())
    {
        codegenStmt(thenStmt.get());
    }

    // 如果没有终止语句，添加跳转到合并块
    if (!builder->GetInsertBlock()->getTerminator())
    {
        builder->CreateBr(mergeBB);
    }

    // 添加else块到函数 - 修改为使用插入方式
    elseBB->insertInto(theFunction);
    builder->SetInsertPoint(elseBB);

    // 生成else分支代码
    for (const auto& elseStmt : stmt->getElseBody())
    {
        codegenStmt(elseStmt.get());
    }

    // 如果没有终止语句，添加跳转到合并块
    if (!builder->GetInsertBlock()->getTerminator())
    {
        builder->CreateBr(mergeBB);
    }

    // 添加合并块到函数 - 修改为使用插入方式
    mergeBB->insertInto(theFunction);
    builder->SetInsertPoint(mergeBB);
}

void CodeGen::codegenWhileStmt(const WhileStmtAST* stmt)
{
    // 创建所需的基本块
    Function* theFunction = builder->GetInsertBlock()->getParent();
    BasicBlock* condBB = BasicBlock::Create(*context, "whilecond", theFunction);
    BasicBlock* loopBB = BasicBlock::Create(*context, "whilebody");
    BasicBlock* afterBB = BasicBlock::Create(*context, "whileend");

    // 跳转到条件块
    builder->CreateBr(condBB);

    // 条件块
    builder->SetInsertPoint(condBB);
    Value* condValue = codegenExpr(stmt->getCondition());
    if (!condValue) return;

    // 将条件表达式转换为布尔值
    condValue = builder->CreateFCmpONE(
        condValue,
        ConstantFP::get(*context, APFloat(0.0)),
        "whilecond");

    // 条件分支
    builder->CreateCondBr(condValue, loopBB, afterBB);

    // 循环体 - 修改为使用插入方式
    loopBB->insertInto(theFunction);
    builder->SetInsertPoint(loopBB);

    // 生成循环体代码
    for (const auto& bodyStmt : stmt->getBody())
    {
        codegenStmt(bodyStmt.get());
    }

    // 循环回条件块
    if (!builder->GetInsertBlock()->getTerminator())
    {
        builder->CreateBr(condBB);
    }

    // 循环后代码 - 修改为使用插入方式
    afterBB->insertInto(theFunction);
    builder->SetInsertPoint(afterBB);
}

void CodeGen::codegenPrintStmt(const PrintStmtAST* stmt)
{
    // 生成打印的表达式值
    Value* value = codegenExpr(stmt->getValue());
    if (!value) return;

    // 获取printf函数
    Function* printfFunc = getPrintfFunction();

    // 创建格式字符串（假设所有值都是double）
    Value* formatStr = createStringConstant("%f\n");

    // 创建printf调用
    std::vector<Value*> args;
    args.push_back(formatStr);
    args.push_back(value);
    builder->CreateCall(printfFunc, args);
}

void CodeGen::codegenAssignStmt(const AssignStmtAST* stmt)
{
    // 生成赋值表达式
    llvm::Value* value = codegenExpr(stmt->getValue());
    if (!value) return;

    // 确定变量的类型
    llvm::Type* valueType = value->getType();

    // 查找或创建变量
    llvm::Value* variable = nullptr;
    if (namedValues.find(stmt->getName()) == namedValues.end())
    {
        // 新变量 - 在当前函数的入口处创建alloca
        IRBuilder<> tmpBuilder(&currentFunction->getEntryBlock(),
                               currentFunction->getEntryBlock().begin());

        // 如果是列表，保持指针类型，否则使用Double类型
        llvm::Type* allocType = valueType->isPointerTy() ? valueType : Type::getLLVMType(*context, Type::Double);

        variable = tmpBuilder.CreateAlloca(allocType, nullptr, stmt->getName());
        namedValues[stmt->getName()] = variable;
    }
    else
    {
        variable = namedValues[stmt->getName()];
    }

    // 存储值到变量
    builder->CreateStore(value, variable);
}

void CodeGen::codegenPassStmt(const PassStmtAST*)
{
    // pass语句不生成任何代码
}

void CodeGen::codegenImportStmt(const ImportStmtAST* stmt)
{
    // 暂时不处理导入语句，以后可以实现模块加载功能
    logErrorV("Import statements are not yet implemented: " + stmt->getModuleName());
}

void CodeGen::codegenClassStmt(const ClassStmtAST* stmt)
{
    // 暂时不处理类定义，以后可以实现面向对象功能
    logErrorV("Class definitions are not yet implemented: " + stmt->getClassName());
}

llvm::Function* CodeGen::codegenFunction(const FunctionAST* func)
{
    // 检查是否已存在同名函数
    Function* function = module->getFunction(func->getName());
    if (function)
    {
        // 函数已存在，返回已有的函数
        return function;
    }

    // 创建函数参数类型
    std::vector<llvm::Type*> argTypes;
    for (const auto& param : func->getParams())
    {
        // 如果参数有类型注解，使用该类型，否则默认为double
        llvm::Type* paramType = param.type.empty() ? Type::getLLVMType(*context, Type::Double) : getTypeFromString(param.type);
        argTypes.push_back(paramType);
    }

    // 确定返回类型
    llvm::Type* returnType = func->getReturnType().empty() ? Type::getLLVMType(*context, Type::Double) : getTypeFromString(func->getReturnType());

    // 创建函数类型
    FunctionType* functionType = FunctionType::get(returnType, argTypes, false);

    // 创建函数
    function = Function::Create(
        functionType,
        Function::ExternalLinkage,
        func->getName(),
        module.get());

    // 设置参数名称
    unsigned idx = 0;
    for (auto& arg : function->args())
    {
        arg.setName(func->getParams()[idx++].name);
    }

    // 创建入口基本块
    BasicBlock* entryBB = BasicBlock::Create(*context, "entry", function);
    builder->SetInsertPoint(entryBB);

    // 保存旧的函数上下文
    Function* savedFunction = currentFunction;
    llvm::Type* savedReturnType = currentReturnType;

    // 设置新的函数上下文
    currentFunction = function;
    currentReturnType = returnType;

    // 清空当前变量表
    namedValues.clear();

    // 为参数创建栈变量
    for (auto& arg : function->args())
    {
        // 为参数创建alloca
        AllocaInst* alloca = builder->CreateAlloca(arg.getType(), nullptr, arg.getName());
        // 继续实现codegenFunction函数
        // 存储参数值到分配的栈空间
        builder->CreateStore(&arg, alloca);

        // 记录变量名到值的映射
        namedValues[std::string(arg.getName())] = alloca;
    }

    // 生成函数体
    for (const auto& stmt : func->getBody())
    {
        codegenStmt(stmt.get());

        // 如果语句是终止指令（如返回语句），后面的代码不再生成
        if (builder->GetInsertBlock()->getTerminator())
            break;
    }

    // 如果函数没有明确的返回语句且不是void类型，添加默认返回值
    if (!builder->GetInsertBlock()->getTerminator())
    {
        if (returnType->isVoidTy())
        {
            builder->CreateRetVoid();
        }
        else if (returnType->isDoubleTy())
        {
            builder->CreateRet(ConstantFP::get(*context, APFloat(0.0)));
        }
        else if (returnType->isIntegerTy())
        {
            builder->CreateRet(ConstantInt::get(returnType, 0));
        }
        else
        {
            // 其他类型的默认返回值
            builder->CreateRet(Constant::getNullValue(returnType));
        }
    }

    // 验证函数
    verifyFunction(*function);

    // 恢复函数上下文
    currentFunction = savedFunction;
    currentReturnType = savedReturnType;

    return function;
}

// 实现列表字面量的代码生成
llvm::Value* CodeGen::codegenListExpr(const ListExprAST* expr)
{
    const auto& elements = expr->getElements();
    size_t numElements = elements.size();

    // 创建列表结构体类型
    llvm::StructType* listType = llvm::StructType::get(*context,
                                                       {Type::getLLVMType(*context, Type::Int),
                                                        llvm::PointerType::get(Type::getLLVMType(*context, Type::Double), 0)});

    // 为列表结构分配栈空间
    llvm::Value* listStruct = builder->CreateAlloca(listType, nullptr, "list");

    // 存储长度
    llvm::Value* lengthPtr = builder->CreateStructGEP(listType, listStruct, 0, "length_ptr");
    builder->CreateStore(llvm::ConstantInt::get(Type::getLLVMType(*context, Type::Int), numElements),
                         lengthPtr);

    // 分配数据数组空间
    llvm::Value* mallocSize = llvm::ConstantInt::get(Type::getLLVMType(*context, Type::Int),
                                                     numElements * sizeof(double));
    llvm::Function* mallocFunc = getMallocFunction();
    llvm::Value* dataPtr = builder->CreateCall(mallocFunc, {mallocSize}, "list_data");
    dataPtr = builder->CreateBitCast(dataPtr,
                                     llvm::PointerType::get(Type::getLLVMType(*context, Type::Double), 0),
                                     "list_data_ptr");

    // 存储数据指针
    llvm::Value* dataPtrPtr = builder->CreateStructGEP(listType, listStruct, 1, "data_ptr_ptr");
    builder->CreateStore(dataPtr, dataPtrPtr);

    // 填充列表元素
    for (size_t i = 0; i < numElements; ++i)
    {
        // 生成元素值
        llvm::Value* elemValue = codegenExpr(elements[i].get());
        if (!elemValue) continue;

        // 计算元素地址
        llvm::Value* elemPtr = builder->CreateGEP(Type::getLLVMType(*context, Type::Double),
                                                  dataPtr,
                                                  llvm::ConstantInt::get(Type::getLLVMType(*context, Type::Int), i),
                                                  "elem_ptr_" + std::to_string(i));

        // 存储元素值
        builder->CreateStore(elemValue, elemPtr);
    }

    return listStruct;
}

// 实现索引操作的代码生成
// 完全重写索引操作的代码生成函数
// 修复索引表达式代码生成
llvm::Value* CodeGen::codegenIndexExpr(const IndexExprAST* expr)
{
    // 获取列表变量
    llvm::Value* listPtr = codegenExpr(expr->getTarget());
    if (!listPtr) return nullptr;

    // 获取目标名称
    std::string targetName = "unknown";
    if (auto* varExpr = dynamic_cast<const VariableExprAST*>(expr->getTarget()))
    {
        targetName = varExpr->getName();
    }

    // 处理可能的非指针类型情况（如直接返回的值）
    llvm::Type* ptrType = listPtr->getType();
    if (!ptrType->isPointerTy())
    {
        // 如果是变量表达式，尝试直接获取变量地址
        if (auto* varExpr = dynamic_cast<const VariableExprAST*>(expr->getTarget()))
        {
            auto it = namedValues.find(varExpr->getName());
            if (it != namedValues.end())
            {
                listPtr = it->second;  // 直接使用变量地址
            }
            else
            {
                return logErrorV("Unknown variable name: " + targetName);
            }
        }
        else
        {
            return logErrorV("List indexing requires a pointer type for: " + targetName);
        }
    }

    // 生成索引值
    llvm::Value* indexValue = codegenExpr(expr->getIndex());
    if (!indexValue) return nullptr;

    // 确保索引是整数类型
    if (!indexValue->getType()->isIntegerTy())
    {
        indexValue = builder->CreateFPToSI(indexValue, Type::getLLVMType(*context, Type::Int), "idx_as_int");
    }

    // 获取元素地址
    llvm::Value* elemPtr = getElementAddress(listPtr, indexValue, targetName);
    if (!elemPtr)
    {
        return logErrorV("Invalid list indexing operation on: " + targetName);
    }

    // 加载元素值（因为这是右值上下文）
    return builder->CreateLoad(Type::getLLVMType(*context, Type::Double), elemPtr, "elem_value");
}

llvm::Value* CodeGen::getElementAddress(llvm::Value* listPtr, llvm::Value* indexValue, const std::string& targetName)
{
    // 定义列表结构体类型
    llvm::StructType* listStructType = llvm::StructType::get(*context,
                                                             {Type::getLLVMType(*context, Type::Int),
                                                              llvm::PointerType::get(Type::getLLVMType(*context, Type::Double), 0)});

    // 处理不同指针类型和层级
    llvm::Type* ptrType = listPtr->getType();

    // 处理非指针类型情况
    if (!ptrType->isPointerTy())
    {
        return nullptr;  // 调用者应检查返回值
    }

    // 处理AllocaInst（本地变量）情况
    if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(listPtr))
    {
        llvm::Type* allocatedType = allocaInst->getAllocatedType();

        // 如果变量直接存储列表结构
        if (allocatedType == listStructType)
        {
            // 不加载，保持列表地址
        }
        // 如果变量存储的是指向列表的指针
        else if (allocatedType->isPointerTy())
        {
            listPtr = builder->CreateLoad(allocatedType, listPtr, targetName + "_ptr");
        }
        else
        {
            return nullptr;  // 不支持的类型
        }
    }

    // 进行边界检查
    llvm::Function* function = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* checkBB = llvm::BasicBlock::Create(*context, "bounds_check", function);
    llvm::BasicBlock* validBB = llvm::BasicBlock::Create(*context, "in_bounds", function);
    llvm::BasicBlock* errorBB = llvm::BasicBlock::Create(*context, "out_of_bounds", function);

    builder->CreateBr(checkBB);
    builder->SetInsertPoint(checkBB);

    // 获取列表长度
    llvm::Value* lengthPtr = builder->CreateStructGEP(listStructType, listPtr, 0, "length_ptr");
    llvm::Value* length = builder->CreateLoad(Type::getLLVMType(*context, Type::Int), lengthPtr, "length");

    // 检查索引是否越界
    llvm::Value* isNegative = builder->CreateICmpSLT(indexValue,
                                                     llvm::ConstantInt::get(Type::getLLVMType(*context, Type::Int), 0),
                                                     "is_negative");
    llvm::Value* isTooLarge = builder->CreateICmpSGE(indexValue, length, "is_too_large");
    llvm::Value* isOutOfBounds = builder->CreateOr(isNegative, isTooLarge, "is_out_of_bounds");

    builder->CreateCondBr(isOutOfBounds, errorBB, validBB);

    // 错误处理块
    builder->SetInsertPoint(errorBB);
    llvm::Function* printfFunc = getPrintfFunction();
    llvm::Value* formatStr = createStringConstant("IndexError: list index out of range\n");
    builder->CreateCall(printfFunc, {formatStr});
    // 使错误情况继续执行但使用默认下标0，避免程序崩溃
    builder->CreateBr(validBB);

    // 有效索引块
    builder->SetInsertPoint(validBB);

    // 更安全的处理：在出错情况下使用0索引
    llvm::PHINode* safeIndex = builder->CreatePHI(Type::getLLVMType(*context, Type::Int), 2, "safe_index");
    safeIndex->addIncoming(llvm::ConstantInt::get(Type::getLLVMType(*context, Type::Int), 0), errorBB);
    safeIndex->addIncoming(indexValue, checkBB);

    // 获取数据指针
    llvm::Value* dataPtrPtr = builder->CreateStructGEP(listStructType, listPtr, 1, "data_ptr_ptr");
    llvm::Value* dataPtr = builder->CreateLoad(llvm::PointerType::get(Type::getLLVMType(*context, Type::Double), 0),
                                               dataPtrPtr, "data_ptr");

    // 获取元素地址
    return builder->CreateGEP(Type::getLLVMType(*context, Type::Double),
                              dataPtr,
                              safeIndex,
                              targetName + "_elem_ptr");
}

// 添加处理索引赋值的函数
void CodeGen::codegenIndexAssignStmt(const IndexAssignStmtAST* stmt)
{
    // 获取目标列表的指针
    llvm::Value* listPtr = codegenExpr(stmt->getTarget());
    if (!listPtr) return;

    // 获取目标名称
    std::string targetName = "unknown";
    if (auto* varExpr = dynamic_cast<const VariableExprAST*>(stmt->getTarget()))
    {
        targetName = varExpr->getName();
    }

    // 处理可能的非指针类型情况
    llvm::Type* ptrType = listPtr->getType();
    if (!ptrType->isPointerTy())
    {
        // 如果是变量表达式，尝试直接获取变量地址
        if (auto* varExpr = dynamic_cast<const VariableExprAST*>(stmt->getTarget()))
        {
            auto it = namedValues.find(varExpr->getName());
            if (it != namedValues.end())
            {
                listPtr = it->second;  // 直接使用变量地址
            }
            else
            {
                logErrorV("Unknown variable name: " + targetName);
                return;
            }
        }
        else
        {
            logErrorV("List indexing requires a pointer type for: " + targetName);
            return;
        }
    }

    // 生成索引值
    llvm::Value* indexValue = codegenExpr(stmt->getIndex());
    if (!indexValue) return;

    // 确保索引是整数类型
    if (!indexValue->getType()->isIntegerTy())
    {
        indexValue = builder->CreateFPToSI(indexValue, Type::getLLVMType(*context, Type::Int), "idx_as_int");
    }

    // 生成要赋的值
    llvm::Value* value = codegenExpr(stmt->getValue());
    if (!value) return;

    // 获取元素地址
    llvm::Value* elemPtr = getElementAddress(listPtr, indexValue, targetName);
    if (!elemPtr)
    {
        logErrorV("Invalid list indexing operation on: " + targetName);
        return;
    }

    // 存储值到元素地址
    builder->CreateStore(value, elemPtr);
}

// 实现访问者模式接口
void CodeGen::visit(ListExprAST* expr)
{
    lastExprValue = codegenListExpr(expr);
}

void CodeGen::visit(IndexExprAST* expr)
{
    lastExprValue = codegenIndexExpr(expr);
}
//===----------------------------------------------------------------------===//
// Visitor模式接口实现
//===----------------------------------------------------------------------===//

void CodeGen::visit(NumberExprAST* expr)
{
    lastExprValue = codegenNumberExpr(expr);
}

void CodeGen::visit(VariableExprAST* expr)
{
    lastExprValue = codegenVariableExpr(expr);
}

void CodeGen::visit(BinaryExprAST* expr)
{
    lastExprValue = codegenBinaryExpr(expr);
}

void CodeGen::visit(CallExprAST* expr)
{
    lastExprValue = codegenCallExpr(expr);
}

void CodeGen::visit(UnaryExprAST* expr)
{
    lastExprValue = codegenUnaryExpr(expr);
}

void CodeGen::visit(ExprStmtAST* stmt)
{
    codegenExprStmt(stmt);
}

void CodeGen::visit(ReturnStmtAST* stmt)
{
    codegenReturnStmt(stmt);
}

void CodeGen::visit(IfStmtAST* stmt)
{
    codegenIfStmt(stmt);
}


void CodeGen::visit(IndexAssignStmtAST* stmt) {
    codegenIndexAssignStmt(stmt);
}
void CodeGen::visit(WhileStmtAST* stmt)
{
    codegenWhileStmt(stmt);
}

void CodeGen::visit(PrintStmtAST* stmt)
{
    codegenPrintStmt(stmt);
}

void CodeGen::visit(AssignStmtAST* stmt)
{
    codegenAssignStmt(stmt);
}

void CodeGen::visit(PassStmtAST* stmt)
{
    codegenPassStmt(stmt);
}

void CodeGen::visit(ImportStmtAST* stmt)
{
    codegenImportStmt(stmt);
}

void CodeGen::visit(ClassStmtAST* stmt)
{
    codegenClassStmt(stmt);
}

void CodeGen::visit(FunctionAST* func)
{
    codegenFunction(func);
}

void CodeGen::visit(ModuleAST* module)
{
    generateModule(module);
}

}  // namespace llvmpy
