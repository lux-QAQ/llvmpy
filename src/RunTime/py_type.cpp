
#include "RunTime/runtime.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <climits>
#include <cctype>
#include <iostream> // For errors


using namespace llvmpy;
//===----------------------------------------------------------------------===//
// 类型检查函数
//===----------------------------------------------------------------------===//


static mpz_t gmp_bool_true;
static mpz_t gmp_bool_false;
static std::atomic_flag gmp_bool_initialized = ATOMIC_FLAG_INIT; // Flag for one-time init


void initialize_static_gmp_bools() {
    if (!gmp_bool_initialized.test_and_set()) { // Ensure one-time initialization
        mpz_init_set_si(gmp_bool_true, 1);
        mpz_init_set_si(gmp_bool_false, 0);
        // Note: These should ideally be cleared on runtime shutdown,
        // but for static duration objects, it might not be strictly necessary
        // unless dealing with memory checkers or specific shutdown requirements.
        // atexit(cleanup_static_gmp_bools); // Example cleanup registration
    }
}


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
#ifdef DEBUG_RUNTIME_py_are_types_compatible
    fprintf(stderr, "DEBUG: Checking compatibility between type %d and %d\n", typeIdA, typeIdB);
    int baseTypeIdA = getBaseTypeId(typeIdA);
    int baseTypeIdB = getBaseTypeId(typeIdB);
    fprintf(stderr, "DEBUG: Base type A: %d, Base type B: %d\n", baseTypeIdA, baseTypeIdB);

#endif
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
            // 使用 mpz_sgn 判断 GMP 整数是否为 0
            return mpz_sgn(((PyPrimitiveObject*)obj)->value.intValue) != 0;

        case PY_TYPE_DOUBLE:
            // 使用 mpf_sgn 判断 GMP 浮点数是否为 0
            // 注意：GMP 认为 0.0, -0.0 等都是 0
            return mpf_sgn(((PyPrimitiveObject*)obj)->value.doubleValue) != 0;

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
            // 其他类型默认为true (e.g., instances, functions)
            return true;
    }
}

//===----------------------------------------------------------------------===//
// 类型提取函数
//===----------------------------------------------------------------------===//

// 从整数对象提取整数值
mpz_ptr py_extract_int(PyObject* obj)
{
    if (!obj || obj->typeId != PY_TYPE_INT)
    {
        // fprintf(stderr, "TypeError: Expected int for extraction\n"); // Optional: suppress for performance?
        return NULL; // Return NULL on type mismatch or null object
    }
    // Return a direct pointer to the internal mpz_t
    return ((PyPrimitiveObject*)obj)->value.intValue;
}

