#include "RunTime/runtime.h"
#include "TypeIDs.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

using namespace llvmpy;
//===----------------------------------------------------------------------===//
// 对象创建函数实现
//===----------------------------------------------------------------------===//

PyObject* py_create_class(const char* name, PyObject* base_cls_obj, PyObject* class_dict_obj)
{
    // 参数类型检查
    if (!name || !class_dict_obj || class_dict_obj->typeId != llvmpy::PY_TYPE_DICT)
    {
        fprintf(stderr, "Error: Invalid arguments for py_create_class\n");
        return NULL;
    }
    if (base_cls_obj && base_cls_obj->typeId != llvmpy::PY_TYPE_CLASS)
    {
        fprintf(stderr, "Error: Base class must be a class object\n");
        return NULL;
    }

    PyClassObject* cls = (PyClassObject*)malloc(sizeof(PyClassObject));
    if (!cls)
    {
        fprintf(stderr, "MemoryError: Failed to allocate class object\n");
        return NULL;
    }

    cls->header.refCount = 1;  // 初始引用计数为 1
    cls->header.typeId = llvmpy::PY_TYPE_CLASS;
    cls->name = strdup(name);  // 复制类名
    if (!cls->name)
    {
        free(cls);
        fprintf(stderr, "MemoryError: Failed to duplicate class name\n");
        return NULL;
    }
    cls->base = (PyClassObject*)base_cls_obj;
    cls->class_dict = (PyDictObject*)class_dict_obj;

    // 增加基类和类字典的引用计数
    if (cls->base) py_incref((PyObject*)cls->base);
    py_incref((PyObject*)cls->class_dict);

    // TODO: 可以在这里查找或注册特定于此类的 PyTypeMethods

    return (PyObject*)cls;
}

PyObject* py_create_instance(PyObject* cls_obj)
{
    if (!cls_obj || cls_obj->typeId != llvmpy::PY_TYPE_CLASS)
    {
        fprintf(stderr, "TypeError: Cannot create instance from non-class object\n");
        return NULL;
    }
    PyClassObject* cls = (PyClassObject*)cls_obj;

    PyInstanceObject* instance = (PyInstanceObject*)malloc(sizeof(PyInstanceObject));
    if (!instance)
    {
        fprintf(stderr, "MemoryError: Failed to allocate instance object\n");
        return NULL;
    }

    instance->header.refCount = 1;
    // TODO: 分配具体的实例类型 ID，可能需要一个全局计数器或更复杂的方案
    // 暂时使用通用的 INSTANCE ID，或者如果类对象存储了实例类型ID，则使用它
    instance->header.typeId = llvmpy::PY_TYPE_INSTANCE;  // 或者 >= PY_TYPE_INSTANCE_BASE
    instance->cls = cls;
    instance->instance_dict = (PyDictObject*)py_create_dict(8, llvmpy::PY_TYPE_STRING);  // 创建空的实例字典
    if (!instance->instance_dict)
    {
        free(instance);
        fprintf(stderr, "MemoryError: Failed to create instance dictionary\n");
        return NULL;
    }

    py_incref((PyObject*)instance->cls);  // 增加类对象的引用计数

    // TODO: 调用类的 __init__ 方法 (需要函数调用机制)

    return (PyObject*)instance;
}

// 创建一个基本对象
static PyObject* py_create_basic_object(int typeId)
{
    PyObject* obj = (PyObject*)malloc(sizeof(PyObject));
    if (!obj)
    {
        fprintf(stderr, "Error: Out of memory\n");
        return NULL;
    }
    obj->refCount = 1;
    obj->typeId = typeId;
    return obj;
}

// 创建整数对象
PyObject* py_create_int(long long int value)  // <-- Changed parameter type for wider input
{
    PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
    if (!obj)
    {
        fprintf(stderr, "Error: Out of memory creating int object\n");
        return NULL;
    }
    obj->header.refCount = 1;
    obj->header.typeId = PY_TYPE_INT;
    // 初始化 GMP 整数
    mpz_init_set_si(obj->value.intValue, value);
    return (PyObject*)obj;
}

