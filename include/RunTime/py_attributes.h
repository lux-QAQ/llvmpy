#ifndef PY_ATTRIBUTES_H
#define PY_ATTRIBUTES_H

#include "runtime_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取对象的属性。
 *
 * 使用类型分发查找并调用合适的 getattr 处理器。
 *
 * @param obj 目标对象。
 * @param attr_name 属性名称 (C 字符串)。
 * @return 属性值对象 (带新引用)，如果属性不存在或出错则返回 NULL。
 */
PyObject* py_object_getattr(PyObject* obj, const char* attr_name);

/**
 * @brief 设置对象的属性。
 *
 * 使用类型分发查找并调用合适的 setattr 处理器。
 *
 * @param obj 目标对象。
 * @param attr_name 属性名称 (C 字符串)。
 * @param value 要设置的属性值对象。
 * @return 0 表示成功，-1 表示失败。
 */
int py_object_setattr(PyObject* obj, const char* attr_name, PyObject* value);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PY_ATTRIBUTES_H