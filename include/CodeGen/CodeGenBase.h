#ifndef CODEGEN_BASE_H
#define CODEGEN_BASE_H

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Verifier.h>

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <stack>
#include <unordered_map>
#include <functional>

#include "ast.h"
#include "ObjectType.h"
#include "TypeIDs.h"

namespace llvmpy {

// 前向声明所有代码生成器组件
class CodeGenBase;
class CodeGenExpr;
class CodeGenStmt;
class CodeGenModule;
class CodeGenType;
class CodeGenRuntime;
class ObjectRuntime;

// 作用域 - 管理变量和类型
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
};

// 符号表 - 管理嵌套作用域
class PySymbolTable {
private:
    std::stack<std::unique_ptr<PyScope>> scopes;

public:
    PySymbolTable() {
        // 创建全局作用域
        pushScope();
    }
    
    PyScope* currentScope();
    void pushScope();
    void popScope();
    
    bool hasVariable(const std::string& name) const;
    llvm::Value* getVariable(const std::string& name);
    void setVariable(const std::string& name, llvm::Value* value, ObjectType* type = nullptr);
    ObjectType* getVariableType(const std::string& name);
};

// 代码生成错误
class PyCodeGenError : public std::runtime_error {
private:
    int line;
    int column;
    bool isTypeError;

public:
    PyCodeGenError(const std::string& message, int line = 0, int column = 0, bool isTypeError = false)
        : std::runtime_error(message), line(line), column(column), isTypeError(isTypeError) {}
    
    int getLine() const { return line; }
    int getColumn() const { return column; }
    bool getIsTypeError() const { return isTypeError; }
    
    std::string formatError() const;
};

// 循环信息 - 用于处理循环控制流
struct LoopInfo {
    llvm::BasicBlock* condBlock;  // 循环条件块
    llvm::BasicBlock* afterBlock; // 循环后的块
    
    LoopInfo(llvm::BasicBlock* cond, llvm::BasicBlock* after)
        : condBlock(cond), afterBlock(after) {}
};

// 基础代码生成器 - 包含通用功能和组件引用
class CodeGenBase {
protected:
    llvm::LLVMContext* context;
    llvm::Module* module;
    llvm::IRBuilder<>* builder;
    
    // 组件引用
    std::unique_ptr<CodeGenExpr> exprGen;
    std::unique_ptr<CodeGenStmt> stmtGen;
    std::unique_ptr<CodeGenModule> moduleGen;
    std::unique_ptr<CodeGenType> typeGen;
    std::unique_ptr<CodeGenRuntime> runtimeGen;
    
    // 符号表和作用域
    PySymbolTable symbolTable;
    
    // 循环管理
    std::stack<LoopInfo> loopStack;
    
    // 当前函数
    llvm::Function* currentFunction;
    ObjectType* currentReturnType;
    
    // 状态跟踪
    bool inReturnStmt;
    llvm::BasicBlock* savedBlock;
    
    // 临时对象跟踪
    std::vector<llvm::Value*> tempObjects;
    
    // 最后生成的表达式信息
    llvm::Value* lastExprValue;
    std::shared_ptr<PyType> lastExprType;
    
    // 验证生成的LLVM IR是否有效
    bool verifyModule();

public:
    // 构造函数
    CodeGenBase();
    CodeGenBase(llvm::Module* mod, llvm::IRBuilder<>* b, llvm::LLVMContext* ctx, ObjectRuntime* rt = nullptr);
    virtual ~CodeGenBase();

        // 添加虚拟方法，用于安全类型转换
        virtual PyCodeGen* asPyCodeGen() { return nullptr; }
    
    // 初始化组件
    void initializeComponents();
    
    // 错误处理
    llvm::Value* logError(const std::string& message, int line = 0, int column = 0);
    llvm::Value* logTypeError(const std::string& message, int line = 0, int column = 0);
    bool logValidationError(const std::string& message, int line = 0, int column = 0);
    
    // 循环控制
    void pushLoopBlocks(llvm::BasicBlock* condBlock, llvm::BasicBlock* afterBlock);
    void popLoopBlocks();
    LoopInfo* getCurrentLoop();
    
    // 基本块管理
    llvm::BasicBlock* createBasicBlock(const std::string& name, llvm::Function* parent = nullptr);
    
    // 临时对象管理
    void addTempObject(llvm::Value* obj, ObjectType* type);
    void releaseTempObjects();
    void clearTempObjects();
    
    // 获取组件接口
    CodeGenExpr* getExprGen() const { return exprGen.get(); }
    CodeGenStmt* getStmtGen() const { return stmtGen.get(); }
    CodeGenModule* getModuleGen() const { return moduleGen.get(); }
    CodeGenType* getTypeGen() const { return typeGen.get(); }
    CodeGenRuntime* getRuntimeGen() const { return runtimeGen.get(); }
    
    // 获取LLVM上下文
    llvm::LLVMContext& getContext() { return *context; }
    llvm::Module* getModule() { return module; }
    llvm::IRBuilder<>& getBuilder() { return *builder; }
    
    // 获取符号表
    PySymbolTable& getSymbolTable() { return symbolTable; }
    
    // 获取/设置当前函数信息
    llvm::Function* getCurrentFunction() const { return currentFunction; }
    void setCurrentFunction(llvm::Function* func) { currentFunction = func; }
    ObjectType* getCurrentReturnType() const { return currentReturnType; }
    void setCurrentReturnType(ObjectType* type) { currentReturnType = type; }
    
    // 获取/设置状态
    bool isInReturnStmt() const { return inReturnStmt; }
    void setInReturnStmt(bool inRet) { inReturnStmt = inRet; }
    llvm::BasicBlock* getSavedBlock() const { return savedBlock; }
    void setSavedBlock(llvm::BasicBlock* block) { savedBlock = block; }
    
    // 获取/设置最后生成的表达式信息
    llvm::Value* getLastExprValue() const { return lastExprValue; }
    void setLastExprValue(llvm::Value* value) { lastExprValue = value; }
    std::shared_ptr<PyType> getLastExprType() const { return lastExprType; }
    void setLastExprType(std::shared_ptr<PyType> type) { lastExprType = type; }
    
    // 实用函数
    llvm::Function* getOrCreateExternalFunction(
        const std::string& name,
        llvm::Type* returnType,
        std::vector<llvm::Type*> paramTypes,
        bool isVarArg = false);
};

} // namespace llvmpy

#endif // CODEGEN_BASE_H