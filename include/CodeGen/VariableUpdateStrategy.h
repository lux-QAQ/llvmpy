#ifndef VARIABLE_UPDATE_STRATEGY_H
#define VARIABLE_UPDATE_STRATEGY_H

#include <string>
#include <memory>
#include <llvm/IR/Value.h>
#include "ObjectType.h"
#include "Ast.h"

namespace llvmpy {

class CodeGenBase;

class VariableUpdateStrategy {
public:
    virtual ~VariableUpdateStrategy() = default;
    
    // 更新变量，返回更新后的值
    virtual llvm::Value* updateVariable(
        CodeGenBase& codeGen,
        const std::string& name,
        llvm::Value* oldValue, 
        llvm::Value* newValue,
        ObjectType* type,
        std::shared_ptr<PyType> valueType) = 0;
};

// 普通变量更新策略
class StandardUpdateStrategy : public VariableUpdateStrategy {
public:
    llvm::Value* updateVariable(
        CodeGenBase& codeGen,
        const std::string& name,
        llvm::Value* oldValue, 
        llvm::Value* newValue,
        ObjectType* type,
        std::shared_ptr<PyType> valueType) override;
};

// 循环变量更新策略
class LoopVariableUpdateStrategy : public VariableUpdateStrategy {
public:
    llvm::Value* updateVariable(
        CodeGenBase& codeGen,
        const std::string& name,
        llvm::Value* oldValue, 
        llvm::Value* newValue,
        ObjectType* type,
        std::shared_ptr<PyType> valueType) override;
};

// 策略工厂
class VariableUpdateStrategyFactory {
public:
    static std::unique_ptr<VariableUpdateStrategy> createStrategy(
        CodeGenBase& codeGen, 
        const std::string& name);
};

}  // namespace llvmpy

#endif  // VARIABLE_UPDATE_STRATEGY_H