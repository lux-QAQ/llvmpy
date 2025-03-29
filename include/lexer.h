#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <stdexcept>
#include "Debugdefine.h"

namespace llvmpy {

// 明确的Token类型分类，便于扩展
enum PyTokenType {
    // 基础标记 (-1 到 -9)
    TOK_EOF = -1,      // 文件结束
    TOK_NEWLINE = -2,  // 换行
    TOK_INDENT = -3,   // 缩进
    TOK_DEDENT = -4,   // 取消缩进
    TOK_ERROR = -5,    // 词法错误
    
    // 保留基础标记位置以便未来扩展

    // 关键字 (-10 到 -49)
    TOK_DEF = -10,     // def
    TOK_IF = -11,      // if
    TOK_ELSE = -12,    // else
    TOK_ELIF = -13,    // elif
    TOK_RETURN = -14,  // return
    TOK_WHILE = -15,   // while
    TOK_FOR = -16,     // for
    TOK_IN = -17,      // in
    TOK_PRINT = -18,   // print
    TOK_IMPORT = -19,  // import
    TOK_CLASS = -20,   // class
    TOK_PASS = -21,    // pass
    TOK_BREAK = -22,   // break
    TOK_CONTINUE = -23,// continue
    TOK_LAMBDA = -24,  // lambda
    TOK_TRY = -25,     // try
    TOK_EXCEPT = -26,  // except
    TOK_FINALLY = -27, // finally
    TOK_WITH = -28,    // with
    TOK_AS = -29,      // as
    TOK_ASSERT = -30,  // assert
    TOK_FROM = -31,    // from
    TOK_GLOBAL = -32,  // global
    TOK_NONLOCAL = -33,// nonlocal
    TOK_RAISE = -34,   // raise
    
    // 操作符和分隔符 (-50 到 -99)
    TOK_LPAREN = -50,  // (
    TOK_RPAREN = -51,  // )
    TOK_LBRACK = -52,  // [
    TOK_RBRACK = -53,  // ]
    TOK_LBRACE = -54,  // {
    TOK_RBRACE = -55,  // }
    TOK_COLON = -56,   // :
    TOK_COMMA = -57,   // ,
    TOK_DOT = -58,     // .
    TOK_PLUS = -59,    // +
    TOK_MINUS = -60,   // -
    TOK_MUL = -61,     // *
    TOK_DIV = -62,     // /
    TOK_MOD = -63,     // %
    TOK_LT = -64,      // <
    TOK_GT = -65,      // >
    TOK_LE = -66,      // <=
    TOK_GE = -67,      // >=
    TOK_EQ = -68,      // ==
    TOK_NEQ = -69,     // !=
    TOK_ASSIGN = -70,  // =
    TOK_PLUS_ASSIGN = -71,   // +=
    TOK_MINUS_ASSIGN = -72,  // -=
    TOK_MUL_ASSIGN = -73,    // *=
    TOK_DIV_ASSIGN = -74,    // /=
    TOK_MOD_ASSIGN = -75,    // %=
    TOK_POWER = -76,         // **
    TOK_FLOOR_DIV = -77,     // //
    TOK_ARROW = -78,         // ->
    TOK_AT = -79,            // @
    
    // 标识符和字面量 (-100 到 -149)
    TOK_IDENTIFIER = -100,   // 标识符
    TOK_NUMBER = -101,       // 通用数字
    TOK_INTEGER = -102,      // 整数
    TOK_FLOAT = -103,        // 浮点数
    TOK_STRING = -104,       // 字符串
    TOK_BYTES = -105,        // 字节字符串
    TOK_BOOL = -106,         // 布尔值
    TOK_NONE = -107,         // None
};

// Token 结构
struct PyToken {
    PyTokenType type;
    std::string value;
    int line;
    int column;

    PyToken(PyTokenType t, const std::string& v, int l, int c)
        : type(t), value(v), line(l), column(c) {}
    
    // 默认构造函数
    PyToken() : type(TOK_ERROR), value(""), line(0), column(0) {}
    
    // 用于调试的友好字符串表示
    std::string toString() const;
};

// 通用注册表模板 - 与Parser使用相同设计
template<typename KeyType, typename ValueType>
class PyRegistry {
private:
    std::unordered_map<KeyType, ValueType> items;

public:
    // 注册项
    void registerItem(const KeyType& key, ValueType value) {
        items[key] = std::move(value);
    }

    // 检查是否存在
    bool hasItem(const KeyType& key) const {
        return items.find(key) != items.end();
    }

    // 获取项
    ValueType getItem(const KeyType& key) const {
        auto it = items.find(key);
        if (it != items.end()) {
            return it->second;
        }
        // 返回默认值
        return ValueType();
    }
    
    // 获取所有项
    const std::unordered_map<KeyType, ValueType>& getAllItems() const {
        return items;
    }
    
    // 清空注册表
    void clear() {
        items.clear();
    }
};

// Token处理器类型
using PyTokenHandlerFunc = std::function<PyToken(class PyLexer&)>;

// Token注册和管理类
class PyTokenRegistry {
private:
    // 各种Token类型的注册表
    static PyRegistry<std::string, PyTokenType> keywordRegistry;
    static PyRegistry<PyTokenType, std::string> tokenNameRegistry;
    static PyRegistry<char, PyTokenType> simpleOperatorRegistry;
    static PyRegistry<std::string, PyTokenType> compoundOperatorRegistry;
    
    // Token处理器注册表
    static PyRegistry<PyTokenType, PyTokenHandlerFunc> tokenHandlerRegistry;
    
    // 已初始化标志
    static bool isInitialized;

public:
    // 初始化注册表
    static void initialize();
    
    // 注册一个关键字
    static void registerKeyword(const std::string& keyword, PyTokenType type);
    
