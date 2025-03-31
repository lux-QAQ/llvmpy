#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "ObjectType.h"
#include "ObjectRuntime.h"
#include "TypeIDs.h"
#include "TypeOperations.h"
#include "ObjectLifecycle.h"

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
    bool isTypeError;

public:
    PyCodeGenError(const std::string& message, int line = -1, int column = -1, bool isTypeError = false)
        : std::runtime_error(message), line(line), column(column), isTypeError(isTypeError) {}

    std::string formatError() const;
    int getLine() const { return line; }
    int getColumn() const { return column; }
    bool isTypeErr() const { return isTypeError; }
};

// ===----------------------------------------------------------------------===//
// 类型安全管理器 - 处理类型验证和转换
// ===----------------------------------------------------------------------===//

class TypeSafetyManager {
public:
    // 验证表达式类型
    static bool validateExprType(PyCodeGen& codegen, ExprAST* expr, int expectedTypeId);
    
    // 验证索引操作
    static bool validateIndexOperation(PyCodeGen& codegen, ExprAST* target, ExprAST* index);
    
    // 验证赋值操作
    static bool validateAssignment(PyCodeGen& codegen, const std::string& varName, ExprAST* value);
    
    // 验证列表元素赋值
    static bool validateListAssignment(PyCodeGen& codegen, ObjectType* listElemType, ExprAST* value);
    
    // 获取运行时类型ID
    static int getRuntimeTypeId(ObjectType* type);
    
    // 检查类型兼容性
    static bool areTypesCompatible(int typeIdA, int typeIdB);
    
    // 生成类型检查代码
    static llvm::Value* generateTypeCheck(PyCodeGen& codegen, llvm::Value* obj, int expectedTypeId);
    
    // 生成类型错误消息
    static llvm::Value* generateTypeError(PyCodeGen& codegen, llvm::Value* obj, int expectedTypeId);
};

// ===----------------------------------------------------------------------===//
// 对象生命周期管理器包装 - 简化与代码生成器的集成
// ===----------------------------------------------------------------------===//

class CodeGenLifecycleManager {
public:
    // 确定表达式对象的来源
    static ObjectLifecycleManager::ObjectSource determineObjectSource(ExprAST* expr);
    
    // 确定对象的目标用途
    static ObjectLifecycleManager::ObjectDestination determineObjectDestination(
        bool isReturnValue, bool isAssignTarget, bool isParameter);
    
    // 处理生成的表达式值
    static llvm::Value* handleExpressionValue(
        PyCodeGen& codegen, 
        llvm::Value* value, 
        ExprAST* expr, 
        bool isReturnValue = false, 
        bool isAssignTarget = false,
        bool isParameter = false);
    
    // 准备函数返回值
    static llvm::Value* prepareReturnValue(
        PyCodeGen& codegen, 
        llvm::Value* value, 
        ObjectType* returnType,
        ExprAST* expr);
    
    // 准备赋值目标
    static llvm::Value* prepareAssignmentTarget(
        PyCodeGen& codegen, 
        llvm::Value* value, 
        ObjectType* targetType,
        ExprAST* expr);
    
    // 准备函数参数
    static llvm::Value* prepareParameter(
        PyCodeGen& codegen, 
        llvm::Value* value, 
        ObjectType* paramType,
        ExprAST* expr);
    
    // 清理临时对象
    static void cleanupTemporaryObjects(PyCodeGen& codegen);

        // 将类型信息附加到LLVM值
        static void attachTypeMetadata(llvm::Value* value, int typeId);
    
        // 从LLVM值获取类型信息
        static int getTypeIdFromMetadata(llvm::Value* value);

      

};

// ===----------------------------------------------------------------------===//
// 操作代码生成管理器 - 简化与TypeOperations的集成
// ===----------------------------------------------------------------------===//

class CodeGenOperationManager {
public:
    // 处理二元操作
    static llvm::Value* handleBinaryOperation(
        PyCodeGen& codegen,
        char op,
        llvm::Value* leftValue,
        llvm::Value* rightValue,
        ObjectType* leftType,
        ObjectType* rightType);
    
    // 处理一元操作
    static llvm::Value* handleUnaryOperation(
        PyCodeGen& codegen,
        char op,
        llvm::Value* operandValue,
        ObjectType* operandType);
    
    // 处理索引操作
    static llvm::Value* handleIndexOperation(
        PyCodeGen& codegen,
        llvm::Value* targetValue,
        llvm::Value* indexValue,
        ObjectType* targetType,
        ObjectType* indexType);
    
