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
            mpf_init2(result_f, RUNTIME_FLOATE_PRECISION);  // Default precision, adjust if needed

            mpf_t temp_a, temp_b;  // Temporaries for potential conversions
            mpf_init2(temp_a, RUNTIME_FLOATE_PRECISION);
            mpf_init2(temp_b, RUNTIME_FLOATE_PRECISION);
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
            mpf_init2(result_f, RUNTIME_FLOATE_PRECISION);
            mpf_t temp_a, temp_b;
            mpf_init2(temp_a, RUNTIME_FLOATE_PRECISION);
            mpf_init2(temp_b, RUNTIME_FLOATE_PRECISION);
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


// 乘法操作符
PyObject* py_object_multiply(PyObject* a, PyObject* b)
{
#ifdef DEBUG_RUNTIME_OPERATORS
    fprintf(stderr, "DEBUG: py_object_multiply called with a=%p, b=%p\n", (void*)a, (void*)b);
    if (a) fprintf(stderr, "DEBUG:   a typeId: %d (%s)\n", a->typeId, py_type_name(a->typeId));
    if (b) fprintf(stderr, "DEBUG:   b typeId: %d (%s)\n", b->typeId, py_type_name(b->typeId));
#endif

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
#ifdef DEBUG_RUNTIME_OPERATORS
        fprintf(stderr, "DEBUG: py_object_multiply: Performing numeric multiplication.\n");
#endif
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
#ifdef DEBUG_RUNTIME_OPERATORS
            fprintf(stderr, "DEBUG:   Result type is float.\n");
#endif
            mpf_t result_f;
            mpf_init2(result_f, RUNTIME_FLOATE_PRECISION);
            mpf_t temp_a, temp_b;
            mpf_init2(temp_a, RUNTIME_FLOATE_PRECISION);
            mpf_init2(temp_b, RUNTIME_FLOATE_PRECISION);
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
                fprintf(stderr, "Internal Error: Unexpected type A in numeric multiply\n");
                mpf_clear(result_f); mpf_clear(temp_a); mpf_clear(temp_b); return NULL;
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
                 fprintf(stderr, "Internal Error: Unexpected type B in numeric multiply\n");
                 mpf_clear(result_f); mpf_clear(temp_a); mpf_clear(temp_b); return NULL;
            }

            mpf_mul(result_f, op_a, op_b);
            PyObject* resultObj = py_create_double_from_mpf(result_f);
#ifdef DEBUG_RUNTIME_OPERATORS
            fprintf(stderr, "DEBUG:   Float multiplication result: %p\n", (void*)resultObj);
#endif
            mpf_clear(result_f);
            if (use_temp_a) mpf_clear(temp_a);
            if (use_temp_b) mpf_clear(temp_b);
            return resultObj;
        }
        else
        {  // int/bool * int/bool
#ifdef DEBUG_RUNTIME_OPERATORS
            fprintf(stderr, "DEBUG:   Result type is int.\n");
#endif
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
                 fprintf(stderr, "Internal Error: Unexpected type A in int multiply\n");
                 mpz_clear(result_z); mpz_clear(temp_a_z); mpz_clear(temp_b_z); return NULL;
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
                 fprintf(stderr, "Internal Error: Unexpected type B in int multiply\n");
                 mpz_clear(result_z); mpz_clear(temp_a_z); mpz_clear(temp_b_z); return NULL;
            }

            mpz_mul(result_z, op_a_z, op_b_z);
            PyObject* resultObj = py_create_int_from_mpz(result_z);
#ifdef DEBUG_RUNTIME_OPERATORS
            fprintf(stderr, "DEBUG:   Int multiplication result: %p\n", (void*)resultObj);
