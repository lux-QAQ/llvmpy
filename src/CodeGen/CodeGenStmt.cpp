#include "CodeGen/CodeGenStmt.h"
#include "CodeGen/CodeGenBase.h"
#include "CodeGen/CodeGenExpr.h"
#include "CodeGen/CodeGenType.h"
#include "CodeGen/CodeGenRuntime.h"
#include "CodeGen/CodeGenModule.h"  // 添加这一行
#include "CodeGen/PyCodeGen.h"      // 添加这一行，确保 PyCodeGen 完整定义
#include "CodeGen/CodeGenVisitor.h"
#include "TypeOperations.h"
#include "ObjectLifecycle.h"
#include "Debugdefine.h"

#include <llvm/IR/Constants.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>  // 可能需要包含这个
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Support/raw_ostream.h>  // 用于打印 LLVM 对象

#include <set>
#include <iostream>  // 用于 std::cerr

namespace llvmpy
{

#ifdef DEBUG_WhileSTmt
// 辅助函数，用于将 LLVM 对象转换为字符串以便打印
std::string llvmObjToString(const llvm::Value* V)
{
    if (!V) return "<null Value>";
    std::string S;
    llvm::raw_string_ostream OS(S);
    V->print(OS);
    return OS.str();
}
std::string llvmObjToString(const llvm::Type* T)
{
    if (!T) return "<null Type>";
    std::string S;
    llvm::raw_string_ostream OS(S);
    T->print(OS);
    return OS.str();
}
std::string llvmObjToString(const llvm::BasicBlock* BB)
{
    if (!BB) return "<null BasicBlock>";
    if (BB->hasName()) return BB->getName().str();
    std::string S;
    llvm::raw_string_ostream OS(S);
    BB->printAsOperand(OS, false);  // 打印块的标签
    return OS.str();
}
#endif

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
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("Entering handleWhileStmt");
    if (stmt->line) DEBUG_LOG("  Source Line: " + std::to_string(*stmt->line));
#endif

    auto& builder = codeGen.getBuilder();
    auto* runtime = codeGen.getRuntimeGen();
    auto& context = codeGen.getContext();
    auto& symTable = codeGen.getSymbolTable();

    llvm::Function* func = codeGen.getCurrentFunction();
    if (!func)
    {
        codeGen.logError("Cannot generate while loop outside a function.", stmt->line ? *stmt->line : 0, stmt->column ? *stmt->column : 0);
#ifdef DEBUG_WhileSTmt
        DEBUG_LOG("  ERROR: Not inside a function. Aborting.");
#endif
        return;
    }

    // --- 1. 创建基本块 ---
    llvm::BasicBlock* preheaderBB = builder.GetInsertBlock();
    llvm::BasicBlock* condBB = codeGen.createBasicBlock("while.cond", func);
    llvm::BasicBlock* bodyBB = codeGen.createBasicBlock("while.body", func);
    llvm::BasicBlock* endBB = codeGen.createBasicBlock("while.end", func);

#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [1] Created Basic Blocks:");
    DEBUG_LOG("      Preheader: " + llvmObjToString(preheaderBB));
    DEBUG_LOG("      Condition: " + llvmObjToString(condBB));
    DEBUG_LOG("      Body:      " + llvmObjToString(bodyBB));
    DEBUG_LOG("      End:       " + llvmObjToString(endBB));
#endif

    // --- 2. 识别循环中修改的变量 ---
    std::set<std::string> assignedInBody;
    CodeGenVisitor visitor(codeGen);
    for (const auto& bodyStmt : stmt->getBody())
    {
        visitor.findAssignedVariables(bodyStmt.get(), assignedInBody);
    }
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [2] Variables assigned in body:");
    if (assignedInBody.empty())
    {
        DEBUG_LOG("      None");
    }
    else
    {
        for (const auto& name : assignedInBody)
        {
            DEBUG_LOG("      - " + name);
        }
    }
#endif

    std::map<std::string, llvm::Value*> valueBeforeLoop;
    std::map<std::string, ObjectType*> typeBeforeLoop;
    std::map<std::string, llvm::PHINode*> phiNodes;

    // --- 3. 从 Preheader 跳转到 Condition ---
    builder.CreateBr(condBB);
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [3] Created branch from Preheader (" + llvmObjToString(preheaderBB) + ") to Condition (" + llvmObjToString(condBB) + ")");
#endif

    // --- 4. Condition Block ---
    builder.SetInsertPoint(condBB);
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [4] Set insert point to Condition block (" + llvmObjToString(condBB) + ")");
#endif

