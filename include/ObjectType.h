#ifndef OBJECT_TYPE_H
#define OBJECT_TYPE_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <llvm/IR/Type.h>
#include <llvm/IR/LLVMContext.h>

#include "TypeIDs.h"  // 包含类型ID定义

namespace llvmpy
{

// 前置声明
class TypeVisitor;
class TypeRegistry;

// 对象类型基类 - 采用访问者模式设计
class ObjectType
{
public:
    enum TypeCategory
    {
        Primitive,  // 原始类型：int, double, bool等
        Container,  // 容器类型：list, dict等
        Reference,  // 引用类型：对象引用
        Function,   // 函数类型
        Unknown     // 未知类型
    };

    // 在ObjectType类的声明中添加:
    void setFeature(const std::string& feature, bool value)
    {
        registerFeature(name, feature, value);
    }

    // 添加一个显式的isReference方法，方便调用
    virtual bool isReference() const
    {
        return hasFeature("reference");
    }

    // 添加一个显式的isMutable方法，方便调用
    virtual bool isMutable() const
    {
        return hasFeature("mutable");
    }
    ObjectType(const std::string& name, TypeCategory category);
    virtual ~ObjectType() = default;

    // 基本信息
    const std::string& getName() const
    {
        return name;
    }
    TypeCategory getCategory() const
    {
        return category;
    }

    // 类型系统接口
    virtual bool canAssignTo(const ObjectType* other) const;
    virtual bool canConvertTo(const ObjectType* other) const;

    // 访问者模式接口
    virtual void accept(TypeVisitor& visitor) const = 0;

    // LLVM类型生成接口
    virtual llvm::Type* getLLVMType(llvm::LLVMContext& context) const = 0;

    // 运行时类型接口
    virtual int getTypeId() const = 0;
    virtual std::string getTypeSignature() const;

    // 类型特性判断 - 不使用硬编码方式
    virtual bool hasFeature(const std::string& feature) const;

    // 注册新特性
    static void registerFeature(const std::string& typeName,
                                const std::string& feature,
                                bool value = true);

protected:
    std::string name;
    TypeCategory category;

    // 特性注册表（基于功能而非类型的特性）
    static std::unordered_map<std::string,
                              std::unordered_map<std::string, bool>>
            typeFeatures;
};

// 类型访问者接口
class TypeVisitor
{
public:
    virtual ~TypeVisitor() = default;

    // 各类型访问方法
    virtual void visit(const class PrimitiveType& type) = 0;
    virtual void visit(const class ListType& type) = 0;
    virtual void visit(const class DictType& type) = 0;
    virtual void visit(const class FunctionType& type) = 0;
};

// 原始类型：int, double, bool等
class PrimitiveType : public ObjectType
{
public:
    PrimitiveType(const std::string& name);

    virtual void accept(TypeVisitor& visitor) const override
    {
        visitor.visit(*this);
    }

    virtual llvm::Type* getLLVMType(llvm::LLVMContext& context) const override;
    virtual int getTypeId() const override;
};

// 容器类型：list
class ListType : public ObjectType
{
public:
    ListType(ObjectType* elemType);

    virtual void accept(TypeVisitor& visitor) const override
    {
        visitor.visit(*this);
    }

    virtual llvm::Type* getLLVMType(llvm::LLVMContext& context) const override;
    virtual int getTypeId() const override;
    virtual std::string getTypeSignature() const override;

    const ObjectType* getElementType() const
    {
        return elementType;
    }

private:
    ObjectType* elementType;
};

// 字典类型：dict
class DictType : public ObjectType
{
public:
    DictType(ObjectType* keyType, ObjectType* valueType);

    virtual void accept(TypeVisitor& visitor) const override
    {
        visitor.visit(*this);
    }

    virtual llvm::Type* getLLVMType(llvm::LLVMContext& context) const override;
    virtual int getTypeId() const override;
    virtual std::string getTypeSignature() const override;

    const ObjectType* getKeyType() const
    {
        return keyType;
    }
    const ObjectType* getValueType() const
    {
        return valueType;
    }

private:
    ObjectType* keyType;
    ObjectType* valueType;
};

// 函数类型
class FunctionType : public ObjectType
{
public:
    FunctionType(ObjectType* returnType, std::vector<ObjectType*> paramTypes);

    virtual void accept(TypeVisitor& visitor) const override
    {
        visitor.visit(*this);
    }

    virtual llvm::Type* getLLVMType(llvm::LLVMContext& context) const override;
    virtual int getTypeId() const override;
    virtual std::string getTypeSignature() const override;

