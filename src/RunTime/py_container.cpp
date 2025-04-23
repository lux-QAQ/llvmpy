#include "RunTime/runtime.h"

#include <cstring>
#include <cstdlib>
#include <cctype>

//===----------------------------------------------------------------------===//
// 列表操作函数
//===----------------------------------------------------------------------===//

// 获取列表长度
int py_list_len(PyObject* obj)
{
    if (!py_check_type(obj, llvmpy::PY_TYPE_LIST))
    {
        py_type_error(obj, llvmpy::PY_TYPE_LIST);
        return 0;
    }

    PyListObject* list = (PyListObject*)obj;
    return list->length;
}

// 获取列表元素
PyObject* py_list_get_item(PyObject* listObj, PyObject* indexObj)
{
#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_list_get_item called.\n");
#endif
    if (!listObj)
    {
        fprintf(stderr, "RuntimeError: Attempting to get item from NULL list\n");
        // In Python, this would likely be a crash or earlier error.
        // Returning None might hide issues. Consider returning NULL.
        return NULL;  // Indicate error more strongly
    }

    // 确保是列表类型
    if (!py_check_type(listObj, llvmpy::PY_TYPE_LIST))
    {
        py_type_error(listObj, llvmpy::PY_TYPE_LIST);
        return NULL;  // Indicate error
    }

    PyListObject* list = (PyListObject*)listObj;

    // 提取 GMP 整数索引
    mpz_ptr idx_mpz = py_extract_int(indexObj);
    long c_index;

    if (!idx_mpz)
    {
        // 尝试布尔值? Python 允许 bool 作为索引 (0 或 1)
        if (py_get_safe_type_id(indexObj) == llvmpy::PY_TYPE_BOOL)
        {
            c_index = py_extract_bool(indexObj) ? 1 : 0;
        }
        else
        {
            fprintf(stderr, "TypeError: list indices must be integers or slices, not '%s'\n",
                    py_type_name(py_get_safe_type_id(indexObj)));
            return NULL;  // Indicate error
        }
    }
    else
    {
        // 检查 GMP 整数是否适合 C long
        if (!mpz_fits_slong_p(idx_mpz))
        {
            fprintf(stderr, "IndexError: cannot fit 'int' index into C long\n");
            return NULL;  // Indicate error
        }
        c_index = mpz_get_si(idx_mpz);
    }

#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_list_get_item: Extracted C index: %ld\n", c_index);
#endif

    // 处理负数索引
    if (c_index < 0)
    {
        c_index += list->length;
    }

    // 边界检查
    if (c_index < 0 || c_index >= list->length)
    {
        fprintf(stderr, "IndexError: list index %ld out of range [0, %d)\n", c_index, list->length);
        return NULL;  // Indicate error
    }

    // 获取元素并增加引用计数
    PyObject* item = list->data[c_index];
    if (item)
    {  // Item could theoretically be NULL if list allows it
        py_incref(item);
        return item;
    }
    else
    {
        // If item is NULL, return None (incref'd)
        PyObject* none_obj = py_get_none();
        py_incref(none_obj);
        return none_obj;
    }
}

// 设置列表元素
void py_list_set_item(PyObject* listObj, PyObject* indexObj, PyObject* item)
{
#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_list_set_item called.\n");
#endif
    if (!py_check_type(listObj, llvmpy::PY_TYPE_LIST))
    {
        py_type_error(listObj, llvmpy::PY_TYPE_LIST);
        return;
    }

    PyListObject* list = (PyListObject*)listObj;

    // 提取 GMP 整数索引
    mpz_ptr idx_mpz = py_extract_int(indexObj);
    long c_index;

    if (!idx_mpz)
    {
        if (py_get_safe_type_id(indexObj) == llvmpy::PY_TYPE_BOOL)
        {
            c_index = py_extract_bool(indexObj) ? 1 : 0;
        }
        else
        {
            fprintf(stderr, "TypeError: list indices must be integers or slices, not '%s'\n",
                    py_type_name(py_get_safe_type_id(indexObj)));
            return;
        }
    }
    else
    {
        if (!mpz_fits_slong_p(idx_mpz))
        {
            fprintf(stderr, "IndexError: cannot fit 'int' index into C long\n");
            return;
        }
        c_index = mpz_get_si(idx_mpz);
    }

#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_list_set_item: Extracted C index: %ld\n", c_index);
#endif

    // 处理负数索引
    if (c_index < 0)
    {
        c_index += list->length;
    }

    // 边界检查 (基于长度，因为这是赋值)
    if (c_index < 0 || c_index >= list->length)
    {
        fprintf(stderr, "IndexError: list assignment index %ld out of range [0, %d)\n",
                c_index, list->length);
        return;
    }

    // --- 类型兼容性检查 (使用 py_smart_convert) ---
    PyObject* final_value = item;
    bool needs_decref_final = false;

    if (list->elemTypeId > 0 && list->elemTypeId != llvmpy::PY_TYPE_ANY)
    {
        PyObject* converted_value = py_smart_convert(item, list->elemTypeId);
        if (!converted_value)
        {
            // py_smart_convert should print the error
            // fprintf(stderr, "TypeError: Cannot assign value of type %s to list with element type %s\n",
            //         py_type_name(py_get_safe_type_id(item)), py_type_name(list->elemTypeId));
            return;
        }
        // If conversion happened and created a new object
        if (converted_value != item)
        {
            final_value = converted_value;
            needs_decref_final = true;
        }
        else
        {
            // smart_convert returned the original object (already compatible),
            // but it might have incref'd it. Decref the extra reference.
            py_decref(converted_value);
        }
    }
    // --- 结束类型兼容性检查 ---

    // 替换项目前先减少旧项目的引用计数
    PyObject* old_item = list->data[c_index];
    if (old_item)
    {
#ifdef DEBUG_RUNTIME_CONTAINER
        fprintf(stderr, "DEBUG: py_list_set_item: Decref old item at index %ld (%p, refcnt before: %d)\n", c_index, (void*)old_item, old_item->refCount);
#endif
        py_decref(old_item);
    }

    // 设置新元素并增加引用计数
    list->data[c_index] = final_value;
    if (final_value)
    {
#ifdef DEBUG_RUNTIME_CONTAINER
        fprintf(stderr, "DEBUG: py_list_set_item: Incref new item at index %ld (%p, refcnt before: %d)\n", c_index, (void*)final_value, final_value->refCount);
#endif
        py_incref(final_value);  // Incref the value being stored in the list
    }

    // 如果 final_value 是转换后创建的新对象，减少它的临时引用计数
    if (needs_decref_final)
    {
#ifdef DEBUG_RUNTIME_CONTAINER
        fprintf(stderr, "DEBUG: py_list_set_item: Decref temporary converted value (%p, refcnt before: %d)\n", (void*)final_value, final_value->refCount);
#endif
        py_decref(final_value);
    }

    // 注意：py_list_set_item 不应该改变列表长度
}

