/**
 * @file lexer.h
 * @brief 定义了 llvmpy 编译器的词法分析器 (Lexer)。
 *
 * 该文件包含词法分析器所需的所有定义，包括 Token 类型、Token 结构、
 * Token 注册表、配置、错误处理以及核心的 PyLexer 类。
 * 词法分析器负责将输入的 Python 源代码字符串分解成一系列 Token，
 * 供解析器 (Parser) 使用。
 *
 * @note 当前 Lexer 实现将整个源代码一次性 tokenize 到 `tokens` 向量中。
 *       对于非常大的文件，这可能会消耗较多内存。未来可以考虑实现
 *       一个按需生成 Token 的迭代器式 Lexer。
 * @todo 支持更完整的 Python 语法，例如 f-strings, 字节字符串字面量的更多形式,
 *       async/await 关键字等。
 * @todo 考虑将 `PyTokenRegistry` 的初始化逻辑移到 .cpp 文件中。
 */
#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <stdexcept>
#include "Debugdefine.h"  ///< 包含调试相关的宏或定义

namespace llvmpy
{

/**
 * @brief 定义 Python Token 的类型。
 *
 * 使用负数是为了与 Bison/Yacc 等工具兼容，这些工具通常将非负数用于
 * 字符字面量 Token。但是目前看了我并没有用这些工具，而是非常脑瘫地自己写的lexer
 * 对 Token 类型进行了分类，便于管理和扩展。
 */
enum PyTokenType
{
    ///< 基础标记 (-1 到 -9)
    TOK_EOF = -1,      ///< 文件结束
    TOK_NEWLINE = -2,  ///< 换行
    TOK_INDENT = -3,   ///< 缩进
    TOK_DEDENT = -4,   ///< 取消缩进
    TOK_ERROR = -5,    ///< 词法错误
                       /// @note 保留 -6 到 -9 以便未来扩展基础标记
    ///< 保留基础标记位置以便未来扩展

