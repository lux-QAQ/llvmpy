#ifndef CODEGEN_EXPR_H
#define CODEGEN_EXPR_H

#include "CodeGen/CodeGenBase.h"
//#include "ast.h"
#include "ObjectType.h"
#include "TypeOperations.h"
#include <unordered_map>
#include <memory>
#include <functional>

namespace llvmpy
{

// 表达式处理器类型
using ExprHandlerFunc = std::function<llvm::Value*(CodeGenBase&, ExprAST*)>;

// 表达式代码生成组件
class CodeGenExpr
{
private:
    CodeGenBase& codeGen;

    // 表达式处理器注册表
    static std::unordered_map<ASTKind, ExprHandlerFunc> exprHandlers;
    static bool handlersInitialized;

    // 处理数字表达式
    llvm::Value* handleNumberExpr(NumberExprAST* expr);

    // 处理字符串表达式
    llvm::Value* handleStringExpr(StringExprAST* expr);

    // 处理布尔表达式
    llvm::Value* handleBoolExpr(BoolExprAST* expr);

    // 处理None表达式
    llvm::Value* handleNoneExpr(NoneExprAST* expr);

    // 处理变量引用表达式
    llvm::Value* handleVariableExpr(VariableExprAST* expr);

    // 处理二元操作表达式
    llvm::Value* handleBinaryExpr(BinaryExprAST* expr);

    // 处理一元操作表达式
    llvm::Value* handleUnaryExpr(UnaryExprAST* expr);

    // 处理函数调用表达式
    llvm::Value* handleCallExpr(CallExprAST* expr);

    // 处理列表表达式
    llvm::Value* handleListExpr(ListExprAST* expr);

    // 处理索引表达式
    llvm::Value* handleIndexExpr(IndexExprAST* expr);

    // 初始化表达式处理器
    static void initializeHandlers();

public:
    CodeGenExpr(CodeGenBase& cg) : codeGen(cg)
    {
        if (!handlersInitialized)
        {
            initializeHandlers();
        }
    }

    // 通用表达式处理入口
    llvm::Value* handleExpr(const ExprAST* expr);

    // 二元操作处理 - 使用TypeOperations
    llvm::Value* handleBinOp(PyTokenType op,
                             llvm::Value* L, llvm::Value* R,
                             std::shared_ptr<PyType> leftType,
                             std::shared_ptr<PyType> rightType);

    // 一元操作处理 - 使用TypeOperations
    llvm::Value* handleUnaryOp(PyTokenType op,
                               llvm::Value* operand,
                               std::shared_ptr<PyType> operandType);

    // 索引操作处理 - 使用runtime
    llvm::Value* handleIndexOperation(llvm::Value* target, llvm::Value* index,
                                      std::shared_ptr<PyType> targetType,
                                      std::shared_ptr<PyType> indexType);
    // 处理字典表达式
    llvm::Value* handleDictExpr(DictExprAST* expr);

    // 字面量创建
    llvm::Value* createIntLiteral(int value);
    llvm::Value* createDoubleLiteral(double value);
    llvm::Value* createBoolLiteral(bool value);
    llvm::Value* createStringLiteral(const std::string& value);
    llvm::Value* createNoneLiteral();
    // 创建整数字面量
llvm::Value* createIntLiteralFromString(const std::string& value);
llvm::Value* createDoubleLiteralFromString(const std::string& value);


    // 列表操作
    llvm::Value* createList(int size, std::shared_ptr<PyType> elemType);
    llvm::Value* createListWithValues(const std::vector<llvm::Value*>& values,
                                      std::shared_ptr<PyType> elemType);
    llvm::Value* getListElement(llvm::Value* list, llvm::Value* index,
                                std::shared_ptr<PyType> listType);
    void setListElement(llvm::Value* list, llvm::Value* index,
                        llvm::Value* value, std::shared_ptr<PyType> listType);

    // 字典操作
    llvm::Value* createDict(ObjectType* keyType, ObjectType* valueType);
    llvm::Value* createDictWithPairs(
            const std::vector<std::pair<llvm::Value*, llvm::Value*>>& pairs,
            ObjectType* keyType,
            ObjectType* valueType);
    llvm::Value* getDictItem(llvm::Value* dict, llvm::Value* key,
                             std::shared_ptr<PyType> dictType);
    void setDictItem(llvm::Value* dict, llvm::Value* key, llvm::Value* value,
                     std::shared_ptr<PyType> dictType);

    // 辅助方法
    CodeGenBase& getCodeGen()
    {
        return codeGen;
    }
};

}  // namespace llvmpy

#endif  // CODEGEN_EXPR_H