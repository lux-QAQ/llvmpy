#ifndef DEBUGDEFINE_H
#define DEBUGDEFINE_H

// 定义DEBUG宏来启用调试模式

#define DEBUG


#ifdef DEBUG



//lexer
#define DEBUG_LEXER

#define DEBUG_LEXER_INDENT


// 定义RECOVER_SOURCE_FROM_TOKENS宏来启用从Token恢复源代码
#define RECOVER_SOURCE_FROM_TOKENS

#define DEBUG_PARSER_Block
#define DEBUG_PARSER_Expr
#define DEBUG_PARSER_Stmt
//#define DEBUG_PARSER_NextToken_detailed
#define DEBUG_PARSER_NextToken
//#define DEBUG_WhileSTmt



#define DEBUG_IfStmt

#define DEBUG_LOG(msg) \
    std::cerr << "DEBUG: " << msg << std::endl;




#define DEBUG_CODEGEN_generateModule
#define DEBUG_CODEGEN_handleFunctionDef
#define DEBUG_CODEGEN_handleFunctionDefStmt


// RUNTIME
//#define DEBUG_RUNTIME_py_object_set_index
// 定义DEBUG_PRINT宏来启用调试打印

#define DEBUG_PRINT(x) std::cerr << "DEBUG: " << x << std::endl
#else

#endif  // DEBUG
#endif  // DEBUGDEFINE_H