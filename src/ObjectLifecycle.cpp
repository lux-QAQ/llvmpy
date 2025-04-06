#include "ObjectLifecycle.h"
#include "TypeIDs.h"
#include "TypeOperations.h"
#include "CodeGen/codegen.h"
#include <iostream>

namespace llvmpy
{

//===----------------------------------------------------------------------===//
// 生命周期管理决策
//===----------------------------------------------------------------------===//

bool ObjectLifecycleManager::needsCopy(ObjectType* type, ObjectSource source, ObjectDestination dest)
{
    if (!type) return false;

    // 不可变基本类型不需要复制
    if (!type->isReference() && type->getCategory() == ObjectType::Primitive)
    {
        return false;
    }

    // 根据来源和目标决定是否需要复制
    switch (source)
    {
        case ObjectSource::FUNCTION_RETURN:
            // 函数返回值已经是新创建的对象或者已经被正确处理
            return false;

        case ObjectSource::BINARY_OP:
        case ObjectSource::UNARY_OP:
            // 操作结果通常是新创建的对象，不需要复制
            return false;

        case ObjectSource::LITERAL:
            // 字面量不需要复制
            return false;

        case ObjectSource::LOCAL_VARIABLE:
            // 本地变量在作为返回值或长期存储时需要复制
            return (dest == ObjectDestination::RETURN || dest == ObjectDestination::STORAGE);

        case ObjectSource::PARAMETER:
            // 参数在作为返回值时需要复制
            return (dest == ObjectDestination::RETURN);

        case ObjectSource::INDEX_ACCESS:
        case ObjectSource::ATTRIBUTE_ACCESS:
            // 索引和属性访问在修改时需要复制
            return (dest == ObjectDestination::ASSIGNMENT || dest == ObjectDestination::RETURN || dest == ObjectDestination::STORAGE);

        default:
            return false;
    }
}

bool ObjectLifecycleManager::needsIncRef(ObjectType* type, ObjectSource source, ObjectDestination dest)
{
    if (!type || !type->isReference())
    {
        return false;
    }

    // 引用类型对象在以下情况需要增加引用计数
    switch (dest)
    {
        case ObjectDestination::ASSIGNMENT:
        case ObjectDestination::STORAGE:
        case ObjectDestination::PARAMETER:
            return true;

        case ObjectDestination::RETURN:
            // 返回值如果是从其他地方获取的对象，需要增加引用计数
            return (source == ObjectSource::LOCAL_VARIABLE || source == ObjectSource::PARAMETER || source == ObjectSource::INDEX_ACCESS || source == ObjectSource::ATTRIBUTE_ACCESS);

        case ObjectDestination::TEMPORARY:
            // 临时对象不需要增加引用计数
            return false;

        default:
            return false;
    }
}

bool ObjectLifecycleManager::needsDecRef(ObjectType* type, ObjectSource source)
{
    if (!type || !type->isReference())
    {
        return false;
    }

    // 临时对象需要在使用后减少引用计数
    return (source == ObjectSource::BINARY_OP || source == ObjectSource::UNARY_OP || source == ObjectSource::FUNCTION_RETURN);
}

//===----------------------------------------------------------------------===//
// 类型识别与转换
//===----------------------------------------------------------------------===//

int ObjectLifecycleManager::getObjectTypeId(ObjectType* type)
{
    if (!type) return PY_TYPE_NONE;

    // 获取类型的运行时ID
    return type->getTypeId();
}

int ObjectLifecycleManager::getElementTypeId(ObjectType* containerType)
{
    if (!containerType) return PY_TYPE_NONE;

    const ListType* listType = dynamic_cast<const ListType*>(containerType);
    if (listType)
    {
        const ObjectType* elemType = listType->getElementType();
        return elemType ? elemType->getTypeId() : PY_TYPE_NONE;
    }

    return PY_TYPE_NONE;
}

bool ObjectLifecycleManager::needsWrapping(llvm::Value* value, ObjectType* type)
{
    if (!value || !type) return false;

    // 如果值是整数或浮点数，但目标类型是引用类型，则需要包装
    bool isPrimitiveValue = value->getType()->isIntegerTy() || value->getType()->isFloatingPointTy();
    return isPrimitiveValue && type->isReference();
}

int ObjectLifecycleManager::getTypeIdFromValue(PyCodeGen& gen, llvm::Value* value)
{
    if (!value) return -1;

    // 尝试从代码生成器获取最后表达式的类型
    auto lastExprType = gen.getLastExprType();
    if (lastExprType && lastExprType->getObjectType())
    {
        return lastExprType->getObjectType()->getTypeId();
    }

    // 根据LLVM类型推断类型ID
    if (value->getType()->isIntegerTy(1))
    {
        return PY_TYPE_BOOL;
    }
    else if (value->getType()->isIntegerTy())
    {
        return PY_TYPE_INT;
    }
    else if (value->getType()->isFloatingPointTy())
    {
        return PY_TYPE_DOUBLE;
    }
    else if (value->getType()->isPointerTy())
    {
        // 指针类型，尝试从指针类型名称判断
        std::string name = value->getName().str();

        // 修正: 使用标准字符串方法代替 startswith
        if (name.find("str") == 0)
        {
            return PY_TYPE_STRING;
        }
        else if (name.find("list") == 0)
        {
            return PY_TYPE_LIST;
        }
        else if (name.find("dict") == 0)
        {
            return PY_TYPE_DICT;
        }

        // 无法确定具体类型，假设为通用引用
        return PY_TYPE_NONE;
    }

    return -1;
}

bool ObjectLifecycleManager::areTypesCompatible(ObjectType* typeA, ObjectType* typeB)
{
    if (!typeA || !typeB) return false;

    // 相同类型当然兼容
    if (typeA == typeB) return true;

    // 如果两个类型都是数值类型，则认为兼容
    bool isANumeric = TypeFeatureChecker::isNumeric(typeA);
    bool isBNumeric = TypeFeatureChecker::isNumeric(typeB);
    if (isANumeric && isBNumeric) return true;

    // 如果两个类型都是序列类型，可能兼容
    bool isASequence = TypeFeatureChecker::isSequence(typeA);
    bool isBSequence = TypeFeatureChecker::isSequence(typeB);
    if (isASequence && isBSequence)
    {
        // 字符串和列表间不兼容

        if ((typeA->getTypeId() == PY_TYPE_STRING && dynamic_cast<const ListType*>(typeB)) || (typeB->getTypeId() == PY_TYPE_STRING && dynamic_cast<const ListType*>(typeA)))
        {
            return false;
        }
        return true;
    }

    // 类型之间的显式兼容性关系
    return typeA->canConvertTo(typeB) || typeB->canConvertTo(typeA);
}

bool ObjectLifecycleManager::isTempObject(PyCodeGen& gen, llvm::Value* value)
{
    if (!value) return false;

    // 临时对象通常是函数调用的结果或二元操作的结果
    if (llvm::CallInst* call = llvm::dyn_cast<llvm::CallInst>(value))
    {
        // 检查函数名是否表示创建新对象
        llvm::Function* func = call->getCalledFunction();
        if (func)
        {
            std::string name = func->getName().str();
            if (name.find("py_create_") == 0 || name.find("py_object_") == 0)
            {
                return true;
            }
        }
    }

    // 修正: 使用通用临时对象检测逻辑，不依赖 isTrackedObject 方法
    // 检查是否为IR指令，且不是全局变量或常量
    if (llvm::Instruction* inst = llvm::dyn_cast<llvm::Instruction>(value))
    {
        // 不是 alloca 指令（本地变量）的指令通常是临时结果
        if (!llvm::isa<llvm::AllocaInst>(inst))
        {
            return true;
        }
    }

    return false;
}

//===----------------------------------------------------------------------===//
// 对象创建与调整
//===----------------------------------------------------------------------===//
// 确定表达式对象的来源
// 修改前的错误代码：
// ObjectLifecycleManager::ObjectSource CodeGenLifecycleManager::determineObjectSource(constExprAST* expr)

// 修改为正确的实现：
ObjectLifecycleManager::ObjectSource ObjectLifecycleManager::determineExprSource(const ExprAST* expr)
{
    if (!expr) return ObjectLifecycleManager::ObjectSource::LITERAL;

    switch (expr->kind())
    {
        case ASTKind::NumberExpr:
        case ASTKind::StringExpr:
        case ASTKind::BoolExpr:
        case ASTKind::NoneExpr:
        case ASTKind::ListExpr:
            return ObjectLifecycleManager::ObjectSource::LITERAL;

        case ASTKind::BinaryExpr:
            return ObjectLifecycleManager::ObjectSource::BINARY_OP;

        case ASTKind::UnaryExpr:
            return ObjectLifecycleManager::ObjectSource::UNARY_OP;

        case ASTKind::CallExpr:
            return ObjectLifecycleManager::ObjectSource::FUNCTION_RETURN;

        case ASTKind::VariableExpr:
            return ObjectLifecycleManager::ObjectSource::LOCAL_VARIABLE;

        case ASTKind::IndexExpr:
            return ObjectLifecycleManager::ObjectSource::INDEX_ACCESS;

        default:
            // 默认假设是临时计算结果
            return ObjectLifecycleManager::ObjectSource::BINARY_OP;
    }
}
llvm::Value* ObjectLifecycleManager::createObject(PyCodeGen& gen, llvm::Value* value, int typeId)
{
    auto& builder = gen.getBuilder();
    llvm::LLVMContext& context = gen.getContext();

    // 根据类型ID创建不同的对象
    switch (typeId)
    {
        case PY_TYPE_INT:
        {
            // 修正：使用公共API创建函数调用
            llvm::FunctionType* funcType = llvm::FunctionType::get(
                    llvm::PointerType::get(context, 0),
                    {llvm::Type::getInt32Ty(context)},
                    false);
            /* llvm::Function* createIntFunc = llvm::Function::Create(
                funcType, 
                llvm::Function::ExternalLinkage,
                "py_create_int", 
                gen.getModule()
            ); */
            llvm::Function* createIntFunc = gen.getOrCreateExternalFunction(
                    "py_create_int",
                    llvm::PointerType::get(context, 0),
                    {llvm::Type::getInt32Ty(context)},
                    false);
            return builder.CreateCall(createIntFunc, {value}, "int_obj");
        }

        case PY_TYPE_DOUBLE:
        {
            llvm::FunctionType* funcType = llvm::FunctionType::get(
                    llvm::PointerType::get(context, 0),
                    {llvm::Type::getDoubleTy(context)},
                    false);
                    llvm::Function* createDoubleFunc = gen.getOrCreateExternalFunction(
                        "py_create_double",
                        llvm::PointerType::get(context, 0),
                        {llvm::Type::getDoubleTy(context)},
                        false
                    );
            return builder.CreateCall(createDoubleFunc, {value}, "double_obj");
        }

        case PY_TYPE_BOOL:
        {
            llvm::FunctionType* funcType = llvm::FunctionType::get(
                    llvm::PointerType::get(context, 0),
                    {llvm::Type::getInt1Ty(context)},
                    false);
                    llvm::Function* createBoolFunc = gen.getOrCreateExternalFunction(
                        "py_create_bool",
                        llvm::PointerType::get(context, 0),
                        {llvm::Type::getInt1Ty(context)},
                        false
                    );
            return builder.CreateCall(createBoolFunc, {value}, "bool_obj");
        }

        case PY_TYPE_STRING:
        {
            llvm::FunctionType* funcType = llvm::FunctionType::get(
                    llvm::PointerType::get(context, 0),
                    {llvm::PointerType::get(context, 0)},
                    false);
                    llvm::Function* createStringFunc = gen.getOrCreateExternalFunction(
                        "py_create_string",
                        llvm::PointerType::get(context, 0),
                        {llvm::PointerType::get(context, 0)},
                        false
                    );
            return builder.CreateCall(createStringFunc, {value}, "string_obj");
        }

        case PY_TYPE_LIST:
        case PY_TYPE_DICT:
            // 对于复杂类型，假设已经创建好了对象
            return value;

        default:
            std::cerr << "警告: 未知的类型ID " << typeId << std::endl;
            return value;
    }
}

llvm::Value* ObjectLifecycleManager::adjustObject(
        PyCodeGen& gen,
        llvm::Value* value,
        ObjectType* type,
        ObjectSource source,
        ObjectDestination dest)
{
    if (!value || !type) return value;

    auto& builder = gen.getBuilder();
    llvm::LLVMContext& context = gen.getContext();

    // 1. 检查是否需要包装
    if (needsWrapping(value, type))
    {
        int typeId = getObjectTypeId(type);
        value = createObject(gen, value, typeId);
        // 修正：不再调用 trackObject，因为这是私有方法
    }

    // 2. 检查是否需要复制
    if (type->isReference() && needsCopy(type, source, dest))
    {
        int typeId = getObjectTypeId(type);

        // 修正：使用公共API创建函数调用
        llvm::FunctionType* copyFuncType = llvm::FunctionType::get(
                llvm::PointerType::get(context, 0),
                {llvm::PointerType::get(context, 0),
                 llvm::Type::getInt32Ty(context)},
                false);
                llvm::Function* copyFunc = gen.getOrCreateExternalFunction(
                    "py_object_copy",
                    llvm::PointerType::get(context, 0),
                    {llvm::PointerType::get(context, 0), llvm::Type::getInt32Ty(context)},
                    false);

        // 重要：使用对象的真实类型ID，而不是硬编码或错误的类型ID
        llvm::Value* typeIdVal = llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(context),
                typeId);

        value = builder.CreateCall(copyFunc, {value, typeIdVal}, "obj_copy");
        // 修正：不再调用 trackObject
    }

