#include "RunTime/runtime.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>


using namespace llvmpy;

//===----------------------------------------------------------------------===//
// 类型强制转换实现 - 用于操作前统一类型
//===----------------------------------------------------------------------===//

// 数值类型强制转换 - 将数值类型统一为双精度浮点数
bool py_coerce_numeric(PyObject* a, PyObject* b, double* out_a, double* out_b)
{
    if (!a || !b || !out_a || !out_b)
    {
        return false;
    }

    int typeA = getBaseTypeId(a->typeId);
    int typeB = getBaseTypeId(b->typeId);

    // 验证类型是否为数值类型
    bool isANumeric = (typeA == PY_TYPE_INT || typeA == PY_TYPE_DOUBLE || typeA == PY_TYPE_BOOL);
    bool isBNumeric = (typeB == PY_TYPE_INT || typeB == PY_TYPE_DOUBLE || typeB == PY_TYPE_BOOL);

    if (!isANumeric || !isBNumeric)
    {
        return false;
    }

    // 提取数值
    if (typeA == PY_TYPE_INT)
    {
        *out_a = (double)((PyPrimitiveObject*)a)->value.intValue;
    }
    else if (typeA == PY_TYPE_DOUBLE)
    {
        *out_a = ((PyPrimitiveObject*)a)->value.doubleValue;
    }
    else if (typeA == PY_TYPE_BOOL)
    {
        *out_a = ((PyPrimitiveObject*)a)->value.boolValue ? 1.0 : 0.0;
    }

    if (typeB == PY_TYPE_INT)
    {
        *out_b = (double)((PyPrimitiveObject*)b)->value.intValue;
    }
    else if (typeB == PY_TYPE_DOUBLE)
    {
        *out_b = ((PyPrimitiveObject*)b)->value.doubleValue;
    }
    else if (typeB == PY_TYPE_BOOL)
    {
        *out_b = ((PyPrimitiveObject*)b)->value.boolValue ? 1.0 : 0.0;
    }

    return true;
}

//===----------------------------------------------------------------------===//
// 二元操作符实现
//===----------------------------------------------------------------------===//

