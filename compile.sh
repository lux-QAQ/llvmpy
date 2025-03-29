#!/bin/bash
export LD_PRELOAD=""
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH
# 1. 首先运行llvmpy编译器生成IR
./build/llvmpy test.py

if [ $? -ne 0 ]; then
    echo "编译Python代码失败"
    exit 1
fi

echo "正在将IR编译为可执行文件..."

# 2. 将LLVM IR编译为目标文件 - 添加PIC支持
llc -filetype=obj output.ll -o output.o -relocation-model=pic

# 3. 使用clang++而不是clang，并明确指定extern "C"处理
clang++ -O0 output.o runtime.cpp -o program -Wl,--no-demangle

echo "编译完成，生成可执行文件 'program'"
./program