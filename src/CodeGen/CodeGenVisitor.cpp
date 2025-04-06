#include "CodeGen/CodeGenVisitor.h"
#include "CodeGen/CodeGenBase.h"
#include "CodeGen/CodeGenType.h"
#include "CodeGen/CodeGenExpr.h"
#include "CodeGen/CodeGenModule.h"
#include "CodeGen/CodeGenRuntime.h"
#include "CodeGen/PyCodeGen.h"
#include "TypeOperations.h"
#include "ObjectLifecycle.h"
#include "TypeIDs.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <iostream>
#include <sstream>

namespace llvmpy {

//===----------------------------------------------------------------------===//
// 构造函数与初始化
//===----------------------------------------------------------------------===//

CodeGenVisitor::CodeGenVisitor(CodeGenBase& cg) 
    : codeGen(cg) {
}

//===----------------------------------------------------------------------===//
// 核心访问方法
//===----------------------------------------------------------------------===//

void CodeGenVisitor::visit(ModuleAST* node) {
    // 访问模块中的所有函数和语句
    for (auto& func : node->getFunctions()) {
        visit(func.get());
    }
    
    for (auto& stmt : node->getStatements()) {
        visit(stmt.get());
    }
}

void CodeGenVisitor::visit(FunctionAST* node) {
    auto* moduleGen = codeGen.getModuleGen();
    llvm::Function* function = moduleGen->handleFunctionDef(node);
    
    if (!function) {
        return;
    }
    
    // 设置函数为当前函数并处理函数体
    codeGen.setCurrentFunction(function);
    
    // 获取函数返回类型
    std::shared_ptr<PyType> returnType = moduleGen->resolveReturnType(node);
    if (returnType && returnType->getObjectType()) {
        codeGen.setCurrentReturnType(returnType->getObjectType());
    }
    
    // 创建入口基本块
    llvm::BasicBlock* entryBlock = moduleGen->createFunctionEntry(function);
    
    // 处理函数体中的语句
    for (auto& stmt : node->body) {
        visit(stmt.get());
    }
    
    // 确保函数有一个返回语句
    if (!entryBlock->getTerminator()) {
        // 对于非void函数，添加适当的返回值
        if (returnType && !returnType->isVoid()) {
            auto& builder = codeGen.getBuilder();
            auto* runtime = codeGen.getRuntimeGen();
            
            // 根据返回类型创建默认返回值
            llvm::Value* defaultReturn = nullptr;
            
            if (returnType->isInt()) {
                defaultReturn = llvm::ConstantInt::get(
                    llvm::Type::getInt32Ty(codeGen.getContext()), 0);
            } else if (returnType->isDouble()) {
                defaultReturn = llvm::ConstantFP::get(
                    llvm::Type::getDoubleTy(codeGen.getContext()), 0.0);
            } else if (returnType->isBool()) {
                defaultReturn = llvm::ConstantInt::get(
                    llvm::Type::getInt1Ty(codeGen.getContext()), false);
            } else {
                // 对于其他类型，返回None
                llvm::Function* getNoneFunc = codeGen.getOrCreateExternalFunction(
                    "py_get_none",
                    llvm::PointerType::get(codeGen.getContext(), 0),
                    {});
                defaultReturn = builder.CreateCall(getNoneFunc);
            }
            
            // 准备返回值并创建返回指令
            if (defaultReturn) {
                llvm::Value* preparedReturn = runtime->prepareReturnValue(
                    defaultReturn, 
                    std::make_shared<PyType>(returnType->getObjectType()),
                    std::make_shared<PyType>(codeGen.getCurrentReturnType()));
                builder.CreateRet(preparedReturn);
            }
        } else {
            auto& builder = codeGen.getBuilder();
            builder.CreateRetVoid();
        }
    }
    
    // 清理函数
    moduleGen->cleanupFunction();
}

void CodeGenVisitor::visit(ExprStmtAST* node) {
    if (!node->getExpr()) {
        return;
    }
    
    // 生成表达式代码
    auto* exprGen = codeGen.getExprGen();
    llvm::Value* exprValue = exprGen->handleExpr(node->getExpr());
    
    // 如果表达式产生了值，但没有被使用，可能需要进行清理
    if (exprValue) {
        auto* runtime = codeGen.getRuntimeGen();
        auto* typeGen = codeGen.getTypeGen();
        std::shared_ptr<PyType> exprType = typeGen->inferExprType(node->getExpr());
        
        // 对于引用类型的临时对象，可能需要减少引用计数
        if (exprType && exprType->isReference() && 
            runtime->isTemporaryObject(exprValue)) {
            runtime->decRef(exprValue);
        }
    }
}

void CodeGenVisitor::visit(ReturnStmtAST* node) {
    // 标记当前处于返回语句中
    codeGen.setInReturnStmt(true);
    
    auto& builder = codeGen.getBuilder();
    auto* runtime = codeGen.getRuntimeGen();
    auto* typeGen = codeGen.getTypeGen();
    
    // 获取函数返回类型
    ObjectType* returnType = codeGen.getCurrentReturnType();
    
    if (!node->getValue()) {
        // 没有返回值，创建void返回
        builder.CreateRetVoid();
        codeGen.setInReturnStmt(false);
        return;
    }
    
    // 生成返回值表达式代码
    auto* exprGen = codeGen.getExprGen();
    llvm::Value* returnValue = exprGen->handleExpr(node->getValue());
    
    if (!returnValue) {
        // 表达式计算失败
        codeGen.setInReturnStmt(false);
        return;
    }
    
    // 推导返回值表达式类型
    std::shared_ptr<PyType> exprType = typeGen->inferExprType(node->getValue());
    
    // 准备返回值 - 进行必要的类型转换和生命周期管理
    llvm::Value* preparedValue = runtime->prepareReturnValue(
        returnValue, 
        exprType,
        returnType ? std::make_shared<PyType>(returnType) : nullptr);
    
    // 创建返回指令
    builder.CreateRet(preparedValue);
    
    // 重置返回语句标记
    codeGen.setInReturnStmt(false);
}

void CodeGenVisitor::visit(IfStmtAST* node) {
    auto* stmtGen = codeGen.getStmtGen();
    
    // 创建条件表达式代码
    auto* exprGen = codeGen.getExprGen();
    llvm::Value* condValue = exprGen->handleExpr(node->getCondition());
    
    if (!condValue) {
        return;
    }
    
    // 获取当前函数
    llvm::Function* func = codeGen.getCurrentFunction();
    if (!func) {
        return;
    }
    
    // 创建必要的基本块
    auto& builder = codeGen.getBuilder();
    llvm::BasicBlock* thenBlock = llvm::BasicBlock::Create(codeGen.getContext(), "if.then", func);
    llvm::BasicBlock* elseBlock = nullptr;
    llvm::BasicBlock* afterBlock = llvm::BasicBlock::Create(codeGen.getContext(), "if.end");
    
    if (!node->getElseBody().empty()) {
        elseBlock = llvm::BasicBlock::Create(codeGen.getContext(), "if.else", func);
        afterBlock = llvm::BasicBlock::Create(codeGen.getContext(), "if.end", func);
        stmtGen->generateBranchingCode(condValue, thenBlock, elseBlock);
    } else {
        afterBlock = llvm::BasicBlock::Create(codeGen.getContext(), "if.end", func);
        stmtGen->generateBranchingCode(condValue, thenBlock, afterBlock);
    }
    
    // 生成then块代码
    builder.SetInsertPoint(thenBlock);
    stmtGen->beginScope();
    for (auto& stmt : node->getThenBody()) {
        visit(stmt.get());
    }
    stmtGen->endScope();
    
    // 如果then块没有终结器（如return），添加跳转到after块
    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateBr(afterBlock);
    }
    
