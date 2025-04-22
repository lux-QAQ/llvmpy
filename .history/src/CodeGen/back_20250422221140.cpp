#include "CodeGen/CodeGenExpr.h"
#include "CodeGen/CodeGenBase.h"
#include "CodeGen/CodeGenType.h"
#include "CodeGen/CodeGenRuntime.h"

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
    double value = expr->getValue();

    // 检查是否是整数
    if (std::floor(value) == value && value <= INT_MAX && value >= INT_MIN)
    {
        // 创建整数字面量
        return createIntLiteral(static_cast<int>(value));
    }
    else
    {
        // 创建浮点数字面量
        return createDoubleLiteral(value);
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
    llvm::Value* value = codeGen.getSymbolTable().getVariable(name);
    ObjectType* type = codeGen.getSymbolTable().getVariableType(name);  // 获取类型信息

    if (!value)
    {
        return codeGen.logError("Unknown variable '" + name + "'",
                                expr->line.value_or(0),
                                expr->column.value_or(0));
    }

    // --- FIX: Load from GlobalVariable if necessary ---
    if (llvm::GlobalVariable* gv = llvm::dyn_cast<llvm::GlobalVariable>(value))
    {
        // 这是一个全局变量 (很可能是顶层函数对象)。加载实际的 PyObject*。
        llvm::Type* expectedPtrType = llvm::PointerType::get(codeGen.getContext(), 0);  // 假设 PyObject* 是 ptr
        if (gv->getValueType() != expectedPtrType)
        {
            return codeGen.logError("Global variable '" + name + "' does not hold expected pointer type.",
                                    expr->line.value_or(0), expr->column.value_or(0));
        }
        // 创建 load 指令
        llvm::Value* loadedValue = codeGen.getBuilder().CreateLoad(expectedPtrType, gv, name + "_loaded");
#ifdef DEBUG_CODEGEN_handleVariableExpr
        DEBUG_LOG_DETAIL("HdlVarExpr", "Loaded value from GlobalVariable '" + name + "': " + llvmObjToString(loadedValue));
#endif
        // 设置表达式的类型 (应该与符号表中的类型匹配)
        if (type)
        {
            expr->setType(PyType::fromObjectType(type));
        }
        else
        {
            expr->setType(PyType::getAny());  // 回退
            codeGen.logWarning("Type information missing for global variable '" + name + "'. Assuming Any.", expr->line.value_or(0), expr->column.value_or(0));
        }
        return loadedValue;  // 返回加载后的值 (PyObject*)
    }
    else
    {
        // 这可能是局部变量 (alloca) 或直接值 (嵌套函数 PyObject*)。
        // 这里不需要 load。后续使用（如赋值源）会自动处理 alloca 的 load。
#ifdef DEBUG_CODEGEN_handleVariableExpr
        DEBUG_LOG_DETAIL("HdlVarExpr", "Using direct value/local for variable '" + name + "': " + llvmObjToString(value));
#endif
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
        return value;  // 返回原始值 (alloca* 或 PyObject*)
    }
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
    auto& symTable = codeGen.getSymbolTable();  // Get symbol table

    // --- MODIFICATION START ---

    // 1. 获取被调用者的表达式 (假设 CallExprAST 存储了 callee 表达式)
    //    如果 CallExprAST 只存了名字，我们需要先处理名字
    const std::string& calleeName = expr->getCallee();  // Get the name as string

    llvm::Value* callableObj = nullptr;
    std::shared_ptr<PyType> callableType = nullptr;

    // 2. 检查 calleeName 是否是符号表中的变量
    if (symTable.hasVariable(calleeName))
    {
        llvm::Value* varValue = symTable.getVariable(calleeName);
        ObjectType* objType = symTable.getVariableType(calleeName);
        callableType = objType ? PyType::fromObjectType(objType) : PyType::getAny();

        // 2.1. 加载变量的值 (可能是全局变量或局部变量)
        if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(varValue))
        {
            // Load from global variable (like top-level function objects)
            callableObj = builder.CreateLoad(gv->getValueType(), gv, calleeName + "_callable_loaded");
        }
        else if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(varValue))
        {
            // Load from local variable (alloca)
            callableObj = builder.CreateLoad(allocaInst->getAllocatedType(), allocaInst, calleeName + "_callable_loaded");
        }
        else
        {
            // Assume it's already a direct value (e.g., PyObject* for nested funcs, though less common here)
            callableObj = varValue;
        }

        // 2.2. 检查类型是否可调用
        if (!callableType->isFunction() && !callableType->isAny())
        {
            return codeGen.logTypeError("Variable '" + calleeName + "' is not callable.",
                                        expr->line.value_or(0), expr->column.value_or(0));
        }
    }
    else
    {
        // --- MODIFICATION: Print symbol table on lookup failure ---
        std::cerr << "--- Symbol Table Dump (Lookup Failed for '" << calleeName << "') ---" << std::endl;
        symTable.dump(std::cerr); // Assuming PySymbolTable has a dump method
        std::cerr << "---------------------------------------------------------" << std::endl;
        // --- End Modification ---

        // 3. 如果不是变量，尝试查找是否为已知函数 (例如内置函数)
        //    当前的错误 "Unknown function: b" 表明这里查找失败了，这是预期的，因为 'b' 是变量。
        //    对于真正的未知函数/变量，这里应该报错。
        //    TODO: 未来可以扩展这里以支持内置函数等。
        return codeGen.logError("Unknown function or variable: " + calleeName,
                                expr->line.value_or(0), expr->column.value_or(0));
    }

    if (!callableObj)
    {
        // 如果前面的逻辑未能解析 callableObj，则报错
        return codeGen.logError("Could not resolve callable expression: " + calleeName,
                                expr->line.value_or(0), expr->column.value_or(0));
    }

    // --- MODIFICATION END ---

    // 4. 生成参数的代码 (与之前类似)
    std::vector<llvm::Value*> args;
    std::vector<std::shared_ptr<PyType>> argTypes;
    for (const auto& arg : expr->getArgs())
    {
        llvm::Value* argValue = handleExpr(arg.get());
        if (!argValue) return nullptr;
        args.push_back(argValue);
        argTypes.push_back(arg->getType());  // 获取参数表达式的类型
    }

    // 5. 准备参数 (与之前类似, 但可能需要从 callableType 获取预期类型)
    std::vector<llvm::Value*> preparedArgs;
    // TODO: 如果 callableType 是具体的 FunctionType，获取参数类型列表
    std::vector<std::shared_ptr<PyType>> expectedParamTypes;  // = callableType->getParamTypes();
    for (size_t i = 0; i < args.size(); ++i)
    {
        std::shared_ptr<PyType> expectedType = (i < expectedParamTypes.size()) ? expectedParamTypes[i] : nullptr;
        llvm::Value* preparedArg = runtime->prepareArgument(args[i], argTypes[i], expectedType);
        if (!preparedArg)
        {
            return codeGen.logError("Failed to prepare argument " + std::to_string(i + 1) + " for call.",
                                    expr->line.value_or(0), expr->column.value_or(0));
        }
        preparedArgs.push_back(preparedArg);
    }

    // 6. 生成对运行时函数调用的代码
    //    你需要一个 CodeGenRuntime 方法来创建这个调用
    //    例如: llvm::Value* createCallFunction(llvm::Value* callableObj, const std::vector<llvm::Value*>& preparedArgs);
    llvm::Value* callResult = runtime->createCallFunction(callableObj, preparedArgs);  // <--- 调用新的 Runtime 方法
    if (!callResult)
    {
        // createCallFunction 内部应记录详细错误
        return codeGen.logError("Failed to generate code for function call.",
                                expr->line.value_or(0), expr->column.value_or(0));
    }

    // 7. 标记返回值的来源 (与之前相同)
    runtime->markObjectSource(callResult, ObjectLifecycleManager::ObjectSource::FUNCTION_RETURN);

    // 8. 推导并设置表达式的返回类型
    //    需要 CodeGenType 中的一个新方法，根据 callableType 和 argTypes 推断
    std::shared_ptr<PyType> returnType = typeGen->inferCallReturnType(callableType, argTypes);  // <--- 调用新的 TypeGen 方法
    expr->setType(returnType);                                                                  // 设置 CallExprAST 节点的类型

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

    // 验证索引操作类型安全性
    if (!typeGen->validateIndexOperation(targetType, indexType))
    {
        return codeGen.logError("Invalid index operation: cannot use " + indexType->getObjectType()->getName() + " to index " + targetType->getObjectType()->getName(),
                                expr->line ? *expr->line : 0,
                                expr->column ? *expr->column : 0);
    }

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
        // 使用通用索引操作
        llvm::Function* indexFunc = codeGen.getOrCreateExternalFunction(
                "py_object_index",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::PointerType::get(codeGen.getContext(), 0)});

        return builder.CreateCall(indexFunc, {target, index}, "index_result");
    }

    // 对于列表索引，我们可以直接使用运行时函数
    if (targetType->isList())
    {
        // 确保索引是 Python 整数或布尔类型 (这部分逻辑保持)
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
                // 更新 indexType shared_ptr
                indexType = PyType::getInt();
            }
            else
            {
                // 如果不能转换为 int，并且原始类型不是 bool，则报错
                return codeGen.logError("List indices must be integers or booleans");
            }
        }

        // --- 修改：使用 py_extract_constant_int ---
        // 获取运行时函数 py_extract_constant_int
        llvm::Function* extractIntFunc = codeGen.getOrCreateExternalFunction(
                "py_extract_constant_int",                           // <--- 使用现有函数
                llvm::Type::getInt32Ty(codeGen.getContext()),        // 返回 i32
                {llvm::PointerType::get(codeGen.getContext(), 0)});  // 参数: PyObject*

        // 'index' 是指向 Python 整数或布尔对象的 PyObject*
        llvm::Value* cIntValue = builder.CreateCall(extractIntFunc, {index}, "index_cint");
        // --- 修改结束 ---

        // 获取 py_list_get_item 函数 (保持不变)
        llvm::Function* getItemFunc = codeGen.getOrCreateExternalFunction(
                "py_list_get_item",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::Type::getInt32Ty(codeGen.getContext())});

        // 使用提取出的 C int 值调用 py_list_get_item
        return builder.CreateCall(getItemFunc, {target, cIntValue}, "list_item");  // <--- 使用 cIntValue
    }

    // 对于字典索引
    if (targetType->isDict())
    {
        // 调用运行时函数获取字典项
        llvm::Function* getItemFunc = codeGen.getOrCreateExternalFunction(
                "py_dict_get_item",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::PointerType::get(codeGen.getContext(), 0)});

        return builder.CreateCall(getItemFunc, {target, index}, "dict_item");
    }

    // 对于字符串索引
    if (targetType->isString())
    {
        // 确保索引是 Python 整数或布尔类型 (这部分逻辑保持)
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
                // 更新 indexType shared_ptr
                indexType = PyType::getInt();
            }
            else
            {
                // 如果不能转换为 int，并且原始类型不是 bool，则报错
                return codeGen.logError("String indices must be integers or booleans");
            }
        }

        // --- 修改：使用 py_extract_constant_int ---
        // 获取运行时函数 py_extract_constant_int
        llvm::Function* extractIntFunc = codeGen.getOrCreateExternalFunction(
                "py_extract_constant_int",                           // <--- 使用现有函数
                llvm::Type::getInt32Ty(codeGen.getContext()),        // 返回 i32
                {llvm::PointerType::get(codeGen.getContext(), 0)});  // 参数: PyObject*

        // 'index' 是指向 Python 整数或布尔对象的 PyObject*
        llvm::Value* cIntValue = builder.CreateCall(extractIntFunc, {index}, "index_cint_str");
        // --- 修改结束 ---

        // 获取 py_string_get_char 函数 (保持不变)
        llvm::Function* getCharFunc = codeGen.getOrCreateExternalFunction(
                "py_string_get_char",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::Type::getInt32Ty(codeGen.getContext())});

        // 使用提取出的 C int 值调用 py_string_get_char
        return builder.CreateCall(getCharFunc, {target, cIntValue}, "str_char");  // <--- 使用 cIntValue
    }

    // 使用通用索引操作
    llvm::Function* indexFunc = codeGen.getOrCreateExternalFunction(
            "py_object_index",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {llvm::PointerType::get(codeGen.getContext(), 0),
             llvm::PointerType::get(codeGen.getContext(), 0)});

    return builder.CreateCall(indexFunc, {target, index}, "index_result");
}

