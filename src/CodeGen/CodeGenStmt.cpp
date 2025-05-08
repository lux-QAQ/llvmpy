#include "CodeGen/CodeGenStmt.h"
#include "CodeGen/CodeGenBase.h"
#include "CodeGen/CodeGenExpr.h"
#include "CodeGen/CodeGenType.h"
#include "CodeGen/CodeGenRuntime.h"
#include "CodeGen/CodeGenModule.h"  // 添加这一行
#include "CodeGen/CodeGenUtil.h"
#include "CodeGen/PyCodeGen.h"  // 添加这一行，确保 PyCodeGen 完整定义

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

#if defined(DEBUG_WhileSTmt) || defined(DEBUG_IfStmt)  // Define helpers if either debug flag is on
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
    stmtHandlers[ASTKind::BreakStmt] = [](CodeGenBase& cg, StmtAST* s)
    {
        static_cast<CodeGenStmt*>(cg.getStmtGen())->handleBreakStmt(static_cast<BreakStmtAST*>(s));
    };
    stmtHandlers[ASTKind::ContinueStmt] = [](CodeGenBase& cg, StmtAST* s)
    {
        static_cast<CodeGenStmt*>(cg.getStmtGen())->handleContinueStmt(static_cast<ContinueStmtAST*>(s));
    };
    stmtHandlers[ASTKind::ImportStmt] = [](CodeGenBase& cg, StmtAST* stmt)
    {
        static_cast<CodeGenStmt*>(cg.getStmtGen())->handleImportStmt(static_cast<ImportStmtAST*>(stmt));
    };

    stmtHandlers[ASTKind::ClassStmt] = [](CodeGenBase& cg, StmtAST* stmt)
    {
        static_cast<CodeGenStmt*>(cg.getStmtGen())->handleClassStmt(static_cast<ClassStmtAST*>(stmt));
    };
    // 注册 FunctionDefStmt 的处理器
    stmtHandlers[ASTKind::FunctionDefStmt] = [](CodeGenBase& cg, StmtAST* s)
    {
        static_cast<CodeGenStmt*>(cg.getStmtGen())->handleFunctionDefStmt(static_cast<FunctionDefStmtAST*>(s));
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

// 修改 handleBlock 签名和实现??
void CodeGenStmt::handleBlock(const std::vector<std::unique_ptr<StmtAST>>& stmts, bool createNewScope /* = true */)
{
    // 创建新作用域 (可选)
    if (createNewScope)
    {
#ifdef DEBUG_CODEGEN_STMT
        DEBUG_LOG("Scope: Pushing new scope in handleBlock.");
#endif
        beginScope();
    }
    else
    {
#ifdef DEBUG_CODEGEN_STMT
        DEBUG_LOG("Scope: Skipping new scope creation in handleBlock (createNewScope=false).");
#endif
    }

    // 处理块中的每个语句
    for (const auto& stmt : stmts)
    {
        handleStmt(stmt.get());

        // 检查基本块是否已经终止（例如由于return语句）
        llvm::BasicBlock* currentBlock = codeGen.getBuilder().GetInsertBlock();
        if (!currentBlock || currentBlock->getTerminator())
        {
#ifdef DEBUG_CODEGEN_STMT
            DEBUG_LOG("  Block terminated early after statement. Stopping block processing.");
#endif
            break;  // 不需要处理块中的后续语句
        }
    }

    // --- ADDED: Generate cleanups if scope ends normally ---
    llvm::BasicBlock* finalBlock = codeGen.getBuilder().GetInsertBlock();
    if (createNewScope && finalBlock && !finalBlock->getTerminator())
    {
        codeGen.getSymbolTable().generateScopeCleanups(codeGen);
    }

    // 关闭作用域 (可选)
    if (createNewScope)
    {
#ifdef DEBUG_CODEGEN_STMT
        DEBUG_LOG("Scope: Popping scope in handleBlock.");
#endif
        endScope();
    }
    else
    {
#ifdef DEBUG_CODEGEN_STMT
        DEBUG_LOG("Scope: Skipping scope pop in handleBlock (createNewScope=false).");
#endif
    }
}

void CodeGenStmt::beginScope()
{
    codeGen.getSymbolTable().pushScope();
}

void CodeGenStmt::endScope()
{
    codeGen.getSymbolTable().popScope(codeGen);
}

// --- 实现 handleFunctionDefStmt ---
// --- 实现 handleFunctionDefStmt ---
void CodeGenStmt::handleFunctionDefStmt(FunctionDefStmtAST* stmt)
{
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
    const FunctionAST* dbgFuncAST = stmt->getFunctionAST();
    std::string dbgPyFuncName = dbgFuncAST ? dbgFuncAST->getName() : "<null AST>";
    DEBUG_LOG_DETAIL("HdlFuncDefStmt", "Entering handleFunctionDefStmt for Python func '" + dbgPyFuncName + "'");
#endif
    // --- 获取 AST 信息 ---
    const FunctionAST* funcAST = stmt->getFunctionAST();
    if (!funcAST)
    {
        codeGen.logError("Null FunctionAST in FunctionDefStmt", stmt->line.value_or(0), stmt->column.value_or(0));
        return;
    }
    std::string pyFuncName = funcAST->getName();  // Python name for binding
    int line = stmt->line.value_or(0);
    int col = stmt->column.value_or(0);

    auto& builder = codeGen.getBuilder();
    auto& context = codeGen.getContext();
    auto* runtime = codeGen.getRuntimeGen();
    auto& symTable = codeGen.getSymbolTable();                         // Get symbol table
    llvm::Type* pyObjectPtrType = llvm::PointerType::get(context, 0);  // PyObject*

    // --- 将 FunctionAST 注册到符号表 ---
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
    DEBUG_LOG_DETAIL("HdlFuncDefStmt", "Defining FunctionAST '" + pyFuncName + "' in current symbol table scope.");
#endif
    symTable.defineFunctionAST(pyFuncName, funcAST);
// --- 步骤 1: 调用 CodeGenModule 生成唯一的 LLVM 函数 ---
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
    DEBUG_LOG_DETAIL("HdlFuncDefStmt", "Calling ModuleGen->handleFunctionDef for '" + pyFuncName + "'...");
#endif
    llvm::Function* llvmFunc = codeGen.getModuleGen()->handleFunctionDef(funcAST);  // Gets the unique LLVM function (e.g., @test.L4.C8)
    if (!llvmFunc)
    {
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
        DEBUG_LOG_DETAIL("HdlFuncDefStmt", "ModuleGen->handleFunctionDef FAILED for '" + pyFuncName + "'.");
#endif
        return;  // Error logged by handleFunctionDef
    }
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
    std::string uniqueLLVMName = llvmFunc->getName().str();
    DEBUG_LOG_DETAIL("HdlFuncDefStmt", "ModuleGen->handleFunctionDef SUCCEEDED for '" + pyFuncName + "'. Got LLVM Func: " + llvmObjToString(llvmFunc) + " ('" + uniqueLLVMName + "')");
#endif
// --- 步骤 2: 调用 CodeGenType 获取 Python 函数类型 ---
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
    DEBUG_LOG_DETAIL("HdlFuncDefStmt", "Calling TypeGen->getFunctionObjectType for '" + pyFuncName + "'...");
#endif
    ObjectType* funcObjectType = codeGen.getTypeGen()->getFunctionObjectType(funcAST);
    if (!funcObjectType || funcObjectType->getCategory() != ObjectType::Function)
    {
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
        DEBUG_LOG_DETAIL("HdlFuncDefStmt", "TypeGen->getFunctionObjectType FAILED for '" + pyFuncName + "'.");
#endif
        codeGen.logError("Failed to get valid FunctionType for function: " + pyFuncName, line, col);
        return;
    }
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
    DEBUG_LOG_DETAIL("HdlFuncDefStmt", "TypeGen->getFunctionObjectType SUCCEEDED for '" + pyFuncName + "'. Type: " + funcObjectType->getName());
#endif
// --- 步骤 3: 调用 CodeGenRuntime 创建 Python 函数对象 ---
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
    DEBUG_LOG_DETAIL("HdlFuncDefStmt", "Calling RuntimeGen->createFunctionObject using LLVM func '" + llvmFunc->getName().str() + "'...");
#endif
    llvm::Value* pyFuncObj = runtime->createFunctionObject(llvmFunc, funcObjectType);  // Wraps the unique LLVM function
    if (!pyFuncObj)
    {
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
        DEBUG_LOG_DETAIL("HdlFuncDefStmt", "RuntimeGen->createFunctionObject FAILED for '" + pyFuncName + "'.");
#endif
        codeGen.logError("Failed to create Python function object for: " + pyFuncName, line, col);
        return;
    }
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
    DEBUG_LOG_DETAIL("HdlFuncDefStmt", "RuntimeGen->createFunctionObject SUCCEEDED for '" + pyFuncName + "'. Value: " + llvmObjToString(pyFuncObj));
#endif

    // --- 步骤 4: 确定存储位置 (Alloca 或 GlobalVariable) ---
    llvm::Function* currentCodeGenFunction = codeGen.getCurrentFunction();
    bool isTopLevel = !currentCodeGenFunction || currentCodeGenFunction->getName() == "__llvmpy_entry";
    llvm::Value* storage = nullptr;  // Pointer to the storage location (AllocaInst* or GlobalVariable*)

    if (isTopLevel)
    {
// --- 顶层函数：查找或创建 GlobalVariable ---
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
        DEBUG_LOG_DETAIL("HdlFuncDefStmt", "Function '" + pyFuncName + "' is top-level. Looking for/Creating GlobalVariable.");
#endif
        std::string gvName = pyFuncName + "_obj_gv";  // Name for the global variable pointer
        storage = codeGen.getModule()->getGlobalVariable(gvName);
        if (!storage)
        {
            llvm::GlobalVariable* funcObjGV = new llvm::GlobalVariable(
                    *codeGen.getModule(),
                    pyObjectPtrType,                                // Type of the global is PyObject*
                    false,                                          // isConstant = false (value can change)
                    llvm::GlobalValue::InternalLinkage,             // Linkage
                    llvm::Constant::getNullValue(pyObjectPtrType),  // Initializer = null
                    gvName);
            // Use None since the name is important for lookup
            funcObjGV->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::None);
            storage = funcObjGV;
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
            DEBUG_LOG_DETAIL("HdlFuncDefStmt", "Created GlobalVariable: " + llvmObjToString(storage) + " in Module@ " + std::to_string(reinterpret_cast<uintptr_t>(codeGen.getModule())));
#endif
        }
        else
        {
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
            DEBUG_LOG_DETAIL("HdlFuncDefStmt", "Found existing GlobalVariable: " + llvmObjToString(storage));
#endif
            // Ensure the type matches if GV already exists
            if (auto* existingGV = llvm::dyn_cast<llvm::GlobalVariable>(storage))
            {
                if (existingGV->getValueType() != pyObjectPtrType)
                {
                    codeGen.logError("GlobalVariable '" + gvName + "' exists but holds the wrong type (expected PyObject*).", line, col);
                    return;
                }
            }
            else
            {
                // 如果 storage 不是 GlobalVariable*，这是一个内部错误
                codeGen.logError("Internal error: Found storage for '" + gvName + "' but it's not a GlobalVariable.", line, col);
                return;
            }
        }

        // Store the new function object into the GlobalVariable
        llvm::BasicBlock* currentBlock = builder.GetInsertBlock();
        if (currentBlock && currentCodeGenFunction)  // Store in current function if possible
        {
            // TODO: Handle DecRef for the *previous* value in the GV if overwriting?
            // This is complex for GVs initialized potentially by ctors.
            // For simplicity, assume the GV just holds the latest reference.
            builder.CreateStore(pyFuncObj, storage);
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
            DEBUG_LOG_DETAIL("HdlFuncDefStmt", "Stored PyObject* into GlobalVariable: " + llvmObjToString(storage) + " in block " + llvmObjToString(currentBlock));
#endif
        }
        else
        {
            // TODO: Store in global constructor if not in a function context.
            codeGen.logWarning("Cannot find insert block to store GlobalVariable for top-level function " + pyFuncName + ". Storage might be delayed or require global ctor.", line, col);
            // Fallback: Try to add to global ctor (complex, needs careful handling)
        }
        // GV now holds the reference returned by createFunctionObject (+1). No extra IncRef needed here.
    }
    else  // isTopLevel == false (Nested function)
    {
// --- 嵌套函数：查找或创建 AllocaInst ---
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
        DEBUG_LOG_DETAIL("HdlFuncDefStmt", "Function '" + pyFuncName + "' is nested. Looking for/Creating AllocaInst.");
#endif

        llvm::AllocaInst* funcObjAlloca = nullptr;
        bool isNewAlloca = false;  // <<<--- 添加标志位

        // 1. Check if variable already exists in the current or parent scopes
        if (symTable.hasVariable(pyFuncName))
        {
            llvm::Value* existingVar = symTable.getVariable(pyFuncName);
            // Ensure it's an AllocaInst (could be a function arg or other value)
            if (llvm::isa<llvm::AllocaInst>(existingVar))
            {
                funcObjAlloca = llvm::cast<llvm::AllocaInst>(existingVar);
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
                DEBUG_LOG_DETAIL("HdlFuncDefStmt", "Found existing AllocaInst for '" + pyFuncName + "': " + llvmObjToString(funcObjAlloca));
#endif
                // Ensure type matches
                if (funcObjAlloca->getAllocatedType() != pyObjectPtrType)
                {
                    codeGen.logError("AllocaInst '" + pyFuncName + "' exists with wrong type.", line, col);
                    return;
                }
                isNewAlloca = false;  // <<<--- 标记为复用
            }
            else
            {
// Variable exists but isn't an Alloca (e.g., function parameter).
// Python allows shadowing parameters with local defs. Create a *new* alloca.
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
                DEBUG_LOG_DETAIL("HdlFuncDefStmt", "Variable '" + pyFuncName + "' exists but is not Alloca. Creating new Alloca for shadowing.");
#endif
                funcObjAlloca = codeGen.createEntryBlockAlloca(pyObjectPtrType, pyFuncName);
                if (!funcObjAlloca) return;  // Error logged by createEntryBlockAlloca
                isNewAlloca = true;          // <<<--- 标记为新建
            }
        }
        else
        {
// 2. Variable doesn't exist, create a new AllocaInst
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
            DEBUG_LOG_DETAIL("HdlFuncDefStmt", "No existing variable/AllocaInst found for '" + pyFuncName + "'. Creating new AllocaInst.");
#endif
            funcObjAlloca = codeGen.createEntryBlockAlloca(pyObjectPtrType, pyFuncName);
            if (!funcObjAlloca) return;  // Error logged by createEntryBlockAlloca
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
            DEBUG_LOG_DETAIL("HdlFuncDefStmt", "Created new AllocaInst for '" + pyFuncName + "': " + llvmObjToString(funcObjAlloca));
#endif
            isNewAlloca = true;  // <<<--- 标记为新建
        }
        storage = funcObjAlloca;  // Storage location is the alloca

        // 3. Store the new function object into the alloca, handling overwrites
        llvm::BasicBlock* currentBlock = builder.GetInsertBlock();
        if (currentBlock)
        {
            // --- MODIFICATION: Only handle overwrite if alloca is NOT new ---
            if (!isNewAlloca)
            {
                // --- Overwrite Handling: DecRef old value ---
                llvm::Value* oldValue = builder.CreateLoad(pyObjectPtrType, funcObjAlloca, pyFuncName + ".old");
                llvm::Value* isNotNull = builder.CreateIsNotNull(oldValue, "isOldValueNotNull");
                llvm::BasicBlock* decRefBlock = codeGen.createBasicBlock("decRefOldFunc", currentCodeGenFunction);
                llvm::BasicBlock* storeNewBlock = codeGen.createBasicBlock("storeNewFunc", currentCodeGenFunction);  // Need continuation block regardless

                builder.CreateCondBr(isNotNull, decRefBlock, storeNewBlock);  // Branch to decref or directly to store

                // DecRef Block:
                builder.SetInsertPoint(decRefBlock);
                runtime->decRef(oldValue);
                builder.CreateBr(storeNewBlock);  // Branch to store after decref

                // Store New Block: (Builder now points here)
                builder.SetInsertPoint(storeNewBlock);
            }
            // --- END MODIFICATION ---
            // If it was a new alloca, the builder's insert point is still where it was,
            // and we directly proceed to store the new value without the decref logic.

            // Store the *new* function object
            builder.CreateStore(pyFuncObj, funcObjAlloca);
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
            DEBUG_LOG_DETAIL("HdlFuncDefStmt", "Stored PyObject* (" + llvmObjToString(pyFuncObj) + ") into AllocaInst: " + llvmObjToString(funcObjAlloca) + " in block " + llvmObjToString(builder.GetInsertBlock()));  // Use current block
#endif

            // Increment ref count of the *new* object...
            runtime->incRef(pyFuncObj);
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
            DEBUG_LOG_DETAIL("HdlFuncDefStmt", "IncRef'd new PyObject* (" + llvmObjToString(pyFuncObj) + ") for nested function storage.");
#endif
        }
        else
        {
            codeGen.logError("Cannot find insert block to store nested function object " + pyFuncName, line, col);
            storage = nullptr;  // Mark storage as failed
        }
    }

    // --- 步骤 5: 调用 PySymbolTable 绑定/更新存储位置 ---
    if (storage)
    {
// Bind the Python name `pyFuncName` to the `storage` location (AllocaInst* or GlobalVariable*)
// This handles both initial definition and re-binding (overwriting).
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
        DEBUG_LOG_DETAIL("HdlFuncDefStmt", "Binding/Rebinding SymbolTable entry for '" + pyFuncName + "' to storage: " + llvmObjToString(storage));
#endif
        symTable.setVariable(pyFuncName, storage, funcObjectType);
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
        DEBUG_LOG_DETAIL("HdlFuncDefStmt", "SymbolTable binding confirmed/updated for '" + pyFuncName + "'.");
#endif
    }
    else
    {
#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
        DEBUG_LOG_DETAIL("HdlFuncDefStmt", "Skipping SymbolTable update for '" + pyFuncName + "' due to storage failure.");
#endif
    }

#ifdef DEBUG_CODEGEN_handleFunctionDefStmt
    DEBUG_LOG_DETAIL("HdlFuncDefStmt", "Leaving handleFunctionDefStmt for '" + pyFuncName + "'");
#endif

    // Reference Counting Summary:
    // - py_create_function returns +1 ref (pyFuncObj).
    // - Top-level: GV holds the ref. Overwriting GV needs careful DecRef (currently simplified).
    // - Nested:
    //    - Overwrite: Load old (+0), DecRef old (-1), Store new (+0), IncRef new (+1). Net: Alloca holds +1 ref to new obj.
    //    - New: Store new (+0), IncRef new (+1). Net: Alloca holds +1 ref to new obj.
    // - SymbolTable::popScope needs to load from AllocaInsts and DecRef them.
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

void CodeGenStmt::handleIfStmtRecursive(IfStmtAST* stmt, llvm::BasicBlock* finalMergeBB)
{
#ifdef DEBUG_IfStmt
    static int indentLevel = 0;  // 注意：静态变量在递归中共享，可能导致日志缩进混乱，仅用于简单调试
    std::string indent(indentLevel * 2, ' ');
    DEBUG_LOG(indent + "-> Entering handleIfStmtRecursive");
    DEBUG_LOG(indent + "   Target finalMergeBB: " + llvmObjToString(finalMergeBB));
    indentLevel++;
#endif

    auto& builder = codeGen.getBuilder();
    llvm::Function* func = codeGen.getCurrentFunction();

    // 1. 处理条件
#ifdef DEBUG_IfStmt
    DEBUG_LOG(indent + "   [1] Handling condition...");
#endif
    llvm::Value* condValue = handleCondition(stmt->getCondition());
    if (!condValue)
    {
#ifdef DEBUG_IfStmt
        DEBUG_LOG(indent + "   [1] Condition generation FAILED. Returning.");
        indentLevel--;
        DEBUG_LOG(indent + "<- Leaving handleIfStmtRecursive (condition failed)");
#endif
        return;
    }
#ifdef DEBUG_IfStmt
    DEBUG_LOG(indent + "   [1] Condition Value: " + llvmObjToString(condValue));
#endif

    // 2. 创建 then 和 else 入口块
    llvm::BasicBlock* thenBB = codeGen.createBasicBlock("then", func);
    llvm::BasicBlock* elseEntryBB = codeGen.createBasicBlock("else", func);
#ifdef DEBUG_IfStmt
    DEBUG_LOG(indent + "   [2] Created blocks: thenBB=" + llvmObjToString(thenBB) + ", elseEntryBB=" + llvmObjToString(elseEntryBB));
#endif

    // 3. 创建条件跳转
    llvm::BasicBlock* currentCondBlock = builder.GetInsertBlock();  // 保存条件判断结束时的块
#ifdef DEBUG_IfStmt
    DEBUG_LOG(indent + "   [3] Creating CondBr from " + llvmObjToString(currentCondBlock) + " on " + llvmObjToString(condValue) + " ? " + llvmObjToString(thenBB) + " : " + llvmObjToString(elseEntryBB));
#endif
    generateBranchingCode(condValue, thenBB, elseEntryBB);

    // 4. 处理 then 分支
#ifdef DEBUG_IfStmt
    DEBUG_LOG(indent + "   [4] Handling 'then' branch (Block: " + llvmObjToString(thenBB) + ")");
#endif
    builder.SetInsertPoint(thenBB);
#ifdef DEBUG_IfStmt
    DEBUG_LOG(indent + "       Set insert point to: " + llvmObjToString(thenBB));
    DEBUG_LOG(indent + "       Calling handleBlock(..., createNewScope=false) for thenBody...");
#endif
    handleBlock(stmt->getThenBody(), false);                    // <<<--- 修改：不创建新作用域
    llvm::BasicBlock* thenEndBlock = builder.GetInsertBlock();  // 获取 then 块结束时的块
#ifdef DEBUG_IfStmt
    DEBUG_LOG(indent + "       Returned from handleBlock for thenBody. Current block: " + llvmObjToString(thenEndBlock));
#endif
    // 如果 then 分支没有终止，跳转到最终合并块
    if (thenEndBlock && !thenEndBlock->getTerminator())
    {
#ifdef DEBUG_IfStmt
        DEBUG_LOG(indent + "       'then' block (" + llvmObjToString(thenEndBlock) + ") did not terminate. Creating Br to finalMergeBB (" + llvmObjToString(finalMergeBB) + ")");
#endif
        builder.CreateBr(finalMergeBB);
    }
#ifdef DEBUG_IfStmt
    else if (thenEndBlock)
    {
        DEBUG_LOG(indent + "       'then' block (" + llvmObjToString(thenEndBlock) + ") already terminated.");
    }
    else
    {
        DEBUG_LOG(indent + "       'then' branch ended with no valid insert block (likely due to return/unreachable).");
    }
#endif

    // 5. 处理 else 分支 (可能是 elif 或 final else)
#ifdef DEBUG_IfStmt
    DEBUG_LOG(indent + "   [5] Handling 'else'/'elif' part (Entry Block: " + llvmObjToString(elseEntryBB) + ")");
#endif
    builder.SetInsertPoint(elseEntryBB);
#ifdef DEBUG_IfStmt
    DEBUG_LOG(indent + "       Set insert point to: " + llvmObjToString(elseEntryBB));
#endif

    StmtAST* elseStmt = stmt->getElseStmt();

    if (elseStmt)
    {
#ifdef DEBUG_IfStmt
        DEBUG_LOG(indent + "       Else statement exists (not null). Kind: " + std::to_string(static_cast<int>(elseStmt->kind())));
#endif
        if (auto* nextIf = dynamic_cast<IfStmtAST*>(elseStmt))
        {
            // Elif case: recursive call
#ifdef DEBUG_IfStmt
            DEBUG_LOG(indent + "       Else statement is IfStmtAST (elif). Making recursive call...");
#endif
            handleIfStmtRecursive(nextIf, finalMergeBB);
#ifdef DEBUG_IfStmt
            DEBUG_LOG(indent + "       Returned from recursive call for elif.");
            indentLevel--;
            DEBUG_LOG(indent + "<- Leaving handleIfStmtRecursive (elif handled)");
#endif
            return;  // Crucial: return after handling elif recursively
        }
        else
        {
            // Final else case
#ifdef DEBUG_IfStmt
            DEBUG_LOG(indent + "       Else statement is NOT IfStmtAST. Treating as final 'else' block.");
#endif
            if (auto* elseBlock = dynamic_cast<BlockStmtAST*>(elseStmt))
            {
#ifdef DEBUG_IfStmt
                DEBUG_LOG(indent + "           Else statement is BlockStmtAST. Calling handleBlock(..., createNewScope=false)...");
#endif
                handleBlock(elseBlock->getStatements(), false);  // <<<--- 修改：不创建新作用域
#ifdef DEBUG_IfStmt
                DEBUG_LOG(indent + "           Returned from handleBlock for else block. Current block: " + llvmObjToString(builder.GetInsertBlock()));
#endif
            }
            else
            {
                // Single statement else
#ifdef DEBUG_IfStmt
                DEBUG_LOG(indent + "           Else statement is a single statement. Calling handleStmt...");
#endif
                handleStmt(elseStmt);  // 单个语句不需要作用域管理
#ifdef DEBUG_IfStmt
                DEBUG_LOG(indent + "           Returned from handleStmt for else statement. Current block: " + llvmObjToString(builder.GetInsertBlock()));
#endif
            }

            llvm::BasicBlock* elseEndBlock = builder.GetInsertBlock();  // 获取 else 块结束时的块
            // 如果 else 分支没有终止，跳转到最终合并块
            if (elseEndBlock && !elseEndBlock->getTerminator())
            {
#ifdef DEBUG_IfStmt
                DEBUG_LOG(indent + "       Final 'else' block (" + llvmObjToString(elseEndBlock) + ") did not terminate. Creating Br to finalMergeBB (" + llvmObjToString(finalMergeBB) + ")");
#endif
                builder.CreateBr(finalMergeBB);
            }
#ifdef DEBUG_IfStmt
            else if (elseEndBlock)
            {
                DEBUG_LOG(indent + "       Final 'else' block (" + llvmObjToString(elseEndBlock) + ") already terminated.");
            }
            else
            {
                DEBUG_LOG(indent + "       Final 'else' branch ended with no valid insert block.");
            }
#endif
        }
    }
    else
    {
        // No else/elif part
#ifdef DEBUG_IfStmt
        DEBUG_LOG(indent + "       Else statement is null (no else/elif). Creating direct Br from elseEntryBB (" + llvmObjToString(elseEntryBB) + ") to finalMergeBB (" + llvmObjToString(finalMergeBB) + ")");
#endif
        builder.CreateBr(finalMergeBB);  // 直接从 else 入口跳到合并块
    }

#ifdef DEBUG_IfStmt
    indentLevel--;
    DEBUG_LOG(indent + "<- Leaving handleIfStmtRecursive (normal exit)");
#endif
}
// 公共入口函数
// 公共入口函数
void CodeGenStmt::handleIfStmt(IfStmtAST* stmt)
{
#ifdef DEBUG_IfStmt
    DEBUG_LOG("-> Entering handleIfStmt (Public Entry)");
#endif
    auto& builder = codeGen.getBuilder();
    llvm::Function* func = codeGen.getCurrentFunction();
    if (!func)
    {
        codeGen.logError("Cannot generate if statement outside a function.", stmt->line ? *stmt->line : 0, stmt->column ? *stmt->column : 0);
#ifdef DEBUG_IfStmt
        DEBUG_LOG("   ERROR: Not inside a function. Aborting.");
        DEBUG_LOG("<- Leaving handleIfStmt (error)");
#endif
        return;
    }

    // 1. 创建整个 if-elif-else 链条最终的合并块
    llvm::BasicBlock* finalMergeBB = codeGen.createBasicBlock("ifcont", func);
#ifdef DEBUG_IfStmt
    DEBUG_LOG("   [1] Created finalMergeBB: " + llvmObjToString(finalMergeBB));
#endif

    // 记录调用前的插入块，用于后续判断
    llvm::BasicBlock* originalInsertBB = builder.GetInsertBlock();
    bool originalBlockTerminated = originalInsertBB && originalInsertBB->getTerminator();
#ifdef DEBUG_IfStmt
    DEBUG_LOG("   [Pre] Original Insert BB: " + llvmObjToString(originalInsertBB) + ", Terminated: " + (originalBlockTerminated ? "Yes" : "No"));
#endif

    // 2. 开始递归处理
#ifdef DEBUG_IfStmt
    DEBUG_LOG("   [2] Calling handleIfStmtRecursive...");
#endif
    handleIfStmtRecursive(stmt, finalMergeBB);
#ifdef DEBUG_IfStmt
    DEBUG_LOG("   [2] Returned from handleIfStmtRecursive. Current block: " + llvmObjToString(builder.GetInsertBlock()));
#endif

    // 3. 将插入点设置到最终的合并块，继续生成后续代码
#ifdef DEBUG_IfStmt
    DEBUG_LOG("   [3] Checking reachability of finalMergeBB (" + llvmObjToString(finalMergeBB) + ")");
#endif
    bool hasUses = !finalMergeBB->use_empty();
    bool hasPreds = llvm::pred_begin(finalMergeBB) != llvm::pred_end(finalMergeBB);
#ifdef DEBUG_IfStmt
    DEBUG_LOG("       Has Uses: " + std::string(hasUses ? "Yes" : "No"));
    DEBUG_LOG("       Has Predecessors: " + std::string(hasPreds ? "Yes" : "No"));
#endif
    if (hasUses || hasPreds)
    {
        // 如果 finalMergeBB 有前驱块或用途，说明它是可达的
#ifdef DEBUG_IfStmt
        DEBUG_LOG("       finalMergeBB is reachable. Setting insert point.");
#endif
        builder.SetInsertPoint(finalMergeBB);
    }
    else if (!originalBlockTerminated)
    {
        // 如果 finalMergeBB 没有前驱/用途，但原始块也没有终止
#ifdef DEBUG_IfStmt
        DEBUG_LOG("       finalMergeBB is unreachable, but original block was not terminated. Erasing finalMergeBB.");
        DEBUG_LOG("       WARNING: Builder insert point might be invalid now.");
#endif
        finalMergeBB->eraseFromParent();
        // 注意：此时 builder 的插入点可能处于无效状态
    }
    else
    {
        // 如果 finalMergeBB 没有前驱/用途，并且原始块已经终止
#ifdef DEBUG_IfStmt
        DEBUG_LOG("       finalMergeBB is unreachable, and original block was terminated. Erasing finalMergeBB as dead code.");
#endif
        finalMergeBB->eraseFromParent();
    }

#ifdef DEBUG_IfStmt
    DEBUG_LOG("<- Leaving handleIfStmt (Public Entry). Final insert point: " + llvmObjToString(builder.GetInsertBlock()));
#endif
}

// 辅助函数：递归查找在给定语句列表（或单个语句）中被赋值的变量名
void CodeGenStmt::findAssignedVariablesInStmt(StmtAST* stmt, std::set<std::string>& assignedVars)
{
    if (!stmt) return;  // Base case: null statement

    if (auto* assignStmt = dynamic_cast<AssignStmtAST*>(stmt))
    {
        assignedVars.insert(assignStmt->getName());
    }
    else if (auto* indexAssignStmt = dynamic_cast<IndexAssignStmtAST*>(stmt))
    {
        // If a[i] = ... should mark 'a' as assigned, more complex analysis is needed.
        // For now, assume it doesn't assign to a simple variable name directly
        // relevant for PHI nodes. If needed, extract the base variable name from
        // indexAssignStmt->getTarget() if it's a VariableExprAST.
        // Example (if needed later):
        // if (auto* targetVar = dynamic_cast<VariableExprAST*>(indexAssignStmt->getTarget())) {
        //     assignedVars.insert(targetVar->getName());
        // }
    }
    else if (auto* ifStmt = dynamic_cast<IfStmtAST*>(stmt))
    {
        // Recurse into the 'then' body (which is a vector)
        for (const auto& s : ifStmt->getThenBody())
        {
            findAssignedVariablesInStmt(s.get(), assignedVars);
        }
        // Recurse into the 'else' statement (which is a single StmtAST*)
        // The recursive call handles if it's null, another IfStmt, a BlockStmt, etc.
        findAssignedVariablesInStmt(ifStmt->getElseStmt(), assignedVars);  // <--- FIX: Use getElseStmt()
    }
    else if (auto* whileStmt = dynamic_cast<WhileStmtAST*>(stmt))
    {
        // Recurse into the loop body (which is a vector)
        for (const auto& s : whileStmt->getBody())
        {
            findAssignedVariablesInStmt(s.get(), assignedVars);
        }
    }
    // --- ADDED: Handle BlockStmtAST explicitly ---
    else if (auto* blockStmt = dynamic_cast<BlockStmtAST*>(stmt))
    {
        for (const auto& s : blockStmt->getStatements())
        {
            findAssignedVariablesInStmt(s.get(), assignedVars);
        }
    }
    // Add cases for other compound statements (ForStmt, FunctionDef, ClassDef?)
    // if they can contain assignments relevant to outer scopes (unlikely for Func/Class defs).
    // For example, if you add ForStmtAST:
    // else if (auto* forStmt = dynamic_cast<ForStmtAST*>(stmt)) {
    //     // Handle assignment to loop variable if necessary
    //     // assignedVars.insert(forStmt->getLoopVariable());
    //     for (const auto& s : forStmt->getBody()) {
    //         findAssignedVariablesInStmt(s.get(), assignedVars);
    //     }
    // }
}

// 修改现有方法

void CodeGenStmt::handleBreakStmt(BreakStmtAST* stmt)
{
#ifdef DEBUG_CODEGEN_handleBreakStmt  // Optional Debugging
    DEBUG_LOG("Entering handleBreakStmt");
#endif
    if (breakTargetStack.empty())
    {
        codeGen.logError("'break' outside loop", stmt->line.value_or(0), stmt->column.value_or(0));
        return;
    }
    llvm::BasicBlock* breakTarget = breakTargetStack.back();
    codeGen.getBuilder().CreateBr(breakTarget);
#ifdef DEBUG_CODEGEN_handleBreakStmt
    DEBUG_LOG("  Generated branch to break target: " + llvmObjToString(breakTarget));
#endif
    // 当前块已被终止，不需要进一步操作
}

void CodeGenStmt::handleContinueStmt(ContinueStmtAST* stmt)
{
#ifdef DEBUG_CODEGEN_ContinueStmt  // Optional Debugging
    DEBUG_LOG("Entering handleContinueStmt");
#endif
    if (continueTargetStack.empty())
    {
        codeGen.logError("'continue' outside loop", stmt->line.value_or(0), stmt->column.value_or(0));
        return;
    }
    llvm::BasicBlock* continueTarget = continueTargetStack.back();

    // 在跳转回循环条件/增量之前，清理当前迭代中产生的临时对象
    codeGen.getRuntimeGen()->cleanupTemporaryObjects();
#ifdef DEBUG_CODEGEN_ContinueStmt
    DEBUG_LOG("  Cleaned up temporary objects before continue.");
#endif

    codeGen.getBuilder().CreateBr(continueTarget);
#ifdef DEBUG_CODEGEN_ContinueStmt
    DEBUG_LOG("  Generated branch to continue target: " + llvmObjToString(continueTarget));
#endif
    // 当前块已被终止，不需要进一步操作
}
void CodeGenStmt::pushBreakTarget(llvm::BasicBlock* target)
{
    breakTargetStack.push_back(target);
}

void CodeGenStmt::popBreakTarget()
{
    if (!breakTargetStack.empty())
    {
        breakTargetStack.pop_back();
    }
    else
    {
        // 可以在这里添加错误处理或日志记录，如果需要的话
        codeGen.logError("Internal error: Popping from empty break target stack.");
    }
}

void CodeGenStmt::pushContinueTarget(llvm::BasicBlock* target)
{
    continueTargetStack.push_back(target);
}

void CodeGenStmt::popContinueTarget()
{
    if (!continueTargetStack.empty())
    {
        continueTargetStack.pop_back();
    }
    else
    {
        // 可以在这里添加错误处理或日志记录，如果需要的话
        codeGen.logError("Internal error: Popping from empty continue target stack.");
    }
}
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
    llvm::Type* pyObjectPtrType = llvm::PointerType::get(context, 0);  // PyObject*

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
    llvm::BasicBlock* latchBB = codeGen.createBasicBlock("while.latch", func);
    llvm::BasicBlock* elseBB = nullptr;  // <-- 新增：else 块 (如果存在)
    llvm::BasicBlock* endBB = codeGen.createBasicBlock("while.end", func);

    // --- NEW: 创建 else 块 (如果 AST 中存在) ---
    if (stmt->getElseStmt())
    {
        elseBB = codeGen.createBasicBlock("while.else", func);
#ifdef DEBUG_WhileSTmt
        DEBUG_LOG("  [1a] Created Else block: " + llvmObjToString(elseBB));
#endif
    }

    // --- Loop control flow targets ---
    llvm::BasicBlock* continueTargetBB = latchBB;
    llvm::BasicBlock* breakTargetBB = endBB;  // 'break' 始终跳过 else 块，直接到 end

#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [1] Created Basic Blocks:");
    DEBUG_LOG("      Preheader: " + llvmObjToString(preheaderBB));
    DEBUG_LOG("      Condition: " + llvmObjToString(condBB));
    DEBUG_LOG("      Body:      " + llvmObjToString(bodyBB));
    DEBUG_LOG("      Latch:     " + llvmObjToString(latchBB));
    if (elseBB)
    {  // <-- Log elseBB if created
        DEBUG_LOG("      Else:      " + llvmObjToString(elseBB));
    }
    DEBUG_LOG("      End:       " + llvmObjToString(endBB));
    DEBUG_LOG("      Continue Target: " + llvmObjToString(continueTargetBB));
    DEBUG_LOG("      Break Target:    " + llvmObjToString(breakTargetBB));
#endif

    // --- Push loop targets onto stacks ---
    pushBreakTarget(breakTargetBB);
    pushContinueTarget(continueTargetBB);
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  Pushed loop targets onto stacks.");
#endif

    // --- 2. 识别循环中修改的变量 ---
    std::set<std::string> assignedInBody;
    for (const auto& bodyStmt : stmt->getBody())
    {
        findAssignedVariablesInStmt(bodyStmt.get(), assignedInBody);
    }
    // --- NEW: Also check variables assigned in the else block ---
    if (stmt->getElseStmt())
    {
        findAssignedVariablesInStmt(stmt->getElseStmt(), assignedInBody);
    }

#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [2] Variables assigned in body or else:");  // <-- Updated log message
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

    // 保存循环前的信息
    std::map<std::string, llvm::Value*> storageBeforeLoop;
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
        llvm::Value* storage = symTable.getVariable(varName);  // Lookup in current scope (preheader)
        ObjectType* varType = symTable.getVariableType(varName);

        if (storage && varType)
        {
            storageBeforeLoop[varName] = storage;
            typeBeforeLoop[varName] = varType;

            // Load the initial value *in the preheader* to feed the PHI
            llvm::Instruction* loadInst = nullptr;
            llvm::IRBuilder<> preheaderBuilder(preheaderBB->getTerminator());  // Build in preheader's terminator position

            if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(storage))
            {
                if (allocaInst->getAllocatedType() != pyObjectPtrType)
                {
                    // --- ERROR HANDLING ---
#ifdef DEBUG_WhileSTmt
                    DEBUG_LOG("      ERROR: AllocaInst for '" + varName + "' has wrong type (" + llvmObjToString(allocaInst->getAllocatedType()) + "), expected PyObject*. Skipping PHI.");
#endif
                    codeGen.logError("Internal error: AllocaInst for '" + varName + "' in loop preheader has unexpected type.", stmt->line.value_or(0), stmt->column.value_or(0));
                    // --- END ERROR HANDLING ---
                    continue;  // Skip PHI for this variable
                }
                loadInst = preheaderBuilder.CreateLoad(pyObjectPtrType, allocaInst, varName + "_init");
            }
            else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(storage))
            {
                if (gv->getValueType() != pyObjectPtrType)
                {
                    // --- ERROR HANDLING ---
#ifdef DEBUG_WhileSTmt
                    DEBUG_LOG("      ERROR: GlobalVariable for '" + varName + "' has wrong type (" + llvmObjToString(gv->getValueType()) + "), expected PyObject*. Skipping PHI.");
#endif
                    codeGen.logError("Internal error: GlobalVariable for '" + varName + "' in loop preheader has unexpected type.", stmt->line.value_or(0), stmt->column.value_or(0));
                    // --- END ERROR HANDLING ---
                    continue;  // Skip PHI for this variable
                }
                loadInst = preheaderBuilder.CreateLoad(pyObjectPtrType, gv, varName + "_init_global");
            }
            else
            {
                // --- ERROR HANDLING ---
#ifdef DEBUG_WhileSTmt
                DEBUG_LOG("      ERROR: Storage for '" + varName + "' is neither AllocaInst nor GlobalVariable. Skipping PHI. Storage: " + llvmObjToString(storage));
#endif
                codeGen.logError("Internal error: Unexpected storage type for variable '" + varName + "' in loop preheader.", stmt->line.value_or(0), stmt->column.value_or(0));
                // --- END ERROR HANDLING ---
                continue;  // Skip PHI for this variable
            }

