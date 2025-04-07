#include "CodeGen/CodeGenBase.h"
#include "CodeGen/CodeGenExpr.h"
#include "CodeGen/CodeGenStmt.h"
#include "CodeGen/CodeGenModule.h"
#include "CodeGen/CodeGenType.h"
#include "CodeGen/CodeGenRuntime.h"
#include "TypeOperations.h"
#include "ObjectLifecycle.h"
#include <iostream>
#include <sstream>

namespace llvmpy
{

//===----------------------------------------------------------------------===//
// PyScope 实现
//===----------------------------------------------------------------------===//

bool PyScope::hasVariable(const std::string& name) const
{
    // 在当前作用域查找变量
    if (variables.find(name) != variables.end())
    {
        return true;
    }

    // 在父作用域中递归查找
    return parent ? parent->hasVariable(name) : false;
}

llvm::Value* PyScope::getVariable(const std::string& name)
{
    // 在当前作用域查找变量
    auto it = variables.find(name);
    if (it != variables.end())
    {
        return it->second;
    }

    // 在父作用域中递归查找
    return parent ? parent->getVariable(name) : nullptr;
}

void PyScope::setVariable(const std::string& name, llvm::Value* value, ObjectType* type)
{
    // 在当前作用域设置变量
    variables[name] = value;

    // 同时存储类型信息（如果提供）
    if (type)
    {
        variableTypes[name] = type;
    }
}

ObjectType* PyScope::getVariableType(const std::string& name)
{
    // 在当前作用域查找变量类型
    auto it = variableTypes.find(name);
    if (it != variableTypes.end())
    {
        return it->second;
    }

    // 在父作用域中递归查找
    return parent ? parent->getVariableType(name) : nullptr;
}

//===----------------------------------------------------------------------===//
// PySymbolTable 实现
//===----------------------------------------------------------------------===//

PyScope* PySymbolTable::currentScope()
{
    // 确保至少有一个作用域
    if (scopes.empty())
    {
        pushScope();
    }
    return scopes.top().get();
}

void PySymbolTable::pushScope()
{
    // 创建新作用域，与父作用域关联
    PyScope* parent = scopes.empty() ? nullptr : scopes.top().get();
    scopes.push(std::make_unique<PyScope>(parent));
}

void PySymbolTable::popScope()
{
    // 删除当前作用域
    if (!scopes.empty())
    {
        scopes.pop();
    }
}

bool PySymbolTable::hasVariable(const std::string& name) const
{
    // 检查变量是否存在于作用域链中
    return !scopes.empty() && scopes.top()->hasVariable(name);
}

llvm::Value* PySymbolTable::getVariable(const std::string& name)
{
    // 获取变量值
    return scopes.empty() ? nullptr : scopes.top()->getVariable(name);
}

void PySymbolTable::setVariable(const std::string& name, llvm::Value* value, ObjectType* type)
{
    // 设置变量及其类型
    if (!scopes.empty())
    {
        scopes.top()->setVariable(name, value, type);
    }
}

ObjectType* PySymbolTable::getVariableType(const std::string& name)
{
    // 获取变量类型
    return scopes.empty() ? nullptr : scopes.top()->getVariableType(name);
}

std::map<std::string, llvm::Value*> PySymbolTable::captureState() const
{
    std::map<std::string, llvm::Value*> state;

    // 使用递归方式访问作用域链
    if (!scopes.empty())
    {
        // 从栈顶开始遍历作用域链
        PyScope* current = scopes.top().get();
        while (current)
        {
            // 添加当前作用域的所有变量
            for (const auto& pair : current->getVariables())
            {
                state[pair.first] = pair.second;
            }

            // 移动到父作用域
            current = current->parent;
        }
    }

    return state;
}

// 使用策略模式更新变量
void PySymbolTable::updateVariable(
        CodeGenBase& codeGen,
        const std::string& name,
        llvm::Value* newValue,
        ObjectType* type,
        std::shared_ptr<PyType> valueType)
{
    llvm::Value* oldValue = getVariable(name);

    // 获取适合的更新策略
    auto strategy = VariableUpdateStrategyFactory::createStrategy(codeGen, name);

    // 使用策略更新变量
    llvm::Value* updatedValue = strategy->updateVariable(
            codeGen, name, oldValue, newValue, type, valueType);

    // 在符号表中设置更新后的值
    setVariable(name, updatedValue, type);
}

std::map<std::string, llvm::Value*> PySymbolTable::getModifiedVars(
        const std::map<std::string, llvm::Value*>& prevState) const
{
    std::map<std::string, llvm::Value*> result;

    // 使用递归方式访问作用域链
    if (!scopes.empty())
    {
        // 从栈顶开始遍历作用域链
        PyScope* current = scopes.top().get();
        while (current)
        {
            // 检查每个变量是否被修改过
            for (const auto& pair : current->getVariables())
            {
                auto it = prevState.find(pair.first);
                if (it == prevState.end() || it->second != pair.second)
                {
                    result[pair.first] = pair.second;
                }
            }

            // 移动到父作用域
            current = current->parent;
        }
    }

    return result;
}

//===----------------------------------------------------------------------===//
// PyCodeGenError 实现
//===----------------------------------------------------------------------===//

std::string PyCodeGenError::formatError() const
{
    std::stringstream ss;
    // 格式化错误信息，包括位置和类型
    ss << (isTypeError ? "TypeError" : "Error");

    if (line > 0)
    {
        ss << " at line " << line;
        if (column > 0)
        {
            ss << ", column " << column;
        }
    }

    ss << ": " << what();
    return ss.str();
}

//===----------------------------------------------------------------------===//
// CodeGenBase 实现
//===----------------------------------------------------------------------===//

CodeGenBase::CodeGenBase()
    : context(nullptr), module(nullptr), builder(nullptr), currentFunction(nullptr), currentReturnType(nullptr), inReturnStmt(false), savedBlock(nullptr), lastExprValue(nullptr), lastExprType(nullptr)
{
}

CodeGenBase::CodeGenBase(llvm::Module* mod, llvm::IRBuilder<>* b, llvm::LLVMContext* ctx, ObjectRuntime* rt)
    : context(ctx), module(mod), builder(b), currentFunction(nullptr), currentReturnType(nullptr), inReturnStmt(false), savedBlock(nullptr), lastExprValue(nullptr), lastExprType(nullptr)
{
    // 初始化各个组件
    initializeComponents();
}

CodeGenBase::~CodeGenBase()
{
    // 清理临时对象
    clearTempObjects();
}

void CodeGenBase::initializeComponents()
{


   

    // 如果LLVM对象未初始化，创建它们
    if (!context)
    {
        context = new llvm::LLVMContext();
    }

    if (!module && context)
    {
        module = new llvm::Module("llvmpy_module", *context);
    }

    if (!builder && context)
    {
        builder = new llvm::IRBuilder<>(*context);
    }

    // 初始化各个组件
    if (!exprGen) exprGen = std::make_unique<CodeGenExpr>(*this);
    if (!stmtGen) stmtGen = std::make_unique<CodeGenStmt>(*this);
    if (!moduleGen) moduleGen = std::make_unique<CodeGenModule>(*this);
    if (!typeGen) typeGen = std::make_unique<CodeGenType>(*this);
    if (!runtimeGen) runtimeGen = std::make_unique<CodeGenRuntime>(*this, nullptr);
}

llvm::Value* CodeGenBase::logError(const std::string& message, int line, int column)
{
    // 记录普通错误
    PyCodeGenError error(message, line, column);
    std::cerr << error.formatError() << std::endl;
    return nullptr;
}

llvm::Value* CodeGenBase::logTypeError(const std::string& message, int line, int column)
{
    // 记录类型错误
    PyCodeGenError error(message, line, column, true);
    std::cerr << error.formatError() << std::endl;
    return nullptr;
}

bool CodeGenBase::logValidationError(const std::string& message, int line, int column)
{
    // 记录验证错误，返回false以简化验证函数的实现
    PyCodeGenError error(message, line, column);
    std::cerr << error.formatError() << std::endl;
    return false;
}

void CodeGenBase::pushLoopBlocks(llvm::BasicBlock* condBlock, llvm::BasicBlock* afterBlock)
{
    // 将新循环块压入栈
    loopStack.push(LoopInfo(condBlock, afterBlock));
}

void CodeGenBase::popLoopBlocks()
{
    // 弹出最近的循环块
    if (!loopStack.empty())
    {
        loopStack.pop();
    }
}

LoopInfo* CodeGenBase::getCurrentLoop()
{
    // 获取当前循环信息
    if (loopStack.empty())
    {
        return nullptr;
    }
    return &loopStack.top();
}

llvm::BasicBlock* CodeGenBase::createBasicBlock(const std::string& name, llvm::Function* parent)
{
    // 创建一个新的基本块
    if (!parent)
    {
        parent = currentFunction;
    }

    if (!parent)
    {
        logError("No current function for basic block creation");
        return nullptr;
    }

    return llvm::BasicBlock::Create(*context, name, parent);
}

void CodeGenBase::addTempObject(llvm::Value* obj, ObjectType* type)
{
    // 跟踪临时对象用于后期清理
    if (obj)
    {
        tempObjects.push_back(obj);
    }
}

void CodeGenBase::releaseTempObjects()
{
    // 释放所有临时对象
    auto runtime = getRuntimeGen();
    if (!runtime)
    {
        return;
    }

    // 减少每个临时对象的引用计数
    for (auto obj : tempObjects)
    {
        if (obj)
        {
            runtime->decRef(obj);
        }
    }

    // 清空临时对象列表
    clearTempObjects();
}

void CodeGenBase::clearTempObjects()
{
    // 清空临时对象列表，不执行引用计数操作
    tempObjects.clear();
}

bool CodeGenBase::verifyModule()
{
    // 验证模块的正确性
    std::string errorInfo;
    llvm::raw_string_ostream errorStream(errorInfo);

    // 使用LLVM的验证器
    if (llvm::verifyModule(*module, &errorStream))
    {
        std::cerr << "Module verification failed: " << errorStream.str() << std::endl;
        return false;
    }

    return true;
}

llvm::Function* CodeGenBase::getOrCreateExternalFunction(
        const std::string& name,
        llvm::Type* returnType,
        std::vector<llvm::Type*> paramTypes,
        bool isVarArg)
{
    // 先查找是否已经存在此函数
    llvm::Function* func = module->getFunction(name);
    if (func)
    {
        return func;
    }

    // 创建函数类型
    llvm::FunctionType* funcType =
            llvm::FunctionType::get(returnType, paramTypes, isVarArg);

    // 创建函数声明
    func = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            name,
            module);

    return func;
}

}  // namespace llvmpy