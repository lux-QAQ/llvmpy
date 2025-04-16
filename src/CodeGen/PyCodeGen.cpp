#include "CodeGen/PyCodeGen.h"
#include "CodeGen/CodeGenBase.h"
#include "CodeGen/CodeGenExpr.h"
#include "CodeGen/CodeGenStmt.h"
#include "CodeGen/CodeGenModule.h"
#include "CodeGen/CodeGenType.h"
#include "CodeGen/CodeGenRuntime.h"

#include "TypeOperations.h"
#include "ObjectLifecycle.h"
#include "TypeIDs.h"
#include <iostream>
#include <fstream>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

namespace llvmpy
{

//===----------------------------------------------------------------------===//
// 构造函数和析构函数
//===----------------------------------------------------------------------===//

// 修改构造函数，正确利用继承关系
PyCodeGen::PyCodeGen()
    : CodeGenBase(),  // 直接初始化基类
      ownsLLVMObjects(true)
{
    // 初始化所有组件
    initializeComponents();  // 直接调用继承的方法

    // 确保类型操作注册表已初始化
    TypeOperationRegistry::getInstance();
}

PyCodeGen::PyCodeGen(llvm::Module* mod, llvm::IRBuilder<>* b, llvm::LLVMContext* ctx, ObjectRuntime* rt)
    : CodeGenBase(mod, b, ctx, rt),  // 使用基类构造函数
      ownsLLVMObjects(false)
{
    // 初始化所有组件
    initializeComponents();  // 直接调用继承的方法

    // 确保类型操作注册表已初始化
    TypeOperationRegistry::getInstance();
}

PyCodeGen::~PyCodeGen()
{
    // 析构函数的工作主要由成员的析构函数完成
    // 如果拥有LLVM对象，则需要在此处释放它们
    // 否则它们由外部所有者管理
}

//===----------------------------------------------------------------------===//
// 代码生成方法
//===----------------------------------------------------------------------===//

/* llvm::Value* PyCodeGen::codegen(ASTNode* node)
{
    if (!node)
    {
        return nullptr;
    }

    // 使用访问者模式处理AST节点
    CodeGenVisitor visitor(*this);
    visitor.visit(node);

    // 返回最后生成的表达式值
    return getLastExprValue();
} */

llvm::Value* PyCodeGen::codegenExpr(const ExprAST* expr)
{
    if (!expr)
    {
        return nullptr;
    }

    // 使用表达式代码生成器处理表达式
    return getExprGen()->handleExpr(expr);
}

void PyCodeGen::codegenStmt(StmtAST* stmt)
{
    if (!stmt)
    {
        return;
    }

    // 使用语句代码生成器处理语句
    getStmtGen()->handleStmt(stmt);
}

bool PyCodeGen::generateModule(ModuleAST* module, const std::string& filename)
{
    if (!module)
    {
        return false;
    }

    // 防御性检查，确保关键指针已初始化
    if (!context)
    {
        std::cerr << "错误: LLVM Context 未初始化" << std::endl;
        return false;
    }

    if (!this->module)
    {
        std::cerr << "错误: LLVM Module 未初始化" << std::endl;
        return false;
    }

    if (!builder)
    {
        std::cerr << "错误: LLVM Builder 未初始化" << std::endl;
        return false;
    }

    // 确保 moduleGen 已创建
    auto modGen = getModuleGen();
    if (!modGen)
    {
        std::cerr << "错误: 模块生成器未初始化" << std::endl;
        initializeComponents();  // 尝试再次初始化组件
        modGen = getModuleGen();
        if (!modGen)
        {
            std::cerr << "错误: 无法初始化模块生成器" << std::endl;
            return false;
        }
    }

    // 传递当前模块
    modGen->setCurrentModule(module);

    // --- 决定是否是入口点 ---
    // 这里简单地假设如果 filename 是 "output.ll" 或默认，则为主入口点
    // 更健壮的方式是让调用者 (如 main.cpp) 明确指定
    bool isEntryPoint = (filename == "output.ll" || filename.empty());

    // TODO ： 这里将是多文件编译的修改关键后续需要完善整个机制

    // 使用模块代码生成器生成整个模块，传入 isEntryPoint 标志
    bool success = modGen->generateModule(module, isEntryPoint);

    // 如果生成成功且指定了文件名，则保存模块到文件
    if (success && !filename.empty())
    {
        std::error_code EC;
        llvm::raw_fd_ostream os(filename, EC);
        if (!EC)
        {
            getModule()->print(os, nullptr);
            return true;
        }
    }

    return success;
}

//===----------------------------------------------------------------------===//
// 组件访问方法
//===----------------------------------------------------------------------===//

CodeGenExpr* PyCodeGen::getExprGen() const
{
    return CodeGenBase::getExprGen();
}

CodeGenStmt* PyCodeGen::getStmtGen() const
{
    return CodeGenBase::getStmtGen();
}

CodeGenModule* PyCodeGen::getModuleGen() const
{
    return CodeGenBase::getModuleGen();
}

CodeGenType* PyCodeGen::getTypeGen() const
{
    return CodeGenBase::getTypeGen();
}

CodeGenRuntime* PyCodeGen::getRuntimeGen() const
{
    return CodeGenBase::getRuntimeGen();
}

//===----------------------------------------------------------------------===//
// 简化访问方法
//===----------------------------------------------------------------------===//

llvm::LLVMContext& PyCodeGen::getContext()
{
    return CodeGenBase::getContext();
}

llvm::Module* PyCodeGen::getModule()
{
    return CodeGenBase::getModule();
}

llvm::IRBuilder<>& PyCodeGen::getBuilder()
{
    return CodeGenBase::getBuilder();
}

//===----------------------------------------------------------------------===//
// 状态查询方法
//===----------------------------------------------------------------------===//

llvm::Function* PyCodeGen::getCurrentFunction()
{
    return CodeGenBase::getCurrentFunction();
}

ObjectType* PyCodeGen::getCurrentReturnType()
{
    return CodeGenBase::getCurrentReturnType();
}

//===----------------------------------------------------------------------===//
// 表达式结果方法
//===----------------------------------------------------------------------===//
// 在适当位置添加方法实现
llvm::Value* PyCodeGen::prepareAssignmentTarget(llvm::Value* value, ObjectType* targetType, const ExprAST* expr)
{
    if (!value || !targetType || !expr) return value;

    // 获取表达式来源
    ObjectLifecycleManager::ObjectSource source =
            ObjectLifecycleManager::determineExprSource(expr);

    // 检查是否需要复制对象
    if (ObjectLifecycleManager::needsCopy(targetType, source,
                                          ObjectLifecycleManager::ObjectDestination::ASSIGNMENT))
    {
        // 复制对象
        llvm::Function* copyFunc = getOrCreateExternalFunction(
                "py_object_copy",
                llvm::PointerType::get(getContext(), 0),
                {llvm::PointerType::get(getContext(), 0),
                 llvm::Type::getInt32Ty(getContext())});

        // 获取对象类型ID
        int typeId = ObjectLifecycleManager::getObjectTypeId(targetType);
        llvm::Value* typeIdValue = llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(getContext()), typeId);

        return getBuilder().CreateCall(copyFunc, {value, typeIdValue}, "copy");
    }
    else if (ObjectLifecycleManager::needsIncRef(targetType, source,
                                                 ObjectLifecycleManager::ObjectDestination::ASSIGNMENT))
    {
        // 增加引用计数
        llvm::Function* incRefFunc = getOrCreateExternalFunction(
                "py_incref",
                llvm::Type::getVoidTy(getContext()),
                {llvm::PointerType::get(getContext(), 0)});

        getBuilder().CreateCall(incRefFunc, {value});
    }

