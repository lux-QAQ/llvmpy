#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "ObjectType.h"
#include "ObjectRuntime.h"
#include "TypeIDs.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Verifier.h>

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <map>
#include <stack>

namespace llvmpy {

// 前向声明
class PyCodeGen;
class PySymbolTable;
class PyScope;

// ===----------------------------------------------------------------------===//
// 代码生成注册表 - 使用模板来简化处理各种AST节点的代码生成函数注册
// ===----------------------------------------------------------------------===//

// 代码生成函数类型定义
using PyNodeHandlerFunc = std::function<llvm::Value*(PyCodeGen&, ASTNode*)>;
using PyExprHandlerFunc = std::function<llvm::Value*(PyCodeGen&, ExprAST*)>;
using PyStmtHandlerFunc = std::function<void(PyCodeGen&, StmtAST*)>;
using PyBinOpHandlerFunc = std::function<llvm::Value*(PyCodeGen&, llvm::Value*, llvm::Value*, char)>;

// 通用注册器模板
template<typename KeyType, typename HandlerType>
class PyCodeGenRegistry {
private:
    std::unordered_map<KeyType, HandlerType> handlers;

public:
    void registerHandler(KeyType key, HandlerType handler) {
        handlers[key] = std::move(handler);
    }

    HandlerType getHandler(KeyType key) const {
        auto it = handlers.find(key);
        if (it != handlers.end()) {
            return it->second;
        }
        return nullptr;
    }

    bool hasHandler(KeyType key) const {
        return handlers.find(key) != handlers.end();
    }
};

// ===----------------------------------------------------------------------===//
// 符号作用域 - 管理变量声明和作用域
// ===----------------------------------------------------------------------===//

class PyScope {
private:
    std::map<std::string, llvm::Value*> variables;
    std::map<std::string, ObjectType*> variableTypes;
    PyScope* parent;

public:
    PyScope(PyScope* p = nullptr) : parent(p) {}

    bool hasVariable(const std::string& name) const;
    llvm::Value* getVariable(const std::string& name);
    void setVariable(const std::string& name, llvm::Value* value, ObjectType* type = nullptr);
    ObjectType* getVariableType(const std::string& name);
    
    PyScope* getParent() const { return parent; }
};

// 符号表 - 管理嵌套作用域
class PySymbolTable {
private:
    std::stack<std::unique_ptr<PyScope>> scopes;

public:
    PySymbolTable();
    ~PySymbolTable() = default;

    PyScope* currentScope();
    void pushScope();
    void popScope();
    
    bool hasVariable(const std::string& name) const;
    llvm::Value* getVariable(const std::string& name);
    void setVariable(const std::string& name, llvm::Value* value, ObjectType* type = nullptr);
    ObjectType* getVariableType(const std::string& name);
};

// ===----------------------------------------------------------------------===//
// 代码生成错误异常
// ===----------------------------------------------------------------------===//

class PyCodeGenError : public std::runtime_error {
private:
    int line;
    int column;

public:
    PyCodeGenError(const std::string& message, int line = -1, int column = -1)
        : std::runtime_error(message), line(line), column(column) {}

    std::string formatError() const;
    int getLine() const { return line; }
    int getColumn() const { return column; }
};

// ===----------------------------------------------------------------------===//
// 主代码生成器类
// ===----------------------------------------------------------------------===//

class PyCodeGen {
private:
    // LLVM核心组件
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;

    // 符号表和作用域管理
    PySymbolTable symbolTable;

    // 当前函数上下文
    llvm::Function* currentFunction;
    ObjectType* currentReturnType;
    
    // 最近生成的表达式值
    llvm::Value* lastExprValue;
    std::shared_ptr<PyType> lastExprType; // 添加这行，用于存储最近表达式的类型
    // 对象运行时支持
    std::unique_ptr<ObjectRuntime> runtime;
    
    // 标记是否正在处理返回语句
    bool inReturnStmt;
    
    // 跟踪需要销毁的临时对象
    std::vector<std::pair<llvm::Value*, ObjectType*>> tempObjects;
    
    // 保存当前基本块前的基本块（用于控制流）
    llvm::BasicBlock* savedBlock;
    
    // 循环控制块映射 (用于break/continue语句)
    struct LoopInfo {
        llvm::BasicBlock* condBlock;
        llvm::BasicBlock* afterBlock;
    };
    std::stack<LoopInfo> loopStack;

    // 处理函数注册表
    static PyCodeGenRegistry<ASTKind, PyNodeHandlerFunc> nodeHandlers;
    static PyCodeGenRegistry<ASTKind, PyExprHandlerFunc> exprHandlers;
    static PyCodeGenRegistry<ASTKind, PyStmtHandlerFunc> stmtHandlers;
    static PyCodeGenRegistry<char, PyBinOpHandlerFunc> binOpHandlers;
    
    // 静态初始化标志
    static bool isInitialized;
    
    // 初始化处理函数注册表
    static void initializeHandlers();
    
    // 错误处理
    llvm::Value* logError(const std::string& message, int line = -1, int column = -1);
    
    // 辅助函数
    llvm::Function* getOrCreateExternalFunction(
        const std::string& name,
        llvm::Type* returnType,
        std::vector<llvm::Type*> paramTypes,
        bool isVarArg = false);

public:
    PyCodeGen();
    ~PyCodeGen() = default;

