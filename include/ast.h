#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <memory>
#include <optional>
namespace llvmpy
{
// 前向声明
class CodeGen;
struct Type;

enum class ASTKind
{
    Module,
    Function,
    IfStmt,
    WhileStmt,
    ReturnStmt,
    PrintStmt,
    AssignStmt,
    ExprStmt,
    PassStmt,
    ImportStmt,
    ClassStmt,  // 添加新类型
    BinaryExpr,
    UnaryExpr,
    NumberExpr,
    StringExpr,
    VariableExpr,
    CallExpr,
    ListExpr,    // 列表字面量 [1, 2, 3]
    IndexExpr,   // 索引操作 a[i]
    IndexAssignStmt,  // 新增：索引赋值语句
    Unknown

};






class ASTNode
{
public:
    virtual ~ASTNode() = default;
    virtual ASTKind kind() const = 0;
    virtual void accept(CodeGen& codegen) = 0;

    // 可选的源码信息（用于报错）
    std::optional<int> line;
    std::optional<int> column;
};

using ASTNodePtr = std::unique_ptr<ASTNode>;

// ======================== Expression Base ========================

// 表达式基类
class ExprAST : public ASTNode
{
public:
    virtual ~ExprAST() = default;
    virtual void accept(CodeGen& codegen) override = 0;
    virtual std::shared_ptr<Type> getType() const = 0;
};
// ======================== Literal: Number ========================
// 数字字面量
class NumberExprAST : public ExprAST
{
    double value;

public:
    NumberExprAST(double val) : value(val)
    {
    }
    double getValue() const
    {
        return value;
    }
    std::shared_ptr<Type> getType() const override;
    void accept(CodeGen& codegen) override;
    ASTKind kind() const override
    {
        return ASTKind::NumberExpr;
    }
};




// 变量引用类的修改
class VariableExprAST : public ExprAST
{
    std::string name;

public:
    VariableExprAST(const std::string& n) : name(n)
    {
    }
    const std::string& getName() const
    {
        return name;
    }
    void accept(CodeGen& codegen) override;
    std::shared_ptr<Type> getType() const override;
    ASTKind kind() const override
    {
        return ASTKind::VariableExpr;
    }  // 添加这一行
};

// 二元运算
// 二元运算类的修改
class BinaryExprAST : public ExprAST
{
    char op;
    std::unique_ptr<ExprAST> lhs, rhs;

public:
    BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs)
        : op(op), lhs(std::move(lhs)), rhs(std::move(rhs))
    {
    }
    char getOp() const
    {
        return op;
    }
    const ExprAST* getLHS() const
    {
        return lhs.get();
    }
    const ExprAST* getRHS() const
    {
        return rhs.get();
    }
    void accept(CodeGen& codegen) override;
    std::shared_ptr<Type> getType() const override;
    ASTKind kind() const override
    {
        return ASTKind::BinaryExpr;
    }  // 添加这一行
};

// 函数调用类的修改
class CallExprAST : public ExprAST
{
    std::string callee;
    std::vector<std::unique_ptr<ExprAST>> args;

public:
    CallExprAST(const std::string& callee, std::vector<std::unique_ptr<ExprAST>> args)
        : callee(callee), args(std::move(args))
    {
    }
    const std::string& getCallee() const
    {
        return callee;
    }
    const std::vector<std::unique_ptr<ExprAST>>& getArgs() const
    {
        return args;
    }
    void accept(CodeGen& codegen) override;
    std::shared_ptr<Type> getType() const override;
    ASTKind kind() const override
    {
        return ASTKind::CallExpr;
    }  // 添加这一行
};
// ======================== Statement Base ========================
// 语句基类
class StmtAST : public ASTNode
{
public:
    virtual ~StmtAST() = default;
    virtual void accept(CodeGen& codegen) override = 0;
};
// ======================== Return ========================
// 表达式语句类的修改
class ExprStmtAST : public StmtAST
{
    std::unique_ptr<ExprAST> expr;

public:
    ExprStmtAST(std::unique_ptr<ExprAST> expr) : expr(std::move(expr))
    {
    }
    const ExprAST* getExpr() const
    {
        return expr.get();
    }
    void accept(CodeGen& codegen) override;
    ASTKind kind() const override
    {
        return ASTKind::ExprStmt;
    }  // 添加这一行
};
// 新增索引赋值语句类型
class IndexAssignStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> target;
    std::unique_ptr<ExprAST> index;
    std::unique_ptr<ExprAST> value;

