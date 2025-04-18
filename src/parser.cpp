#include "parser.h"
#include <iostream>
#include <memory>
#include <sstream>
#include "Debugdefine.h"
//#include "ast.h"
#include <stdexcept>  // 添加头文件以使用 std::runtime_error 或其他异常
namespace llvmpy
{

//===----------------------------------------------------------------------===//
// 静态成员初始化
//===----------------------------------------------------------------------===//

PyParserRegistry<PyTokenType, ExprAST> PyParser::exprRegistry;
PyParserRegistry<PyTokenType, StmtAST> PyParser::stmtRegistry;
std::unordered_map<PyTokenType, PyOperatorInfo> PyParser::operatorRegistry;
bool PyParser::isInitialized = false;


const int POWER_PRECEDENCE = 60;
const int UNARY_PLUS_MINUS_PRECEDENCE = 55; // 低于 **
const int NOT_PRECEDENCE = 8;

//===----------------------------------------------------------------------===//
// PyParseError 类实现
//===----------------------------------------------------------------------===//

std::string PyParseError::formatError() const
{
    std::stringstream ss;
    ss << "Parser error at line " << getLine() << ", column " << getColumn() << ": " << what();
    return ss.str();
}

//===----------------------------------------------------------------------===//
// 注册表初始化和管理方法
//===----------------------------------------------------------------------===//

void PyParser::initializeRegistries()
{
    if (isInitialized) return;

    // 1. 表达式原子/前缀解析器注册 (用于 parsePrimary)
    //    注册字面量、标识符、括号、列表/字典构造等原子表达式的解析器。
    //    一元运算符 (+, -, not) 由 parsePrimary 直接处理，不在此注册。
    registerExprParser(TOK_INTEGER, [](PyParser& p) { return p.parseNumberExpr(); });
    registerExprParser(TOK_FLOAT, [](PyParser& p) { return p.parseNumberExpr(); });
    // Note: TOK_NUMBER might be redundant if TOK_INTEGER/TOK_FLOAT cover all cases
    registerExprParser(TOK_NUMBER, [](PyParser& p) { return p.parseNumberExpr(); });
    registerExprParser(TOK_IDENTIFIER, [](PyParser& p) { return p.parseIdentifierExpr(); }); // Handles variables, function calls, indexing start
    registerExprParser(TOK_LPAREN, [](PyParser& p) { return p.parseParenExpr(); });     // Parenthesized expressions
    registerExprParser(TOK_STRING, [](PyParser& p) { return p.parseStringExpr(); });
    registerExprParser(TOK_BOOL, [](PyParser& p) { return p.parseBoolExpr(); });     // True, False
    registerExprParser(TOK_NONE, [](PyParser& p) { return p.parseNoneExpr(); });     // None
    registerExprParser(TOK_LBRACK, [](PyParser& p) { return p.parseListExpr(); });     // List literals [...]
    registerExprParser(TOK_LBRACE, [](PyParser& p) { return p.parseDictExpr(); });     // Dictionary literals {...}

    // 2. 语句解析器注册 (用于 parseStatement)
    //    注册语句关键字和可以启动语句的 Token。

    //    关键字启动的语句
    registerStmtParser(TOK_DEF, [](PyParser& p) -> std::unique_ptr<StmtAST> {
        // 解析函数定义
        auto funcAST = p.parseFunction();
        if (!funcAST) {
            // parseFunction 内部应该已经记录了错误
            return nullptr;
        }

        // 创建 FunctionDefStmtAST 来包装 FunctionAST
        // FunctionDefStmtAST 的构造函数会从 funcAST 继承位置信息
        auto funcDefStmt = std::make_unique<FunctionDefStmtAST>(std::move(funcAST));

        // CodeGen 稍后会处理 FunctionDefStmtAST，
        // 将函数对象注册到当前作用域。
        return funcDefStmt;
    });
    registerStmtParser(TOK_CLASS, [](PyParser& p) { return p.parseClassDefinition(); });
    registerStmtParser(TOK_RETURN, [](PyParser& p) { return p.parseReturnStmt(); });
    registerStmtParser(TOK_IF, [](PyParser& p) { return p.parseIfStmt(); }); // Handles if/elif/else chain
    registerStmtParser(TOK_WHILE, [](PyParser& p) { return p.parseWhileStmt(); });
    registerStmtParser(TOK_FOR, [](PyParser& p) { return p.parseForStmt(); }); // Assuming parseForStmt exists or logs unimplemented
    registerStmtParser(TOK_PRINT, [](PyParser& p) { return p.parsePrintStmt(); }); // Assuming a simple print function/statement
    registerStmtParser(TOK_IMPORT, [](PyParser& p) { return p.parseImportStmt(); });
    registerStmtParser(TOK_PASS, [](PyParser& p) { return p.parsePassStmt(); });

    //    标识符启动的语句 (复杂情况：赋值、复合赋值、索引赋值、表达式语句)
    registerStmtParser(TOK_IDENTIFIER, [](PyParser& p) -> std::unique_ptr<StmtAST> {
        // --- Existing complex logic for TOK_IDENTIFIER ---
        // (Handles simple assignment, compound assignment, index assignment,
        // and expression statements starting with an identifier like func() or var)
        // --- Keep the multi-case logic from the previous example ---
        auto state = p.saveState();
        std::string idName = p.getCurrentToken().value;
        int line = p.getCurrentToken().line;
        int column = p.getCurrentToken().column;
        p.nextToken(); // Consume identifier

        // Case 1: Indexing involved? id[...]
        if (p.getCurrentToken().type == TOK_LBRACK) {
            auto stateBeforeLBracket = p.saveState();
            p.nextToken(); // Consume '['
            auto indexExpr = p.parseExpression();
            if (!indexExpr) { p.restoreState(state); return p.parseExpressionStmt(); }
            if (p.getCurrentToken().type != TOK_RBRACK) { p.restoreState(state); return p.parseExpressionStmt(); } // Simplified error handling, assume expr stmt
            p.nextToken(); // Consume ']'

            if (p.getCurrentToken().type == TOK_ASSIGN) { // Index Assignment: id[index] = value
                p.nextToken(); // Consume '='
                auto valueExpr = p.parseExpression();
                if (!valueExpr) { p.restoreState(state); return p.parseExpressionStmt(); } // Simplified error handling

                auto varExpr = std::make_unique<VariableExprAST>(idName);
                varExpr->setLocation(line, column);

                // Ensure statement ends correctly
                 if (!p.expectStatementEnd("index assignment")) {
                     // If expectStatementEnd logs/throws, we might not need to return here.
                     // If it returns false, we might restore and parse as expr stmt, or return nullptr.
                     p.restoreState(state); // Example: Restore and try as expression statement on error
                     return p.parseExpressionStmt();
                 }

                return std::make_unique<IndexAssignStmtAST>(std::move(varExpr), std::move(indexExpr), std::move(valueExpr));
            } else { // Index Expression Statement: id[index]
                p.restoreState(state);
                return p.parseExpressionStmt();
            }
        }
        // Case 2: Compound assignment? id op= value
        else if (p.getCurrentToken().type == TOK_PLUS_ASSIGN || p.getCurrentToken().type == TOK_MINUS_ASSIGN ||
                 p.getCurrentToken().type == TOK_MUL_ASSIGN || p.getCurrentToken().type == TOK_DIV_ASSIGN ||
                 p.getCurrentToken().type == TOK_MOD_ASSIGN || p.getCurrentToken().type == TOK_POWER_ASSIGN ||
                 p.getCurrentToken().type == TOK_FLOOR_DIV_ASSIGN)
        {
            PyTokenType compoundOpType = p.getCurrentToken().type;
            int opLine = p.getCurrentToken().line;
            int opCol = p.getCurrentToken().column;
            PyTokenType binaryOpType = TOK_ERROR;
            switch (compoundOpType) { // Map compound op to binary op
                case TOK_PLUS_ASSIGN: binaryOpType = TOK_PLUS; break;
                case TOK_MINUS_ASSIGN: binaryOpType = TOK_MINUS; break;
                case TOK_MUL_ASSIGN: binaryOpType = TOK_MUL; break;
                case TOK_DIV_ASSIGN: binaryOpType = TOK_DIV; break;
                case TOK_MOD_ASSIGN: binaryOpType = TOK_MOD; break;
                case TOK_POWER_ASSIGN: binaryOpType = TOK_POWER; break;
                case TOK_FLOOR_DIV_ASSIGN: binaryOpType = TOK_FLOOR_DIV; break;
                default: return p.logParseError<StmtAST>("Internal error: Unhandled compound assignment operator");
            }
            p.nextToken(); // Consume compound assignment operator

            auto rightExpr = p.parseExpression();
            if (!rightExpr) { p.restoreState(state); return p.parseExpressionStmt(); } // Simplified error handling

            // Desugar: id op= val   =>   id = id op val
            auto varExprLeft = std::make_unique<VariableExprAST>(idName);
            varExprLeft->setLocation(line, column);
            auto varExprRight = std::make_unique<VariableExprAST>(idName);
            varExprRight->setLocation(line, column);
            auto binExpr = std::make_unique<BinaryExprAST>(binaryOpType, std::move(varExprRight), std::move(rightExpr));
            binExpr->setLocation(opLine, opCol);

            // Ensure statement ends correctly
            if (!p.expectStatementEnd("compound assignment")) {
                 p.restoreState(state);
                 return p.parseExpressionStmt();
            }

            auto assignStmt = std::make_unique<AssignStmtAST>(idName, std::move(binExpr));
            assignStmt->setLocation(line, column); // AssignStmt location is the variable's location
            return assignStmt;
        }
        // Case 3: Simple assignment? id = value
        else if (p.getCurrentToken().type == TOK_ASSIGN) {
            // Delegate to parseAssignStmt, passing the idName and its location
            // parseAssignStmt expects current token to be '='
            auto assignStmt = p.parseAssignStmt(idName); // parseAssignStmt should handle location
             // Check if the location was set by parseAssignStmt, otherwise set it here.
             // Use the line member which is std::optional<int>.
             if (assignStmt && !assignStmt->line.has_value()) {
                 // If parseAssignStmt didn't set location, set it here
                 assignStmt->setLocation(line, column);
             }
            return assignStmt; // parseAssignStmt handles consuming '=', value, and trailing newline
        }
        // Case 4: Must be an expression statement (func(), var, etc.)
        else {
            p.restoreState(state); // Restore to before 'id'
            return p.parseExpressionStmt(); // Let parseExpressionStmt handle it
        }
        // --- End of complex logic for TOK_IDENTIFIER ---
    });

    //    Tokens that can start an expression statement (delegate to parseExpressionStmt)
    registerStmtParser(TOK_INTEGER, [](PyParser& p) { return p.parseExpressionStmt(); });
    registerStmtParser(TOK_FLOAT, [](PyParser& p) { return p.parseExpressionStmt(); });
    registerStmtParser(TOK_NUMBER, [](PyParser& p) { return p.parseExpressionStmt(); });
    registerStmtParser(TOK_STRING, [](PyParser& p) { return p.parseExpressionStmt(); });
    registerStmtParser(TOK_LPAREN, [](PyParser& p) { return p.parseExpressionStmt(); }); // e.g., (1 + 2)
    registerStmtParser(TOK_LBRACK, [](PyParser& p) { return p.parseExpressionStmt(); }); // e.g., [1, 2]
    registerStmtParser(TOK_LBRACE, [](PyParser& p) { return p.parseExpressionStmt(); }); // e.g., {'a': 1}
    registerStmtParser(TOK_PLUS, [](PyParser& p) { return p.parseExpressionStmt(); });   // e.g., +1
    registerStmtParser(TOK_MINUS, [](PyParser& p) { return p.parseExpressionStmt(); });  // e.g., -x
    registerStmtParser(TOK_NOT, [](PyParser& p) { return p.parseExpressionStmt(); });    // e.g., not flag
    registerStmtParser(TOK_BOOL, [](PyParser& p) { return p.parseExpressionStmt(); });   // e.g., True
    registerStmtParser(TOK_NONE, [](PyParser& p) { return p.parseExpressionStmt(); });   // e.g., None

    //    INDENT/DEDENT handling (primarily for block structure, not true statements)
    //    These might be better handled within parseBlock, but registering them
    //    can prevent crashes if they appear unexpectedly at the statement level.
    registerStmtParser(TOK_INDENT, [](PyParser& p) -> std::unique_ptr<StmtAST> {
        p.logParseError<StmtAST>("Unexpected indent"); // Indent should follow ':' and newline
        p.nextToken(); // Consume to potentially recover
        return nullptr; // Or maybe a PassStmt? Error seems more appropriate.
    });
    registerStmtParser(TOK_DEDENT, [](PyParser& p) -> std::unique_ptr<StmtAST> {
        // Dedent signifies the end of a block, it shouldn't start a statement.
        // If encountered here, it likely means an empty block or incorrect indentation.
        p.logParseError<StmtAST>("Unexpected dedent");
        // Don't consume, let the block parsing logic handle it if possible.
        return nullptr;
    });

    // 3. 二元操作符注册 (用于 parseExpressionPrecedence)
    //    注册二元运算符的类型、优先级和结合性。
    //    优先级根据 Python 规则设定。

    // Logical OR
    registerOperator(TOK_OR, 4);
    // Logical AND
    registerOperator(TOK_AND, 5);
    // Logical NOT is unary (handled in parsePrimary, precedence 8)

    // Comparisons (all have same precedence level 10 in Python)
    registerOperator(TOK_LT, 10);
    registerOperator(TOK_GT, 10);
    registerOperator(TOK_LE, 10);
    registerOperator(TOK_GE, 10);
    registerOperator(TOK_EQ, 10);    // ==
    registerOperator(TOK_NEQ, 10);   // !=
    registerOperator(TOK_IS, 10);
    registerOperator(TOK_IS_NOT, 10); // Requires lexer to produce TOK_IS_NOT or parser to handle 'is' followed by 'not'
    registerOperator(TOK_IN, 10);
    registerOperator(TOK_NOT_IN, 10); // Requires lexer to produce TOK_NOT_IN or parser to handle 'not' followed by 'in'

    // Binary Additive
    registerOperator(TOK_PLUS, 20);
    registerOperator(TOK_MINUS, 20); // Binary minus

    // Multiplicative
    registerOperator(TOK_MUL, 40);
    registerOperator(TOK_DIV, 40);       // True division /
    registerOperator(TOK_FLOOR_DIV, 40); // Floor division //
    registerOperator(TOK_MOD, 40);       // Modulo %

    // Unary Plus/Minus (handled in parsePrimary, precedence 55)

    // Power (Right associative)
    registerOperator(TOK_POWER, POWER_PRECEDENCE, true); // Use constant, e.g., 60

    // Bitwise operators (if supported, add here with correct precedence)
    // registerOperator(TOK_BITWISE_OR, precedence);  // |
    // registerOperator(TOK_BITWISE_XOR, precedence); // ^
    // registerOperator(TOK_BITWISE_AND, precedence); // &
    // registerOperator(TOK_LSHIFT, precedence);      // <<
    // registerOperator(TOK_RSHIFT, precedence);      // >>
    // Unary bitwise NOT '~' would be handled in parsePrimary

    // Assignment (=) and compound assignments (+= etc.) are handled
    // in the TOK_IDENTIFIER statement parser, not as binary operators here.

    // Mark initialization as complete
    isInitialized = true;
}

// Helper function to check for statement termination (newline, EOF, or dedent)
// Returns true if terminated correctly (and consumes newline if present), false otherwise.
bool PyParser::expectStatementEnd(const std::string& statementType) {
    if (currentToken.type == TOK_NEWLINE) {
        nextToken(); // Consume newline
        return true;
    } else if (currentToken.type == TOK_EOF || currentToken.type == TOK_DEDENT) {
        // End of file or block is also a valid statement end, don't consume.
        return true;
    } else {
        // Log error, but don't throw here, let the caller decide how to handle.
        // Using logParseError would throw, maybe a different logging function is needed?
        // For now, let's assume logParseError is acceptable or adapt it.
        logParseError<StmtAST>("Expected newline or end of block/file after " + statementType + " statement, got " + lexer.getTokenName(currentToken.type));
        return false; // Indicate failure
    }
}

void PyParser::registerExprParser(PyTokenType type, PyExprParserFunc parser)
{
    exprRegistry.registerParser(type, std::move(parser));
}

void PyParser::registerStmtParser(PyTokenType type, PyStmtParserFunc parser)
{
    stmtRegistry.registerParser(type, std::move(parser));
}

// void PyParser::registerOperator(PyTokenType type, char symbol, int precedence, bool rightAssoc)
// {
//     operatorRegistry[type] = PyOperatorInfo(symbol, precedence, rightAssoc);
// }
void PyParser::registerOperator(PyTokenType type, int precedence, bool rightAssoc)
{
    // PyOperatorInfo 构造函数现在直接接受 PyTokenType
    operatorRegistry[type] = PyOperatorInfo(type, precedence, rightAssoc);
}

//===----------------------------------------------------------------------===//
// PyParser 类实现
//===----------------------------------------------------------------------===//

PyParser::PyParser(PyLexer& l) : lexer(l), currentToken(TOK_EOF, "", 1, 1)
{
    // 确保注册表已初始化
    initializeRegistries();

    // 获取第一个token
    nextToken();
}

void PyParser::nextToken()
{
#ifdef DEBUG_PARSER_NextToken_detailed
    std::cerr << "Debug: Entering nextToken. Current token before getNextToken: '" << currentToken.value
              << "' type: " << lexer.getTokenName(currentToken.type) << std::endl;  // Log before
#endif
    currentToken = lexer.getNextToken();
#ifdef DEBUG_PARSER_NextToken_detailed
    std::cerr << "Debug: Exiting nextToken. Current token after getNextToken: '" << currentToken.value
              << "' type: " << lexer.getTokenName(currentToken.type) << std::endl;  // Log after
#endif
#ifdef DEBUG_PARSER_NextToken
    // 原始调试输出保持不变
    std::cerr << "Debug: Next token: '" << currentToken.value
              << "' type: " << lexer.getTokenName(currentToken.type)
              << " at line " << currentToken.line
              << ", col " << currentToken.column << std::endl;
#endif
}

bool PyParser::expectToken(PyTokenType type, const std::string& errorMessage)
{
    if (currentToken.type != type)
    {
        logParseError<ASTNode>(errorMessage);
        return false;
    }
    nextToken();  // 消费预期的token
    return true;
}

bool PyParser::match(PyTokenType type)
{
    if (currentToken.type != type)
    {
        return false;
    }
    nextToken();  // 消费匹配的token
    return true;
}

void PyParser::skipNewlines()
{
    while (currentToken.type == TOK_NEWLINE)
    {
        nextToken();
    }
}

PyParser::PyParserState PyParser::saveState() const
{
    // return PyParserState(currentToken, lexer.peekPosition());
    return PyParserState(currentToken, lexer.saveState().tokenIndex);  // 保存 tokenIndex
}

void PyParser::restoreState(const PyParserState& state)
{
    currentToken = state.token;
    //lexer.resetPosition(state.lexerPosition);
    lexer.restoreState(PyLexerState(state.lexerPosition));  // 使用 lexer 的 restoreState
}

void PyParser::dumpCurrentToken() const
{
    std::cerr << "Current Token: type=" << lexer.getTokenName(currentToken.type)
              << ", value='" << currentToken.value
              << "', line=" << currentToken.line
              << ", col=" << currentToken.column << std::endl;
}

//===----------------------------------------------------------------------===//
// 表达式解析方法
//===----------------------------------------------------------------------===//

std::unique_ptr<ExprAST> PyParser::parseNumberExpr()
{
    double value = std::stod(currentToken.value);
    auto result = makeExpr<NumberExprAST>(value);
    PyToken consumedToken = currentToken;  // 保存 token 信息用于日志
    nextToken();                           // 消费数字 token
#ifdef DEBUG_PARSER_Expr                   // 在 DEBUG_PARSER_Expr 宏下添加日志
    std::cerr << "Debug [parseNumberExpr]: Consumed '" << consumedToken.value
              << "' (type: " << lexer.getTokenName(consumedToken.type)
              << ") at L" << consumedToken.line << " C" << consumedToken.column
              << ". Next token is now: '" << currentToken.value
              << "' (type: " << lexer.getTokenName(currentToken.type)
              << ") at L" << currentToken.line << " C" << currentToken.column << std::endl;
#endif
    return result;
}

std::unique_ptr<ExprAST> PyParser::parseParenExpr()
{
    int line = currentToken.line;
    int column = currentToken.column;

    nextToken();  // 消费'('

    auto expr = parseExpression();
    if (!expr)
        return nullptr;

    if (!expectToken(TOK_RPAREN, "Expected ')'"))
        return nullptr;

    expr->setLocation(line, column);
    return expr;
}

std::unique_ptr<ExprAST> PyParser::parseIdentifierExpr()
{
    std::string idName = currentToken.value;
    int line = currentToken.line;
    int column = currentToken.column;
    PyToken idToken = currentToken;  // 保存标识符 token 信息

    nextToken();  // 消费标识符

    // 函数调用
    if (currentToken.type == TOK_LPAREN)
    {
#ifdef DEBUG_PARSER_Expr
        std::cerr << "Debug [parseIdentifierExpr]: Detected function call for '" << idName << "' at L" << line << " C" << column << ". Current token: LPAREN" << std::endl;
#endif
        nextToken();  // 消费 '('
        std::vector<std::unique_ptr<ExprAST>> args;

        // 处理参数列表
        if (currentToken.type != TOK_RPAREN)  // 检查是否是空参数列表 '()'
        {
            while (true)
            {
#ifdef DEBUG_PARSER_Expr
                std::cerr << "Debug [parseIdentifierExpr]: Before parsing argument. Current token: " << lexer.getTokenName(currentToken.type) << " ('" << currentToken.value << "') at L" << currentToken.line << " C" << currentToken.column << std::endl;
#endif
                auto arg = parseExpression();  // 解析一个参数表达式
                if (!arg)
                {
#ifdef DEBUG_PARSER_Expr
                    std::cerr << "Debug [parseIdentifierExpr]: Argument parsing failed. Current token: " << lexer.getTokenName(currentToken.type) << " ('" << currentToken.value << "')" << std::endl;
#endif
                    return nullptr;  // 参数解析失败
                }
#ifdef DEBUG_PARSER_Expr
                // 在参数解析后添加更详细的日志
                std::cerr << "Debug [parseIdentifierExpr]: After parsing argument. Current token: " << lexer.getTokenName(currentToken.type) << " ('" << currentToken.value << "') at L" << currentToken.line << " C" << currentToken.column << std::endl;
#endif
                args.push_back(std::move(arg));

                // 查看下一个 token
                if (currentToken.type == TOK_RPAREN)
                {  // 如果是右括号，参数列表结束
#ifdef DEBUG_PARSER_Expr
                    std::cerr << "Debug [parseIdentifierExpr]: Found RPAREN, ending argument list." << std::endl;
#endif
                    break;
                }

                if (currentToken.type == TOK_COMMA)
                {  // 如果是逗号
#ifdef DEBUG_PARSER_Expr
                    std::cerr << "Debug [parseIdentifierExpr]: Found COMMA, consuming and expecting next argument or RPAREN." << std::endl;
#endif
                    nextToken();  // 消费逗号
                    // 处理可能的尾随逗号，例如 func(a, b, )
                    if (currentToken.type == TOK_RPAREN)
                    {
                        std::cerr << "Warning: Trailing comma detected in argument list at line "
                                  << currentToken.line << ", col " << currentToken.column << std::endl;
#ifdef DEBUG_PARSER_Expr
                        std::cerr << "Debug [parseIdentifierExpr]: Found RPAREN after trailing comma, ending argument list." << std::endl;
#endif
                        break;  // 参数列表结束
                    }
                    // 逗号后面应该继续解析下一个参数，循环会继续
                }
                else
                {
                    // 既不是逗号也不是右括号，说明语法错误
                    return logParseError<ExprAST>("Expected ',' or ')' in argument list, got " + lexer.getTokenName(currentToken.type));
                }
            }
        }
        else
        {
#ifdef DEBUG_PARSER_Expr
            std::cerr << "Debug [parseIdentifierExpr]: Empty argument list detected (RPAREN found immediately)." << std::endl;
#endif
        }

        // 消费右括号
        if (!expectToken(TOK_RPAREN, "Expected ')' after arguments"))
        {
#ifdef DEBUG_PARSER_Expr
            std::cerr << "Debug [parseIdentifierExpr]: Failed to find expected RPAREN after arguments." << std::endl;
#endif
            return nullptr;
        }
#ifdef DEBUG_PARSER_Expr
        std::cerr << "Debug [parseIdentifierExpr]: Successfully parsed function call '" << idName << "'." << std::endl;
#endif

        auto callExpr = makeExpr<CallExprAST>(idName, std::move(args));
        callExpr->setLocation(line, column);  // 设置位置信息
        return callExpr;
    }

    // 索引操作
    if (currentToken.type == TOK_LBRACK)
    {
        auto varExpr = makeExpr<VariableExprAST>(idName);
        varExpr->setLocation(line, column);  // 设置位置信息
        return parseIndexExpr(std::move(varExpr));
    }

    // 简单变量引用
#ifdef DEBUG_PARSER_Expr
    std::cerr << "Debug [parseIdentifierExpr]: Parsed simple variable '" << idName << "'." << std::endl;
#endif
    auto varExpr = makeExpr<VariableExprAST>(idName);
    varExpr->setLocation(line, column);  // 设置位置信息
    return varExpr;
}

std::unique_ptr<ExprAST> PyParser::parseStringExpr()
{
    // 实现字符串解析
    std::string strValue = currentToken.value;
    auto result = makeExpr<StringExprAST>(strValue);
    nextToken();  // 消费字符串
    return result;
}

std::unique_ptr<ExprAST> PyParser::parseBoolExpr()
{
    // 实现布尔值解析
    bool boolValue = (currentToken.value == "True");
    auto result = makeExpr<BoolExprAST>(boolValue);
    nextToken();  // 消费布尔值
    return result;
}

std::unique_ptr<ExprAST> PyParser::parseNoneExpr()
{
    // 实现None值解析
    auto result = makeExpr<NoneExprAST>();
    nextToken();  // 消费None
    return result;
}



std::unique_ptr<ExprAST> PyParser::parsePrimary()
{
    PyTokenType currentType = currentToken.type;
    int line = currentToken.line;
    int column = currentToken.column;

    // 处理前缀一元运算符
    if (currentType == TOK_PLUS || currentType == TOK_MINUS) {
        nextToken(); // 消费 '+' 或 '-'
        // 递归调用 parseExpressionPrecedence，使用一元运算符的优先级
        auto operand = parseExpressionPrecedence(UNARY_PLUS_MINUS_PRECEDENCE);
        if (!operand) {
            return logParseError<ExprAST>("Expected expression after unary '+' or '-'");
        }
        auto unaryExpr = makeExpr<UnaryExprAST>(currentType, std::move(operand));
        unaryExpr->setLocation(line, column); // 位置是操作符的位置
        return unaryExpr;
    } else if (currentType == TOK_NOT) {
        nextToken(); // 消费 'not'
        // 递归调用 parseExpressionPrecedence，使用 'not' 的优先级
        auto operand = parseExpressionPrecedence(NOT_PRECEDENCE);
        if (!operand) {
            return logParseError<ExprAST>("Expected expression after 'not'");
        }
        auto unaryExpr = makeExpr<UnaryExprAST>(currentType, std::move(operand));
        unaryExpr->setLocation(line, column); // 位置是操作符的位置
        return unaryExpr;
    }

    // 处理原子表达式 (通过注册表)
    auto parser = exprRegistry.getParser(currentType);
    if (parser) {
        // 调用注册的原子解析函数 (如 parseNumberExpr, parseIdentifierExpr 等)
        return parser(*this);
    }

    // 没有匹配的前缀运算符或原子解析器
    return logParseError<ExprAST>("Unexpected token when expecting an expression or prefix operator: '" + currentToken.value + "' (" + lexer.getTokenName(currentType) + ")");
}



// 新的 parseExpressionPrecedence 函数，实现 Pratt 解析器的核心逻辑
std::unique_ptr<ExprAST> PyParser::parseExpressionPrecedence(int minPrecedence)
{
    // 1. 解析左侧表达式 (原子或前缀表达式)
    auto lhs = parsePrimary();
    if (!lhs) {
        return nullptr; // 如果 primary 解析失败，则表达式无效
    }

    // 2. 循环处理优先级 >= minPrecedence 的二元运算符
    while (true) {
        // 查看当前 token 是否是已注册的二元运算符
        auto it = operatorRegistry.find(currentToken.type);
        if (it == operatorRegistry.end()) {
            break; // 不是二元运算符，循环结束
        }

        const PyOperatorInfo& opInfo = it->second;
        int currentPrec = opInfo.precedence;

        // 如果当前运算符优先级低于要求的最低优先级，则停止
        if (currentPrec < minPrecedence) {
            break;
        }

        // 记录操作符信息并消费
        PyTokenType opType = opInfo.opType;
        bool isRightAssoc = opInfo.rightAssoc;
        int opLine = currentToken.line;
        int opCol = currentToken.column;
        nextToken(); // 消费二元运算符

        // 3. 解析右侧表达式
        // 对于左结合运算符，右侧需要解析比当前运算符优先级更高的表达式 (currentPrec + 1)
        // 对于右结合运算符，右侧需要解析优先级等于或高于当前运算符的表达式 (currentPrec)
        int nextMinPrecedence = currentPrec + (isRightAssoc ? 0 : 1);
        auto rhs = parseExpressionPrecedence(nextMinPrecedence);
        if (!rhs) {
            return logParseError<ExprAST>("Expected expression after binary operator '" + lexer.getTokenName(opType) + "'");
        }

        // 4. 合并 LHS 和 RHS
        auto newLhs = makeExpr<BinaryExprAST>(opType, std::move(lhs), std::move(rhs));
        newLhs->setLocation(opLine, opCol); // 位置是操作符的位置
        lhs = std::move(newLhs); // 将新构建的二元表达式作为下一次循环的 LHS
    }

    return lhs; // 返回最终构建的表达式树
}


std::unique_ptr<ExprAST> PyParser::parseBinOpRHS(int exprPrec, std::unique_ptr<ExprAST> LHS)
{
    while (true)
    {
        // 获取当前token对应的操作符信息
        auto it = operatorRegistry.find(currentToken.type);
        if (it == operatorRegistry.end())
            return LHS;  // 不是已注册的二元操作符

        const PyOperatorInfo& opInfo = it->second;

        // 获取当前token的操作符优先级
        int tokPrec = opInfo.precedence;

        // 如果优先级过低，则返回当前的LHS
        if (tokPrec < exprPrec)
            return LHS;

        // --- 修改：保存操作符 Token 类型 ---
        PyTokenType opType = opInfo.opType;  // 使用 opType 成员
        // char binOp = opInfo.symbol; // 不再需要 char symbol
        bool isRightAssoc = opInfo.rightAssoc;
        // --- 结束修改 ---

        int line = currentToken.line;
        int column = currentToken.column;

        nextToken();  // 消费操作符

        // 解析RHS
        auto RHS = parsePrimary();
        if (!RHS)
            return nullptr;  // 解析 RHS 失败

        // 比较下一个操作符的优先级
        auto nextIt = operatorRegistry.find(currentToken.type);
        int nextPrec = (nextIt != operatorRegistry.end()) ? nextIt->second.precedence : -1;

        // 处理右结合性和优先级
        // 如果当前操作符优先级低于下一个，或者同级且是左结合，则先处理下一个
        if (tokPrec < nextPrec || (!isRightAssoc && tokPrec == nextPrec))
        {
            // 如果下一个操作符优先级更高，递归调用 parseBinOpRHS 处理 RHS
            // 注意：传递的优先级是当前操作符的优先级 + 1 (对于左结合) 或 当前优先级 (对于右结合)
            int nextExprPrec = tokPrec + (isRightAssoc ? 0 : 1);
            RHS = parseBinOpRHS(nextExprPrec, std::move(RHS));
            if (!RHS)
                return nullptr;  // 递归解析失败
        }
        // 如果当前操作符优先级更高，或者同级且是右结合，则先合并当前的 LHS 和 RHS

        auto binExpr = makeExpr<BinaryExprAST>(opType, std::move(LHS), std::move(RHS));

        binExpr->setLocation(line, column);  // 设置为操作符的位置
        LHS = std::move(binExpr);            // 将新构建的表达式作为下一次循环的 LHS
    }
}

// 在parseExpression函数中添加
std::unique_ptr<ExprAST> PyParser::parseExpression()
{
#ifdef DEBUG_PARSER_Expr
    std::cerr << "Debug [parseExpression]: Starting expression parsing. Current token: "
              << lexer.getTokenName(currentToken.type) << " ('" << currentToken.value << "') at L"
              << currentToken.line << " C" << currentToken.column << std::endl;
#endif
    // 开始解析，最低优先级为 0
    auto result = parseExpressionPrecedence(0);

#ifdef DEBUG_PARSER_Expr
    if (result) {
         std::cerr << "Debug [parseExpression]: Finished expression parsing. Next token: "
                   << lexer.getTokenName(currentToken.type) << " ('" << currentToken.value << "') at L"
                   << currentToken.line << " C" << currentToken.column << std::endl;
    } else {
         std::cerr << "Debug [parseExpression]: Expression parsing failed." << std::endl;
    }
#endif
    // 类型检查 (如果需要)
     if (result) {
         auto type = result->getType();
         if (!type) {
             std::cerr << "Warning: Expression has no type information" << std::endl;
         }
     }

    return result;
}

// 解析字典字面量
std::unique_ptr<ExprAST> PyParser::parseDictExpr()
{
    int line = currentToken.line;
    int column = currentToken.column;
#ifdef DEBUG_PARSER_Expr
    std::cerr << "Debug [parseDictExpr]: Starting dictionary literal parsing at L" << line << " C" << column << std::endl;
#endif

    nextToken();  // Consume '{'

    std::vector<std::pair<std::unique_ptr<ExprAST>, std::unique_ptr<ExprAST>>> pairs;

    // Handle empty dictionary {}
    if (currentToken.type == TOK_RBRACE)
    {
#ifdef DEBUG_PARSER_Expr
        std::cerr << "Debug [parseDictExpr]: Parsed empty dictionary." << std::endl;
#endif
        nextToken();  // Consume '}'
        auto dictExpr = makeExpr<DictExprAST>(std::move(pairs));
        dictExpr->setLocation(line, column);
        return dictExpr;
    }

    // Parse key-value pairs
    while (true)
    {
        // Parse Key
#ifdef DEBUG_PARSER_Expr
        std::cerr << "Debug [parseDictExpr]: Parsing dictionary key. Current token: " << lexer.getTokenName(currentToken.type) << std::endl;
#endif
        auto key = parseExpression();
        if (!key)
        {
#ifdef DEBUG_PARSER_Expr
            std::cerr << "Debug [parseDictExpr]: Failed to parse dictionary key." << std::endl;
#endif
            return nullptr;  // Error already logged by parseExpression
        }

        // Expect ':'
        if (!expectToken(TOK_COLON, "Expected ':' after dictionary key"))
        {
            return nullptr;
        }

        // Parse Value
#ifdef DEBUG_PARSER_Expr
        std::cerr << "Debug [parseDictExpr]: Parsing dictionary value. Current token: " << lexer.getTokenName(currentToken.type) << std::endl;
#endif
        auto value = parseExpression();
        if (!value)
        {
#ifdef DEBUG_PARSER_Expr
            std::cerr << "Debug [parseDictExpr]: Failed to parse dictionary value." << std::endl;
#endif
            return nullptr;  // Error already logged by parseExpression
        }

        pairs.emplace_back(std::move(key), std::move(value));

        // Check for '}' or ','
        if (currentToken.type == TOK_RBRACE)
        {
#ifdef DEBUG_PARSER_Expr
            std::cerr << "Debug [parseDictExpr]: Found '}', ending dictionary literal." << std::endl;
#endif
            break;  // End of dictionary
        }

        if (currentToken.type == TOK_COMMA)
        {
            nextToken();  // Consume ','
            // Handle trailing comma: {key: value, }
            if (currentToken.type == TOK_RBRACE)
            {
#ifdef DEBUG_PARSER_Expr
                std::cerr << "Debug [parseDictExpr]: Found trailing comma before '}'." << std::endl;
#endif
                break;  // End of dictionary
            }
            // Continue to next key-value pair
#ifdef DEBUG_PARSER_Expr
            std::cerr << "Debug [parseDictExpr]: Found ',', expecting next key-value pair." << std::endl;
#endif
        }
        else
        {
            return logParseError<ExprAST>("Expected ',' or '}' in dictionary literal, got " + lexer.getTokenName(currentToken.type));
        }
    }

    // Expect closing '}'
    if (!expectToken(TOK_RBRACE, "Expected '}' at end of dictionary literal"))
    {
        return nullptr;
    }

#ifdef DEBUG_PARSER_Expr
    std::cerr << "Debug [parseDictExpr]: Successfully parsed dictionary literal with " << pairs.size() << " pairs." << std::endl;
#endif
    auto dictExpr = makeExpr<DictExprAST>(std::move(pairs));
    dictExpr->setLocation(line, column);
    return dictExpr;
}

// 解析列表字面量
std::unique_ptr<ExprAST> PyParser::parseListExpr()
{
    int line = currentToken.line;
    int column = currentToken.column;

    nextToken();  // 消费'['

    std::vector<std::unique_ptr<ExprAST>> elements;

    // 处理空列表
    if (currentToken.type == TOK_RBRACK)
    {
        nextToken();  // 消费']'
        auto listExpr = makeExpr<ListExprAST>(std::move(elements));
        listExpr->setLocation(line, column);
        return listExpr;
    }

    while (true)
    {
        auto element = parseExpression();
        if (!element)
            return nullptr;

        elements.push_back(std::move(element));

        if (currentToken.type == TOK_RBRACK)
            break;

        if (!expectToken(TOK_COMMA, "Expected ',' or ']' in list literal"))
            return nullptr;

        if (currentToken.type == TOK_RBRACK)
            break;
    }

    if (!expectToken(TOK_RBRACK, "Expected ']' at end of list literal"))
        return nullptr;

    auto listExpr = makeExpr<ListExprAST>(std::move(elements));
    listExpr->setLocation(line, column);
    return listExpr;
}

// 解析索引表达式
std::unique_ptr<ExprAST> PyParser::parseIndexExpr(std::unique_ptr<ExprAST> target)
{
    int line = currentToken.line;
    int column = currentToken.column;

    nextToken();  // 消费'['

    auto index = parseExpression();
    if (!index)
        return nullptr;

    if (!expectToken(TOK_RBRACK, "Expected ']' after index expression"))
        return nullptr;

    auto indexExpr = makeExpr<IndexExprAST>(std::move(target), std::move(index));
    indexExpr->setLocation(line, column);
    return indexExpr;
}

//===----------------------------------------------------------------------===//
// 语句解析方法
//===----------------------------------------------------------------------===//

std::unique_ptr<StmtAST> PyParser::parseStatement()
{
    // 使用注册表查找解析器
    auto parser = stmtRegistry.getParser(currentToken.type);
    if (parser)
    {
        return parser(*this);
    }

    // 没有匹配的解析器，报告错误
    return logParseError<StmtAST>("Unknown statement type: " + lexer.getTokenName(currentToken.type));
}

/* std::unique_ptr<StmtAST> PyParser::parseExpressionStmt()
{
    auto expr = parseExpression();
    if (!expr)
        return nullptr;

    // 可选的分号/换行
    if (currentToken.type == TOK_NEWLINE)
        nextToken();  // 消费换行

    return makeStmt<ExprStmtAST>(std::move(expr));
} */
std::unique_ptr<StmtAST> PyParser::parseExpressionStmt()
{
#ifdef DEBUG_PARSER_Stmt  // 在 DEBUG_PARSER_Stmt 宏下添加日志
    std::cerr << "Debug [parseExpressionStmt]: Starting to parse expression statement. Current token: "
              << lexer.getTokenName(currentToken.type) << " ('" << currentToken.value << "') at L"
              << currentToken.line << " C" << currentToken.column << std::endl;
#endif
    auto expr = parseExpression();  // 尝试解析整个表达式语句
    if (!expr)
    {
#ifdef DEBUG_PARSER_Stmt
        std::cerr << "Debug [parseExpressionStmt]: parseExpression returned nullptr." << std::endl;
#endif
        return nullptr;
    }
#ifdef DEBUG_PARSER_Stmt
    std::cerr << "Debug [parseExpressionStmt]: Successfully parsed expression. Checking token after expression. Current token: "
              << lexer.getTokenName(currentToken.type) << " ('" << currentToken.value << "') at L"
              << currentToken.line << " C" << currentToken.column << std::endl;
#endif

    // 检查表达式语句结束后是否是预期的 token (换行符、文件结束或块结束)
    if (currentToken.type == TOK_NEWLINE)
    {
#ifdef DEBUG_PARSER_Stmt
        std::cerr << "Debug [parseExpressionStmt]: Found NEWLINE, consuming it." << std::endl;
#endif
        nextToken();  // 消费换行符
    }
    else if (currentToken.type != TOK_EOF && currentToken.type != TOK_DEDENT)
    {
        // 如果表达式后面跟着非预期的 token，报告错误
#ifdef DEBUG_PARSER_Stmt
        std::cerr << "Debug [parseExpressionStmt]: Unexpected token after expression: "
                  << lexer.getTokenName(currentToken.type) << " ('" << currentToken.value << "'). Reporting error." << std::endl;
#endif
        // 特别检查是否是逗号，这可能指示函数调用解析不完整
        if (currentToken.type == TOK_COMMA)
        {
            return logParseError<StmtAST>("Unexpected comma after expression. Function call argument parsing might be incomplete.");
        }
        // 其他非预期 token
        return logParseError<StmtAST>("Expected newline or end of block/file after expression statement, got " + lexer.getTokenName(currentToken.type));
    }
    // 如果是 TOK_EOF 或 TOK_DEDENT，则表示语句或块正常结束，无需消费
#ifdef DEBUG_PARSER_Stmt
    else
    {
        std::cerr << "Debug [parseExpressionStmt]: Found " << lexer.getTokenName(currentToken.type) << ", statement ends correctly." << std::endl;
    }
    std::cerr << "Debug [parseExpressionStmt]: Returning ExprStmtAST." << std::endl;
#endif

    return makeStmt<ExprStmtAST>(std::move(expr));
}
std::unique_ptr<StmtAST> PyParser::parseReturnStmt()
{
    int line = currentToken.line;
    int column = currentToken.column;

    nextToken();  // 消费'return'

    // 处理空return语句
    if (currentToken.type == TOK_NEWLINE)
    {
        nextToken();  // 消费换行
        auto noneExpr = makeExpr<NoneExprAST>();
        auto stmt = makeStmt<ReturnStmtAST>(std::move(noneExpr));
        return stmt;
    }

    auto value = parseExpression();
    if (!value)
        return nullptr;

    // 确保语句后有NEWLINE，并消费它
    if (currentToken.type == TOK_NEWLINE)
        nextToken();  // 消费换行

    auto stmt = makeStmt<ReturnStmtAST>(std::move(value));
    return stmt;
}

std::unique_ptr<StmtAST> PyParser::parseIfStmt()
{
    int startLine = currentToken.line;
    int startCol = currentToken.column;
    nextToken();  // 消费 'if'

    // 1. 解析 'if' 条件
    auto condition = parseExpression();
    if (!condition)
        return nullptr;

    // 2. 期望 ':' 和 NEWLINE
    if (!expectToken(TOK_COLON, "Expected ':' after if condition"))
        return nullptr;
    // Python 允许在 ':' 后直接换行，或有注释后再换行
    while (currentToken.type != TOK_NEWLINE && currentToken.type != TOK_EOF)
    {
        // 可以选择性地处理注释或简单地跳过
        nextToken();
    }
    if (!expectToken(TOK_NEWLINE, "Expected newline after ':' in if statement"))
        return nullptr;

    // 3. 解析 'then' 代码块
    auto thenBody = parseBlock();
    if (thenBody.empty())
    {  // Python 要求 if 后面必须有代码块 (至少一个 pass)
        return logParseError<StmtAST>("Expected an indented block after 'if' statement");
    }
    // 注意：parseBlock 应该处理 INDENT 和 DEDENT

    // 4. 解析 'else' 部分 (可能是 elif 或 else)
    std::unique_ptr<StmtAST> elseNode = nullptr;  // 初始化 else 部分为 null

    if (currentToken.type == TOK_ELIF)
    {
        // 如果是 'elif'，递归地调用 parseIfStmt 来处理 elif 及后续链条
        // 'elif' 本质上就是一个新的 'if' 语句，出现在上一个 'if' 的 else 位置
        elseNode = parseIfStmt();       // 递归调用会处理 'elif' 关键字的消费
        if (!elseNode) return nullptr;  // 检查递归调用的结果
    }
    else if (currentToken.type == TOK_ELSE)
    {
        nextToken();  // 消费 'else'

        // 期望 ':' 和 NEWLINE
        if (!expectToken(TOK_COLON, "Expected ':' after 'else'"))
            return nullptr;
        while (currentToken.type != TOK_NEWLINE && currentToken.type != TOK_EOF)
        {
            nextToken();
        }
        if (!expectToken(TOK_NEWLINE, "Expected newline after ':' in else statement"))
            return nullptr;

        // 解析 'else' 代码块
        auto elseBodyStmts = parseBlock();
        if (elseBodyStmts.empty())
        {
            return logParseError<StmtAST>("Expected an indented block after 'else' statement");
        }

        // 将 else 块封装成 BlockStmtAST (或者根据你的 AST 设计调整)
        // 如果 else 块只有一个语句，也可以直接用那个语句，但 BlockStmtAST 更通用
        elseNode = std::make_unique<BlockStmtAST>(std::move(elseBodyStmts));
        elseNode->setLocation(startLine, startCol);  // 或者使用 else 关键字的位置
    }

    // 5. 创建并返回顶层 IfStmtAST
    auto ifStmt = makeStmt<IfStmtAST>(std::move(condition), std::move(thenBody), std::move(elseNode));
    // makeStmt 应该会设置初始 'if' 的位置，或者在这里手动设置
    ifStmt->setLocation(startLine, startCol);
    return ifStmt;
}

/* std::unique_ptr<StmtAST> PyParser::parseElifPart()
{
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

    // 解析elif语句体
    auto thenBody = parseBlock();

    // elif语句本质上是一个没有else的if语句
    return makeStmt<IfStmtAST>(std::move(condition),
                               std::move(thenBody),
                               std::vector<std::unique_ptr<StmtAST>>());
} */

std::unique_ptr<StmtAST> PyParser::parseElsePart(std::vector<std::unique_ptr<StmtAST>>& elseBody)
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

