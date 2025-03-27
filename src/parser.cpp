#include "parser.h"
#include <iostream>
#include <Debugdefine.h>

namespace llvmpy
{

Parser::Parser(Lexer& l) : lexer(l), currentToken(TOK_EOF, "", 1, 1)
{
    // 设置二元操作符优先级
    binopPrecedence['+'] = 20;
    binopPrecedence['-'] = 20;
    binopPrecedence['*'] = 40;
    binopPrecedence['/'] = 40;
    binopPrecedence['<'] = 10;
    binopPrecedence['>'] = 10;

    // 获取第一个token
    nextToken();
}

void Parser::nextToken()
{
    currentToken = lexer.getNextToken();
}

int Parser::getTokenPrecedence()
{
    if (currentToken.type == TOK_PLUS) return binopPrecedence['+'];
    if (currentToken.type == TOK_MINUS) return binopPrecedence['-'];
    if (currentToken.type == TOK_MUL) return binopPrecedence['*'];
    if (currentToken.type == TOK_DIV) return binopPrecedence['/'];
    if (currentToken.type == TOK_LT) return binopPrecedence['<'];
    if (currentToken.type == TOK_GT) return binopPrecedence['>'];
    if (currentToken.type == TOK_LE) return binopPrecedence['<'];
    if (currentToken.type == TOK_GE) return binopPrecedence['>'];
    if (currentToken.type == TOK_EQ) return binopPrecedence['='];
    if (currentToken.type == TOK_NEQ) return binopPrecedence['!'];
    return -1;  // 不是二元操作符
}

std::unique_ptr<ExprAST> Parser::logError(const std::string& message)
{
    std::cerr << "Error: " << message << " at line " << currentToken.line
              << ", col " << currentToken.column << std::endl;
    return nullptr;
}

std::unique_ptr<StmtAST> Parser::logErrorStmt(const std::string& message)
{
    logError(message);
    return nullptr;
}

std::unique_ptr<FunctionAST> Parser::logErrorFunc(const std::string& message)
{
    logError(message);
    return nullptr;
}

std::unique_ptr<ModuleAST> Parser::logErrorModule(const std::string& message)
{
    logError(message);
    return nullptr;
}

std::unique_ptr<ExprAST> Parser::parseNumber()
{
    auto result = std::make_unique<NumberExprAST>(std::stod(currentToken.value));
    nextToken();  // consume the number
    return std::move(result);
}

std::unique_ptr<ExprAST> Parser::parseParenExpr()
{
    nextToken();  // consume '('
    auto expr = parseExpression();
    if (!expr)
        return nullptr;

    if (currentToken.type != TOK_RPAREN)
        return logError("expected ')'");

    nextToken();  // consume ')'
    return expr;
}

std::unique_ptr<ExprAST> Parser::parseIdentifier()
{
    std::string idName = currentToken.value;
    nextToken();  // consume identifier

    // 简单变量引用
    if (currentToken.type != TOK_LPAREN)
        return std::make_unique<VariableExprAST>(idName);

    // 函数调用
    nextToken();  // consume '('
    std::vector<std::unique_ptr<ExprAST>> args;

    if (currentToken.type != TOK_RPAREN)
    {
        while (true)
        {
            auto arg = parseExpression();
            if (!arg)
                return nullptr;
            args.push_back(std::move(arg));

            if (currentToken.type == TOK_RPAREN)
                break;

            if (currentToken.type != TOK_COMMA)
                return logError("expected ')' or ',' in argument list");

            nextToken();  // consume ','
        }
    }

    nextToken();  // consume ')'
    return std::make_unique<CallExprAST>(idName, std::move(args));
}

std::unique_ptr<ExprAST> Parser::parsePrimary()
{
    switch (currentToken.type)
    {
        case TOK_IDENTIFIER:
            return parseIdentifier();
        case TOK_NUMBER:
            return parseNumber();
        case TOK_LPAREN:
            return parseParenExpr();
        case TOK_MINUS:  // 处理负号
            return parseUnaryExpr();
        default:
            return logError("unknown token when expecting an expression");
    }
}
std::unique_ptr<ExprAST> Parser::parseUnaryExpr()
{
    // 记住我们是负号
    nextToken();  // 消费'-'

    // 解析操作数
    auto operand = parsePrimary();
    if (!operand)
        return nullptr;

    // 创建一个表达式：0 - operand，这样我们可以复用现有的二元操作符机制
    auto zero = std::make_unique<NumberExprAST>(0.0);
    return std::make_unique<BinaryExprAST>('-', std::move(zero), std::move(operand));
}
std::unique_ptr<ExprAST> Parser::parseBinOpRHS(int exprPrec, std::unique_ptr<ExprAST> LHS)
{
    while (true)
    {
        int tokPrec = getTokenPrecedence();

        // 如果这个二元操作符的优先级小于当前操作符的优先级，则返回LHS
        if (tokPrec < exprPrec)
            return LHS;

        char binOp;
        switch (currentToken.type)
        {
            case TOK_PLUS:
                binOp = '+';
                break;
            case TOK_MINUS:
                binOp = '-';
                break;
            case TOK_MUL:
                binOp = '*';
                break;
            case TOK_DIV:
                binOp = '/';
                break;
            case TOK_LT:
                binOp = '<';
                break;
            case TOK_GT:
                binOp = '>';
                break;
            case TOK_LE:
                binOp = 'l';
                break;  // 用'l'表示<=
            case TOK_GE:
                binOp = 'g';
                break;  // 用'g'表示>=
            case TOK_EQ:
                binOp = 'e';
                break;  // 用'e'表示==
            case TOK_NEQ:
                binOp = 'n';
                break;  // 用'n'表示!=
            default:
                return logError("invalid binary operator");
        }
        nextToken();  // consume operator

        // 解析操作符右边的表达式
        auto RHS = parsePrimary();
        if (!RHS)
            return nullptr;

        // 如果右边的操作符优先级更高，先计算右边
        int nextPrec = getTokenPrecedence();
        if (tokPrec < nextPrec)
        {
            RHS = parseBinOpRHS(tokPrec + 1, std::move(RHS));
            if (!RHS)
                return nullptr;
        }

        // 合并LHS和RHS
        LHS = std::make_unique<BinaryExprAST>(binOp, std::move(LHS), std::move(RHS));
    }
}

std::unique_ptr<ExprAST> Parser::parseExpression()
{
    auto LHS = parsePrimary();
    if (!LHS)
        return nullptr;

    return parseBinOpRHS(0, std::move(LHS));
}

std::unique_ptr<StmtAST> Parser::parseExpressionStmt()
{
    auto expr = parseExpression();
    if (!expr)
        return nullptr;

    return std::make_unique<ExprStmtAST>(std::move(expr));
}

std::unique_ptr<StmtAST> Parser::parseReturnStmt()
{
    nextToken();  // consume 'return'

    auto value = parseExpression();
    if (!value)
        return nullptr;

    // 确保语句后有NEWLINE，并消费它
    if (currentToken.type == TOK_NEWLINE)
    {
        nextToken();  // consume newline
    }

    return std::make_unique<ReturnStmtAST>(std::move(value));
}
std::unique_ptr<StmtAST> Parser::parseElifStmt()
{
    nextToken();  // consume 'elif'

    auto condition = parseExpression();
    if (!condition)
        return nullptr;

    if (currentToken.type != TOK_COLON)
        return logErrorStmt("expected ':' after elif condition");

    nextToken();  // consume ':'

    // 允许在冒号后有空格或注释，然后是换行符
    while (currentToken.type != TOK_NEWLINE && currentToken.type != TOK_EOF)
    {
        nextToken();
    }

    if (currentToken.type != TOK_NEWLINE)
        return logErrorStmt("expected newline after ':'");

    nextToken();  // consume newline

    if (currentToken.type != TOK_INDENT)
        return logErrorStmt("expected indented block after 'elif'");

    nextToken();  // consume indent

    std::vector<std::unique_ptr<StmtAST>> thenBody;
    while (currentToken.type != TOK_DEDENT && currentToken.type != TOK_EOF)
    {
        if (currentToken.type == TOK_NEWLINE)
        {
            nextToken();
            continue;
        }

        auto stmt = parseStatement();
        if (!stmt)
            return nullptr;
        thenBody.push_back(std::move(stmt));
    }

    if (currentToken.type != TOK_DEDENT)
        return logErrorStmt("expected dedent after elif block");

    nextToken();  // consume dedent

    // 创建一个空的else体，如果之后有elif或else块，它们将被parseIfStmt函数处理
    std::vector<std::unique_ptr<StmtAST>> elseBody;

    // 将elif语句转换为一个if语句返回
    return std::make_unique<IfStmtAST>(std::move(condition), std::move(thenBody), std::move(elseBody));
}
std::unique_ptr<StmtAST> Parser::parseIfStmt()
{
    nextToken();  // consume 'if'

    auto condition = parseExpression();
    if (!condition)
        return nullptr;

    if (currentToken.type != TOK_COLON)
        return logErrorStmt("expected ':' after if condition");

    nextToken();  // consume ':'

    // 允许在冒号后有空格或注释，然后是换行符
    while (currentToken.type != TOK_NEWLINE && currentToken.type != TOK_EOF)
    {
        nextToken();
    }

    if (currentToken.type != TOK_NEWLINE)
        return logErrorStmt("expected newline after ':'");

    nextToken();  // consume newline

    if (currentToken.type != TOK_INDENT)
        return logErrorStmt("expected indented block after 'if'");

    nextToken();  // consume indent

    std::vector<std::unique_ptr<StmtAST>> thenBody;
    while (currentToken.type != TOK_DEDENT && currentToken.type != TOK_EOF)
    {
        if (currentToken.type == TOK_NEWLINE)
        {
            nextToken();
            continue;
        }

        auto stmt = parseStatement();
        if (!stmt)
            return nullptr;
        thenBody.push_back(std::move(stmt));
    }

    if (currentToken.type != TOK_DEDENT)
        return logErrorStmt("expected dedent after if block");

    nextToken();  // consume dedent

    // 检查是否有elif部分
    std::vector<std::unique_ptr<StmtAST>> elseBody;

    // 处理所有elif部分
    while (currentToken.type == TOK_ELIF)
    {
        auto elifStmt = parseElifStmt();
        if (!elifStmt)
            return nullptr;

        // 将elif语句添加到else体中
        elseBody.push_back(std::move(elifStmt));

        // 由于parseElifStmt已经处理完整个elif结构，这里不需要额外的操作
    }

    // 检查是否有else部分
    if (currentToken.type == TOK_ELSE)
    {
        nextToken();  // consume 'else'

        if (currentToken.type != TOK_COLON)
            return logErrorStmt("expected ':' after 'else'");

        nextToken();  // consume ':'

        if (currentToken.type != TOK_NEWLINE)
            return logErrorStmt("expected newline after 'else:'");

        nextToken();  // consume newline

        if (currentToken.type != TOK_INDENT)
            return logErrorStmt("expected indented block after 'else:'");

        nextToken();  // consume indent

        while (currentToken.type != TOK_DEDENT && currentToken.type != TOK_EOF)
        {
            if (currentToken.type == TOK_NEWLINE)
            {
                nextToken();
                continue;
            }

            auto stmt = parseStatement();
            if (!stmt)
                return nullptr;
            elseBody.push_back(std::move(stmt));
        }

        if (currentToken.type != TOK_DEDENT)
            return logErrorStmt("expected dedent after else block");

        nextToken();  // consume dedent
    }

    return std::make_unique<IfStmtAST>(std::move(condition), std::move(thenBody), std::move(elseBody));
}

std::unique_ptr<StmtAST> Parser::parsePrintStmt()
{
    nextToken();  // consume 'print'

    if (currentToken.type != TOK_LPAREN)
        return logErrorStmt("expected '(' after 'print'");

    nextToken();  // consume '('

    auto expr = parseExpression();
    if (!expr)
        return nullptr;

    if (currentToken.type != TOK_RPAREN)
        return logErrorStmt("expected ')' after print expression");

    nextToken();  // consume ')'

    return std::make_unique<PrintStmtAST>(std::move(expr));
}

std::unique_ptr<StmtAST> Parser::parseAssignStmt()
{
    std::string name = currentToken.value;
    nextToken();  // consume identifier

    if (currentToken.type != TOK_ASSIGN)
        return logErrorStmt("expected '=' in assignment");

    nextToken();  // consume '='

    auto value = parseExpression();
    if (!value)
        return nullptr;

    return std::make_unique<AssignStmtAST>(name, std::move(value));
}

std::vector<std::unique_ptr<StmtAST>> Parser::parseBlock()
{
    std::vector<std::unique_ptr<StmtAST>> statements;

    if (currentToken.type != TOK_INDENT)
        return statements;  // 空块

    nextToken();  // consume indent

    while (currentToken.type != TOK_DEDENT && currentToken.type != TOK_EOF)
    {
        // 跳过块内的空行
        if (currentToken.type == TOK_NEWLINE)
        {
            nextToken();
            continue;
        }

        auto stmt = parseStatement();
        if (!stmt)
            return statements;  // 出错了，但至少返回已解析的语句
        statements.push_back(std::move(stmt));
    }

    // 关键修改：处理所有连续的DEDENT标记
    while (currentToken.type == TOK_DEDENT)
    {
        nextToken();  // consume dedent
    }

    return statements;
}

std::unique_ptr<StmtAST> Parser::parseStatement()
{
    // 跳过语句开始前的空行
    while (currentToken.type == TOK_NEWLINE)
    {
        nextToken();
    }

    switch (currentToken.type)
    {
        case TOK_RETURN:
            return parseReturnStmt();
        case TOK_IF:
            return parseIfStmt();
        case TOK_WHILE:
            return parseWhileStmt();
        case TOK_PRINT:
            return parsePrintStmt();
        case TOK_IDENTIFIER:
        {
            // 需要向前看一个token来确定是变量引用还是赋值
            std::string name = currentToken.value;
            Token nextTok = lexer.peekToken();
            if (nextTok.type == TOK_ASSIGN)
            {
                return parseAssignStmt();
            }
            else
            {
                return parseExpressionStmt();
            }
        }
        case TOK_NUMBER:
        case TOK_LPAREN:
        case TOK_MINUS:  // 添加对负号的处理 TODO 疑似错误
            return parseExpressionStmt();
        default:
            return logErrorStmt("unknown statement type: " + lexer.getTokenName(currentToken.type));
    }
}

std::unique_ptr<FunctionAST> Parser::parseFunction()
{
#ifdef DEBUG
    // 添加调试输出
    std::cerr << "Debug: Parsing function, current token: '"
              << currentToken.value
              << "' type: " << currentToken.type
              << " at line " << currentToken.line
              << ", col " << currentToken.column << std::endl;
#endif
    nextToken();  // consume 'def'
#ifdef DEBUG
    // 再添加一个调试点，检查吃掉'def'后的状态
    std::cerr << "Debug: After 'def', current token: '"
              << currentToken.value
              << "' type: " << currentToken.type
              << " at line " << currentToken.line
              << ", col " << currentToken.column << std::endl;
#endif
    if (currentToken.type != TOK_IDENTIFIER)
        return logErrorFunc("expected function name after 'def'");

    std::string funcName = currentToken.value;
    nextToken();  // consume function name

    if (currentToken.type != TOK_LPAREN)
        return logErrorFunc("expected '(' after function name");

    nextToken();  // consume '('

    // 解析参数列表
    std::vector<ParamAST> params;
    if (currentToken.type != TOK_RPAREN)
    {
        while (true)
        {
            if (currentToken.type != TOK_IDENTIFIER)
                return logErrorFunc("expected parameter name");

            ParamAST param;
            param.name = currentToken.value;
            param.type = "";  // Python默认不指定类型

            nextToken();  // consume parameter name

            // 检查是否有参数类型注解
            if (currentToken.type == TOK_COLON)
            {
                nextToken();  // consume ':'

                if (currentToken.type != TOK_IDENTIFIER)
                    return logErrorFunc("expected parameter type after ':'");

                param.type = currentToken.value;
                nextToken();  // consume type name
            }

            params.push_back(param);

            if (currentToken.type == TOK_RPAREN)
                break;

            if (currentToken.type != TOK_COMMA)
                return logErrorFunc("expected ')' or ',' in parameter list");

            nextToken();  // consume ','
        }
    }

    nextToken();  // consume ')'

    // 检查返回类型注解
    std::string returnType = "";
    if (currentToken.type == TOK_MINUS && lexer.peekToken().type == TOK_GT)
    {
        nextToken();  // consume '-'
        nextToken();  // consume '>'

        if (currentToken.type != TOK_IDENTIFIER)
            return logErrorFunc("expected return type after '->'");

        returnType = currentToken.value;
        nextToken();  // consume return type
    }
#ifdef DEBUG
    // 在检查冒号前添加调试
    std::cerr << "Debug: Before checking colon, current token: '"
              << currentToken.value
              << "' type: " << currentToken.type
              << " at line " << currentToken.line
              << ", col " << currentToken.column << std::endl;
#endif
    if (currentToken.type != TOK_COLON)
        return logErrorFunc("expected ':' after function declaration");

    nextToken();  // consume ':'
#ifdef DEBUG
    // 在检查换行符前添加调试
    std::cerr << "Debug: After colon, before newline, current token: '"
              << currentToken.value
              << "' type: " << currentToken.type
              << " at line " << currentToken.line
              << ", col " << currentToken.column << std::endl;
#endif

    // 修复：检测词法分析器是否已将NEWLINE和INDENT合并处理
    if (currentToken.type == TOK_INDENT)
    {
        // 词法分析器已经处理了换行符并检测到了缩进，直接解析函数体
        std::vector<std::unique_ptr<StmtAST>> body = parseBlock();
        return std::make_unique<FunctionAST>(funcName, std::move(params), std::move(body), returnType);
    }

    // 跳过换行符后的任何空白和注释
    while (currentToken.type != TOK_NEWLINE && currentToken.type != TOK_INDENT && currentToken.type != TOK_EOF)
    {
        nextToken();
    }

    // 现在检查换行符或直接是缩进标记
    if (currentToken.type != TOK_NEWLINE && currentToken.type != TOK_INDENT)
        return logErrorFunc("expected newline after function declaration");

    // 如果是换行符，消费它并继续
    if (currentToken.type == TOK_NEWLINE)
        nextToken();  // consume newline

    // 解析函数体
    std::vector<std::unique_ptr<StmtAST>> body;
    if (currentToken.type != TOK_INDENT)
        return logErrorFunc("expected indented block for function body");

    body = parseBlock();

    return std::make_unique<FunctionAST>(funcName, std::move(params), std::move(body), returnType);
}

std::unique_ptr<ModuleAST> Parser::parseModule()
{
    std::vector<std::unique_ptr<FunctionAST>> functions;
    std::vector<std::unique_ptr<StmtAST>> topLevelStmts;

    while (currentToken.type != TOK_EOF)
    {
        // 处理连续的DEDENT标记
        while (currentToken.type == TOK_DEDENT)
        {
            nextToken();  // 消费DEDENT
        }

        // 处理空行
        if (currentToken.type == TOK_NEWLINE)
        {
            nextToken();  // 跳过空行
            continue;     // 继续循环以处理可能的后续DEDENT
        }
        // 处理函数定义
        else if (currentToken.type == TOK_DEF)
        {
            auto func = parseFunction();
            if (!func)
                return nullptr;
            functions.push_back(std::move(func));
        }
        // 处理其他顶层语句
        else if (currentToken.type != TOK_EOF)
        {
            auto stmt = parseStatement();
            if (!stmt)
                return nullptr;
            topLevelStmts.push_back(std::move(stmt));
        }
    }

    return std::make_unique<ModuleAST>(std::move(functions), std::move(topLevelStmts));
}

std::unique_ptr<StmtAST> Parser::parseWhileStmt()
{
    nextToken();  // consume 'while'

    auto condition = parseExpression();
    if (!condition)
        return nullptr;

    if (currentToken.type != TOK_COLON)
        return logErrorStmt("expected ':' after while condition");

    nextToken();  // consume ':'

    // 允许在冒号后有空格或注释，然后是换行符
    while (currentToken.type != TOK_NEWLINE && currentToken.type != TOK_EOF)
    {
        nextToken();
    }

    if (currentToken.type != TOK_NEWLINE)
        return logErrorStmt("expected newline after ':'");

    nextToken();  // consume newline

    if (currentToken.type != TOK_INDENT)
        return logErrorStmt("expected indented block after 'while'");

    // 解析循环体
    std::vector<std::unique_ptr<StmtAST>> body;
    body = parseBlock();

    return std::make_unique<WhileStmtAST>(std::move(condition), std::move(body));
}

}  // namespace llvmpy