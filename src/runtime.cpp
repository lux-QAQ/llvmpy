
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdbool>
#include <cmath>
#include <climits>

#include "TypeIDs.h"

// 添加extern "C"声明，防止C++名称修饰
#ifdef __cplusplus
extern "C"
{
#endif

    // 添加结构体前向声明
    typedef struct PyObject_t PyObject;
    typedef struct PyListObject_t PyListObject;
    typedef struct PyDictEntry_t PyDictEntry;
    typedef struct PyDictObject_t PyDictObject;
    typedef struct PyPrimitiveObject_t PyPrimitiveObject;

    // 对象创建函数
    PyObject* py_create_int(int value);
    PyObject* py_create_double(double value);
    PyObject* py_create_bool(bool value);
    PyObject* py_create_string(const char* value);
    PyObject* py_create_list(int size, int elemTypeId);
    PyObject* py_create_dict(int initialCapacity, int keyTypeId);

    // 对象操作函数
    PyObject* py_object_copy(PyObject* obj, int typeId);
    void py_incref(PyObject* obj);
    void py_decref(PyObject* obj);

    // 列表操作函数
    PyObject* py_list_copy(PyObject* obj);
    void py_list_set_item(PyObject* obj, int index, PyObject* item);
    PyObject* py_list_get_item(PyObject* obj, int index);
    PyObject* py_list_append(PyObject* obj, PyObject* item);
    int py_list_len(PyObject* obj);
    void py_list_decref_items(PyListObject* list);

    // 字典操作函数
    void py_dict_set_item(PyObject* obj, PyObject* key, PyObject* value);
    PyObject* py_dict_get_item(PyObject* obj, PyObject* key);
    PyObject* py_dict_keys(PyObject* obj);
    int py_dict_len(PyObject* obj);
    bool py_dict_resize(PyDictObject* dict);
    PyDictEntry* py_dict_find_entry(PyDictObject* dict, PyObject* key);
    unsigned int py_hash_object(PyObject* obj);

    // 类型检查和转换函数
    bool py_check_type(PyObject* obj, int expectedTypeId);
    const char* py_type_name(int typeId);
    void py_type_error(PyObject* obj, int expectedTypeId);
    PyObject* py_ensure_type(PyObject* obj, int expectedTypeId);
    int py_get_base_type_id(int typeId);
    bool py_are_types_compatible(int typeIdA, int typeIdB);
    bool py_ensure_type_compatibility(PyObject* obj, int expectedTypeId);
    bool py_check_list_element_type(PyListObject* list, PyObject* item);

    // 比较函数前向声明
    typedef enum
    {
        PY_CMP_EQ = 0,  // ==
        PY_CMP_NE = 1,  // !=
        PY_CMP_LT = 2,  // <
        PY_CMP_LE = 3,  // <=
        PY_CMP_GT = 4,  // >
        PY_CMP_GE = 5   // >=
    } PyCompareOp;

    PyObject* py_object_compare(PyObject* a, PyObject* b, PyCompareOp op);
    bool py_compare_eq(PyObject* a, PyObject* b);
    bool py_compare_ne(PyObject* a, PyObject* b);

    // 类型提取函数
    int py_extract_int(PyObject* obj);
    double py_extract_double(PyObject* obj);
    bool py_extract_bool(PyObject* obj);
    const char* py_extract_string(PyObject* obj);

    // 类型转换函数
    PyObject* py_convert_int_to_double(PyObject* obj);
    PyObject* py_convert_double_to_int(PyObject* obj);
    PyObject* py_convert_to_bool(PyObject* obj);
    PyObject* py_convert_to_string(PyObject* obj);
    bool py_coerce_numeric(PyObject* a, PyObject* b, double* out_a, double* out_b);

    // 运算函数
    PyObject* py_object_add(PyObject* a, PyObject* b);
    PyObject* py_object_subtract(PyObject* a, PyObject* b);
    PyObject* py_object_multiply(PyObject* a, PyObject* b);
    PyObject* py_object_divide(PyObject* a, PyObject* b);
    PyObject* py_object_modulo(PyObject* a, PyObject* b);
    PyObject* py_object_negate(PyObject* a);
    PyObject* py_object_not(PyObject* a);
    PyObject* py_object_compare(PyObject* a, PyObject* b, PyCompareOp op);

    //===----------------------------------------------------------------------===//
    // 基础工具函数实现
    //===----------------------------------------------------------------------===//

    // print函数实现
    void print(char* str)
    {
        printf("%s\n", str);
    }

    // 打印数字的函数
    void print_number(double num)
    {
        printf("%g\n", num);
    }

    // input函数实现
    char* input()
    {
        char* buffer = (char*)malloc(1024);
        if (!buffer) return NULL;

        if (fgets(buffer, 1024, stdin))
        {
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

    // 基本PyObject结构
    struct PyObject_t
    {
        int refCount;  // 引用计数
        int typeId;    // 类型ID
    };

    // 列表对象
    struct PyListObject_t
    {
        PyObject header;  // PyObject头
        int length;       // 当前长度
        int capacity;     // 分配容量
        int elemTypeId;   // 元素类型ID
        PyObject** data;  // 元素数据指针数组
    };

    // 字典条目
    struct PyDictEntry_t
    {
        PyObject* key;
        PyObject* value;
        int hash;
        bool used;
    };

    // 字典对象
    struct PyDictObject_t
    {
        PyObject header;       // PyObject头
        int size;              // 条目数量
        int capacity;          // 哈希表容量
        int keyTypeId;         // 键类型ID
        PyDictEntry* entries;  // 哈希表
    };

    // 基本类型包装
    struct PyPrimitiveObject_t
    {
        PyObject header;
        union
        {
            int intValue;
            double doubleValue;
            bool boolValue;
            char* stringValue;
        } value;
    };

    //===----------------------------------------------------------------------===//
    // 类型检查和转换辅助函数
    //===----------------------------------------------------------------------===//

    // 检查对象类型是否匹配
    bool py_check_type(PyObject* obj, int expectedTypeId)
    {
        if (!obj) return false;

        // 使用基本类型ID进行比较 - 处理复合类型的情况
        int objBaseTypeId = llvmpy::getBaseTypeId(obj->typeId);
        int expectedBaseTypeId = llvmpy::getBaseTypeId(expectedTypeId);

        return objBaseTypeId == expectedBaseTypeId;
    }

    // 获取类型名称
    const char* py_type_name(int typeId)
    {
        switch (llvmpy::getBaseTypeId(typeId))
        {
            case llvmpy::PY_TYPE_NONE:
                return "None";
            case llvmpy::PY_TYPE_INT:
                return "int";
            case llvmpy::PY_TYPE_DOUBLE:
                return "float";
            case llvmpy::PY_TYPE_BOOL:
                return "bool";
            case llvmpy::PY_TYPE_STRING:
                return "str";
            case llvmpy::PY_TYPE_LIST:
                return "list";
            case llvmpy::PY_TYPE_DICT:
                return "dict";
            default:
                return "unknown";
        }
    }

    // 报告类型错误
    void py_type_error(PyObject* obj, int expectedTypeId)
    {
        fprintf(stderr, "TypeError: Expected type %d (%s), got %d (%s)\n",
                expectedTypeId, py_type_name(expectedTypeId),
                obj ? obj->typeId : -1, obj ? py_type_name(obj->typeId) : "None");
    }

    // 检查类型，如果不匹配则返回错误信息和NULL
    PyObject* py_ensure_type(PyObject* obj, int expectedTypeId)
    {
        if (!obj)
        {
            fprintf(stderr, "TypeError: Object is NULL\n");
            return NULL;
        }

        // 使用基本类型ID进行比较 - 处理复合类型的情况
        if (!py_check_type(obj, expectedTypeId))
        {
            py_type_error(obj, expectedTypeId);
            return NULL;
        }

        return obj;
    }

    // 获取基本类型ID - 使用统一的 TypeIDs.h 函数
    int py_get_base_type_id(int typeId)
    {
        return llvmpy::getBaseTypeId(typeId);
    }

    // 检查两个类型是否兼容
    bool py_are_types_compatible(int typeIdA, int typeIdB)
    {
        // 如果类型完全匹配
        if (typeIdA == typeIdB) return true;

        // 如果包含数值类型，它们互相兼容
        bool aIsNumeric = (typeIdA == llvmpy::PY_TYPE_INT || typeIdA == llvmpy::PY_TYPE_DOUBLE);
        bool bIsNumeric = (typeIdB == llvmpy::PY_TYPE_INT || typeIdB == llvmpy::PY_TYPE_DOUBLE);
        if (aIsNumeric && bIsNumeric) return true;

        // 使用基本类型ID进行比较 - 去除高位标记
        int baseA = llvmpy::getBaseTypeId(typeIdA);
        int baseB = llvmpy::getBaseTypeId(typeIdB);

        // 如果基本类型相同
        return baseA == baseB;
    }

    //===----------------------------------------------------------------------===//
    // 通用对象操作函数
    //===----------------------------------------------------------------------===//

    // 增加对象引用计数
    void py_incref(PyObject* obj)
    {
        if (obj)
        {
            obj->refCount++;
        }
    }

    // 减少对象引用计数，如果减至0则释放对象
    void py_decref(PyObject* obj)
    {
        if (!obj) return;

        obj->refCount--;

        if (obj->refCount <= 0)
        {
            // 根据类型进行清理
            switch (llvmpy::getBaseTypeId(obj->typeId))
            {
                case llvmpy::PY_TYPE_STRING:
                {
                    PyPrimitiveObject* strObj = (PyPrimitiveObject*)obj;
                    if (strObj->value.stringValue)
                    {
                        free(strObj->value.stringValue);
                    }
                    break;
                }

                case llvmpy::PY_TYPE_LIST:
                {
                    PyListObject* listObj = (PyListObject*)obj;
                    // 减少所有元素的引用计数
                    for (int i = 0; i < listObj->length; i++)
                    {
                        if (listObj->data[i])
                        {
                            py_decref(listObj->data[i]);
                        }
                    }
                    free(listObj->data);
                    break;
                }

                case llvmpy::PY_TYPE_DICT:
                {
                    PyDictObject* dictObj = (PyDictObject*)obj;
                    // 减少所有键值对的引用计数
                    for (int i = 0; i < dictObj->capacity; i++)
                    {
                        if (dictObj->entries[i].used)
                        {
                            if (dictObj->entries[i].key)
                            {
                                py_decref(dictObj->entries[i].key);
                            }
                            if (dictObj->entries[i].value)
                            {
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

    // 深度复制对象
    PyObject* py_object_copy(PyObject* obj, int typeId)
    {
        if (!obj) return NULL;

        // 根据类型进行不同的复制操作
        switch (llvmpy::getBaseTypeId(obj->typeId))
        {
            case llvmpy::PY_TYPE_INT:
            {
                PyPrimitiveObject* intObj = (PyPrimitiveObject*)obj;
                return py_create_int(intObj->value.intValue);
            }

            case llvmpy::PY_TYPE_DOUBLE:
            {
                PyPrimitiveObject* doubleObj = (PyPrimitiveObject*)obj;
                return py_create_double(doubleObj->value.doubleValue);
            }

            case llvmpy::PY_TYPE_BOOL:
            {
                PyPrimitiveObject* boolObj = (PyPrimitiveObject*)obj;
                return py_create_bool(boolObj->value.boolValue);
            }

            case llvmpy::PY_TYPE_STRING:
            {
                PyPrimitiveObject* strObj = (PyPrimitiveObject*)obj;
                return py_create_string(strObj->value.stringValue);
            }

            case llvmpy::PY_TYPE_LIST:
            {
                // 使用列表复制函数
                return py_list_copy(obj);
            }

            case llvmpy::PY_TYPE_DICT:
            {
                // 创建新字典并复制内容
                PyDictObject* dictObj = (PyDictObject*)obj;
                PyObject* newDict = py_create_dict(dictObj->capacity, dictObj->keyTypeId);
                if (!newDict) return NULL;

                // 复制所有条目
                for (int i = 0; i < dictObj->capacity; i++)
                {
                    if (dictObj->entries[i].used && dictObj->entries[i].key && dictObj->entries[i].value)
                    {
                        // 复制键和值
                        PyObject* keyCopy = py_object_copy(dictObj->entries[i].key, dictObj->entries[i].key->typeId);
                        PyObject* valueCopy = py_object_copy(dictObj->entries[i].value, dictObj->entries[i].value->typeId);

                        // 设置到新字典
                        py_dict_set_item(newDict, keyCopy, valueCopy);

                        // 减少临时对象的引用计数
                        py_decref(keyCopy);
                        py_decref(valueCopy);
                    }
                }

                return newDict;
            }

            default:
                fprintf(stderr, "TypeError: Cannot copy object of type %s\n", py_type_name(obj->typeId));
                return NULL;
        }
    }

    // 创建一个新的基本对象
    static PyObject* py_create_basic_object(int typeId)
    {
        PyObject* obj = (PyObject*)malloc(sizeof(PyObject));
        if (!obj) return NULL;

        obj->refCount = 1;
        obj->typeId = typeId;

        return obj;
    }

    // 创建一个整数对象
    PyObject* py_create_int(int value)
    {
        PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
        if (!obj) return NULL;

        obj->header.refCount = 1;
        obj->header.typeId = llvmpy::PY_TYPE_INT;
        obj->value.intValue = value;

        return (PyObject*)obj;
    }

    // 创建一个浮点数对象
    PyObject* py_create_double(double value)
    {
        PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
        if (!obj) return NULL;

        obj->header.refCount = 1;
        obj->header.typeId = llvmpy::PY_TYPE_DOUBLE;
        obj->value.doubleValue = value;

        return (PyObject*)obj;
    }

    // 创建一个布尔对象
    PyObject* py_create_bool(bool value)
    {
        PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
        if (!obj) return NULL;

        obj->header.refCount = 1;
        obj->header.typeId = llvmpy::PY_TYPE_BOOL;
        obj->value.boolValue = value;

        return (PyObject*)obj;
    }

    // 创建一个字符串对象
    PyObject* py_create_string(const char* value)
    {
        if (!value) return NULL;

        PyPrimitiveObject* obj = (PyPrimitiveObject*)malloc(sizeof(PyPrimitiveObject));
        if (!obj) return NULL;

        obj->header.refCount = 1;
        obj->header.typeId = llvmpy::PY_TYPE_STRING;
        obj->value.stringValue = strdup(value);

        if (!obj->value.stringValue)
        {
            free(obj);
            return NULL;
        }

        return (PyObject*)obj;
    }

    //===----------------------------------------------------------------------===//
    // 类型提取和转换函数
    //===----------------------------------------------------------------------===//

    // 从整数对象提取整数值
    int py_extract_int(PyObject* obj)
    {
        if (!obj)
        {
            fprintf(stderr, "TypeError: Cannot extract int from NULL\n");
            return 0;
        }

        if (obj->typeId == llvmpy::PY_TYPE_INT)
        {
            return ((PyPrimitiveObject*)obj)->value.intValue;
        }
        else if (obj->typeId == llvmpy::PY_TYPE_DOUBLE)
        {
            // 允许从浮点数提取整数值
            return (int)((PyPrimitiveObject*)obj)->value.doubleValue;
        }
        else if (obj->typeId == llvmpy::PY_TYPE_BOOL)
        {
            // 允许从布尔值提取整数值
            return ((PyPrimitiveObject*)obj)->value.boolValue ? 1 : 0;
        }
        else
        {
            py_type_error(obj, llvmpy::PY_TYPE_INT);
            return 0;
        }
    }

    // 从浮点对象提取浮点值
    double py_extract_double(PyObject* obj)
    {
        if (!obj)
        {
            fprintf(stderr, "TypeError: Cannot extract double from NULL\n");
            return 0.0;
        }

        if (obj->typeId == llvmpy::PY_TYPE_DOUBLE)
        {
            return ((PyPrimitiveObject*)obj)->value.doubleValue;
        }
        else if (obj->typeId == llvmpy::PY_TYPE_INT)
        {
            // 允许从整数提取浮点值
            return (double)((PyPrimitiveObject*)obj)->value.intValue;
        }
        else if (obj->typeId == llvmpy::PY_TYPE_BOOL)
        {
            // 允许从布尔值提取浮点值
            return ((PyPrimitiveObject*)obj)->value.boolValue ? 1.0 : 0.0;
        }
        else
        {
            py_type_error(obj, llvmpy::PY_TYPE_DOUBLE);
            return 0.0;
        }
    }

    // 从布尔对象提取布尔值
    bool py_extract_bool(PyObject* obj)
    {
        if (!obj)
        {
            fprintf(stderr, "TypeError: Cannot extract bool from NULL\n");
            return false;
        }

        if (obj->typeId == llvmpy::PY_TYPE_BOOL)
        {
            return ((PyPrimitiveObject*)obj)->value.boolValue;
        }
        else if (obj->typeId == llvmpy::PY_TYPE_INT)
        {
            // 允许从整数提取布尔值
            return ((PyPrimitiveObject*)obj)->value.intValue != 0;
        }
        else if (obj->typeId == llvmpy::PY_TYPE_DOUBLE)
        {
            // 允许从浮点数提取布尔值
            return ((PyPrimitiveObject*)obj)->value.doubleValue != 0.0;
        }
        else if (obj->typeId == llvmpy::PY_TYPE_STRING)
        {
            // 空字符串为false，非空为true
            const char* str = ((PyPrimitiveObject*)obj)->value.stringValue;
            return str && str[0] != '\0';
        }
        else if (obj->typeId == llvmpy::PY_TYPE_LIST)
        {
            // 空列表为false，非空为true
            return ((PyListObject*)obj)->length > 0;
        }
        else if (obj->typeId == llvmpy::PY_TYPE_DICT)
        {
            // 空字典为false，非空为true
            return ((PyDictObject*)obj)->size > 0;
        }
        else
        {
            py_type_error(obj, llvmpy::PY_TYPE_BOOL);
            return false;
        }
    }

    // 从字符串对象提取字符串
    const char* py_extract_string(PyObject* obj)
    {
        if (!obj)
        {
            fprintf(stderr, "TypeError: Cannot extract string from NULL\n");
            return "";
        }

        if (obj->typeId == llvmpy::PY_TYPE_STRING)
        {
            return ((PyPrimitiveObject*)obj)->value.stringValue;
        }
        else
        {
            py_type_error(obj, llvmpy::PY_TYPE_STRING);
            return "";
        }
    }

    //===----------------------------------------------------------------------===//
    // 类型转换函数
    //===----------------------------------------------------------------------===//

    // 将整数对象转换为浮点数对象
    PyObject* py_convert_int_to_double(PyObject* obj)
    {
        if (!obj) return NULL;

        if (obj->typeId == llvmpy::PY_TYPE_INT)
        {
            int value = ((PyPrimitiveObject*)obj)->value.intValue;
            return py_create_double((double)value);
        }

        // 如果已经是浮点数，直接返回副本
        if (obj->typeId == llvmpy::PY_TYPE_DOUBLE)
        {
            return py_object_copy(obj, obj->typeId);
        }

        py_type_error(obj, llvmpy::PY_TYPE_INT);
        return NULL;
    }

    // 将浮点数对象转换为整数对象
    PyObject* py_convert_double_to_int(PyObject* obj)
    {
        if (!obj) return NULL;

        if (obj->typeId == llvmpy::PY_TYPE_DOUBLE)
        {
            double value = ((PyPrimitiveObject*)obj)->value.doubleValue;
            return py_create_int((int)value);
        }

        // 如果已经是整数，直接返回副本
        if (obj->typeId == llvmpy::PY_TYPE_INT)
        {
            return py_object_copy(obj, obj->typeId);
        }

        py_type_error(obj, llvmpy::PY_TYPE_DOUBLE);
        return NULL;
    }

    // 将对象转换为布尔对象
    PyObject* py_convert_to_bool(PyObject* obj)
    {
        if (!obj) return py_create_bool(false);

        bool boolValue = py_extract_bool(obj);
        return py_create_bool(boolValue);
    }

    // 将对象转换为字符串对象
    PyObject* py_convert_to_string(PyObject* obj)
    {
        if (!obj) return py_create_string("None");

        char buffer[256];

        switch (obj->typeId)
        {
            case llvmpy::PY_TYPE_INT:
            {
                int value = ((PyPrimitiveObject*)obj)->value.intValue;
                snprintf(buffer, sizeof(buffer), "%d", value);
                return py_create_string(buffer);
            }

            case llvmpy::PY_TYPE_DOUBLE:
            {
                double value = ((PyPrimitiveObject*)obj)->value.doubleValue;
                snprintf(buffer, sizeof(buffer), "%g", value);
                return py_create_string(buffer);
            }

            case llvmpy::PY_TYPE_BOOL:
            {
                bool value = ((PyPrimitiveObject*)obj)->value.boolValue;
                return py_create_string(value ? "True" : "False");
            }

            case llvmpy::PY_TYPE_STRING:
            {
                return py_object_copy(obj, obj->typeId);
            }

            case llvmpy::PY_TYPE_LIST:
            {
                return py_create_string("<list>");
            }

            case llvmpy::PY_TYPE_DICT:
            {
                return py_create_string("<dict>");
            }

            default:
                return py_create_string("<unknown object>");
        }
    }

    //===----------------------------------------------------------------------===//
    // 运行时运算函数，支持类型自动转换
    //===----------------------------------------------------------------------===//

    // 自动将两个操作数转换为相同的数值类型
    bool py_coerce_numeric(PyObject* a, PyObject* b, double* out_a, double* out_b)
    {
        if (!a || !b) return false;

        // 如果两个对象都是数值类型，提取它们的值
        bool aIsNumeric = (a->typeId == llvmpy::PY_TYPE_INT || a->typeId == llvmpy::PY_TYPE_DOUBLE || a->typeId == llvmpy::PY_TYPE_BOOL);
        bool bIsNumeric = (b->typeId == llvmpy::PY_TYPE_INT || b->typeId == llvmpy::PY_TYPE_DOUBLE || b->typeId == llvmpy::PY_TYPE_BOOL);

        if (aIsNumeric && bIsNumeric)
        {
            // 从a中提取数值
            if (a->typeId == llvmpy::PY_TYPE_INT)
            {
                *out_a = (double)((PyPrimitiveObject*)a)->value.intValue;
            }
            else if (a->typeId == llvmpy::PY_TYPE_DOUBLE)
            {
                *out_a = ((PyPrimitiveObject*)a)->value.doubleValue;
            }
            else
            {  // PY_TYPE_BOOL
                *out_a = ((PyPrimitiveObject*)a)->value.boolValue ? 1.0 : 0.0;
            }

            // 从b中提取数值
            if (b->typeId == llvmpy::PY_TYPE_INT)
            {
                *out_b = (double)((PyPrimitiveObject*)b)->value.intValue;
            }
            else if (b->typeId == llvmpy::PY_TYPE_DOUBLE)
            {
                *out_b = ((PyPrimitiveObject*)b)->value.doubleValue;
            }
            else
            {  // PY_TYPE_BOOL
                *out_b = ((PyPrimitiveObject*)b)->value.boolValue ? 1.0 : 0.0;
            }

            return true;
        }

        return false;
    }

    // 对象加法 - 支持类型自动转换
    PyObject* py_object_add(PyObject* a, PyObject* b)
    {
        if (!a || !b) return NULL;

        // 处理数值类型
        double a_val, b_val;
        if (py_coerce_numeric(a, b, &a_val, &b_val))
        {
            // 判断结果类型：如果任一操作数是浮点数，则结果是浮点数
            if (a->typeId == llvmpy::PY_TYPE_DOUBLE || b->typeId == llvmpy::PY_TYPE_DOUBLE)
            {
                return py_create_double(a_val + b_val);
            }
            else
            {
                // 两个整数相加产生整数
                return py_create_int((int)a_val + (int)b_val);
            }
        }

        // 字符串连接
        if (a->typeId == llvmpy::PY_TYPE_STRING && b->typeId == llvmpy::PY_TYPE_STRING)
        {
            PyPrimitiveObject* aStr = (PyPrimitiveObject*)a;
            PyPrimitiveObject* bStr = (PyPrimitiveObject*)b;

            if (!aStr->value.stringValue || !bStr->value.stringValue)
            {
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
        fprintf(stderr, "TypeError: unsupported operand type(s) for +: '%s' and '%s'\n",
                py_type_name(a->typeId), py_type_name(b->typeId));
        return NULL;
    }

    // 对象减法
    PyObject* py_object_subtract(PyObject* a, PyObject* b)
    {
        double a_val, b_val;
        if (py_coerce_numeric(a, b, &a_val, &b_val))
        {
            // 判断结果类型
            if (a->typeId == llvmpy::PY_TYPE_DOUBLE || b->typeId == llvmpy::PY_TYPE_DOUBLE)
            {
                return py_create_double(a_val - b_val);
            }
            else
            {
                return py_create_int((int)a_val - (int)b_val);
            }
        }

        // 不支持的操作
        fprintf(stderr, "TypeError: unsupported operand type(s) for -: '%s' and '%s'\n",
                py_type_name(a->typeId), py_type_name(b->typeId));
        return NULL;
    }

    // 对象乘法
    PyObject* py_object_multiply(PyObject* a, PyObject* b)
    {
        if (!a || !b) return NULL;

        // 数值乘法
        double a_val, b_val;
        if (py_coerce_numeric(a, b, &a_val, &b_val))
        {
            // 判断结果类型
            if (a->typeId == llvmpy::PY_TYPE_DOUBLE || b->typeId == llvmpy::PY_TYPE_DOUBLE)
            {
                return py_create_double(a_val * b_val);
            }
            else
            {
                return py_create_int((int)a_val * (int)b_val);
            }
        }

        // 字符串重复 (str * int)
        if (a->typeId == llvmpy::PY_TYPE_STRING && b->typeId == llvmpy::PY_TYPE_INT)
        {
            PyPrimitiveObject* strObj = (PyPrimitiveObject*)a;
            int repeat = ((PyPrimitiveObject*)b)->value.intValue;

            if (repeat <= 0 || !strObj->value.stringValue)
            {
                return py_create_string("");
            }

            size_t len = strlen(strObj->value.stringValue);
            size_t totalLen = len * repeat;

            // 检查重复次数以防止内存爆炸
            if (totalLen > 1000000)
            {
                fprintf(stderr, "Error: String repetition too large\n");
                return NULL;
            }

            char* result = (char*)malloc(totalLen + 1);

            if (!result) return NULL;

            result[0] = '\0';
            for (int i = 0; i < repeat; i++)
            {
                strcat(result, strObj->value.stringValue);
            }

            PyObject* newStr = py_create_string(result);
            free(result);
            return newStr;
        }

        // 整数重复字符串 (int * str) - 为了符合Python语义
        if (a->typeId == llvmpy::PY_TYPE_INT && b->typeId == llvmpy::PY_TYPE_STRING)
        {
            // 调用字符串重复，交换参数
            return py_object_multiply(b, a);
        }

        // 不支持的操作
        fprintf(stderr, "TypeError: unsupported operand type(s) for *: '%s' and '%s'\n",
                py_type_name(a->typeId), py_type_name(b->typeId));
        return NULL;
    }

    // 对象除法
    PyObject* py_object_divide(PyObject* a, PyObject* b)
    {
        double a_val, b_val;
        if (py_coerce_numeric(a, b, &a_val, &b_val))
        {
            if (b_val == 0.0)
            {
                fprintf(stderr, "ZeroDivisionError: division by zero\n");
                return NULL;
            }
            // 除法总是返回浮点数
            return py_create_double(a_val / b_val);
        }

        // 不支持的操作
        fprintf(stderr, "TypeError: unsupported operand type(s) for /: '%s' and '%s'\n",
                py_type_name(a->typeId), py_type_name(b->typeId));
        return NULL;
    }

    // 对象取模
    PyObject* py_object_modulo(PyObject* a, PyObject* b)
    {
        if (!a || !b) return NULL;

        // 只支持整数取模
        if (a->typeId == llvmpy::PY_TYPE_INT && b->typeId == llvmpy::PY_TYPE_INT)
        {
            int a_val = ((PyPrimitiveObject*)a)->value.intValue;
            int b_val = ((PyPrimitiveObject*)b)->value.intValue;

            if (b_val == 0)
            {
                fprintf(stderr, "ZeroDivisionError: modulo by zero\n");
                return NULL;
            }

            return py_create_int(a_val % b_val);
        }

        // 不支持的操作
        fprintf(stderr, "TypeError: unsupported operand type(s) for %%: '%s' and '%s'\n",
                py_type_name(a->typeId), py_type_name(b->typeId));
        return NULL;
    }

    // 一元取负
    PyObject* py_object_negate(PyObject* a)
    {
        if (!a) return NULL;

        if (a->typeId == llvmpy::PY_TYPE_INT)
        {
            int val = ((PyPrimitiveObject*)a)->value.intValue;
            return py_create_int(-val);
        }

        if (a->typeId == llvmpy::PY_TYPE_DOUBLE)
        {
            double val = ((PyPrimitiveObject*)a)->value.doubleValue;
            return py_create_double(-val);
        }

        if (a->typeId == llvmpy::PY_TYPE_BOOL)
        {
            bool val = ((PyPrimitiveObject*)a)->value.boolValue;
            return py_create_int(val ? -1 : 0);
        }

        // 不支持的操作
        fprintf(stderr, "TypeError: bad operand type for unary -: '%s'\n",
                py_type_name(a->typeId));
        return NULL;
    }

    // 逻辑非
    PyObject* py_object_not(PyObject* a)
    {
        if (!a) return py_create_bool(true);  // None 被视为 False

        bool result = !py_extract_bool(a);
        return py_create_bool(result);
    }
    // 在已有的函数声明后添加（但在 extern "C" 范围内）

    //===----------------------------------------------------------------------===//
    // 打印函数实现
    //===----------------------------------------------------------------------===//

    // 打印 Python 对象
    void py_print_object(PyObject* obj)
{
    // 验证参数是否为NULL
    if (!obj)
    {
        printf("None\n");
        return;
    }

    // 增强内存地址有效性检查
    if ((uintptr_t)obj < 0x1000 || (uintptr_t)obj > 0x7FFFFFFFFFFFFFFF)
    {
        printf("%d (possible integer value)\n", (int)(intptr_t)obj);
        return;
    }

#ifdef __cplusplus
    // 使用更强大的异常处理保护
    try
    {
        // 尝试安全验证typeId可以安全访问
        volatile int typeIdCheck = 0;
        try {
            typeIdCheck = obj->typeId;
        } catch (...) {
            // 如果访问typeId失败，可能是整数值被错误传递
            printf("%d (direct integer value)\n", (int)(intptr_t)obj);
            return;
        }

        int typeId = obj->typeId;

        // 检查typeId是否在合理范围内
        if (typeId < 0 || typeId > 1000)
        {
            // 这很可能是一个整数值被错误地当作指针传入
            printf("%d (likely direct integer value)\n", (int)(intptr_t)obj);
            return;
        }

        // 增强安全的打印逻辑
        switch (llvmpy::getBaseTypeId(typeId))
        {
            case llvmpy::PY_TYPE_INT:
            {
                PyPrimitiveObject* intObj = (PyPrimitiveObject*)obj;
                printf("%d\n", intObj->value.intValue);
                break;
            }
            case llvmpy::PY_TYPE_DOUBLE:
            {
                PyPrimitiveObject* doubleObj = (PyPrimitiveObject*)obj;
                printf("%g\n", doubleObj->value.doubleValue);
                break;
            }
            case llvmpy::PY_TYPE_BOOL:
            {
                PyPrimitiveObject* boolObj = (PyPrimitiveObject*)obj;
                printf("%s\n", boolObj->value.boolValue ? "True" : "False");
                break;
            }
            case llvmpy::PY_TYPE_STRING:
            {
                PyPrimitiveObject* strObj = (PyPrimitiveObject*)obj;
                if (strObj && strObj->value.stringValue)
                    printf("%s\n", strObj->value.stringValue);
                else
                    printf("(empty string)\n");
                break;
            }
            case llvmpy::PY_TYPE_LIST:
            {
                PyListObject* listObj = (PyListObject*)obj;
                if (!listObj) {
                    printf("[]\n");
                    break;
                }
                
                printf("[");
                for (int i = 0; i < listObj->length; i++)
                {
                    if (i > 0) printf(", ");
                    PyObject* elem = listObj->data[i];
                    if (elem)
                    {
                        // 安全打印元素 - 避免递归调用可能引起的栈溢出
                        int elemTypeId = 0;
                        try {
                            elemTypeId = elem->typeId;
                        } catch (...) {
                            printf("<invalid>");
                            continue;
                        }
                        
                        switch (llvmpy::getBaseTypeId(elemTypeId))
                        {
                            case llvmpy::PY_TYPE_STRING:
                                printf("\"%s\"", ((PyPrimitiveObject*)elem)->value.stringValue ? 
                                    ((PyPrimitiveObject*)elem)->value.stringValue : "");
                                break;
                            case llvmpy::PY_TYPE_INT:
                                printf("%d", ((PyPrimitiveObject*)elem)->value.intValue);
                                break;
                            case llvmpy::PY_TYPE_DOUBLE:
                                printf("%g", ((PyPrimitiveObject*)elem)->value.doubleValue);
                                break;
                            case llvmpy::PY_TYPE_BOOL:
                                printf("%s", ((PyPrimitiveObject*)elem)->value.boolValue ? "True" : "False");
                                break;
                            default:
                                printf("<%s object>", py_type_name(elemTypeId));
                                break;
                        }
                    }
                    else
                    {
                        printf("None");
                    }
                }
                printf("]\n");
                break;
            }
            case llvmpy::PY_TYPE_DICT:
            {
                printf("{...}\n");  // 简化的字典打印
                break;
            }
            default:
                printf("<%s object at %p>\n", py_type_name(typeId), (void*)obj);
                break;
        }
    }
    catch (...)
    {
        // 更全面的异常处理
        printf("<Error accessing object at %p>\n", (void*)obj);

        // 尝试直接把指针当作整数值打印
        if ((uintptr_t)obj <= INT_MAX)
        {
            printf("Possible integer value: %d\n", (int)(intptr_t)obj);
        }
    }
#else
// 非C++编译环境下的原代码保持不变
// ...
#endif
}

    // 打印整数
    void py_print_int(int value)
    {
        printf("%d\n", value);
    }

    // 打印浮点数
    void py_print_double(double value)
    {
        printf("%g\n", value);
    }

    // 打印布尔值
    void py_print_bool(bool value)
    {
        printf("%s\n", value ? "True" : "False");
    }

    // 打印字符串
    void py_print_string(const char* value)
    {
        printf("%s\n", value ? value : "");
    }
    //===----------------------------------------------------------------------===//
    // 对象比较函数
    //===----------------------------------------------------------------------===//

    // PyTypeId 枚举（确保添加到 TypeIDs.h）
    // 比较操作码

    // 通用比较函数
    PyObject* py_object_compare(PyObject* a, PyObject* b, PyCompareOp op)
    {
        if (!a || !b)
        {
            // NULL 对象只与 NULL 或自身相等
            bool isEqual = (a == b);

            switch (op)
            {
                case PY_CMP_EQ:
                    return py_create_bool(isEqual);
                case PY_CMP_NE:
                    return py_create_bool(!isEqual);
                // 其他比较运算返回 false
                default:
                    return py_create_bool(false);
            }
        }

        // 如果是同一个对象，直接比较相等性
        if (a == b)
        {
            switch (op)
            {
                case PY_CMP_EQ:
                case PY_CMP_LE:
                case PY_CMP_GE:
                    return py_create_bool(true);
                case PY_CMP_NE:
                case PY_CMP_LT:
                case PY_CMP_GT:
                    return py_create_bool(false);
            }
        }

        // 如果类型不同，尝试数值类型强制转换
        double a_val, b_val;
        if (py_coerce_numeric(a, b, &a_val, &b_val))
        {
            bool result = false;

            switch (op)
            {
                case PY_CMP_EQ:
                    result = a_val == b_val;
                    break;
                case PY_CMP_NE:
                    result = a_val != b_val;
                    break;
                case PY_CMP_LT:
                    result = a_val < b_val;
                    break;
                case PY_CMP_LE:
                    result = a_val <= b_val;
                    break;
                case PY_CMP_GT:
                    result = a_val > b_val;
                    break;
                case PY_CMP_GE:
                    result = a_val >= b_val;
                    break;
            }

            return py_create_bool(result);
        }

        // 字符串比较
        if (a->typeId == llvmpy::PY_TYPE_STRING && b->typeId == llvmpy::PY_TYPE_STRING)
        {
            PyPrimitiveObject* aStr = (PyPrimitiveObject*)a;
            PyPrimitiveObject* bStr = (PyPrimitiveObject*)b;

            // 处理空字符串
            const char* aValue = aStr->value.stringValue ? aStr->value.stringValue : "";
            const char* bValue = bStr->value.stringValue ? bStr->value.stringValue : "";

            int cmp = strcmp(aValue, bValue);
            bool result = false;

            switch (op)
            {
                case PY_CMP_EQ:
                    result = cmp == 0;
                    break;
                case PY_CMP_NE:
                    result = cmp != 0;
                    break;
                case PY_CMP_LT:
                    result = cmp < 0;
                    break;
                case PY_CMP_LE:
                    result = cmp <= 0;
                    break;
                case PY_CMP_GT:
                    result = cmp > 0;
                    break;
                case PY_CMP_GE:
                    result = cmp >= 0;
                    break;
            }

            return py_create_bool(result);
        }

        // 列表比较 - 补全部分
        if (a->typeId == llvmpy::PY_TYPE_LIST && b->typeId == llvmpy::PY_TYPE_LIST)
        {
            PyListObject* listA = (PyListObject*)a;
            PyListObject* listB = (PyListObject*)b;

            // 判断列表元素类型是否兼容
            if (listA->elemTypeId != 0 && listB->elemTypeId != 0 && !py_are_types_compatible(listA->elemTypeId, listB->elemTypeId))
            {
                // 如果不兼容，则只能进行相等性比较，且它们不相等
                return py_create_bool(op == PY_CMP_NE);
            }

            // 首先比较长度
            if (op == PY_CMP_EQ || op == PY_CMP_NE)
            {
                bool lengthEqual = (listA->length == listB->length);
                if ((op == PY_CMP_EQ && !lengthEqual) || (op == PY_CMP_NE && !lengthEqual))
                {
                    return py_create_bool(op == PY_CMP_NE);
                }

                // 长度相等，需要逐个比较元素
                for (int i = 0; i < listA->length; i++)
                {
                    PyObject* elemA = listA->data[i];
                    PyObject* elemB = listB->data[i];

                    // 递归比较元素
                    PyObject* elemCompResult = py_object_compare(elemA, elemB, op);
                    if (!elemCompResult) return NULL;  // 比较失败

                    bool elemEqual = ((PyPrimitiveObject*)elemCompResult)->value.boolValue;
                    py_decref(elemCompResult);

                    if (!elemEqual)
                    {
                        return py_create_bool(op == PY_CMP_NE);
                    }
                }

                // 所有元素都相等
                return py_create_bool(op == PY_CMP_EQ);
            }
            else
            {
                // 对于比较运算符，先比较长度，再逐个比较元素
                if (listA->length != listB->length)
                {
                    switch (op)
                    {
                        case PY_CMP_LT:
                            return py_create_bool(listA->length < listB->length);
                        case PY_CMP_LE:
                            return py_create_bool(listA->length <= listB->length);
                        case PY_CMP_GT:
                            return py_create_bool(listA->length > listB->length);
                        case PY_CMP_GE:
                            return py_create_bool(listA->length >= listB->length);
                        default:
                            return py_create_bool(false);
                    }
                }

                // 长度相等，逐个比较元素
                for (int i = 0; i < listA->length; i++)
                {
                    PyObject* elemA = listA->data[i];
                    PyObject* elemB = listB->data[i];

                    // 判断元素是否相等
                    PyObject* elemEqResult = py_object_compare(elemA, elemB, PY_CMP_EQ);
                    if (!elemEqResult) return NULL;  // 比较失败

                    bool elemEqual = ((PyPrimitiveObject*)elemEqResult)->value.boolValue;
                    py_decref(elemEqResult);

                    if (!elemEqual)
                    {
                        // 如果元素不相等，则使用元素间的比较结果
                        return py_object_compare(elemA, elemB, op);
                    }
                }

                // 所有元素都相等，比较相等
                switch (op)
                {
                    case PY_CMP_LT:
                    case PY_CMP_GT:
                        return py_create_bool(false);  // 完全相等的列表不满足 < 或 >
                    case PY_CMP_LE:
                    case PY_CMP_GE:
                        return py_create_bool(true);  // 完全相等的列表满足 <= 或 >=
                    default:
                        return py_create_bool(false);
                }
            }
        }

        // 字典比较 - 补全部分
        if (a->typeId == llvmpy::PY_TYPE_DICT && b->typeId == llvmpy::PY_TYPE_DICT)
        {
            PyDictObject* dictA = (PyDictObject*)a;
            PyDictObject* dictB = (PyDictObject*)b;

            // 字典只支持相等性比较
            if (op == PY_CMP_EQ || op == PY_CMP_NE)
            {
                // 比较大小
                if (dictA->size != dictB->size)
                {
                    return py_create_bool(op == PY_CMP_NE);
                }

                // 获取字典A的所有键
                PyObject* keysA = py_dict_keys((PyObject*)dictA);
                if (!keysA) return NULL;

                PyListObject* keysList = (PyListObject*)keysA;
                bool areEqual = true;

                // 比较每个键值对
                for (int i = 0; i < keysList->length; i++)
                {
                    PyObject* key = keysList->data[i];

                    // 尝试从两个字典中获取值
                    PyObject* valueA = py_dict_get_item((PyObject*)dictA, key);
                    PyObject* valueB = py_dict_get_item((PyObject*)dictB, key);

                    // 如果键不存在于dictB或值不相等
                    if (!valueB)
                    {
                        areEqual = false;
                        break;
                    }

                    // 比较值
                    PyObject* valCompResult = py_object_compare(valueA, valueB, PY_CMP_EQ);
                    if (!valCompResult)
                    {
                        py_decref(keysA);
                        return NULL;
                    }

                    bool valEqual = ((PyPrimitiveObject*)valCompResult)->value.boolValue;
                    py_decref(valCompResult);

                    if (!valEqual)
                    {
                        areEqual = false;
                        break;
                    }
                }

                py_decref(keysA);
                return py_create_bool(op == PY_CMP_EQ ? areEqual : !areEqual);
            }
            else
            {
                // 不支持字典的其他比较操作
                fprintf(stderr, "TypeError: '%s' not supported between instances of 'dict' and 'dict'\n",
                        op == PY_CMP_LT ? "<" : op == PY_CMP_LE ? "<="
                                            : op == PY_CMP_GT   ? ">"
                                            : op == PY_CMP_GE   ? ">="
                                                                : "unknown");
                return NULL;
            }
        }

        // 类型检查函数 - 补全部分
        if ((op == PY_CMP_EQ || op == PY_CMP_NE) && (a->typeId == llvmpy::PY_TYPE_INT || b->typeId == llvmpy::PY_TYPE_INT))
        {
            // 特殊处理：检查任何对象是否为列表类型
            if (a->typeId == llvmpy::PY_TYPE_LIST || b->typeId == llvmpy::PY_TYPE_LIST)
            {
                // 专门处理 "Expected type 5, got 1" 的错误情况
                // 这种情况通常发生在代码试图将整数视为列表时
                fprintf(stderr, "TypeError: Expected type %d (%s), got %d (%s)\n",
                        llvmpy::PY_TYPE_LIST, py_type_name(llvmpy::PY_TYPE_LIST),
                        llvmpy::PY_TYPE_INT, py_type_name(llvmpy::PY_TYPE_INT));

                // 返回不相等结果，避免继续使用错误的类型
                return py_create_bool(op == PY_CMP_NE);
            }
        }

        // 如果类型不同而且无法比较，只能做相等性判断
        if (a->typeId != b->typeId)
        {
            if (op == PY_CMP_EQ || op == PY_CMP_NE)
            {
                return py_create_bool(op == PY_CMP_NE);
            }
            else
            {
                // 对于其他比较操作，报类型错误
                fprintf(stderr, "TypeError: '%s' not supported between instances of '%s' and '%s'\n",
                        op == PY_CMP_LT ? "<" : op == PY_CMP_LE ? "<="
                                            : op == PY_CMP_GT   ? ">"
                                            : op == PY_CMP_GE   ? ">="
                                                                : "unknown",
                        py_type_name(a->typeId),
                        py_type_name(b->typeId));
                return NULL;
            }
        }

        // 通用相等/不等判断 - 默认行为
        if (op == PY_CMP_EQ)
        {
            return py_create_bool(false);  // 不同类型的对象默认不相等
        }
        else if (op == PY_CMP_NE)
        {
            return py_create_bool(true);  // 不同类型的对象默认不相等
        }

        // 不支持的比较操作
        fprintf(stderr, "TypeError: unsupported operand type(s) for %s: '%s' and '%s'\n",
                op == PY_CMP_LT ? "<" : op == PY_CMP_LE ? "<="
                                    : op == PY_CMP_GT   ? ">"
                                    : op == PY_CMP_GE   ? ">="
                                                        : "unknown",
                py_type_name(a->typeId),
                py_type_name(b->typeId));
        return NULL;
    }

    // 添加辅助比较函数
    bool py_compare_eq(PyObject* a, PyObject* b)
    {
        PyObject* result = py_object_compare(a, b, PY_CMP_EQ);
        if (!result) return false;

        bool isEqual = ((PyPrimitiveObject*)result)->value.boolValue;
        py_decref(result);
        return isEqual;
    }

    bool py_compare_ne(PyObject* a, PyObject* b)
    {
        PyObject* result = py_object_compare(a, b, PY_CMP_NE);
        if (!result) return true;  // 默认不相等

        bool notEqual = ((PyPrimitiveObject*)result)->value.boolValue;
        py_decref(result);
        return notEqual;
    }

    // 添加类型一致性检查函数
    bool py_ensure_type_compatibility(PyObject* obj, int expectedTypeId)
    {
        if (!obj) return false;

        // 检查类型兼容性
        bool compatible = py_are_types_compatible(obj->typeId, expectedTypeId);
        if (!compatible)
        {
            py_type_error(obj, expectedTypeId);
        }

        return compatible;
    }

    // 添加列表特定的类型检查函数
    bool py_check_list_element_type(PyListObject* list, PyObject* item)
    {
        if (!list || !item) return false;

        // 如果列表没有指定元素类型，任何类型都可以
        if (list->elemTypeId == 0) return true;

        // 检查元素类型是否兼容
        return py_are_types_compatible(item->typeId, list->elemTypeId);
    }

    //===----------------------------------------------------------------------===//
    // 列表操作函数
    //===----------------------------------------------------------------------===//

    // 创建列表对象
    PyObject* py_create_list(int size, int elemTypeId)
    {
        PyListObject* list = (PyListObject*)malloc(sizeof(PyListObject));
        if (!list) return NULL;

        // 初始化列表头
        list->header.refCount = 1;
        list->header.typeId = llvmpy::PY_TYPE_LIST;
        list->length = 0;
        list->capacity = size > 0 ? size : 8;  // 默认容量为8
        list->elemTypeId = elemTypeId;

        // 分配数据数组
        list->data = (PyObject**)calloc(list->capacity, sizeof(PyObject*));
        if (!list->data)
        {
            free(list);
            return NULL;
        }

        return (PyObject*)list;
    }

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
    PyObject* py_list_get_item(PyObject* obj, int index)
    {
        if (!py_check_type(obj, llvmpy::PY_TYPE_LIST))
        {
            py_type_error(obj, llvmpy::PY_TYPE_LIST);
            return NULL;
        }

        PyListObject* list = (PyListObject*)obj;

        // 索引检查
        if (index < 0 || index >= list->length)
        {
            fprintf(stderr, "IndexError: list index out of range\n");
            return NULL;
        }

        // 返回元素，不增加引用计数
        return list->data[index];
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

        // 修改: 索引检查应该基于容量而不是长度
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

        // 添加: 如果设置的索引超过当前长度，需要更新长度
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

    //===----------------------------------------------------------------------===//
    // 字典操作函数
    //===----------------------------------------------------------------------===//

    // 哈希函数
    unsigned int py_hash_object(PyObject* obj)
    {
        if (!obj) return 0;

        switch (llvmpy::getBaseTypeId(obj->typeId))
        {
            case llvmpy::PY_TYPE_INT:
            {
                int val = ((PyPrimitiveObject*)obj)->value.intValue;
                return (unsigned int)val;
            }

            case llvmpy::PY_TYPE_BOOL:
            {
                bool val = ((PyPrimitiveObject*)obj)->value.boolValue;
                return val ? 1 : 0;
            }

            case llvmpy::PY_TYPE_STRING:
            {
                const char* str = ((PyPrimitiveObject*)obj)->value.stringValue;
                if (!str) return 0;

                // 简单的字符串哈希算法
                unsigned int hash = 5381;
                int c;
                while ((c = *str++))
                {
                    hash = ((hash << 5) + hash) + c;  // hash * 33 + c
                }

                return hash;
            }

            default:
                // 对于不可哈希类型，使用地址作为哈希值
                return (unsigned int)(intptr_t)obj;
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

    // 创建字典对象
    PyObject* py_create_dict(int initialCapacity, int keyTypeId)
    {
        // 确保初始容量是合理的
        if (initialCapacity <= 0)
        {
            initialCapacity = 8;  // 默认初始容量
        }

        // 分配字典对象
        PyDictObject* dict = (PyDictObject*)malloc(sizeof(PyDictObject));
        if (!dict) return NULL;

        // 初始化字典头
        dict->header.refCount = 1;
        dict->header.typeId = llvmpy::PY_TYPE_DICT;
        dict->size = 0;
        dict->capacity = initialCapacity;
        dict->keyTypeId = keyTypeId;

        // 分配哈希表
        dict->entries = (PyDictEntry*)calloc(initialCapacity, sizeof(PyDictEntry));
        if (!dict->entries)
        {
            free(dict);
            return NULL;
        }

        // 初始化所有条目
        for (int i = 0; i < initialCapacity; i++)
        {
            dict->entries[i].key = NULL;
            dict->entries[i].value = NULL;
            dict->entries[i].hash = 0;
            dict->entries[i].used = false;
        }

        return (PyObject*)dict;
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
                        return entry->value;
                    }
                }
            }
        }

        // 键不存在
        fprintf(stderr, "KeyError: Key not found in dictionary\n");
        return NULL;
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

        PyListObject* list = (PyListObject*)keysList;

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

    //===----------------------------------------------------------------------===//
    // 完成对象生命周期管理部分
    //===----------------------------------------------------------------------===//

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

    //===----------------------------------------------------------------------===//
    // C API 结束
    //===----------------------------------------------------------------------===//

#ifdef __cplusplus
}  // extern "C"
#endif