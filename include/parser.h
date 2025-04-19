#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
//#include "ast.h"
#include "TypeIDs.h"
#include "ObjectType.h"
#include "TypeOperations.h"
#include "ObjectLifecycle.h"
#include <memory>
#include <map>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

namespace llvmpy
{

// 前向声明
class PyParser;
class PyCodeGen;

// ============================ 解析器注册器 ============================

// 解析器函数类型模板
template<typename ResultType>
using ParserFunc = std::function<std::unique_ptr<ResultType>(PyParser&)>;

// 表达式解析器函数类型
using PyExprParserFunc = ParserFunc<ExprAST>;

// 语句解析器函数类型
using PyStmtParserFunc = ParserFunc<StmtAST>;

// 通用解析器注册表模板
template<typename TokenT, typename ResultT>
class PyParserRegistry {
private:
    std::unordered_map<TokenT, ParserFunc<ResultT>> parsers;

public:
    // 注册解析器函数
    void registerParser(TokenT token, ParserFunc<ResultT> parser) {
        parsers[token] = std::move(parser);
    }

    // 检查是否有对应的解析器
    bool hasParser(TokenT token) const {
        return parsers.find(token) != parsers.end();
    }

    // 获取解析器函数
    ParserFunc<ResultT> getParser(TokenT token) const {
        auto it = parsers.find(token);
        if (it != parsers.end()) {
            return it->second;
        }
        return nullptr;
    }
};

// ============================ 操作符信息与类型处理 ============================

// 操作符信息结构，扩展为支持类型推导
struct PyOperatorInfo
{
    // char symbol; // 删除旧的 char 成员
    PyTokenType opType; // 新增：使用 PyTokenType 表示操作符
    int precedence;
    bool rightAssoc;

    // 增加对TypeOperations的支持
    std::function<ObjectType*(ObjectType*, ObjectType*)> typeInferFunc; // For binary ops

    // 新增：一元操作符的类型推导函数 (如果需要区分)
    std::function<ObjectType*(ObjectType*)> unaryTypeInferFunc;

    // 更新构造函数以接受 PyTokenType
    PyOperatorInfo(PyTokenType t = TOK_ERROR, int p = -1, bool r = false) // Default precedence to -1
        : opType(t), precedence(p), rightAssoc(r), typeInferFunc(nullptr), unaryTypeInferFunc(nullptr) {}

    // 更新构造函数以接受 PyTokenType 和二元类型推导函数
    PyOperatorInfo(PyTokenType t, int p, bool r,
                  std::function<ObjectType*(ObjectType*, ObjectType*)> inferFunc)
        : opType(t), precedence(p), rightAssoc(r), typeInferFunc(std::move(inferFunc)), unaryTypeInferFunc(nullptr) {}

     // 新增：构造函数以接受 PyTokenType 和一元类型推导函数 (如果需要)
     PyOperatorInfo(PyTokenType t, int p, std::function<ObjectType*(ObjectType*)> unaryInferFunc)
         : opType(t), precedence(p), rightAssoc(false), typeInferFunc(nullptr), unaryTypeInferFunc(std::move(unaryInferFunc)) {}
};




// ============================ 解析错误 ============================

// 解析错误类，增强为支持类型错误
class PyParseError : public std::runtime_error
{
private:
    int line;
    int column;
    bool _isTypeError;

public:
    PyParseError(const std::string& message, int line, int column, bool _isTypeError = false)
        : std::runtime_error(message), line(line), column(column), _isTypeError(_isTypeError) {}

    std::string formatError() const ;
    
    int getLine() const { return line; }
    int getColumn() const { return column; }
    bool isTypeError() const { return _isTypeError; }
};

// ============================ 主解析器类 ============================

class PyParser
{
public:
    PyParser(PyLexer& lexer);

    // 主解析方法
    std::unique_ptr<ModuleAST> parseModule();
    
    // 状态访问器
    PyLexer& getLexer() { return lexer; }
    const PyToken& getCurrentToken() const { return currentToken; }
    
    // 优先级爬升处理（为了处理-2**4这种）
    std::unique_ptr<ExprAST> parseExpressionPrecedence(int minPrecedence);
    // 调试辅助
    void dumpCurrentToken() const;
    bool logTypeBoolError(const std::string& message) const;
    std::unique_ptr<ExprAST> parseCallSuffix(std::unique_ptr<ExprAST> callee);
    std::unique_ptr<ExprAST> parseIndexSuffix(std::unique_ptr<ExprAST> target) ;
    // 类定义私有部分
bool expectStatementEnd(const std::string& errorMessage );
    // 类型系统集成
    TypeOperationRegistry& getTypeOpRegistry() {
        return TypeOperationRegistry::getInstance();
    }

private:
    PyLexer& lexer;
    PyToken currentToken;
    //PyOperatorPrecedence opPrecedence;

    // 解析器注册表
    static PyParserRegistry<PyTokenType, ExprAST> exprRegistry;
    static PyParserRegistry<PyTokenType, StmtAST> stmtRegistry;
    static std::unordered_map<PyTokenType, PyOperatorInfo> operatorRegistry;