// 加法操作符
PyObject* py_object_add(PyObject* a, PyObject* b)
{
    if (!a || !b)
    {
        fprintf(stderr, "TypeError: Cannot perform addition with None\n");
        return NULL;
    }

    int aTypeId = getBaseTypeId(a->typeId);
    int bTypeId = getBaseTypeId(b->typeId);

    // 处理数值类型加法
    if ((aTypeId == PY_TYPE_INT || aTypeId == PY_TYPE_DOUBLE || aTypeId == PY_TYPE_BOOL) &&
        (bTypeId == PY_TYPE_INT || bTypeId == PY_TYPE_DOUBLE || bTypeId == PY_TYPE_BOOL))
    {
        double valA, valB;
        if (!py_coerce_numeric(a, b, &valA, &valB))
        {
            fprintf(stderr, "TypeError: Unsupported operand types for +\n");
            return NULL;
        }

        // 如果两个操作数都是整数，返回整数结果
        if (aTypeId == PY_TYPE_INT && bTypeId == PY_TYPE_INT)
        {
            int intA = ((PyPrimitiveObject*)a)->value.intValue;
            int intB = ((PyPrimitiveObject*)b)->value.intValue;
            return py_create_int(intA + intB);
        }

        // 否则返回浮点数结果
        return py_create_double(valA + valB);
    }

    // 处理字符串连接
    if (aTypeId == PY_TYPE_STRING && bTypeId == PY_TYPE_STRING)
    {
        const char* strA = ((PyPrimitiveObject*)a)->value.stringValue;
        const char* strB = ((PyPrimitiveObject*)b)->value.stringValue;
        
        if (!strA) strA = "";
        if (!strB) strB = "";
        
        size_t lenA = strlen(strA);
        size_t lenB = strlen(strB);
        char* result = (char*)malloc(lenA + lenB + 1);
        
        if (!result)
        {
            fprintf(stderr, "Error: Out of memory\n");
            return NULL;
        }
        
        strcpy(result, strA);
        strcat(result, strB);
        
        PyObject* resultObj = py_create_string(result);
        free(result);
        return resultObj;
    }

    // 处理列表连接
    if (aTypeId == PY_TYPE_LIST && bTypeId == PY_TYPE_LIST)
    {
        PyListObject* listA = (PyListObject*)a;
        PyListObject* listB = (PyListObject*)b;
        
        /* // 检查元素类型兼容性
        if (listA->elemTypeId != 0 && listB->elemTypeId != 0 && 
            listA->elemTypeId != listB->elemTypeId && 
            !py_are_types_compatible(listA->elemTypeId, listB->elemTypeId))
        {
            fprintf(stderr, "TypeError: Cannot concatenate lists with incompatible element types\n");
            return NULL;
        } */
        
        // 创建一个新列表，容量为两个列表长度之和
        int newCapacity = listA->length + listB->length;
        int elemTypeId = listA->elemTypeId != 0 ? listA->elemTypeId : listB->elemTypeId;
        PyObject* resultList = py_create_list(newCapacity, elemTypeId);
        
        if (!resultList)
        {
            return NULL;
        }
        
        PyListObject* resultListObj = (PyListObject*)resultList;
        
        // 复制第一个列表的元素
        for (int i = 0; i < listA->length; i++)
        {
            if (listA->data[i])
            {
                resultListObj->data[i] = listA->data[i];
                py_incref(listA->data[i]);
            }
            else
            {
                resultListObj->data[i] = NULL;
            }
        }
        
        // 复制第二个列表的元素
        for (int i = 0; i < listB->length; i++)
        {
            if (listB->data[i])
            {
                resultListObj->data[listA->length + i] = listB->data[i];
                py_incref(listB->data[i]);
            }
            else
            {
                resultListObj->data[listA->length + i] = NULL;
            }
        }
        
        resultListObj->length = listA->length + listB->length;
        return resultList;
    }

    fprintf(stderr, "TypeError: Unsupported operand types for +: %s and %s\n", 
            py_type_name(a->typeId), py_type_name(b->typeId));
    return NULL;
}

// 减法操作符
PyObject* py_object_subtract(PyObject* a, PyObject* b)
{
    if (!a || !b)
    {
        fprintf(stderr, "TypeError: Cannot perform subtraction with None\n");
        return NULL;
    }

    double valA, valB;
    if (!py_coerce_numeric(a, b, &valA, &valB))
    {
        fprintf(stderr, "TypeError: Unsupported operand types for -\n");
        return NULL;
    }

    // 如果两个操作数都是整数，返回整数结果
    if (a->typeId == PY_TYPE_INT && b->typeId == PY_TYPE_INT)
    {
        int intA = ((PyPrimitiveObject*)a)->value.intValue;
        int intB = ((PyPrimitiveObject*)b)->value.intValue;
        return py_create_int(intA - intB);
    }

    // 否则返回浮点数结果
    return py_create_double(valA - valB);
}

