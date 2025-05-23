// filepath: /home/ljs/code/llvmpy/src/RunTime/py_function.c
#include "RunTime/runtime.h"
#include <stdlib.h>  // for malloc
#include <stdio.h>   // for fprintf, stderr (错误处理)
#include <iostream>  // For debug/error messages
#include <vector>    // 用于错误处理



// 前置声明 FFI 调用部分
static PyObject* py_function_call_via_ffi(PyFunctionObject* func_obj, int num_args, PyObject** args_array);





PyObject* py_function_tp_call_handler(PyObject* callable_func_obj, int num_args, PyObject** args_array) {
    LOG_DEBUG("py_function_tp_call_handler: Calling function %p with %d args.", (void*)callable_func_obj, num_args);
    return py_function_call_via_ffi((PyFunctionObject*)callable_func_obj, num_args, args_array);
}

PyObject* py_bound_method_tp_call_handler(PyObject* callable_bm_obj, int num_args, PyObject** args_array) {
    PyBoundMethodObject* bm = (PyBoundMethodObject*)callable_bm_obj;
    LOG_DEBUG("py_bound_method_tp_call_handler: Calling bound method. self=%p, func=%p, with %d original args.",
              (void*)bm->bm_self, (void*)bm->bm_func, num_args);

    std::vector<PyObject*> new_args_vec;
    new_args_vec.reserve(num_args + 1);
    new_args_vec.push_back(bm->bm_self);
    for (int i = 0; i < num_args; ++i) {
        new_args_vec.push_back(args_array[i]);
    }
    LOG_DEBUG("py_bound_method_tp_call_handler: Dispatching to underlying function with %d total args (self + original).", num_args + 1);
    return py_call_function((PyObject*)bm->bm_func, num_args + 1, new_args_vec.data());
}

