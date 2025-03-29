#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
// 添加extern "C"声明，防止C++名称修饰
#ifdef __cplusplus
extern "C" {
#endif
//===----------------------------------------------------------------------===//
// 基础工具函数实现
//===----------------------------------------------------------------------===//

// print函数实现
void print(char* str) {
    printf("%s\n", str);
}

// 打印数字的函数
void print_number(double num) {
    printf("%g\n", num);
}

// input函数实现
char* input() {
    char* buffer = (char*)malloc(1024);
    if (!buffer) return NULL;
    
    if (fgets(buffer, 1024, stdin)) {
        // 移除换行符
        buffer[strcspn(buffer, "\n")] = 0;
        return buffer;
    }
    
    free(buffer);
    return NULL;
}

//===----------------------------------------------------------------------===//
// 运行时类型定义
//===----------------------------------------------------------------------===//

// 对象类型ID
enum PyTypeId {
    PY_TYPE_NONE = 0,
    PY_TYPE_INT = 1,
    PY_TYPE_DOUBLE = 2,
    PY_TYPE_BOOL = 3,
    PY_TYPE_STRING = 4,
    PY_TYPE_LIST = 5,
    PY_TYPE_DICT = 6
};

// 基本PyObject结构
typedef struct {
    int refCount;    // 引用计数
    int typeId;      // 类型ID
} PyObject;

// 列表对象
typedef struct {
    PyObject header;     // PyObject头
    int length;          // 当前长度
    int capacity;        // 分配容量
    int elemTypeId;      // 元素类型ID
    PyObject** data;     // 元素数据指针数组
} PyListObject;

// 字典条目
typedef struct {
    PyObject* key;
    PyObject* value;
    int hash;
    bool used;
} PyDictEntry;

// 字典对象
typedef struct {
    PyObject header;      // PyObject头
    int size;             // 条目数量
    int capacity;         // 哈希表容量
    int keyTypeId;        // 键类型ID
    PyDictEntry* entries; // 哈希表
} PyDictObject;

// 基本类型包装
typedef struct {
    PyObject header;
    union {
        int intValue;
        double doubleValue;
        bool boolValue;
        char* stringValue;
    } value;
} PyPrimitiveObject;

//===----------------------------------------------------------------------===//
// 通用对象操作函数
//===----------------------------------------------------------------------===//

// 增加对象引用计数
void py_incref(PyObject* obj) {
    if (obj) {
        obj->refCount++;
    }
}

// 减少对象引用计数，如果减至0则释放对象
void py_decref(PyObject* obj) {
    if (!obj) return;
    
    obj->refCount--;
    
    if (obj->refCount <= 0) {
        // 根据类型进行清理
        switch (obj->typeId) {
            case PY_TYPE_STRING: {
                PyPrimitiveObject* strObj = (PyPrimitiveObject*)obj;
                free(strObj->value.stringValue);
                break;
            }
            
            case PY_TYPE_LIST: {
                PyListObject* listObj = (PyListObject*)obj;
                // 减少所有元素的引用计数
                for (int i = 0; i < listObj->length; i++) {
                    if (listObj->data[i]) {
                        py_decref(listObj->data[i]);
                    }
                }
                free(listObj->data);
                break;
            }
            
            case PY_TYPE_DICT: {
                PyDictObject* dictObj = (PyDictObject*)obj;
                // 减少所有键值对的引用计数
                for (int i = 0; i < dictObj->capacity; i++) {
                    if (dictObj->entries[i].used) {
                        if (dictObj->entries[i].key) {
                            py_decref(dictObj->entries[i].key);
                        }
                        if (dictObj->entries[i].value) {
                            py_decref(dictObj->entries[i].value);
                        }
                    }
                }
                free(dictObj->entries);
                break;
            }
            
            // 其他类型不需要特殊处理
            default:
                break;
        }
        
        // 释放对象本身
        free(obj);
    }
}

// 创建一个新的基本对象
static PyObject* py_create_basic_object(int typeId) {
    PyObject* obj = (PyObject*)malloc(sizeof(PyObject));
    if (!obj) return NULL;
    
    obj->refCount = 1;
    obj->typeId = typeId;
    
    return obj;
}

// 创建一个整数对象
PyObject* py_create_int(int value) {
    PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
    if (!obj) return NULL;
    
    obj->header.refCount = 1;
    obj->header.typeId = PY_TYPE_INT;
    obj->value.intValue = value;
    
    return (PyObject*)obj;
}

// 创建一个浮点数对象
PyObject* py_create_double(double value) {
    PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
    if (!obj) return NULL;
    
    obj->header.refCount = 1;
    obj->header.typeId = PY_TYPE_DOUBLE;
    obj->value.doubleValue = value;
    
    return (PyObject*)obj;
}

// 创建一个布尔对象
PyObject* py_create_bool(bool value) {
    PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
    if (!obj) return NULL;
    
    obj->header.refCount = 1;
    obj->header.typeId = PY_TYPE_BOOL;
    obj->value.boolValue = value;
    
    return (PyObject*)obj;
}

// 创建一个字符串对象
PyObject* py_create_string(const char* value) {
    if (!value) return NULL;
    
    PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
    if (!obj) return NULL;
    
    obj->header.refCount = 1;
    obj->header.typeId = PY_TYPE_STRING;
    obj->value.stringValue = strdup(value);
    
    if (!obj->value.stringValue) {
        free(obj);
        return NULL;
    }
    
    return (PyObject*)obj;
}
//===----------------------------------------------------------------------===//
// 运行时运算
//===----------------------------------------------------------------------===//



// 对象加法
PyObject* py_object_add(PyObject* a, PyObject* b) {
    if (!a || !b) return NULL;
    
    // 整数加法
    if (a->typeId == PY_TYPE_INT && b->typeId == PY_TYPE_INT) {
        PyPrimitiveObject* aInt = (PyPrimitiveObject*)a;
        PyPrimitiveObject* bInt = (PyPrimitiveObject*)b;
        return py_create_int(aInt->value.intValue + bInt->value.intValue);
    }
    
    // 浮点数加法
    if ((a->typeId == PY_TYPE_INT || a->typeId == PY_TYPE_DOUBLE) && 
        (b->typeId == PY_TYPE_INT || b->typeId == PY_TYPE_DOUBLE)) {
        double aVal = a->typeId == PY_TYPE_INT ? 
            ((PyPrimitiveObject*)a)->value.intValue : 
            ((PyPrimitiveObject*)a)->value.doubleValue;
        double bVal = b->typeId == PY_TYPE_INT ? 
            ((PyPrimitiveObject*)b)->value.intValue : 
            ((PyPrimitiveObject*)b)->value.doubleValue;
        return py_create_double(aVal + bVal);
    }
    
    // 字符串连接
    if (a->typeId == PY_TYPE_STRING && b->typeId == PY_TYPE_STRING) {
        PyPrimitiveObject* aStr = (PyPrimitiveObject*)a;
        PyPrimitiveObject* bStr = (PyPrimitiveObject*)b;
        
        if (!aStr->value.stringValue || !bStr->value.stringValue) {
            return py_create_string("");
        }
        
        size_t len1 = strlen(aStr->value.stringValue);
        size_t len2 = strlen(bStr->value.stringValue);
        char* result = (char*)malloc(len1 + len2 + 1);
        
        if (!result) return NULL;
        
        strcpy(result, aStr->value.stringValue);
        strcat(result, bStr->value.stringValue);
        
        PyObject* strObj = py_create_string(result);
        free(result);
        return strObj;
    }
    
    // 不支持的操作
    fprintf(stderr, "TypeError: unsupported operand type(s) for +: '%d' and '%d'\n", 
            a->typeId, b->typeId);
    return NULL;
}


//===----------------------------------------------------------------------===//
// 列表操作函数
//===----------------------------------------------------------------------===//

// 创建一个新的列表对象
PyObject* py_create_list(int size, int elemTypeId) {
    PyListObject* list = (PyListObject*)malloc(sizeof(PyListObject));
    if (!list) return NULL;
    
    list->header.refCount = 1;
    list->header.typeId = PY_TYPE_LIST;
    list->length = 0;
    list->capacity = size > 0 ? size : 8; // 默认初始容量为8
    list->elemTypeId = elemTypeId;
    
    // 分配元素数组
    list->data = (PyObject**)malloc(sizeof(PyObject*) * list->capacity);
    if (!list->data) {
        free(list);
        return NULL;
    }
    
    // 初始化为NULL
    for (int i = 0; i < list->capacity; i++) {
        list->data[i] = NULL;
    }
    
    return (PyObject*)list;
}

// 获取列表长度
int py_list_len(PyObject* listObj) {
    if (!listObj || listObj->typeId != PY_TYPE_LIST) {
        return 0;
    }
    
    PyListObject* list = (PyListObject*)listObj;
    return list->length;
}

// 确保列表有足够的容量
static bool py_list_ensure_capacity(PyListObject* list, int minCapacity) {
    if (list->capacity >= minCapacity) {
        return true;
    }
    
    // 计算新容量 (至少是当前的两倍)
    int newCapacity = list->capacity * 2;
    if (newCapacity < minCapacity) {
        newCapacity = minCapacity;
    }
    
    // 重新分配内存
    PyObject** newData = (PyObject**)realloc(list->data, sizeof(PyObject*) * newCapacity);
    if (!newData) {
        return false;
    }
    
    // 初始化新分配的元素为NULL
    for (int i = list->capacity; i < newCapacity; i++) {
        newData[i] = NULL;
    }
    
    list->data = newData;
    list->capacity = newCapacity;
    
    return true;
}

// 获取列表元素
PyObject* py_list_get_item(PyObject* listObj, int index) {
    if (!listObj || listObj->typeId != PY_TYPE_LIST) {
        return NULL;
    }
    
    PyListObject* list = (PyListObject*)listObj;
    
    // 范围检查
    if (index < 0 || index >= list->length) {
        fprintf(stderr, "IndexError: list index %d out of range (0 to %d)\n", 
                index, list->length - 1);
        return NULL;
    }
    
    PyObject* item = list->data[index];
    
    // 增加返回对象的引用计数
    if (item) {
        py_incref(item);
    }
    
    return item;
}

// 设置列表元素
void py_list_set_item(PyObject* listObj, int index, PyObject* item) {
    if (!listObj || listObj->typeId != PY_TYPE_LIST) {
        return;
    }
    
    PyListObject* list = (PyListObject*)listObj;
    
    // 如果索引超出当前长度但在容量范围内，则扩展列表
    if (index >= 0 && index < list->capacity) {
        // 如果是替换现有元素，减少旧元素的引用计数
        if (index < list->length && list->data[index]) {
            py_decref(list->data[index]);
        }
        
        // 设置新元素并增加其引用计数
        list->data[index] = item;
        if (item) {
            py_incref(item);
        }
        
        // 更新列表长度
        if (index >= list->length) {
            list->length = index + 1;
        }
    } else if (index >= 0) {
        // 需要扩展容量
        if (!py_list_ensure_capacity(list, index + 1)) {
            fprintf(stderr, "Error: Failed to resize list\n");
            return;
        }
        
        // 设置新元素并增加其引用计数
        list->data[index] = item;
        if (item) {
            py_incref(item);
        }
        
        // 更新列表长度
        list->length = index + 1;
    } else {
        fprintf(stderr, "IndexError: list index %d out of range\n", index);
    }
}

// 向列表末尾添加元素
PyObject* py_list_append(PyObject* listObj, PyObject* item) {
    if (!listObj || listObj->typeId != PY_TYPE_LIST) {
        return NULL;
    }
    
    PyListObject* list = (PyListObject*)listObj;
    
    // 确保有足够的容量
    if (!py_list_ensure_capacity(list, list->length + 1)) {
        fprintf(stderr, "Error: Failed to resize list\n");
        return NULL;
    }
    
    // 添加元素并增加其引用计数
    py_list_set_item(listObj, list->length, item);
    
    // 返回列表本身 (增加引用计数)
    py_incref(listObj);
    return listObj;
}

// 创建列表的拷贝
PyObject* py_list_copy(PyObject* listObj) {
    if (!listObj || listObj->typeId != PY_TYPE_LIST) {
        return NULL;
    }
    
    PyListObject* srcList = (PyListObject*)listObj;
    
    // 创建一个相同大小的新列表
    PyObject* newListObj = py_create_list(srcList->length, srcList->elemTypeId);
    if (!newListObj) {
        return NULL;
    }
    
    PyListObject* newList = (PyListObject*)newListObj;
    
    // 复制每个元素
    for (int i = 0; i < srcList->length; i++) {
        if (srcList->data[i]) {
            py_list_set_item(newListObj, i, srcList->data[i]);
        }
    }
    
    return newListObj;
}

//===----------------------------------------------------------------------===//
// 哈希表和字典操作函数
//===----------------------------------------------------------------------===//

// 计算哈希值
int py_hash(PyObject* obj) {
    if (!obj) return 0;
    
    switch (obj->typeId) {
        case PY_TYPE_INT: {
            PyPrimitiveObject* intObj = (PyPrimitiveObject*)obj;
            return intObj->value.intValue;
        }
        
        case PY_TYPE_STRING: {
            PyPrimitiveObject* strObj = (PyPrimitiveObject*)obj;
            // 简单字符串哈希函数
            const char* str = strObj->value.stringValue;
            int hash = 0;
            while (str && *str) {
                hash = hash * 31 + *str++;
            }
            return hash;
        }
        
        default:
            // 对于其他类型，使用指针地址作为哈希值
            return (intptr_t)obj;
    }
}

// 比较两个对象是否相等
bool py_objects_equal(PyObject* a, PyObject* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->typeId != b->typeId) return false;
    
    switch (a->typeId) {
        case PY_TYPE_INT: {
            PyPrimitiveObject* aInt = (PyPrimitiveObject*)a;
            PyPrimitiveObject* bInt = (PyPrimitiveObject*)b;
            return aInt->value.intValue == bInt->value.intValue;
        }
        
        case PY_TYPE_DOUBLE: {
            PyPrimitiveObject* aDouble = (PyPrimitiveObject*)a;
            PyPrimitiveObject* bDouble = (PyPrimitiveObject*)b;
            return aDouble->value.doubleValue == bDouble->value.doubleValue;
        }
        
        case PY_TYPE_BOOL: {
            PyPrimitiveObject* aBool = (PyPrimitiveObject*)a;
            PyPrimitiveObject* bBool = (PyPrimitiveObject*)b;
            return aBool->value.boolValue == bBool->value.boolValue;
        }
        
        case PY_TYPE_STRING: {
            PyPrimitiveObject* aStr = (PyPrimitiveObject*)a;
            PyPrimitiveObject* bStr = (PyPrimitiveObject*)b;
            return strcmp(aStr->value.stringValue, bStr->value.stringValue) == 0;
        }
        
        default:
            // 对于其他类型，只有指针相同才相等
            return false;
    }
}