        // 如果then块没有终结器（如return），添加跳转到after块
        if (!builder.GetInsertBlock()->getTerminator()) {
            builder.CreateBr(afterBlock);
        }
        
        // 生成else块代码（如果有）
        if (elseBlock) {
            builder.SetInsertPoint(elseBlock);
            stmtGen->beginScope();
            for (auto& stmt : node->getElseBody()) {
                visit(stmt.get());
            }
            stmtGen->endScope();
            
            // 如果else块没有终结器，添加跳转到after块
            if (!builder.GetInsertBlock()->getTerminator()) {
                builder.CreateBr(afterBlock);
            }
            
            builder.SetInsertPoint(afterBlock);
        } else {
            // 设置插入点到after块
            builder.SetInsertPoint(afterBlock);
        }
}

void CodeGenVisitor::visit(WhileStmtAST* node) {
    auto* stmtGen = codeGen.getStmtGen();
    
    // 获取当前函数
    llvm::Function* func = codeGen.getCurrentFunction();
    if (!func) {
        return;
    }
    
    // 创建必要的基本块
    auto& builder = codeGen.getBuilder();
    llvm::BasicBlock* condBlock = llvm::BasicBlock::Create(codeGen.getContext(), "while.cond", func);
    llvm::BasicBlock* bodyBlock = llvm::BasicBlock::Create(codeGen.getContext(), "while.body", func);
    llvm::BasicBlock* afterBlock = llvm::BasicBlock::Create(codeGen.getContext(), "while.end", func);
    
    // 注册循环块，处理break和continue
    codeGen.pushLoopBlocks(condBlock, afterBlock);
    
    // 跳转到条件块
    builder.CreateBr(condBlock);
    
    // 生成条件块代码
    builder.SetInsertPoint(condBlock);
    auto* exprGen = codeGen.getExprGen();
    llvm::Value* condValue = exprGen->handleExpr(node->getCondition());
    
    if (!condValue) {
        codeGen.popLoopBlocks();
        return;
    }
    
    // 根据条件跳转到循环体或循环后
    stmtGen->generateBranchingCode(condValue, bodyBlock, afterBlock);
    
    // 生成循环体代码
    builder.SetInsertPoint(bodyBlock);
    stmtGen->beginScope();
    for (auto& stmt : node->getBody()) {
        visit(stmt.get());
    }
    stmtGen->endScope();
    
    // 如果循环体没有终结器，跳回条件块
    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateBr(condBlock);
    }
    
    // 设置插入点到循环后块
    builder.SetInsertPoint(afterBlock);
    
    // 清理循环块注册
    codeGen.popLoopBlocks();
}