// 从 mpz_t 创建整数对象 (辅助函数)
PyObject* py_create_int_from_mpz(mpz_srcptr src)
{
    PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
    if (!obj)
    {
        fprintf(stderr, "Error: Out of memory creating int object from mpz\n");
        return NULL;
    }
    obj->header.refCount = 1;
    obj->header.typeId = PY_TYPE_INT;
    mpz_init_set(obj->value.intValue, src);  // Initialize from existing mpz_t
    return (PyObject*)obj;
}
PyObject* py_create_int_bystring(const char* s, int base)
{
    if (!s)
    {
        fprintf(stderr, "Error: Cannot create integer from NULL string.\n");
        return NULL;
    }

    PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
    if (!obj)
    {
        fprintf(stderr, "Error: Out of memory creating int object from string.\n");
        return NULL;
    }
    obj->header.refCount = 1;
    obj->header.typeId = PY_TYPE_INT;

    // Initialize and set the value from the string using GMP
    // mpz_init_set_str returns 0 on success, -1 on failure (invalid string)
    if (mpz_init_set_str(obj->value.intValue, s, base) != 0)
    {
        fprintf(stderr, "Error: Invalid integer string format: \"%s\" with base %d\n", s, base);
        mpz_clear(obj->value.intValue);  // Clear the partially initialized mpz_t
        free(obj);
        return NULL;  // Indicate failure
    }

    return (PyObject*)obj;
}
PyObject* py_create_double_bystring(const char* s, int base, mp_bitcnt_t precision)
{
    if (!s)
    {
        fprintf(stderr, "Error: Cannot create double from NULL string.\n");
        return NULL;
    }

    PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
    if (!obj)
    {
        fprintf(stderr, "Error: Out of memory creating double object from string.\n");
        return NULL;
    }
    obj->header.refCount = 1;
    obj->header.typeId = PY_TYPE_DOUBLE;

    // Determine precision: use provided precision or a default (e.g., 256)
    mp_bitcnt_t prec_to_use = (precision > 0) ? precision : RUNTIME_FLOATE_PRECISION;  // Default precision

    // --- MODIFIED: Use mpf_init2 and mpf_set_str separately ---
    // 1. Initialize the mpf_t with the desired precision
    mpf_init2(obj->value.doubleValue, prec_to_use);

    // 2. Set the value from the string using GMP
    // mpf_set_str returns 0 on success, -1 on failure (invalid string)
    if (mpf_set_str(obj->value.doubleValue, s, base) != 0)
    {
        fprintf(stderr, "Error: Invalid double string format: \"%s\" with base %d\n", s, base);
        mpf_clear(obj->value.doubleValue);  // Clear the initialized mpf_t
        free(obj);
        return NULL;  // Indicate failure
    }
    // --- END MODIFICATION ---

    return (PyObject*)obj;
}

// 创建浮点数对象
PyObject* py_create_double(double value)
{
    PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
    if (!obj)
    {
        fprintf(stderr, "Error: Out of memory creating double object\n");
        return NULL;
    }
    obj->header.refCount = 1;
    obj->header.typeId = PY_TYPE_DOUBLE;
    // 初始化 GMP 浮点数 (设置默认精度，例如 256 位)
    mpf_init2(obj->value.doubleValue, RUNTIME_FLOATE_PRECISION);
    mpf_set_d(obj->value.doubleValue, value);
    return (PyObject*)obj;
}
PyObject* py_create_double_from_mpf(mpf_srcptr src)
{
    PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
    if (!obj)
    {
        fprintf(stderr, "Error: Out of memory creating double object from mpf\n");
        return NULL;
    }
    obj->header.refCount = 1;
    obj->header.typeId = PY_TYPE_DOUBLE;
    // Initialize with the same precision as the source
    mpf_init2(obj->value.doubleValue, mpf_get_prec(src));
    mpf_set(obj->value.doubleValue, src);
    return (PyObject*)obj;
}
// 创建布尔对象
PyObject* py_create_bool(bool value)
{
    PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
    if (!obj)
    {
        fprintf(stderr, "Error: Out of memory creating bool object\n");
        return NULL;
    }
    obj->header.refCount = 1;
    obj->header.typeId = PY_TYPE_BOOL;
    obj->value.boolValue = value;
    return (PyObject*)obj;
}