#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("      Processing variable: '" + varName + "'");
            DEBUG_LOG("        Storage (from preheader scope): " + llvmObjToString(storage));
            DEBUG_LOG("        Initial Value (loaded in preheader): " + llvmObjToString(loadInst));
            DEBUG_LOG("        Initial Type: " + (varType ? varType->getName() : "<null ObjectType>"));
            DEBUG_LOG("        LLVM Type for PHI: " + llvmObjToString(pyObjectPtrType));
#endif
            // 创建 PHI 节点，预期有 2 个传入边 (preheader, latch)
            // Note: If an else block exists and modifies the variable, the PHI might need more inputs,
            // but standard loop structure only needs preheader and latch. Else block doesn't loop back.
            llvm::PHINode* phi = builder.CreatePHI(pyObjectPtrType, 2, varName + ".phi");
#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("        Created PHI node: " + llvmObjToString(phi));
#endif
            // 添加来自 preheader 的第一个传入边
            phi->addIncoming(loadInst, preheaderBB);
#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("        Added incoming edge to PHI: [" + llvmObjToString(loadInst) + ", from " + llvmObjToString(preheaderBB) + "]");
#endif
            phiNodes[varName] = phi;
        }
        else
        {
#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("      Skipping PHI for variable '" + varName + "' (storage or type missing in preheader scope). Storage=" + llvmObjToString(storage) + ", Type=" + (varType ? varType->getName() : "null"));
#endif
            // This might be okay if the variable is defined *only* inside the loop body/else.
            // However, if it was used *before* the loop, this indicates an issue.
            // Consider adding a warning if symTable.lookup(varName) finds it in outer scopes but not current.
        }
    }

    // --- 6. 生成条件判断代码 (使用 PHI 值) ---
    std::map<std::string, llvm::Value*> originalValuesInSymTable;
    for (const auto& [name, phi] : phiNodes)
    {
        originalValuesInSymTable[name] = symTable.getVariable(name);  // Save original storage ptr
        symTable.setVariable(name, phi, typeBeforeLoop[name]);        // Temporarily use PHI as the value for codegen inside condition
#ifdef DEBUG_WhileSTmt
        DEBUG_LOG("      Temporarily updating symtable for '" + name + "' to PHI for condition generation.");
#endif
    }

