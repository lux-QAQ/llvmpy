
#include "CodeGen/CodeGenModule.h"
#include "CodeGen/CodeGenType.h"
#include "CodeGen/CodeGenRuntime.h"
#include "CodeGen/CodeGenStmt.h"  // 添加这一行，确保CodeGenStmt完整定义
#include "CodeGen/CodeGenExpr.h"
#include "CodeGen/CodeGenUtil.h"  // Include the helper header

#include "TypeOperations.h"
#include "ObjectLifecycle.h"
#include "TypeIDs.h"

#include "Debugdefine.h"

#include <iostream>
#include <sstream>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>

namespace llvmpy
{

// 生成完整模块
// 生成完整模块
bool CodeGenModule::generateModule(ModuleAST* module, bool isEntryPoint)
{
#ifdef DEBUG_CODEGEN_generateModule
    DEBUG_LOG_DETAIL("GenMod", "Entering generateModule. isEntryPoint=" + std::string(isEntryPoint ? "true" : "false"));
#endif
    if (!module)
    {
        codeGen.logError("Cannot generate code for null module AST.");
        return false;
    }

    // 设置当前正在处理的模块 AST
    setCurrentModule(module);

    // 初始化模块级资源 (仅一次)
    if (!moduleInitialized)
    {
        addRuntimeFunctions();
        createAndRegisterRuntimeInitializer();
        moduleInitialized = true;
#ifdef DEBUG_CODEGEN_generateModule
        DEBUG_LOG_DETAIL("GenMod", "Module resources initialized (runtime functions, global ctor).");
#endif
    }

    // 获取语句生成器
    auto* stmtGen = codeGen.getStmtGen();
    if (!stmtGen)
    {
        codeGen.logError("Statement generator is null in CodeGenModule::generateModule.");
        return false;
    }

    // --- 根据是否是入口点模块，决定如何处理顶层语句 ---
    if (isEntryPoint)
    {
#ifdef DEBUG_CODEGEN_generateModule
        DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Processing as entry point module.");
#endif
        // --- 1. 生成 __llvmpy_entry 函数 (替代 main) ---
        llvm::FunctionType* entryFnType = llvm::FunctionType::get(
                llvm::Type::getInt32Ty(codeGen.getContext()),  // 返回 i32 退出码
                false);                                        // 无参数

        llvm::Function* entryFn = codeGen.getOrCreateFunction(
                "__llvmpy_entry",  // <--- 重命名入口函数
                entryFnType,
                llvm::GlobalValue::ExternalLinkage);  // 保持 ExternalLinkage
        if (!entryFn)
        {
            codeGen.logError("Failed to create or get entry function '__llvmpy_entry'.");
            return false;
        }
#ifdef DEBUG_CODEGEN_generateModule
        DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Got entry function: " + llvmObjToString(entryFn));
#endif

        // --- 2. 设置 __llvmpy_entry 函数的入口块和插入点 ---
        llvm::BasicBlock* entryBB = nullptr;
        if (entryFn->empty())
        {
            entryBB = createFunctionEntry(entryFn);  // 创建入口块
            if (!entryBB)
            {  // Check if block creation failed
                codeGen.logError("Failed to create entry block for '__llvmpy_entry'.");
                return false;
            }
#ifdef DEBUG_CODEGEN_generateModule
            DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Created new entry block for __llvmpy_entry: " + llvmObjToString(entryBB));
#endif
            codeGen.getBuilder().SetInsertPoint(entryBB);  // 设置插入点到新块
        }
        else
        {
            entryBB = &entryFn->getEntryBlock();
#ifdef DEBUG_CODEGEN_generateModule
            DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Using existing entry block for __llvmpy_entry: " + llvmObjToString(entryBB));
#endif
            if (entryBB->getTerminator())
            {
                // If the entry block already has a terminator, it might indicate a redefinition or an issue.
                // Depending on the desired behavior, you might clear it or log an error.
                // For now, logging an error as it was.
                codeGen.logError("Entry function '__llvmpy_entry' block already has a terminator.", 0, 0);
                return false;
                // Alternative: entryBB->getTerminator()->eraseFromParent(); // Clear terminator if re-entry is allowed
                // codeGen.getBuilder().SetInsertPoint(entryBB);
            }
            else
            {
                // Set insert point to the end of the existing block if no terminator
                codeGen.getBuilder().SetInsertPoint(entryBB);
            }
        }
#ifdef DEBUG_CODEGEN_generateModule
        DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Set initial insert point in __llvmpy_entry.");
#endif

        // --- 3. 保存并设置当前代码生成上下文为 __llvmpy_entry 函数 ---
        llvm::IRBuilderBase::InsertPoint savedIP = codeGen.getBuilder().saveIP();
        llvm::Function* savedFunction = codeGen.getCurrentFunction();
        ObjectType* savedReturnType = codeGen.getCurrentReturnType();

        codeGen.setCurrentFunction(entryFn);
        codeGen.setCurrentReturnType(nullptr);  // __llvmpy_entry returns i32, context is for Python objects
#ifdef DEBUG_CODEGEN_generateModule
        DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Set current function to __llvmpy_entry. Saved context: Func=" + llvmObjToString(savedFunction) + ", IP=" + ipToString(savedIP));
#endif

        // --- 4. 处理模块顶层语句 (包括 def main():) ---
        stmtGen->beginScope();  // Enter top-level scope for __llvmpy_entry
#ifdef DEBUG_CODEGEN_generateModule
        DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Starting statement loop in __llvmpy_entry. Initial block: " + llvmObjToString(codeGen.getBuilder().GetInsertBlock()));
#endif
        for (const auto& stmt : module->getStatements())
        {
            llvm::BasicBlock* currentBlock = codeGen.getBuilder().GetInsertBlock();
            // Ensure we are still inside the entry function
            if (!currentBlock || currentBlock->getParent() != entryFn)
            {
                codeGen.logError("Builder left entry function unexpectedly during statement processing.", stmt->line.value_or(0), stmt->column.value_or(0));
                stmtGen->endScope();
                // Restore context before returning
                codeGen.setCurrentFunction(savedFunction);
                codeGen.setCurrentReturnType(savedReturnType);
                if (savedIP.getBlock()) codeGen.getBuilder().restoreIP(savedIP);
                return false;
            }
            // Check if the current block is already terminated
            if (currentBlock->getTerminator())
            {
                codeGen.logWarning("Statement unreachable after block termination in entry function.",
                                   stmt->line.value_or(0), stmt->column.value_or(0));
                // Skip remaining statements in this block/function? Or just this one?
                // Breaking the loop seems reasonable if the block is terminated.
                break;
            }
#ifdef DEBUG_CODEGEN_generateModule
            DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Loop Iter: Processing Stmt Kind " + std::to_string(static_cast<int>(stmt->kind())) + " at line " + std::to_string(stmt->line.value_or(0)) + " in block " + llvmObjToString(currentBlock));
#endif
            // Process all top-level statements normally
            stmtGen->handleStmt(stmt.get());
#ifdef DEBUG_CODEGEN_generateModule
            // Check the block again after handling the statement
            llvm::BasicBlock* blockAfterStmt = codeGen.getBuilder().GetInsertBlock();
            DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Loop Iter: After handleStmt. Current block: " + llvmObjToString(blockAfterStmt));
            if (blockAfterStmt && blockAfterStmt->getTerminator())
            {
                DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Loop Iter: Block terminated after handling statement.");
            }
#endif
        }

        // --- 5. 调用 Python main 函数 ---
        llvm::BasicBlock* lastBlock = codeGen.getBuilder().GetInsertBlock();
#ifdef DEBUG_CODEGEN_generateModule
        DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Preparing to call Python main. Current block: " + llvmObjToString(lastBlock));
#endif
        // Ensure we are still in the entry function and the block is not terminated
        if (lastBlock && lastBlock->getParent() == entryFn && !lastBlock->getTerminator())
        {
            codeGen.getBuilder().SetInsertPoint(lastBlock);  // Ensure we are at the end

            // 5.1 Look up the Python 'main' function object from the symbol table
            llvm::Value* pyMainFuncObj = codeGen.getSymbolTable().getVariable("main");
            ObjectType* pyMainFuncType = codeGen.getSymbolTable().getVariableType("main");

            // Check if 'main' exists and is a function
            if (!pyMainFuncObj || !pyMainFuncType || pyMainFuncType->getCategory() != ObjectType::Function)
            {
                // If 'main' is not found or not a function, log error and return exit code 1
                codeGen.logError("Python function 'main' not found or is not a function.", 0, 0);
                codeGen.getBuilder().CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(codeGen.getContext()), 1));
            }
            else
            {
// If 'main' is found
#ifdef DEBUG_CODEGEN_generateModule
                DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Found Python main function object: " + llvmObjToString(pyMainFuncObj));
                // Check if pyMainFuncObj is a GlobalVariable, if so, it needs loading
                llvm::Value* funcObjToCall = pyMainFuncObj;  // Assume it's already loaded unless it's a GV
                if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(pyMainFuncObj))
                {
                    DEBUG_LOG_DETAIL("GenMod", "[EntryPt] 'main' object is a GlobalVariable, needs loading before call.");
                    // Load the actual PyObject* from the GlobalVariable
                    funcObjToCall = codeGen.getBuilder().CreateLoad(gv->getValueType(), gv, "main_func_loaded");  // <--- Load happens here
                    DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Loaded 'main' object: " + llvmObjToString(funcObjToCall));
                }
                else
                {
                    DEBUG_LOG_DETAIL("GenMod", "[EntryPt] 'main' object is not a GlobalVariable, using directly.");
                }
#else
                // Non-debug: Load if it's a GlobalVariable
                llvm::Value* funcObjToCall = pyMainFuncObj;
                if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(pyMainFuncObj))
                {
                    funcObjToCall = codeGen.getBuilder().CreateLoad(gv->getValueType(), gv, "main_func_loaded");  // <--- Load happens here
                }
#endif
                // 5.2 Call the runtime function 'py_call_function_noargs'
                // Pass the potentially loaded function object
                llvm::Value* pyResultObj = codeGen.getRuntimeGen()->createCallFunctionNoArgs(funcObjToCall);  // <--- CORRECTED: Use funcObjToCall here
                if (!pyResultObj)
                {
                    // If the call generation fails, log error and return exit code 1
                    codeGen.logError("Failed to generate call to Python main function.", 0, 0);
                    codeGen.getBuilder().CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(codeGen.getContext()), 1));
                }
                else
                {
#ifdef DEBUG_CODEGEN_generateModule
                    DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Generated call to Python main. Result object: " + llvmObjToString(pyResultObj));
#endif
                    // 5.3 Call the runtime function 'py_object_to_exit_code' to convert the result
                    llvm::Value* exitCode = codeGen.getRuntimeGen()->createObjectToExitCode(pyResultObj);
                    if (!exitCode)
                    {
                        // If conversion fails, log error and return exit code 1
                        codeGen.logError("Failed to generate conversion to exit code.", 0, 0);
                        codeGen.getBuilder().CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(codeGen.getContext()), 1));
                    }
                    else
                    {
#ifdef DEBUG_CODEGEN_generateModule
                        DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Generated conversion to exit code: " + llvmObjToString(exitCode));
#endif
                        // 5.4 Return the resulting i32 exit code
                        codeGen.getBuilder().CreateRet(exitCode);
#ifdef DEBUG_CODEGEN_generateModule
                        DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Added return instruction for exit code.");
#endif
                    }
                    // Decrement ref count of pyResultObj if necessary (depends on runtime conventions)
                    // codeGen.getRuntimeGen()->decRef(pyResultObj);
                }
            }
        }
