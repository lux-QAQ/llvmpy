#include "CodeGen/CodeGenExpr.h"
#include "CodeGen/CodeGenBase.h"
#include "CodeGen/CodeGenType.h"
#include "CodeGen/CodeGenRuntime.h"
#include "CodeGen/CodeGenModule.h"
#include "CodeGen/CodeGenUtil.h"

//#include "ObjectRuntime.h"
#include "TypeOperations.h"
#include "ObjectLifecycle.h"
//#include "ast.h"
#include "parser.h"  // For PyTypeParser

#include <llvm/IR/Constants.h>

#include <cmath>
#include <iostream>  // For errors
namespace llvmpy
{

// 静态成员初始化
std::unordered_map<ASTKind, ExprHandlerFunc> CodeGenExpr::exprHandlers;
bool CodeGenExpr::handlersInitialized = false;

void CodeGenExpr::initializeHandlers()
{
    if (handlersInitialized)
    {
        return;
    }

    // 注册表达式处理器
    exprHandlers[ASTKind::NumberExpr] = [](CodeGenBase& cg, ExprAST* expr)
    {
        return static_cast<CodeGenExpr*>(cg.getExprGen())->handleNumberExpr(static_cast<NumberExprAST*>(expr));
    };

    exprHandlers[ASTKind::StringExpr] = [](CodeGenBase& cg, ExprAST* expr)
    {
        return static_cast<CodeGenExpr*>(cg.getExprGen())->handleStringExpr(static_cast<StringExprAST*>(expr));
    };

    exprHandlers[ASTKind::BoolExpr] = [](CodeGenBase& cg, ExprAST* expr)
    {
        return static_cast<CodeGenExpr*>(cg.getExprGen())->handleBoolExpr(static_cast<BoolExprAST*>(expr));
    };

    exprHandlers[ASTKind::NoneExpr] = [](CodeGenBase& cg, ExprAST* expr)
    {
        return static_cast<CodeGenExpr*>(cg.getExprGen())->handleNoneExpr(static_cast<NoneExprAST*>(expr));
    };

    exprHandlers[ASTKind::VariableExpr] = [](CodeGenBase& cg, ExprAST* expr)
    {
        return static_cast<CodeGenExpr*>(cg.getExprGen())->handleVariableExpr(static_cast<VariableExprAST*>(expr));
    };

    exprHandlers[ASTKind::BinaryExpr] = [](CodeGenBase& cg, ExprAST* expr)
    {
        return static_cast<CodeGenExpr*>(cg.getExprGen())->handleBinaryExpr(static_cast<BinaryExprAST*>(expr));
    };

    exprHandlers[ASTKind::UnaryExpr] = [](CodeGenBase& cg, ExprAST* expr)
    {
        return static_cast<CodeGenExpr*>(cg.getExprGen())->handleUnaryExpr(static_cast<UnaryExprAST*>(expr));
    };

    exprHandlers[ASTKind::CallExpr] = [](CodeGenBase& cg, ExprAST* expr)
    {
        return static_cast<CodeGenExpr*>(cg.getExprGen())->handleCallExpr(static_cast<CallExprAST*>(expr));
    };

    exprHandlers[ASTKind::ListExpr] = [](CodeGenBase& cg, ExprAST* expr)
    {
        return static_cast<CodeGenExpr*>(cg.getExprGen())->handleListExpr(static_cast<ListExprAST*>(expr));
    };

    exprHandlers[ASTKind::DictExpr] = [](CodeGenBase& cg, ExprAST* expr)
    {
        // 注意这里的转型和调用
        return static_cast<CodeGenExpr*>(cg.getExprGen())->handleDictExpr(static_cast<DictExprAST*>(expr));
    };

    exprHandlers[ASTKind::IndexExpr] = [](CodeGenBase& cg, ExprAST* expr)
    {
        return static_cast<CodeGenExpr*>(cg.getExprGen())->handleIndexExpr(static_cast<IndexExprAST*>(expr));
    };

    handlersInitialized = true;
}

// 通用表达式处理入口
llvm::Value* CodeGenExpr::handleExpr(const ExprAST* expr)
{
    if (!expr)
    {
        return nullptr;
    }

    // 使用适当的处理器处理表达式
    auto it = exprHandlers.find(expr->kind());
    if (it != exprHandlers.end())
    {
        return it->second(codeGen, const_cast<ExprAST*>(expr));
    }

    return codeGen.logError("Unknown expression type",
                            expr->line ? *expr->line : 0,
                            expr->column ? *expr->column : 0);
}

// 处理数字表达式
llvm::Value* CodeGenExpr::handleNumberExpr(NumberExprAST* expr)
{
    // Get the original string representation from the AST node
    const std::string& valueStr = expr->getValueString();
    // Get the type determined during parsing (int or double)
    std::shared_ptr<PyType> numType = expr->getType();

    if (!numType)
    {
        // This should ideally not happen if parsing and type checking are correct
        return codeGen.logError("Internal error: NumberExprAST has no determined type.",
                                expr->line.value_or(0), expr->column.value_or(0));
    }

    // Check the determined type and call the appropriate string-based creator
    if (numType->isInt())
    {
        // Create integer literal object from string
        return createIntLiteralFromString(valueStr);
    }
    else if (numType->isDouble())
    {
        // Create double literal object from string
        return createDoubleLiteralFromString(valueStr);
    }
    else
    {
        // Fallback or error if the type is somehow not int or double
        return codeGen.logError("Internal error: NumberExprAST has unexpected type: " + numType->getObjectType()->getName(),
                                expr->line.value_or(0), expr->column.value_or(0));
    }
}

// 处理字符串表达式
llvm::Value* CodeGenExpr::handleStringExpr(StringExprAST* expr)
{
    return createStringLiteral(expr->getValue());
}

// 处理布尔表达式
llvm::Value* CodeGenExpr::handleBoolExpr(BoolExprAST* expr)
{
    return createBoolLiteral(expr->getValue());
}

// 处理None表达式
llvm::Value* CodeGenExpr::handleNoneExpr(NoneExprAST* expr)
{
    return createNoneLiteral();
}

// 处理变量引用表达式
llvm::Value* CodeGenExpr::handleVariableExpr(VariableExprAST* expr)
{
    const std::string& name = expr->getName();
    auto& symTable = codeGen.getSymbolTable();
    auto& builder = codeGen.getBuilder();
    auto& context = codeGen.getContext();
    llvm::Type* pyObjectPtrType = llvm::PointerType::get(context, 0);  // PyObject*

    llvm::Value* valueOrStorage = symTable.getVariable(name);
    ObjectType* type = symTable.getVariableType(name);  // 获取类型信息

    if (!valueOrStorage)
    {
        // --- 尝试查找函数 AST (作为函数对象) ---
        const FunctionAST* funcAST = symTable.findFunctionAST(name);
        if (funcAST)
        {
#ifdef DEBUG_CODEGEN_handleVariableExpr
            DEBUG_LOG_DETAIL("HdlVarExpr", "Found FunctionAST for '" + name + "'. Treating as function object.");
#endif
            // 获取代表函数对象的全局变量
            std::string gvName = name + "_obj_gv";  // 与 CodeGenModule 中创建 GV 的名称一致
            llvm::GlobalVariable* funcObjGV = codeGen.getModule()->getGlobalVariable(gvName);

            if (!funcObjGV)
            {
                return codeGen.logError("Cannot find global variable '" + gvName + "' for function object '" + name + "'.",
                                        expr->line.value_or(0), expr->column.value_or(0));
            }
            if (funcObjGV->getValueType() != pyObjectPtrType)
            {
                return codeGen.logError("Global variable '" + gvName + "' for function object '" + name + "' does not hold PyObject*.",
                                        expr->line.value_or(0), expr->column.value_or(0));
            }

            // 加载函数对象
            llvm::Value* loadedFuncObj = builder.CreateLoad(pyObjectPtrType, funcObjGV, name + "_func_obj_loaded");

            // 设置类型
            ObjectType* funcObjType = codeGen.getTypeGen()->getFunctionObjectType(funcAST);
            if (funcObjType)
            {
                expr->setType(PyType::fromObjectType(funcObjType));
            }
            else
            {
                expr->setType(PyType::getAny());
                codeGen.logWarning("Could not determine ObjectType for function '" + name + "'. Assuming Any.", expr->line.value_or(0), expr->column.value_or(0));
            }
            return loadedFuncObj;
        }
        else
        {
            // 既不是变量也不是函数 AST
            return codeGen.logError("Unknown variable or function '" + name + "'",
                                    expr->line.value_or(0), expr->column.value_or(0));
        }
    }

    // --- 变量存在于符号表中 ---
    llvm::Value* loadedValue = nullptr;

    if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(valueOrStorage))
    {
        // --- 局部变量 (AllocaInst*) ---
        if (allocaInst->getAllocatedType() != pyObjectPtrType)
        {
            return codeGen.logError("Internal error: Storage for local variable '" + name + "' is not PyObject**.",
                                    expr->line.value_or(0), expr->column.value_or(0));
        }
        // 创建 load 指令
        loadedValue = builder.CreateLoad(pyObjectPtrType, allocaInst, name + "_val");
#ifdef DEBUG_CODEGEN_handleVariableExpr
        DEBUG_LOG_DETAIL("HdlVarExpr", "Loaded value from AllocaInst '" + name + "': " + llvmObjToString(loadedValue));
#endif
    }
    else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(valueOrStorage))
    {
        // --- 全局变量 (GlobalVariable*) ---
        if (gv->getValueType() != pyObjectPtrType)
        {
            return codeGen.logError("Internal error: Storage for global variable '" + name + "' is not PyObject**.",
                                    expr->line.value_or(0), expr->column.value_or(0));
        }
        // 创建 load 指令
        loadedValue = builder.CreateLoad(pyObjectPtrType, gv, name + "_global_val");
#ifdef DEBUG_CODEGEN_handleVariableExpr
        DEBUG_LOG_DETAIL("HdlVarExpr", "Loaded value from GlobalVariable '" + name + "': " + llvmObjToString(loadedValue));
#endif
    }
    else if (valueOrStorage->getType() == pyObjectPtrType)
    {
        // --- 直接是 PyObject* (例如函数参数) ---
        // 不需要 load
        loadedValue = valueOrStorage;
#ifdef DEBUG_CODEGEN_handleVariableExpr
        DEBUG_LOG_DETAIL("HdlVarExpr", "Using direct PyObject* for variable '" + name + "': " + llvmObjToString(loadedValue));
#endif
    }
    else
    {
        // 符号表中的值类型未知或不正确
        return codeGen.logError("Internal error: Unexpected value type in symbol table for variable '" + name + "'.",
                                expr->line.value_or(0), expr->column.value_or(0));
    }

    // 设置表达式的类型
    if (type)
    {
        expr->setType(PyType::fromObjectType(type));
    }
    else
    {
        expr->setType(PyType::getAny());  // 回退
        codeGen.logWarning("Type information missing for variable '" + name + "'. Assuming Any.", expr->line.value_or(0), expr->column.value_or(0));
    }

    return loadedValue;  // 返回加载后的 PyObject*
}