#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [6] Generating condition code (using PHI values)...");
#endif
    llvm::Value* condValue = handleCondition(stmt->getCondition());

    // 恢复符号表，让其重新指向存储位置 (Alloca/GV)
    for (const auto& [name, originalStorage] : originalValuesInSymTable)
    {
        if (originalStorage)
        {  // Ensure we have something to restore
            symTable.setVariable(name, originalStorage, typeBeforeLoop[name]);
#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("      Restoring symtable for '" + name + "' back to storage ptr (" + llvmObjToString(originalStorage) + ") after condition generation.");
#endif
        }
        else
        {
#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("      Skipping symtable restore for '" + name + "' (original storage was null).");
#endif
        }
    }

#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("      Generated condition value: " + llvmObjToString(condValue));
#endif
    if (!condValue)
    {
#ifdef DEBUG_WhileSTmt
        DEBUG_LOG("      ERROR: Condition generation failed.");
#endif
        // Ensure the current block terminates if condition generation failed mid-block
        if (builder.GetInsertBlock() && !builder.GetInsertBlock()->getTerminator())
        {
            builder.CreateBr(endBB);  // Jump directly to end on error
        }
        builder.SetInsertPoint(endBB);  // Set insert point to end block for subsequent code
        codeGen.logError("Failed to generate condition for while loop.", stmt->line ? *stmt->line : 0, stmt->column ? *stmt->column : 0);
#ifdef DEBUG_WhileSTmt
        DEBUG_LOG("      Jumped to end block (" + llvmObjToString(endBB) + "). Aborting loop generation.");
#endif
        popBreakTarget();
        popContinueTarget();
#ifdef DEBUG_WhileSTmt
        DEBUG_LOG("  Popped loop targets from stacks (condition error).");
#endif
        return;
    }

    // --- 7. 创建条件分支 ---
    llvm::BasicBlock* currentCondExitBlock = builder.GetInsertBlock();  // Should be condBB
    llvm::BasicBlock* falseTargetBB = elseBB ? elseBB : endBB;          // <-- Target if condition is false
    builder.CreateCondBr(condValue, bodyBB, falseTargetBB);
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [7] Created conditional branch from " + llvmObjToString(currentCondExitBlock) + " based on " + llvmObjToString(condValue) + ":");
    DEBUG_LOG("      True -> Body (" + llvmObjToString(bodyBB) + ")");
    DEBUG_LOG("      False -> " + llvmObjToString(falseTargetBB) + (elseBB ? " (Else Block)" : " (End Block)"));  // <-- Updated log
