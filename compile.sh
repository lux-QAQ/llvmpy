#!/bin/bash

# 1. 首先运行llvmpy编译器生成IR
./build/llvmpy test.py

if [ $? -ne 0 ]; then
    echo "编译Python代码失败"
    exit 1
fi

echo "正在将IR编译为可执行文件..."

# 2. 将LLVM IR编译为目标文件 - 添加PIC支持
llc -filetype=obj output.ll -o output.o -relocation-model=pic

# 3. 链接运行时支持函数
cat > runtime.c << EOF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// print函数实现
void print(char* str) {
    printf("%s\n", str);
}

// input函数实现
char* input() {
    char* buffer = malloc(1024);
    fgets(buffer, 1024, stdin);
    // 移除换行符
    buffer[strcspn(buffer, "\n")] = 0;
    return buffer;
}
EOF

# 3. 使用clang直接将LLVM IR和运行时代码编译为可执行文件
clang -O0 output.ll runtime.c -o program

echo "编译完成，生成可执行文件 'program'"
./program