    // 保存解析状态，用于尝试性解析和回溯
    struct PyParserState
    {
        PyToken token;
        size_t lexerPosition;

        PyParserState() : token(TOK_EOF, "", 0, 0), lexerPosition(0) {}
        PyParserState(PyToken t, size_t pos) : token(t), lexerPosition(pos) {}
    };

    // 注册表初始化
    static void initializeRegistries();
    static void registerExprParser(PyTokenType type, PyExprParserFunc parser);
    static void registerStmtParser(PyTokenType type, PyStmtParserFunc parser);
    static void registerOperator(PyTokenType type, int precedence, bool rightAssoc = false);
    static bool isInitialized;

    // 错误处理
    template<typename T>
    std::unique_ptr<T> logParseError(const std::string& message) const {
        throw PyParseError(message, currentToken.line, currentToken.column);
        return nullptr;
    }
    
    // 类型错误处理
    template<typename T>
    std::unique_ptr<T> logTypeError(const std::string& message) const {
        throw PyParseError(message, currentToken.line, currentToken.column, true);
        return nullptr;
    }

    // Token处理
    void nextToken();
    bool expectToken(PyTokenType type, const std::string& errorMessage);
    bool match(PyTokenType type);
    void skipNewlines();

    // 状态管理
    PyParserState saveState() const;
    void restoreState(const PyParserState& state);

    // 表达式解析
    std::unique_ptr<ExprAST> parseNumberExpr();
    std::unique_ptr<ExprAST> parseIdentifierExpr();
    std::unique_ptr<ExprAST> parseParenExpr();
    std::unique_ptr<ExprAST> parseStringExpr();
    std::unique_ptr<ExprAST> parseBoolExpr();
    std::unique_ptr<ExprAST> parseNoneExpr();
    std::unique_ptr<ExprAST> parsePrimary();
    std::unique_ptr<ExprAST> parseBinOpRHS(int exprPrec, std::unique_ptr<ExprAST> LHS);
    std::unique_ptr<ExprAST> parseExpression();
    //std::unique_ptr<ExprAST> parseUnaryExpr();
    std::unique_ptr<ExprAST> parseListExpr();
    std::unique_ptr<ExprAST> parseIndexExpr(std::unique_ptr<ExprAST> target);
    std::unique_ptr<ExprAST> parseDictExpr(); 

    // 新增：类型检查辅助方法
    bool validateBinaryOp(PyTokenType opType, const std::shared_ptr<PyType>& leftType,
        const std::shared_ptr<PyType>& rightType);
        bool validateUnaryOp(PyTokenType opType, const std::shared_ptr<PyType>& operandType);
    bool validateIndexOperation(const std::shared_ptr<PyType>& targetType, 
                               const std::shared_ptr<PyType>& indexType);
    
    // 生命周期标记辅助方法
    void markExpressionAsCopy(ExprAST* expr);
    void markExpressionAsReference(ExprAST* expr);
    ObjectLifecycleManager::ObjectSource determineExprSource(const ExprAST* expr);

    // 模板化表达式解析助手
    template<typename T, typename... Args>
    std::unique_ptr<ExprAST> makeExpr(Args&&... args) {
        auto expr = std::make_unique<T>(std::forward<Args>(args)...);
        expr->setLocation(currentToken.line, currentToken.column);
        return expr;
    }

    // 语句解析
    std::unique_ptr<StmtAST> parseStatement();
    std::unique_ptr<StmtAST> parseExpressionStmt();
    std::unique_ptr<StmtAST> parseReturnStmt();
    std::unique_ptr<StmtAST> parseIfStmt();
    //std::unique_ptr<StmtAST> parseElifPart();
    std::unique_ptr<StmtAST> parseElsePart(std::vector<std::unique_ptr<StmtAST>>& elseBody);
    std::unique_ptr<StmtAST> parsePrintStmt();
    std::unique_ptr<StmtAST> parseAssignStmt();
    std::unique_ptr<StmtAST> parseWhileStmt();
    std::unique_ptr<StmtAST> parseForStmt();
    std::unique_ptr<StmtAST> parseImportStmt();
    std::unique_ptr<StmtAST> parsePassStmt();
    std::unique_ptr<StmtAST> parseAssignStmt(const std::string& varName);

    // 新增：赋值操作类型检查
    bool validateAssignment(const std::string& varName, const ExprAST* valueExpr);
    bool validateListAssignment(const std::shared_ptr<PyType>& listElemType, const ExprAST* valueExpr);

    // 模板化语句解析助手
    template<typename T, typename... Args>
    std::unique_ptr<StmtAST> makeStmt(Args&&... args) {
        auto stmt = std::make_unique<T>(std::forward<Args>(args)...);
        stmt->setLocation(currentToken.line, currentToken.column);
        return stmt;
    }

    // 块解析 - 处理缩进的代码块
    std::vector<std::unique_ptr<StmtAST>> parseBlock();