// 创建字典
PyObject* py_create_dict(int initialCapacity, int keyTypeId) {
    // 确保容量至少为8，并且是2的幂
    if (initialCapacity < 8) {
        initialCapacity = 8;
    } else {
        // 向上取整到2的幂
        int power = 8;
        while (power < initialCapacity) {
            power *= 2;
        }
        initialCapacity = power;
    }
    
    PyDictObject* dict = (PyDictObject*)malloc(sizeof(PyDictObject));
    if (!dict) return NULL;
    
    dict->header.refCount = 1;
    dict->header.typeId = PY_TYPE_DICT;
    dict->size = 0;
    dict->capacity = initialCapacity;
    dict->keyTypeId = keyTypeId;
    
    // 分配哈希表
    dict->entries = (PyDictEntry*)malloc(sizeof(PyDictEntry) * initialCapacity);
    if (!dict->entries) {
        free(dict);
        return NULL;
    }
    
    // 初始化哈希表
    for (int i = 0; i < initialCapacity; i++) {
        dict->entries[i].key = NULL;
        dict->entries[i].value = NULL;
        dict->entries[i].hash = 0;
        dict->entries[i].used = false;
    }
    
    return (PyObject*)dict;
}

// 获取字典条目的索引
static int py_dict_find_slot(PyDictObject* dict, PyObject* key) {
    int hash = py_hash(key);
    int index = hash % dict->capacity;
    int originalIndex = index;
    
    // 线性探测
    while (true) {
        // 找到空槽位
        if (!dict->entries[index].used) {
            return index;
        }
        
        // 找到匹配的键
        if (dict->entries[index].used && py_objects_equal(dict->entries[index].key, key)) {
            return index;
        }
        
        // 继续探测
        index = (index + 1) % dict->capacity;
        
        // 已经探测了一圈，没有找到位置
        if (index == originalIndex) {
            // 字典已满，无法插入
            return -1;
        }
    }
}

