
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdbool>
#include <cmath>
#include <climits>
#include <cctype> // 添加这一行，提供isspace函数
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
    int py_object_len(PyObject* obj);

    // 列表操作函数
    PyObject* py_list_copy(PyObject* obj);
    void py_list_set_item(PyObject* obj, int index, PyObject* item);
    PyObject* py_list_get_item(PyObject* obj, int index);
    PyObject* py_list_append(PyObject* obj, PyObject* item);
    int py_list_len(PyObject* obj);
    int py_get_object_type_id(PyObject* obj);
    int py_extract_int_from_any(PyObject* obj);
    void py_list_decref_items(PyListObject* list);
    // 函数前向声明部分...
PyObject* py_list_get_item_with_type(PyObject* list, int index, int* out_type_id);
PyObject* py_dict_get_item_with_type(PyObject* dict, PyObject* key, int* out_type_id);

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
    int py_get_container_type_info(PyObject* container);
PyObject* py_object_index_with_type(PyObject* obj, PyObject* index, int* out_type_id);
int py_get_list_element_type_id(PyObject* list);
PyObject* py_object_index(PyObject* obj, PyObject* index);
bool py_is_container(PyObject* obj);
bool py_is_sequence(PyObject* obj);
int py_extract_constant_int(PyObject* obj);
int py_get_type_id(PyObject* obj);
const char* py_type_id_to_string(int typeId);
int py_get_safe_type_id(PyObject* obj);
void py_set_index_result_type(PyObject* result, int typeId);

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


