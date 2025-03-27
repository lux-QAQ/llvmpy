#!/bin/bash

echo "开始编译项目..."
cd build
cmake --build . || { echo "编译失败"; exit 1; }

echo "编译成功，开始处理Python文件..."
timeout 10 ./llvmpy  > debug_output.txt 2>&1 || { echo "执行超时或失败"; cat debug_output.txt; exit 1; }

echo "执行成功，输出结果:"
cat debug_output.txt