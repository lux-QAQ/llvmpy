// filepath: /home/ljs/code/llvmpy/src/RunTime/py_function.c
#include "RunTime/runtime_common.h"
#include <stdlib.h>            // for malloc
#include <stdio.h>             // for fprintf, stderr (错误处理)

PyObject* py_create_function(void* func_ptr, int signature_type_id) {
    if (!func_ptr) {
        fprintf(stderr, "Runtime Error: Attempted to create function object with null pointer.\n");
        // 在实际项目中，这里应该引发一个 Python 异常
        return NULL;
    }

    PyFunctionObject* func_obj = (PyFunctionObject*)malloc(sizeof(PyFunctionObject));
    if (!func_obj) {
        fprintf(stderr, "Runtime Error: Failed to allocate memory for function object.\n");
        // 在实际项目中，这里应该引发 MemoryError
        return NULL;
    }

    // 初始化对象头 (使用 PyObject 结构)
    func_obj->header.refCount = 1;          // 新对象引用计数为 1
    func_obj->header.typeId = llvmpy::PY_TYPE_FUNC; // 设置类型为函数

    // 设置函数特定字段
    func_obj->func_ptr = func_ptr;
    func_obj->signature_type_id = signature_type_id;

    // TODO: 初始化其他可能的字段 (如 __name__)

    return (PyObject*)func_obj; // 返回 PyObject* 指针
}

