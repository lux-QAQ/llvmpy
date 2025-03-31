; ModuleID = 'main'
source_filename = "llvmpy_module"

define ptr @test(ptr %a, ptr %b) {
entry:
  %a1 = alloca ptr, align 8
  store ptr %a, ptr %a1, align 8
  %b2 = alloca ptr, align 8
  store ptr %b, ptr %b2, align 8
  %loaded_ptr = load ptr, ptr %a1, align 8
  call void @py_print_object(ptr %loaded_ptr)
  %loaded_ptr3 = load ptr, ptr %b2, align 8
  call void @py_print_object(ptr %loaded_ptr3)
  %preserve_type_result = call ptr @py_convert_any_preserve_type(ptr %a1), !py_type_id !0, !py_is_reference !1
  call void @py_incref(ptr %preserve_type_result)
  ret ptr %preserve_type_result
}

declare void @py_print_object(ptr)

declare ptr @py_convert_any_preserve_type(ptr)

declare void @py_incref(ptr)

define ptr @test2() {
entry:
  %new_list = call ptr @py_create_list(i32 1, i32 1), !py_type_id !2, !py_is_reference !1
  %int_obj = call ptr @py_create_int(i32 999)
  call void @py_list_set_item(ptr %new_list, i32 0, ptr %int_obj)
  ret ptr %new_list
}

declare ptr @py_create_list(i32, i32)

declare void @py_list_set_item(ptr, i32, ptr)

declare ptr @py_create_int(i32)

define i32 @main() {
entry:
  %new_list = call ptr @py_create_list(i32 2, i32 1)
  %int_obj = call ptr @py_create_int(i32 1)
  call void @py_list_set_item(ptr %new_list, i32 0, ptr %int_obj)
  %int_obj1 = call ptr @py_create_int(i32 2)
  call void @py_list_set_item(ptr %new_list, i32 1, ptr %int_obj1)
  %int_obj2 = call ptr @py_create_int(i32 2), !py_type_id !3
  %calltmp = call ptr @test(ptr %new_list, ptr %int_obj2)
  %b = alloca ptr, align 8
  call void @py_incref(ptr %calltmp)
  store ptr %calltmp, ptr %b, align 8
  call void @py_decref(ptr %calltmp)
  %double_obj = call ptr @py_create_double(double 1.100000e+00), !py_type_id !4
  %new_list3 = call ptr @py_create_list(i32 3, i32 1)
  %int_obj4 = call ptr @py_create_int(i32 1)
  call void @py_list_set_item(ptr %new_list3, i32 0, ptr %int_obj4)
  %int_obj5 = call ptr @py_create_int(i32 2)
  call void @py_list_set_item(ptr %new_list3, i32 1, ptr %int_obj5)
  %int_obj6 = call ptr @py_create_int(i32 3)
  call void @py_list_set_item(ptr %new_list3, i32 2, ptr %int_obj6)
  %calltmp7 = call ptr @test(ptr %double_obj, ptr %new_list3)
  %a = alloca ptr, align 8
  call void @py_incref(ptr %calltmp7)
  store ptr %calltmp7, ptr %a, align 8
  call void @py_decref(ptr %calltmp7)
  %calltmp8 = call ptr @test2()
  %b2 = alloca ptr, align 8
  call void @py_incref(ptr %calltmp8)
  store ptr %calltmp8, ptr %b2, align 8
  call void @py_decref(ptr %calltmp8)
  %loaded_ptr = load ptr, ptr %b2, align 8
  call void @py_print_object(ptr %loaded_ptr)
  %loaded_ptr9 = load ptr, ptr %a, align 8
  call void @py_print_object(ptr %loaded_ptr9)
  %loaded_ptr10 = load ptr, ptr %b, align 8
  call void @py_print_object(ptr %loaded_ptr10)
  %loaded_ptr11 = load ptr, ptr %b2, align 8
  call void @py_print_object(ptr %loaded_ptr11)
  ret i32 0
}

declare void @py_decref(ptr)

declare ptr @py_create_double(double)

!0 = !{i32 7}
!1 = !{i1 true}
!2 = !{i32 5}
!3 = !{i32 1}
!4 = !{i32 2}