#endif

    // --- 8. Loop Body Block ---
    builder.SetInsertPoint(bodyBB);
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [8] Set insert point to Body block (" + llvmObjToString(bodyBB) + ")");
    DEBUG_LOG("      Processing loop body statements using handleBlock(..., createNewScope=false)...");
#endif

    // Process body statements. IMPORTANT: Pass 'false' for createNewScope
    // because the loop itself doesn't introduce a new Python scope level.
    // Scopes are managed by function defs, class defs, etc.
    handleBlock(stmt->getBody(), false);

#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("      Finished processing loop body statements via handleBlock. Current block: " + llvmObjToString(builder.GetInsertBlock()));
#endif

    // --- 9. 从 Body 跳转到 Latch (如果 Body 没有终止) ---
    llvm::BasicBlock* bodyEndBB = builder.GetInsertBlock();
    bool bodyTerminated = !bodyEndBB || (bodyEndBB && bodyEndBB->getTerminator());
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [9] Checking if loop body terminated naturally.");
    DEBUG_LOG("      Body End Block: " + llvmObjToString(bodyEndBB));
    DEBUG_LOG(std::string("      Body Terminated: ") + (bodyTerminated ? "Yes (e.g., break, continue, return)" : "No"));
#endif
    if (!bodyTerminated)
    {
        builder.CreateBr(latchBB);  // Loop body finished normally, jump to Latch
#ifdef DEBUG_WhileSTmt
        DEBUG_LOG("      Created branch from Body End (" + llvmObjToString(bodyEndBB) + ") to Latch (" + llvmObjToString(latchBB) + ")");
#endif
    }

    // --- 10. Latch Block ---
    builder.SetInsertPoint(latchBB);
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [10] Set insert point to Latch block (" + llvmObjToString(latchBB) + ")");
#endif
    std::map<std::string, llvm::Value*> valueFromLatch;
    for (const auto& [name, phi] : phiNodes)
    {
        llvm::Value* storage = storageBeforeLoop[name];  // Get the original storage ptr
        llvm::Value* finalVal = nullptr;
        // Load the variable's final value *at the end of the iteration* (in the Latch block)
        if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(storage))
        {
            finalVal = builder.CreateLoad(pyObjectPtrType, allocaInst, name + "_latch_val");
        }
        else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(storage))
        {
            finalVal = builder.CreateLoad(pyObjectPtrType, gv, name + "_latch_val_global");
        }
        else
        {
            // This should ideally not happen if PHI was created correctly
            codeGen.logError("Internal error: Invalid storage type for '" + name + "' when loading in latch.", stmt->line.value_or(0), stmt->column.value_or(0));
#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("        ERROR: Invalid storage type for '" + name + "' in latch. Storage: " + llvmObjToString(storage));
#endif
            finalVal = llvm::UndefValue::get(pyObjectPtrType);  // Use Undef as fallback
        }
