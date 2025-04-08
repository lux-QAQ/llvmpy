#include "CodeGen/CodeGenStmt.h"
#include "CodeGen/CodeGenBase.h"
#include "CodeGen/CodeGenExpr.h"
#include "CodeGen/CodeGenType.h"
#include "CodeGen/CodeGenRuntime.h"
#include "CodeGen/CodeGenModule.h"  // 添加这一行
#include "CodeGen/PyCodeGen.h"  // 添加这一行，确保 PyCodeGen 完整定义
#include "TypeOperations.h"
#include "ObjectLifecycle.h"
#include <llvm/IR/Constants.h>


namespace llvmpy
{

// 静态成员初始化
std::unordered_map<ASTKind, StmtHandlerFunc> CodeGenStmt::stmtHandlers;
bool CodeGenStmt::handlersInitialized = false;

void CodeGenStmt::initializeHandlers()
{
    if (handlersInitialized)
    {
        return;
    }

    // 注册各种语句处理器
    stmtHandlers[ASTKind::ExprStmt] = [](CodeGenBase& cg, StmtAST* stmt)
    {
        static_cast<CodeGenStmt*>(cg.getStmtGen())->handleExprStmt(static_cast<ExprStmtAST*>(stmt));
    };

    stmtHandlers[ASTKind::ReturnStmt] = [](CodeGenBase& cg, StmtAST* stmt)
    {
        static_cast<CodeGenStmt*>(cg.getStmtGen())->handleReturnStmt(static_cast<ReturnStmtAST*>(stmt));
    };

    stmtHandlers[ASTKind::IfStmt] = [](CodeGenBase& cg, StmtAST* stmt)
    {
        static_cast<CodeGenStmt*>(cg.getStmtGen())->handleIfStmt(static_cast<IfStmtAST*>(stmt));
    };

    stmtHandlers[ASTKind::WhileStmt] = [](CodeGenBase& cg, StmtAST* stmt)
    {
        static_cast<CodeGenStmt*>(cg.getStmtGen())->handleWhileStmt(static_cast<WhileStmtAST*>(stmt));
    };

    stmtHandlers[ASTKind::PrintStmt] = [](CodeGenBase& cg, StmtAST* stmt)
    {
        static_cast<CodeGenStmt*>(cg.getStmtGen())->handlePrintStmt(static_cast<PrintStmtAST*>(stmt));
    };

    stmtHandlers[ASTKind::AssignStmt] = [](CodeGenBase& cg, StmtAST* stmt)
    {
        static_cast<CodeGenStmt*>(cg.getStmtGen())->handleAssignStmt(static_cast<AssignStmtAST*>(stmt));
    };

    stmtHandlers[ASTKind::IndexAssignStmt] = [](CodeGenBase& cg, StmtAST* stmt)
    {
        static_cast<CodeGenStmt*>(cg.getStmtGen())->handleIndexAssignStmt(static_cast<IndexAssignStmtAST*>(stmt));
    };

    stmtHandlers[ASTKind::PassStmt] = [](CodeGenBase& cg, StmtAST* stmt)
    {
        static_cast<CodeGenStmt*>(cg.getStmtGen())->handlePassStmt(static_cast<PassStmtAST*>(stmt));
    };

    stmtHandlers[ASTKind::ImportStmt] = [](CodeGenBase& cg, StmtAST* stmt)
    {
        static_cast<CodeGenStmt*>(cg.getStmtGen())->handleImportStmt(static_cast<ImportStmtAST*>(stmt));
    };

    stmtHandlers[ASTKind::ClassStmt] = [](CodeGenBase& cg, StmtAST* stmt)
    {
        static_cast<CodeGenStmt*>(cg.getStmtGen())->handleClassStmt(static_cast<ClassStmtAST*>(stmt));
    };

    handlersInitialized = true;
}

void CodeGenStmt::handleStmt(StmtAST* stmt)
{
    if (!stmt)
    {
        codeGen.logError("Null statement pointer");
        return;
    }

    // 使用适当的处理器处理语句
    auto it = stmtHandlers.find(stmt->kind());
    if (it != stmtHandlers.end())
    {
        it->second(codeGen, stmt);
    }
    else
    {
        codeGen.logError("Unknown statement type",
                         stmt->line ? *stmt->line : 0,
                         stmt->column ? *stmt->column : 0);
    }
}

void CodeGenStmt::handleBlock(const std::vector<std::unique_ptr<StmtAST>>& stmts)
{
    // 创建新作用域
    beginScope();

    // 处理块中的每个语句
    for (const auto& stmt : stmts)
    {
        handleStmt(stmt.get());

        // 检查基本块是否已经终止（例如由于return语句）
        if (codeGen.getBuilder().GetInsertBlock()->getTerminator())
        {
            break;
        }
    }

    // 关闭作用域
    endScope();
}

void CodeGenStmt::beginScope()
{
    codeGen.getSymbolTable().pushScope();
}

void CodeGenStmt::endScope()
{
    codeGen.getSymbolTable().popScope();
}

llvm::Value* CodeGenStmt::handleCondition(const ExprAST* condition)
{
    auto* exprGen = codeGen.getExprGen();
    auto* typeGen = codeGen.getTypeGen();
    auto* runtime = codeGen.getRuntimeGen();
    auto& builder = codeGen.getBuilder();

    // 生成条件表达式的代码
    llvm::Value* condValue = exprGen->handleExpr(condition);
    if (!condValue)
    {
        return nullptr;
    }

    // 获取条件表达式的类型
    std::shared_ptr<PyType> condType = condition->getType();

    // 错误部分：不应该根据类型做特殊处理
    // 对于布尔类型，直接提取布尔值
    // if (condType->isBool())
    // {
    //     return condValue;  // 这里有问题！即使是布尔对象，也需要提取其值
    // }

    // 对于所有类型（包括布尔类型），都需要转换为i1类型
    // 获取对象到布尔值的转换函数
    llvm::Function* objectToBoolFunc = codeGen.getOrCreateExternalFunction(
            "py_object_to_bool",
            llvm::Type::getInt1Ty(codeGen.getContext()),
            {llvm::PointerType::get(codeGen.getContext(), 0)});

    // 调用转换函数
    return builder.CreateCall(objectToBoolFunc, {condValue}, "condval");
}

void CodeGenStmt::generateBranchingCode(llvm::Value* condValue,
                                        llvm::BasicBlock* thenBlock,
                                        llvm::BasicBlock* elseBlock)
{
    auto& builder = codeGen.getBuilder();

    // 创建条件分支
    builder.CreateCondBr(condValue, thenBlock, elseBlock);
}

// 例如，在处理表达式语句时：
void CodeGenStmt::handleExprStmt(ExprStmtAST* stmt)
{
    auto* exprGen = codeGen.getExprGen();
    // 这里使用的是 const ExprAST*
    llvm::Value* exprValue = exprGen->handleExpr(stmt->getExpr());

    // 这里不需要使用表达式的结果，但需要处理可能的临时对象
    auto* runtime = codeGen.getRuntimeGen();
    runtime->cleanupTemporaryObjects();
}

void CodeGenStmt::handleReturnStmt(ReturnStmtAST* stmt)
{
    auto& builder = codeGen.getBuilder();
    auto* exprGen = codeGen.getExprGen();
    auto* runtime = codeGen.getRuntimeGen();

    // 标记我们正在处理返回语句
    codeGen.setInReturnStmt(true);

    // 生成返回值的代码
    llvm::Value* retVal = nullptr;
    if (stmt->getValue())
    {
        // 有明确的返回值
        retVal = exprGen->handleExpr(stmt->getValue());
        if (!retVal)
        {
            codeGen.setInReturnStmt(false);
            return;
        }

        // 准备返回值 - 确保类型匹配并处理生命周期
        if (codeGen.getCurrentReturnType())
        {
            std::shared_ptr<PyType> returnType = PyType::fromObjectType(codeGen.getCurrentReturnType());
            std::shared_ptr<PyType> valueType = stmt->getValue()->getType();

            // 使用运行时系统准备返回值
            retVal = runtime->prepareReturnValue(retVal, valueType, returnType);
        }
    }
    else
    {
        // 没有明确返回值，默认返回None
        retVal = exprGen->createNoneLiteral();
    }

    // 创建返回指令
    builder.CreateRet(retVal);

    // 重置返回语句标志
    codeGen.setInReturnStmt(false);

    // 清理临时对象
    runtime->cleanupTemporaryObjects();

    // 恢复保存的块（如果有的话）
    if (codeGen.getSavedBlock())
    {
        builder.SetInsertPoint(codeGen.getSavedBlock());
    }
}

void CodeGenStmt::handleIfStmt(IfStmtAST* stmt)
{
    auto& builder = codeGen.getBuilder();

    // 获取条件的布尔值
    llvm::Value* condValue = handleCondition(stmt->getCondition());
    if (!condValue) return;

    // 创建必要的基本块
    llvm::Function* func = codeGen.getCurrentFunction();
    llvm::BasicBlock* thenBB = codeGen.createBasicBlock("then", func);
    llvm::BasicBlock* elseBB = codeGen.createBasicBlock("else", func);
    llvm::BasicBlock* mergeBB = codeGen.createBasicBlock("ifcont", func);

    // 创建条件分支
    generateBranchingCode(condValue, thenBB, elseBB);

    // 处理then分支
    builder.SetInsertPoint(thenBB);
    handleBlock(stmt->getThenBody());

    // 如果then分支没有终止（例如，没有return语句），添加跳转到merge块
    if (!builder.GetInsertBlock()->getTerminator())
    {
        builder.CreateBr(mergeBB);
    }

    // 处理else分支
    builder.SetInsertPoint(elseBB);
    handleBlock(stmt->getElseBody());

    // 如果else分支没有终止，添加跳转到merge块
    if (!builder.GetInsertBlock()->getTerminator())
    {
        builder.CreateBr(mergeBB);
    }

    // 继续处理merge块之后的代码
    builder.SetInsertPoint(mergeBB);
}


// 修改现有方法

void CodeGenStmt::handleWhileStmt(WhileStmtAST* stmt)
{
    auto& builder = codeGen.getBuilder();
    auto* runtime = codeGen.getRuntimeGen();
    auto& updateContext = codeGen.getVariableUpdateContext();

    // 创建必要的基本块
    llvm::Function* func = codeGen.getCurrentFunction();
    llvm::BasicBlock* entryBB = builder.GetInsertBlock();
    llvm::BasicBlock* condBB = codeGen.createBasicBlock("while.cond", func);
    llvm::BasicBlock* bodyBB = codeGen.createBasicBlock("while.body", func);
    llvm::BasicBlock* endBB = codeGen.createBasicBlock("while.end", func);

    // 设置变量更新上下文
    updateContext.setLoopContext(condBB, endBB);
    
    // 保存之前的循环信息
    auto* oldLoop = codeGen.getCurrentLoop();
    llvm::BasicBlock* oldLoopBlock = oldLoop ? oldLoop->condBlock : nullptr;
    codeGen.setCurrentLoop(condBB);
    
    // 跳转到条件块
    builder.CreateBr(condBB);
    
    // 为函数参数创建PHI节点
    builder.SetInsertPoint(condBB, condBB->begin());
    for (auto& arg : func->args()) {
        std::string argName = arg.getName().str();
        if (!argName.empty()) {
            // 创建PHI节点
            llvm::PHINode* phi = builder.CreatePHI(arg.getType(), 2, argName + ".phi");
            phi->addIncoming(&arg, entryBB);
            
            // 在符号表中更新参数值为PHI节点
            codeGen.getSymbolTable().setVariable(argName, phi, 
                codeGen.getSymbolTable().getVariableType(argName));
            
            // 注册为循环变量
            updateContext.registerLoopVariable(argName, phi);
        }
    }
    
    // 生成条件代码
    builder.SetInsertPoint(condBB);
    llvm::Value* condValue = handleCondition(stmt->getCondition());
    if (!condValue) {
        updateContext.clearLoopContext();
        codeGen.setCurrentLoop(oldLoopBlock);
        return;
    }
    
    // 创建条件分支
    builder.CreateCondBr(condValue, bodyBB, endBB);
    
    // 生成循环体代码
    builder.SetInsertPoint(bodyBB);
    beginScope();
    
    // 处理循环体语句
    for (auto& bodyStmt : stmt->getBody()) {
        handleStmt(bodyStmt.get());
        if (builder.GetInsertBlock()->getTerminator()) break;
    }
    
    // 如果循环体没有终止，创建到条件块的分支
    llvm::BasicBlock* lastBlock = builder.GetInsertBlock();
    if (!lastBlock->getTerminator()) {
        builder.CreateBr(condBB);
    }
    
    endScope();
    
    // 恢复设置
    codeGen.setCurrentLoop(oldLoopBlock);
    updateContext.clearLoopContext();
    
    // 设置插入点到循环后块
    builder.SetInsertPoint(endBB);
}

void CodeGenStmt::handlePrintStmt(PrintStmtAST* stmt)
{
    auto& builder = codeGen.getBuilder();
    auto* exprGen = codeGen.getExprGen();
    auto* runtime = codeGen.getRuntimeGen();

    // 生成打印值的代码
    llvm::Value* value = exprGen->handleExpr(stmt->getValue());
    if (!value) return;

    // 获取值的类型
    std::shared_ptr<PyType> valueType = stmt->getValue()->getType();

    // 根据值的类型调用不同的打印函数
    if (valueType->isInt())
    {
        // 从Python对象提取整数
        llvm::Function* extractIntFunc = codeGen.getOrCreateExternalFunction(
                "py_extract_int",
                llvm::Type::getInt32Ty(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0)});

        llvm::Value* intValue = builder.CreateCall(extractIntFunc, {value}, "int_value");

        // 调用打印整数函数
        llvm::Function* printIntFunc = codeGen.getOrCreateExternalFunction(
                "py_print_int",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::Type::getInt32Ty(codeGen.getContext())});

