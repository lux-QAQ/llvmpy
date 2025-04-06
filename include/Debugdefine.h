#ifndef DEBUGDEFINE_H
#define DEBUGDEFINE_H

// 定义DEBUG宏来启用调试模式
// 定义RECOVER_SOURCE_FROM_TOKENS宏来启用从Token恢复源代码
#define RECOVER_SOURCE_FROM_TOKENS
#define DEBUG
#define DEBUG_LOG(msg) \
    std::cerr << "DEBUG: " << msg << std::endl;

// 定义DEBUG_PRINT宏来启用调试打印
#ifdef DEBUG
#define DEBUG_PRINT(x) std::cerr << "DEBUG: " << x << std::endl
#else



#endif // DEBUG
#endif // DEBUGDEFINE_H