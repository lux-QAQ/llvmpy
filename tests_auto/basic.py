def func_return(a):
    return a
def func_return_listindex(list,index):
    return list[index]
def func_list(em):
    return [em]
def func_dict(em):
    return {1.2:em}
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
            # 修正递归调用
            return fib(a-1) + fib(a-2)
    return _fib(a)

# --- Test Functions returning True/False ---

def test_fib():
    print("--- Fibonacci Test ---")
    passed = True
    result5 = fib(5)
    expected5 = 5
    if result5 != expected5:
        print("fib(5) FAILED: Expected")
        print(expected5)
        print("Got")
        print(result5)
        passed = False
    result7 = fib(7)
    expected7 = 13
    if result7 != expected7:
        print("fib(7) FAILED: Expected")
        print(expected7)
        print("Got")
        print(result7)
        passed = False
    return passed

def test_arr_test1():
    print("--- arr_test1 ---")
    tmp = [1,2,3]*2
    # 预期 tmp = [1, 2, 3, 1, 2, 3]
    passed = True
    val3 = tmp[3]
    exp3 = 1
    val4 = tmp[4]
    exp4 = 2
    print("tmp[3]:")
    print(val3)
    print("tmp[4]:")
    print(val4)
    if val3 != exp3:
        print("arr_test1 FAILED: tmp[3] Expected")
        print(exp3)
        print("Got")
        print(val3)
        passed = False
    if val4 != exp4:
        print("arr_test1 FAILED: tmp[4] Expected")
        print(exp4)
        print("Got")
        print(val4)
        passed = False
    return passed

def test_arr_test2():
    print("--- arr_test2 ---")
    tmp = [[1,2,3]*2,[4,5,6],{1.2:-1}]*2
    # 预期 tmp = [[1, 2, 3, 1, 2, 3], [4, 5, 6], {1.2: -1}, [1, 2, 3, 1, 2, 3], [4, 5, 6], {1.2: -1}]
    passed = True
    val0_3 = tmp[0][3]
    exp0_3 = 1
    val5_1_2 = tmp[5][1.2]
    exp5_1_2 = -1
    print("tmp[0][3]:")
    print(val0_3)
    print("tmp[5][1.2]:")
    print(val5_1_2)
    if val0_3 != exp0_3:
        print("arr_test2 FAILED: tmp[0][3] Expected")
        print(exp0_3)
        print("Got")
        print(val0_3)
        passed = False
    if val5_1_2 != exp5_1_2:
         print("arr_test2 FAILED: tmp[5][1.2] Expected")
         print(exp5_1_2)
         print("Got")
         print(val5_1_2)
         passed = False
    return passed

def test_func_redefine():
    print("--- func_redefine ---")
    def test():
        print("not me")
        return "not me"
    def test():
        print("its me")
        return "its me"
    first_call_result = test() # 打印一次 "its me"
    returned_func = test # 返回的是后面定义的 test
    second_call_result = returned_func() # 打印一次 "its me"
    expected_result = "its me"
    passed = True
    if first_call_result != expected_result:
        print("func_redefine FAILED: First call Expected")
        print(expected_result)
        print("Got")
        print(first_call_result)
        passed = False
    if second_call_result != expected_result:
        print("func_redefine FAILED: Second call Expected")
        print(expected_result)
        print("Got")
        print(second_call_result)
        passed = False
    return passed

