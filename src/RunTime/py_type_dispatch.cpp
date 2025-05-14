
/* #include "RunTime/py_container.h"
#include "RunTime/py_type_dispatch.h" */
#include "RunTime/runtime.h"  // Include main runtime header for other functions

#include <cstdio>
#include <cstdlib>

#include <functional>  // For std::hash
#include <cmath>       // For fabs
#include <cstring>     // For strcmp

#define MAX_TYPE_ID 512  // Adjust as needed based on your maximum expected type ID

// Global registry for type methods (simple array-based for now)
static const PyTypeMethods* type_registry[MAX_TYPE_ID] = {NULL};

// --- Initialization ---
static std::unordered_map<int, const PyTypeMethods*> type_methods_registry;
static const PyTypeMethods list_methods = {
        /*.index_get =*/py_list_index_get_handler,
        /*.index_set =*/py_list_index_set_handler,
        /*.len       =*/py_list_len_handler,
        /*.getattr   =*/NULL,
        /*.setattr   =*/NULL,
        /*.hash      =*/NULL,  // Lists are unhashable
        /*.equals    =*/NULL,  // TODO: Implement list comparison if needed
};

static const PyTypeMethods dict_methods = {
        /*.index_get =*/py_dict_index_get_handler,
        /*.index_set =*/py_dict_index_set_handler,
        /*.len       =*/py_dict_len_handler,
        /*.getattr   =*/NULL,
        /*.setattr   =*/NULL,
        /*.hash      =*/NULL,  // Dicts are unhashable
        /*.equals    =*/NULL,  // TODO: Implement dict comparison if needed
};

static const PyTypeMethods instance_methods = {
        /*.index_get =*/NULL,
        /*.index_set =*/NULL,
        /*.len       =*/NULL,
        /*.getattr   =*/py_instance_getattr_handler,
        /*.setattr   =*/py_instance_setattr_handler,
        /*.hash      =*/NULL,  // Default instance hash (maybe based on ID/address?)
        /*.equals    =*/NULL,  // Default instance comparison (identity)
};

static const PyTypeMethods class_methods = {
        /*.index_get =*/NULL,
        /*.index_set =*/NULL,
        /*.len       =*/NULL,
        /*.getattr   =*/py_class_getattr_handler,
        /*.setattr   =*/py_class_setattr_handler,
        /*.hash      =*/NULL,  // Class hash (maybe based on name/address?)
        /*.equals    =*/NULL,  // Class comparison (identity?)
};

// 定义内置类型的方法表 (现在包含 hash 和 equals)
static const PyTypeMethods int_methods = {
        /*index_get*/ NULL, /*index_set*/ NULL, /*len*/ NULL,
        /*getattr*/ NULL, /*setattr*/ NULL,
        /*hash*/ _py_int_hash, /*equals*/ _py_numeric_eq};  // Uses modified functions

static const PyTypeMethods float_methods = {
        /*index_get*/ NULL, /*index_set*/ NULL, /*len*/ NULL,
        /*getattr*/ NULL, /*setattr*/ NULL,
        /*hash*/ _py_float_hash, /*equals*/ _py_numeric_eq};  // Uses modified functions

static const PyTypeMethods string_methods = {
        /*index_get*/ _py_string_index_get_handler,
        /*index_set*/ NULL,
        /*len*/ _py_string_len_handler,  // <-- Added string length handler
        /*getattr*/ NULL,
        /*setattr*/ NULL,
        /*hash*/ _py_string_hash,
        /*equals*/ _py_string_eq};

static const PyTypeMethods bool_methods = {
        /*index_get*/ NULL, /*index_set*/ NULL, /*len*/ NULL,
        /*getattr*/ NULL, /*setattr*/ NULL,
        /*hash*/ _py_bool_hash, /*equals*/ _py_bool_eq};  // Uses modified function

static const PyTypeMethods none_methods = {
        /*index_get*/ NULL, /*index_set*/ NULL, /*len*/ NULL,
        /*getattr*/ NULL, /*setattr*/ NULL,
        /*hash*/ _py_none_hash, /*equals*/ _py_none_eq};