// 乘法操作符
PyObject* py_object_multiply(PyObject* a, PyObject* b)
{
    if (!a || !b)
    {
        fprintf(stderr, "TypeError: Cannot perform multiplication with None\n");
        return NULL;
    }

    int aTypeId = getBaseTypeId(a->typeId);
    int bTypeId = getBaseTypeId(b->typeId);

    // 处理数值类型乘法
    if ((aTypeId == PY_TYPE_INT || aTypeId == PY_TYPE_DOUBLE || aTypeId == PY_TYPE_BOOL) &&
        (bTypeId == PY_TYPE_INT || bTypeId == PY_TYPE_DOUBLE || bTypeId == PY_TYPE_BOOL))
    {
        double valA, valB;
        if (!py_coerce_numeric(a, b, &valA, &valB))
        {
            fprintf(stderr, "TypeError: Unsupported operand types for *\n");
            return NULL;
        }

        // 如果两个操作数都是整数，返回整数结果
        if (aTypeId == PY_TYPE_INT && bTypeId == PY_TYPE_INT)
        {
            int intA = ((PyPrimitiveObject*)a)->value.intValue;
            int intB = ((PyPrimitiveObject*)b)->value.intValue;
            return py_create_int(intA * intB);
        }

        // 否则返回浮点数结果
        return py_create_double(valA * valB);
    }

    // 处理字符串重复
    if (aTypeId == PY_TYPE_STRING && bTypeId == PY_TYPE_INT)
    {
        const char* str = ((PyPrimitiveObject*)a)->value.stringValue;
        int count = ((PyPrimitiveObject*)b)->value.intValue;
        
        if (!str) str = "";
        if (count <= 0) return py_create_string("");
        
        size_t len = strlen(str);
        char* result = (char*)malloc(len * count + 1);
        
        if (!result)
        {
            fprintf(stderr, "Error: Out of memory\n");
            return NULL;
        }
        
        result[0] = '\0';
        for (int i = 0; i < count; i++)
        {
            strcat(result, str);
        }
        
        PyObject* resultObj = py_create_string(result);
        free(result);
        return resultObj;
    }
    
    // 处理整数与字符串重复（顺序翻转）
    if (aTypeId == PY_TYPE_INT && bTypeId == PY_TYPE_STRING)
    {
        return py_object_multiply(b, a);
    }

    // 处理列表重复
    if (aTypeId == PY_TYPE_LIST && bTypeId == PY_TYPE_INT)
    {
        PyListObject* list = (PyListObject*)a;
        int count = ((PyPrimitiveObject*)b)->value.intValue;
        
        if (count <= 0) return py_create_list(0, list->elemTypeId);
        
        // 创建新列表
        int newCapacity = list->length * count;
        PyObject* resultList = py_create_list(newCapacity, list->elemTypeId);
        
        if (!resultList)
        {
            return NULL;
        }
        
        PyListObject* resultListObj = (PyListObject*)resultList;
        
        // 复制元素
        for (int c = 0; c < count; c++)
        {
            for (int i = 0; i < list->length; i++)
            {
                int targetIndex = c * list->length + i;
                if (list->data[i])
                {
                    resultListObj->data[targetIndex] = list->data[i];
                    py_incref(list->data[i]);
                }
                else
                {
                    resultListObj->data[targetIndex] = NULL;
                }
            }
        }
        
        resultListObj->length = list->length * count;
        return resultList;
    }
    
    // 处理整数与列表重复（顺序翻转）
    if (aTypeId == PY_TYPE_INT && bTypeId == PY_TYPE_LIST)
    {
        return py_object_multiply(b, a);
    }

    fprintf(stderr, "TypeError: Unsupported operand types for *: %s and %s\n", 
            py_type_name(a->typeId), py_type_name(b->typeId));
    return NULL;
}

// 除法操作符
PyObject* py_object_divide(PyObject* a, PyObject* b)
{
    if (!a || !b)
    {
        fprintf(stderr, "TypeError: Cannot perform division with None\n");
        return NULL;
    }

    double valA, valB;
    if (!py_coerce_numeric(a, b, &valA, &valB))
    {
        fprintf(stderr, "TypeError: Unsupported operand types for /\n");
        return NULL;
    }

    if (valB == 0.0)
    {
        fprintf(stderr, "ZeroDivisionError: Division by zero\n");
        return NULL;
    }

    // 在Python 3中，/总是返回浮点数
    return py_create_double(valA / valB);
}

