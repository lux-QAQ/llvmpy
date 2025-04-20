// 类型检查与转换
#ifndef PY_TYPE_H
#define PY_TYPE_H

#include "runtime_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// 类型检查
bool py_check_type(PyObject* obj, int expectedTypeId);
PyObject* py_ensure_type(PyObject* obj, int expectedTypeId);
bool py_are_types_compatible(int typeIdA, int typeIdB);
bool py_ensure_type_compatibility(PyObject* obj, int expectedTypeId);

// 类型信息
const char* py_type_name(int typeId);
int py_get_base_type_id(int typeId);
int py_get_object_type_id(PyObject* obj);
int py_get_type_id(PyObject* obj);
const char* py_type_id_to_string(int typeId);
int py_get_safe_type_id(PyObject* obj);

// 类型特性检查
bool py_is_container(PyObject* obj);
bool py_is_sequence(PyObject* obj);
bool py_object_to_bool(PyObject* obj);

// 类型提取
int py_extract_int(PyObject* obj);
double py_extract_double(PyObject* obj);
bool py_extract_bool(PyObject* obj);
const char* py_extract_string(PyObject* obj);
PyObject* py_extract_int_from_any(PyObject* obj);
int py_extract_constant_int(PyObject* obj);

// 类型转换
PyObject* py_convert_int_to_double(PyObject* obj);
PyObject* py_convert_double_to_int(PyObject* obj);
PyObject* py_convert_to_bool(PyObject* obj);
PyObject* py_convert_to_string(PyObject* obj);
PyObject* py_convert_any_to_int(PyObject* obj);
PyObject* py_convert_any_to_double(PyObject* obj);
PyObject* py_convert_any_to_bool(PyObject* obj);
PyObject* py_convert_any_to_string(PyObject* obj);
PyObject* py_convert_to_any(PyObject* obj);
PyObject* py_convert_any_preserve_type(PyObject* obj);
PyObject* py_preserve_parameter_type(PyObject* obj);
PyObject* py_smart_convert(PyObject* obj, int targetTypeId);

// 类型错误报告
void py_type_error(PyObject* obj, int expectedTypeId);



/**
* @brief 将 Python 对象转换为整数退出代码。
*
* 处理 None (-> 0)、PyInt (-> 其值) 以及其他可能的类型。
* 对于未处理的类型或错误，返回 1。
*
* @param obj 要转换的 Python 对象。
* @return 整数退出代码（通常 0 表示成功，非零表示错误）。
*/
 int py_object_to_exit_code(PyObject* obj);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // PY_TYPE_H