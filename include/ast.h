#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include <functional>
#include <typeindex>
#include <utility> // For std::pair


#include <llvm/IR/Value.h>
#include <llvm/IR/LLVMContext.h>

#include <ObjectType.h>

namespace llvmpy
{
// 前向声明
class PyCodeGen;
class PyType;

// AST节点类型枚举
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
    ClassStmt,
    BinaryExpr,
    UnaryExpr,
    NumberExpr,
    StringExpr,
    VariableExpr,
    CallExpr,
    ListExpr,
    IndexExpr,
    IndexAssignStmt,
    BoolExpr,  // 添加布尔表达式
    NoneExpr,  // 添加None表达式
    DictExpr,      // 添加字典表达式 
    Unknown
};

// 注册器模板 - 用于各种组件的注册管理
template <typename KeyType, typename ValueType>
class Registry
{
public:
    using CreatorFunc = std::function<std::unique_ptr<ValueType>()>;

    static Registry& getInstance()
    {
        static Registry instance;
        return instance;
    }

    template <typename T, typename... Args>
    void registerType(const KeyType& key, Args&&... args)
    {
        creators[key] = [args...]()
        {
            return std::make_unique<T>(args...);
        };
    }

    std::unique_ptr<ValueType> create(const KeyType& key) const
    {
        auto it = creators.find(key);
        if (it != creators.end())
        {
            return it->second();
        }
        return nullptr;
    }

    bool isRegistered(const KeyType& key) const
    {
        return creators.find(key) != creators.end();
    }

private:
    Registry() = default;
    std::unordered_map<KeyType, CreatorFunc> creators;
};

// AST节点工厂
class ASTFactory;

// 基础AST节点类
class ASTNode
{
public:
    virtual ~ASTNode() = default;
    virtual ASTKind kind() const = 0;
    virtual void accept(PyCodeGen& codegen) = 0;

    // 可选的源码信息（用于报错）
    std::optional<int> line;
    std::optional<int> column;

    // 设置源码位置的工具方法
    ASTNode* setLocation(int lineNum, int colNum)
    {
        line = lineNum;
        column = colNum;
        return this;
    }

    // 对象内存管理标记
    bool needsHeapAllocation() const
    {
        return heapAllocation;
    }
    void setHeapAllocation(bool value)
    {
        heapAllocation = value;
    }

protected:
    // 内存管理标志 - 表示该节点的结果需要堆分配
    bool heapAllocation = false;
};

using ASTNodePtr = std::unique_ptr<ASTNode>;

// ======================== Expression Base ========================

// 表达式基类
class ExprAST : public ASTNode
{
public:
    virtual ~ExprAST() = default;
    virtual void accept(PyCodeGen& codegen) override = 0;
    virtual std::shared_ptr<PyType> getType() const = 0;

    // 值复制和引用管理的辅助方法
    virtual bool needsCopy() const
    {
        return false;
    }
    virtual bool isLValue() const
    {
        return false;
    }
};

// ======================== Statement Base ========================

// 语句基类
class StmtAST : public ASTNode
{
public:
    virtual ~StmtAST() = default;
    virtual void accept(PyCodeGen& codegen) override = 0;

    // 资源清理和作用域管理
    virtual void cleanup(PyCodeGen& codegen)
    {
    }
};

// ======================== 基础模板类 ========================

// CRTP基础模板 - 提供kind()方法统一实现
template <typename Derived, ASTKind K>
class ASTNodeBase : public ASTNode
{
public:
    ASTKind kind() const override
    {
        return K;
    }

    // 通过CRTP实现子类获取
    Derived& asDerived()
    {
        return static_cast<Derived&>(*this);
    }
    const Derived& asDerived() const
    {
        return static_cast<const Derived&>(*this);
    }
};

// 表达式基类模板
template <typename Derived, ASTKind K>
class ExprASTBase : public ExprAST
{
public:
    // 实现kind()方法解决抽象类问题
    ASTKind kind() const override
    {
        return K;
    }
     virtual std::shared_ptr<PyType> getType() const override = 0;

