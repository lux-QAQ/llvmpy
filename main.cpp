#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>

using namespace llvmpy;

std::string readFile(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "错误: 无法打开文件 " << filename << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
#include "Debugdefine.h"
int main(int argc, char** argv)
{
#ifndef DEBUG
    if (argc < 2)
    {
        std::cerr << "用法: " << argv[0] << " <filename.py> [output.ll]" << std::endl;
        return 1;
    }
    std::string inputFile = argv[1];
#endif

#ifdef DEBUG
    std::string inputFile = "/home/ljs/code/llvmpy/test.py";
#endif

    std::string outputFile = (argc > 2) ? argv[2] : "output.ll";

    // 读取源文件
    std::string sourceCode = readFile(inputFile);
    if (sourceCode.empty())
    {
        return 1;
    }

    // 词法分析
    Lexer lexer(sourceCode);

#ifdef DEBUG
#include "lexer.h"
    // 在main函数或解析前添加
    std::cerr << "\n···Main中尝试DUMP所有TOKEN···\n"
              << std::endl;

    std::cerr << "Debug: Dumping all tokens:" << std::endl;
    int idx = 0;
    Token tok = lexer.peekTokenAt(idx++);  // 正确初始化Token对象
    while (tok.type != TOK_EOF)
    {
        std::cerr << "Token #" << idx << ": '" << tok.value
                  << "' type: " << lexer.getTokenName(tok.type)
                  << " at line " << tok.line
                  << ", col " << tok.column << std::endl;
        tok = lexer.peekTokenAt(idx++);  // 获取下一个Token
    }
    std::cerr << "\n···Main中尝试DUMP所有TOKEN···\n"
              << std::endl;
#ifdef RECOVER_SOURCE_FROM_TOKENS
    // 添加以下代码来恢复源代码
    std::cout << "\n尝试从Token恢复源代码...\n"
              << std::endl;
    lexer.recoverSourceFromTokens();
    std::cout << "\n源代码恢复完成，请查看 Token_recovery.py 文件\n"
              << std::endl;
#endif
#endif

    // 语法分析
    Parser parser(lexer);
    auto module = parser.parseModule();

    if (!module)
    {
        std::cerr << "错误: 模块解析失败" << std::endl;
        return 1;
    }

    // 代码生成
    CodeGen codegen;
    codegen.generateModule(module.get());

/*     // 优化LLVM IR (可选)
    llvm::legacy::FunctionPassManager passManager(codegen.getModule());

    // 添加基本优化
    passManager.add(llvm::createInstructionCombiningPass());
    passManager.add(llvm::createReassociatePass());
    passManager.add(llvm::createGVNPass());
    passManager.add(llvm::createCFGSimplificationPass());

    passManager.doInitialization();

    // 对模块中的每个函数运行优化
    for (auto& F : *codegen.getModule())
    {
        passManager.run(F);
    } */

    // 将LLVM IR输出到文件
    std::error_code EC;
    llvm::raw_fd_ostream dest(outputFile, EC, llvm::sys::fs::OF_None);

    if (EC)
    {
        std::cerr << "错误: 无法打开输出文件: " << EC.message() << std::endl;
        return 1;
    }

    codegen.getModule()->print(dest, nullptr);
    dest.flush();

    std::cout << "成功将 " << inputFile << " 编译为 " << outputFile << std::endl;

    return 0;
}