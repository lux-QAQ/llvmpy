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
/* bool py_coerce_numeric(PyObject* a, PyObject* b, double* out_a, double* out_b)
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
} */

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

    // --- GMP Numeric Addition ---
    bool aIsNumeric = (aTypeId == PY_TYPE_INT || aTypeId == PY_TYPE_DOUBLE || aTypeId == PY_TYPE_BOOL);
    bool bIsNumeric = (bTypeId == PY_TYPE_INT || bTypeId == PY_TYPE_DOUBLE || bTypeId == PY_TYPE_BOOL);

    if (aIsNumeric && bIsNumeric)
    {
        mpz_ptr a_int = py_extract_int(a);       // NULL if not int
        mpf_ptr a_float = py_extract_double(a);  // NULL if not float
        bool a_bool_val;
        bool a_is_bool = (aTypeId == PY_TYPE_BOOL);
        if (a_is_bool) a_bool_val = py_extract_bool(a);

        mpz_ptr b_int = py_extract_int(b);
        mpf_ptr b_float = py_extract_double(b);
        bool b_bool_val;
        bool b_is_bool = (bTypeId == PY_TYPE_BOOL);
        if (b_is_bool) b_bool_val = py_extract_bool(b);

        // Determine result type: prefer float if any operand is float
        bool result_is_float = (a_float || b_float);

        if (result_is_float)
        {
            mpf_t result_f;
            mpf_init2(result_f, 256);  // Default precision, adjust if needed

            mpf_t temp_a, temp_b;  // Temporaries for potential conversions
            mpf_init2(temp_a, 256);
            mpf_init2(temp_b, 256);
            bool use_temp_a = false, use_temp_b = false;

            mpf_srcptr op_a, op_b;

            // Convert operand A to float if necessary
            if (a_float)
            {
                op_a = a_float;
            }
            else if (a_int)
            {
                mpf_set_z(temp_a, a_int);
                op_a = temp_a;
                use_temp_a = true;
            }
            else if (a_is_bool)
            {
                mpf_set_ui(temp_a, a_bool_val ? 1 : 0);
                op_a = temp_a;
                use_temp_a = true;
            }
            else
            {  // Should not happen if aIsNumeric is true
                fprintf(stderr, "Internal Error: Unexpected type A in numeric add\n");
                mpf_clear(result_f);
                mpf_clear(temp_a);
                mpf_clear(temp_b);
                return NULL;
            }

            // Convert operand B to float if necessary
            if (b_float)
            {
                op_b = b_float;
            }
            else if (b_int)
            {
                mpf_set_z(temp_b, b_int);
                op_b = temp_b;
                use_temp_b = true;
            }
            else if (b_is_bool)
            {
                mpf_set_ui(temp_b, b_bool_val ? 1 : 0);
                op_b = temp_b;
                use_temp_b = true;
            }
            else
            {  // Should not happen
                fprintf(stderr, "Internal Error: Unexpected type B in numeric add\n");
                mpf_clear(result_f);
                mpf_clear(temp_a);
                mpf_clear(temp_b);
                return NULL;
            }

            mpf_add(result_f, op_a, op_b);
            PyObject* resultObj = py_create_double_from_mpf(result_f);
            mpf_clear(result_f);
            if (use_temp_a) mpf_clear(temp_a);
            if (use_temp_b) mpf_clear(temp_b);
            return resultObj;
        }
        else  // Both operands are effectively integers (int or bool)
        {
            mpz_t result_z;
            mpz_init(result_z);

            mpz_t temp_a_z, temp_b_z;  // Only needed for bools
            mpz_init(temp_a_z);
            mpz_init(temp_b_z);
            bool use_temp_a_z = false, use_temp_b_z = false;

            mpz_srcptr op_a_z, op_b_z;

            if (a_int)
            {
                op_a_z = a_int;
            }
            else if (a_is_bool)
            {
                mpz_set_ui(temp_a_z, a_bool_val ? 1 : 0);
                op_a_z = temp_a_z;
                use_temp_a_z = true;
            }
            else
            {  // Should not happen
                fprintf(stderr, "Internal Error: Unexpected type A in int add\n");
                mpz_clear(result_z);
                mpz_clear(temp_a_z);
                mpz_clear(temp_b_z);
                return NULL;
            }

            if (b_int)
            {
                op_b_z = b_int;
            }
            else if (b_is_bool)
            {
                mpz_set_ui(temp_b_z, b_bool_val ? 1 : 0);
                op_b_z = temp_b_z;
                use_temp_b_z = true;
            }
            else
            {  // Should not happen
                fprintf(stderr, "Internal Error: Unexpected type B in int add\n");
                mpz_clear(result_z);
                mpz_clear(temp_a_z);
                mpz_clear(temp_b_z);
                return NULL;
            }

            mpz_add(result_z, op_a_z, op_b_z);
            PyObject* resultObj = py_create_int_from_mpz(result_z);
            mpz_clear(result_z);
            if (use_temp_a_z) mpz_clear(temp_a_z);
            if (use_temp_b_z) mpz_clear(temp_b_z);
            return resultObj;
        }
    }

    // --- String Concatenation ---
    if (aTypeId == PY_TYPE_STRING && bTypeId == PY_TYPE_STRING)
    {
        const char* strA = py_extract_string(a);  // Use extractor
        const char* strB = py_extract_string(b);  // Use extractor

        if (!strA) strA = "";
        if (!strB) strB = "";

        size_t lenA = strlen(strA);
        size_t lenB = strlen(strB);
        // Check for potential overflow if lengths are huge
        if (lenA > SIZE_MAX - lenB - 1)
        {
            fprintf(stderr, "MemoryError: String concatenation result too large\n");
            return NULL;
        }
        char* result = (char*)malloc(lenA + lenB + 1);

        if (!result)
        {
            fprintf(stderr, "MemoryError: Failed to allocate memory for string concatenation\n");
            return NULL;
        }

        // Use memcpy for potentially better performance with long strings
        memcpy(result, strA, lenA);
        memcpy(result + lenA, strB, lenB);
        result[lenA + lenB] = '\0';

        PyObject* resultObj = py_create_string(result);
        free(result);  // py_create_string makes a copy
        return resultObj;
    }

    // --- List Concatenation ---
    if (aTypeId == PY_TYPE_LIST && bTypeId == PY_TYPE_LIST)
    {
        PyListObject* listA = (PyListObject*)a;
        PyListObject* listB = (PyListObject*)b;

        // Determine element type ID for the new list (simple approach)
        int elemTypeId = listA->elemTypeId;
        if (elemTypeId == llvmpy::PY_TYPE_ANY || elemTypeId == 0)
        {
            elemTypeId = listB->elemTypeId;
        }
        else if (listB->elemTypeId != llvmpy::PY_TYPE_ANY && listB->elemTypeId != 0 && elemTypeId != listB->elemTypeId)
        {
            // More complex type compatibility check might be needed here.
            // For now, if types are different and neither is ANY, default to ANY.
            fprintf(stderr, "Warning: Concatenating lists with different specific element types (%s, %s). Resulting list allows ANY.\n",
                    py_type_name(listA->elemTypeId), py_type_name(listB->elemTypeId));
            elemTypeId = llvmpy::PY_TYPE_ANY;
        }

        // Check for potential overflow
        if (listA->length > INT_MAX - listB->length)
        {
            fprintf(stderr, "MemoryError: List concatenation result too large\n");
            return NULL;
        }
        int newLength = listA->length + listB->length;

        // Create a new list (py_create_list handles initial capacity)
        PyObject* resultListObj = py_create_list(newLength, elemTypeId);
        if (!resultListObj) return NULL;
        PyListObject* resultList = (PyListObject*)resultListObj;

        // Copy elements (using direct assignment and incref)
        // Ensure capacity is sufficient (py_create_list should handle this)
        if (resultList->capacity < newLength)
        {
            fprintf(stderr, "Internal Error: Insufficient capacity after py_create_list in list concatenation.\n");
            py_decref(resultListObj);
            return NULL;
        }

        for (int i = 0; i < listA->length; i++)
        {
            PyObject* item = listA->data[i];
            if (item) py_incref(item);
            resultList->data[i] = item;
        }
        for (int i = 0; i < listB->length; i++)
        {
            PyObject* item = listB->data[i];
            if (item) py_incref(item);
            resultList->data[listA->length + i] = item;
        }

        resultList->length = newLength;
        return resultListObj;
    }

    // --- Unsupported Types ---
    fprintf(stderr, "TypeError: Unsupported operand types for +: %s and %s\n",
            py_type_name(aTypeId), py_type_name(bTypeId));
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

    int aTypeId = getBaseTypeId(a->typeId);
    int bTypeId = getBaseTypeId(b->typeId);

    bool aIsNumeric = (aTypeId == PY_TYPE_INT || aTypeId == PY_TYPE_DOUBLE || aTypeId == PY_TYPE_BOOL);
    bool bIsNumeric = (bTypeId == PY_TYPE_INT || bTypeId == PY_TYPE_DOUBLE || bTypeId == PY_TYPE_BOOL);

    if (aIsNumeric && bIsNumeric)
    {
        mpz_ptr a_int = py_extract_int(a);
        mpf_ptr a_float = py_extract_double(a);
        bool a_bool_val;
        bool a_is_bool = (aTypeId == PY_TYPE_BOOL);
        if (a_is_bool) a_bool_val = py_extract_bool(a);
        mpz_ptr b_int = py_extract_int(b);
        mpf_ptr b_float = py_extract_double(b);
        bool b_bool_val;
        bool b_is_bool = (bTypeId == PY_TYPE_BOOL);
        if (b_is_bool) b_bool_val = py_extract_bool(b);

        bool result_is_float = (a_float || b_float);

        if (result_is_float)
        {
            mpf_t result_f;
            mpf_init2(result_f, 256);
            mpf_t temp_a, temp_b;
            mpf_init2(temp_a, 256);
            mpf_init2(temp_b, 256);
            bool use_temp_a = false, use_temp_b = false;
            mpf_srcptr op_a, op_b;

            if (a_float)
                op_a = a_float;
            else if (a_int)
            {
                mpf_set_z(temp_a, a_int);
                op_a = temp_a;
                use_temp_a = true;
            }
            else if (a_is_bool)
            {
                mpf_set_ui(temp_a, a_bool_val ? 1 : 0);
                op_a = temp_a;
                use_temp_a = true;
            }
            else
            { /* Error */
                mpf_clear(result_f);
                mpf_clear(temp_a);
                mpf_clear(temp_b);
                return NULL;
            }

            if (b_float)
                op_b = b_float;
            else if (b_int)
            {
                mpf_set_z(temp_b, b_int);
                op_b = temp_b;
                use_temp_b = true;
            }
            else if (b_is_bool)
            {
                mpf_set_ui(temp_b, b_bool_val ? 1 : 0);
                op_b = temp_b;
                use_temp_b = true;
            }
            else
            { /* Error */
                mpf_clear(result_f);
                mpf_clear(temp_a);
                mpf_clear(temp_b);
                return NULL;
            }

            mpf_sub(result_f, op_a, op_b);
            PyObject* resultObj = py_create_double_from_mpf(result_f);
            mpf_clear(result_f);
            if (use_temp_a) mpf_clear(temp_a);
            if (use_temp_b) mpf_clear(temp_b);
            return resultObj;
        }
        else
        {  // int/bool - int/bool
            mpz_t result_z;
            mpz_init(result_z);
            mpz_t temp_a_z, temp_b_z;
            mpz_init(temp_a_z);
            mpz_init(temp_b_z);
            bool use_temp_a_z = false, use_temp_b_z = false;
            mpz_srcptr op_a_z, op_b_z;

            if (a_int)
                op_a_z = a_int;
            else if (a_is_bool)
            {
                mpz_set_ui(temp_a_z, a_bool_val ? 1 : 0);
                op_a_z = temp_a_z;
                use_temp_a_z = true;
            }
            else
            { /* Error */
                mpz_clear(result_z);
                mpz_clear(temp_a_z);
                mpz_clear(temp_b_z);
                return NULL;
            }

            if (b_int)
                op_b_z = b_int;
            else if (b_is_bool)
            {
                mpz_set_ui(temp_b_z, b_bool_val ? 1 : 0);
                op_b_z = temp_b_z;
                use_temp_b_z = true;
            }
            else
            { /* Error */
                mpz_clear(result_z);
                mpz_clear(temp_a_z);
                mpz_clear(temp_b_z);
                return NULL;
            }

            mpz_sub(result_z, op_a_z, op_b_z);
            PyObject* resultObj = py_create_int_from_mpz(result_z);
            mpz_clear(result_z);
            if (use_temp_a_z) mpz_clear(temp_a_z);
            if (use_temp_b_z) mpz_clear(temp_b_z);
            return resultObj;
        }
    }

    fprintf(stderr, "TypeError: Unsupported operand types for -: %s and %s\n",
            py_type_name(aTypeId), py_type_name(bTypeId));
    return NULL;
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

    // --- GMP Numeric Multiplication ---
    bool aIsNumeric = (aTypeId == PY_TYPE_INT || aTypeId == PY_TYPE_DOUBLE || aTypeId == PY_TYPE_BOOL);
    bool bIsNumeric = (bTypeId == PY_TYPE_INT || bTypeId == PY_TYPE_DOUBLE || bTypeId == PY_TYPE_BOOL);

    if (aIsNumeric && bIsNumeric)
    {
        mpz_ptr a_int = py_extract_int(a);
        mpf_ptr a_float = py_extract_double(a);
        bool a_bool_val;
        bool a_is_bool = (aTypeId == PY_TYPE_BOOL);
        if (a_is_bool) a_bool_val = py_extract_bool(a);
        mpz_ptr b_int = py_extract_int(b);
        mpf_ptr b_float = py_extract_double(b);
        bool b_bool_val;
        bool b_is_bool = (bTypeId == PY_TYPE_BOOL);
        if (b_is_bool) b_bool_val = py_extract_bool(b);

        bool result_is_float = (a_float || b_float);

        if (result_is_float)
        {
            mpf_t result_f;
            mpf_init2(result_f, 256);
            mpf_t temp_a, temp_b;
            mpf_init2(temp_a, 256);
            mpf_init2(temp_b, 256);
            bool use_temp_a = false, use_temp_b = false;
            mpf_srcptr op_a, op_b;

            if (a_float)
                op_a = a_float;
            else if (a_int)
            {
                mpf_set_z(temp_a, a_int);
                op_a = temp_a;
                use_temp_a = true;
            }
            else if (a_is_bool)
            {
                mpf_set_ui(temp_a, a_bool_val ? 1 : 0);
                op_a = temp_a;
                use_temp_a = true;
            }
            else
            { /* Error */
                mpf_clear(result_f);
                mpf_clear(temp_a);
                mpf_clear(temp_b);
                return NULL;
            }

            if (b_float)
                op_b = b_float;
            else if (b_int)
            {
                mpf_set_z(temp_b, b_int);
                op_b = temp_b;
                use_temp_b = true;
            }
            else if (b_is_bool)
            {
                mpf_set_ui(temp_b, b_bool_val ? 1 : 0);
                op_b = temp_b;
                use_temp_b = true;
            }
            else
            { /* Error */
                mpf_clear(result_f);
                mpf_clear(temp_a);
                mpf_clear(temp_b);
                return NULL;
            }

            mpf_mul(result_f, op_a, op_b);
            PyObject* resultObj = py_create_double_from_mpf(result_f);
            mpf_clear(result_f);
            if (use_temp_a) mpf_clear(temp_a);
            if (use_temp_b) mpf_clear(temp_b);
            return resultObj;
        }
        else
        {  // int/bool * int/bool
            mpz_t result_z;
            mpz_init(result_z);
            mpz_t temp_a_z, temp_b_z;
            mpz_init(temp_a_z);
            mpz_init(temp_b_z);
            bool use_temp_a_z = false, use_temp_b_z = false;
            mpz_srcptr op_a_z, op_b_z;

            if (a_int)
                op_a_z = a_int;
            else if (a_is_bool)
            {
                mpz_set_ui(temp_a_z, a_bool_val ? 1 : 0);
                op_a_z = temp_a_z;
                use_temp_a_z = true;
            }
            else
            { /* Error */
                mpz_clear(result_z);
                mpz_clear(temp_a_z);
                mpz_clear(temp_b_z);
                return NULL;
            }

            if (b_int)
                op_b_z = b_int;
            else if (b_is_bool)
            {
                mpz_set_ui(temp_b_z, b_bool_val ? 1 : 0);
                op_b_z = temp_b_z;
                use_temp_b_z = true;
            }
            else
            { /* Error */
                mpz_clear(result_z);
                mpz_clear(temp_a_z);
                mpz_clear(temp_b_z);
                return NULL;
            }

            mpz_mul(result_z, op_a_z, op_b_z);
            PyObject* resultObj = py_create_int_from_mpz(result_z);
            mpz_clear(result_z);
            if (use_temp_a_z) mpz_clear(temp_a_z);
            if (use_temp_b_z) mpz_clear(temp_b_z);
            return resultObj;
        }
    }

    // --- Sequence Repetition (String * Int or List * Int) ---
    PyObject* seq = NULL;
    PyObject* countObj = NULL;
    int seqTypeId = 0;

    if ((aTypeId == PY_TYPE_STRING || aTypeId == PY_TYPE_LIST) && bTypeId == PY_TYPE_INT)
    {
        seq = a;
        countObj = b;
        seqTypeId = aTypeId;
    }
    else if (aTypeId == PY_TYPE_INT && (bTypeId == PY_TYPE_STRING || bTypeId == PY_TYPE_LIST))
    {
        seq = b;
        countObj = a;
        seqTypeId = bTypeId;
    }

    if (seq && countObj)
    {
        mpz_ptr count_z = py_extract_int(countObj);
        if (!count_z)
        {
            fprintf(stderr, "TypeError: can't multiply sequence by non-int of type '%s'\n", py_type_name(bTypeId));
            return NULL;
        }

        // Check if count fits in a C integer type suitable for loops/malloc
        long count_val;
        if (!mpz_fits_slong_p(count_z) || (count_val = mpz_get_si(count_z)) < 0)
        {
            // Python allows huge counts resulting in MemoryError later,
            // but negative counts result in empty sequence.
            // We limit to 'long' for practical reasons here.
            if (mpz_sgn(count_z) < 0)
            {
                count_val = 0;  // Treat negative count as 0
            }
            else
            {
                fprintf(stderr, "OverflowError: cannot fit 'int' count into C long for sequence repetition\n");
                return NULL;  // Or MemoryError? Python raises OverflowError here.
            }
        }

        if (count_val == 0)
        {
            if (seqTypeId == PY_TYPE_STRING) return py_create_string("");
            if (seqTypeId == PY_TYPE_LIST) return py_create_list(0, ((PyListObject*)seq)->elemTypeId);
        }

        if (seqTypeId == PY_TYPE_STRING)
        {
            const char* str = py_extract_string(seq);
            if (!str) str = "";
            size_t len = strlen(str);

            // Check for potential overflow before malloc
            if (len > 0 && count_val > SIZE_MAX / len)
            {
                fprintf(stderr, "MemoryError: String repetition result too large\n");
                return NULL;
            }
            size_t total_len = len * count_val;
            char* result = (char*)malloc(total_len + 1);
            if (!result)
            {
                fprintf(stderr, "MemoryError: Failed to allocate memory for string repetition\n");
                return NULL;
            }

            char* ptr = result;
            for (long i = 0; i < count_val; ++i)
            {
                memcpy(ptr, str, len);
                ptr += len;
            }
            *ptr = '\0';

            PyObject* resultObj = py_create_string(result);
            free(result);
            return resultObj;
        }
        else if (seqTypeId == PY_TYPE_LIST)
        {
            PyListObject* list = (PyListObject*)seq;

            // Check for potential overflow before creating list
            if (list->length > 0 && count_val > INT_MAX / list->length)
            {
                fprintf(stderr, "MemoryError: List repetition result too large\n");
                return NULL;
            }
            int newLength = list->length * count_val;

            PyObject* resultListObj = py_create_list(newLength, list->elemTypeId);
            if (!resultListObj) return NULL;
            PyListObject* resultList = (PyListObject*)resultListObj;

            if (resultList->capacity < newLength)
            {
                fprintf(stderr, "Internal Error: Insufficient capacity after py_create_list in list repetition.\n");
                py_decref(resultListObj);
                return NULL;
            }

            PyObject** dest_ptr = resultList->data;
            for (long c = 0; c < count_val; ++c)
            {
                for (int i = 0; i < list->length; ++i)
                {
                    PyObject* item = list->data[i];
                    if (item) py_incref(item);
                    *dest_ptr++ = item;
                }
            }
            resultList->length = newLength;
            return resultListObj;
        }
    }

    // --- Unsupported Types ---
    fprintf(stderr, "TypeError: Unsupported operand types for *: %s and %s\n",
            py_type_name(aTypeId), py_type_name(bTypeId));
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

    int aTypeId = getBaseTypeId(a->typeId);
    int bTypeId = getBaseTypeId(b->typeId);

    bool aIsNumeric = (aTypeId == PY_TYPE_INT || aTypeId == PY_TYPE_DOUBLE || aTypeId == PY_TYPE_BOOL);
    bool bIsNumeric = (bTypeId == PY_TYPE_INT || bTypeId == PY_TYPE_DOUBLE || bTypeId == PY_TYPE_BOOL);

    if (aIsNumeric && bIsNumeric)
    {
        mpz_ptr a_int = py_extract_int(a);
        mpf_ptr a_float = py_extract_double(a);
        bool a_bool_val;
        bool a_is_bool = (aTypeId == PY_TYPE_BOOL);
        if (a_is_bool) a_bool_val = py_extract_bool(a);
        mpz_ptr b_int = py_extract_int(b);
        mpf_ptr b_float = py_extract_double(b);
        bool b_bool_val;
        bool b_is_bool = (bTypeId == PY_TYPE_BOOL);
        if (b_is_bool) b_bool_val = py_extract_bool(b);

        // Result is always float
        mpf_t result_f;
        mpf_init2(result_f, 256);
        mpf_t temp_a, temp_b;
        mpf_init2(temp_a, 256);
        mpf_init2(temp_b, 256);
        bool use_temp_a = false, use_temp_b = false;
        mpf_srcptr op_a, op_b;

        // Convert A to float
        if (a_float)
            op_a = a_float;
        else if (a_int)
        {
            mpf_set_z(temp_a, a_int);
            op_a = temp_a;
            use_temp_a = true;
        }
        else if (a_is_bool)
        {
            mpf_set_ui(temp_a, a_bool_val ? 1 : 0);
            op_a = temp_a;
            use_temp_a = true;
        }
        else
        { /* Error */
            mpf_clear(result_f);
            mpf_clear(temp_a);
            mpf_clear(temp_b);
            return NULL;
        }

        // Convert B to float
        if (b_float)
            op_b = b_float;
        else if (b_int)
        {
            mpf_set_z(temp_b, b_int);
            op_b = temp_b;
            use_temp_b = true;
        }
        else if (b_is_bool)
        {
            mpf_set_ui(temp_b, b_bool_val ? 1 : 0);
            op_b = temp_b;
            use_temp_b = true;
        }
        else
        { /* Error */
            mpf_clear(result_f);
            mpf_clear(temp_a);
            mpf_clear(temp_b);
            return NULL;
        }

        // Check for division by zero
        if (mpf_sgn(op_b) == 0)
        {
            fprintf(stderr, "ZeroDivisionError: float division by zero\n");
            mpf_clear(result_f);
            if (use_temp_a) mpf_clear(temp_a);
            if (use_temp_b) mpf_clear(temp_b);
            return NULL;
        }

        mpf_div(result_f, op_a, op_b);
        PyObject* resultObj = py_create_double_from_mpf(result_f);
        mpf_clear(result_f);
        if (use_temp_a) mpf_clear(temp_a);
        if (use_temp_b) mpf_clear(temp_b);
        return resultObj;
    }

    fprintf(stderr, "TypeError: Unsupported operand types for /: %s and %s\n",
            py_type_name(aTypeId), py_type_name(bTypeId));
    return NULL;
}

