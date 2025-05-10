
# Helper to print test results
def print_test_result(test_name, passed):
    if passed:
        print("\033[32mPASSED:\033[0m")
        print(test_name)
    else:
        print("\033[31mFAILED:\033[0m") # Removed f-string
        print(test_name)

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
    
    passed = False
    if loop_executed_count == 3:
        if else_executed:
            passed = True
        else:
            passed = False # Redundant, but keeps structure
    else:
        passed = False # Redundant
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
    
    # Original logic: passed = loop_executed_count == 2 and not else_executed
    # The existing if/else structure for passed was a bit convoluted.
    # Correct expectation: loop runs for item 1, 2. count becomes 2. else_executed is False.
    passed = False
    if loop_executed_count == 2:
        if not else_executed: # Check if else_executed is False
            passed = True
    
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
        
    # Expected sum: 1 + 3 + 5 = 9
    passed = False
    if processed_sum == 9:
        if else_executed:
            passed = True
    
    print_test_result(test_name, passed)
    return passed

# Test 4: Loop with return inside body, else block does NOT execute
# Helper function modified to return else_block status
def func_for_return_in_body():
    items = ["a", "b", "c"]
    else_block_executed = False
    for item in items:
        if item == "b":
            # Return a list: [value_string, else_block_executed_status_boolean]
            # Ensure the boolean is explicitly part of the list
            return ["returned_from_loop", else_block_executed] 
    else:
        else_block_executed = True
    # Ensure the boolean is explicitly part of the list
    return ["loop_completed_normally", else_block_executed]

def test_for_else_with_return_in_body():
    test_name = "test_for_else_with_return_in_body"
    
    result_list = func_for_return_in_body() # MODIFIED: expect a list
    result_value = result_list[0]
    else_was_executed = result_list[1] # This will be a boolean True or False
    
    passed = False
    if result_value == "returned_from_loop":
        # if not else_was_executed:
        if else_was_executed == False: # MODIFIED
            passed = True
            
    print_test_result(test_name, passed)
    return passed

# Test 5: Loop with return inside else block
def func_for_return_in_else():
    items = [10, 20]
    for item in items:
        pass # Do nothing, let loop complete
    else:
        return "returned_from_else"
    # This part should ideally not be reachable if the logic is correct
    # and the for loop always has an else that returns.
    # To be safe and explicit for a compiler, ensure all paths return.
    return "should_not_happen_in_func_for_return_in_else" 

def test_for_else_with_return_in_else():
    test_name = "test_for_else_with_return_in_else"
    result = func_for_return_in_else()
    passed = False
    if result == "returned_from_else":
        passed = True
    print_test_result(test_name, passed)
    return passed

# Test 6: Empty iterable, else block executes
def test_for_else_empty_iterable():
    test_name = "test_for_else_empty_iterable"
    items = []
    loop_executed_flag = False # Renamed for clarity
    else_executed = False
    for item in items:
        loop_executed_flag = True # This should not happen
    else:
        else_executed = True
        
    passed = False
    if not loop_executed_flag:
        if else_executed:
            passed = True
            
    print_test_result(test_name, passed)
    return passed

# Test 7: Nested for loops with break and else
def test_nested_for_else_break():
    test_name = "test_nested_for_else_break"
    outer_loop_count = 0
    inner_loop_executions = 0
    outer_else_executed = False
    
    # Using lists instead of range()
    outer_range = [0, 1] # Equivalent to range(2)
    inner_range = [0, 1, 2] # Equivalent to range(3)

    # To track if inner else blocks executed for each outer iteration
    # Assuming max 2 outer iterations based on outer_range = [0,1]
    inner_else_flag_for_outer_0 = False 
    inner_else_flag_for_outer_1 = False

    for i in outer_range: 
        outer_loop_count = outer_loop_count + 1
        inner_else_executed_this_iteration = False
        for j in inner_range: 
            inner_loop_executions = inner_loop_executions + 1
            if i == 0 :
                if j == 1: # Break inner loop on first outer iteration at j=1
                    break
        else: # Inner else
            inner_else_executed_this_iteration = True
        
        if i == 0:
            inner_else_flag_for_outer_0 = inner_else_executed_this_iteration
        elif i == 1:
            inner_else_flag_for_outer_1 = inner_else_executed_this_iteration
    else: # Outer else
        outer_else_executed = True

    # Expected:
    # Outer loop 0: i=0. Inner loop: j=0, j=1 (breaks). inner_loop_executions += 2. inner_else_flag_for_outer_0 = False
    # Outer loop 1: i=1. Inner loop: j=0, j=1, j=2 (completes). inner_loop_executions += 3. inner_else_flag_for_outer_1 = True
    # Total inner_loop_executions = 5
    # outer_else_executed = True
    passed = False
    if outer_loop_count == 2:
        if inner_loop_executions == 5:
            if inner_else_flag_for_outer_0 == False: # Note: direct boolean comparison
                if inner_else_flag_for_outer_1 == True:
                    if outer_else_executed == True:
                        passed = True
                        
    print_test_result(test_name, passed)
    return passed