    // 处理类型转换
    static llvm::Value* handleTypeConversion(
        PyCodeGen& codegen,
        llvm::Value* value,
        ObjectType* fromType,
        ObjectType* toType);
    
    // 获取操作结果类型
    static ObjectType* getOperationResultType(
        char op,
        ObjectType* leftType,
        ObjectType* rightType);
};

// ===----------------------------------------------------------------------===//
// 主代码生成器类
// ===----------------------------------------------------------------------===//

class PyCodeGen {
private:
    // LLVM核心组件
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    
    
    // 集成ObjectRuntime
    std::unique_ptr<ObjectRuntime> runtime;
    
    // 符号表和作用域管理
    PySymbolTable symbolTable;

    // 当前函数上下文
    llvm::Function* currentFunction;
    ObjectType* currentReturnType;
    
    // 最近生成的表达式值
    llvm::Value* lastExprValue;
    std::shared_ptr<PyType> lastExprType;
    
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
    // 函数缓存 - 避免重复创建相同的函数
    std::unordered_map<std::string, llvm::Function*> externalFunctions;
    // 处理函数注册表
    static PyCodeGenRegistry<ASTKind, PyNodeHandlerFunc> nodeHandlers;
    static PyCodeGenRegistry<ASTKind, PyExprHandlerFunc> exprHandlers;
    static PyCodeGenRegistry<ASTKind, PyStmtHandlerFunc> stmtHandlers;
    static PyCodeGenRegistry<char, PyBinOpHandlerFunc> binOpHandlers;
    
    // 类型安全检查器
    std::unique_ptr<TypeSafetyManager> typeSafetyManager;
    
    // 生命周期管理器
    std::unique_ptr<CodeGenLifecycleManager> lifecycleManager;
    
    // 操作管理器
    std::unique_ptr<CodeGenOperationManager> operationManager;
    
    // 静态初始化标志
    static bool isInitialized;
    
    // 初始化处理函数注册表
    static void initializeHandlers();
    
    
    
    // 类型错误处理
    llvm::Value* logTypeError(const std::string& message, int line = -1, int column = -1);
    
    

public:
std::unique_ptr<llvm::IRBuilder<>> builder;
    PyCodeGen();
    ~PyCodeGen() = default;

    // 从 ObjectRuntime 构造
    PyCodeGen(llvm::Module* mod, llvm::IRBuilder<>* b, llvm::LLVMContext* ctx, ObjectRuntime* rt = nullptr);

    // 确保注册表已初始化
    static void ensureInitialized();

    // 代码生成入口点
    bool generateModule(ModuleAST* module, const std::string& filename = "output.ll");

    // 错误处理
    llvm::Value* logError(const std::string& message, int line = -1, int column = -1);
    

    
    // 辅助函数

    void trackObject(llvm::Value* obj) ;
  
    llvm::Function* getOrCreateExternalFunction(
        const std::string& name,
        llvm::Type* returnType,
        std::vector<llvm::Type*> paramTypes,
        bool isVarArg = false);
     // 新增帮助方法处理索引赋值
    void performIndexAssignment(const ExprAST* target, const ExprAST* index, 
        const ExprAST* valueExpr, const StmtAST* stmt);

        llvm::Value* createDefaultValue(ObjectType* type);

    // 访问器方法
    llvm::LLVMContext& getContext() { return context ? *context : runtime->getContext(); }
    llvm::Module* getModule() { return module ? module.get() : runtime->getModule(); }
    llvm::IRBuilder<>& getBuilder() { 
        return builder ? *builder : *(runtime->getBuilder()); 
    }
    ObjectRuntime& getRuntime() { 
        if (!runtime) {
            throw std::runtime_error("Runtime is not initialized");
        }
        return *runtime; 
    }
    
    // 获取运行时
    ObjectRuntime* getRuntimePtr() const { return runtime.get(); }
    void setRuntime(ObjectRuntime* rt) { runtime.reset(rt); }
    
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

    llvm::Value* handleIndexOperation(llvm::Value* target, llvm::Value* index,
        ObjectType* targetType, ObjectType* indexType);
        llvm::Value* ensurePythonObject(llvm::Value* value, ObjectType* type);
    
    // 处理不同AST节点的接口方法
    llvm::Value* handleNode(ASTNode* node);
    llvm::Value* handleExpr(ExprAST* expr);
    void handleStmt(StmtAST* stmt);
    llvm::Value* handleBinOp(char op, llvm::Value* L, llvm::Value* R, ObjectType* leftType, ObjectType* rightType);
    llvm::Value* handleUnaryOp(char op, llvm::Value* operand, ObjectType* operandType);
    
