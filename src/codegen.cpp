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
    ss << "Code generation error";
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
    exprHandlers.registerHandler(ASTKind::NumberExpr, [](PyCodeGen& gen, ExprAST* expr)
                                 {
        auto* numExpr = static_cast<NumberExprAST*>(expr);
        gen.visit(numExpr);
        return gen.getLastExprValue(); });

    exprHandlers.registerHandler(ASTKind::VariableExpr, [](PyCodeGen& gen, ExprAST* expr)
                                 {
        auto* varExpr = static_cast<VariableExprAST*>(expr);
        gen.visit(varExpr);
        return gen.getLastExprValue(); });

    exprHandlers.registerHandler(ASTKind::BinaryExpr, [](PyCodeGen& gen, ExprAST* expr)
                                 {
        auto* binExpr = static_cast<BinaryExprAST*>(expr);
        gen.visit(binExpr);
        return gen.getLastExprValue(); });

    exprHandlers.registerHandler(ASTKind::CallExpr, [](PyCodeGen& gen, ExprAST* expr)
                                 {
        auto* callExpr = static_cast<CallExprAST*>(expr);
        gen.visit(callExpr);
        return gen.getLastExprValue(); });

    exprHandlers.registerHandler(ASTKind::UnaryExpr, [](PyCodeGen& gen, ExprAST* expr)
                                 {
        auto* unaryExpr = static_cast<UnaryExprAST*>(expr);
        gen.visit(unaryExpr);
        return gen.getLastExprValue(); });

    exprHandlers.registerHandler(ASTKind::StringExpr, [](PyCodeGen& gen, ExprAST* expr)
                                 {
        auto* strExpr = static_cast<StringExprAST*>(expr);
        gen.visit(strExpr);
        return gen.getLastExprValue(); });

    exprHandlers.registerHandler(ASTKind::BoolExpr, [](PyCodeGen& gen, ExprAST* expr)
                                 {
        auto* boolExpr = static_cast<BoolExprAST*>(expr);
        gen.visit(boolExpr);
        return gen.getLastExprValue(); });

    exprHandlers.registerHandler(ASTKind::NoneExpr, [](PyCodeGen& gen, ExprAST* expr)
                                 {
        auto* noneExpr = static_cast<NoneExprAST*>(expr);
        gen.visit(noneExpr);
        return gen.getLastExprValue(); });

    exprHandlers.registerHandler(ASTKind::ListExpr, [](PyCodeGen& gen, ExprAST* expr)
                                 {
        auto* listExpr = static_cast<ListExprAST*>(expr);
        gen.visit(listExpr);
        return gen.getLastExprValue(); });

    exprHandlers.registerHandler(ASTKind::IndexExpr, [](PyCodeGen& gen, ExprAST* expr)
                                 {
        auto* indexExpr = static_cast<IndexExprAST*>(expr);
        gen.visit(indexExpr);
        return gen.getLastExprValue(); });

    // 注册语句处理器
    stmtHandlers.registerHandler(ASTKind::ExprStmt, [](PyCodeGen& gen, StmtAST* stmt)
                                 {
        auto* exprStmt = static_cast<ExprStmtAST*>(stmt);
        gen.visit(exprStmt); });

    stmtHandlers.registerHandler(ASTKind::ReturnStmt, [](PyCodeGen& gen, StmtAST* stmt)
                                 {
    auto* returnStmt = static_cast<ReturnStmtAST*>(stmt);
    gen.visit(returnStmt); });

    // 继续注册处理器
    stmtHandlers.registerHandler(ASTKind::IfStmt, [](PyCodeGen& gen, StmtAST* stmt)
                                 {
    auto* ifStmt = static_cast<IfStmtAST*>(stmt);
    gen.visit(ifStmt); });

    stmtHandlers.registerHandler(ASTKind::WhileStmt, [](PyCodeGen& gen, StmtAST* stmt)
                                 {
    auto* whileStmt = static_cast<WhileStmtAST*>(stmt);
    gen.visit(whileStmt); });

    stmtHandlers.registerHandler(ASTKind::PrintStmt, [](PyCodeGen& gen, StmtAST* stmt)
                                 {
    auto* printStmt = static_cast<PrintStmtAST*>(stmt);
    gen.visit(printStmt); });

    stmtHandlers.registerHandler(ASTKind::AssignStmt, [](PyCodeGen& gen, StmtAST* stmt)
                                 {
    auto* assignStmt = static_cast<AssignStmtAST*>(stmt);
    gen.visit(assignStmt); });

    stmtHandlers.registerHandler(ASTKind::PassStmt, [](PyCodeGen& gen, StmtAST* stmt)
                                 {
    auto* passStmt = static_cast<PassStmtAST*>(stmt);
    gen.visit(passStmt); });

    stmtHandlers.registerHandler(ASTKind::ImportStmt, [](PyCodeGen& gen, StmtAST* stmt)
                                 {
    auto* importStmt = static_cast<ImportStmtAST*>(stmt);
    gen.visit(importStmt); });

    stmtHandlers.registerHandler(ASTKind::ClassStmt, [](PyCodeGen& gen, StmtAST* stmt)
                                 {
    auto* classStmt = static_cast<ClassStmtAST*>(stmt);
    gen.visit(classStmt); });

    stmtHandlers.registerHandler(ASTKind::IndexAssignStmt, [](PyCodeGen& gen, StmtAST* stmt)
                                 {
    auto* indexAssignStmt = static_cast<IndexAssignStmtAST*>(stmt);
    gen.visit(indexAssignStmt); });

    // 注册二元操作符处理器
    binOpHandlers.registerHandler('+', [](PyCodeGen& gen, llvm::Value* L, llvm::Value* R, char op)->llvm::Value* {
        auto& builder = gen.getBuilder();
        auto* lTy = L->getType();
        auto* rTy = R->getType();
        
        // 处理指针与整数相加 - 需要检查变量类型
        if (lTy->isPointerTy() && rTy->isIntegerTy()) {
            // 获取类型信息，通常存储在符号表中
            auto lastExprType = gen.getLastExprType();
            if (lastExprType && lastExprType->getObjectType() && 
                lastExprType->getObjectType()->getName() == "int") {
                // 如果是整数指针，加载整数值
                L = builder.CreateLoad(builder.getInt32Ty(), L, "loadtmp");
                // 执行整数加法
                llvm::Value* addResult = builder.CreateAdd(L, R, "addtmp");
                
                // 检查函数返回类型是否为引用类型（需要包装为对象）
                ObjectType* returnType = gen.getCurrentReturnType();
                if (returnType && returnType->isReference()) {
                    // 如果需要返回对象指针，创建整数对象
                    llvm::Function* createIntFunc = gen.getOrCreateExternalFunction(
                        "py_create_int",
                        llvm::PointerType::get(gen.getContext(), 0),
                        {llvm::Type::getInt32Ty(gen.getContext())}
                    );
                    return builder.CreateCall(createIntFunc, {addResult}, "int_obj");
                }
                
                // 否则直接返回整数结果
                return addResult;
            }
            
            // 如果是容器类型，执行指针算术（数组索引）
            if (lastExprType && TypeFeatureChecker::isContainer(lastExprType->getObjectType())) {
                return builder.CreateGEP(
                    builder.getInt8Ty(),  // 使用i8作为基本类型
                    L,
                    R,
                    "ptridx"
                );
            }
        }
        
        // 处理两个指针（如两个变量）
        if (lTy->isPointerTy() && rTy->isPointerTy()) {
            // 尝试判断两个指针的内容类型
            auto lExprType = gen.getLastExprType();
            
            if (lExprType && lExprType->getObjectType() && 
                lExprType->getObjectType()->getName() == "int") {
                // 加载两个整数并相加
                L = builder.CreateLoad(builder.getInt32Ty(), L, "loadtmp_l");
                R = builder.CreateLoad(builder.getInt32Ty(), R, "loadtmp_r");
                llvm::Value* addResult = builder.CreateAdd(L, R, "addtmp");
                
                // 检查返回类型
                ObjectType* returnType = gen.getCurrentReturnType();
                if (returnType && returnType->isReference()) {
                    // 如果需要返回对象指针，创建整数对象
                    llvm::Function* createIntFunc = gen.getOrCreateExternalFunction(
                        "py_create_int",
                        llvm::PointerType::get(gen.getContext(), 0),
                        {llvm::Type::getInt32Ty(gen.getContext())}
                    );
                    return builder.CreateCall(createIntFunc, {addResult}, "int_obj");
                }
                
                // 否则直接返回整数结果
                return addResult;
            }
            
            // 对于非基本类型的指针，使用运行时对象加法
            llvm::Function* addFunc = gen.getOrCreateExternalFunction(
                "py_object_add",
                llvm::PointerType::get(gen.getContext(), 0),
                {
                    llvm::PointerType::get(gen.getContext(), 0),
                    llvm::PointerType::get(gen.getContext(), 0)
                }
            );
            
            return builder.CreateCall(addFunc, {L, R}, "objaddtmp");
        }
        
        // 标准情况的处理（两个整数相加）
        if (lTy->isIntegerTy() && rTy->isIntegerTy()) {
            llvm::Value* addResult = builder.CreateAdd(L, R, "addtmp");
            
            // 检查返回类型
            ObjectType* returnType = gen.getCurrentReturnType();
            if (returnType && returnType->isReference()) {
                // 如果需要返回对象指针，创建整数对象
                llvm::Function* createIntFunc = gen.getOrCreateExternalFunction(
                    "py_create_int",
                    llvm::PointerType::get(gen.getContext(), 0),
                    {llvm::Type::getInt32Ty(gen.getContext())}
                );
                return builder.CreateCall(createIntFunc, {addResult}, "int_obj");
            }
            
            // 否则直接返回整数结果
            return addResult;
        }
        
        // 浮点数相加
        if (lTy->isDoubleTy() && rTy->isDoubleTy()) {
            return builder.CreateFAdd(L, R, "addtmp");
        }
        
        // 整数和浮点数相加（转换整数为浮点数）
        if (lTy->isIntegerTy() && rTy->isDoubleTy()) {
            L = builder.CreateSIToFP(L, rTy, "conv");
            return builder.CreateFAdd(L, R, "addtmp");
        }
        
        // 浮点数和整数相加（转换整数为浮点数）
        if (lTy->isDoubleTy() && rTy->isIntegerTy()) {
            R = builder.CreateSIToFP(R, lTy, "conv");
            return builder.CreateFAdd(L, R, "addtmp");
        }
        
        // 处理特殊情况：整数与指针的加法 - 仅用于真正需要的场景
        if (rTy->isPointerTy() && lTy->isIntegerTy()) {
            // 获取类型信息
            auto lastExprType = gen.getLastExprType();
            if (lastExprType && lastExprType->getObjectType() && 
                lastExprType->getObjectType()->getName() == "int") {
                // 如果是整数指针，加载整数值
                R = builder.CreateLoad(builder.getInt32Ty(), R, "loadtmp");
                llvm::Value* addResult = builder.CreateAdd(L, R, "addtmp");
                
                // 检查返回类型
                ObjectType* returnType = gen.getCurrentReturnType();
                if (returnType && returnType->isReference()) {
                    llvm::Function* createIntFunc = gen.getOrCreateExternalFunction(
                        "py_create_int",
                        llvm::PointerType::get(gen.getContext(), 0),
                        {llvm::Type::getInt32Ty(gen.getContext())}
                    );
                    return builder.CreateCall(createIntFunc, {addResult}, "int_obj");
                }
                
                return addResult;
            }
            
            // 如果是容器类型，执行指针算术（数组索引）
            if (lastExprType && TypeFeatureChecker::isContainer(lastExprType->getObjectType())) {
                return builder.CreateGEP(
                    builder.getInt8Ty(),
                    R,
                    L,
                    "ptridx"
                );
            }
        }
        
        // 对象加法 - 使用运行时函数
        if (lTy->isPointerTy() || rTy->isPointerTy()) {
            // 将非指针类型转换为指针对象
            if (!lTy->isPointerTy()) {
                llvm::Function* createIntFunc = gen.getOrCreateExternalFunction(
                    "py_create_int",
                    llvm::PointerType::get(gen.getContext(), 0),
                    {llvm::Type::getInt32Ty(gen.getContext())}
                );
                L = builder.CreateCall(createIntFunc, {L}, "int_obj");
            }
            
            if (!rTy->isPointerTy()) {
                llvm::Function* createIntFunc = gen.getOrCreateExternalFunction(
                    "py_create_int",
                    llvm::PointerType::get(gen.getContext(), 0),
                    {llvm::Type::getInt32Ty(gen.getContext())}
                );
                R = builder.CreateCall(createIntFunc, {R}, "int_obj");
            }
            
            // 使用运行时函数处理对象加法
            llvm::Function* addFunc = gen.getOrCreateExternalFunction(
                "py_object_add",
                llvm::PointerType::get(gen.getContext(), 0),
                {
                    llvm::PointerType::get(gen.getContext(), 0),
                    llvm::PointerType::get(gen.getContext(), 0)
                }
            );
            
            return builder.CreateCall(addFunc, {L, R}, "objaddtmp");
        }
        
        // 兜底错误处理
        std::cerr << "警告: 无法处理的加法操作数类型" << std::endl;
        return gen.logError("Invalid operand types for addition");
    });

    binOpHandlers.registerHandler('-', [](PyCodeGen& gen, llvm::Value* L, llvm::Value* R, char op)
                                  {
    auto& builder = gen.getBuilder();
    auto* lTy = L->getType();
    auto* rTy = R->getType();
    
    if (lTy->isIntegerTy() && rTy->isIntegerTy()) {
        return builder.CreateSub(L, R, "subtmp");
    }
    
    if (lTy->isDoubleTy() && rTy->isDoubleTy()) {
        return builder.CreateFSub(L, R, "subtmp");
    }
    
    if (lTy->isIntegerTy() && rTy->isDoubleTy()) {
        L = builder.CreateSIToFP(L, rTy, "conv");
        return builder.CreateFSub(L, R, "subtmp");
    }
    
    if (lTy->isDoubleTy() && rTy->isIntegerTy()) {
        R = builder.CreateSIToFP(R, lTy, "conv");
        return builder.CreateFSub(L, R, "subtmp");
    }
    
    return gen.logError("Invalid operand types for subtraction"); });

    binOpHandlers.registerHandler('*', [](PyCodeGen& gen, llvm::Value* L, llvm::Value* R, char op)
                                  {
    auto& builder = gen.getBuilder();
    auto* lTy = L->getType();
    auto* rTy = R->getType();
    
    if (lTy->isIntegerTy() && rTy->isIntegerTy()) {
        return builder.CreateMul(L, R, "multmp");
    }
    
    if (lTy->isDoubleTy() && rTy->isDoubleTy()) {
        return builder.CreateFMul(L, R, "multmp");
    }
    
    if (lTy->isIntegerTy() && rTy->isDoubleTy()) {
        L = builder.CreateSIToFP(L, rTy, "conv");
        return builder.CreateFMul(L, R, "multmp");
    }
    
    if (lTy->isDoubleTy() && rTy->isIntegerTy()) {
        R = builder.CreateSIToFP(R, lTy, "conv");
        return builder.CreateFMul(L, R, "multmp");
    }
    
    return gen.logError("Invalid operand types for multiplication"); });

    binOpHandlers.registerHandler('/', [](PyCodeGen& gen, llvm::Value* L, llvm::Value* R, char op)
                                  {
    auto& builder = gen.getBuilder();
    auto* lTy = L->getType();
    auto* rTy = R->getType();
    
    // Python中的除法总是返回浮点数（真除法）
    if (lTy->isIntegerTy()) {
        L = builder.CreateSIToFP(L, llvm::Type::getDoubleTy(gen.getContext()), "conv");
    }
    
    if (rTy->isIntegerTy()) {
        R = builder.CreateSIToFP(R, llvm::Type::getDoubleTy(gen.getContext()), "conv");
    }
    
    return builder.CreateFDiv(L, R, "divtmp"); });

    binOpHandlers.registerHandler('<', [](PyCodeGen& gen, llvm::Value* L, llvm::Value* R, char op)
                                  {
    auto& builder = gen.getBuilder();
    auto* lTy = L->getType();
    auto* rTy = R->getType();
    
    if (lTy->isIntegerTy() && rTy->isIntegerTy()) {
        return builder.CreateICmpSLT(L, R, "cmptmp");
    }
    
    if (lTy->isDoubleTy() && rTy->isDoubleTy()) {
        return builder.CreateFCmpOLT(L, R, "cmptmp");
    }
    
    if (lTy->isIntegerTy() && rTy->isDoubleTy()) {
        L = builder.CreateSIToFP(L, rTy, "conv");
        return builder.CreateFCmpOLT(L, R, "cmptmp");
    }
    
    if (lTy->isDoubleTy() && rTy->isIntegerTy()) {
        R = builder.CreateSIToFP(R, lTy, "conv");
        return builder.CreateFCmpOLT(L, R, "cmptmp");
    }
    
    return gen.logError("Invalid operand types for comparison"); });

    // 注册其他操作符处理器...
