#include <iostream>
#include <fstream>
#include <sstream>

// 先包含完整代码生成器定义
#include "CodeGen/codegen.h"  // 包含统一的头文件

// 然后包含其他库
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>

// 再包含应用特定头文件
#include "Debugdefine.h"
#include "TypeIDs.h"
#include "ObjectType.h"
#include "Lexer.h"
#include "Parser.h"

#include "RunTime/runtime.h"           // 包含运行时头文件
#include "RunTime/py_type_dispatch.h"  // 包含类型分派头文件
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
    // 确保类型系统已初始化
    TypeRegistry::getInstance().ensureBasicTypesRegistered();
    TypeFeatureChecker::registerBuiltinFeatureChecks();
    // py_initialize_builtin_type_methods();
#ifndef DEBUG
    if (argc < 2)
    {
        std::cerr << "用法: " << argv[0] << " <filename.py> [output.ll]" << std::endl;
        return 1;
    }
    std::string inputFile = argv[1];
#endif

#ifdef DEBUG
    std::string inputFile;
    if (argc < 2) {
        inputFile = "/home/ljs/code/llvmpy/test.py";
        std::cout << "DEBUG Mode: No input file provided, using default: " << inputFile << std::endl;
    } else {
        inputFile = argv[1];
        std::cout << "DEBUG Mode: Using provided input file: " << inputFile << std::endl;
    }
#endif

    std::string outputFile = (argc > 2) ? argv[2] : "output.ll";

    // 读取源文件
    std::string sourceCode = readFile(inputFile);
    if (sourceCode.empty())
    {
        return 1;
    }

    // 词法分析
    PyLexer lexer(sourceCode);

#ifdef DEBUG_MAIN_TOKEN_DUMP
    // 在main函数或解析前添加
    std::cerr << "\n···Main中尝试DUMP所有TOKEN···\n"
              << std::endl;

    std::cerr << "Debug: Dumping all tokens:" << std::endl;
    int idx = 0;
    PyToken tok = lexer.peekTokenAt(idx++);  // 正确初始化Token对象
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
    PyParser parser(lexer);
    auto module = parser.parseModule();

    if (!module)
    {
        std::cerr << "错误: 模块解析失败" << std::endl;
        return 1;
    }

    // 代码生成
    PyCodeGen codegen;
    // 假设我们总是将第一个文件视为入口点
    bool isEntryPoint = true;  // 或者根据命令行参数等判断
    // TODO： 多文件编译时需要更复杂的逻辑来判断入口点
    // 在 PyCodeGen 中调用 generateModule 时传递 isEntryPoint
    // 或者直接调用 CodeGenModule 的 generateModule
    // codegen.generateModule(module.get(), isEntryPoint); // 如果 PyCodeGen::generateModule 接受标志
    auto modGen = codegen.getModuleGen();
    if (modGen)
    {
        modGen->generateModule(module.get(), isEntryPoint);
    }
    else
    {
        std::cerr << "Error: Module generator not available." << std::endl;
        return 1;
    }

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