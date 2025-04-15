#ifndef TYPE_IDS_H
#define TYPE_IDS_H

namespace llvmpy
{

// 基本类型ID - 这些ID在C和C++代码之间共享
enum PyTypeId
{
    PY_TYPE_NONE = 0,
    PY_TYPE_INT = 1,
    PY_TYPE_DOUBLE = 2,
    PY_TYPE_BOOL = 3,
    PY_TYPE_STRING = 4,
    PY_TYPE_LIST = 5,
    PY_TYPE_DICT = 6,
    PY_TYPE_ANY = 7,     // 添加ANY类型
    PY_TYPE_OBJECT = 0,  // 通用对象类型，与 NONE 相同
    PY_TYPE_FUNC = 8,    // 函数类型
    PY_TYPE_TUPLE = 9,   // 元组类型
    PY_TYPE_SET = 10,    // 集合类型
    PY_TYPE_MAP = 11,    // 映射类型

    


    PY_TYPE_CLASS = 12,     // 类对象本身的类型 ID
    PY_TYPE_INSTANCE = 13,  // 通用实例类型 ID (或者作为基类)
 

    // 复合类型ID基础 - 用于运行时扩展类型ID
    PY_TYPE_LIST_BASE = 100,
    PY_TYPE_DICT_BASE = 200,
    PY_TYPE_FUNC_BASE = 300,

    // 指针和特殊类型 (400及以上)
    PY_TYPE_PTR = 400,        // 基本指针类型
    PY_TYPE_PTR_INT = 401,    // 指向整数的指针
    PY_TYPE_PTR_DOUBLE = 402 , // 指向浮点数的指针

    PY_TYPE_INSTANCE_BASE = 500 // 用户自定义类实例 ID 从这里开始

};

// 从复合类型ID获取基本类型ID (截断高位)
inline int getBaseTypeId(int typeId)
{
    if (typeId >= PY_TYPE_INSTANCE_BASE) return PY_TYPE_INSTANCE; // 所有自定义实例的基类是 INSTANCE
    if (typeId >= PY_TYPE_FUNC_BASE) return PY_TYPE_NONE; // 函数没有明确的基类映射
    if (typeId >= PY_TYPE_DICT_BASE) return PY_TYPE_DICT;
    if (typeId >= PY_TYPE_LIST_BASE) return PY_TYPE_LIST;
    
    if (typeId == PY_TYPE_CLASS) return PY_TYPE_CLASS;
    if (typeId == PY_TYPE_INSTANCE) return PY_TYPE_INSTANCE;
    
    return typeId; // 基本类型返回自身
}

// 从内部类型ID映射到运行时类型ID
inline int mapToRuntimeTypeId(int internalTypeId)
{
    return getBaseTypeId(internalTypeId);
}

}  // namespace llvmpy

// 定义C代码中可见的类型ID
// 仅在C文件中包含时使用，否则使用llvmpy::PyTypeId
#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
enum PyTypeId
{
    PY_TYPE_NONE = 0,
    PY_TYPE_INT = 1,
    PY_TYPE_DOUBLE = 2,
    PY_TYPE_BOOL = 3,
    PY_TYPE_STRING = 4,
    PY_TYPE_LIST = 5,
    PY_TYPE_DICT = 6,
    PY_TYPE_ANY = 7  // 也需要在C版本中添加??是否需要在runtime中添加
};
#endif

#endif  // TYPE_IDS_H