    // 解析else语句体
    auto elseStmts = parseBlock();

    // 将解析到的else语句添加到传入的elseBody中
    for (auto& stmt : elseStmts)
    {
        elseBody.push_back(std::move(stmt));
    }

    // 返回一个空语句，表示else部分已处理
    return makeStmt<PassStmtAST>();
}

std::unique_ptr<StmtAST> PyParser::parseWhileStmt()
{
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

    // 解析while语句体
    auto body = parseBlock();

    return makeStmt<WhileStmtAST>(std::move(condition), std::move(body));
}

std::unique_ptr<StmtAST> PyParser::parsePrintStmt()
{
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

    return makeStmt<PrintStmtAST>(std::move(expr));
}

std::unique_ptr<StmtAST> PyParser::parseAssignStmt(const std::string& varName)
{
    if (currentToken.type != TOK_ASSIGN)
    {
        return logParseError<StmtAST>("Expected '=' in assignment");
    }

    nextToken();  // 消费'='

    auto value = parseExpression();
    if (!value)
        return nullptr;

    // 可选的类型注解处理
    std::shared_ptr<PyType> typeAnnotation = tryParseTypeAnnotation();

    // 创建赋值语句
    auto stmt = std::make_unique<AssignStmtAST>(varName, std::move(value));

    // 设置位置信息
    stmt->setLocation(currentToken.line, currentToken.column);

    // 确保语句正确结束
    if (currentToken.type == TOK_NEWLINE)
    {
        nextToken();  // 消费换行符
    }
    else if (currentToken.type != TOK_EOF)
    {
        return logParseError<StmtAST>("Expected newline after assignment");
    }

    return stmt;
}