// 模运算操作符
PyObject* py_object_modulo(PyObject* a, PyObject* b)
{
    if (!a || !b)
    {
        fprintf(stderr, "TypeError: Cannot perform modulo with None\n");
        return NULL;
    }

    int aTypeId = getBaseTypeId(a->typeId);
    int bTypeId = getBaseTypeId(b->typeId);

    bool aIsNumeric = (aTypeId == PY_TYPE_INT || aTypeId == PY_TYPE_DOUBLE || aTypeId == PY_TYPE_BOOL);
    bool bIsNumeric = (bTypeId == PY_TYPE_INT || bTypeId == PY_TYPE_DOUBLE || bTypeId == PY_TYPE_BOOL);

    if (aIsNumeric && bIsNumeric)
    {
        mpz_ptr a_int = py_extract_int(a);
        mpf_ptr a_float = py_extract_double(a);
        bool a_bool_val;
        bool a_is_bool = (aTypeId == PY_TYPE_BOOL);
        if (a_is_bool) a_bool_val = py_extract_bool(a);
        mpz_ptr b_int = py_extract_int(b);
        mpf_ptr b_float = py_extract_double(b);
        bool b_bool_val;
        bool b_is_bool = (bTypeId == PY_TYPE_BOOL);
        if (b_is_bool) b_bool_val = py_extract_bool(b);

        bool result_is_float = (a_float || b_float);

        if (result_is_float)
        {
            // --- Corrected Float Modulo Implementation ---
            // Python's float % definition: result = a - floor(a/b) * b
            mpf_t result_f; mpf_init2(result_f, 256);
            mpf_t temp_a, temp_b; mpf_init2(temp_a, 256); mpf_init2(temp_b, 256);
            mpf_t div_res, floor_div_res, term_to_sub; // Intermediate results
            mpf_init2(div_res, 256);
            mpf_init2(floor_div_res, 256);
            mpf_init2(term_to_sub, 256);

            bool use_temp_a = false, use_temp_b = false;
            mpf_srcptr op_a, op_b;

            // Convert A to float
            if (a_float) op_a = a_float;
            else if (a_int) { mpf_set_z(temp_a, a_int); op_a = temp_a; use_temp_a = true; }
            else if (a_is_bool) { mpf_set_ui(temp_a, a_bool_val ? 1 : 0); op_a = temp_a; use_temp_a = true; }
            else { /* Error */
                fprintf(stderr, "Internal Error: Unexpected type A in float modulo\n");
                mpf_clear(result_f); mpf_clear(temp_a); mpf_clear(temp_b);
                mpf_clear(div_res); mpf_clear(floor_div_res); mpf_clear(term_to_sub);
                return NULL;
            }

            // Convert B to float
            if (b_float) op_b = b_float;
            else if (b_int) { mpf_set_z(temp_b, b_int); op_b = temp_b; use_temp_b = true; }
            else if (b_is_bool) { mpf_set_ui(temp_b, b_bool_val ? 1 : 0); op_b = temp_b; use_temp_b = true; }
            else { /* Error */
                 fprintf(stderr, "Internal Error: Unexpected type B in float modulo\n");
                 mpf_clear(result_f); mpf_clear(temp_a); mpf_clear(temp_b);
                 mpf_clear(div_res); mpf_clear(floor_div_res); mpf_clear(term_to_sub);
                 if (use_temp_a) mpf_clear(temp_a); // Clear temp_a if used
                 return NULL;
            }

            // Check for division by zero
            if (mpf_sgn(op_b) == 0)
            {
                fprintf(stderr, "ZeroDivisionError: float modulo by zero\n");
                mpf_clear(result_f); mpf_clear(temp_a); mpf_clear(temp_b);
                mpf_clear(div_res); mpf_clear(floor_div_res); mpf_clear(term_to_sub);
                if (use_temp_a) mpf_clear(temp_a); if (use_temp_b) mpf_clear(temp_b);
                return NULL;
            }

            // Calculate a / b
            mpf_div(div_res, op_a, op_b);
            // Calculate floor(a / b)
            mpf_floor(floor_div_res, div_res);
            // Calculate floor(a / b) * b
            mpf_mul(term_to_sub, floor_div_res, op_b);
            // Calculate a - (floor(a / b) * b)
            mpf_sub(result_f, op_a, term_to_sub);

            PyObject* resultObj = py_create_double_from_mpf(result_f);

            // Clear all temporary GMP variables
            mpf_clear(result_f);
            if (use_temp_a) mpf_clear(temp_a);
            if (use_temp_b) mpf_clear(temp_b);
            mpf_clear(div_res);
            mpf_clear(floor_div_res);
            mpf_clear(term_to_sub);

            return resultObj;
        }
        else
        {  // int/bool % int/bool (This part remains the same)
            mpz_t result_z;
            mpz_init(result_z);
            mpz_t temp_a_z, temp_b_z;
            mpz_init(temp_a_z);
            mpz_init(temp_b_z);
            bool use_temp_a_z = false, use_temp_b_z = false;
            mpz_srcptr op_a_z, op_b_z;

            if (a_int)
                op_a_z = a_int;
            else if (a_is_bool)
            {
                mpz_set_ui(temp_a_z, a_bool_val ? 1 : 0);
                op_a_z = temp_a_z;
                use_temp_a_z = true;
            }
            else
            { /* Error */
                fprintf(stderr, "Internal Error: Unexpected type A in int modulo\n");
                mpz_clear(result_z);
                mpz_clear(temp_a_z);
                mpz_clear(temp_b_z);
                return NULL;
            }

            if (b_int)
                op_b_z = b_int;
            else if (b_is_bool)
            {
                mpz_set_ui(temp_b_z, b_bool_val ? 1 : 0);
                op_b_z = temp_b_z;
                use_temp_b_z = true;
            }
            else
            { /* Error */
                 fprintf(stderr, "Internal Error: Unexpected type B in int modulo\n");
                 mpz_clear(result_z);
                 mpz_clear(temp_a_z);
                 mpz_clear(temp_b_z);
                 if (use_temp_a_z) mpz_clear(temp_a_z); // Clear temp_a_z if used
                 return NULL;
            }

            if (mpz_sgn(op_b_z) == 0)
            {
                fprintf(stderr, "ZeroDivisionError: integer modulo by zero\n");
                mpz_clear(result_z);
                if (use_temp_a_z) mpz_clear(temp_a_z);
                if (use_temp_b_z) mpz_clear(temp_b_z);
                return NULL;
            }

            // Use mpz_mod for Python's % behavior (result has same sign as divisor)
            mpz_mod(result_z, op_a_z, op_b_z);

            PyObject* resultObj = py_create_int_from_mpz(result_z);
            mpz_clear(result_z);
            if (use_temp_a_z) mpz_clear(temp_a_z);
            if (use_temp_b_z) mpz_clear(temp_b_z);
            return resultObj;
        }
    }

    fprintf(stderr, "TypeError: Unsupported operand types for %%: %s and %s\n",
            py_type_name(aTypeId), py_type_name(bTypeId));
    return NULL;
}