// 向列表追加元素
PyObject* py_list_append(PyObject* listObj, PyObject* item)
{
#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_list_append called.\n");
#endif
    if (!py_check_type(listObj, llvmpy::PY_TYPE_LIST))
    {
        py_type_error(listObj, llvmpy::PY_TYPE_LIST);
        return NULL;  // Return NULL on error
    }

    PyListObject* list = (PyListObject*)listObj;

    // --- 类型兼容性检查 (使用 py_smart_convert) ---
    PyObject* final_value = item;
    bool needs_decref_final = false;

    if (list->elemTypeId > 0 && list->elemTypeId != llvmpy::PY_TYPE_ANY)
    {
        PyObject* converted_value = py_smart_convert(item, list->elemTypeId);
        if (!converted_value)
        {
            // py_smart_convert should print the error
            return NULL;  // Return NULL on type error during append
        }
        if (converted_value != item)
        {
            final_value = converted_value;
            needs_decref_final = true;
        }
        else
        {
            py_decref(converted_value);  // Decref extra ref if no conversion needed
        }
    }
    // --- 结束类型兼容性检查 ---

    // 检查是否需要扩展容量
    if (list->length >= list->capacity)
    {
        int newCapacity = (list->capacity == 0) ? 8 : list->capacity * 2;  // Start with 8, then double
#ifdef DEBUG_RUNTIME_CONTAINER
        fprintf(stderr, "DEBUG: py_list_append: Resizing list from %d to %d\n", list->capacity, newCapacity);
#endif
        // Check for potential overflow before multiplication
        if (list->capacity > INT_MAX / 2)
        {
            newCapacity = INT_MAX;  // Cap at INT_MAX
            if (list->length >= newCapacity)
            {
                fprintf(stderr, "MemoryError: Cannot expand list capacity beyond INT_MAX\n");
                if (needs_decref_final) py_decref(final_value);
                return NULL;  // Indicate error
            }
        }

        PyObject** newData = (PyObject**)realloc(list->data, newCapacity * sizeof(PyObject*));

        if (!newData)
        {
            fprintf(stderr, "MemoryError: Failed to expand list capacity to %d\n", newCapacity);
            if (needs_decref_final) py_decref(final_value);
            return NULL;  // Indicate error
        }

        // 初始化新分配的内存 (realloc doesn't guarantee zeroing)
        // Only need to NULL out the newly allocated part
        memset(newData + list->capacity, 0, (newCapacity - list->capacity) * sizeof(PyObject*));

        list->data = newData;
        list->capacity = newCapacity;
    }

    // 添加新元素并增加引用计数
    list->data[list->length] = final_value;
    if (final_value)
    {
#ifdef DEBUG_RUNTIME_CONTAINER
        fprintf(stderr, "DEBUG: py_list_append: Incref appended item (%p, refcnt before: %d)\n", (void*)final_value, final_value->refCount);
#endif
        py_incref(final_value);  // Incref the value being stored
    }
#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_list_append: List %p, BEFORE length increment. Current length: %d, Capacity: %d. Appending item %p.\n",
            (void*)list, list->length, list->capacity, (void*)final_value);
#endif
    list->length++;
#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_list_append: List %p, AFTER length increment. New length: %d.\n",
            (void*)list, list->length);
#endif

    // Decref temporary converted value if created
    if (needs_decref_final)
    {
#ifdef DEBUG_RUNTIME_CONTAINER
        fprintf(stderr, "DEBUG: py_list_append: Decref temporary converted value (%p, refcnt before: %d)\n", (void*)final_value, final_value->refCount);
#endif
        py_decref(final_value);
    }

    // Python's append returns None, but returning the list might be useful internally sometimes.
    // Let's return NULL on error, and the list object on success for chaining?
    // Or stick to Python's void-like return (implicitly None)?
    // For now, return the list object on success, NULL on error.
    return listObj;
}

// 复制列表
PyObject* py_list_copy(PyObject* obj)
{
    if (!py_check_type(obj, llvmpy::PY_TYPE_LIST))
    {
        py_type_error(obj, llvmpy::PY_TYPE_LIST);
        return NULL;
    }

    PyListObject* srcList = (PyListObject*)obj;

    // 创建新列表，与原列表容量相同
    PyObject* newListObj = py_create_list(srcList->capacity, srcList->elemTypeId);
    if (!newListObj) return NULL;

    PyListObject* newList = (PyListObject*)newListObj;

    if (newList->capacity < srcList->length)
    {
        // This shouldn't happen with py_create_list logic above, but check defensively
        fprintf(stderr, "InternalError: Insufficient capacity in py_list_copy\n");
        py_decref(newListObj);
        return NULL;
    }
    // 复制元素
    for (int i = 0; i < srcList->length; i++)
    {
        PyObject* srcItem = srcList->data[i];

        // 对每个元素进行深度复制
        PyObject* newItem = NULL;

        if (srcItem)
        {
            // 从列表中获取的元素需要复制
            newItem = py_object_copy(srcItem, srcItem->typeId);
            if (!newItem)
            {
                // 如果复制失败，清理已创建的列表
                py_decref(newListObj);
                return NULL;
            }
        }

        // 添加到新列表
        newList->data[i] = newItem;
        newList->length++;
    }

    return newListObj;
}

// 从列表中减少元素的引用计数
void py_list_decref_items(PyListObject* list)
{
    if (!list) return;

    for (int i = 0; i < list->length; i++)
    {
        if (list->data[i])
        {
            py_decref(list->data[i]);
            list->data[i] = NULL;
        }
    }
}