    // --- 5. 创建 PHI 节点 ---
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [5] Creating PHI nodes:");
#endif
    for (const std::string& varName : assignedInBody)
    {
        llvm::Value* initialVal = symTable.getVariable(varName);
        ObjectType* varType = symTable.getVariableType(varName);
        if (initialVal && varType)
        {
#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("      Processing variable: '" + varName + "'");
            DEBUG_LOG("        Initial Value: " + llvmObjToString(initialVal));
            DEBUG_LOG("        Initial Type: " + (varType ? varType->getName() : "<null ObjectType>"));
            DEBUG_LOG("        LLVM Type: " + llvmObjToString(initialVal->getType()));
#endif
            llvm::PHINode* phi = builder.CreatePHI(initialVal->getType(), 2, varName + ".phi");  // 预期 2 个入口
#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("        Created PHI node: " + llvmObjToString(phi));
#endif
            phi->addIncoming(initialVal, preheaderBB);
#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("        Added incoming edge to PHI: [" + llvmObjToString(initialVal) + ", from " + llvmObjToString(preheaderBB) + "]");
#endif
            phiNodes[varName] = phi;
            symTable.setVariable(varName, phi, varType);
#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("        Updated symbol table for '" + varName + "' to use PHI node.");
#endif
        }
        else
        {
#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("      Skipping PHI for variable '" + varName + "' (not defined before loop or type missing).");
#endif
        }
    }

    // --- 6. 生成条件判断代码 ---
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [6] Generating condition code...");
#endif
    llvm::Value* condValue = handleCondition(stmt->getCondition());
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("      Generated condition value: " + llvmObjToString(condValue));
#endif
    if (!condValue)
    {
#ifdef DEBUG_WhileSTmt
        DEBUG_LOG("      ERROR: Condition generation failed.");
#endif
        // 错误处理：恢复符号表并清理
        for (auto const& [name, val] : valueBeforeLoop)
        {
            if (phiNodes.count(name))
            {
                symTable.setVariable(name, val, typeBeforeLoop[name]);
            }
        }
        // 从 condBB 直接跳到 endBB
        if (condBB->getTerminator())
        {
            llvm::ReplaceInstWithInst(condBB->getTerminator(), llvm::BranchInst::Create(endBB));
        }
        else
        {
            builder.SetInsertPoint(condBB);  // 确保 builder 在 condBB
            builder.CreateBr(endBB);
        }
        builder.SetInsertPoint(endBB);
        codeGen.logError("Failed to generate condition for while loop.", stmt->line ? *stmt->line : 0, stmt->column ? *stmt->column : 0);
#ifdef DEBUG_WhileSTmt
        DEBUG_LOG("      Cleaned up and jumped to end block (" + llvmObjToString(endBB) + "). Aborting loop generation.");
#endif
        return;
    }

    // --- 7. 创建条件分支 ---
    builder.CreateCondBr(condValue, bodyBB, endBB);
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [7] Created conditional branch based on " + llvmObjToString(condValue) + ":");
    DEBUG_LOG("      True -> Body (" + llvmObjToString(bodyBB) + ")");
    DEBUG_LOG("      False -> End (" + llvmObjToString(endBB) + ")");
#endif

    // --- 8. Loop Body Block ---
    builder.SetInsertPoint(bodyBB);
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [8] Set insert point to Body block (" + llvmObjToString(bodyBB) + ")");
#endif
    beginScope();
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("      Started new scope for loop body.");
    DEBUG_LOG("      Processing loop body statements...");
#endif
    for (auto& bodyStmt : stmt->getBody())
    {
#ifdef DEBUG_WhileSTmt
        // 可选：更详细地记录每个语句的处理
        // DEBUG_LOG("        Handling statement of kind: " + std::to_string(static_cast<int>(bodyStmt->kind())));
#endif
        handleStmt(bodyStmt.get());
        if (builder.GetInsertBlock()->getTerminator())
        {
#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("      Loop body terminated early (return/break/continue detected in block " + llvmObjToString(builder.GetInsertBlock()) + ").");
#endif
            break;
        }
    }
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("      Finished processing loop body statements.");
#endif

    // --- 9. Latch Logic ---
    llvm::BasicBlock* latchBB = builder.GetInsertBlock();
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [9] Latch Logic:");
    DEBUG_LOG("      Determined Latch block (current insert block after body): " + llvmObjToString(latchBB));
#endif
    std::map<std::string, llvm::Value*> valueFromLatch;

    bool loopTerminatedEarly = latchBB->getTerminator() != nullptr;
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("      Checking if Latch block (" + llvmObjToString(latchBB) + ") has terminator: " + (loopTerminatedEarly ? "Yes" : "No"));
#endif