    // 注册一个简单操作符
    static void registerSimpleOperator(char op, PyTokenType type);
    
    // 注册一个复合操作符
    static void registerCompoundOperator(const std::string& op, PyTokenType type);
    
    // 注册一个Token处理器
    static void registerTokenHandler(PyTokenType type, PyTokenHandlerFunc handler);
    
    // 获取关键字对应的TokenType
    static PyTokenType getKeywordType(const std::string& keyword);
    
    // 获取简单操作符对应的TokenType
    static PyTokenType getSimpleOperatorType(char op);
    
    // 获取复合操作符对应的TokenType
    static PyTokenType getCompoundOperatorType(const std::string& op);
    
    // 获取TokenType对应的名称
    static std::string getTokenName(PyTokenType type);
    
    // 获取Token处理器
    static PyTokenHandlerFunc getTokenHandler(PyTokenType type);
    
    // 检查是否是关键字
    static bool isKeyword(const std::string& word);
    
    // 检查是否是简单操作符
    static bool isSimpleOperator(char c);
    
    // 检查是否是可能的复合操作符的开始
    static bool isCompoundOperatorStart(char c);
    
    // 获取所有关键字映射
    static const std::unordered_map<std::string, PyTokenType>& getKeywords();
    
    // 判断两个token之间是否需要空格
    static bool needsSpaceBetween(PyTokenType curr, PyTokenType next);
};

// 词法分析器配置
struct PyLexerConfig {
    int tabWidth = 4;                // Tab宽度
    bool useTabsForIndent = false;   // 是否使用Tab作为缩进
    bool strictIndentation = true;   // 是否强制检查一致的缩进
    bool ignoreComments = true;      // 是否忽略注释
    bool supportTypeAnnotations = true; // 是否支持类型注解
    
    PyLexerConfig() = default;
};

// 词法分析器错误
class PyLexerError : public std::runtime_error {
private:
    int line;
    int column;
    
public:
    PyLexerError(const std::string& message, int line, int column)
        : std::runtime_error(message), line(line), column(column) {}
    
    int getLine() const { return line; }
    int getColumn() const { return column; }
    
    std::string formatError() const;
};

// 词法分析器状态 - 用于保存和恢复状态
struct PyLexerState {
    size_t position;
    int line;
    int column;
    size_t tokenIndex;
    
    PyLexerState(size_t pos = 0, int l = 1, int c = 1, size_t idx = 0)
        : position(pos), line(l), column(c), tokenIndex(idx) {}
};

// 词法分析器类
class PyLexer {
private:
    std::string sourceCode;          // 源代码
    size_t position;                 // 当前位置
    int currentLine;                 // 当前行号
    int currentColumn;               // 当前列号
    int currentIndent;               // 当前缩进级别
    std::vector<int> indentStack;    // 缩进栈
    std::vector<PyToken> tokens;     // 已处理的tokens
    size_t tokenIndex;               // 当前读取的token索引
    PyLexerConfig config;            // 配置

    // 字符处理
    char peek() const;               // 查看当前字符
    char peekNext() const;           // 查看下一个字符
    char advance();                  // 前进一个字符
    bool match(char expected);       // 匹配并前进
    bool isAtEnd() const;            // 是否到达源码结尾
    
    // 空白处理
    void skipWhitespace();           // 跳过空白
    void skipComment();              // 跳过注释
    void processIndentation();       // 处理缩进/取消缩进
    int calculateIndent();           // 计算当前行的缩进级别
    

    
    // 辅助方法
    bool isDigit(char c) const;      // 是否是数字
    bool isAlpha(char c) const;      // 是否是字母
    bool isAlphaNumeric(char c) const; // 是否是字母或数字
    void tokenizeSource();           // 将整个源码标记化
    
    // 错误处理
    PyToken errorToken(const std::string& message);

public:
    // 构造函数
    explicit PyLexer(const std::string& source, const PyLexerConfig& config = PyLexerConfig());
    
    // 从文件构造
    static PyLexer fromFile(const std::string& filePath, const PyLexerConfig& config = PyLexerConfig());
        // Token扫描
        PyToken scanToken();             // 扫描单个token
        PyToken handleIdentifier();      // 处理标识符或关键字
        PyToken handleNumber();          // 处理数字
        PyToken handleString();          // 处理字符串
        PyToken handleOperator();        // 处理操作符
    // Token访问
    PyToken getNextToken();            // 获取下一个token
    PyToken peekToken() const;         // 查看当前token
    PyToken peekTokenAt(size_t offset) const; // 查看偏移位置的token
    
    // 状态管理
    PyLexerState saveState() const;    // 保存当前状态
    void restoreState(const PyLexerState& state); // 恢复状态
    size_t peekPosition() const { return position; } // 获取当前位置
    void resetPosition(size_t pos);   // 重置位置
    
    // 错误处理
    [[noreturn]] void error(const std::string& message) const;
    
    // 源码恢复
    void recoverSourceFromTokens(const std::string& filename = "recovered_source.py") const;
    
    // 调试辅助
    const std::vector<PyToken>& getAllTokens() const { return tokens; }
    std::string getTokenName(PyTokenType type) const { return PyTokenRegistry::getTokenName(type); }
    
    // 类型注解支持
    bool hasTypeAnnotation(size_t tokenPos) const;
    std::pair<size_t, std::string> extractTypeAnnotation(size_t startPos) const;
};

// 位置信息 - 用于错误报告和调试
struct PySourceLocation {
    int line;
    int column;
    
    PySourceLocation(int l = 0, int c = 0) : line(l), column(c) {}
    
    std::string toString() const {
        return "line " + std::to_string(line) + ", column " + std::to_string(column);
    }
};

} // namespace llvmpy

#endif // LEXER_H