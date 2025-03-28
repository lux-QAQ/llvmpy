#include "lexer.h"
#include <cctype>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace llvmpy {

//===----------------------------------------------------------------------===//
// Token 方法实现
//===----------------------------------------------------------------------===//

std::string Token::toString() const {
    std::string typeName = TokenRegistry::getTokenName(type);
    std::stringstream ss;
    ss << "Token(" << typeName << ", \"" << value << "\", line=" 
       << line << ", col=" << column << ")";
    return ss.str();
}

//===----------------------------------------------------------------------===//
// TokenRegistry 静态成员定义
//===----------------------------------------------------------------------===//

std::unordered_map<std::string, TokenType> TokenRegistry::keywordMap;
std::unordered_map<TokenType, std::string> TokenRegistry::tokenNames;
std::unordered_map<char, TokenType> TokenRegistry::simpleOperators;
std::unordered_map<std::string, TokenType> TokenRegistry::compoundOperators;

void TokenRegistry::initialize() {
    // 注册关键字
    registerKeyword("def", TOK_DEF);
    registerKeyword("if", TOK_IF);
    registerKeyword("else", TOK_ELSE);
    registerKeyword("elif", TOK_ELIF);
    registerKeyword("return", TOK_RETURN);
    registerKeyword("while", TOK_WHILE);
    registerKeyword("for", TOK_FOR);
    registerKeyword("in", TOK_IN);
    registerKeyword("print", TOK_PRINT);
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
    
    // 注册token名称
    tokenNames[TOK_EOF] = "EOF";
    tokenNames[TOK_NEWLINE] = "NEWLINE";
    tokenNames[TOK_INDENT] = "INDENT";
    tokenNames[TOK_DEDENT] = "DEDENT";
    tokenNames[TOK_ERROR] = "ERROR";
    
    // 关键字名称
    tokenNames[TOK_DEF] = "DEF";
    tokenNames[TOK_IF] = "IF";
    tokenNames[TOK_ELSE] = "ELSE";
    tokenNames[TOK_ELIF] = "ELIF";
    tokenNames[TOK_RETURN] = "RETURN";
    tokenNames[TOK_WHILE] = "WHILE";
    tokenNames[TOK_FOR] = "FOR";
    tokenNames[TOK_IN] = "IN";
    tokenNames[TOK_PRINT] = "PRINT";
    tokenNames[TOK_IMPORT] = "IMPORT";
    tokenNames[TOK_CLASS] = "CLASS";
    tokenNames[TOK_PASS] = "PASS";
    tokenNames[TOK_BREAK] = "BREAK";
    tokenNames[TOK_CONTINUE] = "CONTINUE";
    tokenNames[TOK_LAMBDA] = "LAMBDA";
    tokenNames[TOK_TRY] = "TRY";
    tokenNames[TOK_EXCEPT] = "EXCEPT";
    tokenNames[TOK_FINALLY] = "FINALLY";
    tokenNames[TOK_WITH] = "WITH";
    tokenNames[TOK_AS] = "AS";
    tokenNames[TOK_ASSERT] = "ASSERT";
    tokenNames[TOK_FROM] = "FROM";
    tokenNames[TOK_GLOBAL] = "GLOBAL";
    tokenNames[TOK_NONLOCAL] = "NONLOCAL";
    tokenNames[TOK_RAISE] = "RAISE";
    
    // 操作符名称
    tokenNames[TOK_LPAREN] = "LPAREN";
    tokenNames[TOK_RPAREN] = "RPAREN";
    tokenNames[TOK_LBRACK] = "LBRACK";
    tokenNames[TOK_RBRACK] = "RBRACK";
    tokenNames[TOK_LBRACE] = "LBRACE";
    tokenNames[TOK_RBRACE] = "RBRACE";
    tokenNames[TOK_COLON] = "COLON";
    tokenNames[TOK_COMMA] = "COMMA";
    tokenNames[TOK_DOT] = "DOT";
    tokenNames[TOK_PLUS] = "PLUS";
    tokenNames[TOK_MINUS] = "MINUS";
    tokenNames[TOK_MUL] = "MUL";
    tokenNames[TOK_DIV] = "DIV";
    tokenNames[TOK_MOD] = "MOD";
    tokenNames[TOK_LT] = "LT";
    tokenNames[TOK_GT] = "GT";
    tokenNames[TOK_LE] = "LE";
    tokenNames[TOK_GE] = "GE";
    tokenNames[TOK_EQ] = "EQ";
    tokenNames[TOK_NEQ] = "NEQ";
    tokenNames[TOK_ASSIGN] = "ASSIGN";
    tokenNames[TOK_PLUS_ASSIGN] = "PLUS_ASSIGN";
    tokenNames[TOK_MINUS_ASSIGN] = "MINUS_ASSIGN";
    tokenNames[TOK_MUL_ASSIGN] = "MUL_ASSIGN";
    tokenNames[TOK_DIV_ASSIGN] = "DIV_ASSIGN";
    tokenNames[TOK_MOD_ASSIGN] = "MOD_ASSIGN";
    tokenNames[TOK_POWER] = "POWER";
    tokenNames[TOK_FLOOR_DIV] = "FLOOR_DIV";
    tokenNames[TOK_ARROW] = "ARROW";
    tokenNames[TOK_AT] = "AT";
    
    // 字面量名称
    tokenNames[TOK_IDENTIFIER] = "IDENTIFIER";
    tokenNames[TOK_NUMBER] = "NUMBER";
    tokenNames[TOK_INTEGER] = "INTEGER";
    tokenNames[TOK_FLOAT] = "FLOAT";
    tokenNames[TOK_STRING] = "STRING";
    tokenNames[TOK_BYTES] = "BYTES";
    tokenNames[TOK_BOOL] = "BOOL";
    tokenNames[TOK_NONE] = "NONE";
}