// 扩展字典容量
static bool py_dict_resize(PyDictObject* dict) {
    int oldCapacity = dict->capacity;
    PyDictEntry* oldEntries = dict->entries;
    
    // 新容量是旧容量的两倍
    int newCapacity = oldCapacity * 2;
    
    // 分配新的哈希表
    PyDictEntry* newEntries = (PyDictEntry*)malloc(sizeof(PyDictEntry) * newCapacity);
    if (!newEntries) {
        return false;
    }
    
    // 初始化新哈希表
    for (int i = 0; i < newCapacity; i++) {
        newEntries[i].key = NULL;
        newEntries[i].value = NULL;
        newEntries[i].hash = 0;
        newEntries[i].used = false;
    }
    
    // 更新字典容量和哈希表
    dict->capacity = newCapacity;
    dict->entries = newEntries;
    dict->size = 0; // 暂时将大小设为0，然后重新插入所有键值对
    
    // 重新插入所有键值对
    for (int i = 0; i < oldCapacity; i++) {
        if (oldEntries[i].used) {
            // 查找新的插入位置
            int index = py_dict_find_slot(dict, oldEntries[i].key);
            if (index < 0) {
                // 无法插入，这不应该发生
                fprintf(stderr, "Error: Failed to resize dictionary\n");
                
                // 恢复旧状态
                free(newEntries);
                dict->capacity = oldCapacity;
                dict->entries = oldEntries;
                return false;
            }
            
            // 将键值对复制到新位置
            dict->entries[index].key = oldEntries[i].key;
            dict->entries[index].value = oldEntries[i].value;
            dict->entries[index].hash = oldEntries[i].hash;
            dict->entries[index].used = true;
            dict->size++;
        }
    }
    
    // 释放旧哈希表
    free(oldEntries);
    
    return true;
}

