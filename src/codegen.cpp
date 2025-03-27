#include "codegen.h"
#include <iostream>
#include <llvm/IR/Constants.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Verifier.h>

namespace llvmpy {

CodeGen::CodeGen() {
    // 初始化LLVM上下文和模块
    context = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>("llvmpy", *context);
    builder = std::make_unique<llvm::IRBuilder<>>(*context);
    
    currentFunction = nullptr;
    currentReturnType = nullptr;
}

llvm::Value* CodeGen::logErrorV(const std::string& message) {
    std::cerr << "Error: " << message << std::endl;
    return nullptr;
}

llvm::Type* CodeGen::getTypeFromString(const std::string& typeStr) {
    if (typeStr == "int")
        return llvm::Type::getInt32Ty(*context);
    else if (typeStr == "double" || typeStr == "float")
        return llvm::Type::getDoubleTy(*context);
    else if (typeStr == "string")
        //return llvm::Type::getInt8PtrTy(*context);
        return llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0);
    else if (typeStr == "void")
        return llvm::Type::getVoidTy(*context);
    
    // 默认为double
    return llvm::Type::getDoubleTy(*context);
}

llvm::Type* CodeGen::getLLVMType(std::shared_ptr<Type> type) {
    switch (type->kind) {
        case Type::Int:
            return llvm::Type::getInt32Ty(*context);
        case Type::Double:
            return llvm::Type::getDoubleTy(*context);
        case Type::String:
            //return llvm::Type::getInt8PtrTy(*context);
            return llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0);
        case Type::Void:
            return llvm::Type::getVoidTy(*context);
        default:
            return llvm::Type::getDoubleTy(*context); // 默认
    }
}

void CodeGen::generateModule(ModuleAST* moduleAst) {
    // 处理所有函数
    for (const auto& func : moduleAst->getFunctions()) {
        codegenFunction(func.get());
    }
    
    // 处理顶层语句（需要包装在一个main函数中）
    if (!moduleAst->getTopLevelStmts().empty()) {
        // 创建main函数
        llvm::FunctionType* mainType = 
            llvm::FunctionType::get(llvm::Type::getInt32Ty(*context), false);
        
        llvm::Function* mainFunc = 
            llvm::Function::Create(mainType, llvm::Function::ExternalLinkage, "main", module.get());
        
        // 创建基本块
        llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(*context, "entry", mainFunc);
        builder->SetInsertPoint(entryBB);
        
        // 保存当前函数信息
        llvm::Function* savedFunction = currentFunction;
        llvm::Type* savedReturnType = currentReturnType;
        
        currentFunction = mainFunc;
        currentReturnType = llvm::Type::getInt32Ty(*context);
        
        // 生成所有顶层语句
        for (const auto& stmt : moduleAst->getTopLevelStmts()) {
            codegenStmt(stmt.get());
        }
        
        // 返回0
        builder->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0));
        
        // 验证函数
        llvm::verifyFunction(*mainFunc);
        
        // 恢复之前的函数信息
        currentFunction = savedFunction;
        currentReturnType = savedReturnType;
    }
}

llvm::Value* CodeGen::codegenExpr(const ExprAST* expr) {
    if (auto numExpr = dynamic_cast<const NumberExprAST*>(expr)) {
        return codegenNumberExpr(numExpr);
    } else if (auto varExpr = dynamic_cast<const VariableExprAST*>(expr)) {
        return codegenVariableExpr(varExpr);
    } else if (auto binExpr = dynamic_cast<const BinaryExprAST*>(expr)) {
        return codegenBinaryExpr(binExpr);
    } else if (auto callExpr = dynamic_cast<const CallExprAST*>(expr)) {
        return codegenCallExpr(callExpr);
    }
    
    return logErrorV("unknown expression type");
}

llvm::Value* CodeGen::codegenNumberExpr(const NumberExprAST* expr) {
    return llvm::ConstantFP::get(*context, llvm::APFloat(expr->getValue()));
}