// 整除操作符
PyObject* py_object_floor_divide(PyObject* a, PyObject* b)
{
    if (!a || !b)
    {
        fprintf(stderr, "TypeError: Cannot perform floor division with None\n");
        return NULL;
    }

    int aTypeId = getBaseTypeId(a->typeId);
    int bTypeId = getBaseTypeId(b->typeId);

    bool aIsNumeric = (aTypeId == PY_TYPE_INT || aTypeId == PY_TYPE_DOUBLE || aTypeId == PY_TYPE_BOOL);
    bool bIsNumeric = (bTypeId == PY_TYPE_INT || bTypeId == PY_TYPE_DOUBLE || bTypeId == PY_TYPE_BOOL);

    if (aIsNumeric && bIsNumeric)
    {
        mpz_ptr a_int = py_extract_int(a);
        mpf_ptr a_float = py_extract_double(a);
        bool a_bool_val;
        bool a_is_bool = (aTypeId == PY_TYPE_BOOL);
        if (a_is_bool) a_bool_val = py_extract_bool(a);
        mpz_ptr b_int = py_extract_int(b);
        mpf_ptr b_float = py_extract_double(b);
        bool b_bool_val;
        bool b_is_bool = (bTypeId == PY_TYPE_BOOL);
        if (b_is_bool) b_bool_val = py_extract_bool(b);

        bool result_is_float = (a_float || b_float);

        if (result_is_float)
        {
            // Float // Float -> Float
            mpf_t result_f;
            mpf_init2(result_f, 256);
            mpf_t temp_a, temp_b;
            mpf_init2(temp_a, 256);
            mpf_init2(temp_b, 256);
            bool use_temp_a = false, use_temp_b = false;
            mpf_srcptr op_a, op_b;

            if (a_float)
                op_a = a_float;
            else if (a_int)
            {
                mpf_set_z(temp_a, a_int);
                op_a = temp_a;
                use_temp_a = true;
            }
            else if (a_is_bool)
            {
                mpf_set_ui(temp_a, a_bool_val ? 1 : 0);
                op_a = temp_a;
                use_temp_a = true;
            }
            else
            { /* Error */
                mpf_clear(result_f);
                mpf_clear(temp_a);
                mpf_clear(temp_b);
                return NULL;
            }

            if (b_float)
                op_b = b_float;
            else if (b_int)
            {
                mpf_set_z(temp_b, b_int);
                op_b = temp_b;
                use_temp_b = true;
            }
            else if (b_is_bool)
            {
                mpf_set_ui(temp_b, b_bool_val ? 1 : 0);
                op_b = temp_b;
                use_temp_b = true;
            }
            else
            { /* Error */
                mpf_clear(result_f);
                mpf_clear(temp_a);
                mpf_clear(temp_b);
                return NULL;
            }

            if (mpf_sgn(op_b) == 0)
            {
                fprintf(stderr, "ZeroDivisionError: float floor division by zero\n");
                mpf_clear(result_f);
                if (use_temp_a) mpf_clear(temp_a);
                if (use_temp_b) mpf_clear(temp_b);
                return NULL;
            }

            mpf_div(result_f, op_a, op_b);
            mpf_floor(result_f, result_f);  // Apply floor

            PyObject* resultObj = py_create_double_from_mpf(result_f);
            mpf_clear(result_f);
            if (use_temp_a) mpf_clear(temp_a);
            if (use_temp_b) mpf_clear(temp_b);
            return resultObj;
        }
        else
        {  // int/bool // int/bool -> int
            mpz_t result_z;
            mpz_init(result_z);
            mpz_t temp_a_z, temp_b_z;
            mpz_init(temp_a_z);
            mpz_init(temp_b_z);
            bool use_temp_a_z = false, use_temp_b_z = false;
            mpz_srcptr op_a_z, op_b_z;

            if (a_int)
                op_a_z = a_int;
            else if (a_is_bool)
            {
                mpz_set_ui(temp_a_z, a_bool_val ? 1 : 0);
                op_a_z = temp_a_z;
                use_temp_a_z = true;
            }
            else
            { /* Error */
                mpz_clear(result_z);
                mpz_clear(temp_a_z);
                mpz_clear(temp_b_z);
                return NULL;
            }

            if (b_int)
                op_b_z = b_int;
            else if (b_is_bool)
            {
                mpz_set_ui(temp_b_z, b_bool_val ? 1 : 0);
                op_b_z = temp_b_z;
                use_temp_b_z = true;
            }
            else
            { /* Error */
                mpz_clear(result_z);
                mpz_clear(temp_a_z);
                mpz_clear(temp_b_z);
                return NULL;
            }

            if (mpz_sgn(op_b_z) == 0)
            {
                fprintf(stderr, "ZeroDivisionError: integer division or modulo by zero\n");
                mpz_clear(result_z);
                if (use_temp_a_z) mpz_clear(temp_a_z);
                if (use_temp_b_z) mpz_clear(temp_b_z);
                return NULL;
            }

            // Use mpz_fdiv_q for floor division behavior
            mpz_fdiv_q(result_z, op_a_z, op_b_z);

            PyObject* resultObj = py_create_int_from_mpz(result_z);
            mpz_clear(result_z);
            if (use_temp_a_z) mpz_clear(temp_a_z);
            if (use_temp_b_z) mpz_clear(temp_b_z);
            return resultObj;
        }
    }

    fprintf(stderr, "TypeError: Unsupported operand types for //: %s and %s\n",
            py_type_name(aTypeId), py_type_name(bTypeId));
    return NULL;
}

