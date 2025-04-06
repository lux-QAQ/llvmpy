#include "CodeGen/CodeGenType.h"
#include "CodeGen/CodeGenBase.h"
#include "CodeGen/CodeGenExpr.h"
#include "CodeGen/CodeGenModule.h"
#include "CodeGen/PyCodeGen.h"  // 添加这一行 - 确保完整类型定义
#include "TypeOperations.h"
#include "ObjectLifecycle.h"
#include "TypeIDs.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <iostream>
#include <unordered_set>
#include <cmath>

namespace llvmpy
{

//===----------------------------------------------------------------------===//
// 表达式类型推导
//===----------------------------------------------------------------------===//

std::shared_ptr<PyType> CodeGenType::inferExprType(const ExprAST* expr)
{
    if (!expr)
    {
        return PyType::getAny();
    }

    // 首先检查缓存
    auto it = typeCache.find(expr);
    if (it != typeCache.end())
    {
        return it->second;
    }

    // 根据表达式类型执行不同的推导
    std::shared_ptr<PyType> resultType;

    switch (expr->kind())
    {
        case ASTKind::NumberExpr:
        {
            // 检查是否是整数或浮点数
            auto numExpr = static_cast<const NumberExprAST*>(expr);
            double value = numExpr->getValue();

            if (std::floor(value) == value && value <= INT_MAX && value >= INT_MIN)
            {
                resultType = PyType::getInt();
            }
            else
            {
                resultType = PyType::getDouble();
            }
            break;
        }

        case ASTKind::StringExpr:
            resultType = PyType::getString();
            break;

        case ASTKind::BoolExpr:
            resultType = PyType::getBool();
            break;

        case ASTKind::NoneExpr:
            resultType = PyType::getAny();  // None可视为Any类型
            break;

        case ASTKind::VariableExpr:
        {
            auto varExpr = static_cast<const VariableExprAST*>(expr);
            const std::string& name = varExpr->getName();

            // 从符号表查询变量类型
            ObjectType* varType = codeGen.getSymbolTable().getVariableType(name);
            if (varType)
            {
                resultType = PyType::fromObjectType(varType);
            }
            else
            {
                resultType = PyType::getAny();  // 未知变量类型默认为Any
            }
            break;
        }

        case ASTKind::BinaryExpr:
        {
            auto binExpr = static_cast<const BinaryExprAST*>(expr);
            char op = binExpr->getOp();

            // 递归获取左右操作数类型
            std::shared_ptr<PyType> leftType = inferExprType(binExpr->getLHS());
            std::shared_ptr<PyType> rightType = inferExprType(binExpr->getRHS());

            // 使用二元操作类型推导
            resultType = inferBinaryExprType(op, leftType, rightType);
            break;
        }

        case ASTKind::UnaryExpr:
        {
            auto unaryExpr = static_cast<const UnaryExprAST*>(expr);
            char op = unaryExpr->getOpCode();

            // 递归获取操作数类型
            std::shared_ptr<PyType> operandType = inferExprType(unaryExpr->getOperand());

            // 使用一元操作类型推导
            resultType = inferUnaryExprType(op, operandType);
            break;
        }

        case ASTKind::CallExpr:
        {
            auto callExpr = static_cast<const CallExprAST*>(expr);

            // 获取参数类型
            std::vector<std::shared_ptr<PyType>> argTypes;
            for (const auto& arg : callExpr->getArgs())
            {
                argTypes.push_back(inferExprType(arg.get()));
            }

            // 使用函数调用类型推导
            resultType = inferCallExprType(callExpr->getCallee(), argTypes);
            break;
        }

        case ASTKind::ListExpr:
        {
            auto listExpr = static_cast<const ListExprAST*>(expr);

            // 推导列表元素类型
            std::shared_ptr<PyType> elemType = inferListElementType(listExpr->getElements());

            // 创建列表类型
            resultType = PyType::getList(elemType);
            break;
        }

        case ASTKind::IndexExpr:
        {
            auto indexExpr = static_cast<const IndexExprAST*>(expr);

            // 获取目标和索引类型
            std::shared_ptr<PyType> targetType = inferExprType(indexExpr->getTarget());
            std::shared_ptr<PyType> indexType = inferExprType(indexExpr->getIndex());

            // 使用索引表达式类型推导
            resultType = inferIndexExprType(targetType, indexType);
            break;
        }

        default:
            // 默认情况下使用Any类型
            resultType = PyType::getAny();
            break;
    }

    // 缓存推导结果并返回
    typeCache[expr] = resultType;
    return resultType;
}

std::shared_ptr<PyType> CodeGenType::inferBinaryExprType(
        char op,
        std::shared_ptr<PyType> leftType,
        std::shared_ptr<PyType> rightType)
{
    // 使用TypeOperations推导结果类型
    auto& registry = TypeOperationRegistry::getInstance();

    // 获取类型ID
    int leftTypeId = OperationCodeGenerator::getTypeId(leftType->getObjectType());
    int rightTypeId = OperationCodeGenerator::getTypeId(rightType->getObjectType());

    // 使用类型操作注册表获取结果类型ID
    int resultTypeId = registry.getResultTypeId(leftTypeId, rightTypeId, op);

    // 如果找不到直接对应的类型，尝试找到可操作路径
    if (resultTypeId == -1)
    {
        std::pair<int, int> opPath = registry.findOperablePath(op, leftTypeId, rightTypeId);

        // 如果找到可操作路径，重新获取结果类型
        if (opPath.first != leftTypeId || opPath.second != rightTypeId)
        {
            resultTypeId = registry.getResultTypeId(opPath.first, opPath.second, op);
        }
    }

    // 如果仍然无法确定结果类型，使用类型推导规则
    if (resultTypeId == -1)
    {
        // 对于数值类型操作，结果通常是更一般的类型
        if ((leftType->isInt() || leftType->isDouble()) && (rightType->isInt() || rightType->isDouble()))
        {
            // 如果任一操作数是浮点数，结果也是浮点数
            if (leftType->isDouble() || rightType->isDouble())
            {
                return PyType::getDouble();
            }
            else
            {
                return PyType::getInt();
            }
        }

        // 对于字符串连接
        if (op == '+' && leftType->isString() && rightType->isString())
        {
            return PyType::getString();
        }

        // 对于比较操作，结果是布尔类型
        if (op == '<' || op == '>' || op == 'l' || op == 'g' || op == 'e' || op == 'n')
        {
            return PyType::getBool();
        }

        // 默认使用左操作数类型
        return leftType;
    }

    // 从结果类型ID创建PyType
    return getTypeFromId(resultTypeId);
}

std::shared_ptr<PyType> CodeGenType::inferUnaryExprType(
        char op,
        std::shared_ptr<PyType> operandType)
{
    // 使用TypeOperations推导一元操作结果类型
    auto& registry = TypeOperationRegistry::getInstance();

    // 获取操作数类型ID
    int typeId = OperationCodeGenerator::getTypeId(operandType->getObjectType());

    // 查找一元操作描述符
    UnaryOpDescriptor* desc = registry.getUnaryOpDescriptor(op, typeId);

    if (desc)
    {
        // 如果找到描述符，使用其结果类型
        return getTypeFromId(desc->resultTypeId);
    }

    // 如果没有找到描述符，使用默认推导规则
    switch (op)
    {
        case '-':
            // 数值类型的取负保持类型不变
            if (operandType->isNumeric())
            {
                return operandType;
            }
            break;

        case '!':
            // 逻辑非的结果是布尔类型
            return PyType::getBool();

        case '~':
            // 按位取反，对于整数保持类型不变
            if (operandType->isInt())
            {
                return operandType;
            }
            break;
    }

    // 默认情况返回Any类型
    return PyType::getAny();
}

std::shared_ptr<PyType> CodeGenType::inferIndexExprType(
        std::shared_ptr<PyType> targetType,
        std::shared_ptr<PyType> indexType)
{
    // 根据容器类型确定元素类型
    if (targetType->isList())
    {
        // 列表索引返回元素类型
        return PyType::getListElementType(targetType);
    }
    else if (targetType->isDict())
    {
        // 字典索引返回值类型
        return PyType::getDictValueType(targetType);
    }
    else if (targetType->isString())
    {
        // 字符串索引返回字符（作为字符串）
        return PyType::getString();
    }

    // 默认情况无法确定索引结果类型，返回Any
    return PyType::getAny();
}

std::shared_ptr<PyType> CodeGenType::inferCallExprType(
        const std::string& funcName,
        const std::vector<std::shared_ptr<PyType>>& argTypes)
{
    // 首先尝试从符号表中获取函数信息
    auto* moduleGen = codeGen.getModuleGen();
    FunctionDefInfo* funcInfo = moduleGen->getFunctionInfo(funcName);

    if (funcInfo && funcInfo->returnType)
    {
        // 如果有函数信息，使用其返回类型
        return PyType::fromObjectType(funcInfo->returnType);
    }

    // 处理内置函数的返回类型
    if (funcName == "int")
    {
        return PyType::getInt();
    }
    else if (funcName == "float" || funcName == "double")
    {
        return PyType::getDouble();
    }
    else if (funcName == "bool")
    {
        return PyType::getBool();
    }
    else if (funcName == "str" || funcName == "string")
    {
        return PyType::getString();
    }
    else if (funcName == "list")
    {
        // 如果有参数，尝试推导列表元素类型
        if (!argTypes.empty())
        {
            return PyType::getList(argTypes[0]);
        }
        else
        {
            // 空列表，元素类型为Any
            return PyType::getList(PyType::getAny());
        }
    }

    // 通用函数类型推导 - 根据函数体分析
    auto* funcAST = getFunctionAST(funcName);
    if (funcAST)
    {
        // 如果能找到函数定义，分析函数体内的返回语句
        return analyzeFunctionReturnType(funcAST, argTypes);
    }

    // 如果找不到函数定义，尝试基于参数类型和一般规则推导
    return inferReturnTypeFromContext(funcName, argTypes);
}

// 根据函数名查找函数AST定义
FunctionAST* CodeGenType::getFunctionAST(const std::string& funcName)
{
    // 从模块或全局作用域中查找函数定义
    // 从模块获取函数定义
    auto* moduleGen = codeGen.getModuleGen();
    if (!moduleGen) return nullptr;

    ModuleAST* module = moduleGen->getCurrentModule();
    if (!module) return nullptr;

    // 遍历模块中的函数查找匹配项
    for (const auto& func : module->getFunctions())
    {
        if (func->name == funcName)
        {
            return func.get();
        }
    }

    return nullptr;
}

// 分析函数体中的返回语句，确定返回类型
std::shared_ptr<PyType> CodeGenType::analyzeFunctionReturnType(
        FunctionAST* func,
        const std::vector<std::shared_ptr<PyType>>& argTypes)
{
    // 如果函数定义了返回类型注解，使用注解类型
    if (!func->returnTypeName.empty())
    {
        return PyType::fromString(func->returnTypeName);
    }

    // 从函数中推断返回类型
    std::shared_ptr<PyType> inferredType = func->inferReturnType();
    if (inferredType)
    {
        return inferredType;
    }

    // 分析函数体中的返回语句
    // 这需要更复杂的控制流分析逻辑，这里简化处理
    for (const auto& stmt : func->body)
    {
        if (stmt->kind() == ASTKind::ReturnStmt)
        {
            auto returnStmt = static_cast<ReturnStmtAST*>(stmt.get());
            if (returnStmt->getValue())
            {
                // 假设有函数体分析能力，分析返回值表达式
                // 以下是简化版本，真实实现需要考虑更复杂的表达式和控制流
                return analyzeReturnExpr(returnStmt->getValue(), argTypes);
            }
        }
    }

    // 如果无法从返回语句确定类型，使用函数签名和参数类型推导
    return inferReturnTypeFromContext(func->name, argTypes);
}

// 分析返回表达式类型
std::shared_ptr<PyType> CodeGenType::analyzeReturnExpr(
        const ExprAST* expr,
        const std::vector<std::shared_ptr<PyType>>& argTypes)
{
    if (!expr) return PyType::getAny();

    // 针对常见模式的表达式类型推导
    switch (expr->kind())
    {
        case ASTKind::BinaryExpr:
        {
            // 处理二元运算表达式，如 a+1
            auto binExpr = static_cast<const BinaryExprAST*>(expr);
            char op = binExpr->getOp();

            // 递归分析左右操作数
            std::shared_ptr<PyType> leftType = inferExprType(binExpr->getLHS());
            std::shared_ptr<PyType> rightType = inferExprType(binExpr->getRHS());

            // 使用二元运算类型推导规则
            return inferBinaryExprType(op, leftType, rightType);
        }

        case ASTKind::IndexExpr:
        {
            // 处理索引表达式，如 a[b]
            auto indexExpr = static_cast<const IndexExprAST*>(expr);

            // 递归分析目标和索引
            std::shared_ptr<PyType> targetType = inferExprType(indexExpr->getTarget());
            std::shared_ptr<PyType> indexType = inferExprType(indexExpr->getIndex());

            // 使用索引操作类型推导规则
            return inferIndexExprType(targetType, indexType);
        }

        // 处理其他类型的表达式...
        default:
            // 对于其他表达式，直接推导类型
            return inferExprType(expr);
    }
}

// 基于函数上下文和参数类型推导返回类型
std::shared_ptr<PyType> CodeGenType::inferReturnTypeFromContext(
        const std::string& funcName,
        const std::vector<std::shared_ptr<PyType>>& argTypes)
{
    // 如果参数为空，无法推导，返回Any
    if (argTypes.empty())
    {
        return PyType::getAny();
    }

    // 1. 对于参数类型为数值的函数，通常返回相同的数值类型或更宽泛的数值类型
    if (argTypes[0]->isNumeric())
    {
        // 检查所有参数是否为数值类型
        bool allNumeric = true;
        for (const auto& type : argTypes)
        {
            if (!type->isNumeric())
            {
                allNumeric = false;
                break;
            }
        }

        if (allNumeric)
        {
            // 所有参数都是数值类型，推导最宽泛的数值类型
            bool hasDouble = false;
            for (const auto& type : argTypes)
            {
                if (type->isDouble())
                {
                    hasDouble = true;
                    break;
                }
            }

            return hasDouble ? PyType::getDouble() : PyType::getInt();
        }
    }

    // 2. 对于参数为容器类型的函数，可能返回容器的元素类型
    if (argTypes[0]->isList())
    {
        return PyType::getListElementType(argTypes[0]);
    }
    else if (argTypes[0]->isDict())
    {
        return PyType::getDictValueType(argTypes[0]);
    }
    else if (argTypes[0]->isString())
    {
        return PyType::getString();
    }

    // 如果无法从上下文推导，返回Any类型
    return PyType::getAny();
}

// 获取模块类型
std::shared_ptr<PyType> CodeGenType::getModuleType(const std::string& moduleName)
{
    // 从类型注册表中获取或创建模块类型
    auto& registry = TypeRegistry::getInstance();
    std::string typeName = "module_" + moduleName;
    ObjectType* moduleType = registry.getType(typeName);

    if (!moduleType)
    {
        // 如果不存在，创建一个新的模块类型
        moduleType = new PrimitiveType(typeName);
        moduleType->setFeature("reference", true);
        registry.registerType(typeName, moduleType);
    }

    return std::make_shared<PyType>(moduleType);
}

// 获取类类型
std::shared_ptr<PyType> CodeGenType::getClassType(const std::string& className)
{
    auto& registry = TypeRegistry::getInstance();
    std::string typeName = className + "_class";
    ObjectType* classType = registry.getType(typeName);

    if (!classType)
    {
        classType = new PrimitiveType(typeName);
        classType->setFeature("reference", true);
        registry.registerType(typeName, classType);
    }

    return std::make_shared<PyType>(classType);
}

// 获取类实例类型
std::shared_ptr<PyType> CodeGenType::getClassInstanceType(const std::string& className)
{
    auto& registry = TypeRegistry::getInstance();
    std::string typeName = className + "_instance";
    ObjectType* instanceType = registry.getType(typeName);

    if (!instanceType)
    {
        instanceType = new PrimitiveType(typeName);
        instanceType->setFeature("reference", true);
        registry.registerType(typeName, instanceType);
    }

    return std::make_shared<PyType>(instanceType);
}

std::shared_ptr<PyType> CodeGenType::inferListElementType(
        const std::vector<std::unique_ptr<ExprAST>>& elements)
{
    if (elements.empty())
    {
        // 空列表，使用Any作为元素类型
        return PyType::getAny();
    }

    // 首先推导第一个元素的类型
    std::shared_ptr<PyType> commonType = inferExprType(elements[0].get());

    // 然后与其他元素类型求共同类型
    for (size_t i = 1; i < elements.size(); i++)
    {
        std::shared_ptr<PyType> elemType = inferExprType(elements[i].get());
        commonType = getCommonType(commonType, elemType);
    }

    return commonType;
}

std::shared_ptr<PyType> CodeGenType::getCommonType(
        std::shared_ptr<PyType> typeA,
        std::shared_ptr<PyType> typeB)
{
    // 如果类型相同，直接返回该类型
    if (typeA->equals(*typeB))
    {
        return typeA;
    }

    // 如果任一类型是Any，返回另一类型
    if (typeA->isAny())
    {
        return typeB;
    }
    if (typeB->isAny())
    {
        return typeA;
    }

    // 数值类型提升
    if (typeA->isNumeric() && typeB->isNumeric())
    {
        // 任一类型是double，结果为double
        if (typeA->isDouble() || typeB->isDouble())
        {
            return PyType::getDouble();
        }
        // 否则都是整数，结果为int
        return PyType::getInt();
    }

    // 字符串类型处理
    if (typeA->isString() && typeB->isString())
    {
        return PyType::getString();
    }

    // 容器类型处理
    if (typeA->isList() && typeB->isList())
    {
        // 对于列表，获取元素类型的共同超类型
        std::shared_ptr<PyType> elemTypeA = PyType::getListElementType(typeA);
        std::shared_ptr<PyType> elemTypeB = PyType::getListElementType(typeB);
        std::shared_ptr<PyType> commonElemType = getCommonType(elemTypeA, elemTypeB);

        return PyType::getList(commonElemType);
    }

    // 如果没有明确的共同类型，返回Any
    return PyType::getAny();
}

//===----------------------------------------------------------------------===//
// 类型转换与验证
//===----------------------------------------------------------------------===//
// 实现方法
llvm::Value* CodeGenType::generateTypeConversion(
        llvm::Value* value,
        int fromTypeId,
        int toTypeId)
{
    // 安全获取PyCodeGen实例
    auto* pyCodeGen = dynamic_cast<PyCodeGen*>(&codeGen);
    if (!pyCodeGen)
    {
        // 处理错误情况
        return value;  // 无法转换，直接返回原值
    }

    // 使用OperationCodeGenerator进行转换
    return OperationCodeGenerator::handleTypeConversion(
            *pyCodeGen, value, fromTypeId, toTypeId);
}

llvm::Value* CodeGenType::generateTypeConversion(
        llvm::Value* value,
        std::shared_ptr<PyType> fromType,
        std::shared_ptr<PyType> toType)
{
    if (!value || !fromType || !toType)
    {
        return value;
    }

    // 如果类型相同，无需转换
    if (fromType->equals(*toType))
    {
        return value;
    }

    // 获取类型ID
    int fromTypeId = getTypeId(fromType);
    int toTypeId = getTypeId(toType);

    // 使用TypeOperations进行转换
    return generateTypeConversion(value, fromTypeId, toTypeId);
}

llvm::Value* CodeGenType::generateTypeCheck(
        llvm::Value* value,
        std::shared_ptr<PyType> expectedType)
{
    if (!value || !expectedType)
    {
        return nullptr;
    }

    auto& builder = codeGen.getBuilder();

    // 获取类型ID
    int expectedTypeId = getTypeId(expectedType);

    // 获取检查类型函数
    llvm::Function* checkTypeFunc = codeGen.getOrCreateExternalFunction(
            "py_check_type",
            llvm::Type::getInt1Ty(codeGen.getContext()),
            {llvm::PointerType::get(codeGen.getContext(), 0),
             llvm::Type::getInt32Ty(codeGen.getContext())});

    // 创建类型ID常量
    llvm::Value* typeIdValue = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(codeGen.getContext()), expectedTypeId);

