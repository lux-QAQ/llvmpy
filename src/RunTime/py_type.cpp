
#include "RunTime/runtime.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <climits>
#include <cctype>

using namespace llvmpy;
//===----------------------------------------------------------------------===//
// 类型检查函数
//===----------------------------------------------------------------------===//

// 检查对象类型是否匹配
bool py_check_type(PyObject* obj, int expectedTypeId)
{
    if (!obj)
        return false;

    // 获取对象的实际类型ID
    int actualTypeId = obj->typeId;

    // 如果期望类型是ANY，则任何类型都匹配
    if (expectedTypeId == PY_TYPE_ANY)
        return true;

    // 如果实际类型是ANY，则也匹配
    if (actualTypeId == PY_TYPE_ANY)
        return true;

    // 检查基本类型匹配
    if (getBaseTypeId(actualTypeId) == getBaseTypeId(expectedTypeId))
        return true;

    // 检查类型兼容性
    return py_are_types_compatible(actualTypeId, expectedTypeId);
}

// 确保对象类型匹配，否则返回错误
PyObject* py_ensure_type(PyObject* obj, int expectedTypeId)
{
    if (!obj)
    {
        fprintf(stderr, "Error: NULL object passed to py_ensure_type\n");
        return NULL;
    }

    if (!py_check_type(obj, expectedTypeId))
    {
        py_type_error(obj, expectedTypeId);
        return NULL;
    }

    return obj;
}

// 检查两个类型是否兼容（可相互转换或赋值）
bool py_are_types_compatible(int typeIdA, int typeIdB)
{
    // 如果两个类型相同，当然兼容
    if (getBaseTypeId(typeIdA) == getBaseTypeId(typeIdB))
        return true;

    // ANY类型与任何类型兼容
    if (typeIdA == PY_TYPE_ANY || typeIdB == PY_TYPE_ANY)
        return true;

    // 数值类型之间兼容
    bool isANumeric = typeIdA == PY_TYPE_INT || typeIdA == PY_TYPE_DOUBLE;
    bool isBNumeric = typeIdB == PY_TYPE_INT || typeIdB == PY_TYPE_DOUBLE;
    if (isANumeric && isBNumeric)
        return true;

    // 列表相关类型检查
    if (getBaseTypeId(typeIdA) == PY_TYPE_LIST && getBaseTypeId(typeIdB) == PY_TYPE_LIST)
        return true;

    // 字典相关类型检查
    if (getBaseTypeId(typeIdA) == PY_TYPE_DICT && getBaseTypeId(typeIdB) == PY_TYPE_DICT)
        return true;

    // 其他类型暂不兼容
    return false;
}

// 确保类型兼容性，这是py_check_type的更严格版本
bool py_ensure_type_compatibility(PyObject* obj, int expectedTypeId)
{
    if (!obj)
        return false;

    int actualTypeId = obj->typeId;

    // 如果类型兼容，返回成功
    if (py_are_types_compatible(actualTypeId, expectedTypeId))
        return true;

    // 否则尝试类型转换
    switch (expectedTypeId)
    {
        case PY_TYPE_INT:
            if (actualTypeId == PY_TYPE_DOUBLE || actualTypeId == PY_TYPE_BOOL)
                return true;
            break;

        case PY_TYPE_DOUBLE:
            if (actualTypeId == PY_TYPE_INT || actualTypeId == PY_TYPE_BOOL)
                return true;
            break;

        case PY_TYPE_BOOL:
            // 任何类型都可以转为布尔值
            return true;

        case PY_TYPE_STRING:
            // 暂不支持到字符串的自动转换
            break;

        default:
            if (getBaseTypeId(expectedTypeId) == PY_TYPE_LIST || getBaseTypeId(expectedTypeId) == PY_TYPE_DICT)
            {
                return py_is_container(obj);
            }
    }

    // 转换失败，报告类型错误
    py_type_error(obj, expectedTypeId);
    return false;
}

// 类型错误报告
void py_type_error(PyObject* obj, int expectedTypeId)
{
    const char* actual = py_type_name(obj->typeId);
    const char* expected = py_type_name(expectedTypeId);

    fprintf(stderr, "TypeError: Expected %s but got %s\n", expected, actual);
}

//===----------------------------------------------------------------------===//
// 类型信息函数
//===----------------------------------------------------------------------===//