    // 3. 检查是否需要增加引用计数
    if (type->isReference() && needsIncRef(type, source, dest))
    {
        llvm::FunctionType* incRefFuncType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                {llvm::PointerType::get(context, 0)},
                false);
                llvm::Function* incRefFunc = gen.getOrCreateExternalFunction(
                    "py_incref",
                    llvm::Type::getVoidTy(context),
                    {llvm::PointerType::get(context, 0)},
                    false);

        builder.CreateCall(incRefFunc, {value});
    }

    return value;
}

llvm::Value* ObjectLifecycleManager::handleReturnValue(
        PyCodeGen& gen,
        llvm::Value* value,
        ObjectType* functionReturnType,
        std::shared_ptr<PyType> exprType)
{
    if (!value || !functionReturnType) return value;

    auto& builder = gen.getBuilder();
    llvm::LLVMContext& context = gen.getContext();

    // 1. 获取表达式实际类型
    ObjectType* actualType = exprType ? exprType->getObjectType() : nullptr;

    // 2. 如果表达式类型与函数返回类型不匹配，尝试转换
    if (actualType && actualType != functionReturnType)
    {
        // 需要转换，这里省略具体实现
        // TODO: 添加类型转换逻辑
    }

    // 3. 确保引用类型对象被正确处理
    if (functionReturnType->isReference())
    {
        // 如果是原始类型值但需要返回引用类型，创建对象
        if (needsWrapping(value, functionReturnType))
        {
            int typeId = getObjectTypeId(functionReturnType);
            value = createObject(gen, value, typeId);
        }

        // 3.1. 如果是临时对象，不需要复制，但需要确保使用正确的类型ID
        if (isTempObject(gen, value))
        {
            // 不复制，直接返回
            return value;
        }

        // 3.2. 否则，进行对象复制，确保使用正确的类型ID
        int typeId = getObjectTypeId(functionReturnType);

        // 获取对象复制函数
        llvm::FunctionType* copyFuncType = llvm::FunctionType::get(
                llvm::PointerType::get(context, 0),
                {llvm::PointerType::get(context, 0),
                 llvm::Type::getInt32Ty(context)},
                false);
                llvm::Function* copyFunc = gen.getOrCreateExternalFunction(
                    "py_object_copy",
                    llvm::PointerType::get(context, 0),
                    {llvm::PointerType::get(context, 0), llvm::Type::getInt32Ty(context)},
                    false);

        // 使用正确的类型ID
        llvm::Value* typeIdVal = llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(context),
                typeId);

        return builder.CreateCall(copyFunc, {value, typeIdVal}, "return_obj");
    }

    // 4. 返回原始值
    return value;
}

