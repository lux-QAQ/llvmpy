#include "lexer.h"
#include "parser.h"
#include <cctype>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>

namespace llvmpy
{

//===----------------------------------------------------------------------===//
// PyToken 方法实现
//===----------------------------------------------------------------------===//

std::string PyToken::toString() const
{
    std::string typeName = PyTokenRegistry::getTokenName(type);
    std::stringstream ss;
    ss << "PyToken(" << typeName << ", \"" << value << "\", line="
       << line << ", col=" << column << ")";
    return ss.str();
}

//===----------------------------------------------------------------------===//
// PyTokenRegistry 静态成员初始化
//===----------------------------------------------------------------------===//

PyRegistry<std::string, PyTokenType> PyTokenRegistry::keywordRegistry;
PyRegistry<PyTokenType, std::string> PyTokenRegistry::tokenNameRegistry;
PyRegistry<char, PyTokenType> PyTokenRegistry::simpleOperatorRegistry;
PyRegistry<std::string, PyTokenType> PyTokenRegistry::compoundOperatorRegistry;
PyRegistry<PyTokenType, PyTokenHandlerFunc> PyTokenRegistry::tokenHandlerRegistry;
bool PyTokenRegistry::isInitialized = false;

void PyTokenRegistry::initialize()
{
    if (isInitialized) return;

    // 注册关键字
    registerKeyword("def", TOK_DEF);
    registerKeyword("if", TOK_IF);
    registerKeyword("else", TOK_ELSE);
    registerKeyword("elif", TOK_ELIF);
    registerKeyword("return", TOK_RETURN);
    registerKeyword("while", TOK_WHILE);
    registerKeyword("for", TOK_FOR);
    registerKeyword("in", TOK_IN);
    registerKeyword("print", TOK_PRINT);  // 注意：Python 3 中 print 是函数
    registerKeyword("import", TOK_IMPORT);
    registerKeyword("class", TOK_CLASS);
    registerKeyword("pass", TOK_PASS);
    registerKeyword("break", TOK_BREAK);
    registerKeyword("continue", TOK_CONTINUE);
    registerKeyword("lambda", TOK_LAMBDA);
    registerKeyword("try", TOK_TRY);
    registerKeyword("except", TOK_EXCEPT);
    registerKeyword("finally", TOK_FINALLY);
    registerKeyword("with", TOK_WITH);
    registerKeyword("as", TOK_AS);
    registerKeyword("assert", TOK_ASSERT);
    registerKeyword("from", TOK_FROM);
    registerKeyword("global", TOK_GLOBAL);
    registerKeyword("nonlocal", TOK_NONLOCAL);
    registerKeyword("raise", TOK_RAISE);
    registerKeyword("True", TOK_BOOL);
    registerKeyword("False", TOK_BOOL);
    registerKeyword("None", TOK_NONE);

    registerKeyword("and", TOK_AND);
    registerKeyword("or", TOK_OR);
    registerKeyword("not", TOK_NOT);
    registerKeyword("is", TOK_IS);
    registerKeyword("yield", TOK_YIELD);
    registerKeyword("async", TOK_ASYNC);
    registerKeyword("await", TOK_AWAIT);
    registerKeyword("del", TOK_DEL);

    // 注册简单操作符
    registerSimpleOperator('(', TOK_LPAREN);
    registerSimpleOperator(')', TOK_RPAREN);
    registerSimpleOperator('[', TOK_LBRACK);
    registerSimpleOperator(']', TOK_RBRACK);
    registerSimpleOperator('{', TOK_LBRACE);
    registerSimpleOperator('}', TOK_RBRACE);
    registerSimpleOperator(':', TOK_COLON);
    registerSimpleOperator(',', TOK_COMMA);
    registerSimpleOperator('.', TOK_DOT);
    registerSimpleOperator('+', TOK_PLUS);
    registerSimpleOperator('-', TOK_MINUS);
    registerSimpleOperator('*', TOK_MUL);
    registerSimpleOperator('/', TOK_DIV);
    registerSimpleOperator('%', TOK_MOD);
    registerSimpleOperator('<', TOK_LT);
    registerSimpleOperator('>', TOK_GT);
    registerSimpleOperator('=', TOK_ASSIGN);
    registerSimpleOperator('@', TOK_AT);

    // 注册复合操作符
    registerCompoundOperator("<=", TOK_LE);
    registerCompoundOperator(">=", TOK_GE);
    registerCompoundOperator("==", TOK_EQ);
    registerCompoundOperator("!=", TOK_NEQ);
    registerCompoundOperator("+=", TOK_PLUS_ASSIGN);
    registerCompoundOperator("-=", TOK_MINUS_ASSIGN);
    registerCompoundOperator("*=", TOK_MUL_ASSIGN);
    registerCompoundOperator("/=", TOK_DIV_ASSIGN);
    registerCompoundOperator("%=", TOK_MOD_ASSIGN);
    registerCompoundOperator("**", TOK_POWER);
    registerCompoundOperator("//", TOK_FLOOR_DIV);
    registerCompoundOperator("->", TOK_ARROW);
    registerCompoundOperator("**=", TOK_POWER_ASSIGN);
    registerCompoundOperator("//=", TOK_FLOOR_DIV_ASSIGN);

    // 注册token名称
    tokenNameRegistry.registerItem(TOK_EOF, "EOF");
    tokenNameRegistry.registerItem(TOK_NEWLINE, "NEWLINE");
    tokenNameRegistry.registerItem(TOK_INDENT, "INDENT");
    tokenNameRegistry.registerItem(TOK_DEDENT, "DEDENT");
    tokenNameRegistry.registerItem(TOK_ERROR, "ERROR");

    // 关键字名称
    tokenNameRegistry.registerItem(TOK_DEF, "DEF");
    tokenNameRegistry.registerItem(TOK_IF, "IF");
    tokenNameRegistry.registerItem(TOK_ELSE, "ELSE");
    tokenNameRegistry.registerItem(TOK_ELIF, "ELIF");
    tokenNameRegistry.registerItem(TOK_RETURN, "RETURN");
    tokenNameRegistry.registerItem(TOK_WHILE, "WHILE");
    tokenNameRegistry.registerItem(TOK_FOR, "FOR");
    tokenNameRegistry.registerItem(TOK_IN, "IN");
    tokenNameRegistry.registerItem(TOK_PRINT, "PRINT");
    tokenNameRegistry.registerItem(TOK_IMPORT, "IMPORT");
    tokenNameRegistry.registerItem(TOK_CLASS, "CLASS");
    tokenNameRegistry.registerItem(TOK_PASS, "PASS");
    tokenNameRegistry.registerItem(TOK_BREAK, "BREAK");
    tokenNameRegistry.registerItem(TOK_CONTINUE, "CONTINUE");
    tokenNameRegistry.registerItem(TOK_LAMBDA, "LAMBDA");
    tokenNameRegistry.registerItem(TOK_TRY, "TRY");
    tokenNameRegistry.registerItem(TOK_EXCEPT, "EXCEPT");
    tokenNameRegistry.registerItem(TOK_FINALLY, "FINALLY");
    tokenNameRegistry.registerItem(TOK_WITH, "WITH");
    tokenNameRegistry.registerItem(TOK_AS, "AS");
    tokenNameRegistry.registerItem(TOK_ASSERT, "ASSERT");
    tokenNameRegistry.registerItem(TOK_FROM, "FROM");
    tokenNameRegistry.registerItem(TOK_GLOBAL, "GLOBAL");
    tokenNameRegistry.registerItem(TOK_NONLOCAL, "NONLOCAL");
    tokenNameRegistry.registerItem(TOK_RAISE, "RAISE");

    tokenNameRegistry.registerItem(TOK_YIELD, "YIELD");
    tokenNameRegistry.registerItem(TOK_IS, "IS");

    tokenNameRegistry.registerItem(TOK_NOT, "NOT");
    tokenNameRegistry.registerItem(TOK_AND, "AND");
    tokenNameRegistry.registerItem(TOK_OR, "OR");
    tokenNameRegistry.registerItem(TOK_DEL, "DEL");

    tokenNameRegistry.registerItem(TOK_EXEC, "EXEC");
    tokenNameRegistry.registerItem(TOK_ASYNC, "ASYNC");
    tokenNameRegistry.registerItem(TOK_AWAIT, "AWAIT");

    // 操作符名称
    tokenNameRegistry.registerItem(TOK_LPAREN, "LPAREN");
    tokenNameRegistry.registerItem(TOK_RPAREN, "RPAREN");
    tokenNameRegistry.registerItem(TOK_LBRACK, "LBRACK");
    tokenNameRegistry.registerItem(TOK_RBRACK, "RBRACK");
    tokenNameRegistry.registerItem(TOK_LBRACE, "LBRACE");
    tokenNameRegistry.registerItem(TOK_RBRACE, "RBRACE");
    tokenNameRegistry.registerItem(TOK_COLON, "COLON");
    tokenNameRegistry.registerItem(TOK_COMMA, "COMMA");
    tokenNameRegistry.registerItem(TOK_DOT, "DOT");
    tokenNameRegistry.registerItem(TOK_PLUS, "PLUS");
    tokenNameRegistry.registerItem(TOK_MINUS, "MINUS");
    tokenNameRegistry.registerItem(TOK_MUL, "MUL");
    tokenNameRegistry.registerItem(TOK_DIV, "DIV");
    tokenNameRegistry.registerItem(TOK_MOD, "MOD");
    tokenNameRegistry.registerItem(TOK_LT, "LT");
    tokenNameRegistry.registerItem(TOK_GT, "GT");
    tokenNameRegistry.registerItem(TOK_LE, "LE");
    tokenNameRegistry.registerItem(TOK_GE, "GE");
    tokenNameRegistry.registerItem(TOK_EQ, "EQ");
    tokenNameRegistry.registerItem(TOK_NEQ, "NEQ");
    tokenNameRegistry.registerItem(TOK_ASSIGN, "ASSIGN");
    tokenNameRegistry.registerItem(TOK_PLUS_ASSIGN, "PLUS_ASSIGN");
    tokenNameRegistry.registerItem(TOK_MINUS_ASSIGN, "MINUS_ASSIGN");
    tokenNameRegistry.registerItem(TOK_MUL_ASSIGN, "MUL_ASSIGN");
    tokenNameRegistry.registerItem(TOK_DIV_ASSIGN, "DIV_ASSIGN");
    tokenNameRegistry.registerItem(TOK_MOD_ASSIGN, "MOD_ASSIGN");
    tokenNameRegistry.registerItem(TOK_POWER, "POWER");
    tokenNameRegistry.registerItem(TOK_FLOOR_DIV, "FLOOR_DIV");
    tokenNameRegistry.registerItem(TOK_ARROW, "ARROW");
    tokenNameRegistry.registerItem(TOK_AT, "AT");

    // 字面量名称
    tokenNameRegistry.registerItem(TOK_IDENTIFIER, "IDENTIFIER");
    tokenNameRegistry.registerItem(TOK_NUMBER, "NUMBER");
    tokenNameRegistry.registerItem(TOK_INTEGER, "INTEGER");
    tokenNameRegistry.registerItem(TOK_FLOAT, "FLOAT");
    tokenNameRegistry.registerItem(TOK_STRING, "STRING");
    tokenNameRegistry.registerItem(TOK_BYTES, "BYTES");
    tokenNameRegistry.registerItem(TOK_BOOL, "BOOL");
    tokenNameRegistry.registerItem(TOK_NONE, "NONE");

    // 注册token处理函数
    registerTokenHandler(TOK_IDENTIFIER, [](PyLexer& lexer) -> PyToken
                         { return lexer.handleIdentifier(); });

    registerTokenHandler(TOK_INTEGER, [](PyLexer& lexer) -> PyToken
                         { return lexer.handleNumber(); });

    registerTokenHandler(TOK_FLOAT, [](PyLexer& lexer) -> PyToken
                         { return lexer.handleNumber(); });

    registerTokenHandler(TOK_STRING, [](PyLexer& lexer) -> PyToken
                         { return lexer.handleString(); });

    isInitialized = true;
}

void PyTokenRegistry::registerKeyword(const std::string& keyword, PyTokenType type)
{
    keywordRegistry.registerItem(keyword, type);
}

void PyTokenRegistry::registerSimpleOperator(char op, PyTokenType type)
{
    simpleOperatorRegistry.registerItem(op, type);
}

void PyTokenRegistry::registerCompoundOperator(const std::string& op, PyTokenType type)
{
    compoundOperatorRegistry.registerItem(op, type);
}

void PyTokenRegistry::registerTokenHandler(PyTokenType type, PyTokenHandlerFunc handler)
{
    tokenHandlerRegistry.registerItem(type, std::move(handler));
}

PyTokenType PyTokenRegistry::getKeywordType(const std::string& keyword)
{
    if (keywordRegistry.hasItem(keyword))
    {
        return keywordRegistry.getItem(keyword);
    }
    return TOK_IDENTIFIER;
}

PyTokenType PyTokenRegistry::getSimpleOperatorType(char op)
{
    if (simpleOperatorRegistry.hasItem(op))
    {
        return simpleOperatorRegistry.getItem(op);
    }
    return TOK_ERROR;
}

PyTokenType PyTokenRegistry::getCompoundOperatorType(const std::string& op)
{
    if (compoundOperatorRegistry.hasItem(op))
    {
        return compoundOperatorRegistry.getItem(op);
    }
    return TOK_ERROR;
}

std::string PyTokenRegistry::getTokenName(PyTokenType type)
{
    if (tokenNameRegistry.hasItem(type))
    {
        return tokenNameRegistry.getItem(type);
    }
    return "UNKNOWN";
}

PyTokenHandlerFunc PyTokenRegistry::getTokenHandler(PyTokenType type)
{
    if (tokenHandlerRegistry.hasItem(type))
    {
        return tokenHandlerRegistry.getItem(type);
    }
    return nullptr;
}

bool PyTokenRegistry::isKeyword(const std::string& word)
{
    return keywordRegistry.hasItem(word);
}

bool PyTokenRegistry::isSimpleOperator(char c)
{
    return simpleOperatorRegistry.hasItem(c);
}

bool PyTokenRegistry::isCompoundOperatorStart(char c)
{
    auto& operators = compoundOperatorRegistry.getAllItems();
    for (const auto& pair : operators)
    {
        if (!pair.first.empty() && pair.first[0] == c)
        {
            return true;
        }
    }
    return false;
}

const std::unordered_map<std::string, PyTokenType>& PyTokenRegistry::getKeywords()
{
    return keywordRegistry.getAllItems();
}

bool PyTokenRegistry::needsSpaceBetween(PyTokenType curr, PyTokenType next)
{
    // 一些操作符与标识符之间不需要空格
    if (curr == TOK_IDENTIFIER && (next == TOK_LPAREN || next == TOK_LBRACK || next == TOK_DOT))
    {
        return false;
    }
    // 括号之间不需要空格
    if (curr == TOK_RPAREN && (next == TOK_RPAREN || next == TOK_COMMA || next == TOK_COLON))
    {
        return false;
    }
    if (curr == TOK_LPAREN && next == TOK_RPAREN)
    {
        return false;
    }
    // 默认情况下，如果两个token都是操作符或分隔符，通常不需要空格
    bool currIsOp = curr <= TOK_AT && curr >= TOK_LPAREN;
    bool nextIsOp = next <= TOK_AT && next >= TOK_LPAREN;
    return !(currIsOp && nextIsOp);
}

//===----------------------------------------------------------------------===//
// PyLexerError 方法实现
//===----------------------------------------------------------------------===//

std::string PyLexerError::formatError() const
{
    std::stringstream ss;
    ss << "Lexer error at line " << line << ", column " << column << ": " << what();
    return ss.str();
}

//===----------------------------------------------------------------------===//
// PyLexer 方法实现
//===----------------------------------------------------------------------===//

PyLexer::PyLexer(const std::string& source, const PyLexerConfig& config)
    : sourceCode(source), position(0), currentLine(1), currentColumn(1), currentIndent(0), tokenIndex(0), config(config)
{
    // 确保PyTokenRegistry已初始化
    PyTokenRegistry::initialize();

    // 初始化缩进栈
    indentStack.push_back(0);

    // 对源代码进行分词
    tokenizeSource();
}

PyLexer PyLexer::fromFile(const std::string& filePath, const PyLexerConfig& config)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        throw std::runtime_error("Cannot open file: " + filePath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return PyLexer(buffer.str(), config);
}

char PyLexer::peek() const
{
    if (isAtEnd()) return '\0';
    return sourceCode[position];
}

char PyLexer::peekNext() const
{
    if (position + 1 >= sourceCode.length()) return '\0';
    return sourceCode[position + 1];
}

char PyLexer::advance()
{
    char c = peek();
    position++;

    if (c == '\n')
    {
        currentLine++;
        currentColumn = 1;
    }
    else
    {
        currentColumn++;
    }

    return c;
}

bool PyLexer::match(char expected)
{
    if (isAtEnd() || peek() != expected) return false;
    advance();
    return true;
}

bool PyLexer::isAtEnd() const
{
    return position >= sourceCode.length();
}

void PyLexer::skipWhitespace()
{
    while (!isAtEnd())
    {
        char c = peek();
        if (c == ' ' || c == '\r' || c == '\t')
        {
            advance();
        }
        else if (c == '#')
        {
            skipComment();
        }
        else
        {
            break;
        }
    }
}

void PyLexer::skipComment()
{
    // 注释一直持续到行尾
    while (peek() != '\n' && !isAtEnd())
    {
        advance();
    }
}

int PyLexer::calculateIndent()
{
    int indent = 0;
    size_t pos = position;

    while (pos < sourceCode.length())
    {
        char c = sourceCode[pos++];
        if (c == ' ')
        {
            indent++;
        }
        else if (c == '\t')
        {
            indent += config.tabWidth;
        }
        else
        {
            break;
        }
    }

    return indent;
}

// 修改processIndentation()函数，大约在第318行左右

void PyLexer::processIndentation()
{
    int indent = calculateIndent();

    // 检查这行是否只有注释或空白
    size_t pos = position + indent;
    while (pos < sourceCode.length() && (sourceCode[pos] == ' ' || sourceCode[pos] == '\t')) pos++;
    if (pos >= sourceCode.length() || sourceCode[pos] == '#' || sourceCode[pos] == '\n')
    {
        // 这是一个空行或者只有注释的行，跳过缩进处理
        position += indent;
        currentColumn += indent;
        return;
    }

    // 正常的缩进逻辑
    if (indent > indentStack.back())
    {
        // 增加缩进级别
        indentStack.push_back(indent);
        tokens.emplace_back(TOK_INDENT, "", currentLine, currentColumn);
    }
    else if (indent < indentStack.back())
    {
        // 减少缩进级别，可能需要多个 DEDENT token
        while (!indentStack.empty() && indent < indentStack.back())
        {
            indentStack.pop_back();
            tokens.emplace_back(TOK_DEDENT, "", currentLine, currentColumn);
        }

        // 验证缩进一致性
        if (config.strictIndentation && !indentStack.empty() && indent != indentStack.back())
        {
            error("Inconsistent indentation");
        }
    }

    // 跳过缩进空白
    position += indent;
    currentColumn += indent;
}

bool PyLexer::isDigit(char c) const
{
    return c >= '0' && c <= '9';
}

bool PyLexer::isAlpha(char c) const
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool PyLexer::isAlphaNumeric(char c) const
{
    return isAlpha(c) || isDigit(c);
}

PyToken PyLexer::errorToken(const std::string& message)
{
    return PyToken(TOK_ERROR, message, currentLine, currentColumn);
}

void PyLexer::error(const std::string& message) const
{
    throw PyLexerError(message, currentLine, currentColumn);
}

PyToken PyLexer::handleIdentifier()
{
    size_t start = position;
    while (isAlphaNumeric(peek()))
    {
        advance();
    }

    std::string identifier = sourceCode.substr(start, position - start);

    // 检查是否是关键字
    if (PyTokenRegistry::isKeyword(identifier))
    {
        PyTokenType keywordType = PyTokenRegistry::getKeywordType(identifier);
        return PyToken(keywordType, identifier, currentLine, currentColumn - identifier.length());
    }

    return PyToken(TOK_IDENTIFIER, identifier, currentLine, currentColumn - identifier.length());
}

PyToken PyLexer::handleNumber()
{
    size_t start = position;
    bool isFloat = false;

    // 整数部分
    while (isDigit(peek()))
    {
        advance();
    }

    // 小数部分
    if (peek() == '.' && isDigit(peekNext()))
    {
        isFloat = true;
        advance();  // 消费 '.'

        while (isDigit(peek()))
        {
            advance();
        }
    }

    // 指数部分
    if (peek() == 'e' || peek() == 'E')
    {
        isFloat = true;
        advance();  // 消费 'e' 或 'E'

        if (peek() == '+' || peek() == '-')
        {
            advance();  // 消费符号
        }

        if (!isDigit(peek()))
        {
            return errorToken("Invalid exponential notation");
        }

        while (isDigit(peek()))
        {
            advance();
        }
    }

    std::string number = sourceCode.substr(start, position - start);
    PyTokenType type = isFloat ? TOK_FLOAT : TOK_INTEGER;

    return PyToken(type, number, currentLine, currentColumn - number.length());
}

PyToken PyLexer::handleString()
{
    char quote = peek();  // 记录开始的引号 (' 或 ")
    size_t startLine = currentLine;
    size_t startColumn = currentColumn;
    advance();  // 消费开始引号

    std::stringstream processed_content;  // 使用 stringstream 来构建处理后的字符串内容
    bool errorOccurred = false;
    std::string errorMessage;

    while (!isAtEnd() && peek() != quote)
    {
        char current_char = peek();

        if (current_char == '\\')
        {
            advance();  // 消费反斜杠 '\'
            if (isAtEnd())
            {
                errorOccurred = true;
                errorMessage = "Unterminated string literal after escape sequence";
                break;
            }

            char escaped_char = peek();
            advance();  // 消费转义字符

            switch (escaped_char)
            {
                case 'n':
                    processed_content << '\n';
                    break;
                case 't':
                    processed_content << '\t';
                    break;
                case 'r':
                    processed_content << '\r';
                    break;
                case 'b':
                    processed_content << '\b';
                    break;
                case 'f':
                    processed_content << '\f';
                    break;
                case 'v':
                    processed_content << '\v';
                    break;
                case 'a':
                    processed_content << '\a';
                    break;
                case '\\':
                    processed_content << '\\';
                    break;
                case '\'':
                    processed_content << '\'';
                    break;
                case '"':
                    processed_content << '"';
                    break;
                case '\n':  // 行连接：忽略反斜杠和换行符
                    // 注意：advance() 已经消费了换行符，这里不需要额外操作
                    break;
                case 'x':
                {  // 十六进制转义 \xHH
                    if (isAtEnd())
                    {
                        errorOccurred = true;
                        errorMessage = "Incomplete hex escape sequence \\x";
                        break;
                    }
                    char hex1_char = peek();
                    if (!isxdigit(hex1_char))
                    {
                        errorOccurred = true;
                        errorMessage = "Invalid hex escape sequence \\x" + std::string(1, hex1_char);
                        break;
                    }
                    advance();

                    if (isAtEnd())
                    {
                        errorOccurred = true;
                        errorMessage = "Incomplete hex escape sequence \\x" + std::string(1, hex1_char);
                        break;
                    }
                    char hex2_char = peek();
                    if (!isxdigit(hex2_char))
                    {
                        errorOccurred = true;
                        errorMessage = "Invalid hex escape sequence \\x" + std::string(1, hex1_char) + std::string(1, hex2_char);
                        break;
                    }
                    advance();

                    std::string hex_str;
                    hex_str += hex1_char;
                    hex_str += hex2_char;
                    try
                    {
                        int val = std::stoi(hex_str, nullptr, 16);
                        processed_content << static_cast<char>(val);
                    }
                    catch (const std::invalid_argument& e)
                    {
                        // stoi 理论上不会失败，因为已经检查了 isxdigit
                        errorOccurred = true;
                        errorMessage = "Internal error parsing hex escape sequence";
                        break;
                    }
                    catch (const std::out_of_range& e)
                    {
                        // stoi 理论上不会失败
                        errorOccurred = true;
                        errorMessage = "Internal error parsing hex escape sequence";
                        break;
                    }
                    break;
                }
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                {  // 八进制转义 \ooo
                    std::string oct_str;
                    oct_str += escaped_char;  // 第一位八进制数字 (已经 advance 过)

                    // 最多再读两位八进制数字
                    int count = 1;
                    while (count < 3 && !isAtEnd())
                    {
                        char next_char = peek();
                        if (next_char >= '0' && next_char <= '7')
                        {
                            oct_str += next_char;
                            advance();
                            count++;
                        }
                        else
                        {
                            break;  // 不是八进制数字，停止读取
                        }
                    }

                    try
                    {
                        int val = std::stoi(oct_str, nullptr, 8);
                        if (val > 255)
                        {  // 八进制值不能超过 255 (一个字节)
                            errorOccurred = true;
                            errorMessage = "Octal escape sequence value out of range (must be <= \\377)";
                            break;
                        }
                        processed_content << static_cast<char>(val);
                    }
                    catch (const std::invalid_argument& e)
                    {
                        // stoi 理论上不会失败
                        errorOccurred = true;
                        errorMessage = "Internal error parsing octal escape sequence";
                        break;
                    }
                    catch (const std::out_of_range& e)
                    {
                        // stoi 理论上不会失败
                        errorOccurred = true;
                        errorMessage = "Internal error parsing octal escape sequence";
                        break;
                    }
                    break;
                }
                default:
                    // 未知的转义序列，按照 Python 的行为，保留反斜杠和该字符
                    processed_content << '\\' << escaped_char;
                    // 或者可以选择报错：
                    // errorOccurred = true;
                    // errorMessage = "Unknown escape sequence: \\" + std::string(1, escaped_char);
                    break;
            }
        }
        else
        {
            // 普通字符
            processed_content << current_char;
            advance();
        }

        if (errorOccurred) break;  // 如果在循环内部出错，提前退出
    }

    if (errorOccurred)
    {
        // 可以在这里记录更详细的错误位置信息
        return errorToken(errorMessage);
    }

    if (isAtEnd())
    {
        // 到达文件末尾但没有找到结束引号
        return errorToken("Unterminated string literal");
    }

    // 确认当前字符是匹配的结束引号
    if (peek() != quote)
    {
        // 这理论上不应该发生，因为循环条件会处理
        return errorToken("Internal lexer error: Mismatched quotes in string literal");
    }

    advance();  // 消费结束引号

    // 返回包含处理后内容的 Token
    // 注意：列号计算可能不完全精确，因为它基于原始字符数，但转义序列会改变长度
    // 为了简化，我们仍然使用原始的列号计算方式，或者可以传递 startColumn
    std::string final_content = processed_content.str();
    return PyToken(TOK_STRING, final_content, startLine, startColumn, quote);
}

PyToken PyLexer::handleOperator()
{
    char c = peek();

    // 检查是否可能是复合操作符的开始
    if (PyTokenRegistry::isCompoundOperatorStart(c))
    {
        // 尝试识别复合操作符
        for (int len = 3; len >= 2; len--)
        {  // 当前支持最多3字符的复合操作符
            if (position + len <= sourceCode.length())
            {
                std::string op = sourceCode.substr(position, len);
                PyTokenType type = PyTokenRegistry::getCompoundOperatorType(op);
                if (type != TOK_ERROR)
                {
                    // 找到匹配的复合操作符
                    for (int i = 0; i < len; i++)
                    {
                        advance();
                    }
                    return PyToken(type, op, currentLine, currentColumn - len);
                }
            }
        }
    }

    // 单字符操作符
    PyTokenType type = PyTokenRegistry::getSimpleOperatorType(c);
    if (type != TOK_ERROR)
    {
        advance();
        return PyToken(type, std::string(1, c), currentLine, currentColumn - 1);
    }

    // 未识别的操作符
    advance();
    return errorToken("Unexpected character");
}

PyToken PyLexer::scanToken()
{
    skipWhitespace();

    if (isAtEnd())
    {
        return PyToken(TOK_EOF, "", currentLine, currentColumn);
    }

    char c = peek();

    // 处理换行
    if (c == '\n')
    {
        advance();
        return PyToken(TOK_NEWLINE, "\n", currentLine - 1, currentColumn);
    }

    // 处理标识符
    if (isAlpha(c))
    {
        return handleIdentifier();
    }

    // 处理数字
    if (isDigit(c))
    {
        return handleNumber();
    }

    // 处理字符串
    if (c == '"' || c == '\'')
    {
        return handleString();
    }

    // 处理操作符和分隔符
    return handleOperator();
}

// 修改tokenizeSource()函数，大约在第638行左右

// 改进tokenizeSource()函数的整体缩进处理
void PyLexer::tokenizeSource()
{
    bool atLineStart = true;
    bool lastLineWasEmpty = false;
    // bool inFunctionBody = false; // 暂时不需要这个标志
    int emptyLineCount = 0;

    // 初始化缩进栈，确保至少有一个0级缩进
    if (indentStack.empty())
    {
        indentStack.push_back(0);
    }

    while (!isAtEnd())
    {
        // 处理行首的缩进
        if (atLineStart && peek() != '\n')
        {
            // 判断这是否是一个有内容的行（不只是注释）
            bool isCommentLine = false;
            size_t tempPos = position;
            int indent = calculateIndent();
            tempPos += indent;
            while (tempPos < sourceCode.length() && (sourceCode[tempPos] == ' ' || sourceCode[tempPos] == '\t'))
                tempPos++;
            if (tempPos < sourceCode.length() && sourceCode[tempPos] == '#')
                isCommentLine = true;

            // 只在非注释行处理缩进
            if (!isCommentLine)
            {
                processIndentation();  // processIndentation 内部会添加 INDENT/DEDENT tokens
                emptyLineCount = 0;    // 重置空行计数
            }
            else
            {
                // 对于注释行，只跳过空白，不产生缩进标记
                position += indent;  // processIndentation 内部会处理 position 和 currentColumn，这里不需要重复
                // currentColumn += indent; // processIndentation 内部会处理
            }

            atLineStart = false;
            lastLineWasEmpty = false;

            // 如果跳过缩进和注释后到达行尾或文件尾，则继续下一轮循环
            if (isAtEnd() || peek() == '\n')
            {
                // 如果是换行符，scanToken 会处理它
                // 如果是 EOF，循环将在下次迭代终止
                continue;
            }
        }
        else if (atLineStart && peek() == '\n')
        {
            // 处理空行或行首的换行符
            lastLineWasEmpty = true;
            emptyLineCount++;
            advance();  // 消费 '\n'
            // 只有在前一个token不是NEWLINE时才添加，避免连续多个NEWLINE token
            if (tokens.empty() || tokens.back().type != TOK_NEWLINE)
            {
                tokens.push_back(PyToken(TOK_NEWLINE, "\n", currentLine - 1, 1));  // 列通常重置为1
            }
            else
            {
                // 更新最后一个 NEWLINE 的行号 (如果需要更精确的行号)
                // tokens.back().line = currentLine - 1;
            }
            atLineStart = true;  // 保持 atLineStart 为 true，因为下一行还是行首
            continue;            // 继续下一轮循环处理可能的缩进或下一个 token
        }

        // 扫描token (如果不是行首空白或换行)
        PyToken token = scanToken();  // scanToken 会处理空白和注释直到下一个有效 token 或 EOF

        // --- 移除循环内部的 EOF 处理 ---
        // if (token.type == TOK_EOF) { ... break; }

        // 只有当 scanToken 返回非 EOF 时才添加
        if (token.type != TOK_EOF)
        {
            tokens.push_back(token);
        }
        else
        {
            // 如果 scanToken 返回 EOF，则退出循环，让循环外的逻辑处理
            break;
        }

        // 检查是否需要在行尾添加NEWLINE
        if (token.type == TOK_NEWLINE)
        {
            atLineStart = true;
        }
        else
        {
            // 如果 scanToken 返回的不是 NEWLINE，则下一轮循环不是行首
            // (除非 scanToken 内部消耗了换行符，但标准 scanToken 应该在换行符处停止或返回 NEWLINE)
            // 确保 scanToken 在遇到换行符时返回 TOK_NEWLINE 或停止
            atLineStart = false;  // 假设 scanToken 消耗了非换行符
        }
        // --- 如果 scanToken 返回 EOF，循环将在下一次迭代时因 !isAtEnd() 为 false 或 break 而终止 ---
    }

    // --- 将 DEDENT 生成逻辑移到循环外部 ---
    // 文件结束时确保所有缩进级别都正确关闭
    // 在添加最终 EOF token 之前执行
    while (indentStack.size() > 1)
    {  // 只要栈深度大于1（即存在非0缩进）
        indentStack.pop_back();
        // 使用最后一个非 EOF/NEWLINE token 的行号，或者当前位置的行号
        size_t refLine = currentLine;
        if (!tokens.empty())
        {
            // 查找最后一个有意义的 token 的行号
            for (auto it = tokens.rbegin(); it != tokens.rend(); ++it)
            {
                if (it->type != TOK_EOF && it->type != TOK_NEWLINE && it->type != TOK_INDENT && it->type != TOK_DEDENT)
                {
                    refLine = it->line;
                    break;
                }
            }
        }
        // DEDENT 通常概念上发生在下一行的开始，所以列号为1比较合理
        size_t refCol = 1;
        tokens.emplace_back(TOK_DEDENT, "", refLine, refCol);
#ifdef DEBUG_LEXER_INDENT
        std::cerr << "Debug [tokenizeSource]: Added EOF DEDENT. Stack size: " << indentStack.size() << std::endl;
#endif
    }

    // 确保最后一个token是EOF
    if (tokens.empty() || tokens.back().type != TOK_EOF)
    {
        // 如果最后一个 token 是 NEWLINE，EOF 应该在下一行
        size_t eofLine = currentLine;
        size_t eofCol = currentColumn;
        if (!tokens.empty() && tokens.back().type == TOK_NEWLINE)
        {
            eofLine = tokens.back().line + 1;  // EOF 在换行符之后的新行
            eofCol = 1;
        }
        tokens.emplace_back(TOK_EOF, "", eofLine, eofCol);
#ifdef DEBUG_LEXER
        std::cerr << "Debug [tokenizeSource]: Added final EOF token at L" << eofLine << " C" << eofCol << std::endl;
#endif
    }

    // 后处理：移除文件末尾错误的缩进标记 (这个逻辑现在应该不再需要)
    /*
    size_t i = tokens.size() - 2;  // 跳过EOF
    while (i > 0 && tokens[i].type == TOK_INDENT) {
        tokens.erase(tokens.begin() + i);
        i--;
    }
    */
#ifdef DEBUG_LEXER
    std::cerr << "Debug [tokenizeSource]: Tokenization complete. Total tokens: " << tokens.size() << std::endl;
#endif
}

PyToken PyLexer::getNextToken()
{
    if (tokenIndex < tokens.size())
    {
        return tokens[tokenIndex++];
    }
    return PyToken(TOK_EOF, "", currentLine, currentColumn);
}

PyToken PyLexer::peekToken() const
{
    if (tokenIndex < tokens.size())
    {
        return tokens[tokenIndex];
    }
    return PyToken(TOK_EOF, "", currentLine, currentColumn);
}

PyToken PyLexer::peekTokenAt(size_t offset) const
{
    if (tokenIndex + offset < tokens.size())
    {
        return tokens[tokenIndex + offset];
    }
    return PyToken(TOK_EOF, "", currentLine, currentColumn);
}

PyLexerState PyLexer::saveState() const
{
    /*     return PyLexerState(position, currentLine, currentColumn, tokenIndex); */
    return PyLexerState(tokenIndex);
}

void PyLexer::restoreState(const PyLexerState& state)
{
    /*     position = state.position;
    currentLine = state.line;
    currentColumn = state.column; */
    tokenIndex = state.tokenIndex;
}

void PyLexer::resetPosition(size_t pos)
{
    // 需要重新计算行和列
    size_t oldPos = position;
    position = 0;
    currentLine = 1;
    currentColumn = 1;

    // 向前扫描直到指定位置，更新行和列
    while (position < pos && position < sourceCode.length())
    {
        if (sourceCode[position++] == '\n')
        {
            currentLine++;
            currentColumn = 1;
        }
        else
        {
            currentColumn++;
        }
    }
}

bool PyLexer::hasTypeAnnotation(size_t tokenPos) const
{
    if (tokenPos >= tokens.size()) return false;

    // 检查当前位置是否是冒号，后面跟着类型
    return tokens[tokenPos].type == TOK_COLON && tokenPos + 1 < tokens.size() && tokens[tokenPos + 1].type == TOK_IDENTIFIER;
}

std::pair<size_t, std::string> PyLexer::extractTypeAnnotation(size_t startPos) const
{
    if (!hasTypeAnnotation(startPos))
    {
        return {startPos, ""};
    }

    // 跳过冒号
    size_t pos = startPos + 1;
    std::string typeName = tokens[pos].value;
    pos++;

    // 处理泛型参数，如list[int]
    if (pos < tokens.size() && tokens[pos].type == TOK_LBRACK)
    {
        std::string fullType = typeName + "[";
        pos++;

        // 处理泛型参数内容
        while (pos < tokens.size() && tokens[pos].type != TOK_RBRACK)
        {
            fullType += tokens[pos].value;
            pos++;

            // 如果遇到逗号，添加空格以提高可读性
            if (pos < tokens.size() && tokens[pos].type == TOK_COMMA)
            {
                fullType += ", ";
                pos++;
            }
        }

        // 添加闭括号
        if (pos < tokens.size() && tokens[pos].type == TOK_RBRACK)
        {
            fullType += "]";
            pos++;
        }

        return {pos, fullType};
    }

    return {pos, typeName};
}

void PyLexer::recoverSourceFromTokens(const std::string& filename) const
{
#ifdef RECOVER_SOURCE_FROM_TOKENS
    std::ofstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open file for writing: " << filename << std::endl;
        return;
    }