#ifdef DEBUG_WhileSTmt
        DEBUG_LOG("        Getting final value for PHI '" + name + "' by loading from storage (" + llvmObjToString(storage) + ") in latch block (" + llvmObjToString(latchBB) + "): " + llvmObjToString(finalVal));
#endif
        valueFromLatch[name] = finalVal;
    }

#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("      Cleaning up temporary objects for this iteration (in latch)...");
#endif
    runtime->cleanupTemporaryObjects();  // Clean up temps created *during this iteration* before looping back

    builder.CreateBr(condBB);  // Latch block always jumps back to the Condition block
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("      Created back edge branch from Latch (" + llvmObjToString(latchBB) + ") to Condition (" + llvmObjToString(condBB) + ")");
#endif

    // --- 11. 为 PHI 节点添加来自 Latch 的传入边 ---
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [11] Adding second incoming edges to PHI nodes (from latch " + llvmObjToString(latchBB) + "):");
#endif
    for (const auto& [name, phi] : phiNodes)
    {
        if (valueFromLatch.count(name))
        {
            llvm::Value* latchValue = valueFromLatch[name];
#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("        Adding incoming edge to PHI '" + name + "' (" + llvmObjToString(phi) + "): [" + llvmObjToString(latchValue) + ", from " + llvmObjToString(latchBB) + "]");
#endif
            phi->addIncoming(latchValue, latchBB);
        }
        else
        {
            // This indicates an internal inconsistency if a PHI was created but no latch value was loaded
#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("        ERROR: Latch value missing for PHI node '" + name + "'. Adding UndefValue.");
#endif
            codeGen.logError("Internal error: Latch value missing for PHI node '" + name + "'.", stmt->line ? *stmt->line : 0, stmt->column ? *stmt->column : 0);
            phi->addIncoming(llvm::UndefValue::get(phi->getType()), latchBB);  // Add Undef to keep IR valid
        }
    }

    // --- NEW 11a. Else Block (if it exists) ---
    if (elseBB)
    {
        builder.SetInsertPoint(elseBB);
#ifdef DEBUG_WhileSTmt
        DEBUG_LOG("  [11a] Set insert point to Else block (" + llvmObjToString(elseBB) + ")");
        DEBUG_LOG("        Processing else block statements...");
#endif
        // Get the else statement AST node
        StmtAST* elseStmtNode = stmt->getElseStmt();
        if (elseStmtNode)
        {
            // Check if it's a BlockStmt or a single statement
            if (auto* elseBlockNode = dynamic_cast<BlockStmtAST*>(elseStmtNode))
            {
                handleBlock(elseBlockNode->getStatements(), false);  // Process block, no new scope
            }
            else
            {
                handleStmt(elseStmtNode);  // Process single statement
            }
#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("        Finished processing else block statements. Current block: " + llvmObjToString(builder.GetInsertBlock()));
#endif
        }
        else
        {
            // Should not happen if elseBB was created, but handle defensively
#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("        WARNING: Else block (elseBB) exists, but getElseStmt() returned null.");
#endif
            codeGen.logError("Internal error: Else block generated but AST node is null.", stmt->line.value_or(0), stmt->column.value_or(0));
        }

        // --- NEW 11b. Branch from Else to End (if Else didn't terminate) ---
        llvm::BasicBlock* elseEndBB = builder.GetInsertBlock();
        bool elseTerminated = !elseEndBB || (elseEndBB && elseEndBB->getTerminator());
#ifdef DEBUG_WhileSTmt
        DEBUG_LOG("  [11b] Checking if else block terminated naturally.");
        DEBUG_LOG("        Else End Block: " + llvmObjToString(elseEndBB));
        DEBUG_LOG(std::string("        Else Terminated: ") + (elseTerminated ? "Yes" : "No"));
#endif
        if (!elseTerminated)
        {
            builder.CreateBr(endBB);  // Else block finished normally, jump to End
#ifdef DEBUG_WhileSTmt
            DEBUG_LOG("        Created branch from Else End (" + llvmObjToString(elseEndBB) + ") to End (" + llvmObjToString(endBB) + ")");
#endif
        }
    }

    // --- 12. Loop End Block ---
    // All paths leading out of the loop (condition false, break, potentially else block) converge here.
    builder.SetInsertPoint(endBB);
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  [12] Set insert point to End block (" + llvmObjToString(endBB) + ")");
#endif

    // --- Pop loop targets ---
    popBreakTarget();
    popContinueTarget();
