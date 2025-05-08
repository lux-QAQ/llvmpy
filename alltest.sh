#!/bin/bash

export LD_PRELOAD=""
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH

#  Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

#  Symbols
CHECK_MARK="✔"
CROSS_MARK="✘"
INFO_SYMBOL="ℹ️"

#  directory
BASE_DIR=$(pwd)
TEST_DIR="$BASE_DIR/tests_auto"
BUILD_DIR="$BASE_DIR/build"
RUNTIME_SRC_DIR="$BASE_DIR/src/RunTime"
ENTRY_C_FILE="$BASE_DIR/src/entry.c"
RUNTIME_OBJS_DIR="$BASE_DIR/runtime_objs"
INCLUDE_DIR="$BASE_DIR/include"
LOGS_DIR="$BASE_DIR/logs"
EXECUTABLES_DIR="$LOGS_DIR/bin" # Directory for compiled test executables
CHECKNEEDS_DIR="$TEST_DIR/checkneeds" # Directory for pre-check scripts and their .py files

if [ ! -f "$BUILD_DIR/llvmpy" ]; then
    echo -e "${RED}Error: llvmpy compiler not found at $BUILD_DIR/llvmpy. Please build it first.${NC}"
    exit 1
fi

chmod +x -R $BUILD_DIR/llvmpy
chmod +x -R $TEST_DIR
mkdir -p "$RUNTIME_OBJS_DIR"
mkdir -p "$LOGS_DIR"
mkdir -p "$EXECUTABLES_DIR" # Create directory for executables