    // 调用类型检查函数
    return builder.CreateCall(checkTypeFunc, {value, typeIdValue}, "type_check");
}

llvm::Value* CodeGenType::generateTypeError(
        llvm::Value* value,
        std::shared_ptr<PyType> expectedType)
{
    if (!value || !expectedType)
    {
        return nullptr;
    }

    // 创建类型错误
    std::string errorMsg = "Expected type " + expectedType->getObjectType()->getName() + " but got different type";

    return codeGen.logTypeError(errorMsg);
}

bool CodeGenType::validateExprType(
        ExprAST* expr,
        std::shared_ptr<PyType> expectedType)
{
    if (!expr || !expectedType)
    {
        return false;
    }

    // 推导表达式类型
    std::shared_ptr<PyType> exprType = inferExprType(expr);

    // 使用TypeSafetyChecker检查类型兼容性
    return TypeSafetyChecker::isTypeCompatible(exprType, expectedType);
}

bool CodeGenType::validateAssignment(
        const std::string& varName,
        const ExprAST* valueExpr)
{
    if (!valueExpr)
    {
        return false;
    }

    // 获取变量类型
    ObjectType* varType = codeGen.getSymbolTable().getVariableType(varName);
    if (!varType)
    {
        // 变量不存在，可以赋任何类型
        return true;
    }

    // 获取赋值表达式类型
    std::shared_ptr<PyType> valueType = inferExprType(valueExpr);
    std::shared_ptr<PyType> targetType = PyType::fromObjectType(varType);

    // 检查类型兼容性
    return TypeSafetyChecker::checkAssignmentCompatibility(targetType, valueType);
}

