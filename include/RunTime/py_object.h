// 对象创建与基础操作
#ifndef PY_OBJECT_H
#define PY_OBJECT_H

#include "runtime_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// 对象创建函数
PyObject* py_create_int(int value);
PyObject* py_create_double(double value);
PyObject* py_create_bool(bool value);
PyObject* py_create_string(const char* value);
PyObject* py_create_list(int size, int elemTypeId);
PyObject* py_create_dict(int initialCapacity, int keyTypeId);
PyObject* py_get_none(void);

// 引用计数管理
void py_incref(PyObject* obj);
void py_decref(PyObject* obj);

// 对象复制
PyObject* py_object_copy(PyObject* obj, int typeId);

// 对象长度
int py_object_len(PyObject* obj);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // PY_OBJECT_H