public:
    IndexAssignStmtAST(std::unique_ptr<ExprAST> target, 
                      std::unique_ptr<ExprAST> index,
                      std::unique_ptr<ExprAST> value)
        : target(std::move(target)), index(std::move(index)), value(std::move(value)) {}
    
    const ExprAST* getTarget() const { return target.get(); }
    const ExprAST* getIndex() const { return index.get(); }
    const ExprAST* getValue() const { return value.get(); }
    
    void accept(CodeGen& codegen) override;
    ASTKind kind() const override { return ASTKind::IndexAssignStmt; }
};
// return语句类的修改
class ReturnStmtAST : public StmtAST
{
    std::unique_ptr<ExprAST> value;

public:
    ReturnStmtAST(std::unique_ptr<ExprAST> value) : value(std::move(value))
    {
    }
    const ExprAST* getValue() const
    {
        return value.get();
    }
    void accept(CodeGen& codegen) override;
    ASTKind kind() const override
    {
        return ASTKind::ReturnStmt;
    }  // 添加这一行
};

// 一元操作符类的修改
class UnaryExprAST : public ExprAST
{
private:
    char opCode;
    std::unique_ptr<ExprAST> operand;

public:
    UnaryExprAST(char opcode, std::unique_ptr<ExprAST> operand)
        : opCode(opcode), operand(std::move(operand))
    {
    }

    char getOpCode() const
    {
        return opCode;
    }
    const ExprAST* getOperand() const
    {
        return operand.get();
    }

    void accept(CodeGen& codegen) override;
    std::shared_ptr<Type> getType() const override;
    ASTKind kind() const override
    {
        return ASTKind::UnaryExpr;
    }  // 添加这一行
};

// if语句类的修改
class IfStmtAST : public StmtAST
{
    std::unique_ptr<ExprAST> condition;
    std::vector<std::unique_ptr<StmtAST>> thenBody;
    std::vector<std::unique_ptr<StmtAST>> elseBody;

public:
    IfStmtAST(std::unique_ptr<ExprAST> cond,
              std::vector<std::unique_ptr<StmtAST>> thenB,
              std::vector<std::unique_ptr<StmtAST>> elseB)
        : condition(std::move(cond)), thenBody(std::move(thenB)), elseBody(std::move(elseB))
    {
    }
    const ExprAST* getCondition() const
    {
        return condition.get();
    }
    const std::vector<std::unique_ptr<StmtAST>>& getThenBody() const
    {
        return thenBody;
    }
    const std::vector<std::unique_ptr<StmtAST>>& getElseBody() const
    {
        return elseBody;
    }
    void accept(CodeGen& codegen) override;
    ASTKind kind() const override
    {
        return ASTKind::IfStmt;
    }  // 添加这一行
};

// print语句类的修改
class PrintStmtAST : public StmtAST
{
    std::unique_ptr<ExprAST> value;

public:
    PrintStmtAST(std::unique_ptr<ExprAST> value) : value(std::move(value))
    {
    }
    const ExprAST* getValue() const
    {
        return value.get();
    }
    void accept(CodeGen& codegen) override;
    ASTKind kind() const override
    {
        return ASTKind::PrintStmt;
    }  // 添加这一行
};

// 变量赋值语句类的修改
class AssignStmtAST : public StmtAST
{
    std::string name;
    std::unique_ptr<ExprAST> value;

public:
    AssignStmtAST(const std::string& name, std::unique_ptr<ExprAST> value)
        : name(name), value(std::move(value))
    {
    }
    const std::string& getName() const
    {
        return name;
    }
    const ExprAST* getValue() const
    {
        return value.get();
    }
    void accept(CodeGen& codegen) override;
    ASTKind kind() const override
    {
        return ASTKind::AssignStmt;
    }  // 添加这一行
};
// ======================== Function ========================
// 函数定义参数
struct ParamAST
{
    std::string name;
    std::string type;  // 可能是空的，Python是动态类型的
    
    // 添加构造函数
    ParamAST(const std::string& n, const std::string& t) : name(n), type(t) {}
};

// 函数定义
class FunctionAST : public ASTNode
{
    std::string name;
    std::vector<ParamAST> params;
    std::vector<std::unique_ptr<StmtAST>> body;
    std::string returnType;  // 可能是空的
public:
    FunctionAST(const std::string& name, std::vector<ParamAST> params,
                const std::string& retType, std::vector<std::unique_ptr<StmtAST>> body)
        : name(name), params(std::move(params)), returnType(retType), body(std::move(body))
    {
    }
    const std::string& getName() const
    {
        return name;
    }
    const std::vector<ParamAST>& getParams() const
    {
        return params;
    }
    const std::vector<std::unique_ptr<StmtAST>>& getBody() const
    {
        return body;
    }
    const std::string& getReturnType() const
    {
        return returnType;
    }
    void accept(CodeGen& codegen) override;
    ASTKind kind() const override
    {
        return ASTKind::Function;
    }
};
// ======================== Module ========================
// 整个模块
class ModuleAST : public ASTNode
{
    std::vector<std::unique_ptr<FunctionAST>> functions;
    std::vector<std::unique_ptr<StmtAST>> topLevelStmts;

public:
    ModuleAST(std::vector<std::unique_ptr<StmtAST>> stmts,
              std::vector<std::unique_ptr<FunctionAST>> funcs)
        : functions(std::move(funcs)), topLevelStmts(std::move(stmts))
    {
    }
    const std::vector<std::unique_ptr<FunctionAST>>& getFunctions() const
    {
        return functions;
    }
    const std::vector<std::unique_ptr<StmtAST>>& getTopLevelStmts() const
    {
        return topLevelStmts;
    }
    void accept(CodeGen& codegen) override;
    ASTKind kind() const override
    {
        return ASTKind::Module;
    }
};