def test_list_modification():
    print("--- list_modification_test ---")
    my_list = [10, 20, 30]
    print("Initial my_list[1]:")
    print(my_list[1])
    my_list[1] = 25 # 修改元素
    print("After my_list[1] = 25:")
    print(my_list[1])
    my_list[0] = my_list[2] # 使用其他元素赋值 (my_list[2] is 30)
    print("After my_list[0] = my_list[2]:")
    print(my_list[0])
    # 预期 my_list = [30, 25, 30]
    passed = True
    exp0 = 30
    exp1 = 25
    exp2 = 30
    #exp0, exp1, exp2 = 30, 25, 30
    if my_list[0] != exp0:
        print("list_modification FAILED: my_list[0] Expected")
        print(exp0)
        print("Got")
        print(my_list[0])
        passed = False
    if my_list[1] != exp1:
        print("list_modification FAILED: my_list[1] Expected")
        print(exp1)
        print("Got")
        print(my_list[1])
        passed = False
    if my_list[2] != exp2:
        print("list_modification FAILED: my_list[2] Expected")
        print(exp2)
        print("Got")
        print(my_list[2])
        passed = False
    return passed

def test_dict_modification():
    print("--- dict_modification_test ---")
    my_dict = {"a": 1, "b": 2.5}
    print("Initial my_dict['a']:")
    print(my_dict["a"])
    my_dict["a"] = 100 # 修改现有键的值
    print("After my_dict['a'] = 100:")
    print(my_dict["a"])
    my_dict["c"] = "hello" # 添加新键值对
    print("After my_dict['c'] = 'hello':")
    print(my_dict["c"])
    my_dict["b"] = my_dict["a"] # 使用其他值赋值 (my_dict["a"] is 100)
    print("After my_dict['b'] = my_dict['a']:")
    print(my_dict["b"])
    new_key = "d"
    my_dict[new_key] = 500
    print("After my_dict[new_key] = 500:")
    print(my_dict["d"])
    # 预期 my_dict = {'a': 100, 'b': 100, 'c': 'hello', 'd': 500}
    passed = True
    exp_a = 100
    exp_b = 100
    exp_c = "hello"
    exp_d = 500
    #exp_a, exp_b, exp_c, exp_d = 100, 100, "hello", 500
    if my_dict["a"] != exp_a:
        print("dict_modification FAILED: my_dict['a'] Expected")
        print(exp_a)
        print("Got")
        print(my_dict["a"])
        passed = False
    if my_dict["b"] != exp_b:
        print("dict_modification FAILED: my_dict['b'] Expected")
        print(exp_b)
        print("Got")
        print(my_dict["b"])
        passed = False
    if my_dict["c"] != exp_c:
        print("dict_modification FAILED: my_dict['c'] Expected")
        print(exp_c)
        print("Got")
        print(my_dict["c"])
        passed = False
    # 假设直接访问有效：
    if my_dict["d"] != exp_d:
         print("dict_modification FAILED: my_dict['d'] Expected")
         print(exp_d)
         print("Got")
         print(my_dict["d"])
         passed = False
    # 注意：这里没有检查是否有多余的键
    return passed

def test_nested_container_access():
    print("--- nested_container_access_test ---")
    nested_list = [1, [2, 3], {"x": 4, "y": [5, 6]}]
    nested_list[1][1] = 33 # 修改嵌套列表中的元素
    print("After nested_list[1][1] = 33:")
    print(nested_list[1][1])
    nested_list[2]["y"][0] = 55 # 修改嵌套字典中列表的元素
    print("After nested_list[2]['y'][0] = 55:")
    print(nested_list[2]["y"][0])
    nested_list[2]["z"] = [7, 8] # 在嵌套字典中添加新键值对
    print("After nested_list[2]['z'] = [7, 8]:")
    print(nested_list[2]["z"][0]) # 假设访问新列表的第一个元素
    nested_list[2]["z"][1] = 88 # 修改新添加的嵌套列表元素
    print("After nested_list[2]['z'][1] = 88:")
    print(nested_list[2]["z"][1])
    # 预期 nested_list = [1, [2, 33], {'x': 4, 'y': [55, 6], 'z': [7, 88]}]
    passed = True
    exp_1_1 = 33
    exp_2_y_0 = 55
    exp_2_z_1 = 88
    if nested_list[1][1] != exp_1_1:
        print("nested_container FAILED: nested_list[1][1] Expected")
        print(exp_1_1)
        print("Got")
        print(nested_list[1][1])
        passed = False
    if nested_list[2]["y"][0] != exp_2_y_0:
        print("nested_container FAILED: nested_list[2]['y'][0] Expected")
        print(exp_2_y_0)
        print("Got")
        print(nested_list[2]["y"][0])
        passed = False
    if nested_list[2]["z"][1] != exp_2_z_1:
        print("nested_container FAILED: nested_list[2]['z'][1] Expected")
        print(exp_2_z_1)
        print("Got")
        print(nested_list[2]["z"][1])
        passed = False
    # 检查其他未修改部分是否保持不变
    if nested_list[0] != 1:
        passed = False
        print("nested_container FAILED: nested_list[0] changed")
    if nested_list[1][0] != 2:
        passed = False
        print("nested_container FAILED: nested_list[1][0] changed")
    if nested_list[2]["x"] != 4:
        passed = False
        print("nested_container FAILED: nested_list[2]['x'] changed")
    if nested_list[2]["y"][1] != 6:
        passed = False
        print("nested_container FAILED: nested_list[2]['y'][1] changed")
    if nested_list[2]["z"][0] != 7:
        passed = False
        print("nested_container FAILED: nested_list[2]['z'][0] changed")
    return passed

