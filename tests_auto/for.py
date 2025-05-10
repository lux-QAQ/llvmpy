#!/usr/bin/env python3
# filepath: test_test.py
"""
Test suite for the for...in...else construct, including break, continue, and return.
"""

# Helper to print test results
def print_test_result(test_name, passed):
    if passed:
        print(f"\033[32mPASSED:\033[0m {test_name}")
    else:
        print(f"\033[31mFAILED:\033[0m {test_name}")

# Test 1: Normal loop completion, else block executes
def test_for_else_normal_completion():
    test_name = "test_for_else_normal_completion"
    items = [1, 2, 3]
    loop_executed_count = 0
    else_executed = False
    for item in items:
        loop_executed_count = loop_executed_count + 1
    else:
        else_executed = True
    
    passed = loop_executed_count == 3 and else_executed
    print_test_result(test_name, passed)
    return passed

# Test 2: Loop terminated by break, else block does NOT execute
def test_for_else_with_break():
    test_name = "test_for_else_with_break"
    items = [1, 2, 3, 4, 5]
    loop_executed_count = 0
    else_executed = False
    for item in items:
        if item == 3:
            break
        loop_executed_count = loop_executed_count + 1
    else:
        else_executed = True
    
    passed = loop_executed_count == 2 and not else_executed
    print_test_result(test_name, passed)
    return passed

# Test 3: Loop with continue, else block executes
def test_for_else_with_continue():
    test_name = "test_for_else_with_continue"
    items = [1, 2, 3, 4, 5]
    processed_sum = 0
    else_executed = False
    for item in items:
        if item % 2 == 0: # Skip even numbers
            continue
        processed_sum = processed_sum + item
    else:
        else_executed = True
        
    # Expected sum of odd numbers: 1 + 3 + 5 = 9
    passed = processed_sum == 9 and else_executed
    print_test_result(test_name, passed)
    return passed

# Test 4: Loop with return inside body, else block does NOT execute
def func_for_return_in_body():
    items = ["a", "b", "c"]
    # This flag should remain False if return happens before else
    func_for_return_in_body.else_block_tracker = False 
    for item in items:
        if item == "b":
            return "returned_from_loop"
    else:
        func_for_return_in_body.else_block_tracker = True
    return "loop_completed_normally" # Should not reach here if "b" is found

func_for_return_in_body.else_block_tracker = False # Initialize static-like variable

def test_for_else_with_return_in_body():
    test_name = "test_for_else_with_return_in_body"
    # Reset tracker before each call if necessary, or ensure function is pure
    func_for_return_in_body.else_block_tracker = False
    result = func_for_return_in_body()
    
    passed = result == "returned_from_loop" and not func_for_return_in_body.else_block_tracker
    print_test_result(test_name, passed)
    return passed

# Test 5: Loop with return inside else block
def func_for_return_in_else():
    items = [10, 20]
    for item in items:
        pass # Do nothing, let loop complete
    else:
        return "returned_from_else"
    return "should_not_happen"

def test_for_else_with_return_in_else():
    test_name = "test_for_else_with_return_in_else"
    result = func_for_return_in_else()
    passed = result == "returned_from_else"
    print_test_result(test_name, passed)
    return passed

# Test 6: Empty iterable, else block executes
def test_for_else_empty_iterable():
    test_name = "test_for_else_empty_iterable"
    items = []
    loop_executed = False
    else_executed = False
    for item in items:
        loop_executed = True # This should not happen
    else:
        else_executed = True
        
    passed = not loop_executed and else_executed
    print_test_result(test_name, passed)
    return passed

# Test 7: Nested for loops with break and else
def test_nested_for_else_break():
    test_name = "test_nested_for_else_break"
    outer_loop_count = 0
    inner_loop_executions = 0
    outer_else_executed = False
    inner_else_flags = [] # To track if inner else blocks executed

    for i in range(2): # Outer loop runs twice
        outer_loop_count = outer_loop_count + 1
        inner_else_executed_this_iteration = False
        for j in range(3): # Inner loop
            inner_loop_executions = inner_loop_executions + 1
            if i == 0 and j == 1: # Break inner loop on first outer iteration at j=1
                break
        else: # Inner else
            inner_else_executed_this_iteration = True
        inner_else_flags.append(inner_else_executed_this_iteration)
    else: # Outer else
        outer_else_executed = True

    # Expected:
    # Outer loop 0: i=0. Inner loop: j=0, j=1 (breaks). inner_loop_executions += 2. inner_else_flags[0] = False
    # Outer loop 1: i=1. Inner loop: j=0, j=1, j=2 (completes). inner_loop_executions += 3. inner_else_flags[1] = True
    # Total inner_loop_executions = 5
    # outer_else_executed = True
    passed = (outer_loop_count == 2 and
              inner_loop_executions == 5 and
              inner_else_flags[0] == False and
              inner_else_flags[1] == True and
              outer_else_executed == True)
    print_test_result(test_name, passed)
    return passed

