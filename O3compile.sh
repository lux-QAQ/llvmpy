#!/bin/bash

# --- 配置区域 ---
# 优化级别，用于编译到bitcode以及LTO链接阶段
OPTIMIZATION_LEVEL="-O3" # 明确使用 -O3 进行优化
# LTO 模式: "full" (可能更极致但慢) 或 "thin" (推荐, 编译快)
LTO_MODE="full"
# C++ 标准
CPP_STANDARD="-std=c++23"
# 基础头文件目录 (您项目自身的include)
BASE_INCLUDE_DIR="-Iinclude/"

# --- 自定义LTO库路径 ---
CUSTOM_LIB_BASE_PATH="/opt/custom_lto_libs"
CUSTOM_LIBFFI_INCLUDE_DIR="-I${CUSTOM_LIB_BASE_PATH}/libffi/include"
CUSTOM_GMP_INCLUDE_DIR="-I${CUSTOM_LIB_BASE_PATH}/gmp/include"
CUSTOM_LIBFFI_LIB="${CUSTOM_LIB_BASE_PATH}/libffi/lib/libffi.a"
CUSTOM_GMP_LIB="${CUSTOM_LIB_BASE_PATH}/gmp/lib/libgmp.a"

# 合并所有头文件目录
ALL_INCLUDE_DIRS="${BASE_INCLUDE_DIR} ${CUSTOM_LIBFFI_INCLUDE_DIR} ${CUSTOM_GMP_INCLUDE_DIR}"

# 位置无关代码 (Position Independent Code), 推荐使用
PIC_FLAG="-fPIC"

# 链接时需要的外部库
# 直接链接自定义编译的静态LTO库，并保留mpfr（如果它不是自定义LTO编译的）
# 如果mpfr也是自定义LTO编译的，请类似地添加其.a文件路径
LINKER_LIBS="${CUSTOM_LIBFFI_LIB} ${CUSTOM_GMP_LIB} -lmpfr" # 保留 -lmpfr，除非您也自定义编译了它

# 链接器选择: "lld" (推荐用于LTO) 或 "" (留空则使用系统默认，通常是ld)
LINKER_CHOICE="lld"

# --- 文件与目录定义 ---
# llvmpy 生成的 LLVM IR 文件名
PYTHON_COMPILED_IR="output.ll"
# Python 代码编译后的目标文件名 (将包含bitcode)
PYTHON_COMPILED_OBJ="output_lto.o"
# C 入口文件名
ENTRY_C_SOURCE="src/entry.c"
# C 入口文件编译后的目标文件名 (将包含bitcode)
ENTRY_OBJ="entry_lto.o"
# Runtime C++ 源文件目录
RUNTIME_SRC_DIR="src/RunTime"
# Runtime 编译后目标文件的存放目录 (使用新目录以区分)
RUNTIME_OBJ_DIR="runtime_objs_lto"
# 最终生成的可执行文件名
FINAL_EXECUTABLE="program_lto_custom_libs" # 修改了最终文件名以区分

# --- 编译器设置 (确保使用clang) ---
CC="clang"
CXX="clang++"

# --- 脚本开始 ---
echo "=== LLVM LTO 极致优化编译脚本 (使用自定义LTO库, ${LINKER_CHOICE:-系统默认} 链接器) ==="

# 0. 清理旧文件
echo "步骤 0: 清理旧的构建文件..."
rm -f "$PYTHON_COMPILED_IR" "$PYTHON_COMPILED_OBJ" "$ENTRY_OBJ" "$FINAL_EXECUTABLE"
rm -rf "$RUNTIME_OBJ_DIR"
mkdir -p "$RUNTIME_OBJ_DIR"
echo "清理完成。"
echo

# 1. 运行 llvmpy 编译器生成 LLVM IR
echo "步骤 1: 运行 llvmpy 编译器 (Python -> LLVM IR)..."
# 假设 llvmpy 将输出保存到 PYTHON_COMPILED_IR 定义的文件名 (output.ll)
# 请根据您的实际情况修改此命令
./build/llvmpy test.py

# 检查 llvmpy 是否成功生成 IR 文件
if [ ! -f "$PYTHON_COMPILED_IR" ]; then
    echo "错误: llvmpy 未能生成 $PYTHON_COMPILED_IR 文件。"
    exit 1
fi
echo "$PYTHON_COMPILED_IR 已成功生成。"
echo