// 获取字典项
PyObject* py_dict_get_item(PyObject* dictObj, PyObject* key) {
    if (!dictObj || dictObj->typeId != PY_TYPE_DICT || !key) {
        return NULL;
    }
    
    PyDictObject* dict = (PyDictObject*)dictObj;
    
    // 查找键的位置
    int index = py_dict_find_slot(dict, key);
    if (index < 0 || !dict->entries[index].used) {
        return NULL; // 键不存在
    }
    
    // 返回值并增加引用计数
    PyObject* value = dict->entries[index].value;
    if (value) {
        py_incref(value);
    }
    
    return value;
}

// 设置字典项
void py_dict_set_item(PyObject* dictObj, PyObject* key, PyObject* value) {
    if (!dictObj || dictObj->typeId != PY_TYPE_DICT || !key) {
        return;
    }
    
    PyDictObject* dict = (PyDictObject*)dictObj;
    
    // 检查是否需要扩容
    if (dict->size >= dict->capacity * 0.75) {
        if (!py_dict_resize(dict)) {
            fprintf(stderr, "Error: Failed to resize dictionary\n");
            return;
        }
    }
    
    // 查找键的位置
    int index = py_dict_find_slot(dict, key);
    if (index < 0) {
        fprintf(stderr, "Error: Dictionary is full\n");
        return;
    }
    
    // 如果是替换现有键值对
    if (dict->entries[index].used) {
        // 减少旧值的引用计数
        if (dict->entries[index].value) {
            py_decref(dict->entries[index].value);
        }
    } else {
        // 新增键值对
        dict->entries[index].used = true;
        dict->entries[index].hash = py_hash(key);
        
        // 增加键的引用计数
        dict->entries[index].key = key;
        py_incref(key);
        
        // 增加字典大小
        dict->size++;
    }
    
    // 设置新值并增加其引用计数
    dict->entries[index].value = value;
    if (value) {
        py_incref(value);
    }
}