    const ObjectType* getReturnType() const
    {
        return returnType;
    }
    const std::vector<ObjectType*>& getParamTypes() const
    {
        return paramTypes;
    }
        /** @brief LLVM RTTI support. */
    static bool classof(const ObjectType *O) {
        // ObjectType::TypeCategory is an enum within ObjectType.
        // We access its members directly.
        return O && O->getCategory() == ObjectType::Function;
    }

private:
    ObjectType* returnType;
    std::vector<ObjectType*> paramTypes;
};

// 类型注册表 - 使用注册器模式
class TypeRegistry
{
public:
    // 单例访问
    static TypeRegistry& getInstance();

    // 类型查找与创建
    ObjectType* getType(const std::string& name);
    ListType* getListType(ObjectType* elemType);
    DictType* getDictType(ObjectType* keyType, ObjectType* valueType);
    FunctionType* getFunctionType(ObjectType* returnType,
                                  std::vector<ObjectType*> paramTypes);

    // 根据类型ID获取类型对象
    ObjectType* getTypeById(int typeId);
    // 添加通过函数名查找函数类型的方法
    FunctionType* getFunctionType(const std::string& functionName);

    // 从字符串解析类型
    ObjectType* parseTypeFromString(const std::string& typeStr);

    ObjectType* getSymbolType(const std::string& name) const;
    void registerSymbolType(const std::string& name, ObjectType* type)
    {
        symbolTypes[name] = type;
    }
    // 类型转换
    bool canConvert(ObjectType* from, ObjectType* to);

    // 类型注册
    void registerType(const std::string& name, ObjectType* type);

    // 注册类型创建器
    template <typename T>
    void registerTypeCreator(const std::string& name,
                             std::function<T*(const std::string&)> creator)
    {
        typeCreators[name] = [creator](const std::string& typeName) -> ObjectType*
        {
            return creator(typeName);
        };
    }
    // 添加这个公共方法来确保基本类型已注册
    void ensureBasicTypesRegistered()
    {
        // 检查一个关键基本类型是否存在，如果不存在则注册所有基本类型
        if (!getType("int"))
        {
            registerBuiltinTypes();
        }
    }

    

private:
    TypeRegistry();
    ~TypeRegistry();

    std::unordered_map<std::string, ObjectType*> symbolTypes;

    // 注册基础类型
    void registerBuiltinTypes();

    // 类型存储
    std::unordered_map<std::string, ObjectType*> namedTypes;
    std::unordered_map<std::string, ListType*> listTypes;
    std::unordered_map<std::string, DictType*> dictTypes;
    std::unordered_map<std::string, FunctionType*> functionTypes;

    // 添加ID到类型的映射
    std::unordered_map<int, ObjectType*> typeIdMap;

    // 类型创建器
    std::unordered_map<std::string, std::function<ObjectType*(const std::string&)>> typeCreators;
};

// 类型特性检查助手 - 使用模板方法模式
// 类型特性检查助手 - 使用注册器模式
class TypeFeatureChecker
{
public:
    using FeatureCheckFunc = std::function<bool(const ObjectType*)>;

    // 注册新的特性检查函数
    static void registerFeatureCheck(const std::string& featureName,
                                     FeatureCheckFunc checkFunc)
    {
        getFeatureChecks()[featureName] = std::move(checkFunc);
    }

    static bool isIndexable(const ObjectType* type)
    {
        return hasFeature(type, "indexable");
    }
    // 动态检查类型是否有特定特性
    static bool hasFeature(const ObjectType* type, const std::string& featureName)
    {
        if (!type) return false;

        auto& checkers = getFeatureChecks();
        auto it = checkers.find(featureName);

        if (it != checkers.end())
        {
            // 使用注册的检查函数
            return it->second(type);
        }

        // 回退到类型自身的特性声明
        return type->hasFeature(featureName);
    }

    // 便捷方法 - 现在都是通过hasFeature统一实现
    static bool isNumeric(const ObjectType* type)
    {
        return hasFeature(type, "numeric");
    }

    static bool isContainer(const ObjectType* type)
    {
        return hasFeature(type, "container");
    }

    static bool isSequence(const ObjectType* type)
    {
        return hasFeature(type, "sequence");
    }

    static bool isMapping(const ObjectType* type)
    {
        return hasFeature(type, "mapping");
    }

    static bool isMutable(const ObjectType* type)
    {
        return hasFeature(type, "mutable");
    }

    static bool isReference(const ObjectType* type)
    {
        return hasFeature(type, "reference");
    }

    // 注册所有内置特性检查
    static void registerBuiltinFeatureChecks();
    

private:
    // 特性检查函数注册表 - 使用静态局部变量确保线程安全初始化
    static std::unordered_map<std::string, FeatureCheckFunc>& getFeatureChecks()
    {
        static std::unordered_map<std::string, FeatureCheckFunc> checkers;
        return checkers;
    }

    // 私有构造函数，防止实例化
    TypeFeatureChecker() = default;
};

}  // namespace llvmpy

#endif  // OBJECT_TYPE_H