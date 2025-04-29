// filepath: /home/ljs/code/llvmpy/src/RunTime/py_function.c
#include "RunTime/runtime.h"
#include <stdlib.h>  // for malloc
#include <stdio.h>   // for fprintf, stderr (错误处理)
#include <iostream>  // For debug/error messages
#include <vector>    // 用于错误处理

// --- 辅助函数：将 PyTypeId 映射到 ffi_type* ---
// 注意：这需要根据你的 PyObject* 如何映射到 C 类型来调整
//       目前假设所有 Python 对象都通过 PyObject* (即 void*) 传递
static ffi_type* map_pytype_to_ffitype(int typeId)
{
    // 基础类型映射 (如果你的函数直接操作 C 类型)
    // switch (llvmpy::getBaseTypeId(typeId)) {
    //     case llvmpy::PY_TYPE_INT:    return &ffi_type_sint; // 或者 ffi_type_sint64
    //     case llvmpy::PY_TYPE_DOUBLE: return &ffi_type_double;
    //     case llvmpy::PY_TYPE_BOOL:   return &ffi_type_uint8; // 或者 sint?
    //     case llvmpy::PY_TYPE_NONE:   return &ffi_type_void; // None 通常不直接作为参数类型，但可能用于返回值
    //     // ... 其他基础类型
    // }

    // 对于所有 Python 对象，包括自定义类型、列表、字典、字符串等，
    // 我们都通过 PyObject* (即 void*) 传递
    // 指针类型也视为 void*
    if (typeId >= llvmpy::PY_TYPE_NONE)
    {  // 涵盖所有已知和未知类型 ID
        return &ffi_type_pointer;
    }

    // 默认或错误情况
    fprintf(stderr, "Warning: Cannot map PyTypeId %d to ffi_type, defaulting to pointer.\n", typeId);
    return &ffi_type_pointer;
}

// --- 辅助函数：从 signature_id 获取 ffi 签名信息 ---
// !!! 这是最关键的部分，需要根据你的 signature_id 编码方式实现 !!!
// 假设：signature_id 目前只代表返回类型，所有参数都是 PyObject*
static bool get_ffi_signature(int signature_id, int num_args,
                              ffi_type** ffi_return_type,
                              std::vector<ffi_type*>& ffi_arg_types)
{
    // 1. 确定返回类型 (根据 signature_id)
    //    简单假设：所有函数都返回 PyObject*
    //    你需要根据 signature_id 的实际含义来修改这里
    *ffi_return_type = map_pytype_to_ffitype(signature_id);
    // 如果 signature_id 只是 PY_TYPE_FUNC，则默认返回 PyObject*
    if (signature_id == llvmpy::PY_TYPE_FUNC || !*ffi_return_type)
    {
        *ffi_return_type = &ffi_type_pointer;  // 默认为 PyObject*
    }

    // 2. 确定参数类型
    //    简单假设：所有参数都是 PyObject*
    //    你需要根据 signature_id (如果它编码了参数类型) 来修改这里
    ffi_arg_types.resize(num_args);
    for (int i = 0; i < num_args; ++i)
    {
        ffi_arg_types[i] = &ffi_type_pointer;  // 假设所有参数都是 PyObject*
    }

    // TODO: 在这里添加基于 signature_id 的验证逻辑
    // 例如，检查 signature_id 是否表示一个接受 num_args 个参数的函数

    return true;  // 假设成功解析
}