// 获取类型名称
const char* py_type_name(int typeId)
{
    switch (getBaseTypeId(typeId))
    {
        case PY_TYPE_NONE:
            return "None";
        case PY_TYPE_INT:
            return "int";
        case PY_TYPE_DOUBLE:
            return "float";
        case PY_TYPE_BOOL:
            return "bool";
        case PY_TYPE_STRING:
            return "str";
        case PY_TYPE_LIST:
            return "list";
        case PY_TYPE_DICT:
            return "dict";
        case PY_TYPE_ANY:
            return "any";
        case PY_TYPE_TUPLE:
            return "tuple";
        case PY_TYPE_FUNC:
            return "function";
        default:
            return "unknown";
    }
}

// 获取基本类型ID
int py_get_base_type_id(int typeId)
{
    return getBaseTypeId(typeId);
}

// 获取对象的类型ID
int py_get_object_type_id(PyObject* obj)
{
    return obj ? obj->typeId : PY_TYPE_NONE;
}

// 获取对象类型ID，考虑可能的指针解引用
int py_get_type_id(PyObject* obj)
{
    if (!obj) return PY_TYPE_NONE;

    // 如果类型ID在指针范围，尝试解引用
    if (obj->typeId >= PY_TYPE_PTR)
    {
        PyObject** ptrObj = (PyObject**)obj;
        if (ptrObj && *ptrObj)
            return (*ptrObj)->typeId;
    }

    return obj->typeId;
}

// 将类型ID转换为字符串表示
const char* py_type_id_to_string(int typeId)
{
    static char buffer[128];

    switch (getBaseTypeId(typeId))
    {
        case PY_TYPE_NONE:
            return "None";
        case PY_TYPE_INT:
            return "int";
        case PY_TYPE_DOUBLE:
            return "float";
        case PY_TYPE_BOOL:
            return "bool";
        case PY_TYPE_STRING:
            return "str";
        case PY_TYPE_LIST:
        {
            if (typeId == PY_TYPE_LIST)
                return "list";

            // 提取元素类型
            int elemTypeId = typeId - PY_TYPE_LIST_BASE;
            snprintf(buffer, sizeof(buffer), "list[%s]", py_type_id_to_string(elemTypeId));
            return buffer;
        }
        case PY_TYPE_DICT:
        {
            if (typeId == PY_TYPE_DICT)
                return "dict";

            // 提取键类型
            int keyTypeId = typeId - PY_TYPE_DICT_BASE;
            snprintf(buffer, sizeof(buffer), "dict[%s]", py_type_id_to_string(keyTypeId));
            return buffer;
        }
        case PY_TYPE_ANY:
            return "any";
        default:
        {
            snprintf(buffer, sizeof(buffer), "unknown_type(%d)", typeId);
            return buffer;
        }
    }
}

// 获取安全的类型ID（处理NULL和指针）
int py_get_safe_type_id(PyObject* obj)
{
    if (!obj) return PY_TYPE_NONE;

    // 处理指针
    if (obj->typeId >= PY_TYPE_PTR)
    {
        PyObject** ptrObj = (PyObject**)obj;
        if (ptrObj && *ptrObj)
            return (*ptrObj)->typeId;
        return PY_TYPE_NONE;
    }

    return obj->typeId;
}

//===----------------------------------------------------------------------===//
// 类型特性检查函数
//===----------------------------------------------------------------------===//

// 检查对象是否为容器类型
bool py_is_container(PyObject* obj)
{
    if (!obj) return false;

    int typeId = obj->typeId;
    int baseTypeId = getBaseTypeId(typeId);

    return (baseTypeId == PY_TYPE_LIST || baseTypeId == PY_TYPE_DICT || baseTypeId == PY_TYPE_TUPLE || (typeId >= PY_TYPE_LIST_BASE && typeId < PY_TYPE_DICT_BASE) || (typeId >= PY_TYPE_DICT_BASE && typeId < PY_TYPE_FUNC_BASE));
}

// 检查对象是否为序列类型
bool py_is_sequence(PyObject* obj)
{
    if (!obj) return false;

    int typeId = obj->typeId;
    int baseTypeId = getBaseTypeId(typeId);

    return (baseTypeId == PY_TYPE_LIST || baseTypeId == PY_TYPE_TUPLE || baseTypeId == PY_TYPE_STRING || (typeId >= PY_TYPE_LIST_BASE && typeId < PY_TYPE_DICT_BASE));
}

