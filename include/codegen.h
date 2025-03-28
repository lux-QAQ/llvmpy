#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <map>
#include <unordered_map>
#include <memory>
#include <string>
#include <functional>

namespace llvmpy
{

// 类型系统
struct Type
{
    enum TypeKind
    {
        Int,
        Double,
        String,
        Bool,
        Void,
        Unknown
    };
    TypeKind kind;

    Type(TypeKind k) : kind(k)
    {
    }

    static std::shared_ptr<Type> get(TypeKind k);
    static std::shared_ptr<Type> getInt();
    static std::shared_ptr<Type> getDouble();
    static std::shared_ptr<Type> getString();
    static std::shared_ptr<Type> getBool();
    static std::shared_ptr<Type> getVoid();
    static std::shared_ptr<Type> getUnknown();

    // 辅助方法
    bool isNumeric() const
    {
        return kind == Int || kind == Double;
    }
    bool isString() const
    {
        return kind == String;
    }
    bool isBool() const
    {
        return kind == Bool;
    }
    bool isVoid() const
    {
        return kind == Void;
    }

    // 获取对应的LLVM类型
    static llvm::Type* getLLVMType(llvm::LLVMContext& context, TypeKind kind);
};

// 类型注册表
class TypeRegistry
{
private:
    // 从字符串到Type的映射
    static std::unordered_map<std::string, std::shared_ptr<Type>> typeMap;
    // 注册初始化标志
    static bool initialized;

public:
    // 初始化注册表
    static void initialize();
    // 注册一种类型
    static void registerType(const std::string& name, std::shared_ptr<Type> type);
    // 根据名称获取类型
    static std::shared_ptr<Type> getType(const std::string& name);
    // 确保注册表已初始化
    static void ensureInitialized();
    // 获取所有已注册类型
    static const std::unordered_map<std::string, std::shared_ptr<Type>>& getAllTypes()
    {
        ensureInitialized();
        return typeMap;
    }
};

// 代码生成器
class CodeGen
{
public:
    using ASTNodeHandler = std::function<llvm::Value*(CodeGen&, ASTNode*)>;
    using ExprHandler = std::function<llvm::Value*(CodeGen&, ExprAST*)>;
    using StmtHandler = std::function<void(CodeGen&, StmtAST*)>;

private:
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;

    // 符号表 - 变量名到LLVM值的映射
    std::map<std::string, llvm::Value*> namedValues;
    // 当前函数
    llvm::Function* currentFunction;
    // 当前函数的返回类型
    llvm::Type* currentReturnType;
    // 表达式结果栈 - 存储最近生成的表达式的值
    llvm::Value* lastExprValue;

    // AST节点处理函数注册表
    static std::unordered_map<ASTKind, ASTNodeHandler> nodeHandlers;
    // 表达式处理函数注册表
    static std::unordered_map<ASTKind, ExprHandler> exprHandlers;
    // 语句处理函数注册表
    static std::unordered_map<ASTKind, StmtHandler> stmtHandlers;
    // 二元操作符处理函数注册表
    static std::unordered_map<char, std::function<llvm::Value*(CodeGen&, llvm::Value*, llvm::Value*)>> binOpHandlers;

    // 注册函数
    static void registerNodeHandler(ASTKind kind, ASTNodeHandler handler);
    static void registerExprHandler(ASTKind kind, ExprHandler handler);
    static void registerStmtHandler(ASTKind kind, StmtHandler handler);
    static void registerBinOpHandler(char op, std::function<llvm::Value*(CodeGen&, llvm::Value*, llvm::Value*)> handler);

    // 初始化所有注册表
    static void initializeRegistries();
    static void ensureInitialized();

    // 错误处理
    llvm::Value* logErrorV(const std::string& message);

    // 获取标准库函数
    llvm::Function* getPrintfFunction();
    llvm::Function* getMallocFunction();
    llvm::Function* getFreeFunction();

