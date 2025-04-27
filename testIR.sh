#!/bin/bash
set -e

echo "验证生成的IR..."
llvm-as output.ll -o output.bc || {
    echo "IR验证失败，详细错误:"
    cat output.ll
    exit 1
}

echo "IR验证通过，正在编译为汇编代码..."
llc output.bc -o output.s

echo "生成的汇编代码:"
cat output.s