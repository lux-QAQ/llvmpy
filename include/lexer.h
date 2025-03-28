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
enum TokenType {
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
struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;

    Token(TokenType t, const std::string& v, int l, int c)
        : type(t), value(v), line(l), column(c) {}
    
    // 默认构造函数
    Token() : type(TOK_ERROR), value(""), line(0), column(0) {}
    
    // 用于调试的友好字符串表示
    std::string toString() const;
};

// Token注册和管理类
class TokenRegistry {
private:
    static std::unordered_map<std::string, TokenType> keywordMap;
    static std::unordered_map<TokenType, std::string> tokenNames;
    static std::unordered_map<char, TokenType> simpleOperators;
    static std::unordered_map<std::string, TokenType> compoundOperators;

public:
    // 初始化注册表
    static void initialize();
    
    // 注册一个关键字
    static void registerKeyword(const std::string& keyword, TokenType type);
    
    // 注册一个简单操作符
    static void registerSimpleOperator(char op, TokenType type);
    
    // 注册一个复合操作符
    static void registerCompoundOperator(const std::string& op, TokenType type);
    
    // 获取关键字对应的TokenType
    static TokenType getKeywordType(const std::string& keyword);
    
    // 获取简单操作符对应的TokenType
    static TokenType getSimpleOperatorType(char op);
    
    // 获取复合操作符对应的TokenType
    static TokenType getCompoundOperatorType(const std::string& op);
    
    // 获取TokenType对应的名称
    static std::string getTokenName(TokenType type);
    
    // 检查是否是关键字
    static bool isKeyword(const std::string& word);
    
    // 检查是否是简单操作符
    static bool isSimpleOperator(char c);
    
    // 检查是否是可能的复合操作符的开始
    static bool isCompoundOperatorStart(char c);
    
    // 获取所有关键字映射
    static const std::unordered_map<std::string, TokenType>& getKeywords() {
        return keywordMap;
    }
    
    // 判断两个token之间是否需要空格
    static bool needsSpaceBetween(TokenType curr, TokenType next);
};

// 词法分析器配置
struct LexerConfig {
    int tabWidth = 4;                // Tab宽度
    bool useTabsForIndent = false;   // 是否使用Tab作为缩进
    bool strictIndentation = true;   // 是否强制检查一致的缩进
    bool ignoreComments = true;      // 是否忽略注释
    
    LexerConfig() = default;
};

// 词法分析器错误
class LexerError : public std::runtime_error {
private:
    int line;
    int column;
    
public:
    LexerError(const std::string& message, int line, int column)
        : std::runtime_error(message), line(line), column(column) {}
    
    int getLine() const { return line; }
    int getColumn() const { return column; }
    
    std::string formatError() const;
};

// 词法分析器类
class Lexer {
private:
    std::string sourceCode;          // 源代码
    size_t position;                 // 当前位置
    int currentLine;                 // 当前行号
    int currentColumn;               // 当前列号
    int currentIndent;               // 当前缩进级别
    std::vector<int> indentStack;    // 缩进栈
    std::vector<Token> tokens;       // 已处理的tokens
    size_t tokenIndex;               // 当前读取的token索引
    LexerConfig config;              // 配置

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
    
    // Token扫描
    Token scanToken();               // 扫描单个token
    Token handleIdentifier();        // 处理标识符或关键字
    Token handleNumber();            // 处理数字
    Token handleString();            // 处理字符串
    Token handleOperator();          // 处理操作符
    
    // 辅助方法
    bool isDigit(char c) const;      // 是否是数字
    bool isAlpha(char c) const;      // 是否是字母
    bool isAlphaNumeric(char c) const; // 是否是字母或数字
    void tokenizeSource();           // 将整个源码标记化
    
    // 错误处理
    Token errorToken(const std::string& message);

public:
    // 构造函数
    explicit Lexer(const std::string& source, const LexerConfig& config = LexerConfig());
    
    // 从文件构造
    static Lexer fromFile(const std::string& filePath, const LexerConfig& config = LexerConfig());
    
    // Token访问
    Token getNextToken();            // 获取下一个token
    Token peekToken() const;         // 查看当前token
    Token peekTokenAt(size_t offset) const; // 查看偏移位置的token
    
    // 错误处理
    [[noreturn]] void error(const std::string& message) const;
    
    // 源码恢复
    void recoverSourceFromTokens(const std::string& filename = "recovered_source.py") const;
    
    // 调试辅助
    const std::vector<Token>& getAllTokens() const { return tokens; }
    std::string getTokenName(TokenType type) const { return TokenRegistry::getTokenName(type); }
};

} // namespace llvmpy

#endif // LEXER_H