// 处理二元操作表达式
llvm::Value* CodeGenExpr::handleBinaryExpr(BinaryExprAST* expr)
{
    // 确保 BinaryExprAST 的 getLHS() 和 getRHS() 返回 ExprAST*，而不是 std::unique_ptr<ExprAST>
    // 生成左右操作数的代码
    llvm::Value* L = handleExpr(expr->getLHS());
    if (!L) return nullptr;

    llvm::Value* R = handleExpr(expr->getRHS());
    if (!R) return nullptr;

    // --- 修改：获取 PyTokenType 并传递 ---
    PyTokenType op = expr->getOpType();  // Changed: getOp() -> getOpType()
    return handleBinOp(op, L, R,         // Pass PyTokenType
                       expr->getLHS()->getType(),
                       expr->getRHS()->getType());
}

// 处理一元操作表达式
llvm::Value* CodeGenExpr::handleUnaryExpr(UnaryExprAST* expr)
{
    // 生成操作数的代码
    llvm::Value* operand = handleExpr(expr->getOperand());
    if (!operand) return nullptr;

    // 使用类型推导的一元操作
    PyTokenType op = expr->getOpType();  // Changed: getOpCode() -> getOpType()
    return handleUnaryOp(op, operand,    // Pass PyTokenType
                         expr->getOperand()->getType());
}