    // 类型推断相关的辅助方法
    bool isNumeric() const;
    bool isList() const;
    bool isString() const;
    bool isReference() const;
};

// 语句基类模板
template <typename Derived, ASTKind K>
class StmtASTBase : public StmtAST
{
public:
    // 实现kind()方法解决抽象类问题
    ASTKind kind() const override
    {
        return K;
    }
    // 语句作用域管理
    virtual void beginScope(PyCodeGen& codegen);
    virtual void endScope(PyCodeGen& codegen);
};

// ======================== 具体表达式类 ========================

// 数字字面量
class NumberExprAST : public ExprASTBase<NumberExprAST, ASTKind::NumberExpr>
{
    double value;

public:
    // 添加默认构造函数
    NumberExprAST() : value(0.0)
    {
    }
    NumberExprAST(double val) : value(val)
    {
    }

    double getValue() const 
    {
        return value;
    }
    std::shared_ptr<PyType> getType() const override;
    void accept(PyCodeGen& codegen) override;

    static void registerWithFactory();
};

// 字符串字面量
class StringExprAST : public ExprASTBase<StringExprAST, ASTKind::StringExpr>
{
    std::string value;

public:
    StringExprAST(const std::string& val) : value(val)
    {
    }

    const std::string& getValue() const
    {
        return value;
    }
    std::shared_ptr<PyType> getType() const override;  // 只保留声明，不要内联实现
    void accept(PyCodeGen& codegen) override;

    static void registerWithFactory();
};

// 布尔字面量
class BoolExprAST : public ExprASTBase<BoolExprAST, ASTKind::BoolExpr>
{
    bool value;

public:
    BoolExprAST(bool val) : value(val)
    {
    }

    bool getValue() const
    {
        return value;
    }
    std::shared_ptr<PyType> getType() const override;  // 只保留声明，不要内联实现
    void accept(PyCodeGen& codegen) override;

    static void registerWithFactory();
};

// None值
class NoneExprAST : public ExprASTBase<NoneExprAST, ASTKind::NoneExpr>
{
public:
    NoneExprAST()
    {
    }

    std::shared_ptr<PyType> getType() const override;  // 只保留声明，不要内联实现
    void accept(PyCodeGen& codegen) override;

    static void registerWithFactory();
};

// 变量引用
class VariableExprAST : public ExprASTBase<VariableExprAST, ASTKind::VariableExpr>
{
    std::string name;
    mutable std::shared_ptr<PyType> cachedType;

public:
    // 添加默认构造函数
    VariableExprAST() : name("")
    {
    }
    VariableExprAST(const std::string& n) : name(n)
    {
    }

    const std::string& getName() const
    {
        return name;
    }
    std::shared_ptr<PyType> getType() const override;
    void accept(PyCodeGen& codegen) override;
    bool isLValue() const override
    {
        return true;
    }

    static void registerWithFactory();
};

// 二元运算
class BinaryExprAST : public ExprASTBase<BinaryExprAST, ASTKind::BinaryExpr>
{
    char op;
    std::unique_ptr<ExprAST> lhs, rhs;
    mutable std::shared_ptr<PyType> cachedType;

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
    std::shared_ptr<PyType> getType() const override;
    void accept(PyCodeGen& codegen) override;

    static void registerWithFactory();
};

// 一元操作符
class UnaryExprAST : public ExprASTBase<UnaryExprAST, ASTKind::UnaryExpr>
{
private:
    char opCode;
    std::unique_ptr<ExprAST> operand;
    mutable std::shared_ptr<PyType> cachedType;

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
    std::shared_ptr<PyType> getType() const override;
    void accept(PyCodeGen& codegen) override;

    static void registerWithFactory();
};