//===----------------------------------------------------------------------===//
// 对象复制与转换函数
//===----------------------------------------------------------------------===//

// 通用对象复制函数
PyObject* py_object_copy(PyObject* obj, int typeId) {
    if (!obj) return NULL;
    
    // 检查对象类型是否匹配
    if (obj->typeId != typeId) {
        fprintf(stderr, "TypeError: Expected type %d, got %d\n", typeId, obj->typeId);
        return NULL;
    }
    
    // 根据类型执行不同的复制操作
    switch (obj->typeId) {
        case PY_TYPE_INT: {
            PyPrimitiveObject* intObj = (PyPrimitiveObject*)obj;
            return py_create_int(intObj->value.intValue);
        }
        
        case PY_TYPE_DOUBLE: {
            PyPrimitiveObject* doubleObj = (PyPrimitiveObject*)obj;
            return py_create_double(doubleObj->value.doubleValue);
        }
        
        case PY_TYPE_BOOL: {
            PyPrimitiveObject* boolObj = (PyPrimitiveObject*)obj;
            return py_create_bool(boolObj->value.boolValue);
        }
        
        case PY_TYPE_STRING: {
            PyPrimitiveObject* strObj = (PyPrimitiveObject*)obj;
            return py_create_string(strObj->value.stringValue);
        }
        
        case PY_TYPE_LIST: {
            // 使用列表的专用复制函数
            return py_list_copy(obj);
        }
        
        default:
            // 对于不支持复制的类型，增加引用计数并返回原对象
            py_incref(obj);
            return obj;
    }
}

