/**
 * @file ast.h
 * @brief 定义了 llvmpy 编译器的抽象语法树 (Abstract Syntax Tree, AST) 节点。
 *
 * 该文件包含了所有用于表示 Python 代码结构的 AST 节点类定义。
 * 解析器 (Parser) 负责将源代码转换成这些 AST 节点构成的树状结构，
 * 而代码生成器 (CodeGen) 则遍历这棵树来生成 LLVM IR。
 *
 * @note 随着项目支持更复杂的 Python 特性（如类继承、装饰器、异步等），
 *       这里的 AST 节点可能需要扩展或重构。
 * @note 对于多文件项目，模块间的依赖关系和符号解析需要在 AST 层面或
 *       代码生成阶段进行特殊处理。
 * @todo 改名为Ast.h
 */
#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include <functional>
#include <typeindex>
#include <utility>  // For std::pair

#include <llvm/IR/Value.h>
#include <llvm/IR/LLVMContext.h>

#include "lexer.h"
#include <ObjectType.h>

namespace llvmpy
{
// 前向声明
class PyCodeGen;  // 主代码生成器接口，这是一个很混乱的地方，因为应该抛弃旧的PyCodeGen系统，但是在重构CodeGen时应该把PyCodeGen放弃，那么ast.h下层的lexer或者parser可能也需要修故，所以这里非常失败的选择了保留，目前他们之间兼容还算好。接下里会出现大量的PyCodeGen不在赘述。
class PyType;     // 类型系统包装器

/**
 * @brief AST 节点类型的枚举。
 * 用于快速识别和区分不同的 AST 节点。
 */
enum class ASTKind
{
    Module,           ///< 顶层模块节点
    Function,         ///< 函数定义节点
    IfStmt,           ///< if 语句节点
    WhileStmt,        ///< while 语句节点
    ReturnStmt,       ///< return 语句节点
    PrintStmt,        ///< print 语句节点 (可能未来会被函数调用替代)
    AssignStmt,       ///< 变量赋值语句节点
    ExprStmt,         ///< 表达式语句节点 (表达式单独作为一行)
    PassStmt,         ///< pass 语句节点
    ImportStmt,       ///< import 语句节点
    ClassStmt,        ///< class 定义语句节点
    BlockStmt,        ///< 代码块语句节点
    BinaryExpr,       ///< 二元操作表达式节点
    UnaryExpr,        ///< 一元操作表达式节点
    NumberExpr,       ///< 数字字面量表达式节点
    StringExpr,       ///< 字符串字面量表达式节点
    VariableExpr,     ///< 变量引用表达式节点
    CallExpr,         ///< 函数调用表达式节点
    ListExpr,         ///< 列表字面量表达式节点
    IndexExpr,        ///< 索引访问表达式节点 (如 a[0])
    IndexAssignStmt,  ///< 索引赋值语句节点 (如 a[0] = 1)
    BoolExpr,         ///< 布尔字面量表达式节点 (True, False)
    NoneExpr,         ///< None 字面量表达式节点
    DictExpr,         ///< 字典字面量表达式节点
    FunctionDefStmt,  ///< 函数定义语句节点包装
    Unknown           ///< 未知或错误节点类型
};

/**
 * @brief 通用注册器模板类。
 *
 * 提供了一个基于键值对的注册和创建对象的机制。
 * 可用于实现工厂模式等。
 *
 * @tparam KeyType 注册时使用的键类型。
 * @tparam ValueType 被创建的对象的基础类型（通常是基类指针）。
 *
 * @note 对于 AST 节点，`ASTFactory` 提供了更专门的实现。
 *       这个通用注册器可能在项目的其他部分（如运行时类型注册）更有用。
 */
template <typename KeyType, typename ValueType>
class Registry
{
public:
    /** @brief 创建对象的函数类型别名。*/
    using CreatorFunc = std::function<std::unique_ptr<ValueType>()>;
    /**
     * @brief 获取注册器的单例实例。
     * @return Registry<KeyType, ValueType>& 注册器实例的引用。
     */
    static Registry& getInstance()
    {
        static Registry instance;
        return instance;
    }

    /**
     * @brief 注册一个类型及其构造方式。
     * @tparam T 要注册的具体类型，必须是 ValueType 的子类。
     * @tparam Args 构造 T 类型对象所需的参数类型。
     * @param key 用于标识该类型的键。
     * @param args 构造 T 类型对象所需的参数。
     */
    template <typename T, typename... Args>
    void registerType(const KeyType& key, Args&&... args)
    {
        // 使用 lambda 捕获参数来延迟创建
        creators[key] = [args...]()
        {
            return std::make_unique<T>(args...);  // 调用存储的 lambda 函数来创建对象
        };
    }
    /**
     * @brief 根据键创建对象实例。
     * @param key 要创建对象的键。
     * @return std::unique_ptr<ValueType> 创建的对象指针，如果键未注册则返回 nullptr。
     */
    std::unique_ptr<ValueType> create(const KeyType& key) const
    {
        auto it = creators.find(key);
        if (it != creators.end())
        {
            return it->second();
        }
        return nullptr;
    }
    /**
     * @brief 检查指定的键是否已被注册。
     * @param key 要检查的键。
     * @return bool 如果键已注册则返回 true，否则返回 false。
     */
    bool isRegistered(const KeyType& key) const
    {
        return creators.find(key) != creators.end();
    }

private:
    /** @brief 私有构造函数，确保单例模式。*/
    Registry() = default;
    /** @brief 存储键和创建函数映射的哈希表。*/
    std::unordered_map<KeyType, CreatorFunc> creators;
};



/**
 * @brief 所有 AST 节点的基类。
 *
 * 定义了所有 AST 节点共有的接口和属性。
 */
class ASTNode
{
public:
    /** @brief 虚析构函数，允许子类正确销毁。*/
    virtual ~ASTNode() = default;
    /**
     * @brief 获取 AST 节点的具体类型。
     * @return ASTKind 枚举值，表示节点的类型。
     */
    virtual ASTKind kind() const = 0;
    /**
     * @brief 接受访问者（代码生成器）的方法。
     * 这是访问者模式的核心，用于触发特定节点的代码生成逻辑。
     * @param codegen 代码生成器的引用。
     */
    //virtual void accept(PyCodeGen& codegen) = 0;

    /** @brief 可选的源代码行号，用于错误报告。*/
    std::optional<int> line;
    /** @brief 可选的源代码列号，用于错误报告。*/
    std::optional<int> column;