void TokenRegistry::registerKeyword(const std::string& keyword, TokenType type) {
    keywordMap[keyword] = type;
}

void TokenRegistry::registerSimpleOperator(char op, TokenType type) {
    simpleOperators[op] = type;
}

void TokenRegistry::registerCompoundOperator(const std::string& op, TokenType type) {
    compoundOperators[op] = type;
}

TokenType TokenRegistry::getKeywordType(const std::string& keyword) {
    auto it = keywordMap.find(keyword);
    return (it != keywordMap.end()) ? it->second : TOK_IDENTIFIER;
}

TokenType TokenRegistry::getSimpleOperatorType(char op) {
    auto it = simpleOperators.find(op);
    return (it != simpleOperators.end()) ? it->second : TOK_ERROR;
}

TokenType TokenRegistry::getCompoundOperatorType(const std::string& op) {
    auto it = compoundOperators.find(op);
    return (it != compoundOperators.end()) ? it->second : TOK_ERROR;
}

std::string TokenRegistry::getTokenName(TokenType type) {
    auto it = tokenNames.find(type);
    if (it != tokenNames.end()) {
        return it->second;
    }
    return "UNKNOWN";
}

bool TokenRegistry::isKeyword(const std::string& word) {
    return keywordMap.find(word) != keywordMap.end();
}

bool TokenRegistry::isSimpleOperator(char c) {
    return simpleOperators.find(c) != simpleOperators.end();
}

bool TokenRegistry::isCompoundOperatorStart(char c) {
    for (const auto& pair : compoundOperators) {
        if (!pair.first.empty() && pair.first[0] == c) {
            return true;
        }
    }
    return false;
}

bool TokenRegistry::needsSpaceBetween(TokenType curr, TokenType next) {
    // 一些操作符与标识符之间不需要空格
    if (curr == TOK_IDENTIFIER && (next == TOK_LPAREN || next == TOK_LBRACK || next == TOK_DOT)) {
        return false;
    }
    // 括号之间不需要空格
    if (curr == TOK_RPAREN && (next == TOK_RPAREN || next == TOK_COMMA || next == TOK_COLON)) {
        return false;
    }
    if (curr == TOK_LPAREN && next == TOK_RPAREN) {
        return false;
    }
    // 默认情况下，如果两个token都是操作符或分隔符，通常不需要空格
    bool currIsOp = curr <= TOK_AT && curr >= TOK_LPAREN;
    bool nextIsOp = next <= TOK_AT && next >= TOK_LPAREN;
    return !(currIsOp && nextIsOp);
}

//===----------------------------------------------------------------------===//
// LexerError 方法实现
//===----------------------------------------------------------------------===//

std::string LexerError::formatError() const {
    std::stringstream ss;
    ss << "Lexer error at line " << line << ", column " << column << ": " << what();
    return ss.str();
}

//===----------------------------------------------------------------------===//
// Lexer 方法实现
//===----------------------------------------------------------------------===//

Lexer::Lexer(const std::string& source, const LexerConfig& config)
    : sourceCode(source), position(0), currentLine(1), currentColumn(1),
      currentIndent(0), tokenIndex(0), config(config) {
      
    // 确保TokenRegistry已初始化
    static bool initialized = false;
    if (!initialized) {
        TokenRegistry::initialize();
        initialized = true;
    }
    
    // 初始化缩进栈
    indentStack.push_back(0);
    
    // 对源代码进行分词
    tokenizeSource();
}