//===----------------------------------------------------------------------===//
// ObjectFactory 实现
//===----------------------------------------------------------------------===//

llvm::Value* ObjectFactory::createInt(PyCodeGen& gen, llvm::Value* value)
{
    llvm::LLVMContext& context = gen.getContext();

    // 修正：使用公共API创建函数调用
    llvm::FunctionType* createIntFuncType = llvm::FunctionType::get(
            llvm::PointerType::get(context, 0),
            {llvm::Type::getInt32Ty(context)},
            false);
            llvm::Function* createIntFunc = gen.getOrCreateExternalFunction(
                "py_create_int",
                llvm::PointerType::get(context, 0),
                {llvm::Type::getInt32Ty(context)},
                false);

    return gen.getBuilder().CreateCall(createIntFunc, {value}, "int_obj");
}

llvm::Value* ObjectFactory::createDouble(PyCodeGen& gen, llvm::Value* value)
{
    llvm::LLVMContext& context = gen.getContext();

    llvm::FunctionType* createDoubleFuncType = llvm::FunctionType::get(
            llvm::PointerType::get(context, 0),
            {llvm::Type::getDoubleTy(context)},
            false);
            llvm::Function* createDoubleFunc = gen.getOrCreateExternalFunction(
                "py_create_double",
                llvm::PointerType::get(context, 0),
                {llvm::Type::getDoubleTy(context)},
                false);

    return gen.getBuilder().CreateCall(createDoubleFunc, {value}, "double_obj");
}