#ifdef DEBUG_CODEGEN_generateModule
        // Log why the call to main might not happen
        else if (!lastBlock || lastBlock->getParent() != entryFn)
        {
            DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Cannot call Python main (builder not in entry function or block is null).");
            // If the block is somehow invalid, we might need a default return here too.
            if (lastBlock && !lastBlock->getTerminator())
            {                                                                                                             // Check if we can add a return
                codeGen.getBuilder().CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(codeGen.getContext()), 1));  // Error exit
            }
        }
        else  // lastBlock->getTerminator() is true
        {
            DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Cannot call Python main (block already terminated before call).");
        }
#endif

        stmtGen->endScope();  // Exit top-level scope
#ifdef DEBUG_CODEGEN_generateModule
        DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Finished statement loop and main call sequence.");
#endif

        // --- 6. 恢复之前的代码生成上下文 ---
        codeGen.setCurrentFunction(savedFunction);
        codeGen.setCurrentReturnType(savedReturnType);
        if (savedIP.getBlock())
        {
            codeGen.getBuilder().restoreIP(savedIP);
#ifdef DEBUG_CODEGEN_generateModule
            DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Restored context. Current function: " + llvmObjToString(codeGen.getCurrentFunction()) + ", IP=" + ipToString(codeGen.getBuilder().saveIP()));
#endif
        }
        else
        {
#ifdef DEBUG_CODEGEN_generateModule
            DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Restored context (Func/RetType only). Current function: " + llvmObjToString(codeGen.getCurrentFunction()) + ". Saved IP was invalid.");
#endif
            // If we restored to a state with no function but the builder still has an insert block, it's weird.
            if (!savedFunction && codeGen.getBuilder().GetInsertBlock())
            {
                DEBUG_LOG_DETAIL("GenMod", "[EntryPt] WARNING: Builder has insert block but no current function after restore!");
                // Consider clearing the builder's insert point if this state is invalid
                // codeGen.getBuilder().ClearInsertionPoint();
            }
        }

        // --- 7. 验证生成的 __llvmpy_entry 函数 ---
