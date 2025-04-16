#include "ast.h"
#include "CodeGen/codegen.h"  // 添加这一行，引入CodeGen完整定义
#include "ObjectType.h"
#include "TypeIDs.h"
#include "TypeOperations.h"  // Include for TypeInferencer
#include "ObjectLifecycle.h" // Include for ObjectLifecycleManager
//#include "ObjectRuntime.h"   // Include for ObjectRuntime



#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <utility> // For std::pair
#include <cmath>  // 添加这一行，确保可以使用modf函数
namespace llvmpy
{

//========== PyType 实现 ==========

PyType::PyType(ObjectType* objType) : objectType(objType)
{
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

std::shared_ptr<PyType> PyType::getDictKeyType(const std::shared_ptr<PyType>& dictType) {
    if (!dictType || !dictType->isDict()) return nullptr;
    
    const DictType* dt = dynamic_cast<const DictType*>(dictType->getObjectType());
    if (dt) {
        return std::make_shared<PyType>(const_cast<ObjectType*>(dt->getKeyType()));
    }
    return nullptr;
}

// 获取字典值类型
std::shared_ptr<PyType> PyType::getDictValueType(const std::shared_ptr<PyType>& dictType) {
    if (!dictType || !dictType->isDict()) return nullptr;
    
    const DictType* dt = dynamic_cast<const DictType*>(dictType->getObjectType());
    if (dt) {
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

ASTFactory& ASTFactory::getInstance()
{
    static ASTFactory instance;
    return instance;
}

std::unique_ptr<ASTNode> ASTFactory::createNode(ASTKind kind)
{
    auto it = nodeRegistry.find(kind);
    if (it != nodeRegistry.end())
    {
        return it->second();
    }
    return nullptr;
}

// 注册所有节点类型
void ASTFactory::registerAllNodes()
{
    // 表达式节点
    NumberExprAST::registerWithFactory();
    VariableExprAST::registerWithFactory();
    BinaryExprAST::registerWithFactory();
    UnaryExprAST::registerWithFactory();
    CallExprAST::registerWithFactory();
    ListExprAST::registerWithFactory();
    IndexExprAST::registerWithFactory();
    StringExprAST::registerWithFactory();
    BoolExprAST::registerWithFactory();
    NoneExprAST::registerWithFactory();
    DictExprAST::registerWithFactory();

    // 语句节点
    ExprStmtAST::registerWithFactory();
    IndexAssignStmtAST::registerWithFactory();
    ReturnStmtAST::registerWithFactory();
    IfStmtAST::registerWithFactory();
    WhileStmtAST::registerWithFactory();
    PrintStmtAST::registerWithFactory();
    AssignStmtAST::registerWithFactory();
    PassStmtAST::registerWithFactory();
    ImportStmtAST::registerWithFactory();
    ClassStmtAST::registerWithFactory();

    // 模块和函数
    FunctionAST::registerWithFactory();
    ModuleAST::registerWithFactory();
}



//========== 表达式节点方法实现 ==========

// 工厂注册模板函数
/* template<typename NodeType>
void registerNodeWithFactory(ASTKind kind) {
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<NodeType>(kind, []() {
        return std::make_unique<NodeType>();
    });
} */
template <typename NodeType>
void registerNodeWithFactory(ASTKind kind)
{
    // 对于每种类型，应该使用专门的工厂函数，
    // 而不是尝试使用默认构造函数
    NodeType::registerWithFactory();
}
// 数字表达式
void NumberExprAST::registerWithFactory()
{
    registerNodeWithFactory<NumberExprAST>(ASTKind::NumberExpr);
}

std::shared_ptr<PyType> NumberExprAST::getType() const
{
    // 对整数值和浮点值进行区分
    double intPart;
    if (::modf(value, &intPart) == 0.0)
    {  // 使用全局作用域的modf，而不是std::modf
        // 如果小数部分为0，则为整数
        return PyType::getInt();
    }
    else
    {
        // 否则为浮点数
        return PyType::getDouble();
    }
}

void NumberExprAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

// 变量引用表达式
void VariableExprAST::registerWithFactory()
{
    registerNodeWithFactory<VariableExprAST>(ASTKind::VariableExpr);
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

void VariableExprAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);

    // 缓存从代码生成器获取的类型
    cachedType = codegen.getLastExprType();
}

// 二元操作表达式
void BinaryExprAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<BinaryExprAST>(ASTKind::BinaryExpr, []()
                                               {
        // 创建一个空的二元表达式，后续解析时会设置实际参数
        return std::make_unique<BinaryExprAST>('+', 
            std::make_unique<NumberExprAST>(0), 
            std::make_unique<NumberExprAST>(0)); });
}

std::shared_ptr<PyType> BinaryExprAST::getType() const
{
    if (cachedType)
    {
        return cachedType;
    }

    // 对于比较操作符，返回布尔值
    if (op == '<' || op == '>' || op == 'l' || op == 'g' || op == 'e' || op == 'n')
    {
        cachedType = PyType::getBool();
        return cachedType;
    }

    // 对于加减乘除，返回操作数的共同类型
    auto lhsType = lhs->getType();
    auto rhsType = rhs->getType();

    // 字符串连接
    if (op == '+' && (lhsType->isString() || rhsType->isString()))
    {
        cachedType = PyType::getString();
        return cachedType;
    }

    // 列表加法
    if (op == '+' && lhsType->isList() && rhsType->isList())
    {
        cachedType = getCommonType(lhsType, rhsType);
        return cachedType;
    }

    // 数值运算
    cachedType = getCommonType(lhsType, rhsType);
    return cachedType;
}

void BinaryExprAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

// 一元操作表达式
void UnaryExprAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<UnaryExprAST>(ASTKind::UnaryExpr, []()
                                              {
        // Create a default unary expression
        return std::make_unique<UnaryExprAST>('-', std::make_unique<NumberExprAST>(0)); });
}
// StringExprAST 实现工厂函数
void StringExprAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<StringExprAST>(ASTKind::StringExpr, []()
                                               {
        // 创建一个空字符串表达式，后续解析时会设置实际值
        return std::make_unique<StringExprAST>(""); });
}

void StringExprAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

std::shared_ptr<PyType> StringExprAST::getType() const
{
    return PyType::getString();
}

// BoolExprAST 实现
void BoolExprAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<BoolExprAST>(ASTKind::BoolExpr, []()
                                             {
        // 创建一个默认为false的布尔表达式
        return std::make_unique<BoolExprAST>(false); });
}

void BoolExprAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

std::shared_ptr<PyType> BoolExprAST::getType() const
{
    return PyType::getBool();
}

// NoneExprAST 实现
void NoneExprAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<NoneExprAST>(ASTKind::NoneExpr, []()
                                             { return std::make_unique<NoneExprAST>(); });
}

void NoneExprAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

std::shared_ptr<PyType> NoneExprAST::getType() const
{
    return PyType::getVoid();
}
/* std::shared_ptr<PyType> UnaryExprAST::getType() const
{
    if (cachedType)
    {
        return cachedType;
    }

    // 布尔取反操作
    if (opCode == '!')
    {
        cachedType = PyType::getBool();
        return cachedType;
    }

    // 数值取反操作
    if (opCode == '-')
    {
        cachedType = operand->getType();
        return cachedType;
    }

    // 默认情况：保持操作数类型不变
    cachedType = operand->getType();
    return cachedType;
} */


std::shared_ptr<PyType> UnaryExprAST::getType() const
{
    if (cachedType)
    {
        return cachedType;
    }

    auto operandType = operand->getType();
    if (!operandType) {
        cachedType = PyType::getAny(); // Fallback if operand type unknown
        return cachedType;
    }

    // Use TypeInferencer for consistency
    ObjectType* resultObjType = TypeInferencer::inferUnaryOpResultType(operandType->getObjectType(), opCode);
    if (!resultObjType) {
         cachedType = PyType::getAny(); // Fallback if inference fails
         return cachedType;
    }

    cachedType = PyType::fromObjectType(resultObjType);
    return cachedType;
}

void UnaryExprAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

// CallExprAST 实现工厂函数
void CallExprAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<CallExprAST>(ASTKind::CallExpr, []()
                                             {
        // 创建一个空的函数调用表达式，后续解析时会设置实际参数
        return std::make_unique<CallExprAST>("", std::vector<std::unique_ptr<ExprAST>>()); });
}