    ///< 关键字 (-10 到 -49)
    TOK_DEF = -10,       ///< def
    TOK_IF = -11,        ///< if
    TOK_ELSE = -12,      ///< else
    TOK_ELIF = -13,      ///< elif
    TOK_RETURN = -14,    ///< return
    TOK_WHILE = -15,     ///< while
    TOK_FOR = -16,       ///< for
    TOK_IN = -17,        ///< in
    TOK_PRINT = -18,     ///< 'print' 关键字 (Python 2 风格, 未来可能移除或改为函数调用)
    TOK_IMPORT = -19,    ///< import
    TOK_CLASS = -20,     ///< class
    TOK_PASS = -21,      ///< pass
    TOK_BREAK = -22,     ///< break
    TOK_CONTINUE = -23,  ///< continue
    TOK_LAMBDA = -24,    ///< lambda
    TOK_TRY = -25,       ///< try
    TOK_EXCEPT = -26,    ///< except
    TOK_FINALLY = -27,   ///< finally
    TOK_WITH = -28,      ///< with
    TOK_AS = -29,        ///< as
    TOK_ASSERT = -30,    ///< assert
    TOK_FROM = -31,      ///< from
    TOK_GLOBAL = -32,    ///< global
    TOK_NONLOCAL = -33,  ///< nonlocal
    TOK_RAISE = -34,     ///< raise
    TOK_YIELD = -35,     ///< yield
    TOK_IS = -36,        ///< is
    TOK_IS_NOT = -37,    ///< is not
    TOK_NOT = -38,       ///< not
    TOK_NOT_IN = -39,    ///< not in
    TOK_AND = -40,       ///< and
    TOK_OR = -41,        ///< or  
    TOK_DEL = -42,       ///< del
    TOK_EXEC = -43,      ///< exec
    TOK_ASYNC = -44,      ///< async
    TOK_AWAIT = -45,     ///< await
    /// @note 缺少 True, False, None (作为字面量处理), and, or, not, is, yield, await, async 等关键字
    ///< 操作符和分隔符 (-50 到 -99)
    TOK_LPAREN = -50,        ///< (
    TOK_RPAREN = -51,        ///< )
    TOK_LBRACK = -52,        ///< [
    TOK_RBRACK = -53,        ///< ]
    TOK_LBRACE = -54,        ///< {
    TOK_RBRACE = -55,        ///< }
    TOK_COLON = -56,         ///< :
    TOK_COMMA = -57,         ///< ,
    TOK_DOT = -58,           ///< .
    TOK_PLUS = -59,          ///< +
    TOK_MINUS = -60,         ///< -
    TOK_MUL = -61,           ///< *
    TOK_DIV = -62,           ///< /
    TOK_MOD = -63,           ///< %
    TOK_LT = -64,            ///< <
    TOK_GT = -65,            ///< >
    TOK_LE = -66,            ///< <=
    TOK_GE = -67,            ///< >=
    TOK_EQ = -68,            ///< ==
    TOK_NEQ = -69,           ///< !=
    TOK_ASSIGN = -70,        ///< =
    TOK_PLUS_ASSIGN = -71,   ///< +=
    TOK_MINUS_ASSIGN = -72,  ///< -=
    TOK_MUL_ASSIGN = -73,    ///< *=
    TOK_DIV_ASSIGN = -74,    ///< /=
    TOK_MOD_ASSIGN = -75,    ///< %=
    TOK_POWER = -76,         ///< **
    TOK_FLOOR_DIV = -77,     ///< ///<
    TOK_ARROW = -78,         ///< ->
    TOK_AT = -79,            ///< @
    // @note 缺少位运算符 (&, |, ^, ~, <<, >>), 矩阵乘法 (@) 等
    // === 标识符和字面量 (-100 到 -149) ===
    TOK_IDENTIFIER = -100,  ///< 标识符 (变量名, 函数名等)
    TOK_NUMBER = -101,      ///< 通用数字 (在解析阶段可能进一步区分为整数或浮点数)
    TOK_INTEGER = -102,     ///< 整数 (例如 123, 0xFF)
    TOK_FLOAT = -103,       ///< 浮点数 (例如 3.14, 1e-5)
    TOK_STRING = -104,      ///< 字符串字面量 (例如 "hello", 'world')
    TOK_BYTES = -105,       ///< 字节字符串字面量 (例如 b"data")
    TOK_BOOL = -106,        ///< 布尔字面量 (True, False) - 由解析器从 IDENTIFIER 转换而来?
    TOK_NONE = -107,        ///< None 字面量 - 由解析器从 IDENTIFIER 转换而来?
                            // @note 字面量处理可以更精细，例如区分不同进制的整数，不同引号的字符串等。
};

/**
 * @brief 表示一个词法单元 (Token)。
 *
 * 包含 Token 的类型、原始值字符串以及其在源代码中的位置信息。
 */
struct PyToken
{
    PyTokenType type;   ///< Token 的类型 (见 PyTokenType 枚举)
    std::string value;  ///< Token 的原始文本值 (例如 "def", "my_var", "123", "+")
    int line;           ///< Token 在源代码中的起始行号 (从 1 开始)
    int column;         ///< Token 在源代码中的起始列号 (从 1 开始)
    char quoteChar = 0; // 新增：存储字符串的引号类型 (' 或 ")，0 表示非字符串

    /**
     * @brief 构造函数。
     * @param t Token 类型。
     * @param v Token 的原始值。
     * @param l Token 的行号。
     * @param c Token 的列号。
     * @param qc Token 的引号类型 (目前仅在复原字符串 Token 中使用)。
     */
     PyToken(PyTokenType t, const std::string& v, size_t l, size_t c, char qc = 0) // 修改构造函数
     : type(t), value(v), line(l), column(c), quoteChar(qc) {}

    /**
     * @brief 默认构造函数。
     * 创建一个表示错误的 Token。
     */
    PyToken() : type(TOK_ERROR), value(""), line(0), column(0)
    {
    }