#ifdef DEBUG_CODEGEN_generateModule
        DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Verifying entry function '__llvmpy_entry'...");
#endif
        // Use the utility function for verification
        if (verifyFunction(*entryFn))  // Assuming verifyFunction logs details on failure
        {
            codeGen.logError("LLVM Entry function '__llvmpy_entry' verification failed.");
#ifdef DEBUG_CODEGEN_generateModule
            DEBUG_LOG_DETAIL("GenMod", "[EntryPt] !!! Entry function verification FAILED !!!");
            // Optionally print the function to stderr again if verifyFunction doesn't
            // entryFn->print(llvm::errs());
#endif
            return false;  // Verification failed
        }
#ifdef DEBUG_CODEGEN_generateModule
        DEBUG_LOG_DETAIL("GenMod", "[EntryPt] Entry function verification PASSED.");
#endif
    }
    else  // Non-entry point module
    {
#ifdef DEBUG_CODEGEN_generateModule
        DEBUG_LOG_DETAIL("GenMod", "[NonEntryPt] Processing as non-entry point module.");
#endif
        // --- 1. 保存并设置全局上下文 ---
        // Non-entry point modules generate code at the global scope (e.g., function definitions)
        llvm::IRBuilderBase::InsertPoint savedIP = codeGen.getBuilder().saveIP();
        llvm::Function* savedFunction = codeGen.getCurrentFunction();
        ObjectType* savedReturnType = codeGen.getCurrentReturnType();

        // Set context to global (no current function)
        codeGen.setCurrentFunction(nullptr);
        codeGen.setCurrentReturnType(nullptr);
        // Clear builder insertion point? Or leave it? Leaving it might be fine.
        // codeGen.getBuilder().ClearInsertionPoint();
#ifdef DEBUG_CODEGEN_generateModule
        DEBUG_LOG_DETAIL("GenMod", "[NonEntryPt] Set global context. Saved context: Func=" + llvmObjToString(savedFunction) + ", IP=" + ipToString(savedIP));
#endif

        // --- 2. 处理模块顶层语句 ---
        // Only definition statements (functions, classes, imports) should ideally be present.
        // Executable code at the top level might run at load time via global constructors,
        // or might not run as expected depending on the final linking/execution model.
        stmtGen->beginScope();  // Enter global scope
#ifdef DEBUG_CODEGEN_generateModule
        DEBUG_LOG_DETAIL("GenMod", "[NonEntryPt] Starting statement loop in global context.");
#endif
        for (const auto& stmt : module->getStatements())
        {
#ifdef DEBUG_CODEGEN_generateModule
            DEBUG_LOG_DETAIL("GenMod", "[NonEntryPt] Processing Stmt Kind " + std::to_string(static_cast<int>(stmt->kind())));
#endif
            // Check for potentially problematic top-level executable statements
            if (stmt->kind() != ASTKind::FunctionDefStmt && stmt->kind() != ASTKind::ClassStmt &&  // Assuming ClassStmt is definition-like
                stmt->kind() != ASTKind::ImportStmt /* && other definition kinds */)
            {
                // Log a warning if executable code is found at the top level
                codeGen.logWarning("Top-level executable statement found in non-entry point module. Execution might not occur as expected or might run at load time.",
                                   stmt->line.value_or(0), stmt->column.value_or(0));
            }
            // Handle the statement (e.g., generate function definition)
            stmtGen->handleStmt(stmt.get());
        }
        stmtGen->endScope();  // Exit global scope
#ifdef DEBUG_CODEGEN_generateModule
        DEBUG_LOG_DETAIL("GenMod", "[NonEntryPt] Finished statement loop.");
#endif

        // --- 3. 恢复之前的代码生成上下文 ---
        codeGen.setCurrentFunction(savedFunction);
        codeGen.setCurrentReturnType(savedReturnType);
        // Restore insertion point only if it was valid
        if (savedIP.getBlock())
        {
            codeGen.getBuilder().restoreIP(savedIP);
#ifdef DEBUG_CODEGEN_generateModule
            DEBUG_LOG_DETAIL("GenMod", "[NonEntryPt] Restored context. Current function: " + llvmObjToString(codeGen.getCurrentFunction()) + ", IP=" + ipToString(codeGen.getBuilder().saveIP()));
#endif
        }
        else
        {
#ifdef DEBUG_CODEGEN_generateModule
            DEBUG_LOG_DETAIL("GenMod", "[NonEntryPt] Restored context (Func/RetType only). Current function: " + llvmObjToString(codeGen.getCurrentFunction()) + ". Saved IP was invalid.");
#endif
            if (!savedFunction && codeGen.getBuilder().GetInsertBlock())
            {
                DEBUG_LOG_DETAIL("GenMod", "[NonEntryPt] WARNING: Builder has insert block but no current function after restore!");
            }
        }
    }

    // --- 最后：验证整个 LLVM 模块的正确性 ---
#ifdef DEBUG_CODEGEN_generateModule
    DEBUG_LOG_DETAIL("GenMod", "Verifying entire module...");
#endif
    // Use llvm::verifyModule to check the whole module
    if (llvm::verifyModule(*codeGen.getModule(), &llvm::errs()))
    {
        // If verification fails, log an error. Details are printed to llvm::errs().
        codeGen.logError("LLVM Module verification failed. See stderr output for details.");
#ifdef DEBUG_CODEGEN_generateModule
        DEBUG_LOG_DETAIL("GenMod", "!!! Module verification FAILED !!!");
        // Optionally print the whole module again if needed for debugging
        // codeGen.getModule()->print(llvm::errs(), nullptr);
#endif
        return false;  // Module verification failed
    }
#ifdef DEBUG_CODEGEN_generateModule
    DEBUG_LOG_DETAIL("GenMod", "Module verification PASSED.");
    DEBUG_LOG_DETAIL("GenMod", "Leaving generateModule.");
#endif

    return true;  // Module generated and verified successfully
}