    // 确保注册表已初始化
    static void ensureInitialized();

    // 代码生成入口点
    bool generateModule(ModuleAST* module, const std::string& filename = "output.ll");
    
    // 访问器方法
    llvm::LLVMContext& getContext() { return *context; }
    llvm::Module* getModule() { return module.get(); }
    llvm::IRBuilder<>& getBuilder() { return *builder; }
    ObjectRuntime& getRuntime() { return *runtime; }
    
    // 符号表操作
    void pushScope() { symbolTable.pushScope(); }
    void popScope() { symbolTable.popScope(); }
    bool hasVariable(const std::string& name) const { return symbolTable.hasVariable(name); }
    llvm::Value* getVariable(const std::string& name) { return symbolTable.getVariable(name); }
    void setVariable(const std::string& name, llvm::Value* value, ObjectType* type = nullptr) {
        symbolTable.setVariable(name, value, type);
    }
    ObjectType* getVariableType(const std::string& name) { return symbolTable.getVariableType(name); }
    
    // 当前函数上下文
    llvm::Function* getCurrentFunction() const { return currentFunction; }
    void setCurrentFunction(llvm::Function* func) { currentFunction = func; }
    ObjectType* getCurrentReturnType() const { return currentReturnType; }
    void setCurrentReturnType(ObjectType* type) { currentReturnType = type; }
    
    // 最近生成的表达式值
    llvm::Value* getLastExprValue() const { return lastExprValue; }
    void setLastExprValue(llvm::Value* value) { lastExprValue = value; }
    // 添加这两个方法，用于获取和设置最近表达式的类型
    std::shared_ptr<PyType> getLastExprType() const { return lastExprType; }
    void setLastExprType(std::shared_ptr<PyType> type) { lastExprType = type; }
    // 循环控制
    void pushLoopBlocks(llvm::BasicBlock* condBlock, llvm::BasicBlock* afterBlock);
    void popLoopBlocks();
    LoopInfo* getCurrentLoop();
    
    // 临时对象管理
    void addTempObject(llvm::Value* obj, ObjectType* type);
    void releaseTempObjects();
    void clearTempObjects();
    
    // 返回语句标记
    bool isInReturnStmt() const { return inReturnStmt; }
    void setInReturnStmt(bool value) { inReturnStmt = value; }
    
    // 辅助方法 - 创建块
    llvm::BasicBlock* createBasicBlock(const std::string& name, llvm::Function* parent = nullptr);
    
    // 处理不同AST节点的接口方法
    llvm::Value* handleNode(ASTNode* node);
    llvm::Value* handleExpr(ExprAST* expr);
    void handleStmt(StmtAST* stmt);
    llvm::Value* handleBinOp(char op, llvm::Value* L, llvm::Value* R);
    
    // 获取ObjectType对应的LLVM类型
    llvm::Type* getLLVMType(ObjectType* type);
    
    // Visitor模式接口实现
    void visit(NumberExprAST* expr);
    void visit(VariableExprAST* expr);
    void visit(BinaryExprAST* expr);
    void visit(CallExprAST* expr);
    void visit(UnaryExprAST* expr);
    void visit(StringExprAST* expr);
    void visit(BoolExprAST* expr);
    void visit(NoneExprAST* expr);
    void visit(ListExprAST* expr);
    void visit(IndexExprAST* expr);
    
    void visit(ExprStmtAST* stmt);
    void visit(ReturnStmtAST* stmt);
    void visit(IfStmtAST* stmt);
    void visit(WhileStmtAST* stmt);
    void visit(PrintStmtAST* stmt);
    void visit(AssignStmtAST* stmt);
    void visit(PassStmtAST* stmt);
    void visit(ImportStmtAST* stmt);
    void visit(ClassStmtAST* stmt);
    void visit(IndexAssignStmtAST* stmt);
    
    void visit(FunctionAST* func);
    void visit(ModuleAST* module);
};

// ===----------------------------------------------------------------------===//
// 代码生成帮助函数
// ===----------------------------------------------------------------------===//

// 类型转换助手函数
namespace PyCodeGenHelper {
    // AST类型到LLVM类型转换
    llvm::Type* getLLVMType(llvm::LLVMContext& context, ObjectType* type);
    
    // 数值转换函数
    llvm::Value* convertToDouble(PyCodeGen& codegen, llvm::Value* value);
    llvm::Value* convertToInt(PyCodeGen& codegen, llvm::Value* value);
    llvm::Value* convertToBool(PyCodeGen& codegen, llvm::Value* value);
    
    // 对象深拷贝帮助函数
    llvm::Value* deepCopyValue(PyCodeGen& codegen, llvm::Value* value, ObjectType* type);
    
    // 获取函数的参数类型
    std::vector<ObjectType*> getFunctionParamTypes(const FunctionAST* func);
    
    // 创建局部变量
    llvm::Value* createLocalVariable(PyCodeGen& codegen, const std::string& name, 
                                     ObjectType* type, llvm::Value* initValue = nullptr);
                                     
    // 创建全局变量
    llvm::GlobalVariable* createGlobalVariable(PyCodeGen& codegen, const std::string& name, 
                                             ObjectType* type, llvm::Constant* initValue = nullptr);
}

}  // namespace llvmpy

#endif  // CODEGEN_H