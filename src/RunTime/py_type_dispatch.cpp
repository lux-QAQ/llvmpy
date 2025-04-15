
#include "RunTime/py_container.h"
#include "RunTime/py_type_dispatch.h"
#include "RunTime/runtime.h"  // Include main runtime header for other functions

#include "TypeIDs.h"  // For PY_TYPE_LIST, PY_TYPE_DICT etc.
#include <cstdio>
#include <cstdlib>
#include <string>  // For memset

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
        /*.getattr   =*/NULL,  // 列表没有默认的 getattr
        /*.setattr   =*/NULL,  // 列表没有默认的 setattr
};

static const PyTypeMethods dict_methods = {
        /*.index_get =*/py_dict_index_get_handler,
        /*.index_set =*/py_dict_index_set_handler,
        /*.len       =*/py_dict_len_handler,
        /*.getattr   =*/NULL,  // 字典没有默认的 getattr
        /*.setattr   =*/NULL,  // 字典没有默认的 setattr
};

static const PyTypeMethods instance_methods = {
        /*.index_get =*/NULL,
        /*.index_set =*/NULL,
        /*.len       =*/NULL,
        /*.getattr   =*/py_instance_getattr_handler,
        /*.setattr   =*/py_instance_setattr_handler,
};

static const PyTypeMethods class_methods = {
        /*.index_get =*/NULL,
        /*.index_set =*/NULL,
        /*.len       =*/NULL,
        /*.getattr   =*/py_class_getattr_handler,
        /*.setattr   =*/py_class_setattr_handler,
};

// 定义内置类型的方法表 (现在包含 hash 和 equals)
static const PyTypeMethods int_methods = {
        /*index_get*/ NULL, /*index_set*/ NULL, /*len*/ NULL,
        /*getattr*/ NULL, /*setattr*/ NULL,
        /*hash*/ _py_int_hash, /*equals*/ _py_numeric_eq};

static const PyTypeMethods float_methods = {
        /*index_get*/ NULL, /*index_set*/ NULL, /*len*/ NULL,
        /*getattr*/ NULL, /*setattr*/ NULL,
        /*hash*/ _py_float_hash, /*equals*/ _py_numeric_eq};

static const PyTypeMethods string_methods = {
        /*index_get*/ _py_string_index_get_handler,  // <--- 修改这里
        /*index_set*/ NULL,
        /*len*/ NULL,  // TODO: Implement string length handler
        /*getattr*/ NULL,
        /*setattr*/ NULL,
        /*hash*/ _py_string_hash,
        /*equals*/ _py_string_eq};

static const PyTypeMethods bool_methods = {
        /*index_get*/ NULL, /*index_set*/ NULL, /*len*/ NULL,
        /*getattr*/ NULL, /*setattr*/ NULL,
        /*hash*/ _py_bool_hash, /*equals*/ _py_bool_eq  // Use _py_bool_eq
};

static const PyTypeMethods none_methods = {
        /*index_get*/ NULL, /*index_set*/ NULL, /*len*/ NULL,
        /*getattr*/ NULL, /*setattr*/ NULL,
        /*hash*/ _py_none_hash, /*equals*/ _py_none_eq};

// --- 新增: 实例属性访问处理器实现 ---
static PyObject* py_instance_getattr_handler(PyObject* obj, const char* attr_name)
{
    if (!obj || obj->typeId < llvmpy::PY_TYPE_INSTANCE_BASE || !attr_name)
    {  // 假设实例 ID >= BASE
        // 或者检查 obj->typeId == llvmpy::PY_TYPE_INSTANCE
        fprintf(stderr, "TypeError: getattr called on non-instance or with NULL name\n");
        return NULL;  // 返回 NULL 表示失败
    }
    PyInstanceObject* instance = (PyInstanceObject*)obj;
    PyObject* attr_name_obj = py_create_string(attr_name);  // 需要字符串对象作为 key
    if (!attr_name_obj) return NULL;

    PyObject* value = NULL;

    // 1. 在实例字典中查找
    if (instance->instance_dict)
    {
        value = py_dict_get_item((PyObject*)instance->instance_dict, attr_name_obj);
        // py_dict_get_item 失败时返回 NULL 并打印错误，成功时返回找到的值 (不增引用)
    }

    // 2. 如果实例字典中没有，在类字典中查找 (以及基类)
    if (!value && instance->cls)
    {
        // 递归查找类及其基类
        PyClassObject* current_cls = instance->cls;
        while (current_cls && !value)
        {
            if (current_cls->class_dict)
            {
                value = py_dict_get_item((PyObject*)current_cls->class_dict, attr_name_obj);
            }
            current_cls = current_cls->base;  // 移动到基类
        }
    }

    py_decref(attr_name_obj);  // 释放临时的 key 对象

    if (value)
    {
        py_incref(value);  // 返回前增加引用计数
        return value;
    }
    else
    {
        fprintf(stderr, "AttributeError: '%s' object has no attribute '%s'\n",
                instance->cls ? instance->cls->name : "object", attr_name);
        return NULL;  // 未找到属性
    }
}