Lexer Lexer::fromFile(const std::string& filePath, const LexerConfig& config) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filePath);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return Lexer(buffer.str(), config);
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return sourceCode[position];
}

char Lexer::peekNext() const {
    if (position + 1 >= sourceCode.length()) return '\0';
    return sourceCode[position + 1];
}

char Lexer::advance() {
    char c = peek();
    position++;
    
    if (c == '\n') {
        currentLine++;
        currentColumn = 1;
    } else {
        currentColumn++;
    }
    
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd() || peek() != expected) return false;
    advance();
    return true;
}

bool Lexer::isAtEnd() const {
    return position >= sourceCode.length();
}

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\r' || c == '\t') {
            advance();
        } else if (c == '#') {
            skipComment();
        } else {
            break;
        }
    }
}

void Lexer::skipComment() {
    // 注释一直持续到行尾
    while (peek() != '\n' && !isAtEnd()) {
        advance();
    }
}

int Lexer::calculateIndent() {
    int indent = 0;
    size_t pos = position;
    
    while (pos < sourceCode.length()) {
        char c = sourceCode[pos++];
        if (c == ' ') {
            indent++;
        } else if (c == '\t') {
            indent += config.tabWidth;
        } else {
            break;
        }
    }
    
    return indent;
}

void Lexer::processIndentation() {
    int indent = calculateIndent();
    
    if (indent > indentStack.back()) {
        // 增加缩进级别
        indentStack.push_back(indent);
        tokens.emplace_back(TOK_INDENT, "", currentLine, currentColumn);
    } else if (indent < indentStack.back()) {
        // 减少缩进级别，可能需要多个 DEDENT token
        while (!indentStack.empty() && indent < indentStack.back()) {
            indentStack.pop_back();
            tokens.emplace_back(TOK_DEDENT, "", currentLine, currentColumn);
        }
        
        // 验证缩进一致性
        if (config.strictIndentation && !indentStack.empty() && indent != indentStack.back()) {
            error("Inconsistent indentation");
        }
    }
    
    // 跳过缩进空白
    position += indent;
    currentColumn += indent;
}

bool Lexer::isDigit(char c) const {
    return c >= '0' && c <= '9';
}

