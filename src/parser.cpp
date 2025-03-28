#include "parser.h"
#include <iostream>
#include <sstream>
#include "Debugdefine.h"

namespace llvmpy
{

//===----------------------------------------------------------------------===//
// 静态成员初始化
//===----------------------------------------------------------------------===//

std::unordered_map<TokenType, ExprParserFunc> Parser::exprParsers;
std::unordered_map<TokenType, StmtParserFunc> Parser::stmtParsers;
std::unordered_map<TokenType, OperatorInfo> Parser::operatorInfos;

//===----------------------------------------------------------------------===//
// ParseError 类实现
//===----------------------------------------------------------------------===//

std::string ParseError::formatError() const
{
    std::stringstream ss;
    ss << "Parser error at line " << line << ", column " << column << ": " << what();
    return ss.str();
}

//===----------------------------------------------------------------------===//
// 注册表初始化和管理方法
//===----------------------------------------------------------------------===//

void Parser::initializeRegistries()
{
    // 注册表达式解析器


    
    // 标识符可能是标识符表达式、赋值语句或索引赋值语句
    registerStmtParser(TOK_IDENTIFIER, [](Parser& p) -> std::unique_ptr<StmtAST> {
        // 保存当前状态
        auto state = p.saveState();
        std::string idName = p.currentToken.value;
        int line = p.currentToken.line;
        int column = p.currentToken.column;
        
        // 消费标识符
        p.nextToken();
        
        // 索引赋值: a[index] = value
        if (p.currentToken.type == TOK_LBRACK) {
            auto target = std::make_unique<VariableExprAST>(idName);
            target->line = line;
            target->column = column;
            
            p.nextToken(); // 消费 '['
            auto index = p.parseExpression();
            if (!index) return nullptr;
            
            if (!p.expectToken(TOK_RBRACK, "Expected ']' after index")) {
                return nullptr;
            }
            
            // 赋值
            if (p.currentToken.type == TOK_ASSIGN) {
                p.nextToken(); // 消费 '='
                auto value = p.parseExpression();
                if (!value) return nullptr;
                
                // 处理可能的换行
                if (p.currentToken.type == TOK_NEWLINE) {
                    p.nextToken();
                }
                
                auto stmt = std::make_unique<IndexAssignStmtAST>(
                    std::move(target), std::move(index), std::move(value));
                stmt->line = line;
                stmt->column = column;
                return stmt;
            }
            
            // 不是赋值，回溯并当作表达式处理
            p.restoreState(state);
            return p.parseExpressionStmt();
        }
        // 普通赋值: a = value
        else if (p.currentToken.type == TOK_ASSIGN) {
            return p.parseAssignStmt(idName);
        }
        // 表达式语句
        else {
            p.restoreState(state);
            return p.parseExpressionStmt();
        }
    });

    // 增加其他可能是表达式语句开头的token类型
    registerStmtParser(TOK_LBRACK, [](Parser& p)
                       { return p.parseExpressionStmt(); });
    registerExprParser(TOK_LBRACK, [](Parser& p)
                       { return p.parseListExpr(); });
    registerExprParser(TOK_INTEGER, [](Parser& p)
                       { return p.parseNumberExpr(); });
    registerExprParser(TOK_FLOAT, [](Parser& p)
                       { return p.parseNumberExpr(); });
    registerExprParser(TOK_NUMBER, [](Parser& p)
                       { return p.parseNumberExpr(); });
    registerExprParser(TOK_IDENTIFIER, [](Parser& p)
                       { return p.parseIdentifierExpr(); });
    registerExprParser(TOK_LPAREN, [](Parser& p)
                       { return p.parseParenExpr(); });
    registerExprParser(TOK_STRING, [](Parser& p)
                       { return p.parseStringExpr(); });
    registerExprParser(TOK_MINUS, [](Parser& p)
                       { return p.parseUnaryExpr(); });
    registerExprParser(TOK_BOOL, [](Parser& p)
                       { return p.parseBoolExpr(); });
    registerExprParser(TOK_NONE, [](Parser& p)
                       { return p.parseNoneExpr(); });

    // 注册语句解析器
    registerStmtParser(TOK_RETURN, [](Parser& p)
                       { return p.parseReturnStmt(); });
    registerStmtParser(TOK_IF, [](Parser& p)
                       { return p.parseIfStmt(); });
    registerStmtParser(TOK_WHILE, [](Parser& p)
                       { return p.parseWhileStmt(); });
    registerStmtParser(TOK_FOR, [](Parser& p)
                       { return p.parseForStmt(); });
    registerStmtParser(TOK_PRINT, [](Parser& p)
                       { return p.parsePrintStmt(); });
    registerStmtParser(TOK_IMPORT, [](Parser& p)
                       { return p.parseImportStmt(); });
    registerStmtParser(TOK_PASS, [](Parser& p)
                       { return p.parsePassStmt(); });
    registerStmtParser(TOK_CLASS, [](Parser& p)
                       { return p.parseClassDefinition(); });

    // 默认情况下，标识符可能是赋值语句开头，也可能是表达式语句
    // 在Parser::initializeRegistries函数中
    // 默认情况下，标识符可能是赋值语句开头，也可能是表达式语句
    // 修改 registerStmtParser 中的标识符处理函数
    registerStmtParser(TOK_IDENTIFIER, [](Parser& p)
                       {
    // 保存当前状态
    auto state = p.saveState();
    std::string idName = p.currentToken.value;  // 保存标识符名称
    
    // 消费标识符
    p.nextToken();
    
    // 检查是否是赋值语句
    if (p.currentToken.type == TOK_ASSIGN) {
        return p.parseAssignStmt(idName); // 修改为传递变量名
    }
    
    // 恢复状态并解析为表达式语句
    p.restoreState(state);
    return p.parseExpressionStmt(); });

    // 可能是表达式语句开头的其他token类型
    registerStmtParser(TOK_INTEGER, [](Parser& p)
                       { return p.parseExpressionStmt(); });
    registerStmtParser(TOK_FLOAT, [](Parser& p)
                       { return p.parseExpressionStmt(); });
    registerStmtParser(TOK_STRING, [](Parser& p)
                       { return p.parseExpressionStmt(); });
    registerStmtParser(TOK_LPAREN, [](Parser& p)
                       { return p.parseExpressionStmt(); });
    registerStmtParser(TOK_MINUS, [](Parser& p)
                       { return p.parseExpressionStmt(); });

    // 注册操作符信息
    registerOperator(TOK_PLUS, '+', 20);
    registerOperator(TOK_MINUS, '-', 20);
    registerOperator(TOK_MUL, '*', 40);
    registerOperator(TOK_DIV, '/', 40);
    registerOperator(TOK_MOD, '%', 40);
    registerOperator(TOK_LT, '<', 10);
    registerOperator(TOK_GT, '>', 10);
    registerOperator(TOK_LE, 'l', 10);           // 用'l'表示<=
    registerOperator(TOK_GE, 'g', 10);           // 用'g'表示>=
    registerOperator(TOK_EQ, 'e', 10);           // 用'e'表示==
    registerOperator(TOK_NEQ, 'n', 10);          // 用'n'表示!=
    registerOperator(TOK_POWER, '^', 60, true);  // 幂运算符是右结合的
}

void Parser::registerExprParser(TokenType type, ExprParserFunc parser)
{
    exprParsers[type] = std::move(parser);
}

void Parser::registerStmtParser(TokenType type, StmtParserFunc parser)
{
    stmtParsers[type] = std::move(parser);
}

void Parser::registerOperator(TokenType type, char symbol, int precedence, bool rightAssoc)
{
    operatorInfos[type] = OperatorInfo(symbol, precedence, rightAssoc);
}

int Parser::getOperatorPrecedence(TokenType type)
{
    auto it = operatorInfos.find(type);
    return (it != operatorInfos.end()) ? it->second.precedence : -1;
}

char Parser::getOperatorSymbol(TokenType type)
{
    auto it = operatorInfos.find(type);
    return (it != operatorInfos.end()) ? it->second.symbol : '\0';
}

bool Parser::isRightAssociative(TokenType type)
{
    auto it = operatorInfos.find(type);
    return (it != operatorInfos.end()) ? it->second.rightAssoc : false;
}

void Parser::ensureInitialized()
{
    static bool initialized = false;
    if (!initialized)
    {
        initializeRegistries();
        initialized = true;
    }
}

//===----------------------------------------------------------------------===//
// Parser 类实现
//===----------------------------------------------------------------------===//

Parser::Parser(Lexer& l) : lexer(l), currentToken(TOK_EOF, "", 1, 1)
{
    // 确保注册表已初始化
    ensureInitialized();

    // 获取第一个token
    nextToken();
}

void Parser::nextToken()
{
    currentToken = lexer.getNextToken();

#ifdef DEBUG
    // 添加调试输出
    std::cerr << "Debug: Next token: '" << currentToken.value
              << "' type: " << lexer.getTokenName(currentToken.type)
              << " at line " << currentToken.line
              << ", col " << currentToken.column << std::endl;
#endif
}

bool Parser::expectToken(TokenType type, const std::string& errorMessage)
{
    if (currentToken.type != type)
    {
        logError(errorMessage);
        return false;
    }
    nextToken();  // 消费预期的token
    return true;
}

bool Parser::match(TokenType type)
{
    if (currentToken.type != type)
    {
        return false;
    }
    nextToken();  // 消费匹配的token
    return true;
}

void Parser::skipNewlines()
{
    while (currentToken.type == TOK_NEWLINE)
    {
        nextToken();
    }
}

Parser::ParserState Parser::saveState() const
{
    return ParserState(currentToken, lexer.peekToken().column);
}

void Parser::restoreState(const ParserState& state)
{
    // 注意：这里的实现是一个简化版，实际上需要更复杂的状态恢复机制
    // 在完整实现中，应该保存和恢复lexer的完整状态
    currentToken = state.token;
}

std::unique_ptr<ExprAST> Parser::logError(const std::string& message) const
{
    std::cerr << "Error: " << message << " at line " << currentToken.line
              << ", col " << currentToken.column << std::endl;
    return nullptr;
}

std::unique_ptr<StmtAST> Parser::logErrorStmt(const std::string& message) const
{
    logError(message);
    return nullptr;
}

std::unique_ptr<FunctionAST> Parser::logErrorFunc(const std::string& message) const
{
    logError(message);
    return nullptr;
}

std::unique_ptr<ModuleAST> Parser::logErrorModule(const std::string& message) const
{
    logError(message);
    return nullptr;
}

void Parser::dumpCurrentToken() const
{
    std::cerr << "Current Token: type=" << lexer.getTokenName(currentToken.type)
              << ", value='" << currentToken.value
              << "', line=" << currentToken.line
              << ", col=" << currentToken.column << std::endl;
}

//===----------------------------------------------------------------------===//
// 表达式解析方法
//===----------------------------------------------------------------------===//

std::unique_ptr<ExprAST> Parser::parseNumberExpr()
{
    auto result = std::make_unique<NumberExprAST>(std::stod(currentToken.value));
    result->line = currentToken.line;
    result->column = currentToken.column;
    nextToken();  // 消费数字
    return result;
}

std::unique_ptr<ExprAST> Parser::parseParenExpr()
{
    int line = currentToken.line;
    int column = currentToken.column;

    nextToken();  // 消费'('

    auto expr = parseExpression();
    if (!expr)
        return nullptr;

    if (!expectToken(TOK_RPAREN, "Expected ')'"))
        return nullptr;

    expr->line = line;
    expr->column = column;
    return expr;
}

// 修改parseIdentifierExpr方法，检查是否为索引操作
std::unique_ptr<ExprAST> Parser::parseIdentifierExpr()
{
    std::string idName = currentToken.value;
    int line = currentToken.line;
    int column = currentToken.column;

    nextToken();  // 消费标识符

    // 函数调用
    if (currentToken.type == TOK_LPAREN)
    {
        // 现有函数调用处理代码
    }

    // 索引操作
    if (currentToken.type == TOK_LBRACK)
    {
        auto varExpr = std::make_unique<VariableExprAST>(idName);
        varExpr->line = line;
        varExpr->column = column;
        return parseIndexExpr(std::move(varExpr));
    }

    // 简单变量引用
    auto varExpr = std::make_unique<VariableExprAST>(idName);
    varExpr->line = line;
    varExpr->column = column;
    return varExpr;
}

std::unique_ptr<ExprAST> Parser::parseStringExpr()
{
    // 字符串字面量暂时作为错误返回，未来可以实现
    return logError("String literals not yet implemented");
}

std::unique_ptr<ExprAST> Parser::parseBoolExpr()
{
    // 布尔字面量暂时作为错误返回，未来可以实现
    return logError("Boolean literals not yet implemented");
}

std::unique_ptr<ExprAST> Parser::parseNoneExpr()
{
    // None字面量暂时作为错误返回，未来可以实现
    return logError("None literal not yet implemented");
}

std::unique_ptr<ExprAST> Parser::parseUnaryExpr()
{
    // 记录负号位置
    int line = currentToken.line;
    int column = currentToken.column;

    nextToken();  // 消费'-'

    // 解析操作数
    auto operand = parsePrimary();
    if (!operand)
        return nullptr;

    // 创建一个表达式：0 - operand，这样我们可以复用现有的二元操作符机制
    auto zero = std::make_unique<NumberExprAST>(0.0);
    zero->line = line;
    zero->column = column;

    auto binExpr = std::make_unique<BinaryExprAST>('-', std::move(zero), std::move(operand));
    binExpr->line = line;
    binExpr->column = column;
    return binExpr;
}

std::unique_ptr<ExprAST> Parser::parsePrimary()
{
    // 通过查找注册表获取并调用适当的解析函数
    auto it = exprParsers.find(currentToken.type);
    if (it != exprParsers.end())
    {
        return it->second(*this);
    }

    // 没有匹配的解析器，报告错误
    return logError("Unexpected token when expecting an expression: " + lexer.getTokenName(currentToken.type));
}

std::unique_ptr<ExprAST> Parser::parseBinOpRHS(int exprPrec, std::unique_ptr<ExprAST> LHS)
{
    while (true)
    {
        // 获取当前token的操作符优先级
        int tokPrec = getOperatorPrecedence(currentToken.type);

        // 如果优先级过低，则返回当前的LHS
        if (tokPrec < exprPrec)
            return LHS;

        // 保存操作符信息
        TokenType opToken = currentToken.type;
        int line = currentToken.line;
        int column = currentToken.column;

        nextToken();  // 消费操作符

        // 解析RHS
        auto RHS = parsePrimary();
        if (!RHS)
            return nullptr;

        // 比较下一个操作符的优先级
        int nextPrec = getOperatorPrecedence(currentToken.type);

        // 如果当前操作符优先级低于下一个操作符优先级，先解析右边的表达式
        bool needHigherPrec = !isRightAssociative(opToken) && tokPrec < nextPrec;
        bool needEqualPrec = isRightAssociative(opToken) && tokPrec <= nextPrec;

        if (needHigherPrec || needEqualPrec)
        {
            RHS = parseBinOpRHS(tokPrec + (needHigherPrec ? 1 : 0), std::move(RHS));
            if (!RHS)
                return nullptr;
        }

        // 获取操作符符号
        char binOp = getOperatorSymbol(opToken);

        // 构建二元表达式
        auto binExpr = std::make_unique<BinaryExprAST>(binOp, std::move(LHS), std::move(RHS));
        binExpr->line = line;
        binExpr->column = column;
        LHS = std::move(binExpr);
    }
}

std::unique_ptr<ExprAST> Parser::parseExpression()
{
    auto LHS = parsePrimary();
    if (!LHS)
        return nullptr;

    return parseBinOpRHS(0, std::move(LHS));
}

//===----------------------------------------------------------------------===//
// 语句解析方法
//===----------------------------------------------------------------------===//

std::unique_ptr<StmtAST> Parser::parseStatement()
{
    // 使用注册表查找解析器
    auto it = stmtParsers.find(currentToken.type);
    if (it != stmtParsers.end())
    {
        return it->second(*this);
    }

    // 检查可能是表达式语句的token类型
    if (currentToken.type == TOK_INTEGER || currentToken.type == TOK_FLOAT || currentToken.type == TOK_STRING || currentToken.type == TOK_LPAREN || currentToken.type == TOK_LBRACK || currentToken.type == TOK_MINUS)
    {
        return parseExpressionStmt();
    }

    // 没有匹配的解析器，报告错误
    return logErrorStmt("Unknown statement type: " + lexer.getTokenName(currentToken.type));
}

std::unique_ptr<StmtAST> Parser::parseExpressionStmt()
{
    int line = currentToken.line;
    int column = currentToken.column;

    auto expr = parseExpression();
    if (!expr)
        return nullptr;

    // 可选的分号/换行
    if (currentToken.type == TOK_NEWLINE)
        nextToken();  // 消费换行

    auto stmt = std::make_unique<ExprStmtAST>(std::move(expr));
    stmt->line = line;
    stmt->column = column;
    return stmt;
}

std::unique_ptr<StmtAST> Parser::parseReturnStmt()
{
    int line = currentToken.line;
    int column = currentToken.column;

    nextToken();  // 消费'return'

    // 处理空return语句
    if (currentToken.type == TOK_NEWLINE)
    {
        // TODO: 创建一个空的返回值表达式
        return logErrorStmt("Empty return statement not yet implemented");
    }

    auto value = parseExpression();
    if (!value)
        return nullptr;

    // 确保语句后有NEWLINE，并消费它
    if (currentToken.type == TOK_NEWLINE)
        nextToken();  // 消费换行

    auto stmt = std::make_unique<ReturnStmtAST>(std::move(value));
    stmt->line = line;
    stmt->column = column;
    return stmt;
}

std::unique_ptr<StmtAST> Parser::parseIfStmt()
{
    int line = currentToken.line;
    int column = currentToken.column;

    nextToken();  // 消费'if'

    // 解析条件表达式
    auto condition = parseExpression();
    if (!condition)
        return nullptr;

    // 检查冒号
    if (!expectToken(TOK_COLON, "Expected ':' after if condition"))
        return nullptr;

    // 跳过到行尾的所有token
    while (currentToken.type != TOK_NEWLINE && currentToken.type != TOK_EOF)
        nextToken();

    // 检查换行
    if (!expectToken(TOK_NEWLINE, "Expected newline after ':'"))
        return nullptr;

    // 检查缩进
    if (!expectToken(TOK_INDENT, "Expected indented block after 'if'"))
        return nullptr;

    // 解析if块内的语句
    std::vector<std::unique_ptr<StmtAST>> thenBody;
    while (currentToken.type != TOK_DEDENT && currentToken.type != TOK_EOF)
    {
        // 跳过空行
        if (currentToken.type == TOK_NEWLINE)
        {
            nextToken();
            continue;
        }

        // 如果遇到dedent，说明块结束了
        if (currentToken.type == TOK_DEDENT || currentToken.type == TOK_EOF)
            break;

        auto stmt = parseStatement();
        if (!stmt)
            return nullptr;

        thenBody.push_back(std::move(stmt));
    }

    // 检查dedent
    if (!expectToken(TOK_DEDENT, "Expected dedent after if block"))
        return nullptr;

    // 处理elif部分
    std::vector<std::unique_ptr<StmtAST>> elseBody;
    while (currentToken.type == TOK_ELIF)
    {
        auto elifStmt = parseElifPart();
        if (!elifStmt)
            return nullptr;

        elseBody.push_back(std::move(elifStmt));
    }

    // 处理else部分
    if (currentToken.type == TOK_ELSE)
    {
        if (parseElsePart(elseBody) == nullptr)
            return nullptr;
    }

    auto ifStmt = std::make_unique<IfStmtAST>(std::move(condition),
                                              std::move(thenBody),
                                              std::move(elseBody));
    ifStmt->line = line;
    ifStmt->column = column;
    return ifStmt;
}

std::unique_ptr<StmtAST> Parser::parseElifPart()
{
    int line = currentToken.line;
    int column = currentToken.column;

    nextToken();  // 消费'elif'

    // 解析条件表达式
    auto condition = parseExpression();
    if (!condition)
        return nullptr;

    // 检查冒号
    if (!expectToken(TOK_COLON, "Expected ':' after elif condition"))
        return nullptr;

    // 跳过到行尾的所有token
    while (currentToken.type != TOK_NEWLINE && currentToken.type != TOK_EOF)
        nextToken();

    // 检查换行
    if (!expectToken(TOK_NEWLINE, "Expected newline after ':'"))
        return nullptr;

    // 检查缩进
    if (!expectToken(TOK_INDENT, "Expected indented block after 'elif'"))
        return nullptr;

    // 解析elif块内的语句
    std::vector<std::unique_ptr<StmtAST>> thenBody;
    while (currentToken.type != TOK_DEDENT && currentToken.type != TOK_EOF)
    {
        // 跳过空行
        if (currentToken.type == TOK_NEWLINE)
        {
            nextToken();
            continue;
        }

        // 如果遇到dedent，说明块结束了
        if (currentToken.type == TOK_DEDENT || currentToken.type == TOK_EOF)
            break;

        auto stmt = parseStatement();
        if (!stmt)
            return nullptr;

        thenBody.push_back(std::move(stmt));
    }

    // 检查dedent
    if (!expectToken(TOK_DEDENT, "Expected dedent after elif block"))
        return nullptr;

    // elif 语句本质上是一个没有else部分的if语句
    auto elifStmt = std::make_unique<IfStmtAST>(std::move(condition),
                                                std::move(thenBody),
                                                std::vector<std::unique_ptr<StmtAST>>());
    elifStmt->line = line;
    elifStmt->column = column;
    return elifStmt;
}

std::unique_ptr<StmtAST> Parser::parseElsePart(std::vector<std::unique_ptr<StmtAST>>& elseBody)
{
    nextToken();  // 消费'else'

    // 检查冒号
    if (!expectToken(TOK_COLON, "Expected ':' after 'else'"))
        return nullptr;

    // 跳过到行尾的所有token
    while (currentToken.type != TOK_NEWLINE && currentToken.type != TOK_EOF)
        nextToken();

    // 检查换行
    if (!expectToken(TOK_NEWLINE, "Expected newline after 'else:'"))
        return nullptr;

    // 检查缩进
    if (!expectToken(TOK_INDENT, "Expected indented block after 'else:'"))
        return nullptr;

    // 解析else块内的语句
    std::vector<std::unique_ptr<StmtAST>> elseStmts;
    while (currentToken.type != TOK_DEDENT && currentToken.type != TOK_EOF)
    {
        // 跳过空行
        if (currentToken.type == TOK_NEWLINE)
        {
            nextToken();
            continue;
        }

        // 如果遇到dedent，说明块结束了
        if (currentToken.type == TOK_DEDENT || currentToken.type == TOK_EOF)
            break;

        auto stmt = parseStatement();
        if (!stmt)
            return nullptr;

        elseStmts.push_back(std::move(stmt));
    }

    // 检查dedent
    if (!expectToken(TOK_DEDENT, "Expected dedent after else block"))
        return nullptr;

    // 将解析到的else语句添加到传入的elseBody中
    for (auto& stmt : elseStmts)
    {
        elseBody.push_back(std::move(stmt));
    }

    return std::make_unique<EmptyStmtAST>();  // 返回一个空语句，实际的语句已添加到elseBody
}

std::unique_ptr<StmtAST> Parser::parseWhileStmt()
{
    int line = currentToken.line;
    int column = currentToken.column;

    nextToken();  // 消费'while'

    // 解析条件表达式
    auto condition = parseExpression();
    if (!condition)
        return nullptr;

    // 检查冒号
    if (!expectToken(TOK_COLON, "Expected ':' after while condition"))
        return nullptr;

    // 跳过到行尾的所有token
    while (currentToken.type != TOK_NEWLINE && currentToken.type != TOK_EOF)
        nextToken();

    // 检查换行
    if (!expectToken(TOK_NEWLINE, "Expected newline after ':'"))
        return nullptr;

    // 检查缩进
    if (!expectToken(TOK_INDENT, "Expected indented block after 'while'"))
        return nullptr;

    // 解析while块内的语句
    std::vector<std::unique_ptr<StmtAST>> body;
    while (currentToken.type != TOK_DEDENT && currentToken.type != TOK_EOF)
    {
        // 跳过空行
        if (currentToken.type == TOK_NEWLINE)
        {
            nextToken();
            continue;
        }

        // 如果遇到dedent，说明块结束了
        if (currentToken.type == TOK_DEDENT || currentToken.type == TOK_EOF)
            break;

        auto stmt = parseStatement();
        if (!stmt)
            return nullptr;

        body.push_back(std::move(stmt));
    }

    // 检查dedent
    if (!expectToken(TOK_DEDENT, "Expected dedent after while block"))
        return nullptr;

    auto whileStmt = std::make_unique<WhileStmtAST>(std::move(condition), std::move(body));
    whileStmt->line = line;
    whileStmt->column = column;
    return whileStmt;
}

std::unique_ptr<StmtAST> Parser::parsePrintStmt()
{
    int line = currentToken.line;
    int column = currentToken.column;

    nextToken();  // 消费'print'

    // 检查左括号
    if (!expectToken(TOK_LPAREN, "Expected '(' after 'print'"))
        return nullptr;

    // 解析打印的表达式
    auto expr = parseExpression();
    if (!expr)
        return nullptr;

    // 检查右括号
    if (!expectToken(TOK_RPAREN, "Expected ')' after print expression"))
        return nullptr;

    // 可选的分号/换行
    if (currentToken.type == TOK_NEWLINE)
        nextToken();  // 消费换行

    auto printStmt = std::make_unique<PrintStmtAST>(std::move(expr));
    printStmt->line = line;
    printStmt->column = column;
    return printStmt;
}

// 修改parseAssignStmt函数，添加参数以接收变量名
std::unique_ptr<StmtAST> Parser::parseAssignStmt(const std::string& varName)
{
    int line = currentToken.line;
    int column = currentToken.column - varName.length() - 1;  // 调整位置到变量名开始处

    // 当前token应该是ASSIGN (=)
    if (currentToken.type != TOK_ASSIGN)
    {
        return logErrorStmt("Expected '=' in assignment");
    }
    nextToken();  // 消费等号

    // 解析赋值的表达式
    auto value = parseExpression();
    if (!value)
        return nullptr;

    // 可选的分号/换行
    if (currentToken.type == TOK_NEWLINE)
        nextToken();  // 消费换行

    auto assignStmt = std::make_unique<AssignStmtAST>(varName, std::move(value));
    assignStmt->line = line;
    assignStmt->column = column;
    return assignStmt;
}

std::unique_ptr<StmtAST> Parser::parseForStmt()
{
    // For语句暂时作为错误返回，未来可以实现
    return logErrorStmt("For statement not yet implemented");
}

std::unique_ptr<StmtAST> Parser::parseImportStmt()
{
    int line = currentToken.line;
    int column = currentToken.column;

    nextToken();  // 消费'import'

    // 解析导入的模块名
    if (currentToken.type != TOK_IDENTIFIER)
    {
        return logErrorStmt("Expected module name after 'import'");
    }

    std::string moduleName = currentToken.value;
    nextToken();  // 消费模块名

    // 处理 'as' 别名
    std::string alias = "";
    if (currentToken.type == TOK_AS)
    {
        nextToken();  // 消费'as'

        if (currentToken.type != TOK_IDENTIFIER)
        {
            return logErrorStmt("Expected identifier after 'as'");
        }

        alias = currentToken.value;
        nextToken();  // 消费别名
    }

    // 确保语句后有NEWLINE，并消费它
    if (currentToken.type == TOK_NEWLINE)
        nextToken();  // 消费换行

    auto importStmt = std::make_unique<ImportStmtAST>(moduleName, alias);
    importStmt->line = line;
    importStmt->column = column;
    return importStmt;
}

std::unique_ptr<StmtAST> Parser::parsePassStmt()
{
    int line = currentToken.line;
    int column = currentToken.column;

    nextToken();  // 消费'pass'

    // 确保语句后有NEWLINE，并消费它
    if (currentToken.type == TOK_NEWLINE)
        nextToken();  // 消费换行

    auto passStmt = std::make_unique<PassStmtAST>();
    passStmt->line = line;
    passStmt->column = column;
    return passStmt;
}

std::unique_ptr<StmtAST> Parser::parseClassDefinition()
{
    int line = currentToken.line;
    int column = currentToken.column;

    nextToken();  // 消费'class'

    // 解析类名
    if (currentToken.type != TOK_IDENTIFIER)
    {
        return logErrorStmt("Expected class name after 'class' keyword");
    }

    std::string className = currentToken.value;
    nextToken();  // 消费类名

    // 解析可选的继承
    std::vector<std::string> baseClasses;
    if (currentToken.type == TOK_LPAREN)
    {
        nextToken();  // 消费'('

        // 解析基类列表
        if (currentToken.type != TOK_RPAREN)
        {
            while (true)
            {
                if (currentToken.type != TOK_IDENTIFIER)
                {
                    return logErrorStmt("Expected base class name in inheritance list");
                }

                baseClasses.push_back(currentToken.value);
                nextToken();  // 消费基类名

                if (currentToken.type == TOK_RPAREN)
                    break;

                if (!expectToken(TOK_COMMA, "Expected ')' or ',' in base class list"))
                    return nullptr;
            }
        }

        nextToken();  // 消费')'
    }

    // 检查冒号
    if (!expectToken(TOK_COLON, "Expected ':' after class definition"))
        return nullptr;

    // 跳过到行尾的所有token
    while (currentToken.type != TOK_NEWLINE && currentToken.type != TOK_EOF)
        nextToken();

    // 检查换行
    if (!expectToken(TOK_NEWLINE, "Expected newline after ':'"))
        return nullptr;

    // 检查缩进
    if (!expectToken(TOK_INDENT, "Expected indented block after class definition"))
        return nullptr;

    // 解析类体
    std::vector<std::unique_ptr<StmtAST>> body;
    std::vector<std::unique_ptr<FunctionAST>> methods;

    while (currentToken.type != TOK_DEDENT && currentToken.type != TOK_EOF)
    {
        // 跳过空行
        if (currentToken.type == TOK_NEWLINE)
        {
            nextToken();
            continue;
        }

        // 如果遇到dedent，说明块结束了
        if (currentToken.type == TOK_DEDENT || currentToken.type == TOK_EOF)
            break;

        // 检查是否是方法定义
        if (currentToken.type == TOK_DEF)
        {
            auto method = parseFunction();
            if (!method)
                return nullptr;

            methods.push_back(std::move(method));
        }
        else
        {
            auto stmt = parseStatement();
            if (!stmt)
                return nullptr;

            body.push_back(std::move(stmt));
        }
    }

    // 检查dedent
    if (!expectToken(TOK_DEDENT, "Expected dedent after class body"))
        return nullptr;

    auto classStmt = std::make_unique<ClassStmtAST>(className, baseClasses,
                                                    std::move(body), std::move(methods));
    classStmt->line = line;
    classStmt->column = column;
    return classStmt;
}

std::vector<ParamAST> Parser::parseParameters()
{
    std::vector<ParamAST> params;

    // 检查左括号
    if (!expectToken(TOK_LPAREN, "Expected '(' in parameter list"))
        return params;

    // 空参数列表
    if (currentToken.type == TOK_RPAREN)
    {
        nextToken();  // 消费')'
        return params;
    }

    // 解析参数列表
    while (true)
    {
        if (currentToken.type != TOK_IDENTIFIER)
        {
            logError("Expected parameter name");
            return params;
        }

        std::string paramName = currentToken.value;
        nextToken();  // 消费参数名

        // 解析可选的类型注解
        std::string paramType = "";
        if (currentToken.type == TOK_COLON)
        {
            nextToken();  // 消费':'

            if (currentToken.type != TOK_IDENTIFIER)
            {
                logError("Expected type after ':'");
                return params;
            }

            paramType = currentToken.value;
            nextToken();  // 消费类型
        }

        params.emplace_back(paramName, paramType);

        if (currentToken.type == TOK_RPAREN)
            break;

        if (!expectToken(TOK_COMMA, "Expected ')' or ',' in parameter list"))
            return params;
    }

    nextToken();  // 消费')'
    return params;
}

std::string Parser::parseReturnTypeAnnotation()
{
    // 默认无返回类型
    std::string returnType = "";

    // 检查是否有返回类型注解 '->'
    if (currentToken.type == TOK_ARROW)
    {
        nextToken();  // 消费'->'

        if (currentToken.type != TOK_IDENTIFIER)
        {
            logError("Expected return type after '->'");
            return returnType;
        }

        returnType = currentToken.value;
        nextToken();  // 消费类型
    }

    return returnType;
}

std::unique_ptr<FunctionAST> Parser::parseFunction()
{
    int line = currentToken.line;
    int column = currentToken.column;

    nextToken();  // 消费'def'

    // 解析函数名
    if (currentToken.type != TOK_IDENTIFIER)
    {
        return logErrorFunc("Expected function name after 'def'");
    }

    std::string funcName = currentToken.value;
    nextToken();  // 消费函数名

    // 解析参数列表
    auto params = parseParameters();

    // 解析可选的返回类型注解
    std::string returnType = parseReturnTypeAnnotation();

    // 检查冒号
    if (!expectToken(TOK_COLON, "Expected ':' after function signature"))
        return nullptr;

    // 跳过到行尾的所有token
    while (currentToken.type != TOK_NEWLINE && currentToken.type != TOK_EOF)
        nextToken();

    // 检查换行
    if (!expectToken(TOK_NEWLINE, "Expected newline after ':'"))
        return nullptr;

    // 解析函数体
    auto body = parseBlock();
    if (body.empty() && currentToken.type != TOK_DEDENT)
    {
        return logErrorFunc("Expected function body");
    }

    auto func = std::make_unique<FunctionAST>(funcName, params, returnType, std::move(body));
    func->line = line;
    func->column = column;
    return func;
}

std::vector<std::unique_ptr<StmtAST>> Parser::parseBlock()
{
    std::vector<std::unique_ptr<StmtAST>> statements;

    // 检查缩进
    if (!expectToken(TOK_INDENT, "Expected indented block"))
        return statements;

    // 解析块内的语句
    while (currentToken.type != TOK_DEDENT && currentToken.type != TOK_EOF)
    {
        // 跳过空行
        if (currentToken.type == TOK_NEWLINE)
        {
            nextToken();
            continue;
        }

        // 如果遇到dedent，说明块结束了
        if (currentToken.type == TOK_DEDENT || currentToken.type == TOK_EOF)
            break;

        auto stmt = parseStatement();
        if (!stmt)
            return statements;

        statements.push_back(std::move(stmt));
    }

    // 检查dedent
    if (!expectToken(TOK_DEDENT, "Expected dedent at end of block"))
        return statements;

    return statements;
}

std::unique_ptr<ModuleAST> Parser::parseModule()
{
    int line = currentToken.line;
    int column = currentToken.column;

    // 收集模块中的语句和函数
    std::vector<std::unique_ptr<StmtAST>> statements;
    std::vector<std::unique_ptr<FunctionAST>> functions;

    // 跳过开头的空行
    while (currentToken.type == TOK_NEWLINE)
        nextToken();

    while (currentToken.type != TOK_EOF)
    {
        // 处理函数定义
        if (currentToken.type == TOK_DEF)
        {
            auto func = parseFunction();
            if (!func)
                return nullptr;

            functions.push_back(std::move(func));
        }
        // 处理类定义
        else if (currentToken.type == TOK_CLASS)
        {
            auto classStmt = parseClassDefinition();
            if (!classStmt)
                return nullptr;

            statements.push_back(std::move(classStmt));
        }
        // 处理其他语句
        else if (currentToken.type != TOK_NEWLINE)
        {
            auto stmt = parseStatement();
            if (!stmt)
                return nullptr;

            statements.push_back(std::move(stmt));
        }
        else
        {
            // 跳过多余的换行
            nextToken();
        }
    }

    auto module = std::make_unique<ModuleAST>(std::move(statements), std::move(functions));
    module->line = line;
    module->column = column;
    return module;
}

Lexer& Parser::getLexer()
{
    return lexer;
}

// 在initializeRegistries函数中注册列表表达式解析器

// 解析列表字面量
std::unique_ptr<ExprAST> Parser::parseListExpr()
{
    int line = currentToken.line;
    int column = currentToken.column;

    nextToken();  // 消费 '['

    std::vector<std::unique_ptr<ExprAST>> elements;

    // 空列表
    if (currentToken.type == TOK_RBRACK)
    {
        nextToken();  // 消费 ']'
        auto result = std::make_unique<ListExprAST>(std::move(elements));
        result->line = line;
        result->column = column;
        return result;
    }

    // 解析第一个元素
    auto element = parseExpression();
    if (!element)
        return nullptr;

    elements.push_back(std::move(element));

    // 解析其余元素
    while (currentToken.type == TOK_COMMA)
    {
        nextToken();  // 消费 ','

        // 允许尾随逗号
        if (currentToken.type == TOK_RBRACK)
            break;

        element = parseExpression();
        if (!element)
            return nullptr;

        elements.push_back(std::move(element));
    }

    if (!expectToken(TOK_RBRACK, "Expected ']' in list literal"))
        return nullptr;

    auto result = std::make_unique<ListExprAST>(std::move(elements));
    result->line = line;
    result->column = column;
    return result;
}

// 解析索引操作
std::unique_ptr<ExprAST> Parser::parseIndexExpr(std::unique_ptr<ExprAST> target)
{
    int line = currentToken.line;
    int column = currentToken.column;

    nextToken();  // 消费 '['

    auto index = parseExpression();
    if (!index)
        return nullptr;

    if (!expectToken(TOK_RBRACK, "Expected ']' after index expression"))
        return nullptr;

    auto result = std::make_unique<IndexExprAST>(std::move(target), std::move(index));
    result->line = line;
    result->column = column;
    return result;
}

//===----------------------------------------------------------------------===//
// OperatorPrecedence 类实现
//===----------------------------------------------------------------------===//

void OperatorPrecedence::addOperator(char op, int precedence)
{
    precedenceMap[op] = precedence;
}

int OperatorPrecedence::getTokenPrecedence(TokenType tokenType) const
{
    // 这里应该根据tokenType转换为对应的操作符字符
    // 简单起见，我们直接调用Parser类的静态方法
    return 0;  // 默认值，实际实现中会被覆盖
}
}  // namespace llvmpy