#ifdef DEBUG_WhileSTmt
    DEBUG_LOG("  Popped loop targets from stacks (normal exit).");
    DEBUG_LOG("Exiting handleWhileStmt");
#endif
}

void CodeGenStmt::handlePrintStmt(PrintStmtAST* stmt)
{
    auto& builder = codeGen.getBuilder();
    auto* exprGen = codeGen.getExprGen();
    auto* runtime = codeGen.getRuntimeGen();
    auto& context = codeGen.getContext();

    // 1. 生成要打印的值的 LLVM Value (PyObject*)
    llvm::Value* value = exprGen->handleExpr(stmt->getValue());
    if (!value) return;

    // 2. 获取通用的对象打印运行时函数 py_print_object
    llvm::Function* printObjectFunc = codeGen.getOrCreateExternalFunction(
            "py_print_object",
            llvm::Type::getVoidTy(context),
            {llvm::PointerType::get(context, 0)}  // 参数是 PyObject*
    );

    // 3. 调用 py_print_object 函数
    //    确保 value 是 PyObject* 类型 (exprGen->handleExpr 应该返回 PyObject*)
    if (!value->getType()->isPointerTy())
    {
        // 如果 exprGen 可能返回非指针类型，这里需要包装
        // 但根据现有代码，它似乎总是返回 PyObject*
        codeGen.logError("Internal error: Value for print is not a PyObject*", stmt->line.value_or(0), stmt->column.value_or(0));
        runtime->cleanupTemporaryObjects();  // 清理可能已生成的 value
        return;
    }
    builder.CreateCall(printObjectFunc, {value});

    // 4. 清理生成 value 时可能产生的临时对象
    runtime->cleanupTemporaryObjects();
}