    return value;
}
/* llvm::Value* PyCodeGen::getLastExprValue()
{
    return CodeGenBase::getLastExprValue();
} */

std::shared_ptr<PyType> PyCodeGen::getLastExprType()
{
    return CodeGenBase::getLastExprType();
}

//===----------------------------------------------------------------------===//
// 错误处理
//===----------------------------------------------------------------------===//

llvm::Value* PyCodeGen::logError(const std::string& message, int line, int column)
{
    return CodeGenBase::logError(message, line, column);  // 使用基类方法
}

//===----------------------------------------------------------------------===//
// 兼容旧版接口
//===----------------------------------------------------------------------===//



// 修改后:
llvm::Value* PyCodeGen::handleExpr(const ExprAST* expr)
{
    return codegenExpr(expr);
}

void PyCodeGen::handleStmt(StmtAST* stmt)
{
    codegenStmt(stmt);
}

llvm::Value* PyCodeGen::handleBinOp(char op, llvm::Value* L, llvm::Value* R,
                                    ObjectType* leftType, ObjectType* rightType)
{
    // 将ObjectType转换为PyType进行处理
    std::shared_ptr<PyType> leftPyType = std::make_shared<PyType>(leftType);
    std::shared_ptr<PyType> rightPyType = std::make_shared<PyType>(rightType);

    // 利用TypeOperations自动处理二元操作类型推导
    return getExprGen()->handleBinOp(op, L, R, leftPyType, rightPyType);
}

}  // namespace llvmpy