// 创建字符串对象
PyObject* py_create_string(const char* value)
{
    PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
    if (!obj)
    {
        fprintf(stderr, "Error: Out of memory\n");
        return NULL;
    }
    obj->header.refCount = 1;
    obj->header.typeId = PY_TYPE_STRING;

    if (value)
    {
        size_t len = strlen(value) + 1;
        obj->value.stringValue = (char*)malloc(len);
        if (!obj->value.stringValue)
        {
            fprintf(stderr, "Error: Out of memory for string\n");
            free(obj);
            return NULL;
        }
        strcpy(obj->value.stringValue, value);
    }
    else
    {
        obj->value.stringValue = NULL;
    }

    return (PyObject*)obj;
}

// 创建列表对象
PyObject* py_create_list(int size, int elemTypeId)
{
    // 分配列表对象内存
    PyListObject* list = (PyListObject*)malloc(sizeof(PyListObject));
    if (!list)
    {
        fprintf(stderr, "Error: Out of memory for list\n");
        return NULL;
    }

    // 初始化列表头
    list->header.refCount = 1;
    list->header.typeId = PY_TYPE_LIST;
    list->length = 0;
    list->elemTypeId = elemTypeId;

    // 分配数据数组内存
    int capacity = size > 0 ? size : 8;  // 默认初始容量为8
    list->capacity = capacity;
    list->data = (PyObject**)calloc(capacity, sizeof(PyObject*));

    if (!list->data)
    {
        fprintf(stderr, "Error: Out of memory for list data\n");
        free(list);
        return NULL;
    }

    return (PyObject*)list;
}

// 创建字典对象
PyObject* py_create_dict(int initialCapacity, int keyTypeId)
{
    // 分配字典对象内存
    PyDictObject* dict = (PyDictObject*)malloc(sizeof(PyDictObject));
    if (!dict)
    {
        fprintf(stderr, "Error: Out of memory for dictionary\n");
        return NULL;
    }

    // 初始化字典头
    dict->header.refCount = 1;
    dict->header.typeId = PY_TYPE_DICT;
    dict->size = 0;
    dict->keyTypeId = keyTypeId;

    // 分配条目数组内存
    int capacity = initialCapacity > 0 ? initialCapacity : 8;  // 默认初始容量为8
    dict->capacity = capacity;
    dict->entries = (PyDictEntry*)calloc(capacity, sizeof(PyDictEntry));

    if (!dict->entries)
    {
        fprintf(stderr, "Error: Out of memory for dictionary entries\n");
        free(dict);
        return NULL;
    }

    // 初始化所有条目
    for (int i = 0; i < capacity; i++)
    {
        dict->entries[i].key = NULL;
        dict->entries[i].value = NULL;
        dict->entries[i].hash = 0;
        dict->entries[i].used = false;
    }

    return (PyObject*)dict;
}

// 获取None对象
PyObject* py_get_none(void)
{
    // 创建静态单例None对象
    static PyObject* noneObj = NULL;

    if (!noneObj)
    {
        noneObj = py_create_basic_object(PY_TYPE_NONE);
        if (!noneObj)
        {
            fprintf(stderr, "Error: Failed to create None object\n");
            return NULL;
        }
        // None对象是单例，引用计数设为很大
        noneObj->refCount = INT_MAX;
    }

    return noneObj;
}

//===----------------------------------------------------------------------===//
// 引用计数管理实现
//===----------------------------------------------------------------------===//

