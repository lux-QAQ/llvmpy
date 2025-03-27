#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>
#include <map>

namespace llvmpy
{

// Token类型枚举
enum TokenType
{
    // 特殊token
    TOK_EOF = -1,      // 文件结束
    TOK_NEWLINE = -2,  // 换行
    TOK_INDENT = -3,   // 缩进
    TOK_DEDENT = -4,   // 取消缩进

    // 关键字
    TOK_DEF = -10,   // 定义函数
    TOK_IF = -11,    // 条件语句
    TOK_ELSE = -12,  // 否则
    TOK_ELIF = -13,  // 否则如果
    TOK_RETURN = -14,
    TOK_WHILE = -15,
    TOK_FOR = -16,
    TOK_IN = -17,
    TOK_PRINT = -18,

    // 操作符和分隔符
    TOK_LPAREN = -20,  // (
    TOK_RPAREN = -21,  // )
    TOK_COLON = -22,   // :
    TOK_COMMA = -23,   // ,
    TOK_PLUS = -24,    // +
    TOK_MINUS = -25,   // -
    TOK_MUL = -26,     // *
    TOK_DIV = -27,     // /
    TOK_LT = -28,      // <
    TOK_GT = -29,      // >
    TOK_LE = -30,      // <=
    TOK_GE = -31,      // >=
    TOK_EQ = -32,      // ==
    TOK_NEQ = -33,     // !=
    TOK_ASSIGN = -34,  // =

    // 标识符和文字量
    TOK_IDENTIFIER = -40,
    TOK_NUMBER = -41,
    TOK_STRING = -42,
};

// Token结构
struct Token
{
    TokenType type;
    std::string value;
    int line;
    int column;

    Token(TokenType t, const std::string& v, int l, int c)
        : type(t), value(v), line(l), column(c)
    {
    }
};

// 词法分析器类
class Lexer
{
private:
    std::string sourceCode;
    size_t position;
    int currentLine;
    int currentColumn;
    int currentIndent;
    std::vector<int> indentStack;
    std::vector<Token> tokens;
    size_t tokenIndex;

    // 关键字映射
    std::map<std::string, TokenType> keywords;

    // 辅助方法
    char peek() const;
    char advance();
    bool match(char expected);
    void skipWhitespace();
    void skipComment();
    Token scanToken();
    Token handleIdentifier();
    Token handleNumber();
    Token handleString();
    void processIndentation();

public:
    Lexer(const std::string& source);

    // 获取下一个token
    Token getNextToken();

    // 前瞻token
    Token peekToken();

    // 前瞻特定位置的token
    Token peekTokenAt(size_t index) const;

    // 从token恢复源代码
    void recoverSourceFromTokens() const;

    // 辅助方法
    bool isAtEnd() const;
    std::string getTokenName(TokenType type) const;
};

}  // namespace llvmpy

#endif  // LEXER_H