# 2. 将 Python 编译的 LLVM IR 编译为包含 bitcode 的目标文件
echo "步骤 2: 编译 Python 生成的 LLVM IR ($PYTHON_COMPILED_IR -> $PYTHON_COMPILED_OBJ)..."
# 使用 clang++ 将 .ll 文件编译为 .o 文件，并嵌入 bitcode 供 LTO 使用
# -flto=${LTO_MODE} : 告诉clang这个目标文件是为LTO准备的
# ${OPTIMIZATION_LEVEL} : 在此阶段也进行优化
${CXX} ${PIC_FLAG} -c "$PYTHON_COMPILED_IR" -o "$PYTHON_COMPILED_OBJ" "${OPTIMIZATION_LEVEL}" -flto=${LTO_MODE}
if [ $? -ne 0 ]; then
    echo "错误: 编译 $PYTHON_COMPILED_IR 失败 (${CXX})"
    exit 1
fi
echo "$PYTHON_COMPILED_OBJ 已生成 (包含 LLVM bitcode)。"
echo

# 3. 编译所有的 Runtime 源文件为包含 bitcode 的目标文件
echo "步骤 3: 编译 Runtime 源文件 (C++ -> LLVM bitcode)..."
RUNTIME_OBJS_LIST="" # 用于收集所有runtime目标文件的路径
for src_file in "$RUNTIME_SRC_DIR"/*.cpp; do
    # 从源文件名生成目标文件名
    obj_file="$RUNTIME_OBJ_DIR/$(basename "${src_file%.cpp}.o")"
    echo "  编译中: $src_file -> $obj_file"
    ${CXX} ${PIC_FLAG} -c "$src_file" -o "$obj_file" ${ALL_INCLUDE_DIRS} "${OPTIMIZATION_LEVEL}" "${CPP_STANDARD}" -flto=${LTO_MODE}
    if [ $? -ne 0 ]; then
        echo "错误: 编译 Runtime 文件 $src_file 失败"
        exit 1
    fi
    RUNTIME_OBJS_LIST="$RUNTIME_OBJS_LIST $obj_file"
done
echo "所有 Runtime 目标文件已生成 (均包含 LLVM bitcode)。"
echo

# 4. 编译 entry.c 为包含 bitcode 的目标文件
echo "步骤 4: 编译入口文件 ($ENTRY_C_SOURCE -> $ENTRY_OBJ)..."
${CC} ${PIC_FLAG} -c "$ENTRY_C_SOURCE" -o "$ENTRY_OBJ" ${ALL_INCLUDE_DIRS} "${OPTIMIZATION_LEVEL}" -flto=${LTO_MODE}
if [ $? -ne 0 ]; then
    echo "错误: 编译 $ENTRY_C_SOURCE 失败"
    exit 1
fi
echo "$ENTRY_OBJ 已生成 (包含 LLVM bitcode)。"
echo

# 5. LTO 链接所有包含 bitcode 的目标文件
echo "步骤 5: 使用 LTO 链接所有目标文件以生成最终可执行程序..."
# 使用 clang++ 进行最终链接，它能正确处理C++ Runtime和标准库。
# -flto=${LTO_MODE} : 在链接阶段启用LTO。
# ${OPTIMIZATION_LEVEL} : 指示LTO过程的优化级别。

LINKER_CMD_PREFIX="${CXX}"
if [ "$LINKER_CHOICE" == "lld" ]; then
    LINKER_CMD_PREFIX="${CXX} -fuse-ld=lld"
    echo "使用 lld 链接器。"
else
    echo "使用系统默认链接器。"
fi

echo "链接命令: $LINKER_CMD_PREFIX -flto=${LTO_MODE} ${OPTIMIZATION_LEVEL} $PYTHON_COMPILED_OBJ $ENTRY_OBJ $RUNTIME_OBJS_LIST -o $FINAL_EXECUTABLE $LINKER_LIBS"
$LINKER_CMD_PREFIX -flto=${LTO_MODE} "${OPTIMIZATION_LEVEL}" "$PYTHON_COMPILED_OBJ" "$ENTRY_OBJ" $RUNTIME_OBJS_LIST -o "$FINAL_EXECUTABLE" $LINKER_LIBS

if [ $? -ne 0 ]; then
    echo "错误: LTO 链接失败"
    exit 1
fi
echo "编译和 LTO 链接完成！"
echo "最终可执行文件: '$FINAL_EXECUTABLE'"
echo

# 6. 运行程序 (可选)
# echo "步骤 6: 运行程序..."
# ./"$FINAL_EXECUTABLE"
# if [ $? -ne 0 ]; then
#     echo "程序运行时发生错误。"
#     exit 1
# fi
# echo "程序运行结束。"

echo "构建脚本执行完毕。"
chmod +x ./$FINAL_EXECUTABLE
./$FINAL_EXECUTABLE