// --- 新增：创建并注册运行时初始化函数 ---
/* void CodeGenModule::createAndRegisterRuntimeInitializer()
{
    llvm::LLVMContext& context = codeGen.getContext();
    llvm::Module* module = codeGen.getModule();

    // 1. 创建初始化函数 __llvmpy_runtime_init
    llvm::FunctionType* initFuncType = llvm::FunctionType::get(llvm::Type::getVoidTy(context), false);
    llvm::Function* initFunc = llvm::Function::Create(
        initFuncType,
        llvm::Function::InternalLinkage, // 或者 PrivateLinkage
        "__llvmpy_runtime_init",
        module
    );

    // 创建入口块
    llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(context, "entry", initFunc);
    llvm::IRBuilder<> builder(entryBB); // 临时 builder

    // 获取 py_initialize_builtin_type_methods
    llvm::Function* runtimeInitCore = codeGen.getOrCreateExternalFunction(
        "py_initialize_builtin_type_methods",
        llvm::Type::getVoidTy(context),
        {}
    );

    // 在 __llvmpy_runtime_init 中调用核心初始化函数
    builder.CreateCall(runtimeInitCore);
    builder.CreateRetVoid(); // 添加返回

    // 2. 注册到 llvm.global_ctors
    // 定义构造函数记录的类型: { i32, void ()*, i8* }
    llvm::StructType* ctorEntryType = llvm::StructType::get(
        context,
        { llvm::Type::getInt32Ty(context),   // 优先级
          llvm::PointerType::get(initFuncType, 0), // 函数指针
          llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0) // 数据指针 (通常为 null)
        }
    );

    // 创建构造函数记录实例
    llvm::Constant* ctorEntry = llvm::ConstantStruct::get(
        ctorEntryType,
        { llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 65535), // 优先级 (65535 是默认)
          initFunc, // 初始化函数
          llvm::ConstantPointerNull::get(llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)) // 数据指针为 null
        }
    );

    // 创建包含此记录的数组
    llvm::ArrayType* arrayType = llvm::ArrayType::get(ctorEntryType, 1);
    llvm::Constant* ctorsArray = llvm::ConstantArray::get(arrayType, {ctorEntry});

    // 创建全局变量 @llvm.global_ctors
    new llvm::GlobalVariable(
        *module,
        arrayType,
        false, // isConstant = false
        llvm::GlobalValue::AppendingLinkage, // 必须是 AppendingLinkage
        ctorsArray, // 初始化值
        "llvm.global_ctors" // 名称
    );
} */

void CodeGenModule::createAndRegisterRuntimeInitializer()
{
    llvm::LLVMContext& context = codeGen.getContext();
    llvm::Module* module = codeGen.getModule();

    // 1. 检查是否已存在，避免重复创建
    if (module->getFunction("__llvmpy_runtime_init"))
    {
        return;
    }

    // 2. 创建初始化函数 __llvmpy_runtime_init
    llvm::FunctionType* initFuncType = llvm::FunctionType::get(llvm::Type::getVoidTy(context), false);
    llvm::Function* initFunc = llvm::Function::Create(
            initFuncType,
            llvm::Function::InternalLinkage,  // 内部链接，不需要导出
            "__llvmpy_runtime_init",
            module);
    initFunc->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);  // 优化：允许合并相同函数

    // 创建入口块
    llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(context, "entry", initFunc);
    llvm::IRBuilder<> builder(entryBB);  // 使用临时的 IRBuilder

    // 获取核心运行时初始化函数 (假设存在)
    llvm::Function* runtimeInitCore = codeGen.getOrCreateExternalFunction(
            "py_initialize_builtin_type_methods",  // 示例名称
            llvm::Type::getVoidTy(context),
            {});

    // 在 __llvmpy_runtime_init 中调用核心初始化函数
    builder.CreateCall(runtimeInitCore);
    builder.CreateRetVoid();  // 添加 return void

    // 3. 注册到 llvm.global_ctors
    // 定义构造函数记录的类型: { i32 priority, void ()* function_ptr, i8* data_ptr }
    llvm::StructType* ctorEntryType = llvm::StructType::get(
            context,
            {
                    llvm::Type::getInt32Ty(context),                              // 优先级
                    llvm::PointerType::getUnqual(initFuncType),                   // 函数指针 (使用 getUnqual)
                    llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context))  // 数据指针 (通常为 null)
            });

    // 创建构造函数记录实例
    llvm::Constant* ctorEntry = llvm::ConstantStruct::get(
            ctorEntryType,
            {
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 65535),                               // 优先级 (65535 是默认/较低优先级)
                    initFunc,                                                                                     // 初始化函数指针
                    llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context)))  // 数据指针为 null
            });

    // 创建包含此记录的数组类型
    llvm::ArrayType* arrayType = llvm::ArrayType::get(ctorEntryType, 1);

    // 获取或创建 @llvm.global_ctors 全局变量
    llvm::GlobalVariable* globalCtors = module->getGlobalVariable("llvm.global_ctors");
    if (!globalCtors)
    {
        // 如果不存在，创建新的
        globalCtors = new llvm::GlobalVariable(
                *module,
                arrayType,                                         // 数组类型
                false,                                             // isConstant = false
                llvm::GlobalValue::AppendingLinkage,               // 必须是 AppendingLinkage
                llvm::ConstantArray::get(arrayType, {ctorEntry}),  // 初始化值
                "llvm.global_ctors"                                // 名称
        );
    }
    else
    {
        // 如果已存在，追加新的构造函数记录
        llvm::ConstantArray* oldInitializer = llvm::dyn_cast<llvm::ConstantArray>(globalCtors->getInitializer());
        std::vector<llvm::Constant*> newCtors;
        if (oldInitializer)
        {
            for (auto& op : oldInitializer->operands())
            {
                newCtors.push_back(llvm::cast<llvm::Constant>(op));
            }
        }
        newCtors.push_back(ctorEntry);  // 添加新的记录

        // 创建新的数组类型和初始化器
        llvm::ArrayType* newArrayType = llvm::ArrayType::get(ctorEntryType, newCtors.size());
        globalCtors->mutateType(llvm::PointerType::getUnqual(newArrayType));            // 更新全局变量类型
        globalCtors->setInitializer(llvm::ConstantArray::get(newArrayType, newCtors));  // 设置新的初始化器
    }
}
// 创建模块初始化函数
llvm::Function* CodeGenModule::createModuleInitFunction()
{
    // 创建模块初始化函数类型
    llvm::FunctionType* moduleInitType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(codeGen.getContext()), false);

    // 创建模块初始化函数
    llvm::Function* moduleInitFn = llvm::Function::Create(
            moduleInitType,
            llvm::Function::InternalLinkage,
            "__module_init",
            codeGen.getModule());

    // 创建函数入口块
    llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(
            codeGen.getContext(), "entry", moduleInitFn);

    // 保存当前插入点
    llvm::BasicBlock* savedBlock = codeGen.getBuilder().GetInsertBlock();
    llvm::Function* savedFunction = codeGen.getCurrentFunction();
    ObjectType* savedReturnType = codeGen.getCurrentReturnType();

    // 设置插入点到初始化函数
    codeGen.getBuilder().SetInsertPoint(entryBB);
    codeGen.setCurrentFunction(moduleInitFn);
    codeGen.setCurrentReturnType(nullptr);

    // 这里可以添加模块级初始化代码，比如设置全局变量等

    // 创建返回指令
    codeGen.getBuilder().CreateRetVoid();

    // 恢复之前的插入点
    if (savedBlock)
    {
        codeGen.getBuilder().SetInsertPoint(savedBlock);
    }
    codeGen.setCurrentFunction(savedFunction);
    codeGen.setCurrentReturnType(savedReturnType);

    return moduleInitFn;
}