llvm::Value* CodeGen::codegenVariableExpr(const VariableExprAST* expr) {
    llvm::Value* V = namedValues[expr->getName()];
    if (!V)
        return logErrorV("Unknown variable name: " + expr->getName());
    return builder->CreateLoad(llvm::Type::getDoubleTy(*context), V, expr->getName().c_str());
}

llvm::Value* CodeGen::codegenBinaryExpr(const BinaryExprAST* expr) {
    llvm::Value* L = codegenExpr(expr->getLHS());
    llvm::Value* R = codegenExpr(expr->getRHS());
    
    if (!L || !R)
        return nullptr;
    
    switch (expr->getOp()) {
        case '+':
            return builder->CreateFAdd(L, R, "addtmp");
        case '-':
            return builder->CreateFSub(L, R, "subtmp");
        case '*':
            return builder->CreateFMul(L, R, "multmp");
        case '/':
            return builder->CreateFDiv(L, R, "divtmp");
        case '<':
            L = builder->CreateFCmpULT(L, R, "cmptmp");
            // 转换为0.0或1.0
            return builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*context), "booltmp");
        case '>':
            L = builder->CreateFCmpUGT(L, R, "cmptmp");
            return builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*context), "booltmp");
        case 'l': // <=
            L = builder->CreateFCmpULE(L, R, "cmptmp");
            return builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*context), "booltmp");
        case 'g': // >=
            L = builder->CreateFCmpUGE(L, R, "cmptmp");
            return builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*context), "booltmp");
        case 'e': // ==
            L = builder->CreateFCmpUEQ(L, R, "cmptmp");
            return builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*context), "booltmp");
        case 'n': // !=
            L = builder->CreateFCmpUNE(L, R, "cmptmp");
            return builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*context), "booltmp");
        default:
            return logErrorV("Invalid binary operator");
    }
}