    // 类型处理
    llvm::Type* getTypeFromString(const std::string& typeStr);
    llvm::Type* getLLVMType(std::shared_ptr<Type> type);

    // 字符串处理
    llvm::Value* createStringConstant(const std::string& value);

    // 基本代码生成方法
    llvm::Value* codegenExpr(const ExprAST* expr);
    void codegenStmt(const StmtAST* stmt);

public:
    CodeGen();
    ~CodeGen() = default;

    // 生成整个模块的代码
    void generateModule(ModuleAST* moduleAST);

    // 访问器方法
    llvm::Module* getModule() const
    {
        return module.get();
    }
    llvm::LLVMContext& getContext()
    {
        return *context;
    }
    llvm::IRBuilder<>& getBuilder()
    {
        return *builder;
    }
    llvm::Value* getLastExprValue() const
    {
        return lastExprValue;
    }
    void setLastExprValue(llvm::Value* value)
    {
        lastExprValue = value;
    }

    // 符号表操作
    bool hasVariable(const std::string& name) const
    {
        return namedValues.find(name) != namedValues.end();
    }
    llvm::Value* getVariable(const std::string& name)
    {
        return namedValues[name];
    }
    void setVariable(const std::string& name, llvm::Value* value)
    {
        namedValues[name] = value;
    }

    // 从AST调用处理函数
    llvm::Value* handleNode(ASTNode* node);
    llvm::Value* handleExpr(ExprAST* expr);
    void handleStmt(StmtAST* stmt);

    // 代码生成接口方法
    llvm::Function* codegenFunction(const FunctionAST* func);
    llvm::Value* codegenNumberExpr(const NumberExprAST* expr);
    llvm::Value* codegenVariableExpr(const VariableExprAST* expr);
    llvm::Value* codegenBinaryExpr(const BinaryExprAST* expr);
    llvm::Value* codegenCallExpr(const CallExprAST* expr);
    llvm::Value* codegenUnaryExpr(const UnaryExprAST* expr);
    llvm::Value* codegenListExpr(const ListExprAST* expr) ;
    llvm::Value* codegenIndexExpr(const IndexExprAST* expr);
    llvm::Value* getElementAddress(llvm::Value* listPtr, llvm::Value* indexValue, const std::string& targetName);
  

    void codegenExprStmt(const ExprStmtAST* stmt);
    void codegenReturnStmt(const ReturnStmtAST* stmt);
    void codegenIfStmt(const IfStmtAST* stmt);
    void codegenWhileStmt(const WhileStmtAST* stmt);
    void codegenPrintStmt(const PrintStmtAST* stmt);
    void codegenAssignStmt(const AssignStmtAST* stmt);
    void codegenPassStmt(const PassStmtAST* stmt);
    void codegenImportStmt(const ImportStmtAST* stmt);
    void codegenClassStmt(const ClassStmtAST* stmt);
    void codegenIndexAssignStmt(const IndexAssignStmtAST* stmt);

    // Visitor模式接口
    void visit(NumberExprAST* expr);
    void visit(VariableExprAST* expr);
    void visit(BinaryExprAST* expr);
    void visit(CallExprAST* expr);
    void visit(UnaryExprAST* expr);
    void visit(ExprStmtAST* stmt);
    void visit(ReturnStmtAST* stmt);
    void visit(IfStmtAST* stmt);
    void visit(WhileStmtAST* stmt);
    void visit(PrintStmtAST* stmt);
    void visit(AssignStmtAST* stmt);
    void visit(PassStmtAST* stmt);
    void visit(ImportStmtAST* stmt);
    void visit(ClassStmtAST* stmt);
    void visit(FunctionAST* func);
    void visit(ModuleAST* module);
    void visit(ListExprAST* expr);
    void visit(IndexExprAST* expr);
    void visit(IndexAssignStmtAST* stmt);
};

}  // namespace llvmpy

#endif  // CODEGEN_H