// 获取列表元素并返回其类型ID - 用于索引操作
PyObject* py_list_get_item_with_type(PyObject* listObj, PyObject* indexObj, int* out_type_id)
{
#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_list_get_item_with_type called.\n");
#endif
    // Default type ID to NONE in case of early exit
    if (out_type_id) *out_type_id = llvmpy::PY_TYPE_NONE;

    if (!listObj || !out_type_id)
    {
        fprintf(stderr, "RuntimeError: Invalid arguments to py_list_get_item_with_type (list=%p, out_type_id=%p)\n", (void*)listObj, (void*)out_type_id);
        return NULL;  // Indicate error
    }

    // 确保是列表类型
    if (!py_check_type(listObj, llvmpy::PY_TYPE_LIST))
    {
        py_type_error(listObj, llvmpy::PY_TYPE_LIST);
        return NULL;  // Indicate error
    }

    PyListObject* list = (PyListObject*)listObj;

    // 提取 GMP 整数索引
    mpz_ptr idx_mpz = py_extract_int(indexObj);
    long c_index;

    if (!idx_mpz)
    {
        if (py_get_safe_type_id(indexObj) == llvmpy::PY_TYPE_BOOL)
        {
            c_index = py_extract_bool(indexObj) ? 1 : 0;
        }
        else
        {
            fprintf(stderr, "TypeError: list indices must be integers or slices, not '%s'\n",
                    py_type_name(py_get_safe_type_id(indexObj)));
            return NULL;
        }
    }
    else
    {
        if (!mpz_fits_slong_p(idx_mpz))
        {
            fprintf(stderr, "IndexError: cannot fit 'int' index into C long\n");
            return NULL;
        }
        c_index = mpz_get_si(idx_mpz);
    }

#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_list_get_item_with_type: Extracted C index: %ld\n", c_index);
#endif

    // 处理负数索引
    if (c_index < 0)
    {
        c_index += list->length;
    }

    // 边界检查
    if (c_index < 0 || c_index >= list->length)
    {
        fprintf(stderr, "IndexError: list index %ld out of range [0, %d)\n", c_index, list->length);
        return NULL;  // Indicate error
    }

    // 获取元素
    PyObject* item = list->data[c_index];

    // 设置类型ID
    if (item)
    {
        *out_type_id = item->typeId;

        // If list has specific element type, use that (potentially more specific than item->typeId)
        // Example: list[int] containing an int object. elemTypeId might be PY_TYPE_LIST_INT.
        // This logic seems reversed, usually item->typeId is more specific.
        // Let's prioritize item->typeId unless item is NULL.
        // if (list->elemTypeId > 0 && list->elemTypeId != llvmpy::PY_TYPE_ANY)
        // {
        //     *out_type_id = list->elemTypeId;
        // }
    }
    else
    {
        *out_type_id = llvmpy::PY_TYPE_NONE;
    }

    // 增加引用计数并返回 (return NULL if item is NULL)
    if (item)
    {
        py_incref(item);
        return item;
    }
    else
    {
        // Return None if the slot contains NULL
        PyObject* none_obj = py_get_none();
        py_incref(none_obj);
        return none_obj;
    }
}

// 获取列表元素类型ID
int py_get_list_element_type_id(PyObject* list)
{
    if (!list || llvmpy::getBaseTypeId(list->typeId) != llvmpy::PY_TYPE_LIST)
    {
        return 0;  // 无效或不是列表
    }

    PyListObject* listObj = (PyListObject*)list;

    // 如果列表声明了元素类型，直接返回
    if (listObj->elemTypeId > 0)
    {
        return listObj->elemTypeId;
    }

    // 尝试从第一个元素推断类型
    if (listObj->length > 0 && listObj->data[0])
    {
        return listObj->data[0]->typeId;
    }

    return 0;  // 未知元素类型
}

//===----------------------------------------------------------------------===//
// 字典操作函数
//===----------------------------------------------------------------------===//

// 获取字典大小
int py_dict_len(PyObject* obj)
{
    if (!py_check_type(obj, llvmpy::PY_TYPE_DICT))
    {
        py_type_error(obj, llvmpy::PY_TYPE_DICT);
        return 0;
    }

    PyDictObject* dict = (PyDictObject*)obj;
    return dict->size;
}

// 哈希函数
unsigned int py_hash_object(PyObject* obj)
{
    if (!obj)
    {
#ifdef DEBUG_RUNTIME_CONTAINER
        fprintf(stderr, "DEBUG: py_hash_object called with NULL object, returning 0.\n");
#endif
        return 0;  // Hash of NULL pointer
    }

    // Handle None explicitly? CPython hashes None to a constant.
    if (py_get_safe_type_id(obj) == llvmpy::PY_TYPE_NONE)
    {
        // Return a constant hash for None, e.g., 0 or a specific value
        return 0;  // Or some other constant like 1234567
    }

    int typeId = py_get_safe_type_id(obj);
    const PyTypeMethods* methods = py_get_type_methods(typeId);

#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_hash_object: Hashing object %p, typeId %d (%s), methods=%p\n", (void*)obj, typeId, py_type_name(typeId), (void*)methods);
#endif

    if (methods && methods->hash)
    {
        // 调用类型特定的哈希函数 (already GMP-aware if implemented correctly)
        unsigned int hash_val = methods->hash(obj);
#ifdef DEBUG_RUNTIME_CONTAINER
        fprintf(stderr, "DEBUG: py_hash_object: Got hash %u from type-specific method.\n", hash_val);
#endif
        return hash_val;
    }
    else
    {
        // 处理不可哈希类型
        int baseTypeId = llvmpy::getBaseTypeId(typeId);
        // Check known unhashable types explicitly
        if (baseTypeId == llvmpy::PY_TYPE_LIST || baseTypeId == llvmpy::PY_TYPE_DICT /* || add other known unhashable types */)
        {
            fprintf(stderr, "TypeError: unhashable type: '%s'\n", py_type_name(typeId));
            // How to signal error? Raise exception? Return specific value?
            // For now, return 0 and rely on caller to handle potential collisions.
            // Ideally, set an error flag/exception.
            return 0;  // Problematic: 0 can be a valid hash
        }

        // Fallback: Use address? (Generally bad for value types)
        // CPython raises TypeError for instances of classes without __hash__
        fprintf(stderr, "TypeError: unhashable type: '%s' (no hash method found)\n", py_type_name(typeId));
        // Raise exception or return error indicator needed here.
        return 0;  // Problematic
    }
}