// 幂运算操作符
PyObject* py_object_power(PyObject* a, PyObject* b)
{
    if (!a || !b)
    {
        fprintf(stderr, "TypeError: Cannot perform power operation with None\n");
        return NULL;
    }

    int aTypeId = getBaseTypeId(a->typeId);
    int bTypeId = getBaseTypeId(b->typeId);

    bool aIsNumeric = (aTypeId == PY_TYPE_INT || aTypeId == PY_TYPE_DOUBLE || aTypeId == PY_TYPE_BOOL);
    bool bIsNumeric = (bTypeId == PY_TYPE_INT || bTypeId == PY_TYPE_DOUBLE || bTypeId == PY_TYPE_BOOL);

    if (aIsNumeric && bIsNumeric)
    {
        mpz_ptr a_int = py_extract_int(a);
        mpf_ptr a_float = py_extract_double(a);
        bool a_bool_val;
        bool a_is_bool = (aTypeId == PY_TYPE_BOOL);
        if (a_is_bool) a_bool_val = py_extract_bool(a);
        mpz_ptr b_int = py_extract_int(b);
        mpf_ptr b_float = py_extract_double(b);
        bool b_bool_val;
        bool b_is_bool = (bTypeId == PY_TYPE_BOOL);
        if (b_is_bool) b_bool_val = py_extract_bool(b);

        // --- Case 1: Integer base, Integer exponent ---
        if ((a_int || a_is_bool) && (b_int || b_is_bool))
        {
            mpz_t base_z, exp_z, result_z;
            mpz_init(base_z);
            mpz_init(exp_z);
            mpz_init(result_z);

            if (a_int)
                mpz_set(base_z, a_int);
            else
                mpz_set_ui(base_z, a_bool_val ? 1 : 0);
            if (b_int)
                mpz_set(exp_z, b_int);
            else
                mpz_set_ui(exp_z, b_bool_val ? 1 : 0);

            // Handle negative exponent: result is float
            if (mpz_sgn(exp_z) < 0)
            {
                // Check base == 0
                if (mpz_sgn(base_z) == 0)
                {
                    fprintf(stderr, "ZeroDivisionError: 0 cannot be raised to a negative power\n");
                    mpz_clear(base_z);
                    mpz_clear(exp_z);
                    mpz_clear(result_z);
                    return NULL;
                }
                // Convert base and exponent to float and calculate
                mpf_t base_f, exp_f, result_f;
                mpf_init2(base_f, 256);
                mpf_init2(exp_f, 256);
                mpf_init2(result_f, 256);
                mpf_set_z(base_f, base_z);
                mpf_set_z(exp_f, exp_z);  // mpf can handle negative exponent for pow

                // mpf_pow_ui requires unsigned long exponent. Need general float power.
                // GMP doesn't have a direct mpf_pow_f. Use logarithms or external library (like MPFR's mpfr_pow).
                // Simple fallback: convert to double and use pow()
                fprintf(stderr, "Warning: GMP mpf_pow with float exponent not directly available. Using double precision via pow().\n");
                double base_d = mpf_get_d(base_f);
                double exp_d = mpf_get_d(exp_f);
                double result_d = pow(base_d, exp_d);
                mpf_set_d(result_f, result_d);

                PyObject* resultObj = py_create_double_from_mpf(result_f);
                mpf_clear(base_f);
                mpf_clear(exp_f);
                mpf_clear(result_f);
                mpz_clear(base_z);
                mpz_clear(exp_z);
                mpz_clear(result_z);
                return resultObj;
            }
            else
            {  // Non-negative integer exponent
                // Check if exponent fits in unsigned long for mpz_pow_ui
                if (mpz_fits_ulong_p(exp_z))
                {
                    unsigned long exp_ul = mpz_get_ui(exp_z);
                    mpz_pow_ui(result_z, base_z, exp_ul);
                    PyObject* resultObj = py_create_int_from_mpz(result_z);
                    mpz_clear(base_z);
                    mpz_clear(exp_z);
                    mpz_clear(result_z);
                    return resultObj;
                }
                else
                {
                    // Exponent too large for mpz_pow_ui. Result will be huge or potentially float.
                    // Fallback to float calculation for simplicity, though precision is lost.
                    fprintf(stderr, "Warning: Integer exponent too large for mpz_pow_ui. Falling back to float calculation.\n");
                    // Fall through to float case below
                }
            }
            mpz_clear(base_z);
            mpz_clear(exp_z);
            mpz_clear(result_z);  // Clear temps if falling through
        }

        // --- Case 2: At least one operand is float (or large int exponent fallback) ---
        // Result is always float
        mpf_t base_f, exp_f, result_f;
        mpf_init2(base_f, 256);
        mpf_init2(exp_f, 256);
        mpf_init2(result_f, 256);
        mpf_t temp_a, temp_b;
        mpf_init2(temp_a, 256);
        mpf_init2(temp_b, 256);  // For conversions
        bool use_temp_a = false, use_temp_b = false;
        mpf_srcptr op_a_f, op_b_f;  // Operands as floats

        // Convert base A to float
        if (a_float)
            op_a_f = a_float;
        else if (a_int)
        {
            mpf_set_z(temp_a, a_int);
            op_a_f = temp_a;
            use_temp_a = true;
        }
        else if (a_is_bool)
        {
            mpf_set_ui(temp_a, a_bool_val ? 1 : 0);
            op_a_f = temp_a;
            use_temp_a = true;
        }
        else
        { /* Error */
            mpf_clear(base_f);
            mpf_clear(exp_f);
            mpf_clear(result_f);
            mpf_clear(temp_a);
            mpf_clear(temp_b);
            return NULL;
        }

        // Convert exponent B to float
        if (b_float)
            op_b_f = b_float;
        else if (b_int)
        {
            mpf_set_z(temp_b, b_int);
            op_b_f = temp_b;
            use_temp_b = true;
        }
        else if (b_is_bool)
        {
            mpf_set_ui(temp_b, b_bool_val ? 1 : 0);
            op_b_f = temp_b;
            use_temp_b = true;
        }
        else
        { /* Error */
            mpf_clear(base_f);
            mpf_clear(exp_f);
            mpf_clear(result_f);
            mpf_clear(temp_a);
            mpf_clear(temp_b);
            return NULL;
        }

        // Check 0.0 ** negative
        if (mpf_sgn(op_a_f) == 0 && mpf_sgn(op_b_f) < 0)
        {
            fprintf(stderr, "ZeroDivisionError: 0.0 cannot be raised to a negative power\n");
            mpf_clear(base_f);
            mpf_clear(exp_f);
            mpf_clear(result_f);
            if (use_temp_a) mpf_clear(temp_a);
            if (use_temp_b) mpf_clear(temp_b);
            return NULL;
        }

        // Use mpf_pow_ui if exponent is a non-negative integer representable as ulong
        bool exp_is_simple_int = false;
        unsigned long exp_ul = 0;
        if (!b_float && b_int && mpz_sgn(b_int) >= 0 && mpz_fits_ulong_p(b_int))
        {
            exp_ul = mpz_get_ui(b_int);
            exp_is_simple_int = true;
        }
        else if (!b_float && b_is_bool)
        {
            exp_ul = b_bool_val ? 1 : 0;
            exp_is_simple_int = true;
        }

        if (exp_is_simple_int)
        {
            mpf_pow_ui(result_f, op_a_f, exp_ul);
        }
        else
        {
            // General float power - use double precision fallback
            fprintf(stderr, "Warning: GMP mpf_pow with float exponent not directly available. Using double precision via pow().\n");
            double base_d = mpf_get_d(op_a_f);
            double exp_d = mpf_get_d(op_b_f);
            // Add domain error check for pow (e.g., negative base to non-integer power)
            if (base_d < 0 && exp_d != floor(exp_d))
            {
                fprintf(stderr, "ValueError: negative number cannot be raised to a fractional power\n");
                mpf_clear(base_f);
                mpf_clear(exp_f);
                mpf_clear(result_f);
                if (use_temp_a) mpf_clear(temp_a);
                if (use_temp_b) mpf_clear(temp_b);
                return NULL;
            }
            double result_d = pow(base_d, exp_d);
            mpf_set_d(result_f, result_d);
        }

        PyObject* resultObj = py_create_double_from_mpf(result_f);
        mpf_clear(base_f);
        mpf_clear(exp_f);
        mpf_clear(result_f);
        if (use_temp_a) mpf_clear(temp_a);
        if (use_temp_b) mpf_clear(temp_b);
        return resultObj;
    }

    fprintf(stderr, "TypeError: Unsupported operand types for **: %s and %s\n",
            py_type_name(aTypeId), py_type_name(bTypeId));
    return NULL;
}