llvm::Value* CodeGen::codegenCallExpr(const CallExprAST* expr) {
    // 查找函数名
    llvm::Function* calledFunc = module->getFunction(expr->getCallee());
    if (!calledFunc)
        return logErrorV("Unknown function: " + expr->getCallee());
    
    // 验证参数数量 - 修改为更灵活的检查
    if (calledFunc->arg_size() < expr->getArgs().size())
        return logErrorV("Too many arguments in function call");
    
    std::vector<llvm::Value*> argsValues;
    for (const auto& arg : expr->getArgs()) {
        argsValues.push_back(codegenExpr(arg.get()));
        if (!argsValues.back())
            return nullptr;
    }
    
    // 为缺少的参数填充默认值(0.0)
    while (argsValues.size() < calledFunc->arg_size()) {
        argsValues.push_back(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
    }
    
    return builder->CreateCall(calledFunc, argsValues, "calltmp");
}

void CodeGen::codegenStmt(const StmtAST* stmt) {
    if (auto exprStmt = dynamic_cast<const ExprStmtAST*>(stmt)) {
        codegenExprStmt(exprStmt);
    } else if (auto returnStmt = dynamic_cast<const ReturnStmtAST*>(stmt)) {
        codegenReturnStmt(returnStmt);
    } else if (auto ifStmt = dynamic_cast<const IfStmtAST*>(stmt)) {
        codegenIfStmt(ifStmt);
    } else if (auto whileStmt = dynamic_cast<const WhileStmtAST*>(stmt)) {
        codegenWhileStmt(whileStmt);
    } else if (auto printStmt = dynamic_cast<const PrintStmtAST*>(stmt)) {
        codegenPrintStmt(printStmt);
    } else if (auto assignStmt = dynamic_cast<const AssignStmtAST*>(stmt)) {
        codegenAssignStmt(assignStmt);
    } else {
        std::cerr << "Error: unknown statement type" << std::endl;
    }
}

void CodeGen::codegenExprStmt(const ExprStmtAST* stmt) {
    codegenExpr(stmt->getExpr());
}

void CodeGen::codegenReturnStmt(const ReturnStmtAST* stmt) {
    llvm::Value* retVal = codegenExpr(stmt->getValue());
    if (retVal) {
        // 检查返回值类型是否与函数返回类型一致
        if (currentReturnType->isVoidTy()) {
            builder->CreateRetVoid();
        } else {
            // 如果需要，可以进行类型转换
            if (retVal->getType() != currentReturnType) {
                if (currentReturnType->isDoubleTy() && retVal->getType()->isIntegerTy()) {
                    retVal = builder->CreateSIToFP(retVal, currentReturnType, "cast");
                } else if (currentReturnType->isIntegerTy() && retVal->getType()->isDoubleTy()) {
                    retVal = builder->CreateFPToSI(retVal, currentReturnType, "cast");
                }
                // 可以添加其他需要的类型转换
            }
            builder->CreateRet(retVal);
        }
    }
}

void CodeGen::codegenIfStmt(const IfStmtAST* stmt) {
    llvm::Value* condValue = codegenExpr(stmt->getCondition());
    if (!condValue)
        return;

    condValue = builder->CreateFCmpONE(
        condValue, llvm::ConstantFP::get(*context, llvm::APFloat(0.0)), "ifcond");

    llvm::Function* theFunction = builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*context, "then", theFunction);
    llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(*context, "else");
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "ifcont");

    builder->CreateCondBr(condValue, thenBB, elseBB);

    // Then 块
    builder->SetInsertPoint(thenBB);
    for (const auto& thenStmt : stmt->getThenBody()) {
        codegenStmt(thenStmt.get());
    }
    bool thenHasTerminator = builder->GetInsertBlock()->getTerminator() != nullptr;
    if (!thenHasTerminator)
        builder->CreateBr(mergeBB);

    // Else 块
    elseBB->insertInto(theFunction);
    builder->SetInsertPoint(elseBB);
    for (const auto& elseStmt : stmt->getElseBody()) {
        codegenStmt(elseStmt.get());
    }
    bool elseHasTerminator = builder->GetInsertBlock()->getTerminator() != nullptr;
    if (!elseHasTerminator)
        builder->CreateBr(mergeBB);

    // 只有在必要时才创建 mergeBB
    if (!thenHasTerminator || !elseHasTerminator) {
        mergeBB->insertInto(theFunction);
        builder->SetInsertPoint(mergeBB);
    } else {
        // 否则删除mergeBB，因为两边都有终止指令
        delete mergeBB;
    }
}


llvm::Function* CodeGen::getPrintfFunction() {
    llvm::Function* func = module->getFunction("printf");
    
    if (!func) {
        // printf函数类型：int printf(char*, ...)
        std::vector<llvm::Type*> argsTypes;
        argsTypes.push_back(llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0));
        
        llvm::FunctionType* printfType = 
            llvm::FunctionType::get(llvm::Type::getInt32Ty(*context), argsTypes, true);
        
        func = llvm::Function::Create(
            printfType, llvm::Function::ExternalLinkage, "printf", module.get());
        
        func->setCallingConv(llvm::CallingConv::C);
    }
    
    return func;
}

void CodeGen::codegenPrintStmt(const PrintStmtAST* stmt) {
    llvm::Function* printfFunc = getPrintfFunction();
    
    llvm::Value* value = codegenExpr(stmt->getValue());
    if (!value)
        return;
    
    // 创建格式字符串
/*     llvm::Value* formatStr;
    if (value->getType()->isDoubleTy()) {
        formatStr = builder->CreateGlobalStringPtr("%f\n");
    } else if (value->getType()->isIntegerTy()) {
        formatStr = builder->CreateGlobalStringPtr("%d\n");
    } else {
        formatStr = builder->CreateGlobalStringPtr("%s\n");
    } */
     // 3. 替换废弃的 CreateGlobalStringPtr 方法
// 可以使用以下替代方式获取格式字符串
    // 创建格式字符串
    llvm::Value* formatStr;
    if (value->getType()->isDoubleTy()) {
        auto globalStr = builder->CreateGlobalString("%f\n", "fmt_double");
        formatStr = builder->CreatePointerCast(globalStr, 
            llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0));
    } else if (value->getType()->isIntegerTy()) {
        auto globalStr = builder->CreateGlobalString("%d\n", "fmt_int");
        formatStr = builder->CreatePointerCast(globalStr, 
            llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0));
    } else {
        auto globalStr = builder->CreateGlobalString("%s\n", "fmt_str");
        formatStr = builder->CreatePointerCast(globalStr, 
            llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0));
    }
    
    // 创建printf调用
    std::vector<llvm::Value*> args;
    args.push_back(formatStr);
    args.push_back(value);
    
    builder->CreateCall(printfFunc, args);
}