// 增加对象引用计数
void py_incref(PyObject* obj)
{
    if (obj && obj->refCount < INT_MAX)
    {
        // LOG_DEBUG("py_incref: %p, type: %s (%d), refCount before: %d", // Original
        //          (void*)obj, py_type_name(obj->typeId), obj->typeId, obj->refCount);
        // Simplified to avoid too much noise for incref if not strictly needed for this bug
        if (llvmpy::getBaseTypeId(obj->typeId) == llvmpy::PY_TYPE_ITERATOR_BASE || obj->typeId == llvmpy::PY_TYPE_BOOL)
        {  // Log only for relevant types
            LOG_DEBUG("py_incref: %p, type: %s (%d), refCount before: %d",
                      (void*)obj, py_type_name(obj->typeId), obj->typeId, obj->refCount);
        }
        obj->refCount++;
    }
}
/**
 * @brief 专门用于处理迭代器对象引用计数减少的内部辅助函数。
 */
void py_iterator_decref_specialized(PyObject* obj)
{
    LOG_DEBUG("py_iterator_decref_specialized ENTER: %p, obj->typeId: %d (%s), refCount: %d",
              (void*)obj, obj->typeId, py_type_name(obj->typeId), obj->refCount);
    switch (obj->typeId)
    {
        case llvmpy::PY_TYPE_LIST_ITERATOR:
        {
            PyListIteratorObject* iter = (PyListIteratorObject*)obj;
            LOG_DEBUG("py_iterator_decref_specialized (ListIter): %p, decref'ing iterable %p", (void*)obj, (void*)iter->iterable);
            py_decref(iter->iterable);
        }
        break;
        case llvmpy::PY_TYPE_STRING_ITERATOR:
        {
            PyStringIteratorObject* iter = (PyStringIteratorObject*)obj;
            LOG_DEBUG("py_iterator_decref_specialized (StringIter): %p, decref'ing iterable %p", (void*)obj, (void*)iter->iterable);
            py_decref(iter->iterable);
        }
        break;
        default:
            LOG_WARN("py_iterator_decref_specialized called with unhandled specific iterator typeId %d (%s) for obj %p",
                     obj->typeId, py_type_name(obj->typeId), (void*)obj);
            // fprintf(stderr, "Warning: py_iterator_decref_specialized called with unhandled specific iterator typeId %d (%s)\n", obj->typeId, py_type_name(obj->typeId)); // Replaced
            break;
    }
    LOG_DEBUG("py_iterator_decref_specialized EXIT: Freeing obj %p", (void*)obj);
    free(obj);
}
// 减少对象引用计数，如果减至0则释放对象
void py_decref(PyObject* obj)
{
    if (!obj || obj->typeId == PY_TYPE_NONE) return;

    // LOG_DEBUG("py_decref ENTER: %p, type: %s (%d), refCount before: %d", // Original
    //           (void*)obj, py_type_name(obj->typeId), obj->typeId, obj->refCount);
    // Simplified to avoid too much noise, focus on relevant types
    bool is_relevant_type = (llvmpy::getBaseTypeId(obj->typeId) == llvmpy::PY_TYPE_ITERATOR_BASE || obj->typeId == llvmpy::PY_TYPE_BOOL);
    if (is_relevant_type)
    {
        LOG_DEBUG("py_decref ENTER: %p, type: %s (%d), refCount before: %d",
                  (void*)obj, py_type_name(obj->typeId), obj->typeId, obj->refCount);
    }

    if (obj->refCount <= 0)
    {
        LOG_WARN("py_decref called on object %p with refCount <= 0 (type: %s (%d))",
                 (void*)obj, py_type_name(obj->typeId), obj->typeId);
        // fprintf(stderr, "Warning: py_decref called on object with refCount <= 0 (type: %d)\n", obj->typeId); // Replaced
        return;
    }

    obj->refCount--;

    if (is_relevant_type)
    {  // Log refCount after decrement for relevant types
        LOG_DEBUG("py_decref AFTER DECR: %p, type: %s (%d), refCount now: %d",
                  (void*)obj, py_type_name(obj->typeId), obj->typeId, obj->refCount);
    }

    if (obj->refCount == 0)
    {
        // 根据类型执行清理
        int original_typeId_at_decref_zero = obj->typeId;
        int baseTypeId = llvmpy::getBaseTypeId(original_typeId_at_decref_zero);
        LOG_DEBUG("py_decref: %p, refCount is 0. original_typeId: %s (%d), baseTypeId: %s (%d). Preparing to free/spec_decref.",
                  (void*)obj, py_type_name(original_typeId_at_decref_zero), original_typeId_at_decref_zero,
                  py_type_name(baseTypeId), baseTypeId);  // Assuming py_type_name can handle baseTypeIds too

        switch (baseTypeId)
        {
            case llvmpy::PY_TYPE_LIST:
                LOG_DEBUG("py_decref (%p): Freeing List.", (void*)obj);
                py_list_decref_items((PyListObject*)obj);
                free(((PyListObject*)obj)->data);
                free(obj);
                break;
            case llvmpy::PY_TYPE_DICT:
                // 释放字典条目中的键和值
                for (int i = 0; i < ((PyDictObject*)obj)->capacity; i++)
                {
                    PyDictEntry* entry = &((PyDictObject*)obj)->entries[i];
                    if (entry->used)
                    {
                        py_decref(entry->key);    // Decref key
                        py_decref(entry->value);  // Decref value
                    }
                }
                free(((PyDictObject*)obj)->entries);  // 释放条目数组
                free(obj);
                break;
            case llvmpy::PY_TYPE_STRING:
                LOG_DEBUG("py_decref (%p): Freeing String.", (void*)obj);
                if (((PyPrimitiveObject*)obj)->value.stringValue)
                {
                    free(((PyPrimitiveObject*)obj)->value.stringValue);
                }
                free(obj);
                break;
            case llvmpy::PY_TYPE_CLASS:
            {
                PyClassObject* cls = (PyClassObject*)obj;
                // Name might be shared in the future, but free if strdup'd
                free((void*)cls->name);
                py_decref((PyObject*)cls->base);
                py_decref((PyObject*)cls->class_dict);
                free(obj);
            }
            break;
            case llvmpy::PY_TYPE_INSTANCE:
            {
                PyInstanceObject* instance = (PyInstanceObject*)obj;
                py_decref((PyObject*)instance->cls);
                py_decref((PyObject*)instance->instance_dict);
                free(obj);
            }
            break;
            case llvmpy::PY_TYPE_FUNC:
                // Add cleanup for PyFunctionObject if needed (e.g., free name/docstring)
                // Assuming func_ptr doesn't need freeing here
                free(obj);
                break;

            // --- GMP Cleanup ---
            case llvmpy::PY_TYPE_INT:
                mpz_clear(((PyPrimitiveObject*)obj)->value.intValue);  // 清理 GMP 整数
                free(obj);
                break;
            case llvmpy::PY_TYPE_DOUBLE:
                mpf_clear(((PyPrimitiveObject*)obj)->value.doubleValue);  // 清理 GMP 浮点数
                free(obj);
                break;
                // --- End GMP Cleanup ---
            case llvmpy::PY_TYPE_ITERATOR_BASE:
                LOG_DEBUG("py_decref: %p, baseTypeId is ITERATOR_BASE. Calling py_iterator_decref_specialized. Current obj->typeId: %s (%d)",
                          (void*)obj, py_type_name(obj->typeId), obj->typeId);
                py_iterator_decref_specialized(obj);
                break;
            case llvmpy::PY_TYPE_BOOL:
                LOG_DEBUG("py_decref (%p): Freeing Bool.", (void*)obj);
                free(obj);
                break;
            case llvmpy::PY_TYPE_NONE:  // Should not reach here due to initial check

            default:  // 包含其他基本类型和未知类型
                     LOG_WARN("py_decref on unhandled base typeId %d (original typeId %d, name: %s) for obj %p",
                         baseTypeId, obj->typeId, py_type_name(obj->typeId), (void*)obj);
                // fprintf(stderr, "Warning: py_decref on unhandled base typeId %d (original typeId %d, name: %s)\n", baseTypeId, obj->typeId, py_type_name(obj->typeId)); // Replaced
                free(obj);
                break;
        }
    }
    else if (obj->refCount < 0)
   {
        LOG_ERROR("Object %p refCount fell below zero! (type: %s (%d))",
                  (void*)obj, py_type_name(obj->typeId), obj->typeId);
        // fprintf(stderr, "Error: Object refCount fell below zero! (type: %d, name: %s)\n", obj->typeId, py_type_name(obj->typeId)); // Replaced
    }
}