    /**
     * @brief 设置节点的源代码位置信息。
     * @param lineNum 源代码行号。
     * @param colNum 源代码列号。
     * @return ASTNode* 返回当前节点指针，方便链式调用。
     */
    ASTNode* setLocation(int lineNum, int colNum)
    {
        line = lineNum;
        column = colNum;
        return this;
    }
    /**
     * @brief 检查此节点表示的值是否需要在堆上分配内存。
     * @return bool 如果需要在堆上分配则返回 true。
     * @note 这个标志可能与 Python 的对象模型有关（例如，区分栈上的原始类型和堆上的对象）。
     *       未来可能需要更精细的内存管理策略，或者将此信息移到类型系统中。
     * @warning 目前几乎没有用到
     */
    bool needsHeapAllocation() const
    {
        return heapAllocation;
    }
    /**
     * @brief 设置此节点是否需要在堆上分配内存。
     * @param value true 表示需要在堆上分配。
     * @warning 目前几乎没有用到
     */
    void setHeapAllocation(bool value)
    {
        heapAllocation = value;
    }
    /**
     * @brief 获取节点类型的整数表示。
     * @return int 节点类型的整数值。
     */
    int getKind() const
    {
        return static_cast<int>(kind());
    }

protected:
    /**
     * @brief 内存管理标志，指示此节点计算结果是否代表一个需要在堆上分配的 Python 对象。
     * @note 默认为 false。像列表、字典、类实例等通常需要设置为 true。
     * @warning 目前几乎没有用到
     */
    bool heapAllocation = false;
};
/** @brief ASTNode 智能指针的类型别名。*/
using ASTNodePtr = std::unique_ptr<ASTNode>;

// ======================== Expression Base ========================

/**
 * @brief 所有表达式 AST 节点的基类。
 * 表达式是计算并产生值的代码片段。
 */
class ExprAST : public ASTNode
{
public:
    virtual ~ExprAST() = default;
    /**
     * @brief 接受代码生成器访问。
     * @param codegen 代码生成器引用。
     */
    //virtual void accept(PyCodeGen& codegen) override = 0;
    /**
     * @brief 获取表达式计算结果的静态类型。
     * @return std::shared_ptr<PyType> 表示表达式类型的 PyType 对象。
     * @note 类型推断可能在此方法中进行，或者依赖于类型检查阶段的结果。
     */
    virtual std::shared_ptr<PyType> getType() const = 0;

    /**
     * @brief 指示表达式的结果是否需要在使用时进行复制。
     * 通常用于处理函数调用返回值或可能被修改的临时对象。
     * @return bool 如果需要复制则返回 true。
     * @note 这与 Python 的引用语义和可变/不可变类型有关。
     */
    virtual bool needsCopy() const
    {
        return false;
    }
    /**
     * @brief 指示表达式是否可以用作赋值的目标（左值）。
     * 例如，变量名和索引访问是左值，而数字字面量不是。
     * @return bool 如果是左值则返回 true。
     */
    virtual bool isLValue() const
    {
        return false;  // 默认表达式不是左值
    }
};

// ======================== Statement Base ========================

/**
 * @brief 所有语句 AST 节点的基类。
 * 语句是执行操作但不直接产生值的代码单元（尽管它们可能包含表达式）。
 */
class StmtAST : public ASTNode
{
public:
    virtual ~StmtAST() = default;
    /**
     * @brief 接受代码生成器访问。
     * @param codegen 代码生成器引用。
     */
    //virtual void accept(PyCodeGen& codegen) override = 0;

    /**
     * @brief 在语句执行后执行清理操作（例如，管理作用域或资源）。
     * @param codegen 代码生成器引用。
     * @note 目前为空实现，子类可以覆盖以添加特定逻辑。
     */
     // TODO?
    virtual void cleanup(PyCodeGen& codegen)
    {
    }
};

// ======================== 基础模板类 ========================

/**
 * @brief CRTP (Curiously Recurring Template Pattern) 基类模板，用于简化 `kind()` 方法的实现。
 * @tparam Derived 派生类类型。
 * @tparam K 派生类对应的 ASTKind。
 */
template <typename Derived, ASTKind K>
class ASTNodeBase : public ASTNode
{
public:
    /**
     * @brief 实现基类的 `kind()` 方法。
     * @return ASTKind 返回模板参数 K 指定的节点类型。
     */
    ASTKind kind() const override
    {
        return K;
    }

    /**
     * @brief 将基类指针安全地转换为派生类引用。
     * @return Derived& 派生类的引用。
     */
    Derived& asDerived()
    {
        return static_cast<Derived&>(*this);
    }

    /**
     * @brief 将基类指针安全地转换为派生类常量引用。
     * @return const Derived& 派生类的常量引用。
     */
    const Derived& asDerived() const
    {
        return static_cast<const Derived&>(*this);
    }
};

/**
 * @brief 表达式节点的 CRTP 基类模板。
 * 继承自 ExprAST 并使用 ASTNodeBase 实现 `kind()`。
 * @tparam Derived 派生类类型。
 * @tparam K 派生类对应的 ASTKind。
 */
template <typename Derived, ASTKind K>
class ExprASTBase : public ExprAST
{
public:
    /**
     * @brief 实现 `kind()` 方法。
     * @return ASTKind 返回模板参数 K 指定的节点类型。
     */
    ASTKind kind() const override
    {
        return K;
    }
    /**
     * @brief 获取表达式的静态类型（纯虚函数，由具体子类实现）。
     * @return std::shared_ptr<PyType> 表达式的类型。
     */
    virtual std::shared_ptr<PyType> getType() const override = 0;