void CodeGenVisitor::visit(AssignStmtAST* node) {
    auto* typeGen = codeGen.getTypeGen();
    auto* stmtGen = codeGen.getStmtGen();
    
    // 获取变量类型和新值
    ObjectType* varType = codeGen.getSymbolTable().getVariableType(node->getName());
    std::shared_ptr<PyType> valueType = typeGen->inferExprType(node->getValue());
    
    // 验证赋值类型兼容性
    if (!typeGen->validateAssignment(node->getName(), node->getValue())) {
        std::string errorMsg = "Cannot assign value of type '" + 
                             (valueType ? valueType->getObjectType()->getName() : "unknown") + 
                             "' to variable '" + node->getName() + "' of type '" + 
                             (varType ? varType->getName() : "unknown") + "'";
        codeGen.logTypeError(errorMsg, 
                            node->line ? *node->line : 0, 
                            node->column ? *node->column : 0);
        return;
    }
    
    // 生成赋值代码
    auto* exprGen = codeGen.getExprGen();
    llvm::Value* valueExpr = exprGen->handleExpr(node->getValue());
    
    if (!valueExpr) {
        return;
    }
    
    // 执行赋值操作，处理类型转换和生命周期
    stmtGen->assignVariable(node->getName(), valueExpr, valueType);
}

