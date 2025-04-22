#include "CodeGen/CodeGenBase.h"
#include "CodeGen/CodeGenExpr.h"
#include "CodeGen/CodeGenStmt.h"
#include "CodeGen/CodeGenModule.h"
#include "CodeGen/CodeGenType.h"
#include "CodeGen/CodeGenRuntime.h"
#include "CodeGen/CodeGenUtil.h"
#include "TypeOperations.h"
#include "ObjectLifecycle.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Casting.h> // For llvm::dyn_cast
#include <llvm/Support/raw_ostream.h>

#include <iostream>
#include <sstream>

namespace llvmpy
{

//===----------------------------------------------------------------------===//
// PyScope 实现
//===----------------------------------------------------------------------===//
// --- PyScope 函数 AST 方法实现 ---
void PyScope::defineFunctionAST(const std::string& name, const FunctionAST* ast)
{
    // 允许覆盖，以便处理内部函数覆盖外部同名函数的情况
    functionDefinitions[name] = ast;
    #ifdef DEBUG_SYMBOL_TABLE // 假设有这样的宏
    std::cerr << "Debug [PyScope]: Defined FunctionAST '" << name << "' in this scope." << std::endl;
    #endif
}



const FunctionAST* PyScope::findFunctionAST(const std::string& name) const
{
    // 在当前作用域查找
    auto it = functionDefinitions.find(name);
    if (it != functionDefinitions.end())
    {
        return it->second;
    }
    // 在父作用域中递归查找
    return parent ? parent->findFunctionAST(name) : nullptr;
}

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
// --- PySymbolTable 函数 AST 方法实现 ---
// (这些方法直接委托给当前的 PyScope)

/**
 * @brief 在当前作用域定义一个函数 AST。
 */
 void PySymbolTable::defineFunctionAST(const std::string& name, const FunctionAST* ast)
 {
     if (!scopes.empty())
     {
         scopes.top()->defineFunctionAST(name, ast);
     }
     else
     {
         // 应该总是有作用域，记录错误
         std::cerr << "Error: Cannot define function AST '" << name << "': No active scope in symbol table." << std::endl;
     }
 }
 
 /**
  * @brief 从当前作用域开始向上查找函数 AST 定义。
  */
 const FunctionAST* PySymbolTable::findFunctionAST(const std::string& name) const
 {
     return scopes.empty() ? nullptr : scopes.top()->findFunctionAST(name);
 }
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
size_t PySymbolTable::getCurrentScopeDepth() const
{
    return scopes.size(); // 直接返回栈的大小即可
}
void PySymbolTable::popScope()
{
    // 删除当前作用域
    if (!scopes.empty())
    {
        // 在弹出作用域前，可以添加逻辑来处理作用域内变量的生命周期（例如 decref）
        // PyScope* scopeToPop = scopes.top().get();
        // for (auto const& [name, val] : scopeToPop->getVariables()) {
        //     ObjectType* type = scopeToPop->getVariableType(name);
        //     if (type && type->isReference()) {
        //         // 需要 CodeGenRuntime 的引用来调用 decref
        //         // codeGen.getRuntimeGen()->decRef(val);
        //     }
        // }
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


// 新增：logWarning 实现
void CodeGenBase::logWarning(const std::string& message, int line, int column)
{
    // 格式化警告信息并输出到 cerr
    std::stringstream ss;
    ss << "Warning";
    if (line > 0) {
        ss << " at line " << line;
        if (column > 0) {
            ss << ", column " << column;
        }
    }
    ss << ": " << message;
    std::cerr << ss.str() << std::endl;
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

llvm::Function* CodeGenBase::getOrCreateFunction(
    const std::string& name,
    llvm::FunctionType* funcType,
    llvm::GlobalValue::LinkageTypes linkage,
    const llvm::AttributeList& attributes)
{
    // 尝试获取或插入函数
    // getOrInsertFunction 返回一个 Constant*，需要转换
    llvm::FunctionCallee funcCallee = module->getOrInsertFunction(name, funcType, attributes);

    // 尝试将 Constant* 转换为 Function*
    llvm::Function* func = llvm::dyn_cast<llvm::Function>(funcCallee.getCallee());

    if (!func) {
        // 如果转换失败，可能是因为已存在同名但类型不兼容的全局对象 (例如全局变量)
        logError("Failed to get or create function '" + name + "'. A global object with the same name but incompatible type might exist.", 0, 0);
        return nullptr;
    }

    // 检查返回的函数类型是否与请求的类型完全匹配
    // getOrInsertFunction 会处理类型转换，但最好还是显式检查以防万一
    if (func->getFunctionType() != funcType) {
        // 这通常发生在函数已存在但签名不匹配的情况下
        logError("Function '" + name + "' already exists with a different signature. Requested: " + llvmObjToString(funcType) + ", Found: " + llvmObjToString(func->getFunctionType()), 0, 0);
        // 打印现有函数的 IR 以帮助调试
        // func->print(llvm::errs());
        return nullptr; // 返回 nullptr 表示失败
    }

    // 如果函数是新创建的，设置链接类型 (getOrInsertFunction 默认使用 ExternalLinkage)
    if (func->empty()) { // 新创建的函数没有基本块
        func->setLinkage(linkage);
        // 可以设置其他属性，如 UnnamedAddr 等
        // func->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    }
    // 如果函数已存在，我们通常不应该更改其链接类型或属性

    return func;
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