    // --- 类型相关的辅助方法 (未来可能移至 PyType 或 TypeChecker)唉，反正还没想好 ---
    /** @brief 检查表达式类型是否为数字（int 或 double）。*/
    bool isNumeric() const;
    /** @brief 检查表达式类型是否为列表。*/
    bool isList() const;
    /** @brief 检查表达式类型是否为字符串。*/
    bool isString() const;
    /** @brief 检查表达式类型是否为引用类型（需要引用计数）。*/
    bool isReference() const;
};

/**
 * @brief 语句节点的 CRTP 基类模板。
 * 继承自 StmtAST 并使用 ASTNodeBase 实现 `kind()`。
 * @tparam Derived 派生类类型。
 * @tparam K 派生类对应的 ASTKind。
 */
template <typename Derived, ASTKind K>
class StmtASTBase : public StmtAST
{
public:
    /**
     * @brief 实现 `kind()` 方法。
     * @return ASTKind 返回模板参数 K 指定的节点类型。
     */
    ASTKind kind() const override
    {
        return K;
    }
    /**
     * @brief 在处理语句前开始一个新的作用域（如果需要）。
     * @param codegen 代码生成器引用。
     * @note 主要用于复合语句如 if, while, function, class。
     */
    virtual void beginScope(PyCodeGen& codegen);
    /**
     * @brief 在处理语句后结束当前作用域。
     * @param codegen 代码生成器引用。
     */
    virtual void endScope(PyCodeGen& codegen);
};

// 将定义添加到这里：
template <typename Derived, ASTKind K>
void StmtASTBase<Derived, K>::beginScope(PyCodeGen& codegen)
{
    // 默认实现：什么都不做
    // (void)codegen; // 可选：避免未使用参数警告
}

template <typename Derived, ASTKind K>
void StmtASTBase<Derived, K>::endScope(PyCodeGen& codegen)
{
    // 默认实现：什么都不做
    // (void)codegen; // 可选：避免未使用参数警告
}

// ======================== 具体表达式类 ========================

/**
 * @brief 数字字面量表达式节点 (例如 123, 4.56)。
 */
class NumberExprAST : public ExprASTBase<NumberExprAST, ASTKind::NumberExpr>
{
    double value;  ///< 存储数字的值（统一使用 double）。

public:
    /** @brief 默认构造函数，用于工厂创建。*/
    NumberExprAST() : value(0.0)
    {
    }
    /** @brief 构造函数。*/
    NumberExprAST(double val) : value(val)
    {
    }

    /** @brief 获取数字的值。*/
    double getValue() const
    {
        return value;
    }
    /** @brief 获取表达式类型（int 或 double）。*/
    std::shared_ptr<PyType> getType() const override;

};

/**
 * @brief 字符串字面量表达式节点 (例如 "hello", 'world')。
 */
class StringExprAST : public ExprASTBase<StringExprAST, ASTKind::StringExpr>
{
    std::string value;  ///< 存储字符串的值。

public:
    /** @brief 构造函数。*/
    StringExprAST(const std::string& val) : value(val)
    {
    }

    /** @brief 获取字符串的值。*/
    const std::string& getValue() const
    {
        return value;
    }
    /** @brief 获取表达式类型 (string)。*/
    std::shared_ptr<PyType> getType() const override;

};

/**
 * @brief 布尔字面量表达式节点 (True, False)。
 */
class BoolExprAST : public ExprASTBase<BoolExprAST, ASTKind::BoolExpr>
{
    bool value;  ///< 存储布尔值。

public:
    /** @brief 构造函数。*/
    BoolExprAST(bool val) : value(val)
    {
    }

    /** @brief 获取布尔值。*/
    bool getValue() const
    {
        return value;
    }
    /** @brief 获取表达式类型 (bool)。*/
    std::shared_ptr<PyType> getType() const override;

};

/**
 * @brief None 字面量表达式节点。
 */
class NoneExprAST : public ExprASTBase<NoneExprAST, ASTKind::NoneExpr>
{
public:
    /** @brief 构造函数。*/
    NoneExprAST()
    {
    }

    /** @brief 获取表达式类型 (None)。*/
    std::shared_ptr<PyType> getType() const override;

};

/**
 * @brief 变量引用表达式节点
 * @warning 唉唉几乎没有用到这玩意
 */
class VariableExprAST : public ExprASTBase<VariableExprAST, ASTKind::VariableExpr>
{
    std::string name;  ///< 变量名。
    /** @brief 缓存推断出的变量类型，避免重复查找符号表。*/
    mutable std::shared_ptr<PyType> cachedType;

public:
    /** @brief 默认构造函数，用于工厂创建。*/
    VariableExprAST() : name("")
    {
    }
    /** @brief 构造函数。*/
    VariableExprAST(const std::string& n) : name(n)
    {
    }

    /** @brief 获取变量名。*/
    const std::string& getName() const
    {
        return name;
    }
    /** @brief 获取变量类型（会查询符号表）。*/
    std::shared_ptr<PyType> getType() const override;

    /** @brief 变量引用是左值。*/
    bool isLValue() const override
    {
        return true;
    }


};

/**
 * @brief 二元操作表达式节点 (例如 a + b, x > y)。
 */
class BinaryExprAST : public ExprASTBase<BinaryExprAST, ASTKind::BinaryExpr>
{
    
    PyTokenType opType; // 新增：使用 PyTokenType 表示操作符
    std::unique_ptr<ExprAST> lhs, rhs;  ///< 左操作数。
    /** @brief 缓存推断出的结果类型。*/
    mutable std::shared_ptr<PyType> cachedType;  ///< 右操作数。

public:
    /** @brief 构造函数。*/

    // 更新构造函数以接受 PyTokenType
    BinaryExprAST(PyTokenType op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs)
        : opType(op), lhs(std::move(lhs)), rhs(std::move(rhs))
    {
    }

    /** @brief 获取操作符。*/
    // char getOp() const // 删除旧 getter
    // {
    //     return op;
    // }
    // 更新 getter 以返回 PyTokenType
    PyTokenType getOpType() const
    {
        return opType;
    }
    /** @brief 获取左操作数。*/
    const ExprAST* getLHS() const
    {
        return lhs.get();
    }
    /** @brief 获取右操作数。*/
    const ExprAST* getRHS() const
    {
        return rhs.get();
    }
    /** @brief 获取表达式结果类型。*/
    std::shared_ptr<PyType> getType() const override;

};

/**
 * @brief 一元操作表达式节点 (例如 -x, not y)。
 * @bug 这个地方有很大的问题，甚至连最基础的-和not都无法正常工作
 */
class UnaryExprAST : public ExprASTBase<UnaryExprAST, ASTKind::UnaryExpr>
{
private:
  
    PyTokenType opType; // 新增：使用 PyTokenType 表示操作符
    std::unique_ptr<ExprAST> operand;  ///< 操作数。
    /** @brief 缓存推断出的结果类型。*/
    mutable std::shared_ptr<PyType> cachedType;

public:
     /** @brief 构造函数。*/
   
    // 更新构造函数以接受 PyTokenType
    UnaryExprAST(PyTokenType opcode, std::unique_ptr<ExprAST> operand)
        : opType(opcode), operand(std::move(operand))
    {
    }