// 添加运行时库函数声明
void CodeGenModule::addRuntimeFunctions()
{
    // 处理打印函数
    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_print_int",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::Type::getInt32Ty(codeGen.getContext())},
                false);
    }

    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_print_double",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::Type::getDoubleTy(codeGen.getContext())},
                false);
    }

    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_print_string",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0)},
                false);
    }

    // 处理对象创建函数
    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_create_int",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::Type::getInt32Ty(codeGen.getContext())},
                false);
    }

    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_create_double",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::Type::getDoubleTy(codeGen.getContext())},
                false);
    }

    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_create_bool",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::Type::getInt1Ty(codeGen.getContext())},
                false);
    }

    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_create_string",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0)},
                false);
    }

    // 处理对象操作函数
    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_object_add",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::PointerType::get(codeGen.getContext(), 0)},
                false);
    }

    // 索引操作函数
    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_object_index",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::PointerType::get(codeGen.getContext(), 0)},
                false);
    }

    // 引用计数管理函数
    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_incref",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0)},
                false);

        codeGen.getOrCreateExternalFunction(
                "py_decref",
                llvm::Type::getVoidTy(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0)},
                false);
    }

    // 类型检查函数
    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_check_type",
                llvm::Type::getInt1Ty(codeGen.getContext()),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::Type::getInt32Ty(codeGen.getContext())},
                false);
    }

    // 获取 None 对象函数
    {
        // 修改为使用 getOrCreateExternalFunction
        codeGen.getOrCreateExternalFunction(
                "py_get_none",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {},
                false);
    }
    {
        codeGen.getOrCreateExternalFunction(  // 添加 py_create_function 声明
                "py_create_function",
                llvm::PointerType::get(codeGen.getContext(), 0),   // 返回 PyObject*
                {llvm::PointerType::get(codeGen.getContext(), 0),  // func_ptr (void*)
                 llvm::Type::getInt32Ty(codeGen.getContext())},    // signature_type_id
                false);
    }
    {
        codeGen.getOrCreateExternalFunction(
                "py_call_function_noargs",
                llvm::PointerType::get(codeGen.getContext(), 0),    // 返回 PyObject*
                {llvm::PointerType::get(codeGen.getContext(), 0)},  // func_obj
                false);
    }
    {
        codeGen.getOrCreateExternalFunction(
                "py_object_to_exit_code",
                llvm::Type::getInt32Ty(codeGen.getContext()),       // 返回 i32
                {llvm::PointerType::get(codeGen.getContext(), 0)},  // obj
                false);
    }
}

// 处理函数定义
/* llvm::Function* CodeGenModule::handleFunctionDef(FunctionAST* funcAST)
{
    if (!funcAST)
    {
        return nullptr;
    }

    // 解析函数返回类型
    std::shared_ptr<PyType> returnType = resolveReturnType(funcAST);

    // 解析函数参数类型
    std::vector<std::shared_ptr<PyType>> paramTypes;
    funcAST->resolveParamTypes();
    for (auto& param : funcAST->params)
    {
        if (param.resolvedType)
        {
            paramTypes.push_back(param.resolvedType);
        }
        else
        {
            // 如果参数类型未解析，使用 Any 类型
            paramTypes.push_back(PyType::getAny());
        }
    }

    // 创建函数类型
    llvm::FunctionType* fnType = createFunctionType(returnType, paramTypes);

    // 创建函数
    std::string funcName = funcAST->name;
    std::vector<llvm::Type*> llvmParamTypes;
    for (auto& paramType : paramTypes)
    {
        llvmParamTypes.push_back(llvm::PointerType::get(codeGen.getContext(), 0));  // Python 对象指针
    }

    // 获取返回类型
    llvm::Type* llvmReturnType = llvm::PointerType::get(codeGen.getContext(), 0);
    if (returnType->isVoid())
    {
        llvmReturnType = llvm::Type::getVoidTy(codeGen.getContext());
    }

    llvm::Function* function = codeGen.getOrCreateExternalFunction(
            funcName,
            llvmReturnType,
            llvmParamTypes,
            false);

    // 处理函数参数
    handleFunctionParams(function, funcAST->params, paramTypes);

    // 创建函数入口基本块
    llvm::BasicBlock* entryBB = createFunctionEntry(function);
    codeGen.getBuilder().SetInsertPoint(entryBB);

    // 保存当前函数上下文
    llvm::Function* savedFunction = codeGen.getCurrentFunction();
    ObjectType* savedReturnType = codeGen.getCurrentReturnType();

    // 设置当前函数上下文
    codeGen.setCurrentFunction(function);
    codeGen.setCurrentReturnType(returnType->getObjectType());

    // 添加函数引用
    std::vector<ObjectType*> paramObjectTypes;
    for (auto& type : paramTypes)
    {
        paramObjectTypes.push_back(type->getObjectType());
    }
    addFunctionReference(funcName, function, returnType->getObjectType(), paramObjectTypes);

    // 处理函数体
    auto* stmtGen = codeGen.getStmtGen();
    stmtGen->beginScope();

    for (auto& stmt : funcAST->body)
    {
        stmtGen->handleStmt(stmt.get());
    }

    // 如果函数没有显式的返回语句，添加默认返回值 None
    if (!codeGen.getBuilder().GetInsertBlock()->getTerminator())
    {
        // 获取None对象
        llvm::Function* getNoneFn = codeGen.getOrCreateExternalFunction(
                "py_get_none",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {});

        llvm::Value* noneValue = codeGen.getBuilder().CreateCall(getNoneFn);

        // 返回None
        codeGen.getBuilder().CreateRet(noneValue);
    }

    // 结束函数作用域
    stmtGen->endScope();

    // 清理函数资源
    cleanupFunction();

    // 恢复函数上下文
    codeGen.setCurrentFunction(savedFunction);
    codeGen.setCurrentReturnType(savedReturnType);

    // 验证函数
    std::string errorInfo;
    llvm::raw_string_ostream errorStream(errorInfo);
    if (llvm::verifyFunction(*function, &errorStream))
    {
        std::cerr << "Function " << funcName << " verification failed: "
                  << errorStream.str() << std::endl;
        function->eraseFromParent();
        return nullptr;
    }

    return function;
} */