// 查找字典项
PyDictEntry* py_dict_find_entry(PyDictObject* dict, PyObject* key)
{
    if (!dict || !key) return NULL;

    unsigned int hash = py_hash_object(key);
    // TODO: Handle error from py_hash_object if it indicates unhashable type

    unsigned int index = hash % dict->capacity;
    int probes = 0;  // To detect full table loop

#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_dict_find_entry: Looking for key %p (hash %u) starting at index %u in dict %p (cap %d)\n", (void*)key, hash, index, (void*)dict, dict->capacity);
#endif

    // 线性探测
    while (probes < dict->capacity)
    {
        unsigned int current_idx = (index + probes) % dict->capacity;
        PyDictEntry* entry = &dict->entries[current_idx];

#ifdef DEBUG_RUNTIME_CONTAINER
        fprintf(stderr, "DEBUG: py_dict_find_entry: Probing index %u (used=%d)\n", current_idx, entry->used);
#endif

        if (!entry->used)
        {
            // Found an empty slot, key is not present. Return this slot for potential insertion.
#ifdef DEBUG_RUNTIME_CONTAINER
            fprintf(stderr, "DEBUG: py_dict_find_entry: Found empty slot at index %u. Key not present.\n", current_idx);
#endif
            return entry;  // Return the empty slot
        }

        // Check if hash matches AND keys are equal
        if (entry->hash == hash && entry->key)
        {
#ifdef DEBUG_RUNTIME_CONTAINER
            fprintf(stderr, "DEBUG: py_dict_find_entry: Hash matches at index %u. Comparing keys (%p vs %p).\n", current_idx, (void*)key, (void*)entry->key);
#endif
            // Compare keys using GMP-aware comparison
            PyObject* cmpResultObj = py_object_compare(key, entry->key, PY_CMP_EQ);
            bool isEqual = false;
            if (cmpResultObj)
            {
                if (py_get_safe_type_id(cmpResultObj) == llvmpy::PY_TYPE_BOOL)
                {
                    isEqual = py_extract_bool(cmpResultObj);
                }
                else
                {
                    fprintf(stderr, "Warning: Equality comparison returned non-boolean type '%s'\n", py_type_name(py_get_safe_type_id(cmpResultObj)));
                    // Treat non-bool result as not equal? Or error?
                }
                py_decref(cmpResultObj);
            }
            else
            {
                // py_object_compare failed (e.g., TypeError)
                fprintf(stderr, "Warning: Key comparison failed during dictionary lookup.\n");
                // Treat as not equal? Or propagate error?
            }

            if (isEqual)
            {
#ifdef DEBUG_RUNTIME_CONTAINER
                fprintf(stderr, "DEBUG: py_dict_find_entry: Keys match! Found entry at index %u.\n", current_idx);
#endif
                return entry;  // Found the key
            }
#ifdef DEBUG_RUNTIME_CONTAINER
            else
            {
                fprintf(stderr, "DEBUG: py_dict_find_entry: Keys do not match at index %u.\n", current_idx);
            }
#endif
        }
        probes++;
    }

    // If we exit the loop, the table is full and the key wasn't found.
    // This indicates a need to resize *before* calling find_entry for insertion,
    // or an internal error if just searching.
#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_dict_find_entry: Table full or key not found after %d probes.\n", probes);
#endif
    return NULL;  // Indicate key not found (or table full)
}

// 重新调整字典大小
bool py_dict_resize(PyDictObject* dict)
{
    if (!dict) return false;

    int oldCapacity = dict->capacity;
    PyDictEntry* oldEntries = dict->entries;

    // 新容量为旧容量的两倍 (handle zero capacity)
    int newCapacity = (oldCapacity == 0) ? 8 : oldCapacity * 2;
    // Check for potential overflow
    if (oldCapacity > 0 && newCapacity / 2 != oldCapacity)
    {
        fprintf(stderr, "MemoryError: Dictionary capacity overflow during resize.\n");
        return false;  // Cannot resize further
    }

#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_dict_resize: Resizing dict %p from %d to %d\n", (void*)dict, oldCapacity, newCapacity);
#endif

    // 分配新的条目数组 (use calloc for zero-initialization)
    PyDictEntry* newEntries = (PyDictEntry*)calloc(newCapacity, sizeof(PyDictEntry));
    if (!newEntries)
    {
        fprintf(stderr, "MemoryError: Failed to allocate memory for dictionary resize (capacity %d)\n", newCapacity);
        return false;
    }

    // Update dictionary structure BEFORE rehashing
    dict->entries = newEntries;
    dict->capacity = newCapacity;
    dict->size = 0;  // Reset size, will be incremented during re-insertion

    // 重新插入所有条目 from old table
    for (int i = 0; i < oldCapacity; i++)
    {
        PyDictEntry* oldEntry = &oldEntries[i];
        if (oldEntry->used && oldEntry->key /* && oldEntry->value - allow None value */)
        {
#ifdef DEBUG_RUNTIME_CONTAINER
            fprintf(stderr, "DEBUG: py_dict_resize: Rehashing old entry %d (key %p, value %p)\n", i, (void*)oldEntry->key, (void*)oldEntry->value);
#endif
            // Re-insert into the *new* table. py_dict_set_item will handle hashing,
            // finding slot in new table, and incref.
            py_dict_set_item((PyObject*)dict, oldEntry->key, oldEntry->value);

            // We need to decref the key and value from the *old* table now,
            // as they are no longer referenced by the old table structure.
            // py_dict_set_item already incref'd them for the new table.
            py_decref(oldEntry->key);
            if (oldEntry->value)
            {  // Decref value only if it's not NULL
                py_decref(oldEntry->value);
            }
        }
    }

    // 释放旧条目数组
    free(oldEntries);

#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_dict_resize: Resize complete for dict %p. New size: %d\n", (void*)dict, dict->size);
#endif
    return true;
}

