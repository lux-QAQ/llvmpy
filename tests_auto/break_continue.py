def test_always_return(y):
    x = 0
    while y < 10:
        x = x + 1
        return x # Return inside loop
        # continue # This continue is unreachable due to return
    return 99 # Code after loop

def while_and_fib():
    ans = 0
    a=0
    # count  = 0 # count is unused
    while 1:
        # It's generally not recommended to define functions inside a loop
        # in standard Python for performance reasons, but we keep it for testing.
        def fib(a):
            if a <=2:
                def _fib(a):
                    if a == 0:
                        return 0
                    elif a == 1:
                        return 1
                    elif a==2:
                        return 1
            else :
                def _fib(a):
                    return fib(a-1) + fib(a-2)
            return _fib(a)
        
        fib_val = fib(a)
        if fib_val <= 55:
            pass # Continue loop if fib(a) is small enough
        else:
            ans = fib_val # Store the first fib value > 55
            break # Exit loop
        a = a + 1 # Increment a for the next fib calculation
    return ans

# --- New Test Functions ---

def test_simple_break():
    print("--- test_simple_break ---")
    i = 0
    count = 0
    limit = 10
    break_at = 5
    passed = True
    while i < limit:
        if i == break_at:
            print("Breaking loop at i =")
            print(i)
            break
        count = count + i
        i = i + 1
    
    print("Final i:")
    print(i)
    print("Final count:")
    print(count)
    
    expected_i = 5
    expected_count = 0 + 1 + 2 + 3 + 4 # Sum of i from 0 to 4
    expected_count_val = 10 

    if i != expected_i:
        print("simple_break FAILED: i Expected")
        print(expected_i)
        print("Got")
        print(i)
        passed = False
    if count != expected_count_val:
        print("simple_break FAILED: count Expected")
        print(expected_count_val)
        print("Got")
        print(count)
        passed = False
    return passed

def test_simple_continue():
    print("--- test_simple_continue ---")
    i = 0
    count = 0
    skipped_count = 0
    limit = 6 # Loop 0, 1, 2, 3, 4, 5
    skip_at = 3
    passed = True
    while i < limit:
        if i == skip_at:
            print("Continuing loop at i =")
            print(i)
            skipped_count = skipped_count + 1
            i = i + 1 # Increment i before continue to avoid infinite loop
            continue
        
        print("Processing i:") # Debug print
        print(i)
        count = count + i
        i = i + 1

    print("Final i:")
    print(i)
    print("Final count:")
    print(count)
    print("Final skipped_count:")
    print(skipped_count)

    expected_i = 6
    expected_count = 0 + 1 + 2 + 4 + 5 # Sum of i from 0 to 5, skipping 3
    expected_count_val = 12
    expected_skipped_count = 1

    if i != expected_i:
        print("simple_continue FAILED: i Expected")
        print(expected_i)
        print("Got")
        print(i)
        passed = False
    if count != expected_count_val:
        print("simple_continue FAILED: count Expected")
        print(expected_count_val)
        print("Got")
        print(count)
        passed = False
    if skipped_count != expected_skipped_count:
        print("simple_continue FAILED: skipped_count Expected")
        print(expected_skipped_count)
        print("Got")
        print(skipped_count)
        passed = False
    return passed

def test_nested_break():
    print("--- test_nested_break ---")
    outer_i = 0
    outer_limit = 3
    inner_limit = 4
    total_inner_loops = 0
    passed = True

    while outer_i < outer_limit:
        inner_i = 0
        print("Outer loop i:")
        print(outer_i)
        while inner_i < inner_limit:
            if inner_i == 2:
                print("Breaking inner loop at inner_i =")
                print(inner_i)
                break # Break only the inner loop
            
            print("Inner loop j:") # Use j for inner loop variable name for clarity
            print(inner_i)
            total_inner_loops = total_inner_loops + 1
            inner_i = inner_i + 1
        # This code runs after inner loop finishes (normally or by break)
        print("After inner loop, outer_i:")
        print(outer_i)
        outer_i = outer_i + 1

    print("Final outer_i:")
    print(outer_i)
    print("Total inner loops executed:")
    print(total_inner_loops)

    expected_outer_i = 3
    # Inner loop breaks at inner_i=2, so it runs for inner_i=0, 1 (2 times) for each outer loop.
    # Outer loop runs 3 times (outer_i=0, 1, 2).
    expected_total_inner_loops = 2 * 3 
    expected_total_inner_loops_val = 6

    if outer_i != expected_outer_i:
        print("nested_break FAILED: outer_i Expected")
        print(expected_outer_i)
        print("Got")
        print(outer_i)
        passed = False
    if total_inner_loops != expected_total_inner_loops_val:
        print("nested_break FAILED: total_inner_loops Expected")
        print(expected_total_inner_loops_val)
        print("Got")
        print(total_inner_loops)
        passed = False
    return passed

