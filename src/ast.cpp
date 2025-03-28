#include "ast.h"
#include "codegen.h"

namespace llvmpy {





// NumberExprAST实现
void NumberExprAST::accept(CodeGen& codegen) {
    codegen.visit(this);
}

std::shared_ptr<Type> NumberExprAST::getType() const {
    return Type::getDouble(); // 数字字面量默认为double类型
}

// VariableExprAST实现
void VariableExprAST::accept(CodeGen& codegen) {
    codegen.visit(this);
}

std::shared_ptr<Type> VariableExprAST::getType() const {
    // 变量类型通常需要从符号表中查找，这里简化处理为double
    // 在更完整的实现中，应该通过符号表查询变量类型
    return Type::getDouble();
}

// BinaryExprAST实现
void BinaryExprAST::accept(CodeGen& codegen) {
    codegen.visit(this);
}

std::shared_ptr<Type> BinaryExprAST::getType() const {
    // 对于比较操作符，返回结果为布尔值(在这里用double表示)
    if (op == '<' || op == '>' || op == 'l' || op == 'g' || op == 'e' || op == 'n') {
        return Type::getDouble(); // 比较操作返回布尔值(用double表示)
    }
    
    // 其他二元操作返回与操作数相同的类型
    // 简化起见，我们总是返回double
    return Type::getDouble();
}

// CallExprAST实现
void CallExprAST::accept(CodeGen& codegen) {
    codegen.visit(this);
}

std::shared_ptr<Type> CallExprAST::getType() const {
    // 在完整实现中，应该查找函数声明以确定返回类型
    // 简化起见，我们总是返回double
    return Type::getDouble();
}

// ExprStmtAST实现
void ExprStmtAST::accept(CodeGen& codegen) {
    codegen.visit(this);
}

// ReturnStmtAST实现
void ReturnStmtAST::accept(CodeGen& codegen) {
    codegen.visit(this);
}

// IfStmtAST实现
void IfStmtAST::accept(CodeGen& codegen) {
    codegen.visit(this);
}

// PrintStmtAST实现
void PrintStmtAST::accept(CodeGen& codegen) {
    codegen.visit(this);
}

// AssignStmtAST实现
void AssignStmtAST::accept(CodeGen& codegen) {
    codegen.visit(this);
}

// FunctionAST实现
void FunctionAST::accept(CodeGen& codegen) {
    codegen.visit(this);
}

// ModuleAST实现
void ModuleAST::accept(CodeGen& codegen) {
    codegen.visit(this);
}
// WhileStmtAST实现
void WhileStmtAST::accept(CodeGen& codegen) {
    codegen.visit(this);
}

// ListExprAST实现
void ListExprAST::accept(CodeGen& codegen) {
    codegen.visit(this);
}

std::shared_ptr<Type> ListExprAST::getType() const {
    // 列表类型，实际上应该是一个复合类型
    // 但我们目前使用指针类型表示
    return Type::getString(); // 使用String类型表示指针
}

// IndexExprAST实现
void IndexExprAST::accept(CodeGen& codegen) {
    codegen.visit(this);
}
// IndexAssignStmtAST实现
void IndexAssignStmtAST::accept(CodeGen& codegen) {
    codegen.visit(this);
}
std::shared_ptr<Type> IndexExprAST::getType() const {
    // 索引操作返回的元素类型
    // 目前简化为Double
    return Type::getDouble();
}

// 添加新类型的accept方法实现 TODO
// PassStmtAST实现
void PassStmtAST::accept(CodeGen& codegen) {
    // 空操作，不做任何事
    // 实际应该是: codegen.visit(this);
}

// ImportStmtAST实现
void ImportStmtAST::accept(CodeGen& codegen) {
    // codegen.visit(this);
}

// ClassStmtAST实现
void ClassStmtAST::accept(CodeGen& codegen) {
    // codegen.visit(this);
}

// UnaryExprAST实现
void UnaryExprAST::accept(CodeGen& codegen) {
    // codegen.visit(this);
}

std::shared_ptr<Type> UnaryExprAST::getType() const {
    // 简化处理，返回与操作数相同的类型
    return operand->getType();
}

} // namespace llvmpy