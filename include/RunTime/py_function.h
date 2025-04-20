// filepath: /home/ljs/code/llvmpy/include/RunTime/py_function.h
#ifndef PY_FUNCTION_H
#define PY_FUNCTION_H

#include "runtime_common.h" // 包含 PyObjectHeader 定义
#include "TypeIDs.h"        // 包含 PY_TYPE_FUNC

#ifdef __cplusplus
extern "C" {
#endif



/**
 * @brief 创建一个新的 Python 函数对象。
 *
 * @param func_ptr 指向已编译的 LLVM 函数代码的指针。
 * @param signature_type_id 代表函数签名的类型 ID (来自编译器的 FunctionType)。
 * @return 新创建的函数对象 (PyObject*)，失败时返回 NULL。
 *         返回的对象引用计数为 1。
 */
PyObject* py_create_function(void* func_ptr, int signature_type_id);



/**
 * @brief 调用没有参数的python函数对象。
 *
 * @param func_obj python函数对象（pyObject*带有py_type_func类型）。
 * @return 从python函数（PyObject*）或错误的null中返回返回值。
 *返回新参考。
 */
 PyObject* py_call_function_noargs(PyObject* func_obj);



/**
 * @brief 调用一个 Python 函数对象，支持传递参数。
 *
 * @param callable Python 可调用对象 (PyObject*，应为 PyFunctionObject*)。
 * @param num_args 传递的参数数量。
 * @param args_array 指向 PyObject* 参数数组的指针。
 * @return PyObject* 函数的返回值 (新引用)，或在出错时返回 NULL。
 */
 PyObject* py_call_function(PyObject* callable, int num_args, PyObject** args_array);

 

static ffi_type* map_pytype_to_ffitype(int typeId);

static bool get_ffi_signature(int signature_id, int num_args,
    ffi_type** ffi_return_type,
    std::vector<ffi_type*>& ffi_arg_types);
#ifdef __cplusplus
} // extern "C"
#endif

#endif // PY_FUNCTION_H