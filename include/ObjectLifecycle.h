#ifndef OBJECT_LIFECYCLE_H
#define OBJECT_LIFECYCLE_H

//#include "ast.h"
#include "ObjectType.h"
#include "CodeGen/CodeGenBase.h"
#include "TypeIDs.h"
#include <llvm/IR/Value.h>
#include <string>
#include <memory>

namespace llvmpy
{

class PyCodeGen;

/**
 * 对象生命周期管理器 - 负责确定对象何时需要复制、引用计数增减和类型控制
 */
class ObjectLifecycleManager
{
public:
    // 将类型信息附加到LLVM值 (接受常量整数类型ID)
    static void attachTypeMetadata(llvm::Value* value, int typeId);

    // 从LLVM值获取类型信息
    static int getTypeIdFromMetadata(llvm::Value* value);

    // 新增: 将类型信息附加到LLVM值 (接受动态类型ID值)
    static void attachDynamicTypeMetadata(PyCodeGen& gen, llvm::Value* value, llvm::Value* typeIdValue);

    // 新增: 尝试从LLVM常量中提取整数值
    static int extractConstantInt(llvm::Value* value);

    /**
     * 确定表达式的对象来源
     * @param expr 表达式AST节点
     * @return 对象来源类型
     */
    enum class ObjectSource
    {
        FUNCTION_RETURN,   // 函数返回值
        BINARY_OP,         // 二元操作结果
        UNARY_OP,          // 一元操作结果
        LOCAL_VARIABLE,    // 局部变量
        PARAMETER,         // 函数参数
        LITERAL,           // 字面量
        INDEX_ACCESS,      // 索引访问
        ATTRIBUTE_ACCESS,  // 属性访问
        TEMPORARY          // 临时对象 - 添加这一行
    };
    static ObjectSource determineExprSource(const ExprAST* expr);

    bool isContainerType(llvm::Value* value);
    llvm::Value* ensureContainer(PyCodeGen& codegen, llvm::Value* value, int containerTypeId);
    // 对象来源类型 - 决定了对象的处理方式
    // 对象来源类型 - 决定了对象的处理方式

    // 对象目标类型 - 决定了对象的使用方式
    enum class ObjectDestination
    {
        RETURN,      // 作为返回值
        ASSIGNMENT,  // 作为赋值目标
        PARAMETER,   // 作为函数参数
        TEMPORARY,   // 临时使用（如表达式计算中）
        STORAGE      // 长期存储（如全局变量）
    };

    /**
     * 决定是否需要复制对象
     * @param type 对象类型
     * @param source 对象来源
     * @param dest 对象目标
     * @return 如果需要复制返回true，否则返回false
     */
    static bool needsCopy(ObjectType* type, ObjectSource source, ObjectDestination dest = ObjectDestination::TEMPORARY);

    /**
     * 决定是否需要增加引用计数
     * @param type 对象类型
     * @param source 对象来源
     * @param dest 对象目标
     * @return 如果需要增加引用计数返回true，否则返回false
     */
    static bool needsIncRef(ObjectType* type, ObjectSource source, ObjectDestination dest = ObjectDestination::TEMPORARY);

    /**
     * 决定是否需要释放对象（减少引用计数）
     * @param type 对象类型
     * @param source 对象来源
     * @return 如果需要释放返回true，否则返回false
     */
    static bool needsDecRef(ObjectType* type, ObjectSource source);

    /**
     * 获取对象的正确类型ID（用于复制/创建操作）
     * @param type 对象类型
     * @return 对象的运行时类型ID
     */
    static int getObjectTypeId(ObjectType* type);

    /**
     * 获取容器元素的类型ID
     * @param containerType 容器类型
     * @return 容器元素类型的ID
     */
    static int getElementTypeId(ObjectType* containerType);

    /**
     * 检查对象是否需要包装（从基本类型转为引用类型）
     * @param value LLVM值
     * @param type 目标类型
     * @return 是否需要包装
     */
    static bool needsWrapping(llvm::Value* value, ObjectType* type);

