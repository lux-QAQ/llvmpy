#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"
#include <memory>
#include <map>

namespace llvmpy {

class Parser {
private:
    Lexer& lexer;
    Token currentToken;
    
    // 错误处理
    std::unique_ptr<ExprAST> logError(const std::string& message);
    std::unique_ptr<StmtAST> logErrorStmt(const std::string& message);
    std::unique_ptr<FunctionAST> logErrorFunc(const std::string& message);
    std::unique_ptr<ModuleAST> logErrorModule(const std::string& message);
    
    // 获取下一个token
    void nextToken();
    
    // 表达式解析
    std::unique_ptr<ExprAST> parseNumber();
    std::unique_ptr<ExprAST> parseIdentifier();
    std::unique_ptr<ExprAST> parseParenExpr();
    std::unique_ptr<ExprAST> parsePrimary();
    std::unique_ptr<ExprAST> parseBinOpRHS(int exprPrec, std::unique_ptr<ExprAST> LHS);
    std::unique_ptr<ExprAST> parseExpression();
    std::unique_ptr<ExprAST> parseUnaryExpr(); // 解析一元表达式的方法
    
    // 语句解析
    std::unique_ptr<StmtAST> parseStatement();
    std::unique_ptr<StmtAST> parseExpressionStmt();
    std::unique_ptr<StmtAST> parseReturnStmt();
    std::unique_ptr<StmtAST> parseIfStmt();
    std::unique_ptr<StmtAST> parsePrintStmt();
    std::unique_ptr<StmtAST> parseAssignStmt();
    std::vector<std::unique_ptr<StmtAST>> parseBlock();
    std::unique_ptr<StmtAST> parseWhileStmt();
    std::unique_ptr<StmtAST> parseElifStmt();
    
    // 函数解析
    std::unique_ptr<FunctionAST> parseFunction();
    
    // 操作符优先级
    std::map<char, int> binopPrecedence;
    int getTokenPrecedence();
    
public:
    Parser(Lexer& lexer);
    
    // 解析整个模块
    std::unique_ptr<ModuleAST> parseModule();
};

} // namespace llvmpy

#endif // PARSER_H