// 将Python对象转换为布尔值
bool py_object_to_bool(PyObject* obj)
{
    if (!obj) return false;

    switch (getBaseTypeId(obj->typeId))
    {
        case PY_TYPE_BOOL:
            return ((PyPrimitiveObject*)obj)->value.boolValue;

        case PY_TYPE_INT:
            return ((PyPrimitiveObject*)obj)->value.intValue != 0;

        case PY_TYPE_DOUBLE:
            return ((PyPrimitiveObject*)obj)->value.doubleValue != 0.0;

        case PY_TYPE_STRING:
        {
            const char* str = ((PyPrimitiveObject*)obj)->value.stringValue;
            return str && *str != '\0';
        }

        case PY_TYPE_LIST:
        {
            PyListObject* list = (PyListObject*)obj;
            return list->length > 0;
        }

        case PY_TYPE_DICT:
        {
            PyDictObject* dict = (PyDictObject*)obj;
            return dict->size > 0;
        }

        case PY_TYPE_NONE:
            return false;

        default:
            // 其他类型默认为true
            return true;
    }
}

//===----------------------------------------------------------------------===//
// 类型提取函数
//===----------------------------------------------------------------------===//

// 从整数对象提取整数值
int py_extract_int(PyObject* obj)
{
    if (!obj || obj->typeId != PY_TYPE_INT)
    {
        fprintf(stderr, "TypeError: Expected int\n");
        return 0;
    }

    return ((PyPrimitiveObject*)obj)->value.intValue;
}

// 从浮点数对象提取浮点值
double py_extract_double(PyObject* obj)
{
    if (!obj || obj->typeId != PY_TYPE_DOUBLE)
    {
        fprintf(stderr, "TypeError: Expected float\n");
        return 0.0;
    }

    return ((PyPrimitiveObject*)obj)->value.doubleValue;
}

// 从布尔对象提取布尔值
bool py_extract_bool(PyObject* obj)
{
    if (!obj || obj->typeId != PY_TYPE_BOOL)
    {
        fprintf(stderr, "TypeError: Expected bool\n");
        return false;
    }

    return ((PyPrimitiveObject*)obj)->value.boolValue;
}

// 从字符串对象提取字符串
const char* py_extract_string(PyObject* obj)
{
    if (!obj || obj->typeId != PY_TYPE_STRING)
    {
        fprintf(stderr, "TypeError: Expected string\n");
        return "";
    }

    return ((PyPrimitiveObject*)obj)->value.stringValue ? ((PyPrimitiveObject*)obj)->value.stringValue : "";
}

// 从任意对象中提取整数值
/* int py_extract_int_from_any(PyObject* obj)
{
    if (!obj) {
        fprintf(stderr, "Error: Cannot extract int from NULL object\n");
        return 0; // 返回默认值而不是NULL
    }
    
    switch (obj->typeId)
    {
        case PY_TYPE_INT:
            return ((PyPrimitiveObject*)obj)->value.intValue;
            
            case PY_TYPE_DOUBLE: {
                // 正确处理浮点数到整数的转换
                double val = ((PyPrimitiveObject*)obj)->value.doubleValue;
                return (int)val; // 明确转换
            }
            
        case PY_TYPE_BOOL:
            return ((PyPrimitiveObject*)obj)->value.boolValue ? 1 : 0;
            
        case PY_TYPE_STRING:
        {
            const char* str = ((PyPrimitiveObject*)obj)->value.stringValue;
            if (!str || *str == '\0') return 0;
            
            // 尝试将字符串转换为整数
            char* end;
            long val = strtol(str, &end, 10);
            
            // 检查转换是否成功
            if (end == str || *end != '\0')
            {
                fprintf(stderr, "ValueError: Could not convert string to int: '%s'\n", str);
                return 0;
            }
            
            return (int)val;
        }
            
        default:
            fprintf(stderr, "TypeError: Cannot convert object of type %s to int\n", 
                   py_type_name(obj->typeId));
            return 0;
    }
} */

PyObject* py_extract_int_from_any(PyObject* obj)
{
    if (!obj)
    {
        fprintf(stderr, "Error: Cannot extract int from NULL object\n");
        return NULL;
    }

    int value = 0;

    switch (obj->typeId)
    {
        case PY_TYPE_INT:
            // 修复: py_incref 返回 void，不能作为函数返回值
            py_incref(obj);  // 先增加引用计数
            return obj;      // 再返回对象

        case PY_TYPE_DOUBLE:
        {
            // 浮点数到整数的转换
            double val = ((PyPrimitiveObject*)obj)->value.doubleValue;
            value = (int)val;
            break;
        }

        case PY_TYPE_BOOL:
            value = ((PyPrimitiveObject*)obj)->value.boolValue ? 1 : 0;
            break;

        case PY_TYPE_STRING:
        {
            const char* str = ((PyPrimitiveObject*)obj)->value.stringValue;
            if (!str)
                value = 0;
            else
                value = atoi(str);
            break;
        }

        default:
            fprintf(stderr, "Warning: Cannot extract int from type %d\n", obj->typeId);
            return NULL;
    }

    // 创建新的整数对象
    return py_create_int(value);
}
// 尝试从常量中提取整数值
int py_extract_constant_int(PyObject* obj)
{
    if (!obj) return 0;

    // 处理整数常量
    if (obj->typeId == PY_TYPE_INT)
        return ((PyPrimitiveObject*)obj)->value.intValue;

    // 处理布尔常量
    if (obj->typeId == PY_TYPE_BOOL)
        return ((PyPrimitiveObject*)obj)->value.boolValue ? 1 : 0;

    // 其他类型无法提取为常量整数
    fprintf(stderr, "TypeError: Cannot extract constant int from type %s\n",
            py_type_name(obj->typeId));
    return 0;
}