PyObject* py_call_function(PyObject* callable, int num_args, PyObject** args_array)
{
#ifdef DEBUG_RUNTIME_py_call_function
    fprintf(stderr, "Debug: py_call_function called with %d arguments.\n", num_args);
#endif

    // 1. 检查 callable (同之前)
    if (!callable)
    {
        fprintf(stderr, "Runtime Error: Attempted to call null object.\n");
        return NULL;
    }
    int type_id = py_get_type_id(callable);
    // 允许调用 PY_TYPE_FUNC 或更具体的函数类型 (>= FUNC_BASE)
    if (type_id != llvmpy::PY_TYPE_FUNC && type_id < llvmpy::PY_TYPE_FUNC_BASE)
    {
        fprintf(stderr, "Runtime Error: Attempted to call non-function object (Type ID: %d).\n", type_id);
        return NULL;
    }

    PyFunctionObject* py_func = (PyFunctionObject*)callable;
    void* generic_func_ptr = py_func->func_ptr;
    int signature_id = py_func->signature_type_id;  // 获取签名ID

    if (!generic_func_ptr)
    {
        fprintf(stderr, "Runtime Error: Function object contains null function pointer.\n");
        return NULL;
    }
#ifdef DEBUG_RUNTIME_py_call_function
    if (num_args == 0)
    {
        typedef PyObject* (*LLVMFunctionNoArgs)();
        LLVMFunctionNoArgs llvm_func_ptr = (LLVMFunctionNoArgs)generic_func_ptr;
        PyObject* result = nullptr;
        try
        {
            fprintf(stderr, "Debug: Calling 0-arg function directly (bypassing libffi).\n");  // 添加调试输出
            result = llvm_func_ptr();
            if (!result)
            {
                fprintf(stderr, "Runtime Warning: Called function returned NULL. Returning None.\n");
                result = py_get_none();
                py_incref(result);
            }
            return result;  // 返回新引用
        }
        catch (const std::exception& e)
        {
            fprintf(stderr, "Runtime Error: Exception during direct 0-arg function call: %s\n", e.what());
            if (result) py_decref(result);
            return NULL;
        }
        catch (...)
        {
            fprintf(stderr, "Runtime Error: Unknown exception during direct 0-arg function call.\n");
            if (result) py_decref(result);
            return NULL;
        }
    }
#endif
    PyObject* result = nullptr;  // 用于存储 ffi_call 的返回值
    ffi_cif cif;                 // 调用接口结构体
    ffi_type* ffi_return_type_ptr;
    std::vector<ffi_type*> ffi_arg_type_ptrs;
    // ffi_call 需要一个 void* 数组，每个元素指向一个参数值
    std::vector<void*> ffi_arg_values(num_args);

    // 2. 解析签名并准备 libffi 类型
    if (!get_ffi_signature(signature_id, num_args, &ffi_return_type_ptr, ffi_arg_type_ptrs))
    {
        fprintf(stderr, "Runtime Error: Invalid signature ID (%d) or argument count mismatch (%d).\n", signature_id, num_args);
        // TODO: Raise appropriate Python exception
        return NULL;
    }

    // 3. 准备调用接口 (cif)
    ffi_type** ffi_arg_types_array = num_args > 0 ? ffi_arg_type_ptrs.data() : nullptr;
    ffi_status prep_status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, num_args, ffi_return_type_ptr, ffi_arg_types_array);

    if (prep_status != FFI_OK)
    {
        fprintf(stderr, "Runtime Error: ffi_prep_cif failed with status %d.\n", prep_status);
        // TODO: Raise InternalError
        return NULL;
    }

    // 4. 准备参数值数组 (void**)
    //    因为我们假设所有参数都是 PyObject*，所以 ffi_arg_values 的每个元素
    //    应该指向 args_array 中对应的 PyObject* 指针。
    for (int i = 0; i < num_args; ++i)
    {
        ffi_arg_values[i] = &args_array[i];
    }

    // 5. 执行调用
    void** ffi_arg_values_array = num_args > 0 ? ffi_arg_values.data() : nullptr;
    try
    {
        // ffi_call 将返回值写入 &result 指向的内存位置
        ffi_call(&cif, FFI_FN(generic_func_ptr), &result, ffi_arg_values_array);

        // 6. 处理返回值 (假设被调函数返回新引用或 NULL)
        if (!result)
        {
            // 如果被调函数返回 NULL，通常表示错误或返回 None
            fprintf(stderr, "Runtime Warning: Called function returned NULL. Returning None.\n");
            result = py_get_none();  // 获取 None 单例
            py_incref(result);       // 返回一个新的 None 引用
        }
        // 如果 result 非 NULL，我们假设它已经是新引用，无需 incref
    }
    catch (const std::exception& e)
    {
        fprintf(stderr, "Runtime Error: C++ exception during ffi_call: %s\n", e.what());
        if (result) py_decref(result);  // 清理可能部分创建的结果
        // TODO: Set Python exception
        return NULL;
    }
    catch (...)
    {
        fprintf(stderr, "Runtime Error: Unknown exception during ffi_call.\n");
        if (result) py_decref(result);
        // TODO: Set Python exception
        return NULL;
    }

    return result;  // 返回新引用
}