// 处理函数调用表达式
llvm::Value* CodeGenExpr::handleCallExpr(CallExprAST* expr)
{
    auto& builder = codeGen.getBuilder();
    auto* runtime = codeGen.getRuntimeGen();
    auto* typeGen = codeGen.getTypeGen();
    auto& symTable = codeGen.getSymbolTable();

    const std::string& calleeName = expr->getCallee();

    llvm::Value* callableValue = nullptr;  // Can be llvm::Function* or PyObject* GV
    std::shared_ptr<PyType> callableType = nullptr;
    bool isDirectCall = false;  // Flag to indicate direct LLVM call

    // 1. Check if it's a variable in the symbol table
    if (symTable.hasVariable(calleeName))
    {
        llvm::Value* varValue = symTable.getVariable(calleeName);
        ObjectType* objType = symTable.getVariableType(calleeName);
        callableType = objType ? PyType::fromObjectType(objType) : PyType::getAny();

        // Load the PyObject* from the variable (GV or Alloca)
        if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(varValue))
        {
            callableValue = builder.CreateLoad(gv->getValueType(), gv, calleeName + "_callable_loaded");
        }
        else if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(varValue))
        {
            callableValue = builder.CreateLoad(allocaInst->getAllocatedType(), allocaInst, calleeName + "_callable_loaded");
        }
        else
        {
            callableValue = varValue;  // Should already be PyObject* (e.g., nested func)
        }

        if (!callableType->isFunction() && !callableType->isAny())
        {
            return codeGen.logTypeError("Variable '" + calleeName + "' is not callable.",
                                        expr->line.value_or(0), expr->column.value_or(0));
        }
        // It's a variable, so it's NOT a direct call to the llvm::Function
        isDirectCall = false;
    }
    // 2. If not a variable, check if it's a known FunctionAST
    else
    {
        const FunctionAST* funcAST = symTable.findFunctionAST(calleeName);
        if (funcAST)
        {
#ifdef DEBUG_CODEGEN_handleCallExpr
            DEBUG_LOG_DETAIL("HdlCallExpr", "Found FunctionAST for '" + calleeName + "' in symbol table.");
#endif
            // Get LLVM function info from the module cache
            llvm::Function* llvmFunc = codeGen.getModuleGen()->getCachedFunction(funcAST);

            if (!llvmFunc)
            {
                // This error should not happen now with the pre-caching
                return codeGen.logError("Internal Error: Found FunctionAST for '" + calleeName + "' but failed to get LLVM function from cache (should be pre-cached).",
                                        expr->line.value_or(0), expr->column.value_or(0));
            }

            // Get the function's type
            ObjectType* funcObjType = codeGen.getTypeGen()->getFunctionObjectType(funcAST);
            if (!funcObjType || funcObjType->getCategory() != ObjectType::Function)
            {
                return codeGen.logError("Found FunctionAST for '" + calleeName + "' but failed to get valid ObjectType.",
                                        expr->line.value_or(0), expr->column.value_or(0));
            }
            callableType = PyType::fromObjectType(funcObjType);

            // --- REVISED LOGIC ---
            // If we found the FunctionAST and its corresponding LLVM Function,
            // we can perform a direct call. No need to load from _obj_gv here.
            isDirectCall = true;
            callableValue = llvmFunc;  // Store the llvm::Function* itself
#ifdef DEBUG_CODEGEN_handleCallExpr
            if (llvmFunc == codeGen.getCurrentFunction())
            {
                DEBUG_LOG_DETAIL("HdlCallExpr", "Detected RECURSIVE call to '" + calleeName + "'. Will generate direct call.");
            }
            else
            {
                DEBUG_LOG_DETAIL("HdlCallExpr", "Detected call to known function '" + calleeName + "' (possibly outer scope). Will generate direct call.");
            }
#endif
            // --- END REVISED LOGIC ---
        }
        else
        {
            // --- MODIFICATION: Print symbol table on lookup failure ---
            std::cerr << "--- Symbol Table Dump (Variable and AST Lookup Failed for '" << calleeName << "') ---" << std::endl;
            symTable.dump(std::cerr);
            std::cerr << "---------------------------------------------------------" << std::endl;
            // --- End Modification ---
            return codeGen.logError("Unknown function or variable: " + calleeName,
                                    expr->line.value_or(0), expr->column.value_or(0));
        }
    }

    if (!callableValue)
    {
        return codeGen.logError("Could not resolve callable expression: " + calleeName,
                                expr->line.value_or(0), expr->column.value_or(0));
    }

    // 4. Generate arguments (same as before)
    std::vector<llvm::Value*> args;
    std::vector<std::shared_ptr<PyType>> argTypes;
    for (const auto& arg : expr->getArgs())
    {
        llvm::Value* argValue = handleExpr(arg.get());
        if (!argValue) return nullptr;
        args.push_back(argValue);
        argTypes.push_back(arg->getType());
    }

    // 5. Prepare arguments based on call type
    std::vector<llvm::Value*> preparedArgs;
    if (isDirectCall)
    {
        // For direct calls, arguments should match the LLVM function signature (likely ptr)
        // Assuming prepareArgument handles this or we adjust here.
        // Let's assume prepareArgument is smart enough for now, or returns the ptr directly.
        llvm::Function* targetFunc = llvm::cast<llvm::Function>(callableValue);
        llvm::FunctionType* funcType = targetFunc->getFunctionType();
        if (args.size() != funcType->getNumParams())
        {
            return codeGen.logError("Argument count mismatch for direct call to '" + calleeName + "'. Expected "
                                            + std::to_string(funcType->getNumParams()) + ", got " + std::to_string(args.size()) + ".",
                                    expr->line.value_or(0), expr->column.value_or(0));
        }
        for (size_t i = 0; i < args.size(); ++i)
        {
            // We need to ensure the argument is a PyObject* (ptr) for the direct call
            // Assuming handleExpr returns PyObject* and prepareArgument doesn't change it unnecessarily.
            // A simple pass-through might be sufficient if handleExpr guarantees PyObject*.
            llvm::Value* preparedArg = runtime->prepareArgument(args[i], argTypes[i], nullptr);  // Pass nullptr for expected type? Or get from funcType?
            if (!preparedArg || preparedArg->getType() != funcType->getParamType(i))
            {
                return codeGen.logError("Failed to prepare or type mismatch for argument " + std::to_string(i + 1) + " in direct call to '" + calleeName + "'. Expected " + llvmObjToString(funcType->getParamType(i)) + ".",
                                        expr->line.value_or(0), expr->column.value_or(0));
            }
            preparedArgs.push_back(preparedArg);
        }
    }
    else
    {
        // For runtime calls, prepare arguments for py_call_function (usually PyObject*)
        std::vector<std::shared_ptr<PyType>> expectedParamTypes;  // TODO: Get from callableType if possible
        for (size_t i = 0; i < args.size(); ++i)
        {
            std::shared_ptr<PyType> expectedType = (i < expectedParamTypes.size()) ? expectedParamTypes[i] : nullptr;
            llvm::Value* preparedArg = runtime->prepareArgument(args[i], argTypes[i], expectedType);
            if (!preparedArg)
            {
                return codeGen.logError("Failed to prepare argument " + std::to_string(i + 1) + " for runtime call.",
                                        expr->line.value_or(0), expr->column.value_or(0));
            }
            preparedArgs.push_back(preparedArg);
        }
    }

    // 6. Generate the call instruction
    llvm::Value* callResult = nullptr;
    if (isDirectCall)
    {
#ifdef DEBUG_CODEGEN_handleCallExpr
        DEBUG_LOG_DETAIL("HdlCallExpr", "Generating direct LLVM call instruction to '" + calleeName + "'");
#endif
        llvm::Function* targetFunc = llvm::cast<llvm::Function>(callableValue);
        callResult = builder.CreateCall(targetFunc->getFunctionType(), targetFunc, preparedArgs, "direct_call_res");
    }
    else
    {
#ifdef DEBUG_CODEGEN_handleCallExpr
        DEBUG_LOG_DETAIL("HdlCallExpr", "Generating runtime call via py_call_function for '" + calleeName + "'");
#endif
        // callableValue here is the loaded PyObject*
        callResult = runtime->createCallFunction(callableValue, preparedArgs);
        if (!callResult)
        {
            return codeGen.logError("Failed to generate code for runtime function call.",
                                    expr->line.value_or(0), expr->column.value_or(0));
        }
    }

    // 7. Mark return value source (same as before)
    runtime->markObjectSource(callResult, ObjectLifecycleManager::ObjectSource::FUNCTION_RETURN);

    // 8. Infer and set return type (same as before)
    std::shared_ptr<PyType> returnType = typeGen->inferCallReturnType(callableType, argTypes);
    expr->setType(returnType);

    return callResult;
}
// 字典表达式的具体代码生成逻辑 (修正后)
llvm::Value* CodeGenExpr::handleDictExpr(DictExprAST* expr)
{
    // 使用 CodeGenRuntime 代理
    auto* runtime = codeGen.getRuntimeGen();  // 获取 CodeGenRuntime* 代理
    if (!runtime)
    {
        // CodeGenRuntime 应该总是可以通过 CodeGenBase 获取
        return codeGen.logError("Internal error: CodeGenRuntime is not available.", expr->line.value_or(0), expr->column.value_or(0));
    }

    // 1. 获取推断出的类型
    auto dictType = expr->getType();
    if (!dictType || !dictType->isDict())
    {
        return codeGen.logError("Internal error: DictExprAST has invalid or non-dictionary type", expr->line.value_or(0), expr->column.value_or(0));
    }

    auto keyPyType = PyType::getDictKeyType(dictType);
    auto valuePyType = PyType::getDictValueType(dictType);

    if (!keyPyType || !valuePyType)
    {
        return codeGen.logError("Internal error: DictExprAST has invalid key/value PyTypes", expr->line.value_or(0), expr->column.value_or(0));
    }

    ObjectType* keyObjType = keyPyType->getObjectType();
    ObjectType* valueObjType = valuePyType->getObjectType();

    if (!keyObjType || !valueObjType)
    {
        return codeGen.logError("Internal error: Could not resolve ObjectType for dict key/value", expr->line.value_or(0), expr->column.value_or(0));
    }

    // 2. 创建空字典对象 (使用 CodeGenExpr 辅助函数，它内部调用 CodeGenRuntime)
    llvm::Value* dictObj = createDict(keyObjType, valueObjType);

    if (!dictObj)
    {
        // 错误已在 createDict 或 runtime->createDict 中记录
        return nullptr;
    }

    // 3. 迭代并添加键值对
    for (const auto& pair : expr->getPairs())
    {
        // 使用 handleExpr 生成键和值的代码
        llvm::Value* keyValRaw = handleExpr(pair.first.get());
        llvm::Value* valueValRaw = handleExpr(pair.second.get());

        if (!keyValRaw || !valueValRaw)
        {
            return nullptr;  // 错误已由 handleExpr 记录
        }

        // 注意：原始代码在这里有 ObjectLifecycleManager::adjustObject。
        // 按照要求，我们在此函数中避免了需要它的 PyCodeGen* 转型。
        // 这假设原始的键/值对象适合插入，或者任何必要的调整（如复制）
        // 在其他地方发生，或由运行时的 setDictItem 隐式处理（这可能有风险）。
        // 为了简化并满足要求，我们直接使用原始值。
        llvm::Value* keyValAdjusted = keyValRaw;
        llvm::Value* valueValAdjusted = valueValRaw;

        // 4. 调用运行时设置项 (使用 CodeGenExpr 辅助函数，它内部调用 CodeGenRuntime)
        // 传递推断出的 dictType，尽管基本的 setDictItem 可能不使用它。
        setDictItem(dictObj, keyValAdjusted, valueValAdjusted, dictType);
        // 键/值的引用计数由 py_dict_set_item 内部处理
    }

    // 5. 将字典标记为字面量来源
    runtime->markObjectSource(dictObj, ObjectLifecycleManager::ObjectSource::LITERAL);

    // 6. 在 CodeGenBase 状态中设置结果
    codeGen.setLastExprValue(dictObj);
    codeGen.setLastExprType(dictType);
    return dictObj;  // 返回生成的值
}

