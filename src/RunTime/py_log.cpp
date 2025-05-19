
#include "RunTime/runtime.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Global state for logging
static msg_type g_current_min_log_level = RUNTIME_g_current_min_log_level; // Default: only errors and higher
static bool g_py_log_initialized = false;

// Configuration for function-specific logging
#define MAX_LOG_ENABLED_FUNCTIONS 50 // Adjust as needed
#define MAX_FUNC_NAME_LEN 64         // Adjust as needed

static char g_log_enabled_function_names[MAX_LOG_ENABLED_FUNCTIONS][MAX_FUNC_NAME_LEN];
static int g_log_enabled_function_count = 0;

// --- Start of ulog_set_min_level, ulog_get_min_level, get_short_filename, ulog_core implementations ---
void ulog_set_min_level(msg_type level) {
    g_current_min_log_level = level;
}

msg_type ulog_get_min_level(void) {
    return g_current_min_log_level;
}

static const char* get_short_filename(const char* path) {
    if (path == NULL) return "unknown_file";
    const char* filename = strrchr(path, '/');
    if (filename) {
        return filename + 1;
    }
    const char* backslash_filename = strrchr(path, '\\');
    if (backslash_filename) {
        return backslash_filename + 1;
    }
    return path;
}

void ulog_core(const char* file, int line, const char* func_name, msg_type type, const char* format, ...) {
    if (type < g_current_min_log_level) {
        return;
    }

#define COLOR_NONE "\033[0m"
#define RED        "\033[1;31m"
#define BLUE       "\033[1;34m"
#define GREEN      "\033[1;32m"
#define YELLOW     "\033[1;33m"

    char* msg_content = NULL;
    va_list arg_list_raw;
    va_start(arg_list_raw, format);

    va_list arg_list_for_len;
    va_copy(arg_list_for_len, arg_list_raw);
    int len = vsnprintf(NULL, 0, format, arg_list_for_len);
    va_end(arg_list_for_len);

    if (len < 0) {
        fprintf(stderr, "[ULOG_CORE_ERROR @ %s:%d]: vsnprintf length calculation failed!\n", get_short_filename(__FILE__), __LINE__);
        va_end(arg_list_raw);
        return;
    }

    msg_content = (char*)malloc(len + 1);
    if (msg_content == NULL) {
        fprintf(stderr, "[ULOG_CORE_ERROR @ %s:%d]: malloc for %d bytes failed!\n", get_short_filename(__FILE__), __LINE__, len + 1);
        va_end(arg_list_raw);
        return;
    }
    
    vsnprintf(msg_content, len + 1, format, arg_list_raw);
    va_end(arg_list_raw);

    const char* short_file_name = get_short_filename(file);
    const char* display_func_name = (func_name == NULL) ? "" : func_name;

    switch(type) {
        case MSG_DEBUG:
            printf(BLUE "[DEBUG] [%s:%d:%s()]: %s\n" COLOR_NONE, short_file_name, line, display_func_name, msg_content);
            break;
        case MSG_INFO:
            printf(GREEN "[INFO] [%s:%d:%s()]: %s\n" COLOR_NONE, short_file_name, line, display_func_name, msg_content);
            break;
        case MSG_WARN:
            fprintf(stderr, YELLOW "[WARN] [%s:%d:%s()]: %s\n" COLOR_NONE, short_file_name, line, display_func_name, msg_content);
            break;
        case MSG_ERROR:
            fprintf(stderr, RED "[ERROR] [%s:%d:%s()]: %s\n" COLOR_NONE, short_file_name, line, display_func_name, msg_content);
            break;
        default:
            fprintf(stderr, "[ULOG_CORE_ERROR @ %s:%d]: Unknown log type %d for message: %s\n", get_short_filename(__FILE__), __LINE__, type, msg_content);
    }
    free(msg_content);
#undef COLOR_NONE
#undef RED
#undef BLUE
#undef GREEN
#undef YELLOW
}
// --- End of ulog_core and related functions ---

// Define ENABLE_LOGS_FOR_FUNCTION temporarily for processing Debugdefine.h
// This macro will now populate the C array.
#pragma push_macro("ENABLE_LOGS_FOR_FUNCTION")
#undef ENABLE_LOGS_FOR_FUNCTION // Undefine the version from py_log.h (which is a no-op)
#define ENABLE_LOGS_FOR_FUNCTION(func_name_str) \
    do { \
        if (g_log_enabled_function_count < MAX_LOG_ENABLED_FUNCTIONS) { \
            strncpy(g_log_enabled_function_names[g_log_enabled_function_count], func_name_str, MAX_FUNC_NAME_LEN - 1); \
            g_log_enabled_function_names[g_log_enabled_function_count][MAX_FUNC_NAME_LEN - 1] = '\0'; /* Ensure null termination */ \
            g_log_enabled_function_count++; \
        } else { \
            fprintf(stderr, "[PY_LOG_INIT_WARN]: Exceeded MAX_LOG_ENABLED_FUNCTIONS. Cannot enable logging for '%s'.\n", func_name_str); \
        } \
    } while (0)

void py_log_init(msg_type de_type) {
    if (g_py_log_initialized) {
        return;
    }
    // Initialize the array/count before including the config file
    g_log_enabled_function_count = 0; 
    memset(g_log_enabled_function_names, 0, sizeof(g_log_enabled_function_names)); // Optional: clear array

    // The macro ENABLE_LOGS_FOR_FUNCTION is defined above this function.
    // Including py_log_config.h here will execute those macro calls.
    #include "RunTime/runtime_debug_config.h" // Populates g_log_enabled_function_names

    g_py_log_initialized = true;
    ulog_set_min_level(de_type);
}

// Restore the original definition of ENABLE_LOGS_FOR_FUNCTION from py_log.h
#pragma pop_macro("ENABLE_LOGS_FOR_FUNCTION")

// Undefine the macro after its use in py_log_init
//#undef ENABLE_LOGS_FOR_FUNCTION


bool py_should_log(const char* func_name, msg_type type) {
    if (!g_py_log_initialized) {
        py_log_init(RUNTIME_g_current_min_log_level); // Ensure initialization
    }

    if (type == MSG_ERROR) {
        return true; // Always pass ERROR to ulog_core, which will check global level
    }

    if (type < g_current_min_log_level) {
        return false;
    }

    // Check if the function is specifically configured for logging
    for (int i = 0; i < g_log_enabled_function_count; ++i) {
        if (strncmp(g_log_enabled_function_names[i], func_name, MAX_FUNC_NAME_LEN) == 0) {
            return true; // Function found in the enabled list
        }
    }

    // Default policy for functions not listed in Debugdefine.h
    #ifdef DEBUG_LOG_FOR_UNLISTED_FUNCTIONS
        return true;
    #else
        return false; // Default: unlisted functions do not log DEBUG/INFO/WARN
    #endif
}