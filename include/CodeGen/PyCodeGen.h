#ifndef PY_CODEGEN_H
#define PY_CODEGEN_H

#include "CodeGen/CodeGenBase.h"
#include "CodeGen/CodeGenExpr.h"
#include "CodeGen/CodeGenStmt.h"
#include "CodeGen/CodeGenModule.h"
#include "CodeGen/CodeGenType.h"
#include "CodeGen/CodeGenRuntime.h"
#include "ast.h"
#include <llvm/IR/Value.h>

namespace llvmpy
{

// 主代码生成器类 - 使用所有组件提供统一接口
class PyCodeGen : public CodeGenBase
{  // 修改为继承自 CodeGenBase
private:
    // 是否拥有所有LLVM对象
    bool ownsLLVMObjects;

public:
    // 构造函数 - 创建所有LLVM对象
    PyCodeGen();

    // 构造函数 - 使用现有LLVM对象
    PyCodeGen(llvm::Module* mod, llvm::IRBuilder<>* b, llvm::LLVMContext* ctx, ObjectRuntime* rt = nullptr);

    // 析构函数 - 清理资源
    ~PyCodeGen();

    // 覆盖基类方法，实现安全类型转换
    virtual PyCodeGen* asPyCodeGen() override
    {
        return this;
    }

    // 推入新作用域
    void pushScope()
    {
        getSymbolTable().pushScope();
    }

    // 弹出当前作用域
    void popScope()
    {
        getSymbolTable().popScope();
    }

    llvm::Value* prepareAssignmentTarget(llvm::Value* value, ObjectType* targetType,const ExprAST* expr);
    // 生成AST节点代码
    llvm::Value* codegen(ASTNode* node);

    // 表达式处理
    llvm::Value* codegenExpr(const ExprAST* expr);

    // 语句处理
    void codegenStmt(StmtAST* stmt);

    // 生成模块代码
    bool generateModule(ModuleAST* module, const std::string& filename = "output.ll");

    // 获取组件
    CodeGenBase* getBase() const
    {
        return const_cast<PyCodeGen*>(this);
    }  // 返回 this
    CodeGenExpr* getExprGen() const;
    CodeGenStmt* getStmtGen() const;
    CodeGenModule* getModuleGen() const;
    CodeGenType* getTypeGen() const;
    CodeGenRuntime* getRuntimeGen() const;

    // 简化访问方法
    llvm::LLVMContext& getContext();
    llvm::Module* getModule();
    llvm::IRBuilder<>& getBuilder();

    // 状态查询方法
    llvm::Function* getCurrentFunction();
    ObjectType* getCurrentReturnType();

    // 表达式结果方法
    llvm::Value* getLastExprValue();
    std::shared_ptr<PyType> getLastExprType();

    // 错误处理
    llvm::Value* logError(const std::string& message, int line = 0, int column = 0);

    // 兼容旧版接口 - 这些将转发到相应的组件
    llvm::Value* handleNode(ASTNode* node);
    llvm::Value* handleExpr(const ExprAST* expr);
    void handleStmt(StmtAST* stmt);
    llvm::Value* handleBinOp(char op, llvm::Value* L, llvm::Value* R,
                             ObjectType* leftType, ObjectType* rightType);

    // 返回运行时对象接口
    ObjectRuntime& getRuntime()
    {
        auto* runtime = getRuntimeGen()->getRuntime();
        if (!runtime)
        {
            throw std::runtime_error("Runtime not initialized");
        }
        return *runtime;
    }

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

}  // namespace llvmpy

#endif  // PY_CODEGEN_H