// 在CodeGenRuntime类的public部分添加:

// 代理对象创建方法
llvm::Value* CodeGenRuntime::createIntObject(llvm::Value* value)
{
    /*   if (runtime)
    {
        return runtime->createIntObject(value);
    } */

    // 如果没有runtime，使用内置实现
    llvm::Function* createFunc = getRuntimeFunction(
            "py_create_int",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {llvm::Type::getInt32Ty(codeGen.getContext())});

    return codeGen.getBuilder().CreateCall(createFunc, {value}, "int_obj");
}

llvm::Value* CodeGenRuntime::createDoubleObject(llvm::Value* value)
{
    /*  if (runtime)
    {
        return runtime->createDoubleObject(value);
    } */

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

// 创建整数字面量
llvm::Value* CodeGenExpr::createIntLiteral(int value)
{
    auto& builder = codeGen.getBuilder();
    auto* runtime = codeGen.getRuntimeGen();

    // 创建整数常量
    llvm::Value* intValue = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(codeGen.getContext()), value);

    // 使用运行时创建整数对象
    return runtime->createIntObject(intValue);
}

// 创建浮点数字面量
llvm::Value* CodeGenExpr::createDoubleLiteral(double value)
{
    auto& builder = codeGen.getBuilder();
    auto* runtime = codeGen.getRuntimeGen();

    // 创建浮点数常量
    llvm::Value* doubleValue = llvm::ConstantFP::get(
            llvm::Type::getDoubleTy(codeGen.getContext()), value);

    // 使用运行时创建浮点数对象
    return runtime->createDoubleObject(doubleValue);
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

    // 创建大小常量
    llvm::Value* sizeValue = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(codeGen.getContext()), size);

    // 使用运行时创建列表对象
    return runtime->createList(sizeValue, elemType->getObjectType());
}

// 创建带有初始值的列表
llvm::Value* CodeGenExpr::createListWithValues(const std::vector<llvm::Value*>& values,
                                               std::shared_ptr<PyType> elemType)
{
    auto* runtime = codeGen.getRuntimeGen();

    // 首先创建一个空列表
    llvm::Value* list = createList(values.size(), elemType);

    // 然后填充列表元素
    for (size_t i = 0; i < values.size(); i++)
    {
        // 创建索引
        llvm::Value* index = llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(codeGen.getContext()), i);

        // 设置列表元素
        setListElement(list, index, values[i], elemType);

        // 使元素的引用计数+1（因为列表现在拥有该元素的一个引用）
        runtime->incRef(values[i]);
    }

    return list;
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

    // 使用运行时设置列表元素
    runtime->setListElement(list, index, value);
}

}  // namespace llvmpy