    /**
     * @brief 生成 Token 的字符串表示，主要用于调试。
     * @return std::string 描述 Token 的字符串。
     */
    std::string toString() const;
};
/**
 * @brief 通用注册表模板类。
 *
 * 提供了一个简单的键值对存储和查找机制。
 * 在 Lexer 中用于管理关键字、操作符等映射关系。
 *
 * @tparam KeyType 键的类型。
 * @tparam ValueType 值的类型。
 */
template <typename KeyType, typename ValueType>
class PyRegistry
{
private:
    std::unordered_map<KeyType, ValueType> items;  ///< 存储键值对的哈希表。

public:
    /**
      * @brief 注册一个键值对。
      * 如果键已存在，则覆盖旧值。
      * @param key 要注册的键。
      * @param value 要注册的值。
      */
    void registerItem(const KeyType& key, ValueType value)
    {
        items[key] = std::move(value);
    }

    /**
      * @brief 检查指定的键是否存在于注册表中。
      * @param key 要检查的键。
      * @return bool 如果键存在则返回 true，否则返回 false。
      */
    bool hasItem(const KeyType& key) const
    {
        return items.find(key) != items.end();
    }

    /**
      * @brief 获取指定键对应的值。
      * @param key 要查找的键。
      * @return ValueType 键对应的值。如果键不存在，则返回 ValueType 的默认构造值。
      */
    ValueType getItem(const KeyType& key) const
    {
        auto it = items.find(key);
        if (it != items.end())
        {
            return it->second;
        }
        // 返回默认值
        return ValueType();
    }

    /**
      * @brief 获取注册表中所有键值对的常量引用。
      * @return const std::unordered_map<KeyType, ValueType>& 包含所有项的哈希表。
      */
    const std::unordered_map<KeyType, ValueType>& getAllItems() const
    {
        return items;
    }

    /**
      * @brief 清空注册表中的所有项。
      */
    void clear()
    {
        items.clear();
    }
};
// 前向声明 PyLexer 类
class PyLexer;

/**
 * @brief Token 处理器函数的类型别名。
 * 定义了处理特定类型 Token 的函数的签名。
 * @param lexer 对当前 PyLexer 实例的引用。
 * @return PyToken 处理后生成的 Token。
 * @note 当前 Lexer 实现未使用此处理器函数机制，而是直接在 `scanToken` 中处理。
 *       未来如果采用更灵活的 Token 处理方式，此类型别名会很有用。
 */
using PyTokenHandlerFunc = std::function<PyToken(PyLexer&)>;

/**
 * @brief 管理 Token 相关信息的注册表类。
 *
 * 这是一个静态类，用于集中存储和查询关键字、操作符、Token 名称等信息。
 * 通过 `initialize()` 方法进行一次性初始化。
 */
class PyTokenRegistry
{
private:
    // 各种 Token 类型的注册表
    /** @brief 存储关键字字符串到 Token 类型的映射。*/
    static PyRegistry<std::string, PyTokenType> keywordRegistry;
    /** @brief 存储 Token 类型到其名称字符串的映射 (用于调试)。*/
    static PyRegistry<PyTokenType, std::string> tokenNameRegistry;
    /** @brief 存储单字符操作符到 Token 类型的映射。*/
    static PyRegistry<char, PyTokenType> simpleOperatorRegistry;
    /** @brief 存储多字符操作符字符串到 Token 类型的映射。*/
    static PyRegistry<std::string, PyTokenType> compoundOperatorRegistry;

    /** @brief Token 处理器注册表 (当前未使用)。*/
    static PyRegistry<PyTokenType, PyTokenHandlerFunc> tokenHandlerRegistry;

    /** @brief 标志，指示注册表是否已初始化。*/
    static bool isInitialized;

public:
    /**
         * @brief 初始化所有 Token 注册表。
         * 填充关键字、操作符、Token 名称等映射。
         * 必须在使用任何注册表功能之前调用一次。
         * @note 应该确保此方法只被调用一次。
         */
    static void initialize();