std::unique_ptr<StmtAST> PyParser::parseForStmt()
{
    // For语句暂时不实现，返回错误
    return logParseError<StmtAST>("For statements not yet implemented");
}

std::unique_ptr<StmtAST> PyParser::parseImportStmt()
{
    nextToken();  // 消费'import'

    // 导入的模块名必须是标识符
    if (currentToken.type != TOK_IDENTIFIER)
    {
        return logParseError<StmtAST>("Expected module name after 'import'");
    }

    std::string moduleName = currentToken.value;
    nextToken();  // 消费模块名

    // 检查是否有别名
    std::string alias = "";
    if (currentToken.type == TOK_AS)
    {
        nextToken();  // 消费'as'

        if (currentToken.type != TOK_IDENTIFIER)
        {
            return logParseError<StmtAST>("Expected identifier after 'as'");
        }

        alias = currentToken.value;
        nextToken();  // 消费别名
    }

    // 可选的换行
    if (currentToken.type == TOK_NEWLINE)
        nextToken();  // 消费换行

    return makeStmt<ImportStmtAST>(moduleName, alias);
}

std::unique_ptr<StmtAST> PyParser::parsePassStmt()
{
    nextToken();  // 消费'pass'

    // 可选的换行
    if (currentToken.type == TOK_NEWLINE)
        nextToken();  // 消费换行

    return makeStmt<PassStmtAST>();
}