        builder.CreateCall(printIntFunc, {intValue});
    }
    else if (valueType->isDouble())
    {
        // 从Python对象提取浮点数
        llvm::Function* extractDoubleFunc = codeGen.getOrCreateExternalFunction(
                "py_extract_double",
                llvm::Type::getDoubleTy(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0)});

        llvm::Value* doubleValue = builder.CreateCall(extractDoubleFunc, {value}, "double_value");

        // 调用打印浮点数函数
        llvm::Function* printDoubleFunc = codeGen.getOrCreateExternalFunction(
                "py_print_double",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::Type::getDoubleTy(codeGen.getContext())});

        builder.CreateCall(printDoubleFunc, {doubleValue});
    }
    else if (valueType->isString())
    {
        // 从Python对象提取字符串
        llvm::Function* extractStringFunc = codeGen.getOrCreateExternalFunction(
                "py_extract_string",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0)});

        llvm::Value* strPtr = builder.CreateCall(extractStringFunc, {value}, "str_ptr");

        // 调用打印字符串函数
        llvm::Function* printStringFunc = codeGen.getOrCreateExternalFunction(
                "py_print_string",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0)});

        builder.CreateCall(printStringFunc, {strPtr});
    }
    else
    {
        // 对于其他类型，先转换为字符串再打印
        llvm::Function* toStringFunc = codeGen.getOrCreateExternalFunction(
                "py_convert_to_string",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0)});

        llvm::Value* strObj = builder.CreateCall(toStringFunc, {value}, "str_obj");

        // 从字符串对象提取C字符串
        llvm::Function* extractStringFunc = codeGen.getOrCreateExternalFunction(
                "py_extract_string",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0)});

        llvm::Value* strPtr = builder.CreateCall(extractStringFunc, {strObj}, "str_ptr");

        // 调用打印字符串函数
        llvm::Function* printStringFunc = codeGen.getOrCreateExternalFunction(
                "py_print_string",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0)});

        builder.CreateCall(printStringFunc, {strPtr});

        // 处理临时字符串对象的生命周期
        runtime->decRef(strObj);
    }

    // 清理临时对象
    runtime->cleanupTemporaryObjects();
}