// 设置字典项
void py_dict_set_item(PyObject* obj, PyObject* key, PyObject* value)
{
#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_dict_set_item called for dict %p, key %p, value %p.\n", (void*)obj, (void*)key, (void*)value);
#endif
    if (!py_check_type(obj, llvmpy::PY_TYPE_DICT))
    {
        py_type_error(obj, llvmpy::PY_TYPE_DICT);
        return;
    }

    if (!key)
    {
        // Python raises TypeError for None key, not KeyError
        fprintf(stderr, "TypeError: unhashable type: 'NoneType'\n");
        return;
    }

    PyDictObject* dict = (PyDictObject*)obj;

    // --- 类型兼容性检查 (Key) ---
    // Hashability is checked later by py_hash_object.
    // Check declared key type if specified.
    if (dict->keyTypeId > 0 && dict->keyTypeId != llvmpy::PY_TYPE_ANY)
    {
        // Use py_are_types_compatible or a similar check if needed.
        // For now, assume hashability check is sufficient.
        // if (!py_are_types_compatible(py_get_safe_type_id(key), dict->keyTypeId)) {
        //     fprintf(stderr, "TypeError: Cannot use type %s as dictionary key for dict with key type %s\n",
        //             py_type_name(py_get_safe_type_id(key)), py_type_name(dict->keyTypeId));
        //     return;
        // }
    }
    // --- Value type check? ---
    // Python dicts don't enforce value types by default. Add if needed.

    // 检查是否需要扩容 (Load factor check)
    // Use > instead of >= for load factor < 1.0 (e.g., 0.75)
    // Check before insertion attempt.
    // Python resizes when dict is 2/3 full. Let's use ~0.75.
    if (dict->size * 4 >= dict->capacity * 3 && dict->capacity > 0)  // Avoid division
    {
#ifdef DEBUG_RUNTIME_CONTAINER
        fprintf(stderr, "DEBUG: py_dict_set_item: Load factor exceeded (%d/%d). Resizing dict %p.\n", dict->size, dict->capacity, (void*)dict);
#endif
        if (!py_dict_resize(dict))
        {
            fprintf(stderr, "MemoryError: Failed to resize dictionary during setitem\n");
            // If resize fails, we cannot insert.
            return;
        }
        // After resize, capacity and entry pointers have changed.
    }

    // 查找条目 (find entry returns existing entry or first empty slot)
    PyDictEntry* entry = py_dict_find_entry(dict, key);

    if (!entry)
    {
        // This implies the table is full AND the key wasn't found,
        // which shouldn't happen if resize logic is correct.
        fprintf(stderr, "InternalError: Dictionary find failed unexpectedly during setitem (table full?). Dict %p, cap %d, size %d\n", (void*)dict, dict->capacity, dict->size);
        return;
    }

    // Check if the entry found is already used (i.e., key exists)
    if (entry->used && entry->key)
    {
        // Key found, update value
#ifdef DEBUG_RUNTIME_CONTAINER
        fprintf(stderr, "DEBUG: py_dict_set_item: Key found at entry %p. Updating value.\n", (void*)entry);
#endif
        PyObject* old_value = entry->value;

        // Update value and handle ref counts
        entry->value = value;
        if (value) py_incref(value);
        if (old_value) py_decref(old_value);
    }
    else
    {
        // Key not found, insert new entry into the empty slot returned by find_entry
#ifdef DEBUG_RUNTIME_CONTAINER
        fprintf(stderr, "DEBUG: py_dict_set_item: Key not found. Inserting new entry at %p.\n", (void*)entry);
#endif
        // Check if hash object failed earlier (if py_hash_object could signal error)
        unsigned int hash = py_hash_object(key);  // Re-hash or store hash from find_entry if possible
        // TODO: Handle hash error signal here

        entry->key = key;
        entry->value = value;
        entry->hash = hash;  // Store hash
        entry->used = true;

        // Incref the new key and value being stored
        py_incref(key);
        if (value) py_incref(value);

        dict->size++;
#ifdef DEBUG_RUNTIME_CONTAINER
        fprintf(stderr, "DEBUG: py_dict_set_item: New entry inserted. Dict %p size now %d.\n", (void*)dict, dict->size);
#endif
    }
}

// 获取字典项
PyObject* py_dict_get_item(PyObject* obj, PyObject* key)
{
#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_dict_get_item called for dict %p, key %p.\n", (void*)obj, (void*)key);
#endif
    if (!py_check_type(obj, llvmpy::PY_TYPE_DICT))
    {
        py_type_error(obj, llvmpy::PY_TYPE_DICT);
        return NULL;  // Indicate error
    }

    if (!key)
    {
        // Python raises KeyError for None key lookup
        fprintf(stderr, "KeyError: None\n");  // Match Python's KeyError message for None
        return NULL;                          // Indicate error
    }

    PyDictObject* dict = (PyDictObject*)obj;

    // 查找条目
    PyDictEntry* entry = py_dict_find_entry(dict, key);

    // Check if entry was found AND is used AND key matches (find_entry guarantees match if entry->used)
    if (entry && entry->used && entry->key)
    {
        // Found the key, return value (without incref - caller should incref if needed)
        PyObject* value = entry->value;
#ifdef DEBUG_RUNTIME_CONTAINER
        fprintf(stderr, "DEBUG: py_dict_get_item: Key found. Returning value %p.\n", (void*)value);
#endif
        // NOTE: CPython's PyDict_GetItem does NOT incref the result.
        // Let's follow that convention. The caller (e.g., index handler) must incref.
        // if (value) py_incref(value); // NO INCREF HERE
        return value;  // Return value (can be NULL if None was stored)
    }
    else
    {
        // Key not found
        // CPython's PyDict_GetItem returns NULL if key not found (without setting error).
        // Operations like `dict[key]` check for NULL and raise KeyError.
#ifdef DEBUG_RUNTIME_CONTAINER
        fprintf(stderr, "DEBUG: py_dict_get_item: Key not found.\n");
#endif
        // Set KeyError? No, follow PyDict_GetItem convention.
        // fprintf(stderr, "KeyError: Key not found in dictionary\n"); // Don't print error here
        return NULL;  // Indicate key not found
    }
}