//===----------------------------------------------------------------------===//
// 一元操作符实现
//===----------------------------------------------------------------------===//

// 一元取负操作符
PyObject* py_object_negate(PyObject* a)
{
    if (!a)
    {
        fprintf(stderr, "TypeError: bad operand type for unary -: 'NoneType'\n");
        return NULL;
    }

    int aTypeId = getBaseTypeId(a->typeId);

    if (aTypeId == PY_TYPE_INT)
    {
        mpz_ptr val_z = py_extract_int(a);
        if (!val_z)
        { /* Error */
            return NULL;
        }
        mpz_t result_z;
        mpz_init(result_z);
        mpz_neg(result_z, val_z);
        PyObject* resultObj = py_create_int_from_mpz(result_z);
        mpz_clear(result_z);
        return resultObj;
    }
    else if (aTypeId == PY_TYPE_DOUBLE)
    {
        mpf_ptr val_f = py_extract_double(a);
        if (!val_f)
        { /* Error */
            return NULL;
        }
        mpf_t result_f;
        // Initialize with same precision as original
        mpf_init2(result_f, mpf_get_prec(val_f));
        mpf_neg(result_f, val_f);
        PyObject* resultObj = py_create_double_from_mpf(result_f);
        mpf_clear(result_f);
        return resultObj;
    }
    else if (aTypeId == PY_TYPE_BOOL)
    {
        bool value = py_extract_bool(a);
        // Negation of bool results in int -1 or 0
        return py_create_int(value ? -1 : 0);
    }

    fprintf(stderr, "TypeError: bad operand type for unary -: '%s'\n",
            py_type_name(aTypeId));
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
    // 1. Handle NULL / None (same as before)
    if (!a || !b)
    {
        bool result = false;
        if (op == PY_CMP_EQ)
            result = (a == b);
        else if (op == PY_CMP_NE)
            result = (a != b);
        else
        {
            // Python 3 raises TypeError for None comparisons other than ==, !=
            fprintf(stderr, "TypeError: '%s' not supported between instances of '%s' and '%s'\n",
                    py_compare_op_name(op),
                    a ? py_type_name(py_get_safe_type_id(a)) : "NoneType",
                    b ? py_type_name(py_get_safe_type_id(b)) : "NoneType");
            return NULL;  // Indicate error
        }
        return py_create_bool(result);
    }

    // 2. Use type dispatch for EQ and NE (relies on GMP-aware equals from py_type_dispatch)
    if (op == PY_CMP_EQ || op == PY_CMP_NE)
    {
        int typeIdA = py_get_safe_type_id(a);
        const PyTypeMethods* methodsA = py_get_type_methods(typeIdA);

        if (methodsA && methodsA->equals)
        {
            PyObject* resultObj = methodsA->equals(a, b);
            if (resultObj)
            {
                if (py_get_safe_type_id(resultObj) == PY_TYPE_BOOL)
                {
                    bool eq_result = py_extract_bool(resultObj);
                    py_decref(resultObj);
                    return py_create_bool(op == PY_CMP_EQ ? eq_result : !eq_result);
                }
                else
                {
                    fprintf(stderr, "TypeError: __eq__ returned non-boolean (type %s)\n", py_type_name(resultObj->typeId));
                    py_decref(resultObj);
                    return NULL;  // Indicate error
                }
            }
            else
            {
                // equals call failed internally
                fprintf(stderr, "Error: equals method call failed for type %s\n", py_type_name(typeIdA));
                return NULL;  // Indicate error
            }
        }
        // Fallback to identity comparison if no equals method
        // fprintf(stderr, "Debug: Type %s has no specific equals method, using identity comparison.\n", py_type_name(typeIdA));
        bool identity_result = (a == b);
        return py_create_bool(op == PY_CMP_EQ ? identity_result : !identity_result);
    }

    // 3. Handle other comparison ops (<, <=, >, >=) using GMP
    int aTypeId = getBaseTypeId(a->typeId);
    int bTypeId = getBaseTypeId(b->typeId);

    // --- GMP Numeric Comparison ---
    bool aIsNumeric = (aTypeId == PY_TYPE_INT || aTypeId == PY_TYPE_DOUBLE || aTypeId == PY_TYPE_BOOL);
    bool bIsNumeric = (bTypeId == PY_TYPE_INT || bTypeId == PY_TYPE_DOUBLE || bTypeId == PY_TYPE_BOOL);

    if (aIsNumeric && bIsNumeric)
    {
        mpz_ptr a_int = py_extract_int(a);
        mpf_ptr a_float = py_extract_double(a);
        bool a_bool_val;
        bool a_is_bool = (aTypeId == PY_TYPE_BOOL);
        if (a_is_bool) a_bool_val = py_extract_bool(a);
        mpz_ptr b_int = py_extract_int(b);
        mpf_ptr b_float = py_extract_double(b);
        bool b_bool_val;
        bool b_is_bool = (bTypeId == PY_TYPE_BOOL);
        if (b_is_bool) b_bool_val = py_extract_bool(b);

        int cmp_result;  // Stores result of GMP comparison functions (<0, 0, >0)

        // Determine comparison strategy
        if (a_float || b_float)
        {  // At least one float
            mpf_t temp_a, temp_b;
            mpf_init2(temp_a, 256);
            mpf_init2(temp_b, 256);
            bool use_temp_a = false, use_temp_b = false;
            mpf_srcptr op_a, op_b;

            if (a_float)
                op_a = a_float;
            else if (a_int)
            {
                mpf_set_z(temp_a, a_int);
                op_a = temp_a;
                use_temp_a = true;
            }
            else if (a_is_bool)
            {
                mpf_set_ui(temp_a, a_bool_val ? 1 : 0);
                op_a = temp_a;
                use_temp_a = true;
            }
            else
            { /* Error */
                mpf_clear(temp_a);
                mpf_clear(temp_b);
                return NULL;
            }

            if (b_float)
                op_b = b_float;
            else if (b_int)
            {
                mpf_set_z(temp_b, b_int);
                op_b = temp_b;
                use_temp_b = true;
            }
            else if (b_is_bool)
            {
                mpf_set_ui(temp_b, b_bool_val ? 1 : 0);
                op_b = temp_b;
                use_temp_b = true;
            }
            else
            { /* Error */
                mpf_clear(temp_a);
                mpf_clear(temp_b);
                return NULL;
            }

            cmp_result = mpf_cmp(op_a, op_b);
            if (use_temp_a) mpf_clear(temp_a);
            if (use_temp_b) mpf_clear(temp_b);
        }
        else
        {  // Both int/bool
            mpz_t temp_a_z, temp_b_z;
            mpz_init(temp_a_z);
            mpz_init(temp_b_z);
            bool use_temp_a_z = false, use_temp_b_z = false;
            mpz_srcptr op_a_z, op_b_z;

            if (a_int)
                op_a_z = a_int;
            else if (a_is_bool)
            {
                mpz_set_ui(temp_a_z, a_bool_val ? 1 : 0);
                op_a_z = temp_a_z;
                use_temp_a_z = true;
            }
            else
            { /* Error */
                mpz_clear(temp_a_z);
                mpz_clear(temp_b_z);
                return NULL;
            }

            if (b_int)
                op_b_z = b_int;
            else if (b_is_bool)
            {
                mpz_set_ui(temp_b_z, b_bool_val ? 1 : 0);
                op_b_z = temp_b_z;
                use_temp_b_z = true;
            }
            else
            { /* Error */
                mpz_clear(temp_a_z);
                mpz_clear(temp_b_z);
                return NULL;
            }

            cmp_result = mpz_cmp(op_a_z, op_b_z);
            if (use_temp_a_z) mpz_clear(temp_a_z);
            if (use_temp_b_z) mpz_clear(temp_b_z);
        }

        // Convert cmp_result to boolean based on operator
        bool result = false;
        switch (op)
        {
            case PY_CMP_LT:
                result = (cmp_result < 0);
                break;
            case PY_CMP_LE:
                result = (cmp_result <= 0);
                break;
            // EQ/NE handled above
            case PY_CMP_GT:
                result = (cmp_result > 0);
                break;
            case PY_CMP_GE:
                result = (cmp_result >= 0);
                break;
            default:  // Should not happen
                fprintf(stderr, "Internal Error: Unhandled comparison operator %d in numeric compare\n", op);
                return NULL;
        }
        return py_create_bool(result);
    }

    // --- String Comparison ---
    if (aTypeId == PY_TYPE_STRING && bTypeId == PY_TYPE_STRING)
    {
        const char* strA = py_extract_string(a);
        const char* strB = py_extract_string(b);
        if (!strA) strA = "";
        if (!strB) strB = "";

        int cmpResult = strcmp(strA, strB);
        bool result = false;
        switch (op)
        {
            case PY_CMP_LT:
                result = (cmpResult < 0);
                break;
            case PY_CMP_LE:
                result = (cmpResult <= 0);
                break;
            // EQ/NE handled above
            case PY_CMP_GT:
                result = (cmpResult > 0);
                break;
            case PY_CMP_GE:
                result = (cmpResult >= 0);
                break;
            default:  // Should not happen
                fprintf(stderr, "Internal Error: Unhandled comparison operator %d for strings\n", op);
                return NULL;
        }
        return py_create_bool(result);
    }

    // 4. Incompatible types for ordering comparison -> TypeError
    fprintf(stderr, "TypeError: '%s' not supported between instances of '%s' and '%s'\n",
            py_compare_op_name(op),
            py_type_name(aTypeId), py_type_name(bTypeId));
    return NULL;  // Indicate TypeError
}

