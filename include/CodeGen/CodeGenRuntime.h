#ifndef CODEGEN_RUNTIME_H
#define CODEGEN_RUNTIME_H

#include "CodeGen/CodeGenBase.h"

#include "ObjectRuntime.h"
#include <llvm/IR/Value.h>
#include <memory>
#include <string>
#include <vector>

namespace llvmpy
{

// 运行时代码生成组件
class CodeGenRuntime
{
private:
    CodeGenBase& codeGen;
    ObjectRuntime* runtime;  // 已有的运行时接口对象 (可能为空)

    // 运行时函数缓存
    std::unordered_map<std::string, llvm::Function*> runtimeFunctions;

    // 对象生命周期管理映射
    std::map<llvm::Value*, ObjectLifecycleManager::ObjectSource> objectSources;

public:
    CodeGenRuntime(CodeGenBase& cg, ObjectRuntime* rt = nullptr)
        : codeGen(cg), runtime(rt)
    {
    }

    // 运行时函数管理
    llvm::Function* getRuntimeFunction(
            const std::string& name,
            llvm::Type* returnType,
            std::vector<llvm::Type*> paramTypes);
    // 临时对象跟踪
    void trackTempObject(llvm::Value* obj);
    // 对象生命周期管理
    void markObjectSource(
            llvm::Value* obj,
            ObjectLifecycleManager::ObjectSource source);

    ObjectLifecycleManager::ObjectSource getObjectSource(llvm::Value* obj);

    ObjectLifecycleManager::ObjectSource determineExprSource(const ExprAST* expr);

    // 对象操作
    llvm::Value* incRef(llvm::Value* obj);
    llvm::Value* decRef(llvm::Value* obj);
    llvm::Value* copyObject(llvm::Value* obj, std::shared_ptr<PyType> type);
    llvm::Value* createIntObject(llvm::Value* value);
    llvm::Value* createDoubleObject(llvm::Value* value);
    llvm::Value* createBoolObject(llvm::Value* value);
    llvm::Value* createStringObject(llvm::Value* value);
    llvm::Value* createList(llvm::Value* size, ObjectType* elemType);
    llvm::Value* getListElement(llvm::Value* list, llvm::Value* index);
    void setListElement(llvm::Value* list, llvm::Value* index, llvm::Value* value);

    // 字典操作代理
    llvm::Value* createDict(ObjectType* keyType, ObjectType* valueType);
    llvm::Value* getDictItem(llvm::Value* dict, llvm::Value* key);
    void setDictItem(llvm::Value* dict, llvm::Value* key, llvm::Value* value);

    // 准备返回值
    llvm::Value* prepareReturnValue(
            llvm::Value* value,
            std::shared_ptr<PyType> valueType,
            std::shared_ptr<PyType> returnType);

    // 准备参数
    llvm::Value* prepareArgument(
            llvm::Value* value,
            std::shared_ptr<PyType> valueType,
            std::shared_ptr<PyType> paramType);

    // 准备赋值目标
    llvm::Value* prepareAssignmentTarget(
            llvm::Value* value,
            std::shared_ptr<PyType> targetType,
            std::shared_ptr<PyType> valueType);

    // 清理临时对象
    void cleanupTemporaryObjects();
    void releaseTrackedObjects();

    // 管理对象元数据
    void attachTypeMetadata(llvm::Value* value, int typeId);
    int getTypeIdFromMetadata(llvm::Value* value);

    // 判断对象是否需要特殊处理
    bool needsLifecycleManagement(std::shared_ptr<PyType> type);
    bool isTemporaryObject(llvm::Value* value);

    // 访问外部运行时
    ObjectRuntime* getRuntime()
    {
        return runtime;
    }

    // 辅助方法
    CodeGenBase& getCodeGen()
    {
        return codeGen;
    }
};

}  // namespace llvmpy

#endif  // CODEGEN_RUNTIME_H