// getType: Infers the dictionary type based on key/value pairs
std::shared_ptr<PyType> DictExprAST::getType() const
{
    if (cachedType) {
        return cachedType;
    }

    if (pairs.empty()) {
        // Empty dictionary defaults to dict[any, any]
        cachedType = PyType::getDict(PyType::getAny(), PyType::getAny());
        return cachedType;
    }

    std::shared_ptr<PyType> commonKeyType = nullptr;
    std::shared_ptr<PyType> commonValueType = nullptr;

    for (const auto& pair : pairs) {
        if (!pair.first || !pair.second) {
             // Should not happen with a valid parser, but handle defensively
             commonKeyType = PyType::getAny();
             commonValueType = PyType::getAny();
             std::cerr << "Warning: Invalid key or value expression in DictExprAST." << std::endl;
             break;
        }
        auto keyType = pair.first->getType();
        auto valueType = pair.second->getType();

        if (!keyType || !valueType) { // If any expression fails type inference
            commonKeyType = PyType::getAny();
            commonValueType = PyType::getAny();
            std::cerr << "Warning: Could not infer type for key or value in DictExprAST." << std::endl;
            break;
        }

        if (!commonKeyType) { // First element
            commonKeyType = keyType;
        } else if (!commonKeyType->isAny()) {
            commonKeyType = getCommonType(commonKeyType, keyType);
             if (!commonKeyType) { // If getCommonType returns nullptr
                 commonKeyType = PyType::getAny(); // Fallback if no common type
                 std::cerr << "Warning: Incompatible key types found in DictExprAST, defaulting to Any." << std::endl;
             }
        }

        if (!commonValueType) { // First element
            commonValueType = valueType;
        } else if (!commonValueType->isAny()) {
            commonValueType = getCommonType(commonValueType, valueType);
             if (!commonValueType) { // If getCommonType returns nullptr
                 commonValueType = PyType::getAny(); // Fallback if no common type
                 std::cerr << "Warning: Incompatible value types found in DictExprAST, defaulting to Any." << std::endl;
             }
        }

        // If we already defaulted to Any, no need to check further
        if (commonKeyType->isAny() && commonValueType->isAny()) {
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

void CallExprAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);

    // 缓存从代码生成器获取的类型
    cachedType = codegen.getLastExprType();
}

// ListExprAST 实现工厂函数
void ListExprAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<ListExprAST>(ASTKind::ListExpr, []()
                                             {
        // 创建一个空列表表达式，后续解析时会设置实际元素
        return std::make_unique<ListExprAST>(std::vector<std::unique_ptr<ExprAST>>()); });
}

// DictExprAST 实现工厂函数
void DictExprAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    // Register creator for DictExprAST
    factory.registerNodeCreator<DictExprAST>(ASTKind::DictExpr, []() {
        // Create an empty dictionary expression
        return std::make_unique<DictExprAST>();
    });
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

void ListExprAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}


void DictExprAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

// 索引表达式
// IndexExprAST 实现工厂函数
void IndexExprAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<IndexExprAST>(ASTKind::IndexExpr, []()
                                              {
        // 创建一个空的索引表达式，后续解析时会设置实际目标和索引
        return std::make_unique<IndexExprAST>(
            std::make_unique<VariableExprAST>(""),
            std::make_unique<NumberExprAST>(0)
        ); });
}