def test_container_assignment():
    print("--- container_assignment_test ---")
    list_a = [1, 2]
    list_b = list_a
    list_b[0] = 11
    print("list_a[0]:")
    print(list_a[0]) # 预期 11
    print("list_b[0]:")
    print(list_b[0]) # 预期 11
    passed = True
    if list_a[0] != 11:
        print("container_assignment FAILED: list_a[0] Expected 11 Got")
        print(list_a[0])
        passed = False
    if list_a[1] != 2: # 检查未修改部分
        print("container_assignment FAILED: list_a[1] Expected 2 Got")
        print(list_a[1])
        passed = False

    dict_a = {"k1": 10}
    dict_b = dict_a
    dict_b["k1"] = 110
    dict_b["k2"] = 220
    print("dict_a['k1']:")
    print(dict_a["k1"]) # 预期 110
    print("dict_b['k1']:")
    print(dict_b["k1"]) # 预期 110
    print("dict_a['k2']:")
    print(dict_a["k2"]) # 预期 220
    if dict_a["k1"] != 110:
        print("container_assignment FAILED: dict_a['k1'] Expected 110 Got")
        print(dict_a["k1"])
        passed = False
    if dict_a["k2"] != 220:
        print("container_assignment FAILED: dict_a['k2'] Expected 220 Got")
        print(dict_a["k2"])
        passed = False
    return passed

def test_complex_list_while():
    print("--- complex_list_while_test ---")
    data = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
    i = 0
    limit = 10
    while i < limit:
        if data[i] % 2 == 0:
            data[i] = data[i] * 10
        else:
            data[i] = data[i] + 100
        # print(data[i]) # 减少打印
        i = i + 1
    print("Final data list:") # 打印最终列表用于调试
    print(data)
    # 预期 data = [0, 101, 20, 103, 40, 105, 60, 107, 80, 109]
    passed = True
    expected_values = [0, 101, 20, 103, 40, 105, 60, 107, 80, 109]
    check_idx = 0
    while check_idx < limit:
        if data[check_idx] != expected_values[check_idx]:
            print("complex_list_while FAILED: data index")
            print(check_idx)
            print("Expected")
            print(expected_values[check_idx])
            print("Got")
            print(data[check_idx])
            passed = False
            # 可以选择 break 来停止进一步检查，或者检查所有元素
        check_idx = check_idx + 1
    return passed