bool CodeGenType::validateListAssignment(
        std::shared_ptr<PyType> listElemType,
        const ExprAST* valueExpr)
{
    if (!listElemType || !valueExpr)
    {
        return false;
    }

    // 获取赋值表达式类型
    std::shared_ptr<PyType> valueType = inferExprType(valueExpr);

    // 检查类型兼容性
    return TypeSafetyChecker::isTypeCompatible(valueType, listElemType);
}

bool CodeGenType::validateIndexOperation(
        std::shared_ptr<PyType> targetType,
        std::shared_ptr<PyType> indexType)
{
    return TypeSafetyChecker::checkIndexOperation(targetType, indexType);
}

//===----------------------------------------------------------------------===//
// 类型工具方法
//===----------------------------------------------------------------------===//

llvm::Type* CodeGenType::getLLVMType(std::shared_ptr<PyType> type)
{
    if (!type)
    {
        // 默认使用指针类型
        return llvm::PointerType::get(codeGen.getContext(), 0);
    }

    // 我们在Python中主要使用PyObject指针类型
    if (type->isReference() || type->isList() || type->isDict() || type->isString() || type->isAny())
    {
        return llvm::PointerType::get(codeGen.getContext(), 0);
    }

    // 对于具体的基本类型，可以返回相应的LLVM类型
    if (type->isInt())
    {
        return llvm::Type::getInt32Ty(codeGen.getContext());
    }
    else if (type->isDouble())
    {
        return llvm::Type::getDoubleTy(codeGen.getContext());
    }
    else if (type->isBool())
    {
        return llvm::Type::getInt1Ty(codeGen.getContext());
    }
    else if (type->isVoid())
    {
        return llvm::Type::getVoidTy(codeGen.getContext());
    }

    // 默认使用指针类型
    return llvm::PointerType::get(codeGen.getContext(), 0);
}