// 处理列表表达式
llvm::Value* CodeGenExpr::handleListExpr(ListExprAST* expr)
{
    auto* typeGen = codeGen.getTypeGen();
    auto* runtime = codeGen.getRuntimeGen();

    const auto& elements = expr->getElements();
    std::vector<llvm::Value*> elemValues;

    // 生成列表元素的代码
    for (const auto& elem : elements)
    {
        llvm::Value* elemValue = handleExpr(elem.get());
        if (!elemValue) return nullptr;
        elemValues.push_back(elemValue);
    }

    // 推导列表元素类型
    std::shared_ptr<PyType> elemType = typeGen->inferListElementType(elements);

    // 创建列表
    std::shared_ptr<PyType> listType = PyType::getList(elemType);
    expr->setType(listType);

    // 使用运行时API创建列表
    llvm::Value* list = createListWithValues(elemValues, elemType);

    // 标记列表为字面量，便于生命周期管理
    runtime->markObjectSource(list, ObjectLifecycleManager::ObjectSource::LITERAL);

    return list;
}

// 处理索引表达式
llvm::Value* CodeGenExpr::handleIndexExpr(IndexExprAST* expr)
{
    auto* typeGen = codeGen.getTypeGen();
    auto* runtime = codeGen.getRuntimeGen();

    // 生成目标和索引的代码
    llvm::Value* target = handleExpr(expr->getTarget());
    if (!target) return nullptr;

    llvm::Value* index = handleExpr(expr->getIndex());
    if (!index) return nullptr;

    // 获取目标和索引的类型
    std::shared_ptr<PyType> targetType = expr->getTarget()->getType();
    std::shared_ptr<PyType> indexType = expr->getIndex()->getType();

    /*    // 验证索引操作类型安全性
    if (!typeGen->validateIndexOperation(targetType, indexType))
    {
        return codeGen.logError("Invalid index operation: cannot use " + indexType->getObjectType()->getName() + " to index " + targetType->getObjectType()->getName(),
                                expr->line ? *expr->line : 0,
                                expr->column ? *expr->column : 0);
    } */

    // 推导索引表达式的结果类型
    std::shared_ptr<PyType> resultType = typeGen->inferIndexExprType(targetType, indexType);
    expr->setType(resultType);

    // 执行索引操作
    llvm::Value* result = handleIndexOperation(target, index, targetType, indexType);

    // 标记索引结果为索引访问，便于生命周期管理
    if (result)
    {
        runtime->markObjectSource(result, ObjectLifecycleManager::ObjectSource::INDEX_ACCESS);
    }

    return result;
}

