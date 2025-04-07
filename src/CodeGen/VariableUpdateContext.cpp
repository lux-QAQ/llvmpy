#include "CodeGen/VariableUpdateContext.h"
#include "CodeGen/CodeGenBase.h"
#include <llvm/IR/Instructions.h>

namespace llvmpy {

// 这个文件实现了VariableUpdateContext类中较为复杂的方法
// 大多数简单方法已经在头文件中内联实现

// 辅助方法：从PHI节点中移除与指定块相关的传入值
void removeIncomingFromPhi(llvm::PHINode* phi, llvm::BasicBlock* block) {
    for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
        if (phi->getIncomingBlock(i) == block) {
            phi->removeIncomingValue(i, false);
            return;
        }
    }
}

// 辅助方法：为当前循环中的所有变量创建PHI节点
void VariableUpdateContext::createPhiNodesForCurrentLoop(CodeGenBase& codeGen) {
    if (loopContextStack.empty()) return;
    
    auto& builder = codeGen.getBuilder();
    auto& currentLoop = loopContextStack.back();
    auto* headerBlock = currentLoop.headerBlock;
    
    // 保存当前插入点
    llvm::BasicBlock* savedBlock = builder.GetInsertBlock();
    llvm::BasicBlock::iterator savedPoint = builder.GetInsertPoint();
    
    // 设置插入点到循环头部的开始
    builder.SetInsertPoint(headerBlock, headerBlock->begin());
    
    // 为当前作用域中的所有变量创建PHI节点
    auto& symbolTable = codeGen.getSymbolTable();
    auto currentScope = symbolTable.currentScope();
    
    if (currentScope) {
        for (auto& [name, value] : currentScope->getVariables()) {
            // 跳过已有PHI节点的变量
            if (loopVariables.find(name) != loopVariables.end()) continue;
            
            // 创建新的PHI节点
            llvm::PHINode* phi = builder.CreatePHI(value->getType(), 2, name + ".phi");
            
            // 注册为循环变量
            registerLoopVariable(name, phi);
            
            // 初始化PHI节点的入边
            phi->addIncoming(value, savedBlock);
        }
    }
    
    // 恢复原来的插入点
    builder.SetInsertPoint(savedBlock, savedPoint);
}

// 辅助方法：复制变量更新记录到嵌套循环
void VariableUpdateContext::propagateVariablesToNestedLoop() {
    if (loopContextStack.size() < 2) return;
    
    // 最后一个元素是当前循环
    auto& currentLoop = loopContextStack.back();
    // 倒数第二个元素是父循环
    auto& parentLoop = loopContextStack[loopContextStack.size() - 2];
    
    for (auto& [name, record] : parentLoop.variables) {
        if (currentLoop.variables.find(name) == currentLoop.variables.end()) {
            currentLoop.variables[name] = record;
        }
    }
}

// 更新循环变量在所有嵌套循环中的值
void VariableUpdateContext::updateVariableInAllLoops(
    const std::string& name, llvm::Value* newValue, llvm::BasicBlock* block) {
    
    // 现在可以遍历向量中的所有元素
    for (auto& loopContext : loopContextStack) {
        auto it = loopContext.variables.find(name);
        if (it != loopContext.variables.end()) {
            auto& record = it->second;
            record.lastValue = newValue;
            record.lastUpdateBlock = block;
            record.needsUpdate = true;
        }
    }
    
    // 添加到待更新列表
    if (std::find(pendingUpdates.begin(), pendingUpdates.end(), name) == pendingUpdates.end()) {
        pendingUpdates.push_back(name);
    }
}

// 合并来自嵌套循环的变量更新
void VariableUpdateContext::mergeNestedLoopUpdates() {
    if (loopContextStack.size() < 2) return;
    
    auto& innerLoop = loopContextStack.back();  // 最后一个是内层循环
    auto& outerLoop = loopContextStack[loopContextStack.size() - 2];  // 倒数第二个是外层循环
    
    // 将内层循环的变量更新传播到外层循环
    for (const auto& name : pendingUpdates) {
        auto innerIt = innerLoop.variables.find(name);
        auto outerIt = outerLoop.variables.find(name);
        
        if (innerIt != innerLoop.variables.end() && outerIt != outerLoop.variables.end()) {
            auto& innerRecord = innerIt->second;
            auto& outerRecord = outerIt->second;
            
            if (innerRecord.needsUpdate && innerRecord.lastValue) {
                outerRecord.lastValue = innerRecord.lastValue;
                outerRecord.lastUpdateBlock = innerLoop.exitBlock;
                outerRecord.needsUpdate = true;
            }
        }
    }
}

}  // namespace llvmpy