#include "Ast.h"
#include "Parser.h"

#include "ObjectType.h"
#include "TypeIDs.h"
#include "TypeOperations.h"     // Include for TypeInferencer
#include "ObjectLifecycle.h"    // Include for ObjectLifecycleManager
#include "CodeGen/PyCodeGen.h"  // <--- 添加这一行，包含 PyCodeGen 的完整定义
//#include "ObjectRuntime.h"   // Include for ObjectRuntime

#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <utility>  // For std::pair
#include <cmath>    // 添加这一行，确保可以使用modf函数
namespace llvmpy
{

//========== PyType 实现 ==========

PyType::PyType(ObjectType* objType) : objectType(objType)
{
}

bool PyType::isFunction() const
{
    // 检查 objectType 是否有效，并且其类型 ID 是 PY_TYPE_FUNC
    // 或者如果函数有更具体的类型 ID (>= PY_TYPE_FUNC_BASE)，也应视为函数
    return objectType && (objectType->getTypeId() == PY_TYPE_FUNC || objectType->getTypeId() >= PY_TYPE_FUNC_BASE);
    // 或者更简单地，如果 ObjectType 有 category() 方法:
    // return objectType && objectType->category() == ObjectType::Function;
}

bool PyType::isVoid() const
{
    return objectType && objectType->getName() == "void";
}

bool PyType::isInt() const
{
    return objectType && objectType->getName() == "int";
}

bool PyType::isDouble() const
{
    return objectType && objectType->getName() == "double";
}

bool PyType::isNumeric() const
{
    return objectType && TypeFeatureChecker::isNumeric(objectType);
}

bool PyType::isBool() const
{
    return objectType && objectType->getName() == "bool";
}

bool PyType::isString() const
{
    return objectType && objectType->getName() == "string";
}

bool PyType::isList() const
{
    return objectType && TypeFeatureChecker::isSequence(objectType) && !isString();  // 排除字符串
}

bool PyType::isDict() const
{
    return objectType && TypeFeatureChecker::isMapping(objectType);
}

bool PyType::isAny() const
{
    return objectType && objectType->getName() == "any";
}

bool PyType::isReference() const
{
    return objectType && objectType->isReference();
}

bool PyType::equals(const PyType& other) const
{
    if (!objectType || !other.objectType)
        return false;

    // 如果其中一个是Any类型，总是兼容
    if (isAny() || other.isAny())
        return true;

    // 基本类型直接比较名称
    if (!isList() && !isDict() && !other.isList() && !other.isDict())
    {
        return objectType->getName() == other.objectType->getName();
    }

    // 对于复合类型，需要比较它们的签名
    return objectType->getTypeSignature() == other.objectType->getTypeSignature();
}

// PyType工厂方法
std::shared_ptr<PyType> PyType::getVoid()
{
    auto& registry = TypeRegistry::getInstance();
    return std::make_shared<PyType>(registry.getType("void"));
}

std::shared_ptr<PyType> PyType::getInt()
{
    auto& registry = TypeRegistry::getInstance();
    return std::make_shared<PyType>(registry.getType("int"));
}

std::shared_ptr<PyType> PyType::getDouble()
{
    auto& registry = TypeRegistry::getInstance();
    return std::make_shared<PyType>(registry.getType("double"));
}

std::shared_ptr<PyType> PyType::getBool()
{
    auto& registry = TypeRegistry::getInstance();
    return std::make_shared<PyType>(registry.getType("bool"));
}

std::shared_ptr<PyType> PyType::getString()
{
    auto& registry = TypeRegistry::getInstance();
    return std::make_shared<PyType>(registry.getType("string"));
}

std::shared_ptr<PyType> PyType::getAny()
{
    // 确保any类型只被初始化一次
    static std::shared_ptr<PyType> anyType = nullptr;

    if (!anyType)
    {
        ObjectType* objType = TypeRegistry::getInstance().getType("any");

        // 双重检查，确保objType有效
        if (!objType)
        {
            std::cerr << "警告: 'any'类型未注册，尝试获取'object'类型" << std::endl;
            objType = TypeRegistry::getInstance().getType("object");
        }

        // 如果仍然无效，创建一个硬编码的默认类型
        if (!objType)
        {
            std::cerr << "警告: 无法获取'any'或'object'类型，使用'int'类型作为备选" << std::endl;
            objType = TypeRegistry::getInstance().getType("int");

            // 最后的防线
            if (!objType)
            {
                std::cerr << "错误: 类型系统未初始化，创建硬编码的默认类型" << std::endl;

                // 创建一个new的PrimitiveType而不是在栈上创建防止函数返回后销毁
                objType = new PrimitiveType("any");
                TypeRegistry::getInstance().registerType("any", objType);
            }
        }

        anyType = std::make_shared<PyType>(objType);
    }

    return anyType;
}

std::shared_ptr<PyType> PyType::getList(const std::shared_ptr<PyType>& elemType)
{
    auto& registry = TypeRegistry::getInstance();
    return std::make_shared<PyType>(registry.getListType(elemType->getObjectType()));
}

std::shared_ptr<PyType> PyType::getDict(const std::shared_ptr<PyType>& keyType,
                                        const std::shared_ptr<PyType>& valueType)
{
    auto& registry = TypeRegistry::getInstance();
    return std::make_shared<PyType>(
            registry.getDictType(keyType->getObjectType(), valueType->getObjectType()));
}

std::shared_ptr<PyType> PyType::fromString(const std::string& typeName)
{
    auto& registry = TypeRegistry::getInstance();
    ObjectType* objType = registry.parseTypeFromString(typeName);
    if (!objType)
    {
        return getAny();  // 默认返回Any类型
    }
    return std::make_shared<PyType>(objType);
}

std::shared_ptr<PyType> PyType::fromObjectType(ObjectType* objType)
{
    if (!objType) return nullptr;
    return std::make_shared<PyType>(objType);
}

// 获取列表元素类型的静态方法
std::shared_ptr<PyType> PyType::getListElementType(const std::shared_ptr<PyType>& listType)
{
    if (!listType || !listType->isList()) return nullptr;

    const ListType* lt = dynamic_cast<const ListType*>(listType->getObjectType());
    if (lt)
    {
        return std::make_shared<PyType>(const_cast<ObjectType*>(lt->getElementType()));
    }
    return nullptr;
}

std::shared_ptr<PyType> PyType::getDictKeyType(const std::shared_ptr<PyType>& dictType)
{
    if (!dictType || !dictType->isDict()) return nullptr;

    const DictType* dt = dynamic_cast<const DictType*>(dictType->getObjectType());
    if (dt)
    {
        return std::make_shared<PyType>(const_cast<ObjectType*>(dt->getKeyType()));
    }
    return nullptr;
}

// 获取字典值类型
std::shared_ptr<PyType> PyType::getDictValueType(const std::shared_ptr<PyType>& dictType)
{
    if (!dictType || !dictType->isDict()) return nullptr;

    const DictType* dt = dynamic_cast<const DictType*>(dictType->getObjectType());
    if (dt)
    {
        return std::make_shared<PyType>(const_cast<ObjectType*>(dt->getValueType()));
    }
    return nullptr;
}

//========== 类型推断函数实现 ==========

std::shared_ptr<PyType> inferExprType(const ExprAST* expr)
{
    if (!expr) return PyType::getAny();
    return expr->getType();
}

// 改进列表元素类型推断
std::shared_ptr<PyType> inferListElementType(const std::vector<std::unique_ptr<ExprAST>>& elements)
{
    if (elements.empty())
    {
        return PyType::getAny();
    }

    // 使用频率统计找出最常见的类型
    std::unordered_map<std::string, int> typeFrequency;
    std::shared_ptr<PyType> mostCommonType = elements[0]->getType();
    int maxFreq = 1;

    for (const auto& elem : elements)
    {
        auto elemType = elem->getType();
        std::string signature = elemType->getObjectType()->getTypeSignature();
        int freq = ++typeFrequency[signature];

        if (freq > maxFreq)
        {
            maxFreq = freq;
            mostCommonType = elemType;
        }
    }

    // 如果最常见类型占比高，使用它而不是求最小公共类型
    if (maxFreq >= elements.size() * 0.75)
    {
        return mostCommonType;
    }

    // 否则使用原有逻辑找最小公共类型
    std::shared_ptr<PyType> commonType = elements[0]->getType();
    for (size_t i = 1; i < elements.size(); ++i)
    {
        commonType = getCommonType(commonType, elements[i]->getType());
    }

    return commonType;
}

std::shared_ptr<PyType> getCommonType(const std::shared_ptr<PyType>& t1, const std::shared_ptr<PyType>& t2)
{
    if (!t1 || !t2) return PyType::getAny();

    try
    {
        // 相同类型直接返回
        if (t1->equals(*t2)) return t1;

        // 如果有一个是Any类型，返回Any
        if (t1->isAny() || t2->isAny()) return PyType::getAny();

        // 数值类型转换规则
        if (t1->isNumeric() && t2->isNumeric())
        {
            if (t1->isDouble() || t2->isDouble())
            {
                return PyType::getDouble();
            }
            else
            {
                return PyType::getInt();
            }
        }

        // 对于列表，尝试找到元素的公共类型
        if (t1->isList() && t2->isList())
        {
            auto listType1 = dynamic_cast<ListType*>(t1->getObjectType());
            auto listType2 = dynamic_cast<ListType*>(t2->getObjectType());

            if (listType1 && listType2)
            {
                auto elemType1 = std::make_shared<PyType>(const_cast<ObjectType*>(listType1->getElementType()));
                auto elemType2 = std::make_shared<PyType>(const_cast<ObjectType*>(listType2->getElementType()));
                auto commonElemType = getCommonType(elemType1, elemType2);

                return PyType::getList(commonElemType);
            }
        }

        // 默认返回动态类型
        return PyType::getAny();
    }
    catch (const std::exception& e)
    {
        std::cerr << "类型推断异常: " << e.what() << std::endl;
        return PyType::getAny();  // 出错时返回Any类型
    }
}

//========== ASTFactory 实现 ==========

//========== 表达式节点方法实现 ==========

// 工厂注册模板函数
/* template<typename NodeType>
void registerNodeWithFactory(ASTKind kind) {
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<NodeType>(kind, []() {
        return std::make_unique<NodeType>();
    });
} */

NumberExprAST::NumberExprAST() : valueString("0"), determinedType(PyType::getInt())  // Default to integer 0
{
    // Note: Default constructor might need adjustment depending on factory usage
}

NumberExprAST::NumberExprAST(std::string valStr, std::shared_ptr<PyType> type)
    : valueString(std::move(valStr)), determinedType(std::move(type))
{
    // Basic validation: ensure type is not null
    if (!determinedType)
    {
        std::cerr << "Warning: NumberExprAST created with null type for value '" << valueString << "'. Defaulting to int." << std::endl;
        determinedType = PyType::getInt();  // Fallback
    }
}

std::shared_ptr<PyType> NumberExprAST::getType() const
{
    return determinedType ? determinedType : PyType::getInt();
}

std::shared_ptr<PyType> VariableExprAST::getType() const
{
    if (cachedType)
    {
        return cachedType;
    }

    // 在CodeGen中添加一个方法来获取变量类型
    // 这里只能返回一个临时类型，实际类型将在代码生成时确定
    return PyType::getAny();
}

std::shared_ptr<PyType> BinaryExprAST::getType() const
{
    if (cachedType)
    {
        return cachedType;
    }

    // 对于比较操作符，直接返回布尔值
    if (opType == TOK_LT || opType == TOK_GT || opType == TOK_LE || opType == TOK_GE || opType == TOK_EQ || opType == TOK_NEQ || opType == TOK_IS || opType == TOK_IS_NOT || opType == TOK_IN || opType == TOK_NOT_IN)
    {
        cachedType = PyType::getBool();
        return cachedType;
    }

    // 对于其他二元操作，使用类型推导器
    auto lhsType = lhs->getType();
    auto rhsType = rhs->getType();

    if (!lhsType || !rhsType)
    {
        cachedType = PyType::getAny();  // Fallback if operand types are unknown
        return cachedType;
    }

    ObjectType* lhsObjType = lhsType->getObjectType();
    ObjectType* rhsObjType = rhsType->getObjectType();

    if (!lhsObjType || !rhsObjType)
    {
        cachedType = PyType::getAny();  // Fallback if ObjectTypes are invalid
        return cachedType;
    }

    // --- 修改：直接使用 TypeInferencer ---
    // 使用 TypeInferencer 进行推导，它返回 ObjectType*
    ObjectType* resultObjType = TypeInferencer::inferBinaryOpResultType(lhsObjType, rhsObjType, opType);

    if (!resultObjType)
    {
        // 如果 TypeInferencer 推断失败，返回 Any
        cachedType = PyType::getAny();
        std::cerr << "Warning [BinaryExprAST::getType]: Type inference failed for binary operator "
                  << opType << ". Defaulting to Any." << std::endl;
    }
    else
    {
        // 从推断出的 ObjectType 创建 PyType
        cachedType = PyType::fromObjectType(resultObjType);
        if (!cachedType)
        {  // Double check in case fromObjectType fails
            cachedType = PyType::getAny();
            std::cerr << "Warning [BinaryExprAST::getType]: Failed to create PyType from inferred ObjectType. Defaulting to Any." << std::endl;
        }
    }
    // --- 结束修改 ---

    return cachedType;
}

std::shared_ptr<PyType> StringExprAST::getType() const
{
    return PyType::getString();
}

std::shared_ptr<PyType> BoolExprAST::getType() const
{
    return PyType::getBool();
}

std::shared_ptr<PyType> NoneExprAST::getType() const
{
    return PyType::getVoid();
}

std::shared_ptr<PyType> UnaryExprAST::getType() const
{
    if (cachedType)
    {
        return cachedType;
    }

    auto operandType = operand->getType();
    if (!operandType)
    {
        cachedType = PyType::getAny();  // Fallback if operand type unknown
        return cachedType;
    }

    // Use TypeInferencer for consistency
    ObjectType* resultObjType = TypeInferencer::inferUnaryOpResultType(operandType->getObjectType(), opType);
    if (!resultObjType)
    {
        cachedType = PyType::getAny();  // Fallback if inference fails
        return cachedType;
    }

    cachedType = PyType::fromObjectType(resultObjType);
    return cachedType;
}

// getType: Infers the dictionary type based on key/value pairs
std::shared_ptr<PyType> DictExprAST::getType() const
{
    if (cachedType)
    {
        return cachedType;
    }

    if (pairs.empty())
    {
        // Empty dictionary defaults to dict[any, any]
        cachedType = PyType::getDict(PyType::getAny(), PyType::getAny());
        return cachedType;
    }

    std::shared_ptr<PyType> commonKeyType = nullptr;
    std::shared_ptr<PyType> commonValueType = nullptr;

    for (const auto& pair : pairs)
    {
        if (!pair.first || !pair.second)
        {
            // Should not happen with a valid parser, but handle defensively
            commonKeyType = PyType::getAny();
            commonValueType = PyType::getAny();
            std::cerr << "Warning: Invalid key or value expression in DictExprAST." << std::endl;
            break;
        }
        auto keyType = pair.first->getType();
        auto valueType = pair.second->getType();

        if (!keyType || !valueType)
        {  // If any expression fails type inference
            commonKeyType = PyType::getAny();
            commonValueType = PyType::getAny();
            std::cerr << "Warning: Could not infer type for key or value in DictExprAST." << std::endl;
            break;
        }

        if (!commonKeyType)
        {  // First element
            commonKeyType = keyType;
        }
        else if (!commonKeyType->isAny())
        {
            commonKeyType = getCommonType(commonKeyType, keyType);
            if (!commonKeyType)
            {                                      // If getCommonType returns nullptr
                commonKeyType = PyType::getAny();  // Fallback if no common type
                std::cerr << "Warning: Incompatible key types found in DictExprAST, defaulting to Any." << std::endl;
            }
        }

        if (!commonValueType)
        {  // First element
            commonValueType = valueType;
        }
        else if (!commonValueType->isAny())
        {
            commonValueType = getCommonType(commonValueType, valueType);
            if (!commonValueType)
            {                                        // If getCommonType returns nullptr
                commonValueType = PyType::getAny();  // Fallback if no common type
                std::cerr << "Warning: Incompatible value types found in DictExprAST, defaulting to Any." << std::endl;
            }
        }

        // If we already defaulted to Any, no need to check further
        if (commonKeyType->isAny() && commonValueType->isAny())
        {
            break;
        }
    }
    // Ensure we have valid types, default to Any if inference failed during loop init
    if (!commonKeyType) commonKeyType = PyType::getAny();
    if (!commonValueType) commonValueType = PyType::getAny();

    cachedType = PyType::getDict(commonKeyType, commonValueType);
    return cachedType;
}

std::shared_ptr<PyType> CallExprAST::getType() const
{
    if (cachedType)
    {
        return cachedType;
    }

    // 实际类型应由代码生成器确定
    // 这里只能返回一个临时类型
    return PyType::getAny();
}

std::shared_ptr<PyType> ListExprAST::getType() const
{
    if (cachedType)
    {
        return cachedType;
    }

    // 推断列表元素类型
    auto elemType = inferListElementType(elements);

    // 创建列表类型
    cachedType = PyType::getList(elemType);
    return cachedType;
}

std::shared_ptr<PyType> IndexExprAST::getType() const
{
    if (cachedType)
        return cachedType;

    auto targetTypePtr = target->getType();
    auto indexTypePtr = index->getType();  // 获取索引表达式的类型

    if (!targetTypePtr || !indexTypePtr)
    {
        // 如果目标或索引类型无法推断，则返回 Any
        cachedType = PyType::getAny();
        std::cerr << "Warning: Could not infer type for target or index in IndexExprAST." << std::endl;
        return cachedType;
    }

    ObjectType* targetObjType = targetTypePtr->getObjectType();
    ObjectType* indexObjType = indexTypePtr->getObjectType();  // 获取索引的 ObjectType

    if (!targetObjType || !indexObjType)
    {
        // 如果无法获取 ObjectType，则返回 Any
        cachedType = PyType::getAny();
        std::cerr << "Warning: Could not get ObjectType for target or index in IndexExprAST." << std::endl;
        return cachedType;
    }

    // 使用 TypeInferencer 来推断索引操作的结果类型
    ObjectType* resultObjType = TypeInferencer::inferIndexOpResultType(targetObjType, indexObjType);
    if (!resultObjType)
    {
        // 如果 TypeInferencer 推断失败，则返回 Any
        cachedType = PyType::getAny();
        std::cerr << "Warning: Type inference failed for index operation in IndexExprAST." << std::endl;
        return cachedType;
    }

    // 从推断出的 ObjectType 创建 PyType
    cachedType = PyType::fromObjectType(resultObjType);
    return cachedType;
}

//========== 语句节点方法实现 ==========
//被base/codegen接管
/* void WhileStmtAST::beginScope(PyCodeGen& codegen)
{
    codegen.pushScope();
}

void WhileStmtAST::endScope(PyCodeGen& codegen)
{
    codegen.popScope();
} */

//========== 类和模块方法实现 ==========

std::shared_ptr<PyType> FunctionAST::inferReturnType() const
{
    // 1. 优先使用显式声明的返回类型
    if (!returnTypeName.empty())
    {
        TypeRegistry::getInstance().ensureBasicTypesRegistered();
        ObjectType* declaredType = TypeRegistry::getInstance().getType(returnTypeName);
        if (declaredType)
        {
            return std::make_shared<PyType>(declaredType);
        }
    }

    // 2. 构建参数类型映射表，用于检测参数透传
    std::unordered_map<std::string, ObjectType*> paramTypeMap;
    for (const auto& param : params)
    {
        ObjectType* paramType = nullptr;
        if (!param.typeName.empty())
        {
            paramType = TypeRegistry::getInstance().getType(param.typeName);
        }
        // 尝试从符号表获取类型信息
        if (!paramType)
        {
            paramType = TypeRegistry::getInstance().getSymbolType(param.name);
        }
        // 如果还是没有，使用any类型
        if (!paramType)
        {
            paramType = TypeRegistry::getInstance().getType("any");
        }

        paramTypeMap[param.name] = paramType;
    }

    // 3. 分析所有return语句
    for (const auto& stmt : body)
    {
        if (auto retStmt = dynamic_cast<const ReturnStmtAST*>(stmt.get()))
        {
            const ExprAST* retExpr = retStmt->getValue();

            // 3.1 处理无返回值的情况
            if (!retExpr)
            {
                return std::make_shared<PyType>(TypeRegistry::getInstance().getType("none"));
            }

            // 3.2 检查是否是直接返回参数的情况（如 return a）
            if (auto varExpr = dynamic_cast<const VariableExprAST*>(retExpr))
            {
                const std::string& varName = varExpr->getName();
                auto paramIt = paramTypeMap.find(varName);

                if (paramIt != paramTypeMap.end())
                {
                    // 这是直接参数透传的情况，优先保留参数类型
                    ObjectType* paramType = paramIt->second;

                    // 检查参数是否是容器类型
                    int typeId = paramType->getTypeId();
                    if (typeId == PY_TYPE_LIST || (typeId >= PY_TYPE_LIST_BASE && typeId < PY_TYPE_DICT_BASE) || TypeFeatureChecker::hasFeature(paramType, "container") || TypeFeatureChecker::hasFeature(paramType, "sequence"))
                    {
                        // 对于容器类型，保留其完整类型信息
                        return std::make_shared<PyType>(paramType);
                    }

                    // 对于浮点数等，也保留原始类型
                    if (typeId == PY_TYPE_DOUBLE || typeId == PY_TYPE_INT)
                    {
                        return std::make_shared<PyType>(paramType);
                    }

                    // 对于其他参数类型，也直接返回参数类型
                    return std::make_shared<PyType>(paramType);
                }

                // 检查是否是局部变量
                ObjectType* varType = TypeRegistry::getInstance().getSymbolType(varName);
                if (varType)
                {
                    return std::make_shared<PyType>(varType);
                }
            }

            // 3.3 检查是否是特定类型表达式
            if (dynamic_cast<const ListExprAST*>(retExpr))
            {
                // 是列表字面量
                return PyType::getList(PyType::getAny());
            }

            // 3.4 如果是函数调用，尝试获取函数返回类型
            if (auto callExprAst = dynamic_cast<const CallExprAST*>(retExpr))  // 使用 callExprAst 避免命名冲突
            {
                const ExprAST* calleeNode = callExprAst->getCalleeExpr();  // 修改: getCallee() -> getCalleeExpr()
                if (calleeNode)
                {
                    if (auto varCallee = dynamic_cast<const VariableExprAST*>(calleeNode))
                    {
                        // 如果被调用者是简单的变量名
                        const std::string& funcName = varCallee->getName();
                        FunctionType* funcRegistryType = TypeRegistry::getInstance().getFunctionType(funcName);
                        if (funcRegistryType)
                        {
                            ObjectType* returnObjType = const_cast<ObjectType*>(funcRegistryType->getReturnType());
                            if (returnObjType)
                            {
                                return std::make_shared<PyType>(returnObjType);
                            }
                        }
                    }
                    else
                    {
                        // 如果被调用者是更复杂的表达式 (例如 (lambda x: x)() 或 obj.method())
                        // 需要推断 calleeNode 本身的类型
                        std::shared_ptr<PyType> calleePyType = calleeNode->getType();  // 假设 getType() 已实现并有效
                        if (calleePyType && calleePyType->isFunction())
                        {
                            // 如果 calleeNode 的类型是函数类型，获取其返回类型
                            if (auto actualFuncType = dynamic_cast<const FunctionType*>(calleePyType->getObjectType()))
                            {
                                ObjectType* returnObjType = const_cast<ObjectType*>(actualFuncType->getReturnType());
                                if (returnObjType)
                                {
                                    return PyType::fromObjectType(returnObjType);
                                }
                            }
                        }
                        // 此处可能需要更高级的推断或回退逻辑
                    }
                }
            }

            // 3.5 使用表达式自身的类型
            if (retExpr->getType())
            {
                // 优先使用表达式自身类型
                auto exprType = retExpr->getType();

                // 特别处理容器类型，确保类型信息完整保留
                ObjectType* objType = exprType->getObjectType();
                if (objType)
                {
                    int typeId = objType->getTypeId();
                    if (typeId == PY_TYPE_LIST || (typeId >= PY_TYPE_LIST_BASE && typeId < PY_TYPE_DICT_BASE) || TypeFeatureChecker::hasFeature(objType, "container"))
                    {
                        return std::make_shared<PyType>(objType);
                    }
                }

                return exprType;
            }
        }
    }

    // 4. 如果无法从return语句推断，尝试通过函数名推断
    if (name.find("get_") == 0 || name.find("create_") == 0)
    {
        std::string typeName = name.substr(name.find("_") + 1);
        if (typeName == "list" || typeName == "array")
        {
            return PyType::getList(PyType::getAny());
        }
        else if (typeName == "dict" || typeName == "map")
        {
            return PyType::getDict(PyType::getAny(), PyType::getAny());
        }
        else if (typeName == "int")
        {
            return PyType::getInt();
        }
        else if (typeName == "float" || typeName == "double")
        {
            return PyType::getDouble();
        }
        else if (typeName == "bool")
        {
            return PyType::getBool();
        }
        else if (typeName == "str" || typeName == "string")
        {
            return PyType::getString();
        }
    }

    // 5. 如果所有推断方法都失败，默认使用Any类型
    return std::make_shared<PyType>(TypeRegistry::getInstance().getType("any"));
}

std::shared_ptr<PyType> FunctionAST::getReturnType() const
{
    if (cachedReturnType)
    {
        return cachedReturnType;
    }

    cachedReturnType = inferReturnType();
    return cachedReturnType;
}

/* void FunctionAST::resolveParamTypes()
{
    // 如果已经解析过参数类型，直接返回
    if (allParamsResolved)
    {
        return;
    }

    for (auto& param : params)

    {
        if (param.resolvedType)
        {
            continue;
        }
        if (!param.resolvedType)
        {
            if (!param.typeName.empty())
            {
                // 如果有类型名，尝试解析
                param.resolvedType = PyType::fromString(param.typeName);
            }

            // 如果仍然没有类型，优先使用int类型
            if (!param.resolvedType)
            {
                param.resolvedType = PyType::getInt();
            }
        }

        // 确保resolvedType不为空
        if (!param.resolvedType)
        {
            std::cerr << "警告: 无法解析参数 " << param.name << " 的类型，使用默认int类型" << std::endl;// 逆天?
            param.resolvedType = PyType::getInt();
        }
    }

    // 标记所有参数已解析
    allParamsResolved = true;
} */

void FunctionAST::resolveParamTypes()
{
    // 如果已经解析过参数类型，直接返回
    if (allParamsResolved)
    {
        return;
    }

    for (auto& param : params) // Ensure 'params' is a member that can be modified or 'param' is a reference
    {
        if (param.resolvedType) // 如果已经有解析好的类型，则跳过
        {
            continue;
        }

        if (!param.typeName.empty())
        {
            // 如果有类型注解字符串 (param.typeName)，尝试从字符串解析
            param.resolvedType = PyType::fromString(param.typeName);
            if (!param.resolvedType || (param.resolvedType->isAny() && param.typeName != "any")) {
                // 如果 fromString 返回 nullptr (表示无法识别类型名)
                // 或者返回了 Any 但原始注解不是 "any" (说明解析可能不完全符合预期)
                // 打印警告并回退到 Any
                std::cerr << "警告: 无法完全解析参数 '" << param.name
                          << "' 的类型注解 '" << param.typeName
                          << "'。将使用 'Any' 类型。" << std::endl;
                param.resolvedType = PyType::getAny();
            }
        }
        else
        {
            // 如果没有类型注解 (param.typeName 为空)，则默认设置为 Any 类型
            param.resolvedType = PyType::getAny();
        }

        // 最后的保障：如果经过上述步骤 param.resolvedType 仍然是 nullptr
        // (理论上不应该发生，因为 PyType::fromString 和 PyType::getAny 应该总能返回一个有效的 PyType)
        // 则强制设置为 Any 并打印错误。
        if (!param.resolvedType)
        {
            std::cerr << "错误: 参数 '" << param.name
                      << "' 的类型解析最终失败。强制使用 'Any' 类型。" << std::endl;
            param.resolvedType = PyType::getAny();
        }
    }

    // 标记所有参数已解析
    allParamsResolved = true;
}

// 模块

/* // 为ModuleAST添加必要的方法
void ModuleAST::addFunction(std::unique_ptr<FunctionAST> func)
{
    if (func)
    {
        functions.push_back(std::move(func));
    }
}

void ModuleAST::addStatement(std::unique_ptr<StmtAST> stmt)
{
    if (stmt) {
        statements.push_back(std::move(stmt));
    }
} */

//========== 模板方法实现 ==========

/* template <typename Derived, ASTKind K>
void StmtASTBase<Derived, K>::beginScope(PyCodeGen& codegen)
{
    // 默认实现：什么都不做
}

template <typename Derived, ASTKind K>
void StmtASTBase<Derived, K>::endScope(PyCodeGen& codegen)
{
    // 默认实现：什么都不做
} */

template <typename Derived, ASTKind K>
bool ExprASTBase<Derived, K>::isNumeric() const
{
    auto type = static_cast<const Derived*>(this)->getType();
    return type && type->isNumeric();
}

template <typename Derived, ASTKind K>
bool ExprASTBase<Derived, K>::isList() const
{
    auto type = static_cast<const Derived*>(this)->getType();
    return type && type->isList();
}

template <typename Derived, ASTKind K>
bool ExprASTBase<Derived, K>::isString() const
{
    auto type = static_cast<const Derived*>(this)->getType();
    return type && type->isString();
}

template <typename Derived, ASTKind K>
bool ExprASTBase<Derived, K>::isReference() const
{
    auto type = static_cast<const Derived*>(this)->getType();
    return type && type->isReference();
}

}  // namespace llvmpy