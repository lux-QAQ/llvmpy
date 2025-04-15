#ifndef CODEGEN_VISITOR_H
#define CODEGEN_VISITOR_H

#include "ast.h"
#include "CodeGen/VariableUpdateContext.h"
#include <llvm/IR/Value.h>
#include <memory>
#include <set>

namespace llvmpy
{

// 前向声明
class CodeGenBase;
class ModuleAST;
class FunctionAST;
class StmtAST;
class ExprStmtAST;
class ReturnStmtAST;
class IfStmtAST;
class WhileStmtAST;
class AssignStmtAST;
class IndexAssignStmtAST;
class PrintStmtAST;
class PassStmtAST;
class ImportStmtAST;
class ClassStmtAST;
class DictExprAST;  // <--- 添加 DictExprAST 前向声明
/**
 * 代码生成访问者类 - 实现访问者模式来处理AST节点
 * 
 * 该类将AST节点的处理委托给适当的代码生成组件，通过CodeGenBase访问
 * 各种代码生成器（如表达式、语句、模块等）。
 */
class CodeGenVisitor
{
private:
    CodeGenBase& codeGen;  // 代码生成基础设施引用

public:
    /**
     * 构造函数
     * @param cg 代码生成基础设施引用
     */
    CodeGenVisitor(CodeGenBase& cg);

    // 辅助函数：检查基本块是否有终结器指令
    bool builder_has_terminator(llvm::BasicBlock* block);
    // 辅助函数：为给定类型生成默认值
    llvm::Value* generateDefaultValue(CodeGenBase& codeGen, std::shared_ptr<PyType> type);
    void analyzeExpressionForLoopVars(
            const ExprAST* expr,
            VariableUpdateContext& updateContext,
            llvm::BasicBlock* loopHeader,
            llvm::BasicBlock* entryBlock);

    void findAssignedVariables(StmtAST* stmt, std::set<std::string>& assignedVars);

    //===----------------------------------------------------------------------===//
    // 核心访问方法 - 处理各种AST节点类型
    //===----------------------------------------------------------------------===//

    /**
     * 访问模块节点
     * @param node 模块AST节点
     */
    void visit(ModuleAST* node);

    /**
     * 访问函数定义节点
     * @param node 函数AST节点
     */
    void visit(FunctionAST* node);

    /**
     * 访问表达式语句节点
     * @param node 表达式语句AST节点
     */
    void visit(ExprStmtAST* node);

    /**
     * 访问返回语句节点
     * @param node 返回语句AST节点
     */
    void visit(ReturnStmtAST* node);

    /**
     * 访问条件语句节点
     * @param node if语句AST节点
     */
    void visit(IfStmtAST* node);

    /**
     * 访问循环语句节点
     * @param node while语句AST节点
     */
    void visit(WhileStmtAST* node);

    /**
     * 访问赋值语句节点
     * @param node 赋值语句AST节点
     */
    void visit(AssignStmtAST* node);

    /**
     * 访问索引赋值语句节点
     * @param node 索引赋值语句AST节点
     */
    void visit(IndexAssignStmtAST* node);

    /**
     * 访问打印语句节点
     * @param node 打印语句AST节点
     */
    void visit(PrintStmtAST* node);

    /**
     * 访问pass语句节点
     * @param node pass语句AST节点
     */
    void visit(PassStmtAST* node);

    /**
     * 访问导入语句节点
     * @param node 导入语句AST节点
     */
    void visit(ImportStmtAST* node);

    /**
     * 访问类定义语句节点
     * @param node 类定义语句AST节点
     */
    void visit(ClassStmtAST* node);

    /**
     * 访问通用语句节点 - 根据具体类型分派到对应的visit方法
     * @param node 语句AST节点
     */
    void visit(StmtAST* node);

    /**
     * 访问通用AST节点 - 根据具体类型分派到对应的visit方法
     * @param node AST节点
     */
    void visit(ASTNode* node);

    /**
     * 访问字典表达式节点
     * @param node 字典表达式AST节点
     */
    void visit(DictExprAST* node);  // <--- 添加 visit 声明??
};

}  // namespace llvmpy

#endif  // CODEGEN_VISITOR_H