// 所有公共类型定义、结构体声明和枚举
#ifndef PY_RUNTIME_COMMON_H
#define PY_RUNTIME_COMMON_H
#include "TypeIDs.h"

#include <cstddef>
#include <vector>
#include <cstddef>  // For size_t, NULL
#include <cstring>  // For strcmp

#include <gmp.h>   // <--- 包含 GMP 头文件
#include <mpfr.h>  // MPFR 头文件
#include <ffi.h>   // 包含 libffi 头文件
#ifdef __cplusplus
extern "C"
{
#endif

    // 基本对象结构体声明
    typedef struct PyObject_t PyObject;
    typedef struct PyListObject_t PyListObject;
    typedef struct PyDictEntry_t PyDictEntry;
    typedef struct PyDictObject_t PyDictObject;
    typedef struct PyTupleObject_t PyTupleObject;  // Forward declaration
    typedef struct PyPrimitiveObject_t PyPrimitiveObject;

    typedef struct PyClassObject_t PyClassObject;
    typedef struct PyInstanceObject_t PyInstanceObject;
    typedef struct PyBoundMethodObject_t PyBoundMethodObject;

    typedef struct PyFunctionObject_t PyFunctionObject;

    // 结构体定义
    struct PyObject_t
    {
        int refCount;  // 引用计数
        int typeId;    // 类型ID
    };

    struct PyListObject_t
    {
        PyObject header;  // PyObject头
        int length;       // 当前长度
        int capacity;     // 分配容量
        int elemTypeId;   // 元素类型ID
        PyObject** data;  // 元素数据指针数组
    };

    struct PyDictEntry_t
    {
        PyObject* key;
        PyObject* value;
        int hash;
        bool used;
    };

    struct PyDictObject_t
    {
        PyObject header;       // PyObject头
        int size;              // 条目数量
        int capacity;          // 哈希表容量
        int keyTypeId;         // 键类型ID
        PyDictEntry* entries;  // 哈希表
    };

    struct PyPrimitiveObject_t
    {
        PyObject header;
        union
        {
            // int intValue;      // <--- 移除原生 int
            // double doubleValue; // <--- 移除原生 double
            mpz_t intValue;     // <--- 使用 GMP 整数类型
            mpf_t doubleValue;  // <--- 使用 GMP 浮点数类型 (需要设置精度)
            bool boolValue;
            char* stringValue;
        } value;
    };

    // 类对象结构体
    struct PyClassObject_t
    {
        PyObject header;           // PyObject 头 (typeId 应为 PY_TYPE_CLASS)
        const char* name;          // 类名
        PyClassObject* base;       // 基类 (简单起见，先支持单继承)
        PyDictObject* class_dict;  // 类属性字典 (包含方法、类变量等)
        // 可以添加 PyTypeMethods* 指针，或者依赖基于 typeId 的动态查找
        //可能：PyTypeMethods*type_methods；//用于自定义行为
        //可能：MRO（方法解析顺序）列表
        // 可以添加其他字段，如 __name__, __doc__, 闭包变量等?
    };

    struct PyFunctionObject_t
    {
        PyObject header;       /**< 标准对象头 (refCount, typeId)。typeId 应为 PY_TYPE_FUNC。*/
        void* func_ptr;        /**< 指向编译后的 LLVM 函数的指针。*/
        int signature_type_id; /**< 来自编译器的 FunctionType 的类型 ID，描述函数签名。*/
        // 可以添加其他字段，如 __name__, __doc__, 闭包变量等
    };

    // 实例对象结构体
    struct PyInstanceObject_t
    {
        PyObject header;              // PyObject 头 (typeId 应为 PY_TYPE_INSTANCE 或 >= PY_TYPE_INSTANCE_BASE)
        PyClassObject* cls;           // 指向该实例所属的类对象
        PyDictObject* instance_dict;  // 实例属性字典
    };

    struct PyBoundMethodObject_t
    {
        PyObject header;
        PyObject* bm_self;          // 绑定的实例 ('self')
        PyFunctionObject* bm_func;  // 原始的函数对象 (确保是 PyFunctionObject*)
    };

    /**
     * @brief 元组对象结构。
     * items 是一个柔性数组成员，或者如果编译器不支持，则为 PyObject**。
     * 如果是 PyObject**，则 items 指针本身需要在 PyTupleObject 之后分配。
     * 这里我们使用柔性数组成员的风格。
     */
    struct PyTupleObject_t
    {
        PyObject header;    ///< 对象头 (PyObject_t)，包含引用计数和类型 ID (应为 PY_TYPE_TUPLE)。
        int size;           ///< 元组中的元素数量。
        PyObject* items[];  ///< 指向元组元素的指针数组 (柔性数组成员)。
    };
    // 比较操作符枚举
    typedef enum
    {
        PY_CMP_EQ = 0,  // ==
        PY_CMP_NE = 1,  // !=
        PY_CMP_LT = 2,  // <
        PY_CMP_LE = 3,  // <=
        PY_CMP_GT = 4,  // >
        PY_CMP_GE = 5   // >=
    } PyCompareOp;

    /**
     * @brief 列表迭代器对象结构。
     */
    typedef struct PyListIteratorObject_t
    {
        PyObject header;     ///< 对象头 (PyObject_t)，包含引用计数和类型 ID (应为 PY_TYPE_LIST_ITERATOR)。
        PyObject* iterable;  ///< 指向被迭代的列表对象 (PyListObject*)。迭代器持有对此对象的引用。
        int current_index;   ///< 当前迭代到的列表索引。
    } PyListIteratorObject;

    /**
     * @brief 字符串迭代器对象结构。
     */
    typedef struct PyStringIteratorObject_t
    {
        PyObject header;            ///< 对象头 (PyObject_t)，包含引用计数和类型 ID (应为 PY_TYPE_STRING_ITERATOR)。
        PyObject* iterable;         ///< 指向被迭代的字符串对象 (PyPrimitiveObject*)。迭代器持有对此对象的引用。
        size_t current_char_index;  ///< 当前迭代到的字符索引。
    } PyStringIteratorObject;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // PY_RUNTIME_COMMON_H