// 将Python对象转换为布尔值
bool py_object_to_bool(PyObject* obj)
{
    if (!obj)
        return false;
    
    // 检查对象类型
    switch (llvmpy::getBaseTypeId(obj->typeId))
    {
        case llvmpy::PY_TYPE_BOOL:
            return ((PyPrimitiveObject*)obj)->value.boolValue;
            
        case llvmpy::PY_TYPE_INT:
            return ((PyPrimitiveObject*)obj)->value.intValue != 0;
            
        case llvmpy::PY_TYPE_DOUBLE:
            return ((PyPrimitiveObject*)obj)->value.doubleValue != 0.0;
            
        case llvmpy::PY_TYPE_STRING:
            return ((PyPrimitiveObject*)obj)->value.stringValue && 
                   *((PyPrimitiveObject*)obj)->value.stringValue != '\0';
            
        case llvmpy::PY_TYPE_LIST:
            return ((PyListObject*)obj)->length > 0;
            
        case llvmpy::PY_TYPE_DICT:
            return ((PyDictObject*)obj)->size > 0;
            
        case llvmpy::PY_TYPE_NONE:
            return false;
            
        default:
            fprintf(stderr, "WARNING: py_object_to_bool called on unknown type: %d\n", obj->typeId);
            return true;  // 默认为true以避免程序崩溃
    }
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
    // 获取 None 单例对象 - 在extern "C" 块内添加
PyObject* py_get_none()
{
    // None 对象应该是单例的，使用静态变量确保只创建一次
    static PyObject* noneObj = nullptr;
    
    // 如果 None 对象尚未创建，则创建它
    if (!noneObj) {
        // 分配一个基本对象结构体空间
        noneObj = (PyObject*)malloc(sizeof(PyObject));
        if (!noneObj) {
            fprintf(stderr, "错误: 无法为 None 对象分配内存\n");
            return nullptr;
        }
        
        // 初始化 None 对象
        noneObj->refCount = 1;      // 开始引用计数为1
        noneObj->typeId = llvmpy::PY_TYPE_NONE;  // 设置为 None 类型
        
        // None 是单例，永远不应该被释放，但为了防止程序结束时的内存检查报错，
        // 可以注册一个 atexit 处理函数来释放它
    }
    
    // 增加引用计数 (因为每次调用都应该增加一次引用)
    py_incref(noneObj);
    
    return noneObj;
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
// 获取对象的类型ID
int py_get_object_type_id(PyObject* obj)
{
    if (!obj)
    {
        // 返回 NONE 类型或错误值
        return llvmpy::PY_TYPE_NONE;
    }
    
    // 安全检查 - 确保指针有效
    if ((uintptr_t)obj < 0x1000 || (uintptr_t)obj > 0x7FFFFFFFFFFFFFFF)
    {
        fprintf(stderr, "ERROR: Invalid object pointer in py_get_object_type_id: %p\n", obj);
        return llvmpy::PY_TYPE_NONE;
    }
    
    // 直接返回对象的类型ID
    return obj->typeId;
}
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
// 在 extern "C" 块内添加以下函数

// any 类型转换到 int
// any 类型转换到 int
// any 类型转换到 int - 保留类型特性的智能版本
PyObject* py_convert_any_to_int(PyObject* obj)
{
    if (!obj) {
        fprintf(stderr, "错误: py_convert_any_to_int 接收到空对象\n");
        return py_create_int(0);
    }
    
    // 使用正确的类型ID检查 - 使用PY_TYPE_PTR (400)值而不是依赖未定义的名称
    if (obj->typeId >= 400) { // PY_TYPE_PTR及其派生类型都大于400
        PyObject** ptrToObj = (PyObject**)obj;
        if (ptrToObj && *ptrToObj) {
            // 使用指针指向的实际对象
            return py_convert_any_to_int(*ptrToObj);
        }
    }
    
    // 使用getBaseTypeId获取基本类型ID
    int baseTypeId = llvmpy::getBaseTypeId(obj->typeId);
    
    switch (baseTypeId)
    {
        case llvmpy::PY_TYPE_INT:
            // 整数保持原样
            py_incref(obj);
            return obj;
            
        case llvmpy::PY_TYPE_DOUBLE: {
            // 关键改进：对于浮点数，直接返回原始对象，不做类型转换
            // 这允许表达式保留其原始类型，避免精度丢失
            py_incref(obj);
            return obj;
        }
            
        case llvmpy::PY_TYPE_BOOL: {
            PyPrimitiveObject* boolObj = (PyPrimitiveObject*)obj;
            return py_create_int(boolObj->value.boolValue ? 1 : 0);
        }
            
        case llvmpy::PY_TYPE_STRING: {
            PyPrimitiveObject* strObj = (PyPrimitiveObject*)obj;
            if (!strObj->value.stringValue) {
                return py_create_int(0);
            }
            
            // 尝试将字符串转换为数值
            char* endptr;
            
            // 检查是否包含小数点
            if (strchr(strObj->value.stringValue, '.')) {
                // 尝试解析为浮点数
                double dblValue = strtod(strObj->value.stringValue, &endptr);
                if (*endptr == '\0') {
                    // 成功解析为浮点数，返回浮点数对象
                    return py_create_double(dblValue);
                }
            }
            
            // 尝试解析为整数
            int value = (int)strtol(strObj->value.stringValue, &endptr, 10);
            if (*endptr != '\0') {
                fprintf(stderr, "警告: 无法将字符串 '%s' 转换为数字\n", strObj->value.stringValue);
            }
            
            return py_create_int(value);
        }
            
        case llvmpy::PY_TYPE_LIST:
        case llvmpy::PY_TYPE_DICT: {
            // 对于容器类型，返回原始对象，保留类型信息
            py_incref(obj);
            return obj;
        }
            
        case llvmpy::PY_TYPE_NONE:
            return py_create_int(0);
            
        default:
            // 未知类型时保持原样，增加引用计数
            fprintf(stderr, "提示: 保留未知类型 %d 的原始值\n", obj->typeId);
            py_incref(obj);
            return obj;
    }
}


// 检测对象的实际类型，处理指针和包装类型
int py_detect_actual_type(PyObject* obj)
{
    if (!obj) return llvmpy::PY_TYPE_NONE;
    
    // 处理指针类型
    if (obj->typeId >= 400) { // PY_TYPE_PTR 范围
        PyObject** ptrToObj = (PyObject**)obj;
        if (ptrToObj && *ptrToObj) {
            return (*ptrToObj)->typeId;
        }
    }
    
    return obj->typeId;
}

// 提取对象的实际值，无论它是普通对象还是指针
PyObject* py_unwrap_object(PyObject* obj)
{
    if (!obj) return nullptr;
    
    // 处理指针类型
    if (obj->typeId >= 400) { // PY_TYPE_PTR 范围
        PyObject** ptrToObj = (PyObject**)obj;
        if (ptrToObj && *ptrToObj) {
            return *ptrToObj;
        }
    }
    
    return obj;
}

// 通用any类型转换 - 智能保留原始类型
// 智能类型保留函数 - 专门处理参数透传返回
PyObject* py_convert_any_preserve_type(PyObject* obj)
{
    if (!obj) {
        return py_create_int(0); // 默认返回0
    }
    
    // 处理指针类型
    if (obj->typeId >= 400) { // 指针类型范围
        PyObject** ptrToObj = (PyObject**)obj;
        if (ptrToObj && *ptrToObj) {
            // 增加引用计数并返回指向的对象
            py_incref(*ptrToObj);
            return *ptrToObj;
        }
    }
    
    // 处理容器类型
    if ((obj->typeId >= llvmpy::PY_TYPE_LIST_BASE && obj->typeId < llvmpy::PY_TYPE_DICT_BASE) ||
        (obj->typeId >= llvmpy::PY_TYPE_DICT_BASE && obj->typeId < llvmpy::PY_TYPE_FUNC_BASE) ||
        obj->typeId == llvmpy::PY_TYPE_LIST || 
        obj->typeId == llvmpy::PY_TYPE_DICT) {
        // 是容器类型，保留原始类型
        py_incref(obj);
        return obj;
    }
    
    // 处理基本类型
    switch (llvmpy::getBaseTypeId(obj->typeId))
    {
        case llvmpy::PY_TYPE_INT:
        case llvmpy::PY_TYPE_DOUBLE:
        case llvmpy::PY_TYPE_BOOL:
        case llvmpy::PY_TYPE_STRING:
        case llvmpy::PY_TYPE_ANY:
            // 直接保留原始类型
            py_incref(obj);
            return obj;
        
        default:
            // 未知类型也保留原始类型
            py_incref(obj);
            return obj;
    }
}

// 保留浮点/容器类型的整数提取函数
// 保留浮点/容器类型的整数提取函数
int py_extract_preserve_int(PyObject* obj)
{
    if (!obj) return 0;
    
    // 解包指针
    PyObject* actual = obj;
    if (obj->typeId >= 400) {
        PyObject** ptrToObj = (PyObject**)obj;
        if (ptrToObj && *ptrToObj) {
            actual = *ptrToObj;
        }
    }
    
    // 根据实际类型进行处理
    switch (llvmpy::getBaseTypeId(actual->typeId))
    {
        case llvmpy::PY_TYPE_INT:
            return ((PyPrimitiveObject*)actual)->value.intValue;
            
        case llvmpy::PY_TYPE_DOUBLE:
            // 对于浮点数，返回int部分但尝试保留原始类型
            return (int)((PyPrimitiveObject*)actual)->value.doubleValue;
            
        case llvmpy::PY_TYPE_BOOL:
            return ((PyPrimitiveObject*)actual)->value.boolValue ? 1 : 0;
            
        case llvmpy::PY_TYPE_LIST:
        case llvmpy::PY_TYPE_DICT:
            // 对于容器类型，返回长度或0
            if (actual->typeId == llvmpy::PY_TYPE_LIST) {
                return py_list_len(actual);
            } else if (actual->typeId == llvmpy::PY_TYPE_DICT) {
                return py_dict_len(actual);
            }
            // 故意继续到默认情况
            
        default:
            fprintf(stderr, "警告: 无法从类型 %d 提取整数值\n", actual->typeId);
            return 0;
    }
}

// any 类型转换到 double
PyObject* py_convert_any_to_double(PyObject* obj)
{
    if (!obj)
    {
        fprintf(stderr, "TypeError: Cannot convert NULL to double\n");
        return NULL;
    }

    switch (llvmpy::getBaseTypeId(obj->typeId))
    {
        case llvmpy::PY_TYPE_INT:
            return py_create_double((double)((PyPrimitiveObject*)obj)->value.intValue);
            
        case llvmpy::PY_TYPE_DOUBLE:
            py_incref(obj);
            return obj;
            
        case llvmpy::PY_TYPE_BOOL:
            return py_create_double(((PyPrimitiveObject*)obj)->value.boolValue ? 1.0 : 0.0);
            
        case llvmpy::PY_TYPE_STRING:
            fprintf(stderr, "Warning: String to double conversion not fully implemented\n");
            return py_create_double(0.0);
            
        case llvmpy::PY_TYPE_NONE:
            return py_create_double(0.0);
            
        default:
            fprintf(stderr, "TypeError: Cannot convert %s to double\n", py_type_name(obj->typeId));
            return py_create_double(0.0);
    }
}

// any 类型转换到 bool
PyObject* py_convert_any_to_bool(PyObject* obj)
{
    if (!obj)
    {
        fprintf(stderr, "TypeError: Cannot convert NULL to bool\n");
        return NULL;
    }

    switch (llvmpy::getBaseTypeId(obj->typeId))
    {
        case llvmpy::PY_TYPE_INT:
            return py_create_bool(((PyPrimitiveObject*)obj)->value.intValue != 0);
            
        case llvmpy::PY_TYPE_DOUBLE:
            return py_create_bool(((PyPrimitiveObject*)obj)->value.doubleValue != 0.0);
            
        case llvmpy::PY_TYPE_BOOL:
            py_incref(obj);
            return obj;
            
        case llvmpy::PY_TYPE_STRING:
            {
                const char* str = ((PyPrimitiveObject*)obj)->value.stringValue;
                return py_create_bool(str && str[0] != '\0');
            }
            
        case llvmpy::PY_TYPE_LIST:
            return py_create_bool(((PyListObject*)obj)->length > 0);
            
        case llvmpy::PY_TYPE_DICT:
            return py_create_bool(((PyDictObject*)obj)->size > 0);
            
        case llvmpy::PY_TYPE_NONE:
            return py_create_bool(false);
            
        default:
            fprintf(stderr, "TypeError: Cannot convert %s to bool\n", py_type_name(obj->typeId));
            return py_create_bool(false);
    }
}

// any 类型转换到 string
PyObject* py_convert_any_to_string(PyObject* obj)
{
    if (!obj)
    {
        fprintf(stderr, "TypeError: Cannot convert NULL to string\n");
        return NULL;
    }

    char buffer[128];
    
    switch (llvmpy::getBaseTypeId(obj->typeId))
    {
        case llvmpy::PY_TYPE_INT:
            snprintf(buffer, sizeof(buffer), "%d", ((PyPrimitiveObject*)obj)->value.intValue);
            return py_create_string(buffer);
            
        case llvmpy::PY_TYPE_DOUBLE:
            snprintf(buffer, sizeof(buffer), "%g", ((PyPrimitiveObject*)obj)->value.doubleValue);
            return py_create_string(buffer);
            
        case llvmpy::PY_TYPE_BOOL:
            return py_create_string(((PyPrimitiveObject*)obj)->value.boolValue ? "True" : "False");
            
        case llvmpy::PY_TYPE_STRING:
            py_incref(obj);
            return obj;
            
        case llvmpy::PY_TYPE_LIST:
            return py_create_string("[list]");
            
        case llvmpy::PY_TYPE_DICT:
            return py_create_string("{dict}");
            
        case llvmpy::PY_TYPE_NONE:
            return py_create_string("None");
            
        default:
            fprintf(stderr, "TypeError: Cannot convert %s to string\n", py_type_name(obj->typeId));
            return py_create_string("<unknown>");
    }
}


// 执行索引操作并获取结果类型 - 核心函数
PyObject* py_object_index_with_type(PyObject* obj, PyObject* index, int* out_type_id)
{
    if (!obj || !out_type_id) {
        // 确保有效参数
        if (out_type_id) *out_type_id = llvmpy::PY_TYPE_NONE;
        return py_get_none();
    }
    
    // 解包可能的指针
    PyObject* actual_obj = obj;
    if (obj->typeId >= 400) { // 指针类型范围
        PyObject** ptr_obj = (PyObject**)obj;
        if (ptr_obj && *ptr_obj) {
            actual_obj = *ptr_obj;
        }
    }
    
    // 同样解包索引
    PyObject* actual_index = index;
    if (index && index->typeId >= 400) {
        PyObject** ptr_index = (PyObject**)index;
        if (ptr_index && *ptr_index) {
            actual_index = *ptr_index;
        }
    }
    
    // 获取基本类型ID
    int baseTypeId = llvmpy::getBaseTypeId(actual_obj->typeId);
    
    // 根据对象类型执行相应的索引操作
    switch (baseTypeId) {
        case llvmpy::PY_TYPE_LIST: {
            PyListObject* list = (PyListObject*)actual_obj;
            int idx = py_extract_int_from_any(actual_index);
            
            if (idx < 0 || idx >= list->length) {
                fprintf(stderr, "错误: 列表索引越界: %d, 长度: %d\n", idx, list->length);
                *out_type_id = llvmpy::PY_TYPE_NONE;
                return py_get_none();
            }
            
            PyObject* item = list->data[idx];
            
            // 确定结果类型
            if (item) {
                *out_type_id = item->typeId;
                
                // 如果是列表且有元素类型，使用特殊的列表类型ID编码
                if (list->elemTypeId > 0 && list->elemTypeId != llvmpy::PY_TYPE_ANY) {
                    // 检查是否与列表声明的元素类型匹配
                    if (llvmpy::getBaseTypeId(item->typeId) == llvmpy::getBaseTypeId(list->elemTypeId)) {
                        *out_type_id = list->elemTypeId;
                    }
                }
            } else {
                *out_type_id = llvmpy::PY_TYPE_NONE;
            }
            
            py_incref(item);
            return item;
        }
            
        case llvmpy::PY_TYPE_DICT: {
            PyDictObject* dict = (PyDictObject*)actual_obj;
            PyDictEntry* entry = py_dict_find_entry(dict, actual_index);
            
            if (entry && entry->used && entry->value) {
                *out_type_id = entry->value->typeId;
                py_incref(entry->value);
                return entry->value;
            } else {
                fprintf(stderr, "警告: 字典中找不到指定的键\n");
                *out_type_id = llvmpy::PY_TYPE_NONE;
                return py_get_none();
            }
        }
            
        case llvmpy::PY_TYPE_STRING: {
            int idx = py_extract_int_from_any(actual_index);
            PyPrimitiveObject* strObj = (PyPrimitiveObject*)actual_obj;
            
            if (!strObj->value.stringValue) {
                *out_type_id = llvmpy::PY_TYPE_STRING;
                return py_create_string("");
            }
            
            size_t len = strlen(strObj->value.stringValue);
            if (idx < 0 || (size_t)idx >= len) {
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
    if (!container) {
        return llvmpy::PY_TYPE_ANY;
    }
    
    // 解包指针类型
    PyObject* actual = container;
    if (container->typeId >= 400) {
        PyObject** ptrObj = (PyObject**)container;
        if (ptrObj && *ptrObj) {
            actual = *ptrObj;
        }
    }
    
    // 检查容器类型
    int baseTypeId = llvmpy::getBaseTypeId(actual->typeId);
    
    if (baseTypeId == llvmpy::PY_TYPE_LIST) {
        PyListObject* list = (PyListObject*)actual;
        
        // 获取列表声明的元素类型
        if (list->elemTypeId > 0) {
            return list->elemTypeId;
        }
        
        // 尝试从实际元素推断类型
        if (list->length > 0 && list->data[0]) {
            return list->data[0]->typeId;
        }
        
        // 默认为ANY类型
        return llvmpy::PY_TYPE_ANY;
    }
    else if (baseTypeId == llvmpy::PY_TYPE_DICT) {
        PyDictObject* dict = (PyDictObject*)actual;
        
        // 返回值类型
        return dict->keyTypeId > 0 ? 
               llvmpy::PY_TYPE_DICT_BASE + dict->keyTypeId : 
               llvmpy::PY_TYPE_DICT;
    }
    
    // 其他容器类型或不是容器
    return llvmpy::PY_TYPE_ANY;
}

// 获取列表元素类型ID
int py_get_list_element_type_id(PyObject* list)
{
    if (!list || llvmpy::getBaseTypeId(list->typeId) != llvmpy::PY_TYPE_LIST) {
        return 0; // 无效或不是列表
    }
    
    PyListObject* listObj = (PyListObject*)list;
    
    // 如果列表声明了元素类型，直接返回
    if (listObj->elemTypeId > 0) {
        return listObj->elemTypeId;
    }
    
    // 尝试从第一个元素推断类型
    if (listObj->length > 0 && listObj->data[0]) {
        return listObj->data[0]->typeId;
    }
    
    return 0; // 未知元素类型
}



// 类型检测函数 - 检查对象是否为容器类型
bool py_is_container(PyObject* obj)
{
    if (!obj) return false;
    
    int typeId = obj->typeId;
    int baseTypeId = llvmpy::getBaseTypeId(typeId);
    
    return (baseTypeId == llvmpy::PY_TYPE_LIST || 
            baseTypeId == llvmpy::PY_TYPE_DICT || 
            baseTypeId == llvmpy::PY_TYPE_TUPLE ||
            (typeId >= llvmpy::PY_TYPE_LIST_BASE && typeId < llvmpy::PY_TYPE_DICT_BASE) ||
            (typeId >= llvmpy::PY_TYPE_DICT_BASE && typeId < llvmpy::PY_TYPE_FUNC_BASE));
}

// 类型检测函数 - 检查对象是否为序列类型
bool py_is_sequence(PyObject* obj)
{
    if (!obj) return false;
    
    int typeId = obj->typeId;
    int baseTypeId = llvmpy::getBaseTypeId(typeId);
    
    return (baseTypeId == llvmpy::PY_TYPE_LIST || 
            baseTypeId == llvmpy::PY_TYPE_TUPLE || 
            baseTypeId == llvmpy::PY_TYPE_STRING ||
            (typeId >= llvmpy::PY_TYPE_LIST_BASE && typeId < llvmpy::PY_TYPE_DICT_BASE));
}

// 将ANY类型转换为整数
// 将返回int的版本重命名为 py_extract_int_from_any
// 增强版本的提取整数函数
int py_extract_int_from_any(PyObject* obj)
{
    if (!obj) {
        return 0;
    }
    
    // 首先检查是否是整数指针 (i32*)
    // 通过尝试安全地访问 typeId 来检测
    int typeId = 0;
    bool isRawIntPtr = false;
    
    // 使用轻量级的指针有效性检查
    if ((uintptr_t)obj < 0x10000) {
        // 小值通常是无效指针或直接传递的整数
        return (int)(intptr_t)obj;
    }
    
    // 首先尝试访问typeId - 如果是整数指针，这将返回整数值
    try {
        // 可能的整数指针处理
        int* intPtr = (int*)obj;
        if (intPtr) {
            // 尝试安全读取值
            int intValue = *intPtr;
            // 如果值在合理范围内，很可能是整数指针
            if (intValue >= -10000000 && intValue <= 10000000) {
                return intValue;
            }
        }
        
        // 尝试作为PyObject处理
        typeId = obj->typeId;
    } catch (...) {
        // 如果出现异常，假设这是一个整数指针
        int* intPtr = (int*)obj;
        if (intPtr) {
            return *intPtr;
        }
        return 0;
    }
    
    // 处理指针类型
    PyObject* actual = obj;
    if (typeId >= 400) {
        PyObject** ptrObj = (PyObject**)obj;
        if (ptrObj && *ptrObj) {
            actual = *ptrObj;
        }
    }
    
    // 根据实际类型提取整数
    int baseTypeId = llvmpy::getBaseTypeId(actual->typeId);
    
    switch (baseTypeId) {
        case llvmpy::PY_TYPE_INT: {
            PyPrimitiveObject* intObj = (PyPrimitiveObject*)actual;
            return intObj->value.intValue;
        }
        
        case llvmpy::PY_TYPE_DOUBLE: {
            PyPrimitiveObject* doubleObj = (PyPrimitiveObject*)actual;
            return (int)doubleObj->value.doubleValue;
        }
        
        case llvmpy::PY_TYPE_BOOL: {
            PyPrimitiveObject* boolObj = (PyPrimitiveObject*)actual;
            return boolObj->value.boolValue ? 1 : 0;
        }
        
        case llvmpy::PY_TYPE_STRING: {
            PyPrimitiveObject* strObj = (PyPrimitiveObject*)actual;
            if (strObj->value.stringValue) {
                // 尝试直接转换
                char* endptr;
                long value = strtol(strObj->value.stringValue, &endptr, 10);
                
                // 检查是否成功转换
                if (*endptr == '\0' || isspace(*endptr)) {
                    return (int)value;
                }
                
                // 尝试作为浮点数解析
                if (strchr(strObj->value.stringValue, '.')) {
                    double dblVal = strtod(strObj->value.stringValue, &endptr);
                    if (*endptr == '\0' || isspace(*endptr)) {
                        return (int)dblVal;
                    }
                }
                
                // 最后尝试使用atoi
                return atoi(strObj->value.stringValue);
            }
            return 0;
        }
        
        case llvmpy::PY_TYPE_LIST: {
            PyListObject* listObj = (PyListObject*)actual;
            return listObj->length; // 返回列表长度
        }
        
        case llvmpy::PY_TYPE_DICT: {
            PyDictObject* dictObj = (PyDictObject*)actual;
            return dictObj->size; // 返回字典大小
        }
        
        case llvmpy::PY_TYPE_ANY: {
            // 尝试根据实际值转换
            fprintf(stderr, "警告: 尝试从ANY类型提取整数值，可能不准确\n");
            return 0;
        }
        
        default:
            // 对于小整数值，可能是直接传递的整数指针
            if ((uintptr_t)obj < 10000) {
                return (int)(intptr_t)obj;
            }
            
            fprintf(stderr, "警告: 无法从类型 %d 提取整数，返回0\n", actual->typeId);
            return 0;
    }
}




PyObject* py_object_index(PyObject* obj, PyObject* index)
{
    if (!obj) {
        fprintf(stderr, "错误: 试图在NULL对象上执行索引操作\n");
        return py_get_none();
    }
    
    // 首先解包，确保处理真实对象而不是指针
    PyObject* actual_obj = obj;
    if (obj->typeId >= 400) { // 指针类型范围
        PyObject** ptr_obj = (PyObject**)obj;
        if (ptr_obj && *ptr_obj) {
            actual_obj = *ptr_obj;
        }
    }
    
    // 同样解包索引
    PyObject* actual_index = index;
    if (index && index->typeId >= 400) {
        PyObject** ptr_index = (PyObject**)index;
        if (ptr_index && *ptr_index) {
            actual_index = *ptr_index;
        }
    }
    
    // 获取对象的基本类型ID
    int baseTypeId = llvmpy::getBaseTypeId(actual_obj->typeId);
    
    // 尝试根据对象类型选择适当的索引函数
    switch (baseTypeId) {
        case llvmpy::PY_TYPE_LIST: {
            int idx = py_extract_int_from_any(actual_index);
            PyListObject* list = (PyListObject*)actual_obj;
            
            if (idx < 0 || idx >= list->length) {
                fprintf(stderr, "错误: 列表索引越界: %d, 长度: %d\n", idx, list->length);
                return py_get_none();
            }
            
            PyObject* item = list->data[idx];
            py_incref(item);
            return item;
        }
            
        case llvmpy::PY_TYPE_DICT: {
            PyDictObject* dict = (PyDictObject*)actual_obj;
            PyDictEntry* entry = py_dict_find_entry(dict, actual_index);
            
            if (entry && entry->used) {
                py_incref(entry->value);
                return entry->value;
            } else {
                fprintf(stderr, "警告: 字典中找不到指定的键\n");
                return py_get_none();
            }
        }
            
        case llvmpy::PY_TYPE_STRING: {
            int idx = py_extract_int_from_any(actual_index);
            PyPrimitiveObject* strObj = (PyPrimitiveObject*)actual_obj;
            
            if (!strObj->value.stringValue) {
                return py_create_string("");
            }
            
            size_t len = strlen(strObj->value.stringValue);
            if (idx < 0 || (size_t)idx >= len) {
                fprintf(stderr, "警告: 字符串索引越界: %d, 长度: %zu\n", idx, len);
                return py_create_string("");
            }
            
            char buf[2] = {strObj->value.stringValue[idx], '\0'};
            return py_create_string(buf);
        }
            
        case llvmpy::PY_TYPE_TUPLE: {
            // 假设元组结构与列表相似
            int idx = py_extract_int_from_any(actual_index);
            PyListObject* tuple = (PyListObject*)actual_obj; // 使用列表对象结构
            
            if (idx < 0 || idx >= tuple->length) {
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
    if (!str || str->typeId != llvmpy::PY_TYPE_STRING) {
        fprintf(stderr, "错误: py_string_get_char 需要字符串对象\n");
        return py_create_string("");
    }
    
    PyPrimitiveObject* strObj = (PyPrimitiveObject*)str;
    if (!strObj->value.stringValue) {
        return py_create_string("");
    }
    
    size_t len = strlen(strObj->value.stringValue);
    if (index < 0 || (size_t)index >= len) {
        fprintf(stderr, "警告: 字符串索引越界: %d, 长度: %zu\n", index, len);
        return py_create_string("");
    }
    
    char buf[2] = {strObj->value.stringValue[index], '\0'};
    return py_create_string(buf);
}



// 从具体类型转换到 any 类型
PyObject* py_convert_to_any(PyObject* obj)
{
    if (!obj)
    {
        fprintf(stderr, "TypeError: Cannot convert NULL to any\n");
        return NULL;
    }
    
    // 对于 any 类型，直接增加引用计数并返回
    py_incref(obj);
    return obj;
}


// 智能类型保留函数 - 用于处理参数透传返回
PyObject* py_preserve_parameter_type(PyObject* obj) {
    if (!obj) {
        fprintf(stderr, "错误: py_preserve_parameter_type 接收到空对象\n");
        return py_create_int(0);
    }
    
    // 增加引用计数并返回原始对象
    py_incref(obj);
    return obj;
}

// 智能类型转换函数 - 用于函数返回值类型转换
PyObject* py_smart_convert(PyObject* obj, int targetTypeId) {
    if (!obj) return nullptr;
    
    // 获取对象的实际类型ID
    int sourceTypeId = py_get_object_type_id(obj);
    
    // 如果源类型和目标类型相同，或者目标类型是ANY，则保留原始类型
    if (sourceTypeId == targetTypeId || targetTypeId == llvmpy::PY_TYPE_ANY) {
        py_incref(obj);
        return obj;
    }
    
    // 对于特殊类型，执行专门的转换逻辑
    switch (targetTypeId) {
        case llvmpy::PY_TYPE_INT:
            return py_convert_any_to_int(obj);
            
        case llvmpy::PY_TYPE_DOUBLE:
            return py_convert_any_to_double(obj);
            
        case llvmpy::PY_TYPE_BOOL:
            return py_convert_any_to_bool(obj);
            
        case llvmpy::PY_TYPE_STRING:
            return py_convert_any_to_string(obj);
            
        default:
            // 对于容器类型和未知类型，保留原始类型
            if (sourceTypeId == llvmpy::PY_TYPE_LIST || 
                sourceTypeId == llvmpy::PY_TYPE_DICT ||
                targetTypeId == llvmpy::PY_TYPE_LIST ||
                targetTypeId == llvmpy::PY_TYPE_DICT) {
                py_incref(obj);
                return obj;
            }
            
            // 默认情况，创建一个默认值
            fprintf(stderr, "警告: 未处理的类型转换 %d -> %d\n", sourceTypeId, targetTypeId);
            switch (targetTypeId) {
                case llvmpy::PY_TYPE_INT:
                    return py_create_int(0);
                case llvmpy::PY_TYPE_DOUBLE:
                    return py_create_double(0.0);
                case llvmpy::PY_TYPE_BOOL:
                    return py_create_bool(false);
                case llvmpy::PY_TYPE_STRING:
                    return py_create_string("");
                default:
                    py_incref(obj);
                    return obj;
            }
    }
}

// 从指针获取实际对象类型
int py_get_actual_type(PyObject* obj) {
    if (!obj) return llvmpy::PY_TYPE_NONE;
    
    // 处理指针类型
    if (obj->typeId >= 400) { // 指针类型范围
        PyObject** ptrObj = (PyObject**)obj;
        if (ptrObj && *ptrObj) {
            return (*ptrObj)->typeId;
        }
    }
    
    return obj->typeId;
}


// 类型ID转字符串，用于调试
const char* py_type_id_to_string(int typeId) {
    switch (llvmpy::getBaseTypeId(typeId)) {
        case llvmpy::PY_TYPE_NONE: return "none";
        case llvmpy::PY_TYPE_INT: return "int";
        case llvmpy::PY_TYPE_DOUBLE: return "double";
        case llvmpy::PY_TYPE_BOOL: return "bool";
        case llvmpy::PY_TYPE_STRING: return "string";
        case llvmpy::PY_TYPE_LIST: return "list";
        case llvmpy::PY_TYPE_DICT: return "dict";
        case llvmpy::PY_TYPE_ANY: return "any";
        case llvmpy::PY_TYPE_TUPLE: return "tuple";
        default: return "unknown";
    }
}

// 安全获取对象的类型ID
int py_get_safe_type_id(PyObject* obj) {
    if (!obj) return llvmpy::PY_TYPE_NONE;
    return obj->typeId;
}

// 为索引操作设置结果类型元数据的函数 - 安全处理所有类型
void py_set_index_result_type(PyObject* result, int typeId) {
    if (!result) return;
    
    // 设置类型ID
    result->typeId = typeId;
    
    // 打印调试信息
    #ifdef DEBUG
    fprintf(stderr, "设置索引结果类型: %s (ID: %d)\n", py_type_id_to_string(typeId), typeId);
    #endif
}
int py_extract_constant_int(PyObject* obj) {
    if (!obj) return 0;
    
    // 处理基本类型
    switch (obj->typeId) {
        case llvmpy::PY_TYPE_INT:
            return ((PyPrimitiveObject*)obj)->value.intValue;
        case llvmpy::PY_TYPE_BOOL:
            return ((PyPrimitiveObject*)obj)->value.boolValue ? 1 : 0;
        case llvmpy::PY_TYPE_DOUBLE: {
            double val = ((PyPrimitiveObject*)obj)->value.doubleValue;
            return (int)val;
        }
        case llvmpy::PY_TYPE_STRING: {
            try {
                const char* str = ((PyPrimitiveObject*)obj)->value.stringValue;
                return str ? atoi(str) : 0;
            } catch (...) {
                return 0;
            }
        }
        default:
            return 0;
    }
}

// 获取对象的类型ID - 运行时函数
int py_get_type_id(PyObject* obj) {
    return obj ? obj->typeId : llvmpy::PY_TYPE_NONE;
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


// 获取列表元素并返回其类型ID - 用于索引操作
PyObject* py_list_get_item_with_type(PyObject* list, int index, int* out_type_id)
{
    if (!list || !out_type_id) {
        if (out_type_id) *out_type_id = llvmpy::PY_TYPE_NONE;
        return py_get_none();
    }
    
    // 确保是列表类型
    if (list->typeId != llvmpy::PY_TYPE_LIST) {
        fprintf(stderr, "类型错误: 对象不是列表 (类型ID: %d)\n", list->typeId);
        *out_type_id = llvmpy::PY_TYPE_NONE;
        return py_get_none();
    }
    
    PyListObject* listObj = (PyListObject*)list;
    
    // 边界检查
    if (index < 0 || index >= listObj->length) {
        fprintf(stderr, "索引错误: 列表索引 %d 超出范围 [0, %d)\n", index, listObj->length);
        *out_type_id = llvmpy::PY_TYPE_NONE;
        return py_get_none();
    }
    
    // 获取元素
    PyObject* item = listObj->data[index];
    
    // 设置类型ID
    if (item) {
        *out_type_id = item->typeId;
        
        // 如果列表有元素类型信息但元素没有明确类型，使用列表的元素类型
        if (listObj->elemTypeId > 0 && listObj->elemTypeId != llvmpy::PY_TYPE_ANY) {
            *out_type_id = listObj->elemTypeId;
        }
    } else {
        *out_type_id = llvmpy::PY_TYPE_NONE;
    }
    
    // 增加引用计数并返回
    if (item) py_incref(item);
    return item ? item : py_get_none();
}

// 获取字典值并返回其类型ID - 用于索引操作
PyObject* py_dict_get_item_with_type(PyObject* dict, PyObject* key, int* out_type_id)
{
    if (!dict || !key || !out_type_id) {
        if (out_type_id) *out_type_id = llvmpy::PY_TYPE_NONE;
        return py_get_none();
    }
    
    // 确保是字典类型
    if (dict->typeId != llvmpy::PY_TYPE_DICT) {
        fprintf(stderr, "类型错误: 对象不是字典 (类型ID: %d)\n", dict->typeId);
        *out_type_id = llvmpy::PY_TYPE_NONE;
        return py_get_none();
    }
    
    PyDictObject* dictObj = (PyDictObject*)dict;
    
    // 查找条目
    PyDictEntry* entry = py_dict_find_entry(dictObj, key);
    if (!entry || !entry->used || !entry->value) {
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

    int py_object_len(PyObject* obj)
    {
        if (!obj) return 0;
        
        // 根据对象类型获取长度
        switch (obj->typeId) {
            case llvmpy::PY_TYPE_LIST:
                return ((PyListObject*)obj)->length;
                
            case llvmpy::PY_TYPE_DICT:
                return ((PyDictObject*)obj)->size;
                
            case llvmpy::PY_TYPE_STRING:
                return ((PyPrimitiveObject*)obj)->value.stringValue ? 
                       strlen(((PyPrimitiveObject*)obj)->value.stringValue) : 0;
                
            default:
                return 0;
        }
    }


    // 获取列表元素
    PyObject* py_list_get_item(PyObject* list, int index)
{
    if (!list) {
        fprintf(stderr, "错误: 尝试从NULL列表获取元素\n");
        return py_get_none();
    }
    
    // 确保是列表类型
    if (list->typeId != llvmpy::PY_TYPE_LIST) {
        fprintf(stderr, "类型错误: 对象不是列表 (类型ID: %d)\n", list->typeId);
        return py_get_none();
    }
    
    PyListObject* listObj = (PyListObject*)list;
    
    // 边界检查
    if (index < 0 || index >= listObj->length) {
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