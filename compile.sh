#!/bin/bash
export LD_PRELOAD=""
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH

# 0. 清理旧文件 (可选)
rm -f output.ll output.o entry.o program runtime_objs/*.o
mkdir -p runtime_objs

# 1. 首先运行llvmpy编译器生成IR
echo "运行 llvmpy 编译器..."
./build/llvmpy test.py

if [ $? -ne 0 ]; then
    echo "编译Python代码失败 (llvmpy)"
    exit 1
fi

# 2. 将LLVM IR编译为目标文件
echo "编译 LLVM IR (output.ll -> output.o)..."
clang++ -c output.ll -o output.o -O0

if [ $? -ne 0 ]; then
    echo "编译 LLVM IR 失败 (clang++)"
    exit 1
fi

# 3. 编译所有的Runtime源文件为目标文件
echo "编译 Runtime 源文件..."
RUNTIME_OBJS=""
for src_file in src/RunTime/*.cpp; do
    obj_file="runtime_objs/$(basename ${src_file%.cpp}.o)"
    echo "  Compiling $src_file -> $obj_file"
    # 使用 clang++ 编译 C++ 文件，添加 -fPIC 如果需要的话
    clang++ -c "$src_file" -o "$obj_file" -Iinclude/ -O0 -std=c++20 -fPIC
    if [ $? -ne 0 ]; then
        echo "编译 Runtime 文件 $src_file 失败"
        exit 1
    fi
    RUNTIME_OBJS="$RUNTIME_OBJS $obj_file"
done

# 4. 编译 entry.c 为目标文件
echo "编译 entry.c -> entry.o..."
# 使用 clang 编译 C 文件，添加 -fPIC 如果需要的话
clang -c src/entry.c -o entry.o -Iinclude/ -O0 -fPIC
if [ $? -ne 0 ]; then
    echo "编译 entry.c 失败"
    exit 1
fi

# 5. 链接所有目标文件 <---- 修改此步骤
echo "链接目标文件..."
# 使用 clang++ 进行最终链接，添加 -lffi
clang++ output.o entry.o $RUNTIME_OBJS -o program -O0 -lffi -lgmp -lmpfr # <-- 添加 -lffi

if [ $? -ne 0 ]; then
    echo "链接失败"
    exit 1
fi

echo "编译完成，生成可执行文件 'program'"

# 6. 运行程序
echo "运行程序..."
./program