// 模运算操作符
PyObject* py_object_modulo(PyObject* a, PyObject* b)
{
    if (!a || !b)
    {
        fprintf(stderr, "TypeError: Cannot perform modulo with None\n");
        return NULL;
    }

    // 处理整数类型的模运算
    if (a->typeId == PY_TYPE_INT && b->typeId == PY_TYPE_INT)
    {
        int intA = ((PyPrimitiveObject*)a)->value.intValue;
        int intB = ((PyPrimitiveObject*)b)->value.intValue;
        
        if (intB == 0)
        {
            fprintf(stderr, "ZeroDivisionError: Modulo by zero\n");
            return NULL;
        }
        
        return py_create_int(intA % intB);
    }

    // 处理浮点数模运算 (使用fmod)
    double valA, valB;
    if (!py_coerce_numeric(a, b, &valA, &valB))
    {
        fprintf(stderr, "TypeError: Unsupported operand types for %%\n");
        return NULL;
    }

    if (valB == 0.0)
    {
        fprintf(stderr, "ZeroDivisionError: Modulo by zero\n");
        return NULL;
    }

    return py_create_double(fmod(valA, valB));
}

// 整除操作符
PyObject* py_object_floor_divide(PyObject* a, PyObject* b)
{
    if (!a || !b)
    {
        fprintf(stderr, "TypeError: Cannot perform floor division with None\n");
        return NULL;
    }

    // 处理整数类型的整除
    if (a->typeId == PY_TYPE_INT && b->typeId == PY_TYPE_INT)
    {
        int intA = ((PyPrimitiveObject*)a)->value.intValue;
        int intB = ((PyPrimitiveObject*)b)->value.intValue;
        
        if (intB == 0)
        {
            fprintf(stderr, "ZeroDivisionError: Integer division by zero\n");
            return NULL;
        }
        
        return py_create_int(intA / intB);
    }

    // 处理浮点数整除
    double valA, valB;
    if (!py_coerce_numeric(a, b, &valA, &valB))
    {
        fprintf(stderr, "TypeError: Unsupported operand types for //\n");
        return NULL;
    }

    if (valB == 0.0)
    {
        fprintf(stderr, "ZeroDivisionError: Floor division by zero\n");
        return NULL;
    }

    return py_create_double(floor(valA / valB));
}

// 幂运算操作符
PyObject* py_object_power(PyObject* a, PyObject* b)
{
    if (!a || !b)
    {
        fprintf(stderr, "TypeError: Cannot perform power operation with None\n");
        return NULL;
    }

    double valA, valB;
    if (!py_coerce_numeric(a, b, &valA, &valB))
    {
        fprintf(stderr, "TypeError: Unsupported operand types for **\n");
        return NULL;
    }

    // 如果底数为0，指数为负，则错误
    if (valA == 0.0 && valB < 0.0)
    {
        fprintf(stderr, "ZeroDivisionError: 0.0 cannot be raised to a negative power\n");
        return NULL;
    }

    double result = pow(valA, valB);

    // 如果两个操作数都是整数，并且结果也是整数，则返回整数
    if (a->typeId == PY_TYPE_INT && b->typeId == PY_TYPE_INT && 
        result == (int)result && valB >= 0)
    {
        return py_create_int((int)result);
    }

    return py_create_double(result);
}

//===----------------------------------------------------------------------===//
// 一元操作符实现
//===----------------------------------------------------------------------===//

// 一元取负操作符
PyObject* py_object_negate(PyObject* a)
{
    if (!a)
    {
        fprintf(stderr, "TypeError: Cannot negate None\n");
        return NULL;
    }

    if (a->typeId == PY_TYPE_INT)
    {
        int value = ((PyPrimitiveObject*)a)->value.intValue;
        return py_create_int(-value);
    }
    else if (a->typeId == PY_TYPE_DOUBLE)
    {
        double value = ((PyPrimitiveObject*)a)->value.doubleValue;
        return py_create_double(-value);
    }
    else if (a->typeId == PY_TYPE_BOOL)
    {
        bool value = ((PyPrimitiveObject*)a)->value.boolValue;
        return py_create_int(value ? -1 : 0);
    }

    fprintf(stderr, "TypeError: Unsupported operand type for unary -: %s\n", 
            py_type_name(a->typeId));
    return NULL;
}