def test_nested_continue():
    print("--- test_nested_continue ---")
    outer_i = 0
    outer_limit = 2
    inner_limit = 3
    processed_count = 0
    skipped_inner_count = 0
    passed = True

    while outer_i < outer_limit:
        inner_i = 0
        print("Outer loop i:")
        print(outer_i)
        while inner_i < inner_limit:
            if inner_i == 1:
                print("Continuing inner loop at inner_i =")
                print(inner_i)
                skipped_inner_count = skipped_inner_count + 1
                inner_i = inner_i + 1 # Increment before continue
                continue # Continue only the inner loop
            
            print("Processing inner loop j:") # Use j for inner loop variable name
            print(inner_i)
            processed_count = processed_count + 1
            inner_i = inner_i + 1
        # This code runs after inner loop finishes
        print("After inner loop, outer_i:")
        print(outer_i)
        outer_i = outer_i + 1

    print("Final outer_i:")
    print(outer_i)
    print("Total processed count:")
    print(processed_count)
    print("Total skipped inner count:")
    print(skipped_inner_count)

    expected_outer_i = 2
    # Inner loop continues at inner_i=1. Runs for inner_i=0, 2 (2 times) for each outer loop.
    # Outer loop runs 2 times (outer_i=0, 1).
    expected_processed_count = 2 * 2
    expected_processed_count_val = 4
    # Inner loop skips 1 time for each outer loop. Outer loop runs 2 times.
    expected_skipped_inner_count = 1 * 2
    expected_skipped_inner_count_val = 2

    if outer_i != expected_outer_i:
        print("nested_continue FAILED: outer_i Expected")
        print(expected_outer_i)
        print("Got")
        print(outer_i)
        passed = False
    if processed_count != expected_processed_count_val:
        print("nested_continue FAILED: processed_count Expected")
        print(expected_processed_count_val)
        print("Got")
        print(processed_count)
        passed = False
    if skipped_inner_count != expected_skipped_inner_count_val:
        print("nested_continue FAILED: skipped_inner_count Expected")
        print(expected_skipped_inner_count_val)
        print("Got")
        print(skipped_inner_count)
        passed = False
    return passed

def test_break_continue_combo():
    print("--- test_break_continue_combo ---")
    i = 0
    limit = 10
    processed_sum = 0
    continue_count = 0
    passed = True

    while i < limit:
        # Continue for even numbers > 0
        if i % 2 == 0:
            if i > 0: # Avoid continuing for i=0
                print("Continuing at i =")
                print(i)
                continue_count = continue_count + 1
                i = i + 1
                continue
        
        # Break if i reaches 7
        if i == 7:
            print("Breaking at i =")
            print(i)
            break

        # Process odd numbers (and 0) before break
        print("Processing i:")
        print(i)
        processed_sum = processed_sum + i
        i = i + 1

    print("Final i:")
    print(i)
    print("Final processed_sum:")
    print(processed_sum)
    print("Final continue_count:")
    print(continue_count)

    expected_i = 7 # Loop breaks when i is 7
    # Processed numbers: 0, 1, 3, 5 (i=7 is break, i=2,4,6 are continue)
    expected_processed_sum = 0 + 1 + 3 + 5
    expected_processed_sum_val = 9
    # Continued numbers: 2, 4, 6
    expected_continue_count = 3

    if i != expected_i:
        print("break_continue_combo FAILED: i Expected")
        print(expected_i)
        print("Got")
        print(i)
        passed = False
    if processed_sum != expected_processed_sum_val:
        print("break_continue_combo FAILED: processed_sum Expected")
        print(expected_processed_sum_val)
        print("Got")
        print(processed_sum)
        passed = False
    if continue_count != expected_continue_count:
        print("break_continue_combo FAILED: continue_count Expected")
        print(expected_continue_count)
        print("Got")
        print(continue_count)
        passed = False
    return passed


def main():
    all_passed = True

    # --- Original Tests ---
    print("--- Running Break/Continue Tests ---")
    
    # Test test_always_return
    result_always_return = test_always_return(0)
    expected_always_return = 1
    print("test_always_return(0):")
    print(result_always_return)
    if result_always_return == expected_always_return:
         print("\033[32m测试 test_always_return - PASSED\033[0m\n")
    else:
         print("\033[31m测试 test_always_return - FAILED\033[0m\n")
         all_passed = False

    # Test while_and_fib
    result_while_fib = while_and_fib()
    expected_while_fib = 89 # First fib > 55
    print("while_and_fib result:")
    print(result_while_fib)
    if result_while_fib == expected_while_fib:
        print("\033[32m测试 while_and_fib - PASSED\033[0m\n")
    else:
        print("\033[31m测试 while_and_fib - FAILED\033[0m\n")
        all_passed = False

    # --- New Tests ---
    if test_simple_break():
        print("\033[32m测试 test_simple_break - PASSED\033[0m\n")
    else:
        print("\033[31m测试 test_simple_break - FAILED\033[0m\n")
        all_passed = False

    if test_simple_continue():
        print("\033[32m测试 test_simple_continue - PASSED\033[0m\n")
    else:
        print("\033[31m测试 test_simple_continue - FAILED\033[0m\n")
        all_passed = False

    if test_nested_break():
        print("\033[32m测试 test_nested_break - PASSED\033[0m\n")
    else:
        print("\033[31m测试 test_nested_break - FAILED\033[0m\n")
        all_passed = False

    if test_nested_continue():
        print("\033[32m测试 test_nested_continue - PASSED\033[0m\n")
    else:
        print("\033[31m测试 test_nested_continue - FAILED\033[0m\n")
        all_passed = False

    if test_break_continue_combo():
        print("\033[32m测试 test_break_continue_combo - PASSED\033[0m\n")
    else:
        print("\033[31m测试 test_break_continue_combo - FAILED\033[0m\n")
        all_passed = False

    # --- Final Summary ---
    print("\n--- Break/Continue Tests Summary ---")
    if all_passed:
        print("\033[32m所有 break/continue 测试均通过\033[0m")
        return 0 # Indicate success
    else:
        print("\033[31m有 break/continue 测试未通过\033[0m")
        return 1 # Indicate failure