#include "RunTime/runtime.h"        // 需要 py_type_name
#include <stdio.h>

PyObject* py_object_getattr(PyObject* obj, const char* attr_name) {
    if (!obj || !attr_name) {
        fprintf(stderr, "Error: getattr called with NULL object or attribute name\n");
        return NULL;
    }

    int typeId = py_get_safe_type_id(obj);
    const PyTypeMethods* methods = py_get_type_methods(typeId); // 会进行基类回退

    if (methods && methods->getattr) {
        return methods->getattr(obj, attr_name); // 调用注册的处理器
    } else {
        // 如果没有特定的 getattr 处理器 (例如对于 int, double 等)
        fprintf(stderr, "TypeError: '%s' object has no attributes (or getattr not implemented)\n", py_type_name(typeId));
        return NULL;
    }
}

int py_object_setattr(PyObject* obj, const char* attr_name, PyObject* value) {
     if (!obj || !attr_name) {
        fprintf(stderr, "Error: setattr called with NULL object or attribute name\n");
        return -1;
    }

    int typeId = py_get_safe_type_id(obj);
    const PyTypeMethods* methods = py_get_type_methods(typeId); // 会进行基类回退

    if (methods && methods->setattr) {
        return methods->setattr(obj, attr_name, value); // 调用注册的处理器
    } else {
        // 如果没有特定的 setattr 处理器
        fprintf(stderr, "TypeError: '%s' object has no attributes (or setattr not implemented)\n", py_type_name(typeId));
        return -1;
    }
}