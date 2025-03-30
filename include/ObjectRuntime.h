#ifndef OBJECT_RUNTIME_H
#define OBJECT_RUNTIME_H

#include "ObjectType.h"
#include "TypeOperations.h"
#include "ObjectLifecycle.h"
#include "TypeIDs.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>

namespace llvmpy
{



// 运行时类型系统管理器
class ObjectRuntime
{
public:
    ObjectRuntime(llvm::Module* mod, llvm::IRBuilder<>* b);

    // 初始化运行时
    void initialize();
    std::unique_ptr<PyCodeGen> asPyCodeGen();
    //===----------------------------------------------------------------------===//
    // 对象创建操作
    //===----------------------------------------------------------------------===//

    // 创建基本类型对象
    llvm::Value* createIntObject(llvm::Value* value);
    llvm::Value* createDoubleObject(llvm::Value* value);
    llvm::Value* createBoolObject(llvm::Value* value);
    llvm::Value* createStringObject(llvm::Value* value);

    // 创建容器类型对象
    llvm::Value* createList(llvm::Value* size, ObjectType* elemType);
    llvm::Value* createListWithValues(std::vector<llvm::Value*> values, ObjectType* elemType);
    llvm::Value* createDict(ObjectType* keyType, ObjectType* valueType);

    // 创建通用对象 - 根据类型自动选择正确的创建方法
    llvm::Value* createObject(llvm::Value* value, ObjectType* type);

    //===----------------------------------------------------------------------===//
    // 容器操作
    //===----------------------------------------------------------------------===//

    // 列表操作
    llvm::Value* getListElement(llvm::Value* list, llvm::Value* index);
    void setListElement(llvm::Value* list, llvm::Value* index, llvm::Value* value);
    llvm::Value* getListLength(llvm::Value* list);
    llvm::Value* appendToList(llvm::Value* list, llvm::Value* value);

    // 字典操作
    llvm::Value* getDictItem(llvm::Value* dict, llvm::Value* key);
    void setDictItem(llvm::Value* dict, llvm::Value* key, llvm::Value* value);
    llvm::Value* getDictKeys(llvm::Value* dict);

    //===----------------------------------------------------------------------===//
    // 类型操作
    //===----------------------------------------------------------------------===//

    // 类型转换
    llvm::Value* convertValue(llvm::Value* value, ObjectType* fromType, ObjectType* toType);

    // 二元操作
    llvm::Value* performBinaryOp(char op, llvm::Value* left, llvm::Value* right,
                                 ObjectType* leftType, ObjectType* rightType);

    // 一元操作
    llvm::Value* performUnaryOp(char op, llvm::Value* operand, ObjectType* type);

    // 类型检查
    llvm::Value* checkType(llvm::Value* obj, ObjectType* expectedType);
    llvm::Value* isInstance(llvm::Value* obj, int typeId);

    //===----------------------------------------------------------------------===//
    // 对象生命周期管理
    //===----------------------------------------------------------------------===//

    // 引用计数操作
    llvm::Value* incRef(llvm::Value* obj);
    llvm::Value* decRef(llvm::Value* obj);

    // 对象复制
    llvm::Value* copyObject(llvm::Value* obj, ObjectType* type);

    // 对象跟踪与清理
    void trackObject(llvm::Value* obj);
    void clearTrackedObjects();
    void setupCleanupForFunction();

    // 函数返回值准备
    llvm::Value* prepareReturnValue(llvm::Value* value, ObjectType* type);

    // 参数准备
    llvm::Value* prepareArgument(llvm::Value* value, ObjectType* fromType, ObjectType* paramType);

    //===----------------------------------------------------------------------===//
    // 运行时类型信息
    //===----------------------------------------------------------------------===//

    // 获取LLVM类型表示
    llvm::StructType* getPyObjectStructType();
    llvm::StructType* getListStructType(ObjectType* elemType);
    llvm::StructType* getDictStructType();

    // 类型ID映射
    int mapTypeIdToRuntime(const ObjectType* type);
    llvm::Value* getTypeIdFromObject(llvm::Value* obj);

    // 获取运行时类型名称
    std::string getTypeNameForId(int typeId);

    // 获取代码生成相关对象
    llvm::Module* getModule()
    {
        return module;
    }
    llvm::IRBuilder<>* getBuilder()
    {
        return builder;
    }
    llvm::LLVMContext& getContext()
    {
        return context;
    }

private:
    llvm::Module* module;
    llvm::IRBuilder<>* builder;
    llvm::LLVMContext& context;

    // 操作代码生成器
    std::unique_ptr<OperationCodeGenerator> opCodeGen;

    // 操作结果处理器
    std::unique_ptr<OperationResultHandler> resultHandler;

    // 对象生命周期管理器
    std::unique_ptr<ObjectLifecycleManager> lifecycleManager;

    // 运行时函数缓存
    std::unordered_map<std::string, llvm::Function*> runtimeFuncs;

    // 运行时类型缓存
    std::unordered_map<std::string, llvm::StructType*> runtimeTypes;

    // 对象追踪 - 用于自动清理临时对象
    std::vector<llvm::Value*> trackedObjects;

    // 运行时函数管理
    llvm::Function* getRuntimeFunction(const std::string& name,
                                       llvm::Type* returnType,
                                       std::vector<llvm::Type*> argTypes);

    // 生成类型检查代码
    llvm::Value* generateTypeCheck(llvm::Value* obj, int expectedTypeId);

    // 确保对象为PyObject指针类型
    llvm::Value* ensurePyObjectPtr(llvm::Value* value);

    // 初始化方法
    void createRuntimeTypes();
    void declareRuntimeFunctions();
    void registerTypeOperations();
};

}  // namespace llvmpy

#endif  // OBJECT_RUNTIME_H