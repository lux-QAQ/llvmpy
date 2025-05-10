#include "RunTime/runtime.h"


//===----------------------------------------------------------------------===//
// error 相关函数,待完善
//===----------------------------------------------------------------------===//
void py_runtime_error(const char* error_key, int line_number) {
    const char* error_type_name = "Error";
    char message_buffer[256];
    strncpy(message_buffer, error_key, sizeof(message_buffer) - 1);
    message_buffer[sizeof(message_buffer) - 1] = '\0';

    if (strcmp(error_key, "TypeError_NotIterable") == 0) {
        error_type_name = "TypeError";
        snprintf(message_buffer, sizeof(message_buffer), "object is not iterable");
    } else if (strcmp(error_key, "StopIteration") == 0) {
        // py_next 返回 NULL 表示 StopIteration，通常不通过此函数报告
        // 但如果需要显式引发 StopIteration 异常，可以在此处理
        error_type_name = "StopIteration";
        message_buffer[0] = '\0'; // StopIteration 通常没有消息体
    }
    // 可以添加更多错误键的映射

    if (line_number > 0) {
        fprintf(stderr, "Traceback (most recent call last):\n");
        // 实际应用中，文件名和模块名需要从调用栈或其他上下文中获取
        fprintf(stderr, "  File \"<unknown>\", line %d, in <module>\n", line_number);
    }
    fprintf(stderr, "%s: %s\n", error_type_name, message_buffer);
    
    // 注意：LLVM IR 在调用此函数后通常有 unreachable 指令。
    // 在一个完整的运行时中，这里可能会设置一个全局异常状态并通过 longjmp 返回到主事件循环，
    // 或者如果这是不可恢复的错误，则调用 exit()。
    // 对于当前链接错误，仅打印错误信息即可。
}