    // 函数定义解析
    std::unique_ptr<FunctionAST> parseFunction();
    std::vector<ParamAST> parseParameters();
    std::string parseReturnTypeAnnotation();
    
    // 类定义解析
    std::unique_ptr<StmtAST> parseClassDefinition();
    
    // 类型注解解析，增强为支持TypeIDs.h
    std::shared_ptr<PyType> parseTypeAnnotation();
    
    // 通用解析辅助函数
    template<typename T>
    std::vector<T> parseDelimitedList(PyTokenType start, PyTokenType end, PyTokenType separator,
                                     std::function<T()> parseElement);
                                     
    // 解析带类型注解的表达式
    std::shared_ptr<PyType> tryParseTypeAnnotation();
};

// ============================ 类型解析器 ============================

// 类型解析器 - 用于从字符串解析类型表达式，增强为与TypeIDs.h集成
class PyTypeParser {
public:
    // 从类型注解字符串解析类型
    static std::shared_ptr<PyType> parseType(const std::string& typeStr);
    
    // 从标识符解析基本类型
    static std::shared_ptr<PyType> parsePrimitiveType(const std::string& name);
    
    // 解析列表类型: list[T]
    static std::shared_ptr<PyType> parseListType(const std::string& typeStr);
    
    // 解析字典类型: dict[K, V]
    static std::shared_ptr<PyType> parseDictType(const std::string& typeStr);
    
    // 新增：将类型转换为运行时类型ID
    static int getTypeId(const std::shared_ptr<PyType>& type) {
        if (!type) return PY_TYPE_NONE;
        
        ObjectType* objType = type->getObjectType();
        if (!objType) return PY_TYPE_NONE;
        
        // 使用mapToRuntimeTypeId确保返回正确的运行时类型ID
        return mapToRuntimeTypeId(objType->getTypeId());
    }
    
    // 新增：从类型名称获取类型ID
    static int getTypeIdFromName(const std::string& typeName) {
        if (typeName == "int") return PY_TYPE_INT;
        if (typeName == "float" || typeName == "double") return PY_TYPE_DOUBLE;
        if (typeName == "bool") return PY_TYPE_BOOL;
        if (typeName == "str" || typeName == "string") return PY_TYPE_STRING;
        if (typeName == "list") return PY_TYPE_LIST;
        if (typeName == "dict") return PY_TYPE_DICT;
        if (typeName == "None" || typeName == "none") return PY_TYPE_NONE;
        
        // 未知类型
        return PY_TYPE_NONE;
    }
};

// 新增：表达式类型推导辅助类，与TypeOperations集成
class ExpressionTypeInferer {
public:
    // 推导二元操作表达式类型
    static std::shared_ptr<PyType> inferBinaryExprType(PyTokenType opType, // 使用 PyTokenType
        const std::shared_ptr<PyType>& leftType,
        const std::shared_ptr<PyType>& rightType) {
        if (!leftType || !rightType) return PyType::getAny();
        
        ObjectType* leftObjType = leftType->getObjectType();
        ObjectType* rightObjType = rightType->getObjectType();
        
        if (!leftObjType || !rightObjType) return PyType::getAny();
        
        // 使用TypeOperations中的类型推导器
        ObjectType* resultType = TypeInferencer::inferBinaryOpResultType(
            leftObjType, rightObjType, opType); // 传递 opType

        if (!resultType) return PyType::getAny();

        return std::make_shared<PyType>(resultType);
    }
    
    // 推导一元操作表达式类型
    static std::shared_ptr<PyType> inferUnaryExprType(PyTokenType opType, // 使用 PyTokenType
        const std::shared_ptr<PyType>& operandType){
        if (!operandType) return PyType::getAny();
        
        ObjectType* objType = operandType->getObjectType();
        if (!objType) return PyType::getAny();
        
        // 使用TypeOperations中的类型推导器
        ObjectType* resultType = TypeInferencer::inferUnaryOpResultType(objType, opType);
        
        if (!resultType) return PyType::getAny();
        
        return std::make_shared<PyType>(resultType);
    }
    
    // 推导索引表达式类型
    static std::shared_ptr<PyType> inferIndexExprType(const std::shared_ptr<PyType>& targetType) {
        if (!targetType) return PyType::getAny();
        
        // 对于列表类型，返回元素类型
        if (targetType->isList()) {
            const ListType* listType = dynamic_cast<const ListType*>(targetType->getObjectType());
            if (listType) {
                return std::make_shared<PyType>(const_cast<ObjectType*>(listType->getElementType()));
            }
        }
        
        // 对于字典类型，返回值类型
        if (targetType->isDict()) {
            const DictType* dictType = dynamic_cast<const DictType*>(targetType->getObjectType());
            if (dictType) {
                return std::make_shared<PyType>(const_cast<ObjectType*>(dictType->getValueType()));
            }
        }
        
        // 对于字符串类型，索引返回字符串
        if (targetType->isString()) {
            return PyType::getString();
        }
        
        return PyType::getAny();
    }
};

}  // namespace llvmpy

#endif  // PARSER_H