llvm::Function* CodeGenModule::handleFunctionDef(const FunctionAST* funcAST)  // 接受 const*
{
#ifdef DEBUG_CODEGEN_handleFunctionDef
    std::string dbgFuncName = funcAST ? funcAST->getName() : "<null AST>";
    DEBUG_LOG_DETAIL("HdlFuncDef", "Entering handleFunctionDef for '" + dbgFuncName + "'");
#endif
    if (!funcAST)
    {
        codeGen.logError("Null FunctionAST passed to handleFunctionDef.");
        return nullptr;
    }

    std::string funcName = funcAST->getName();  // Get name early for logging

    // --- 保存当前上下文 ---
#ifdef DEBUG_CODEGEN_handleFunctionDef
    DEBUG_LOG_DETAIL("HdlFuncDef", "Saving context...");
#endif
    llvm::IRBuilderBase::InsertPoint savedIP = codeGen.getBuilder().saveIP();
    llvm::Function* savedFunction = codeGen.getCurrentFunction();
    ObjectType* savedReturnTypeCtx = codeGen.getCurrentReturnType();
#ifdef DEBUG_CODEGEN_handleFunctionDef
    DEBUG_LOG_DETAIL("HdlFuncDef", "Saved IP: {" + ipToString(savedIP) + "}, Saved Func: " + llvmObjToString(savedFunction));
#endif

    // --- 解析类型 ---
#ifdef DEBUG_CODEGEN_handleFunctionDef
    DEBUG_LOG_DETAIL("HdlFuncDef", "Resolving return and param types for '" + funcName + "'...");
#endif
    std::shared_ptr<PyType> returnType = resolveReturnType(funcAST);
    std::vector<std::shared_ptr<PyType>> paramTypes;
    // Ensure getParams() is a const method in FunctionAST
    for (const auto& param : funcAST->getParams())
    {
        // Assuming param.resolvedType is accessible or has a const getter
        paramTypes.push_back(param.resolvedType ? param.resolvedType : PyType::getAny());
    }
#ifdef DEBUG_CODEGEN_handleFunctionDef
    DEBUG_LOG_DETAIL("HdlFuncDef", "Resolved return type: " + returnType->toString());
    // Log param types if needed
#endif

    // --- 创建 LLVM 函数类型和函数 ---
#ifdef DEBUG_CODEGEN_handleFunctionDef
    DEBUG_LOG_DETAIL("HdlFuncDef", "Creating LLVM function type and function for '" + funcName + "'...");
#endif
    llvm::FunctionType* fnType = createFunctionType(returnType, paramTypes);
    if (!fnType)
    {
        codeGen.logError("Failed to create LLVM function type for: " + funcName);
        // Restore context before returning
        codeGen.setCurrentFunction(savedFunction);
        codeGen.setCurrentReturnType(savedReturnTypeCtx);
        if (savedIP.getBlock()) codeGen.getBuilder().restoreIP(savedIP);
        return nullptr;
    }

    // Use getOrCreateFunction with InternalLinkage for user-defined functions
    llvm::Function* function = codeGen.getOrCreateFunction(
            funcName,
            fnType,
            llvm::GlobalValue::InternalLinkage);  // Internal linkage for user functions

    if (!function)
    {
        // getOrCreateFunction logs errors internally if creation/retrieval fails
        // Restore context before returning
        codeGen.setCurrentFunction(savedFunction);
        codeGen.setCurrentReturnType(savedReturnTypeCtx);
        if (savedIP.getBlock()) codeGen.getBuilder().restoreIP(savedIP);
        return nullptr;
    }
#ifdef DEBUG_CODEGEN_handleFunctionDef
    DEBUG_LOG_DETAIL("HdlFuncDef", "Got LLVM function: " + llvmObjToString(function));
#endif

    // --- 检查函数是否已存在且非空 (健壮性检查) ---
    if (!function->empty())
    {
        // Function already exists and has content. Avoid redefining.
        codeGen.logWarning("Function '" + funcName + "' already has a body. Skipping redefinition.",
                           funcAST->line.value_or(0), funcAST->column.value_or(0));
        // **重要：恢复上下文，因为我们没有改变它**
        codeGen.setCurrentFunction(savedFunction);
        codeGen.setCurrentReturnType(savedReturnTypeCtx);
        if (savedIP.getBlock()) codeGen.getBuilder().restoreIP(savedIP);
#ifdef DEBUG_CODEGEN_handleFunctionDef
        DEBUG_LOG_DETAIL("HdlFuncDef", "Restored context without changes as function body exists.");
#endif
        return function;  // Return the existing function
    }

    // --- 函数是新创建的或为空，继续生成 ---
#ifdef DEBUG_CODEGEN_handleFunctionDef
    DEBUG_LOG_DETAIL("HdlFuncDef", "Setting up entry block and params for '" + funcName + "'...");
#endif
    llvm::BasicBlock* entryBB = createFunctionEntry(function);
    if (!entryBB)
    {  // createFunctionEntry might fail if function is invalid
        codeGen.logError("Failed to create entry block for function: " + funcName);
        // Restore context
        codeGen.setCurrentFunction(savedFunction);
        codeGen.setCurrentReturnType(savedReturnTypeCtx);
        if (savedIP.getBlock()) codeGen.getBuilder().restoreIP(savedIP);
        // Consider erasing the potentially problematic function?
        // function->eraseFromParent();
        return nullptr;
    }

    codeGen.getBuilder().SetInsertPoint(entryBB);               // **改变插入点**
    codeGen.setCurrentFunction(function);                       // **改变当前函数**
    codeGen.setCurrentReturnType(returnType->getObjectType());  // **改变返回类型上下文**
#ifdef DEBUG_CODEGEN_handleFunctionDef
    DEBUG_LOG_DETAIL("HdlFuncDef", "Set insert point to new entry block: " + llvmObjToString(entryBB));
    DEBUG_LOG_DETAIL("HdlFuncDef", "Set current function to: " + llvmObjToString(function));
#endif
    // Ensure getParams() is const
    handleFunctionParams(function, funcAST->getParams(), paramTypes);

    // --- 生成函数体 ---
    auto* stmtGen = codeGen.getStmtGen();
    stmtGen->beginScope();  // New scope for the function body
#ifdef DEBUG_CODEGEN_handleFunctionDef
    DEBUG_LOG_DETAIL("HdlFuncDef", "Generating function body for '" + funcName + "'...");
#endif
    // Ensure getBody() is const
    for (const auto& stmt : funcAST->getBody())
    {
        // Check if the current block is still valid and reachable
        llvm::BasicBlock* currentBlock = codeGen.getBuilder().GetInsertBlock();
        if (!currentBlock || currentBlock->getParent() != function)
        {
            codeGen.logError("Builder left the current function '" + funcName + "' unexpectedly during body generation.", stmt->line.value_or(0), stmt->column.value_or(0));
            // Attempt to restore context partially before failing
            stmtGen->endScope();
            codeGen.setCurrentFunction(savedFunction);
            codeGen.setCurrentReturnType(savedReturnTypeCtx);
            if (savedIP.getBlock()) codeGen.getBuilder().restoreIP(savedIP);
            return nullptr;  // Abort generation for this function
        }
        if (currentBlock->getTerminator())
        {
#ifdef DEBUG_CODEGEN_handleFunctionDef
            DEBUG_LOG_DETAIL("HdlFuncDef", "Body generation stopped early (block terminated before statement at line " + std::to_string(stmt->line.value_or(0)) + ").");
#endif
            codeGen.logWarning("Statement unreachable after block termination in function '" + funcName + "'.", stmt->line.value_or(0), stmt->column.value_or(0));
            break;  // Stop processing statements in this block
        }
#ifdef DEBUG_CODEGEN_handleFunctionDef
        DEBUG_LOG_DETAIL("HdlFuncDef", "Body: Handling Stmt Kind " + std::to_string(static_cast<int>(stmt->kind())) + " at line " + std::to_string(stmt->line.value_or(0)));
#endif
        stmtGen->handleStmt(stmt.get());  // Generate code for the statement
    }
#ifdef DEBUG_CODEGEN_handleFunctionDef
    DEBUG_LOG_DETAIL("HdlFuncDef", "Finished generating function body for '" + funcName + "'.");
#endif
    stmtGen->endScope();  // End the function body scope

    // --- 添加默认返回 (如果需要) ---
    llvm::BasicBlock* lastBlock = codeGen.getBuilder().GetInsertBlock();
#ifdef DEBUG_CODEGEN_handleFunctionDef
    DEBUG_LOG_DETAIL("HdlFuncDef", "Checking for default return for '" + funcName + "'. Last block: " + llvmObjToString(lastBlock));
#endif
    // Ensure the builder is still inside the function we are generating
    if (lastBlock && lastBlock->getParent() == function && !lastBlock->getTerminator())
    {
        codeGen.getBuilder().SetInsertPoint(lastBlock);  // Ensure we are at the end
#ifdef DEBUG_CODEGEN_handleFunctionDef
        DEBUG_LOG_DETAIL("HdlFuncDef", "Adding default return to block " + llvmObjToString(lastBlock));
#endif
        if (returnType->isVoid())
        {
            codeGen.getBuilder().CreateRetVoid();
        }
        else
        {
            // Default return for non-void functions is None
            llvm::Value* noneValue = codeGen.getExprGen()->createNoneLiteral();
            if (noneValue)
            {
                codeGen.getBuilder().CreateRet(noneValue);
            }
            else
            {
                codeGen.logError("Failed to create default 'None' return value for function: " + funcName);
                // No return added, verification will likely fail.
            }
        }
#ifdef DEBUG_CODEGEN_handleFunctionDef
        if (lastBlock->getTerminator()) DEBUG_LOG_DETAIL("HdlFuncDef", "Added default return instruction.");
#endif
    }
#ifdef DEBUG_CODEGEN_handleFunctionDef
    else if (!lastBlock || lastBlock->getParent() != function)
    {
        DEBUG_LOG_DETAIL("HdlFuncDef", "No default return needed (builder not in current function '" + funcName + "').");
    }
    else
    {
        DEBUG_LOG_DETAIL("HdlFuncDef", "No default return needed (block already terminated).");
    }
#endif

    // --- 清理函数资源 ---
#ifdef DEBUG_CODEGEN_handleFunctionDef
    DEBUG_LOG_DETAIL("HdlFuncDef", "Cleaning up function resources for '" + funcName + "'...");
#endif
    cleanupFunction();  // Release temps, etc.

    // --- 恢复之前的上下文 ---
#ifdef DEBUG_CODEGEN_handleFunctionDef
    DEBUG_LOG_DETAIL("HdlFuncDef", "Restoring context. Target IP: {" + ipToString(savedIP) + "}, Target Func: " + llvmObjToString(savedFunction));
#endif
    codeGen.setCurrentFunction(savedFunction);
    codeGen.setCurrentReturnType(savedReturnTypeCtx);
    // 只有在 savedIP 有效时才恢复
    if (savedIP.getBlock())
    {
        codeGen.getBuilder().restoreIP(savedIP);  // **恢复插入点**
#ifdef DEBUG_CODEGEN_handleFunctionDef
        DEBUG_LOG_DETAIL("HdlFuncDef", "Restored IP.");
#endif
    }
    else
    {
#ifdef DEBUG_CODEGEN_handleFunctionDef
        DEBUG_LOG_DETAIL("HdlFuncDef", "Skipped restoring IP (savedIP was invalid). Builder might be in unexpected state if savedFunction is null.");
        if (!savedFunction && codeGen.getBuilder().GetInsertBlock())
        {
            DEBUG_LOG_DETAIL("HdlFuncDef", "WARNING: Builder has insert block but no current function after restore!");
        }
        // It's often normal for the builder to have no insert block if savedFunction was null (global context)
#endif
    }
#ifdef DEBUG_CODEGEN_handleFunctionDef
    DEBUG_LOG_DETAIL("HdlFuncDef", "Context restored. Current function: " + llvmObjToString(codeGen.getCurrentFunction()));
#endif

    // --- 验证生成的函数 ---
#ifdef DEBUG_CODEGEN_handleFunctionDef
    DEBUG_LOG_DETAIL("HdlFuncDef", "Verifying function '" + funcName + "'...");
#endif
    if (verifyFunction(*function))  // Using the helper function
    {
        codeGen.logError("LLVM Function '" + funcName + "' verification failed. See stderr for details.");
#ifdef DEBUG_CODEGEN_handleFunctionDef
        DEBUG_LOG_DETAIL("HdlFuncDef", "!!! Function verification FAILED for '" + funcName + "' !!!");
        function->print(llvm::errs());  // 打印 IR 到 stderr
#endif
        // Consider erasing the invalid function to prevent further issues
        // function->eraseFromParent();
        return nullptr;  // Indicate failure
    }
#ifdef DEBUG_CODEGEN_handleFunctionDef
    DEBUG_LOG_DETAIL("HdlFuncDef", "Function verification PASSED for '" + funcName + "'.");
    DEBUG_LOG_DETAIL("HdlFuncDef", "Leaving handleFunctionDef for '" + funcName + "'");
#endif

    return function;  // Return the successfully generated and verified function
}