     /** @brief 获取操作符。*/
    // char getOpCode() const // 删除旧 getter
    // {
    //     return opCode;
    // }
    // 更新 getter 以返回 PyTokenType
    PyTokenType getOpType() const
    {
        return opType;
    }
    /** @brief 获取操作数。*/
    const ExprAST* getOperand() const
    {
        return operand.get();
    }
    /** @brief 获取表达式结果类型。*/
    std::shared_ptr<PyType> getType() const override;

};

/**
 * @brief 函数调用表达式节点 (例如 func(a, b), obj.method())。
 */
class CallExprAST : public ExprASTBase<CallExprAST, ASTKind::CallExpr>
{
    std::string callee;                          ///< 被调用函数或方法的名称。
    std::vector<std::unique_ptr<ExprAST>> args;  ///< 参数列表。
    /** @brief 缓存推断出的函数返回类型。*/
    mutable std::shared_ptr<PyType> cachedType;

public:
    /** @brief 构造函数。*/
    CallExprAST(const std::string& callee, std::vector<std::unique_ptr<ExprAST>> args)
        : callee(callee), args(std::move(args))
    {
    }

    /** @brief 获取被调用者名称。*/
    const std::string& getCallee() const
    {
        return callee;
    }
    /** @brief 获取参数列表。*/
    const std::vector<std::unique_ptr<ExprAST>>& getArgs() const
    {
        return args;
    }
    /** @brief 获取函数返回类型。*/
    std::shared_ptr<PyType> getType() const override;

    /** @brief 设置推断出的返回类型（由类型检查器调用）。*/
    void setType(const std::shared_ptr<PyType>& type)
    {
        cachedType = type;
    }
    /** @brief 函数调用结果通常需要复制或进行引用计数管理。*/
    bool needsCopy() const override
    {
        return true;
    }


};

/**
 * @brief 列表字面量表达式节点 (例如 [1, 2, "a"])。
 */
class ListExprAST : public ExprASTBase<ListExprAST, ASTKind::ListExpr>
{
    std::vector<std::unique_ptr<ExprAST>> elements;  ///< 列表元素。
    /** @brief 缓存推断出的列表类型 。*/
    mutable std::shared_ptr<PyType> cachedType;

public:
    /** @brief 构造函数。*/
    ListExprAST(std::vector<std::unique_ptr<ExprAST>> elems)
        : elements(std::move(elems))
    {
        setHeapAllocation(true);  // 列表对象总是在堆上分配
    }

    /** @brief 获取列表元素。*/
    const std::vector<std::unique_ptr<ExprAST>>& getElements() const
    {
        return elements;
    }
    /** @brief 获取列表类型。*/
    std::shared_ptr<PyType> getType() const override;

    /** @brief 设置推断出的列表类型（由类型检查器调用）。*/
    void setType(std::shared_ptr<PyType> type)
    {
        cachedType = type;
    }
    /** @brief 新创建的列表对象本身不需要复制（其内容可能需要）。*/
    bool needsCopy() const override
    {
        return false;
    }


};

/**
 * @brief 字典字面量表达式节点 (例如 {"a": 1, "b": 2})。
 */
class DictExprAST : public ExprASTBase<DictExprAST, ASTKind::DictExpr>
{
    /** @brief 存储键值对的向量。*/
    std::vector<std::pair<std::unique_ptr<ExprAST>, std::unique_ptr<ExprAST>>> pairs;
    /** @brief 缓存推断出的字典类型 (例如 Dict<str, int>)。*/
    mutable std::shared_ptr<PyType> cachedType;

public:
    /** @brief 默认构造函数，用于工厂创建。*/
    DictExprAST() = default;
    /** @brief 解析器使用的构造函数。*/
    DictExprAST(std::vector<std::pair<std::unique_ptr<ExprAST>, std::unique_ptr<ExprAST>>> p)
        : pairs(std::move(p))
    {
        setHeapAllocation(true);  // 字典对象总是在堆上分配
    }

    /** @brief 获取键值对列表。*/
    const std::vector<std::pair<std::unique_ptr<ExprAST>, std::unique_ptr<ExprAST>>>& getPairs() const
    {
        return pairs;
    }
    /** @brief 获取字典类型。*/
    std::shared_ptr<PyType> getType() const override;

    /** @brief 新创建的字典对象本身不需要复制。*/
    bool needsCopy() const override
    {
        return false;
    }
    /** @brief 设置推断出的字典类型（由类型检查器调用）。*/
    void setType(std::shared_ptr<PyType> type)
    {
        cachedType = type;
    }


};

/**
 * @brief 索引访问表达式节点 (例如 my_list[i], my_dict[key])。
 */
class IndexExprAST : public ExprASTBase<IndexExprAST, ASTKind::IndexExpr>
{
    std::unique_ptr<ExprAST> target;  ///< 被索引的目标对象 (列表、字典等)。
    std::unique_ptr<ExprAST> index;   ///< 索引值。
    /** @brief 缓存推断出的元素类型。*/
    mutable std::shared_ptr<PyType> cachedType;

public:
    /** @brief 构造函数。*/
    IndexExprAST(std::unique_ptr<ExprAST> target, std::unique_ptr<ExprAST> index)
        : target(std::move(target)), index(std::move(index))
    {
    }

    /** @brief 获取目标对象。*/
    const ExprAST* getTarget() const
    {
        return target.get();
    }
    /** @brief 获取索引值。*/
    const ExprAST* getIndex() const
    {
        return index.get();
    }
    /** @brief 设置推断出的元素类型（由类型检查器调用）。*/
    void setType(std::shared_ptr<PyType> type)
    {
        cachedType = type;
    }
    /** @brief 获取索引访问结果的类型。*/
    std::shared_ptr<PyType> getType() const override;

    /** @brief 索引访问可以作为左值 (例如 my_list[i] = value)。*/
    bool isLValue() const override
    {
        return true;
    }


};

// ======================== 具体语句类 ========================

/**
 * @brief 表达式语句节点。
 * 表示一个表达式单独作为一行语句执行，其结果通常被丢弃。
 */
class ExprStmtAST : public StmtASTBase<ExprStmtAST, ASTKind::ExprStmt>
{
    std::unique_ptr<ExprAST> expr;  ///< 包含的表达式。

public:
    /** @brief 构造函数。*/
    ExprStmtAST(std::unique_ptr<ExprAST> expr) : expr(std::move(expr))
    {
    }