// 获取字典值并返回其类型ID - 用于索引操作
PyObject* py_dict_get_item_with_type(PyObject* dictObj, PyObject* key, int* out_type_id)
{
#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_dict_get_item_with_type called for dict %p, key %p.\n", (void*)dictObj, (void*)key);
#endif
    // Default type ID to NONE in case of early exit
    if (out_type_id) *out_type_id = llvmpy::PY_TYPE_NONE;

    if (!dictObj || !key || !out_type_id)
    {
        fprintf(stderr, "RuntimeError: Invalid arguments to py_dict_get_item_with_type (dict=%p, key=%p, out_type_id=%p)\n", (void*)dictObj, (void*)key, (void*)out_type_id);
        return NULL;  // Indicate error
    }

    // 确保是字典类型
    if (!py_check_type(dictObj, llvmpy::PY_TYPE_DICT))
    {
        py_type_error(dictObj, llvmpy::PY_TYPE_DICT);
        return NULL;  // Indicate error
    }

    PyDictObject* dict = (PyDictObject*)dictObj;

    // 查找条目
    PyDictEntry* entry = py_dict_find_entry(dict, key);

    // Check if entry was found AND is used AND key matches
    if (entry && entry->used && entry->key)
    {
        PyObject* value = entry->value;
        if (value)
        {
            *out_type_id = value->typeId;
            py_incref(value);  // Incref the returned value
#ifdef DEBUG_RUNTIME_CONTAINER
            fprintf(stderr, "DEBUG: py_dict_get_item_with_type: Found key. Returning value %p (type %d), incref'd.\n", (void*)value, *out_type_id);
#endif
            return value;
        }
        else
        {
            // Value is None (stored as NULL)
            *out_type_id = llvmpy::PY_TYPE_NONE;
            PyObject* none_obj = py_get_none();
            py_incref(none_obj);  // Incref None
#ifdef DEBUG_RUNTIME_CONTAINER
            fprintf(stderr, "DEBUG: py_dict_get_item_with_type: Found key. Value is None. Returning None (%p), incref'd.\n", (void*)none_obj);
#endif
            return none_obj;
        }
    }
    else
    {
        // Key not found
#ifdef DEBUG_RUNTIME_CONTAINER
        fprintf(stderr, "DEBUG: py_dict_get_item_with_type: Key not found.\n");
#endif
        // Raise KeyError? No, return NULL like py_dict_get_item.
        // fprintf(stderr, "KeyError: Key not found in dictionary\n");
        return NULL;  // Indicate key not found
    }
}

// 获取字典键
PyObject* py_dict_keys(PyObject* obj)
{
    if (!py_check_type(obj, llvmpy::PY_TYPE_DICT))
    {
        py_type_error(obj, llvmpy::PY_TYPE_DICT);
        return NULL;
    }

    PyDictObject* dict = (PyDictObject*)obj;

    // 创建列表存储键
    PyObject* keysList = py_create_list(dict->size, dict->keyTypeId);
    if (!keysList) return NULL;

    // 收集所有键
    for (int i = 0; i < dict->capacity; i++)
    {
        if (dict->entries[i].used && dict->entries[i].key)
        {
            // 将键添加到列表中
            py_list_append(keysList, dict->entries[i].key);
        }
    }

    return keysList;
}

//===----------------------------------------------------------------------===//
// 索引操作函数
//===----------------------------------------------------------------------===//