#endif
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

    if ((aTypeId == PY_TYPE_STRING || aTypeId == PY_TYPE_LIST) && (bTypeId == PY_TYPE_INT || bTypeId == PY_TYPE_BOOL)) // Allow bool as count
    {
        seq = a;
        countObj = b;
        seqTypeId = aTypeId;
    }
    else if ((aTypeId == PY_TYPE_INT || aTypeId == PY_TYPE_BOOL) && (bTypeId == PY_TYPE_STRING || bTypeId == PY_TYPE_LIST)) // Allow bool as count
    {
        seq = b;
        countObj = a;
        seqTypeId = bTypeId;
    }

    if (seq && countObj)
    {
#ifdef DEBUG_RUNTIME_OPERATORS
        fprintf(stderr, "DEBUG: py_object_multiply: Performing sequence repetition (type %s * %s).\n",
                py_type_name(seqTypeId), py_type_name(py_get_safe_type_id(countObj)));
        fprintf(stderr, "DEBUG:   Sequence object: %p, Count object: %p\n", (void*)seq, (void*)countObj);
#endif
        mpz_ptr count_z = py_extract_int(countObj);
        long count_val;

        if (count_z) { // If countObj is int
             if (!mpz_fits_slong_p(count_z) || (count_val = mpz_get_si(count_z)) < 0)
             {
                 if (mpz_sgn(count_z) < 0)
                 {
                     count_val = 0;
#ifdef DEBUG_RUNTIME_OPERATORS
                     fprintf(stderr, "DEBUG:   Negative int count treated as 0.\n");
#endif
                 }
                 else
                 {
                     fprintf(stderr, "OverflowError: cannot fit 'int' count into C long for sequence repetition\n");
                     return NULL;
                 }
             }
        } else if (py_get_safe_type_id(countObj) == PY_TYPE_BOOL) { // If countObj is bool
            count_val = py_extract_bool(countObj) ? 1 : 0;
#ifdef DEBUG_RUNTIME_OPERATORS
            fprintf(stderr, "DEBUG:   Bool count extracted as %ld.\n", count_val);
#endif
        } else { // Not int or bool
            fprintf(stderr, "TypeError: can't multiply sequence by non-int, non-bool of type '%s'\n", py_type_name(py_get_safe_type_id(countObj)));
            return NULL;
        }

#ifdef DEBUG_RUNTIME_OPERATORS
        fprintf(stderr, "DEBUG:   Extracted count_val: %ld\n", count_val);
#endif

        if (count_val == 0)
        {
#ifdef DEBUG_RUNTIME_OPERATORS
            fprintf(stderr, "DEBUG:   Count is 0, returning empty sequence.\n");
#endif
            if (seqTypeId == PY_TYPE_STRING) return py_create_string("");
            if (seqTypeId == PY_TYPE_LIST) return py_create_list(0, ((PyListObject*)seq)->elemTypeId);
        }

        if (seqTypeId == PY_TYPE_STRING)
        {
            const char* str = py_extract_string(seq);
            if (!str) str = "";
            size_t len = strlen(str);
#ifdef DEBUG_RUNTIME_OPERATORS
            fprintf(stderr, "DEBUG:   Original string: '%s', length: %zu\n", str, len);
#endif
            // Check for potential overflow before malloc
            if (len > 0 && count_val > SIZE_MAX / len)
            {
                fprintf(stderr, "MemoryError: String repetition result too large\n");
                return NULL;
            }
            size_t total_len = len * count_val;
#ifdef DEBUG_RUNTIME_OPERATORS
            fprintf(stderr, "DEBUG:   Calculated total_len: %zu\n", total_len);
#endif
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
#ifdef DEBUG_RUNTIME_OPERATORS
            fprintf(stderr, "DEBUG:   Created repeated string: '%s', object: %p\n", result, (void*)resultObj);
#endif
            free(result);
            return resultObj;
        }
        else if (seqTypeId == PY_TYPE_LIST)
        {
            PyListObject* list = (PyListObject*)seq;
#ifdef DEBUG_RUNTIME_OPERATORS
            fprintf(stderr, "DEBUG:   Original list: %p, length: %d, capacity: %d, elemTypeId: %d\n",
                    (void*)list, list->length, list->capacity, list->elemTypeId);
#endif
            // Check for potential overflow before creating list
            if (list->length > 0 && count_val > INT_MAX / list->length)
            {
                fprintf(stderr, "MemoryError: List repetition result too large\n");
                return NULL;
            }
            int newLength = list->length * count_val;
#ifdef DEBUG_RUNTIME_OPERATORS
            fprintf(stderr, "DEBUG:   Calculated newLength: %d\n", newLength);
#endif
            // Create list with capacity hint = newLength
            PyObject* resultListObj = py_create_list(newLength, list->elemTypeId);
            if (!resultListObj) return NULL;
            PyListObject* resultList = (PyListObject*)resultListObj;
#ifdef DEBUG_RUNTIME_OPERATORS
            fprintf(stderr, "DEBUG:   py_create_list returned: %p, initial length: %d, capacity: %d\n",
                    (void*)resultList, resultList->length, resultList->capacity);
#endif
            // Check if capacity is sufficient (py_create_list should guarantee this)
            if (resultList->capacity < newLength)
            {
                fprintf(stderr, "Internal Error: Insufficient capacity (%d < %d) after py_create_list in list repetition.\n",
                        resultList->capacity, newLength);
                py_decref(resultListObj);
                return NULL;
            }

            PyObject** dest_ptr = resultList->data;
            for (long c = 0; c < count_val; ++c)
            {
                for (int i = 0; i < list->length; ++i)
                {
                    PyObject* item = list->data[i];
#ifdef DEBUG_RUNTIME_OPERATORS
                    // Be cautious with printing item details if it could be complex
                    // fprintf(stderr, "DEBUG:     Copying item %p from original index %d to new index %ld\n",
                    //         (void*)item, i, (long)(dest_ptr - resultList->data));
#endif
                    if (item) py_incref(item); // Incref item from original list
                    *dest_ptr++ = item;        // Copy pointer to new list
                }
            }
#ifdef DEBUG_RUNTIME_OPERATORS
            long items_copied = dest_ptr - resultList->data;
            fprintf(stderr, "DEBUG:   Finished copying elements. items_copied = %ld (expected %d)\n", items_copied, newLength);
#endif
            // Set the final length of the new list
            resultList->length = newLength;
#ifdef DEBUG_RUNTIME_OPERATORS
            fprintf(stderr, "DEBUG:   Set resultList->length = %d\n", resultList->length);
            fprintf(stderr, "DEBUG:   Returning new list object: %p\n", (void*)resultListObj);
#endif
            return resultListObj;
        }
    }

    // --- Unsupported Types ---
    fprintf(stderr, "TypeError: Unsupported operand types for *: %s and %s\n",
            py_type_name(aTypeId), py_type_name(bTypeId));
    return NULL;
}