// 修改现有方法

void CodeGenStmt::handleAssignStmt(AssignStmtAST* stmt)
{
    auto& builder = codeGen.getBuilder();
    auto* exprGen = codeGen.getExprGen();
    auto* typeGen = codeGen.getTypeGen();
    auto* runtime = codeGen.getRuntimeGen();
    auto& symTable = codeGen.getSymbolTable();
    auto& context = codeGen.getContext();
    llvm::Type* pyObjectPtrType = llvm::PointerType::get(context, 0);  // PyObject*

    const std::string& varName = stmt->getName();
    const ExprAST* valueExpr = stmt->getValue();
    int line = stmt->line.value_or(0);
    int col = stmt->column.value_or(0);

    // 1. 生成 RHS 的 PyObject* 值
    llvm::Value* rhsRawValue = exprGen->handleExpr(valueExpr);
    if (!rhsRawValue) return;  // Error logged by handleExpr

    // 2. 获取 RHS 的类型信息
    std::shared_ptr<PyType> rhsPyType = valueExpr->getType();
    if (!rhsPyType)
    {
        codeGen.logWarning("Type information missing for RHS of assignment to '" + varName + "'. Assuming Any.", line, col);
        rhsPyType = PyType::getAny();
    }
    ObjectType* rhsObjType = rhsPyType->getObjectType();
    if (!rhsObjType)
    {
        codeGen.logError("Internal error: Could not get ObjectType for RHS of assignment to '" + varName + "'.", line, col);
        runtime->cleanupTemporaryObjects();
        return;
    }

    // 3. 确保 rhsValue 是实际的 PyObject* (处理 RHS 是变量的情况)
    //    注意：handleExpr 现在应该总是返回 PyObject* (通过 load 加载)
    llvm::Value* valueToStore = rhsRawValue;
    if (!valueToStore->getType()->isPointerTy())
    {
        // handleExpr 应该已经返回 PyObject* 了，这里加个保险
        codeGen.logError("Internal error: RHS expression did not evaluate to a PyObject* for assignment to '" + varName + "'.", line, col);
        runtime->cleanupTemporaryObjects();
        return;
    }

    // 4. 处理赋值目标 (LHS)
    size_t scopeDepth = symTable.getCurrentScopeDepth();
    bool isLocalVar = scopeDepth > 1;  // 假设 depth 1 是全局/模块

    llvm::Value* storage = nullptr;  // 指向 AllocaInst* 或 GlobalVariable*

    if (symTable.hasVariable(varName))
    {
        // --- 变量已存在 ---
        storage = symTable.getVariable(varName);

        if (isLocalVar)
        {
            // --- 更新现有局部变量 ---
            if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(storage))
            {
                if (allocaInst->getAllocatedType() != pyObjectPtrType)
                {
                    codeGen.logError("Internal error: Storage for local variable '" + varName + "' is not PyObject**.", line, col);
                    runtime->cleanupTemporaryObjects();
                    return;
                }
                // 1. Load old value
                llvm::Value* oldPyObject = builder.CreateLoad(pyObjectPtrType, allocaInst, varName + "_old");
                // 2. DecRef old value
                runtime->decRef(oldPyObject);
                // 3. Store new value
                builder.CreateStore(valueToStore, allocaInst);
                // 4. IncRef new value
                runtime->incRef(valueToStore);
                // 5. Update type in symbol table
                symTable.setVariable(varName, storage, rhsObjType);  // storage 不变, 更新类型
            }
            else
            {
                // 符号表中有，但不是 AllocaInst*，这在函数作用域内不应该发生
                codeGen.logError("Internal error: Expected AllocaInst* for existing local variable '" + varName + "' but found different value type.", line, col);
                runtime->cleanupTemporaryObjects();
                return;
            }
        }
        else
        {
            // --- 更新现有全局变量 ---
            if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(storage))
            {
                if (gv->getValueType() != pyObjectPtrType)
                {
                    codeGen.logError("Internal error: Storage for global variable '" + varName + "' is not PyObject**.", line, col);
                    runtime->cleanupTemporaryObjects();
                    return;
                }
                // 1. Load old value
                llvm::Value* oldPyObject = builder.CreateLoad(pyObjectPtrType, gv, varName + "_old_global");
                // 2. DecRef old value
                runtime->decRef(oldPyObject);
                // 3. Store new value
                builder.CreateStore(valueToStore, gv);
                // 4. IncRef new value
                runtime->incRef(valueToStore);
                // 5. Update type in symbol table
                symTable.setVariable(varName, storage, rhsObjType);  // storage 不变, 更新类型
            }
            else
            {
                // 符号表中有，但不是 GlobalVariable*，这在全局作用域内可能表示其他错误
                codeGen.logError("Internal error: Expected GlobalVariable* for existing global variable '" + varName + "' but found different value type.", line, col);
                runtime->cleanupTemporaryObjects();
                return;
            }
        }
    }
    else
    {
        // --- 新变量 ---
        if (isLocalVar)
        {
            // --- 定义新局部变量 ---
            // 1. Create storage (alloca) in entry block
            llvm::AllocaInst* allocaInst = codeGen.createEntryBlockAlloca(pyObjectPtrType, varName);
            if (!allocaInst)
            {  // createEntryBlockAlloca 内部会 logError
                runtime->cleanupTemporaryObjects();
                return;
            }
            storage = allocaInst;
            // 2. Store initial value
            builder.CreateStore(valueToStore, allocaInst);
            // 3. Add entry to symbol table
            symTable.setVariable(varName, storage, rhsObjType);  // 存储 AllocaInst*
            // 4. IncRef initial value
            runtime->incRef(valueToStore);
        }
        else
        {
            // --- 定义新全局变量 ---
            // 1. Create GlobalVariable (initialized to null initially, then stored)
            //    或者直接用 valueToStore 初始化？需要考虑初始化时机。
            //    更安全的方式是创建 GV，然后在 __llvmpy_entry 或模块初始化代码中 store 初始值。
            //    这里简化处理：假设可以在当前位置创建并初始化 GV。
            //    注意：这可能不完全符合 Python 全局变量在函数定义前不可用的规则，
            //    一个更完整的实现需要延迟初始化。
            llvm::Constant* initializer = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(pyObjectPtrType));  // Initialize with NULL
            llvm::GlobalVariable* gv = new llvm::GlobalVariable(
                    *codeGen.getModule(),
                    pyObjectPtrType,
                    false,                               // isConstant
                    llvm::GlobalValue::InternalLinkage,  // 或者 ExternalLinkage? 取决于是否跨模块
                    initializer,
                    varName + ".gv");
            storage = gv;

            // 2. Store initial value (覆盖 null)
            builder.CreateStore(valueToStore, gv);
            // 3. Add entry to symbol table
            symTable.setVariable(varName, storage, rhsObjType);  // 存储 GlobalVariable*
            // 4. IncRef initial value
            runtime->incRef(valueToStore);
        }
    }

    // 5. Mark last computed expression (optional)
    codeGen.setLastExprValue(valueToStore);  // 使用实际存储的 PyObject*
    codeGen.setLastExprType(rhsPyType);

    // 6. Cleanup temporary objects created during RHS evaluation
    runtime->cleanupTemporaryObjects();
}