# Test 8: Nested for loops with continue and else
def test_nested_for_else_continue():
    test_name = "test_nested_for_else_continue"
    outer_sum = 0
    inner_sum_total = 0
    outer_else_executed = False
    inner_else_count = 0

    # Using lists instead of range()
    outer_loop_iter = [1, 2] # Equivalent to range(1, 3)
    inner_loop_iter = [1, 2, 3] # Equivalent to range(1, 4)

    for i in outer_loop_iter: 
        outer_sum = outer_sum + i
        current_inner_sum = 0
        if i == 1: # For first outer iteration, let inner loop run normally
            for j in inner_loop_iter: 
                current_inner_sum = current_inner_sum + j
            else:
                inner_else_count = inner_else_count + 1
        else: # For second outer iteration (i=2), use continue in inner loop
            for j in inner_loop_iter: 
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
    passed = False
    if outer_sum == 3:
        if inner_sum_total == 10:
            if inner_else_count == 2:
                if outer_else_executed == True:
                    passed = True
                    
    print_test_result(test_name, passed)
    return passed

# Test 9: Complex scenario with break, continue, and else
def test_for_else_complex_break_continue():
    test_name = "test_for_else_complex_break_continue"
    numbers = [1, 2, 3, 4, 5, 6, 7, 8]
    
    # Scenario 1: Loop breaks
    processed_items_s1 = [] # Using list concatenation instead of append
    processed_items_s1_count = 0
    else_executed_s1 = False
    
    for x in numbers:
        if x % 2 == 0: # Even number
            continue
        if x == 5: # Break condition
            break
        processed_items_s1 = processed_items_s1 + [x]
        processed_items_s1_count = processed_items_s1_count + 1
    else:
        else_executed_s1 = True # Should not execute
    
    scenario1_passed = False
    # Expected: processed_items_s1 = [1, 3], count = 2
    if processed_items_s1_count == 2:
        if processed_items_s1[0] == 1:
            if processed_items_s1[1] == 3:
                if not else_executed_s1:
                    scenario1_passed = True

    # Scenario 2: Loop completes
    processed_items_s2 = [] # Reset for scenario 2
    processed_items_s2_count = 0
    else_executed_s2 = False # Reset for scenario 2

    for x in numbers: # numbers is [1, 2, 3, 4, 5, 6, 7, 8]
        if x == 7: # Skip 7
            continue
        # if x > 10: # Break condition (never met with current numbers list)
        #    break 
        # The above break condition x > 10 will never be met with the given 'numbers' list.
        # So, it effectively means the loop completes unless another break is hit.
        # For this test, we assume it's intended for the loop to complete normally.
        processed_items_s2 = processed_items_s2 + [x]
        processed_items_s2_count = processed_items_s2_count + 1
    else:
        else_executed_s2 = True # Should execute

    # Expected: 1,2,3,4,5,6,8 (7 is skipped), count = 7
    expected_scenario2_items = [1,2,3,4,5,6,8]
    expected_scenario2_count = 7
    
    scenario2_passed = False
    if else_executed_s2:
        if processed_items_s2_count == expected_scenario2_count:
            items_match = True
            # Manual comparison since direct list comparison might not be supported or for clarity
            idx = 0
            while idx < expected_scenario2_count:
                if processed_items_s2[idx] != expected_scenario2_items[idx]:
                    items_match = False
                    break
                idx = idx + 1
            if items_match:
                scenario2_passed = True
    
    passed = False
    if scenario1_passed:
        if scenario2_passed:
            passed = True
            
    print_test_result(test_name, passed)
    return passed

# Main execution
def main():
    print("--- Running For...Else Test Suite ---")
    results = [] # Use list concatenation
    results_count = 0

    # Helper to add result and increment count
    # This cannot be a nested def in the restricted syntax, so inline the logic or make it a global helper if needed
    # For now, direct manipulation:
    
    current_result = test_for_else_normal_completion()
    results = results + [current_result]
    results_count = results_count + 1
    
    current_result = test_for_else_with_break()
    results = results + [current_result]
    results_count = results_count + 1
    
    current_result = test_for_else_with_continue()
    results = results + [current_result]
    results_count = results_count + 1
    
    current_result = test_for_else_with_return_in_body()
    results = results + [current_result]
    results_count = results_count + 1
    
    current_result = test_for_else_with_return_in_else()
    results = results + [current_result]
    results_count = results_count + 1
    
    current_result = test_for_else_empty_iterable()
    results = results + [current_result]
    results_count = results_count + 1
    
    current_result = test_nested_for_else_break()
    results = results + [current_result]
    results_count = results_count + 1
    
    current_result = test_nested_for_else_continue()
    results = results + [current_result]
    results_count = results_count + 1
    
    current_result = test_for_else_complex_break_continue()
    results = results + [current_result]
    results_count = results_count + 1

    print("\n--- Test Summary ---")
    passed_count = 0
    idx = 0
    while idx < results_count:
        if results[idx]: # Check if the result is True (passed)
            passed_count = passed_count + 1
        idx = idx + 1
            
    total_tests = results_count # Use our manually tracked count
    failed_count = total_tests - passed_count

    # print(f"\033[32mTotal Passed: {passed_count}/{total_tests}\033[0m") # Original
    print("\033[32mTotal Passed: ") # Python 3 print to mimic no newline
    print(passed_count)
    print("/")
    print(total_tests)
    print("\033[0m")


    if failed_count > 0:
        # print(f"\033[31mTotal Failed: {failed_count}/{total_tests}\033[0m") # Original
        print("\033[31mTotal Failed: ")
        print(failed_count)
        print("/")
        print(total_tests)
        print("\033[0m")
        print("\033[31mSome tests failed.\033[0m")
        return 1 # Indicate failure
    else:
        print("\033[32mAll tests passed successfully!\033[0m")
        return 0 # Indicate success