// 修改现有方法

void CodeGenStmt::handleAssignStmt(AssignStmtAST* stmt)
{
    auto& builder = codeGen.getBuilder();
    auto* exprGen = codeGen.getExprGen();
    auto* typeGen = codeGen.getTypeGen();
    auto* runtime = codeGen.getRuntimeGen();

    // 获取变量名和值表达式
    const std::string& varName = stmt->getName();
    const ExprAST* valueExpr = stmt->getValue();

    // 验证类型兼容性
    if (!typeGen->validateAssignment(varName, valueExpr)) {
        codeGen.logError("Type error in assignment to '" + varName + "'");
        return;
    }

    // 生成值表达式的代码
    llvm::Value* value = exprGen->handleExpr(valueExpr);
    if (!value) return;

    // 获取类型信息
    ObjectType* targetType = codeGen.getSymbolTable().getVariableType(varName);
    std::shared_ptr<PyType> valueType = valueExpr->getType();
    
    // 准备用于赋值的值
    PyCodeGen* pyCodeGen = codeGen.asPyCodeGen();
    if (pyCodeGen && codeGen.getSymbolTable().hasVariable(varName)) {
        value = pyCodeGen->prepareAssignmentTarget(value, targetType, valueExpr);
        if (!value) return;
    }
    
    // 使用符号表的更新方法处理变量更新
    if (codeGen.getSymbolTable().hasVariable(varName)) {
        codeGen.getSymbolTable().updateVariable(codeGen, varName, value, targetType, valueType);
    } else {
        codeGen.getSymbolTable().setVariable(varName, value, valueType->getObjectType());
        if (valueType && valueType->isReference()) {
            runtime->incRef(value);
        }
    }

    // 标记最后计算的表达式
    codeGen.setLastExprValue(value);
    codeGen.setLastExprType(valueType);
    
    // 清理临时对象
    runtime->cleanupTemporaryObjects();
}
void CodeGenStmt::handleIndexAssignStmt(IndexAssignStmtAST* stmt)
{
    auto* exprGen = codeGen.getExprGen();
    auto* runtime = codeGen.getRuntimeGen();

    // 获取目标对象
    llvm::Value* target = exprGen->handleExpr(stmt->getTarget());
    if (!target) return;

    // 获取索引
    llvm::Value* index = exprGen->handleExpr(stmt->getIndex());
    if (!index) return;

    // 获取值
    llvm::Value* value = exprGen->handleExpr(stmt->getValue());
    if (!value) return;

    // 获取类型信息
    std::shared_ptr<PyType> targetType = stmt->getTarget()->getType();
    std::shared_ptr<PyType> indexType = stmt->getIndex()->getType();
    std::shared_ptr<PyType> valueType = stmt->getValue()->getType();

    // 根据容器类型处理索引赋值
    if (targetType->isList())
    {
        // 列表索引赋值
        // 验证索引类型 (应该是整数)
        if (!indexType->isInt() && !indexType->isBool())
        {
            codeGen.logError("List indices must be integers");
            return;
        }

        // 验证赋值类型与列表元素类型的兼容性
        std::shared_ptr<PyType> elemType = PyType::getListElementType(targetType);

        // 准备赋值
        if (elemType && !valueType->equals(*elemType))
        {
            // 类型不同，尝试转换
            value = runtime->prepareAssignmentTarget(
                    value, valueType, elemType);
        }

        // 创建列表索引赋值
        llvm::Function* setItemFunc = codeGen.getOrCreateExternalFunction(
                "py_list_set_item",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {
                        llvm::PointerType::get(codeGen.getContext(), 0),  // list
                        llvm::Type::getInt32Ty(codeGen.getContext()),     // index
                        llvm::PointerType::get(codeGen.getContext(), 0)   // item
                });

        // 确保索引是int32类型
        llvm::Value* indexValue = index;
        if (indexType->isBool())
        {
            // 从布尔值提取整数
            llvm::Function* extractBoolFunc = codeGen.getOrCreateExternalFunction(
                    "py_extract_bool",
                    llvm::Type::getInt1Ty(codeGen.getContext()),
                    {llvm::PointerType::get(codeGen.getContext(), 0)});

            llvm::Value* boolValue = codeGen.getBuilder().CreateCall(
                    extractBoolFunc, {index}, "bool_val");

            // 将bool转为int32
            indexValue = codeGen.getBuilder().CreateZExt(
                    boolValue, llvm::Type::getInt32Ty(codeGen.getContext()), "int_index");
        }
        else
        {
            // 从整数对象提取int32
            llvm::Function* extractIntFunc = codeGen.getOrCreateExternalFunction(
                    "py_extract_int",
                    llvm::Type::getInt32Ty(codeGen.getContext()),
                    {llvm::PointerType::get(codeGen.getContext(), 0)});

            indexValue = codeGen.getBuilder().CreateCall(
                    extractIntFunc, {index}, "int_index");
        }

        // 设置列表元素
        codeGen.getBuilder().CreateCall(setItemFunc, {target, indexValue, value});
    }
    else if (targetType->isDict())
    {
        // 字典索引赋值
        llvm::Function* setItemFunc = codeGen.getOrCreateExternalFunction(
                "py_dict_set_item",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {
                        llvm::PointerType::get(codeGen.getContext(), 0),  // dict
                        llvm::PointerType::get(codeGen.getContext(), 0),  // key
                        llvm::PointerType::get(codeGen.getContext(), 0)   // value
                });

        // 设置字典项
        codeGen.getBuilder().CreateCall(setItemFunc, {target, index, value});
    }
    else
    {
        // 通用索引赋值
        llvm::Function* setIndexFunc = codeGen.getOrCreateExternalFunction(
                "py_object_set_index",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {
                        llvm::PointerType::get(codeGen.getContext(), 0),  // target
                        llvm::PointerType::get(codeGen.getContext(), 0),  // index
                        llvm::PointerType::get(codeGen.getContext(), 0)   // value
                });

        // 设置通用索引
        codeGen.getBuilder().CreateCall(setIndexFunc, {target, index, value});
    }

    // 清理临时对象
    runtime->cleanupTemporaryObjects();
}