static int py_instance_setattr_handler(PyObject* obj, const char* attr_name, PyObject* value)
{
    if (!obj || obj->typeId < llvmpy::PY_TYPE_INSTANCE_BASE || !attr_name)
    {
        fprintf(stderr, "TypeError: setattr called on non-instance or with NULL name\n");
        return -1;  // 返回 -1 表示失败
    }
    PyInstanceObject* instance = (PyInstanceObject*)obj;

    // 实例属性总是设置在实例字典中
    if (!instance->instance_dict)
    {
        // 如果实例还没有字典，创建一个 (需要确定 key 类型，通常是 string)
        instance->instance_dict = (PyDictObject*)py_create_dict(8, llvmpy::PY_TYPE_STRING);
        if (!instance->instance_dict)
        {
            fprintf(stderr, "MemoryError: Failed to create instance dictionary\n");
            return -1;
        }
    }

    PyObject* attr_name_obj = py_create_string(attr_name);
    if (!attr_name_obj) return -1;

    // 调用 py_dict_set_item 来设置属性
    py_dict_set_item((PyObject*)instance->instance_dict, attr_name_obj, value);

    py_decref(attr_name_obj);  // 释放临时的 key 对象
    return 0;                  // 返回 0 表示成功
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
    // 递归查找类及其基类
    PyClassObject* current_cls = cls;
    while (current_cls && !value)
    {
        if (current_cls->class_dict)
        {
            value = py_dict_get_item((PyObject*)current_cls->class_dict, attr_name_obj);
        }
        current_cls = current_cls->base;  // 移动到基类
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

    // 类属性设置在类字典中
    if (!cls->class_dict)
    {
        // 通常类在创建时就应该有字典
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
    // 注册内置类型的方法
    py_register_type_methods(llvmpy::PY_TYPE_INT, &int_methods);
    py_register_type_methods(llvmpy::PY_TYPE_DOUBLE, &float_methods); 
    py_register_type_methods(llvmpy::PY_TYPE_STRING, &string_methods);
    py_register_type_methods(llvmpy::PY_TYPE_BOOL, &bool_methods);
    py_register_type_methods(llvmpy::PY_TYPE_NONE, &none_methods);

    // 注册 List 和 Dict 的方法 (假设它们已定义并包含 index_set 等)
     py_register_type_methods(llvmpy::PY_TYPE_LIST, &list_methods);
     py_register_type_methods(llvmpy::PY_TYPE_DICT, &dict_methods);
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
    return (unsigned int)((PyPrimitiveObject*)obj)->value.intValue;
}

static unsigned int _py_float_hash(PyObject* obj)
{
    // 基于 std::hash<double> 实现，更健壮
    return std::hash<double>{}(((PyPrimitiveObject*)obj)->value.doubleValue);
    // // 或者一个简单的基于位模式的哈希 (可能不够好)
    // union { double d; unsigned long long i; } u;
    // u.d = ((PyPrimitiveObject*)obj)->value.doubleValue;
    // // 简单的位异或哈希
    // return (unsigned int)(u.i ^ (u.i >> 32));
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
    double val_self, val_other;
    if (py_coerce_numeric(self, other, &val_self, &val_other))
    {
        // 对浮点数使用近似比较可能更好，但 Python 的 == 对于 int/float 混合是精确的
        return py_create_bool(val_self == val_other);
    }
    // 类型不兼容，返回 False (Python 中 1 == '1' 是 False)
    return py_create_bool(false);
}

static PyObject* _py_string_eq(PyObject* self, PyObject* other)
{
    if (!other || py_get_safe_type_id(other) != llvmpy::PY_TYPE_STRING)
    {
        return py_create_bool(false);  // 不同类型不相等
    }
    const char* str_self = ((PyPrimitiveObject*)self)->value.stringValue;
    const char* str_other = ((PyPrimitiveObject*)other)->value.stringValue;
    if (str_self == str_other) return py_create_bool(true);     // 同一指针或都为 NULL
    if (!str_self || !str_other) return py_create_bool(false);  // 一个为 NULL
    return py_create_bool(strcmp(str_self, str_other) == 0);
}

static PyObject* _py_bool_eq(PyObject* self, PyObject* other)
{
    // Bool 可以和数值比较 (True==1, False==0)
    double val_self, val_other;
    if (py_coerce_numeric(self, other, &val_self, &val_other))
    {
        return py_create_bool(val_self == val_other);
    }
    return py_create_bool(false);
}

static PyObject* _py_none_eq(PyObject* self, PyObject* other)
{
    (void)self;  // Unused
    // None 只等于 None (即同一个单例对象)
    return py_create_bool(other && py_get_safe_type_id(other) == llvmpy::PY_TYPE_NONE);
    // 或者更严格地检查是否是同一个单例对象指针 (如果 py_get_none 返回单例)
    // return py_create_bool(self == other);
}

// LIST Handlers
static PyObject* py_list_index_get_handler(PyObject* container, PyObject* index)
{
    // 1. Type Check container (redundant if dispatch works, but good practice)
    if (!py_check_type(container, llvmpy::PY_TYPE_LIST))
    {
        py_type_error(container, llvmpy::PY_TYPE_LIST);
        return py_get_none();  // Or NULL? Consistent error handling needed.
    }
    PyListObject* list = (PyListObject*)container;

    // 2. Convert index to C int
    PyObject* idxObj = py_extract_int_from_any(index);
    if (!idxObj)
    {
        // Error already printed by py_extract_int_from_any
        return py_get_none();
    }
    int int_index = py_extract_int(idxObj);
    py_decref(idxObj);  // Decref the temporary int object

    // 3. Bounds Check
    if (int_index < 0 || int_index >= list->length)
    {
        fprintf(stderr, "IndexError: list index out of range (index: %d, size: %d)\n", int_index, list->length);
        return py_get_none();
    }

    // 4. Get Item and Incref
    PyObject* item = list->data[int_index];
    if (item)
    {  // Should generally not be NULL in a valid list
        py_incref(item);
    }
    else
    {
        // Handle potential NULL item? Return None?
        item = py_get_none();  // Return None if item is somehow NULL
        py_incref(item);
    }
    return item;
}

static PyObject* _py_string_index_get_handler(PyObject* container, PyObject* index)
{
    // 1. Type Check container
    if (!py_check_type(container, llvmpy::PY_TYPE_STRING))
    {
        py_type_error(container, llvmpy::PY_TYPE_STRING);
        return py_get_none();  // Or NULL?
    }

    // 2. Convert index PyObject* to C int
    PyObject* idxObj = py_extract_int_from_any(index);  // Returns new reference or NULL
    if (!idxObj)
    {
        // Error already printed by py_extract_int_from_any
        return py_get_none();
    }
    int int_index = py_extract_int(idxObj);  // Extracts C value
    py_decref(idxObj);                       // Decref the temporary int object

    // 3. Call the original py_string_get_char
    // py_string_get_char handles bounds checking and returns a new reference
    return py_string_get_char(container, int_index);
}

static void py_list_index_set_handler(PyObject* container, PyObject* index, PyObject* value)
{
    // 1. Type Check container
    if (!py_check_type(container, llvmpy::PY_TYPE_LIST))
    {
        py_type_error(container, llvmpy::PY_TYPE_LIST);
        return;
    }
    PyListObject* list = (PyListObject*)container;

    // 2. Convert index to C int
    PyObject* idxObj = py_extract_int_from_any(index);  // 返回新引用或 NULL
    if (!idxObj)
    {
        return;  // Error printed
    }
    int int_index = py_extract_int(idxObj);  // 提取 C 值
    py_decref(idxObj);                       // 释放 py_extract_int_from_any 返回的引用

    // 3. Bounds Check
    if (int_index < 0 || int_index >= list->length)
    {
        fprintf(stderr, "IndexError: list assignment index out of range (index: %d, size: %d)\n", int_index, list->length);
        return;
    }

    // 4. Type Check Value (if list has specific element type)
    PyObject* final_value = value;    // 默认使用原始值
    bool needs_decref_final = false;  // 标记是否需要释放转换后的值

    // 检查列表是否定义了具体的元素类型 (elemTypeId > 0 且不是 ANY)
    if (list->elemTypeId > 0 && list->elemTypeId != llvmpy::PY_TYPE_ANY)
    {
        // 尝试将 value 智能转换为列表期望的 elemTypeId
        PyObject* converted_value = py_smart_convert(value, list->elemTypeId);  // 返回新引用或 NULL

        if (!converted_value)
        {
            // 转换失败，py_smart_convert 应该已打印错误
            fprintf(stderr, "TypeError: Cannot assign value of type %s to list with element type %s\n",
                    py_type_name(value ? value->typeId : llvmpy::PY_TYPE_NONE), py_type_name(list->elemTypeId));
            return;
        }

        // 如果转换成功且返回了与原始值不同的新对象
        if (converted_value != value)
        {
            final_value = converted_value;  // 使用转换后的值
            needs_decref_final = true;      // 标记这个新对象在使用后需要释放
        }
        else
        {
            // 如果 py_smart_convert 返回了原始对象 (因为类型兼容)，它已经增加了引用计数。
            // 我们不需要额外处理 final_value，但需要释放 py_smart_convert 返回的这个多余的引用。
            py_decref(converted_value);
        }
    }
    else
    {
        // 列表元素类型是 ANY 或未指定，不需要转换。
        // py_list_set_item 内部会处理 final_value (即原始 value) 的引用计数。
    }

    // 5. Set Item (显式转换第一个参数为 PyObject*)
    py_list_set_item((PyObject*)list, int_index, final_value);  // <--- 修改在这里

    // 6. 如果在步骤 4 中创建了新的转换对象 (converted_value != value)，在这里释放它。
    //    因为 py_list_set_item 已经对 final_value (即 converted_value) 增加了引用，
    //    所以我们现在需要减少 py_smart_convert 返回时的那次额外引用。
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
