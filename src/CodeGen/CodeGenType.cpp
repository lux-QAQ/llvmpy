#include "CodeGen/CodeGenType.h"
#include "CodeGen/CodeGenBase.h"
#include "CodeGen/CodeGenExpr.h"
#include "CodeGen/CodeGenModule.h"
#include "CodeGen/PyCodeGen.h"  // 添加这一行 - 确保完整类型定义
#include "TypeOperations.h"
#include "ObjectLifecycle.h"
#include "TypeIDs.h"
#include "parser.h"
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
            // --- MODIFIED: Get type directly from NumberExprAST ---
            auto numExpr = static_cast<const NumberExprAST*>(expr);
            // The type (int or double) was determined during parsing and stored.
            resultType = numExpr->getType();
            // Ensure a valid type is returned, fallback to Any if somehow null
            if (!resultType)
            {
                std::cerr << "Warning: NumberExprAST returned null type in inferExprType. Defaulting to Any." << std::endl;
                resultType = PyType::getAny();
            }
            // --- End Modification ---
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
            PyTokenType op = binExpr->getOpType();

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
            PyTokenType op = unaryExpr->getOpType();

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
        case ASTKind::DictExpr:
        {
            auto dictExpr = static_cast<const DictExprAST*>(expr);
            const auto& pairs = dictExpr->getPairs();  // Use getPairs()

            if (pairs.empty())
            {
                // Empty dict: dict[any, any]
                resultType = PyType::getDict(PyType::getAny(), PyType::getAny());
            }
            else
            {
                // Infer common key and value types
                std::shared_ptr<PyType> commonKeyType = inferExprType(pairs[0].first.get());
                std::shared_ptr<PyType> commonValueType = inferExprType(pairs[0].second.get());

                for (size_t i = 1; i < pairs.size(); ++i)
                {
                    std::shared_ptr<PyType> keyType = inferExprType(pairs[i].first.get());
                    std::shared_ptr<PyType> valueType = inferExprType(pairs[i].second.get());
                    commonKeyType = getCommonType(commonKeyType, keyType);
                    commonValueType = getCommonType(commonValueType, valueType);
                }
                resultType = PyType::getDict(commonKeyType, commonValueType);
            }
            break;  // Add break statement
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
        PyTokenType op,  // Changed: char -> PyTokenType
        std::shared_ptr<PyType> leftType,
        std::shared_ptr<PyType> rightType)
{
    // 使用TypeOperations推导结果类型
    auto& registry = TypeOperationRegistry::getInstance();

    // 获取类型ID
    int leftTypeId = OperationCodeGenerator::getTypeId(leftType->getObjectType());
    int rightTypeId = OperationCodeGenerator::getTypeId(rightType->getObjectType());

    // 使用类型操作注册表获取结果类型ID (现在传递 PyTokenType)
    int resultTypeId = registry.getResultTypeId(leftTypeId, rightTypeId, op);

    // 如果找不到直接对应的类型，尝试找到可操作路径 (现在传递 PyTokenType)
    if (resultTypeId == -1)
    {
        std::pair<int, int> opPath = registry.findOperablePath(op, leftTypeId, rightTypeId);

        // 如果找到可操作路径，重新获取结果类型 (现在传递 PyTokenType)
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

        // 对于字符串连接 (使用 PyTokenType)
        if (op == TOK_PLUS && leftType->isString() && rightType->isString())
        {
            return PyType::getString();
        }

        // 对于比较操作，结果是布尔类型 (使用 PyTokenType)
        if (op == TOK_LT || op == TOK_GT || op == TOK_LE || op == TOK_GE || op == TOK_EQ || op == TOK_NEQ || op == TOK_IS || op == TOK_IS_NOT || op == TOK_IN || op == TOK_NOT_IN)
        {
            return PyType::getBool();
        }

        // 默认使用左操作数类型 (或者返回 Any 更安全)
        return PyType::getAny();  // Changed: return leftType -> return PyType::getAny()
    }

    // 从结果类型ID创建PyType
    return getTypeFromId(resultTypeId);
}