llvm::Value* ObjectFactory::createBool(PyCodeGen& gen, llvm::Value* value)
{
    llvm::LLVMContext& context = gen.getContext();

    llvm::FunctionType* createBoolFuncType = llvm::FunctionType::get(
            llvm::PointerType::get(context, 0),
            {llvm::Type::getInt1Ty(context)},
            false);
            llvm::Function* createBoolFunc = gen.getOrCreateExternalFunction(
                "py_create_bool",
                llvm::PointerType::get(context, 0),
                {llvm::Type::getInt1Ty(context)},
                false);

    return gen.getBuilder().CreateCall(createBoolFunc, {value}, "bool_obj");
}

llvm::Value* ObjectFactory::createString(PyCodeGen& gen, llvm::Value* value)
{
    llvm::LLVMContext& context = gen.getContext();

    llvm::FunctionType* createStringFuncType = llvm::FunctionType::get(
            llvm::PointerType::get(context, 0),
            {llvm::PointerType::get(context, 0)},
            false);
            llvm::Function* createStringFunc = gen.getOrCreateExternalFunction(
                "py_create_string",
                llvm::PointerType::get(context, 0),
                {llvm::PointerType::get(context, 0)},
                false);

    return gen.getBuilder().CreateCall(createStringFunc, {value}, "string_obj");
}