// --- 新增: 实例属性访问处理器实现 ---
static PyObject* py_instance_getattr_handler(PyObject* obj, const char* attr_name)
{
    if (!obj || obj->typeId < llvmpy::PY_TYPE_INSTANCE_BASE || !attr_name)
    {
        fprintf(stderr, "TypeError: getattr called on non-instance or with NULL name\n");
        return NULL;
    }
    PyInstanceObject* instance = (PyInstanceObject*)obj;
    PyObject* attr_name_obj = py_create_string(attr_name);
    if (!attr_name_obj) return NULL;

    PyObject* value = NULL;

    // 1. 在实例字典中查找
    if (instance->instance_dict)
    {
        value = py_dict_get_item((PyObject*)instance->instance_dict, attr_name_obj);
    }

    // 2. 如果实例字典中没有，在类字典中查找 (以及基类)
    if (!value && instance->cls)
    {
        PyClassObject* current_cls = instance->cls;
        while (current_cls && !value)
        {
            if (current_cls->class_dict)
            {
                value = py_dict_get_item((PyObject*)current_cls->class_dict, attr_name_obj);
            }
            current_cls = current_cls->base;
        }
    }

    py_decref(attr_name_obj);

    if (value)
    {
        py_incref(value);
        return value;
    }
    else
    {
        fprintf(stderr, "AttributeError: '%s' object has no attribute '%s'\n",
                instance->cls ? instance->cls->name : "object", attr_name);
        return NULL;
    }
}

static int py_instance_setattr_handler(PyObject* obj, const char* attr_name, PyObject* value)
{
    if (!obj || obj->typeId < llvmpy::PY_TYPE_INSTANCE_BASE || !attr_name)
    {
        fprintf(stderr, "TypeError: setattr called on non-instance or with NULL name\n");
        return -1;
    }
    PyInstanceObject* instance = (PyInstanceObject*)obj;

    if (!instance->instance_dict)
    {
        instance->instance_dict = (PyDictObject*)py_create_dict(8, llvmpy::PY_TYPE_STRING);
        if (!instance->instance_dict)
        {
            fprintf(stderr, "MemoryError: Failed to create instance dictionary\n");
            return -1;
        }
    }

    PyObject* attr_name_obj = py_create_string(attr_name);
    if (!attr_name_obj) return -1;

    py_dict_set_item((PyObject*)instance->instance_dict, attr_name_obj, value);

    py_decref(attr_name_obj);
    return 0;
}

// --- 新增: 类属性访问处理器实现 ---
static PyObject* py_class_getattr_handler(PyObject* obj, const char* attr_name)
{
    if (!obj || obj->typeId != llvmpy::PY_TYPE_CLASS || !attr_name)
    {
        fprintf(stderr, "TypeError: getattr called on non-class or with NULL name\n");
        return NULL;
    }
    PyClassObject* cls = (PyClassObject*)obj;
    PyObject* attr_name_obj = py_create_string(attr_name);
    if (!attr_name_obj) return NULL;

    PyObject* value = NULL;
    PyClassObject* current_cls = cls;
    while (current_cls && !value)
    {
        if (current_cls->class_dict)
        {
            value = py_dict_get_item((PyObject*)current_cls->class_dict, attr_name_obj);
        }
        current_cls = current_cls->base;
    }

    py_decref(attr_name_obj);

    if (value)
    {
        py_incref(value);
        return value;
    }
    else
    {
        fprintf(stderr, "AttributeError: type object '%s' has no attribute '%s'\n", cls->name, attr_name);
        return NULL;
    }
}

static int py_class_setattr_handler(PyObject* obj, const char* attr_name, PyObject* value)
{
    if (!obj || obj->typeId != llvmpy::PY_TYPE_CLASS || !attr_name)
    {
        fprintf(stderr, "TypeError: setattr called on non-class or with NULL name\n");
        return -1;
    }
    PyClassObject* cls = (PyClassObject*)obj;

    if (!cls->class_dict)
    {
        fprintf(stderr, "InternalError: Class '%s' has no dictionary\n", cls->name);
        return -1;
    }

    PyObject* attr_name_obj = py_create_string(attr_name);
    if (!attr_name_obj) return -1;

    py_dict_set_item((PyObject*)cls->class_dict, attr_name_obj, value);

    py_decref(attr_name_obj);
    return 0;
}