// 一元逻辑非操作符
PyObject* py_object_not(PyObject* a)
{
    if (!a)
    {
        // None被认为是False
        return py_create_bool(true);
    }

    bool value = py_object_to_bool(a);
    return py_create_bool(!value);
}

//===----------------------------------------------------------------------===//
// 比较操作符实现
//===----------------------------------------------------------------------===//

// 比较操作
// 比较操作 (根据之前的讨论修改)
PyObject* py_object_compare(PyObject* a, PyObject* b, PyCompareOp op)
{
    // 1. 处理 NULL 或 None
    if (!a || !b)
    {
        bool result = false;
        // None == None is True, None != None is False
        if (op == PY_CMP_EQ) result = (a == b);
        // None < Other, None <= Other, None > Other, None >= Other are TypeErrors in Python 3
        // For simplicity, we might return False or handle as error. Let's return False for non-EQ/NE.
        else if (op == PY_CMP_NE) result = (a != b);
        else {
             // Python 3 raises TypeError for None comparisons other than ==, !=
             // We should ideally signal an error here. Returning False is simpler but less accurate.
             // fprintf(stderr, "TypeError: '%s' not supported between instances of 'NoneType' and '%s'\n",
             //         py_compare_op_name(op), py_type_name(a ? a->typeId : b->typeId));
             // return NULL; // Indicate error
             return py_create_bool(false); // Simplified: return False
        }
        return py_create_bool(result);
    }

    // 2. 优先使用类型分发处理 EQ 和 NE
    if (op == PY_CMP_EQ || op == PY_CMP_NE)
    {
        int typeIdA = py_get_safe_type_id(a);
        const PyTypeMethods* methodsA = py_get_type_methods(typeIdA);

        if (methodsA && methodsA->equals)
        {
            PyObject* resultObj = methodsA->equals(a, b); // 调用 a 的 equals 方法
            if (resultObj) {
                 // 检查返回的是否是布尔值
                 if (py_get_safe_type_id(resultObj) == PY_TYPE_BOOL) {
                     bool eq_result = py_extract_bool(resultObj);
                     py_decref(resultObj); // 减少 equals 返回对象的引用计数
                     return py_create_bool(op == PY_CMP_EQ ? eq_result : !eq_result);
                 } else {
                     // equals 没有返回布尔值，这是错误的
                     fprintf(stderr, "TypeError: __eq__ returned non-boolean (type %s)\n", py_type_name(resultObj->typeId));
                     py_decref(resultObj);
                     // Python 会报错，我们暂时返回 False
                     return py_create_bool(false);
                 }
            } else {
                 // equals 调用失败或返回 NULL (可能内部出错)
                 fprintf(stderr, "Error during equality comparison for type %s\n", py_type_name(typeIdA));
                 // 返回 False 或 NULL 表示错误
                 return py_create_bool(false); // 简化处理
            }
        }
        // --- Pythonic Fallback (Optional, more complex): ---
        // If a's type doesn't have equals, or if it returned a special "NotImplemented" value,
        // Python would then try b's type's equals method: b.__eq__(a).
        // For simplicity, we'll fall back directly to identity comparison if a's type doesn't handle it.

        // --- Fallback to Identity Comparison ---
        // fprintf(stderr, "Warning: Type %s has no specific equals method, falling back to identity comparison.\n", py_type_name(typeIdA));
        bool identity_result = (a == b); // Compare pointers
        return py_create_bool(op == PY_CMP_EQ ? identity_result : !identity_result);
    }

    // 3. 处理其他比较操作 (<, <=, >, >=)
    int aTypeId = getBaseTypeId(a->typeId);
    int bTypeId = getBaseTypeId(b->typeId);

    // 尝试数值比较
    if ((aTypeId == PY_TYPE_INT || aTypeId == PY_TYPE_DOUBLE || aTypeId == PY_TYPE_BOOL) &&
        (bTypeId == PY_TYPE_INT || bTypeId == PY_TYPE_DOUBLE || bTypeId == PY_TYPE_BOOL))
    {
        double valA, valB;
        // py_coerce_numeric should handle bools correctly
        if (py_coerce_numeric(a, b, &valA, &valB))
        {
            bool result = false;
            switch (op)
            {
                // EQ/NE handled above by type dispatch
                case PY_CMP_LT: result = valA < valB; break;
                case PY_CMP_LE: result = valA <= valB; break;
                case PY_CMP_GT: result = valA > valB; break;
                case PY_CMP_GE: result = valA >= valB; break;
                default:
                    fprintf(stderr, "Internal Error: Unhandled comparison operator %d\n", op);
                    return py_create_bool(false); // Should not happen
            }
            return py_create_bool(result);
        }
        // If coercion fails even for numeric types, something is wrong.
        // Fall through to general TypeError.
    }

    // 尝试字符串比较
    if (aTypeId == PY_TYPE_STRING && bTypeId == PY_TYPE_STRING)
    {
        const char* strA = ((PyPrimitiveObject*)a)->value.stringValue;
        const char* strB = ((PyPrimitiveObject*)b)->value.stringValue;

        if (!strA) strA = ""; // Treat NULL string pointer as empty string
        if (!strB) strB = "";

        int cmpResult = strcmp(strA, strB);
        bool result = false;

        switch (op)
        {
            // EQ/NE handled above by type dispatch
            case PY_CMP_LT: result = (cmpResult < 0); break;
            case PY_CMP_LE: result = (cmpResult <= 0); break;
            case PY_CMP_GT: result = (cmpResult > 0); break;
            case PY_CMP_GE: result = (cmpResult >= 0); break;
             default:
                fprintf(stderr, "Internal Error: Unhandled comparison operator %d for strings\n", op);
                return py_create_bool(false); // Should not happen
        }
        return py_create_bool(result);
    }

    // 4. 处理不支持排序的类型或混合类型
    // Python 3 generally raises TypeError for ordering comparisons between incompatible types.
    // List and Dict comparisons for ordering are also TypeErrors.
    // Since EQ/NE are handled above, any remaining comparison here between incompatible types
    // or types that don't support ordering (like list, dict) should be an error.

    fprintf(stderr, "TypeError: '%s' not supported between instances of '%s' and '%s'\n",
            py_compare_op_name(op), // Helper function to get "<", "<=", etc. string
            py_type_name(a->typeId), py_type_name(b->typeId));

    // Return NULL to indicate an error occurred, or a specific error object if implemented.
    // Returning a boolean might hide the error.
    return NULL; // Indicate TypeError
    // Or, for simplicity if error handling is basic:
    // return py_create_bool(false);
}