// 获取compare_op名字的赋值函数
const char* py_compare_op_name(PyCompareOp op)
{
    switch (op)
    {
        case PY_CMP_LT:
            return "<";
        case PY_CMP_LE:
            return "<=";
        case PY_CMP_EQ:
            return "==";
        case PY_CMP_NE:
            return "!=";
        case PY_CMP_GT:
            return ">";
        case PY_CMP_GE:
            return ">=";
        default:
            return "op";
    }
}

// 相等性比较辅助函数 (可以保留，但现在主要逻辑在 py_object_compare 中)
bool py_compare_eq(PyObject* a, PyObject* b)
{
    PyObject* result = py_object_compare(a, b, PY_CMP_EQ);
    if (!result)
    {
        fprintf(stderr, "Error: py_object_compare(EQ) failed\n");
        return false;
    }  // Error occurred in compare
    bool eq = py_extract_bool(result);
    py_decref(result);
    return eq;
}

bool py_compare_ne(PyObject* a, PyObject* b) {
    PyObject* result = py_object_compare(a, b, PY_CMP_NE);
    if (!result){
        fprintf(stderr, "Error: py_object_compare(NE) failed\n");
         return true;
     } // Error occurred, maybe treat as unequal? Or propagate error?
    bool ne = py_extract_bool(result);
    py_decref(result);
    return ne;
}