int CodeGenType::getTypeId(std::shared_ptr<PyType> type)
{
    if (!type || !type->getObjectType())
    {
        return PY_TYPE_NONE;
    }

    return type->getObjectType()->getTypeId();
}

std::shared_ptr<PyType> CodeGenType::getTypeFromId(int typeId)
{
    switch (getBaseTypeId(typeId))
    {
        case PY_TYPE_INT:
            return PyType::getInt();

        case PY_TYPE_DOUBLE:
            return PyType::getDouble();

        case PY_TYPE_BOOL:
            return PyType::getBool();

        case PY_TYPE_STRING:
            return PyType::getString();

        case PY_TYPE_LIST:
            // 对于列表类型，需要创建列表类型
            return PyType::getList(PyType::getAny());

        case PY_TYPE_DICT:
            // 对于字典类型，需要创建字典类型
            return PyType::getDict(PyType::getAny(), PyType::getAny());

        case PY_TYPE_ANY:
            return PyType::getAny();

        case PY_TYPE_NONE:
        default:
            return PyType::getAny();
    }
}

void CodeGenType::clearTypeCache()
{
    typeCache.clear();
}

//===----------------------------------------------------------------------===//
// TypeSafetyChecker 实现
//===----------------------------------------------------------------------===//

bool TypeSafetyChecker::isTypeCompatible(
        std::shared_ptr<PyType> exprType,
        std::shared_ptr<PyType> expectedType)
{
    if (!exprType || !expectedType)
    {
        return false;
    }

    // Any类型与任何类型兼容
    if (exprType->isAny() || expectedType->isAny())
    {
        return true;
    }

    // 相同类型兼容
    if (exprType->equals(*expectedType))
    {
        return true;
    }

    // 数值类型兼容性
    if (exprType->isNumeric() && expectedType->isNumeric())
    {
        // Int可以赋值给Double
        if (exprType->isInt() && expectedType->isDouble())
        {
            return true;
        }
    }

    // 列表类型兼容性
    if (exprType->isList() && expectedType->isList())
    {
        // 获取元素类型
        std::shared_ptr<PyType> exprElemType = PyType::getListElementType(exprType);
        std::shared_ptr<PyType> expectedElemType = PyType::getListElementType(expectedType);

        // 递归检查元素类型兼容性
        return isTypeCompatible(exprElemType, expectedElemType);
    }

    // 字典类型兼容性
    if (exprType->isDict() && expectedType->isDict())
    {
        // 获取键值类型
        std::shared_ptr<PyType> exprKeyType = PyType::getDictKeyType(exprType);
        std::shared_ptr<PyType> expectedKeyType = PyType::getDictKeyType(expectedType);
        std::shared_ptr<PyType> exprValueType = PyType::getDictValueType(exprType);
        std::shared_ptr<PyType> expectedValueType = PyType::getDictValueType(expectedType);

        // 递归检查键值类型兼容性
        return isTypeCompatible(exprKeyType, expectedKeyType) && isTypeCompatible(exprValueType, expectedValueType);
    }

    // 使用TypeOperations检查更复杂的类型兼容性
    ObjectType* exprObjType = exprType->getObjectType();
    ObjectType* expectedObjType = expectedType->getObjectType();

    if (exprObjType && expectedObjType)
    {
        // 检查类型是否可以转换
        return exprObjType->canConvertTo(expectedObjType);
    }

    return false;
}