    if (!loopTerminatedEarly)
    {
#ifdef DEBUG_WhileSTmt
        DEBUG_LOG("      Latch block has no terminator. Processing back edge:");
#endif
        // 获取循环体结束时变量的最终值 (来自循环体作用域)
        for (const auto& [name, phi] : phiNodes)
        {
            // --- 从符号表获取循环体结束时的值 ---
            llvm::Value* finalVal = symTable.getVariable(name);  // 获取当前作用域的值
#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("        Getting final value for PHI '" + name + "' from latch scope: " + llvmObjToString(finalVal));
#endif
            if (!finalVal)
            {
                // 如果在当前作用域找不到（理论上不应发生，因为 assignedInBody 保证了赋值），
                // 作为健壮性回退，使用循环前的值。
                finalVal = valueBeforeLoop[name];
#ifdef DEBUG_WhileSTmt
                DEBUG_LOG("          WARNING: Final value not found in scope for '" + name + "', using value from before loop: " + llvmObjToString(finalVal));
#endif
                codeGen.logWarning("Variable '" + name + "' not found at end of loop body scope, using value from before loop for PHI backedge.", stmt->line ? *stmt->line : 0, stmt->column ? *stmt->column : 0);
            }
            // --- 修正结束 ---
            valueFromLatch[name] = finalVal;  // 现在 finalVal 已声明并赋值
        }

        // --- 新增：清理当前迭代的临时对象 ---
#ifdef DEBUG_WhileSTmt
        DEBUG_LOG("      Cleaning up temporary objects for this iteration...");
#endif
        runtime->cleanupTemporaryObjects();  // 在跳转前回溯并 decref 临时对象
        // --- 新增结束 ---

        // 创建回边: Latch -> Cond
        builder.CreateBr(condBB);
#ifdef DEBUG_WhileSTmt
        DEBUG_LOG("      Created back edge branch from Latch (" + llvmObjToString(latchBB) + ") to Condition (" + llvmObjToString(condBB) + ")");
#endif
    }
    // else: latchBB 已经有终结符 (return/break)，不需要回边
    // 注意：如果是因为 return 或 break 退出，临时对象的清理可能需要由 handleReturnStmt 或 handleBreakStmt 触发

    endScope();
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("      Ended loop body scope.");
#endif
    // (可选) 恢复 break/continue 目标
    // codeGen.popLoopBlocks();

    // --- 10. 添加 PHI 节点的第二个传入边 (来自 latch) ---
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [10] Adding second incoming edges to PHI nodes (in block " + llvmObjToString(condBB) + "):");
#endif
    if (!loopTerminatedEarly)
    {
#ifdef DEBUG_WhileSTmt
        DEBUG_LOG("      Loop did not terminate early. Adding edges from Latch (" + llvmObjToString(latchBB) + ")");
#endif
        for (const auto& [name, phi] : phiNodes)
        {
            // 使用在 latchBB 确定分支前获取的值
            if (valueFromLatch.count(name))
            {
                llvm::Value* latchValue = valueFromLatch[name];
#ifdef DEBUG_WhileSTmt
                DEBUG_LOG("        Adding incoming edge to PHI '" + name + "' (" + llvmObjToString(phi) + "): [" + llvmObjToString(latchValue) + ", from " + llvmObjToString(latchBB) + "]");
#endif
                phi->addIncoming(latchValue, latchBB);  // 边2: 来自 latchBB
            }
            else
            {
                // 理论上不应发生，因为 valueFromLatch 是基于 phiNodes 构建的
#ifdef DEBUG_WhileSTmt
                DEBUG_LOG("        ERROR: Latch value missing for PHI node '" + name + "'. Adding UndefValue.");
#endif
                codeGen.logError("Internal error: Latch value missing for PHI node '" + name + "'.", stmt->line ? *stmt->line : 0, stmt->column ? *stmt->column : 0);
                phi->addIncoming(llvm::UndefValue::get(phi->getType()), latchBB);  // 避免崩溃
            }
        }
    }
    else
    {
#ifdef DEBUG_WhileSTmt
        DEBUG_LOG("      Loop terminated early. No second incoming edges added from latch block " + llvmObjToString(latchBB) + ".");
        // 注意：如果支持 'continue'，它也应该为 PHI 添加来自 continue 块的边。
        // 这个简化逻辑假设只有 'break' 或 'return' 会导致提前终止而不产生回边。
#endif
    }
    // 对于从 break 跳出的路径，它们直接跳到 endBB，不影响 condBB 的 PHI

    // --- 11. 循环结束块 (endBB) ---
    builder.SetInsertPoint(endBB);
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [11] Set insert point to End block (" + llvmObjToString(endBB) + ")");
    DEBUG_LOG("Exiting handleWhileStmt");
#endif

    // 循环结束后，符号表中被PHI覆盖的变量的值需要处理吗？
    // 不需要。后续代码如果访问这些变量，应该通过符号表获取到PHI节点本身。
    // PHI节点的值就是循环结束后的正确值。
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
    if (!typeGen->validateAssignment(varName, valueExpr))
    {
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
    if (pyCodeGen && codeGen.getSymbolTable().hasVariable(varName))
    {
        value = pyCodeGen->prepareAssignmentTarget(value, targetType, valueExpr);
        if (!value) return;
    }

    // 使用符号表的更新方法处理变量更新
    if (codeGen.getSymbolTable().hasVariable(varName))
    {
        codeGen.getSymbolTable().updateVariable(codeGen, varName, value, targetType, valueType);
    }
    else
    {
        codeGen.getSymbolTable().setVariable(varName, value, valueType->getObjectType());
        if (valueType && valueType->isReference())
        {
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