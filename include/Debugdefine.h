#ifndef DEBUGDEFINE_H
#define DEBUGDEFINE_H

// 定义DEBUG宏来启用调试模式

#define DEBUG


#ifdef DEBUG

//main
//#define DEBUG_MAIN_TOKEN_DUMP

//lexer
//#define DEBUG_LEXER

//#define DEBUG_LEXER_INDENT



// 定义RECOVER_SOURCE_FROM_TOKENS宏来启用从Token恢复源代码
//#define RECOVER_SOURCE_FROM_TOKENS

#define DEBUG_PARSER_Block
#define DEBUG_PARSER_Expr
#define DEBUG_PARSER_Stmt
#define DEBUG_PARSER_NextToken_detailed
#define DEBUG_PARSER_NextToken



#define DEBUG_WhileSTmt

#define DEBUG_CODEGEN_handleCallExpr

#define DEBUG_IfStmt

#define DEBUG_LOG(msg) \
    std::cerr << "DEBUG: " << msg << std::endl;



#define DEBUG_CODEGEN_TYPE
#define DEBUG_CODEGEN_generateModule
#define DEBUG_CODEGEN_handleFunctionDef
#define DEBUG_CODEGEN_handleFunctionDefStmt
#define DEBUG_CODEGEN_STMT
#define DEBUG_WhileSTmt_terminated
#define DEBUG_CODEGEN_verifyFunction
#define DEBUG_CODEGEN_handleVariableExpr
#define DEBUG_CODEGEN_getCachedFunction

#define DEBUG_CODEGEN_createListWithValues
#define DEBUG_CODEGEN_getCommonType
#define DEBUG_CODEGEN_inferListElementType

#define DEBUG_SYMBOL_TABLE

#define DEBUG_inferExprType_expr



// RUNTIME


#define RUNTIME_TIMER

// 定义DEBUG_PRINT宏来启用调试打印

#define DEBUG_PRINT(x) std::cerr << "DEBUG: " << x << std::endl
#else

#endif  // DEBUG
#endif  // DEBUGDEFINE_H