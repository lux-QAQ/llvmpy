def func_return(a):
    return a
def func_return_listindex(list,index):
    return list[index]
def func_list(em):
    return [em]
def func_dict(em):
    return {1.2:em}
def func_fib(em):
    if em <= 0:
        return 0
    elif em == 1:
        return 1
    elif em == 2:
        return 1
    else:
        return func_fib(em-1) + func_fib(em-2)
def arr_test1():
    print("arr_test1")
    tmp = [1,2,3]*2
    print(tmp[3])
    print(tmp[4])
    return 0
def arr_test2():
    print("arr_test2")
    tmp = [[1,2,3]*2,[4,5,6],{1.2:-1}]*2
    print(tmp[0][3])
    print(tmp[5][1.2])
    return 0
    


def list_modification_test():
    print("list_modification_test")
    my_list = [10, 20, 30]
    print(my_list[1])
    my_list[1] = 25 # 修改元素
    print(my_list[1])
    my_list[0] = my_list[2] # 使用其他元素赋值
    print(my_list[0])
    return 0

def dict_modification_test():
    print("dict_modification_test")
    my_dict = {"a": 1, "b": 2.5}
    print(my_dict["a"])
    my_dict["a"] = 100 # 修改现有键的值
    print(my_dict["a"])
    my_dict["c"] = "hello" # 添加新键值对 (隐式创建)
    print(my_dict["c"])
    my_dict["b"] = my_dict["a"] # 使用其他值赋值
    print(my_dict["b"])
    # 使用变量作为键添加新键值对
    new_key = "d"
    my_dict[new_key] = 500
    print(my_dict["d"])
    return 0

def nested_container_access_test():
    print("nested_container_access_test")
    nested_list = [1, [2, 3], {"x": 4, "y": [5, 6]}]
    print(nested_list[0])      # 访问顶层元素
    print(nested_list[1][0])   # 访问嵌套列表的元素
    print(nested_list[2]["x"]) # 访问嵌套字典的值
    print(nested_list[2]["y"][1]) # 访问嵌套字典中列表的元素
    nested_list[1][1] = 33 # 修改嵌套列表中的元素
    print(nested_list[1][1])
    nested_list[2]["y"][0] = 55 # 修改嵌套字典中列表的元素
    print(nested_list[2]["y"][0])
    # 在嵌套字典中添加新键值对
    nested_list[2]["z"] = [7, 8]
    print(nested_list[2]["z"][0])
    # 修改新添加的嵌套列表元素
    nested_list[2]["z"][1] = 88
    print(nested_list[2]["z"][1])
    return 0

def container_assignment_test():
    print("container_assignment_test")
    list_a = [1, 2]
    list_b = list_a # 赋值（在 Python 中是引用）
    list_b[0] = 11
    print(list_a[0]) # 检查 list_a 是否也被修改
    print(list_b[0])

    dict_a = {"k1": 10}
    dict_b = dict_a
    dict_b["k1"] = 110
    print(dict_a["k1"]) # 检查 dict_a 是否也被修改
    print(dict_b["k1"])
    dict_b["k2"] = 220 # 添加到 dict_b (隐式创建)
    print(dict_a["k2"]) # 检查是否也出现在 dict_a 中
    return 0

# --- 新增复杂测试用例 ---

def complex_list_while_test():
    print("complex_list_while_test")
    # 注意：由于没有 append，我们预先创建足够大的列表或仅修改现有元素
    # 这里我们演示修改现有元素
    data = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
    i = 0
    limit = 10 # 假设 len(data) 是 10
    while i < limit:
        if data[i] % 2 == 0:
            data[i] = data[i] * 10
        else:
            data[i] = data[i] + 100
        print(data[i]) # 打印中间结果
        i = i + 1
    print(data) # 打印最终列表
    return 0