//===----------------------------------------------------------------------===//
// 对象复制实现
//===----------------------------------------------------------------------===//

// 深度复制对象
PyObject* py_object_copy(PyObject* obj, int typeId)
{
    if (!obj) return NULL;
    if (obj->typeId == PY_TYPE_NONE) return py_get_none();  // None is singleton

    // Use传入的类型ID或对象自身的类型ID
    int actualTypeId = typeId > 0 ? typeId : obj->typeId;
    int baseTypeId = getBaseTypeId(actualTypeId);

    switch (baseTypeId)
    {
        case PY_TYPE_INT:
        {
            // 使用 py_extract_int 获取内部指针，然后用辅助函数创建新对象
            mpz_ptr src_val = py_extract_int(obj);
            if (!src_val)
            {  // Should not happen if type is INT
                fprintf(stderr, "Error copying int: failed to extract value\n");
                return NULL;
            }
            return py_create_int_from_mpz(src_val);  // Create new GMP int
        }

        case PY_TYPE_DOUBLE:
        {
            // 使用 py_extract_double 获取内部指针，然后用辅助函数创建新对象
            mpf_ptr src_val = py_extract_double(obj);
            if (!src_val)
            {  // Should not happen if type is DOUBLE
                fprintf(stderr, "Error copying double: failed to extract value\n");
                return NULL;
            }
            return py_create_double_from_mpf(src_val);  // Create new GMP float
        }

        case PY_TYPE_BOOL:
        {
            // Bool is simple, just copy the value
            bool value = ((PyPrimitiveObject*)obj)->value.boolValue;
            return py_create_bool(value);
        }

        case PY_TYPE_STRING:
        {
            // String needs a new memory allocation and copy
            const char* value = ((PyPrimitiveObject*)obj)->value.stringValue;
            return py_create_string(value);  // py_create_string handles allocation
        }

        case PY_TYPE_LIST:
        {
            PyListObject* srcList = (PyListObject*)obj;
            // Create a new list with the same capacity and element type ID
            PyObject* newListObj = py_create_list(srcList->capacity, srcList->elemTypeId);
            if (!newListObj) return NULL;

            PyListObject* newList = (PyListObject*)newListObj;

            // Deep copy elements
            for (int i = 0; i < srcList->length; i++)
            {
                PyObject* srcItem = srcList->data[i];
                PyObject* newItem = NULL;
                if (srcItem)
                {
                    // Recursively copy each element
                    newItem = py_object_copy(srcItem, srcItem->typeId);
                    if (!newItem)
                    {
                        // Cleanup partially created list if element copy fails
                        py_decref(newListObj);  // This will decref already copied items
                        return NULL;
                    }
                }
                // Directly assign (no need for append logic here)
                newList->data[i] = newItem;  // newItem is NULL if srcItem was NULL
            }
            newList->length = srcList->length;  // Set the correct length
            return newListObj;
        }

        case PY_TYPE_DICT:
        {
            PyDictObject* srcDict = (PyDictObject*)obj;
            // Create a new dict with same capacity and key type ID
            PyObject* newDictObj = py_create_dict(srcDict->capacity, srcDict->keyTypeId);
            if (!newDictObj) return NULL;

            PyDictObject* newDict = (PyDictObject*)newDictObj;

            // Copy entries
            for (int i = 0; i < srcDict->capacity; i++)
            {
                PyDictEntry* srcEntry = &srcDict->entries[i];
                if (srcEntry->used && srcEntry->key)
                {
                    // Deep copy key and value
                    PyObject* newKey = py_object_copy(srcEntry->key, srcEntry->key->typeId);
                    if (!newKey)
                    {
                        py_decref(newDictObj);  // Cleanup
                        return NULL;
                    }

                    PyObject* newValue = NULL;
                    if (srcEntry->value)
                    {
                        newValue = py_object_copy(srcEntry->value, srcEntry->value->typeId);
                        if (!newValue)
                        {
                            py_decref(newKey);
                            py_decref(newDictObj);  // Cleanup
                            return NULL;
                        }
                    }

                    // Use py_dict_set_item which handles hashing and resizing if needed
                    // (Simpler than manually replicating hash table logic here)
                    // Note: py_dict_set_item increments ref counts, which is correct for the new dict
                    py_dict_set_item(newDictObj, newKey, newValue);

                    // py_dict_set_item took ownership (or incremented refs), so we decref our copies
                    py_decref(newKey);
                    if (newValue) py_decref(newValue);

                    // Check for potential errors during setitem (e.g., resize failure)
                    // This requires py_dict_set_item to have error reporting, which it currently lacks.
                    // Assuming success for now.
                }
            }
            // Size is handled by py_dict_set_item calls
            return newDictObj;
        }

        case PY_TYPE_NONE:  // Already handled at the beginning
            return py_get_none();

        // Add cases for CLASS, INSTANCE, FUNC if deep copy is desired/possible
        case PY_TYPE_CLASS:
            fprintf(stderr, "Warning: Deep copying class object not fully implemented.\n");
            // Shallow copy for now? Or return NULL?
            py_incref(obj);  // Shallow copy
            return obj;
        case PY_TYPE_INSTANCE:
            fprintf(stderr, "Warning: Deep copying instance object not fully implemented (calls __deepcopy__?).\n");
            // Shallow copy for now? Or return NULL?
            py_incref(obj);  // Shallow copy
            return obj;
        case PY_TYPE_FUNC:
            fprintf(stderr, "Warning: Copying function object (shallow copy).\n");
            py_incref(obj);  // Functions are typically shared, shallow copy is okay
            return obj;

        default:
            fprintf(stderr, "Warning: Copying unsupported type %d\n", baseTypeId);
            // Maybe shallow copy as a fallback?
            // py_incref(obj);
            // return obj;
            return NULL;  // Or return NULL for unsupported types
    }
}

//===----------------------------------------------------------------------===//
// 对象长度实现
//===----------------------------------------------------------------------===//

// 获取对象长度
int py_object_len(PyObject* obj)
{
    if (!obj) return 0;

    int baseTypeId = getBaseTypeId(obj->typeId);

    switch (baseTypeId)
    {
        case PY_TYPE_STRING:
        {
            PyPrimitiveObject* stringObj = (PyPrimitiveObject*)obj;
            if (stringObj->value.stringValue)
            {
                return strlen(stringObj->value.stringValue);
            }
            return 0;
        }

        case PY_TYPE_LIST:
        {
            PyListObject* list = (PyListObject*)obj;
            return list->length;
        }

        case PY_TYPE_DICT:
        {
            PyDictObject* dict = (PyDictObject*)obj;
            return dict->size;
        }

        default:
            fprintf(stderr, "TypeError: Object of type %d has no len()\n", baseTypeId);
            return 0;
    }
}

//===----------------------------------------------------------------------===//
// 类型提取工具函数 (支持索引操作)
//===----------------------------------------------------------------------===//
