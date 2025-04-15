#include "RunTime/runtime.h"
#include "Debugdefine.h"
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cstdint> 

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
PyObject* py_list_get_item(PyObject* list, int index)
{
    if (!list)
    {
        fprintf(stderr, "错误: 尝试从NULL列表获取元素\n");
        return py_get_none();
    }

    // 确保是列表类型
    if (list->typeId != llvmpy::PY_TYPE_LIST)
    {
        fprintf(stderr, "类型错误: 对象不是列表 (类型ID: %d)\n", list->typeId);
        return py_get_none();
    }

    PyListObject* listObj = (PyListObject*)list;

    // 边界检查
    if (index < 0 || index >= listObj->length)
    {
        fprintf(stderr, "索引错误: 列表索引 %d 超出范围 [0, %d)\n", index, listObj->length);
        return py_get_none();
    }

    // 增加引用计数并返回
    PyObject* item = listObj->data[index];
    py_incref(item);
    return item;
}

// 设置列表元素
void py_list_set_item(PyObject* obj, int index, PyObject* item)
{
    if (!py_check_type(obj, llvmpy::PY_TYPE_LIST))
    {
        py_type_error(obj, llvmpy::PY_TYPE_LIST);
        return;
    }

    PyListObject* list = (PyListObject*)obj;

    // 索引检查应该基于容量而不是长度
    if (index < 0 || index >= list->capacity)
    {
        fprintf(stderr, "IndexError: list index out of range (capacity: %d, index: %d)\n",
                list->capacity, index);
        return;
    }

    // 类型兼容性检查
    if (list->elemTypeId != 0 && item && !py_are_types_compatible(item->typeId, list->elemTypeId))
    {
        fprintf(stderr, "TypeError: Cannot add type %s to list of %s\n",
                py_type_name(item->typeId), py_type_name(list->elemTypeId));
        return;
    }

    // 替换项目前先减少旧项目的引用计数
    if (list->data[index])
    {
        py_decref(list->data[index]);
    }

    // 设置新元素并增加引用计数
    list->data[index] = item;
    if (item)
    {
        py_incref(item);
    }

    // 如果设置的索引超过当前长度，需要更新长度
    if (index >= list->length)
    {
        list->length = index + 1;
    }
}