llvm::Value* ObjectFactory::createList(PyCodeGen& gen, llvm::Value* size, ObjectType* elemType)
{
    llvm::LLVMContext& context = gen.getContext();
    int elemTypeId = ObjectLifecycleManager::getElementTypeId(elemType);

    llvm::FunctionType* createListFuncType = llvm::FunctionType::get(
            llvm::PointerType::get(context, 0),
            {llvm::Type::getInt32Ty(context),
             llvm::Type::getInt32Ty(context)},
            false);
            llvm::Function* createListFunc = gen.getOrCreateExternalFunction(
                "py_create_list",
                llvm::PointerType::get(context, 0),
                {llvm::Type::getInt32Ty(context), llvm::Type::getInt32Ty(context)},
                false);

    llvm::Value* elemTypeIdVal = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(context),
            elemTypeId);

    return gen.getBuilder().CreateCall(createListFunc, {size, elemTypeIdVal}, "list_obj");
}

llvm::Value* ObjectFactory::createDict(PyCodeGen& gen, ObjectType* keyType, ObjectType* valueType)
{
    llvm::LLVMContext& context = gen.getContext();
    int keyTypeId = ObjectLifecycleManager::getObjectTypeId(keyType);

    llvm::FunctionType* createDictFuncType = llvm::FunctionType::get(
            llvm::PointerType::get(context, 0),
            {llvm::Type::getInt32Ty(context),
             llvm::Type::getInt32Ty(context)},
            false);
            llvm::Function* createDictFunc = gen.getOrCreateExternalFunction(
                "py_create_dict",
                llvm::PointerType::get(context, 0),
                {llvm::Type::getInt32Ty(context), llvm::Type::getInt32Ty(context)},
                false);

    llvm::Value* initialCapacity = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(context),
            8  // 默认初始容量
    );

    llvm::Value* keyTypeIdVal = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(context),
            keyTypeId);

    return gen.getBuilder().CreateCall(createDictFunc, {initialCapacity, keyTypeIdVal}, "dict_obj");
}