void CodeGenStmt::handlePassStmt(PassStmtAST* stmt)
{
    // pass语句不生成任何代码
}

void CodeGenStmt::handleImportStmt(ImportStmtAST* stmt)
{
    auto& builder = codeGen.getBuilder();

    // 获取模块名
    const std::string& moduleName = stmt->getModuleName();
    const std::string& alias = stmt->getAlias();

    // 调用运行时导入函数
    llvm::Function* importModuleFunc = codeGen.getOrCreateExternalFunction(
            "py_import_module",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {llvm::PointerType::get(codeGen.getContext(), 0)});

    // 创建模块名字符串
    llvm::Value* moduleNameValue = builder.CreateGlobalString(moduleName, "module_name");

    // 导入模块
    llvm::Value* moduleObj = builder.CreateCall(importModuleFunc, {moduleNameValue}, "module_obj");

    // 将模块对象存储到符号表中
    std::string moduleScopeName = alias.empty() ? moduleName : alias;

    // 获取模块类型
    auto* typeGen = codeGen.getTypeGen();
    auto moduleType = typeGen->getModuleType(moduleName);

    // 将模块添加到符号表
    codeGen.getSymbolTable().setVariable(
            moduleScopeName,
            moduleObj,
            moduleType->getObjectType());

    // 清理临时对象
    auto* runtime = codeGen.getRuntimeGen();
    runtime->cleanupTemporaryObjects();
}