    /**
         * @brief 注册一个关键字及其对应的 Token 类型。
         * @param keyword 关键字字符串 (例如 "if")。
         * @param type 对应的 PyTokenType (例如 TOK_IF)。
         */
    static void registerKeyword(const std::string& keyword, PyTokenType type);

    /**
         * @brief 注册一个单字符操作符及其对应的 Token 类型。
         * @param op 操作符字符 (例如 '+')。
         * @param type 对应的 PyTokenType (例如 TOK_PLUS)。
         */
    static void registerSimpleOperator(char op, PyTokenType type);

    /**
         * @brief 注册一个多字符操作符及其对应的 Token 类型。
         * @param op 操作符字符串 (例如 "==")。
         * @param type 对应的 PyTokenType (例如 TOK_EQ)。
         */
    static void registerCompoundOperator(const std::string& op, PyTokenType type);

    /**
         * @brief 注册一个 Token 处理器函数 (当前未使用)。
         * @param type 要处理的 Token 类型。
         * @param handler 处理该类型 Token 的函数。
         */
    static void registerTokenHandler(PyTokenType type, PyTokenHandlerFunc handler);

    /**
     * @brief 根据关键字字符串获取其对应的 Token 类型。
     * @param keyword 关键字字符串。
     * @return PyTokenType 对应的 Token 类型。如果不是关键字，则返回默认值 (通常是 TOK_ERROR 或 0)。
     */
    static PyTokenType getKeywordType(const std::string& keyword);

    const std::unordered_map<std::string, PyTokenType>& getKeywords();

    /**
      * @brief 根据单字符操作符获取其对应的 Token 类型。
      * @param op 操作符字符。
      * @return PyTokenType 对应的 Token 类型。如果不是已注册的单字符操作符，则返回默认值。
      */
    static PyTokenType getSimpleOperatorType(char op);

    /**
      * @brief 根据多字符操作符字符串获取其对应的 Token 类型。
      * @param op 操作符字符串。
      * @return PyTokenType 对应的 Token 类型。如果不是已注册的多字符操作符，则返回默认值。
      */
    static PyTokenType getCompoundOperatorType(const std::string& op);

    /**
      * @brief 获取指定 Token 类型的名称字符串 (用于调试)。
      * @param type Token 类型。
      * @return std::string Token 类型的名称 (例如 "TOK_IF", "TOK_IDENTIFIER")。如果未注册，则返回空字符串或 "UNKNOWN"。
      */
    static std::string getTokenName(PyTokenType type);

    /**
     * @brief 获取指定 Token 类型的处理器函数 (当前未使用)。
     * @param type Token 类型。
     * @return PyTokenHandlerFunc 对应的处理器函数。如果未注册，则返回空的 std::function。
     */
    static PyTokenHandlerFunc getTokenHandler(PyTokenType type);

    /**
      * @brief 检查给定的字符串是否是已注册的关键字。
      * @param word 要检查的字符串。
      * @return bool 如果是关键字则返回 true，否则返回 false。
      */
    static bool isKeyword(const std::string& word);

    /**
      * @brief 检查给定的字符是否是已注册的单字符操作符。
      * @param c 要检查的字符。
      * @return bool 如果是单字符操作符则返回 true，否则返回 false。
      */
    static bool isSimpleOperator(char c);

    /**
      * @brief 检查给定的字符是否可能是多字符操作符的起始字符。
      * 用于优化多字符操作符的扫描。
      * @param c 要检查的字符。
      * @return bool 如果可能是多字符操作符的开始则返回 true。
      */
    static bool isCompoundOperatorStart(char c);