// 创建函数类型
llvm::FunctionType* CodeGenModule::createFunctionType(
        std::shared_ptr<PyType> returnType,
        const std::vector<std::shared_ptr<PyType>>& paramTypes)
{
    // 获取返回类型的LLVM表示
    llvm::Type* llvmReturnType;
    if (returnType->isVoid())
    {
        llvmReturnType = llvm::Type::getVoidTy(codeGen.getContext());
    }
    else
    {
        // 对于Python类型，统一使用PyObject指针
        llvmReturnType = llvm::PointerType::get(codeGen.getContext(), 0);
    }

    // 获取参数类型的LLVM表示
    std::vector<llvm::Type*> llvmParamTypes;
    for (auto& paramType : paramTypes)
    {
        // 对于Python类型，统一使用PyObject指针
        llvmParamTypes.push_back(llvm::PointerType::get(codeGen.getContext(), 0));
    }

    // 创建函数类型
    return llvm::FunctionType::get(llvmReturnType, llvmParamTypes, false);
}

// 处理函数参数
void CodeGenModule::handleFunctionParams(
        llvm::Function* function,
        const std::vector<ParamAST>& params,
        std::vector<std::shared_ptr<PyType>>& paramTypes)
{
    // 命名函数参数
    size_t idx = 0;
    for (auto& arg : function->args())
    {
        if (idx < params.size())
        {
            arg.setName(params[idx].name);

            // 在当前作用域中添加参数变量
            llvm::Value* paramValue = &arg;
            codeGen.getSymbolTable().setVariable(
                    params[idx].name,
                    paramValue,
                    paramTypes[idx]->getObjectType());

            idx++;
        }
    }
}