    int currentIndent = 0;
    bool atLineStart = true;
    PyToken previousToken = {TOK_ERROR, "", 0, 0};  // 用于跟踪前一个 token

    for (size_t i = 0; i < tokens.size(); i++)
    {
        const PyToken& token = tokens[i];

        // --- 处理行首缩进 ---
        if (atLineStart)
        {
            if (token.type != TOK_INDENT && token.type != TOK_DEDENT && token.type != TOK_EOF && token.type != TOK_NEWLINE)
            {
                file << std::string(currentIndent, ' ');
                atLineStart = false;
            }
        }

        // --- 更智能的空格处理 ---
        bool needsSpace = false;
        if (!atLineStart && i > 0)
        {                       // 只有在非行首且不是第一个 token 时才考虑加空格
            needsSpace = true;  // 默认需要空格

            // --- 不需要空格的条件 ---
            // 1. 点号前后
            if (previousToken.type == TOK_DOT || token.type == TOK_DOT)
            {
                needsSpace = false;
            }
            // 2. 左括号/方括号/花括号之后
            else if (previousToken.type == TOK_LPAREN || previousToken.type == TOK_LBRACK || previousToken.type == TOK_LBRACE)
            {
                needsSpace = false;
            }
            // 3. 右括号/方括号/花括号之前
            else if (token.type == TOK_RPAREN || token.type == TOK_RBRACK || token.type == TOK_RBRACE)
            {
                needsSpace = false;
            }
            // 4. 逗号/冒号之前
            else if (token.type == TOK_COMMA || token.type == TOK_COLON)
            {
                needsSpace = false;
            }
            // 5. 函数调用/索引: IDENTIFIER/Keyword 后跟 ( 或 [
            else if ((previousToken.type == TOK_IDENTIFIER || PyTokenRegistry::isKeyword(previousToken.value)) && (token.type == TOK_LPAREN || token.type == TOK_LBRACK))
            {
                needsSpace = false;
            }
            // 6. 冒号之后 (例如类型注解，字典字面量) - 通常需要空格，但切片不需要，这里简化，先允许空格
            // 7. 逗号之后 - 通常需要空格，让默认逻辑处理

            // --- 可能需要强制空格的情况 (覆盖默认) ---
            // (可以根据需要添加，例如二元操作符两侧，但目前的默认添加空格可能已足够)
        }

        if (needsSpace)
        {
            file << " ";
        }

        // --- 处理 Token 本身 ---
        switch (token.type)
        {
            case TOK_INDENT:
                currentIndent += 4;  // 使用4个空格作为缩进
                break;

            case TOK_DEDENT:
                currentIndent = std::max(0, currentIndent - 4);
                break;

            case TOK_NEWLINE:
                file << "\n";
                atLineStart = true;
                break;

            case TOK_EOF:
                // 忽略EOF
                break;

            case TOK_STRING:
            {
                // 恢复字符串字面量，需要重新转义特殊字符
                char quote = token.quoteChar;
                // 如果没有记录引号类型，默认使用双引号
                if (quote != '\'' && quote != '"')
                {
                    quote = '"';
                }
                file << quote;  // 写入起始引号

                for (char c : token.value)
                {
                    unsigned char uc = static_cast<unsigned char>(c);
                    switch (uc)
                    {
                        case '\n':
                            file << "\\n";
                            break;
                        case '\t':
                            file << "\\t";
                            break;
                        case '\r':
                            file << "\\r";
                            break;
                        case '\b':
                            file << "\\b";
                            break;
                        case '\f':
                            file << "\\f";
                            break;
                        case '\v':
                            file << "\\v";
                            break;
                        case '\a':
                            file << "\\a";
                            break;
                        case '\\':
                            file << "\\\\";
                            break;
                        case '\'':
                            file << (quote == '\'' ? "\\'" : "'");
                            break;
                        case '"':
                            file << (quote == '"' ? "\\\"" : "\"");
                            break;
                        default:
                            // 检查是否为其他需要转义的 ASCII 控制字符 (0x00-0x1F, 0x7F)
                            // 排除上面已经处理过的 \t, \n, \v, \f, \r, \b, \a
                            if ((uc < 32 && uc != '\t' && uc != '\n' && uc != '\v' && uc != '\f' && uc != '\r' && uc != '\b' && uc != '\a') || uc == 127)
                            {
                                // 输出为 \xHH 格式
                                file << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                                     << static_cast<int>(uc) << std::dec;
                            }
                            else
                            {
                                // 可打印 ASCII 或多字节 UTF-8 字符，直接写入
                                file << c;
                            }
                            break;
                    }
                }
                file << quote;  // 写入结束引号
                atLineStart = false;
                break;
            }

            default:
                // 写入token的值
                file << token.value;
                atLineStart = false;
                break;
        }
        // 更新 previousToken，跳过不影响布局的 token
        if (token.type != TOK_INDENT && token.type != TOK_DEDENT && token.type != TOK_NEWLINE && token.type != TOK_EOF)
        {
            previousToken = token;
        }
    }

    file.close();
    std::cout << "Source code recovered to: " << filename << std::endl;
#else
    std::cerr << "Source recovery disabled. Define RECOVER_SOURCE_FROM_TOKENS to enable." << std::endl;
#endif
}

}  // namespace llvmpy