    /** @brief 获取包含的表达式。*/
    const ExprAST* getExpr() const
    {
        return expr.get();
    }

};

/**
 * @brief 索引赋值语句节点 (例如 my_list[i] = value, my_dict[key] = value)。
 */
class IndexAssignStmtAST : public StmtASTBase<IndexAssignStmtAST, ASTKind::IndexAssignStmt>
{
    // @note 这两个构造函数和成员变量的设计有些冗余和混乱，未来可以考虑统一。
    // 理想情况下，targetExpr 应该始终是一个 IndexExprAST。
    std::unique_ptr<ExprAST> target;      ///< (旧) 目标对象。
    std::unique_ptr<ExprAST> index;       ///< (旧) 索引值。
    std::unique_ptr<ExprAST> value;       ///< (旧) 要赋的值。
    std::unique_ptr<ExprAST> targetExpr;  ///< (新) 赋值目标表达式，通常是 IndexExprAST。
    std::unique_ptr<ExprAST> valueExpr;   ///< (新) 要赋的值表达式。

public:
    /** @brief 旧的构造函数。*/
    IndexAssignStmtAST(std::unique_ptr<ExprAST> target,
                       std::unique_ptr<ExprAST> index,
                       std::unique_ptr<ExprAST> value)
        : target(std::move(target)), index(std::move(index)), value(std::move(value))
    {
    }

    /** @brief 新的构造函数，接受目标索引表达式和值表达式。*/
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
    // 修改这三个getter方法，使其能处理两种构造方式？？
    /**
     * @brief 获取赋值的目标对象。
     * 尝试从两种构造方式中提取目标。
     */
    const ExprAST* getTarget() const
    {
        if (target) return target.get();
        if (targetExpr)
        {
            // 如果 targetExpr 是 IndexExprAST，则返回其 target
            if (auto indexExpr = dynamic_cast<const IndexExprAST*>(targetExpr.get()))
            {
                return indexExpr->getTarget();
            }
            // 否则，假设 targetExpr 本身就是目标（虽然这非常逆天，不符合索引赋值的语义）
            return targetExpr.get();
        }
        return nullptr;
    }
    /**
     * @brief 获取赋值的索引值。
     * 尝试从两种构造方式中提取索引。
     */
    const ExprAST* getIndex() const
    {
        if (index) return index.get();
        if (targetExpr)
        {
            // 如果 targetExpr 是 IndexExprAST，则返回其 index
            if (auto indexExpr = dynamic_cast<const IndexExprAST*>(targetExpr.get()))
            {
                return indexExpr->getIndex();
            }
        }
        return nullptr;
    }

    /**
     * @brief 获取要赋的值表达式。
     * 尝试从两种构造方式中提取值。
     */
    const ExprAST* getValue() const
    {
        if (value) return value.get();
        return valueExpr.get();
    }


};

/**
 * @brief return 语句节点。
 */
class ReturnStmtAST : public StmtASTBase<ReturnStmtAST, ASTKind::ReturnStmt>
{
    /** @brief 返回值表达式，可以为 nullptr 表示 `return` 或 `return None`。*/
    std::unique_ptr<ExprAST> value;

public:
    /** @brief 构造函数。*/
    ReturnStmtAST(std::unique_ptr<ExprAST> value) : value(std::move(value))
    {
    }

    /** @brief 获取返回值表达式。*/
    const ExprAST* getValue() const
    {
        return value.get();
    }

    /** @brief return 语句可能需要特殊的清理逻辑（例如，确保返回值被正确处理）。*/
    //void cleanup(PyCodeGen& codegen) override;


};

/**
 * @brief if 语句节点 (包括 elif 和 else)。
 */
// class IfStmtAST : public StmtASTBase<IfStmtAST, ASTKind::IfStmt>
// {
//     std::unique_ptr<ExprAST> condition;              ///< if 条件表达式。
//     std::vector<std::unique_ptr<StmtAST>> thenBody;  ///< if 分支的语句块。
//     std::vector<std::unique_ptr<StmtAST>> elseBody;  ///< else 或 elif 分支的语句块。

// public:
//     /** @brief 构造函数。*/
//     IfStmtAST(std::unique_ptr<ExprAST> cond,
//               std::vector<std::unique_ptr<StmtAST>> thenB,
//               std::vector<std::unique_ptr<StmtAST>> elseB)
//         : condition(std::move(cond)), thenBody(std::move(thenB)), elseBody(std::move(elseB))
//     {
//     }

//     /** @brief 获取条件表达式。*/
//     const ExprAST* getCondition() const
//     {
//         return condition.get();
//     }
//     /** @brief 获取 then 分支语句块。*/
//     const std::vector<std::unique_ptr<StmtAST>>& getThenBody() const
//     {
//         return thenBody;
//     }
//     /** @brief 获取 else/elif 分支语句块。*/
//     const std::vector<std::unique_ptr<StmtAST>>& getElseBody() const
//     {
//         return elseBody;
//     }

//     /** @brief if 语句块开始时可能需要创建新作用域。*/
//     void beginScope(PyCodeGen& codegen) override;
//     /** @brief if 语句块结束时需要销毁作用域。*/
//     void endScope(PyCodeGen& codegen) override;


// };

class IfStmtAST : public StmtASTBase<IfStmtAST, ASTKind::IfStmt> { // <--- 新代码
    std::unique_ptr<ExprAST> condition;
    std::vector<std::unique_ptr<StmtAST>> thenBody;
    std::unique_ptr<StmtAST> elseStmt;

public:
    IfStmtAST(std::unique_ptr<ExprAST> Condition,
              std::vector<std::unique_ptr<StmtAST>> ThenBody,
              std::unique_ptr<StmtAST> ElseStmt)
        : condition(std::move(Condition)),
          thenBody(std::move(ThenBody)),
          elseStmt(std::move(ElseStmt)) {}

    // kind() 方法现在由 StmtASTBase 提供

    ExprAST* getCondition() const { return condition.get(); }
    const std::vector<std::unique_ptr<StmtAST>>& getThenBody() const { return thenBody; }
    StmtAST* getElseStmt() const { return elseStmt.get(); }

};


class BlockStmtAST : public StmtASTBase<BlockStmtAST, ASTKind::BlockStmt> { // <--- 新代码
    std::vector<std::unique_ptr<StmtAST>> statements;
public:
    BlockStmtAST(std::vector<std::unique_ptr<StmtAST>> Stmts)
        : statements(std::move(Stmts)) {}

    // kind() 方法现在由 StmtASTBase 提供，不需要手动实现