// ... (rest of the file: py_object_divide, py_object_modulo, etc.) ...

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
        mpf_init2(result_f, RUNTIME_FLOATE_PRECISION);
        mpf_t temp_a, temp_b;
        mpf_init2(temp_a, RUNTIME_FLOATE_PRECISION);
        mpf_init2(temp_b, RUNTIME_FLOATE_PRECISION);
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
            mpf_t result_f; mpf_init2(result_f, RUNTIME_FLOATE_PRECISION);
            mpf_t temp_a, temp_b; mpf_init2(temp_a, RUNTIME_FLOATE_PRECISION); mpf_init2(temp_b, RUNTIME_FLOATE_PRECISION);
            mpf_t div_res, floor_div_res, term_to_sub; // Intermediate results
            mpf_init2(div_res, RUNTIME_FLOATE_PRECISION);
            mpf_init2(floor_div_res, RUNTIME_FLOATE_PRECISION);
            mpf_init2(term_to_sub, RUNTIME_FLOATE_PRECISION);

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
            mpf_init2(result_f, RUNTIME_FLOATE_PRECISION);
            mpf_t temp_a, temp_b;
            mpf_init2(temp_a, RUNTIME_FLOATE_PRECISION);
            mpf_init2(temp_b, RUNTIME_FLOATE_PRECISION);
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




static void mpf_set_from_mpfr(mpf_t rop, const mpfr_t op) {
    // 直接调用 MPFR 库函数进行转换，避免通过字符串中转
    mpfr_get_f(rop, op, MPFR_RNDN);
}