# 0. 清理文件
echo -e "${BLUE}${INFO_SYMBOL} Cleaning up old common files...${NC}"
rm -f "$BASE_DIR/output.ll" "$BASE_DIR/output.o"
rm -f "$BASE_DIR"/*.test # Old location, if any
rm -f "$EXECUTABLES_DIR"/*.test # Clean executables from new location
rm -f "$RUNTIME_OBJS_DIR"/*.o "$BASE_DIR/entry.o"
# Clean previous logs for common compilation
COMMON_COMPILE_LOG="$LOGS_DIR/common_compilation.log"
rm -f "$COMMON_COMPILE_LOG"
touch "$COMMON_COMPILE_LOG"
# Clean logs from previous checkneeds scripts and individual test logs
rm -f "$LOGS_DIR"/_*.sh.log
rm -f "$LOGS_DIR"/*.py.compile.log


# 1. 编译运行时源文件
echo -e "${BLUE}${INFO_SYMBOL} Compiling common runtime objects and entry point (logs in $COMMON_COMPILE_LOG)...${NC}"
RUNTIME_OBJS_PATHS=""
COMMON_COMPILE_SUCCESS=true
for src_file in "$RUNTIME_SRC_DIR"/*.cpp; do
    if [ -f "$src_file" ]; then
        obj_file="$RUNTIME_OBJS_DIR/$(basename "${src_file%.cpp}.o")"
        clang++ -c "$src_file" -o "$obj_file" -I"$INCLUDE_DIR/" -O0 -std=c++20 -fPIC >> "$COMMON_COMPILE_LOG" 2>&1
        if [ $? -ne 0 ]; then
            echo -e "${RED}Error compiling runtime file $src_file. Check $COMMON_COMPILE_LOG. Aborting.${NC}"
            COMMON_COMPILE_SUCCESS=false
            break
        fi
        RUNTIME_OBJS_PATHS="$RUNTIME_OBJS_PATHS $obj_file"
    fi
done

if [ "$COMMON_COMPILE_SUCCESS" = true ]; then
    if [ ! -f "$ENTRY_C_FILE" ]; then
        echo -e "${RED}Error: Entry file $ENTRY_C_FILE not found. Aborting.${NC}"
        COMMON_COMPILE_SUCCESS=false
    else
        clang -c "$ENTRY_C_FILE" -o "$BASE_DIR/entry.o" -I"$INCLUDE_DIR/" -O0 -fPIC >> "$COMMON_COMPILE_LOG" 2>&1
        if [ $? -ne 0 ]; then
            echo -e "${RED}Error compiling $ENTRY_C_FILE. Check $COMMON_COMPILE_LOG. Aborting.${NC}"
            COMMON_COMPILE_SUCCESS=false
        fi
    fi
fi

if [ "$COMMON_COMPILE_SUCCESS" = false ]; then
    exit 1
fi
ENTRY_OBJ_PATH="$BASE_DIR/entry.o"
echo -e "${GREEN}${CHECK_MARK} Common objects compiled successfully.${NC}\n"


# 2. 执行 CheckNeeds 预检查脚本
echo -e "${BLUE}${INFO_SYMBOL} Starting Pre-checks from '$CHECKNEEDS_DIR'...${NC}"
PRECHECK_PASSED_ALL=true
if [ ! -d "$CHECKNEEDS_DIR" ]; then
    echo -e "${YELLOW}Warning: CheckNeeds directory $CHECKNEEDS_DIR not found. Skipping pre-checks.${NC}"
else
    CHECKNEEDS_SCRIPTS=$(find "$CHECKNEEDS_DIR" -maxdepth 1 -name "_*.sh" -type f | sort)
    if [ -z "$CHECKNEEDS_SCRIPTS" ]; then
        echo -e "${YELLOW}No pre-check scripts (_*.sh) found in $CHECKNEEDS_DIR. Skipping pre-checks.${NC}"
    else
        for check_script in $CHECKNEEDS_SCRIPTS; do
            script_basename=$(basename "$check_script")
            # echo -e "${BLUE}--- Running Pre-check Script: $script_basename ---${NC}" # 修改：注释掉此行以获得更简洁的输出
            chmod +x "$check_script" # Ensure it's executable
            
            # Execute the script. It will print its own PASSED/FAILED status.
            # Its exit code (0 for success, non-0 for failure) determines the outcome.
            if "$check_script"; then # This checks $? of the script
                # The script itself already prints PASSED/FAILED, so we might not need to repeat it here
                # Or, keep a summary line:
                # echo -e "${GREEN}Pre-check $script_basename PASSED.${NC}"
                : # Script handles its own detailed output
            else
                # echo -e "${RED}Pre-check $script_basename FAILED. See its output above or its log file.${NC}"
                PRECHECK_PASSED_ALL=false
                # break # Uncomment to stop on the first pre-check failure
            fi
        done

        if [ "$PRECHECK_PASSED_ALL" = false ]; then
            echo -e "\n${RED}${CROSS_MARK} One or more critical pre-checks from '$CHECKNEEDS_DIR' failed. Aborting further tests.${NC}"
            exit 1
        else
            echo -e "\n${GREEN}${CHECK_MARK} All critical pre-checks from '$CHECKNEEDS_DIR' passed successfully.${NC}"
        fi
    fi
fi
echo "" # Newline for separation


# 3. 获取并执行常规测试文件 (位于 tests_auto, 不包括 checkneeds 子目录中的 .py)
echo -e "${BLUE}--- Starting Main Automated Tests (from $TEST_DIR, excluding subdirectories) ---${NC}"
TEST_FILES=$(find "$TEST_DIR" -maxdepth 1 -name "*.py" -type f | sort)

if [ -z "$TEST_FILES" ]; then
    echo -e "${YELLOW}No Python test files found directly in $TEST_DIR. Nothing further to test.${NC}"
    # If pre-checks passed but no main tests, it's still a success overall for pre-checks.
    # The exit status will be handled by the summary at the end.
    # We need to initialize counts if we might skip the loop.
    passed_count=0
    failed_count=0
    # exit 0 # Or let it proceed to summary
else
    test_num=0
    passed_count=0
    failed_count=0

    # 遍历每个测试文件
    for test_file in $TEST_FILES; do
        test_num=$((test_num + 1))
        test_name_py=$(basename "$test_file")
        test_base_name="${test_name_py%.py}"
        executable_file_name="${test_base_name}.test"
        executable_path="$EXECUTABLES_DIR/$executable_file_name" # Executables go into logs/bin
        
        test_compile_log_file="$LOGS_DIR/${test_name_py}.compile.log"
        rm -f "$test_compile_log_file" 
        touch "$test_compile_log_file"

        rm -f "$BASE_DIR/output.ll" "$BASE_DIR/output.o" "$executable_path"

        current_test_failed=false
        failure_reason=""

        # --- 开始：解决 llvmpy 总是编译 test.py 的变通方法 ---
        LLVMPY_EXPECTED_INPUT_FILE="$BASE_DIR/test.py"
        BACKUP_OF_ORIGINAL_TEST_PY="$BASE_DIR/test.py.runtest_backup"
        ORIGINAL_TEST_PY_WAS_MOVED=false

        if [ -f "$LLVMPY_EXPECTED_INPUT_FILE" ]; then
            mv "$LLVMPY_EXPECTED_INPUT_FILE" "$BACKUP_OF_ORIGINAL_TEST_PY"
            ORIGINAL_TEST_PY_WAS_MOVED=true
        fi
        cp "$test_file" "$LLVMPY_EXPECTED_INPUT_FILE"
        # --- 结束：准备工作 ---

        # Step 3.1: 编译IR
        "$BUILD_DIR/llvmpy" "$LLVMPY_EXPECTED_INPUT_FILE" "$BASE_DIR/output.ll" >> "$test_compile_log_file" 2>&1
        compile_status=$?

        # --- 开始：清理变通方法产生的文件 ---
        rm "$LLVMPY_EXPECTED_INPUT_FILE"
        if [ "$ORIGINAL_TEST_PY_WAS_MOVED" = true ]; then
            mv "$BACKUP_OF_ORIGINAL_TEST_PY" "$LLVMPY_EXPECTED_INPUT_FILE"
        fi
        # --- 结束：清理工作 ---

        if [ $compile_status -ne 0 ]; then
            current_test_failed=true
            failure_reason="llvmpy compilation failed (code $compile_status) for $test_name_py. Log: $test_compile_log_file"
        fi

        # Step 3.2: 编译LLVM IR为目标文件
        if [ "$current_test_failed" = false ]; then
            clang++ -c "$BASE_DIR/output.ll" -o "$BASE_DIR/output.o" -O0 >> "$test_compile_log_file" 2>&1
            if [ $? -ne 0 ]; then
                current_test_failed=true
                failure_reason="LLVM IR to object compilation failed. Log: $test_compile_log_file"
            fi
        fi
        OUTPUT_OBJ_PATH="$BASE_DIR/output.o"

        # Step 3.3: 链接目标文件
        if [ "$current_test_failed" = false ]; then
            clang++ "$OUTPUT_OBJ_PATH" "$ENTRY_OBJ_PATH" $RUNTIME_OBJS_PATHS -o "$executable_path" -O0 -lffi -lgmp -lmpfr >> "$test_compile_log_file" 2>&1
            if [ $? -ne 0 ]; then
                current_test_failed=true
                failure_reason="Linking failed. Log: $test_compile_log_file"
            fi
        fi

        # Step 3.4: 运行程序
        if [ "$current_test_failed" = false ]; then
            "$executable_path" > /dev/null 2>&1 
            exit_code=$?
            # For regular tests, we assume exit code 0 means success unless specified otherwise.
            # The problem description implies these regular tests should exit 0 for PASS.
            if [ $exit_code -ne 0 ]; then
                current_test_failed=true
                "$executable_path" >> "$test_compile_log_file" 2>&1 
                failure_reason="Program $executable_file_name exited with code $exit_code. Output/Error in $test_compile_log_file"
            fi
        fi

        # Step 3.5: 记录结果
        if [ "$current_test_failed" = true ]; then
            echo -e "[Test #${test_num}: ${test_name_py}] --- ${RED}${CROSS_MARK} FAILED${NC} (${failure_reason})"
            failed_count=$((failed_count + 1))
        else
            echo -e "[Test #${test_num}: ${test_name_py}] --- ${GREEN}${CHECK_MARK} PASSED${NC}"
            passed_count=$((passed_count + 1))
        fi
    done
fi # End of if [ -z "$TEST_FILES" ]

# 4. 输出测试结果总结
echo -e "\n${BLUE}--- Test Summary ---${NC}"
# Consider pre-check results in the final summary if needed, though failure there exits early.
# The counts below are for the main test loop.
if [ -n "$TEST_FILES" ]; then # Only print pass/fail counts if main tests ran
    echo -e "${GREEN}Main Tests Passed: $passed_count${NC}"
    echo -e "${RED}Main Tests Failed: $failed_count${NC}"

    if [ $failed_count -eq 0 ]; then
        echo -e "${GREEN}${CHECK_MARK} All main tests passed!${NC}"
        exit 0
    else
        echo -e "${RED}${CROSS_MARK} Some main tests failed.${NC}"
        exit 1
    fi
elif [ "$PRECHECK_PASSED_ALL" = true ]; then # No main tests, but pre-checks passed
    echo -e "${GREEN}All pre-checks passed and no main tests to run.${NC}"
    exit 0
else # Should have exited earlier if pre-checks failed, but as a fallback:
    echo -e "${RED}Pre-checks failed or no tests found/executed.${NC}"
    exit 1
fi