// 获取compare_op名字的赋值函数
const char* py_compare_op_name(PyCompareOp op) {
    switch(op) {
        case PY_CMP_LT: return "<";
        case PY_CMP_LE: return "<=";
        case PY_CMP_EQ: return "==";
        case PY_CMP_NE: return "!=";
        case PY_CMP_GT: return ">";
        case PY_CMP_GE: return ">=";
        default: return "op";
    }
}



// 相等性比较辅助函数 (可以保留，但现在主要逻辑在 py_object_compare 中)
bool py_compare_eq(PyObject* a, PyObject* b)
{
    PyObject* result = py_object_compare(a, b, PY_CMP_EQ);
    if (!result)
    {
        // py_object_compare returning NULL indicates an error
        // Handle error appropriately, maybe return false or propagate error
        return false;
    }

    bool eq = false;
    if (py_get_safe_type_id(result) == PY_TYPE_BOOL) {
        eq = ((PyPrimitiveObject*)result)->value.boolValue;
    } else {
        // Should not happen if py_object_compare works correctly
        fprintf(stderr, "Internal Error: py_object_compare(EQ) returned non-bool\n");
    }
    py_decref(result);
    return eq;
}

// 不等性比较辅助函数
bool py_compare_ne(PyObject* a, PyObject* b)
{
    PyObject* result = py_object_compare(a, b, PY_CMP_NE);
    if (!result)
    {
        // Handle error
        return true; // Or propagate error
    }

    bool ne = false;
     if (py_get_safe_type_id(result) == PY_TYPE_BOOL) {
        ne = ((PyPrimitiveObject*)result)->value.boolValue;
    } else {
        fprintf(stderr, "Internal Error: py_object_compare(NE) returned non-bool\n");
    }
    py_decref(result);
    return ne;
}

