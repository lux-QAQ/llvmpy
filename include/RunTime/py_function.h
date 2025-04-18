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

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PY_FUNCTION_H