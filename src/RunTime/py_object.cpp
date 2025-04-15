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



PyObject* py_create_class(const char* name, PyObject* base_cls_obj, PyObject* class_dict_obj) {
    // 参数类型检查
    if (!name || !class_dict_obj || class_dict_obj->typeId != llvmpy::PY_TYPE_DICT) {
        fprintf(stderr, "Error: Invalid arguments for py_create_class\n");
        return NULL;
    }
    if (base_cls_obj && base_cls_obj->typeId != llvmpy::PY_TYPE_CLASS) {
         fprintf(stderr, "Error: Base class must be a class object\n");
         return NULL;
    }

    PyClassObject* cls = (PyClassObject*)malloc(sizeof(PyClassObject));
    if (!cls) {
        fprintf(stderr, "MemoryError: Failed to allocate class object\n");
        return NULL;
    }

    cls->header.refCount = 1; // 初始引用计数为 1
    cls->header.typeId = llvmpy::PY_TYPE_CLASS;
    cls->name = strdup(name); // 复制类名
    if (!cls->name) {
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

PyObject* py_create_instance(PyObject* cls_obj) {
    if (!cls_obj || cls_obj->typeId != llvmpy::PY_TYPE_CLASS) {
        fprintf(stderr, "TypeError: Cannot create instance from non-class object\n");
        return NULL;
    }
    PyClassObject* cls = (PyClassObject*)cls_obj;

    PyInstanceObject* instance = (PyInstanceObject*)malloc(sizeof(PyInstanceObject));
    if (!instance) {
        fprintf(stderr, "MemoryError: Failed to allocate instance object\n");
        return NULL;
    }

    instance->header.refCount = 1;
    // TODO: 分配具体的实例类型 ID，可能需要一个全局计数器或更复杂的方案
    // 暂时使用通用的 INSTANCE ID，或者如果类对象存储了实例类型ID，则使用它
    instance->header.typeId = llvmpy::PY_TYPE_INSTANCE; // 或者 >= PY_TYPE_INSTANCE_BASE
    instance->cls = cls;
    instance->instance_dict = (PyDictObject*)py_create_dict(8, llvmpy::PY_TYPE_STRING); // 创建空的实例字典
    if (!instance->instance_dict) {
        free(instance);
        fprintf(stderr, "MemoryError: Failed to create instance dictionary\n");
        return NULL;
    }

    py_incref((PyObject*)instance->cls); // 增加类对象的引用计数

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
PyObject* py_create_int(int value)
{
    PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
    if (!obj)
    {
        fprintf(stderr, "Error: Out of memory\n");
        return NULL;
    }
    obj->header.refCount = 1;
    obj->header.typeId = PY_TYPE_INT;
    obj->value.intValue = value;
    return (PyObject*)obj;
}

// 创建浮点数对象
PyObject* py_create_double(double value)
{
    PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
    if (!obj)
    {
        fprintf(stderr, "Error: Out of memory\n");
        return NULL;
    }
    obj->header.refCount = 1;
    obj->header.typeId = PY_TYPE_DOUBLE;
    obj->value.doubleValue = value;
    return (PyObject*)obj;
}

// 创建布尔对象
PyObject* py_create_bool(bool value)
{
    PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
    if (!obj)
    {
        fprintf(stderr, "Error: Out of memory\n");
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
        obj->refCount++;
    }
}

// 减少对象引用计数，如果减至0则释放对象
void py_decref(PyObject* obj) {
    if (!obj) return;
    obj->refCount--;
    if (obj->refCount == 0) {
        // 根据类型执行清理
        int baseTypeId = llvmpy::getBaseTypeId(obj->typeId); // 使用基类 ID 判断
        switch (baseTypeId) {
            case llvmpy::PY_TYPE_LIST:
                py_list_decref_items((PyListObject*)obj); // 释放列表元素
                free(((PyListObject*)obj)->data);         // 释放数据数组
                free(obj);
                break;
            case llvmpy::PY_TYPE_DICT:
                // 释放字典条目中的键和值
                for (int i = 0; i < ((PyDictObject*)obj)->capacity; i++) {
                    PyDictEntry* entry = &((PyDictObject*)obj)->entries[i];
                    if (entry->used) {
                        py_decref(entry->key);
                        py_decref(entry->value);
                    }
                }
                free(((PyDictObject*)obj)->entries); // 释放条目数组
                free(obj);
                break;
            case llvmpy::PY_TYPE_STRING:
                free(((PyPrimitiveObject*)obj)->value.stringValue); // 释放字符串内存
                free(obj);
                break;
            // --- 新增: 处理类和实例 ---
            case llvmpy::PY_TYPE_CLASS:
                {
                    PyClassObject* cls = (PyClassObject*)obj;
                    free((void*)cls->name); // 释放 strdup 分配的内存
                    py_decref((PyObject*)cls->base); // 减少基类引用
                    py_decref((PyObject*)cls->class_dict); // 减少类字典引用
                    free(obj);
                }
                break;
            case llvmpy::PY_TYPE_INSTANCE: // 或检查 >= PY_TYPE_INSTANCE_BASE
                {
                    PyInstanceObject* instance = (PyInstanceObject*)obj;
                    py_decref((PyObject*)instance->cls); // 减少类引用
                    py_decref((PyObject*)instance->instance_dict); // 减少实例字典引用
                    free(obj);
                }
                break;
            // --- End 新增 ---
            case llvmpy::PY_TYPE_INT:
            case llvmpy::PY_TYPE_DOUBLE:
            case llvmpy::PY_TYPE_BOOL:
            case llvmpy::PY_TYPE_NONE: // None 通常是单例，不应被释放，但以防万一
            default: // 包含其他基本类型和未知类型
                free(obj);
                break;
        }
    }
}

//===----------------------------------------------------------------------===//
// 对象复制实现
//===----------------------------------------------------------------------===//

// 深度复制对象
PyObject* py_object_copy(PyObject* obj, int typeId)
{
    if (!obj) return NULL;
    
    // 使用传入的类型ID或对象自身的类型ID
    int actualTypeId = typeId > 0 ? typeId : obj->typeId;
    int baseTypeId = getBaseTypeId(actualTypeId);
    
    switch (baseTypeId)
    {
        case PY_TYPE_INT:
        {
            int value = ((PyPrimitiveObject*)obj)->value.intValue;
            return py_create_int(value);
        }
        
        case PY_TYPE_DOUBLE:
        {
            double value = ((PyPrimitiveObject*)obj)->value.doubleValue;
            return py_create_double(value);
        }
        
        case PY_TYPE_BOOL:
        {
            bool value = ((PyPrimitiveObject*)obj)->value.boolValue;
            return py_create_bool(value);
        }
        
        case PY_TYPE_STRING:
        {
            const char* value = ((PyPrimitiveObject*)obj)->value.stringValue;
            return py_create_string(value);
        }
        
        case PY_TYPE_LIST:
        {
            PyListObject* srcList = (PyListObject*)obj;
            PyObject* newListObj = py_create_list(srcList->capacity, srcList->elemTypeId);
            if (!newListObj) return NULL;
            
            PyListObject* newList = (PyListObject*)newListObj;
            
            // 复制元素
            for (int i = 0; i < srcList->length; i++)
            {
                PyObject* srcItem = srcList->data[i];
                if (srcItem)
                {
                    // 深度复制每个元素
                    PyObject* newItem = py_object_copy(srcItem, srcItem->typeId);
                    if (!newItem)
                    {
                        py_decref(newListObj);
                        return NULL;
                    }
                    
                    newList->data[i] = newItem;
                }
                else
                {
                    newList->data[i] = NULL;
                }
            }
            
            newList->length = srcList->length;
            return newListObj;
        }
        
        case PY_TYPE_DICT:
        {
            PyDictObject* srcDict = (PyDictObject*)obj;
            PyObject* newDictObj = py_create_dict(srcDict->capacity, srcDict->keyTypeId);
            if (!newDictObj) return NULL;
            
            PyDictObject* newDict = (PyDictObject*)newDictObj;
            
            // 复制条目
            for (int i = 0; i < srcDict->capacity; i++)
            {
                if (srcDict->entries[i].used && srcDict->entries[i].key)
                {
                    // 深度复制键和值
                    PyObject* newKey = py_object_copy(srcDict->entries[i].key, srcDict->entries[i].key->typeId);
                    if (!newKey)
                    {
                        py_decref(newDictObj);
                        return NULL;
                    }
                    
                    PyObject* newValue = NULL;
                    if (srcDict->entries[i].value)
                    {
                        newValue = py_object_copy(srcDict->entries[i].value, srcDict->entries[i].value->typeId);
                        if (!newValue)
                        {
                            py_decref(newKey);
                            py_decref(newDictObj);
                            return NULL;
                        }
                    }
                    
                    // 计算哈希值
                    unsigned int hash = py_hash_object(newKey);
                    
                    // 在新字典中找到合适的位置
                    unsigned int index = hash % newDict->capacity;
                    bool found = false;
                    
                    // 线性探测
                    for (int j = 0; j < newDict->capacity; j++)
                    {
                        unsigned int idx = (index + j) % newDict->capacity;
                        if (!newDict->entries[idx].used)
                        {
                            // 找到未使用的槽位
                            newDict->entries[idx].key = newKey;
                            newDict->entries[idx].value = newValue;
                            newDict->entries[idx].hash = hash;
                            newDict->entries[idx].used = true;
                            found = true;
                            break;
                        }
                    }
                    
                    if (!found)
                    {
                        // 应该不会到这里，因为新字典的容量与原字典相同
                        fprintf(stderr, "Error: Failed to insert item during dictionary copy\n");
                        py_decref(newKey);
                        if (newValue) py_decref(newValue);
                        py_decref(newDictObj);
                        return NULL;
                    }
                    
                    newDict->size++;
                }
            }
            
            return newDictObj;
        }
        
        case PY_TYPE_NONE:
            return py_get_none();
            
        default:
            fprintf(stderr, "Warning: Copying unsupported type %d\n", baseTypeId);
            return NULL;
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

