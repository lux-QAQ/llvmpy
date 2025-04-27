def func_return(a): 
     return a 
 def func_return_listindex(list, index): 
     return list[index] 
 def func_list(em): 
     return[em] 
 def func_dict(em): 
     return {1.2: em} 
 def func_fib(em): 
     if em <= 0: 
         return 0 
     elif em == 1: 
         return 1 
     elif em == 2: 
         return 1 
     else: 
         return func_fib(em - 1) + func_fib(em - 2) 
 def arr_test1(): 
     print("arr_test1") 
     tmp = [1, 2, 3] * 2 
     print(tmp[3]) 
     print(tmp[4]) 
     return 0 
 def arr_test2(): 
     print("arr_test2") 
     tmp = [[1, 2, 3] * 2, [4, 5, 6], {1.2: - 1}] * 2 
     print(tmp[0] [3]) 
     print(tmp[5] [1.2]) 
     return 0 

 def list_modification_test(): 
     print("list_modification_test") 
     my_list = [10, 20, 30] 
     print(my_list[1]) 
     my_list[1] = 25 
     print(my_list[1]) 
     my_list[0] = my_list[2] 
     print(my_list[0]) 
     return 0 
 def dict_modification_test(): 
     print("dict_modification_test") 
     my_dict = {"a": 1, "b": 2.5} 
     print(my_dict["a"]) 
     my_dict["a"] = 100 
     print(my_dict["a"]) 
     my_dict["c"] = "hello" 
     print(my_dict["c"]) 
     my_dict["b"] = my_dict["a"] 
     print(my_dict["b"]) 

     new_key = "d" 
     my_dict[new_key] = 500 
     print(my_dict["d"]) 
     return 0 
 def nested_container_access_test(): 
     print("nested_container_access_test") 
     nested_list = [1, [2, 3], {"x": 4, "y": [5, 6]}] 
     print(nested_list[0]) 
     print(nested_list[1] [0]) 
     print(nested_list[2] ["x"]) 
     print(nested_list[2] ["y"] [1]) 
     nested_list[1] [1] = 33 
     print(nested_list[1] [1]) 
     nested_list[2] ["y"] [0] = 55 
     print(nested_list[2] ["y"] [0]) 

     nested_list[2] ["z"] = [7, 8] 
     print(nested_list[2] ["z"] [0]) 

     nested_list[2] ["z"] [1] = 88 
     print(nested_list[2] ["z"] [1]) 
     return 0 
 def container_assignment_test(): 
     print("container_assignment_test") 
     list_a = [1, 2] 
     list_b = list_a 
     list_b[0] = 11 
     print(list_a[0]) 
     print(list_b[0]) 
     dict_a = {"k1": 10} 
     dict_b = dict_a 
     dict_b["k1"] = 110 
     print(dict_a["k1"]) 
     print(dict_b["k1"]) 
     dict_b["k2"] = 220 
     print(dict_a["k2"]) 
     return 0 

 def complex_list_while_test(): 
     print("complex_list_while_test") 


     data = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9] 
     i = 0 
     limit = 10 
     while i < limit: 
         if data[i] % 2 == 0: 
             data[i] = data[i] * 10 
         else: 
             data[i] = data[i] + 100 
         print(data[i]) 
         i = i + 1 
     print(data) 
     return 0 
 def complex_dict_while_test(): 
     print("complex_dict_while_test") 
     data = {"counter": 0, "limit": 5, "values": {}} 
     i = 0 
     while data["counter"] < data["limit"]: 

         key_str = "key" 







         current_count = data["counter"] 
         data["values"] [current_count] = current_count * current_count 
         print(data["values"] [current_count]) 

         data["counter"] = data["counter"] + 1 

     print(data) 

     j = 0 
     while j < data["limit"]: 
         if data["values"] [j] > 10: 
             data["values"] [j] = - 1 
         j = j + 1 
     print(data) 
     return 0 
 def nested_loop_modification_test(): 
     print("nested_loop_modification_test") 

     nested_list = [[1, 2, 3], [4, 5, 6], [7, 8, 9]] 
     outer_limit = 3 
     inner_limit = 3 
     i = 0 
     while i < outer_limit: 
         j = 0 
         while j < inner_limit: 
             nested_list[i] [j] = nested_list[i] [j] * (i + 1) + j 
             j = j + 1 
         i = i + 1 
     print(nested_list) 

     nested_list[1] [1] = nested_list[0] [0] + nested_list[2] [2] 
     print(nested_list) 
     return 0 
 def dict_list_interaction_test(): 
     print("dict_list_interaction_test") 
     data = {"items": [10, 20, 30], "status": "initial", "nested": {"a": 1}} 


     i = 0 
     limit = 3 
     while i < limit: 
         data["items"] [i] = data["items"] [i] + i 
         i = i + 1 
     print(data["items"]) 

     data["status"] = "processing" 
     print(data["status"]) 

     data["results"] = [100, 200] 
     print(data["results"]) 

     data["results"] [0] = data["items"] [0] 
     print(data["results"]) 

     data["nested"] ["a"] = data["nested"] ["a"] + 5 
     print(data["nested"] ["a"]) 


     data["nested"] ["b"] = data["items"] [2] 
     print(data["nested"]) 
     print(data) 
     return 0 
 def list_dict_interaction_test(): 
     print("list_dict_interaction_test") 
     data = [{"id": 0, "value": 1.0}, {"id": 1, "value": 2.5}, {"id": 2, "value": 0.5}] 
     i = 0 
     limit = 3 

     while i < limit: 
         current_dict = data[i] 
         current_dict["value"] = current_dict["value"] * 10 

         current_dict["processed"] = True 

         if current_dict["id"] == 1: 
             current_dict["special"] = "yes" 


         data[i] = current_dict 
         print(data[i]) 
         i = i + 1 

     print(data) 

     data[0] ["value"] = data[1] ["value"] + data[2] ["value"] 
     print(data[0]) 
     return 0 
 def main(): 
     print("--- Basic Tests ---") 
     print(func_return(1)) 
     print(func_return_listindex([1, 2, 3], 1)) 
     test3 = func_list(1.2) 
     print(test3) 
     test4 = func_dict(5) 
     print(test4) 
     print(func_fib(5)) 


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
     print(1.22) 
     return 0 
 main() 