    /**
     * @brief 判断两个相邻的 Token 之间是否需要插入空格 (用于源码恢复)。
     * 例如，关键字和标识符之间需要空格，但括号和标识符之间不需要。
     * @param curr 当前 Token 类型。
     * @param next 下一个 Token 类型。
     * @return bool 如果需要空格则返回 true。
     * @note 这个函数主要用于 `recoverSourceFromTokens` 功能。
     */
    static bool needsSpaceBetween(PyTokenType curr, PyTokenType next);
};

/**
 * @brief 词法分析器的配置选项。
 */
struct PyLexerConfig
{
    int tabWidth = 4;                    ///< 一个 Tab 字符代表的空格数，用于缩进计算。??
    bool useTabsForIndent = false;       ///< 是否允许使用 Tab 字符进行缩进 (Python 官方推荐使用空格)。
    bool strictIndentation = true;       ///< 是否强制检查缩进的一致性 (混合使用 Tab 和空格通常是错误的)。
    bool ignoreComments = true;          ///< 是否在词法分析阶段直接忽略注释。
    bool supportTypeAnnotations = true;  ///< 是否识别并处理类型注解语法 (例如 '->' 和 ':')。

    /** @brief 默认构造函数。*/
    PyLexerConfig() = default;
};

/**
 * @brief 词法分析器错误类。
 * 继承自 std::runtime_error，并添加了错误位置信息。
 */
class PyLexerError : public std::runtime_error
{
private:
    int line;    ///< 发生错误的行号。
    int column;  ///< 发生错误的列号。

public:
    /**
         * @brief 构造函数。
         * @param message 错误信息。
         * @param line 错误行号。
         * @param column 错误列号。
         */
    PyLexerError(const std::string& message, int line, int column)
        : std::runtime_error(message), line(line), column(column)
    {
    }

    /** @brief 获取错误发生的行号。*/
    int getLine() const
    {
        return line;
    }
    /** @brief 获取错误发生的列号。*/
    int getColumn() const
    {
        return column;
    }

    /**
         * @brief 格式化错误信息，包含位置。
         * @return std::string 格式化后的错误字符串。
         */
    std::string formatError() const;
};

/**
 * @brief 词法分析器状态。
 * 用于保存和恢复词法分析器的状态，主要用于解析器可能需要的回溯或预读。
 * @note 当前实现仅保存 `tokenIndex`，因为 Lexer 是一次性处理完所有 Token。
 *       如果改为迭代器式 Lexer，则需要保存 `position`, `line`, `column`, `indentStack` 等更详细的状态。
 */
struct PyLexerState
{
    // size_t position; // 当前字符位置 (在迭代器式 Lexer 中需要)
    // int line;       // 当前行号 (在迭代器式 Lexer 中需要)
    // int column;     // 当前列号 (在迭代器式 Lexer 中需要)
    // std::vector<int> indentStack; // 缩进栈状态 (在迭代器式 Lexer 中需要)
    size_t tokenIndex;  ///< 当前读取到的 Token 在 `tokens` 向量中的索引。

    /**
      * @brief 构造函数。
      * @param idx Token 索引，默认为 0。
      */
    PyLexerState(size_t idx = 0) : tokenIndex(idx)
    {
    }
};

/**
 * @brief Python 词法分析器类。
 *
 * 负责将源代码字符串转换为 Token 序列。
 * 处理包括关键字、标识符、数字、字符串、操作符、注释以及 Python 特有的缩进。
 */
class PyLexer
{
private:
    std::string sourceCode;        ///< 要分析的源代码字符串。
    size_t position;               ///< 当前在 `sourceCode` 中的字符索引。
    int currentLine;               ///< 当前处理的行号 (从 1 开始)。
    int currentColumn;             ///< 当前处理的列号 (从 1 开始)。
    int currentIndent;             ///< 当前行的缩进级别 (通常是空格数)。
    std::vector<int> indentStack;  ///< 缩进级别栈，用于检测 INDENT 和 DEDENT。栈底通常是 0。
    std::vector<PyToken> tokens;   ///< 存储词法分析结果的 Token 向量。
    size_t tokenIndex;             ///< 当前通过 `getNextToken()` 返回的 Token 在 `tokens` 向量中的索引。
    PyLexerConfig config;          ///< 词法分析器的配置。

