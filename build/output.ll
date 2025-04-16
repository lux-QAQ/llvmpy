; ModuleID = 'llvmpy_module'
source_filename = "llvmpy_module"

declare void @py_print_int(i32)

declare void @py_print_double(double)

declare void @py_print_string(ptr)

declare ptr @py_create_int(i32)

declare ptr @py_create_double(double)

declare ptr @py_create_bool(i1)

declare ptr @py_create_string(ptr)

declare ptr @py_object_add(ptr, ptr)

declare ptr @py_object_index(ptr, ptr)

declare void @py_incref(ptr)

declare void @py_decref(ptr)

declare i1 @py_check_type(ptr, i32)

declare ptr @py_get_none()

define internal void @__module_init() {
entry:
  ret void
}

define ptr @main() {
entry:
  %int_obj = call ptr @py_create_int(i32 1)
  %int_obj1 = call ptr @py_create_int(i32 2)
  %list_obj = call ptr @py_create_list(i32 2, i32 1)
  call void @py_list_set_item(ptr %list_obj, i32 0, ptr %int_obj)
  call void @py_incref(ptr %int_obj)
  call void @py_list_set_item(ptr %list_obj, i32 1, ptr %int_obj1)
  call void @py_incref(ptr %int_obj1)
  %int_obj2 = call ptr @py_create_int(i32 1)
  %index_result = call ptr @py_object_index(ptr %list_obj, ptr %int_obj2)
  %str_obj = call ptr @py_convert_to_string(ptr %index_result)
  %str_ptr = call ptr @py_extract_string(ptr %str_obj)
  call void @py_print_string(ptr %str_ptr)
  call void @py_decref(ptr %str_obj)
  %int_obj3 = call ptr @py_create_int(i32 1)
  %int_obj4 = call ptr @py_create_int(i32 3)
  call void @py_object_set_index(ptr %list_obj, ptr %int_obj3, ptr %int_obj4)
  %int_obj5 = call ptr @py_create_int(i32 1)
  %index_result6 = call ptr @py_object_index(ptr %list_obj, ptr %int_obj5)
  %str_obj7 = call ptr @py_convert_to_string(ptr %index_result6)
  %str_ptr8 = call ptr @py_extract_string(ptr %str_obj7)
  call void @py_print_string(ptr %str_ptr8)
  call void @py_decref(ptr %str_obj7)
  %dict_obj = call ptr @py_create_dict(i32 8, i32 1)
  %int_obj9 = call ptr @py_create_int(i32 1)
  %int_obj10 = call ptr @py_create_int(i32 2)
  call void @py_dict_set_item(ptr %dict_obj, ptr %int_obj9, ptr %int_obj10)
  %int_obj11 = call ptr @py_create_int(i32 2)
  %int_obj12 = call ptr @py_create_int(i32 3)
  call void @py_dict_set_item(ptr %dict_obj, ptr %int_obj11, ptr %int_obj12)
  call void @py_decref(ptr %list_obj)
  %int_obj13 = call ptr @py_create_int(i32 2)
  %int_obj14 = call ptr @py_create_int(i32 1)
  %index_result15 = call ptr @py_object_index(ptr %dict_obj, ptr %int_obj14)
  %int_obj16 = call ptr @py_create_int(i32 2)
  %any_op_result = call ptr @py_object_add(ptr %index_result15, ptr %int_obj16), !py.type !0
  call void @py_object_set_index(ptr %dict_obj, ptr %int_obj13, ptr %any_op_result)
  %int_obj17 = call ptr @py_create_int(i32 2)
  %index_result18 = call ptr @py_object_index(ptr %dict_obj, ptr %int_obj17)
  %str_obj19 = call ptr @py_convert_to_string(ptr %index_result18)
  %str_ptr20 = call ptr @py_extract_string(ptr %str_obj19)
  call void @py_print_string(ptr %str_ptr20)
  call void @py_decref(ptr %str_obj19)
  %int_obj21 = call ptr @py_create_int(i32 0)
  ret ptr %int_obj21
}

declare ptr @py_create_list(i32, i32)

declare void @py_list_set_item(ptr, i32, ptr)

declare ptr @py_convert_to_string(ptr)

declare ptr @py_extract_string(ptr)

declare void @py_object_set_index(ptr, ptr, ptr)

declare ptr @py_create_dict(i32, i32)

declare void @py_dict_set_item(ptr, ptr, ptr)

!0 = !{i32 7}
