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
        
        // 检查元素类型兼容性
        if (listA->elemTypeId != 0 && listB->elemTypeId != 0 && 
            listA->elemTypeId != listB->elemTypeId && 
            !py_are_types_compatible(listA->elemTypeId, listB->elemTypeId))
        {
            fprintf(stderr, "TypeError: Cannot concatenate lists with incompatible element types\n");
            return NULL;
        }
        
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
PyObject* py_object_compare(PyObject* a, PyObject* b, PyCompareOp op)
{
    if (!a || !b)
    {
        // 特殊处理None比较
        bool result = false;
        
        if (op == PY_CMP_EQ)
        {
            result = (a == b);  // 只有两个都是None时才相等
        }
        else if (op == PY_CMP_NE)
        {
            result = (a != b);  // 一个是None，一个不是None时不相等
        }
        
        return py_create_bool(result);
    }

    int aTypeId = getBaseTypeId(a->typeId);
    int bTypeId = getBaseTypeId(b->typeId);

    // 数值类型比较
    if ((aTypeId == PY_TYPE_INT || aTypeId == PY_TYPE_DOUBLE || aTypeId == PY_TYPE_BOOL) &&
        (bTypeId == PY_TYPE_INT || bTypeId == PY_TYPE_DOUBLE || bTypeId == PY_TYPE_BOOL))
    {
        double valA, valB;
        if (!py_coerce_numeric(a, b, &valA, &valB))
        {
            fprintf(stderr, "TypeError: Cannot compare %s and %s\n",
                    py_type_name(a->typeId), py_type_name(b->typeId));
            return NULL;
        }

        bool result = false;
        switch (op)
        {
            case PY_CMP_EQ: result = (valA == valB); break;
            case PY_CMP_NE: result = (valA != valB); break;
            case PY_CMP_LT: result = (valA < valB); break;
            case PY_CMP_LE: result = (valA <= valB); break;
            case PY_CMP_GT: result = (valA > valB); break;
            case PY_CMP_GE: result = (valA >= valB); break;
        }
        
        return py_create_bool(result);
    }

    // 字符串比较
    if (aTypeId == PY_TYPE_STRING && bTypeId == PY_TYPE_STRING)
    {
        const char* strA = ((PyPrimitiveObject*)a)->value.stringValue;
        const char* strB = ((PyPrimitiveObject*)b)->value.stringValue;
        
        if (!strA) strA = "";
        if (!strB) strB = "";
        
        int cmpResult = strcmp(strA, strB);
        bool result = false;
        
        switch (op)
        {
            case PY_CMP_EQ: result = (cmpResult == 0); break;
            case PY_CMP_NE: result = (cmpResult != 0); break;
            case PY_CMP_LT: result = (cmpResult < 0); break;
            case PY_CMP_LE: result = (cmpResult <= 0); break;
            case PY_CMP_GT: result = (cmpResult > 0); break;
            case PY_CMP_GE: result = (cmpResult >= 0); break;
        }
        
        return py_create_bool(result);
    }

    // 同类型列表比较
    if (aTypeId == PY_TYPE_LIST && bTypeId == PY_TYPE_LIST)
    {
        PyListObject* listA = (PyListObject*)a;
        PyListObject* listB = (PyListObject*)b;
        
        // 只处理等于和不等于
        if (op == PY_CMP_EQ || op == PY_CMP_NE)
        {
            bool isEqual = false;
            
            // 长度必须相同
            if (listA->length == listB->length)
            {
                isEqual = true;
                
                // 比较每个元素
                for (int i = 0; i < listA->length; i++)
                {
                    PyObject* elemA = listA->data[i];
                    PyObject* elemB = listB->data[i];
                    
                    // 如果两元素都为NULL，认为相等
                    if (!elemA && !elemB)
                        continue;
                    
                    // 如果一个为NULL另一个不是，不相等
                    if (!elemA || !elemB)
                    {
                        isEqual = false;
                        break;
                    }
                    
                    // 递归比较元素
                    PyObject* cmpResult = py_object_compare(elemA, elemB, PY_CMP_EQ);
                    if (!cmpResult)
                    {
                        isEqual = false;
                        break;
                    }
                    
                    bool elemEqual = ((PyPrimitiveObject*)cmpResult)->value.boolValue;
                    py_decref(cmpResult);
                    
                    if (!elemEqual)
                    {
                        isEqual = false;
                        break;
                    }
                }
            }
            
            return py_create_bool(op == PY_CMP_EQ ? isEqual : !isEqual);
        }
        
        fprintf(stderr, "TypeError: '<', '<=', '>' and '>=' not supported between instances of 'list'\n");
        return NULL;
    }

    // 字典比较（只支持等于和不等于）
    if (aTypeId == PY_TYPE_DICT && bTypeId == PY_TYPE_DICT)
    {
        if (op == PY_CMP_EQ || op == PY_CMP_NE)
        {
            PyDictObject* dictA = (PyDictObject*)a;
            PyDictObject* dictB = (PyDictObject*)b;
            
            bool isEqual = false;
            
            // 长度必须相同
            if (dictA->size == dictB->size)
            {
                isEqual = true;
                
                // 比较所有键值对
                for (int i = 0; i < dictA->capacity; i++)
                {
                    if (dictA->entries[i].used && dictA->entries[i].key)
                    {
                        PyObject* key = dictA->entries[i].key;
                        PyObject* valueA = dictA->entries[i].value;
                        PyObject* valueB = py_dict_get_item(b, key);
                        
                        // 如果b中没有此键或值不匹配
                        if (!valueB)
                        {
                            isEqual = false;
                            break;
                        }
                        
                        // 递归比较值
                        PyObject* cmpResult = py_object_compare(valueA, valueB, PY_CMP_EQ);
                        py_decref(valueB);  // 释放获取的值
                        
                        if (!cmpResult)
                        {
                            isEqual = false;
                            break;
                        }
                        
                        bool valueEqual = ((PyPrimitiveObject*)cmpResult)->value.boolValue;
                        py_decref(cmpResult);
                        
                        if (!valueEqual)
                        {
                            isEqual = false;
                            break;
                        }
                    }
                }
            }
            
            return py_create_bool(op == PY_CMP_EQ ? isEqual : !isEqual);
        }
        
        fprintf(stderr, "TypeError: '<', '<=', '>' and '>=' not supported between instances of 'dict'\n");
        return NULL;
    }

    // 不同类型之间的比较（除了数值类型之外）
    if (op == PY_CMP_EQ)
    {
        return py_create_bool(false);  // 不同类型永远不相等
    }
    else if (op == PY_CMP_NE)
    {
        return py_create_bool(true);   // 不同类型永远不相等
    }
    else
    {
        fprintf(stderr, "TypeError: Cannot compare %s and %s\n",
                py_type_name(a->typeId), py_type_name(b->typeId));
        return NULL;
    }
}