// 从浮点数对象提取浮点值
mpf_ptr py_extract_double(PyObject* obj)
{
    if (!obj || obj->typeId != PY_TYPE_DOUBLE)
    {
        // fprintf(stderr, "TypeError: Expected float for extraction\n"); // Optional: suppress for performance?
        return NULL; // Return NULL on type mismatch or null object
    }
    // Return a direct pointer to the internal mpf_t
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

// 从任意对象中提取整数值 (返回新的 PyObject*)
PyObject* py_extract_int_from_any(PyObject* obj)
{
    if (!obj)
    {
        fprintf(stderr, "Error: Cannot extract int from NULL object\n");
        return NULL; // Or return py_create_int(0)? Depends on desired semantics.
    }

    if (obj->typeId == PY_TYPE_INT)
    {
        py_incref(obj); // Type matches, just incref and return
        return obj;
    }

    mpz_t temp_z; // Temporary GMP integer for conversion
    mpz_init(temp_z); // Initialize temporary
    bool conversion_ok = true;

    switch (obj->typeId)
    {
        case PY_TYPE_DOUBLE:
            // GMP float to int conversion (truncates towards zero)
            mpz_set_f(temp_z, ((PyPrimitiveObject*)obj)->value.doubleValue);
            break;

        case PY_TYPE_BOOL:
            mpz_set_si(temp_z, ((PyPrimitiveObject*)obj)->value.boolValue ? 1 : 0);
            break;

        case PY_TYPE_STRING:
        {
            const char* str = ((PyPrimitiveObject*)obj)->value.stringValue;
            // Attempt conversion from base 10 string
            if (!str || mpz_set_str(temp_z, str, 10) != 0) {
                 fprintf(stderr, "ValueError: Could not convert string '%s' to int\n", str ? str : "<null>");
                 conversion_ok = false;
            }
            break;
        }

        default:
            fprintf(stderr, "TypeError: Cannot extract int from type %s\n", py_type_name(obj->typeId));
            conversion_ok = false;
    }

    PyObject* result = NULL;
    if (conversion_ok) {
        // Create a *new* integer object from the temporary GMP integer
        // Assumes a function like py_create_int_from_mpz exists
        result = py_create_int_from_mpz(temp_z);
    }

    mpz_clear(temp_z); // Clear the temporary GMP integer
    return result; // Return the new PyObject* or NULL if conversion failed
}
// 尝试从常量中提取整数值
mpz_ptr py_extract_constant_int(PyObject* obj)
{
    if (!obj) return NULL;

    if (obj->typeId == PY_TYPE_INT) {
        // 直接返回内部的 mpz_t 指针
        return ((PyPrimitiveObject*)obj)->value.intValue;
    }

    // 处理布尔常量，返回指向静态 GMP 0 或 1 的指针
    if (obj->typeId == PY_TYPE_BOOL) {
        // Lazy initialization using atomic flag (thread-safe)
        if (!gmp_bool_initialized.test_and_set()) {
            mpz_init_set_si(gmp_bool_true, 1);
            mpz_init_set_si(gmp_bool_false, 0);
            // Consider registering a cleanup function if necessary
            // atexit([](){ mpz_clear(gmp_bool_true); mpz_clear(gmp_bool_false); });
        }

        // 返回指向静态 mpz_t 的指针
        return ((PyPrimitiveObject*)obj)->value.boolValue ? gmp_bool_true : gmp_bool_false;
    }

    // 其他类型无法提取为常量整数指针
    // fprintf(stderr, "TypeError: Cannot extract constant int pointer from type %s\n", py_type_name(obj->typeId)); // Optional warning
    return NULL; // 返回 NULL 表示无法提取
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

    mpf_t temp_f;
    // Initialize with a default precision (e.g., 256 bits) or obj's precision if available
    mpf_init2(temp_f, 256);
    // Convert GMP integer to GMP float
    mpf_set_z(temp_f, ((PyPrimitiveObject*)obj)->value.intValue);

    // Create a new float object from the temporary GMP float
    // Assumes py_create_double_from_mpf exists
    PyObject* result = py_create_double_from_mpf(temp_f);

    mpf_clear(temp_f); // Clear the temporary
    return result;
}

// 将浮点数对象转换为整数对象
PyObject* py_convert_double_to_int(PyObject* obj)
{
    if (!obj || obj->typeId != PY_TYPE_DOUBLE)
    {
        fprintf(stderr, "TypeError: Expected float for conversion to int\n");
        return NULL;
    }

    mpz_t temp_z;
    mpz_init(temp_z);
    // Convert GMP float to GMP integer (truncates towards zero)
    mpz_set_f(temp_z, ((PyPrimitiveObject*)obj)->value.doubleValue);

    // Create a new integer object from the temporary GMP integer
    // Assumes py_create_int_from_mpz exists
    PyObject* result = py_create_int_from_mpz(temp_z);

    mpz_clear(temp_z); // Clear the temporary
    return result;
}

// 将对象转换为布尔对象
PyObject* py_convert_to_bool(PyObject* obj)
{
    // No GMP specific changes here, relies on py_object_to_bool
    bool boolValue = py_object_to_bool(obj);
    return py_create_bool(boolValue); // Assumes py_create_bool exists
}

// 将对象转换为字符串对象
PyObject* py_convert_to_string(PyObject* obj)
{
    if (!obj)
        return py_create_string("None"); // Assumes py_create_string exists

    char* buffer = NULL; // GMP functions allocate memory

    switch (getBaseTypeId(obj->typeId))
    {
        case PY_TYPE_NONE:
            return py_create_string("None");

        case PY_TYPE_INT:
        {
            // Convert GMP integer to string (base 10)
            buffer = mpz_get_str(NULL, 10, ((PyPrimitiveObject*)obj)->value.intValue);
            if (!buffer) {
                fprintf(stderr, "Error converting GMP int to string\n");
                return py_create_string(""); // Or handle error differently
            }
            PyObject* str_obj = py_create_string(buffer);
            free(buffer); // Free memory allocated by mpz_get_str
            return str_obj;
        }

        case PY_TYPE_DOUBLE:
        {
            // Convert GMP float to string
            // mpf_get_str provides digits and exponent separately.
            // We need to format it properly.
            mp_exp_t exponent;
            // Request enough digits for reasonable precision, 0 means use default based on precision
            buffer = mpf_get_str(NULL, &exponent, 10, 0, ((PyPrimitiveObject*)obj)->value.doubleValue);
             if (!buffer) {
                fprintf(stderr, "Error converting GMP float to string\n");
                return py_create_string(""); // Or handle error differently
            }

            // Simple formatting (could be improved for scientific notation etc.)
            size_t len = strlen(buffer);
            // Allocate enough space for sign, digits, decimal point, 'e', exponent sign, exponent digits, null terminator
            char* formatted_buffer = (char*)malloc(len + 20);
            if (!formatted_buffer) {
                free(buffer);
                fprintf(stderr, "Memory allocation failed for float string formatting\n");
                return py_create_string("");
            }

            if (len == 0 || strcmp(buffer, "0") == 0) { // Handle zero
                 strcpy(formatted_buffer, "0.0");
            } else {
                // This formatting is basic and might not match Python's exactly
                // It aims for a decimal representation.
                char sign = (buffer[0] == '-') ? '-' : '+';
                const char* digits = (sign == '-') ? buffer + 1 : buffer;
                len = strlen(digits); // Length of digits only

                if (exponent <= 0) { // Like 0.00xyz
                    sprintf(formatted_buffer, "%c0.%0*d%s", sign, (int)(-exponent), 0, digits);
                    // Remove trailing zeros after decimal point if desired
                } else if (exponent >= (mp_exp_t)len) { // Like xy000.0
                    sprintf(formatted_buffer, "%c%s%0*d.0", sign, digits, (int)(exponent - len), 0);
                } else { // Like xyz.abc
                    sprintf(formatted_buffer, "%c%.*s.%s", sign, (int)exponent, digits, digits + exponent);
                    // Remove trailing zeros after decimal point if desired
                }
                if (sign == '+') { // Remove leading '+' if present
                    memmove(formatted_buffer, formatted_buffer + 1, strlen(formatted_buffer));
                }
            }

            free(buffer); // Free memory from mpf_get_str
            PyObject* str_obj = py_create_string(formatted_buffer);
            free(formatted_buffer); // Free the formatting buffer
            return str_obj;
        }

        case PY_TYPE_BOOL:
        {
            bool val = ((PyPrimitiveObject*)obj)->value.boolValue;
            return py_create_string(val ? "True" : "False");
        }

        case PY_TYPE_STRING:
            // String object: create a copy (assuming py_object_copy handles strings)
            return py_object_copy(obj, PY_TYPE_STRING);

        default:
        {
            // Other types: use default representation
            const char* typeName = py_type_name(obj->typeId);
            char temp_buffer[256]; // Static buffer okay for this fallback
            snprintf(temp_buffer, sizeof(temp_buffer), "<%s object at %p>", typeName, (void*)obj);
            return py_create_string(temp_buffer);
        }
    }
}

// 将任意对象转换为整数对象
PyObject* py_convert_any_to_int(PyObject* obj)
{
    // py_extract_int_from_any already does the conversion and returns a new object or NULL
    PyObject* result = py_extract_int_from_any(obj);
    if (result) {
        return result;
    } else {
        // If extraction/conversion failed, return 0
        return py_create_int(0); // Assumes py_create_int(0) creates GMP 0
    }
}

// 将任意对象转换为浮点数对象
PyObject* py_convert_any_to_double(PyObject* obj)
{
    if (!obj)
        return py_create_double(0.0); // Assumes py_create_double creates GMP 0.0

    if (obj->typeId == PY_TYPE_DOUBLE) {
        py_incref(obj); // Type matches, just incref and return
        return obj;
    }

    mpf_t temp_f;
    mpf_init2(temp_f, 256); // Initialize temporary with default precision
    bool conversion_ok = true;

    switch (obj->typeId)
    {
        case PY_TYPE_INT:
            mpf_set_z(temp_f, ((PyPrimitiveObject*)obj)->value.intValue);
            break;

        case PY_TYPE_BOOL:
            mpf_set_si(temp_f, ((PyPrimitiveObject*)obj)->value.boolValue ? 1 : 0);
            break;

        case PY_TYPE_STRING:
        {
            const char* str = ((PyPrimitiveObject*)obj)->value.stringValue;
            // Attempt conversion from base 10 string
            if (!str || mpf_set_str(temp_f, str, 10) != 0) {
                 fprintf(stderr, "ValueError: Could not convert string '%s' to float\n", str ? str : "<null>");
                 conversion_ok = false;
            }
            break;
        }

        default:
            fprintf(stderr, "TypeError: Cannot convert %s to float\n", py_type_name(obj->typeId));
            conversion_ok = false;
    }

    PyObject* result = NULL;
    if (conversion_ok) {
        // Create a new float object from the temporary GMP float
        result = py_create_double_from_mpf(temp_f);
    } else {
        // Conversion failed, return 0.0
        result = py_create_double(0.0);
    }

    mpf_clear(temp_f); // Clear the temporary
    return result;
}

// 将任意对象转换为布尔对象
PyObject* py_convert_any_to_bool(PyObject* obj)
{
    // Relies on py_object_to_bool which is updated
    return py_convert_to_bool(obj);
}

// 将任意对象转换为字符串对象
PyObject* py_convert_any_to_string(PyObject* obj)
{
    // Relies on py_convert_to_string which is updated
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
      if (!obj) return NULL;
 
     int sourceTypeId = py_get_safe_type_id(obj);
     int baseSourceTypeId = llvmpy::getBaseTypeId(sourceTypeId);
     int baseTargetTypeId = llvmpy::getBaseTypeId(targetTypeId);
 
     // 1. Check if types are already compatible
     if (py_are_types_compatible(sourceTypeId, targetTypeId))
     {
         py_incref(obj);
         return obj;
     }
 
     // 2. Use updated conversion functions
     if (baseTargetTypeId == llvmpy::PY_TYPE_INT)
     {
         // Use py_convert_any_to_int which handles Double, Bool, String -> Int
         return py_convert_any_to_int(obj);
     }
     else if (baseTargetTypeId == llvmpy::PY_TYPE_DOUBLE)
     {
          // Use py_convert_any_to_double which handles Int, Bool, String -> Double
         return py_convert_any_to_double(obj);
     }
     else if (baseTargetTypeId == llvmpy::PY_TYPE_BOOL)
     {
         return py_convert_to_bool(obj); // Handles conversion from any type
     }
     else if (baseTargetTypeId == llvmpy::PY_TYPE_STRING)
     {
         return py_convert_to_string(obj); // Handles conversion from common types
     }
 
     // 3. If no rule matches, conversion fails
     // fprintf(stderr, "TypeError: Cannot smart convert type %s to %s\n", py_type_name(sourceTypeId), py_type_name(targetTypeId));
     return NULL;
 }


 int py_object_to_exit_code(PyObject* obj) {
    if (!obj) {
        // std::cerr << "Runtime Warning: Converting null object to exit code, returning 1." << std::endl;
        return 1; // Error case
    }

    int typeId = py_get_type_id(obj);

    if (typeId == PY_TYPE_NONE) {
        return 0; // None maps to exit code 0
    } else if (typeId == PY_TYPE_INT) {
        // Extract GMP int pointer
        mpz_ptr gmp_int = py_extract_int(obj);
        if (!gmp_int) { // Should not happen if typeId is PY_TYPE_INT, but check anyway
             // std::cerr << "Runtime Error: Failed to extract GMP int for exit code." << std::endl;
             return 1;
        }
        // Check if it fits in a C signed long (typical for exit codes)
        if (mpz_fits_slong_p(gmp_int)) {
            return (int)mpz_get_si(gmp_int); // Convert GMP int to C signed long
        } else {
             // std::cerr << "Runtime Warning: Integer exit code out of range for standard int, returning 1." << std::endl;
             return 1; // Value out of range
        }
    } else {
        // Non-None/non-Int result in error code 1
        // std::cerr << "Runtime Warning: Converting object of type " << py_type_name(typeId)
        //           << " to exit code, returning 1." << std::endl;
        return 1;
    }
}

