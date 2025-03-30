#ifndef TYPE_IDS_H
#define TYPE_IDS_H

namespace llvmpy {

// 基本类型ID - 这些ID在C和C++代码之间共享
enum PyTypeId {
    PY_TYPE_NONE    = 0,
    PY_TYPE_INT     = 1,
    PY_TYPE_DOUBLE  = 2,
    PY_TYPE_BOOL    = 3,
    PY_TYPE_STRING  = 4,
    PY_TYPE_LIST    = 5,
    PY_TYPE_DICT    = 6,
    PY_TYPE_ANY     = 7,  // 添加ANY类型
    PY_TYPE_OBJECT  = 0,  // 通用对象类型，与 NONE 相同
    PY_TYPE_FUNC   = 8,  // 函数类型
    // 复合类型ID基础 - 用于运行时扩展类型ID
    PY_TYPE_LIST_BASE = 100,
    PY_TYPE_DICT_BASE = 200,
    PY_TYPE_FUNC_BASE = 300
};

// 从复合类型ID获取基本类型ID (截断高位)
inline int getBaseTypeId(int typeId) {
    if (typeId >= PY_TYPE_FUNC_BASE) return PY_TYPE_NONE;
    if (typeId >= PY_TYPE_DICT_BASE) return PY_TYPE_DICT;
    if (typeId >= PY_TYPE_LIST_BASE) return PY_TYPE_LIST;
    return typeId;
}

// 从内部类型ID映射到运行时类型ID
inline int mapToRuntimeTypeId(int internalTypeId) {
    return getBaseTypeId(internalTypeId);
}

} // namespace llvmpy

// 定义C代码中可见的类型ID
// 仅在C文件中包含时使用，否则使用llvmpy::PyTypeId
#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
enum PyTypeId {
    PY_TYPE_NONE    = 0,
    PY_TYPE_INT     = 1,
    PY_TYPE_DOUBLE  = 2,
    PY_TYPE_BOOL    = 3,
    PY_TYPE_STRING  = 4,
    PY_TYPE_LIST    = 5,
    PY_TYPE_DICT    = 6,
    PY_TYPE_ANY     = 7  // 也需要在C版本中添加??是否需要在runtime中添加
};
#endif

#endif // TYPE_IDS_H