// 函数调用
class CallExprAST : public ExprASTBase<CallExprAST, ASTKind::CallExpr>
{
    std::string callee;
    std::vector<std::unique_ptr<ExprAST>> args;
    mutable std::shared_ptr<PyType> cachedType;

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
    std::shared_ptr<PyType> getType() const override;
    void accept(PyCodeGen& codegen) override;
    // 添加缺失的setType方法
    void setType(const std::shared_ptr<PyType>& type)
    {
        cachedType = type;
    }
    bool needsCopy() const override
    {
        return true;
    }  // 函数调用结果通常需要复制

    static void registerWithFactory();
};

// 列表字面量表达式
class ListExprAST : public ExprASTBase<ListExprAST, ASTKind::ListExpr>
{
    std::vector<std::unique_ptr<ExprAST>> elements;
    mutable std::shared_ptr<PyType> cachedType;

public:
    ListExprAST(std::vector<std::unique_ptr<ExprAST>> elems)
        : elements(std::move(elems))
    {
        setHeapAllocation(true);
    }

    const std::vector<std::unique_ptr<ExprAST>>& getElements() const
    {
        return elements;
    }
    std::shared_ptr<PyType> getType() const override;
    void accept(PyCodeGen& codegen) override;
    // 添加 setType 方法
    void setType(std::shared_ptr<PyType> type)
    {
        cachedType = type;
    }
    bool needsCopy() const override
    {
        return false;
    }  // 新创建的列表不需要复制

    static void registerWithFactory();
};




// 字典字面量表达式
/* class DictExprAST : public ExprASTBase<DictExprAST, ASTKind::DictExpr>
{
    std::vector<std::pair<std::unique_ptr<ExprAST>, std::unique_ptr<ExprAST>>> pairs;
    mutable std::shared_ptr<PyType> cachedType;

public:
    // Default constructor for factory
    DictExprAST() = default;

    // Constructor used by parser
    DictExprAST(std::vector<std::pair<std::unique_ptr<ExprAST>, std::unique_ptr<ExprAST>>> p)
        : pairs(std::move(p)) {}

    const std::vector<std::pair<std::unique_ptr<ExprAST>, std::unique_ptr<ExprAST>>>& getPairs() const
    {
        return pairs;
    }

    std::shared_ptr<PyType> getType() const override; // Declaration only
    void accept(PyCodeGen& codegen) override; // Declaration only
    bool needsCopy() const override { return false; } // New dicts don't need copying initially

    static void registerWithFactory(); // Declaration only
}; */



// 索引表达式
class IndexExprAST : public ExprASTBase<IndexExprAST, ASTKind::IndexExpr>
{
    std::unique_ptr<ExprAST> target;
    std::unique_ptr<ExprAST> index;
    mutable std::shared_ptr<PyType> cachedType;

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
    // 添加 setType 方法
    void setType(std::shared_ptr<PyType> type)
    {
        cachedType = type;
    }
    std::shared_ptr<PyType> getType() const override;
    void accept(PyCodeGen& codegen) override;
    bool isLValue() const override
    {
        return true;
    }

    static void registerWithFactory();
};

// ======================== 具体语句类 ========================

// 表达式语句
class ExprStmtAST : public StmtASTBase<ExprStmtAST, ASTKind::ExprStmt>
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
    void accept(PyCodeGen& codegen) override;

    static void registerWithFactory();
};

// 索引赋值语句
class IndexAssignStmtAST : public StmtASTBase<IndexAssignStmtAST, ASTKind::IndexAssignStmt>
{
    std::unique_ptr<ExprAST> target;
    std::unique_ptr<ExprAST> index;
    std::unique_ptr<ExprAST> value;
    std::unique_ptr<ExprAST> targetExpr;  // 可以是IndexExprAST或其他表达式
    std::unique_ptr<ExprAST> valueExpr;

public:
    IndexAssignStmtAST(std::unique_ptr<ExprAST> target,
                       std::unique_ptr<ExprAST> index,
                       std::unique_ptr<ExprAST> value)
        : target(std::move(target)), index(std::move(index)), value(std::move(value))
    {
    }

    // 构造函数 - 只接受索引表达式和值表达式
    IndexAssignStmtAST(std::unique_ptr<ExprAST> targetExpr,
                       std::unique_ptr<ExprAST> valueExpr)
        : targetExpr(std::move(targetExpr)), valueExpr(std::move(valueExpr))
    {
    }