def test_complex_dict_while():
    print("--- complex_dict_while_test ---")
    data = {"counter": 0, "limit": 5, "values": {}}
    # First loop: populate values
    while data["counter"] < data["limit"]:
        current_count = data["counter"]
        data["values"][current_count] = current_count * current_count
        # print(data["values"][current_count])
        data["counter"] = data["counter"] + 1
    print("Data after first loop:") # 调试用
    print(data)
    # 预期 data = {'counter': 5, 'limit': 5, 'values': {0: 0, 1: 1, 2: 4, 3: 9, 4: 16}}

    # Second loop: modify values
    j = 0
    while j < data["limit"]:
         if data["values"][j] > 10:
             data["values"][j] = -1 # Values 16 (j=4) will become -1
         j = j + 1
    print("Final data dict:") # 调试用
    print(data)
    # 预期 data = {'counter': 5, 'limit': 5, 'values': {0: 0, 1: 1, 2: 4, 3: 9, 4: -1}}
    passed = True
    if data["counter"] != 5:
        print("complex_dict_while FAILED: data['counter'] Expected 5 Got")
        print(data["counter"])
        passed = False
    if data["limit"] != 5: # 检查 limit 是否意外改变
        print("complex_dict_while FAILED: data['limit'] Expected 5 Got")
        print(data["limit"])
        passed = False

    expected_values = {0: 0, 1: 1, 2: 4, 3: 9, 4: -1}
    check_key = 0
    while check_key < data["limit"]: # 假设键是 0 到 limit-1
        if data["values"][check_key] != expected_values[check_key]:
             print("complex_dict_while FAILED: data['values'] key")
             print(check_key)
             print("Expected")
             print(expected_values[check_key])
             print("Got")
             print(data["values"][check_key])
             passed = False
        check_key = check_key + 1
    # 注意：这里没有检查 values 是否包含多余的键
    return passed

def test_nested_loop_modification():
    print("--- nested_loop_modification_test ---")
    nested_list = [[1, 2, 3], [4, 5, 6], [7, 8, 9]]
    outer_limit = 3
    inner_limit = 3
    i = 0
    while i < outer_limit:
        j = 0
        while j < inner_limit:
            nested_list[i][j] = nested_list[i][j] * (i + 1) + j
            j = j + 1
        i = i + 1
    # 预期 after loops: [[1, 3, 5], [8, 11, 14], [21, 25, 29]]
    print("List after loops:") # 调试用
    print(nested_list)
    nested_list[1][1] = nested_list[0][0] + nested_list[2][2] # 1 + 29 = 30
    print("Final list:") # 调试用
    print(nested_list)
    # 预期 final: [[1, 3, 5], [8, 30, 14], [21, 25, 29]]
    passed = True
    expected_final = [[1, 3, 5], [8, 30, 14], [21, 25, 29]]
    outer_idx = 0
    while outer_idx < outer_limit:
        inner_idx = 0
        while inner_idx < inner_limit:
            if nested_list[outer_idx][inner_idx] != expected_final[outer_idx][inner_idx]:
                print("nested_loop FAILED: list index")
                print(outer_idx)
                print(inner_idx)
                print("Expected")
                print(expected_final[outer_idx][inner_idx])
                print("Got")
                print(nested_list[outer_idx][inner_idx])
                passed = False
            inner_idx = inner_idx + 1
        outer_idx = outer_idx + 1
    return passed