    /**
     * 调整对象，确保类型正确（复制/引用/包装/拆包）
     * @param gen 代码生成器
     * @param value 原始值
     * @param type 对象类型
     * @param source 对象来源
     * @param dest 对象目标
     * @return 调整后的值
     */
    static llvm::Value* adjustObject(
            PyCodeGen& gen,
            llvm::Value* value,
            ObjectType* type,
            ObjectSource source,
            ObjectDestination dest = ObjectDestination::TEMPORARY);

    /**
     * 创建对象 - 根据基本类型创建Python对象
     * @param gen 代码生成器
     * @param value 基本类型值
     * @param typeId 类型ID
     * @return 创建的对象
     */
    static llvm::Value* createObject(
            PyCodeGen& gen,
            llvm::Value* value,
            int typeId);

    /**
     * 处理函数返回值 - 确保返回的对象类型正确
     * @param gen 代码生成器
     * @param value 返回值
     * @param functionReturnType 函数返回类型
     * @param exprType 表达式类型
     * @return 处理后的返回值
     */
    static llvm::Value* handleReturnValue(
            PyCodeGen& gen,
            llvm::Value* value,
            ObjectType* functionReturnType,
            std::shared_ptr<PyType> exprType);

    /**
     * 获取LLVM值的类型ID
     * @param gen 代码生成器
     * @param value LLVM值
     * @return 类型ID，如果无法确定返回-1
     */
    static int getTypeIdFromValue(PyCodeGen& gen, llvm::Value* value);

    /**
     * 检查两个类型是否兼容（可以相互转换）
     * @param typeA 类型A
     * @param typeB 类型B
     * @return 是否兼容
     */
    static bool areTypesCompatible(ObjectType* typeA, ObjectType* typeB);

    /**
     * 判断是否为临时对象
     * @param gen 代码生成器
     * @param value LLVM值
     * @return 是否为临时对象
     */
    static bool isTempObject(PyCodeGen& gen, llvm::Value* value);
};

/**
 * 对象工厂类 - 负责创建各种类型的对象
 */
class ObjectFactory
{
public:
    /**
     * 创建整数对象
     * @param gen 代码生成器
     * @param value 整数值
     * @return 整数对象
     */
    static llvm::Value* createInt(PyCodeGen& gen, llvm::Value* value);

    /**
     * 创建浮点数对象
     * @param gen 代码生成器
     * @param value 浮点数值
     * @return 浮点数对象
     */
    static llvm::Value* createDouble(PyCodeGen& gen, llvm::Value* value);

    /**
     * 创建布尔对象
     * @param gen 代码生成器
     * @param value 布尔值
     * @return 布尔对象
     */
    static llvm::Value* createBool(PyCodeGen& gen, llvm::Value* value);

    /**
     * 创建字符串对象
     * @param gen 代码生成器
     * @param value 字符串值
     * @return 字符串对象
     */
    static llvm::Value* createString(PyCodeGen& gen, llvm::Value* value);

    /**
     * 创建列表对象
     * @param gen 代码生成器
     * @param size 列表大小
     * @param elemType 元素类型
     * @return 列表对象
     */
    static llvm::Value* createList(PyCodeGen& gen, llvm::Value* size, ObjectType* elemType);

    /**
     * 创建字典对象
     * @param gen 代码生成器
     * @param keyType 键类型
     * @param valueType 值类型
     * @return 字典对象
     */
    static llvm::Value* createDict(PyCodeGen& gen, ObjectType* keyType, ObjectType* valueType);
};

/**
 * 对象类型检查器
 */
class ObjectTypeChecker
{
public:
    /**
     * 检查值是否为指定类型
     * @param gen 代码生成器
     * @param value 值
     * @param type 类型
     * @return 是否满足类型要求
     */
    static bool isOfType(PyCodeGen& gen, llvm::Value* value, ObjectType* type);

    /**
     * 获取类型转换路径
     * @param fromType 源类型
     * @param toType 目标类型
     * @return 转换难度（数字越小越容易），-1表示不可转换
     */
    static int getConversionPath(ObjectType* fromType, ObjectType* toType);
};

}  // namespace llvmpy

#endif  // OBJECT_LIFECYCLE_H