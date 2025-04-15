#ifndef CODEGEN_MODULE_H
#define CODEGEN_MODULE_H

#include "CodeGen/CodeGenBase.h"
#include "ast.h"
#include <unordered_map>
#include <string>
#include <vector>

namespace llvmpy
{

// 函数定义信息
struct FunctionDefInfo
{
    std::string name;
    llvm::Function* function;
    ObjectType* returnType;
    std::vector<ObjectType*> paramTypes;
    bool isExternal;

    FunctionDefInfo()
        : function(nullptr), returnType(nullptr), isExternal(false)
    {
    }
};

// 模块代码生成组件
class CodeGenModule
{
private:
    CodeGenBase& codeGen;

    // 函数定义缓存
    std::unordered_map<std::string, FunctionDefInfo> functionDefs;

    // 模块初始化标志
    bool moduleInitialized;

    // 创建模块初始化函数
    llvm::Function* createModuleInitFunction();

    // 添加运行时库函数引用
    void addRuntimeFunctions();
    // 运行时库初始化函数
    void createAndRegisterRuntimeInitializer();

    ModuleAST* currentModule;  // 添加成员变量

public:
    CodeGenModule(CodeGenBase& cg)
        : codeGen(cg), moduleInitialized(false), currentModule(nullptr)
    {
    }

    // 生成完整模块
    bool generateModule(ModuleAST* module);

    bool generateModule(ModuleAST* module, bool isEntryPoint = true); // 默认为 true 以保持单文件行为

  

    // 处理函数定义
    llvm::Function* handleFunctionDef(FunctionAST* funcAST);

    // 为函数创建类型
    llvm::FunctionType* createFunctionType(
            std::shared_ptr<PyType> returnType,
            const std::vector<std::shared_ptr<PyType>>& paramTypes);

    // 处理函数参数
    void handleFunctionParams(llvm::Function* function,
                              const std::vector<ParamAST>& params,
                              std::vector<std::shared_ptr<PyType>>& paramTypes);

    // 设置函数返回类型
    std::shared_ptr<PyType> resolveReturnType(FunctionAST* funcAST);

    // 创建函数入口基本块
    llvm::BasicBlock* createFunctionEntry(llvm::Function* function);

    // 处理函数返回
    void handleFunctionReturn(llvm::Value* returnValue,
                              std::shared_ptr<PyType> returnType);
    ModuleAST* getCurrentModule() const
    {
            // 添加空指针检查
    if (!currentModule) {
        // 可以选择返回nullptr或创建一个空模块
        static ModuleAST emptyModule;
        return &emptyModule; // 或者 return nullptr;
    }
    return currentModule;
    }
    // 清理函数资源 (临时对象等)
    void cleanupFunction();

    // 添加函数引用
    void addFunctionReference(const std::string& name, llvm::Function* function,
                              ObjectType* returnType,
                              const std::vector<ObjectType*>& paramTypes,
                              bool isExternal = false);

    // 查找函数引用
    FunctionDefInfo* getFunctionInfo(const std::string& name);

    // 辅助方法
    CodeGenBase& getCodeGen()
    {
        return codeGen;
    }



    // 设置当前模块
    void setCurrentModule(ModuleAST* module)
    {
        currentModule = module;
    }
};

}  // namespace llvmpy

#endif  // CODEGEN_MODULE_H