void py_initialize_builtin_type_methods()
{

    // 设置GMP默认精度
    mpf_set_default_prec(RUNTIME_FLOATE_PRECISION);  // 设置默认精度为256位
    // 注册内置类型的方法
    py_register_type_methods(llvmpy::PY_TYPE_INT, &int_methods);
    py_register_type_methods(llvmpy::PY_TYPE_DOUBLE, &float_methods);
    py_register_type_methods(llvmpy::PY_TYPE_STRING, &string_methods);
    py_register_type_methods(llvmpy::PY_TYPE_BOOL, &bool_methods);
    py_register_type_methods(llvmpy::PY_TYPE_NONE, &none_methods);

    // 注册 List 和 Dict 的方法 (假设它们已定义并包含 index_set 等)
    py_register_type_methods(llvmpy::PY_TYPE_LIST, &list_methods);
    py_register_type_methods(llvmpy::PY_TYPE_DICT, &dict_methods);

    initialize_static_gmp_bools();
    // 注意：List 和 Dict 应该是不可哈希的，所以它们的 hash 槽位应该是 NULL
    // 它们的 equals 应该实现基于内容的比较（如果需要 a_list == b_list）或保持默认的身份比较。

    // --- 确保 List 和 Dict 的 index_set 处理器被正确注册 ---
    // 这部分逻辑应该在 py_container.cpp 或类似地方完成，确保 list_methods 和 dict_methods
    // 的 index_set 指向正确的函数 (e.g., _py_list_set_item_internal, _py_dict_set_item_internal)
    // 并且这些函数被 py_initialize_builtin_type_methods 调用以完成注册。
    // 例如:
    // 在 py_container.cpp 中:
    // static void _py_list_set_item_dispatch(PyObject* obj, PyObject* index, PyObject* value) { ... }
    // static void _py_dict_set_item_dispatch(PyObject* obj, PyObject* key, PyObject* value) { ... }
    // static const PyTypeMethods list_methods = { ..., index_set = _py_list_set_item_dispatch, hash = NULL, ... };
    // static const PyTypeMethods dict_methods = { ..., index_set = _py_dict_set_item_dispatch, hash = NULL, ... };
    // 然后在 py_initialize_builtin_type_methods 中注册 list_methods 和 dict_methods
}

void py_initialize_builtin_log()
{
#ifdef DEBUG
    // 如果定义了 DEBUG，则启用更详细的日志记录，例如 MSG_DEBUG
    ulog_set_min_level(MSG_DEBUG);
    LOG_INFO("调试模式已启用。日志级别设置为 DEBUG。");
#else
    // 如果未定义 DEBUG，则坚持使用默认的 MSG_ERROR 或其他生产环境级别
    // g_current_min_log_level 默认已是 MSG_ERROR，这里可以显式设置或依赖默认值
    ulog_set_min_level(MSG_ERROR);
    // LOG_INFO("生产模式。日志级别设置为 ERROR。"); // 这条INFO在ERROR级别下不会显示
#endif
}

void py_runtime_initialize()
{
    py_initialize_builtin_type_methods();

    py_initialize_builtin_log();
}

bool py_register_type_methods(int typeId, const PyTypeMethods* methods)
{
    if (typeId < 0 || typeId >= MAX_TYPE_ID)
    {
        fprintf(stderr, "Error: Invalid typeId %d for method registration.\n", typeId);
        return false;
    }
    if (type_registry[typeId] != NULL)
    {
        // Allow re-registration for flexibility, maybe log a warning?
        // fprintf(stderr, "Warning: Re-registering methods for typeId %d.\n", typeId);
    }
    if (!methods)
    {
        fprintf(stderr, "Error: Cannot register NULL methods for typeId %d.\n", typeId);
        return false;
    }
    type_registry[typeId] = methods;
    return true;
}

const PyTypeMethods* py_get_type_methods(int typeId)
{
    if (typeId < 0 || typeId >= MAX_TYPE_ID)
    {
        return NULL;
    }
    // TODO: Handle composite types? If typeId > BASE, maybe check base?
    // For now, direct lookup.
    return type_registry[typeId];
}

// --- Internal Handlers Implementation ---

// --- Hash Functions ---
static unsigned int _py_int_hash(PyObject* obj)
{
    // Ensure type safety (though dispatch should handle it)
    if (py_get_safe_type_id(obj) != llvmpy::PY_TYPE_INT) return 0;  // Error
    mpz_ptr val = py_extract_int(obj);
    if (!val) return 0;  // Error extracting
    // Use mpz_get_ui for a simple hash. Loses info for large numbers.
    return mpz_get_ui(val);
}

static unsigned int _py_float_hash(PyObject* obj)
{
    if (py_get_safe_type_id(obj) != llvmpy::PY_TYPE_DOUBLE) return 0;  // Error
    mpf_ptr val = py_extract_double(obj);
    if (!val) return 0;  // Error extracting
    // Get double representation and use std::hash. Loses precision.
    double d_val = mpf_get_d(val);
    // Check for potential NaN/Inf from mpf_get_d if needed
    // if (std::isnan(d_val) || std::isinf(d_val)) { ... handle special hash ... }
    return std::hash<double>{}(d_val);
}