    const std::vector<std::unique_ptr<StmtAST>>& getStatements() const { return statements; }

};

/**
 * @brief while 语句节点。
 */
class WhileStmtAST : public StmtASTBase<WhileStmtAST, ASTKind::WhileStmt>
{
private:
    std::unique_ptr<ExprAST> condition;          ///< 循环条件表达式。
    std::vector<std::unique_ptr<StmtAST>> body;  ///< 循环体语句块。
    // @note 不支持 while...else 结构。

public:
    /** @brief 构造函数。*/
    WhileStmtAST(std::unique_ptr<ExprAST> cond, std::vector<std::unique_ptr<StmtAST>> b)
        : condition(std::move(cond)), body(std::move(b))
    {
    }

    /** @brief 获取条件表达式。*/
    const ExprAST* getCondition() const
    {
        return condition.get();
    }
    /** @brief 获取循环体语句块。*/
    const std::vector<std::unique_ptr<StmtAST>>& getBody() const
    {
        return body;
    }

    /** @brief while 循环体开始时需要创建新作用域。*/
    void beginScope(PyCodeGen& codegen) override;
    /** @brief while 循环体结束时需要销毁作用域。*/
    void endScope(PyCodeGen& codegen) override;


};

/**
 * @brief print 语句节点。
 * @todo 这是 Python 2 的语法，print 是 语法级别的关键字它直接在 编译阶段 被处理，不是一个普通的函数或对象，目前这样设计是为了方便调试和立刻看到成效但同时埋下了很多坑，version2到version3做到一半才发现这些问题。在 Python 3 中，print 是一个函数。
 *       未来应考虑将其转换为 `CallExprAST`。
 */
class PrintStmtAST : public StmtASTBase<PrintStmtAST, ASTKind::PrintStmt>
{
    std::unique_ptr<ExprAST> value;  ///< 要打印的表达式。

public:
    /** @brief 构造函数。*/
    PrintStmtAST(std::unique_ptr<ExprAST> value) : value(std::move(value))
    {
    }

    /** @brief 获取要打印的表达式。*/
    const ExprAST* getValue() const
    {
        return value.get();
    }



};

/**
 * @brief 变量赋值语句节点 (例如 x = 10)。
 */
class AssignStmtAST : public StmtASTBase<AssignStmtAST, ASTKind::AssignStmt>
{
    std::string name;                ///< 被赋值的变量名。
    std::unique_ptr<ExprAST> value;  ///< 赋值表达式。
    // @note 不支持多重赋值 (a = b = 1) 或解包赋值 (x, y = t)。

public:
    /** @brief 默认构造函数，用于工厂创建。*/
    AssignStmtAST() : name(""), value(nullptr)
    {
    }
    /** @brief 构造函数。*/
    AssignStmtAST(const std::string& name, std::unique_ptr<ExprAST> value)
        : name(name), value(std::move(value))
    {
    }

    /** @brief 获取变量名。*/
    const std::string& getName() const
    {
        return name;
    }
    /** @brief 获取赋值表达式。*/
    const ExprAST* getValue() const
    {
        return value.get();
    }

};

/**
 * @brief pass 语句节点。
 * 表示一个空操作。
 */
class PassStmtAST : public StmtASTBase<PassStmtAST, ASTKind::PassStmt>
{
public:
    /** @brief 构造函数。*/
    PassStmtAST()
    {
    }

};

/**
 * @brief import 语句节点 (例如 import module, import module as alias)。
 * @warning 目前不支持  import  语法。也不支持 from ... import ... 语法。
 * @todo 正如todo所说，这是需要todo的
 */
class ImportStmtAST : public StmtASTBase<ImportStmtAST, ASTKind::ImportStmt>
{
    std::string moduleName;  ///< 要导入的模块名。
    std::string alias;       ///< 模块的别名 (可选)。

public:
    /** @brief 构造函数。*/
    ImportStmtAST(const std::string& module, const std::string& alias = "")
        : moduleName(module), alias(alias)
    {
    }

    /** @brief 获取模块名。*/
    const std::string& getModuleName() const
    {
        return moduleName;
    }
    /** @brief 获取别名。*/
    const std::string& getAlias() const
    {
        return alias;
    }

};

// ======================== 函数和模块 ========================

/**
 * @brief 函数定义中的参数信息结构。
 */
struct ParamAST
{
    std::string name;      ///< 参数名。
    std::string typeName;  ///< 参数的类型注解字符串 (例如 "int", "str", "List[int]")。
    /** @brief 解析后的实际类型对象。在类型检查阶段填充。*/
    std::shared_ptr<PyType> resolvedType;

    /** @brief 默认构造函数。*/
    ParamAST() : name(""), typeName("")
    {
    }
    /** @brief 带类型注解的构造函数。*/
    ParamAST(const std::string& n, const std::string& t) : name(n), typeName(t)
    {
    }
    /** @brief 不带类型注解的构造函数。*/
    ParamAST(const std::string& n) : name(n), typeName("")
    {
    }  // typeName 为空表示无注解
};





/**
 * @brief 函数定义节点 (def func(...): ...)。
 * @warning 类型推断可能比较复杂，需要处理多个 return 语句和不同分支，干脆交给runtime。这一块做的非常杂乱有的是直接现场分析，有的是直接视为Any让runtime分析。
 */
class FunctionAST : public ASTNodeBase<FunctionAST, ASTKind::Function>
{
public:
    std::string name;  ///< 函数名。

    std::vector<ParamAST> params;                ///< 参数列表。
    std::vector<std::unique_ptr<StmtAST>> body;  ///< 函数体语句块。

    std::string returnTypeName;  ///< 返回类型的注解字符串。
    /** @brief 函数所属的类名，如果为空则表示普通函数。*/
    std::string classContext;

    /** @brief 缓存推断或注解的返回类型。*/
    mutable std::shared_ptr<PyType> cachedReturnType;
    /** @brief 标记返回类型是否已解析。*/
    bool returnTypeResolved = false;
    /** @brief 标记所有参数类型是否已解析。*/
    bool allParamsResolved = false;