std::shared_ptr<PyType> CodeGenType::inferUnaryExprType(
        PyTokenType op,  // Changed: char -> PyTokenType
        std::shared_ptr<PyType> operandType)
{
    // 使用TypeOperations推导一元操作结果类型
    auto& registry = TypeOperationRegistry::getInstance();

    // 获取操作数类型ID
    int typeId = OperationCodeGenerator::getTypeId(operandType->getObjectType());

    // 查找一元操作描述符 (现在传递 PyTokenType)
    UnaryOpDescriptor* desc = registry.getUnaryOpDescriptor(op, typeId);

    if (desc)
    {
        // 如果找到描述符，使用其结果类型
        return getTypeFromId(desc->resultTypeId);
    }

    // 如果没有找到描述符，使用默认推导规则 (使用 PyTokenType)
    switch (op)
    {
        case TOK_MINUS:  // Changed: '-' -> TOK_MINUS
            // 数值类型的取负保持类型不变
            if (operandType->isNumeric())
            {
                return operandType;
            }
            // 如果不是数值类型，则 fallthrough 到 default
            break;

        case TOK_NOT:  // Changed: '!' -> TOK_NOT
            // 逻辑非的结果是布尔类型
            return PyType::getBool();

        // case TOK_TILDE: // Changed: '~' -> TOK_TILDE (If you add bitwise not)
        //     // 按位取反，对于整数保持类型不变
        //     if (operandType->isInt())
        //     {
        //         return operandType;
        //     }
        //     break;

        // --- 添加 default 分支处理其他未明确处理的操作符 ---
        default:
            // 对于其他未知或不支持的一元操作，默认返回 Any
            // 也可以在这里添加日志记录
            std::cerr << "Warning: Unhandled unary operator token " << op << " in inferUnaryExprType. Returning Any." << std::endl;
            break;  // Fallthrough 到函数末尾的 return PyType::getAny()
                    // --- 结束添加 ---
    }

    // 默认情况返回Any类型 (包括从 TOK_MINUS 的非数值情况 fallthrough)
    return PyType::getAny();
}