// 解析函数返回类型
/* std::shared_ptr<PyType> CodeGenModule::resolveReturnType(const FunctionAST* funcAST)
{
    // 如果已经解析，直接返回
    if (funcAST->returnTypeResolved)
    {
        return funcAST->getReturnType();
    }

    // 如果有明确的返回类型注解，使用注解类型
    if (!funcAST->returnTypeName.empty())
    {
        // 解析类型名称
        std::shared_ptr<PyType> returnType = PyType::fromString(funcAST->returnTypeName);
        funcAST->cachedReturnType = returnType;
        funcAST->returnTypeResolved = true;
        return returnType;
    }

    // 否则尝试推断返回类型
    std::shared_ptr<PyType> inferredType = funcAST->inferReturnType();
    if (inferredType)
    {
        funcAST->cachedReturnType = inferredType;
        funcAST->returnTypeResolved = true;
        return inferredType;
    }

    // 如果无法推断，默认为Any类型
    funcAST->cachedReturnType = PyType::getAny();
    funcAST->returnTypeResolved = true;
    return funcAST->cachedReturnType;
} */

std::shared_ptr<PyType> CodeGenModule::resolveReturnType(const FunctionAST* funcAST)
{
    // 如果 AST 节点本身已经缓存了类型 (可能在之前的分析阶段完成)
    // 注意：这里假设 getReturnType() 是 const 方法
    if (funcAST->returnTypeResolved)  // <--- 修改这里：直接访问成员变量
    {
        // 注意：getReturnType() 也需要是 const 方法
        return funcAST->getReturnType();
    }

    // 如果有明确的返回类型注解，使用注解类型
    // 注意：这里假设 getReturnTypeName() 是 const 方法
    if (!funcAST->getReturnTypeName().empty())
    {
        // 解析类型名称
        std::shared_ptr<PyType> returnType = PyType::fromString(funcAST->getReturnTypeName());
        // 不再在此处修改 funcAST
        return returnType;
    }

    // 否则尝试推断返回类型
    // 注意：这里假设 inferReturnType() 是 const 方法
    std::shared_ptr<PyType> inferredType = funcAST->inferReturnType();
    if (inferredType)
    {
        // 不再在此处修改 funcAST
        return inferredType;
    }

    // 如果无法推断，默认为Any类型
    // 不再在此处修改 funcAST
    return PyType::getAny();  // 直接返回 Any 类型
}

// 创建函数入口基本块
llvm::BasicBlock* CodeGenModule::createFunctionEntry(llvm::Function* function)
{
    return llvm::BasicBlock::Create(codeGen.getContext(), "entry", function);
}

// 处理函数返回 ？这里实际上需要使用类型推导？
void CodeGenModule::handleFunctionReturn(
        llvm::Value* returnValue,
        std::shared_ptr<PyType> returnType)
{
    auto* runtime = codeGen.getRuntimeGen();

    // 准备返回值 - 确保类型匹配（添加第三个参数，值类型与返回类型相同）
    llvm::Value* preparedValue = runtime->prepareReturnValue(
            returnValue,
            returnType,   // 值类型
            returnType);  // 函数返回类型

    // 创建返回指令
    codeGen.getBuilder().CreateRet(preparedValue);
}

/* // 另一个可能的修复方案
void CodeGenModule::handleFunctionReturn(
    llvm::Value* returnValue, 
    std::shared_ptr<PyType> returnType) {
    
    auto* runtime = codeGen.getRuntimeGen();
    auto* typeGen = codeGen.getTypeGen();
    
    // 获取实际返回值的类型（可能需要从返回值中推导）
    std::shared_ptr<PyType> valueType = returnType; // 这里假设两者相同，实际可能需要推导
    
    // 准备返回值 - 确保类型匹配
    llvm::Value* preparedValue = runtime->prepareReturnValue(
        returnValue, 
        valueType,
        returnType);
    
    // 创建返回指令
    codeGen.getBuilder().CreateRet(preparedValue);
} */
// 清理函数资源
void CodeGenModule::cleanupFunction()
{
    // 释放函数中的临时对象
    codeGen.releaseTempObjects();
    codeGen.getVariableUpdateContext().clearLoopVariables();
}

// 添加函数引用
void CodeGenModule::addFunctionReference(
        const std::string& name,
        llvm::Function* function,
        ObjectType* returnType,
        const std::vector<ObjectType*>& paramTypes,
        bool isExternal)
{
    // 创建函数定义信息
    FunctionDefInfo info;
    info.name = name;
    info.function = function;
    info.returnType = returnType;
    info.paramTypes = paramTypes;
    info.isExternal = isExternal;

    // 添加到函数定义映射
    functionDefs[name] = info;
}

// 查找函数引用
FunctionDefInfo* CodeGenModule::getFunctionInfo(const std::string& name)
{
    auto it = functionDefs.find(name);
    if (it != functionDefs.end())
    {
        return &it->second;
    }
    return nullptr;
}

}  // namespace llvmpy