    // 类型安全操作
    bool validateExprType(ExprAST* expr, int expectedTypeId);
    bool validateIndexOperation(ExprAST* target, ExprAST* index);
    bool validateAssignment(const std::string& varName, ExprAST* value);
    llvm::Value* generateTypeCheck(llvm::Value* obj, int expectedTypeId);
    llvm::Value* generateTypeError(llvm::Value* obj, int expectedTypeId);
    
    // 对象生命周期管理
    llvm::Value* handleExpressionValue(llvm::Value* value, ExprAST* expr, 
                                     bool isReturnValue = false, 
                                     bool isAssignTarget = false,
                                     bool isParameter = false);
    llvm::Value* prepareReturnValue(llvm::Value* value, ObjectType* returnType, ExprAST* expr);
    llvm::Value* prepareAssignmentTarget(llvm::Value* value, ObjectType* targetType, ExprAST* expr);
    llvm::Value* prepareParameter(llvm::Value* value, ObjectType* paramType, ExprAST* expr);
    
    // 操作代码生成
    llvm::Value* generateBinaryOperation(char op, llvm::Value* L, llvm::Value* R, 
                                       ObjectType* leftType, ObjectType* rightType);
    llvm::Value* generateUnaryOperation(char op, llvm::Value* operand, ObjectType* operandType);
    llvm::Value* generateIndexOperation(llvm::Value* target, llvm::Value* index,
                                      ObjectType* targetType, ObjectType* indexType);
    llvm::Value* generateTypeConversion(llvm::Value* value, ObjectType* fromType, ObjectType* toType);
    
    void debugFunctionReuse(const std::string& name, llvm::Function* func) ;


    // 委托对象创建方法给运行时
    llvm::Value* createIntObject(llvm::Value* value) {
        return runtime ? runtime->createIntObject(value) : nullptr;
    }
    
    llvm::Value* createDoubleObject(llvm::Value* value) {
        return runtime ? runtime->createDoubleObject(value) : nullptr;
    }
    
    llvm::Value* createBoolObject(llvm::Value* value) {
        return runtime ? runtime->createBoolObject(value) : nullptr;
    }
    
    llvm::Value* createStringObject(llvm::Value* value) {
        return runtime ? runtime->createStringObject(value) : nullptr;
    }
    
    // 获取ObjectType对应的LLVM类型
    llvm::Type* getLLVMType(ObjectType* type);
    
    // Visitor模式接口实现 - AST 节点访问方法
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
                                             
    // 类型ID转换帮助函数
    int getTypeIdFromObjectType(ObjectType* type);
    int getRuntimeTypeId(ObjectType* type);
    int getBaseTypeId(int typeId);
    
    // 类型检查帮助函数
    llvm::Value* generateTypeCheckCode(PyCodeGen& codegen, llvm::Value* obj, int expectedTypeId);
    llvm::Value* generateTypeErrorCode(PyCodeGen& codegen, llvm::Value* obj, int expectedTypeId, const std::string& message = "");
    
    // 特殊处理"Expected type 5, got 1"错误
    llvm::Value* generateListIndexTypeCheck(PyCodeGen& codegen, llvm::Value* target, llvm::Value* index);
    
    // 生成对象生命周期管理代码
    llvm::Value* generateIncRefCode(PyCodeGen& codegen, llvm::Value* obj);
    llvm::Value* generateDecRefCode(PyCodeGen& codegen, llvm::Value* obj);
    llvm::Value* generateCopyObjectCode(PyCodeGen& codegen, llvm::Value* obj, ObjectType* type);
}

// ===----------------------------------------------------------------------===//
// 扩展类型注册表管理
// ===----------------------------------------------------------------------===//

// 类型注册表管理器 - 管理代码生成相关的类型信息
class CodeGenTypeRegistry {
private:
    static std::unordered_map<int, std::string> typeNames;
    static std::unordered_map<std::string, int> typeIds;
    static std::unordered_map<int, llvm::Type*> llvmTypes;
    
public:
    // 初始化类型表
    static void initialize(llvm::LLVMContext& context);
    
    // 获取类型名称
    static std::string getTypeName(int typeId);
    
    // 获取类型ID
    static int getTypeId(const std::string& typeName);
    
    // 获取LLVM类型
    static llvm::Type* getLLVMType(llvm::LLVMContext& context, int typeId);
    
    // 注册类型
    static void registerType(int typeId, const std::string& name, llvm::Type* llvmType = nullptr);
    
    // 检查类型兼容性
    static bool areTypesCompatible(int typeIdA, int typeIdB);
};

}  // namespace llvmpy

#endif  // CODEGEN_H