ObjectType* CodeGenType::getFunctionObjectType(const FunctionAST* funcAST)
{
    if (!funcAST)
    {
        codeGen.logError("Cannot get ObjectType for null FunctionAST");
        return nullptr;
    }

    // --- 1. Resolve Return Type (Default to Any if no hint) ---
    ObjectType* returnObjectType = nullptr;
    // 使用存储在 FunctionAST 中的返回类型名称字符串
    const std::string& returnTypeName = funcAST->getReturnTypeName();  // 使用 string getter

    if (!returnTypeName.empty())
    {  // 检查字符串是否为空
        // 使用 PyTypeParser 从字符串解析类型
        std::shared_ptr<PyType> returnPyType = PyTypeParser::parseType(returnTypeName);
        if (returnPyType)
        {
            returnObjectType = returnPyType->getObjectType();
        }
        else
        {
            codeGen.logWarning("Failed to parse return type name '" + returnTypeName + "' for function: " + funcAST->getName() + ". Defaulting to any.",
                               funcAST->line.value_or(0), funcAST->column.value_or(0));
        }
    }

    // Default to 'object' if no hint or parsing failed
    if (!returnObjectType)
    {
        returnObjectType = TypeRegistry::getInstance().getType("object");
        if (!returnObjectType)
        {
            codeGen.logError("Default type 'object' not found in TypeRegistry!");
            return nullptr;
        }
    }

    // --- 2. Resolve Parameter Types (Default to Any if no hint) ---
    std::vector<ObjectType*> paramObjectTypes;
    for (const auto& param : funcAST->getParams())
    {
        ObjectType* paramObjectType = nullptr;
        // 使用存储在 ParamAST 中的参数类型名称字符串
        const std::string& paramTypeName = param.typeName;  // 直接访问 string 成员

        if (!paramTypeName.empty())
        {  // 检查字符串是否为空
            // 使用 PyTypeParser 从字符串解析类型
            std::shared_ptr<PyType> paramPyType = PyTypeParser::parseType(paramTypeName);
            if (paramPyType)
            {
                paramObjectType = paramPyType->getObjectType();
            }
            else
            {
                codeGen.logWarning("Failed to parse type name '" + paramTypeName + "' for parameter '" + param.name
                                           + "' in function: " + funcAST->getName() + ". Defaulting to any.",
                                   funcAST->line.value_or(0), funcAST->column.value_or(0));
            }
        }

        // Default to 'object' if no hint or parsing failed
        if (!paramObjectType)
        {
            paramObjectType = TypeRegistry::getInstance().getType("object");
            if (!paramObjectType)
            {
                codeGen.logError("Default type 'object' not found in TypeRegistry!");
                return nullptr;
            }
        }
        paramObjectTypes.push_back(paramObjectType);
    }

    // --- 3. Get/Create FunctionType from Registry ---
    ObjectType* functionType = TypeRegistry::getInstance().getFunctionType(returnObjectType, paramObjectTypes);

    if (!functionType)
    {
        codeGen.logError("Failed to get or create FunctionType in TypeRegistry for function: " + funcAST->getName());
        return nullptr;
    }

#ifdef DEBUG_CODEGEN_TYPE
    std::cerr << "Debug [CodeGenType]: Resolved static FunctionType for '" << funcAST->getName()
              << "' as " << functionType->getName() << std::endl;
#endif

    return functionType;  // This will be a FunctionType*
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

std::shared_ptr<PyType> CodeGenType::inferCallReturnType(
        std::shared_ptr<PyType> callableType,
        const std::vector<std::shared_ptr<PyType>>& argTypes)
{
    // TODO: Implement more sophisticated type inference based on callableType signature
    // For now, if the callable is a function (or Any), assume it can return Any.
    if (callableType && (callableType->isFunction() || callableType->isAny()))
    {
        // In the future, if callableType holds FunctionType info, return its return type.
        // e.g., if (auto* funcType = dynamic_cast<FunctionObjectType*>(callableType->getObjectType())) {
        //          return PyType::fromObjectType(funcType->getReturnType());
        //      }
        return PyType::getAny();
    }

    // If the callable type is not a function or Any, it's an error handled elsewhere.
    // Return Any as a fallback, though ideally an error type might be better.
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
    const FunctionAST* funcAST = getFunctionAST(funcName);
    if (funcAST)
    {
        // 如果能找到函数定义，分析函数体内的返回语句
        return analyzeFunctionReturnType(funcAST, argTypes);
    }

    // 如果找不到函数定义，尝试基于参数类型和一般规则推导
    return inferReturnTypeFromContext(funcName, argTypes);
}

// 根据函数名查找函数AST定义
const FunctionAST* CodeGenType::getFunctionAST(const std::string& funcName)  // <--- 改为 const FunctionAST*
{
    // --- 1. 优先从符号表查找当前及父作用域 ---
    const FunctionAST* funcFromScope = codeGen.getSymbolTable().findFunctionAST(funcName);
    if (funcFromScope)
    {
#ifdef DEBUG_CODEGEN_TYPE
        std::cerr << "Debug [CodeGenType]: Found FunctionAST for '" << funcName << "' in symbol table scope." << std::endl;
#endif
        return funcFromScope;
    }

// --- 2. 如果作用域中未找到，再查找顶层模块 ---
#ifdef DEBUG_CODEGEN_TYPE
    std::cerr << "Debug [CodeGenType]: FunctionAST for '" << funcName << "' not found in scope, searching module top-level..." << std::endl;
#endif

    auto* moduleGen = codeGen.getModuleGen();
    if (!moduleGen)
    {
        codeGen.logError("Module generator is not available in CodeGenType::getFunctionAST");
        return nullptr;
    }

    ModuleAST* module = moduleGen->getCurrentModule();
    if (!module)
    {
        // 在非模块顶层查找时，如果符号表里没有，这里找不到是正常的
        // codeGen.logWarning("Current module is not set in CodeGenType::getFunctionAST when searching top-level for: " + funcName);
        return nullptr;
    }

    // 遍历模块的顶层语句列表
    for (const auto& stmt : module->getStatements())
    {
        if (stmt->kind() == ASTKind::FunctionDefStmt)
        {
            // 强制转换为 FunctionDefStmtAST
            auto* funcDefStmt = static_cast<const FunctionDefStmtAST*>(stmt.get());  // <--- 使用 const*
            // 获取底层的 FunctionAST (现在是 const*)
            const FunctionAST* funcAST = funcDefStmt->getFunctionAST();  // <--- 改为 const FunctionAST*

            if (funcAST && funcAST->getName() == funcName)
            {
#ifdef DEBUG_CODEGEN_TYPE
                std::cerr << "Debug [CodeGenType]: Found FunctionAST for '" << funcName << "' in module top-level." << std::endl;
#endif
                return funcAST;  // <--- 直接返回 const*
            }
        }
        // TODO: 查找类中的方法
    }

#ifdef DEBUG_CODEGEN_TYPE
    std::cerr << "Debug [CodeGenType]: FunctionAST for '" << funcName << "' not found anywhere." << std::endl;
#endif
    // 在模块顶层也未找到
    return nullptr;
}

// 分析函数体中的返回语句，确定返回类型
std::shared_ptr<PyType> CodeGenType::analyzeFunctionReturnType(
        const FunctionAST* func,  // <--- 改为 const FunctionAST*
        const std::vector<std::shared_ptr<PyType>>& argTypes)
{
    // 如果函数定义了返回类型注解，使用注解类型
    if (!func->returnTypeName.empty())
    {
        // PyType::fromString 应该接受 const string&
        return PyType::fromString(func->getReturnTypeName());  // 使用 getter
    }

    // 从函数中推断返回类型 (inferReturnType 应该是 const 方法)
    std::shared_ptr<PyType> inferredType = func->inferReturnType();
    if (inferredType)
    {
        return inferredType;
    }

    // 分析函数体中的返回语句
    for (const auto& stmt : func->getBody())  // 使用 getter
    {
        if (stmt->kind() == ASTKind::ReturnStmt)
        {
            // static_cast 可以用于 const 指针
            auto returnStmt = static_cast<const ReturnStmtAST*>(stmt.get());  // <--- 使用 const*
            if (returnStmt->getValue())
            {
                // analyzeReturnExpr 应该接受 const ExprAST*
                return analyzeReturnExpr(returnStmt->getValue(), argTypes);
            }
            else
            {
                // 如果是 'return' 或 'return None'
                return PyType::getAny();  // 或者 PyType::getNone()，取决于你的语义
            }
        }
    }

    // 如果无法从返回语句确定类型，使用函数签名和参数类型推导
    return inferReturnTypeFromContext(func->getName(), argTypes);  // 使用 getter
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
            PyTokenType op = binExpr->getOpType();  // Changed: getOp() -> getOpType()

            // 递归分析左右操作数
            std::shared_ptr<PyType> leftType = inferExprType(binExpr->getLHS());
            std::shared_ptr<PyType> rightType = inferExprType(binExpr->getRHS());

            // 使用二元运算类型推导规则
            return inferBinaryExprType(op, leftType, rightType);
        }
        case ASTKind::DictExpr:
        {
            auto dictExpr = static_cast<const DictExprAST*>(expr);
            // --- FIX: Use getPairs() instead of getElements() ---
            const auto& pairs = dictExpr->getPairs();
            std::shared_ptr<PyType> resultType;  // Declare resultType here

            if (pairs.empty())
            {
                // Empty dict: dict[any, any]
                resultType = PyType::getDict(PyType::getAny(), PyType::getAny());
            }
            else
            {
                // Infer common key and value types using pairs
                std::shared_ptr<PyType> commonKeyType = inferExprType(pairs[0].first.get());
                std::shared_ptr<PyType> commonValueType = inferExprType(pairs[0].second.get());

                for (size_t i = 1; i < pairs.size(); ++i)
                {
                    std::shared_ptr<PyType> keyType = inferExprType(pairs[i].first.get());
                    std::shared_ptr<PyType> valueType = inferExprType(pairs[i].second.get());
                    commonKeyType = getCommonType(commonKeyType, keyType);
                    commonValueType = getCommonType(commonValueType, valueType);
                }
                resultType = PyType::getDict(commonKeyType, commonValueType);
            }
            // --- FIX: Add return statement ---
            return resultType;
            // break; // break is now unreachable after return
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
    return PyType::getAny();
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

    /// !!!这里更好的办法可能是直接返回Any，因为后续可能有更多复杂的类型，他们是否允许被加入或者处理，应该留在rt
    // 然后与其他元素类型求共同类型
/*     for (size_t i = 1; i < elements.size(); i++)
    {
        std::shared_ptr<PyType> elemType = inferExprType(elements[i].get());
#ifdef DEBUG_CODEGEN_inferListElementType
        std::cerr << "DEBUG [inferListElementType]: Comparing commonType=" << commonType->toString()
                  << " with element type=" << elemType->toString() << std::endl;
#endif
        commonType = getCommonType(commonType, elemType);
#ifdef DEBUG_CODEGEN_inferListElementType
        std::cerr << "DEBUG [inferListElementType]: Updated commonType=" << commonType->toString() << std::endl;
#endif
    } */

    return  PyType::getAny(); // 这里显然还是感觉有问题，因为没有正确推断而是把这些全留给了RT
}
std::shared_ptr<PyType> CodeGenType::getCommonType(
        std::shared_ptr<PyType> typeA,
        std::shared_ptr<PyType> typeB)
{
#ifdef DEBUG_CODEGEN_getCommonType
    // --- DEBUGGING START ---
    std::cerr << "DEBUG [getCommonType]: Comparing A=" << typeA->toString()
              << " (ID: " << getTypeId(typeA) << ")"
              << " and B=" << typeB->toString()
              << " (ID: " << getTypeId(typeB) << ")" << std::endl;
// --- DEBUGGING END ---
#endif

    // --- Handle Any type comparison FIRST ---
    if (typeA->isAny())
    {
#ifdef DEBUG_CODEGEN_getCommonType
        std::cerr << "DEBUG [getCommonType]: A is Any, returning B=" << typeB->toString() << std::endl;
#endif
        return typeB;
    }
    if (typeB->isAny())
    {
#ifdef DEBUG_CODEGEN_getCommonType
        std::cerr << "DEBUG [getCommonType]: B is Any, returning A=" << typeA->toString() << std::endl;
#endif
        return typeA;
    }

    // --- Handle equal types ---
    if (typeA->equals(*typeB))
    {
#ifdef DEBUG_CODEGEN_getCommonType
        std::cerr << "DEBUG [getCommonType]: Types are equal, returning A=" << typeA->toString() << std::endl;
#endif
        return typeA;
    }

    // --- Handle numeric promotion ---
    if (typeA->isNumeric() && typeB->isNumeric())
    {
        if (typeA->isDouble() || typeB->isDouble())
        {
#ifdef DEBUG_CODEGEN_getCommonType
            std::cerr << "DEBUG [getCommonType]: Numeric promotion to double" << std::endl;
#endif
            return PyType::getDouble();
        }
        else
        {
#ifdef DEBUG_CODEGEN_getCommonType
            std::cerr << "DEBUG [getCommonType]: Numeric promotion to int" << std::endl;
#endif
            return PyType::getInt();
        }
    }

    // --- Handle string concatenation ---
    if (typeA->isString() && typeB->isString())
    {
#ifdef DEBUG_CODEGEN_getCommonType
        std::cerr << "DEBUG [getCommonType]: String + String -> String" << std::endl;
#endif
        return PyType::getString();
    }

    // --- Handle list + list ---
    if (typeA->isList() && typeB->isList())
    {
        std::shared_ptr<PyType> elemTypeA = PyType::getListElementType(typeA);
        std::shared_ptr<PyType> elemTypeB = PyType::getListElementType(typeB);
#ifdef DEBUG_CODEGEN_getCommonType
        std::cerr << "DEBUG [getCommonType]: List + List. Finding common element type for "
                  << elemTypeA->toString() << " and " << elemTypeB->toString() << std::endl;
#endif
        std::shared_ptr<PyType> commonElemType = getCommonType(elemTypeA, elemTypeB);  // Recursive call
#ifdef DEBUG_CODEGEN_getCommonType
        std::cerr << "DEBUG [getCommonType]: Common element type is " << commonElemType->toString()
                  << ". Returning list[" << commonElemType->toString() << "]" << std::endl;
#endif
        return PyType::getList(commonElemType);
    }

    // --- ADDED: Handle dict + dict ---
    if (typeA->isDict() && typeB->isDict())
    {
        std::shared_ptr<PyType> keyTypeA = PyType::getDictKeyType(typeA);
        std::shared_ptr<PyType> valueTypeA = PyType::getDictValueType(typeA);
        std::shared_ptr<PyType> keyTypeB = PyType::getDictKeyType(typeB);
        std::shared_ptr<PyType> valueTypeB = PyType::getDictValueType(typeB);
#ifdef DEBUG_CODEGEN_getCommonType
        std::cerr << "DEBUG [getCommonType]: Dict + Dict. Finding common types for keys ("
                  << keyTypeA->toString() << ", " << keyTypeB->toString() << ") and values ("
                  << valueTypeA->toString() << ", " << valueTypeB->toString() << ")" << std::endl;
#endif
        std::shared_ptr<PyType> commonKeyType = getCommonType(keyTypeA, keyTypeB);        // Recursive call
        std::shared_ptr<PyType> commonValueType = getCommonType(valueTypeA, valueTypeB);  // Recursive call
#ifdef DEBUG_CODEGEN_getCommonType
        std::cerr << "DEBUG [getCommonType]: Common key type is " << commonKeyType->toString()
                  << ", common value type is " << commonValueType->toString()
                  << ". Returning dict[" << commonKeyType->toString() << ", " << commonValueType->toString() << "]" << std::endl;
#endif
        return PyType::getDict(commonKeyType, commonValueType);
    }
// --- END ADDED ---

// --- Fallback for incompatible types (e.g., list + int, dict + string, list + dict) ---
#ifdef DEBUG_CODEGEN_getCommonType
    std::cerr << "DEBUG [getCommonType]: Incompatible types (" << typeA->toString()
              << ", " << typeB->toString() << "), returning Any" << std::endl;
#endif
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