// 执行索引操作并获取结果类型 - 核心函数
PyObject* py_object_index_with_type(PyObject* obj, PyObject* index, int* out_type_id)
{
    if (!obj || !out_type_id)
    {
        // 确保有效参数
        if (out_type_id) *out_type_id = llvmpy::PY_TYPE_NONE;
        // Return NULL on error, not None
        fprintf(stderr, "RuntimeError: Invalid arguments to py_object_index_with_type\n");
        return NULL;
    }
    *out_type_id = llvmpy::PY_TYPE_NONE;  // Default to None type

    // 解包可能的指针 (保持不变)
    PyObject* actual_obj = obj;
    if (obj && obj->typeId >= 400)  // Check obj validity
    {
        PyObject** ptr_obj = (PyObject**)obj;
        if (ptr_obj && *ptr_obj)
        {
            actual_obj = *ptr_obj;
        }
        else
        {
            fprintf(stderr, "RuntimeError: Dereferencing NULL pointer object in py_object_index_with_type\n");
            return NULL;
        }
    }
    if (!actual_obj)
    {
        fprintf(stderr, "TypeError: 'NoneType' object is not subscriptable\n");
        return NULL;
    }

    // 同样解包索引 (保持不变)
    PyObject* actual_index = index;
    if (index && index->typeId >= 400)  // Check index validity
    {
        PyObject** ptr_index = (PyObject**)index;
        if (ptr_index && *ptr_index)
        {
            actual_index = *ptr_index;
        }
        else
        {
            fprintf(stderr, "RuntimeError: Dereferencing NULL pointer index in py_object_index_with_type\n");
            return NULL;
        }
    }
    if (!actual_index)
    {
        fprintf(stderr, "TypeError: subscript indices must be integers, slices, or other valid key types, not 'NoneType'\n");
        return NULL;
    }

    // 获取基本类型ID
    int baseTypeId = llvmpy::getBaseTypeId(actual_obj->typeId);

    // 根据对象类型执行相应的索引操作
    switch (baseTypeId)
    {
        case llvmpy::PY_TYPE_LIST:
        {
            PyListObject* list = (PyListObject*)actual_obj;
            long c_index;  // Use long for index

            // --- GMP Index Extraction ---
            mpz_ptr idx_mpz = py_extract_int(actual_index);
            if (!idx_mpz)
            {
                if (py_get_safe_type_id(actual_index) == llvmpy::PY_TYPE_BOOL)
                {
                    c_index = py_extract_bool(actual_index) ? 1 : 0;
                }
                else
                {
                    fprintf(stderr, "TypeError: list indices must be integers or slices, not '%s'\n",
                            py_type_name(py_get_safe_type_id(actual_index)));
                    return NULL;  // Return NULL on error
                }
            }
            else
            {
                if (!mpz_fits_slong_p(idx_mpz))
                {
                    fprintf(stderr, "IndexError: cannot fit 'int' index into C long\n");
                    return NULL;  // Return NULL on error
                }
                c_index = mpz_get_si(idx_mpz);
            }
            // --- End GMP Index Extraction ---

            // Handle negative index
            if (c_index < 0)
            {
                c_index += list->length;
            }

            // Bounds check
            if (c_index < 0 || c_index >= list->length)
            {
                fprintf(stderr, "IndexError: list index %ld out of range [0, %d)\n", c_index, list->length);
                // *out_type_id remains PY_TYPE_NONE
                return NULL;  // Return NULL on error
            }

            PyObject* item = list->data[c_index];

            // 确定结果类型
            if (item)
            {
                *out_type_id = item->typeId;
                // Optional: Refine type based on list->elemTypeId (logic seems complex/potentially reversed, keep simple for now)
                // if (list->elemTypeId > 0 && list->elemTypeId != llvmpy::PY_TYPE_ANY) { ... }
                py_incref(item);  // Incref the item being returned
                return item;
            }
            else
            {
                // Item is NULL, return None (incref'd)
                *out_type_id = llvmpy::PY_TYPE_NONE;
                PyObject* none_obj = py_get_none();
                py_incref(none_obj);
                return none_obj;
            }
        }

        case llvmpy::PY_TYPE_DICT:
        {
            PyDictObject* dict = (PyDictObject*)actual_obj;
            // Use the existing py_dict_get_item_with_type which handles GMP keys and returns incref'd value/None
            PyObject* value = py_dict_get_item_with_type((PyObject*)dict, actual_index, out_type_id);
            if (!value)
            {
                // Key not found, py_dict_get_item_with_type returns NULL and sets *out_type_id to NONE.
                // Print KeyError here to match Python behavior for direct indexing.
                // Note: py_dict_get_item doesn't print error, but indexing should.
                // Need a way to represent the key in the error message.
                // For now, a generic message.
                fprintf(stderr, "KeyError\n");  // Simplified KeyError message
            }
            return value;  // Return the result (already incref'd or NULL)
        }

        case llvmpy::PY_TYPE_STRING:
        {
            const char* c_str = py_extract_string(actual_obj);
            if (!c_str)
            {
                fprintf(stderr, "InternalError: String object has NULL value in py_object_index_with_type\n");
                return NULL;
            }
            size_t len = strlen(c_str);
            long c_index;  // Use long for index

            // --- GMP Index Extraction ---
            mpz_ptr idx_mpz = py_extract_int(actual_index);
            if (!idx_mpz)
            {
                if (py_get_safe_type_id(actual_index) == llvmpy::PY_TYPE_BOOL)
                {
                    c_index = py_extract_bool(actual_index) ? 1 : 0;
                }
                else
                {
                    fprintf(stderr, "TypeError: string indices must be integers, not '%s'\n",
                            py_type_name(py_get_safe_type_id(actual_index)));
                    return NULL;  // Return NULL on error
                }
            }
            else
            {
                if (!mpz_fits_slong_p(idx_mpz))
                {
                    fprintf(stderr, "IndexError: cannot fit 'int' index into C long\n");
                    return NULL;  // Return NULL on error
                }
                c_index = mpz_get_si(idx_mpz);
            }
            // --- End GMP Index Extraction ---

            // Handle negative index
            if (c_index < 0)
            {
                c_index += len;
            }

            // Bounds check
            if (c_index < 0 || (size_t)c_index >= len)
            {
                fprintf(stderr, "IndexError: string index %ld out of range [0, %zu)\n", c_index, len);
                // *out_type_id remains PY_TYPE_NONE
                return NULL;  // Return NULL on error
            }

            // Create single-character string
            char buf[2] = {c_str[c_index], '\0'};
            *out_type_id = llvmpy::PY_TYPE_STRING;
            // py_create_string returns a new object with refcount 1
            return py_create_string(buf);
        }

        default:
            fprintf(stderr, "TypeError: '%s' object is not subscriptable\n", py_type_name(actual_obj->typeId));
            // *out_type_id remains PY_TYPE_NONE
            return NULL;  // Return NULL for unsupported type
    }
}

// 获取容器类型信息
int py_get_container_type_info(PyObject* container)
{
    if (!container)
    {
        return llvmpy::PY_TYPE_ANY;
    }

    // 解包指针类型
    PyObject* actual = container;
    if (container->typeId >= 400)
    {
        PyObject** ptrObj = (PyObject**)container;
        if (ptrObj && *ptrObj)
        {
            actual = *ptrObj;
        }
    }

    // 检查容器类型
    int baseTypeId = llvmpy::getBaseTypeId(actual->typeId);

    if (baseTypeId == llvmpy::PY_TYPE_LIST)
    {
        PyListObject* list = (PyListObject*)actual;

        // 获取列表声明的元素类型
        if (list->elemTypeId > 0)
        {
            return list->elemTypeId;
        }

        // 尝试从实际元素推断类型
        if (list->length > 0 && list->data[0])
        {
            return list->data[0]->typeId;
        }

        // 默认为ANY类型
        return llvmpy::PY_TYPE_ANY;
    }
    else if (baseTypeId == llvmpy::PY_TYPE_DICT)
    {
        PyDictObject* dict = (PyDictObject*)actual;

        // 返回值类型
        return dict->keyTypeId > 0 ? llvmpy::PY_TYPE_DICT_BASE + dict->keyTypeId : llvmpy::PY_TYPE_DICT;
    }

    // 其他容器类型或不是容器
    return llvmpy::PY_TYPE_ANY;
}

/**
 * @brief 通用索引赋值函数 (Revised using dispatch table).
 *
 * Looks up and calls the appropriate type-specific index setting function.
 *
 * @param obj 目标容器对象 (PyObject*)。
 * @param index 索引对象 (PyObject*)。
 * @param value 要赋的值 (PyObject*)。
 */