//===----------------------------------------------------------------------===//
// 类型转换函数
//===----------------------------------------------------------------------===//

// 将整数对象转换为浮点数对象
PyObject* py_convert_int_to_double(PyObject* obj)
{
    if (!obj || obj->typeId != PY_TYPE_INT)
    {
        fprintf(stderr, "TypeError: Expected int for conversion to float\n");
        return NULL;
    }

    int intValue = ((PyPrimitiveObject*)obj)->value.intValue;
    return py_create_double((double)intValue);
}

// 将浮点数对象转换为整数对象
PyObject* py_convert_double_to_int(PyObject* obj)
{
    if (!obj || obj->typeId != PY_TYPE_DOUBLE)
    {
        fprintf(stderr, "TypeError: Expected float for conversion to int\n");
        return NULL;
    }

    double doubleValue = ((PyPrimitiveObject*)obj)->value.doubleValue;
    return py_create_int((int)doubleValue);
}

// 将对象转换为布尔对象
PyObject* py_convert_to_bool(PyObject* obj)
{
    if (!obj)
        return py_create_bool(false);

    bool boolValue = py_object_to_bool(obj);
    return py_create_bool(boolValue);
}

// 将对象转换为字符串对象
PyObject* py_convert_to_string(PyObject* obj)
{
    if (!obj)
        return py_create_string("None");

    char buffer[256];

    switch (getBaseTypeId(obj->typeId))
    {
        case PY_TYPE_NONE:
            return py_create_string("None");

        case PY_TYPE_INT:
        {
            int val = ((PyPrimitiveObject*)obj)->value.intValue;
            snprintf(buffer, sizeof(buffer), "%d", val);
            return py_create_string(buffer);
        }

        case PY_TYPE_DOUBLE:
        {
            double val = ((PyPrimitiveObject*)obj)->value.doubleValue;
            snprintf(buffer, sizeof(buffer), "%g", val);
            return py_create_string(buffer);
        }

        case PY_TYPE_BOOL:
        {
            bool val = ((PyPrimitiveObject*)obj)->value.boolValue;
            return py_create_string(val ? "True" : "False");
        }

        case PY_TYPE_STRING:
            // 字符串对象直接复制
            return py_object_copy(obj, PY_TYPE_STRING);

        default:
        {
            // 其他类型使用类型名称
            const char* typeName = py_type_name(obj->typeId);
            snprintf(buffer, sizeof(buffer), "<%s object at %p>", typeName, (void*)obj);
            return py_create_string(buffer);
        }
    }
}

// 将任意对象转换为整数对象
PyObject* py_convert_any_to_int(PyObject* obj)
{
    if (!obj)
        return py_create_int(0);

    // 修复: py_extract_int_from_any 现在返回 PyObject* 而不是 int
    PyObject* result = py_extract_int_from_any(obj);
    if (result)
    {
        return result;  // 直接返回结果，不需要再次创建整数对象
    }
    else
    {
        return py_create_int(0);  // 如果转换失败，返回默认值
    }
}

// 将任意对象转换为浮点数对象
PyObject* py_convert_any_to_double(PyObject* obj)
{
    if (!obj)
        return py_create_double(0.0);

    double doubleValue = 0.0;

    switch (obj->typeId)
    {
        case PY_TYPE_INT:
            doubleValue = (double)((PyPrimitiveObject*)obj)->value.intValue;
            break;

        case PY_TYPE_DOUBLE:
            doubleValue = ((PyPrimitiveObject*)obj)->value.doubleValue;
            break;

        case PY_TYPE_BOOL:
            doubleValue = ((PyPrimitiveObject*)obj)->value.boolValue ? 1.0 : 0.0;
            break;

        case PY_TYPE_STRING:
        {
            const char* str = ((PyPrimitiveObject*)obj)->value.stringValue;
            if (str && *str != '\0')
            {
                char* end;
                doubleValue = strtod(str, &end);

                if (end == str || *end != '\0')
                {
                    fprintf(stderr, "ValueError: Could not convert string to float: '%s'\n", str);
                }
            }
            break;
        }

        default:
            fprintf(stderr, "TypeError: Cannot convert %s to float\n", py_type_name(obj->typeId));
    }

    return py_create_double(doubleValue);
}