void CodeGenVisitor::visit(IndexAssignStmtAST* node) {
    auto* typeGen = codeGen.getTypeGen();
    auto* exprGen = codeGen.getExprGen();
    auto& builder = codeGen.getBuilder();
    
    // 生成目标表达式（被索引的容器）
    llvm::Value* targetValue = exprGen->handleExpr(node->getTarget());
    if (!targetValue) {
        return;
    }
    
    // 生成索引表达式
    llvm::Value* indexValue = exprGen->handleExpr(node->getIndex());
    if (!indexValue) {
        return;
    }
    
    // 生成值表达式
    llvm::Value* valueExpr = exprGen->handleExpr(node->getValue());
    if (!valueExpr) {
        return;
    }
    
    // 获取类型信息
    std::shared_ptr<PyType> targetType = typeGen->inferExprType(node->getTarget());
    std::shared_ptr<PyType> indexType = typeGen->inferExprType(node->getIndex());
    std::shared_ptr<PyType> valueType = typeGen->inferExprType(node->getValue());
    
    // 验证索引操作
    if (!typeGen->validateIndexOperation(targetType, indexType)) {
        std::string errorMsg = "Invalid index operation: cannot use " + 
                            indexType->getObjectType()->getName() + 
                            " to index " + targetType->getObjectType()->getName();
        codeGen.logTypeError(errorMsg, 
                            node->line ? *node->line : 0, 
                            node->column ? *node->column : 0);
        return;
    }
    
    // 获取目标元素类型 - 处理 list[x] = y 情况
    std::shared_ptr<PyType> elemType;
    if (targetType->isList()) {
        elemType = PyType::getListElementType(targetType);
    } else if (targetType->isDict()) {
        elemType = PyType::getDictValueType(targetType);
    } else {
        std::string errorMsg = "Target is not a container type that supports item assignment";
        codeGen.logTypeError(errorMsg, 
                            node->line ? *node->line : 0, 
                            node->column ? *node->column : 0);
        return;
    }
    
    // 验证赋值类型兼容性
    if (!typeGen->validateListAssignment(elemType, node->getValue())) {
        std::string errorMsg = "Cannot assign value of type '" + 
                             valueType->getObjectType()->getName() + 
                             "' to container element of type '" + 
                             elemType->getObjectType()->getName() + "'";
        codeGen.logTypeError(errorMsg, 
                            node->line ? *node->line : 0, 
                            node->column ? *node->column : 0);
        return;
    }
    
    // 使用类型系统进行必要的类型转换
    llvm::Value* convertedValue = typeGen->generateTypeConversion(
        valueExpr, valueType, elemType);
    
    // 生成索引赋值代码
    auto* runtime = codeGen.getRuntimeGen();
    
    if (targetType->isList()) {
        // 列表元素赋值
        runtime->trackTempObject(convertedValue);
        exprGen->setListElement(targetValue, indexValue, convertedValue, targetType);
    } else if (targetType->isDict()) {
        // 字典元素赋值
        llvm::Function* setItemFunc = codeGen.getOrCreateExternalFunction(
            "py_dict_set_item",
            llvm::Type::getVoidTy(codeGen.getContext()),
            {
                llvm::PointerType::get(codeGen.getContext(), 0),
                llvm::PointerType::get(codeGen.getContext(), 0),
                llvm::PointerType::get(codeGen.getContext(), 0)
            });
        
        builder.CreateCall(setItemFunc, {targetValue, indexValue, convertedValue});
    }
}