void py_object_set_index(PyObject* obj, PyObject* index, PyObject* value)
{
#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_object_set_index called for obj %p, index %p, value %p.\n", (void*)obj, (void*)index, (void*)value);
#endif
    if (!obj)
    {
        fprintf(stderr, "TypeError: 'NoneType' object does not support item assignment\n");
        return;
    }
    if (!index)
    {
        fprintf(stderr, "TypeError: subscript indices must be integers, slices, or other valid key types, not 'NoneType'\n");
        return;
    }
    // value can be NULL (representing None)

    // --- Pointer Dereferencing (Optional) ---
    // ...
    // --- End Pointer Dereferencing ---

    // 1. Get the type ID of the target object
    int typeId = py_get_safe_type_id(obj);
#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_object_set_index: obj typeId: %d (%s)\n", typeId, py_type_name(typeId));
#endif

    // 2. Look up the methods for this type ID
    const PyTypeMethods* methods = py_get_type_methods(typeId);
#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_object_set_index: py_get_type_methods(%d) returned: %p\n", typeId, (void*)methods);
    if (methods)
    {
        fprintf(stderr, "DEBUG: py_object_set_index: methods->index_set: %p\n", (void*)methods->index_set);
    }
#endif

    // 3. Check if an index_set method exists and call it
    if (methods && methods->index_set)
    {
        // The handler function is responsible for:
        // - Checking index type validity
        // - Performing bounds/key checks
        // - Handling value type compatibility (if applicable)
        // - Managing reference counts for old/new values
        // - Printing appropriate errors (TypeError, IndexError, KeyError, ValueError).
        methods->index_set(obj, index, value);
        // The handler returns void, errors are printed internally.
    }
    else
    {
        // 4. Handle unsupported types (object does not support item assignment)
        fprintf(stderr, "TypeError: '%s' object does not support item assignment\n", py_type_name(typeId));
        // Consider setting an exception state here.
    }
}

// 索引操作
PyObject* py_object_index(PyObject* obj, PyObject* index)
{
#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_object_index called for obj %p, index %p.\n", (void*)obj, (void*)index);
#endif
    if (!obj)
    {
        // Accessing index on None?
        fprintf(stderr, "TypeError: 'NoneType' object is not subscriptable\n");
        return NULL;
    }
    if (!index)
    {
        fprintf(stderr, "TypeError: subscript indices must be integers, slices, or other valid key types, not 'NoneType'\n");
        return NULL;
    }

    // --- Pointer Dereferencing (Optional, based on your type system) ---
    // PyObject* actual_obj = obj;
    // if (obj->typeId >= 400) { ... dereference ... }
    // PyObject* actual_index = index;
    // if (index->typeId >= 400) { ... dereference ... }
    // For simplicity, assume obj and index are the actual objects for now.
    // --- End Pointer Dereferencing ---

    // 1. Get the type ID of the target object
    int typeId = py_get_safe_type_id(obj);
#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_object_index: obj typeId: %d (%s)\n", typeId, py_type_name(typeId));
#endif

    // 2. Look up the methods for this type ID
    const PyTypeMethods* methods = py_get_type_methods(typeId);
#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_object_index: py_get_type_methods(%d) returned: %p\n", typeId, (void*)methods);
    if (methods)
    {
        fprintf(stderr, "DEBUG: py_object_index: methods->index_get: %p\n", (void*)methods->index_get);
    }
#endif

    // 3. Check if an index_get method exists and call it
    if (methods && methods->index_get)
    {
        // The handler function is responsible for:
        // - Checking index type validity (e.g., int for list, hashable for dict)
        // - Performing bounds/key checks
        // - Returning the found item with an INCREMENTED reference count, or NULL on error.
        // - Printing appropriate errors (TypeError, IndexError, KeyError).
        PyObject* result = methods->index_get(obj, index);
#ifdef DEBUG_RUNTIME_CONTAINER
        fprintf(stderr, "DEBUG: py_object_index: index_get handler returned %p.\n", (void*)result);
#endif
        // The handler should return NULL on error (e.g., IndexError, KeyError, TypeError)
        // and print the specific error message.
        return result;  // Return result directly (already incref'd or NULL)
    }
    else
    {
        // 4. Handle unsupported types (object is not subscriptable)
        fprintf(stderr, "TypeError: '%s' object is not subscriptable\n", py_type_name(typeId));
        return NULL;
    }
}

// 字符串字符访问
PyObject* py_string_get_char(PyObject* strObj, PyObject* indexObj)
{
#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_string_get_char called for str %p, index %p.\n", (void*)strObj, (void*)indexObj);
#endif
    if (!py_check_type(strObj, llvmpy::PY_TYPE_STRING))
    {
        // Should this be TypeError?
        fprintf(stderr, "RuntimeError: py_string_get_char requires a string object, got '%s'\n", py_type_name(py_get_safe_type_id(strObj)));
        return NULL;  // Indicate error
    }

    const char* c_str = py_extract_string(strObj);
    if (!c_str)
    {
        // Should not happen for a valid string object, but handle defensively
        fprintf(stderr, "InternalError: String object has NULL value in py_string_get_char\n");
        return NULL;  // Indicate error
    }
    size_t len = strlen(c_str);

    // 提取 GMP 整数索引
    mpz_ptr idx_mpz = py_extract_int(indexObj);
    long c_index;

    if (!idx_mpz)
    {
        if (py_get_safe_type_id(indexObj) == llvmpy::PY_TYPE_BOOL)
        {
            c_index = py_extract_bool(indexObj) ? 1 : 0;
        }
        else
        {
            fprintf(stderr, "TypeError: string indices must be integers, not '%s'\n",
                    py_type_name(py_get_safe_type_id(indexObj)));
            return NULL;
        }
    }
    else
    {
        if (!mpz_fits_slong_p(idx_mpz))
        {
            fprintf(stderr, "IndexError: cannot fit 'int' index into C long\n");
            return NULL;
        }
        c_index = mpz_get_si(idx_mpz);
    }

#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_string_get_char: Extracted C index: %ld\n", c_index);
#endif

    // 处理负数索引
    if (c_index < 0)
    {
        c_index += len;
    }

    // 边界检查
    if (c_index < 0 || (size_t)c_index >= len)
    {
        fprintf(stderr, "IndexError: string index %ld out of range [0, %zu)\n", c_index, len);
        return NULL;  // Indicate error
    }

    // Create a new string for the single character
    char buf[2] = {c_str[c_index], '\0'};
    PyObject* result = py_create_string(buf);  // py_create_string handles incref for the new string
#ifdef DEBUG_RUNTIME_CONTAINER
    fprintf(stderr, "DEBUG: py_string_get_char: Returning new string '%s' (%p).\n", buf, (void*)result);
#endif
    return result;
}

// 为索引操作设置结果类型元数据的函数 - 安全处理所有类型
void py_set_index_result_type(PyObject* result, int typeId)
{
    if (!result) return;

    // 设置类型ID
    result->typeId = typeId;

// 打印调试信息
#ifdef DEBUG
    fprintf(stderr, "设置索引结果类型: %s (ID: %d)\n", py_type_id_to_string(typeId), typeId);
#endif
}