    // === 字符处理方法 ===
    /** @brief 查看当前位置的字符，但不移动位置。*/
    char peek() const;
    /** @brief 查看下一个位置的字符，但不移动位置。*/
    char peekNext() const;
    /** @brief 读取当前位置的字符，并将位置向前移动一个。*/
    char advance();
    /** @brief 如果当前字符匹配 `expected`，则前进一个字符并返回 true，否则返回 false。*/
    bool match(char expected);
    /** @brief 检查是否已到达源代码字符串的末尾。*/
    bool isAtEnd() const;

    // === 空白和注释处理 ===
    /** @brief 跳过当前位置的空白字符 (空格, Tab, 但不包括换行符)。*/
    void skipWhitespace();
    /** @brief 跳过当前位置的注释 (从 '#' 到行尾)。*/
    void skipComment();
    /** @brief 处理行首的缩进，生成 INDENT 或 DEDENT Token。*/
    void processIndentation();
    /** @brief 计算当前行开始处的缩进级别 (空格数)。*/
    int calculateIndent();

    // === 辅助方法 ===
    /** @brief 检查字符是否为数字 '0'-'9'。*/
    bool isDigit(char c) const;
    /** @brief 检查字符是否为字母 'a'-'z', 'A'-'Z' 或下划线 '_'。*/
    bool isAlpha(char c) const;
    /** @brief 检查字符是否为字母、数字或下划线。*/
    bool isAlphaNumeric(char c) const;
    /** @brief 对整个 `sourceCode` 进行词法分析，填充 `tokens` 向量。在构造函数中调用。*/
    void tokenizeSource();

        // === 错误处理 ===
    /**
     * @brief 创建一个表示错误的 Token。
     * @param message 错误信息。
     * @return PyToken 类型为 TOK_ERROR 的 Token。
     */
     PyToken errorToken(const std::string& message);

public:
     /**
     * @brief 构造函数。
     * @param source 要进行词法分析的源代码字符串。
     * @param config 词法分析器配置 (可选)。
     */
     explicit PyLexer(const std::string& source, const PyLexerConfig& config = PyLexerConfig());


       /**
     * @brief 从文件创建 PyLexer 实例。
     * 读取文件内容并构造 Lexer。
     * @param filePath 源代码文件的路径。
     * @param config 词法分析器配置 (可选)。
     * @return PyLexer 构造的词法分析器实例。
     * @throws std::runtime_error 如果文件无法打开或读取。
     */
     static PyLexer fromFile(const std::string& filePath, const PyLexerConfig& config = PyLexerConfig());

    // === Token 扫描方法 ===
    /** @brief 从当前位置扫描并返回下一个 Token。这是核心的 Token 生成逻辑。*/
    PyToken scanToken();
    /** @brief 处理标识符或关键字。*/
    PyToken handleIdentifier();
    /** @brief 处理数字字面量 (整数或浮点数)。*/
    PyToken handleNumber();
    /** @brief 处理字符串字面量 (支持单引号和双引号)。*/
    PyToken handleString();
    /** @brief 处理操作符 (单字符或多字符)。*/
    PyToken handleOperator();
    // === Token 访问 ===
    /**
     * @brief 获取下一个 Token。
     * 从 `tokens` 向量中返回下一个 Token，并移动 `tokenIndex`。
     * @return PyToken 下一个 Token。如果已到达末尾，则返回 TOK_EOF。
     */
     PyToken getNextToken();

     /**
      * @brief 查看当前要返回的 Token，但不移动 `tokenIndex`。
      * @return PyToken 当前 Token。如果已到达末尾，则返回 TOK_EOF。
      */
     PyToken peekToken() const;
 
