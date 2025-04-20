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


PyObject* py_create_class(const char* name, PyObject* base_cls_obj, PyObject* class_dict_obj);
PyObject* py_create_instance(PyObject* cls_obj); // 参数是类对象


// 引用计数管理
void py_incref(PyObject* obj);
void py_decref(PyObject* obj);

// 对象复制
PyObject* py_object_copy(PyObject* obj, int typeId);

// 对象长度
int py_object_len(PyObject* obj);

// 添加函数创建声明
PyObject* py_create_function(void* func_ptr, int signature_type_id);

PyObject* py_call_function_noargs(PyObject* func_obj);
PyObject* py_call_function(PyObject* callable, int num_args, PyObject** args_array);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif // PY_OBJECT_H