// 添加大于运算符
binOpHandlers.registerHandler('>', [](PyCodeGen& gen, llvm::Value* L, llvm::Value* R, char op)
{
    auto& builder = gen.getBuilder();
    auto* lTy = L->getType();
    auto* rTy = R->getType();
    
    if (lTy->isIntegerTy() && rTy->isIntegerTy()) {
        return builder.CreateICmpSGT(L, R, "cmptmp");
    }
    
    if (lTy->isDoubleTy() && rTy->isDoubleTy()) {
        return builder.CreateFCmpOGT(L, R, "cmptmp");
    }
    
    if (lTy->isIntegerTy() && rTy->isDoubleTy()) {
        L = builder.CreateSIToFP(L, rTy, "conv");
        return builder.CreateFCmpOGT(L, R, "cmptmp");
    }
    
    if (lTy->isDoubleTy() && rTy->isIntegerTy()) {
        R = builder.CreateSIToFP(R, lTy, "conv");
        return builder.CreateFCmpOGT(L, R, "cmptmp");
    }
    
    return gen.logError("Invalid operand types for comparison");
});

// 添加小于等于运算符
binOpHandlers.registerHandler('l', [](PyCodeGen& gen, llvm::Value* L, llvm::Value* R, char op)
{
    auto& builder = gen.getBuilder();
    auto* lTy = L->getType();
    auto* rTy = R->getType();
    
    if (lTy->isIntegerTy() && rTy->isIntegerTy()) {
        return builder.CreateICmpSLE(L, R, "cmptmp");
    }
    
    if (lTy->isDoubleTy() && rTy->isDoubleTy()) {
        return builder.CreateFCmpOLE(L, R, "cmptmp");
    }
    
    if (lTy->isIntegerTy() && rTy->isDoubleTy()) {
        L = builder.CreateSIToFP(L, rTy, "conv");
        return builder.CreateFCmpOLE(L, R, "cmptmp");
    }
    
    if (lTy->isDoubleTy() && rTy->isIntegerTy()) {
        R = builder.CreateSIToFP(R, lTy, "conv");
        return builder.CreateFCmpOLE(L, R, "cmptmp");
    }
    
    return gen.logError("Invalid operand types for comparison");
});

// 添加大于等于运算符
binOpHandlers.registerHandler('g', [](PyCodeGen& gen, llvm::Value* L, llvm::Value* R, char op)
{
    auto& builder = gen.getBuilder();
    auto* lTy = L->getType();
    auto* rTy = R->getType();
    
    if (lTy->isIntegerTy() && rTy->isIntegerTy()) {
        return builder.CreateICmpSGE(L, R, "cmptmp");
    }
    
    if (lTy->isDoubleTy() && rTy->isDoubleTy()) {
        return builder.CreateFCmpOGE(L, R, "cmptmp");
    }
    
    if (lTy->isIntegerTy() && rTy->isDoubleTy()) {
        L = builder.CreateSIToFP(L, rTy, "conv");
        return builder.CreateFCmpOGE(L, R, "cmptmp");
    }
    
    if (lTy->isDoubleTy() && rTy->isIntegerTy()) {
        R = builder.CreateSIToFP(R, lTy, "conv");
        return builder.CreateFCmpOGE(L, R, "cmptmp");
    }
    
    return gen.logError("Invalid operand types for comparison");
});

