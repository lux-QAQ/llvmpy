#ifndef VARIABLE_UPDATE_CONTEXT_H
#define VARIABLE_UPDATE_CONTEXT_H

#include <map>
#include <string>
#include <stack>
#include <memory>
#include <vector>
#include <llvm/IR/Value.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>
#include "Ast.h"
#include "ObjectType.h"

namespace llvmpy
{

// 前置声明
class CodeGenBase;

// 变量更新记录 - 用于跟踪变量在循环中的变化
struct VariableUpdateRecord
{
    std::string name;                   // 变量名
    llvm::PHINode* phi;                 // 关联的PHI节点
    llvm::Value* lastValue;             // 上次赋值
    llvm::BasicBlock* lastUpdateBlock;  // 上次更新的基本块
    bool needsUpdate;                   // 是否需要更新PHI节点

    VariableUpdateRecord(const std::string& n, llvm::PHINode* p)
        : name(n), phi(p), lastValue(nullptr), lastUpdateBlock(nullptr), needsUpdate(false)
    {
    }
        // 添加默认构造函数
        VariableUpdateRecord()
        : name(""), phi(nullptr), lastValue(nullptr), lastUpdateBlock(nullptr), needsUpdate(false)
    {
    }


};

// 循环上下文 - 支持嵌套循环
struct LoopContext
{
    llvm::BasicBlock* headerBlock;                          // 循环条件块
    llvm::BasicBlock* exitBlock;                            // 循环退出块
    std::map<std::string, VariableUpdateRecord> variables;  // 此循环中的变量记录

    LoopContext(llvm::BasicBlock* header, llvm::BasicBlock* exit)
        : headerBlock(header), exitBlock(exit)
    {
    }
};

class VariableUpdateContext
{
private:
    // 循环上下文栈 - 支持嵌套循环
    std::vector<LoopContext> loopContextStack;

    // 记录循环变量的PHI节点映射 (全局视图)
    std::map<std::string, llvm::PHINode*> loopVariables;

    // 记录需要更新的变量
    std::vector<std::string> pendingUpdates;

    // 记录每个变量的类型
    std::map<std::string, std::shared_ptr<PyType>> variableTypes;

public:
    void createPhiNodesForCurrentLoop(CodeGenBase& codeGen);
    void mergeNestedLoopUpdates();
    // 设置循环上下文
    void propagateVariablesToNestedLoop();
    void updateVariableInAllLoops(
            const std::string& name, llvm::Value* newValue, llvm::BasicBlock* block);

    void setLoopContext(llvm::BasicBlock* headerBlock, llvm::BasicBlock* exitBlock)
    {
        loopContextStack.push_back(LoopContext(headerBlock, exitBlock));
        // 如果不是首个循环，将外层循环的变量复制到新循环
        if (loopContextStack.size() > 1)
        {
            auto outerLoop = std::next(loopContextStack.begin());
            for (auto& [name, record] : outerLoop->variables)
            {
                registerLoopVariable(name, record.phi);
            }
        }
    }

    // 清除当前循环上下文
    void clearLoopContext()
    {
        if (!loopContextStack.empty())
        {
            // 应用所有挂起的更新
            applyPendingUpdates();
            loopContextStack.pop_back();
        }
    }

    // 检查是否在循环上下文中
    bool inLoopContext() const
    {
        return !loopContextStack.empty();
    }

    // 获取当前循环上下文
    const LoopContext* getCurrentLoopContext() const
    {
        if (loopContextStack.empty())
        {
            return nullptr;
        }
        return &loopContextStack.back();
    }

    // 注册循环变量
    // 注册循环变量
    void registerLoopVariable(const std::string& name, llvm::PHINode* phi)
    {
        loopVariables[name] = phi;
        if (!loopContextStack.empty())
        {
            // 使用emplace代替operator[]，避免默认构造
            loopContextStack.back().variables.emplace(
                    name, VariableUpdateRecord(name, phi));
        }
    }

    // 更新循环变量的值
    // 更新循环变量的值
    void updateLoopVariable(const std::string& name, llvm::Value* newValue, llvm::BasicBlock* block)
    {
        if (loopVariables.find(name) != loopVariables.end())
        {
            pendingUpdates.push_back(name);
            if (!loopContextStack.empty())
            {
                // 先查找是否存在该变量
                auto it = loopContextStack.back().variables.find(name);
                if (it != loopContextStack.back().variables.end())
                {
                    // 如果找到，直接更新记录
                    auto& record = it->second;
                    record.lastValue = newValue;
                    record.lastUpdateBlock = block;
                    record.needsUpdate = true;
                }
            }
        }
    }

    // 应用所有挂起的变量更新
    void applyPendingUpdates()
    {
        if (loopContextStack.empty()) return;

        auto& currentLoop = loopContextStack.back();
        for (const auto& name : pendingUpdates)
        {
            auto it = currentLoop.variables.find(name);
            if (it != currentLoop.variables.end())
            {
                auto& record = it->second;
                if (record.needsUpdate && record.phi && record.lastValue && record.lastUpdateBlock)
                {
                    // 检查这个入边是否已经存在
                    bool hasIncoming = false;
                    for (unsigned i = 0; i < record.phi->getNumIncomingValues(); ++i)
                    {
                        if (record.phi->getIncomingBlock(i) == record.lastUpdateBlock)
                        {
                            record.phi->setIncomingValue(i, record.lastValue);
                            hasIncoming = true;
                            break;
                        }
                    }

                    // 如果入边不存在，添加新的入边
                    if (!hasIncoming)
                    {
                        record.phi->addIncoming(record.lastValue, record.lastUpdateBlock);
                    }

                    record.needsUpdate = false;
                }
            }
        }

        pendingUpdates.clear();
    }

    // 获取变量的PHI节点
    llvm::PHINode* getLoopVariablePhi(const std::string& name) const
    {
        auto it = loopVariables.find(name);
        return (it != loopVariables.end()) ? it->second : nullptr;
    }

    // 获取当前循环的header块
    llvm::BasicBlock* getLoopHeader() const
    {
        if (loopContextStack.empty())
        {
            return nullptr;
        }
        return loopContextStack.back().headerBlock;
    }

    // 获取当前循环的exit块
    llvm::BasicBlock* getLoopExit() const
    {
        if (loopContextStack.empty())
        {
            return nullptr;
        }
        return loopContextStack.back().exitBlock;
    }

    // 清除所有循环变量
    void clearLoopVariables()
    {
        loopVariables.clear();
        pendingUpdates.clear();
        loopContextStack.clear();  // 使用clear代替while循环pop
    }

    // 设置变量类型
    void setVariableType(const std::string& name, std::shared_ptr<PyType> type)
    {
        variableTypes[name] = type;
    }

    // 获取变量类型
    std::shared_ptr<PyType> getVariableType(const std::string& name) const
    {
        auto it = variableTypes.find(name);
        return (it != variableTypes.end()) ? it->second : nullptr;
    }

    // 检查变量是否在任何循环中
    bool isVariableInLoop(const std::string& name) const
    {
        return loopVariables.find(name) != loopVariables.end();
    }
};

}  // namespace llvmpy

#endif  // VARIABLE_UPDATE_CONTEXT_H