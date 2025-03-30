#include "TypeOperations.h"
#include "ObjectLifecycle.h"
#include "codegen.h"
#include "TypeIDs.h"
#include "lexer.h"

#include <iostream>
#include <sstream>
#include <unordered_set>

namespace llvmpy
{

//===----------------------------------------------------------------------===//
// 类型操作注册表实现
//===----------------------------------------------------------------------===//

// 单例模式
TypeOperationRegistry& TypeOperationRegistry::getInstance()
{
    static TypeOperationRegistry instance;
    return instance;
}

TypeOperationRegistry::TypeOperationRegistry()
{
    // 在构造函数中初始化所有内置操作
    initializeBuiltinOperations();
}

void TypeOperationRegistry::initializeBuiltinOperations()
{
    // 注册整数操作
    registerBinaryOp('+', PY_TYPE_INT, PY_TYPE_INT, PY_TYPE_INT, "py_object_add", true);
    registerBinaryOp('-', PY_TYPE_INT, PY_TYPE_INT, PY_TYPE_INT, "py_object_subtract", true);
    registerBinaryOp('*', PY_TYPE_INT, PY_TYPE_INT, PY_TYPE_INT, "py_object_multiply", true);
    registerBinaryOp('/', PY_TYPE_INT, PY_TYPE_INT, PY_TYPE_DOUBLE, "py_object_divide", true);
    registerBinaryOp('%', PY_TYPE_INT, PY_TYPE_INT, PY_TYPE_INT, "py_object_modulo", true);

    // 注册浮点数操作
    registerBinaryOp('+', PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_add", true);
    registerBinaryOp('-', PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_subtract", true);
    registerBinaryOp('*', PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_multiply", true);
    registerBinaryOp('/', PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_divide", true);

    // 注册混合数值类型操作 (整数和浮点数)
    registerBinaryOp('+', PY_TYPE_INT, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_add", true);
    registerBinaryOp('+', PY_TYPE_DOUBLE, PY_TYPE_INT, PY_TYPE_DOUBLE, "py_object_add", true);
    registerBinaryOp('-', PY_TYPE_INT, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_subtract", true);
    registerBinaryOp('-', PY_TYPE_DOUBLE, PY_TYPE_INT, PY_TYPE_DOUBLE, "py_object_subtract", true);
    registerBinaryOp('*', PY_TYPE_INT, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_multiply", true);
    registerBinaryOp('*', PY_TYPE_DOUBLE, PY_TYPE_INT, PY_TYPE_DOUBLE, "py_object_multiply", true);
    registerBinaryOp('/', PY_TYPE_INT, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_divide", true);
    registerBinaryOp('/', PY_TYPE_DOUBLE, PY_TYPE_INT, PY_TYPE_DOUBLE, "py_object_divide", true);

    // 注册字符串操作
    registerBinaryOp('+', PY_TYPE_STRING, PY_TYPE_STRING, PY_TYPE_STRING, "py_object_add", true);
    registerBinaryOp('*', PY_TYPE_STRING, PY_TYPE_INT, PY_TYPE_STRING, "py_object_multiply", true);
    registerBinaryOp('*', PY_TYPE_INT, PY_TYPE_STRING, PY_TYPE_STRING, "py_object_multiply", true);

    // 注册列表操作
    registerBinaryOp('+', PY_TYPE_LIST, PY_TYPE_LIST, PY_TYPE_LIST, "py_object_add", true);
    registerBinaryOp('*', PY_TYPE_LIST, PY_TYPE_INT, PY_TYPE_LIST, "py_object_multiply", true);
    registerBinaryOp('*', PY_TYPE_INT, PY_TYPE_LIST, PY_TYPE_LIST, "py_object_multiply", true);

    // 注册类型转换
    registerTypeConversion(PY_TYPE_INT, PY_TYPE_DOUBLE, "py_convert_int_to_double", 1);
    registerTypeConversion(PY_TYPE_DOUBLE, PY_TYPE_INT, "py_convert_double_to_int", 2);
    registerTypeConversion(PY_TYPE_INT, PY_TYPE_BOOL, "py_convert_to_bool", 1);
    registerTypeConversion(PY_TYPE_DOUBLE, PY_TYPE_BOOL, "py_convert_to_bool", 1);
    registerTypeConversion(PY_TYPE_STRING, PY_TYPE_BOOL, "py_convert_to_bool", 2);

    // 注册类型兼容性
    registerTypeCompatibility(PY_TYPE_INT, PY_TYPE_DOUBLE, true);
    registerTypeCompatibility(PY_TYPE_INT, PY_TYPE_BOOL, true);
    registerTypeCompatibility(PY_TYPE_DOUBLE, PY_TYPE_BOOL, true);
    registerTypeCompatibility(PY_TYPE_STRING, PY_TYPE_BOOL, true);

    // 注册类型提升规则 (操作数类型混合时的结果类型)
    registerTypePromotion(PY_TYPE_INT, PY_TYPE_DOUBLE, '+', PY_TYPE_DOUBLE);
    registerTypePromotion(PY_TYPE_INT, PY_TYPE_DOUBLE, '-', PY_TYPE_DOUBLE);
    registerTypePromotion(PY_TYPE_INT, PY_TYPE_DOUBLE, '*', PY_TYPE_DOUBLE);
    registerTypePromotion(PY_TYPE_INT, PY_TYPE_DOUBLE, '/', PY_TYPE_DOUBLE);
}

void TypeOperationRegistry::registerBinaryOp(
    char op,
    int leftTypeId,
    int rightTypeId,
    int resultTypeId,
    const std::string& runtimeFunc,
    bool needsWrap,
    std::function<llvm::Value*(PyCodeGen&, llvm::Value*, llvm::Value*)> customImpl)
{
    BinaryOpDescriptor descriptor;
    descriptor.leftTypeId = leftTypeId;
    descriptor.rightTypeId = rightTypeId;
    descriptor.resultTypeId = resultTypeId;
    descriptor.runtimeFunction = runtimeFunc;
    descriptor.needsWrap = needsWrap;
    descriptor.customImpl = customImpl;

    auto key = std::make_pair(leftTypeId, rightTypeId);
    binaryOps[op][key] = descriptor;
}

void TypeOperationRegistry::registerUnaryOp(
    char op,
    int operandTypeId,
    int resultTypeId,
    const std::string& runtimeFunc,
    bool needsWrap,
    std::function<llvm::Value*(PyCodeGen&, llvm::Value*)> customImpl)
{
    UnaryOpDescriptor descriptor;
    descriptor.operandTypeId = operandTypeId;
    descriptor.resultTypeId = resultTypeId;
    descriptor.runtimeFunction = runtimeFunc;
    descriptor.needsWrap = needsWrap;
    descriptor.customImpl = customImpl;

    unaryOps[op][operandTypeId] = descriptor;
}

void TypeOperationRegistry::registerTypeConversion(
    int sourceTypeId,
    int targetTypeId,
    const std::string& runtimeFunc,
    int conversionCost,
    std::function<llvm::Value*(PyCodeGen&, llvm::Value*)> customImpl)
{
    TypeConversionDescriptor descriptor;
    descriptor.sourceTypeId = sourceTypeId;
    descriptor.targetTypeId = targetTypeId;
    descriptor.runtimeFunction = runtimeFunc;
    descriptor.conversionCost = conversionCost;
    descriptor.customImpl = customImpl;

    typeConversions[sourceTypeId][targetTypeId] = descriptor;
}

void TypeOperationRegistry::registerTypeCompatibility(int typeIdA, int typeIdB, bool compatible)
{
    auto key = std::make_pair(typeIdA, typeIdB);
    typeCompatibility[key] = compatible;

    // 兼容性是对称的
    auto reverseKey = std::make_pair(typeIdB, typeIdA);
    typeCompatibility[reverseKey] = compatible;
}

void TypeOperationRegistry::registerTypePromotion(int typeIdA, int typeIdB, char op, int resultTypeId)
{
    size_t hashValue = (static_cast<size_t>(typeIdA) << 16) | (static_cast<size_t>(typeIdB) << 8) | static_cast<size_t>(op);

   // 使用 std::make_tuple 创建元组并添加到 map 中，不再需要手动计算哈希值
   typePromotions[std::make_tuple(typeIdA, typeIdB, op)] = resultTypeId;

    // 操作数顺序通常不重要，所以也注册反向的组合
    size_t reverseHashValue = (static_cast<size_t>(typeIdB) << 16) | (static_cast<size_t>(typeIdA) << 8) | static_cast<size_t>(op);

     // 操作数顺序通常不重要，所以也注册反向的组合
     typePromotions[std::make_tuple(typeIdB, typeIdA, op)] = resultTypeId;
}

BinaryOpDescriptor* TypeOperationRegistry::getBinaryOpDescriptor(char op, int leftTypeId, int rightTypeId)
{
    auto opIt = binaryOps.find(op);
    if (opIt != binaryOps.end())
    {
        auto key = std::make_pair(leftTypeId, rightTypeId);
        auto& descriptors = opIt->second;
        auto descIt = descriptors.find(key);

        if (descIt != descriptors.end())
        {
            return &descIt->second;
        }
    }

    return nullptr;
}

UnaryOpDescriptor* TypeOperationRegistry::getUnaryOpDescriptor(char op, int operandTypeId)
{
    auto opIt = unaryOps.find(op);
    if (opIt != unaryOps.end())
    {
        auto& descriptors = opIt->second;
        auto descIt = descriptors.find(operandTypeId);

        if (descIt != descriptors.end())
        {
            return &descIt->second;
        }
    }

    return nullptr;
}

TypeConversionDescriptor* TypeOperationRegistry::getTypeConversionDescriptor(int sourceTypeId, int targetTypeId)
{
    auto sourceIt = typeConversions.find(sourceTypeId);
    if (sourceIt != typeConversions.end())
    {
        auto& targetMap = sourceIt->second;
        auto targetIt = targetMap.find(targetTypeId);

        if (targetIt != targetMap.end())
        {
            return &targetIt->second;
        }
    }

    return nullptr;
}

bool TypeOperationRegistry::areTypesCompatible(int typeIdA, int typeIdB)
{
    // 相同类型总是兼容的
    if (typeIdA == typeIdB)
    {
        return true;
    }

    // 检查注册的兼容性
    auto key = std::make_pair(typeIdA, typeIdB);
    auto it = typeCompatibility.find(key);

    if (it != typeCompatibility.end())
    {
        return it->second;
    }

    // 默认不兼容
    return false;
}

int TypeOperationRegistry::getResultTypeId(int leftTypeId, int rightTypeId, char op)
{
    // 如果类型相同，通常结果也是该类型 (除非特殊操作)
    if (leftTypeId == rightTypeId)
    {
        // 检查是否有注册的操作规则
        BinaryOpDescriptor* desc = getBinaryOpDescriptor(op, leftTypeId, rightTypeId);
        if (desc)
        {
            return desc->resultTypeId;
        }

        // 否则默认返回相同类型
        return leftTypeId;
    }

    // 检查注册的类型提升规则
    auto result = typePromotions.find(std::make_tuple(leftTypeId, rightTypeId, op));
    if (result != typePromotions.end())
    {
        return result->second;
    }

    // 否则返回比较高级的类型 (简单启发式规则)
    return std::max(leftTypeId, rightTypeId);
}

TypeConversionDescriptor* TypeOperationRegistry::findBestConversion(int fromTypeId, int toTypeId)
{
    // 如果类型相同，不需要转换
    if (fromTypeId == toTypeId)
    {
        return nullptr;
    }

    // 尝试直接转换
    TypeConversionDescriptor* directConv = getTypeConversionDescriptor(fromTypeId, toTypeId);
    if (directConv)
    {
        return directConv;
    }

    // 寻找中间转换路径 (简单版本，只考虑一步中间转换)
    std::unordered_set<int> intermediateTypes = {
        PY_TYPE_INT, PY_TYPE_DOUBLE, PY_TYPE_BOOL, PY_TYPE_STRING};

    TypeConversionDescriptor* bestConv = nullptr;
    int bestCost = INT_MAX;

    for (int midType : intermediateTypes)
    {
        if (midType == fromTypeId || midType == toTypeId)
        {
            continue;
        }

        TypeConversionDescriptor* firstStep = getTypeConversionDescriptor(fromTypeId, midType);
        TypeConversionDescriptor* secondStep = getTypeConversionDescriptor(midType, toTypeId);

        if (firstStep && secondStep)
        {
            int totalCost = firstStep->conversionCost + secondStep->conversionCost;
            if (totalCost < bestCost)
            {
                bestCost = totalCost;
                bestConv = firstStep;  // 返回第一步转换
            }
        }
    }

    return bestConv;
}

std::pair<int, int> TypeOperationRegistry::findOperablePath(char op, int leftTypeId, int rightTypeId)
{
    // 检查是否已有直接的操作路径
    if (getBinaryOpDescriptor(op, leftTypeId, rightTypeId))
    {
        return {leftTypeId, rightTypeId};
    }

    // 尝试将左操作数转换为右操作数类型
    if (areTypesCompatible(leftTypeId, rightTypeId))
    {
        if (getBinaryOpDescriptor(op, rightTypeId, rightTypeId))
        {
            return {rightTypeId, rightTypeId};
        }
    }

    // 尝试将右操作数转换为左操作数类型
    if (areTypesCompatible(rightTypeId, leftTypeId))
    {
        if (getBinaryOpDescriptor(op, leftTypeId, leftTypeId))
        {
            return {leftTypeId, leftTypeId};
        }
    }

    // 尝试将两个操作数都转换为一个公共类型
    std::vector<int> commonTypes = {PY_TYPE_DOUBLE, PY_TYPE_INT, PY_TYPE_STRING, PY_TYPE_BOOL};

    for (int commonType : commonTypes)
    {
        if (areTypesCompatible(leftTypeId, commonType) && areTypesCompatible(rightTypeId, commonType))
        {
            if (getBinaryOpDescriptor(op, commonType, commonType))
            {
                return {commonType, commonType};
            }
        }
    }

    // 没有找到可行路径，返回原始类型对
    return {leftTypeId, rightTypeId};
}

//===----------------------------------------------------------------------===//
// 操作符映射实现
//===----------------------------------------------------------------------===//

std::string OperatorMapper::getBinaryOpName(char op)
{
    switch (op)
    {
        case '+':
            return "add";
        case '-':
            return "subtract";
        case '*':
            return "multiply";
        case '/':
            return "divide";
        case '%':
            return "modulo";
        case '&':
            return "and";
        case '|':
            return "or";
        case '^':
            return "xor";
        case '<':
            return "lt";
        case '>':
            return "gt";
        case '=':
            return "eq";
        case '!':
            return "ne";
        default:
            return "unknown";
    }
}

std::string OperatorMapper::getUnaryOpName(char op)
{
    switch (op)
    {
        case '-':
            return "neg";
        case '~':
            return "invert";
        case '!':
            return "not";
        default:
            return "unknown";
    }
}

std::string OperatorMapper::getComparisonOpName(char op)
{
    switch (op)
    {
        case '<':
            return "lt";
        case '>':
            return "gt";
        case '=':
            return "eq";  // ==
        case '!':
            return "ne";  // !=
        case '[':
            return "le";  // <=
        case ']':
            return "ge";  // >=
        default:
            return "unknown";
    }
}

std::string OperatorMapper::getRuntimeFunctionName(const std::string& base, const std::string& opName)
{
    std::stringstream ss;
    ss << "py_" << base << "_" << opName;
    return ss.str();
}

//===----------------------------------------------------------------------===//
// 操作代码生成器实现
//===----------------------------------------------------------------------===//

llvm::Value* OperationCodeGenerator::handleBinaryOp(
    PyCodeGen& gen,
    char op,
    llvm::Value* left,
    llvm::Value* right,
    int leftTypeId,
    int rightTypeId)
{
    auto& builder = gen.getBuilder();
    llvm::LLVMContext& context = gen.getContext();
    auto& registry = TypeOperationRegistry::getInstance();

    // 获取操作描述符
    BinaryOpDescriptor* descriptor = registry.getBinaryOpDescriptor(op, leftTypeId, rightTypeId);

    // 如果找不到直接的操作描述符，尝试找到类型转换路径
    if (!descriptor)
    {
        std::pair<int, int> opPath = registry.findOperablePath(op, leftTypeId, rightTypeId);

        // 如果需要转换左操作数
        if (opPath.first != leftTypeId)
        {
            TypeConversionDescriptor* convDesc = registry.getTypeConversionDescriptor(leftTypeId, opPath.first);
            if (convDesc)
            {
                left = handleTypeConversion(gen, left, leftTypeId, opPath.first);
                leftTypeId = opPath.first;
            }
        }

        // 如果需要转换右操作数
        if (opPath.second != rightTypeId)
        {
            TypeConversionDescriptor* convDesc = registry.getTypeConversionDescriptor(rightTypeId, opPath.second);
            if (convDesc)
            {
                right = handleTypeConversion(gen, right, rightTypeId, opPath.second);
                rightTypeId = opPath.second;
            }
        }

        // 再次尝试获取操作描述符
        descriptor = registry.getBinaryOpDescriptor(op, leftTypeId, rightTypeId);
    }

    // 如果仍然找不到操作描述符，处理错误
    if (!descriptor)
    {
        std::cerr << "No binary operation " << op << " defined for types "
                  << leftTypeId << " and " << rightTypeId << std::endl;
        return nullptr;
    }

    // 如果有自定义实现，使用它
    if (descriptor->customImpl)
    {
        return descriptor->customImpl(gen, left, right);
    }

    // 否则使用运行时函数
    llvm::FunctionType* funcType = llvm::FunctionType::get(
        llvm::PointerType::get(context, 0),
        {llvm::PointerType::get(context, 0),
         llvm::PointerType::get(context, 0)},
        false);

    llvm::Function* func = llvm::Function::Create(
        funcType,
        llvm::Function::ExternalLinkage,
        descriptor->runtimeFunction,
        gen.getModule());

    // 如果操作数不是指针类型，需要包装为对象
    if (!left->getType()->isPointerTy())
    {
        left = createObject(gen, left, leftTypeId);
    }

    if (!right->getType()->isPointerTy())
    {
        right = createObject(gen, right, rightTypeId);
    }

    // 调用运行时函数
    return builder.CreateCall(func, {left, right}, "binop_result");
}

llvm::Value* OperationCodeGenerator::handleUnaryOp(
    PyCodeGen& gen,
    char op,
    llvm::Value* operand,
    int operandTypeId)
{
    auto& builder = gen.getBuilder();
    llvm::LLVMContext& context = gen.getContext();
    auto& registry = TypeOperationRegistry::getInstance();

    // 获取操作描述符
    UnaryOpDescriptor* descriptor = registry.getUnaryOpDescriptor(op, operandTypeId);

    if (!descriptor)
    {
        std::cerr << "No unary operation " << op << " defined for type " << operandTypeId << std::endl;
        return nullptr;
    }

    // 如果有自定义实现，使用它
    if (descriptor->customImpl)
    {
        return descriptor->customImpl(gen, operand);
    }

    // 否则使用运行时函数
    llvm::FunctionType* funcType = llvm::FunctionType::get(
        llvm::PointerType::get(context, 0),
        {llvm::PointerType::get(context, 0)},
        false);

    llvm::Function* func = llvm::Function::Create(
        funcType,
        llvm::Function::ExternalLinkage,
        descriptor->runtimeFunction,
        gen.getModule());

    // 如果操作数不是指针类型，需要包装为对象
    if (!operand->getType()->isPointerTy())
    {
        operand = createObject(gen, operand, operandTypeId);
    }

    // 调用运行时函数
    return builder.CreateCall(func, {operand}, "unaryop_result");
}

llvm::Value* OperationCodeGenerator::handleTypeConversion(
    PyCodeGen& gen,
    llvm::Value* value,
    int fromTypeId,
    int toTypeId)
{
    // 如果类型相同，不需要转换
    if (fromTypeId == toTypeId)
    {
        return value;
    }

    auto& builder = gen.getBuilder();
    llvm::LLVMContext& context = gen.getContext();
    auto& registry = TypeOperationRegistry::getInstance();

    // 获取类型转换描述符
    TypeConversionDescriptor* descriptor = registry.getTypeConversionDescriptor(fromTypeId, toTypeId);

    if (!descriptor)
    {
        std::cerr << "No conversion defined from type " << fromTypeId << " to " << toTypeId << std::endl;
        return value;  // 返回原值，可能导致后续错误
    }

    // 如果有自定义实现，使用它
    if (descriptor->customImpl)
    {
        return descriptor->customImpl(gen, value);
    }

    // 对于基本数值类型之间的转换，使用LLVM指令
    if (fromTypeId == PY_TYPE_INT && toTypeId == PY_TYPE_DOUBLE)
    {
        if (value->getType()->isIntegerTy())
        {
            return builder.CreateSIToFP(value, llvm::Type::getDoubleTy(context), "int_to_double");
        }
    }
    else if (fromTypeId == PY_TYPE_DOUBLE && toTypeId == PY_TYPE_INT)
    {
        if (value->getType()->isDoubleTy())
        {
            return builder.CreateFPToSI(value, llvm::Type::getInt32Ty(context), "double_to_int");
        }
    }

    // 否则使用运行时函数
    llvm::FunctionType* funcType = llvm::FunctionType::get(
        llvm::PointerType::get(context, 0),
        {llvm::PointerType::get(context, 0)},
        false);

    llvm::Function* func = llvm::Function::Create(
        funcType,
        llvm::Function::ExternalLinkage,
        descriptor->runtimeFunction,
        gen.getModule());

    // 如果值不是指针类型，需要包装为对象
    if (!value->getType()->isPointerTy())
    {
        value = createObject(gen, value, fromTypeId);
    }

    // 调用运行时函数
    return builder.CreateCall(func, {value}, "conversion_result");
}

int OperationCodeGenerator::getTypeId(ObjectType* type)
{
    if (!type) return PY_TYPE_NONE;

    return type->getTypeId();
}

llvm::Value* OperationCodeGenerator::createObject(PyCodeGen& gen, llvm::Value* value, int typeId)
{
    auto& builder = gen.getBuilder();
    llvm::LLVMContext& context = gen.getContext();

    // 根据类型ID创建不同的对象
    llvm::FunctionType* funcType = nullptr;
    llvm::Function* createFunc = nullptr;
    std::string funcName;

    switch (typeId)
    {
        case PY_TYPE_INT:
            funcName = "py_create_int";
            funcType = llvm::FunctionType::get(
                llvm::PointerType::get(context, 0),
                {llvm::Type::getInt32Ty(context)},
                false);
            break;

        case PY_TYPE_DOUBLE:
            funcName = "py_create_double";
            funcType = llvm::FunctionType::get(
                llvm::PointerType::get(context, 0),
                {llvm::Type::getDoubleTy(context)},
                false);
            break;

        case PY_TYPE_BOOL:
            funcName = "py_create_bool";
            funcType = llvm::FunctionType::get(
                llvm::PointerType::get(context, 0),
                {llvm::Type::getInt1Ty(context)},
                false);
            break;

        case PY_TYPE_STRING:
            funcName = "py_create_string";
            funcType = llvm::FunctionType::get(
                llvm::PointerType::get(context, 0),
                {llvm::PointerType::get(context, 0)},
                false);
            break;

        default:
            std::cerr << "Cannot create object of type " << typeId << std::endl;
            return value;
    }

    createFunc = llvm::Function::Create(
        funcType,
        llvm::Function::ExternalLinkage,
        funcName,
        gen.getModule());

    // 类型转换，如果需要的话
    if (typeId == PY_TYPE_INT && !value->getType()->isIntegerTy(32))
    {
        if (value->getType()->isIntegerTy())
        {
            value = builder.CreateIntCast(value, llvm::Type::getInt32Ty(context), true, "intcast");
        }
        else if (value->getType()->isDoubleTy())
        {
            value = builder.CreateFPToSI(value, llvm::Type::getInt32Ty(context), "double_to_int");
        }
    }
    else if (typeId == PY_TYPE_DOUBLE && !value->getType()->isDoubleTy())
    {
        if (value->getType()->isIntegerTy())
        {
            value = builder.CreateSIToFP(value, llvm::Type::getDoubleTy(context), "int_to_double");
        }
    }
    else if (typeId == PY_TYPE_BOOL && !value->getType()->isIntegerTy(1))
    {
        value = builder.CreateICmpNE(value, llvm::ConstantInt::get(value->getType(), 0), "to_bool");
    }

    // 调用创建函数
    return builder.CreateCall(createFunc, {value}, "obj_create");
}

//===----------------------------------------------------------------------===//
// 操作结果处理器实现
//===----------------------------------------------------------------------===//

llvm::Value* OperationResultHandler::adjustResult(
    PyCodeGen& gen,
    llvm::Value* result,
    int resultTypeId,
    int expectedTypeId)
{
    // 如果类型已经匹配，不需要调整
    if (resultTypeId == expectedTypeId)
    {
        return result;
    }

    auto& builder = gen.getBuilder();
    auto& registry = TypeOperationRegistry::getInstance();

    // 检查是否需要类型转换
    TypeConversionDescriptor* convDesc = registry.getTypeConversionDescriptor(resultTypeId, expectedTypeId);
    if (convDesc)
    {
        result = OperationCodeGenerator::handleTypeConversion(gen, result, resultTypeId, expectedTypeId);
        return result;
    }

    // 如果没有直接的转换，可能需要拆箱或装箱
    if (result->getType()->isPointerTy() && expectedTypeId == PY_TYPE_INT)
    {
        // 尝试从对象中提取整数值
        llvm::FunctionType* extractFuncType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(gen.getContext()),
            {llvm::PointerType::get(gen.getContext(), 0)},
            false);

        llvm::Function* extractFunc = llvm::Function::Create(
            extractFuncType,
            llvm::Function::ExternalLinkage,
            "py_extract_int",
            gen.getModule());

        return builder.CreateCall(extractFunc, {result}, "extract_int");
    }
    else if (!result->getType()->isPointerTy() && (expectedTypeId == PY_TYPE_NONE || expectedTypeId >= PY_TYPE_LIST_BASE))
    {
        // 将基本类型值包装为对象
        return OperationCodeGenerator::createObject(gen, result, resultTypeId);
    }

    // 如果都不适用，返回原值
    return result;
}

llvm::Value* OperationResultHandler::handleFunctionReturn(
    PyCodeGen& gen,
    llvm::Value* result,
    int resultTypeId,
    bool isFunctionReturn)
{
    auto& builder = gen.getBuilder();
    llvm::LLVMContext& context = gen.getContext();

    // 如果不是函数返回，不需要特殊处理
    if (!isFunctionReturn)
    {
        return result;
    }

    // 对于引用类型，需要确保正确处理对象生命周期
    bool isResultReference = (resultTypeId == PY_TYPE_STRING || resultTypeId == PY_TYPE_LIST || resultTypeId == PY_TYPE_DICT || resultTypeId >= PY_TYPE_LIST_BASE);

    if (isResultReference && result->getType()->isPointerTy())
    {
        // 复制对象，并确保使用正确的类型ID
        llvm::FunctionType* copyFuncType = llvm::FunctionType::get(
            llvm::PointerType::get(context, 0),
            {llvm::PointerType::get(context, 0),
             llvm::Type::getInt32Ty(context)},
            false);

        llvm::Function* copyFunc = llvm::Function::Create(
            copyFuncType,
            llvm::Function::ExternalLinkage,
            "py_object_copy",
            gen.getModule());

        // 使用正确的类型ID - 这是修复"TypeError: Expected type 5, got 1"错误的关键部分
        llvm::Value* typeIdValue = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(context),
            resultTypeId);

        return builder.CreateCall(copyFunc, {result, typeIdValue}, "return_copy");
    }

    return result;
}

llvm::Value* OperationResultHandler::prepareArgument(
    PyCodeGen& gen,
    llvm::Value* value,
    int fromTypeId,
    int toTypeId)
{
    // 如果类型已经匹配，不需要调整
    if (fromTypeId == toTypeId)
    {
        return value;
    }

    auto& builder = gen.getBuilder();
    auto& registry = TypeOperationRegistry::getInstance();

    // 检查是否需要类型转换
    TypeConversionDescriptor* convDesc = registry.getTypeConversionDescriptor(fromTypeId, toTypeId);
    if (convDesc)
    {
        value = OperationCodeGenerator::handleTypeConversion(gen, value, fromTypeId, toTypeId);
        return value;
    }

    // 如果是传递基本类型到引用类型参数，包装为对象
    if (!value->getType()->isPointerTy() && (toTypeId == PY_TYPE_OBJECT || toTypeId >= PY_TYPE_LIST_BASE))
    {
        return OperationCodeGenerator::createObject(gen, value, fromTypeId);
    }

    // 如果都不适用，返回原值
    return value;
}

//===----------------------------------------------------------------------===//
// 类型推导器实现
//===----------------------------------------------------------------------===//

ObjectType* TypeInferencer::inferBinaryOpResultType(
    ObjectType* leftType,
    ObjectType* rightType,
    char op)
{
    if (!leftType || !rightType)
    {
        return nullptr;
    }

    // 获取类型ID
    int leftTypeId = leftType->getTypeId();
    int rightTypeId = rightType->getTypeId();

    // 使用类型操作注册表获取结果类型ID
    int resultTypeId = TypeOperationRegistry::getInstance().getResultTypeId(leftTypeId, rightTypeId, op);

    // 基于结果类型ID创建ObjectType
    switch (resultTypeId)
    {
        case PY_TYPE_INT:
            return TypeRegistry::getInstance().getType("int");

        case PY_TYPE_DOUBLE:
            return TypeRegistry::getInstance().getType("double");

        case PY_TYPE_BOOL:
            return TypeRegistry::getInstance().getType("bool");

        case PY_TYPE_STRING:
            return TypeRegistry::getInstance().getType("string");

        case PY_TYPE_LIST:
            // 对于列表，需要保持元素类型
            if (const ListType* leftList = dynamic_cast<const ListType*>(leftType))
            {
                return TypeRegistry::getInstance().getListType(
                    const_cast<ObjectType*>(leftList->getElementType()));
            }
            else if (const ListType* rightList = dynamic_cast<const ListType*>(rightType))
            {
                return TypeRegistry::getInstance().getListType(
                    const_cast<ObjectType*>(rightList->getElementType()));
            }
            // 默认为Object元素类型
            return TypeRegistry::getInstance().getListType(
                TypeRegistry::getInstance().getType("object"));

        case PY_TYPE_DICT:
            // 对于字典，保持键值类型
            if (const DictType* leftDict = dynamic_cast<const DictType*>(leftType))
            {
                return TypeRegistry::getInstance().getDictType(
                    const_cast<ObjectType*>(leftDict->getKeyType()),
                    const_cast<ObjectType*>(leftDict->getValueType()));
            }
            // 默认键为string，值为object
            return TypeRegistry::getInstance().getDictType(
                TypeRegistry::getInstance().getType("string"),
                TypeRegistry::getInstance().getType("object"));

        default:
            // 对于未知类型ID，返回通用object类型
            return TypeRegistry::getInstance().getType("object");
    }
}

ObjectType* TypeInferencer::inferUnaryOpResultType(
    ObjectType* operandType,
    char op)
{
    if (!operandType)
    {
        return nullptr;
    }

    // 对于一元操作，结果类型通常与操作数类型相同，除了特殊情况
    switch (op)
    {
        case '-':
            // 数值取负，类型保持不变
            if (TypeFeatureChecker::isNumeric(operandType))
            {
                return operandType;
            }
            // 如果不是数值类型，返回整型（默认行为）
            return TypeRegistry::getInstance().getType("int");

        case '!':  // not 操作
        case '~':  // 逻辑非
            // 布尔操作通常返回布尔类型
            return TypeRegistry::getInstance().getType("bool");

        default:
            // 未知一元操作，返回与操作数相同的类型
            return operandType;
    }
}

// 实现 TypeInferencer::getCommonSuperType 方法
ObjectType* TypeInferencer::getCommonSuperType(
    ObjectType* typeA,
    ObjectType* typeB)
{
    if (!typeA) return typeB;
    if (!typeB) return typeA;
    if (typeA == typeB) return typeA;

    // 获取类型ID
    int typeIdA = typeA->getTypeId();
    int typeIdB = typeB->getTypeId();

    // 如果是基本数值类型
    bool isANumeric = (typeIdA == PY_TYPE_INT || typeIdA == PY_TYPE_DOUBLE || typeIdA == PY_TYPE_BOOL);
    bool isBNumeric = (typeIdB == PY_TYPE_INT || typeIdB == PY_TYPE_DOUBLE || typeIdB == PY_TYPE_BOOL);

    if (isANumeric && isBNumeric)
    {
        // 数值类型提升规则: bool < int < double
        if (typeIdA == PY_TYPE_DOUBLE || typeIdB == PY_TYPE_DOUBLE)
        {
            return TypeRegistry::getInstance().getType("double");
        }
        else if (typeIdA == PY_TYPE_INT || typeIdB == PY_TYPE_INT)
        {
            return TypeRegistry::getInstance().getType("int");
        }
        else
        {
            return TypeRegistry::getInstance().getType("bool");
        }
    }

    // 如果是同类容器类型，尝试合并元素类型
    if (typeIdA == PY_TYPE_LIST && typeIdB == PY_TYPE_LIST)
    {
        const ListType* listA = dynamic_cast<const ListType*>(typeA);
        const ListType* listB = dynamic_cast<const ListType*>(typeB);
        if (listA && listB)
        {
            ObjectType* elemCommonType = getCommonSuperType(
                const_cast<ObjectType*>(listA->getElementType()),
                const_cast<ObjectType*>(listB->getElementType()));
            return TypeRegistry::getInstance().getListType(elemCommonType);
        }
    }

    if (typeIdA == PY_TYPE_DICT && typeIdB == PY_TYPE_DICT)
    {
        const DictType* dictA = dynamic_cast<const DictType*>(typeA);
        const DictType* dictB = dynamic_cast<const DictType*>(typeB);
        if (dictA && dictB)
        {
            ObjectType* keyCommonType = getCommonSuperType(
                const_cast<ObjectType*>(dictA->getKeyType()),
                const_cast<ObjectType*>(dictB->getKeyType()));
            ObjectType* valueCommonType = getCommonSuperType(
                const_cast<ObjectType*>(dictA->getValueType()),
                const_cast<ObjectType*>(dictB->getValueType()));
            return TypeRegistry::getInstance().getDictType(keyCommonType, valueCommonType);
        }
    }

    // 如果类型不兼容或者没有明确的共同类型，返回通用Object类型
    return TypeRegistry::getInstance().getType("object");
}

// 添加从PyTokenType到操作符字符的映射辅助函数
char convertTokenTypeToOperator(PyTokenType tokenType)
{
    switch (tokenType)
    {
        case TOK_PLUS:
            return '+';
        case TOK_MINUS:
            return '-';
        case TOK_MUL:
            return '*';
        case TOK_DIV:
            return '/';
        case TOK_MOD:
            return '%';
        case TOK_LT:
            return '<';
        case TOK_GT:
            return '>';
        case TOK_LE:
            return '[';  // 使用'['表示小于等于，与OperatorMapper::getComparisonOpName一致
        case TOK_GE:
            return ']';  // 使用']'表示大于等于，与OperatorMapper::getComparisonOpName一致
        case TOK_EQ:
            return '=';
        case TOK_NEQ:
            return '!';
        default:
            // 不支持的标记类型，返回空字符
            return '\0';
    }
}
// 添加支持PyTokenType的重载版本
ObjectType* TypeInferencer::inferBinaryOpResultType(
    ObjectType* leftType,
    ObjectType* rightType,
    PyTokenType opToken)
{
    char op = convertTokenTypeToOperator(opToken);
    if (op == '\0')
    {
        // 不支持的操作符，返回默认类型
        return TypeRegistry::getInstance().getType("object");
    }
    return inferBinaryOpResultType(leftType, rightType, op);
}

// 添加支持PyTokenType的重载版本
ObjectType* TypeInferencer::inferUnaryOpResultType(
    ObjectType* operandType,
    PyTokenType opToken)
{
    char op = convertTokenTypeToOperator(opToken);
    if (op == '\0')
    {
        // 不支持的操作符，返回与操作数相同的类型
        return operandType;
    }
    return inferUnaryOpResultType(operandType, op);
}

}  // namespace llvmpy
