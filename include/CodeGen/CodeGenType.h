#ifndef CODEGEN_TYPE_H
#define CODEGEN_TYPE_H

#include "CodeGen/CodeGenBase.h"
//#include "ast.h"
#include "ObjectType.h"
#include "TypeOperations.h"
#include "TypeIDs.h"
#include <llvm/IR/Type.h>
#include <memory>

namespace llvmpy
{

// 类型推导和类型检查组件
class CodeGenType
{
private:
    CodeGenBase& codeGen;

    // 类型推导缓存
    std::unordered_map<const ExprAST*, std::shared_ptr<PyType>> typeCache;

public:
    CodeGenType(CodeGenBase& cg) : codeGen(cg)
    {
    }

    // 获取模块类型
    std::shared_ptr<PyType> getModuleType(const std::string& moduleName);

    // 获取类类型
    std::shared_ptr<PyType> getClassType(const std::string& className);

    // 获取类实例类型
    std::shared_ptr<PyType> getClassInstanceType(const std::string& className);

    /**
     * @brief 推断函数调用的返回类型。
     *
     * @param callableType 被调用者的类型。
     * @param argTypes 实际参数的类型列表。
     * @return std::shared_ptr<PyType> 推断出的返回类型。目前简单返回 Any。
     */
    std::shared_ptr<PyType> inferCallReturnType(
            std::shared_ptr<PyType> callableType,
            const std::vector<std::shared_ptr<PyType>>& argTypes);
    // 尝试获取函数对象类型
    ObjectType* getFunctionObjectType(const FunctionAST* funcAST);

    // 推导表达式类型 - 使用TypeOperations
    std::shared_ptr<PyType> inferExprType(const ExprAST* expr);

    // 推导二元操作表达式类型
    std::shared_ptr<PyType> inferBinaryExprType(PyTokenType op,  // Changed: char -> PyTokenType
                                                std::shared_ptr<PyType> leftType,
                                                std::shared_ptr<PyType> rightType);

    // 推导一元操作表达式类型
    std::shared_ptr<PyType> inferUnaryExprType(PyTokenType op,  // Changed: char -> PyTokenType
                                               std::shared_ptr<PyType> operandType);

    // 推导索引表达式类型
    std::shared_ptr<PyType> inferIndexExprType(
            std::shared_ptr<PyType> targetType,
            std::shared_ptr<PyType> indexType);

    // 推导函数调用表达式类型
    std::shared_ptr<PyType> inferCallExprType(
            const std::string& funcName,
            const std::vector<std::shared_ptr<PyType>>& argTypes);

    // 推导列表元素类型
    std::shared_ptr<PyType> inferListElementType(
            const std::vector<std::unique_ptr<ExprAST>>& elements);

    // 获取两个类型的共同超类型
    std::shared_ptr<PyType> getCommonType(
            std::shared_ptr<PyType> typeA,
            std::shared_ptr<PyType> typeB);

    // 类型转换代码生成
    llvm::Value* generateTypeConversion(
            llvm::Value* value,
            std::shared_ptr<PyType> fromType,
            std::shared_ptr<PyType> toType);
    llvm::Value* generateTypeConversion(
            llvm::Value* value,
            int fromTypeId,
            int toTypeId);

    // 类型检查代码生成
    llvm::Value* generateTypeCheck(
            llvm::Value* value,
            std::shared_ptr<PyType> expectedType);

    // 类型错误代码生成
    llvm::Value* generateTypeError(
            llvm::Value* value,
            std::shared_ptr<PyType> expectedType);

    const FunctionAST* getFunctionAST(const std::string& funcName);

    std::shared_ptr<PyType> analyzeFunctionReturnType(
            const FunctionAST* func,
            const std::vector<std::shared_ptr<PyType>>& argTypes);

    std::shared_ptr<PyType> analyzeReturnExpr(
            const ExprAST* expr,
            const std::vector<std::shared_ptr<PyType>>& argTypes);

    std::shared_ptr<PyType> inferReturnTypeFromContext(
            const std::string& funcName,
            const std::vector<std::shared_ptr<PyType>>& argTypes);

    bool validateIndexOperation(
            std::shared_ptr<PyType> targetType,
            std::shared_ptr<PyType> indexType);

    // 类型验证
    bool validateExprType(
            ExprAST* expr,
            std::shared_ptr<PyType> expectedType);

    bool validateAssignment(
            const std::string& varName,
            const ExprAST* valueExpr);

    bool validateListAssignment(
            std::shared_ptr<PyType> listElemType,
            const ExprAST* valueExpr);

    // LLVM类型转换
    llvm::Type* getLLVMType(std::shared_ptr<PyType> type);

    // 辅助方法
    CodeGenBase& getCodeGen()
    {
        return codeGen;
    }

    // 类型ID处理
    int getTypeId(std::shared_ptr<PyType> type);
    std::shared_ptr<PyType> getTypeFromId(int typeId);

    // 清理类型缓存
    void clearTypeCache();
};

// 类型安全操作辅助类
class TypeSafetyChecker
{
public:
    // 检查表达式类型与期望类型是否兼容
    static bool isTypeCompatible(
            std::shared_ptr<PyType> exprType,
            std::shared_ptr<PyType> expectedType);

    // 检查索引操作的类型安全性
    static bool checkIndexOperation(
            std::shared_ptr<PyType> targetType,
            std::shared_ptr<PyType> indexType);

    // 检查赋值操作的类型安全性
    static bool checkAssignmentCompatibility(
            std::shared_ptr<PyType> targetType,
            std::shared_ptr<PyType> valueType);

    // 检查两个类型是否有关联（兼容或可转换）
    static bool areTypesRelated(
            ObjectType* typeA,
            ObjectType* typeB);

    // 使用TypeIDs.h中的函数检查运行时类型兼容性
    static bool areTypesCompatible(int typeIdA, int typeIdB);
};

}  // namespace llvmpy

#endif  // CODEGEN_TYPE_H