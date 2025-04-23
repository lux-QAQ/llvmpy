#include "RunTime/runtime.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ratio>

using namespace llvmpy;

// 前向声明内部使用的辅助函数
static void py_print_object_inline(PyObject* obj);



// 打印Python对象函数实现
void py_print_object(PyObject* obj)
{
    if (!obj)
    {
        printf("None\n");
        return;
    }

    // 根据类型打印不同形式
    int typeId = py_get_safe_type_id(obj); // 使用安全获取类型ID的函数
    #ifdef DEBUG_RUNTIME_print
    printf("py_print_object: typeId = %d\n", typeId);
    #endif
    switch (typeId)
    {
        case llvmpy::PY_TYPE_INT:
            // 使用 GMP 打印整数
            gmp_printf("%Zd\n", ((PyPrimitiveObject*)obj)->value.intValue);
            break;

            case llvmpy::PY_TYPE_DOUBLE:
            {
                mpf_ptr val = ((PyPrimitiveObject*)obj)->value.doubleValue;
                // 检查浮点数是否为整数
                if (mpf_integer_p(val)) {
                    // 如果是整数，打印为 x.0 的形式
                    // 先打印整数部分，再打印 ".0"
            
                    gmp_printf("%.0Ff.0\n", val);
                } else {
                    // 否则，使用通用格式打印

                    gmp_printf("%Fg\n", val);
                }
            }
            break;

        case llvmpy::PY_TYPE_BOOL:
            // bool 类型保持不变
            printf("%s\n", ((PyPrimitiveObject*)obj)->value.boolValue ? "True" : "False");
            break;

        case llvmpy::PY_TYPE_STRING:
            // 字符串打印时不带引号
            printf("%s\n", ((PyPrimitiveObject*)obj)->value.stringValue ? ((PyPrimitiveObject*)obj)->value.stringValue : "");
            break;

        case llvmpy::PY_TYPE_LIST:
        {
            PyListObject* list = (PyListObject*)obj;
            printf("[");
            for (int i = 0; i < list->length; i++)
            {
                if (i > 0) printf(", ");
                py_print_object_inline(list->data[i]); // 递归调用内联打印
            }
            printf("]\n");
            break;
        }

        case llvmpy::PY_TYPE_DICT:
        {
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
            break;
        }

        case llvmpy::PY_TYPE_NONE:
            printf("None\n");
            break;

        case llvmpy::PY_TYPE_FUNC:
            // TODO: 打印更详细的函数信息 (如名称)
            printf("<function object at %p>\n", (void*)obj);
            break;

        // 可以添加对 Class 和 Instance 的处理
        case llvmpy::PY_TYPE_CLASS:
             printf("<class '%s'>\n", ((PyClassObject*)obj)->name ? ((PyClassObject*)obj)->name : "unknown");
             break;
        // case llvmpy::PY_TYPE_INSTANCE: // 实例的打印通常调用 __repr__ 或 __str__

        default:
            // 尝试获取基类类型
            int baseTypeId = py_get_base_type_id(typeId);
            if (baseTypeId == llvmpy::PY_TYPE_LIST) // 处理派生列表类型
            {
                PyListObject* list = (PyListObject*)obj;
                printf("[");
                for (int i = 0; i < list->length; i++)
                {
                    if (i > 0) printf(", ");
                    py_print_object_inline(list->data[i]);
                }
                printf("]\n");
            }
            else if (baseTypeId == llvmpy::PY_TYPE_DICT) // 处理派生字典类型
            {
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
            else if (baseTypeId == llvmpy::PY_TYPE_INSTANCE) // 处理实例类型
            {
                 // 尝试调用 __repr__ 或 __str__ (如果实现了方法调用)
                 // PyObject* repr_str = py_call_method(obj, "__repr__", 0, NULL);
                 // if (repr_str) { ... print repr_str ...; py_decref(repr_str); } else { ... fallback ... }
                 // Fallback:
                 PyClassObject* cls = ((PyInstanceObject*)obj)->cls;
                 printf("<%s object at %p>\n", cls && cls->name ? cls->name : "object", (void*)obj);
            }
            else
            {
                // 未知类型
                printf("<%s object at %p>\n", py_type_name(typeId), (void*)obj);
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

    int typeId = py_get_safe_type_id(obj);
    switch (typeId)
    {
        case llvmpy::PY_TYPE_INT:
            // 使用 GMP 打印整数到 stdout
            mpz_out_str(stdout, 10, ((PyPrimitiveObject*)obj)->value.intValue);
            break;

            case llvmpy::PY_TYPE_DOUBLE:
            {
                mpf_ptr val = ((PyPrimitiveObject*)obj)->value.doubleValue;
                // 检查浮点数是否为整数
                if (mpf_integer_p(val)) {
                    // 如果是整数，打印为 x.0 的形式
                    gmp_printf("%.0Ff.0", val);
                } else {
                    // 否则，使用通用格式打印
                    gmp_printf("%Fg", val);
                }
            }
            break;

        case llvmpy::PY_TYPE_BOOL:
            printf("%s", ((PyPrimitiveObject*)obj)->value.boolValue ? "True" : "False");
            break;

        case llvmpy::PY_TYPE_STRING:
            // 字符串内联打印时带引号 (模拟 repr)
            printf("'%s'", ((PyPrimitiveObject*)obj)->value.stringValue ? ((PyPrimitiveObject*)obj)->value.stringValue : "");
            // 或者使用双引号: printf("\"%s\"", ...);
            break;

        case llvmpy::PY_TYPE_LIST:
        {
            PyListObject* list = (PyListObject*)obj;
            printf("[");
            for (int i = 0; i < list->length; i++)
            {
                if (i > 0) printf(", ");
                py_print_object_inline(list->data[i]); // 递归调用
            }
            printf("]");
            break;
        }

        case llvmpy::PY_TYPE_DICT:
        {
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

        case llvmpy::PY_TYPE_NONE:
            printf("None");
            break;

        case llvmpy::PY_TYPE_FUNC:
            printf("<function object at %p>", (void*)obj);
            break;

        case llvmpy::PY_TYPE_CLASS:
             printf("<class '%s'>", ((PyClassObject*)obj)->name ? ((PyClassObject*)obj)->name : "unknown");
             break;

        default:
             // 尝试获取基类类型
            int baseTypeId = py_get_base_type_id(typeId);
            if (baseTypeId == llvmpy::PY_TYPE_LIST) // 处理派生列表类型
            {
                PyListObject* list = (PyListObject*)obj;
                printf("[");
                for (int i = 0; i < list->length; i++)
                {
                    if (i > 0) printf(", ");
                    py_print_object_inline(list->data[i]);
                }
                printf("]");
            }
            else if (baseTypeId == llvmpy::PY_TYPE_DICT) // 处理派生字典类型
            {
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
            else if (baseTypeId == llvmpy::PY_TYPE_INSTANCE) // 处理实例类型
            {
                 // Fallback representation for inline printing
                 PyClassObject* cls = ((PyInstanceObject*)obj)->cls;
                 printf("<%s object at %p>", cls && cls->name ? cls->name : "object", (void*)obj);
            }
            else
            {
                // 未知类型
                printf("<%s object at %p>", py_type_name(typeId), (void*)obj);
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
    if (value == (long long)value) // Use long long for wider range check
    {
        printf("%lld\n", (long long)value);
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
    // 直接打印 C 字符串，通常用于字面量
    if (value)
    {
        printf("%s\n", value);
    }
    else
    {
        printf("None\n"); // 或者打印空行？保持 None 吧
    }
}

// 输入函数
char* input(void)
{
    const int MAX_INPUT_SIZE = 1024;
    char* buffer = (char*)malloc(MAX_INPUT_SIZE);

    if (!buffer)
    {
        fprintf(stderr, "MemoryError: Failed to allocate buffer for input()\n");
        return NULL;  // 内存分配失败
    }

    // 读取一行输入
    if (fgets(buffer, MAX_INPUT_SIZE, stdin))
    {
        // 移除末尾的换行符
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n')
        {
            buffer[len - 1] = '\0';
        }
        // Handle potential buffer overflow if input exceeds MAX_INPUT_SIZE - 1
        // (fgets prevents buffer overflow, but the line might be truncated)
    }
    else
    {
        // 读取失败或 EOF
        if (feof(stdin)) {
             fprintf(stderr, "EOFError: EOF when reading a line\n");
             // Decide behavior: return NULL, empty string, or raise specific error
             free(buffer);
             return NULL; // Indicate EOF error
        } else if (ferror(stdin)) {
             perror("Error reading input");
             free(buffer);
             return NULL; // Indicate read error
        }
        // If fgets returns NULL without EOF or error, treat as empty input?
        buffer[0] = '\0';
    }

    return buffer;
}