// 向列表追加元素
PyObject* py_list_append(PyObject* obj, PyObject* item)
{
    if (!py_check_type(obj, llvmpy::PY_TYPE_LIST))
    {
        py_type_error(obj, llvmpy::PY_TYPE_LIST);
        return NULL;
    }

    PyListObject* list = (PyListObject*)obj;

    // 类型兼容性检查
    if (list->elemTypeId != 0 && item && !py_are_types_compatible(item->typeId, list->elemTypeId))
    {
        fprintf(stderr, "TypeError: Cannot append type %s to list of %s\n",
                py_type_name(item->typeId), py_type_name(list->elemTypeId));
        return obj;
    }

    // 检查是否需要扩展容量
    if (list->length >= list->capacity)
    {
        int newCapacity = list->capacity * 2;
        PyObject** newData = (PyObject**)realloc(list->data, newCapacity * sizeof(PyObject*));

        if (!newData)
        {
            fprintf(stderr, "MemoryError: Failed to expand list capacity\n");
            return obj;
        }

        // 初始化新分配的内存
        for (int i = list->capacity; i < newCapacity; i++)
        {
            newData[i] = NULL;
        }

        list->data = newData;
        list->capacity = newCapacity;
    }

    // 添加新元素并增加引用计数
    list->data[list->length] = item;
    if (item)
    {
        py_incref(item);
    }

    list->length++;

    return obj;
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
PyObject* py_list_get_item_with_type(PyObject* list, int index, int* out_type_id)
{
    if (!list || !out_type_id)
    {
        if (out_type_id) *out_type_id = llvmpy::PY_TYPE_NONE;
        return py_get_none();
    }

    // 确保是列表类型
    if (list->typeId != llvmpy::PY_TYPE_LIST)
    {
        fprintf(stderr, "类型错误: 对象不是列表 (类型ID: %d)\n", list->typeId);
        *out_type_id = llvmpy::PY_TYPE_NONE;
        return py_get_none();
    }

    PyListObject* listObj = (PyListObject*)list;

    // 边界检查
    if (index < 0 || index >= listObj->length)
    {
        fprintf(stderr, "索引错误: 列表索引 %d 超出范围 [0, %d)\n", index, listObj->length);
        *out_type_id = llvmpy::PY_TYPE_NONE;
        return py_get_none();
    }

    // 获取元素
    PyObject* item = listObj->data[index];

    // 设置类型ID
    if (item)
    {
        *out_type_id = item->typeId;

        // 如果列表有元素类型信息但元素没有明确类型，使用列表的元素类型
        if (listObj->elemTypeId > 0 && listObj->elemTypeId != llvmpy::PY_TYPE_ANY)
        {
            *out_type_id = listObj->elemTypeId;
        }
    }
    else
    {
        *out_type_id = llvmpy::PY_TYPE_NONE;
    }

    // 增加引用计数并返回
    if (item) py_incref(item);
    return item ? item : py_get_none();
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
    if (!obj) return 0; // Hash of None or NULL pointer? CPython hashes None.

    int typeId = py_get_safe_type_id(obj);
    const PyTypeMethods* methods = py_get_type_methods(typeId);

    if (methods && methods->hash)
    {
        // 调用类型特定的哈希函数
        return methods->hash(obj);
    }
    else
    {
        // 处理不可哈希类型
        // 检查是否是已知不可哈希类型
        // 实际上这个部分应该交一个专门的机，，例如注册器之类的来处理
        // 但是目前来说还没有那么多复杂的内置类型所以暂时先硬编码这样
        int baseTypeId = llvmpy::getBaseTypeId(typeId);
        if (baseTypeId == llvmpy::PY_TYPE_LIST || baseTypeId == llvmpy::PY_TYPE_DICT) {
             fprintf(stderr, "TypeError: unhashable type: '%s'\n", py_type_name(typeId));
             // 在实际应用中，这里应该设置一个异常状态并可能返回一个特殊值或让调用者处理
             return 0; // 或者一个特定的错误代码/标记
        }

        // 对于其他没有明确 hash 方法的类型，可以默认使用地址作为哈希值，
        // 但这对于值类型（如未来的自定义类实例）可能不合适。
        // 或者更严格地报错。
        // CPython 对没有 __hash__ 的自定义类实例也是不可哈希的，除非继承自实现了 __hash__ 的基类
        // 或者定义了 __eq__ 但没有定义 __hash__=None。
        fprintf(stderr, "Warning: Type '%s' does not have a specific hash method, using address.\n", py_type_name(typeId));
        return (unsigned int)(uintptr_t)obj; // 使用地址作为后备，但这通常不好
    }
}

// 查找字典项
PyDictEntry* py_dict_find_entry(PyDictObject* dict, PyObject* key)
{
    if (!dict || !key) return NULL;

    unsigned int hash = py_hash_object(key);
    unsigned int index = hash % dict->capacity;

    // 线性探测
    for (int i = 0; i < dict->capacity; i++)
    {
        unsigned int idx = (index + i) % dict->capacity;
        PyDictEntry* entry = &dict->entries[idx];

        if (!entry->used)
        {
            // 找到未使用的槽位
            return entry;
        }

        if (entry->hash == hash && entry->key)
        {
            // 比较键是否相等
            PyObject* cmpResult = py_object_compare(key, entry->key, PY_CMP_EQ);
            if (cmpResult)
            {
                bool isEqual = ((PyPrimitiveObject*)cmpResult)->value.boolValue;
                py_decref(cmpResult);

                if (isEqual)
                {
                    return entry;
                }
            }
        }
    }

    // 没有找到合适的条目
    return NULL;
}

// 重新调整字典大小
bool py_dict_resize(PyDictObject* dict)
{
    if (!dict) return false;

    int oldCapacity = dict->capacity;
    PyDictEntry* oldEntries = dict->entries;

    // 新容量为旧容量的两倍
    int newCapacity = oldCapacity * 2;

    // 分配新的条目数组
    PyDictEntry* newEntries = (PyDictEntry*)calloc(newCapacity, sizeof(PyDictEntry));
    if (!newEntries) return false;

    // 初始化新条目
    for (int i = 0; i < newCapacity; i++)
    {
        newEntries[i].key = NULL;
        newEntries[i].value = NULL;
        newEntries[i].hash = 0;
        newEntries[i].used = false;
    }

    // 更新字典
    dict->entries = newEntries;
    dict->capacity = newCapacity;
    dict->size = 0;  // 将在重新插入过程中增加

    // 重新插入所有条目
    for (int i = 0; i < oldCapacity; i++)
    {
        if (oldEntries[i].used && oldEntries[i].key && oldEntries[i].value)
        {
            // 重新插入条目
            py_dict_set_item((PyObject*)dict, oldEntries[i].key, oldEntries[i].value);

            // 减少引用计数，因为py_dict_set_item会增加它们
            py_decref(oldEntries[i].key);
            py_decref(oldEntries[i].value);
        }
    }

    // 释放旧数组
    free(oldEntries);

    return true;
}

// 设置字典项
void py_dict_set_item(PyObject* obj, PyObject* key, PyObject* value)
{
    if (!py_check_type(obj, llvmpy::PY_TYPE_DICT))
    {
        py_type_error(obj, llvmpy::PY_TYPE_DICT);
        return;
    }

    if (!key)
    {
        fprintf(stderr, "KeyError: None is not a valid dictionary key\n");
        return;
    }

    PyDictObject* dict = (PyDictObject*)obj;

    // 类型兼容性检查
    if (dict->keyTypeId != 0 && !py_are_types_compatible(key->typeId, dict->keyTypeId))
    {
        fprintf(stderr, "TypeError: Cannot use type %s as dictionary key for dict with key type %s\n",
                py_type_name(key->typeId), py_type_name(dict->keyTypeId));
        return;
    }

    // 检查是否需要扩容
    if (dict->size >= dict->capacity * 0.75)
    {
        if (!py_dict_resize(dict))
        {
            fprintf(stderr, "MemoryError: Failed to resize dictionary\n");
            return;
        }
    }

    // 计算哈希值
    unsigned int hash = py_hash_object(key);
    unsigned int index = hash % dict->capacity;

    // 查找项目
    bool foundEmptySlot = false;
    int emptySlotIndex = -1;

    // 线性探测
    for (int i = 0; i < dict->capacity; i++)
    {
        unsigned int idx = (index + i) % dict->capacity;
        PyDictEntry* entry = &dict->entries[idx];

        if (!entry->used)
        {
            // 找到未使用的槽位
            if (!foundEmptySlot)
            {
                foundEmptySlot = true;
                emptySlotIndex = idx;
            }
            continue;
        }

        if (entry->hash == hash && entry->key)
        {
            // 比较键是否相等
            PyObject* cmpResult = py_object_compare(key, entry->key, PY_CMP_EQ);
            if (cmpResult)
            {
                bool isEqual = ((PyPrimitiveObject*)cmpResult)->value.boolValue;
                py_decref(cmpResult);

                if (isEqual)
                {
                    // 找到匹配的键，更新值
                    if (entry->value)
                    {
                        py_decref(entry->value);
                    }

                    entry->value = value;
                    if (value)
                    {
                        py_incref(value);
                    }

                    return;
                }
            }
        }
    }

    // 没有找到匹配的键，插入新条目
    if (foundEmptySlot)
    {
        PyDictEntry* entry = &dict->entries[emptySlotIndex];

        entry->key = key;
        entry->value = value;
        entry->hash = hash;
        entry->used = true;

        // 增加引用计数
        if (key) py_incref(key);
        if (value) py_incref(value);

        dict->size++;
    }
    else
    {
        // 这不应该发生，因为我们已经检查了负载因子并进行了扩容
        fprintf(stderr, "Error: Dictionary full, no empty slots found\n");
    }
}

// 获取字典项
PyObject* py_dict_get_item(PyObject* obj, PyObject* key)
{
    if (!py_check_type(obj, llvmpy::PY_TYPE_DICT))
    {
        py_type_error(obj, llvmpy::PY_TYPE_DICT);
        return NULL;
    }

    if (!key)
    {
        fprintf(stderr, "KeyError: None is not a valid dictionary key\n");
        return NULL;
    }

    PyDictObject* dict = (PyDictObject*)obj;

    // 计算哈希值
    unsigned int hash = py_hash_object(key);
    unsigned int index = hash % dict->capacity;

    // 线性探测
    for (int i = 0; i < dict->capacity; i++)
    {
        unsigned int idx = (index + i) % dict->capacity;
        PyDictEntry* entry = &dict->entries[idx];

        if (!entry->used)
        {
            // 找到未使用的槽位，说明键不存在
            break;
        }

        if (entry->hash == hash && entry->key)
        {
            // 比较键是否相等
            PyObject* cmpResult = py_object_compare(key, entry->key, PY_CMP_EQ);
            if (cmpResult)
            {
                bool isEqual = ((PyPrimitiveObject*)cmpResult)->value.boolValue;
                py_decref(cmpResult);

                if (isEqual)
                {
                    // 找到匹配的键，返回值
                    PyObject* value = entry->value;
                    return value;
                }
            }
        }
    }

    // 键不存在
    fprintf(stderr, "KeyError: Key not found in dictionary\n");
    return NULL;
}

// 获取字典值并返回其类型ID - 用于索引操作
PyObject* py_dict_get_item_with_type(PyObject* dict, PyObject* key, int* out_type_id)
{
    if (!dict || !key || !out_type_id)
    {
        if (out_type_id) *out_type_id = llvmpy::PY_TYPE_NONE;
        return py_get_none();
    }

    // 确保是字典类型
    if (dict->typeId != llvmpy::PY_TYPE_DICT)
    {
        fprintf(stderr, "类型错误: 对象不是字典 (类型ID: %d)\n", dict->typeId);
        *out_type_id = llvmpy::PY_TYPE_NONE;
        return py_get_none();
    }

    PyDictObject* dictObj = (PyDictObject*)dict;

    // 查找条目
    PyDictEntry* entry = py_dict_find_entry(dictObj, key);
    if (!entry || !entry->used || !entry->value)
    {
        fprintf(stderr, "警告: 字典中找不到指定的键\n");
        *out_type_id = llvmpy::PY_TYPE_NONE;
        return py_get_none();
    }

    // 设置类型ID
    *out_type_id = entry->value->typeId;

    // 增加引用计数并返回
    py_incref(entry->value);
    return entry->value;
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
        return py_get_none();
    }

    // 解包可能的指针
    PyObject* actual_obj = obj;
    if (obj->typeId >= 400)
    {  // 指针类型范围
        PyObject** ptr_obj = (PyObject**)obj;
        if (ptr_obj && *ptr_obj)
        {
            actual_obj = *ptr_obj;
        }
    }

    // 同样解包索引
    PyObject* actual_index = index;
    if (index && index->typeId >= 400)
    {
        PyObject** ptr_index = (PyObject**)index;
        if (ptr_index && *ptr_index)
        {
            actual_index = *ptr_index;
        }
    }

    // 获取基本类型ID
    int baseTypeId = llvmpy::getBaseTypeId(actual_obj->typeId);

    // 根据对象类型执行相应的索引操作
    switch (baseTypeId)
    {
        case llvmpy::PY_TYPE_LIST:
        {
            PyListObject* list = (PyListObject*)actual_obj;
            PyObject* idxObj = py_extract_int_from_any(actual_index);
            int idx = py_extract_int(idxObj);
            // 使用完后记得减少引用计数
            py_decref(idxObj);

            if (idx < 0 || idx >= list->length)
            {
                fprintf(stderr, "错误: 列表索引越界: %d, 长度: %d\n", idx, list->length);
                *out_type_id = llvmpy::PY_TYPE_NONE;
                return py_get_none();
            }

            PyObject* item = list->data[idx];

            // 确定结果类型
            if (item)
            {
                *out_type_id = item->typeId;

                // 如果是列表且有元素类型，使用特殊的列表类型ID编码
                if (list->elemTypeId > 0 && list->elemTypeId != llvmpy::PY_TYPE_ANY)
                {
                    // 检查是否与列表声明的元素类型匹配
                    if (llvmpy::getBaseTypeId(item->typeId) == llvmpy::getBaseTypeId(list->elemTypeId))
                    {
                        *out_type_id = list->elemTypeId;
                    }
                }
            }
            else
            {
                *out_type_id = llvmpy::PY_TYPE_NONE;
            }

            py_incref(item);
            return item;
        }

        case llvmpy::PY_TYPE_DICT:
        {
            PyDictObject* dict = (PyDictObject*)actual_obj;
            PyDictEntry* entry = py_dict_find_entry(dict, actual_index);

            if (entry && entry->used && entry->value)
            {
                *out_type_id = entry->value->typeId;
                py_incref(entry->value);
                return entry->value;
            }
            else
            {
                fprintf(stderr, "警告: 字典中找不到指定的键\n");
                *out_type_id = llvmpy::PY_TYPE_NONE;
                return py_get_none();
            }
        }

        case llvmpy::PY_TYPE_STRING:
        {
            PyObject* idxObj = py_extract_int_from_any(actual_index);
            int idx = py_extract_int(idxObj);
            // 使用完后记得减少引用计数
            py_decref(idxObj);
            PyPrimitiveObject* strObj = (PyPrimitiveObject*)actual_obj;

            if (!strObj->value.stringValue)
            {
                *out_type_id = llvmpy::PY_TYPE_STRING;
                return py_create_string("");
            }

            size_t len = strlen(strObj->value.stringValue);
            if (idx < 0 || (size_t)idx >= len)
            {
                fprintf(stderr, "警告: 字符串索引越界: %d, 长度: %zu\n", idx, len);
                *out_type_id = llvmpy::PY_TYPE_STRING;
                return py_create_string("");
            }

            char buf[2] = {strObj->value.stringValue[idx], '\0'};
            *out_type_id = llvmpy::PY_TYPE_STRING;
            return py_create_string(buf);
        }

        default:
            fprintf(stderr, "错误: 类型 %d 不支持索引操作\n", actual_obj->typeId);
            *out_type_id = llvmpy::PY_TYPE_ANY;
            return py_get_none();
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
    if (!obj)
    {
        fprintf(stderr, "错误: 试图在 NULL 对象上执行索引赋值\n");
        return;
    }
    if (!index)
    {
        fprintf(stderr, "错误: 索引赋值使用了 NULL 索引\n");
        return;
    }
    // value can be NULL (representing None)

    // 1. Get the type ID of the target object
    // Use py_get_safe_type_id to handle potential NULL obj, though checked above
    int typeId = py_get_safe_type_id(obj);
#ifdef DEBUG_RUNTIME_py_object_set_index
    // --- 添加调试打印 ---
    fprintf(stderr, "DEBUG: py_object_set_index called for obj typeId: %d (%s)\n", typeId, py_type_name(typeId));
// --- 结束调试打印 ---
#endif

    // 2. Look up the methods for this type ID
    const PyTypeMethods* methods = py_get_type_methods(typeId);
#ifdef DEBUG_RUNTIME_py_object_set_index
    // --- 添加调试打印 ---
    fprintf(stderr, "DEBUG: py_get_type_methods(%d) returned: %p\n", typeId, (void*)methods);
    if (methods)
    {
        fprintf(stderr, "DEBUG: methods->index_set: %p\n", (void*)methods->index_set);
    }
// --- 结束调试打印 ---
#endif

    // 3. Check if an index_set method exists and call it
    if (methods && methods->index_set)
    {
        methods->index_set(obj, index, value);
    }
    else
    {
        // 4. Handle unsupported types
        // Use getBaseTypeId for a slightly more general error message if needed
        int baseTypeId = llvmpy::getBaseTypeId(typeId);  // getBaseTypeId 定义在 include/TypeIDs.h
        const PyTypeMethods* base_methods = (baseTypeId != typeId) ? py_get_type_methods(baseTypeId) : NULL;
#ifdef DEBUG_RUNTIME_py_object_set_index
        // --- 添加调试打印 ---
        fprintf(stderr, "DEBUG: No valid index_set handler found directly for typeId %d. Base typeId: %d\n", typeId, baseTypeId);
// --- 结束调试打印 ---
#endif

        if (base_methods && base_methods->index_set)
        {
            // If base type has handler, maybe use it? Or require specific type?
            // For now, require specific type or exact base type match.
            fprintf(stderr, "类型错误: 类型 %s (base: %s) 不支持精确的索引赋值, 但基类支持\n",
                    py_type_name(typeId), py_type_name(baseTypeId));
        }
        else
        {
            fprintf(stderr, "类型错误: 类型 %s 不支持索引赋值\n", py_type_name(typeId));
        }
    }
}

// 索引操作
PyObject* py_object_index(PyObject* obj, PyObject* index)
{
    if (!obj)
    {
        fprintf(stderr, "错误: 试图在NULL对象上执行索引操作\n");
        return py_get_none();
    }

    // 首先解包，确保处理真实对象而不是指针
    PyObject* actual_obj = obj;
    if (obj->typeId >= 400)
    {  // 指针类型范围
        PyObject** ptr_obj = (PyObject**)obj;
        if (ptr_obj && *ptr_obj)
        {
            actual_obj = *ptr_obj;
        }
    }

    // 同样解包索引
    PyObject* actual_index = index;
    if (index && index->typeId >= 400)
    {
        PyObject** ptr_index = (PyObject**)index;
        if (ptr_index && *ptr_index)
        {
            actual_index = *ptr_index;
        }
    }

    // 获取对象的基本类型ID
    int baseTypeId = llvmpy::getBaseTypeId(actual_obj->typeId);

    // 尝试根据对象类型选择适当的索引函数
    switch (baseTypeId)
    {
        case llvmpy::PY_TYPE_LIST:
        {
            PyObject* idxObj = py_extract_int_from_any(actual_index);
            int idx = py_extract_int(idxObj);
            // 使用完后记得减少引用计数
            py_decref(idxObj);
            PyListObject* list = (PyListObject*)actual_obj;

            if (idx < 0 || idx >= list->length)
            {
                fprintf(stderr, "错误: 列表索引越界: %d, 长度: %d\n", idx, list->length);
                return py_get_none();
            }

            PyObject* item = list->data[idx];
            py_incref(item);
            return item;
        }

        case llvmpy::PY_TYPE_DICT:
        {
            PyDictObject* dict = (PyDictObject*)actual_obj;
            PyDictEntry* entry = py_dict_find_entry(dict, actual_index);

            if (entry && entry->used)
            {
                py_incref(entry->value);
                return entry->value;
            }
            else
            {
                fprintf(stderr, "警告: 字典中找不到指定的键\n");
                return py_get_none();
            }
        }

        case llvmpy::PY_TYPE_STRING:
        {
            PyObject* idxObj = py_extract_int_from_any(actual_index);
            int idx = py_extract_int(idxObj);
            // 使用完后记得减少引用计数
            py_decref(idxObj);
            PyPrimitiveObject* strObj = (PyPrimitiveObject*)actual_obj;

            if (!strObj->value.stringValue)
            {
                return py_create_string("");
            }

            size_t len = strlen(strObj->value.stringValue);
            if (idx < 0 || (size_t)idx >= len)
            {
                fprintf(stderr, "警告: 字符串索引越界: %d, 长度: %zu\n", idx, len);
                return py_create_string("");
            }

            char buf[2] = {strObj->value.stringValue[idx], '\0'};
            return py_create_string(buf);
        }

        case llvmpy::PY_TYPE_TUPLE:
        {
            // 假设元组结构与列表相似
            PyObject* idxObj = py_extract_int_from_any(actual_index);
            int idx = py_extract_int(idxObj);
            // 使用完后记得减少引用计数
            py_decref(idxObj);
            PyListObject* tuple = (PyListObject*)actual_obj;  // 使用列表对象结构

            if (idx < 0 || idx >= tuple->length)
            {
                fprintf(stderr, "错误: 元组索引越界: %d, 长度: %d\n", idx, tuple->length);
                return py_get_none();
            }

            PyObject* item = tuple->data[idx];
            py_incref(item);
            return item;
        }

        default:
            fprintf(stderr, "错误: 类型 %d 不支持索引操作\n", actual_obj->typeId);
            return py_get_none();
    }
}

// 字符串字符访问
PyObject* py_string_get_char(PyObject* str, int index)
{
    if (!str || str->typeId != llvmpy::PY_TYPE_STRING)
    {
        fprintf(stderr, "错误: py_string_get_char 需要字符串对象\n");
        return py_create_string("");
    }

    PyPrimitiveObject* strObj = (PyPrimitiveObject*)str;
    if (!strObj->value.stringValue)
    {
        return py_create_string("");
    }

    size_t len = strlen(strObj->value.stringValue);
    if (index < 0 || (size_t)index >= len)
    {
        fprintf(stderr, "警告: 字符串索引越界: %d, 长度: %zu\n", index, len);
        return py_create_string("");
    }

    char buf[2] = {strObj->value.stringValue[index], '\0'};
    return py_create_string(buf);
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