// 相等性比较辅助函数
bool py_compare_eq(PyObject* a, PyObject* b)
{
    PyObject* result = py_object_compare(a, b, PY_CMP_EQ);
    if (!result)
    {
        return false;
    }
    
    bool eq = ((PyPrimitiveObject*)result)->value.boolValue;
    py_decref(result);
    return eq;
}

// 不等性比较辅助函数
bool py_compare_ne(PyObject* a, PyObject* b)
{
    PyObject* result = py_object_compare(a, b, PY_CMP_NE);
    if (!result)
    {
        return true;
    }
    
    bool ne = ((PyPrimitiveObject*)result)->value.boolValue;
    py_decref(result);
    return ne;
}

// 小于比较辅助函数
bool py_compare_lt(PyObject* a, PyObject* b)
{
    PyObject* result = py_object_compare(a, b, PY_CMP_LT);
    if (!result)
    {
        return false;
    }
    
    bool lt = ((PyPrimitiveObject*)result)->value.boolValue;
    py_decref(result);
    return lt;
}

// 小于等于比较辅助函数
bool py_compare_le(PyObject* a, PyObject* b)
{
    PyObject* result = py_object_compare(a, b, PY_CMP_LE);
    if (!result)
    {
        return false;
    }
    
    bool le = ((PyPrimitiveObject*)result)->value.boolValue;
    py_decref(result);
    return le;
}

// 大于比较辅助函数
bool py_compare_gt(PyObject* a, PyObject* b)
{
    PyObject* result = py_object_compare(a, b, PY_CMP_GT);
    if (!result)
    {
        return false;
    }
    
    bool gt = ((PyPrimitiveObject*)result)->value.boolValue;
    py_decref(result);
    return gt;
}

// 大于等于比较辅助函数
bool py_compare_ge(PyObject* a, PyObject* b)
{
    PyObject* result = py_object_compare(a, b, PY_CMP_GE);
    if (!result)
    {
        return false;
    }
    
    bool ge = ((PyPrimitiveObject*)result)->value.boolValue;
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