     /**
      * @brief 查看相对于当前 `tokenIndex` 指定偏移量的 Token。
      * @param offset 偏移量 (0 表示当前 Token, 1 表示下一个, -1 表示上一个)。
      * @return PyToken 指定偏移量的 Token。如果越界，则行为未定义或返回 TOK_ERROR/TOK_EOF。
      */
     PyToken peekTokenAt(size_t offset) const;

    // === 状态管理 ===
    /**
     * @brief 保存当前的词法分析器状态 (主要是 `tokenIndex`)。
     * @return PyLexerState 包含当前状态的对象。
     */
     PyLexerState saveState() const;

     /**
      * @brief 恢复到之前保存的状态。
      * @param state 要恢复到的状态对象。
      */
     void restoreState(const PyLexerState& state);
 
     /**
      * @brief 获取当前字符处理的位置 (主要用于调试或迭代器式 Lexer)。
      * @return size_t 当前在 `sourceCode` 中的字符索引。
      */
     size_t peekPosition() const { return position; }
 
     /**
      * @brief 重置字符处理的位置 (主要用于调试或特殊场景)。
      * @param pos 要设置的字符索引。
      * @warning 谨慎使用，可能破坏 Lexer 状态。
      */
     void resetPosition(size_t pos);

    // === 错误处理 ===
    /**
     * @brief 抛出一个词法分析错误。
     * @param message 错误信息。
     * @throws PyLexerError 包含错误信息和当前位置的异常。
     */
     [[noreturn]] void error(const std::string& message) const;

    // === 源码恢复 (调试功能) ===
    /**
     * @brief 根据分析得到的 Token 序列尝试恢复源代码，并写入文件。
     * 主要用于调试 Lexer 本身。
     * @param filename 要写入恢复后源代码的文件名。
     * @note 恢复的源代码可能不完全符合原始格式，尤其是注释和空格。
     * @todo       自定义报错文件名字
     */
     void recoverSourceFromTokens(const std::string& filename = "recovered_source.py") const;



    // === 调试辅助 ===
    /**
     * @brief 获取包含所有已分析 Token 的向量的常量引用。
     * @return const std::vector<PyToken>& Token 向量。
     */
     const std::vector<PyToken>& getAllTokens() const { return tokens; }

     /**
      * @brief 获取指定 Token 类型的名称字符串 (方便调试)。
      * @param type Token 类型。
      * @return std::string Token 名称。
      */
     std::string getTokenName(PyTokenType type) const { return PyTokenRegistry::getTokenName(type); }
 

    // === 类型注解支持 ===
    /**
     * @brief 检查从指定 Token 位置开始是否存在类型注解语法 (':' 或 '->')。
     * @param tokenPos 起始 Token 在 `tokens` 向量中的索引。
     * @return bool 如果存在类型注解则返回 true。
     * @note 这个功能可能更适合放在 Parser 中处理。
     */
     bool hasTypeAnnotation(size_t tokenPos) const;

     /**
      * @brief 从指定的 Token 位置提取类型注解字符串。
      * @param startPos 起始 Token 在 `tokens` 向量中的索引。
      * @return std::pair<size_t, std::string> 返回包含注解结束位置索引和注解字符串的 pair。
      * @note 这个功能可能更适合放在 Parser 中处理。
      */
     std::pair<size_t, std::string> extractTypeAnnotation(size_t startPos) const;
};

/**
 * @brief 表示源代码中的位置信息。
 * 主要用于错误报告和调试。
 */
 struct PySourceLocation {
    int line;   ///< 行号 (从 1 开始)。
    int column; ///< 列号 (从 1 开始)。

    /**
     * @brief 构造函数。
     * @param l 行号，默认为 0。
     * @param c 列号，默认为 0。
     */
    PySourceLocation(int l = 0, int c = 0) : line(l), column(c) {}

    /**
     * @brief 生成位置信息的字符串表示。
     * @return std::string 格式如 "line X, column Y" 的字符串。
     */
    std::string toString() const {
        return "line " + std::to_string(line) + ", column " + std::to_string(column);
    }
};

}  // namespace llvmpy

#endif  ///< LEXER_H