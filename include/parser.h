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
class PyParser;

// ============================ 解析器注册器 ============================

// 解析器函数类型模板
template<typename ResultType>
using ParserFunc = std::function<std::unique_ptr<ResultType>(PyParser&)>;

// 表达式解析器函数类型
using PyExprParserFunc = ParserFunc<ExprAST>;

// 语句解析器函数类型
using PyStmtParserFunc = ParserFunc<StmtAST>;

// 通用解析器注册表模板
template<typename TokenT, typename ResultT>
class PyParserRegistry {
private:
    std::unordered_map<TokenT, ParserFunc<ResultT>> parsers;

public:
    // 注册解析器函数
    void registerParser(TokenT token, ParserFunc<ResultT> parser) {
        parsers[token] = std::move(parser);
    }

    // 检查是否有对应的解析器
    bool hasParser(TokenT token) const {
        return parsers.find(token) != parsers.end();
    }

    // 获取解析器函数
    ParserFunc<ResultT> getParser(TokenT token) const {
        auto it = parsers.find(token);
        if (it != parsers.end()) {
            return it->second;
        }
        return nullptr;
    }
};

// ============================ 操作符信息 ============================

// 操作符信息结构
struct PyOperatorInfo
{
    char symbol;
    int precedence;
    bool rightAssoc;

    PyOperatorInfo(char s = '\0', int p = 0, bool r = false)
        : symbol(s), precedence(p), rightAssoc(r) {}
};

// 操作符优先级类
class PyOperatorPrecedence
{
private:
    std::map<char, int> precedenceMap;

public:
    PyOperatorPrecedence() {}

    // 添加操作符优先级
    void addOperator(char op, int precedence) {
        precedenceMap[op] = precedence;
    }

    // 获取Token的操作符优先级
    int getTokenPrecedence(PyTokenType tokenType, const PyOperatorInfo& info) const {
        auto it = precedenceMap.find(info.symbol);
        if (it != precedenceMap.end()) {
            return it->second;
        }
        return -1; // 未知优先级
    }
};

// ============================ 解析错误 ============================

// 解析错误类
class PyParseError : public std::runtime_error
{
private:
    int line;
    int column;

public:
    PyParseError(const std::string& message, int line, int column)
        : std::runtime_error(message), line(line), column(column) {}

    std::string formatError() const ;
    
    int getLine() const { return line; }
    int getColumn() const { return column; }
};

// ============================ 主解析器类 ============================

class PyParser
{
public:
    PyParser(PyLexer& lexer);

    // 主解析方法
    std::unique_ptr<ModuleAST> parseModule();
    
    // 状态访问器
    PyLexer& getLexer() { return lexer; }
    const PyToken& getCurrentToken() const { return currentToken; }
    
    // 调试辅助
    void dumpCurrentToken() const;

private:
    PyLexer& lexer;
    PyToken currentToken;
    PyOperatorPrecedence opPrecedence;

    // 解析器注册表
    static PyParserRegistry<PyTokenType, ExprAST> exprRegistry;
    static PyParserRegistry<PyTokenType, StmtAST> stmtRegistry;
    static std::unordered_map<PyTokenType, PyOperatorInfo> operatorRegistry;

    // 保存解析状态，用于尝试性解析和回溯
    struct PyParserState
    {
        PyToken token;
        size_t lexerPosition;

        PyParserState() : token(TOK_EOF, "", 0, 0), lexerPosition(0) {}
        PyParserState(PyToken t, size_t pos) : token(t), lexerPosition(pos) {}
    };

    // 注册表初始化
    static void initializeRegistries();
    static void registerExprParser(PyTokenType type, PyExprParserFunc parser);
    static void registerStmtParser(PyTokenType type, PyStmtParserFunc parser);
    static void registerOperator(PyTokenType type, char symbol, int precedence, bool rightAssoc = false);
    static bool isInitialized;

    // 错误处理
    template<typename T>
    std::unique_ptr<T> logParseError(const std::string& message) const {
        throw PyParseError(message, currentToken.line, currentToken.column);
        return nullptr;
    }

    // Token处理
    void nextToken();
    bool expectToken(PyTokenType type, const std::string& errorMessage);
    bool match(PyTokenType type);
    void skipNewlines();

    // 状态管理
    PyParserState saveState() const;
    void restoreState(const PyParserState& state);

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

    // 模板化表达式解析助手
    template<typename T, typename... Args>
    std::unique_ptr<ExprAST> makeExpr(Args&&... args) {
        auto expr = std::make_unique<T>(std::forward<Args>(args)...);
        expr->setLocation(currentToken.line, currentToken.column);
        return expr;
    }

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

    // 模板化语句解析助手
    template<typename T, typename... Args>
    std::unique_ptr<StmtAST> makeStmt(Args&&... args) {
        auto stmt = std::make_unique<T>(std::forward<Args>(args)...);
        stmt->setLocation(currentToken.line, currentToken.column);
        return stmt;
    }

    // 块解析 - 处理缩进的代码块
    std::vector<std::unique_ptr<StmtAST>> parseBlock();

    // 函数定义解析
    std::unique_ptr<FunctionAST> parseFunction();
    std::vector<ParamAST> parseParameters();
    std::string parseReturnTypeAnnotation();
    
    // 类定义解析
    std::unique_ptr<StmtAST> parseClassDefinition();
    
    // 类型注解解析
    std::shared_ptr<PyType> parseTypeAnnotation();
    
    // 通用解析辅助函数
    template<typename T>
    std::vector<T> parseDelimitedList(PyTokenType start, PyTokenType end, PyTokenType separator,
                                     std::function<T()> parseElement);
                                     
    // 解析带类型注解的表达式
    std::shared_ptr<PyType> tryParseTypeAnnotation();
};

// ============================ 类型解析器 ============================

// 类型解析器 - 用于从字符串解析类型表达式
class PyTypeParser {
public:
    // 从类型注解字符串解析类型
    static std::shared_ptr<PyType> parseType(const std::string& typeStr);
    
    // 从标识符解析基本类型
    static std::shared_ptr<PyType> parsePrimitiveType(const std::string& name);
    
    // 解析列表类型: list[T]
    static std::shared_ptr<PyType> parseListType(const std::string& typeStr);
    
    // 解析字典类型: dict[K, V]
    static std::shared_ptr<PyType> parseDictType(const std::string& typeStr);
};

}  // namespace llvmpy

#endif  // PARSER_H