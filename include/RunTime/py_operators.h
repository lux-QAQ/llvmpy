// 运算符操作
#ifndef PY_OPERATORS_H
#define PY_OPERATORS_H

#include "runtime_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// 数值转换辅助
bool py_coerce_numeric(PyObject* a, PyObject* b, double* out_a, double* out_b);

// 算术运算
PyObject* py_object_power(PyObject* a, PyObject* b);
PyObject* py_object_add(PyObject* a, PyObject* b);
PyObject* py_object_subtract(PyObject* a, PyObject* b);
PyObject* py_object_multiply(PyObject* a, PyObject* b);
PyObject* py_object_divide(PyObject* a, PyObject* b);
PyObject* py_object_modulo(PyObject* a, PyObject* b);
PyObject* py_object_negate(PyObject* a);
PyObject* py_object_not(PyObject* a);

// 比较运算
PyObject* py_object_compare(PyObject* a, PyObject* b, PyCompareOp op);
bool py_compare_eq(PyObject* a, PyObject* b);
bool py_compare_ne(PyObject* a, PyObject* b);

// 辅助函数
const char* py_compare_op_name(PyCompareOp op);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // PY_OPERATORS_H