// 将任意对象转换为布尔对象
PyObject* py_convert_any_to_bool(PyObject* obj)
{
    return py_convert_to_bool(obj);
}

// 将任意对象转换为字符串对象
PyObject* py_convert_any_to_string(PyObject* obj)
{
    return py_convert_to_string(obj);
}

// 将对象转换为any类型对象（保持原类型）
PyObject* py_convert_to_any(PyObject* obj)
{
    if (!obj)
        return py_get_none();

    // 对于ANY类型，我们只是增加引用计数并返回
    py_incref(obj);
    return obj;
}

// 保留对象类型但转换为ANY类型
PyObject* py_convert_any_preserve_type(PyObject* obj)
{
    if (!obj)
        return py_get_none();

    // 创建原始对象的新副本
    PyObject* copy = py_object_copy(obj, obj->typeId);
    if (!copy)
        return py_get_none();

    return copy;
}

// 保留参数类型（用于函数调用）
PyObject* py_preserve_parameter_type(PyObject* obj)
{
    if (!obj)
        return py_get_none();

    // 增加引用计数并返回
    py_incref(obj);
    return obj;
}

// 智能类型转换（尝试找到最佳转换路径）
/**
 * @brief Attempts to convert an object to a target type ID based on predefined rules.
 *
 * Returns a *new* object on successful conversion, or the *original* object
 * (with incremented ref count) if no conversion is needed.
 * Returns NULL if conversion is not possible or fails.
 * Caller is responsible for decref'ing the returned object when done.
 *
 * @param obj The object to convert.
 * @param targetTypeId The desired target type ID.
 * @return Converted object (new or original+incref) or NULL.
 */
PyObject* py_smart_convert(PyObject* obj, int targetTypeId)
{
    if (!obj) return NULL;  // Cannot convert NULL

    int sourceTypeId = py_get_safe_type_id(obj);
    int baseSourceTypeId = llvmpy::getBaseTypeId(sourceTypeId);
    int baseTargetTypeId = llvmpy::getBaseTypeId(targetTypeId);

    // 1. Check if types are already compatible (or identical)
    if (py_are_types_compatible(sourceTypeId, targetTypeId))
    {
        py_incref(obj);  // No conversion needed, return original with incremented ref count
        return obj;
    }

    // 2. Implement specific conversion rules (mimic TypeOperationRegistry)
    // Example rules:
    if (baseTargetTypeId == llvmpy::PY_TYPE_INT)
    {
        if (baseSourceTypeId == llvmpy::PY_TYPE_DOUBLE)
        {
            return py_convert_double_to_int(obj);  // Assumes this returns a new object
        }
        if (baseSourceTypeId == llvmpy::PY_TYPE_BOOL)
        {
            // Convert bool to int (0 or 1)
            bool boolVal = ((PyPrimitiveObject*)obj)->value.boolValue;
            return py_create_int(boolVal ? 1 : 0);  // Returns a new object
        }
        // Add string to int conversion if desired
    }
    else if (baseTargetTypeId == llvmpy::PY_TYPE_DOUBLE)
    {
        if (baseSourceTypeId == llvmpy::PY_TYPE_INT)
        {
            return py_convert_int_to_double(obj);  // Assumes this returns a new object
        }
        if (baseSourceTypeId == llvmpy::PY_TYPE_BOOL)
        {
            // Convert bool to double (0.0 or 1.0)
            bool boolVal = ((PyPrimitiveObject*)obj)->value.boolValue;
            return py_create_double(boolVal ? 1.0 : 0.0);  // Returns a new object
        }
        // Add string to double conversion if desired
    }
    else if (baseTargetTypeId == llvmpy::PY_TYPE_BOOL)
    {
        // Most types can convert to bool
        return py_convert_to_bool(obj);  // Assumes this returns a new object
    }
    else if (baseTargetTypeId == llvmpy::PY_TYPE_STRING)
    {
        // Convert common types to string
        return py_convert_to_string(obj);  // Assumes this returns a new object
    }
    // Add more rules for list/dict compatibility, custom types etc.

    // 3. If no rule matches, conversion fails
    // fprintf(stderr, "TypeError: Cannot convert type %s to %s\n", py_type_name(sourceTypeId), py_type_name(targetTypeId));
    return NULL;
}