void CodeGen::codegenAssignStmt(const AssignStmtAST* stmt) {
    llvm::Value* value = codegenExpr(stmt->getValue());
    if (!value)
        return;
    
    // 检查变量是否存在
    llvm::Value* variable = namedValues[stmt->getName()];
    
    if (!variable) {
        // 创建新变量
        llvm::Function* theFunction = builder->GetInsertBlock()->getParent();
        llvm::IRBuilder<> entryBuilder(&theFunction->getEntryBlock(), 
                                       theFunction->getEntryBlock().begin());
        
        llvm::AllocaInst* alloca = entryBuilder.CreateAlloca(
            llvm::Type::getDoubleTy(*context), 0, stmt->getName().c_str());
        
        namedValues[stmt->getName()] = alloca;
        variable = alloca;
    }
    
    builder->CreateStore(value, variable);
}

llvm::Function* CodeGen::codegenFunction(const FunctionAST* func) {
    // 检查函数是否已经定义
    llvm::Function* theFunction = module->getFunction(func->getName());
    
    if (!theFunction) {
        // 确定返回类型
        llvm::Type* returnType;
        if (func->getReturnType().empty()) {
            returnType = llvm::Type::getDoubleTy(*context); // 默认返回double
        } else {
            returnType = getTypeFromString(func->getReturnType());
        }
        
        // 创建函数参数类型列表
        std::vector<llvm::Type*> argTypes;
        for (const auto& param : func->getParams()) {
            if (param.type.empty()) {
                argTypes.push_back(llvm::Type::getDoubleTy(*context)); // 默认参数类型为double
            } else {
                argTypes.push_back(getTypeFromString(param.type));
            }
        }
        
        // 创建函数类型
        llvm::FunctionType* funcType = 
            llvm::FunctionType::get(returnType, argTypes, false);
        
        // 创建函数
        theFunction = llvm::Function::Create(
            funcType, llvm::Function::ExternalLinkage, func->getName(), module.get());
        
        // 设置参数名称
        unsigned idx = 0;
        for (auto &arg : theFunction->args()) {
            arg.setName(func->getParams()[idx++].name);
        }
    }
    
    // 函数已存在或为声明，现在添加函数体
    if (!theFunction->empty()) {
        return logErrorV("Function cannot be redefined"), nullptr;
    }
    
    // 创建一个新的基本块
    llvm::BasicBlock* basicBlock = llvm::BasicBlock::Create(*context, "entry", theFunction);
    builder->SetInsertPoint(basicBlock);
    
    // 保存当前函数信息
    llvm::Function* savedFunction = currentFunction;
    llvm::Type* savedReturnType = currentReturnType;
    
    currentFunction = theFunction;
    currentReturnType = theFunction->getReturnType();
    
    // 清空变量表
    namedValues.clear();
    
    // 为参数创建变量
    for (auto &arg : theFunction->args()) {
        llvm::AllocaInst* alloca = builder->CreateAlloca(
            arg.getType(), 0, arg.getName().str().c_str());
        
        builder->CreateStore(&arg, alloca);
        namedValues[std::string(arg.getName())] = alloca;
    }
    
    // 生成函数体
    for (const auto& stmt : func->getBody()) {
        codegenStmt(stmt.get());
    }
    
    // 如果基本块没有终止指令（比如return），添加一个返回语句
    if (basicBlock->getTerminator() == nullptr) {
        if (currentReturnType->isVoidTy()) {
            builder->CreateRetVoid();
        } else if (currentReturnType->isDoubleTy()) {
            builder->CreateRet(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
        } else if (currentReturnType->isIntegerTy()) {
            builder->CreateRet(llvm::ConstantInt::get(currentReturnType, 0));
        } else {
            // 对于其他类型，创建空返回
            builder->CreateRet(llvm::Constant::getNullValue(currentReturnType));
        }
    }
    if (!builder->GetInsertBlock()->getTerminator()) {
        if (currentReturnType->isDoubleTy())
            builder->CreateRet(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
        else if (currentReturnType->isIntegerTy())
            builder->CreateRet(llvm::ConstantInt::get(currentReturnType, 0));
        else
            builder->CreateRetVoid();
    }
    
    
    // 验证函数
    llvm::verifyFunction(*theFunction);
    
    // 恢复之前的函数信息
    currentFunction = savedFunction;
    currentReturnType = savedReturnType;
    
    return theFunction;
}

void CodeGen::codegenWhileStmt(const WhileStmtAST* stmt) {
    llvm::Function* theFunction = builder->GetInsertBlock()->getParent();
    
    // 创建循环所需的基本块
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*context, "while.cond", theFunction);
    llvm::BasicBlock* loopBB = llvm::BasicBlock::Create(*context, "while.body");
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(*context, "while.end");
    
    // 跳转到条件判断块
    builder->CreateBr(condBB);
    
    // 条件判断块
    builder->SetInsertPoint(condBB);
    
    // 生成条件表达式
    llvm::Value* condValue = codegenExpr(stmt->getCondition());
    if (!condValue)
        return;
    
    // 转换条件为布尔值 (double != 0)
    condValue = builder->CreateFCmpONE(
        condValue, 
        llvm::ConstantFP::get(*context, llvm::APFloat(0.0)),
        "whilecond"
    );
    
    // 根据条件跳转
    builder->CreateCondBr(condValue, loopBB, afterBB);
    
    // 循环体块 - 使用insertInto而不是getBasicBlockList().push_back()
    loopBB->insertInto(theFunction);
    builder->SetInsertPoint(loopBB);
    
    // 生成循环体语句
    for (const auto& bodyStmt : stmt->getBody()) {
        codegenStmt(bodyStmt.get());
    }
    
    // 循环体结束后跳回条件判断块
    builder->CreateBr(condBB);
    
    // 循环结束后的代码 - 使用insertInto而不是getBasicBlockList().push_back()
    afterBB->insertInto(theFunction);
    builder->SetInsertPoint(afterBB);
}

void CodeGen::visit(WhileStmtAST* stmt) {
    codegenWhileStmt(stmt);
}


// 访问器方法实现
void CodeGen::visit(NumberExprAST* expr) {
    codegenNumberExpr(expr);
}

void CodeGen::visit(VariableExprAST* expr) {
    codegenVariableExpr(expr);
}

void CodeGen::visit(BinaryExprAST* expr) {
    codegenBinaryExpr(expr);
}

void CodeGen::visit(CallExprAST* expr) {
    codegenCallExpr(expr);
}

void CodeGen::visit(ExprStmtAST* stmt) {
    codegenExprStmt(stmt);
}

void CodeGen::visit(ReturnStmtAST* stmt) {
    codegenReturnStmt(stmt);
}

void CodeGen::visit(IfStmtAST* stmt) {
    codegenIfStmt(stmt);
}

void CodeGen::visit(PrintStmtAST* stmt) {
    codegenPrintStmt(stmt);
}

void CodeGen::visit(AssignStmtAST* stmt) {
    codegenAssignStmt(stmt);
}

void CodeGen::visit(FunctionAST* func) {
    codegenFunction(func);
}

void CodeGen::visit(ModuleAST* module) {
    generateModule(module);
}

} // namespace llvmpy