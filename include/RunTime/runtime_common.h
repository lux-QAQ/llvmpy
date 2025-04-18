// 所有公共类型定义、结构体声明和枚举
#ifndef PY_RUNTIME_COMMON_H
#define PY_RUNTIME_COMMON_H

#include "TypeIDs.h"
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// 基本对象结构体声明
typedef struct PyObject_t PyObject;
typedef struct PyListObject_t PyListObject;
typedef struct PyDictEntry_t PyDictEntry;
typedef struct PyDictObject_t PyDictObject;
typedef struct PyPrimitiveObject_t PyPrimitiveObject;

typedef struct PyClassObject_t PyClassObject;     
typedef struct PyInstanceObject_t PyInstanceObject; 

typedef struct PyFunctionObject_t PyFunctionObject;

// 结构体定义
struct PyObject_t {
    int refCount;  // 引用计数
    int typeId;    // 类型ID
};

struct PyListObject_t {
    PyObject header;  // PyObject头
    int length;       // 当前长度
    int capacity;     // 分配容量
    int elemTypeId;   // 元素类型ID
    PyObject** data;  // 元素数据指针数组
};

struct PyDictEntry_t {
    PyObject* key;
    PyObject* value;
    int hash;
    bool used;
};

struct PyDictObject_t {
    PyObject header;       // PyObject头
    int size;              // 条目数量
    int capacity;          // 哈希表容量
    int keyTypeId;         // 键类型ID
    PyDictEntry* entries;  // 哈希表
};

struct PyPrimitiveObject_t {
    PyObject header;
    union {
        int intValue;
        double doubleValue;
        bool boolValue;
        char* stringValue;
    } value;
};


// 类对象结构体
struct PyClassObject_t {
    PyObject header;        // PyObject 头 (typeId 应为 PY_TYPE_CLASS)
    const char* name;       // 类名
    PyClassObject* base;    // 基类 (简单起见，先支持单继承)
    PyDictObject* class_dict; // 类属性字典 (包含方法、类变量等)
    // 可以添加 PyTypeMethods* 指针，或者依赖基于 typeId 的动态查找
};


struct PyFunctionObject_t {
    PyObject header;            /**< 标准对象头 (refCount, typeId)。typeId 应为 PY_TYPE_FUNC。*/
    void* func_ptr;             /**< 指向编译后的 LLVM 函数的指针。*/
    int   signature_type_id;    /**< 来自编译器的 FunctionType 的类型 ID，描述函数签名。*/
    // 可以添加其他字段，如 __name__, __doc__, 闭包变量等
};

// 实例对象结构体
struct PyInstanceObject_t {
    PyObject header;        // PyObject 头 (typeId 应为 PY_TYPE_INSTANCE 或 >= PY_TYPE_INSTANCE_BASE)
    PyClassObject* cls;     // 指向该实例所属的类对象
    PyDictObject* instance_dict; // 实例属性字典
};

// 比较操作符枚举
typedef enum {
    PY_CMP_EQ = 0,  // ==
    PY_CMP_NE = 1,  // !=
    PY_CMP_LT = 2,  // <
    PY_CMP_LE = 3,  // <=
    PY_CMP_GT = 4,  // >
    PY_CMP_GE = 5   // >=
} PyCompareOp;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // PY_RUNTIME_COMMON_H