void CodeGenVisitor::visit(PrintStmtAST* node) {
    auto& builder = codeGen.getBuilder();
    auto* exprGen = codeGen.getExprGen();
    auto* typeGen = codeGen.getTypeGen();
    
    // 生成表达式代码
    llvm::Value* exprValue = exprGen->handleExpr(node->getValue());
    if (!exprValue) {
        return;
    }
    
    // 获取表达式类型
    std::shared_ptr<PyType> exprType = typeGen->inferExprType(node->getValue());
    
    // 根据类型选择不同的打印函数
    if (exprType->isInt()) {
        // 打印整数
        llvm::Function* printIntFunc = codeGen.getOrCreateExternalFunction(
            "py_print_int",
            llvm::Type::getVoidTy(codeGen.getContext()),
            {llvm::Type::getInt32Ty(codeGen.getContext())});
        
        builder.CreateCall(printIntFunc, {exprValue});
    } else if (exprType->isDouble()) {
        // 打印浮点数
        llvm::Function* printDoubleFunc = codeGen.getOrCreateExternalFunction(
            "py_print_double",
            llvm::Type::getVoidTy(codeGen.getContext()),
            {llvm::Type::getDoubleTy(codeGen.getContext())});
        
        builder.CreateCall(printDoubleFunc, {exprValue});
    } else if (exprType->isBool()) {
        // 打印布尔值
        llvm::Function* printBoolFunc = codeGen.getOrCreateExternalFunction(
            "py_print_bool",
            llvm::Type::getVoidTy(codeGen.getContext()),
            {llvm::Type::getInt1Ty(codeGen.getContext())});
        
        builder.CreateCall(printBoolFunc, {exprValue});
    } else {
        // 打印对象 - 使用通用打印函数
        llvm::Function* printObjFunc = codeGen.getOrCreateExternalFunction(
            "py_print_object",
            llvm::Type::getVoidTy(codeGen.getContext()),
            {llvm::PointerType::get(codeGen.getContext(), 0)});
        
        builder.CreateCall(printObjFunc, {exprValue});
    }
}

void CodeGenVisitor::visit(PassStmtAST* node) {
    // pass语句不生成代码
}

void CodeGenVisitor::visit(ImportStmtAST* node) {
    // 在这个简单实现中，暂不处理import语句
    // 实际实现应该处理模块加载和符号导入
}

void CodeGenVisitor::visit(ClassStmtAST* node) {
    // 在这个简单实现中，暂不处理类定义
    // 实际实现应该处理类型定义、方法和属性
}

void CodeGenVisitor::visit(StmtAST* node) {
    // 根据语句类型分派到具体处理方法
    switch (node->kind()) {
        case ASTKind::ExprStmt:
            visit(static_cast<ExprStmtAST*>(node));
            break;
        case ASTKind::ReturnStmt:
            visit(static_cast<ReturnStmtAST*>(node));
            break;
        case ASTKind::IfStmt:
            visit(static_cast<IfStmtAST*>(node));
            break;
        case ASTKind::WhileStmt:
            visit(static_cast<WhileStmtAST*>(node));
            break;
        case ASTKind::AssignStmt:
            visit(static_cast<AssignStmtAST*>(node));
            break;
        case ASTKind::IndexAssignStmt:
            visit(static_cast<IndexAssignStmtAST*>(node));
            break;
        case ASTKind::PrintStmt:
            visit(static_cast<PrintStmtAST*>(node));
            break;
        case ASTKind::PassStmt:
            visit(static_cast<PassStmtAST*>(node));
            break;
        case ASTKind::ImportStmt:
            visit(static_cast<ImportStmtAST*>(node));
            break;
        case ASTKind::ClassStmt:
            visit(static_cast<ClassStmtAST*>(node));
            break;
        default:
            std::cerr << "Unknown statement type encountered in visitor" << std::endl;
            break;
    }
}

void CodeGenVisitor::visit(ASTNode* node) {
    // 根据节点类型分派到具体处理方法
    switch (node->kind()) {
        case ASTKind::Module:
            visit(static_cast<ModuleAST*>(node));
            break;
        case ASTKind::Function:
            visit(static_cast<FunctionAST*>(node));
            break;
        default:
            // 对于其他AST节点，如果是语句尝试作为语句处理
            if (dynamic_cast<StmtAST*>(node)) {
                visit(static_cast<StmtAST*>(node));
            } else {
                std::cerr << "Unknown AST node type encountered in visitor" << std::endl;
            }
            break;
    }
}

} // namespace llvmpy