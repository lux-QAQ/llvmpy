#!/bin/bash

# === 极致 Release + LTO 优化编译脚本 ===

# 编译器设定（确保使用 Clang）
CC="clang"
CXX="clang++"

# --- 优化参数 ---
inline_threshold=1000
OPT_LEVEL="-O3 -ffast-math -march=native -funroll-loops"
EXTRA_FLAGS="-fstrict-aliasing -fomit-frame-pointer -fno-rtti -fvisibility=hidden -fno-semantic-interposition"
LTO_FLAGS="-flto=full -fwhole-program-vtables -fsplit-lto-unit"
LLVM_TUNING="-mllvm -inline-threshold=$inline_threshold"
CPP_STANDARD="-std=c++23"
PIC_FLAG="-fPIC"

# 链接器设置
LINKER_CHOICE="lld"
LINKER_FLAGS="-fuse-ld=lld -Wl,--lto-O3,--icf=all,--no-undefined"

# 自定义头与库路径
BASE_INCLUDE_DIR="-Iinclude/"
CUSTOM_LIB_BASE_PATH="/opt/custom_lto_libs"
CUSTOM_LIBFFI_INCLUDE="-I${CUSTOM_LIB_BASE_PATH}/libffi/include"
CUSTOM_GMP_INCLUDE="-I${CUSTOM_LIB_BASE_PATH}/gmp/include"
CUSTOM_LIBFFI_LIB="${CUSTOM_LIB_BASE_PATH}/libffi/lib/libffi.a"
CUSTOM_GMP_LIB="${CUSTOM_LIB_BASE_PATH}/gmp/lib/libgmp.a"
LINKER_LIBS="${CUSTOM_LIBFFI_LIB} ${CUSTOM_GMP_LIB} -lmpfr"
ALL_INCLUDE_DIRS="${BASE_INCLUDE_DIR} ${CUSTOM_LIBFFI_INCLUDE} ${CUSTOM_GMP_INCLUDE}"

# 文件定义
PYTHON_COMPILED_IR="output.ll"
PYTHON_COMPILED_OBJ="output_lto.o"
ENTRY_C_SOURCE="src/entry.c"
ENTRY_OBJ="entry_lto.o"
RUNTIME_SRC_DIR="src/RunTime"
RUNTIME_OBJ_DIR="runtime_objs_lto"
FINAL_EXECUTABLE="program_lto_custom_libs"

echo "=== LLVM 极致优化构建流程开始 ==="
echo "使用 ThinLTO + Link Time Optimization + 高级指令调度"

# 步骤 0：清理
rm -f "$PYTHON_COMPILED_IR" "$PYTHON_COMPILED_OBJ" "$ENTRY_OBJ" "$FINAL_EXECUTABLE"
rm -rf "$RUNTIME_OBJ_DIR"
mkdir -p "$RUNTIME_OBJ_DIR"

# 步骤 1：运行 llvmpy 编译器（假设输出为 output.ll）
./build/llvmpy test.py
if [ ! -f "$PYTHON_COMPILED_IR" ]; then
    echo "❌ 错误：未生成 LLVM IR ($PYTHON_COMPILED_IR)"
    exit 1
fi

# 步骤 2：编译 LLVM IR -> obj
$CXX -c "$PYTHON_COMPILED_IR" -o "$PYTHON_COMPILED_OBJ" \
  $OPT_LEVEL $EXTRA_FLAGS $LTO_FLAGS $LLVM_TUNING $PIC_FLAG

# 步骤 3：编译 Runtime
RUNTIME_OBJS_LIST=""
for src in "$RUNTIME_SRC_DIR"/*.cpp; do
    obj="$RUNTIME_OBJ_DIR/$(basename "${src%.cpp}.o")"
    echo "编译 Runtime: $src -> $obj"
    $CXX -c "$src" -o "$obj" \
      $OPT_LEVEL $EXTRA_FLAGS $LTO_FLAGS $LLVM_TUNING $PIC_FLAG $CPP_STANDARD $ALL_INCLUDE_DIRS
    RUNTIME_OBJS_LIST="$RUNTIME_OBJS_LIST $obj"
done

# 步骤 4：编译入口文件 entry.c
$CC -c "$ENTRY_C_SOURCE" -o "$ENTRY_OBJ" \
  $OPT_LEVEL $EXTRA_FLAGS $LTO_FLAGS $LLVM_TUNING $PIC_FLAG $ALL_INCLUDE_DIRS

# 步骤 5：LTO 链接生成最终可执行文件
$CXX $LINKER_FLAGS \
  $OPT_LEVEL $EXTRA_FLAGS $LTO_FLAGS $LLVM_TUNING \
  "$PYTHON_COMPILED_OBJ" "$ENTRY_OBJ" $RUNTIME_OBJS_LIST \
  -o "$FINAL_EXECUTABLE" $LINKER_LIBS

if [ $? -ne 0 ]; then
    echo "❌ 错误：链接失败"
    exit 1
fi

chmod +x "$FINAL_EXECUTABLE"
echo "✅ 构建完成：$FINAL_EXECUTABLE"
./"$FINAL_EXECUTABLE"
