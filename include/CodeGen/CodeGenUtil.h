#ifndef CODEGEN_UTIL_H
#define CODEGEN_UTIL_H

//#include <iostream>
#include <string>
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/raw_ostream.h>
#include "CodeGen/CodeGenBase.h" // For codeGen reference

// Helper to get string representation
inline std::string llvmObjToString(const llvm::Value* V) {
    if (!V) return "<null Value>";
    std::string S;
    llvm::raw_string_ostream OS(S);
    V->print(OS);
    return OS.str();
}
inline std::string llvmObjToString(const llvm::Type* T) {
    if (!T) return "<null Type>";
    std::string S;
    llvm::raw_string_ostream OS(S);
    T->print(OS);
    return OS.str();
}
inline std::string llvmObjToString(const llvm::BasicBlock* BB) {
    if (!BB) return "<null BasicBlock>";
    if (BB->hasName()) return BB->getName().str();
    std::string S;
    llvm::raw_string_ostream OS(S);
    BB->printAsOperand(OS, false);
    return OS.str();
}
inline std::string llvmObjToString(const llvm::Function* F) {
    if (!F) return "<null Function>";
    return F->getName().str();
}



// Add this new overload for llvm::Module
inline std::string llvmObjToString(const llvm::Module* M) {
    if (!M) return "<null Module>";
    return M->getModuleIdentifier(); // Or M->getName().str() if you prefer the LLVM name
}

inline std::string ipToString(const llvm::IRBuilderBase::InsertPoint& IP) {
    if (!IP.getBlock()) return "<invalid IP>";
    return "Block: " + llvmObjToString(IP.getBlock()) + ", Point: " + (IP.getPoint() == IP.getBlock()->end() ? "end" : llvmObjToString(&*IP.getPoint()));
}


// Debug logging macro - Requires 'codeGen' to be accessible in the scope
#define DEBUG_LOG_DETAIL(tag, msg) \
    do { \
        llvm::Function* curF = codeGen.getCurrentFunction(); \
        llvm::BasicBlock* curBB = codeGen.getBuilder().GetInsertBlock(); \
        llvm::IRBuilderBase::InsertPoint curIP = codeGen.getBuilder().saveIP(); \
        std::cerr << "[" << tag << "] " \
                  << "CurFunc: " << llvmObjToString(curF) << ", " \
                  << "CurBlock: " << llvmObjToString(curBB) << ", " \
                  << "CurIP: {" << ipToString(curIP) << "} - " \
                  << (msg) << std::endl; \
        codeGen.getBuilder().restoreIP(curIP); /* Restore IP just in case */ \
    } while(0)



#endif // CODEGEN_UTIL_H