std::unique_ptr<StmtAST> PyParser::parseClassDefinition()
{
    nextToken();  // 消费'class'

    // 类名必须是标识符
    if (currentToken.type != TOK_IDENTIFIER)
    {
        return logParseError<StmtAST>("Expected class name after 'class'");
    }

    std::string className = currentToken.value;
    nextToken();  // 消费类名

    // 解析可能的基类列表
    std::vector<std::string> baseClasses;
    if (currentToken.type == TOK_LPAREN)
    {
        nextToken();  // 消费'('

        // 空的基类列表
        if (currentToken.type != TOK_RPAREN)
        {
            while (true)
            {
                if (currentToken.type != TOK_IDENTIFIER)
                {
                    return logParseError<StmtAST>("Expected base class name in inheritance list");
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
    std::vector<std::unique_ptr<StmtAST>> classBody;
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

        // 解析方法定义 (以def开头)
        if (currentToken.type == TOK_DEF)
        {
            auto method = parseFunction();
            if (!method)
                return nullptr;

            methods.push_back(std::move(method));
        }
        // 解析其他类体语句
        else
        {
            auto stmt = parseStatement();
            if (!stmt)
                return nullptr;

            classBody.push_back(std::move(stmt));
        }
    }

    // 检查dedent
    if (!expectToken(TOK_DEDENT, "Expected dedent after class body"))
        return nullptr;

    return makeStmt<ClassStmtAST>(className, baseClasses,
                                  std::move(classBody),
                                  std::move(methods));
}

std::vector<ParamAST> PyParser::parseParameters()
{
    std::vector<ParamAST> params;

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
            logParseError<ParamAST>("Expected parameter name");
            return params;
        }

        std::string paramName = currentToken.value;
        nextToken();  // 消费参数名

        // 检查是否有类型注解
        std::string typeName = "";
        if (currentToken.type == TOK_COLON)
        {
            nextToken();  // 消费':'

            if (currentToken.type != TOK_IDENTIFIER)
            {
                logParseError<ParamAST>("Expected type name after ':'");
                return params;
            }

            typeName = currentToken.value;
            nextToken();  // 消费类型名
        }

        params.emplace_back(paramName, typeName);

        if (currentToken.type == TOK_RPAREN)
            break;

        if (!expectToken(TOK_COMMA, "Expected ')' or ',' in parameter list"))
            return params;
    }

    nextToken();  // 消费')'
    return params;
}

std::string PyParser::parseReturnTypeAnnotation()
{
    std::string returnType = "";

    if (currentToken.type == TOK_ARROW)
    {
        nextToken();  // 消费'->'

        if (currentToken.type != TOK_IDENTIFIER)
        {
            logParseError<std::string>("Expected return type after '->'");
            return returnType;
        }

        returnType = currentToken.value;
        nextToken();  // 消费返回类型
    }

    return returnType;
}

std::unique_ptr<FunctionAST> PyParser::parseFunction()
{
    nextToken();  // 消费'def'

    // 函数名必须是标识符
    if (currentToken.type != TOK_IDENTIFIER)
    {
        return logParseError<FunctionAST>("Expected function name after 'def'");
    }

    std::string funcName = currentToken.value;
    nextToken();  // 消费函数名

    // 解析参数列表
    auto params = parseParameters();

    // 解析可能的返回类型注解
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

    // 创建函数AST
    auto result = std::make_unique<FunctionAST>(funcName, std::move(params),
                                                returnType, std::move(body));
    result->setLocation(currentToken.line, currentToken.column);

    // 设置返回类型自动推断标志
    if (returnType == "auto" || returnType.empty())
    {
        result->resolveParamTypes();
    }

    return result;
}

// 在parseBlock函数中添加作用域管理
std::vector<std::unique_ptr<StmtAST>> PyParser::parseBlock()
{
#ifdef DEBUG_PARSER_Block
    std::cerr << "Debug [parseBlock]: Entering parseBlock. Expecting INDENT. Current token: "
              << lexer.getTokenName(currentToken.type) << " ('" << currentToken.value << "') at L"
              << currentToken.line << " C" << currentToken.column << std::endl;
#endif
    if (!expectToken(TOK_INDENT, "Expected indented block"))
    {
        // 如果没有 INDENT，返回空列表，让调用者处理错误（例如 if/else/while 后面必须有块）
        return {};
    }

    std::vector<std::unique_ptr<StmtAST>> statements;
    // 循环解析语句，直到遇到 DEDENT 或 EOF
    while (currentToken.type != TOK_DEDENT && currentToken.type != TOK_EOF)
    {
        // 跳过空行（纯粹的 NEWLINE token）
        while (currentToken.type == TOK_NEWLINE)
        {
#ifdef DEBUG_PARSER_Block
            std::cerr << "Debug [parseBlock]: Skipping NEWLINE." << std::endl;
#endif
            nextToken();
        }

        // 如果跳过换行后遇到 DEDENT 或 EOF，则块结束
        if (currentToken.type == TOK_DEDENT || currentToken.type == TOK_EOF)
        {
            break;
        }

#ifdef DEBUG_PARSER_Block
        std::cerr << "Debug [parseBlock]: Parsing statement inside block. Current token: "
                  << lexer.getTokenName(currentToken.type) << " ('" << currentToken.value << "') at L"
                  << currentToken.line << " C" << currentToken.column << std::endl;
#endif
        auto stmt = parseStatement();
        if (!stmt)
        {
            // 如果语句解析失败，停止解析块并向上层报告错误（通过返回部分解析的列表）
            // 或者可以考虑更健壮的错误恢复，但现在先停止
#ifdef DEBUG_PARSER_Block
            std::cerr << "Debug [parseBlock]: parseStatement returned nullptr. Aborting block parsing." << std::endl;
#endif
            // 可能需要消耗掉出错行的剩余 token 直到 NEWLINE 来尝试恢复？
            // for now, just return what we have, error should have been thrown/logged
            return statements;  // 或者直接返回 {} 表示失败？取决于错误处理策略
        }
        statements.push_back(std::move(stmt));

        // parseStatement 应该负责消费语句后的 NEWLINE (如果适用)
        // 这里不需要额外处理 NEWLINE，除非 parseStatement 不能保证这一点
    }

    // 期望 DEDENT 来结束块
    if (!expectToken(TOK_DEDENT, "Expected dedent (unindent) to end block"))
    {
        // 如果没有 DEDENT，这是一个严重的缩进错误
        // 错误已由 expectToken 记录，但我们可能仍需返回已解析的语句
        // 或者根据策略清空列表表示块解析失败
#ifdef DEBUG_PARSER_Block
        std::cerr << "Debug [parseBlock]: Failed to find DEDENT. Returning potentially incomplete block." << std::endl;
#endif
    }
#ifdef DEBUG_PARSER_Block
    else
    {
        std::cerr << "Debug [parseBlock]: Exiting parseBlock successfully. Found DEDENT. Parsed " << statements.size() << " statements." << std::endl;
    }
#endif

    return statements;
}

std::unique_ptr<ModuleAST> PyParser::parseModule()
{
    std::string moduleName = "main";  // 默认模块名
    std::vector<std::unique_ptr<StmtAST>> topLevelStmts;
    std::vector<std::unique_ptr<FunctionAST>> functions;

    // 解析文件中的所有语句
    while (currentToken.type != TOK_EOF)
    {
        // 跳过多余的空行
        if (currentToken.type == TOK_NEWLINE)
        {
            nextToken();
            continue;
        }

        // 解析函数定义
        if (currentToken.type == TOK_DEF)
        {
            auto func = parseFunction();
            if (func)
                functions.push_back(std::move(func));
        }
        // 解析类定义（类也是顶级语句）
        else if (currentToken.type == TOK_CLASS)
        {
            auto classStmt = parseClassDefinition();
            if (classStmt)
                topLevelStmts.push_back(std::move(classStmt));
        }
        // 解析其他顶级语句
        else
        {
            auto stmt = parseStatement();
            if (stmt)
                topLevelStmts.push_back(std::move(stmt));
        }
    }

    // 创建模块AST
    auto module = std::make_unique<ModuleAST>(moduleName,
                                              std::move(topLevelStmts),
                                              std::move(functions));
    return module;
}
std::shared_ptr<PyType> PyParser::tryParseTypeAnnotation()
{
    // 保存当前解析状态以便回溯
    auto state = saveState();

    // 尝试解析类型注解
    if (currentToken.type == TOK_COLON)
    {
        nextToken();  // 消费冒号

        auto typeAnnotation = parseTypeAnnotation();
        if (typeAnnotation)
        {
            return typeAnnotation;
        }
    }

    // 没有解析到类型注解，恢复状态
    restoreState(state);
    return nullptr;
}

std::shared_ptr<PyType> PyParser::parseTypeAnnotation()
{
    // 基本类型直接对应标识符
    if (currentToken.type == TOK_IDENTIFIER)
    {
        std::string typeName = currentToken.value;
        nextToken();  // 消费类型名

        // 检查是否有泛型参数，如list[int]
        if (currentToken.type == TOK_LBRACK)
        {
            nextToken();  // 消费'['

            // 解析列表元素类型
            if (typeName == "list")
            {
                auto elemType = parseTypeAnnotation();
                if (!elemType)
                {
                    return logParseError<PyType>("Expected type in list[]");
                }

                if (!expectToken(TOK_RBRACK, "Expected ']' after list element type"))
                    return nullptr;

                return PyType::getList(elemType);
            }
            // 解析字典类型：dict[KeyType, ValueType]
            else if (typeName == "dict")
            {
                auto keyType = parseTypeAnnotation();
                if (!keyType)
                {
                    return logParseError<PyType>("Expected key type in dict[]");
                }

                if (!expectToken(TOK_COMMA, "Expected ',' after dict key type"))
                    return nullptr;

                auto valueType = parseTypeAnnotation();
                if (!valueType)
                {
                    return logParseError<PyType>("Expected value type in dict[]");
                }

                if (!expectToken(TOK_RBRACK, "Expected ']' after dict value type"))
                    return nullptr;

                return PyType::getDict(keyType, valueType);
            }
            else
            {
                return logParseError<PyType>("Unsupported generic type: " + typeName);
            }
        }

        // 处理基本类型
        return PyType::fromString(typeName);
    }

    return logParseError<PyType>("Expected type name");
}

//===----------------------------------------------------------------------===//
// PyTypeParser 类实现
//===----------------------------------------------------------------------===//

std::shared_ptr<PyType> PyTypeParser::parseType(const std::string& typeStr)
{
    // 如果类型字符串为空，返回Any类型
    if (typeStr.empty())
    {
        return PyType::getAny();
    }

    // 检查是否是列表类型
    size_t listPos = typeStr.find("list[");
    if (listPos == 0 && typeStr.back() == ']')
    {
        return parseListType(typeStr);
    }

    // 检查是否是字典类型
    size_t dictPos = typeStr.find("dict[");
    if (dictPos == 0 && typeStr.back() == ']')
    {
        return parseDictType(typeStr);
    }

    // 处理基本类型
    return parsePrimitiveType(typeStr);
}

std::shared_ptr<PyType> PyTypeParser::parsePrimitiveType(const std::string& name)
{
    if (name == "int") return PyType::getInt();
    if (name == "float" || name == "double") return PyType::getDouble();
    if (name == "bool") return PyType::getBool();
    if (name == "str" || name == "string") return PyType::getString();
    if (name == "None" || name == "void") return PyType::getVoid();
    if (name == "any" || name == "Any") return PyType::getAny();

    // 未知类型当作动态类型处理
    return PyType::getAny();
}

std::shared_ptr<PyType> PyTypeParser::parseListType(const std::string& typeStr)
{
    // 从list[ElementType]中提取ElementType
    size_t start = typeStr.find('[') + 1;
    size_t end = typeStr.rfind(']');

    if (start >= end || start == std::string::npos || end == std::string::npos)
    {
        // 解析错误，返回列表<any>类型
        return PyType::getList(PyType::getAny());
    }

    std::string elementTypeStr = typeStr.substr(start, end - start);
    auto elementType = parseType(elementTypeStr);

    return PyType::getList(elementType);
}

std::shared_ptr<PyType> PyTypeParser::parseDictType(const std::string& typeStr)
{
    // 从dict[KeyType, ValueType]中提取KeyType和ValueType
    size_t start = typeStr.find('[') + 1;
    size_t end = typeStr.rfind(']');

    if (start >= end || start == std::string::npos || end == std::string::npos)
    {
        // 解析错误，返回dict<any, any>类型
        return PyType::getDict(PyType::getAny(), PyType::getAny());
    }

    std::string innerTypes = typeStr.substr(start, end - start);

    // 查找逗号分隔的两个类型
    size_t commaPos = innerTypes.find(',');
    if (commaPos == std::string::npos)
    {
        // 解析错误，返回dict<any, any>类型
        return PyType::getDict(PyType::getAny(), PyType::getAny());
    }

    std::string keyTypeStr = innerTypes.substr(0, commaPos);
    std::string valueTypeStr = innerTypes.substr(commaPos + 1);

    // 去除可能的空格
    keyTypeStr.erase(remove_if(keyTypeStr.begin(), keyTypeStr.end(), isspace), keyTypeStr.end());
    valueTypeStr.erase(remove_if(valueTypeStr.begin(), valueTypeStr.end(), isspace), valueTypeStr.end());

    auto keyType = parseType(keyTypeStr);
    auto valueType = parseType(valueTypeStr);

    return PyType::getDict(keyType, valueType);
}

template <typename T>
std::vector<T> PyParser::parseDelimitedList(PyTokenType start, PyTokenType end, PyTokenType separator,
                                            std::function<T()> parseElement)
{
    std::vector<T> elements;

    // 消费开始标记
    if (!expectToken(start, "Expected delimiter"))
        return elements;

    // 处理空列表
    if (currentToken.type == end)
    {
        nextToken();  // 消费结束标记
        return elements;
    }

    // 解析元素列表
    while (true)
    {
        // 解析一个元素
        auto element = parseElement();
        elements.push_back(std::move(element));

        // 如果遇到结束标记，退出循环
        if (currentToken.type == end)
            break;

        // 否则应该遇到分隔符
        if (!expectToken(separator, "Expected separator or end delimiter"))
            break;

        // 处理可能的尾随分隔符
        if (currentToken.type == end)
            break;
    }

    // 消费结束标记
    if (!expectToken(end, "Expected end delimiter"))
        return elements;

    return elements;
}

}  // namespace llvmpy