void CodeGenStmt::handleClassStmt(ClassStmtAST* stmt)
{
    auto& builder = codeGen.getBuilder();
    auto* typeGen = codeGen.getTypeGen();

    // 获取类名和基类
    const std::string& className = stmt->getClassName();
    const std::vector<std::string>& baseClasses = stmt->getBaseClasses();

    // 创建类型对象
    llvm::Function* createClassFunc = codeGen.getOrCreateExternalFunction(
            "py_create_class",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {
                    llvm::PointerType::get(codeGen.getContext(), 0),  // class name
                    llvm::PointerType::get(codeGen.getContext(), 0)   // base classes list
            });

    // 创建类名字符串
    llvm::Value* classNameValue = builder.CreateGlobalString(className, "class_name");

    // 创建基类列表
    llvm::Value* baseClassesList = nullptr;
    if (baseClasses.empty())
    {
        // 没有基类，传入None
        llvm::Function* getNoneFunc = codeGen.getOrCreateExternalFunction(
                "py_get_none",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {});

        baseClassesList = builder.CreateCall(getNoneFunc, {}, "no_bases");
    }
    else
    {
        // 有基类，创建基类列表
        auto* exprGen = codeGen.getExprGen();
        auto elementType = PyType::getAny();

        // 创建空列表
        baseClassesList = exprGen->createList(baseClasses.size(), elementType);

        // 填充基类
        for (size_t i = 0; i < baseClasses.size(); i++)
        {
            // 获取基类对象
            llvm::Value* baseClass = codeGen.getSymbolTable().getVariable(baseClasses[i]);
            if (!baseClass)
            {
                codeGen.logError("Base class not found: " + baseClasses[i]);
                continue;
            }

            // 添加到列表
            llvm::Value* index = llvm::ConstantInt::get(
                    llvm::Type::getInt32Ty(codeGen.getContext()), i);

            exprGen->setListElement(baseClassesList, index, baseClass, elementType);
        }
    }

    // 创建类对象
    llvm::Value* classObj = builder.CreateCall(
            createClassFunc, {classNameValue, baseClassesList}, "class_obj");

    // 创建新作用域来处理类体
    beginScope();

    // 将self添加到当前作用域
    llvm::Function* getSelfFuncType = codeGen.getOrCreateExternalFunction(
            "py_get_self_default",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {});

    llvm::Value* self = builder.CreateCall(getSelfFuncType, {}, "self");

    codeGen.getSymbolTable().setVariable(
            "self",
            self,
            typeGen->getClassInstanceType(className)->getObjectType());

    // 处理类体中的方法和属性
    auto* stmtGen = codeGen.getStmtGen();
    for (auto& method : stmt->getMethods())
    {
        // 设置方法的类上下文
        method->setClassContext(className);
        stmtGen->handleMethod(method.get(), classObj);
    }

    // 结束类作用域
    endScope();

    // 将类对象添加到当前作用域
    codeGen.getSymbolTable().setVariable(
            className,
            classObj,
            typeGen->getClassType(className)->getObjectType());

    // 清理临时对象
    auto* runtime = codeGen.getRuntimeGen();
    runtime->cleanupTemporaryObjects();
}