// 添加等于运算符
binOpHandlers.registerHandler('e', [](PyCodeGen& gen, llvm::Value* L, llvm::Value* R, char op)-> llvm::Value*
{
    auto& builder = gen.getBuilder();
    auto* lTy = L->getType();
    auto* rTy = R->getType();
    
    if (lTy->isIntegerTy() && rTy->isIntegerTy()) {
        return builder.CreateICmpEQ(L, R, "cmptmp");
    }
    
    if (lTy->isDoubleTy() && rTy->isDoubleTy()) {
        return builder.CreateFCmpOEQ(L, R, "cmptmp");
    }
    
    if (lTy->isIntegerTy() && rTy->isDoubleTy()) {
        L = builder.CreateSIToFP(L, rTy, "conv");
        return builder.CreateFCmpOEQ(L, R, "cmptmp");
    }
    
    if (lTy->isDoubleTy() && rTy->isIntegerTy()) {
        R = builder.CreateSIToFP(R, lTy, "conv");
        return builder.CreateFCmpOEQ(L, R, "cmptmp");
    }
    
    if (lTy->isPointerTy() && rTy->isPointerTy()) {
        // 对象比较，需要调用运行时函数
        llvm::Function* compareFunc = gen.getOrCreateExternalFunction(
            "py_object_equals",
            llvm::Type::getInt1Ty(gen.getContext()),
            {llvm::PointerType::get(gen.getContext(), 0), 
             llvm::PointerType::get(gen.getContext(), 0)});
        
        return gen.getBuilder().CreateCall(compareFunc, {L, R}, "obj_equals");
    }
    
    return gen.logError("Invalid operand types for equality comparison");
});

