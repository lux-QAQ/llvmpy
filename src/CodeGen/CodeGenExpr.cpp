
#include <cmath>

#include "CodeGen/CodeGenExpr.h"
#include "CodeGen/CodeGenBase.h"
#include "CodeGen/CodeGenType.h"
#include "CodeGen/CodeGenRuntime.h"

#include "ObjectRuntime.h"
#include "TypeOperations.h"
#include "ObjectLifecycle.h"
#include "ast.h"
#include <llvm/IR/Constants.h>

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
    // 在符号表中查找变量
    const std::string& name = expr->getName();
    llvm::Value* value = codeGen.getSymbolTable().getVariable(name);

    if (!value)
    {
        return codeGen.logError("Unknown variable '" + name + "'",
                                expr->line ? *expr->line : 0,
                                expr->column ? *expr->column : 0);
    }

    return value;
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

    // 使用类型推导的二元操作
    return handleBinOp(expr->getOp(), L, R,
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
    return handleUnaryOp(expr->getOpCode(), operand,
                         expr->getOperand()->getType());
}

// 处理函数调用表达式
llvm::Value* CodeGenExpr::handleCallExpr(CallExprAST* expr)
{
    auto& builder = codeGen.getBuilder();
    auto* runtime = codeGen.getRuntimeGen();
    auto* typeGen = codeGen.getTypeGen();

    const std::string& callee = expr->getCallee();

    // 生成参数的代码，同时收集参数类型
    std::vector<llvm::Value*> args;
    std::vector<std::shared_ptr<PyType>> argTypes;

    for (const auto& arg : expr->getArgs())
    {
        llvm::Value* argValue = handleExpr(arg.get());
        if (!argValue) return nullptr;

        // 收集参数的值和类型
        args.push_back(argValue);
        argTypes.push_back(arg->getType());
    }

    // 获取函数声明
    llvm::Function* func = codeGen.getModule()->getFunction(callee);
    if (!func)
    {
        return codeGen.logError("Unknown function: " + callee,
                                expr->line ? *expr->line : 0,
                                expr->column ? *expr->column : 0);
    }

    // 执行函数调用前的参数准备
    std::vector<llvm::Value*> preparedArgs;
    for (size_t i = 0; i < args.size(); i++)
    {
        llvm::Value* preparedArg = runtime->prepareArgument(
                args[i], argTypes[i], nullptr);

        if (!preparedArg)
        {
            return codeGen.logError("Failed to prepare argument " + std::to_string(i + 1) + " for function " + callee);
        }

        preparedArgs.push_back(preparedArg);
    }

    // 调用函数
    llvm::Value* callResult = builder.CreateCall(func, preparedArgs, "calltmp");

    // 标记返回值为函数返回，便于生命周期管理
    runtime->markObjectSource(callResult, ObjectLifecycleManager::ObjectSource::FUNCTION_RETURN);

    // 推导返回类型
    std::shared_ptr<PyType> returnType = typeGen->inferCallExprType(callee, argTypes);
    expr->setType(returnType);

    return callResult;
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
llvm::Value* CodeGenExpr::handleBinOp(char op, llvm::Value* L, llvm::Value* R,
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
        return codeGen.logError("Unsupported binary operation: " + std::string(1, op) + " between " + leftType->getObjectType()->getName() + " and " + rightType->getObjectType()->getName());
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
llvm::Value* CodeGenExpr::handleUnaryOp(char op, llvm::Value* operand,
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
        return codeGen.logError("Unsupported unary operation: " + std::string(1, op) + " on " + operandType->getObjectType()->getName());
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
        // 确保索引是整数
        if (!indexType->isInt() && !indexType->isBool())
        {
            // 尝试转换
            auto& registry = TypeOperationRegistry::getInstance();
            TypeConversionDescriptor* convDesc =
                    registry.getTypeConversionDescriptor(indexTypeId, PY_TYPE_INT);

            if (convDesc)
            {
                index = OperationCodeGenerator::handleTypeConversion(
                        codeGen, index, indexTypeId, PY_TYPE_INT);
                indexTypeId = PY_TYPE_INT;
            }
            else
            {
                return codeGen.logError("List indices must be integers");
            }
        }

        // 调用运行时函数获取列表元素
        llvm::Function* getItemFunc = codeGen.getOrCreateExternalFunction(
                "py_list_get_item",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::Type::getInt32Ty(codeGen.getContext())});

        // 我们需要确保索引是32位整数
        if (index->getType() != llvm::Type::getInt32Ty(codeGen.getContext()))
        {
            index = builder.CreateIntCast(index, llvm::Type::getInt32Ty(codeGen.getContext()), true, "idxcast");
        }

        return builder.CreateCall(getItemFunc, {target, index}, "list_item");
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
        // 确保索引是整数
        if (!indexType->isInt() && !indexType->isBool())
        {
            // 尝试转换
            auto& registry = TypeOperationRegistry::getInstance();
            TypeConversionDescriptor* convDesc =
                    registry.getTypeConversionDescriptor(indexTypeId, PY_TYPE_INT);

            if (convDesc)
            {
                index = OperationCodeGenerator::handleTypeConversion(
                        codeGen, index, indexTypeId, PY_TYPE_INT);
                indexTypeId = PY_TYPE_INT;
            }
            else
            {
                return codeGen.logError("String indices must be integers");
            }
        }

        // 调用运行时函数获取字符串字符（作为新字符串）
        llvm::Function* getCharFunc = codeGen.getOrCreateExternalFunction(
                "py_string_get_char",
                llvm::PointerType::get(codeGen.getContext(), 0),
                {llvm::PointerType::get(codeGen.getContext(), 0),
                 llvm::Type::getInt32Ty(codeGen.getContext())});

        // 我们需要确保索引是32位整数
        if (index->getType() != llvm::Type::getInt32Ty(codeGen.getContext()))
        {
            index = builder.CreateIntCast(index, llvm::Type::getInt32Ty(codeGen.getContext()), true, "idxcast");
        }

        return builder.CreateCall(getCharFunc, {target, index}, "str_char");
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
    if (runtime)
    {
        return runtime->createIntObject(value);
    }

    // 如果没有runtime，使用内置实现
    llvm::Function* createFunc = getRuntimeFunction(
            "py_create_int",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {llvm::Type::getInt32Ty(codeGen.getContext())});

    return codeGen.getBuilder().CreateCall(createFunc, {value}, "int_obj");
}

llvm::Value* CodeGenRuntime::createDoubleObject(llvm::Value* value)
{
    if (runtime)
    {
        return runtime->createDoubleObject(value);
    }

    llvm::Function* createFunc = getRuntimeFunction(
            "py_create_double",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {llvm::Type::getDoubleTy(codeGen.getContext())});

    return codeGen.getBuilder().CreateCall(createFunc, {value}, "double_obj");
}

llvm::Value* CodeGenRuntime::createBoolObject(llvm::Value* value)
{
    if (runtime)
    {
        return runtime->createBoolObject(value);
    }

    llvm::Function* createFunc = getRuntimeFunction(
            "py_create_bool",
            llvm::PointerType::get(codeGen.getContext(), 0),
            {llvm::Type::getInt1Ty(codeGen.getContext())});

    return codeGen.getBuilder().CreateCall(createFunc, {value}, "bool_obj");
}

llvm::Value* CodeGenRuntime::createStringObject(llvm::Value* value)
{
    if (runtime)
    {
        return runtime->createStringObject(value);
    }

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