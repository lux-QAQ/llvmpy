/**
 * @file TypeIDs.h
 * @brief 定义了 llvmpy 编译器和运行时共享的基础 Python 类型标识符。
 *
 * 这个文件包含一个枚举 `PyTypeId`，用于表示 Python 的核心数据类型。
 * 这些 ID 用于类型检查、代码生成以及运行时类型信息的传递。
 *
 * @note 随着项目发展，特别是支持用户自定义类和多文件项目时，
 *       当前的固定 ID 系统可能需要演进为一个更动态或可扩展的类型注册机制。
 *       例如，使用字符串名称或唯一的运行时句柄来标识用户类型。
 */
#ifndef TYPE_IDS_H
#define TYPE_IDS_H

namespace llvmpy
{
/**
 * @brief Python 核心类型标识符枚举。
 *
 * 这些 ID 用于在 编译时 和 运行时 表示 Python 的基本和内置复合类型。
 *
 * @warning PY_TYPE_OBJECT 和 PY_TYPE_NONE 具有相同的值 (0)。这可能需要在未来
 *          进行区分，因为实际上PY_TYPE_OBJECT基本没有用到过，这或许是一个无用的枚举值
 * @warning 复合类型（如 List<int>）和用户自定义类型目前使用基于范围的 ID。
 *          这种方法在多文件编译和大量用户类型时可能不够健壮，未来可能需要
 *          一个全局类型注册表或基于名称/哈希的类型识别系统。
 * @todo 未来可能需要一个更复杂的系统来编码或注册这些具体类型。
 */
enum PyTypeId
{
    PY_TYPE_NONE = 0,    // 无类型
    PY_TYPE_INT = 1,     // 整数类型
    PY_TYPE_DOUBLE = 2,  // 浮点数类型
    PY_TYPE_BOOL = 3,    // 布尔类型
    PY_TYPE_STRING = 4,  // 字符串类型
    PY_TYPE_LIST = 5,    // 列表类型
    PY_TYPE_DICT = 6,    // 字典类型
    /**
     * @brief 表示任意类型 (Any)。
     * 主要用于函数签名或变量，表示可以接受或存储任何类型的值。
     * 在编译时通常会放宽类型检查，在运行时需要进行动态类型检查。
     * @todo 需要明确 Any 类型在代码生成时检查中的具体实现策略。
     */
    PY_TYPE_ANY = 7,  // ANY类型
    /**
     * @brief 表示 Python 的映射类型 (map)。
     * @warning 这个 ID 可能与 `PY_TYPE_DICT` 重复或需要更明确的区分。
     *       目前在代码中几乎没有使用到这个 ID。
     */
    PY_TYPE_OBJECT = 0,  // 通用对象类型，与 NONE 相同TODO
    PY_TYPE_FUNC = 8,    // 函数类型
    PY_TYPE_TUPLE = 9,   // 元组类型
    PY_TYPE_SET = 10,    // 集合类型
    PY_TYPE_MAP = 11,    // 映射类型

    PY_TYPE_CLASS = 12,     // 类对象本身的类型 ID
    PY_TYPE_INSTANCE = 13,  // 通用实例类型 ID (或者作为基类)

    // 复合类型ID基础 - 用于运行时扩展类型ID
    PY_TYPE_LIST_BASE = 100,  // 列表类型基础 ID
    PY_TYPE_DICT_BASE = 200,  // 字典类型基础 ID
    PY_TYPE_FUNC_BASE = 300,  // 函数类型基础 ID

    // 指针和特殊类型 (400及以上)
    PY_TYPE_PTR = 400,         // 基本指针类型，指针类型的开头
    PY_TYPE_PTR_INT = 401,     // 指向整数的指针
    PY_TYPE_PTR_DOUBLE = 402,  // 指向浮点数的指针
    /**
     * @brief 用户自定义类实例的特定类型 ID 的起始基数。
     * @todo 这里为了是为类保留的。可能还需要更加缜密的思考。
     */
    PY_TYPE_INSTANCE_BASE = 500  // 用户自定义类实例 ID 从这里开始。TODO

};

/**
 * @brief 从复合或扩展的类型 ID 获取其基础类型 ID。
 *
 *
 * @param typeId 可能的扩展类型 ID。
 * @return 对应的基础类型 ID (`PyTypeId` 中的一个值)。
 *
 * @todo 随着类型系统变得复杂（例如支持继承），这个函数的逻辑可能需要调整，
 *       或者被 `PyType` 类层次结构中的方法所取代。
 */
inline int getBaseTypeId(int typeId)
{
    // 用户自定义实例都映射到 PY_TYPE_INSTANCE
    if (typeId >= PY_TYPE_INSTANCE_BASE) return PY_TYPE_INSTANCE;
    // @note 函数类型目前没有统一的基础映射，返回 NONE 可能不理想，需要重新考虑。
    if (typeId >= PY_TYPE_FUNC_BASE) return PY_TYPE_FUNC; // 映射回 PY_TYPE_FUNC
    // 具体字典类型映射回 PY_TYPE_DICT
    if (typeId >= PY_TYPE_DICT_BASE) return PY_TYPE_DICT;
    // 具体列表类型映射回 PY_TYPE_LIST
    if (typeId >= PY_TYPE_LIST_BASE) return PY_TYPE_LIST;

    // 类对象本身
    if (typeId == PY_TYPE_CLASS) return PY_TYPE_CLASS;
    // 通用实例类型（虽然通常不会直接使用此 ID）
    if (typeId == PY_TYPE_INSTANCE) return PY_TYPE_INSTANCE;

    // 对于基础类型，直接返回自身
    // @note 需要确保 typeId 在基础类型范围内是有效的 PyTypeId 值。
    return typeId;
}

// 从内部类型ID映射到运行时类型ID
// TODO：runtime解构
inline int mapToRuntimeTypeId(int internalTypeId)
{
    return getBaseTypeId(internalTypeId);
}

}  // namespace llvmpy

#endif  // TYPE_IDS_H