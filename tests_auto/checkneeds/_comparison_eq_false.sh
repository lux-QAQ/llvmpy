#!/bin/bash

# Script-specific settings
PYTHON_FILE_NAME_IN_CHECKNEEDS_DIR="_comparison_eq_false.py"
EXPECTED_EXIT_CODE=0
# ---

# Colors and Symbols
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'
CHECK_MARK="✔"
CROSS_MARK="✘"

BASE_DIR=$(pwd)
SCRIPT_BASENAME=$(basename "$0")
PYTHON_FILE_PATH="$BASE_DIR/tests_auto/checkneeds/$PYTHON_FILE_NAME_IN_CHECKNEEDS_DIR"

LOGS_DIR_ABS="$BASE_DIR/logs"
CHECKNEEDS_LOG_FILE="$LOGS_DIR_ABS/${SCRIPT_BASENAME}.log"

BUILD_DIR_ABS="$BASE_DIR/build"
LLVMPY_EXEC="$BUILD_DIR_ABS/llvmpy"

TEMP_OUTPUT_LL="$BASE_DIR/${SCRIPT_BASENAME}.ll"
TEMP_OUTPUT_O="$BASE_DIR/${SCRIPT_BASENAME}.o"
EXECUTABLE_DIR_ABS="$LOGS_DIR_ABS/bin"
EXECUTABLE_PATH="$EXECUTABLE_DIR_ABS/${SCRIPT_BASENAME}.test"

ENTRY_OBJ_PATH_ABS="$BASE_DIR/entry.o"
RUNTIME_OBJS_DIR_ABS="$BASE_DIR/runtime_objs"
RUNTIME_OBJS_PATHS_STR=""
for obj_file_path in "$RUNTIME_OBJS_DIR_ABS"/*.o; do
    if [ -f "$obj_file_path" ]; then
        RUNTIME_OBJS_PATHS_STR="$RUNTIME_OBJS_PATHS_STR $obj_file_path"
    fi
done

mkdir -p "$LOGS_DIR_ABS"
mkdir -p "$EXECUTABLE_DIR_ABS"
rm -f "$CHECKNEEDS_LOG_FILE"
echo "--- Log for CheckNeeds Script: $SCRIPT_BASENAME ---" > "$CHECKNEEDS_LOG_FILE"
date >> "$CHECKNEEDS_LOG_FILE"

rm -f "$TEMP_OUTPUT_LL" "$TEMP_OUTPUT_O" "$EXECUTABLE_PATH"

if [ ! -f "$LLVMPY_EXEC" ]; then echo "Error: $LLVMPY_EXEC not found." >> "$CHECKNEEDS_LOG_FILE"; exit 1; fi
if [ ! -f "$PYTHON_FILE_PATH" ]; then echo "Error: Python file $PYTHON_FILE_PATH not found." >> "$CHECKNEEDS_LOG_FILE"; exit 1; fi
if [ ! -f "$ENTRY_OBJ_PATH_ABS" ]; then echo "Error: $ENTRY_OBJ_PATH_ABS not found." >> "$CHECKNEEDS_LOG_FILE"; exit 1; fi

LLVMPY_WORKAROUND_INPUT_FILE="$BASE_DIR/test.py"
BACKUP_OF_ORIGINAL_TEST_PY="$BASE_DIR/test.py.${SCRIPT_BASENAME}_backup"
ORIGINAL_TEST_PY_WAS_MOVED_FOR_WORKAROUND=false

if [ -f "$LLVMPY_WORKAROUND_INPUT_FILE" ]; then
    mv "$LLVMPY_WORKAROUND_INPUT_FILE" "$BACKUP_OF_ORIGINAL_TEST_PY"
    ORIGINAL_TEST_PY_WAS_MOVED_FOR_WORKAROUND=true
fi
cp "$PYTHON_FILE_PATH" "$LLVMPY_WORKAROUND_INPUT_FILE"

echo "Running: $LLVMPY_EXEC $LLVMPY_WORKAROUND_INPUT_FILE $TEMP_OUTPUT_LL" >> "$CHECKNEEDS_LOG_FILE"
"$LLVMPY_EXEC" "$LLVMPY_WORKAROUND_INPUT_FILE" "$TEMP_OUTPUT_LL" >> "$CHECKNEEDS_LOG_FILE" 2>&1
compile_status=$?

rm "$LLVMPY_WORKAROUND_INPUT_FILE"
if [ "$ORIGINAL_TEST_PY_WAS_MOVED_FOR_WORKAROUND" = true ]; then
    mv "$BACKUP_OF_ORIGINAL_TEST_PY" "$LLVMPY_WORKAROUND_INPUT_FILE"
fi

if [ $compile_status -ne 0 ]; then
    echo -e "[CheckScript: ${SCRIPT_BASENAME}] ${RED}${CROSS_MARK} FAILED (llvmpy compilation)${NC}. See $CHECKNEEDS_LOG_FILE"
    exit 1
fi

echo "Running: clang++ -c $TEMP_OUTPUT_LL -o $TEMP_OUTPUT_O -O0" >> "$CHECKNEEDS_LOG_FILE"
clang++ -c "$TEMP_OUTPUT_LL" -o "$TEMP_OUTPUT_O" -O0 >> "$CHECKNEEDS_LOG_FILE" 2>&1
if [ $? -ne 0 ]; then
    echo -e "[CheckScript: ${SCRIPT_BASENAME}] ${RED}${CROSS_MARK} FAILED (IR to object compilation)${NC}. See $CHECKNEEDS_LOG_FILE"
    rm -f "$TEMP_OUTPUT_LL"
    exit 1
fi

echo "Running: clang++ $TEMP_OUTPUT_O $ENTRY_OBJ_PATH_ABS $RUNTIME_OBJS_PATHS_STR -o $EXECUTABLE_PATH -O0 -lffi -lgmp -lmpfr" >> "$CHECKNEEDS_LOG_FILE"
clang++ "$TEMP_OUTPUT_O" "$ENTRY_OBJ_PATH_ABS" $RUNTIME_OBJS_PATHS_STR -o "$EXECUTABLE_PATH" -O0 -lffi -lgmp -lmpfr >> "$CHECKNEEDS_LOG_FILE" 2>&1
if [ $? -ne 0 ]; then
    echo -e "[CheckScript: ${SCRIPT_BASENAME}] ${RED}${CROSS_MARK} FAILED (Linking)${NC}. See $CHECKNEEDS_LOG_FILE"
    rm -f "$TEMP_OUTPUT_LL" "$TEMP_OUTPUT_O"
    exit 1
fi

echo "Running: $EXECUTABLE_PATH" >> "$CHECKNEEDS_LOG_FILE"
"$EXECUTABLE_PATH" >> "$CHECKNEEDS_LOG_FILE" 2>&1
actual_exit_code=$?
echo "Actual exit code: $actual_exit_code" >> "$CHECKNEEDS_LOG_FILE"

rm -f "$TEMP_OUTPUT_LL" "$TEMP_OUTPUT_O"

if [ $actual_exit_code -eq $EXPECTED_EXIT_CODE ]; then
    echo -e "[CheckScript: ${SCRIPT_BASENAME}] ${GREEN}${CHECK_MARK} PASSED${NC} (Exit code $actual_exit_code as expected)"
    exit 0
else
    echo -e "[CheckScript: ${SCRIPT_BASENAME}] ${RED}${CROSS_MARK} FAILED${NC} (Expected exit code $EXPECTED_EXIT_CODE, got $actual_exit_code). See $CHECKNEEDS_LOG_FILE"
    exit 1
fi