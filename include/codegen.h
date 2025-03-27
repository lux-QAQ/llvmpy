#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include <map>
#include <memory>
#include <string>

namespace llvmpy
{

struct Type
{
    enum TypeKind
    {
        Int,
        Double,
        String,
        Void,
        Unknown
    };

    TypeKind kind;

    Type(TypeKind k) : kind(k)
    {
    }
    static std::shared_ptr<Type> getInt()
    {
        return std::make_shared<Type>(Int);
    }
    static std::shared_ptr<Type> getDouble()
    {
        return std::make_shared<Type>(Double);
    }
    static std::shared_ptr<Type> getString()
    {
        return std::make_shared<Type>(String);
    }
    static std::shared_ptr<Type> getVoid()
    {
        return std::make_shared<Type>(Void);
    }
    static std::shared_ptr<Type> getUnknown()
    {
        return std::make_shared<Type>(Unknown);
    }
};

class CodeGen
{
private:
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;

    // 符号表
    std::map<std::string, llvm::Value*> namedValues;

    // 当前函数
    llvm::Function* currentFunction;

    // 当前返回值类型
    llvm::Type* currentReturnType;

    // 辅助方法
    llvm::Value* logErrorV(const std::string& message);
    llvm::Type* getTypeFromString(const std::string& typeStr);
    llvm::Type* getLLVMType(std::shared_ptr<Type> type);

public:
    CodeGen();

    // 生成模块
    void generateModule(ModuleAST* module);

    // 生成表达式
    llvm::Value* codegenExpr(const ExprAST* expr);
    llvm::Value* codegenNumberExpr(const NumberExprAST* expr);
    llvm::Value* codegenVariableExpr(const VariableExprAST* expr);
    llvm::Value* codegenBinaryExpr(const BinaryExprAST* expr);
    llvm::Value* codegenCallExpr(const CallExprAST* expr);

    // 生成语句
    void codegenStmt(const StmtAST* stmt);
    void codegenExprStmt(const ExprStmtAST* stmt);
    void codegenReturnStmt(const ReturnStmtAST* stmt);
    void codegenIfStmt(const IfStmtAST* stmt);
    void codegenPrintStmt(const PrintStmtAST* stmt);
    void codegenAssignStmt(const AssignStmtAST* stmt);

    // 生成函数
    llvm::Function* codegenFunction(const FunctionAST* func);

    // 创建printf函数声明
    llvm::Function* getPrintfFunction();

    // 获取LLVM模块
    llvm::Module* getModule() const
    {
        return module.get();
    }

    // 访问器方法
    void visit(NumberExprAST* expr);
    void visit(VariableExprAST* expr);
    void visit(BinaryExprAST* expr);
    void visit(CallExprAST* expr);
    void visit(ExprStmtAST* stmt);
    void visit(ReturnStmtAST* stmt);
    void visit(IfStmtAST* stmt);
    void visit(PrintStmtAST* stmt);
    void visit(AssignStmtAST* stmt);
    void visit(FunctionAST* func);
    void visit(ModuleAST* module);
    void visit(WhileStmtAST* stmt);
    void codegenWhileStmt(const WhileStmtAST* stmt);
};

}  // namespace llvmpy

#endif  // CODEGEN_H