    /*     const ExprAST* getTarget() const
    {
        return target.get();
    }
    const ExprAST* getIndex() const
    {
        return index.get();
    }
    const ExprAST* getValue() const
    {
        return value.get();
    } */
    // 修改这三个getter方法，使其能处理两种构造方式
    const ExprAST* getTarget() const
    {
        if (target)
            return target.get();
        if (targetExpr)
        {
            if (auto indexExpr = dynamic_cast<const IndexExprAST*>(targetExpr.get()))
            {
                return indexExpr->getTarget();
            }
            return targetExpr.get();
        }
        return nullptr;
    }

    const ExprAST* getIndex() const
    {
        if (index)
            return index.get();
        if (targetExpr)
        {
            if (auto indexExpr = dynamic_cast<const IndexExprAST*>(targetExpr.get()))
            {
                return indexExpr->getIndex();
            }
        }
        return nullptr;
    }

    const ExprAST* getValue() const
    {
        if (value)
            return value.get();
        return valueExpr.get();
    }
    void accept(PyCodeGen& codegen) override;

    static void registerWithFactory();
};

// return语句
class ReturnStmtAST : public StmtASTBase<ReturnStmtAST, ASTKind::ReturnStmt>
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
    void accept(PyCodeGen& codegen) override;
    void cleanup(PyCodeGen& codegen) override;

    static void registerWithFactory();
};

// if语句
class IfStmtAST : public StmtASTBase<IfStmtAST, ASTKind::IfStmt>
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
    void accept(PyCodeGen& codegen) override;
    void beginScope(PyCodeGen& codegen) override;
    void endScope(PyCodeGen& codegen) override;

    static void registerWithFactory();
};

// while语句
class WhileStmtAST : public StmtASTBase<WhileStmtAST, ASTKind::WhileStmt>
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
    void accept(PyCodeGen& codegen) override;
    void beginScope(PyCodeGen& codegen) override;
    void endScope(PyCodeGen& codegen) override;

    static void registerWithFactory();
};

// print语句
class PrintStmtAST : public StmtASTBase<PrintStmtAST, ASTKind::PrintStmt>
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
    void accept(PyCodeGen& codegen) override;

    static void registerWithFactory();
};

// 变量赋值语句
class AssignStmtAST : public StmtASTBase<AssignStmtAST, ASTKind::AssignStmt>
{
    std::string name;
    std::unique_ptr<ExprAST> value;

public:
    // 添加默认构造函数
    AssignStmtAST() : name(""), value(nullptr)
    {
    }
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
    void accept(PyCodeGen& codegen) override;

    static void registerWithFactory();
};

// pass语句
class PassStmtAST : public StmtASTBase<PassStmtAST, ASTKind::PassStmt>
{
public:
    PassStmtAST()
    {
    }
    void accept(PyCodeGen& codegen) override;

    static void registerWithFactory();
};

// import语句
class ImportStmtAST : public StmtASTBase<ImportStmtAST, ASTKind::ImportStmt>
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
    void accept(PyCodeGen& codegen) override;

    static void registerWithFactory();
};

// ======================== 函数和模块 ========================

// 函数定义参数
struct ParamAST
{
    std::string name;
    std::string typeName;                  // 类型名称
    std::shared_ptr<PyType> resolvedType;  // 解析后的实际类型
    // 添加默认构造函数
    ParamAST() : name(""), typeName("")
    {
    }
    ParamAST(const std::string& n, const std::string& t) : name(n), typeName(t)
    {
    }
    ParamAST(const std::string& n) : name(n), typeName("")
    {
    }
};

// 函数定义
class FunctionAST : public ASTNodeBase<FunctionAST, ASTKind::Function>
{
public:
    std::string name;

    std::vector<ParamAST> params;
    std::vector<std::unique_ptr<StmtAST>> body;

    std::string returnTypeName;  // 返回类型名
    std::string classContext;    // 函数所属的类名