    /// @note 以下构造函数签名可能需要统一或改进。
    /** @brief 用于内部或测试的构造函数。*/
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
            // typeName 留空，因为类型已解析
            this->params.push_back(std::move(param));
        }
        // 标记类型已解析
        returnTypeResolved = true;
        allParamsResolved = true;
    }

    /** @brief 默认构造函数，用于工厂创建。*/
    FunctionAST() : name(""), returnTypeName("")
    {
    }
    /** @brief 解析器使用的构造函数。*/
    FunctionAST(const std::string& name, std::vector<ParamAST> params,
                const std::string& retType, std::vector<std::unique_ptr<StmtAST>> body)
        : name(name), params(std::move(params)), returnTypeName(retType), body(std::move(body))
    {
    }

    /** @brief 获取函数名。*/
    const std::string& getName() const
    {
        return name;
    }
    /** @brief 获取参数列表。*/
    const std::vector<ParamAST>& getParams() const
    {
        return params;
    }
    /** @brief 获取函数体语句块。*/
    const std::vector<std::unique_ptr<StmtAST>>& getBody() const
    {
        return body;
    }
    /** @brief 获取返回类型注解字符串。*/
    const std::string& getReturnTypeName() const
    {
        return returnTypeName;
    }

    /**
     * @brief 根据函数体中的 return 语句推断实际返回类型。
     * @return std::shared_ptr<PyType> 推断出的返回类型，可能为 Any 或 None。
     * @warning 类型推断可能比较复杂，需要处理多个 return 语句和不同分支，干脆交给runtime。这一块做的非常杂乱有的是直接现场分析，有的是直接视为Any让runtime分析。
     */
    std::shared_ptr<PyType> inferReturnType() const;

    /**
     * @brief 获取函数的返回类型。
     * 如果有类型注解，则解析注解；否则尝试进行类型推断。
     * 使用缓存避免重复计算。
     * @return std::shared_ptr<PyType> 函数的返回类型。
     */
    std::shared_ptr<PyType> getReturnType() const;

    /**
     * @brief 解析函数参数列表中的类型注解字符串，填充 `resolvedType` 字段。
     * @note 需要类型系统支持从字符串解析类型。
     */
    void resolveParamTypes();



    /**
     * @brief 设置函数所属的类上下文。
     * @param className 类名。
     */
    void setClassContext(const std::string& className)
    {
        this->classContext = className;
    }

    /** @brief 获取函数所属的类上下文。*/
    const std::string& getClassContext() const
    {
        return classContext;
    }

    /**
     * @brief 判断此函数是否为类的方法。
     * @return bool 如果是方法则返回 true。
     */
    bool isMethod() const
    {
        return !classContext.empty();
    }


};



/**
 * @brief 函数定义语句节点。
 *
 * 这个节点包装了一个 FunctionAST，表示一个 'def' 语句。
 * 代码生成器会处理这个节点，在运行时创建函数对象并将其绑定到作用域。
 */
 class FunctionDefStmtAST : public StmtASTBase<FunctionDefStmtAST, ASTKind::FunctionDefStmt> // 需要在 ASTKind 中添加 FunctionDefStmt
 {
     std::unique_ptr<FunctionAST> funcAST;
 
 public:
     /** @brief 构造函数。*/
     FunctionDefStmtAST(std::unique_ptr<FunctionAST> func) : funcAST(std::move(func))
     {
         // 继承 FunctionAST 的位置信息
         if (funcAST && funcAST->line.has_value() && funcAST->column.has_value()) {
              this->setLocation(funcAST->line.value(), funcAST->column.value());
         }
     }
 
     /** @brief 获取包含的 FunctionAST。*/
     const FunctionAST* getFunctionAST() const
     {
         return funcAST.get();
     }
 
  
 };


/**
 * @brief 类定义语句节点 (class MyClass(Base1, Base2): ...)。
 * @todo 正如todo所说，这里是需要todo的，而且应该是需要终点关照的我其实不认为一个简单的class ClassStmtAST就能完成所有的事情 
 */
class ClassStmtAST : public StmtASTBase<ClassStmtAST, ASTKind::ClassStmt>
{
    std::string className;                       ///< 类名。
    std::vector<std::string> baseClasses;        ///< 基类名称列表。
    std::vector<std::unique_ptr<StmtAST>> body;  ///< 类体中的语句（主要是方法定义和属性赋值）。
    /** @brief 类的方法列表 (从 body 中提取或单独存储)。*/
    std::vector<std::unique_ptr<FunctionAST>> methods;
    // @note 当前设计可能需要调整，body 和 methods 的关系需要明确。
    //       类属性的定义和处理也需要考虑。

public:
    /** @brief 构造函数。*/
    ClassStmtAST(const std::string& name,
                 const std::vector<std::string>& bases,
                 std::vector<std::unique_ptr<StmtAST>> body,
                 std::vector<std::unique_ptr<FunctionAST>> methods)
        : className(name), baseClasses(bases), body(std::move(body)), methods(std::move(methods))
    {
    }

    /** @brief 获取类名。*/
    const std::string& getClassName() const
    {
        return className;
    }
    /** @brief 获取基类名称列表。*/
    const std::vector<std::string>& getBaseClasses() const
    {
        return baseClasses;
    }
    /** @brief 获取类体语句块。*/
    const std::vector<std::unique_ptr<StmtAST>>& getBody() const
    {
        return body;
    }
    /** @brief 获取方法列表。*/
    const std::vector<std::unique_ptr<FunctionAST>>& getMethods() const
    {
        return methods;
    }

};

/**
 * @brief 顶层模块节点，代表一个 Python 文件。
 */
class ModuleAST : public ASTNodeBase<ModuleAST, ASTKind::Module>
{
    std::string moduleName;  ///< 模块名称（通常是文件名）。
    /** @brief 模块中定义的函数列表。*/
    std::vector<std::unique_ptr<FunctionAST>> functions;
    /** @brief 模块顶层的语句列表（包括类定义、import、赋值等）。*/
    std::vector<std::unique_ptr<StmtAST>> statements;
    /// @note functions 列表可能冗余，因为 FunctionAST 也是 StmtAST 的子类（通过 FunctionDefStmt?）。
    /// @note     可以考虑只保留 statements 列表。

public:
    /** @brief 构造函数。*/
    ModuleAST(const std::string& name,
              std::vector<std::unique_ptr<StmtAST>> stmts,
              std::vector<std::unique_ptr<FunctionAST>> funcs)
        : moduleName(name), functions(std::move(funcs)), statements(std::move(stmts))
    {
    }

    /** @brief 默认构造函数。*/
    ModuleAST() : moduleName("main") {};

    /**
      * @brief 向模块添加一个函数定义。
      * @param func 函数 AST 节点的 unique_ptr。
      * @note 可能需要更新 statements 列表。
      */
    void addFunction(std::unique_ptr<FunctionAST> func);

    /**
      * @brief 向模块添加一个顶层语句。
      * @param stmt 语句 AST 节点的 unique_ptr。
      */
    void addStatement(std::unique_ptr<StmtAST> stmt);

