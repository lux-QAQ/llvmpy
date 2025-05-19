#include "RunTime/py_log.h"
#define RUNTIME_g_current_min_log_level MSG_INFO

//#define DEBUG_RUNTIME_py_object_set_index
//#define DEBUG_RUNTIME_OPERATORS
//#define DEBUG_RUNTIME_CONTAINER
//#define DEBUG_RUNTIME_print
//#define DEBUG_RUNTIME_print_double
//#define DEBUG_RUNTIME_py_call_function
//#define DEBUG_RUNTIME_py_call_function_noargs
//#define DEBUG_CODEGEN_RUNTIME_createCallFunctionNoArgs

ENABLE_LOGS_FOR_FUNCTION("py_initialize_builtin_log");
ENABLE_LOGS_FOR_FUNCTION("py_incref");
// ENABLE_LOGS_FOR_FUNCTION("py_decref") // Example: logs for py_decref are currently disabled
ENABLE_LOGS_FOR_FUNCTION("py_iter");
ENABLE_LOGS_FOR_FUNCTION("py_next");
ENABLE_LOGS_FOR_FUNCTION("py_iterator_decref_specialized");