    mutable std::shared_ptr<PyType> cachedReturnType;
    bool returnTypeResolved = false;
    bool allParamsResolved = false;  // 添加这个字段

    // 添加带参数的构造函数
    FunctionAST(const std::string& name,
                std::shared_ptr<PyType> returnType,
                std::vector<std::pair<std::string, std::shared_ptr<PyType>>> params)
        : name(name), cachedReturnType(returnType)
    {
        for (auto& p : params)
        {
            ParamAST param;
            param.name = p.first;
            param.resolvedType = p.second;
            this->params.push_back(std::move(param));
        }
    }

    // 添加默认构造函数
    FunctionAST() : name(""), returnTypeName("")
    {
    }  // ？returnTypeName("") 合理吗
    FunctionAST(const std::string& name, std::vector<ParamAST> params,
                const std::string& retType, std::vector<std::unique_ptr<StmtAST>> body)
        : name(name), params(std::move(params)), returnTypeName(retType), body(std::move(body))
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
    const std::string& getReturnTypeName() const
    {
        return returnTypeName;
    }

    // 基于函数体分析实际返回类型
    std::shared_ptr<PyType> inferReturnType() const;

    // 解析并获取返回类型 (惰性求值)
    std::shared_ptr<PyType> getReturnType() const;

    // 解析参数类型
    void resolveParamTypes();

    void accept(PyCodeGen& codegen) override;
    // 设置类上下文
    void setClassContext(const std::string& className)
    {
        this->classContext = className;
    }

    // 获取类上下文
    const std::string& getClassContext() const
    {
        return classContext;
    }

    // 判断是否为方法
    bool isMethod() const
    {
        return !classContext.empty();
    }

    static void registerWithFactory();
};

// 类定义
class ClassStmtAST : public StmtASTBase<ClassStmtAST, ASTKind::ClassStmt>
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
    void accept(PyCodeGen& codegen) override;

    static void registerWithFactory();
};

// 整个模块
// 修改 ModuleAST 类定义
class ModuleAST : public ASTNodeBase<ModuleAST, ASTKind::Module>
{
    std::string moduleName;
    std::vector<std::unique_ptr<FunctionAST>> functions;
    std::vector<std::unique_ptr<StmtAST>> statements;  // 保留一个变量

public:
    ModuleAST(const std::string& name,
              std::vector<std::unique_ptr<StmtAST>> stmts,
              std::vector<std::unique_ptr<FunctionAST>> funcs)
        : moduleName(name), functions(std::move(funcs)), statements(std::move(stmts))
    {
    }

    // 默认构造函数
    ModuleAST()
        : moduleName("main"), functions(), statements() {};

    void addFunction(std::unique_ptr<FunctionAST> func);
    void addStatement(std::unique_ptr<StmtAST> stmt);

    const std::string& getModuleName() const
    {
        return moduleName;
    }
    const std::vector<std::unique_ptr<FunctionAST>>& getFunctions() const
    {
        return functions;
    }

    // 统一使用 statements 接口
    const std::vector<std::unique_ptr<StmtAST>>& getStatements() const
    {
        return statements;
    }

    void accept(PyCodeGen& codegen) override;
    static void registerWithFactory();
};

// ======================== AST工厂和注册器 ========================

// AST节点工厂 - 使用单例模式和注册器模式
class ASTFactory
{
public:
    // 获取单例实例
    static ASTFactory& getInstance();

    // 工厂方法 - 创建不同类型的AST节点
    template <typename NodeType, typename... Args>
    std::unique_ptr<NodeType> create(Args&&... args)
    {
        return std::make_unique<NodeType>(std::forward<Args>(args)...);
    }

    // 基于类型ID创建节点的工厂方法
    std::unique_ptr<ASTNode> createNode(ASTKind kind);

    // 注册创建器
    template <typename NodeType>
    void registerNodeCreator(ASTKind kind)
    {
        nodeRegistry[kind] = []()
        { return std::make_unique<NodeType>(); };
    }

