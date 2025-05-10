#ifndef CODEGEN_STMT_H
#define CODEGEN_STMT_H

#include "CodeGen/CodeGenBase.h"
//#include "Ast.h"
#include <unordered_map>
#include <set>
#include <memory>
#include <functional>

namespace llvmpy
{

// 语句处理器类型
using StmtHandlerFunc = std::function<void(CodeGenBase&, StmtAST*)>;

// 语句代码生成组件
class CodeGenStmt
{
private:
    CodeGenBase& codeGen;

    // 语句处理器注册表
    static std::unordered_map<ASTKind, StmtHandlerFunc> stmtHandlers;
    static bool handlersInitialized;

    // 处理表达式语句
    void handleExprStmt(ExprStmtAST* stmt);

    // 处理索引赋值语句
    void handleIndexAssignStmt(IndexAssignStmtAST* stmt);

    // 处理return语句
    void handleReturnStmt(ReturnStmtAST* stmt);

    // 处理if语句
    void handleIfStmt(IfStmtAST* stmt);

    // 处理while语句
    void handleWhileStmt(WhileStmtAST* stmt);

    // --- 添加循环目标块的栈 ---
    std::vector<llvm::BasicBlock*> breakTargetStack;     ///< 存储当前循环的 break 跳转目标块 (通常是 endBB)
    std::vector<llvm::BasicBlock*> continueTargetStack;  ///< 存储当前循环的 continue 跳转目标块 (通常是 condBB 或 latch)

    // 处理 break 
    void handleBreakStmt(BreakStmtAST* stmt);     
    // 处理 continue
    void handleContinueStmt(ContinueStmtAST* stmt); 
    void pushBreakTarget(llvm::BasicBlock* target);
    void popBreakTarget();
    void pushContinueTarget(llvm::BasicBlock* target);
    void popContinueTarget();


    // 处理for语句 (如果支持)
    void handleForStmt(const ForStmtAST* stmt); // Added ForStmt handler declaration

    // 处理print语句
    void handlePrintStmt(PrintStmtAST* stmt);

    // 处理变量赋值语句
    void handleAssignStmt(AssignStmtAST* stmt);

    // 处理pass语句
    void handlePassStmt(PassStmtAST* stmt);

    // 处理import语句
    void handleImportStmt(ImportStmtAST* stmt);

    // 处理类定义语句
    void handleClassStmt(ClassStmtAST* stmt);

    // 处理函数定义（包装后的）
    void handleFunctionDefStmt(FunctionDefStmtAST* stmt);  // <--- 添加声明

    // 初始化语句处理器
    static void initializeHandlers();

public:
    CodeGenStmt(CodeGenBase& cg) : codeGen(cg)
    {
        if (!handlersInitialized)
        {
            initializeHandlers();
        }
    }

    // 通用语句处理入口
    void handleStmt(StmtAST* stmt);

    // 处理语句块
    void handleBlock(const std::vector<std::unique_ptr<StmtAST>>& stmts, bool createNewScope /* = true */);

    // 作用域管理
    void beginScope();
    void endScope();

    // 条件语句处理

    llvm::Value* handleCondition(const ExprAST* condition);
    void handleIfStmtRecursive(IfStmtAST* stmt, llvm::BasicBlock* finalMergeBB);
    void generateBranchingCode(llvm::Value* condValue,
                               llvm::BasicBlock* thenBlock,
                               llvm::BasicBlock* elseBlock);

    void handleMethod(FunctionAST* method, llvm::Value* classObj);

    // 循环语句处理
    void handleLoopSetup(llvm::BasicBlock* condBlock,
                         llvm::BasicBlock* bodyBlock,
                         llvm::BasicBlock* afterBlock);

    // 变量声明与赋值
    void declareVariable(const std::string& name,
                         llvm::Value* value,
                         std::shared_ptr<PyType> type);
    void assignVariable(const std::string& name,
                        llvm::Value* value,
                        std::shared_ptr<PyType> valueType);

    // 辅助方法
    CodeGenBase& getCodeGen()
    {
        return codeGen;
    }

    static void findAssignedVariablesInStmt(StmtAST* stmt, std::set<std::string>& assignedVars);
};

}  // namespace llvmpy

#endif  // CODEGEN_STMT_H