// 类型转换函数: int -> double
PyObject* py_convert_int_to_double(PyObject* intObj) {
    if (!intObj || intObj->typeId != PY_TYPE_INT) {
        return NULL;
    }
    
    PyPrimitiveObject* intPrimitive = (PyPrimitiveObject*)intObj;
    return py_create_double((double)intPrimitive->value.intValue);
}

// 类型转换函数: double -> int
PyObject* py_convert_double_to_int(PyObject* doubleObj) {
    if (!doubleObj || doubleObj->typeId != PY_TYPE_DOUBLE) {
        return NULL;
    }
    
    PyPrimitiveObject* doublePrimitive = (PyPrimitiveObject*)doubleObj;
    return py_create_int((int)doublePrimitive->value.doubleValue);
}

// 类型转换函数: int/double -> bool
PyObject* py_convert_to_bool(PyObject* obj) {
    if (!obj) return py_create_bool(false);
    
    switch (obj->typeId) {
        case PY_TYPE_INT: {
            PyPrimitiveObject* intObj = (PyPrimitiveObject*)obj;
            return py_create_bool(intObj->value.intValue != 0);
        }
        
        case PY_TYPE_DOUBLE: {
            PyPrimitiveObject* doubleObj = (PyPrimitiveObject*)obj;
            return py_create_bool(doubleObj->value.doubleValue != 0.0);
        }
        
        case PY_TYPE_BOOL: {
            // 已经是布尔类型，创建拷贝
            PyPrimitiveObject* boolObj = (PyPrimitiveObject*)obj;
            return py_create_bool(boolObj->value.boolValue);
        }
        
        case PY_TYPE_STRING: {
            PyPrimitiveObject* strObj = (PyPrimitiveObject*)obj;
            // 空字符串为false，非空为true
            return py_create_bool(strObj->value.stringValue && strObj->value.stringValue[0] != '\0');
        }
        
        case PY_TYPE_LIST: {
            PyListObject* listObj = (PyListObject*)obj;
            // 空列表为false，非空为true
            return py_create_bool(listObj->length > 0);
        }
        
        default:
            // 默认非NULL对象为true
            return py_create_bool(true);
    }
}

//===----------------------------------------------------------------------===//
// 调试和打印函数
//===----------------------------------------------------------------------===//

// 打印对象信息
void py_print_object(PyObject* obj) {
    if (!obj) {
        printf("None\n");
        return;
    }
    
    switch (obj->typeId) {
        case PY_TYPE_INT: {
            PyPrimitiveObject* intObj = (PyPrimitiveObject*)obj;
            printf("%d\n", intObj->value.intValue);
            break;
        }
        
        case PY_TYPE_DOUBLE: {
            PyPrimitiveObject* doubleObj = (PyPrimitiveObject*)obj;
            printf("%g\n", doubleObj->value.doubleValue);
            break;
        }
        
        case PY_TYPE_BOOL: {
            PyPrimitiveObject* boolObj = (PyPrimitiveObject*)obj;
            printf("%s\n", boolObj->value.boolValue ? "True" : "False");
            break;
        }
        
        case PY_TYPE_STRING: {
            PyPrimitiveObject* strObj = (PyPrimitiveObject*)obj;
            printf("%s\n", strObj->value.stringValue ? strObj->value.stringValue : "");
            break;
        }
        
        case PY_TYPE_LIST: {
            PyListObject* listObj = (PyListObject*)obj;
            printf("[");
            for (int i = 0; i < listObj->length; i++) {
                if (i > 0) printf(", ");
                if (listObj->data[i]) {
                    // 简化打印，不递归调用以避免循环引用
                    printf("item");
                } else {
                    printf("None");
                }
            }
            printf("]\n");
            break;
        }
        
        case PY_TYPE_DICT: {
            PyDictObject* dictObj = (PyDictObject*)obj;
            printf("{...%d items...}\n", dictObj->size);
            break;
        }
        
        default:
            printf("<unknown object>\n");
            break;
    }
}

// 打印对象引用计数和类型信息 (用于调试)
void py_debug_object(PyObject* obj) {
    if (!obj) {
        printf("NULL object\n");
        return;
    }
    
    printf("Object at %p: typeId=%d, refCount=%d\n", 
           (void*)obj, obj->typeId, obj->refCount);
}


// 注意：在文件末尾关闭extern "C"块
#ifdef __cplusplus
}
#endif