    // 类型判断助手
    template <typename T>
    static bool is(const ASTNode* node)
    {
        return node && node->kind() == T::NodeKind;
    }

    // 类型转换助手
    template <typename T>
    static T* as(ASTNode* node)
    {
        return is<T>(node) ? static_cast<T*>(node) : nullptr;
    }

    template <typename T>
    static const T* as(const ASTNode* node)
    {
        return is<T>(node) ? static_cast<const T*>(node) : nullptr;
    }

    // 初始化所有节点类型
    void registerAllNodes();

    // 添加特化工厂函数注册方法
    template <typename NodeType>
    void registerNodeCreator(ASTKind kind, std::function<std::unique_ptr<NodeType>()> creator)
    {
        nodeRegistry[kind] = [creator]() -> std::unique_ptr<ASTNode>
        {
            return creator();
        };
    }

private:
    ASTFactory()
    {
        registerAllNodes();
    }
    ~ASTFactory() = default;

    // 禁止拷贝和赋值
    ASTFactory(const ASTFactory&) = delete;
    ASTFactory& operator=(const ASTFactory&) = delete;

    // 节点创建函数映射
    std::unordered_map<ASTKind, std::function<std::unique_ptr<ASTNode>()>> nodeRegistry;
};

// ======================== 类型系统接口 ========================

// Python类型包装器
class PyType
{
public:
    // 构造函数
    PyType(ObjectType* objType = nullptr);

    std::string toString() const
    {
        if (!objectType) return "unknown";
        return objectType->getName();
    }

    // 获取底层ObjectType对象
    ObjectType* getObjectType()
    {
        return objectType;
    }
    const ObjectType* getObjectType() const
    {
        return objectType;
    }

    // 类型判断方法
    bool isVoid() const;
    bool isInt() const;
    bool isDouble() const;
    bool isNumeric() const;
    bool isBool() const;
    bool isString() const;
    bool isList() const;
    bool isDict() const;
    bool isAny() const;
    bool isReference() const;

    // 类型比较
    bool equals(const PyType& other) const;

    // 工厂方法
    static std::shared_ptr<PyType> getVoid();
    static std::shared_ptr<PyType> getInt();
    static std::shared_ptr<PyType> getDouble();
    static std::shared_ptr<PyType> getBool();
    static std::shared_ptr<PyType> getString();
    static std::shared_ptr<PyType> getAny();
    static std::shared_ptr<PyType> getList(const std::shared_ptr<PyType>& elemType);
    static std::shared_ptr<PyType> getDict(const std::shared_ptr<PyType>& keyType,
                                           const std::shared_ptr<PyType>& valueType);
    static std::shared_ptr<PyType> fromObjectType(ObjectType* objType);

    // 获取列表元素类型的静态方法
    static std::shared_ptr<PyType> getListElementType(const std::shared_ptr<PyType>& listType);

    static std::shared_ptr<PyType> getDictKeyType(const std::shared_ptr<PyType>& dictType);

    // 获取字典值类型
    static std::shared_ptr<PyType> getDictValueType(const std::shared_ptr<PyType>& dictType);

    // 类型解析 - 从字符串解析类型
    static std::shared_ptr<PyType> fromString(const std::string& typeName);

private:
    ObjectType* objectType;
};

// 类型推断助手函数
std::shared_ptr<PyType> inferExprType(const ExprAST* expr);
std::shared_ptr<PyType> inferListElementType(const std::vector<std::unique_ptr<ExprAST>>& elements);
std::shared_ptr<PyType> getCommonType(const std::shared_ptr<PyType>& t1, const std::shared_ptr<PyType>& t2);

// 内存管理助手
class MemoryManager
{
public:
    static void trackObject(PyCodeGen& codegen, llvm::Value* obj);
    static void releaseTrackedObjects(PyCodeGen& codegen);
    static llvm::Value* prepareReturnValue(PyCodeGen& codegen, llvm::Value* value, PyType* type);
};

}  // namespace llvmpy

#endif  // AST_H