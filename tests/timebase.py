def calculate_complex_value(initial_integer_val, initial_float_val, total_iterations, results_list):
   

    current_i = initial_integer_val # Expected to be an integer type
    current_f = initial_float_val   # Expected to be a float type
    
    iteration_count = 0
    while iteration_count < total_iterations:
        # --- Integer Dominant Operations ---
        int_divisor = (iteration_count % 17) + 1 
        current_i = (current_i * 5 - iteration_count * 2 + 101)
        if current_i < 0:
            current_i = -current_i + iteration_count + 1
        # Original: current_i = int(current_i / int_divisor)
        # Assumes assignment of float (result of /) to int variable truncates
        current_i = current_i / int_divisor 

        if current_i > 1000000000000: # 10^12
             # Original: current_i = int(current_i / ((iteration_count % 100) + 1000)) + (iteration_count % 700) + 1
             # current_i / float_expr results in float. Sum is float. Assignment to current_i (int) truncates.
             current_i = (current_i / ((iteration_count % 100) + 1000.0)) + (iteration_count % 700) + 1
        if current_i == 0: # Avoid division by zero later
            current_i = iteration_count + 1

        # --- Float Dominant Operations ---
        float_divisor = 1.000002 + (iteration_count % 23) * 0.00000003 # Always > 1
        
        # Original line used \ and float()
        # current_f = (current_f * (1.0000015 + (iteration_count % 11)*0.0000002) + \
        #              float(iteration_count + 1) / (1000.0 + (iteration_count % 101)) ) \
        #             / float_divisor + 0.076543
        # Rewritten without \ and float()
        numerator_val = (current_f * (1.0000015 + (iteration_count % 11) * 0.0000002) + (iteration_count + 1) * 1.0 / (1000.0 + (iteration_count % 101)))
        current_f = numerator_val / float_divisor + 0.076543
        
        if current_f > 1.0e20:
            # Original: current_f = current_f / (1.0e10 + (iteration_count % 1001)) + iteration_count * 1.0
            current_f = current_f / (1.0e10 + (iteration_count % 1001)) + iteration_count * 1.0
        elif current_f < 1.0e-10:
            if current_f > -1.0e-10:
                if current_f != 0.0:
                    # Ensure RHS is float for multiplication
                    current_f = current_f * (1.0e5 + iteration_count * 1.0)
        
        # Original: if current_f <= 0.0 and iteration_count % 3 != 0:
        if current_f <= 0.0:
            if iteration_count % 3 != 0: # Occasionally allow negative, but mostly positive
                # Original: current_f = 0.0000123 + iteration_count * 1.0 * 0.00000003
                current_f = 0.0000123 + iteration_count * 1.0 * 0.00000003


        # --- Mixed Type Operations with Conditionals ---
        if iteration_count % 3 == 0:
            temp_f_divisor = 234.567 + (iteration_count % 13) * 1.0 # Ensure divisor is float
            # Original: temp_f_val = float(current_i % (2000 + iteration_count % 200)) / temp_f_divisor
            temp_f_val = (current_i % (2000 + iteration_count % 200)) * 1.0 / temp_f_divisor
            current_f += temp_f_val
            
            # Original: current_i += int(current_f * (0.05 + (iteration_count % 7)*0.005)) % \ (15000 + iteration_count % 300)
            # Rewritten without int() and \
            term_for_modulo_i = current_f * (0.05 + (iteration_count % 7) * 0.005) # This is float
            # current_i (int) += float_val % int_val (results in float in Python). Relies on truncation for current_i.
            current_i += term_for_modulo_i % (15000 + iteration_count % 300)
        else:
            # Original: temp_i_val = int(current_f * (3.456 + (iteration_count % 9)*0.002))
            temp_i_val = current_f * (3.456 + (iteration_count % 9) * 0.002) # temp_i_val is now float
            # current_i (int) -= temp_i_val (float) % int_val (results in float). Relies on truncation for current_i.
            current_i -= temp_i_val % (3000 + iteration_count % 400)
            if current_i < 0: 
                current_i = -current_i + iteration_count + 1 # current_i remains int
            
            # Original: mixed_op_float_divisor = float(current_i % (150 + iteration_count % 19)) + 0.123
            mixed_op_float_divisor = (current_i % (150 + iteration_count % 19)) * 1.0 + 0.123
            if mixed_op_float_divisor == 0.0: 
                mixed_op_float_divisor = 0.123
            current_f = current_f / mixed_op_float_divisor
            if current_f == 0.0: 
                 # Original: current_f = 0.0000456 + float(iteration_count) * 0.00000004
                 current_f = 0.0000456 + iteration_count * 1.0 * 0.00000004

        # --- Power Operations ---
        if iteration_count % 7 == 0:
            base_for_float_power = current_f / 200.0 + 0.005
            if base_for_float_power <= 0.0: # Ensure float comparison
                base_for_float_power = 0.001 
            float_exponent = 1.005 + (iteration_count % 10) * 0.001
            current_f = base_for_float_power ** float_exponent
            # Ensure RHS is float for multiplication
            current_f = current_f * (150.0 + (iteration_count % 29) * 1.0) 

            base_for_int_power = (current_i % 6) + 2 
            int_exponent = (iteration_count % 2) + 2
            powered_int = base_for_int_power ** int_exponent
            # Original: current_i = int((current_i + powered_int) / ((iteration_count % 5) + 1))
            # Make divisor float to ensure float division, then rely on truncation for assignment to current_i.
            current_i = (current_i + powered_int) / ((iteration_count % 5) + 1.0) 
            if current_i == 0 : 
                current_i = iteration_count + 3

        iteration_count += 1
    
    # Assign final results to the list
    results_list[0] = current_i
    results_list[1] = current_f
    # No explicit return needed

def main():
    # List to store results from calculate_complex_value
    # results_list[0] will be g_final_i, results_list[1] will be g_final_f
    results_list = [0, 0.0]
  

    # **重要**: 调整此迭代次数以获得合理的运行时间。
    iterations = 200000   # 初始测试值，请根据您的编译器性能调整

    initial_i = 9876543210
    initial_f = 54321.012345

    # 调用计算函数，结果将存储在列表中
    calculate_complex_value(initial_i, initial_f, iterations, results_list)
    
    g_final_i = results_list[0]
    g_final_f = results_list[1]

    print("Number of iterations:")
    print(iterations) # This is a local variable in main
    print("Final integer result:")
    print(g_final_i)
    print("Final float result:")
    print(g_final_f)
    
    return 0