// 小于比较辅助函数
bool py_compare_lt(PyObject* a, PyObject* b) {
    PyObject* result = py_object_compare(a, b, PY_CMP_LT);
    if (!result) {
        fprintf(stderr, "Error: py_object_compare(LT) failed\n");
        return false;} // Error (TypeError) occurred
    bool lt = py_extract_bool(result);
    py_decref(result);
    return lt;
}

// 小于等于比较辅助函数
bool py_compare_le(PyObject* a, PyObject* b) {
    PyObject* result = py_object_compare(a, b, PY_CMP_LE);
    if (!result){
        fprintf(stderr, "Error: py_object_compare(LE) failed\n");
        return false; }// Error (TypeError) occurred
    bool le = py_extract_bool(result);
    py_decref(result);
    return le;
}

// 大于比较辅助函数
bool py_compare_gt(PyObject* a, PyObject* b) {
    PyObject* result = py_object_compare(a, b, PY_CMP_GT);
    if (!result) {
        fprintf(stderr, "Error: py_object_compare(GT) failed\n");
        return false;} // Error (TypeError) occurred
    bool gt = py_extract_bool(result);
    py_decref(result);
    return gt;
}

// 大于等于比较辅助函数
bool py_compare_ge(PyObject* a, PyObject* b) {
    PyObject* result = py_object_compare(a, b, PY_CMP_GE);
    if (!result) {
        fprintf(stderr, "Error: py_object_compare(GE) failed\n");
        return false;} // Error (TypeError) occurred
    bool ge = py_extract_bool(result);
    py_decref(result);
    return ge;
}