    /** @brief 获取模块名称。*/
    const std::string& getModuleName() const
    {
        return moduleName;
    }
    /** @brief 获取函数列表。*/
    const std::vector<std::unique_ptr<FunctionAST>>& getFunctions() const
    {
        return functions;
    }
    /** @brief 获取顶层语句列表。*/
    const std::vector<std::unique_ptr<StmtAST>>& getStatements() const
    {
        return statements;
    }


};

// ======================== AST工厂和注册器 ========================



// ======================== 类型系统接口 ========================

/**
 * @brief Python 类型包装器类。
 *
 * 封装了底层的 `ObjectType`，并提供了更方便的类型查询和操作接口。
 * 同时提供了静态工厂方法来获取常用类型的实例。
 * @bug 这个类与 `ObjectType` 的关系复杂，反正问题一大堆。TODO，这一块应该被统一因为涉及到runtime系统
 * @warning 这个类与 `ObjectType` 的关系复杂，反正问题一大堆。TODO
 *       对于用户自定义类型和泛型，当前的工厂方法和 `fromString` 可能不足。
 *       未来可能需要一个全局的类型管理器来处理类型的创建、注册和查找。
 */
class PyType
{
public:
    /**
     * @brief 构造函数。
     * @param objType 底层的 ObjectType 指针，可以为 nullptr。
     */
    PyType(ObjectType* objType = nullptr);
    /**
     * @brief 获取类型的字符串表示。
     * @return std::string 类型的名称 (例如 "int")。唉反人类的设计
     */
    std::string toString() const
    {
        if (!objectType) return "unknown";
        return objectType->getName();
    }

    /** @brief 获取底层的 ObjectType 对象指针。*/
    ObjectType* getObjectType()
    {
        return objectType;
    }
    /** @brief 获取底层的 ObjectType 对象常量指针。*/
    const ObjectType* getObjectType() const
    {
        return objectType;
    }

    // --- 类型判断方法 ---
    bool isVoid() const;       ///< 是否为 Void 类型 (通常用于无返回值的函数)
    bool isInt() const;        ///< 是否为整数类型
    bool isDouble() const;     ///< 是否为浮点数类型
    bool isNumeric() const;    ///< 是否为数字类型 (Int 或 Double)
    bool isBool() const;       ///< 是否为布尔类型
    bool isString() const;     ///< 是否为字符串类型
    bool isList() const;       ///< 是否为列表类型 (泛型基础)
    bool isDict() const;       ///< 是否为字典类型 (泛型基础)
    bool isAny() const;        ///< 是否为 Any 类型
    bool isReference() const;  ///< 是否为引用类型 (需要引用计数)

    /**
     * @brief 比较两个 PyType 是否相等。
     * @param other 要比较的另一个 PyType 对象。
     * @return bool 如果类型相等则返回 true。
     * @todo 需要处理泛型类型（如 List_A == List_B）。可能需要runtime了因为比较类型不一样嘛
     */
    bool equals(const PyType& other) const;

    // --- 静态工厂方法 ---
    static std::shared_ptr<PyType> getVoid();    ///< 获取 Void 类型实例
    static std::shared_ptr<PyType> getInt();     ///< 获取 Int 类型实例
    static std::shared_ptr<PyType> getDouble();  ///< 获取 Double 类型实例
    static std::shared_ptr<PyType> getBool();    ///< 获取 Bool 类型实例
    static std::shared_ptr<PyType> getString();  ///< 获取 String 类型实例
    static std::shared_ptr<PyType> getAny();     ///< 获取 Any 类型实例
    /** @brief 获取列表类型实例 (例如 List<int>)。*/
    static std::shared_ptr<PyType> getList(const std::shared_ptr<PyType>& elemType);
    /** @brief 获取字典类型实例 (例如 Dict<str, int>)。*/
    static std::shared_ptr<PyType> getDict(const std::shared_ptr<PyType>& keyType,
                                           const std::shared_ptr<PyType>& valueType);
    /** @brief 从 ObjectType 创建 PyType 实例。*/
    static std::shared_ptr<PyType> fromObjectType(ObjectType* objType);

    /** @brief 获取列表类型的元素类型。*/
    static std::shared_ptr<PyType> getListElementType(const std::shared_ptr<PyType>& listType);
    /** @brief 获取字典类型的键类型。*/
    static std::shared_ptr<PyType> getDictKeyType(const std::shared_ptr<PyType>& dictType);
    /** @brief 获取字典类型的值类型。*/
    static std::shared_ptr<PyType> getDictValueType(const std::shared_ptr<PyType>& dictType);

    /**
     * @brief 从类型名称字符串解析 PyType。
     * @param typeName 类型名称 (例如 "int")。这一块应该被
     * @return std::shared_ptr<PyType> 解析得到的 PyType 实例。
     * @note 对于用户自定义类型和复杂的泛型，需要更强大的解析逻辑或类型注册表。
     */
    static std::shared_ptr<PyType> fromString(const std::string& typeName);

private:
    /** @brief 指向底层 ObjectType 的指针。*/
    ObjectType* objectType;
    // @note 考虑是否应该使用 shared_ptr<ObjectType> 来管理 ObjectType 的生命周期，
    //       或者由一个全局的 TypeManager 来管理。
};

// ======================== 类型推断和内存管理助手 ========================
/// @bug 这些助手函数/类可能更适合放在类型检查或代码生成的模块中，而不是 ast.h。

/**
 * @brief 推断表达式的类型。
 * @param expr 要推断类型的表达式 AST 节点。
 * @return std::shared_ptr<PyType> 推断出的类型。
 */
/**
 * @brief 推断表达式的类型。
 * @param expr 要推断类型的表达式 AST 节点。
 * @return std::shared_ptr<PyType> 推断出的类型。
 */
std::shared_ptr<PyType> inferExprType(const ExprAST* expr);

/**
  * @brief 推断列表字面量中元素的共同类型。
  * @param elements 列表元素的表达式 AST 节点列表。
  * @return std::shared_ptr<PyType> 推断出的元素类型，如果不兼容则可能为 Any。
  */
std::shared_ptr<PyType> inferListElementType(const std::vector<std::unique_ptr<ExprAST>>& elements);

/**
  * @brief 获取两个类型的最具体共同超类型。
  * @param t1 第一个类型。
  * @param t2 第二个类型。
  * @return std::shared_ptr<PyType> 共同超类型，如果完全不兼容则可能为 Any 或 Object。
  * @note 需要类型继承关系的支持。
  */
std::shared_ptr<PyType> getCommonType(const std::shared_ptr<PyType>& t1, const std::shared_ptr<PyType>& t2);



}  // namespace llvmpy

#endif  // AST_H