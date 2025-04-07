#include "CodeGen/VariableUpdateStrategy.h"
#include "CodeGen/CodeGenBase.h"
#include "CodeGen/CodeGenRuntime.h"
#include <llvm/IR/IRBuilder.h>

namespace llvmpy {

llvm::Value* StandardUpdateStrategy::updateVariable(
    CodeGenBase& codeGen,
    const std::string& name,
    llvm::Value* oldValue, 
    llvm::Value* newValue,
    ObjectType* type,
    std::shared_ptr<PyType> valueType) 
{
    auto* runtime = codeGen.getRuntimeGen();
    
    // 处理引用计数
    if (oldValue && type && TypeFeatureChecker::isReference(type)) {
        runtime->decRef(oldValue);
    }
    
    if (newValue && valueType && valueType->isReference()) {
        runtime->incRef(newValue);
    }
    
    return newValue;
}

llvm::Value* LoopVariableUpdateStrategy::updateVariable(
    CodeGenBase& codeGen,
    const std::string& name,
    llvm::Value* oldValue, 
    llvm::Value* newValue,
    ObjectType* type,
    std::shared_ptr<PyType> valueType) 
{
    auto& context = codeGen.getVariableUpdateContext();
    auto* phi = context.getLoopVariablePhi(name);
    auto* runtime = codeGen.getRuntimeGen();
    
    // 处理引用计数
    if (oldValue && type && TypeFeatureChecker::isReference(type)) {
        runtime->decRef(oldValue);
    }
    
    if (newValue && valueType && valueType->isReference()) {
        runtime->incRef(newValue);
    }
    
    // 如果是循环变量且有关联的PHI节点
    if (phi) {
        // 更新PHI节点中从当前块过来的值
        llvm::BasicBlock* currentBlock = codeGen.getBuilder().GetInsertBlock();
        if (!currentBlock->getTerminator()) {
            // 更新循环上下文中的记录
            context.updateLoopVariable(name, newValue, currentBlock);
            // 修改：返回新值而非PHI节点，这样同一块内的后续代码会使用新值
            return newValue; 
        }
        
        // 只有在当前块有终结器时才返回PHI，通常是在循环的结束处
        return phi;
    }
    
    return newValue;
}

std::unique_ptr<VariableUpdateStrategy> VariableUpdateStrategyFactory::createStrategy(
    CodeGenBase& codeGen, 
    const std::string& name) 
{
    auto& context = codeGen.getVariableUpdateContext();
    
    // 如果在循环中且变量有PHI节点，使用循环更新策略
    if (context.inLoopContext() && context.getLoopVariablePhi(name)) {
        return std::make_unique<LoopVariableUpdateStrategy>();
    }
    
    // 默认使用标准更新策略
    return std::make_unique<StandardUpdateStrategy>();
}

}  // namespace llvmpy