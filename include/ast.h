#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <memory>

namespace llvmpy {

// 前向声明
class CodeGen;
struct Type;

// 基本AST节点
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void accept(CodeGen& codegen) = 0;
};

// 表达式基类
class ExprAST : public ASTNode {
public:
    virtual ~ExprAST() = default;
    virtual void accept(CodeGen& codegen) override = 0;
    virtual std::shared_ptr<Type> getType() const = 0;
};

// 数字字面量
class NumberExprAST : public ExprAST {
    double value;
public:
    NumberExprAST(double val) : value(val) {}
    double getValue() const { return value; }
    void accept(CodeGen& codegen) override;
    std::shared_ptr<Type> getType() const override;
};

// 变量引用
class VariableExprAST : public ExprAST {
    std::string name;
public:
    VariableExprAST(const std::string& n) : name(n) {}
    const std::string& getName() const { return name; }
    void accept(CodeGen& codegen) override;
    std::shared_ptr<Type> getType() const override;
};

// 二元运算
class BinaryExprAST : public ExprAST {
    char op;
    std::unique_ptr<ExprAST> lhs, rhs;
public:
    BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs)
        : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
    char getOp() const { return op; }
    const ExprAST* getLHS() const { return lhs.get(); }
    const ExprAST* getRHS() const { return rhs.get(); }
    void accept(CodeGen& codegen) override;
    std::shared_ptr<Type> getType() const override;
};

// 函数调用
class CallExprAST : public ExprAST {
    std::string callee;
    std::vector<std::unique_ptr<ExprAST>> args;
public:
    CallExprAST(const std::string& callee, std::vector<std::unique_ptr<ExprAST>> args)
        : callee(callee), args(std::move(args)) {}
    const std::string& getCallee() const { return callee; }
    const std::vector<std::unique_ptr<ExprAST>>& getArgs() const { return args; }
    void accept(CodeGen& codegen) override;
    std::shared_ptr<Type> getType() const override;
};

// 语句基类
class StmtAST : public ASTNode {
public:
    virtual ~StmtAST() = default;
    virtual void accept(CodeGen& codegen) override = 0;
};

// 表达式语句
class ExprStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> expr;
public:
    ExprStmtAST(std::unique_ptr<ExprAST> expr) : expr(std::move(expr)) {}
    const ExprAST* getExpr() const { return expr.get(); }
    void accept(CodeGen& codegen) override;
};

// return语句
class ReturnStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> value;
public:
    ReturnStmtAST(std::unique_ptr<ExprAST> value) : value(std::move(value)) {}
    const ExprAST* getValue() const { return value.get(); }
    void accept(CodeGen& codegen) override;
};


// 如果需要一个专门的一元操作符AST类
class UnaryExprAST : public ExprAST
{
private:
    char opCode;
    std::unique_ptr<ExprAST> operand;

public:
    UnaryExprAST(char opcode, std::unique_ptr<ExprAST> operand)
        : opCode(opcode), operand(std::move(operand)) {}

    char getOpCode() const { return opCode; }
    const ExprAST* getOperand() const { return operand.get(); }

    void accept(CodeGen& codegen) override;
    std::shared_ptr<Type> getType() const override;
};

// if语句
class IfStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> condition;
    std::vector<std::unique_ptr<StmtAST>> thenBody;
    std::vector<std::unique_ptr<StmtAST>> elseBody;
public:
    IfStmtAST(std::unique_ptr<ExprAST> cond,
              std::vector<std::unique_ptr<StmtAST>> thenB,
              std::vector<std::unique_ptr<StmtAST>> elseB)
        : condition(std::move(cond)), thenBody(std::move(thenB)), elseBody(std::move(elseB)) {}
    const ExprAST* getCondition() const { return condition.get(); }
    const std::vector<std::unique_ptr<StmtAST>>& getThenBody() const { return thenBody; }
    const std::vector<std::unique_ptr<StmtAST>>& getElseBody() const { return elseBody; }
    void accept(CodeGen& codegen) override;
};

// print语句
class PrintStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> value;
public:
    PrintStmtAST(std::unique_ptr<ExprAST> value) : value(std::move(value)) {}
    const ExprAST* getValue() const { return value.get(); }
    void accept(CodeGen& codegen) override;
};

// 变量赋值语句
class AssignStmtAST : public StmtAST {
    std::string name;
    std::unique_ptr<ExprAST> value;
public:
    AssignStmtAST(const std::string& name, std::unique_ptr<ExprAST> value)
        : name(name), value(std::move(value)) {}
    const std::string& getName() const { return name; }
    const ExprAST* getValue() const { return value.get(); }
    void accept(CodeGen& codegen) override;
};

// 函数定义参数
struct ParamAST {
    std::string name;
    std::string type; // 可能是空的，Python是动态类型的
};

// 函数定义
class FunctionAST : public ASTNode {
    std::string name;
    std::vector<ParamAST> params;
    std::vector<std::unique_ptr<StmtAST>> body;
    std::string returnType; // 可能是空的
public:
    FunctionAST(const std::string& name, std::vector<ParamAST> params,
                std::vector<std::unique_ptr<StmtAST>> body, const std::string& retType)
        : name(name), params(std::move(params)), body(std::move(body)), returnType(retType) {}
    const std::string& getName() const { return name; }
    const std::vector<ParamAST>& getParams() const { return params; }
    const std::vector<std::unique_ptr<StmtAST>>& getBody() const { return body; }
    const std::string& getReturnType() const { return returnType; }
    void accept(CodeGen& codegen) override;
};

// 整个模块
class ModuleAST : public ASTNode {
    std::vector<std::unique_ptr<FunctionAST>> functions;
    std::vector<std::unique_ptr<StmtAST>> topLevelStmts;
public:
    ModuleAST(std::vector<std::unique_ptr<FunctionAST>> funcs,
              std::vector<std::unique_ptr<StmtAST>> stmts)
        : functions(std::move(funcs)), topLevelStmts(std::move(stmts)) {}
    const std::vector<std::unique_ptr<FunctionAST>>& getFunctions() const { return functions; }
    const std::vector<std::unique_ptr<StmtAST>>& getTopLevelStmts() const { return topLevelStmts; }
    void accept(CodeGen& codegen) override;
};

// 在其他语句类定义之后添加
class WhileStmtAST : public StmtAST
{
private:
    std::unique_ptr<ExprAST> condition;
    std::vector<std::unique_ptr<StmtAST>> body;

public:
    WhileStmtAST(std::unique_ptr<ExprAST> cond, std::vector<std::unique_ptr<StmtAST>> b)
        : condition(std::move(cond)), body(std::move(b)) {}

    const ExprAST* getCondition() const { return condition.get(); }
    const std::vector<std::unique_ptr<StmtAST>>& getBody() const { return body; }

    void accept(CodeGen& codegen) override;
};

} // namespace llvmpy

#endif // AST_H