// 列表字面量表达式
class ListExprAST : public ExprAST
{
    std::vector<std::unique_ptr<ExprAST>> elements;

public:
    ListExprAST(std::vector<std::unique_ptr<ExprAST>> elems)
        : elements(std::move(elems))
    {
    }
    
    const std::vector<std::unique_ptr<ExprAST>>& getElements() const
    {
        return elements;
    }
    
    void accept(CodeGen& codegen) override;
    std::shared_ptr<Type> getType() const override;
    ASTKind kind() const override
    {
        return ASTKind::ListExpr;
    }
};

// 索引表达式
class IndexExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> target;
    std::unique_ptr<ExprAST> index;

public:
    IndexExprAST(std::unique_ptr<ExprAST> target, std::unique_ptr<ExprAST> index)
        : target(std::move(target)), index(std::move(index))
    {
    }
    
    const ExprAST* getTarget() const
    {
        return target.get();
    }
    
    const ExprAST* getIndex() const
    {
        return index.get();
    }
    
    void accept(CodeGen& codegen) override;
    std::shared_ptr<Type> getType() const override;
    ASTKind kind() const override
    {
        return ASTKind::IndexExpr;
    }
};


// 在其他语句类定义之后添加
// while语句类的修改
class WhileStmtAST : public StmtAST
{
private:
    std::unique_ptr<ExprAST> condition;
    std::vector<std::unique_ptr<StmtAST>> body;

public:
    WhileStmtAST(std::unique_ptr<ExprAST> cond, std::vector<std::unique_ptr<StmtAST>> b)
        : condition(std::move(cond)), body(std::move(b))
    {
    }

    const ExprAST* getCondition() const
    {
        return condition.get();
    }
    const std::vector<std::unique_ptr<StmtAST>>& getBody() const
    {
        return body;
    }

    void accept(CodeGen& codegen) override;
    ASTKind kind() const override
    {
        return ASTKind::WhileStmt;
    }  // 添加这一行
};

// 添加pass语句类
class PassStmtAST : public StmtAST
{
public:
    PassStmtAST()
    {
    }
    void accept(CodeGen& codegen) override;
    ASTKind kind() const override
    {
        return ASTKind::PassStmt;
    }
};

class EmptyStmtAST : public StmtAST
{
public:
    EmptyStmtAST()
    {
    }
    void accept(CodeGen& codegen) override
    {
    }
    ASTKind kind() const override
    {
        return ASTKind::Unknown;
    }
};

// 添加导入语句类
class ImportStmtAST : public StmtAST
{
    std::string moduleName;
    std::string alias;

public:
    ImportStmtAST(const std::string& module, const std::string& alias = "")
        : moduleName(module), alias(alias)
    {
    }
    const std::string& getModuleName() const
    {
        return moduleName;
    }
    const std::string& getAlias() const
    {
        return alias;
    }
    void accept(CodeGen& codegen) override;
    ASTKind kind() const override
    {
        return ASTKind::ImportStmt;
    }
};

// 添加类定义语句类
class ClassStmtAST : public StmtAST
{
    std::string className;
    std::vector<std::string> baseClasses;
    std::vector<std::unique_ptr<StmtAST>> body;
    std::vector<std::unique_ptr<FunctionAST>> methods;

public:
    ClassStmtAST(const std::string& name,
                 const std::vector<std::string>& bases,
                 std::vector<std::unique_ptr<StmtAST>> body,
                 std::vector<std::unique_ptr<FunctionAST>> methods)
        : className(name), baseClasses(bases), body(std::move(body)), methods(std::move(methods))
    {
    }

    const std::string& getClassName() const
    {
        return className;
    }
    const std::vector<std::string>& getBaseClasses() const
    {
        return baseClasses;
    }
    const std::vector<std::unique_ptr<StmtAST>>& getBody() const
    {
        return body;
    }
    const std::vector<std::unique_ptr<FunctionAST>>& getMethods() const
    {
        return methods;
    }
    void accept(CodeGen& codegen) override;
    ASTKind kind() const override
    {
        return ASTKind::ClassStmt;
    }
};

}  // namespace llvmpy

#endif  // AST_H