/* bool TypeSafetyChecker::checkIndexOperation(
        std::shared_ptr<PyType> targetType,
        std::shared_ptr<PyType> indexType)
{
    if (!targetType || !indexType)
    {
        return false;
    }

    // 检查目标类型是否可索引
    if (targetType->isList() || targetType->isString())
    {
        // 对于列表和字符串，索引应该是整数
        return indexType->isInt() || indexType->isBool();
    }
    else if (targetType->isDict())
    {
        // 对于字典，需要检查索引类型是否匹配键类型
        std::shared_ptr<PyType> keyType = PyType::getDictKeyType(targetType);
        return isTypeCompatible(indexType, keyType);
    }

    // 其他类型不支持索引操作
    return false;
} */

bool TypeSafetyChecker::checkIndexOperation(
        std::shared_ptr<PyType> targetType,
        std::shared_ptr<PyType> indexType)
{
    if (!targetType || !indexType)
    {
        return false;
    }

    // 处理any类型 - 允许any类型作为容器或索引
    if (targetType->isAny() || indexType->isAny())
    {
        return true;  // 放宽限制，假设运行时会处理类型检查
    }

    // 检查目标类型是否可索引
    if (targetType->isList() || targetType->isString())
    {
        // 对于列表和字符串，索引应该是整数
        return indexType->isInt() || indexType->isBool();
    }
    else if (targetType->isDict())
    {
        // 对于字典，需要检查索引类型是否匹配键类型
        std::shared_ptr<PyType> keyType = PyType::getDictKeyType(targetType);
        return isTypeCompatible(indexType, keyType);
    }

    // 其他类型不支持索引操作
    return false;
}

bool TypeSafetyChecker::checkAssignmentCompatibility(
        std::shared_ptr<PyType> targetType,
        std::shared_ptr<PyType> valueType)
{
    // 直接使用类型兼容性检查
    return isTypeCompatible(valueType, targetType);
}

bool TypeSafetyChecker::areTypesRelated(
        ObjectType* typeA,
        ObjectType* typeB)
{
    if (!typeA || !typeB)
    {
        return false;
    }

    // 检查类型是否相同
    if (typeA == typeB)
    {
        return true;
    }

    // 检查类型是否可以相互转换
    return typeA->canConvertTo(typeB) || typeB->canConvertTo(typeA);
}

bool TypeSafetyChecker::areTypesCompatible(int typeIdA, int typeIdB)
{
    // 使用TypeOperations中的兼容性检查
    auto& registry = TypeOperationRegistry::getInstance();
    return registry.areTypesCompatible(typeIdA, typeIdB);
}

}  // namespace llvmpy