/* std::shared_ptr<PyType> IndexExprAST::getType() const
{
    if (cachedType)
        return cachedType;

    auto targetTypePtr = target->getType();
    if (!targetTypePtr)
        return PyType::getAny();

    // 尝试从符号表获取更精确的类型
    const VariableExprAST* varExpr = dynamic_cast<const VariableExprAST*>(target.get());
    if (varExpr)
    {
        std::string varName = varExpr->getName();

        // 使用TypeRegistry获取符号类型
        ObjectType* symbolType = TypeRegistry::getInstance().getSymbolType(varName);
        if (symbolType)
        {
            // 优先使用符号表中的类型
            if (symbolType->getTypeId() == PY_TYPE_LIST)
            {
                // 如果是列表类型，获取元素类型
                const ListType* listType = dynamic_cast<const ListType*>(symbolType);
                if (listType)
                {
                    cachedType = std::make_shared<PyType>(
                        const_cast<ObjectType*>(listType->getElementType()));
                    return cachedType;
                }
            }
        }
    }

    // 无法从符号表获取更精确信息，使用标准处理逻辑
    ObjectType* targetType = targetTypePtr->getObjectType();
    if (!targetType)
        return PyType::getAny();

    // 使用类型ID而不是类型名称判断
    int typeId = targetType->getTypeId();

    // 如果是列表，返回列表元素类型
    if (typeId == PY_TYPE_LIST)
    {
        // 修复这里 - 直接对targetType进行类型转换，而不是调用不存在的getObjectType()方法
        auto listType = dynamic_cast<ListType*>(targetType);
        if (listType)
        {
            cachedType = std::make_shared<PyType>(const_cast<ObjectType*>(listType->getElementType()));
            return cachedType;
        }
    }
    else if (typeId == PY_TYPE_DICT)
    {
        // 同样修复这里
        auto dictType = dynamic_cast<DictType*>(targetType);
        if (dictType)
        {
            cachedType = std::make_shared<PyType>(const_cast<ObjectType*>(dictType->getValueType()));
            return cachedType;
        }
    }
    else if (typeId == PY_TYPE_STRING)
    {
        cachedType = PyType::getString();
        return cachedType;
    }
    else
    {
        cachedType = PyType::getAny();
    }

    return cachedType;
} */
std::shared_ptr<PyType> IndexExprAST::getType() const
{
    if (cachedType)
        return cachedType;

    auto targetTypePtr = target->getType();
    auto indexTypePtr = index->getType(); // 获取索引表达式的类型

    if (!targetTypePtr || !indexTypePtr) {
        // 如果目标或索引类型无法推断，则返回 Any
        cachedType = PyType::getAny();
        std::cerr << "Warning: Could not infer type for target or index in IndexExprAST." << std::endl;
        return cachedType;
    }

    ObjectType* targetObjType = targetTypePtr->getObjectType();
    ObjectType* indexObjType = indexTypePtr->getObjectType(); // 获取索引的 ObjectType

    if (!targetObjType || !indexObjType) {
        // 如果无法获取 ObjectType，则返回 Any
        cachedType = PyType::getAny();
         std::cerr << "Warning: Could not get ObjectType for target or index in IndexExprAST." << std::endl;
        return cachedType;
    }

    // 使用 TypeInferencer 来推断索引操作的结果类型
    ObjectType* resultObjType = TypeInferencer::inferIndexOpResultType(targetObjType, indexObjType);
    if (!resultObjType) {
        // 如果 TypeInferencer 推断失败，则返回 Any
        cachedType = PyType::getAny();
        std::cerr << "Warning: Type inference failed for index operation in IndexExprAST." << std::endl;
        return cachedType;
    }

    // 从推断出的 ObjectType 创建 PyType
    cachedType = PyType::fromObjectType(resultObjType);
    return cachedType;
}
void IndexExprAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

//========== 语句节点方法实现 ==========

// ExprStmtAST 实现工厂函数
void ExprStmtAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<ExprStmtAST>(ASTKind::ExprStmt, []()
                                             {
        // 创建一个空的表达式语句，后续解析时会设置实际表达式
        return std::make_unique<ExprStmtAST>(std::make_unique<NumberExprAST>(0)); });
}

void ExprStmtAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

// 索引赋值语句
void IndexAssignStmtAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<IndexAssignStmtAST>(ASTKind::IndexAssignStmt, []()
                                                    {
        // 创建一个空的索引赋值语句，后续解析时会设置实际目标、索引和值
        return std::make_unique<IndexAssignStmtAST>(
            std::make_unique<VariableExprAST>(""),
            std::make_unique<NumberExprAST>(0),
            std::make_unique<NumberExprAST>(0)
        ); });
}

void IndexAssignStmtAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

// 返回语句
// ReturnStmtAST 实现工厂函数
void ReturnStmtAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<ReturnStmtAST>(ASTKind::ReturnStmt, []()
                                               {
        // 创建一个空的return语句，后续解析时会设置实际返回值
        return std::make_unique<ReturnStmtAST>(nullptr); });
}

void ReturnStmtAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}



// if语句
void IfStmtAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<IfStmtAST>(ASTKind::IfStmt, []()
                                           {
        // 创建一个空的if语句，后续解析时会设置实际条件和代码块
        return std::make_unique<IfStmtAST>(
            std::make_unique<BoolExprAST>(true),
            std::vector<std::unique_ptr<StmtAST>>(),
            std::vector<std::unique_ptr<StmtAST>>()
        ); });
}

void IfStmtAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

void IfStmtAST::beginScope(PyCodeGen& codegen)
{
    codegen.pushScope();
}

void IfStmtAST::endScope(PyCodeGen& codegen)
{
    codegen.popScope();
}

// while语句
void WhileStmtAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<WhileStmtAST>(ASTKind::WhileStmt, []()
                                              {
        // 创建一个空的while语句，后续解析时会设置实际条件和代码块
        return std::make_unique<WhileStmtAST>(
            std::make_unique<BoolExprAST>(true),
            std::vector<std::unique_ptr<StmtAST>>()
        ); });
}

void WhileStmtAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

void WhileStmtAST::beginScope(PyCodeGen& codegen)
{
    codegen.pushScope();
}

void WhileStmtAST::endScope(PyCodeGen& codegen)
{
    codegen.popScope();
}

// print语句
void PrintStmtAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<PrintStmtAST>(ASTKind::PrintStmt, []()
                                              {
        // 创建一个空的print语句，后续解析时会设置实际打印值
        return std::make_unique<PrintStmtAST>(std::make_unique<StringExprAST>("")); });
}

void PrintStmtAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

// 赋值语句
void AssignStmtAST::registerWithFactory()
{
    registerNodeWithFactory<AssignStmtAST>(ASTKind::AssignStmt);
}

void AssignStmtAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

// pass语句
void PassStmtAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<PassStmtAST>(ASTKind::PassStmt, []()
                                             { return std::make_unique<PassStmtAST>(); });
}

void PassStmtAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

// import语句
void ImportStmtAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<ImportStmtAST>(ASTKind::ImportStmt, []()
                                               {
        // 创建一个空的import语句，后续解析时会设置实际模块名和别名
        return std::make_unique<ImportStmtAST>("", ""); });
}

void ImportStmtAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

//========== 类和模块方法实现 ==========

// 类定义
void ClassStmtAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<ClassStmtAST>(ASTKind::ClassStmt, []()
                                              {
        // 创建一个空的类定义，后续解析时会设置实际类名、基类和成员
        return std::make_unique<ClassStmtAST>(
            "",
            std::vector<std::string>(),
            std::vector<std::unique_ptr<StmtAST>>(),
            std::vector<std::unique_ptr<FunctionAST>>()
        ); });
}

void ClassStmtAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

// 函数定义
void FunctionAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<FunctionAST>(ASTKind::Function, []()
                                             {
        // 创建一个空的函数定义，后续解析时会设置实际名称、参数和函数体
        return std::make_unique<FunctionAST>(
            "",
            std::vector<ParamAST>(),
            "",
            std::vector<std::unique_ptr<StmtAST>>()
        ); });
}