def test_dict_list_interaction():
    print("--- dict_list_interaction_test ---")
    data = {"items": [10, 20, 30], "status": "initial", "nested": {"a": 1}}
    # 修改列表元素
    i = 0
    limit = 3
    while i < limit:
        data["items"][i] = data["items"][i] + i
        i = i + 1
    # 预期 items: [10, 21, 32]
    data["status"] = "processing"
    data["results"] = [100, 200]
    data["results"][0] = data["items"][0] # results[0] = 10
    # 预期 results: [10, 200]
    data["nested"]["a"] = data["nested"]["a"] + 5 # nested.a = 6
    data["nested"]["b"] = data["items"][2] # nested.b = 32
    # 预期 nested: {'a': 6, 'b': 32}
    print("Final data dict:") # 调试用
    print(data)
    passed = True
    # 检查 items
    expected_items = [10, 21, 32]
    item_idx = 0
    while item_idx < limit:
        if data["items"][item_idx] != expected_items[item_idx]:
            print("dict_list FAILED: items index")
            print(item_idx)
            print("Expected")
            print(expected_items[item_idx])
            print("Got")
            print(data["items"][item_idx])
            passed = False
        item_idx = item_idx + 1
    # 检查 status
    if data["status"] != "processing":
        print("dict_list FAILED: status Expected 'processing' Got")
        print(data["status"])
        passed = False
    # 检查 results
    expected_results = [10, 200]
    if data["results"][0] != expected_results[0]:
        print("dict_list FAILED: results[0] Expected")
        print(expected_results[0])
        print("Got")
        print(data["results"][0])
        passed = False
    if data["results"][1] != expected_results[1]:
        print("dict_list FAILED: results[1] Expected")
        print(expected_results[1])
        print("Got")
        print(data["results"][1])
        passed = False
    # 检查 nested
    if data["nested"]["a"] != 6:
        print("dict_list FAILED: nested['a'] Expected 6 Got")
        print(data["nested"]["a"])
        passed = False
    if data["nested"]["b"] != 32:
        print("dict_list FAILED: nested['b'] Expected 32 Got")
        print(data["nested"]["b"])
        passed = False
    return passed

def test_list_dict_interaction():
    print("--- list_dict_interaction_test ---")
    data = [{"id": 0, "value": 1.0}, {"id": 1, "value": 2.5}, {"id": 2, "value": 0.5}]
    i = 0
    limit = 3
    while i < limit:
        current_dict = data[i]
        current_dict["value"] = current_dict["value"] * 10
        current_dict["processed"] = True
        if current_dict["id"] == 1:
            current_dict["special"] = "yes"
        # data[i] = current_dict # 引用修改，这行非必需
        # print(data[i])
        i = i + 1
    # 预期 after loop: [{'id': 0, 'value': 10.0, 'processed': True}, {'id': 1, 'value': 25.0, 'processed': True, 'special': 'yes'}, {'id': 2, 'value': 5.0, 'processed': True}]
    data[0]["value"] = data[1]["value"] + data[2]["value"] # 25.0 + 5.0 = 30.0
    # 预期 final: [{'id': 0, 'value': 30.0, 'processed': True}, {'id': 1, 'value': 25.0, 'processed': True, 'special': 'yes'}, {'id': 2, 'value': 5.0, 'processed': True}]
    print("Final data list:") # 调试用
    print(data)
    passed = True
    # Check data[0]
    if data[0]["id"] != 0:
        passed = False
        print("list_dict FAILED: data[0]['id']")
    if data[0]["value"] != 30.0:
        passed = False
        print("list_dict FAILED: data[0]['value'] Expected 30.0 Got")
        print(data[0]["value"])
    if data[0]["processed"] != True: # 假设布尔值比较有效
        passed = False
        print("list_dict FAILED: data[0]['processed']")
    # Check data[1]
    if data[1]["id"] != 1:
        passed = False
        print("list_dict FAILED: data[1]['id']")
    if data[1]["value"] != 25.0:
        passed = False
        print("list_dict FAILED: data[1]['value'] Expected 25.0 Got")
        print(data[1]["value"])
    if data[1]["processed"] != True:
        passed = False
        print("list_dict FAILED: data[1]['processed']")
    if data[1]["special"] != "yes":
        passed = False
        print("list_dict FAILED: data[1]['special']")
    # Check data[2]
    if data[2]["id"] != 2:
        passed = False
        print("list_dict FAILED: data[2]['id']")
    if data[2]["value"] != 5.0:
        passed = False
        print("list_dict FAILED: data[2]['value'] Expected 5.0 Got")
        print(data[2]["value"])
    if data[2]["processed"] != True:
        passed = False
        print("list_dict FAILED: data[2]['processed']")
    # 注意：没有检查字典中是否有多余的键
    return passed