bool Lexer::isAlpha(char c) const {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Lexer::isAlphaNumeric(char c) const {
    return isAlpha(c) || isDigit(c);
}

Token Lexer::errorToken(const std::string& message) {
    return Token(TOK_ERROR, message, currentLine, currentColumn);
}

void Lexer::error(const std::string& message) const {
    throw LexerError(message, currentLine, currentColumn);
}

Token Lexer::handleIdentifier() {
    size_t start = position;
    while (isAlphaNumeric(peek())) {
        advance();
    }
    
    std::string identifier = sourceCode.substr(start, position - start);
    
    // 检查是否是关键字
    if (TokenRegistry::isKeyword(identifier)) {
        TokenType keywordType = TokenRegistry::getKeywordType(identifier);
        return Token(keywordType, identifier, currentLine, currentColumn - identifier.length());
    }
    
    return Token(TOK_IDENTIFIER, identifier, currentLine, currentColumn - identifier.length());
}

Token Lexer::handleNumber() {
    size_t start = position;
    bool isFloat = false;
    
    // 整数部分
    while (isDigit(peek())) {
        advance();
    }
    
    // 小数部分
    if (peek() == '.' && isDigit(peekNext())) {
        isFloat = true;
        advance(); // 消费 '.'
        
        while (isDigit(peek())) {
            advance();
        }
    }
    
    // 指数部分
    if (peek() == 'e' || peek() == 'E') {
        isFloat = true;
        advance(); // 消费 'e' 或 'E'
        
        if (peek() == '+' || peek() == '-') {
            advance(); // 消费符号
        }
        
        if (!isDigit(peek())) {
            return errorToken("Invalid exponential notation");
        }
        
        while (isDigit(peek())) {
            advance();
        }
    }
    
    std::string number = sourceCode.substr(start, position - start);
    TokenType type = isFloat ? TOK_FLOAT : TOK_INTEGER;
    
    return Token(type, number, currentLine, currentColumn - number.length());
}

Token Lexer::handleString() {
    char quote = peek();
    advance(); // 消费开始引号
    
    size_t start = position;
    while (!isAtEnd() && peek() != quote) {
        if (peek() == '\\') {
            advance(); // 跳过转义字符
            if (isAtEnd()) {
                return errorToken("Unterminated string");
            }
        }
        advance();
    }
    
    if (isAtEnd()) {
        return errorToken("Unterminated string");
    }
    
    std::string str = sourceCode.substr(start, position - start);
    advance(); // 消费结束引号
    
    return Token(TOK_STRING, str, currentLine, currentColumn - str.length() - 2);
}

Token Lexer::handleOperator() {
    char c = peek();
    
    // 检查是否可能是复合操作符的开始
    if (TokenRegistry::isCompoundOperatorStart(c)) {
        // 尝试识别复合操作符
        for (int len = 3; len >= 2; len--) { // 当前支持最多3字符的复合操作符
            if (position + len <= sourceCode.length()) {
                std::string op = sourceCode.substr(position, len);
                TokenType type = TokenRegistry::getCompoundOperatorType(op);
                if (type != TOK_ERROR) {
                    // 找到匹配的复合操作符
                    for (int i = 0; i < len; i++) {
                        advance();
                    }
                    return Token(type, op, currentLine, currentColumn - len);
                }
            }
        }
    }
    
    // 单字符操作符
    TokenType type = TokenRegistry::getSimpleOperatorType(c);
    if (type != TOK_ERROR) {
        advance();
        return Token(type, std::string(1, c), currentLine, currentColumn - 1);
    }
    
    // 未识别的操作符
    advance();
    return errorToken("Unexpected character");
}

Token Lexer::scanToken() {
    skipWhitespace();
    
    if (isAtEnd()) {
        return Token(TOK_EOF, "", currentLine, currentColumn);
    }
    
    char c = peek();
    
    // 处理换行
    if (c == '\n') {
        advance();
        return Token(TOK_NEWLINE, "\n", currentLine - 1, currentColumn);
    }
    
    // 处理标识符
    if (isAlpha(c)) {
        return handleIdentifier();
    }
    
    // 处理数字
    if (isDigit(c)) {
        return handleNumber();
    }
    
    // 处理字符串
    if (c == '"' || c == '\'') {
        return handleString();
    }
    
    // 处理操作符和分隔符
    return handleOperator();
}

void Lexer::tokenizeSource() {
    bool atLineStart = true;
    
    while (!isAtEnd()) {
        // 处理行首的缩进
        if (atLineStart && peek() != '\n') {
            processIndentation();
            atLineStart = false;
        }
        
        // 扫描token
        Token token = scanToken();
        tokens.push_back(token);
        
        // 检查是否需要在行尾添加NEWLINE
        if (token.type == TOK_NEWLINE) {
            atLineStart = true;
        } else if (token.type == TOK_EOF) {
            // 文件结束时确保所有缩进级别都正确关闭
            while (indentStack.size() > 1) {
                indentStack.pop_back();
                tokens.emplace_back(TOK_DEDENT, "", currentLine, currentColumn);
            }
            break;
        }
    }
    
    // 确保最后一个token是EOF
    if (tokens.empty() || tokens.back().type != TOK_EOF) {
        tokens.emplace_back(TOK_EOF, "", currentLine, currentColumn);
    }
}

Token Lexer::getNextToken() {
    if (tokenIndex < tokens.size()) {
        return tokens[tokenIndex++];
    }
    return Token(TOK_EOF, "", currentLine, currentColumn);
}

Token Lexer::peekToken() const {
    if (tokenIndex < tokens.size()) {
        return tokens[tokenIndex];
    }
    return Token(TOK_EOF, "", currentLine, currentColumn);
}

Token Lexer::peekTokenAt(size_t offset) const {
    if (tokenIndex + offset < tokens.size()) {
        return tokens[tokenIndex + offset];
    }
    return Token(TOK_EOF, "", currentLine, currentColumn);
}

void Lexer::recoverSourceFromTokens(const std::string& filename) const {
#ifdef RECOVER_SOURCE_FROM_TOKENS
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << filename << std::endl;
        return;
    }
    
    int currentIndent = 0;
    bool atLineStart = true;
    
    for (size_t i = 0; i < tokens.size(); i++) {
        const Token& token = tokens[i];
        
        switch (token.type) {
            case TOK_INDENT:
                currentIndent += 4; // 使用4个空格作为缩进
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
                
            default:
                if (atLineStart) {
                    // 添加缩进
                    file << std::string(currentIndent, ' ');
                    atLineStart = false;
                } else if (i > 0 && TokenRegistry::needsSpaceBetween(tokens[i-1].type, token.type)) {
                    file << " ";
                }
                
                // 写入token的值
                file << token.value;
                break;
        }
    }
    
    file.close();
    std::cout << "Source code recovered to: " << filename << std::endl;
#else
    std::cerr << "Source recovery disabled. Define RECOVER_SOURCE_FROM_TOKENS to enable." << std::endl;
#endif
}

} // namespace llvmpy