//===----------------------------------------------------------------------===//
// 位操作符实现
//===----------------------------------------------------------------------===//

// 按位与操作符
PyObject* py_object_bitwise_and(PyObject* a, PyObject* b)
{
    if (!a || !b) {
        fprintf(stderr, "TypeError: Cannot perform bitwise AND with None\n");
        return NULL;
    }
    if (py_get_safe_type_id(a) != PY_TYPE_INT || py_get_safe_type_id(b) != PY_TYPE_INT) {
        fprintf(stderr, "TypeError: unsupported operand type(s) for &: '%s' and '%s'\n",
                py_type_name(py_get_safe_type_id(a)), py_type_name(py_get_safe_type_id(b)));
        return NULL;
    }

    mpz_ptr a_int = py_extract_int(a);
    mpz_ptr b_int = py_extract_int(b);
    if (!a_int || !b_int) { /* Error extracting */ return NULL; }

    mpz_t result_z;
    mpz_init(result_z);
    mpz_and(result_z, a_int, b_int);
    PyObject* resultObj = py_create_int_from_mpz(result_z);
    mpz_clear(result_z);
    return resultObj;
}

// 按位或操作符
PyObject* py_object_bitwise_or(PyObject* a, PyObject* b)
{
    if (!a || !b) {
        fprintf(stderr, "TypeError: Cannot perform bitwise OR with None\n");
        return NULL;
    }
    if (py_get_safe_type_id(a) != PY_TYPE_INT || py_get_safe_type_id(b) != PY_TYPE_INT) {
        fprintf(stderr, "TypeError: unsupported operand type(s) for |: '%s' and '%s'\n",
                py_type_name(py_get_safe_type_id(a)), py_type_name(py_get_safe_type_id(b)));
        return NULL;
    }

    mpz_ptr a_int = py_extract_int(a);
    mpz_ptr b_int = py_extract_int(b);
    if (!a_int || !b_int) { /* Error extracting */ return NULL; }

    mpz_t result_z;
    mpz_init(result_z);
    mpz_ior(result_z, a_int, b_int); // ior = inclusive or
    PyObject* resultObj = py_create_int_from_mpz(result_z);
    mpz_clear(result_z);
    return resultObj;
}

// 按位异或操作符
PyObject* py_object_bitwise_xor(PyObject* a, PyObject* b)
{
     if (!a || !b) {
        fprintf(stderr, "TypeError: Cannot perform bitwise XOR with None\n");
        return NULL;
    }
    if (py_get_safe_type_id(a) != PY_TYPE_INT || py_get_safe_type_id(b) != PY_TYPE_INT) {
        fprintf(stderr, "TypeError: unsupported operand type(s) for ^: '%s' and '%s'\n",
                py_type_name(py_get_safe_type_id(a)), py_type_name(py_get_safe_type_id(b)));
        return NULL;
    }

    mpz_ptr a_int = py_extract_int(a);
    mpz_ptr b_int = py_extract_int(b);
    if (!a_int || !b_int) { /* Error extracting */ return NULL; }

    mpz_t result_z;
    mpz_init(result_z);
    mpz_xor(result_z, a_int, b_int);
    PyObject* resultObj = py_create_int_from_mpz(result_z);
    mpz_clear(result_z);
    return resultObj;
}

// 按位取反操作符
PyObject* py_object_bitwise_not(PyObject* a)
{
    if (!a) {
        fprintf(stderr, "TypeError: bad operand type for unary ~: 'NoneType'\n");
        return NULL;
    }
    if (py_get_safe_type_id(a) != PY_TYPE_INT) {
         fprintf(stderr, "TypeError: bad operand type for unary ~: '%s'\n",
                py_type_name(py_get_safe_type_id(a)));
        return NULL;
    }

    mpz_ptr a_int = py_extract_int(a);
    if (!a_int) { /* Error extracting */ return NULL; }

    mpz_t result_z;
    mpz_init(result_z);
    // Bitwise NOT (~) in Python is -(x+1)
    mpz_add_ui(result_z, a_int, 1);
    mpz_neg(result_z, result_z);
    // mpz_com(result_z, a_int); // mpz_com is one's complement, Python's ~ is two's complement based

    PyObject* resultObj = py_create_int_from_mpz(result_z);
    mpz_clear(result_z);
    return resultObj;
}


// 左移操作符
PyObject* py_object_left_shift(PyObject* a, PyObject* b)
{
    if (!a || !b) {
        fprintf(stderr, "TypeError: Cannot perform left shift with None\n");
        return NULL;
    }
    if (py_get_safe_type_id(a) != PY_TYPE_INT || py_get_safe_type_id(b) != PY_TYPE_INT) {
        fprintf(stderr, "TypeError: unsupported operand type(s) for <<: '%s' and '%s'\n",
                py_type_name(py_get_safe_type_id(a)), py_type_name(py_get_safe_type_id(b)));
        return NULL;
    }

    mpz_ptr a_int = py_extract_int(a);
    mpz_ptr b_int = py_extract_int(b); // Shift amount
    if (!a_int || !b_int) { /* Error extracting */ return NULL; }

    // Check if shift amount is negative
    if (mpz_sgn(b_int) < 0) {
        fprintf(stderr, "ValueError: negative shift count\n");
        return NULL;
    }

    // Check if shift amount fits in unsigned long
    if (!mpz_fits_ulong_p(b_int)) {
        // Python raises OverflowError here if it's huge, but practically it might lead to MemoryError.
        // We'll raise OverflowError for consistency.
        fprintf(stderr, "OverflowError: shift count too large\n");
        return NULL;
    }
    unsigned long shift_count = mpz_get_ui(b_int);

    mpz_t result_z;
    mpz_init(result_z);
    // Left shift is multiplication by 2^shift_count
    mpz_mul_2exp(result_z, a_int, shift_count);

    PyObject* resultObj = py_create_int_from_mpz(result_z);
    mpz_clear(result_z);
    return resultObj;
}

// 右移操作符
PyObject* py_object_right_shift(PyObject* a, PyObject* b)
{
    if (!a || !b) {
        fprintf(stderr, "TypeError: Cannot perform right shift with None\n");
        return NULL;
    }
     if (py_get_safe_type_id(a) != PY_TYPE_INT || py_get_safe_type_id(b) != PY_TYPE_INT) {
        fprintf(stderr, "TypeError: unsupported operand type(s) for >>: '%s' and '%s'\n",
                py_type_name(py_get_safe_type_id(a)), py_type_name(py_get_safe_type_id(b)));
        return NULL;
    }

    mpz_ptr a_int = py_extract_int(a);
    mpz_ptr b_int = py_extract_int(b); // Shift amount
    if (!a_int || !b_int) { /* Error extracting */ return NULL; }

    // Check if shift amount is negative
    if (mpz_sgn(b_int) < 0) {
        fprintf(stderr, "ValueError: negative shift count\n");
        return NULL;
    }

    // Check if shift amount fits in unsigned long
    if (!mpz_fits_ulong_p(b_int)) {
        // If shift count is huge, result is likely 0 or -1.
        // Python doesn't raise OverflowError here.
        // We can handle large shifts by checking the sign.
        if (mpz_sgn(a_int) < 0) {
             // Shifting negative number by huge amount -> -1
             return py_create_int(-1);
        } else {
             // Shifting non-negative number by huge amount -> 0
             return py_create_int(0);
        }
        // fprintf(stderr, "OverflowError: shift count too large\n"); // Or just calculate?
        // return NULL;
    }
    unsigned long shift_count = mpz_get_ui(b_int);

    mpz_t result_z;
    mpz_init(result_z);
    // Right shift is floor division by 2^shift_count
    mpz_fdiv_q_2exp(result_z, a_int, shift_count);

    PyObject* resultObj = py_create_int_from_mpz(result_z);
    mpz_clear(result_z);
    return resultObj;
}