// 辅助函数：检查mpf_t是否表示整数
/* static bool mpf_integer_p(const mpf_t op) {
    // 检查浮点数是否为整数（没有小数部分）
    mpf_t int_part, frac_part;
    mpf_init(int_part);
    mpf_init(frac_part);
    
    // 分离整数和小数部分
    mpf_floor(int_part, op);
    mpf_sub(frac_part, op, int_part);
    
    // 检查小数部分是否为0
    bool is_integer = (mpf_sgn(frac_part) == 0);
    
    mpf_clear(int_part);
    mpf_clear(frac_part);
    return is_integer;
} */

// 辅助函数：安全地将mpf_t转换为long整数，如果超出范围返回非零错误码
static int mpf_get_si_checked(long *result, const mpf_t op) {
    // 检查是否在long范围内
    if (mpf_cmp_si(op, LONG_MAX) > 0 || mpf_cmp_si(op, LONG_MIN) < 0) {
        return -1;  // 超出范围
    }
    
    // 转换为长整型
    *result = mpf_get_si(op);
    return 0;  // 成功
}

// 幂运算操作符
// 优化后的幂运算操作符
PyObject* py_object_power(PyObject* a, PyObject* b) {
    if (!a || !b) {
        fprintf(stderr, "TypeError: Cannot perform power operation with None\n");
        return NULL;
    }

    int aTypeId = getBaseTypeId(a->typeId);
    int bTypeId = getBaseTypeId(b->typeId);
    bool aIsNumeric = (aTypeId==PY_TYPE_INT||aTypeId==PY_TYPE_DOUBLE||aTypeId==PY_TYPE_BOOL);
    bool bIsNumeric = (bTypeId==PY_TYPE_INT||bTypeId==PY_TYPE_DOUBLE||bTypeId==PY_TYPE_BOOL);
    if (!aIsNumeric || !bIsNumeric) {
        fprintf(stderr,
            "TypeError: Unsupported operand types for **: %s and %s\n",
            py_type_name(aTypeId), py_type_name(bTypeId));
        return NULL;
    }

    // 提取原始值
    mpz_ptr a_int   = py_extract_int(a);
    mpf_ptr a_float = py_extract_double(a);
    bool   a_is_bool= (aTypeId==PY_TYPE_BOOL);
    bool   a_bool_v = a_is_bool?py_extract_bool(a):false;
    mpz_ptr b_int   = py_extract_int(b);
    mpf_ptr b_float = py_extract_double(b);
    bool   b_is_bool= (bTypeId==PY_TYPE_BOOL);
    bool   b_bool_v = b_is_bool?py_extract_bool(b):false;

    // 快速路径1: 特殊情况处理
    // x^0 = 1 (x != 0)
    if ((b_int && mpz_sgn(b_int) == 0) || (b_is_bool && !b_bool_v)) {
        if ((a_int && mpz_sgn(a_int) == 0) || (a_is_bool && !a_bool_v)) {
            // Python中 0^0 = 1
            return py_create_int(1);
        }
        return py_create_int(1);
    }
    
    // x^1 = x
    if ((b_int && mpz_cmp_ui(b_int, 1) == 0) || (b_is_bool && b_bool_v)) {
        // 直接返回底数
        if (a_int) {
            py_incref(a);  // 因为不创建新对象，需要增加引用计数
            return a;
        } else if (a_float) {
            py_incref(a);
            return a;
        } else if (a_is_bool) {
            py_incref(a);
            return a;
        }
    }
    
    // 1^x = 1
    if ((a_int && mpz_cmp_ui(a_int, 1) == 0) || (a_is_bool && a_bool_v && b_int && mpz_sgn(b_int) >= 0)) {
        return py_create_int(1);
    }
    
    // 0^x = 0 (x > 0)
    if ((a_int && mpz_sgn(a_int) == 0) || (a_is_bool && !a_bool_v)) {
        if ((b_int && mpz_sgn(b_int) > 0) || (b_is_bool && b_bool_v)) {
            return py_create_int(0);
        } else if ((b_int && mpz_sgn(b_int) < 0) || (b_float && mpf_sgn(b_float) < 0)) {
            // 0^负数 -> 错误
            fprintf(stderr, "ZeroDivisionError: 0 cannot be raised to a negative power\n");
            return NULL;
        }
    }

    // 快速路径2: 整型底数 + 小正整数指数 (针对常见情况优化)
    if ((a_int || a_is_bool) && (b_int || b_is_bool)) {
        mpz_t base_z, exp_z, result_z;
        mpz_init(base_z); 
        
        if (a_int) mpz_set(base_z, a_int);
        else mpz_set_ui(base_z, a_bool_v?1:0);
        
        // 只有当指数是正整数且可存入unsigned long时才使用快速路径
        if (b_int) {
            if (mpz_sgn(b_int) >= 0 && mpz_fits_ulong_p(b_int)) {
                unsigned long e = mpz_get_ui(b_int);
                mpz_init(result_z);
                mpz_pow_ui(result_z, base_z, e);
                PyObject* ret = py_create_int_from_mpz(result_z);
                mpz_clear(result_z);
                mpz_clear(base_z);
                return ret;
            } else if (mpz_sgn(b_int) < 0 && mpz_sgn(base_z) == 0) {
                // 0^负数 -> 错误
                mpz_clear(base_z);
                fprintf(stderr, "ZeroDivisionError: 0 cannot be raised to a negative power\n");
                return NULL;
            }
        } else if (b_is_bool && b_bool_v) { // b是True，即b=1
            PyObject* ret = py_create_int_from_mpz(base_z);
            mpz_clear(base_z);
            return ret;
        } else if (b_is_bool && !b_bool_v) { // b是False，即b=0
            mpz_clear(base_z);
            return py_create_int(1); // x^0 = 1
        }
        
        mpz_clear(base_z);
    }

    // 快速路径3: 整数/布尔底数 + 浮点指数，且指数为整数值
    if ((a_int || a_is_bool) && b_float) {
        mpf_t result_f;
        long exp_val;
        
        // 检查浮点指数是否为整数，并且可以表示为long
        if (mpf_integer_p(b_float) && mpf_get_si_checked(&exp_val, b_float) == 0) {
            if (exp_val >= 0) {
                // 使用整数幂算法
                mpz_t base_z, result_z;
                mpz_init(base_z);
                mpz_init(result_z);
                
                if (a_int) mpz_set(base_z, a_int);
                else mpz_set_ui(base_z, a_bool_v?1:0);
                
                // 如果底数为0且指数为负，则报错
                if (mpz_sgn(base_z) == 0 && exp_val < 0) {
                    mpz_clears(base_z, result_z, NULL);
                    fprintf(stderr, "ZeroDivisionError: 0 cannot be raised to a negative power\n");
                    return NULL;
                }
                
                mpz_pow_ui(result_z, base_z, (unsigned long)exp_val);
                PyObject* ret = py_create_int_from_mpz(result_z);
                mpz_clears(base_z, result_z, NULL);
                return ret;
            } else if (exp_val < 0) {
                // 负指数，结果是浮点数
                mpf_init2(result_f, RUNTIME_FLOATE_PRECISION);
                
                // 计算整数底数^正整数指数，然后取倒数
                mpz_t base_z;
                mpz_init(base_z);
                if (a_int) mpz_set(base_z, a_int);
                else mpz_set_ui(base_z, a_bool_v?1:0);
                
                // 如果底数为0且指数为负，则报错
                if (mpz_sgn(base_z) == 0) {
                    mpz_clear(base_z);
                    mpf_clear(result_f);
                    fprintf(stderr, "ZeroDivisionError: 0 cannot be raised to a negative power\n");
                    return NULL;
                }
                
                // 计算base^(-exp_val)的倒数
                if (mpz_fits_sint_p(base_z)) {
                    // 对于小整数底数，可以直接用浮点运算
                    long base_val = mpz_get_si(base_z);
                    mpf_set_d(result_f, pow(base_val, exp_val));
                } else {
                    // 对于大整数底数，先计算base^|exp_val|，然后取倒数
                    mpz_t temp;
                    mpz_init(temp);
                    mpz_pow_ui(temp, base_z, (unsigned long)(-exp_val));
                    
                    // 将大整数结果转为浮点
                    mpf_set_z(result_f, temp);
                    mpf_ui_div(result_f, 1, result_f);  // 1/result
                    mpz_clear(temp);
                }
                
                mpz_clear(base_z);
                PyObject* ret = py_create_double_from_mpf(result_f);
                mpf_clear(result_f);
                return ret;
            }
        }
    }

    // Case 2: 浮点或大指数 → MPFR 计算
    mpfr_t base_f, exp_f, result_f;
    mpfr_init2(base_f, RUNTIME_FLOATE_PRECISION);
    mpfr_init2(exp_f, RUNTIME_FLOATE_PRECISION);
    mpfr_init2(result_f, RUNTIME_FLOATE_PRECISION);

    // 无损导入 GMP → MPFR
    if (a_int)        mpfr_set_z(base_f, a_int, MPFR_RNDN);
    else if (a_float) mpfr_set_f(base_f, a_float, MPFR_RNDN);
    else              mpfr_set_ui(base_f, a_bool_v?1:0, MPFR_RNDN);

    if (b_int)        mpfr_set_z(exp_f, b_int, MPFR_RNDN);
    else if (b_float) mpfr_set_f(exp_f, b_float, MPFR_RNDN);
    else              mpfr_set_ui(exp_f, b_bool_v?1:0, MPFR_RNDN);

    // 0^负数 -> 错误
    if (mpfr_zero_p(base_f) && mpfr_sgn(exp_f)<0) {
        mpfr_clears(base_f, exp_f, result_f, NULL);
        fprintf(stderr,"ZeroDivisionError: 0.0 cannot be raised to a negative power\n");
        return NULL;
    }

    // 优化: 检查指数是否为整数，使用更高效的mpfr_pow_si
    if (mpfr_integer_p(exp_f) && mpfr_fits_slong_p(exp_f, MPFR_RNDN)) {
        long exp_val = mpfr_get_si(exp_f, MPFR_RNDN);
        mpfr_pow_si(result_f, base_f, exp_val, MPFR_RNDN);
    } else {
        // 通用幂运算
        mpfr_pow(result_f, base_f, exp_f, MPFR_RNDN);
    }

    // 负底数+非整数指数 → NaN
    if (mpfr_nan_p(result_f)) {
        mpfr_clears(base_f, exp_f, result_f, NULL);
        fprintf(stderr,"ValueError: negative number cannot be raised to a fractional power\n");
        return NULL;
    }

    // 将 mpfr_t 转回 mpf_t，再 py_create
    mpf_t gmp_res;
    mpf_init2(gmp_res, mpfr_get_prec(result_f));
    mpf_set_from_mpfr(gmp_res, result_f);
    PyObject* ret = py_create_double_from_mpf(gmp_res);

    // 清理
    mpf_clear(gmp_res);
    mpfr_clears(base_f, exp_f, result_f, NULL);
    return ret;
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
    // Step 0: Handle C NULL pointers.
    // These are distinct from Python's None object.
    // This check is for robustness against actual NULL pointers in the system.
    if (a == NULL || b == NULL) {
        bool result = false;
        if (op == PY_CMP_EQ) result = (a == b);      // True if both are C NULL.
        else if (op == PY_CMP_NE) result = (a != b); // False if both are C NULL.
        else {
            // Ordering comparisons with a C NULL pointer should be an error.
            fprintf(stderr, "TypeError: '%s' not supported with NULL pointer operand\n", py_compare_op_name(op));
            return NULL; // Indicate error
        }
        return py_create_bool(result);
    }

    PyObject* none_singleton = py_get_none(); // Get the unique None object

    // Step 1: Handle comparisons involving Python's None object.
    bool a_is_python_none = (a == none_singleton);
    bool b_is_python_none = (b == none_singleton);

    if (a_is_python_none || b_is_python_none) {
        // If either operand is Python's None:
        bool identity_match = (a == b); // Since None is a singleton, pointer comparison is key.
        if (op == PY_CMP_EQ) {
            return py_create_bool(identity_match);
        } else if (op == PY_CMP_NE) {
            return py_create_bool(!identity_match);
        } else {
            // Ordering comparisons (<, <=, >, >=) with None raise TypeError in Python 3.
            fprintf(stderr, "TypeError: '%s' not supported between instances of '%s' and '%s'\n",
                    py_compare_op_name(op),
                    py_type_name(py_get_safe_type_id(a)), // py_get_safe_type_id should correctly name PY_TYPE_NONE
                    py_type_name(py_get_safe_type_id(b)));
            return NULL; // Indicate TypeError
        }
    }

    // Step 2: Neither 'a' nor 'b' is Python's None. Proceed with rich comparison / other types.
    // Handle EQ and NE via type-specific 'equals' method or fallback to identity.
    if (op == PY_CMP_EQ || op == PY_CMP_NE) {
        int typeIdA = py_get_safe_type_id(a);
        const PyTypeMethods* methodsA = py_get_type_methods(typeIdA);

        if (methodsA && methodsA->equals) {
            PyObject* resultObj = methodsA->equals(a, b); // 'a' and 'b' are not None here.
            if (resultObj) {
                if (py_get_safe_type_id(resultObj) == PY_TYPE_BOOL) {
                    bool eq_result = py_extract_bool(resultObj);
                    py_decref(resultObj);
                    return py_create_bool(op == PY_CMP_EQ ? eq_result : !eq_result);
                } else {
                    // __eq__ should return a boolean or NotImplemented.
                    // If it's not bool, it's a TypeError.
                    fprintf(stderr, "TypeError: __eq__ returned non-boolean (type %s)\n", py_type_name(resultObj->typeId));
                    py_decref(resultObj);
                    return NULL;
                }
            } else {
                // methodsA->equals returned NULL. This could mean an error,
                // or it could be a way to signal NotImplemented.
                // Python's rich comparison is complex. For now, fallback to identity.
                // A more complete system would try b->type->methods->equals(b,a) if a->equals returned NotImplemented.
                fprintf(stderr, "Debug: equals method for type %s returned NULL or was not found, falling back to identity for EQ/NE.\n", py_type_name(typeIdA));
            }
        }
        // Fallback to identity comparison if no specific 'equals' or if it returned NULL (simplified).
        bool identity_result = (a == b); // C pointer comparison
        return py_create_bool(op == PY_CMP_EQ ? identity_result : !identity_result);
    }

    // Step 3: Handle ordering comparisons (<, <=, >, >=) for non-None types.
    // (None cases for these ops already returned TypeError in Step 1).
    int aTypeId = getBaseTypeId(a->typeId);
    int bTypeId = getBaseTypeId(b->typeId);

    // --- GMP Numeric Comparison ---
    bool aIsNumeric = (aTypeId == PY_TYPE_INT || aTypeId == PY_TYPE_DOUBLE || aTypeId == PY_TYPE_BOOL);
    bool bIsNumeric = (bTypeId == PY_TYPE_INT || bTypeId == PY_TYPE_DOUBLE || bTypeId == PY_TYPE_BOOL);

    if (aIsNumeric && bIsNumeric) {
        // ... (Your existing GMP numeric comparison logic for LT, LE, GT, GE) ...
        // This logic is only reached if neither 'a' nor 'b' is None.
        // Ensure this part is correct and handles all numeric type combinations.
        mpz_ptr a_int = py_extract_int(a);
        mpf_ptr a_float = py_extract_double(a);
        bool a_bool_val = false;
        bool a_is_bool = (aTypeId == PY_TYPE_BOOL);
        if (a_is_bool) a_bool_val = py_extract_bool(a);

        mpz_ptr b_int = py_extract_int(b);
        mpf_ptr b_float = py_extract_double(b);
        bool b_bool_val = false;
        bool b_is_bool = (bTypeId == PY_TYPE_BOOL);
        if (b_is_bool) b_bool_val = py_extract_bool(b);

        int cmp_val;

        if (a_float || b_float) {
            mpf_t temp_a, temp_b;
            mpf_init2(temp_a, RUNTIME_FLOATE_PRECISION); mpf_init2(temp_b, RUNTIME_FLOATE_PRECISION);
            bool use_temp_a = false, use_temp_b = false;
            mpf_srcptr op_a_f, op_b_f;

            if (a_float) op_a_f = a_float;
            else if (a_int) { mpf_set_z(temp_a, a_int); op_a_f = temp_a; use_temp_a = true; }
            else if (a_is_bool) { mpf_set_ui(temp_a, a_bool_val ? 1 : 0); op_a_f = temp_a; use_temp_a = true; }
            else { mpf_clear(temp_a); mpf_clear(temp_b); return NULL; }

            if (b_float) op_b_f = b_float;
            else if (b_int) { mpf_set_z(temp_b, b_int); op_b_f = temp_b; use_temp_b = true; }
            else if (b_is_bool) { mpf_set_ui(temp_b, b_bool_val ? 1 : 0); op_b_f = temp_b; use_temp_b = true; }
            else { if(use_temp_a) mpf_clear(temp_a); mpf_clear(temp_b); return NULL; }
            
            cmp_val = mpf_cmp(op_a_f, op_b_f);
            if (use_temp_a) mpf_clear(temp_a);
            if (use_temp_b) mpf_clear(temp_b);
        } else { 
            mpz_t temp_a_z, temp_b_z;
            mpz_init(temp_a_z); mpz_init(temp_b_z);
            bool use_temp_a_z = false, use_temp_b_z = false;
            mpz_srcptr op_a_z, op_b_z;

            if (a_int) op_a_z = a_int;
            else if (a_is_bool) { mpz_set_ui(temp_a_z, a_bool_val ? 1 : 0); op_a_z = temp_a_z; use_temp_a_z = true; }
            else { mpz_clear(temp_a_z); mpz_clear(temp_b_z); return NULL; }

            if (b_int) op_b_z = b_int;
            else if (b_is_bool) { mpz_set_ui(temp_b_z, b_bool_val ? 1 : 0); op_b_z = temp_b_z; use_temp_b_z = true; }
            else { if(use_temp_a_z) mpz_clear(temp_a_z); mpz_clear(temp_b_z); return NULL; }

            cmp_val = mpz_cmp(op_a_z, op_b_z);
            if (use_temp_a_z) mpz_clear(temp_a_z);
            if (use_temp_b_z) mpz_clear(temp_b_z);
        }

        bool final_result = false;
        switch (op) {
            case PY_CMP_LT: final_result = (cmp_val < 0); break;
            case PY_CMP_LE: final_result = (cmp_val <= 0); break;
            case PY_CMP_GT: final_result = (cmp_val > 0); break;
            case PY_CMP_GE: final_result = (cmp_val >= 0); break;
            default: fprintf(stderr, "Internal Error: Unhandled numeric comparison op %d\n", op); return NULL;
        }
        return py_create_bool(final_result);
    }

    // --- String Comparison ---
    if (aTypeId == PY_TYPE_STRING && bTypeId == PY_TYPE_STRING) {
        // ... (Your existing string comparison logic for LT, LE, GT, GE) ...
        // This logic is only reached if neither 'a' nor 'b' is None.
        const char* strA = py_extract_string(a);
        const char* strB = py_extract_string(b);
        // Since a and b are not C NULL here, strA and strB should not be NULL
        // unless py_extract_string can return NULL for valid string objects (which it shouldn't).

        int cmpResult_str = strcmp(strA, strB); // Assuming strA and strB are valid
        bool final_result_str = false;
        switch (op) {
            case PY_CMP_LT: final_result_str = (cmpResult_str < 0); break;
            case PY_CMP_LE: final_result_str = (cmpResult_str <= 0); break;
            case PY_CMP_GT: final_result_str = (cmpResult_str > 0); break;
            case PY_CMP_GE: final_result_str = (cmpResult_str >= 0); break;
            default: fprintf(stderr, "Internal Error: Unhandled string comparison op %d\n", op); return NULL;
        }
        return py_create_bool(final_result_str);
    }

    // Step 4: Incompatible types for ordering comparison.
    fprintf(stderr, "TypeError: '%s' not supported between instances of '%s' and '%s'\n",
            py_compare_op_name(op),
            py_type_name(aTypeId), py_type_name(bTypeId));
    return NULL; // Indicate TypeError
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