// 二元操作处理函数
llvm::Value* CodeGenExpr::handleBinOp(PyTokenType op, llvm::Value* L, llvm::Value* R,
                                      std::shared_ptr<PyType> leftType,
                                      std::shared_ptr<PyType> rightType)
{
    auto* runtime = codeGen.getRuntimeGen();
    auto* typeGen = codeGen.getTypeGen();

    // 获取类型ID用于操作
    int leftTypeId = OperationCodeGenerator::getTypeId(leftType->getObjectType());
    int rightTypeId = OperationCodeGenerator::getTypeId(rightType->getObjectType());

    // 使用TypeOperations处理二元操作
    auto& registry = TypeOperationRegistry::getInstance();

    // 查找操作描述符
    BinaryOpDescriptor* desc = registry.getBinaryOpDescriptor(op, leftTypeId, rightTypeId);

    // 如果找不到直接匹配的操作，尝试找到可转换路径
    if (!desc)
    {
        std::pair<int, int> opPath = registry.findOperablePath(op, leftTypeId, rightTypeId);

        // 如果需要转换左操作数
        if (opPath.first != leftTypeId)
        {
            TypeConversionDescriptor* convDesc =
                    registry.getTypeConversionDescriptor(leftTypeId, opPath.first);

            if (convDesc)
            {
                // 转换左操作数
                // 或者方案2: 使用通用接口避免转型
                L = OperationCodeGenerator::handleTypeConversion(
                        codeGen, L, leftTypeId, opPath.first);

                // 更新类型ID
                leftTypeId = opPath.first;
            }
        }

        // 如果需要转换右操作数
        if (opPath.second != rightTypeId)
        {
            TypeConversionDescriptor* convDesc =
                    registry.getTypeConversionDescriptor(rightTypeId, opPath.second);

            if (convDesc)
            {
                // 转换右操作数
                // 修改后:
                R = OperationCodeGenerator::handleTypeConversion(
                        codeGen, R, rightTypeId, opPath.second);

                // 更新类型ID
                rightTypeId = opPath.second;
            }
        }

        // 尝试再次获取操作描述符
        desc = registry.getBinaryOpDescriptor(op, leftTypeId, rightTypeId);
    }

    if (!desc)
    {
        // --- 修改：错误信息使用 op (PyTokenType) ---
        // Note: Need a way to convert PyTokenType back to string for error message
        // For now, just log the integer value or a generic message
        return codeGen.logError("Unsupported binary operation (token: " + std::to_string(op) + ") between " + leftType->getObjectType()->getName() + " and " + rightType->getObjectType()->getName());
        // --- 结束修改 ---
    }

    // 执行操作
    llvm::Value* result;

    // 修改为:
    if (desc->customImpl)
    {
        // 使用自定义实现 - 安全地将 CodeGenBase 转为 PyCodeGen
        PyCodeGen* pyCodeGen = codeGen.asPyCodeGen();
        if (pyCodeGen)
        {
            result = desc->customImpl(*pyCodeGen, L, R);
        }
        else
        {
            // 使用默认运行时函数作为备选
            result = OperationCodeGenerator::handleBinaryOp(
                    codeGen, op, L, R, leftTypeId, rightTypeId);
        }
    }
    else
    {
        // 使用默认运行时函数
        result = OperationCodeGenerator::handleBinaryOp(
                codeGen, op, L, R, leftTypeId, rightTypeId);
    }

    // 标记结果为二元操作结果，便于生命周期管理
    if (result)
    {
        runtime->markObjectSource(result, ObjectLifecycleManager::ObjectSource::BINARY_OP);
    }

    return result;
}

// 一元操作处理函数
llvm::Value* CodeGenExpr::handleUnaryOp(PyTokenType op, llvm::Value* operand,
                                        std::shared_ptr<PyType> operandType)
{
    auto* runtime = codeGen.getRuntimeGen();

    // 获取类型ID用于操作
    int operandTypeId = OperationCodeGenerator::getTypeId(operandType->getObjectType());

    // 使用TypeOperations处理一元操作
    auto& registry = TypeOperationRegistry::getInstance();

    // 查找操作描述符
    UnaryOpDescriptor* desc = registry.getUnaryOpDescriptor(op, operandTypeId);

    if (!desc)
    {
        // --- 修改：错误信息使用 op (PyTokenType) ---
        // Note: Need a way to convert PyTokenType back to string for error message
        return codeGen.logError("Unsupported unary operation (token: " + std::to_string(op) + ") on " + operandType->getObjectType()->getName());
        // --- 结束修改 ---
    }

    // 执行操作
    llvm::Value* result;
    PyCodeGen* pyCodeGen = codeGen.asPyCodeGen();
    if (desc->customImpl)
    {
        // 使用自定义实现
        result = desc->customImpl(*pyCodeGen, operand);
    }
    else
    {
        // 使用默认运行时函数
        result = OperationCodeGenerator::handleUnaryOp(
                codeGen, op, operand, operandTypeId);
    }

    // 标记结果为一元操作结果，便于生命周期管理
    if (result)
    {
        runtime->markObjectSource(result, ObjectLifecycleManager::ObjectSource::UNARY_OP);
    }

    return result;
}