// 小于比较辅助函数
bool py_compare_lt(PyObject* a, PyObject* b)
{
    PyObject* result = py_object_compare(a, b, PY_CMP_LT);
    if (!result)
    {
        // Handle error (TypeError occurred)
        return false; // Or propagate error
    }

    bool lt = false;
    if (py_get_safe_type_id(result) == PY_TYPE_BOOL) {
        lt = ((PyPrimitiveObject*)result)->value.boolValue;
    } else {
         fprintf(stderr, "Internal Error: py_object_compare(LT) returned non-bool\n");
    }
    py_decref(result);
    return lt;
}

// 小于等于比较辅助函数
bool py_compare_le(PyObject* a, PyObject* b)
{
    PyObject* result = py_object_compare(a, b, PY_CMP_LE);
     if (!result)
    {
        // Handle error
        return false;
    }
    bool le = false;
    if (py_get_safe_type_id(result) == PY_TYPE_BOOL) {
        le = ((PyPrimitiveObject*)result)->value.boolValue;
    } else {
         fprintf(stderr, "Internal Error: py_object_compare(LE) returned non-bool\n");
    }
    py_decref(result);
    return le;
}

// 大于比较辅助函数
bool py_compare_gt(PyObject* a, PyObject* b)
{
    PyObject* result = py_object_compare(a, b, PY_CMP_GT);
     if (!result)
    {
        // Handle error
        return false;
    }
    bool gt = false;
    if (py_get_safe_type_id(result) == PY_TYPE_BOOL) {
        gt = ((PyPrimitiveObject*)result)->value.boolValue;
    } else {
         fprintf(stderr, "Internal Error: py_object_compare(GT) returned non-bool\n");
    }
    py_decref(result);
    return gt;
}

// 大于等于比较辅助函数
bool py_compare_ge(PyObject* a, PyObject* b)
{
    PyObject* result = py_object_compare(a, b, PY_CMP_GE);
     if (!result)
    {
        // Handle error
        return false;
    }
    bool ge = false;
    if (py_get_safe_type_id(result) == PY_TYPE_BOOL) {
        ge = ((PyPrimitiveObject*)result)->value.boolValue;
    } else {
         fprintf(stderr, "Internal Error: py_object_compare(GE) returned non-bool\n");
    }
    py_decref(result);
    return ge;
}

//===----------------------------------------------------------------------===//
// 位操作符实现
//===----------------------------------------------------------------------===//

// 按位与操作符
PyObject* py_object_bitwise_and(PyObject* a, PyObject* b)
{
    if (!a || !b)
    {
        fprintf(stderr, "TypeError: Cannot perform bitwise AND with None\n");
        return NULL;
    }

    // 整数类型按位与
    if (a->typeId == PY_TYPE_INT && b->typeId == PY_TYPE_INT)
    {
        int intA = ((PyPrimitiveObject*)a)->value.intValue;
        int intB = ((PyPrimitiveObject*)b)->value.intValue;
        return py_create_int(intA & intB);
    }

    fprintf(stderr, "TypeError: Unsupported operand types for &: %s and %s\n", 
            py_type_name(a->typeId), py_type_name(b->typeId));
    return NULL;
}