# Test 8: Nested for loops with continue and else
def test_nested_for_else_continue():
    test_name = "test_nested_for_else_continue"
    outer_sum = 0
    inner_sum_total = 0
    outer_else_executed = False
    inner_else_count = 0

    for i in range(1, 3): # Outer: 1, 2
        outer_sum = outer_sum + i
        current_inner_sum = 0
        if i == 1: # For first outer iteration, let inner loop run normally
            for j in range(1, 4): # Inner: 1, 2, 3
                current_inner_sum = current_inner_sum + j
            else:
                inner_else_count = inner_else_count + 1
        else: # For second outer iteration (i=2), use continue in inner loop
            for j in range(1, 4): # Inner: 1, 2, 3
                if j == 2:
                    continue
                current_inner_sum = current_inner_sum + j
            else:
                inner_else_count = inner_else_count + 1
        inner_sum_total = inner_sum_total + current_inner_sum
    else:
        outer_else_executed = True
    
    # Expected:
    # i=1: outer_sum=1. Inner loop (1,2,3) sum = 6. inner_else_count=1. inner_sum_total=6
    # i=2: outer_sum=1+2=3. Inner loop (1, (skip 2), 3) sum = 1+3=4. inner_else_count=2. inner_sum_total=6+4=10
    # outer_else_executed = True
    passed = (outer_sum == 3 and
              inner_sum_total == 10 and
              inner_else_count == 2 and
              outer_else_executed == True)
    print_test_result(test_name, passed)
    return passed

# Test 9: Complex scenario with break, continue, and else
def test_for_else_complex_break_continue():
    test_name = "test_for_else_complex_break_continue"
    numbers = [1, 2, 3, 4, 5, 6, 7, 8]
    processed_items = []
    else_executed = False
    
    # Scenario 1: Loop breaks
    for x in numbers:
        if x % 2 == 0: # Even number
            continue
        if x == 5: # Break condition
            break
        processed_items.append(x)
    else:
        else_executed = True # Should not execute
    
    scenario1_passed = (processed_items[0] == 1 and processed_items[1] == 3 and
                        len(processed_items) == 2 and not else_executed)

    # Scenario 2: Loop completes
    processed_items = [] # Reset
    else_executed = False # Reset
    for x in numbers:
        if x == 7: # Skip 7
            continue
        if x > 10: # Break condition (never met)
            break
        processed_items.append(x)
    else:
        else_executed = True # Should execute

    # Expected: 1,2,3,4,5,6,8 (7 is skipped)
    expected_scenario2_items = [1,2,3,4,5,6,8]
    scenario2_passed = True
    if len(processed_items) != len(expected_scenario2_items):
        scenario2_passed = False
    else:
        for k in range(len(processed_items)):
            if processed_items[k] != expected_scenario2_items[k]:
                scenario2_passed = False
                break
    scenario2_passed = scenario2_passed and else_executed
    
    passed = scenario1_passed and scenario2_passed
    print_test_result(test_name, passed)
    return passed

# Main execution
def main():
    print("--- Running For...Else Test Suite ---")
    results = []
    results.append(test_for_else_normal_completion())
    results.append(test_for_else_with_break())
    results.append(test_for_else_with_continue())
    results.append(test_for_else_with_return_in_body())
    results.append(test_for_else_with_return_in_else())
    results.append(test_for_else_empty_iterable())
    results.append(test_nested_for_else_break())
    results.append(test_nested_for_else_continue())
    results.append(test_for_else_complex_break_continue())

    print("\n--- Test Summary ---")
    passed_count = 0
    for r in results:
        if r:
            passed_count = passed_count + 1
            
    total_tests = len(results)
    failed_count = total_tests - passed_count

    print(f"\033[32mTotal Passed: {passed_count}/{total_tests}\033[0m")
    if failed_count > 0:
        print(f"\033[31mTotal Failed: {failed_count}/{total_tests}\033[0m")
        print("\033[31mSome tests failed.\033[0m")
        # In a real test runner, this would set a non-zero exit code
        return 1
    else:
        print("\033[32mAll tests passed successfully!\033[0m")
        return 0