//===----------------------------------------------------------------------===//
// ObjectTypeChecker 实现
//===----------------------------------------------------------------------===//

bool ObjectTypeChecker::isOfType(PyCodeGen& gen, llvm::Value* value, ObjectType* type)
{
    if (!value || !type) return false;

    auto& builder = gen.getBuilder();
    llvm::LLVMContext& context = gen.getContext();

    // 对于基本类型的直接检查
    if (!type->isReference())
    {
        return value->getType() == type->getLLVMType(context);
    }

    // 对于引用类型，使用运行时的类型检查
    int expectedTypeId = type->getTypeId();

    // 获取或创建类型检查函数
    llvm::FunctionType* checkTypeFuncType = llvm::FunctionType::get(
            llvm::Type::getInt1Ty(context),
            {llvm::PointerType::get(context, 0),
             llvm::Type::getInt32Ty(context)},
            false);
            llvm::Function* checkTypeFunc = gen.getOrCreateExternalFunction(
                "py_check_type",
                llvm::Type::getInt1Ty(context),
                {llvm::PointerType::get(context, 0), llvm::Type::getInt32Ty(context)},
                false);

    llvm::Value* typeIdVal = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(context),
            expectedTypeId);

    return builder.CreateCall(checkTypeFunc, {value, typeIdVal});
}

int ObjectTypeChecker::getConversionPath(ObjectType* fromType, ObjectType* toType)
{
    if (!fromType || !toType) return -1;

    // 相同类型不需要转换
    if (fromType == toType) return 0;

    // 获取类型ID
    int fromTypeId = fromType->getTypeId();
    int toTypeId = toType->getTypeId();

    // 定义转换路径代价
    std::unordered_map<std::pair<int, int>, int, PairHash> conversionCosts = {
            // 数值类型之间的转换
            {{PY_TYPE_INT, PY_TYPE_DOUBLE}, 1},
            {{PY_TYPE_DOUBLE, PY_TYPE_INT}, 2},  // 可能丢失精度，代价更高

            // 到布尔类型的转换
            {{PY_TYPE_INT, PY_TYPE_BOOL}, 1},
            {{PY_TYPE_DOUBLE, PY_TYPE_BOOL}, 1},
            {{PY_TYPE_STRING, PY_TYPE_BOOL}, 2},

            // 字符串转换
            {{PY_TYPE_INT, PY_TYPE_STRING}, 2},
            {{PY_TYPE_DOUBLE, PY_TYPE_STRING}, 2},
            {{PY_TYPE_BOOL, PY_TYPE_STRING}, 2}};

    // 查找转换代价
    auto key = std::make_pair(fromTypeId, toTypeId);
    auto it = conversionCosts.find(key);

    if (it != conversionCosts.end())
    {
        return it->second;
    }

    // 默认情况，无法转换
    return -1;
}

}  // namespace llvmpy