// 索引操作处理函数
llvm::Value* CodeGenExpr::handleIndexOperation(llvm::Value* target, llvm::Value* index,
                                               std::shared_ptr<PyType> targetType,
                                               std::shared_ptr<PyType> indexType)
{
    auto* runtime = codeGen.getRuntimeGen();
    auto& builder = codeGen.getBuilder();

    // 获取目标和索引的类型ID
    int targetTypeId = OperationCodeGenerator::getTypeId(targetType->getObjectType());
    int indexTypeId = OperationCodeGenerator::getTypeId(indexType->getObjectType());

    // 处理ANY类型的特殊情况
    if (targetTypeId == PY_TYPE_ANY || indexTypeId == PY_TYPE_ANY)
    {
        // 使用通用索引操作 (py_object_index 接受 PyObject*)
        llvm::Function* indexFunc = codeGen.getOrCreateExternalFunction(
                "py_object_index",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::PointerType::get(codeGen.getContext(), 0)});

        return builder.CreateCall(indexFunc, {target, index}, "index_result");
    }

    // 对于列表索引
    if (targetType->isList())
    {
        // 确保索引是 Python 整数或布尔类型 (或可转换)
        if (!indexType->isInt() && !indexType->isBool())
        {
            // 尝试将 Python 对象转换为整数对象
            auto& registry = TypeOperationRegistry::getInstance();
            TypeConversionDescriptor* convDesc =
                    registry.getTypeConversionDescriptor(indexTypeId, PY_TYPE_INT);

            if (convDesc)
            {
                // 转换 index (这会生成一个新的 PyObject*)
                index = OperationCodeGenerator::handleTypeConversion(
                        codeGen, index, indexTypeId, PY_TYPE_INT);
                indexTypeId = PY_TYPE_INT;
                indexType = PyType::getInt();  // 更新类型
            }
            else
            {
                return codeGen.logError("List indices must be integers or booleans");
            }
        }

        // --- 修改：直接调用 py_list_get_item，传递 PyObject* index ---
        // 获取 py_list_get_item 函数 (签名已在 CodeGenRuntime 中更新)
        llvm::Function* getItemFunc = runtime->getRuntimeFunction(  // Use runtime->getRuntimeFunction
                "py_list_get_item",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::PointerType::get(codeGen.getContext(), 0)});  // <-- 确认签名

        // 直接使用 PyObject* index 调用
        return builder.CreateCall(getItemFunc, {target, index}, "list_item");  // <-- 使用 PyObject* index
        // --- 修改结束 ---
    }

    // 对于字典索引 (保持不变，py_dict_get_item 接受 PyObject*)
    if (targetType->isDict())
    {
        llvm::Function* getItemFunc = runtime->getRuntimeFunction(  // Use runtime->getRuntimeFunction
                "py_dict_get_item",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::PointerType::get(codeGen.getContext(), 0)});

        return builder.CreateCall(getItemFunc, {target, index}, "dict_item");
    }

    // 对于字符串索引
    if (targetType->isString())
    {
        // 确保索引是 Python 整数或布尔类型 (或可转换)
        if (!indexType->isInt() && !indexType->isBool())
        {
            // 尝试将 Python 对象转换为整数对象
            auto& registry = TypeOperationRegistry::getInstance();
            TypeConversionDescriptor* convDesc =
                    registry.getTypeConversionDescriptor(indexTypeId, PY_TYPE_INT);

            if (convDesc)
            {
                index = OperationCodeGenerator::handleTypeConversion(
                        codeGen, index, indexTypeId, PY_TYPE_INT);
                indexTypeId = PY_TYPE_INT;
                indexType = PyType::getInt();  // 更新类型
            }
            else
            {
                return codeGen.logError("String indices must be integers or booleans");
            }
        }

        // --- 修改：直接调用 py_string_get_char，传递 PyObject* index ---
        // 获取 py_string_get_char 函数 (签名已在 py_container.h 中更新)
        llvm::Function* getCharFunc = runtime->getRuntimeFunction(  // Use runtime->getRuntimeFunction
                "py_string_get_char",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::PointerType::get(codeGen.getContext(), 0)});  // <-- 修改签名

        // 直接使用 PyObject* index 调用
        return builder.CreateCall(getCharFunc, {target, index}, "str_char");  // <-- 使用 PyObject* index
        // --- 修改结束 ---
    }

    // 对于其他类型或未知类型，使用通用索引操作 (py_object_index 接受 PyObject*)
    llvm::Function* indexFunc = runtime->getRuntimeFunction(  // Use runtime->getRuntimeFunction
            "py_object_index",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {llvm::PointerType::get(codeGen.getContext(), 0),
             llvm::PointerType::get(codeGen.getContext(), 0)});

    return builder.CreateCall(indexFunc, {target, index}, "index_result");
}

// 在CodeGenRuntime类的public部分添加:

// 代理对象创建方法
llvm::Value* CodeGenRuntime::createIntObject(llvm::Value* value)  // Takes LLVM i32
{
    llvm::Function* createFunc = getRuntimeFunction(
            "py_create_int",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {llvm::Type::getInt32Ty(codeGen.getContext())});
    return codeGen.getBuilder().CreateCall(createFunc, {value}, "int_obj");
}

llvm::Value* CodeGenRuntime::createDoubleObject(llvm::Value* value)  // Takes LLVM double
{
    llvm::Function* createFunc = getRuntimeFunction(
            "py_create_double",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {llvm::Type::getDoubleTy(codeGen.getContext())});
    return codeGen.getBuilder().CreateCall(createFunc, {value}, "double_obj");
}

llvm::Value* CodeGenRuntime::createBoolObject(llvm::Value* value)
{
    /*  if (runtime)
    {
        return runtime->createBoolObject(value);
    } */

    llvm::Function* createFunc = getRuntimeFunction(
            "py_create_bool",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {llvm::Type::getInt1Ty(codeGen.getContext())});

    return codeGen.getBuilder().CreateCall(createFunc, {value}, "bool_obj");
}

llvm::Value* CodeGenRuntime::createStringObject(llvm::Value* value)
{
    /*  if (runtime)
    {
        return runtime->createStringObject(value);
    } */

    llvm::Function* createFunc = getRuntimeFunction(
            "py_create_string",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {llvm::PointerType::get(llvm::Type::getInt8Ty(codeGen.getContext()), 0)});

    return codeGen.getBuilder().CreateCall(createFunc, {value}, "str_obj");
}

// 创建整数字面量 (original, takes int)
llvm::Value* CodeGenExpr::createIntLiteral(int value)
{
    auto& builder = codeGen.getBuilder();
    auto* runtime = codeGen.getRuntimeGen();
    llvm::Value* intValue = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(codeGen.getContext()), value);
    return runtime->createIntObject(intValue);  // Calls original runtime proxy
}

llvm::Value* CodeGenRuntime::createIntObjectFromString(llvm::Value* strPtr)
{
    // 获取签名：PyObject* py_create_int_bystring(const char* s, int base)
    llvm::Function* createFunc = getRuntimeFunction(
            "py_create_int_bystring",
            llvm::PointerType::get(codeGen.getContext(), 0),                          // Return type: PyObject* (ptr)
            {llvm::PointerType::get(llvm::Type::getInt8Ty(codeGen.getContext()), 0),  // Arg 1: char* (ptr)
             llvm::Type::getInt32Ty(codeGen.getContext())});                          // Arg 2: int (i32) <-- 添加 base 类型

    // 创建 base 参数 (通常是 10)
    llvm::Value* baseValue = llvm::ConstantInt::get(llvm::Type::getInt32Ty(codeGen.getContext()), 10);

    // 调用时传递 strPtr 和 baseValue
    return codeGen.getBuilder().CreateCall(createFunc, {strPtr, baseValue}, "int_obj_from_str");  // <-- 传递两个参数
}