def main():
    all_passed = True

    # --- Run Tests ---
    print("--- Running Basic Tests ---")

    # Test Simple Functions & Fibonacci (保留一些简单打印)
    print("func_return(1):")
    print(func_return(1))
    print("func_return_listindex([1,2,3],1):")
    print(func_return_listindex([1,2,3],1))
    test3 = func_list(1.2)
    print("func_list(1.2):")
    print(test3)
    test4= func_dict(5)
    print("func_dict(5):")
    print(test4)

    if test_fib():
        print("\033[32m测试 Fibonacci - PASSED\033[0m\n")
    else:
        print("\033[31m测试 Fibonacci - FAILED\033[0m\n")
        all_passed = False

    # Test Array/List Operations
    if test_arr_test1():
        print("\033[32m测试 arr_test1 - PASSED\033[0m\n")
    else:
        print("\033[31m测试 arr_test1 - FAILED\033[0m\n")
        all_passed = False

    if test_arr_test2():
        print("\033[32m测试 arr_test2 - PASSED\033[0m\n")
    else:
        print("\033[31m测试 arr_test2 - FAILED\033[0m\n")
        all_passed = False

    # Test Function Redefinition
    if test_func_redefine():
         print("\033[32m测试 func_redefine - PASSED\033[0m\n")
    else:
         print("\033[31m测试 func_redefine - FAILED\033[0m\n")
         all_passed = False

    # Test Modification Operations
    if test_list_modification():
        print("\033[32m测试 list_modification - PASSED\033[0m\n")
    else:
        print("\033[31m测试 list_modification - FAILED\033[0m\n")
        all_passed = False

    if test_dict_modification():
        print("\033[32m测试 dict_modification - PASSED\033[0m\n")
    else:
        print("\033[31m测试 dict_modification - FAILED\033[0m\n")
        all_passed = False

    if test_nested_container_access():
        print("\033[32m测试 nested_container_access - PASSED\033[0m\n")
    else:
        print("\033[31m测试 nested_container_access - FAILED\033[0m\n")
        all_passed = False

    if test_container_assignment():
        print("\033[32m测试 container_assignment - PASSED\033[0m\n")
    else:
        print("\033[31m测试 container_assignment - FAILED\033[0m\n")
        all_passed = False

    # Test Complex Loop & Interaction
    if test_complex_list_while():
        print("\033[32m测试 complex_list_while - PASSED\033[0m\n")
    else:
        print("\033[31m测试 complex_list_while - FAILED\033[0m\n")
        all_passed = False

    if test_complex_dict_while():
        print("\033[32m测试 complex_dict_while - PASSED\033[0m\n")
    else:
        print("\033[31m测试 complex_dict_while - FAILED\033[0m\n")
        all_passed = False

    if test_nested_loop_modification():
        print("\033[32m测试 nested_loop_modification - PASSED\033[0m\n")
    else:
        print("\033[31m测试 nested_loop_modification - FAILED\033[0m\n")
        all_passed = False

    if test_dict_list_interaction():
        print("\033[32m测试 dict_list_interaction - PASSED\033[0m\n")
    else:
        print("\033[31m测试 dict_list_interaction - FAILED\033[0m\n")
        all_passed = False

    if test_list_dict_interaction():
        print("\033[32m测试 list_dict_interaction - PASSED\033[0m\n")
    else:
        print("\033[31m测试 list_dict_interaction - FAILED\033[0m\n")
        all_passed = False

    # --- Final Summary ---
    print("\n--- Basic Tests Summary ---")
    if all_passed:
        print("\033[32m所有 basic 测试均通过\033[0m")
        return 0 # Indicate success
    else:
        print("\033[31m有 basic 测试未通过\033[0m")
        return 1 # Indicate failure