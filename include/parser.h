#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"
#include <memory>
#include <map>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

namespace llvmpy
{

// 前向声明
class Parser;

// 解析器函数类型
using ExprParserFunc = std::function<std::unique_ptr<ExprAST>(Parser&)>;
using StmtParserFunc = std::function<std::unique_ptr<StmtAST>(Parser&)>;

// 操作符信息结构
struct OperatorInfo
{
    char symbol;
    int precedence;
    bool rightAssoc;

    OperatorInfo(char s = '\0', int p = 0, bool r = false)
        : symbol(s), precedence(p), rightAssoc(r)
    {
    }
};

// 操作符优先级类，管理和查询二元操作符的优先级
class OperatorPrecedence
{
private:
    std::map<char, int> precedenceMap;

public:
    OperatorPrecedence()
    {
    }

    // 添加操作符优先级
    void addOperator(char op, int precedence);

    // 获取Token的操作符优先级
    int getTokenPrecedence(TokenType tokenType) const;
};

// 解析错误
class ParseError : public std::runtime_error
{
private:
    int line;
    int column;

public:
    ParseError(const std::string& message, int line, int column)
        : std::runtime_error(message), line(line), column(column)
    {
    }

    std::string formatError() const;
    int getLine() const
    {
        return line;
    }
    int getColumn() const
    {
        return column;
    }
};

class Parser
{
private:
    Lexer& lexer;
    Token currentToken;
    OperatorPrecedence opPrecedence;

    // 解析器注册表
    static std::unordered_map<TokenType, ExprParserFunc> exprParsers;
    static std::unordered_map<TokenType, StmtParserFunc> stmtParsers;
    static std::unordered_map<TokenType, OperatorInfo> operatorInfos;

    // 保存解析状态，用于恢复
    struct ParserState
    {
        Token token;
        size_t lexerPosition;

        ParserState() : token(TOK_EOF, "", 0, 0), lexerPosition(0)
        {
        }
        ParserState(Token t, size_t pos) : token(t), lexerPosition(pos)
        {
        }
    };

    // 注册表初始化和管理
    static void initializeRegistries();
    static void registerExprParser(TokenType type, ExprParserFunc parser);
    static void registerStmtParser(TokenType type, StmtParserFunc parser);
    static void registerOperator(TokenType type, char symbol, int precedence, bool rightAssoc = false);
    static int getOperatorPrecedence(TokenType type);
    static char getOperatorSymbol(TokenType type);
    static bool isRightAssociative(TokenType type);
    static void ensureInitialized();

    // 错误处理
    std::unique_ptr<ExprAST> logError(const std::string& message) const;
    std::unique_ptr<StmtAST> logErrorStmt(const std::string& message) const;
    std::unique_ptr<FunctionAST> logErrorFunc(const std::string& message) const;
    std::unique_ptr<ModuleAST> logErrorModule(const std::string& message) const;

    // Token处理
    void nextToken();
    bool expectToken(TokenType type, const std::string& errorMessage);
    bool match(TokenType type);
    void skipNewlines();

    // 状态管理
    ParserState saveState() const;
    void restoreState(const ParserState& state);

    // 表达式解析
    std::unique_ptr<ExprAST> parseNumberExpr();
    std::unique_ptr<ExprAST> parseIdentifierExpr();
    std::unique_ptr<ExprAST> parseParenExpr();
    std::unique_ptr<ExprAST> parseStringExpr();
    std::unique_ptr<ExprAST> parseBoolExpr();
    std::unique_ptr<ExprAST> parseNoneExpr();
    std::unique_ptr<ExprAST> parsePrimary();
    std::unique_ptr<ExprAST> parseBinOpRHS(int exprPrec, std::unique_ptr<ExprAST> LHS);
    std::unique_ptr<ExprAST> parseExpression();
    std::unique_ptr<ExprAST> parseUnaryExpr();
    std::unique_ptr<ExprAST> parseListExpr();
    std::unique_ptr<ExprAST> parseIndexExpr(std::unique_ptr<ExprAST> target);

    // 语句解析
    std::unique_ptr<StmtAST> parseStatement();
    std::unique_ptr<StmtAST> parseExpressionStmt();
    std::unique_ptr<StmtAST> parseReturnStmt();
    std::unique_ptr<StmtAST> parseIfStmt();
    std::unique_ptr<StmtAST> parseElifPart();
    std::unique_ptr<StmtAST> parseElsePart(std::vector<std::unique_ptr<StmtAST>>& elseBody);
    std::unique_ptr<StmtAST> parsePrintStmt();
    std::unique_ptr<StmtAST> parseAssignStmt();
    std::unique_ptr<StmtAST> parseWhileStmt();
    std::unique_ptr<StmtAST> parseForStmt();
    std::unique_ptr<StmtAST> parseImportStmt();
    std::unique_ptr<StmtAST> parsePassStmt();
    std::unique_ptr<StmtAST> parseAssignStmt(const std::string& varName);

    // 块解析
    std::vector<std::unique_ptr<StmtAST>> parseBlock();

    // 函数解析
    std::unique_ptr<FunctionAST> parseFunction();
    std::vector<ParamAST> parseParameters();
    std::string parseReturnTypeAnnotation();

    // 类解析
    std::unique_ptr<StmtAST> parseClassDefinition();

public:
    Parser(Lexer& lexer);

    // 解析整个模块
    std::unique_ptr<ModuleAST> parseModule();

    // 获取词法分析器
    Lexer& getLexer();

    // 调试辅助功能
    void dumpCurrentToken() const;
};

}  // namespace llvmpy

#endif  // PARSER_H