PyObject* py_create_function(void* func_ptr, int signature_type_id)
{
    if (!func_ptr)
    {
        fprintf(stderr, "Runtime Error: Attempted to create function object with null pointer.\n");
        // 在实际项目中，这里应该引发一个 Python 异常
        return NULL;
    }

    PyFunctionObject* func_obj = (PyFunctionObject*)malloc(sizeof(PyFunctionObject));
    if (!func_obj)
    {
        fprintf(stderr, "Runtime Error: Failed to allocate memory for function object.\n");
        // 在实际项目中，这里应该引发 MemoryError
        return NULL;
    }

    // 初始化对象头 (使用 PyObject 结构)
    func_obj->header.refCount = 1;                   // 新对象引用计数为 1
    func_obj->header.typeId = llvmpy::PY_TYPE_FUNC;  // 设置类型为函数

    // 设置函数特定字段
    func_obj->func_ptr = func_ptr;
    func_obj->signature_type_id = signature_type_id;

    // TODO: 初始化其他可能的字段 (如 __name__)

    return (PyObject*)func_obj;  // 返回 PyObject* 指针
}

PyObject* py_call_function_noargs(PyObject* func_obj)
{
#ifdef DEBUG_RUNTIME_py_call_function_noargs
    fprintf(stderr, "Debug: py_call_function_noargs called.\n");
#endif

    // 可以用 ffi 实现，但对于 0 参数，直接调用更简单
    if (!func_obj)
    {
        std::cerr << "Runtime Error: Attempted to call null function object." << std::endl;
        return nullptr;
    }
    int type_id = py_get_type_id(func_obj);
    if (type_id != llvmpy::PY_TYPE_FUNC && type_id < llvmpy::PY_TYPE_FUNC_BASE)
    {
        std::cerr << "Runtime Error: Attempted to call non-function object (Type ID: "
                  << type_id << ")" << std::endl;
        return nullptr;
    }

    PyFunctionObject* py_func = (PyFunctionObject*)func_obj;
    void* generic_func_ptr = py_func->func_ptr;
    if (!generic_func_ptr)
    {
        std::cerr << "Runtime Error: Function object contains null function pointer." << std::endl;
        return nullptr;
    }

    // 假设 0 参数函数总是返回 PyObject*
    typedef PyObject* (*LLVMFunctionNoArgs)();
    LLVMFunctionNoArgs llvm_func_ptr = (LLVMFunctionNoArgs)generic_func_ptr;

    PyObject* result = nullptr;
    try
    {
        result = llvm_func_ptr();
        if (!result)
        {
            std::cerr << "Runtime Warning: Called function returned NULL. Returning None." << std::endl;
            result = py_get_none();
            py_incref(result);  // <--- 添加此行
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Runtime Error: Exception during function call: " << e.what() << std::endl;
        if (result) py_decref(result);
        return nullptr;
    }
    catch (...)
    {
        std::cerr << "Runtime Error: Unknown exception during function call." << std::endl;
        if (result) py_decref(result);
        return nullptr;
    }

    return result;
}