def complex_dict_while_test():
    print("complex_dict_while_test")
    data = {"counter": 0, "limit": 5, "values": {}}
    i = 0
    while data["counter"] < data["limit"]:
        key_str = "key" # 简单起见，用固定前缀+数字作键 (需要字符串拼接支持，否则用数字作键)
        # 假设有简单的数字转字符串或直接用数字键
        # data["values"][data["counter"]] = data["counter"] * data["counter"] # 使用数字键
        # 或者如果字符串拼接和转换支持:
        # key_str = "key_" + str(data["counter"]) # 需要 str() 和 + 支持
        # data["values"][key_str] = data["counter"] * data["counter"]
        
        # 简化：使用数字作为键，并演示隐式创建
        current_count = data["counter"]
        data["values"][current_count] = current_count * current_count 
        print(data["values"][current_count])

        # 修改顶层 counter
        data["counter"] = data["counter"] + 1
        
    print(data)
    # 再次循环，修改嵌套字典的值
    j = 0
    while j < data["limit"]: # 使用更新后的 limit (虽然这里没变)
         if data["values"][j] > 10:
             data["values"][j] = -1
         j = j + 1
    print(data)
    return 0

def nested_loop_modification_test():
    print("nested_loop_modification_test")
    # 假设列表长度已知或固定
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
    print(nested_list)
    # 访问并修改特定嵌套元素
    nested_list[1][1] = nested_list[0][0] + nested_list[2][2]
    print(nested_list)
    return 0

def dict_list_interaction_test():
    print("dict_list_interaction_test")
    data = {"items": [10, 20, 30], "status": "initial", "nested": {"a": 1}}
    
    # 修改列表元素
    i = 0
    limit = 3 # 假设列表长度已知
    while i < limit:
        data["items"][i] = data["items"][i] + i
        i = i + 1
    print(data["items"])

    # 修改状态
    data["status"] = "processing"
    print(data["status"])

    # 添加新键值对，值为列表
    data["results"] = [100, 200]
    print(data["results"])

    # 修改新列表的元素
    data["results"][0] = data["items"][0] # 使用其他列表元素赋值
    print(data["results"])

    # 修改嵌套字典的值
    data["nested"]["a"] = data["nested"]["a"] + 5
    print(data["nested"]["a"])
    
    # 在嵌套字典中添加新键
    data["nested"]["b"] = data["items"][2] # 使用列表元素赋值
    print(data["nested"])

    print(data)
    return 0

def list_dict_interaction_test():
    print("list_dict_interaction_test")
    data = [{"id": 0, "value": 1.0}, {"id": 1, "value": 2.5}, {"id": 2, "value": 0.5}]
    i = 0
    limit = 3 # 假设列表长度已知
    
    while i < limit:
        current_dict = data[i] # 引用字典
        current_dict["value"] = current_dict["value"] * 10
        # 添加新键值对到字典中 (隐式创建)
        current_dict["processed"] = True 
        # 基于id添加条件键
        if current_dict["id"] == 1:
            current_dict["special"] = "yes"
        
        # 修改回列表（虽然是引用，效果已生效，但演示显式写回）
        data[i] = current_dict 
        print(data[i])
        i = i + 1
        
    print(data)
    # 访问和修改特定字典的特定键
    data[0]["value"] = data[1]["value"] + data[2]["value"]
    print(data[0])
    return 0


def main():
    print("--- Basic Tests ---")
    print(func_return(1))
    print(func_return_listindex([1,2,3],1))
    test3 = func_list(1.2)
    print(test3)
    test4= func_dict(5)
    print(test4)
    print(func_fib(5))
    # print(func_fib(6)) # 注释掉一些减少输出
    # print(func_fib(7))

    print("\n--- Array/List Tests ---")
    arr_test1()
    arr_test2()

    print("\n--- Modification Tests ---")
    list_modification_test()
    dict_modification_test()
    nested_container_access_test()
    container_assignment_test()

    print("\n--- Complex Loop & Interaction Tests ---")
    complex_list_while_test()
    complex_dict_while_test()
    nested_loop_modification_test()
    dict_list_interaction_test()
    list_dict_interaction_test()

    print("\n--- All Tests Done ---")
    return 0
