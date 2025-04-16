; ModuleID = 'llvmpy_module'
source_filename = "llvmpy_module"

@llvm.global_ctors = appending global [1 x { i32, ptr, ptr }] [{ i32, ptr, ptr } { i32 65535, ptr @__llvmpy_runtime_init, ptr null }]
@str_const = private unnamed_addr constant [13 x i8] c"\E6\B5\8B\E8\AF\95\E4\B8\8B'!'\00", align 1

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

define internal void @__llvmpy_runtime_init() {
entry:
  call void @py_initialize_builtin_type_methods()
  ret void
}

declare void @py_initialize_builtin_type_methods()

define ptr @fib(ptr %n, ptr %d, ptr %current_len) {
entry:
  %any_cmp_result = call ptr @py_object_compare(ptr %n, ptr %current_len, i32 2), !py.type !0
  %condval = call i1 @py_object_to_bool(ptr %any_cmp_result)
  br i1 %condval, label %then, label %else

then:                                             ; preds = %entry
  %index_result = call ptr @py_object_index(ptr %d, ptr %n)
  ret ptr %index_result

else:                                             ; preds = %entry
  br label %ifcont

ifcont:                                           ; preds = %else
  call void @py_incref(ptr %current_len)
  br label %while.cond

while.cond:                                       ; preds = %while.body, %ifcont
  %i.phi = phi ptr [ %current_len, %ifcont ], [ %any_op_result11, %while.body ]
  %int_obj = call ptr @py_create_int(i32 1)
  %any_op_result = call ptr @py_object_add(ptr %n, ptr %int_obj), !py.type !1
  %any_cmp_result1 = call ptr @py_object_compare(ptr %i.phi, ptr %any_op_result, i32 2), !py.type !0
  %condval2 = call i1 @py_object_to_bool(ptr %any_cmp_result1)
  br i1 %condval2, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %int_obj3 = call ptr @py_create_int(i32 1)
  %any_op_result4 = call ptr @py_object_subtract(ptr %i.phi, ptr %int_obj3), !py.type !1
  %index_result5 = call ptr @py_object_index(ptr %d, ptr %any_op_result4)
  %int_obj6 = call ptr @py_create_int(i32 2)
  %any_op_result7 = call ptr @py_object_subtract(ptr %i.phi, ptr %int_obj6), !py.type !1
  %index_result8 = call ptr @py_object_index(ptr %d, ptr %any_op_result7)
  %any_op_result9 = call ptr @py_object_add(ptr %index_result5, ptr %index_result8), !py.type !1
  call void @py_object_set_index(ptr %d, ptr %i.phi, ptr %any_op_result9)
  %int_obj10 = call ptr @py_create_int(i32 1)
  %any_op_result11 = call ptr @py_object_add(ptr %int_obj10, ptr %i.phi), !py.type !1
  call void @py_incref(ptr %any_op_result11)
  call void @py_decref(ptr %i.phi)
  br label %while.cond

while.end:                                        ; preds = %while.cond
  %index_result12 = call ptr @py_object_index(ptr %d, ptr %n)
  ret ptr %index_result12
}

declare ptr @py_object_compare(ptr, ptr, i32)

declare i1 @py_object_to_bool(ptr)

declare ptr @py_object_subtract(ptr, ptr)

declare void @py_object_set_index(ptr, ptr, ptr)

define ptr @main() {
entry:
  %dict_obj = call ptr @py_create_dict(i32 8, i32 1)
  %int_obj = call ptr @py_create_int(i32 0)
  %int_obj1 = call ptr @py_create_int(i32 0)
  call void @py_dict_set_item(ptr %dict_obj, ptr %int_obj, ptr %int_obj1)
  %int_obj2 = call ptr @py_create_int(i32 1)
  %int_obj3 = call ptr @py_create_int(i32 1)
  call void @py_dict_set_item(ptr %dict_obj, ptr %int_obj2, ptr %int_obj3)
  %int_obj4 = call ptr @py_create_int(i32 2)
  %int_obj5 = call ptr @py_create_int(i32 1)
  call void @py_dict_set_item(ptr %dict_obj, ptr %int_obj4, ptr %int_obj5)
  %int_obj6 = call ptr @py_create_int(i32 3)
  %int_obj7 = call ptr @py_create_int(i32 10)
  call void @py_incref(ptr %int_obj7)
  call void @py_incref(ptr %dict_obj)
  call void @py_incref(ptr %int_obj6)
  %calltmp = call ptr @fib(ptr %int_obj7, ptr %dict_obj, ptr %int_obj6)
  %str_obj = call ptr @py_convert_to_string(ptr %calltmp)
  %str_ptr = call ptr @py_extract_string(ptr %str_obj)
  call void @py_print_string(ptr %str_ptr)
  call void @py_decref(ptr %str_obj)
  %int_obj8 = call ptr @py_create_int(i32 11)
  %int_obj9 = call ptr @py_create_int(i32 20)
  call void @py_incref(ptr %int_obj9)
  call void @py_incref(ptr %dict_obj)
  call void @py_incref(ptr %int_obj8)
  %calltmp10 = call ptr @fib(ptr %int_obj9, ptr %dict_obj, ptr %int_obj8)
  %str_obj11 = call ptr @py_convert_to_string(ptr %calltmp10)
  %str_ptr12 = call ptr @py_extract_string(ptr %str_obj11)
  call void @py_print_string(ptr %str_ptr12)
  call void @py_decref(ptr %str_obj11)
  %int_obj13 = call ptr @py_create_int(i32 21)
  %int_obj14 = call ptr @py_create_int(i32 30)
  call void @py_incref(ptr %int_obj14)
  call void @py_incref(ptr %dict_obj)
  call void @py_incref(ptr %int_obj13)
  %calltmp15 = call ptr @fib(ptr %int_obj14, ptr %dict_obj, ptr %int_obj13)
  %str_obj16 = call ptr @py_convert_to_string(ptr %calltmp15)
  %str_ptr17 = call ptr @py_extract_string(ptr %str_obj16)
  call void @py_print_string(ptr %str_ptr17)
  call void @py_decref(ptr %str_obj16)
  %str_obj18 = call ptr @py_create_string(ptr @str_const)
  %str_ptr19 = call ptr @py_extract_string(ptr %str_obj18)
  call void @py_print_string(ptr %str_ptr19)
  %int_obj20 = call ptr @py_create_int(i32 1)
  %str_obj21 = call ptr @py_convert_to_string(ptr %int_obj20)
  %str_ptr22 = call ptr @py_extract_string(ptr %str_obj21)
  call void @py_print_string(ptr %str_ptr22)
  call void @py_decref(ptr %str_obj21)
  %int_obj23 = call ptr @py_create_int(i32 0)
  ret ptr %int_obj23
}

declare ptr @py_create_dict(i32, i32)

declare void @py_dict_set_item(ptr, ptr, ptr)

declare ptr @py_convert_to_string(ptr)

declare ptr @py_extract_string(ptr)

!0 = !{i32 3}
!1 = !{i32 7}
