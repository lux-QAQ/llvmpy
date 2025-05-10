#ifndef PY_LOG_H
#define PY_LOG_H

#include <stdio.h>
#include <stdbool.h> // For bool type in C, if not in C++ mode or implicitly available

// msg_type 定义了日志的级别
typedef enum {
    MSG_DEBUG = 0,
    MSG_INFO,
    MSG_WARN,
    MSG_ERROR
} msg_type;

#ifdef __cplusplus
extern "C" {
#endif

void ulog_set_min_level(msg_type level);
msg_type ulog_get_min_level(void);
void ulog_core(const char* file, int line, const char* func_name, msg_type type, const char* format, ...);

// Initialization function to be called once at startup
void py_log_init(void);

// Runtime check function
bool py_should_log(const char* func_name, msg_type type);

#ifdef __cplusplus
} // extern "C"
#endif

// This macro is used by Debugdefine.h. It's a declaration here.
// Its actual purpose is to be defined during py_log_init in py_log.cpp.
#define ENABLE_LOGS_FOR_FUNCTION(func_name_str)

// Logging macros
#define LOG_DEBUG(format, ...) \
    do { \
        if (py_should_log(__func__, MSG_DEBUG)) { \
            ulog_core(__FILE__, __LINE__, __func__, MSG_DEBUG, format, ##__VA_ARGS__); \
        } \
    } while (0)

#define LOG_INFO(format, ...) \
    do { \
        if (py_should_log(__func__, MSG_INFO)) { \
            ulog_core(__FILE__, __LINE__, __func__, MSG_INFO, format, ##__VA_ARGS__); \
        } \
    } while (0)

#define LOG_WARN(format, ...) \
    do { \
        if (py_should_log(__func__, MSG_WARN)) { \
            ulog_core(__FILE__, __LINE__, __func__, MSG_WARN, format, ##__VA_ARGS__); \
        } \
    } while (0)

#define LOG_ERROR(format, ...) \
    ulog_core(__FILE__, __LINE__, __func__, MSG_ERROR, format, ##__VA_ARGS__)

#endif // !PY_LOG_H