static unsigned int _py_string_hash(PyObject* obj)
{
    const char* str = ((PyPrimitiveObject*)obj)->value.stringValue;
    if (!str) return 0;
    unsigned int hash = 5381;
    int c;
    while ((c = *str++))
    {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

static unsigned int _py_bool_hash(PyObject* obj)
{
    // Ensure type safety
    if (py_get_safe_type_id(obj) != llvmpy::PY_TYPE_BOOL)
    {
        fprintf(stderr, "TypeError: Expected bool for hash\n");
        return 0;  // Error
    }
    return ((PyPrimitiveObject*)obj)->value.boolValue ? 1 : 0;
}

static unsigned int _py_none_hash(PyObject* obj)
{
    (void)obj;  // Unused
    // None 的哈希值通常是一个固定的特殊值，或者就是 0
    return 0;  // 或者一个特定的常量
}

// --- Equals Functions ---
// 辅助函数：比较两个数值对象是否相等
static PyObject* _py_numeric_eq(PyObject* self, PyObject* other)
{
    int self_type = py_get_safe_type_id(self);
    int other_type = py_get_safe_type_id(other);

    mpz_ptr self_int = NULL, other_int = NULL;
    mpf_ptr self_float = NULL, other_float = NULL;

    // Extract values based on type
    if (self_type == llvmpy::PY_TYPE_INT)
        self_int = py_extract_int(self);
    else if (self_type == llvmpy::PY_TYPE_DOUBLE)
        self_float = py_extract_double(self);

    if (other_type == llvmpy::PY_TYPE_INT)
        other_int = py_extract_int(other);
    else if (other_type == llvmpy::PY_TYPE_DOUBLE)
        other_float = py_extract_double(other);

    // --- Perform GMP comparison ---
    if (self_int && other_int)
    {  // int == int
        return py_create_bool(mpz_cmp(self_int, other_int) == 0);
    }
    else if (self_float && other_float)
    {  // double == double
        return py_create_bool(mpf_cmp(self_float, other_float) == 0);
    }
    else if (self_int && other_float)
    {  // int == double
        return py_create_bool(mpf_cmp_z(other_float, self_int) == 0);
    }
    else if (self_float && other_int)
    {  // double == int
        return py_create_bool(mpf_cmp_z(self_float, other_int) == 0);
    }
    // --- Handle comparison with bool ---
    // If 'self' is numeric and 'other' is bool, delegate to _py_bool_eq
    else if (other_type == llvmpy::PY_TYPE_BOOL)
    {
        return _py_bool_eq(other, self);  // Swap arguments, bool handler is primary
    }
    // If 'self' is bool, dispatch should have gone to _py_bool_eq directly.
    // If 'other' is not numeric or bool, comparison is False.

    // --- Default: Incompatible types or error extracting values ---
    return py_create_bool(false);
}

static PyObject* _py_string_eq(PyObject* self, PyObject* other)
{
    if (!other || py_get_safe_type_id(other) != llvmpy::PY_TYPE_STRING)
    {
        return py_create_bool(false);
    }
    const char* str_self = py_extract_string(self);    // Use extractor
    const char* str_other = py_extract_string(other);  // Use extractor
    if (str_self == str_other) return py_create_bool(true);
    if (!str_self || !str_other) return py_create_bool(false);
    return py_create_bool(strcmp(str_self, str_other) == 0);
}

static PyObject* _py_bool_eq(PyObject* self, PyObject* other)
{
    // self is guaranteed to be bool by dispatch
    if (py_get_safe_type_id(self) != llvmpy::PY_TYPE_BOOL) return py_create_bool(false);  // Safety check
    bool self_val = py_extract_bool(self);                                                // Use extractor
    int other_type = py_get_safe_type_id(other);

    if (other_type == llvmpy::PY_TYPE_BOOL)
    {
        // bool == bool
        bool other_val = py_extract_bool(other);  // Use extractor
        return py_create_bool(self_val == other_val);
    }
    else if (other_type == llvmpy::PY_TYPE_INT)
    {
        // bool == int
        mpz_ptr other_int = py_extract_int(other);
        if (!other_int) return py_create_bool(false);  // Error extracting
        // Compare mpz with 0 or 1
        return py_create_bool(mpz_cmp_ui(other_int, self_val ? 1 : 0) == 0);
    }
    else if (other_type == llvmpy::PY_TYPE_DOUBLE)
    {
        // bool == double
        mpf_ptr other_float = py_extract_double(other);
        if (!other_float) return py_create_bool(false);  // Error extracting
        // Compare mpf with 0 or 1
        return py_create_bool(mpf_cmp_ui(other_float, self_val ? 1 : 0) == 0);
    }
    else
    {
        // Comparison with non-numeric/non-bool types is False
        return py_create_bool(false);
    }
}

static PyObject* _py_none_eq(PyObject* self, PyObject* other)
{
    (void)self;  // Unused
    // None is only equal to None
    return py_create_bool(other && py_get_safe_type_id(other) == llvmpy::PY_TYPE_NONE);
    // Could also compare pointers if py_get_none() is a singleton:
    // return py_create_bool(self == other);
}

// LIST Handlers
static PyObject* py_list_index_get_handler(PyObject* container, PyObject* index)
{
    if (!py_check_type(container, llvmpy::PY_TYPE_LIST))
    {
        py_type_error(container, llvmpy::PY_TYPE_LIST);
        return NULL;  // Return NULL on error
    }
    PyListObject* list = (PyListObject*)container;

    // Convert index using GMP-aware extractor if necessary, or standard int extraction
    mpz_ptr idx_mpz = py_extract_int(index);  // Try extracting GMP int first
    long int_index;                           // Use long for wider range
    if (idx_mpz)
    {
        if (!mpz_fits_slong_p(idx_mpz))
        {
            fprintf(stderr, "IndexError: list index cannot fit in C long\n");
            return NULL;
        }
        int_index = mpz_get_si(idx_mpz);
    }
    else
    {
        // Fallback or specific handling if index isn't a PyInt
        // Maybe try py_extract_constant_int or error out
        fprintf(stderr, "TypeError: list indices must be integers, not %s\n", py_type_name(py_get_safe_type_id(index)));
        return NULL;
    }

    // Handle negative indices
    if (int_index < 0)
    {
        int_index += list->length;
    }

    // Bounds Check
    if (int_index < 0 || int_index >= list->length)
    {
        fprintf(stderr, "IndexError: list index out of range (index: %ld, size: %d)\n", int_index, list->length);
        return NULL;  // Return NULL on error
    }

    // Get Item and Incref
    PyObject* item = list->data[int_index];
    if (item)
    {
        py_incref(item);
        return item;
    }
    else
    {
        // If item is somehow NULL, return None (incref'd)
        PyObject* none_obj = py_get_none();
        py_incref(none_obj);  // py_get_none might not incref
        return none_obj;
    }
}

static PyObject* _py_string_index_get_handler(PyObject* container, PyObject* index)
{
    if (!py_check_type(container, llvmpy::PY_TYPE_STRING))
    {
        py_type_error(container, llvmpy::PY_TYPE_STRING);
        return NULL;  // Return NULL on error
    }

    // --- 移除索引提取逻辑 ---
    /*
    // Convert index using GMP-aware extractor
    mpz_ptr idx_mpz = py_extract_int(index);
    long int_index;
    if (idx_mpz) {
        if (!mpz_fits_slong_p(idx_mpz)) {
             fprintf(stderr, "IndexError: string index cannot fit in C long\n");
             return NULL;
        }
        int_index = mpz_get_si(idx_mpz);
    } else {
        // Handle bool index
        if (py_get_safe_type_id(index) == llvmpy::PY_TYPE_BOOL) {
            int_index = py_extract_bool(index) ? 1 : 0;
        } else {
            fprintf(stderr, "TypeError: string indices must be integers or booleans, not %s\n", py_type_name(py_get_safe_type_id(index)));
            return NULL;
        }
    }
    */

    // 直接调用更新后的 py_string_get_char，传递 PyObject* index
    // py_string_get_char 内部会处理索引提取、负数索引和边界检查
    return py_string_get_char(container, index);  // <-- 修改这里
}

static void py_list_index_set_handler(PyObject* container, PyObject* index, PyObject* value)
{
    if (!py_check_type(container, llvmpy::PY_TYPE_LIST))
    {
        py_type_error(container, llvmpy::PY_TYPE_LIST);
        return;
    }
    PyListObject* list = (PyListObject*)container;  // Still useful for elemTypeId check

    // --- 移除索引提取、负数处理和边界检查逻辑 ---
    /*
    // Convert index using GMP-aware extractor
    mpz_ptr idx_mpz = py_extract_int(index);
    long int_index;
    if (idx_mpz) {
         if (!mpz_fits_slong_p(idx_mpz)) {
             fprintf(stderr, "IndexError: list index cannot fit in C long\n");
             return;
         }
        int_index = mpz_get_si(idx_mpz);
    } else {
        // Handle bool index
        if (py_get_safe_type_id(index) == llvmpy::PY_TYPE_BOOL) {
            int_index = py_extract_bool(index) ? 1 : 0;
        } else {
            fprintf(stderr, "TypeError: list indices must be integers or booleans, not %s\n", py_type_name(py_get_safe_type_id(index)));
            return;
        }
    }

    // Handle negative indices
    if (int_index < 0) {
        int_index += list->length;
    }

    // Bounds Check
    if (int_index < 0 || int_index >= list->length)
    {
        fprintf(stderr, "IndexError: list assignment index out of range (index: %ld, size: %d)\n", int_index, list->length);
        return;
    }
    */

    // Type Check Value (using py_smart_convert) - This part can remain here,
    // or could potentially be moved entirely into py_list_set_item. Keeping it here for now.
    PyObject* final_value = value;
    bool needs_decref_final = false;

    if (list->elemTypeId > 0 && list->elemTypeId != llvmpy::PY_TYPE_ANY)
    {
        PyObject* converted_value = py_smart_convert(value, list->elemTypeId);
        if (!converted_value)
        {
            // py_smart_convert should print the error
            // fprintf(stderr, "TypeError: Cannot assign value of type %s to list with element type %s\n",
            //         py_type_name(py_get_safe_type_id(value)), py_type_name(list->elemTypeId));
            return;
        }
        if (converted_value != value)
        {
            final_value = converted_value;
            needs_decref_final = true;
        }
        else
        {
            // smart_convert returned the original object, decref the extra ref it might have added
            py_decref(converted_value);
        }
    }

    // 直接调用更新后的 py_list_set_item，传递 PyObject* index
    // py_list_set_item 内部会处理索引提取、负数索引、边界检查和实际设置
    py_list_set_item(container, index, final_value);  // <-- 修改这里

    // Decref converted value if necessary
    if (needs_decref_final)
    {
        py_decref(final_value);
    }
}

static int py_list_len_handler(PyObject* obj)
{
    if (!py_check_type(obj, llvmpy::PY_TYPE_LIST))
    {
        py_type_error(obj, llvmpy::PY_TYPE_LIST);
        return -1;  // Indicate error
    }
    return ((PyListObject*)obj)->length;
}

static int _py_string_len_handler(PyObject* obj)
{
    if (!py_check_type(obj, llvmpy::PY_TYPE_STRING))
    {
        py_type_error(obj, llvmpy::PY_TYPE_STRING);
        return -1;  // Indicate error
    }
    const char* str = py_extract_string(obj);  // Use extractor
    return str ? strlen(str) : 0;
}

// DICT Handlers
static PyObject* py_dict_index_get_handler(PyObject* container, PyObject* index)
{
    // py_dict_get_item already does type checking and key checking
    PyObject* value = py_dict_get_item(container, index);
    if (value)
    {
        py_incref(value);  // py_dict_get_item might not incref, ensure we do for consistency
        return value;
    }
    else
    {
        // py_dict_get_item prints KeyError
        return py_get_none();  // Return None on key error
    }
}

static void py_dict_index_set_handler(PyObject* container, PyObject* index, PyObject* value)
{
    // py_dict_set_item handles type checking, key compatibility, value compatibility (if dict stores value type), resizing, ref counting
    // We might want to add value type checking here if PyDictObject is extended to store valueTypeId
    /*
    if (dict->valueTypeId != llvmpy::PY_TYPE_ANY && dict->valueTypeId != llvmpy::PY_TYPE_NONE) {
         PyObject* converted_value = py_smart_convert(value, dict->valueTypeId);
         if (!converted_value) {
             // Error
             return;
         }
         value = converted_value;
         // Handle ref counting of converted_value if different...
    }
    */
    py_dict_set_item(container, index, value);
}

static int py_dict_len_handler(PyObject* obj)
{
    if (!py_check_type(obj, llvmpy::PY_TYPE_DICT))
    {
        py_type_error(obj, llvmpy::PY_TYPE_DICT);
        return -1;  // Indicate error
    }
    return ((PyDictObject*)obj)->size;
}
