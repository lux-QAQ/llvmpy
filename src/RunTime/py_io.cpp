#include "RunTime/runtime.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ratio>


using namespace llvmpy;

// 前向声明内部使用的辅助函数
static void py_print_object_inline(PyObject* obj);

// 打印字符串函数实现
void print(char* str) 
{
    if (str) 
    {
        printf("%s\n", str);
    } 
    else 
    {
        printf("None\n");
    }
}

// 打印数字函数实现
void print_number(double num) 
{
    // 检查数字是否为整数
    if (num == (int)num) 
    {
        printf("%d\n", (int)num);
    } 
    else 
    {
        printf("%g\n", num);
    }
}

// 打印Python对象函数实现
void py_print_object(PyObject* obj) 
{
    if (!obj) 
    {
        printf("None\n");
        return;
    }

    // 根据类型打印不同形式
    switch (obj->typeId) 
    {
        case PY_TYPE_INT:
            printf("%d\n", ((PyPrimitiveObject*)obj)->value.intValue);
            break;
        
        case PY_TYPE_DOUBLE:
            printf("%g\n", ((PyPrimitiveObject*)obj)->value.doubleValue);
            break;
        
        case PY_TYPE_BOOL:
            printf("%s\n", ((PyPrimitiveObject*)obj)->value.boolValue ? "True" : "False");
            break;
        
        case PY_TYPE_STRING:
            printf("%s\n", ((PyPrimitiveObject*)obj)->value.stringValue ? 
                  ((PyPrimitiveObject*)obj)->value.stringValue : "");
            break;
        
        case PY_TYPE_LIST: {
            PyListObject* list = (PyListObject*)obj;
            printf("[");
            for (int i = 0; i < list->length; i++) 
            {
                // 递归打印元素
                if (i > 0) printf(", ");
                py_print_object_inline(list->data[i]);
            }
            printf("]\n");
            break;
        }
        
        case PY_TYPE_DICT: {
            PyDictObject* dict = (PyDictObject*)obj;
            printf("{");
            bool first = true;
            
            // 遍历字典中的所有项
            for (int i = 0; i < dict->capacity; i++) 
            {
                if (dict->entries[i].used && dict->entries[i].key) 
                {
                    if (!first) printf(", ");
                    first = false;
                    
                    // 打印键值对
                    py_print_object_inline(dict->entries[i].key);
                    printf(": ");
                    py_print_object_inline(dict->entries[i].value);
                }
            }
            printf("}\n");
            break;
        }
        
        case PY_TYPE_NONE:
            printf("None\n");
            break;
        
        default:
            // 处理派生类型
            int baseTypeId = py_get_base_type_id(obj->typeId);
            if (baseTypeId == PY_TYPE_LIST) {
                PyListObject* list = (PyListObject*)obj;
                printf("[");
                for (int i = 0; i < list->length; i++) 
                {
                    if (i > 0) printf(", ");
                    py_print_object_inline(list->data[i]);
                }
                printf("]\n");
            }
            else if (baseTypeId == PY_TYPE_DICT) {
                PyDictObject* dict = (PyDictObject*)obj;
                printf("{");
                bool first = true;
                for (int i = 0; i < dict->capacity; i++) 
                {
                    if (dict->entries[i].used && dict->entries[i].key) 
                    {
                        if (!first) printf(", ");
                        first = false;
                        py_print_object_inline(dict->entries[i].key);
                        printf(": ");
                        py_print_object_inline(dict->entries[i].value);
                    }
                }
                printf("}\n");
            }
            else {
                printf("<object at %p>\n", (void*)obj);
            }
    }
}

// 辅助函数 - 内联打印对象（不打印换行符）
static void py_print_object_inline(PyObject* obj) 
{
    if (!obj) 
    {
        printf("None");
        return;
    }

    // 根据类型打印不同形式
    switch (obj->typeId) 
    {
        case PY_TYPE_INT:
            printf("%d", ((PyPrimitiveObject*)obj)->value.intValue);
            break;
        
        case PY_TYPE_DOUBLE:
            printf("%g", ((PyPrimitiveObject*)obj)->value.doubleValue);
            break;
        
        case PY_TYPE_BOOL:
            printf("%s", ((PyPrimitiveObject*)obj)->value.boolValue ? "True" : "False");
            break;
        
        case PY_TYPE_STRING:
            printf("\"%s\"", ((PyPrimitiveObject*)obj)->value.stringValue ? 
                  ((PyPrimitiveObject*)obj)->value.stringValue : "");
            break;
        
        case PY_TYPE_LIST: {
            PyListObject* list = (PyListObject*)obj;
            printf("[");
            for (int i = 0; i < list->length; i++) 
            {
                if (i > 0) printf(", ");
                py_print_object_inline(list->data[i]);
            }
            printf("]");
            break;
        }
        
        case PY_TYPE_DICT: {
            PyDictObject* dict = (PyDictObject*)obj;
            printf("{");
            bool first = true;
            
            for (int i = 0; i < dict->capacity; i++) 
            {
                if (dict->entries[i].used && dict->entries[i].key) 
                {
                    if (!first) printf(", ");
                    first = false;
                    
                    py_print_object_inline(dict->entries[i].key);
                    printf(": ");
                    py_print_object_inline(dict->entries[i].value);
                }
            }
            printf("}");
            break;
        }
        
        case PY_TYPE_NONE:
            printf("None");
            break;
        
        default:
            // 处理派生类型
            int baseTypeId = py_get_base_type_id(obj->typeId);
            if (baseTypeId == PY_TYPE_LIST) {
                PyListObject* list = (PyListObject*)obj;
                printf("[");
                for (int i = 0; i < list->length; i++) 
                {
                    if (i > 0) printf(", ");
                    py_print_object_inline(list->data[i]);
                }
                printf("]");
            }
            else if (baseTypeId == PY_TYPE_DICT) {
                PyDictObject* dict = (PyDictObject*)obj;
                printf("{");
                bool first = true;
                for (int i = 0; i < dict->capacity; i++) 
                {
                    if (dict->entries[i].used && dict->entries[i].key) 
                    {
                        if (!first) printf(", ");
                        first = false;
                        py_print_object_inline(dict->entries[i].key);
                        printf(": ");
                        py_print_object_inline(dict->entries[i].value);
                    }
                }
                printf("}");
            }
            else {
                printf("<object at %p>", (void*)obj);
            }
    }
}

// 直接打印基本数据类型
void py_print_int(int value) 
{
    printf("%d\n", value);
}

void py_print_double(double value) 
{
    // 检查是否为整数
    if (value == (int)value) 
    {
        printf("%d\n", (int)value);
    } 
    else 
    {
        printf("%g\n", value);
    }
}

void py_print_bool(bool value) 
{
    printf("%s\n", value ? "True" : "False");
}

void py_print_string(const char* value) 
{
    if (value) 
    {
        printf("%s\n", value);
    } 
    else 
    {
        printf("\n");
    }
}

// 输入函数
char* input(void) 
{
    const int MAX_INPUT_SIZE = 1024;
    char* buffer = (char*)malloc(MAX_INPUT_SIZE);
    
    if (!buffer) {
        return NULL; // 内存分配失败
    }
    
    // 读取一行输入
    if (fgets(buffer, MAX_INPUT_SIZE, stdin)) {
        // 移除末尾的换行符
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
    } else {
        // 读取失败，返回空字符串
        buffer[0] = '\0';
    }
    
    return buffer;
}