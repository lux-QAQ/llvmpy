#ifndef OBJECT_RUNTIME_H
#define OBJECT_RUNTIME_H

#include "ObjectType.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <unordered_map>
#include <string>
#include <vector>

namespace llvmpy
{

// 运行时类型系统管理器
class ObjectRuntime
{
public:
ObjectRuntime(llvm::Module* mod, llvm::IRBuilder<>* b)
    : module(mod), builder(b), context(mod->getContext()) {
    // 初始化运行时
    initialize();
    
    // 不再需要单独调用registerFeatureChecks
    // 特性检查已在全局初始化
}
    

    // 初始化运行时
    void initialize();

    // 添加字典创建方法
    llvm::Value* createDict(ObjectType* keyType, ObjectType* valueType);
    // 添加向列表追加元素方法
    llvm::Value* appendToList(llvm::Value* list, llvm::Value* value);
    // 添加对象类型转换方法

    // 获取列表追加函数
    // 获取字典创建函数
    // 对象创建函数
    llvm::Value* createList(llvm::Value* size, ObjectType* elemType);
    llvm::Value* createListWithValues(std::vector<llvm::Value*> values,
                                      ObjectType* elemType);

    void registerFeatureChecks();
    void trackObject(llvm::Value* obj);


    // 列表操作
    llvm::Value* getListElement(llvm::Value* list, llvm::Value* index);
    void setListElement(llvm::Value* list, llvm::Value* index, llvm::Value* value);
    llvm::Value* getListLength(llvm::Value* list);


    // 字典操作
    llvm::Value* getDictItem(llvm::Value* dict, llvm::Value* key);
    void setDictItem(llvm::Value* dict, llvm::Value* key, llvm::Value* value);
    llvm::Value* getDictKeys(llvm::Value* dict);

    // 对象操作
    llvm::Value* copyObject(llvm::Value* obj, ObjectType* type);
    llvm::Value* convertObject(llvm::Value* obj,
                               ObjectType* fromType,
                               ObjectType* toType);

    // 引用计数管理
    llvm::Value* incRef(llvm::Value* obj);
    llvm::Value* decRef(llvm::Value* obj);

    // 类型检查
    llvm::Value* checkType(llvm::Value* obj, ObjectType* expectedType);

    // 处理函数返回值
    llvm::Value* prepareReturnValue(llvm::Value* value, ObjectType* type);
    void setupCleanupForFunction();

    // 获取运行时类型信息
    llvm::StructType* getListStructType(ObjectType* elemType);
    llvm::StructType* getDictStructType();
    llvm::StructType* getPyObjectStructType();

    int mapTypeIdToRuntime(const ObjectType* type);

private:
    llvm::Module* module;
    llvm::IRBuilder<>* builder;
    llvm::LLVMContext& context;

    // 运行时函数缓存
    std::unordered_map<std::string, llvm::Function*> runtimeFuncs;

    // 运行时类型缓存
    std::unordered_map<std::string, llvm::StructType*> runtimeTypes;

    // 对象追踪 - 用于自动清理临时对象
    std::vector<llvm::Value*> trackedObjects;

    // 帮助函数
    llvm::Function* getRuntimeFunction(const std::string& name,
                                       llvm::Type* returnType,
                                       std::vector<llvm::Type*> argTypes);

    // 创建运行时类型
    void createRuntimeTypes();

    // 创建运行时工具函数
    void declareRuntimeFunctions();

    // 实现运行时工具函数
    void implementRuntimeFunctions();
};

}  // namespace llvmpy

#endif  // OBJECT_RUNTIME_H