// 按位或操作符
PyObject* py_object_bitwise_or(PyObject* a, PyObject* b)
{
    if (!a || !b)
    {
        fprintf(stderr, "TypeError: Cannot perform bitwise OR with None\n");
        return NULL;
    }

    // 整数类型按位或
    if (a->typeId == PY_TYPE_INT && b->typeId == PY_TYPE_INT)
    {
        int intA = ((PyPrimitiveObject*)a)->value.intValue;
        int intB = ((PyPrimitiveObject*)b)->value.intValue;
        return py_create_int(intA | intB);
    }

    fprintf(stderr, "TypeError: Unsupported operand types for |: %s and %s\n", 
            py_type_name(a->typeId), py_type_name(b->typeId));
    return NULL;
}

// 按位异或操作符
PyObject* py_object_bitwise_xor(PyObject* a, PyObject* b)
{
    if (!a || !b)
    {
        fprintf(stderr, "TypeError: Cannot perform bitwise XOR with None\n");
        return NULL;
    }

    // 整数类型按位异或
    if (a->typeId == PY_TYPE_INT && b->typeId == PY_TYPE_INT)
    {
        int intA = ((PyPrimitiveObject*)a)->value.intValue;
        int intB = ((PyPrimitiveObject*)b)->value.intValue;
        return py_create_int(intA ^ intB);
    }

    fprintf(stderr, "TypeError: Unsupported operand types for ^: %s and %s\n", 
            py_type_name(a->typeId), py_type_name(b->typeId));
    return NULL;
}

// 按位取反操作符
PyObject* py_object_bitwise_not(PyObject* a)
{
    if (!a)
    {
        fprintf(stderr, "TypeError: Cannot perform bitwise NOT on None\n");
        return NULL;
    }

    // 整数类型按位取反
    if (a->typeId == PY_TYPE_INT)
    {
        int intA = ((PyPrimitiveObject*)a)->value.intValue;
        return py_create_int(~intA);
    }

    fprintf(stderr, "TypeError: Unsupported operand type for ~: %s\n", 
            py_type_name(a->typeId));
    return NULL;
}

// 左移操作符
PyObject* py_object_left_shift(PyObject* a, PyObject* b)
{
    if (!a || !b)
    {
        fprintf(stderr, "TypeError: Cannot perform left shift with None\n");
        return NULL;
    }

    // 检查左操作数是否为整数
    if (a->typeId != PY_TYPE_INT)
    {
        fprintf(stderr, "TypeError: Left operand must be integer for <<\n");
        return NULL;
    }

    // 检查右操作数是否为整数
    if (b->typeId != PY_TYPE_INT)
    {
        fprintf(stderr, "TypeError: Right operand must be integer for <<\n");
        return NULL;
    }

    int intA = ((PyPrimitiveObject*)a)->value.intValue;
    int intB = ((PyPrimitiveObject*)b)->value.intValue;

    // 检查移位量是否为负
    if (intB < 0)
    {
        fprintf(stderr, "ValueError: Negative shift count\n");
        return NULL;
    }

    return py_create_int(intA << intB);
}

// 右移操作符
PyObject* py_object_right_shift(PyObject* a, PyObject* b)
{
    if (!a || !b)
    {
        fprintf(stderr, "TypeError: Cannot perform right shift with None\n");
        return NULL;
    }

    // 检查左操作数是否为整数
    if (a->typeId != PY_TYPE_INT)
    {
        fprintf(stderr, "TypeError: Left operand must be integer for >>\n");
        return NULL;
    }

    // 检查右操作数是否为整数
    if (b->typeId != PY_TYPE_INT)
    {
        fprintf(stderr, "TypeError: Right operand must be integer for >>\n");
        return NULL;
    }

    int intA = ((PyPrimitiveObject*)a)->value.intValue;
    int intB = ((PyPrimitiveObject*)b)->value.intValue;

    // 检查移位量是否为负
    if (intB < 0)
    {
        fprintf(stderr, "ValueError: Negative shift count\n");
        return NULL;
    }

    return py_create_int(intA >> intB);
}