llvm::Value* CodeGenRuntime::createDoubleObjectFromString(llvm::Value* strPtr)
{
    // 获取签名：PyObject* py_create_double_bystring(const char* s, int base, mp_bitcnt_t precision)
    // 注意：mp_bitcnt_t 通常是 unsigned long，这里用 i64 近似
    llvm::Type* precisionType = llvm::Type::getInt64Ty(codeGen.getContext());  // 使用 i64 代表 mp_bitcnt_t

    llvm::Function* createFunc = getRuntimeFunction(
            "py_create_double_bystring",
            llvm::PointerType::get(codeGen.getContext(), 0),                          // Return type: PyObject* (ptr)
            {llvm::PointerType::get(llvm::Type::getInt8Ty(codeGen.getContext()), 0),  // Arg 1: char* (ptr)
             llvm::Type::getInt32Ty(codeGen.getContext()),                            // Arg 2: int (i32) <-- 添加 base 类型
             precisionType});                                                         // Arg 3: mp_bitcnt_t (i64) <-- 添加 precision 类型

    // 创建 base 参数 (通常是 10)
    llvm::Value* baseValue = llvm::ConstantInt::get(llvm::Type::getInt32Ty(codeGen.getContext()), 10);
    // 创建 precision 参数 (传递 0 让 C++ 端使用默认精度)
    llvm::Value* precisionValue = llvm::ConstantInt::get(precisionType, 0);

    // 调用时传递 strPtr, baseValue, precisionValue
    return codeGen.getBuilder().CreateCall(createFunc, {strPtr, baseValue, precisionValue}, "double_obj_from_str");  // <-- 传递三个参数
}

// 创建浮点数字面量 (original, takes double)
llvm::Value* CodeGenExpr::createDoubleLiteral(double value)
{
    auto& builder = codeGen.getBuilder();
    auto* runtime = codeGen.getRuntimeGen();
    llvm::Value* doubleValue = llvm::ConstantFP::get(
            llvm::Type::getDoubleTy(codeGen.getContext()), value);
    return runtime->createDoubleObject(doubleValue);  // Calls original runtime proxy
}

// 创建整数字面量
llvm::Value* CodeGenExpr::createIntLiteralFromString(const std::string& value)
{
    auto& builder = codeGen.getBuilder();
    auto* runtime = codeGen.getRuntimeGen();

    // 创建 C 字符串常量 (as global)
    // Note: LLVM may optimize identical constants. Runtime function should copy if needed.
    llvm::Value* strPtr = builder.CreateGlobalString(value, "int_str_const");

    // 使用运行时从字符串创建整数对象
    return runtime->createIntObjectFromString(strPtr);
}

// 创建浮点数字面量
llvm::Value* CodeGenExpr::createDoubleLiteralFromString(const std::string& value)
{
    auto& builder = codeGen.getBuilder();
    auto* runtime = codeGen.getRuntimeGen();

    // 创建 C 字符串常量 (as global)
    llvm::Value* strPtr = builder.CreateGlobalString(value, "double_str_const");

    // 使用运行时从字符串创建浮点数对象
    return runtime->createDoubleObjectFromString(strPtr);
}

// 创建布尔字面量
llvm::Value* CodeGenExpr::createBoolLiteral(bool value)
{
    auto& builder = codeGen.getBuilder();
    auto* runtime = codeGen.getRuntimeGen();

    // 创建布尔常量
    llvm::Value* boolValue = llvm::ConstantInt::get(
            llvm::Type::getInt1Ty(codeGen.getContext()), value ? 1 : 0);

    // 使用运行时创建布尔对象
    return runtime->createBoolObject(boolValue);
}

// 创建字符串字面量
llvm::Value* CodeGenExpr::createStringLiteral(const std::string& value)
{
    auto& builder = codeGen.getBuilder();
    auto* runtime = codeGen.getRuntimeGen();

    // 创建字符串常量
    llvm::Value* strPtr = builder.CreateGlobalString(value, "str_const");

    // 使用运行时创建字符串对象
    return runtime->createStringObject(strPtr);
}

// 创建None字面量
llvm::Value* CodeGenExpr::createNoneLiteral()
{
    auto* runtime = codeGen.getRuntimeGen();

    // 使用运行时获取None对象
    llvm::Function* getNoneFunc = codeGen.getOrCreateExternalFunction(
            "py_get_none",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {});

    return codeGen.getBuilder().CreateCall(getNoneFunc, {}, "none");
}

// 创建空字典
llvm::Value* CodeGenExpr::createDict(ObjectType* keyType, ObjectType* valueType)
{
    auto* runtime = codeGen.getRuntimeGen();
    // 使用 CodeGenRuntime 代理方法
    llvm::Value* dictObj = runtime->createDict(keyType, valueType);
    if (!dictObj)
    {
        codeGen.logError("Failed to create dictionary object via CodeGenRuntime");
        return nullptr;
    }
    return dictObj;
}

// 创建带有初始键值对的字典
// 创建带有初始键值对的字典
llvm::Value* CodeGenExpr::createDictWithPairs(
        const std::vector<std::pair<llvm::Value*, llvm::Value*>>& pairs,
        ObjectType* keyType,
        ObjectType* valueType)
{
    auto* runtime = codeGen.getRuntimeGen();

    // 1. 创建空字典
    llvm::Value* dictObj = createDict(keyType, valueType);
    if (!dictObj)
    {
        return nullptr;  // 错误已在 createDict 中记录
    }

    // 2. 填充键值对
    for (const auto& pair : pairs)
    {
        llvm::Value* key = pair.first;
        llvm::Value* value = pair.second;

        if (!key || !value)
        {
            codeGen.logError("Invalid key or value provided to createDictWithPairs");
            // 如果错误处理需要，考虑对 dictObj 进行 decref
            return nullptr;
        }

        // 3. 使用 CodeGenExpr 代理方法设置项
        // 这里不容易获得整体的 dict PyType，传递 nullptr。
        setDictItem(dictObj, key, value, nullptr);

        // 4. 引用计数:
        // py_dict_set_item 在插入 *新* 项或替换现有项时内部处理 incref。
        // 这里不需要显式的 incRef。
        // runtime->incRef(key);   // 不需要 - 由 py_dict_set_item 处理
        // runtime->incRef(value); // 不需要 - 由 py_dict_set_item 处理
    }

    // 可选：如果 runtime->createDict 未处理，则标记来源
    // runtime->markObjectSource(dictObj, ObjectLifecycleManager::ObjectSource::LITERAL);
    return dictObj;
}

