#include "lexer.h"
#include <cctype>
#include <iostream>
#include <fstream>  // 添加这个头文件
namespace llvmpy
{

Lexer::Lexer(const std::string& source)
    : sourceCode(source), position(0), currentLine(1), currentColumn(1), currentIndent(0), tokenIndex(0)
{
    // 初始化关键字映射
    keywords["def"] = TOK_DEF;
    keywords["if"] = TOK_IF;
    keywords["else"] = TOK_ELSE;
    keywords["elif"] = TOK_ELIF;
    keywords["return"] = TOK_RETURN;
    keywords["while"] = TOK_WHILE;
    keywords["for"] = TOK_FOR;
    keywords["in"] = TOK_IN;
    keywords["print"] = TOK_PRINT;

    // 初始化缩进栈
    indentStack.push_back(0);

    // 扫描所有token
    while (!isAtEnd())
    {
        tokens.push_back(scanToken());
    }

    // 添加EOF token
    tokens.push_back(Token(TOK_EOF, "", currentLine, currentColumn));
}

/* char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return sourceCode[position];
}

char Lexer::advance() {
    char current = sourceCode[position++];
    if (current == '\n') {
        currentLine++;
        currentColumn = 1;
    } else {
        currentColumn++;
    }
    return current;
} */
// 修改peek()方法来处理多种换行符
char Lexer::peek() const
{
    if (isAtEnd()) return '\0';

    // 当前字符
    char current = sourceCode[position];

    // 如果是回车符，并且下一个字符是换行符，只返回换行符
    if (current == '\r' && position + 1 < sourceCode.length() && sourceCode[position + 1] == '\n')
    {
        return '\n';
    }

    // 将单独的回车符也视为换行符
    if (current == '\r')
    {
        return '\n';
    }

    return current;
}

// 修改advance()方法来正确处理换行符
char Lexer::advance()
{
    char current = sourceCode[position++];

    // 处理CRLF换行符
    if (current == '\r' && !isAtEnd() && sourceCode[position] == '\n')
    {
        position++;  // 跳过LF部分
        current = '\n';
    }
    else if (current == '\r')
    {
        // 将单独的CR视为换行符
        current = '\n';
    }

    if (current == '\n')
    {
        currentLine++;
        currentColumn = 1;
    }
    else
    {
        currentColumn++;
    }
    return current;
}

bool Lexer::match(char expected)
{
    if (isAtEnd() || sourceCode[position] != expected) return false;
    position++;
    currentColumn++;
    return true;
}

void Lexer::skipWhitespace()
{
    while (!isAtEnd())
    {
        char c = peek();
        if (c == ' ' || c == '\r' || c == '\t')
        {
            advance();
        }
        else
        {
            break;
        }
    }
}

void Lexer::skipComment()
{
    // 跳过注释直到行尾
    while (peek() != '\n' && !isAtEnd())
    {
        advance();
    }
}

void Lexer::processIndentation()
{
    int indent = 0;
    while (peek() == ' ' || peek() == '\t')
    {
        if (peek() == ' ')
            indent++;
        else if (peek() == '\t')
            indent += 8;  // Tab算8个空格
        advance();
    }

    // 忽略空行或者只有注释的行
    if (peek() == '\n' || peek() == '#')
    {
        return;
    }

    // 处理缩进变化
    if (indent > indentStack.back())
    {
        indentStack.push_back(indent);
        tokens.push_back(Token(TOK_INDENT, "", currentLine, currentColumn));
    }
    else if (indent < indentStack.back())
    {
        while (indent < indentStack.back())
        {
            indentStack.pop_back();
            tokens.push_back(Token(TOK_DEDENT, "", currentLine, currentColumn));
        }
        if (indent != indentStack.back())
        {
            std::cerr << "Error: inconsistent indentation at line " << currentLine << std::endl;
        }
    }
}





Token Lexer::handleIdentifier()
{
    std::string identifier;
    int startColumn = currentColumn;

    while (isalnum(peek()) || peek() == '_')
    {
        identifier += advance();
    }

    // 检查是否是关键字
    if (keywords.find(identifier) != keywords.end())
    {
        return Token(keywords[identifier], identifier, currentLine, startColumn);
    }

    return Token(TOK_IDENTIFIER, identifier, currentLine, startColumn);
}

Token Lexer::handleNumber()
{
    std::string numStr;
    int startColumn = currentColumn;
    bool isFloat = false;

    while (isdigit(peek()) || peek() == '.')
    {
        if (peek() == '.')
        {
            if (isFloat) break;  // 已经有一个小数点了
            isFloat = true;
        }
        numStr += advance();
    }

    return Token(TOK_NUMBER, numStr, currentLine, startColumn);
}

Token Lexer::handleString()
{
    std::string str;
    int startColumn = currentColumn;

    // 跳过开始的引号
    advance();

    while (peek() != '"' && !isAtEnd())
    {
        if (peek() == '\\')
        {
            advance();  // 跳过转义字符
            switch (peek())
            {
                case 'n':
                    str += '\n';
                    break;
                case 't':
                    str += '\t';
                    break;
                case 'r':
                    str += '\r';
                    break;
                case '\\':
                    str += '\\';
                    break;
                case '"':
                    str += '"';
                    break;
                default:
                    str += peek();
            }
        }
        else
        {
            str += peek();
        }
        advance();
    }

    if (isAtEnd())
    {
        std::cerr << "Error: unterminated string at line " << currentLine << std::endl;
        return Token(TOK_STRING, str, currentLine, startColumn);
    }

    // 跳过结束的引号
    advance();

    return Token(TOK_STRING, str, currentLine, startColumn);
}

Token Lexer::scanToken()
{
    skipWhitespace();

    if (isAtEnd())
    {
        return Token(TOK_EOF, "", currentLine, currentColumn);
    }

    char c = peek();
    int startColumn = currentColumn;

    // 处理新行和缩进
    if (c == '\n')
    {
        advance();
        int newlineLineNum = currentLine - 1;
        int newlineColNum = startColumn;

        // 处理连续的空行
        while (peek() == '\n')
        {
            advance();
        }

        // 先检查行是否为空或只包含注释
        int tempPos = position;
        int tempIndent = 0;

        // 计算缩进
        while (position < sourceCode.length() && (sourceCode[position] == ' ' || sourceCode[position] == '\t'))
        {
            if (sourceCode[position] == ' ')
                tempIndent++;
            else if (sourceCode[position] == '\t')
                tempIndent += 8;
            position++;
        }

        // 检查是否是空行或注释行
        bool emptyOrCommentLine = position >= sourceCode.length() || sourceCode[position] == '\n' || sourceCode[position] == '#';

        // 重置位置
        position = tempPos;

        // 先添加NEWLINE标记
        tokens.push_back(Token(TOK_NEWLINE, "\n", newlineLineNum, newlineColNum));

        // 如果不是空行或注释行，处理缩进
        if (!emptyOrCommentLine)
            processIndentation();

        // 返回下一个token
        return scanToken();
    }

    // 处理注释
    if (c == '#')
    {
        skipComment();
        return scanToken();
    }

    // 处理标识符和关键字
    if (isalpha(c) || c == '_')
    {
        return handleIdentifier();
    }

    // 处理数字
    if (isdigit(c))
    {
        return handleNumber();
    }

    // 处理字符串
    if (c == '"')
    {
        return handleString();
    }

    // 处理操作符和分隔符
    advance();  // 消费当前字符
    switch (c)
    {
        case '(':
            return Token(TOK_LPAREN, "(", currentLine, startColumn);
        case ')':
            return Token(TOK_RPAREN, ")", currentLine, startColumn);
        case ':':
            return Token(TOK_COLON, ":", currentLine, startColumn);
        case ',':
            return Token(TOK_COMMA, ",", currentLine, startColumn);
        case '+':
            return Token(TOK_PLUS, "+", currentLine, startColumn);
        case '-':
            return Token(TOK_MINUS, "-", currentLine, startColumn);
        case '*':
            return Token(TOK_MUL, "*", currentLine, startColumn);
        case '/':
            return Token(TOK_DIV, "/", currentLine, startColumn);
        case '<':
            if (match('=')) return Token(TOK_LE, "<=", currentLine, startColumn);
            return Token(TOK_LT, "<", currentLine, startColumn);
        case '>':
            if (match('=')) return Token(TOK_GE, ">=", currentLine, startColumn);
            return Token(TOK_GT, ">", currentLine, startColumn);
        case '=':
            if (match('=')) return Token(TOK_EQ, "==", currentLine, startColumn);
            return Token(TOK_ASSIGN, "=", currentLine, startColumn);
        case '!':
            if (match('=')) return Token(TOK_NEQ, "!=", currentLine, startColumn);
            break;
    }

    std::cerr << "Error: unknown character '" << c << "' at line " << currentLine << ", column " << startColumn << std::endl;
    return scanToken();  // 跳过未知字符，继续扫描
}

Token Lexer::getNextToken()
{
    if (tokenIndex < tokens.size())
    {
        return tokens[tokenIndex++];
    }
    return Token(TOK_EOF, "", currentLine, currentColumn);
}

Token Lexer::peekToken()
{
    if (tokenIndex < tokens.size())
    {
        return tokens[tokenIndex];
    }
    return Token(TOK_EOF, "", currentLine, currentColumn);
}
Token Lexer::peekTokenAt(size_t index) const
{
    if (index < tokens.size())
    {
        return tokens[index];
    }
    return Token(TOK_EOF, "", currentLine, currentColumn);
}

bool Lexer::isAtEnd() const
{
    return position >= sourceCode.length();
}

std::string Lexer::getTokenName(TokenType type) const
{
    switch (type)
    {
        case TOK_EOF:
            return "EOF";
        case TOK_NEWLINE:
            return "NEWLINE";
        case TOK_INDENT:
            return "INDENT";
        case TOK_DEDENT:
            return "DEDENT";
        case TOK_DEF:
            return "DEF";
        case TOK_IF:
            return "IF";
        case TOK_ELSE:
            return "ELSE";
        case TOK_ELIF:
            return "ELIF";
        case TOK_RETURN:
            return "RETURN";
        case TOK_WHILE:
            return "WHILE";
        case TOK_FOR:
            return "FOR";
        case TOK_IN:
            return "IN";
        case TOK_PRINT:
            return "PRINT";
        case TOK_LPAREN:
            return "(";
        case TOK_RPAREN:
            return ")";
        case TOK_COLON:
            return ":";
        case TOK_COMMA:
            return ",";
        case TOK_PLUS:
            return "+";
        case TOK_MINUS:
            return "-";
        case TOK_MUL:
            return "*";
        case TOK_DIV:
            return "/";
        case TOK_LT:
            return "<";
        case TOK_GT:
            return ">";
        case TOK_LE:
            return "<=";
        case TOK_GE:
            return ">=";
        case TOK_EQ:
            return "==";
        case TOK_NEQ:
            return "!=";
        case TOK_ASSIGN:
            return "=";
        case TOK_IDENTIFIER:
            return "IDENTIFIER";
        case TOK_NUMBER:
            return "NUMBER";
        case TOK_STRING:
            return "STRING";
        default:
            return "UNKNOWN";
    }
}



// 判断是否是关键字（可以添加在类实现内部）
bool isKeyword(TokenType type)
{
    return (type == TOK_DEF || type == TOK_IF || type == TOK_ELSE || type == TOK_ELIF ||
            type == TOK_RETURN || type == TOK_WHILE || type == TOK_FOR || type == TOK_IN ||
            type == TOK_PRINT);
}
// 判断两个token之间是否需要空格（可以添加在类实现内部）
bool needsSpaceBetween(TokenType curr, TokenType next)
{
    // 这些token后通常不需要空格
    if (curr == TOK_LPAREN || curr == TOK_COLON)
    {
        return false;
    }
    
    // 这些token前通常不需要空格
    if (next == TOK_RPAREN || next == TOK_COMMA || next == TOK_COLON ||
        next == TOK_NEWLINE || next == TOK_EOF)
    {
        return false;
    }
    
    // 操作符前后通常需要空格
    if ((curr == TOK_PLUS || curr == TOK_MINUS || curr == TOK_MUL || curr == TOK_DIV ||
         curr == TOK_LT || curr == TOK_GT || curr == TOK_LE || curr == TOK_GE ||
         curr == TOK_EQ || curr == TOK_NEQ || curr == TOK_ASSIGN) ||
        (next == TOK_PLUS || next == TOK_MINUS || next == TOK_MUL || next == TOK_DIV ||
         next == TOK_LT || next == TOK_GT || next == TOK_LE || next == TOK_GE ||
         next == TOK_EQ || next == TOK_NEQ || next == TOK_ASSIGN))
    {
        return true;
    }
    
    // 关键字前后通常需要空格
    if (isKeyword(curr) || isKeyword(next))
    {
        return true;
    }
    
    // 默认情况下返回true，确保标识符之间有空格
    return true;
}
void Lexer::recoverSourceFromTokens() const
{
    std::ofstream outFile("Token_recovery.py");
    if (!outFile.is_open())
    {
        std::cerr << "错误: 无法打开 Token_recovery.py 进行写入" << std::endl;
        return;
    }
    
    int currentIndent = 0;
    bool needIndent = true;
    
    size_t idx = 0;
    Token tok = peekTokenAt(idx++);
    
    while (tok.type != TOK_EOF)
    {
        switch (tok.type)
        {
            case TOK_INDENT:
                currentIndent += 4;  // 使用4个空格表示一个缩进级别
                break;
                
            case TOK_DEDENT:
                currentIndent -= 4;
                if (currentIndent < 0) currentIndent = 0;
                break;
                
            case TOK_NEWLINE:
                outFile << "\n";
                needIndent = true;
                break;
                
            case TOK_STRING:
                if (needIndent)
                {
                    outFile << std::string(currentIndent, ' ');
                    needIndent = false;
                }
                outFile << "\"" << tok.value << "\"";
                break;
                
            default:
                if (needIndent)
                {
                    outFile << std::string(currentIndent, ' ');
                    needIndent = false;
                }
                
                // 根据token类型输出值
                outFile << tok.value;
                
                // 在某些token后添加空格
                if (idx < tokens.size())
                {
                    Token nextTok = peekTokenAt(idx);
                    // 判断是否需要在当前token和下一个token之间添加空格
                    if (needsSpaceBetween(tok.type, nextTok.type))
                    {
                        outFile << " ";
                    }
                }
                break;
        }
        
        tok = peekTokenAt(idx++);
    }
    
    outFile.close();
    std::cout << "源代码已恢复至 Token_recovery.py" << std::endl;
}




}  // namespace llvmpy