/* 
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
    if (!targetType) {
        codeGen.logError("Type information missing for target of index assignment", stmt->line.value_or(0), stmt->column.value_or(0));
        runtime->cleanupTemporaryObjects(); // 清理已生成的值
        return;
   }

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
 */

void CodeGenStmt::handleIndexAssignStmt(IndexAssignStmtAST* stmt)
{
    auto* exprGen = codeGen.getExprGen();
    auto* runtime = codeGen.getRuntimeGen();
    auto& builder = codeGen.getBuilder();  // 获取 Builder
    auto& context = codeGen.getContext();  // 获取 LLVMContext

    // 1. 获取目标对象、索引和值的 LLVM Value (PyObject*)
    llvm::Value* target = exprGen->handleExpr(stmt->getTarget());
    if (!target) return;  // 错误已记录

    llvm::Value* index = exprGen->handleExpr(stmt->getIndex());
    if (!index)
    {
        runtime->cleanupTemporaryObjects();  // 清理 target
        return;                              // 错误已记录
    }

    llvm::Value* value = exprGen->handleExpr(stmt->getValue());
    if (!value)
    {
        runtime->cleanupTemporaryObjects();  // 清理 target 和 index
        return;                              // 错误已记录
    }

    // 2. 获取通用的索引设置运行时函数 py_object_set_index
    //    该函数负责在运行时检查 target 的类型并调用正确的具体函数
    llvm::Function* setIndexFunc = codeGen.getOrCreateExternalFunction(
            "py_object_set_index",           // 函数名
            llvm::Type::getVoidTy(context),  // 返回类型: void
            {
                    // 参数类型:
                    llvm::PointerType::get(context, 0),  // target (PyObject*)
                    llvm::PointerType::get(context, 0),  // index (PyObject*)
                    llvm::PointerType::get(context, 0)   // value (PyObject*)
            });

    // 3. 调用通用索引设置函数
    //    假设 target, index, value 已经是正确的 PyObject* 类型
    builder.CreateCall(setIndexFunc, {target, index, value});

    // 4. 清理生成 target, index, value 时可能产生的临时对象
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