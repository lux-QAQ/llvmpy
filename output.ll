; ModuleID = 'main'
source_filename = "llvmpy_module"

define i32 @main() {
entry:
  %a = alloca ptr, align 8
  %new_list = call ptr @py_create_list(i32 6, i32 1)
  %int_obj = call ptr @py_create_int(i32 2)
  call void @py_list_set_item(ptr %new_list, i32 0, ptr %int_obj)
  %int_obj1 = call ptr @py_create_int(i32 2)
  call void @py_list_set_item(ptr %new_list, i32 1, ptr %int_obj1)
  %int_obj2 = call ptr @py_create_int(i32 1)
  call void @py_list_set_item(ptr %new_list, i32 2, ptr %int_obj2)
  %int_obj3 = call ptr @py_create_int(i32 24)
  call void @py_list_set_item(ptr %new_list, i32 3, ptr %int_obj3)
  %int_obj4 = call ptr @py_create_int(i32 5)
  call void @py_list_set_item(ptr %new_list, i32 4, ptr %int_obj4)
  %int_obj5 = call ptr @py_create_int(i32 3)
  call void @py_list_set_item(ptr %new_list, i32 5, ptr %int_obj5)
  store ptr %new_list, ptr %a, align 8
  %loaded_ptr = load ptr, ptr %a, align 8
  call void @py_print_object(ptr %loaded_ptr)
  %loaded_target_ptr = load ptr, ptr %a, align 8
  %int_obj6 = call ptr @py_create_int(i32 3)
  call void @py_list_set_item(ptr %loaded_target_ptr, i32 1, ptr %int_obj6)
  %loaded_target_ptr7 = load ptr, ptr %a, align 8
  %int_obj8 = call ptr @py_create_int(i32 4)
  call void @py_list_set_item(ptr %loaded_target_ptr7, i32 2, ptr %int_obj8)
  %loaded_target_ptr9 = load ptr, ptr %a, align 8
  %int_obj10 = call ptr @py_create_int(i32 5)
  call void @py_list_set_item(ptr %loaded_target_ptr9, i32 3, ptr %int_obj10)
  %loaded_target_ptr11 = load ptr, ptr %a, align 8
  %int_obj12 = call ptr @py_create_int(i32 2)
  call void @py_list_set_item(ptr %loaded_target_ptr11, i32 0, ptr %int_obj12)
  %loaded_ptr13 = load ptr, ptr %a, align 8
  call void @py_print_object(ptr %loaded_ptr13)
  ret i32 0
}

declare ptr @py_create_list(i32, i32)

declare void @py_list_set_item(ptr, i32, ptr)

declare ptr @py_create_int(i32)

declare void @py_print_object(ptr)