// 获取字典项 (如果需要，可以实现)
llvm::Value* CodeGenExpr::getDictItem(llvm::Value* dict, llvm::Value* key,
                                      std::shared_ptr<PyType> dictType)
{
    auto* runtime = codeGen.getRuntimeGen();
    // 使用 CodeGenRuntime 代理方法
    llvm::Value* item = runtime->getDictItem(dict, key);

    // 关于引用计数的说明：py_dict_get_item 返回一个 *借用* (borrowed) 引用。
    // 运行时函数 *不会* 增加引用计数。
    // 如果调用者需要持有该引用，必须调用 incRef。
    // 这个辅助函数只是返回运行时调用的结果。
    // 调用者（例如 handleIndexExpr）负责管理生命周期。

    // 错误处理：如果键未找到或目标不是字典，py_dict_get_item 返回 NULL。
    // 调用者应处理 NULL 结果。
    return item;
}

// 设置字典项 (如果需要，可以实现)
void CodeGenExpr::setDictItem(llvm::Value* dict, llvm::Value* key, llvm::Value* value,
                              std::shared_ptr<PyType> dictType)
{
    auto* runtime = codeGen.getRuntimeGen();
    // 使用 CodeGenRuntime 代理方法
    runtime->setDictItem(dict, key, value);

    // 关于引用计数的说明：py_dict_set_item 内部处理引用计数。
    // - 如果键是新的：incref(key), incref(value)
    // - 如果键已存在：decref(old_value), incref(new_value)
    // 这里不需要显式的引用计数管理。
}

// 创建列表
llvm::Value* CodeGenExpr::createList(int size, std::shared_ptr<PyType> elemType)
{
    auto& builder = codeGen.getBuilder();
    auto* runtime = codeGen.getRuntimeGen();
    auto& context = codeGen.getContext();  // 获取 LLVMContext

    // 创建大小常量 (i32)
    llvm::Value* sizeValue = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(context), size);

    // --- FIX: Calculate elemTypeId and pass as llvm::Value* ---
    // 1. 获取 ObjectType*
    ObjectType* elemObjType = elemType->getObjectType();
    if (!elemObjType)
    {
        codeGen.logError("Internal Error: Cannot get ObjectType for list element type in createList.");
        return nullptr;
    }
    // 2. 获取类型 ID (int)
    int elemTypeIdInt = OperationCodeGenerator::getTypeId(elemObjType);
    // 3. 创建类型 ID 的 LLVM 常量 (i32)
    llvm::Value* elemTypeIdValue = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(context), elemTypeIdInt);

    // 使用运行时创建列表对象，传递 i32 size 和 i32 type ID
    return runtime->createList(sizeValue, elemTypeIdValue);
    // --- FIX END ---
}

// 创建带有初始值的列表
llvm::Value* CodeGenExpr::createListWithValues(const std::vector<llvm::Value*>& values,
                                               std::shared_ptr<PyType> elemType)
{
    auto* runtime = codeGen.getRuntimeGen();
    auto& context = codeGen.getContext();

    // 1. 创建一个初始为空的列表 (长度为0)，但可以指定初始容量
    //    传递 values.size() 作为初始容量提示
    llvm::Value* initialCapacity = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), values.size());
    // --- FIX 1: Use getObjectType() ---
    int elemTypeIdInt = OperationCodeGenerator::getTypeId(elemType->getObjectType());  // 获取元素类型ID

#ifdef DEBUG_CODEGEN_createListWithValues
    // --- DEBUGGING START ---
    std::cerr << "DEBUG [createListWithValues]: Inferred elemType: " << elemType->toString()
              << ", ObjectType: " << (elemType->getObjectType() ? elemType->getObjectType()->getName() : "null")
              << ", Got elemTypeIdInt: " << elemTypeIdInt << std::endl;
// --- DEBUGGING END ---
#endif

    llvm::Value* elemTypeIdValue = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), elemTypeIdInt);
    // --- 修改 CodeGenRuntime::createList 的调用签名以匹配 ---
    // --- Call assumes createList now takes llvm::Value* for type ID ---
    llvm::Value* list = runtime->createList(initialCapacity, elemTypeIdValue);  // createList 现在接收 i32 容量和 i32 类型ID

    if (!list)
    {
        codeGen.logError("Failed to create list object");
        return nullptr;
    }

    // 2. 循环调用 py_list_append 添加元素
    llvm::Function* appendFunc = runtime->getRuntimeFunction(
            "py_list_append",
            llvm::PointerType::get(context, 0),   // Return: PyObject* (the list itself or NULL on error)
            {llvm::PointerType::get(context, 0),  // Arg 1: PyObject* (list)
             llvm::PointerType::get(context, 0)}  // Arg 2: PyObject* (item)
    );

    for (size_t i = 0; i < values.size(); ++i)
    {
        llvm::Value* item = values[i];
        if (!item)
        {
            // Should not happen if previous codegen succeeded, but check defensively
            codeGen.logError("NULL value encountered while creating list literal at index " + std::to_string(i));
            // Cleanup: decref the partially built list and already appended items? Complex.
            // For now, just return null.
            runtime->decRef(list);  // Decref the list itself
            return nullptr;
        }

        // 调用 py_list_append
        // py_list_append 内部会处理引用计数 (incref item)
        codeGen.getBuilder().CreateCall(appendFunc, {list, item});

        // 注意：不再需要创建和管理 Python 索引对象
        // 注意：不再需要手动调用 runtime->incRef(item)，因为 py_list_append 会做
    }

    return list;  // 返回构建好的列表
}

// 获取列表元素
llvm::Value* CodeGenExpr::getListElement(llvm::Value* list, llvm::Value* index,
                                         std::shared_ptr<PyType> listType)
{
    auto* runtime = codeGen.getRuntimeGen();

    // 使用运行时获取列表元素
    return runtime->getListElement(list, index);
}

// 设置列表元素
void CodeGenExpr::setListElement(llvm::Value* list, llvm::Value* index,
                                 llvm::Value* value, std::shared_ptr<PyType> listType)
{
    auto* runtime = codeGen.getRuntimeGen();

    // 确保 index 是 ptr 类型 (PyObject*)
    if (!index->getType()->isPointerTy())
    {
        codeGen.logError("Internal Error: setListElement called with non-pointer index type.");
        // Attempting to proceed might crash, better to stop or handle gracefully.
        return;  // Or throw an exception / assert
    }

    // 使用运行时设置列表元素 - runtime->setListElement 期望 ptr 索引
    runtime->setListElement(list, index, value);
}

}  // namespace llvmpy