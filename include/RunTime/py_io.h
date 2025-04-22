// 输入输出操作
#ifndef PY_IO_H
#define PY_IO_H

#include "runtime_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// 打印函数

void py_print_object(PyObject* obj);
void py_print_int(int value);
void py_print_double(double value);
void py_print_bool(bool value);
void py_print_string(const char* value);

// 输入函数
char* input(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // PY_IO_H