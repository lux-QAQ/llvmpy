#ifndef CODEGEN_RUNTIME_H
#define CODEGEN_RUNTIME_H

#include "ObjectLifecycle.h"
#include "CodeGen/CodeGenBase.h"

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
    llvm::Value* createList(llvm::Value* initialCapacity, llvm::Value* elemTypeIdValue);
    llvm::Value* createIntObjectFromString(llvm::Value* strPtr);
    llvm::Value* createDoubleObjectFromString(llvm::Value* strPtr);
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

    /**
     * @brief 生成对运行时函数 py_call_function 的调用。
     *
     * @param callableObj 代表 Python 可调用对象 (PyObject*) 的 LLVM 值。
     * @param preparedArgs 代表已准备好的参数 (PyObject*) 的 LLVM 值列表。
     * @return llvm::Value* 表示调用返回的 Python 对象 (PyObject*)。
     */
    llvm::Value* createCallFunction(
            llvm::Value* callableObj,
            const std::vector<llvm::Value*>& preparedArgs);
    /**
     * @brief 生成了对运行时函数py_call_function_noargs的调用。
     *代表Python函数对象（PyObject*）的@param funcobj llvm值。
     * @return llvm值表示返回的Python对象（PyObject*）。
     */
    llvm::Value* createCallFunctionNoArgs(llvm::Value* funcObj);

    /**
     * @brief生成对运行时函数py_object_to_exit_code的调用。
     *代表Python对象（PyObject*）的@param PyoBj llvm值。
     * @return llvm值表示整数退出代码（i32）。
     */
    llvm::Value* createObjectToExitCode(llvm::Value* pyObj);

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

    /**
     * @brief 创建一个 Python 函数对象，包装给定的 LLVM 函数。
     * @param llvmFunc 指向已编译的 LLVM 函数的指针。
     * @param funcObjectType 代表此函数 Python 签名的 ObjectType* (应为 FunctionType*)。
     * @return 指向新创建的 Python 函数对象 (PyObject*) 的 llvm::Value*。
     */
    llvm::Value* createFunctionObject(llvm::Function* llvmFunc, ObjectType* funcObjectType);

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

    // Added methods
    llvm::Type* getPyObjectPtrType();
    // void callDecRef(llvm::Value* obj); // Removed, use decRef
    void callRuntimeError(const std::string& errorType, int line);
};

}  // namespace llvmpy

#endif  // CODEGEN_RUNTIME_H