std::shared_ptr<PyType> FunctionAST::inferReturnType() const {
    // 1. 优先使用显式声明的返回类型
    if (!returnTypeName.empty()) {
        TypeRegistry::getInstance().ensureBasicTypesRegistered();
        ObjectType* declaredType = TypeRegistry::getInstance().getType(returnTypeName);
        if (declaredType) {
            return std::make_shared<PyType>(declaredType);
        }
    }
    
    // 2. 构建参数类型映射表，用于检测参数透传
    std::unordered_map<std::string, ObjectType*> paramTypeMap;
    for (const auto& param : params) {
        ObjectType* paramType = nullptr;
        if (!param.typeName.empty()) {
            paramType = TypeRegistry::getInstance().getType(param.typeName);
        }
        // 尝试从符号表获取类型信息
        if (!paramType) {
            paramType = TypeRegistry::getInstance().getSymbolType(param.name);
        }
        // 如果还是没有，使用any类型
        if (!paramType) {
            paramType = TypeRegistry::getInstance().getType("any");
        }
        
        paramTypeMap[param.name] = paramType;
    }
    
    // 3. 分析所有return语句
    for (const auto& stmt : body) {
        if (auto retStmt = dynamic_cast<const ReturnStmtAST*>(stmt.get())) {
            const ExprAST* retExpr = retStmt->getValue();
            
            // 3.1 处理无返回值的情况
            if (!retExpr) {
                return std::make_shared<PyType>(TypeRegistry::getInstance().getType("none"));
            }
            
            // 3.2 检查是否是直接返回参数的情况（如 return a）
            if (auto varExpr = dynamic_cast<const VariableExprAST*>(retExpr)) {
                const std::string& varName = varExpr->getName();
                auto paramIt = paramTypeMap.find(varName);
                
                if (paramIt != paramTypeMap.end()) {
                    // 这是直接参数透传的情况，优先保留参数类型
                    ObjectType* paramType = paramIt->second;
                    
                    // 检查参数是否是容器类型
                    int typeId = paramType->getTypeId();
                    if (typeId == PY_TYPE_LIST || 
                        (typeId >= PY_TYPE_LIST_BASE && typeId < PY_TYPE_DICT_BASE) ||
                        TypeFeatureChecker::hasFeature(paramType, "container") ||
                        TypeFeatureChecker::hasFeature(paramType, "sequence")) {
                        // 对于容器类型，保留其完整类型信息
                        return std::make_shared<PyType>(paramType);
                    }
                    
                    // 对于浮点数等，也保留原始类型
                    if (typeId == PY_TYPE_DOUBLE || typeId == PY_TYPE_INT) {
                        return std::make_shared<PyType>(paramType);
                    }
                    
                    // 对于其他参数类型，也直接返回参数类型
                    return std::make_shared<PyType>(paramType);
                }
                
                // 检查是否是局部变量
                ObjectType* varType = TypeRegistry::getInstance().getSymbolType(varName);
                if (varType) {
                    return std::make_shared<PyType>(varType);
                }
            }
            
            // 3.3 检查是否是特定类型表达式
            if (dynamic_cast<const ListExprAST*>(retExpr)) {
                // 是列表字面量
                return PyType::getList(PyType::getAny());
            }
            
            // 3.4 如果是函数调用，尝试获取函数返回类型
            if (auto callExpr = dynamic_cast<const CallExprAST*>(retExpr)) {
                FunctionType* funcType = TypeRegistry::getInstance().getFunctionType(callExpr->getCallee());
                if (funcType) {
                    ObjectType* returnType = const_cast<ObjectType*>(funcType->getReturnType());
                    if (returnType) {
                        return std::make_shared<PyType>(returnType);
                    }
                }
            }
            
            // 3.5 使用表达式自身的类型
            if (retExpr->getType()) {
                // 优先使用表达式自身类型
                auto exprType = retExpr->getType();
                
                // 特别处理容器类型，确保类型信息完整保留
                ObjectType* objType = exprType->getObjectType();
                if (objType) {
                    int typeId = objType->getTypeId();
                    if (typeId == PY_TYPE_LIST || 
                        (typeId >= PY_TYPE_LIST_BASE && typeId < PY_TYPE_DICT_BASE) ||
                        TypeFeatureChecker::hasFeature(objType, "container")) {
                        return std::make_shared<PyType>(objType);
                    }
                }
                
                return exprType;
            }
        }
    }
    
    // 4. 如果无法从return语句推断，尝试通过函数名推断
    if (name.find("get_") == 0 || name.find("create_") == 0) {
        std::string typeName = name.substr(name.find("_") + 1);
        if (typeName == "list" || typeName == "array") {
            return PyType::getList(PyType::getAny());
        } else if (typeName == "dict" || typeName == "map") {
            return PyType::getDict(PyType::getAny(), PyType::getAny());
        } else if (typeName == "int") {
            return PyType::getInt();
        } else if (typeName == "float" || typeName == "double") {
            return PyType::getDouble();
        } else if (typeName == "bool") {
            return PyType::getBool();
        } else if (typeName == "str" || typeName == "string") {
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

void FunctionAST::resolveParamTypes()
{
    // 如果已经解析过参数类型，直接返回
    if (allParamsResolved)
    {
        return;
    }

    for (auto& param : params)
    {
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
            std::cerr << "警告: 无法解析参数 " << param.name << " 的类型，使用默认int类型" << std::endl;
            param.resolvedType = PyType::getInt();
        }
    }

    // 标记所有参数已解析
    allParamsResolved = true;
}

void FunctionAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

// 模块

// 为ModuleAST添加必要的方法
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
}

void ModuleAST::registerWithFactory()
{
    auto& factory = ASTFactory::getInstance();
    factory.registerNodeCreator<ModuleAST>(ASTKind::Module, []()
                                           {
        // 创建一个空的模块，后续解析时会设置实际名称和内容
        return std::make_unique<ModuleAST>(
            "",
            std::vector<std::unique_ptr<StmtAST>>(),
            std::vector<std::unique_ptr<FunctionAST>>()
        ); });
}

void ModuleAST::accept(PyCodeGen& codegen)
{
    CodeGenVisitor visitor(codegen);
    visitor.visit(this);
}

//========== 模板方法实现 ==========

template <typename Derived, ASTKind K>
void StmtASTBase<Derived, K>::beginScope(PyCodeGen& codegen)
{
    // 默认实现：什么都不做
}

template <typename Derived, ASTKind K>
void StmtASTBase<Derived, K>::endScope(PyCodeGen& codegen)
{
    // 默认实现：什么都不做
}

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