// 添加不等运算符
binOpHandlers.registerHandler('n', [](PyCodeGen& gen, llvm::Value* L, llvm::Value* R, char op)-> llvm::Value*
{
    auto& builder = gen.getBuilder();
    auto* lTy = L->getType();
    auto* rTy = R->getType();
    
    if (lTy->isIntegerTy() && rTy->isIntegerTy()) {
        return builder.CreateICmpNE(L, R, "cmptmp");
    }
    
    if (lTy->isDoubleTy() && rTy->isDoubleTy()) {
        return builder.CreateFCmpONE(L, R, "cmptmp");
    }
    
    if (lTy->isIntegerTy() && rTy->isDoubleTy()) {
        L = builder.CreateSIToFP(L, rTy, "conv");
        return builder.CreateFCmpONE(L, R, "cmptmp");
    }
    
    if (lTy->isDoubleTy() && rTy->isIntegerTy()) {
        R = builder.CreateSIToFP(R, lTy, "conv");
        return builder.CreateFCmpONE(L, R, "cmptmp");
    }
    
    if (lTy->isPointerTy() && rTy->isPointerTy()) {
        // 对象比较，需要调用运行时函数
        llvm::Function* compareFunc = gen.getOrCreateExternalFunction(
            "py_object_not_equals",
            llvm::Type::getInt1Ty(gen.getContext()),
            {llvm::PointerType::get(gen.getContext(), 0), 
             llvm::PointerType::get(gen.getContext(), 0)});
        
        return gen.getBuilder().CreateCall(compareFunc, {L, R}, "obj_not_equals");
    }
    
    return gen.logError("Invalid operand types for inequality comparison");
});
    // 添加对函数和模块的处理
    nodeHandlers.registerHandler(ASTKind::Function, [](PyCodeGen& gen, ASTNode* node) {
        auto* func = static_cast<FunctionAST*>(node);
        gen.visit(func);
        return nullptr;  // 函数定义不产生值
    });
    
    nodeHandlers.registerHandler(ASTKind::Module, [](PyCodeGen& gen, ASTNode* node) {
        auto* module = static_cast<ModuleAST*>(node);
        gen.visit(module);
        return nullptr;  // 模块不产生值
    });
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
      lastExprType(nullptr), // 添加这行，初始化为nullptr
      inReturnStmt(false),
      savedBlock(nullptr)
      {
        // 确保注册表已初始化
        ensureInitialized();
        
        // 确保类型系统已初始化
        TypeRegistry::getInstance().ensureBasicTypesRegistered();
        TypeFeatureChecker::registerBuiltinFeatureChecks();
       
        // 创建运行时支持
        runtime = std::make_unique<ObjectRuntime>(module.get(), builder.get());
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

llvm::Function* PyCodeGen::getOrCreateExternalFunction(
    const std::string& name,
    llvm::Type* returnType,
    std::vector<llvm::Type*> paramTypes,
    bool isVarArg)
{
    llvm::Function* func = module->getFunction(name);

    if (!func)
    {
        llvm::FunctionType* funcType =
            llvm::FunctionType::get(returnType, paramTypes, isVarArg);

        func = llvm::Function::Create(
            funcType, llvm::Function::ExternalLinkage, name, *module);
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

    return llvm::BasicBlock::Create(*context, name, parent);
}

//===----------------------------------------------------------------------===//
// 临时对象管理
//===----------------------------------------------------------------------===//

void PyCodeGen::addTempObject(llvm::Value* obj, ObjectType* type)
{
    if (obj && type && type->isReference())
    {
        tempObjects.push_back({obj, type});
    }
}

void PyCodeGen::releaseTempObjects()
{
    for (auto& [value, type] : tempObjects)
    {
        if (value && type && type->isReference())
        {
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
// AST节点处理接口
//===----------------------------------------------------------------------===//

llvm::Value* PyCodeGen::handleNode(ASTNode* node)
{
    auto handler = nodeHandlers.getHandler(node->kind());
    if (handler)
    {
        return handler(*this, node);
    }

    return logError("Unhandled node type",
                    node->line.value_or(-1),
                    node->column.value_or(-1));
}

llvm::Value* PyCodeGen::handleExpr(ExprAST* expr)
{
    auto handler = exprHandlers.getHandler(expr->kind());
    if (handler)
    {
        return handler(*this, expr);
    }

    return logError("Unhandled expression type",
                    expr->line.value_or(-1),
                    expr->column.value_or(-1));
}

void PyCodeGen::handleStmt(StmtAST* stmt)
{
    auto handler = stmtHandlers.getHandler(stmt->kind());
    if (handler)
    {
        handler(*this, stmt);
        return;
    }

    logError("Unhandled statement type",
             stmt->line.value_or(-1),
             stmt->column.value_or(-1));
}

llvm::Value* PyCodeGen::handleBinOp(char op, llvm::Value* L, llvm::Value* R)
{
    auto handler = binOpHandlers.getHandler(op);
    if (handler)
    {
        return handler(*this, L, R, op);
    }

    return logError(std::string("Unknown binary operator: ") + op);
}

//===----------------------------------------------------------------------===//
// 表达式访问者方法
//===----------------------------------------------------------------------===//

void PyCodeGen::visit(NumberExprAST* expr)
{
    double value = expr->getValue();

    // 检查是否为整数
    if (value == std::floor(value) && value <= INT32_MAX && value >= INT32_MIN)
    {
        lastExprValue = llvm::ConstantInt::get(*context,
                                               llvm::APInt(32, static_cast<int64_t>(value), true));
    }
    else
    {
        lastExprValue = llvm::ConstantFP::get(*context, llvm::APFloat(value));
    }
        // 设置表达式类型
        setLastExprType(expr->getType());
}

void PyCodeGen::visit(VariableExprAST* expr)
{
    const std::string& name = expr->getName();

    llvm::Value* value = getVariable(name);
    if (!value)
    {
        logError("Unknown variable name: " + name,
                 expr->line.value_or(-1),
                 expr->column.value_or(-1));
        return;
    }

    // 获取变量类型
    ObjectType* type = getVariableType(name);

    // 如果是引用类型，需要检查是否需要增加引用计数
    if (type && type->isReference() && expr->needsCopy())
    {
        lastExprValue = runtime->copyObject(value, type);
        addTempObject(lastExprValue, type);
    }
    else
    {
        lastExprValue = value;
    }
        // 设置表达式类型 - 优先使用从符号表获取的类型
        if (type) {
            setLastExprType(std::make_shared<PyType>(type));
        } else {
            setLastExprType(expr->getType());
        }
}

void PyCodeGen::visit(BinaryExprAST* expr)
{
    // 生成左右操作数的代码
    auto* lhs = static_cast<const ExprAST*>(expr->getLHS());
    auto* rhs = static_cast<const ExprAST*>(expr->getRHS());

    llvm::Value* L = handleExpr(const_cast<ExprAST*>(lhs));
    llvm::Value* R = handleExpr(const_cast<ExprAST*>(rhs));

    if (!L || !R)
    {
        logError("Invalid binary expression operands",
                 expr->line.value_or(-1),
                 expr->column.value_or(-1));
        return;
    }

    // 处理二元操作
    lastExprValue = handleBinOp(expr->getOp(), L, R);

    // 检查结果是否为引用类型，若是，则需要管理其生命周期
    auto typePtr = expr->getType();
    if (typePtr)
    {
        ObjectType* type = typePtr->getObjectType();
        if (type && type->isReference())
        {
            addTempObject(lastExprValue, type);
        }
    }
        // 设置表达式类型
        setLastExprType(expr->getType());
}

void PyCodeGen::visit(CallExprAST* expr)
{
    const std::string& callee = expr->getCallee();
    const auto& args = expr->getArgs();

    // 查找函数
    llvm::Function* calleeF = module->getFunction(callee);
    if (!calleeF)
    {
        logError("Unknown function referenced: " + callee,
                 expr->line.value_or(-1),
                 expr->column.value_or(-1));
        return;
    }

    // 检查参数数量
    if (calleeF->arg_size() != args.size())
    {
        logError("Incorrect number of arguments passed",
                 expr->line.value_or(-1),
                 expr->column.value_or(-1));
        return;
    }

    // 评估所有参数
    std::vector<llvm::Value*> argValues;
    for (size_t i = 0; i < args.size(); i++)
    {
        argValues.push_back(handleExpr(const_cast<ExprAST*>(args[i].get())));
        if (!argValues.back())
        {
            return;  // 错误已在handleExpr中报告
        }
    }

    // 调用函数
    lastExprValue = builder->CreateCall(calleeF, argValues, "calltmp");

    // 检查返回值类型，如果是引用类型，需要管理其生命周期
    auto typePtr = expr->getType();
    if (typePtr)
    {
        ObjectType* type = typePtr->getObjectType();
        if (type && type->isReference())
        {
            addTempObject(lastExprValue, type);
        }
    }
        // 设置表达式类型
        setLastExprType(expr->getType());
}

void PyCodeGen::visit(UnaryExprAST* expr)
{
    // 生成操作数代码
    auto* operand = static_cast<const ExprAST*>(expr->getOperand());
    llvm::Value* op = handleExpr(const_cast<ExprAST*>(operand));

    if (!op)
    {
        logError("Invalid unary expression operand",
                 expr->line.value_or(-1),
                 expr->column.value_or(-1));
        return;
    }

    // 根据操作符处理
    char opCode = expr->getOpCode();
    switch (opCode)
    {
        case '-':
            if (op->getType()->isIntegerTy())
            {
                lastExprValue = builder->CreateNeg(op, "negtmp");
            }
            else if (op->getType()->isDoubleTy())
            {
                lastExprValue = builder->CreateFNeg(op, "negtmp");
            }
            else
            {
                logError("Invalid type for negation",
                         expr->line.value_or(-1),
                         expr->column.value_or(-1));
                return;
            }
            break;
        case '!':
            // 转换操作数为布尔值，然后取反
            {
                llvm::Value* boolVal = PyCodeGenHelper::convertToBool(*this, op);
                lastExprValue = builder->CreateNot(boolVal, "nottmp");
            }
            break;
        default:
            logError(std::string("Unknown unary operator: ") + opCode,
                     expr->line.value_or(-1),
                     expr->column.value_or(-1));
            return;
    }
    
    // 设置表达式类型
    setLastExprType(expr->getType());
}

void PyCodeGen::visit(StringExprAST* expr)
{
    const std::string& value = expr->getValue();

    // 创建全局字符串常量
    llvm::Constant* strConst = llvm::ConstantDataArray::getString(*context, value);

    // 创建全局变量存储字符串
    llvm::GlobalVariable* globalStr = new llvm::GlobalVariable(
        *module,
        strConst->getType(),
        true,
        llvm::GlobalValue::PrivateLinkage,
        strConst,
        ".str");

    // 创建指向字符串开头的指针
    llvm::Constant* zero = llvm::ConstantInt::get(*context, llvm::APInt(32, 0));
    llvm::Constant* indices[] = {zero, zero};
    llvm::Constant* strPtr = llvm::ConstantExpr::getGetElementPtr(
        strConst->getType(), globalStr, indices);

    // 使用运行时函数创建一个字符串对象
    // 在这里，我们假设运行时有一个createString函数
    lastExprValue = builder->CreatePointerCast(strPtr, llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0));

    // 使用运行时创建字符串对象
    auto stringType = TypeRegistry::getInstance().getType("string");
    lastExprValue = runtime->convertObject(lastExprValue,
                                           TypeRegistry::getInstance().getType("cstring"),
                                           stringType);

    // 字符串是引用类型，需要管理其生命周期
    addTempObject(lastExprValue, stringType);

        // 设置表达式类型
        setLastExprType(expr->getType());
}

void PyCodeGen::visit(BoolExprAST* expr)
{
    lastExprValue = llvm::ConstantInt::get(*context, llvm::APInt(1, expr->getValue() ? 1 : 0));
        // 设置表达式类型
        setLastExprType(expr->getType());
}

void PyCodeGen::visit(NoneExprAST* expr)
{
    // None值通常表示为null指针
    lastExprValue = llvm::ConstantPointerNull::get(llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0));

        // 设置表达式类型
        setLastExprType(expr->getType());
}

void PyCodeGen::visit(ListExprAST* expr)
{
    const auto& elements = expr->getElements();

    // 确定列表元素类型
    auto typePtr = expr->getType();
    if (!typePtr)
    {
        logError("Cannot determine list element type",
                 expr->line.value_or(-1),
                 expr->column.value_or(-1));
        return;
    }

    ObjectType* listType = typePtr->getObjectType();
    if (!dynamic_cast<ListType*>(listType))
    {
        logError("Expected list type",
                 expr->line.value_or(-1),
                 expr->column.value_or(-1));
        return;
    }

    ListType* listTypeObj = static_cast<ListType*>(listType);
    ObjectType* elemType = const_cast<ObjectType*>(listTypeObj->getElementType());

    // 计算元素值
    std::vector<llvm::Value*> elemValues;
    for (const auto& elem : elements)
    {
        llvm::Value* elemValue = handleExpr(const_cast<ExprAST*>(elem.get()));
        if (!elemValue)
        {
            return;  // 错误已经在handleExpr中报告
        }
        elemValues.push_back(elemValue);
    }

    // 使用运行时创建列表
    lastExprValue = runtime->createListWithValues(elemValues, elemType);

    // 列表是引用类型，需要管理其生命周期
    addTempObject(lastExprValue, listType);

        // 设置表达式类型
        setLastExprType(expr->getType());
}

void PyCodeGen::visit(IndexExprAST* expr)
{
    // 生成目标和索引表达式代码
    auto* target = static_cast<const ExprAST*>(expr->getTarget());
    auto* index = static_cast<const ExprAST*>(expr->getIndex());

    llvm::Value* targetValue = handleExpr(const_cast<ExprAST*>(target));
    llvm::Value* indexValue = handleExpr(const_cast<ExprAST*>(index));

    if (!targetValue || !indexValue)
    {
        return;  // 错误已经在handleExpr中报告
    }

    // 确定目标类型
    auto targetTypePtr = target->getType();
    if (!targetTypePtr)
    {
        logError("Cannot determine indexed object type",
                 expr->line.value_or(-1),
                 expr->column.value_or(-1));
        return;
    }

    ObjectType* targetType = targetTypePtr->getObjectType();

    // 根据目标类型处理索引操作
    if (dynamic_cast<ListType*>(targetType))
    {
        // 列表索引操作
        lastExprValue = runtime->getListElement(targetValue, indexValue);
    }
    else if (dynamic_cast<DictType*>(targetType))
    {
        // 字典索引操作
        lastExprValue = runtime->getDictItem(targetValue, indexValue);
    }
    else if (targetType->getName() == "string")//？
{
    // 字符串索引操作 - 获取单个字符
    llvm::Function* getCharFunc = getOrCreateExternalFunction(
        "py_string_get_char",
        llvm::PointerType::get(*context, 0),
        {llvm::PointerType::get(*context, 0), llvm::Type::getInt32Ty(*context)});
    
    lastExprValue = builder->CreateCall(getCharFunc, {targetValue, indexValue}, "get_char");
    
    // 添加临时对象跟踪
    auto stringType = TypeRegistry::getInstance().getType("string");
    addTempObject(lastExprValue, stringType);
}
    else
    {
        logError("Type does not support indexing",
                 expr->line.value_or(-1),
                 expr->column.value_or(-1));
        return;
    }

    // 检查结果是否为引用类型，若是，则需要管理其生命周期
    auto typePtr = expr->getType();
    if (typePtr)
    {
        ObjectType* type = typePtr->getObjectType();
        if (type && type->isReference())
        {
            addTempObject(lastExprValue, type);
        }
    }

        // 设置表达式类型
        setLastExprType(expr->getType());
}

//===----------------------------------------------------------------------===//
// 语句访问者方法
//===----------------------------------------------------------------------===//

void PyCodeGen::visit(ExprStmtAST* stmt)
{
    auto* expr = static_cast<const ExprAST*>(stmt->getExpr());

    // 生成表达式代码，但丢弃结果
    handleExpr(const_cast<ExprAST*>(expr));

    // 释放临时对象
    releaseTempObjects();
}

void PyCodeGen::visit(ReturnStmtAST* stmt)
{
    auto* value = static_cast<const ExprAST*>(stmt->getValue());

    // 标记我们正在处理返回语句
    setInReturnStmt(true);

    // 生成返回值代码
    llvm::Value* returnValue = nullptr;
    if (value)
    {
        returnValue = handleExpr(const_cast<ExprAST*>(value));
    }
    else
    {
        // 无返回值，使用void
        returnValue = nullptr;
    }

    // 清除返回语句标记
    setInReturnStmt(false);

    // 为返回值准备适当的内存管理
    if (returnValue && currentReturnType)
    {
        // 如果返回引用类型，需要确保正确的生命周期管理
        if (currentReturnType->isReference())
        {
            returnValue = runtime->prepareReturnValue(returnValue, currentReturnType);
        }
    }

    // 创建返回指令
    if (returnValue)
    {
        builder->CreateRet(returnValue);
    }
    else
    {
        builder->CreateRetVoid();
    }

    // 释放临时对象，但跳过返回值
    clearTempObjects();
}

void PyCodeGen::visit(IfStmtAST* stmt)
{
    auto* condition = static_cast<const ExprAST*>(stmt->getCondition());
    const auto& thenBody = stmt->getThenBody();
    const auto& elseBody = stmt->getElseBody();

    // 生成条件表达式代码
    llvm::Value* condValue = handleExpr(const_cast<ExprAST*>(condition));
    if (!condValue)
    {
        return;  // 错误已经在handleExpr中报告
    }

    // 将条件转换为布尔值
    condValue = PyCodeGenHelper::convertToBool(*this, condValue);

    // 创建基本块
    llvm::Function* func = currentFunction;
    llvm::BasicBlock* thenBB = createBasicBlock("then", func);
    llvm::BasicBlock* elseBB = createBasicBlock("else", func);
    llvm::BasicBlock* mergeBB = createBasicBlock("ifcont", func);

    // 创建条件分支
    builder->CreateCondBr(condValue, thenBB, elseBB);

    // 生成then块代码
    builder->SetInsertPoint(thenBB);
    pushScope();
    for (const auto& stmt : thenBody)
    {
        handleStmt(const_cast<StmtAST*>(stmt.get()));
    }
    popScope();

    // 如果没有终止指令（如return），添加跳转到合并块
    if (!builder->GetInsertBlock()->getTerminator())
    {
        builder->CreateBr(mergeBB);
    }

    // 生成else块代码
    builder->SetInsertPoint(elseBB);
    pushScope();
    for (const auto& stmt : elseBody)
    {
        handleStmt(const_cast<StmtAST*>(stmt.get()));
    }
    popScope();

    // 如果没有终止指令，添加跳转到合并块
    if (!builder->GetInsertBlock()->getTerminator())
    {
        builder->CreateBr(mergeBB);
    }

    // 继续在合并块插入代码
    builder->SetInsertPoint(mergeBB);
}

void PyCodeGen::visit(WhileStmtAST* stmt)
{
    auto* condition = static_cast<const ExprAST*>(stmt->getCondition());
    const auto& body = stmt->getBody();

    // 创建循环的基本块
    llvm::Function* func = currentFunction;
    llvm::BasicBlock* condBB = createBasicBlock("while.cond", func);
    llvm::BasicBlock* bodyBB = createBasicBlock("while.body", func);
    llvm::BasicBlock* afterBB = createBasicBlock("while.end", func);

    // 向循环栈添加这个循环的信息（用于break和continue）
    pushLoopBlocks(condBB, afterBB);

    // 跳转到条件块
    builder->CreateBr(condBB);

    // 生成条件块代码
    builder->SetInsertPoint(condBB);
    llvm::Value* condValue = handleExpr(const_cast<ExprAST*>(condition));
    if (!condValue)
    {
        popLoopBlocks();
        return;  // 错误已经在handleExpr中报告
    }

    // 将条件转换为布尔值
    condValue = PyCodeGenHelper::convertToBool(*this, condValue);

    // 创建条件分支
    builder->CreateCondBr(condValue, bodyBB, afterBB);

    // 生成循环体代码
    builder->SetInsertPoint(bodyBB);
    pushScope();
    for (const auto& stmt : body)
    {
        handleStmt(const_cast<StmtAST*>(stmt.get()));
    }
    popScope();

    // 如果没有终止指令（如return, break），跳回条件块
    if (!builder->GetInsertBlock()->getTerminator())
    {
        builder->CreateBr(condBB);
    }

    // 继续在循环后的块插入代码
    builder->SetInsertPoint(afterBB);

    // 从循环栈弹出这个循环
    popLoopBlocks();
}

/* void PyCodeGen::visit(PrintStmtAST* stmt)
{
    auto* value = static_cast<const ExprAST*>(stmt->getValue());

    // 生成表达式代码
    llvm::Value* printValue = handleExpr(const_cast<ExprAST*>(value));
    if (!printValue)
    {
        return;  // 错误已经在handleExpr中报告
    }

    // 根据值类型调用适当的print函数
    llvm::Type* valueTy = printValue->getType();

    if (valueTy->isIntegerTy(32))
    {
        // 打印整数
        llvm::Function* printfFunc = getOrCreateExternalFunction(
            "printf",
            llvm::Type::getInt32Ty(*context),
            {llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0)},
            true);

        // 创建格式字符串
        llvm::Value* formatStr = builder->CreateGlobalStringPtr("%d\n");

        // 调用printf
        builder->CreateCall(printfFunc, {formatStr, printValue});
    }
    else if (valueTy->isDoubleTy())
    {
        // 打印浮点数
        llvm::Function* printfFunc = getOrCreateExternalFunction(
            "printf",
            llvm::Type::getInt32Ty(*context),
            {llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0)},
            true);

        // 创建格式字符串
        llvm::Value* formatStr = builder->CreateGlobalStringPtr("%f\n");

        // 调用printf
        builder->CreateCall(printfFunc, {formatStr, printValue});
    }
    else if (valueTy->isPointerTy())
{
    // 根据类型调用不同的打印函数
    auto typePtr = stmt->getValue()->getType();
    ObjectType* type = typePtr ? typePtr->getObjectType() : nullptr;
    
    if (type && type->getName() == "string") {
        // 打印字符串
        llvm::Function* printStrFunc = getOrCreateExternalFunction(
            "py_print_string",
            llvm::Type::getVoidTy(*context),
            {llvm::PointerType::get(*context, 0)});
        
        builder->CreateCall(printStrFunc, {printValue});
    }
    else if (type && TypeFeatureChecker::isSequence(type)) {
        // 打印列表或其他序列
        llvm::Function* printListFunc = getOrCreateExternalFunction(
            "py_print_list",
            llvm::Type::getVoidTy(*context),
            {llvm::PointerType::get(*context, 0)});
        
        builder->CreateCall(printListFunc, {printValue});
    }
    else if (type && TypeFeatureChecker::isMapping(type)) {
        // 打印字典
        llvm::Function* printDictFunc = getOrCreateExternalFunction(
            "py_print_dict",
            llvm::Type::getVoidTy(*context),
            {llvm::PointerType::get(*context, 0)});
        
        builder->CreateCall(printDictFunc, {printValue});
    }
    else {
        // 通用对象打印
        llvm::Function* printObjFunc = getOrCreateExternalFunction(
            "py_print_object",
            llvm::Type::getVoidTy(*context),
            {llvm::PointerType::get(*context, 0)});
        
        builder->CreateCall(printObjFunc, {printValue});
    }
}
    else
    {
        logError("Unsupported type for print",
                 stmt->line.value_or(-1),
                 stmt->column.value_or(-1));
    }

    // 释放临时对象
    releaseTempObjects();
} */
void PyCodeGen::visit(PrintStmtAST* stmt)
{
    auto* value = static_cast<const ExprAST*>(stmt->getValue());

    // 生成表达式代码
    llvm::Value* printValue = handleExpr(const_cast<ExprAST*>(value));
    if (!printValue)
    {
        return;  // 错误已经在handleExpr中报告
    }

    // 根据值类型调用适当的print函数
    llvm::Type* valueTy = printValue->getType();

    if (valueTy->isIntegerTy(32))
    {
        // 打印整数
        llvm::Function* printfFunc = getOrCreateExternalFunction(
            "printf",
            llvm::Type::getInt32Ty(*context),
            {llvm::PointerType::get(*context, 0)},
            true);

        // 使用%d格式（整数格式）而非%f
        llvm::Value* formatStr = builder->CreateGlobalStringPtr("%d\n", "", 0, module.get());

        // 调用printf，直接传递整数值
        builder->CreateCall(printfFunc, {formatStr, printValue});
    }
    else if (valueTy->isDoubleTy())
    {
        // 打印浮点数
        llvm::Function* printfFunc = getOrCreateExternalFunction(
            "printf",
            llvm::Type::getInt32Ty(*context),
            {llvm::PointerType::get(*context, 0)},
            true);

        // 创建格式字符串 - 显式传递模块指针
        llvm::Value* formatStr = builder->CreateGlobalStringPtr("%f\n", "", 0, module.get());

        // 调用printf
        builder->CreateCall(printfFunc, {formatStr, printValue});
    }
    else if (valueTy->isPointerTy())
    {
                      // 获取变量的类型
        auto typePtr = stmt->getValue()->getType();
        ObjectType* type = typePtr ? typePtr->getObjectType() : nullptr;

           // 检查是否是变量引用，按现代LLVM指针模型处理
           if (llvm::AllocaInst* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(printValue))
           {
            // 查看变量类型
            llvm::Type* allocatedType = allocaInst->getAllocatedType();
            
            if (allocatedType->isIntegerTy(32))
            {
                // 整数变量 - 处理代码保持不变
                llvm::Function* printfFunc = getOrCreateExternalFunction(
                    "printf",
                    llvm::Type::getInt32Ty(*context),
                    {llvm::PointerType::get(*context, 0)},
                    true);
                
                // 加载整数值
                llvm::Value* loadedValue = builder->CreateLoad(builder->getInt32Ty(), printValue, "load_int");
                
                // 使用整数格式
                llvm::Value* formatStr = builder->CreateGlobalStringPtr("%d\n", "", 0, module.get());
                
                // 调用printf
                builder->CreateCall(printfFunc, {formatStr, loadedValue});
                
                // 处理完成，提前返回
                releaseTempObjects();
                return;
            }
            else if (allocatedType->isDoubleTy())
            {
                // 浮点变量
                llvm::Function* printfFunc = getOrCreateExternalFunction(
                    "printf",
                    llvm::Type::getInt32Ty(*context),
                    {llvm::PointerType::get(*context, 0)},
                    true);
                
                // 加载浮点值
                llvm::Value* loadedValue = builder->CreateLoad(builder->getDoubleTy(), printValue, "load_double");
                
                // 使用浮点格式
                llvm::Value* formatStr = builder->CreateGlobalStringPtr("%f\n", "", 0, module.get());
                
                // 调用printf
                builder->CreateCall(printfFunc, {formatStr, loadedValue});
                
                // 处理完成，提前返回
                releaseTempObjects();
                return;
            }
        }

        // 尝试根据符号表检查类型和值
        if (auto* varExpr = dynamic_cast<VariableExprAST*>(const_cast<ExprAST*>(stmt->getValue())))
        {
            const std::string& varName = varExpr->getName();
            ObjectType* varType = getVariableType(varName);
            
            if (varType && varType->getName() == "int")
            {
                // 处理整数变量 - 代码保持不变
                llvm::Function* printfFunc = getOrCreateExternalFunction(
                    "printf",
                    llvm::Type::getInt32Ty(*context),
                    {llvm::PointerType::get(*context, 0)},
                    true);
                
                // 加载整数值
                llvm::Value* loadedValue = builder->CreateLoad(builder->getInt32Ty(), printValue, "load_int");
                
                // 使用整数格式
                llvm::Value* formatStr = builder->CreateGlobalStringPtr("%d\n", "", 0, module.get());
                
                // 调用printf
                builder->CreateCall(printfFunc, {formatStr, loadedValue});
                
                // 处理完成，提前返回
                releaseTempObjects();
                return;
            }
        }
        
        
        if (type && type->getName() == "string") {
            // 打印字符串
            llvm::Function* printStrFunc = getOrCreateExternalFunction(
                "py_print_string",
                llvm::Type::getVoidTy(*context),
                {llvm::PointerType::get(*context, 0)});
            
            builder->CreateCall(printStrFunc, {printValue});
        }
        else if (type && TypeFeatureChecker::isSequence(type)) {
            // 打印列表或其他序列
            llvm::Function* printListFunc = getOrCreateExternalFunction(
                "py_print_list",
                llvm::Type::getVoidTy(*context),
                {llvm::PointerType::get(*context, 0)});
            
            builder->CreateCall(printListFunc, {printValue});
        }
        else if (type && TypeFeatureChecker::isMapping(type)) {
            // 打印字典
            llvm::Function* printDictFunc = getOrCreateExternalFunction(
                "py_print_dict",
                llvm::Type::getVoidTy(*context),
                {llvm::PointerType::get(*context, 0)});
            
            builder->CreateCall(printDictFunc, {printValue});
        }
        else {
            // 通用对象打印
            llvm::Function* printObjFunc = getOrCreateExternalFunction(
                "py_print_object",
                llvm::Type::getVoidTy(*context),
                {llvm::PointerType::get(*context, 0)});
            
            builder->CreateCall(printObjFunc, {printValue});
        }
    }
    else
    {
        logError("Unsupported type for print",
                 stmt->line.value_or(-1),
                 stmt->column.value_or(-1));
    }

    // 释放临时对象
    releaseTempObjects();
}
// 补全 visit(AssignStmtAST* stmt) 方法
void PyCodeGen::visit(AssignStmtAST* stmt)
{
    const std::string& name = stmt->getName();
    auto* value = static_cast<const ExprAST*>(stmt->getValue());

    // 生成赋值表达式代码
    llvm::Value* assignValue = handleExpr(const_cast<ExprAST*>(value));
    if (!assignValue)
    {
        return;  // 错误已经在handleExpr中报告
    }

    // 检查变量是否已存在
    llvm::Value* existingValue = getVariable(name);

    // 获取赋值值的类型
    auto valueTypePtr = value->getType();
    if (!valueTypePtr)
    {
        logError("Cannot determine type of assignment value",
                 stmt->line.value_or(-1),
                 stmt->column.value_or(-1));
        return;
    }

    ObjectType* valueType = valueTypePtr->getObjectType();

    // 如果变量已存在
    if (existingValue)
    {
        // 获取变量现有类型
        ObjectType* existingType = getVariableType(name);

        // 类型检查 - 确保新值类型与现有变量类型兼容
        if (existingType && !valueType->canAssignTo(existingType))
        {
            logError("Type mismatch in assignment to '" + name + "'",
                     stmt->line.value_or(-1),
                     stmt->column.value_or(-1));
            return;
        }

        // 如果是引用类型且不是同一个引用，需要处理引用计数
        if (existingType && existingType->isReference())
        {
            // 递减旧值的引用计数
            runtime->decRef(existingValue);

            // 新值如果是引用类型，可能需要拷贝
            if (valueType && valueType->isReference())
            {
                // 如果是临时对象，可以直接使用，无需拷贝
                bool isTempObj = false;
                for (const auto& [obj, type] : tempObjects)
                {
                    if (obj == assignValue)
                    {
                        isTempObj = true;
                        break;
                    }
                }

                if (!isTempObj)
                {
                    // 不是临时对象，需要增加引用计数
                    assignValue = runtime->incRef(assignValue);
                }
                else
                {
                    // 是临时对象，从临时对象列表中移除，防止后续自动释放
                    for (auto it = tempObjects.begin(); it != tempObjects.end(); ++it)
                    {
                        if (it->first == assignValue)
                        {
                            tempObjects.erase(it);
                            break;
                        }
                    }
                }
            }
        }

        // 创建存储指令，更新变量值
        if (llvm::AllocaInst* alloca = llvm::dyn_cast<llvm::AllocaInst>(existingValue))
        {
            builder->CreateStore(assignValue, alloca);
        }
        else
        {
            logError("Cannot assign to non-lvalue: " + name,
                     stmt->line.value_or(-1),
                     stmt->column.value_or(-1));
            return;
        }
    }
    // 否则创建新变量
    else
    {
        // 为新变量分配空间
        llvm::AllocaInst* alloca = builder->CreateAlloca(
            assignValue->getType(), nullptr, name);

        // 存储初始值
        builder->CreateStore(assignValue, alloca);

        // 如果是引用类型，需要处理引用计数
        if (valueType && valueType->isReference())
        {
            // 检查是否是临时对象
            bool isTempObj = false;
            for (const auto& [obj, type] : tempObjects)
            {
                if (obj == assignValue)
                {
                    isTempObj = true;
                    break;
                }
            }

            if (!isTempObj)
            {
                // 不是临时对象，需要增加引用计数
                runtime->incRef(assignValue);
            }
            else
            {
                // 是临时对象，从临时对象列表中移除，防止后续自动释放
                for (auto it = tempObjects.begin(); it != tempObjects.end(); ++it)
                {
                    if (it->first == assignValue)
                    {
                        tempObjects.erase(it);
                        break;
                    }
                }
            }
        }

        // 将变量添加到符号表
        setVariable(name, alloca, valueType);
    }

    // 释放临时对象
    releaseTempObjects();
}
void PyCodeGen::visit(ImportStmtAST* stmt)
{
    const std::string& moduleName = stmt->getModuleName();
    const std::string& alias = stmt->getAlias();
    
    // 调用运行时导入函数
    llvm::Function* importFunc = getOrCreateExternalFunction(
        "py_import_module",
        llvm::PointerType::get(*context, 0),
        {builder->CreateGlobalStringPtr(moduleName)->getType()});
    
     llvm::Value* moduleObj = builder->CreateCall(importFunc, 
                                               {builder->CreateGlobalStringPtr(moduleName, "", 0, module.get())}, 
                                               "imported_module");
    
    // 将模块对象存储在符号表中
    std::string symName = alias.empty() ? moduleName : alias;
    
    // 创建模块变量
    llvm::AllocaInst* alloca = builder->CreateAlloca(
        moduleObj->getType(), nullptr, symName);
    
    // 存储模块对象
    builder->CreateStore(moduleObj, alloca);
    
    // 注册到符号表
    auto moduleType = TypeRegistry::getInstance().getType("module");
    setVariable(symName, alloca, moduleType);
}
void PyCodeGen::visit(ClassStmtAST* stmt)
{
    const std::string& className = stmt->getClassName();
    const std::vector<std::string>& baseClasses = stmt->getBaseClasses();
    const auto& methods = stmt->getMethods();
    
    // 创建类对象
    llvm::Function* createClassFunc = getOrCreateExternalFunction(
        "py_create_class",
        llvm::PointerType::get(*context, 0),
        {builder->CreateGlobalStringPtr(className)->getType()});
    
    llvm::Value* classObj = builder->CreateCall(createClassFunc, 
                                              {builder->CreateGlobalStringPtr(className)}, 
                                              "class_obj");
    
    // 处理基类
    for (const auto& baseName : baseClasses) {
        llvm::Value* baseClass = getVariable(baseName);
        if (!baseClass) {
            logError("Unknown base class: " + baseName,
                    stmt->line.value_or(-1),
                    stmt->column.value_or(-1));
            return;
        }
        
        // 添加基类
        llvm::Function* addBaseFunc = getOrCreateExternalFunction(
            "py_add_base_class",
            llvm::Type::getVoidTy(*context),
            {llvm::PointerType::get(*context, 0), llvm::PointerType::get(*context, 0)});
        
        // 从变量加载基类对象 - 使用现代LLVM API
        llvm::Value* baseValue = builder->CreateLoad(
            llvm::PointerType::get(*context, 0), baseClass, "base_class_value");
        
        builder->CreateCall(addBaseFunc, {classObj, baseValue});
    }
    
    // 生成方法
    for (const auto& method : methods) {
        // 处理方法定义
        handleNode(const_cast<FunctionAST*>(method.get()));
        
        // 获取函数
        llvm::Function* methodFunc = module->getFunction(method->getName());
        if (!methodFunc) {
            continue; // 方法创建失败
        }
        
        // 将方法添加到类
        llvm::Function* addMethodFunc = getOrCreateExternalFunction(
            "py_add_method",
            llvm::Type::getVoidTy(*context),
            {llvm::PointerType::get(*context, 0), 
             builder->CreateGlobalStringPtr(method->getName())->getType(),
             llvm::PointerType::get(*context, 0)});
        
        builder->CreateCall(addMethodFunc, 
                          {classObj, 
                           builder->CreateGlobalStringPtr(method->getName()),
                           builder->CreateBitCast(methodFunc, 
                                               llvm::PointerType::get(*context, 0))});
    }
    
    // 处理类变量定义(类属性)
    for (const auto& stmt : stmt->getBody()) {
        if (auto assignStmt = dynamic_cast<const AssignStmtAST*>(stmt.get())) {
            // 计算赋值表达式
            handleStmt(const_cast<StmtAST*>(stmt.get()));
            
            // 获取变量值
            llvm::Value* value = getVariable(assignStmt->getName());
            if (!value) continue;
            
            // 加载值 - 使用现代LLVM API
            llvm::Value* loadedValue = builder->CreateLoad(
                llvm::PointerType::get(*context, 0), value, "attr_value");
            
            // 添加类属性
            llvm::Function* addAttrFunc = getOrCreateExternalFunction(
                "py_add_class_attr",
                llvm::Type::getVoidTy(*context),
                {llvm::PointerType::get(*context, 0), 
                 builder->CreateGlobalStringPtr(assignStmt->getName())->getType(),
                 llvm::PointerType::get(*context, 0)});
            
            builder->CreateCall(addAttrFunc, 
                              {classObj, 
                               builder->CreateGlobalStringPtr(assignStmt->getName()),
                               loadedValue});
        }
    }
    
    // 将类对象保存到变量
    llvm::AllocaInst* alloca = builder->CreateAlloca(
        classObj->getType(), nullptr, className);
    
    builder->CreateStore(classObj, alloca);
    
    // 注册到符号表
    auto classType = TypeRegistry::getInstance().getType("class");
    setVariable(className, alloca, classType);
}

// 实现IndexAssignStmtAST访问方法
void PyCodeGen::visit(IndexAssignStmtAST* stmt)
{
    // 获取目标、索引和值表达式
    auto* target = static_cast<const ExprAST*>(stmt->getTarget());
    auto* index = static_cast<const ExprAST*>(stmt->getIndex());
    auto* value = static_cast<const ExprAST*>(stmt->getValue());

    // 生成目标表达式代码
    llvm::Value* targetValue = handleExpr(const_cast<ExprAST*>(target));
    if (!targetValue)
    {
        return;  // 错误已经在handleExpr中报告
    }

    // 生成索引表达式代码
    llvm::Value* indexValue = handleExpr(const_cast<ExprAST*>(index));
    if (!indexValue)
    {
        return;  // 错误已经在handleExpr中报告
    }

    // 生成值表达式代码
    llvm::Value* assignValue = handleExpr(const_cast<ExprAST*>(value));
    if (!assignValue)
    {
        return;  // 错误已经在handleExpr中报告
    }

    // 确定目标类型
    auto targetTypePtr = target->getType();
    if (!targetTypePtr)
    {
        logError("Cannot determine indexed object type",
                 stmt->line.value_or(-1),
                 stmt->column.value_or(-1));
        return;
    }

    ObjectType* targetType = targetTypePtr->getObjectType();

    // 确定值类型
    auto valueTypePtr = value->getType();
    if (!valueTypePtr)
    {
        logError("Cannot determine assignment value type",
                 stmt->line.value_or(-1),
                 stmt->column.value_or(-1));
        return;
    }

    ObjectType* valueType = valueTypePtr->getObjectType();

    // 根据目标类型处理索引赋值操作
    if (dynamic_cast<ListType*>(targetType))
    {
        // 获取列表元素类型
        ListType* listType = static_cast<ListType*>(targetType);
        ObjectType* elemType = const_cast<ObjectType*>(listType->getElementType());

        // 类型检查 - 确保值类型与列表元素类型兼容
        if (!valueType->canAssignTo(elemType))
        {
            logError("Type mismatch in list assignment",
                     stmt->line.value_or(-1),
                     stmt->column.value_or(-1));
            return;
        }

        // 如果需要，执行类型转换
        if (valueType->getTypeId() != elemType->getTypeId())
        {
            assignValue = runtime->convertObject(assignValue, valueType, elemType);
        }

        // 设置列表元素
        runtime->setListElement(targetValue, indexValue, assignValue);
    }
    else if (dynamic_cast<DictType*>(targetType))
    {
        // 获取字典值类型
        DictType* dictType = static_cast<DictType*>(targetType);
        ObjectType* keyType = const_cast<ObjectType*>(dictType->getKeyType());
        ObjectType* valType = const_cast<ObjectType*>(dictType->getValueType());

        // 类型检查 - 确保值类型与字典值类型兼容
        if (!valueType->canAssignTo(valType))
        {
            logError("Type mismatch in dictionary assignment",
                     stmt->line.value_or(-1),
                     stmt->column.value_or(-1));
            return;
        }

        // 设置字典项
        runtime->setDictItem(targetValue, indexValue, assignValue);
    }
    else
    {
        logError("Type does not support indexed assignment",
                 stmt->line.value_or(-1),
                 stmt->column.value_or(-1));
        return;
    }

    // 释放临时对象
    releaseTempObjects();
}

// 实现FunctionAST访问方法
// filepath: [codegen.cpp](http://_vscodecontentref_/0)

void PyCodeGen::visit(FunctionAST* func)
{
    const std::string& name = func->getName();
    const auto& params = func->getParams();

    // 使用新的公共接口确保类型系统已初始化
    TypeRegistry::getInstance().ensureBasicTypesRegistered();
    
    // 确保特性检查器也已初始化
    TypeFeatureChecker::registerBuiltinFeatureChecks();

    // 首先确保参数类型已解析
    const_cast<FunctionAST*>(func)->resolveParamTypes();

    // 获取函数返回类型
    std::shared_ptr<PyType> returnTypePtr = func->getReturnType();
    if (!returnTypePtr)
    {
        std::cerr << "警告: 无法确定函数 " << name << " 的返回类型，使用Int类型" << std::endl;
        returnTypePtr = PyType::getInt(); // 优先使用int类型而不是any
    }

    ObjectType* returnType = returnTypePtr->getObjectType();
    if (!returnType) {
        std::cerr << "警告: 函数 " << name << " 的返回类型对象无效，使用Int类型" << std::endl;
        returnTypePtr = PyType::getInt();
        returnType = returnTypePtr->getObjectType();
        
        // 再次检查，确保我们有一个有效的类型
        if (!returnType) {
            logError("严重错误: 无法创建有效的返回类型，终止编译");
            return;
        }
    }

    // 创建函数参数类型列表
    std::vector<llvm::Type*> paramTypes;
    for (const auto& param : params)
    {
        if (!param.resolvedType)
        {
            std::cerr << "警告: 参数 " << param.name << " 未解析类型，使用Int类型" << std::endl;
            const_cast<ParamAST&>(param).resolvedType = PyType::getInt();
        }
        
        ObjectType* paramType = param.resolvedType->getObjectType();
        if (!paramType) {
            std::cerr << "警告: 参数 " << param.name << " 的类型对象无效，使用Int类型" << std::endl;
            const_cast<ParamAST&>(param).resolvedType = PyType::getInt();
            paramType = param.resolvedType->getObjectType();
            
            // 再次检查，确保我们有一个有效的类型
            if (!paramType) {
                logError("严重错误: 无法创建有效的参数类型，终止编译");
                return;
            }
        }
        
        // 获取LLVM类型，并添加额外的防护
        llvm::Type* llvmParamType = nullptr;
        try {
            llvmParamType = paramType->getLLVMType(*context);
        } catch (const std::exception& e) {
            std::cerr << "警告: 获取参数 " << param.name << " 的LLVM类型时出错: " 
                      << e.what() << "，使用整数类型代替" << std::endl;
            llvmParamType = llvm::Type::getInt32Ty(*context);  // 使用int32类型，更安全
        }
        
        if (!llvmParamType) {
            std::cerr << "警告: 参数 " << param.name << " 的LLVM类型为空，使用整数类型代替" << std::endl;
            llvmParamType = llvm::Type::getInt32Ty(*context);  // 使用int32类型，更安全
        }
        
        paramTypes.push_back(llvmParamType);
    }

    // 创建函数类型 - 特殊处理main函数
    llvm::Type* llvmReturnType = returnType->getLLVMType(*context);
    llvm::FunctionType* funcType;
    
    if (name == "main") {
        // 确保main函数返回int32，符合C标准
        funcType = llvm::FunctionType::get(llvm::Type::getInt32Ty(*context), paramTypes, false);
    } else {
        funcType = llvm::FunctionType::get(llvmReturnType, paramTypes, false);
    }

    // 如果是重定义，先移除旧定义
    if (llvm::Function* oldFunc = module->getFunction(name)) {
        if (!oldFunc->empty()) {
            // 不允许重定义非声明的函数
            logError("Function '" + name + "' already defined",
                     func->line.value_or(-1),
                     func->column.value_or(-1));
            return;
        }
        oldFunc->eraseFromParent();
    }

    // 创建函数
    llvm::Function* llvmFunc =
        llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, name, *module);

    // 设置参数名
    unsigned idx = 0;
    for (auto& arg : llvmFunc->args())
    {
        arg.setName(params[idx++].name);
    }

    // 创建基本块
    llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(*context, "entry", llvmFunc);
    builder->SetInsertPoint(entryBB);

    // 保存当前函数上下文
    llvm::Function* oldFunc = currentFunction;
    ObjectType* oldReturnType = currentReturnType;

    // 设置新的函数上下文
    currentFunction = llvmFunc;
    currentReturnType = returnType;

    // 开始新作用域
    pushScope();

    // 为函数参数创建局部变量
    idx = 0;
    for (auto& arg : llvmFunc->args())
    {
        // 分配参数空间
        llvm::AllocaInst* alloca = builder->CreateAlloca(arg.getType(), nullptr, arg.getName());

        // 存储参数值
        builder->CreateStore(&arg, alloca);

        // 将参数添加到符号表
        ObjectType* paramType = params[idx].resolvedType->getObjectType();
        setVariable(params[idx].name, alloca, paramType);

        idx++;
    }

    // 设置运行时清理
    if (returnType->isReference())
    {
        runtime->setupCleanupForFunction();
    }

    // 生成函数体
    const auto& body = func->getBody();
    for (const auto& stmt : body)
    {
        handleStmt(const_cast<StmtAST*>(stmt.get()));
    }

    // 如果函数没有明确的返回语句，添加默认返回值
    if (!builder->GetInsertBlock()->getTerminator())
    {
        if (name == "main")
        {
            // main函数默认返回0
            builder->CreateRet(llvm::ConstantInt::get(*context, llvm::APInt(32, 0)));
        }
        else if (returnType->getTypeId() == 0)
        {  // void类型
            builder->CreateRetVoid();
        }
        else if (returnType->getName() == "int")
        {
            builder->CreateRet(llvm::ConstantInt::get(*context, llvm::APInt(32, 0)));
        }
        else if (returnType->getName() == "double")
        {
            builder->CreateRet(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
        }
        else if (returnType->getName() == "bool")
        {
            builder->CreateRet(llvm::ConstantInt::get(*context, llvm::APInt(1, 0)));
        }
        else if (returnType->isReference())
        {
            // 返回None对象
            builder->CreateRet(llvm::ConstantPointerNull::get(
                llvm::PointerType::get(*context, 0)));
        }
        else
        {
            // 默认返回0
            builder->CreateRet(llvm::Constant::getNullValue(llvmReturnType));
        }
    }

    // 验证函数
    std::string errorInfo;
    llvm::raw_string_ostream errorStream(errorInfo);
    if (llvm::verifyFunction(*llvmFunc, &errorStream))
    {
        // 验证失败，删除函数并报错
        llvmFunc->eraseFromParent();
        logError("Function verification failed: " + errorInfo,
                 func->line.value_or(-1),
                 func->column.value_or(-1));
        return;
    }

    // 恢复旧的函数上下文
    currentFunction = oldFunc;
    currentReturnType = oldReturnType;

    // 结束作用域
    popScope();
}
void PyCodeGen::visit(PassStmtAST* stmt)
{
    // pass语句不生成任何代码
    // 它只是作为语法占位符
}
// 实现ModuleAST访问方法
void PyCodeGen::visit(ModuleAST* moduleAST)
{
    const std::string& moduleName = moduleAST->getModuleName();
    module->setModuleIdentifier(moduleName);

    /* // 处理顶级函数定义
    for (const auto& func : moduleAST->getFunctions())
    {
        handleNode(const_cast<FunctionAST*>(func.get()));
    }

    // 处理顶级语句
    for (const auto& stmt : moduleAST->getTopLevelStmts())
    {
        handleStmt(const_cast<StmtAST*>(stmt.get()));
    } */
}

// 实现generateModule方法
/* bool PyCodeGen::generateModule(ModuleAST* moduleAST, const std::string& filename)
{
    try
    {
        // 访问模块AST
        visit(moduleAST);

        // 验证模块
        std::string errorInfo;
        llvm::raw_string_ostream errorStream(errorInfo);
        if (llvm::verifyModule(*module, &errorStream))
        {
            logError("Module verification failed: " + errorInfo);
            return false;
        }

        // 输出LLVM IR到文件
        std::error_code EC;
        llvm::raw_fd_ostream dest(filename, EC);

        if (EC)
        {
            logError("Could not open file: " + EC.message());
            return false;
        }

        module->print(dest, nullptr);
        return true;
    }
    catch (const PyCodeGenError& e)
    {
        std::cerr << e.formatError() << std::endl;
        return false;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error during code generation: " << e.what() << std::endl;
        return false;
    }
} */
// 修改generateModule方法，确保所有代码在main函数内生成
bool PyCodeGen::generateModule(ModuleAST* moduleAST, const std::string& filename)
{
    try
    {
        // 确保节点处理器已初始化
        ensureInitialized();
        
        // 注册模块和函数节点处理器(如果尚未注册)
        if (!nodeHandlers.hasHandler(ASTKind::Module)) {
            nodeHandlers.registerHandler(ASTKind::Module, [](PyCodeGen& gen, ASTNode* node) {
                auto* module = static_cast<ModuleAST*>(node);
                gen.visit(module);
                return nullptr;  // 模块不产生值
            });
        }
        
        if (!nodeHandlers.hasHandler(ASTKind::Function)) {
            nodeHandlers.registerHandler(ASTKind::Function, [](PyCodeGen& gen, ASTNode* node) {
                auto* func = static_cast<FunctionAST*>(node);
                gen.visit(func);
                return nullptr;  // 函数定义不产生值
            });
        }
        
        // 重置模块状态
        currentFunction = nullptr;
        currentReturnType = nullptr;
        lastExprValue = nullptr;
        lastExprType = nullptr;
        
        // 设置模块名称
        const std::string& moduleName = moduleAST->getModuleName();
        module->setModuleIdentifier(moduleName);
        
        // 预处理阶段：解析所有函数的参数类型
        for (const auto& func : moduleAST->getFunctions()) {
            // 首先确保函数参数类型被解析
            const_cast<FunctionAST*>(func.get())->resolveParamTypes();
        }
        
        // 首先处理所有函数定义 - 这些应该是全局的
        for (const auto& func : moduleAST->getFunctions())
        {
            // 确保处理任何类型的FunctionAST
            visit(const_cast<FunctionAST*>(func.get()));
        }

        // 查找是否已有名为"main"的函数(源代码中定义的)
        llvm::Function* mainFunc = module->getFunction("main");
        bool userDefinedMain = (mainFunc != nullptr);
        
        // 处理顶级语句 - 如果存在且没有用户定义的main函数
        if (!moduleAST->getTopLevelStmts().empty())
        {
            if (userDefinedMain) {
                // 如果用户已定义main函数，创建一个__main__函数用于顶级代码
                llvm::FunctionType* mainFuncType = llvm::FunctionType::get(
                    llvm::Type::getInt32Ty(*context), false);
                
                llvm::Function* topLevelFunc = llvm::Function::Create(
                    mainFuncType,
                    llvm::Function::ExternalLinkage,
                    "__main__",
                    *module);
                
                // 创建入口基本块
                llvm::BasicBlock* mainBB = llvm::BasicBlock::Create(
                    *context, "entry", topLevelFunc);
                
                // 保存当前函数上下文
                llvm::Function* oldFunc = currentFunction;
                ObjectType* oldReturnType = currentReturnType;
                
                // 设置新函数为当前函数
                currentFunction = topLevelFunc;
                builder->SetInsertPoint(mainBB);
                
                // 创建顶级作用域
                pushScope();
                
                // 处理顶级语句
                for (const auto& stmt : moduleAST->getTopLevelStmts())
                {
                    handleStmt(const_cast<StmtAST*>(stmt.get()));
                }
                
                // 添加返回语句
                if (!builder->GetInsertBlock()->getTerminator()) {
                    builder->CreateRet(llvm::ConstantInt::get(*context, llvm::APInt(32, 0)));
                }
                
                // 结束顶级作用域
                popScope();
                
                // 恢复之前的函数上下文
                currentFunction = oldFunc;
                currentReturnType = oldReturnType;
                
                // 修改main函数，使其调用__main__，然后调用用户的main函数
                llvm::Function* execMainFunc = llvm::Function::Create(
                    mainFuncType,
                    llvm::Function::ExternalLinkage,
                    "__execute_main",
                    *module);
                
                mainBB = llvm::BasicBlock::Create(*context, "entry", execMainFunc);
                builder->SetInsertPoint(mainBB);
                
                // 调用__main__
                builder->CreateCall(topLevelFunc);
                
                // 调用用户的main函数
                llvm::Value* result = builder->CreateCall(mainFunc);
                builder->CreateRet(result);
            }
            else {
                // 没有用户定义的main，创建标准main
                llvm::FunctionType* mainFuncType = llvm::FunctionType::get(
                    llvm::Type::getInt32Ty(*context), false);
                
                mainFunc = llvm::Function::Create(
                    mainFuncType,
                    llvm::Function::ExternalLinkage,
                    "main",
                    *module);
                
                // 创建入口基本块
                llvm::BasicBlock* mainBB = llvm::BasicBlock::Create(
                    *context, "entry", mainFunc);
                
                // 保存当前函数上下文
                llvm::Function* oldFunc = currentFunction;
                ObjectType* oldReturnType = currentReturnType;
                
                // 设置main函数为当前函数
                currentFunction = mainFunc;
                builder->SetInsertPoint(mainBB);
                
                // 创建顶级作用域
                pushScope();
                
                // 处理顶级语句
                for (const auto& stmt : moduleAST->getTopLevelStmts())
                {
                    if (!stmt) {
                        logError("Null statement encountered in top-level statements");
                        continue;
                    }
                    
                    try {
                        handleStmt(const_cast<StmtAST*>(stmt.get()));
                    } catch (const PyCodeGenError& e) {
                        std::cerr << "Error handling statement: " << e.formatError() << std::endl;
                    } catch (const std::exception& e) {
                        std::cerr << "Exception handling statement: " << e.what() << std::endl;
                    }
                }
                
                // 添加main函数的返回语句
                if (!builder->GetInsertBlock()->getTerminator()) {
                    builder->CreateRet(llvm::ConstantInt::get(*context, llvm::APInt(32, 0)));
                }
                
                // 结束顶级作用域
                popScope();
                
                // 恢复之前的函数上下文
                currentFunction = oldFunc;
                currentReturnType = oldReturnType;
            }
        }
        else if (!userDefinedMain)
        {
            // 没有顶级语句和main函数，创建一个空的main函数以满足可执行程序要求
            llvm::FunctionType* mainFuncType = llvm::FunctionType::get(
                llvm::Type::getInt32Ty(*context), false);
            
            mainFunc = llvm::Function::Create(
                mainFuncType,
                llvm::Function::ExternalLinkage,
                "main",
                *module);
            
            llvm::BasicBlock* mainBB = llvm::BasicBlock::Create(
                *context, "entry", mainFunc);
            
            builder->SetInsertPoint(mainBB);
            builder->CreateRet(llvm::ConstantInt::get(*context, llvm::APInt(32, 0)));
        }
        
        // 清理函数上下文和临时对象
        currentFunction = nullptr;
        currentReturnType = nullptr;
        clearTempObjects();
        
        // 验证模块
        std::string errorInfo;
        llvm::raw_string_ostream errorStream(errorInfo);
        if (llvm::verifyModule(*module, &errorStream))
        {
            logError("Module verification failed: " + errorInfo);
            return false;
        }

        // 输出LLVM IR到文件
        std::error_code EC;
        llvm::raw_fd_ostream dest(filename, EC);

        if (EC)
        {
            logError("Could not open file: " + EC.message());
            return false;
        }

        module->print(dest, nullptr);
        return true;
    }
    catch (const PyCodeGenError& e)
    {
        std::cerr << e.formatError() << std::endl;
        return false;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error during code generation: " << e.what() << std::endl;
        return false;
    }
    catch (...)
    {
        std::cerr << "Unknown error during code generation" << std::endl;
        return false;
    }
}
// 实现PyCodeGenHelper命名空间中的辅助函数
namespace PyCodeGenHelper
{

// 获取LLVM类型
llvm::Type* getLLVMType(llvm::LLVMContext& context, ObjectType* type)
{
    if (!type) return llvm::Type::getVoidTy(context);
    return type->getLLVMType(context);
}

// 数值转换函数
llvm::Value* convertToDouble(PyCodeGen& codegen, llvm::Value* value)
{
    if (value->getType()->isIntegerTy())
    {
        return codegen.getBuilder().CreateSIToFP(value, llvm::Type::getDoubleTy(codegen.getContext()), "conv_to_double");
    }
    return value;
}

llvm::Value* convertToInt(PyCodeGen& codegen, llvm::Value* value)
{
    if (value->getType()->isDoubleTy())
    {
        return codegen.getBuilder().CreateFPToSI(value, llvm::Type::getInt32Ty(codegen.getContext()), "conv_to_int");
    }
    return value;
}

llvm::Value* convertToBool(PyCodeGen& codegen, llvm::Value* value)
{
    llvm::IRBuilder<>& builder = codegen.getBuilder();
    llvm::Type* valueTy = value->getType();

    if (valueTy->isIntegerTy(1))
    {
        return value;  // 已经是bool类型
    }
    else if (valueTy->isIntegerTy())
    {
        return builder.CreateICmpNE(value,
                                    llvm::ConstantInt::get(valueTy, 0), "conv_to_bool");
    }
    else if (valueTy->isDoubleTy())
    {
        return builder.CreateFCmpONE(value,
                                     llvm::ConstantFP::get(valueTy, 0.0), "conv_to_bool");
    }
    else if (valueTy->isPointerTy())
    {
        return builder.CreateICmpNE(value,
                                    llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(valueTy)),
                                    "conv_to_bool");
    }

    // 默认转换为true
    return llvm::ConstantInt::get(llvm::Type::getInt1Ty(codegen.getContext()), 1);
}

// 深拷贝对象函数
llvm::Value* deepCopyValue(PyCodeGen& codegen, llvm::Value* value, ObjectType* type)
{
    if (!type->isReference())
    {
        // 原始类型直接返回值
        return value;
    }

    // 使用运行时进行对象拷贝
    return codegen.getRuntime().copyObject(value, type);
}

// 获取函数参数类型
std::vector<ObjectType*> PyCodeGenHelper::getFunctionParamTypes(const FunctionAST* func)
{
    std::vector<ObjectType*> paramTypes;
    for (const auto& param : func->getParams())
    {
        if (param.resolvedType && param.resolvedType->getObjectType())
        {
            paramTypes.push_back(param.resolvedType->getObjectType());
        }
        else
        {
            // 使用安全的默认类型
            auto safeType = PyType::getAny();
            paramTypes.push_back(safeType->getObjectType());
        }
    }
    return paramTypes;
}

// 创建局部变量
llvm::Value* createLocalVariable(PyCodeGen& codegen, const std::string& name,
                                 ObjectType* type, llvm::Value* initValue)
{
    llvm::Type* llvmType = type->getLLVMType(codegen.getContext());
    llvm::IRBuilder<>& builder = codegen.getBuilder();

    // 分配局部变量空间
    llvm::AllocaInst* alloca = builder.CreateAlloca(llvmType, nullptr, name);

    // 如果有初始值，存储它
    if (initValue)
    {
        builder.CreateStore(initValue, alloca);
    }

    // 将变量添加到符号表
    codegen.setVariable(name, alloca, type);

    return alloca;
}

// 创建全局变量
llvm::GlobalVariable* createGlobalVariable(PyCodeGen& codegen, const std::string& name,
                                           ObjectType* type, llvm::Constant* initValue)
{
    llvm::Type* llvmType = type->getLLVMType(codegen.getContext());

    // 如果没有提供初始值，创建一个零值
    if (!initValue)
    {
        initValue = llvm::Constant::getNullValue(llvmType);
    }

    // 创建全局变量
    llvm::GlobalVariable* global = new llvm::GlobalVariable(
        *codegen.getModule(),
        llvmType,
        false,  // 非常量
        llvm::GlobalValue::ExternalLinkage,
        initValue,
        name);

    // 将变量添加到符号表
    codegen.setVariable(name, global, type);

    return global;
}

}  // namespace PyCodeGenHelper
}  // namespace llvmpy