PyObject* py_class_tp_call_handler(PyObject* callable_class_obj, int num_args, PyObject** args_array) {
    PyClassObject* cls = (PyClassObject*)callable_class_obj;
    LOG_DEBUG("py_class_tp_call_handler: Instantiating class '%s' (%p) with %d args.",
              cls->name ? cls->name : "<unnamed_class>", (void*)cls, num_args);

    PyObject* instance = py_create_instance(callable_class_obj);
    if (!instance) {
        LOG_ERROR("py_class_tp_call_handler: Failed to create instance of class '%s'.", cls->name ? cls->name : "<unnamed_class>");
        return NULL;
    }
    LOG_DEBUG("py_class_tp_call_handler: Instance %p created for class '%s'.", (void*)instance, cls->name ? cls->name : "<unnamed_class>");

    PyObject* init_method_attr = py_object_getattr(instance, "__init__");
    if (init_method_attr) {
        LOG_DEBUG("py_class_tp_call_handler: Found __init__ attribute %p (type: %s).",
                  (void*)init_method_attr, py_type_name(init_method_attr->typeId));
        const PyTypeMethods* init_methods = py_get_type_methods(py_get_type_id(init_method_attr));
        if (init_methods && init_methods->tp_call) {
            LOG_DEBUG("py_class_tp_call_handler: Calling __init__ method.");
            PyObject* init_result = py_call_function(init_method_attr, num_args, args_array);
            if (!init_result) {
                LOG_ERROR("RuntimeError: __init__ call failed for instance of '%s'. Cleaning up instance.", cls->name ? cls->name : "<unnamed_class>");
                py_decref(init_method_attr);
                py_decref(instance);
                return NULL;
            }
            if (init_result != py_get_none()) {
                LOG_WARN("RuntimeWarning: __init__ for class '%s' should return None, but returned type %s.",
                         cls->name ? cls->name : "<unnamed_class>", py_type_name(init_result->typeId));
            }
            py_decref(init_result);
            LOG_DEBUG("py_class_tp_call_handler: __init__ call completed.");
        } else {
            LOG_WARN("RuntimeWarning: __init__ attribute for class '%s' found but is not callable (type: %s).",
                     cls->name ? cls->name : "<unnamed_class>", py_type_name(init_method_attr->typeId));
        }
        py_decref(init_method_attr);
    } else {
        LOG_DEBUG("py_class_tp_call_handler: No __init__ method found for class '%s'.", cls->name ? cls->name : "<unnamed_class>");
        if (num_args > 0) {
             LOG_ERROR("TypeError: object.__new__(%s) takes no arguments (got %d)", cls->name ? cls->name : "object", num_args);
             py_decref(instance);
             return NULL;
        }
    }
    LOG_DEBUG("py_class_tp_call_handler: Instance %p of class '%s' fully initialized. Returning.", (void*)instance, cls->name ? cls->name : "<unnamed_class>");
    return instance;
}
// --- 辅助函数：将 PyTypeId 映射到 ffi_type* ---
// 注意：这需要根据你的 PyObject* 如何映射到 C 类型来调整
//       目前假设所有 Python 对象都通过 PyObject* (即 void*) 传递
static ffi_type* map_pytype_to_ffitype(int typeId) {
    LOG_DEBUG("map_pytype_to_ffitype: Mapping typeId %d (%s)", typeId, py_type_name(typeId));
    // 对于所有 Python 对象，我们都通过 PyObject* (即 void*) 传递
    if (typeId >= llvmpy::PY_TYPE_NONE) {
        return &ffi_type_pointer;
    }
    LOG_WARN("map_pytype_to_ffitype: Cannot map PyTypeId %d to ffi_type, defaulting to pointer.", typeId);
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

PyObject* py_call_function(PyObject* callable, int num_args, PyObject** args_array) {
    LOG_DEBUG("py_call_function (dispatcher): callable=%p (type: %s), num_args=%d",
              (void*)callable, callable ? py_type_name(callable->typeId) : "NULL", num_args);

    if (!callable) {
        LOG_ERROR("Runtime Error: Attempted to call null object.");
        // py_runtime_error("TypeError_CallNull", 0); // Or your error reporting
        return NULL;
    }

    int type_id = py_get_type_id(callable);
    const PyTypeMethods* methods = py_get_type_methods(type_id);

    if (methods && methods->tp_call) {
        LOG_DEBUG("py_call_function: Dispatching to tp_call for type %s (%d)", py_type_name(type_id), type_id);
        return methods->tp_call(callable, num_args, args_array);
    }

    LOG_ERROR("TypeError: '%s' object (typeId: %d) is not callable.", py_type_name(type_id), type_id);
    // py_runtime_error("TypeError_NotCallable", 0); // Or your error reporting
    return NULL;
}

// --- FFI 调用实现 ---
static PyObject* py_function_call_via_ffi(PyFunctionObject* py_func, int num_args, PyObject** args_array) {
    LOG_DEBUG("py_function_call_via_ffi: func_obj=%p, func_ptr=%p, signature_id=%d, num_args=%d",
              (void*)py_func, py_func ? py_func->func_ptr : NULL, py_func ? py_func->signature_type_id : -1, num_args);

    if (!py_func) { // Should be caught by dispatcher, but good for safety
        LOG_ERROR("py_function_call_via_ffi: py_func is NULL.");
        return NULL;
    }
    void* generic_func_ptr = py_func->func_ptr;
    int signature_id = py_func->signature_type_id;

    if (!generic_func_ptr) {
        LOG_ERROR("Runtime Error: Function object %p contains null function pointer.", (void*)py_func);
        return NULL;
    }

    // 调试：直接调用0参数函数 (如果定义了 DEBUG_RUNTIME_PY_CALL_FUNCTION_NOARGS_DIRECT)
#ifdef DEBUG_RUNTIME_PY_CALL_FUNCTION_NOARGS_DIRECT
    if (num_args == 0) {
        LOG_DEBUG("py_function_call_via_ffi: Attempting direct 0-arg call (DEBUG_RUNTIME_PY_CALL_FUNCTION_NOARGS_DIRECT enabled).");
        typedef PyObject* (*LLVMFunctionNoArgs)();
        LLVMFunctionNoArgs llvm_func_ptr = (LLVMFunctionNoArgs)generic_func_ptr;
        PyObject* direct_result = nullptr;
        try {
            direct_result = llvm_func_ptr();
            if (!direct_result) {
                LOG_WARN("Runtime Warning: Direct 0-arg function call returned NULL. Returning None.");
                direct_result = py_get_none();
                py_incref(direct_result);
            }
            LOG_DEBUG("py_function_call_via_ffi: Direct 0-arg call returned %p.", (void*)direct_result);
            return direct_result;
        } catch (const std::exception& e) {
            LOG_ERROR("Runtime Error: Exception during direct 0-arg function call: %s", e.what());
            if (direct_result) py_decref(direct_result);
            return NULL;
        } catch (...) {
            LOG_ERROR("Runtime Error: Unknown exception during direct 0-arg function call.");
            if (direct_result) py_decref(direct_result);
            return NULL;
        }
    }
#endif

    PyObject* result = nullptr;
    ffi_cif cif;
    ffi_type* ffi_return_type_ptr;
    std::vector<ffi_type*> ffi_arg_type_ptrs_vec;
    std::vector<void*> ffi_arg_values_vec(num_args);

    LOG_DEBUG("py_function_call_via_ffi: Preparing FFI call.");
    if (!get_ffi_signature(signature_id, num_args, &ffi_return_type_ptr, ffi_arg_type_ptrs_vec)) {
        LOG_ERROR("Runtime Error: Invalid signature ID (%d) or argument count mismatch (%d) for FFI call.", signature_id, num_args);
        return NULL;
    }
    // 确保 get_ffi_signature 填充的 ffi_arg_type_ptrs_vec 的大小与 num_args 一致
    if (ffi_arg_type_ptrs_vec.size() != (size_t)num_args) {
        LOG_ERROR("Internal Error: get_ffi_signature filled %zu arg types, but num_args is %d.", ffi_arg_type_ptrs_vec.size(), num_args);
        return NULL; // Mismatch indicates an issue in get_ffi_signature
    }


    ffi_type** ffi_arg_types_array = num_args > 0 ? ffi_arg_type_ptrs_vec.data() : nullptr;
    ffi_status prep_status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, num_args, ffi_return_type_ptr, ffi_arg_types_array);

    if (prep_status != FFI_OK) {
        LOG_ERROR("Runtime Error: ffi_prep_cif failed with status %d.", prep_status);
        return NULL;
    }
    LOG_DEBUG("py_function_call_via_ffi: ffi_prep_cif successful.");

    for (int i = 0; i < num_args; ++i) {
        ffi_arg_values_vec[i] = &args_array[i]; // Pass address of PyObject*
        LOG_DEBUG("py_function_call_via_ffi: Arg %d: value_ptr=%p, PyObject*=%p (type: %s)",
                  i, ffi_arg_values_vec[i], args_array[i], args_array[i] ? py_type_name(args_array[i]->typeId) : "NULL");
    }
    void** ffi_arg_values_array = num_args > 0 ? ffi_arg_values_vec.data() : nullptr;

    LOG_DEBUG("py_function_call_via_ffi: Executing ffi_call to func_ptr %p.", generic_func_ptr);
    try {
        ffi_call(&cif, FFI_FN(generic_func_ptr), &result, ffi_arg_values_array);
        LOG_DEBUG("py_function_call_via_ffi: ffi_call returned. Raw result PyObject* = %p", (void*)result);
        if (!result) {
            LOG_WARN("Runtime Warning: Called function via FFI returned NULL. Returning None.");
            result = py_get_none();
            py_incref(result); // py_get_none() returns a borrowed ref, so incref for new ownership
        }
        // 假设被调函数如果成功，返回的是新引用或正确处理了引用计数
    } catch (const std::exception& e) {
        LOG_ERROR("Runtime Error: C++ exception during ffi_call: %s", e.what());
        if (result) py_decref(result);
        return NULL;
    } catch (...) {
        LOG_ERROR("Runtime Error: Unknown exception during ffi_call.");
        if (result) py_decref(result);
        return NULL;
    }
    LOG_DEBUG("py_function_call_via_ffi: Returning result %p (type: %s)", (void*)result, result ? py_type_name(result->typeId) : "NULL");
    return result;
}
PyObject* py_create_function(void* func_ptr, int signature_type_id)  // 新
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