void CodeGenStmt::handleMethod(FunctionAST* method, llvm::Value* classObj)
{
    // 生成方法的代码
    auto& builder = codeGen.getBuilder();
    auto* moduleGen = codeGen.getModuleGen();

    // 生成方法
    llvm::Function* methodFunc = moduleGen->handleFunctionDef(method);
    if (!methodFunc) return;

    // 将方法添加到类
    llvm::Function* addMethodFunc = codeGen.getOrCreateExternalFunction(
            "py_class_add_method",
            llvm::Type::getVoidTy(codeGen.getContext()),
            {
                    llvm::PointerType::get(codeGen.getContext(), 0),  // class
                    llvm::PointerType::get(codeGen.getContext(), 0),  // method name
                    llvm::PointerType::get(codeGen.getContext(), 0)   // method
            });

    // 创建方法名字符串
    llvm::Value* methodNameValue = builder.CreateGlobalString(method->name, "method_name");

    // 将方法包装为Python可调用对象
    llvm::Function* wrapMethodFunc = codeGen.getOrCreateExternalFunction(
            "py_wrap_function",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {llvm::PointerType::get(llvm::FunctionType::get(
                                            llvm::PointerType::get(codeGen.getContext(), 0),
                                            {llvm::PointerType::get(codeGen.getContext(), 0)},
                                            true),
                                    0)});

    // 包装方法
    llvm::Value* wrappedMethod = builder.CreateCall(
            wrapMethodFunc,
            {builder.CreateBitCast(
                    methodFunc,
                    llvm::PointerType::get(llvm::FunctionType::get(
                                                   llvm::PointerType::get(codeGen.getContext(), 0),
                                                   {llvm::PointerType::get(codeGen.getContext(), 0)},
                                                   true),
                                           0))},
            "wrapped_method");

    // 添加方法到类
    builder.CreateCall(addMethodFunc, {classObj, methodNameValue, wrappedMethod});
}

void CodeGenStmt::assignVariable(const std::string& name,
                                 llvm::Value* value,
                                 std::shared_ptr<PyType> valueType)
{
    auto* runtime = codeGen.getRuntimeGen();

    // 检查变量是否已经存在
    if (codeGen.getSymbolTable().hasVariable(name))
    {
        // 变量已存在，需要处理旧引用
        llvm::Value* oldValue = codeGen.getSymbolTable().getVariable(name);
        ObjectType* oldType = codeGen.getSymbolTable().getVariableType(name);

        // 如果旧值是引用类型，减少引用计数
        if (oldValue && oldType && oldType->isReference())
        {
            runtime->decRef(oldValue);
        }
    }

    // 设置新变量
    codeGen.getSymbolTable().setVariable(
            name,
            value,
            valueType ? valueType->getObjectType() : nullptr);

    // 对于引用类型的新变量，增加引用计数
    if (valueType && valueType->isReference())
    {
        runtime->incRef(value);
    }
}

}  // namespace llvmpy