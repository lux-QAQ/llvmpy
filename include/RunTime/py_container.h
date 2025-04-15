// 容器类型操作
#ifndef PY_CONTAINER_H
#define PY_CONTAINER_H

#include "runtime_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// 列表操作
int py_list_len(PyObject* obj);
PyObject* py_list_get_item(PyObject* obj, int index);
void py_list_set_item(PyObject* obj, int index, PyObject* item);
PyObject* py_list_append(PyObject* obj, PyObject* item);
PyObject* py_list_copy(PyObject* obj);
void py_list_decref_items(PyListObject* list);
PyObject* py_list_get_item_with_type(PyObject* list, int index, int* out_type_id);
int py_get_list_element_type_id(PyObject* list);

// 字典操作
int py_dict_len(PyObject* obj);
void py_dict_set_item(PyObject* obj, PyObject* key, PyObject* value);
PyObject* py_dict_get_item(PyObject* obj, PyObject* key);
PyObject* py_dict_keys(PyObject* obj);
bool py_dict_resize(PyDictObject* dict);
PyDictEntry* py_dict_find_entry(PyDictObject* dict, PyObject* key);
PyObject* py_dict_get_item_with_type(PyObject* dict, PyObject* key, int* out_type_id);

// 索引操作
void py_object_set_index(PyObject* obj, PyObject* index, PyObject* value);// 通用索引赋值
PyObject* py_object_index(PyObject* obj, PyObject* index);
PyObject* py_object_index_with_type(PyObject* obj, PyObject* index, int* out_type_id);
PyObject* py_string_get_char(PyObject* str, int index);
void py_set_index_result_type(PyObject* result, int typeId);
int py_get_container_type_info(PyObject* container);

// 哈希操作
unsigned int py_hash_object(PyObject* obj);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // PY_CONTAINER_H