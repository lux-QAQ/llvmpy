#!/bin/bash
export LD_PRELOAD=""
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH





# 2. 将LLVM IR编译为目标文件
echo "编译 LLVM IR (output.ll -> output.o)..."
clang++ -c output.ll -o output.o -O0 # 使用 -O0 保持一致

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
    # 使用 clang++ 编译 C++ 文件
    clang++ -c "$src_file" -o "$obj_file" -Iinclude/ -O0 -std=c++20 # 添加 C++ 标准
    if [ $? -ne 0 ]; then
        echo "编译 Runtime 文件 $src_file 失败"
        exit 1
    fi
    RUNTIME_OBJS="$RUNTIME_OBJS $obj_file"
done

# 4. 编译 entry.c 为目标文件  <---- 新增步骤
echo "编译 entry.c -> entry.o..."
# 使用 clang 编译 C 文件
clang -c src/entry.c -o entry.o -Iinclude/ -O0 # 使用 clang，并添加 include 路径
if [ $? -ne 0 ]; then
    echo "编译 entry.c 失败"
    exit 1
fi

# 5. 链接所有目标文件 <---- 修改步骤
echo "链接目标文件..."
# 使用 clang++ 进行最终链接，因为它需要链接 C++ 标准库
# 添加 entry.o 到链接列表
clang++ output.o entry.o $RUNTIME_OBJS -o program -O0

if [ $? -ne 0 ]; then
    echo "链接失败"
    exit 1
fi

echo "编译完成，生成可执行文件 'program'"

# 6. 运行程序 <---- 修改步骤编号
echo "运行程序..."
./program