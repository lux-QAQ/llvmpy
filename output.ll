; ModuleID = 'llvmpy_module'
source_filename = "llvmpy_module"

@str_const = private unnamed_addr constant [11 x i8] c"teststring\00", align 1

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

define ptr @returntest1(ptr %a) {
entry:
  %int_obj = call ptr @py_create_int(i32 1)
  %any_op_result = call ptr @py_object_add(ptr %a, ptr %int_obj), !py.type !0
  ret ptr %any_op_result
}

define ptr @returntest2(ptr %a, ptr %b) {
entry:
  %index_result = call ptr @py_object_index(ptr %a, ptr %b)
  %int_obj = call ptr @py_create_int(i32 999)
  %any_op_result = call ptr @py_object_add(ptr %index_result, ptr %int_obj), !py.type !0
  ret ptr %any_op_result
}

define ptr @returntest3(ptr %a) {
entry:
  %int_obj = call ptr @py_create_int(i32 2)
  %any_op_result = call ptr @py_object_add(ptr %a, ptr %int_obj), !py.type !0
  ret ptr %any_op_result
}

define ptr @returntest4(ptr %a) {
entry:
  %int_obj = call ptr @py_create_int(i32 3)
  %any_op_result = call ptr @py_object_add(ptr %a, ptr %int_obj), !py.type !0
  ret ptr %any_op_result
}

define ptr @whiletest(ptr %a) {
entry:
  %int_obj = call ptr @py_create_int(i32 2)
  br label %while.cond

while.cond:                                       ; preds = %while.body, %entry
  %b.phi = phi ptr [ %int_obj, %entry ], [ %any_op_result, %while.body ]
  %int_obj1 = call ptr @py_create_int(i32 5)
  %any_cmp_result = call ptr @py_object_compare(ptr %b.phi, ptr %int_obj1, i32 2), !py.type !1
  %condval = call i1 @py_object_to_bool(ptr %any_cmp_result)
  br i1 %condval, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %int_obj2 = call ptr @py_create_int(i32 1)
  %any_op_result = call ptr @py_object_add(ptr %b.phi, ptr %int_obj2), !py.type !0
  call void @py_incref(ptr %any_op_result)
  %str_obj = call ptr @py_convert_to_string(ptr %any_op_result)
  %str_ptr = call ptr @py_extract_string(ptr %str_obj)
  call void @py_print_string(ptr %str_ptr)
  call void @py_decref(ptr %str_obj)
  br label %while.cond

while.end:                                        ; preds = %while.cond
  ret ptr %a
}

declare ptr @py_object_compare(ptr, ptr, i32)

declare i1 @py_object_to_bool(ptr)

declare ptr @py_convert_to_string(ptr)

declare ptr @py_extract_string(ptr)

define ptr @fib(ptr %a) {
entry:
  %int_obj = call ptr @py_create_int(i32 1)
  %any_cmp_result = call ptr @py_object_compare(ptr %a, ptr %int_obj, i32 2), !py.type !1
  %condval = call i1 @py_object_to_bool(ptr %any_cmp_result)
  br i1 %condval, label %then, label %else

then:                                             ; preds = %entry
  %int_obj1 = call ptr @py_create_int(i32 0)
  ret ptr %int_obj1

else:                                             ; preds = %entry
  %int_obj2 = call ptr @py_create_int(i32 2)
  %any_cmp_result3 = call ptr @py_object_compare(ptr %a, ptr %int_obj2, i32 2), !py.type !1
  %condval4 = call i1 @py_object_to_bool(ptr %any_cmp_result3)
  br i1 %condval4, label %then5, label %else6

ifcont:                                           ; No predecessors!
  %0 = call ptr @py_get_none()
  ret ptr %0

then5:                                            ; preds = %else
  %int_obj8 = call ptr @py_create_int(i32 1)
  ret ptr %int_obj8

else6:                                            ; preds = %else
  br label %ifcont7

ifcont7:                                          ; preds = %else6
  %int_obj9 = call ptr @py_create_int(i32 3)
  %any_cmp_result10 = call ptr @py_object_compare(ptr %a, ptr %int_obj9, i32 2), !py.type !1
  %condval11 = call i1 @py_object_to_bool(ptr %any_cmp_result10)
  br i1 %condval11, label %then12, label %else13

then12:                                           ; preds = %ifcont7
  %int_obj15 = call ptr @py_create_int(i32 1)
  ret ptr %int_obj15

else13:                                           ; preds = %ifcont7
  br label %ifcont14

ifcont14:                                         ; preds = %else13
  %int_obj16 = call ptr @py_create_int(i32 4)
  %any_cmp_result17 = call ptr @py_object_compare(ptr %a, ptr %int_obj16, i32 2), !py.type !1
  %condval18 = call i1 @py_object_to_bool(ptr %any_cmp_result17)
  br i1 %condval18, label %then19, label %else20

then19:                                           ; preds = %ifcont14
  %int_obj22 = call ptr @py_create_int(i32 2)
  ret ptr %int_obj22

else20:                                           ; preds = %ifcont14
  br label %ifcont21

ifcont21:                                         ; preds = %else20
  %int_obj23 = call ptr @py_create_int(i32 1)
  %any_op_result = call ptr @py_object_subtract(ptr %a, ptr %int_obj23), !py.type !0
  call void @py_incref(ptr %any_op_result)
  %calltmp = call ptr @fib(ptr %any_op_result)
  %int_obj24 = call ptr @py_create_int(i32 2)
  %any_op_result25 = call ptr @py_object_subtract(ptr %a, ptr %int_obj24), !py.type !0
  call void @py_incref(ptr %any_op_result25)
  %calltmp26 = call ptr @fib(ptr %any_op_result25)
  %any_op_result27 = call ptr @py_object_add(ptr %calltmp, ptr %calltmp26), !py.type !0
  ret ptr %any_op_result27
}

declare ptr @py_object_subtract(ptr, ptr)

define ptr @testrecursion(ptr %a, ptr %b) {
entry:
  br label %while.cond

while.cond:                                       ; preds = %while.body, %entry
  %a.phi = phi ptr [ %a, %entry ], [ %any_op_result, %while.body ]
  %any_cmp_result = call ptr @py_object_compare(ptr %a.phi, ptr %b, i32 2), !py.type !1
  %condval = call i1 @py_object_to_bool(ptr %any_cmp_result)
  br i1 %condval, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  call void @py_incref(ptr %a.phi)
  %calltmp = call ptr @fib(ptr %a.phi)
  %str_obj = call ptr @py_convert_to_string(ptr %calltmp)
  %str_ptr = call ptr @py_extract_string(ptr %str_obj)
  call void @py_print_string(ptr %str_ptr)
  call void @py_decref(ptr %str_obj)
  %int_obj = call ptr @py_create_int(i32 1)
  %any_op_result = call ptr @py_object_add(ptr %a.phi, ptr %int_obj), !py.type !0
  call void @py_incref(ptr %any_op_result)
  br label %while.cond

while.end:                                        ; preds = %while.cond
  %int_obj1 = call ptr @py_create_int(i32 1)
  ret ptr %int_obj1
}

define ptr @teststring() {
entry:
  %str_obj = call ptr @py_create_string(ptr @str_const)
  ret ptr %str_obj
}

define ptr @main() {
entry:
  %int_obj = call ptr @py_create_int(i32 1)
  %double_obj = call ptr @py_create_double(double 2.500000e+00)
  %list_obj = call ptr @py_create_list(i32 2, i32 2)
  call void @py_list_set_item(ptr %list_obj, i32 0, ptr %int_obj)
  call void @py_incref(ptr %int_obj)
  call void @py_list_set_item(ptr %list_obj, i32 1, ptr %double_obj)
  call void @py_incref(ptr %double_obj)
  %double_obj1 = call ptr @py_create_double(double 5.000000e-01)
  call void @py_incref(ptr %double_obj1)
  %calltmp = call ptr @returntest3(ptr %double_obj1)
  %str_obj = call ptr @py_convert_to_string(ptr %calltmp)
  %str_ptr = call ptr @py_extract_string(ptr %str_obj)
  call void @py_print_string(ptr %str_ptr)
  call void @py_decref(ptr %str_obj)
  call void @py_incref(ptr %double_obj1)
  %calltmp2 = call ptr @returntest4(ptr %double_obj1)
  %str_obj3 = call ptr @py_convert_to_string(ptr %calltmp2)
  %str_ptr4 = call ptr @py_extract_string(ptr %str_obj3)
  call void @py_print_string(ptr %str_ptr4)
  call void @py_decref(ptr %str_obj3)
  call void @py_incref(ptr %double_obj1)
  %calltmp5 = call ptr @returntest1(ptr %double_obj1)
  %str_obj6 = call ptr @py_convert_to_string(ptr %calltmp5)
  %str_ptr7 = call ptr @py_extract_string(ptr %str_obj6)
  call void @py_print_string(ptr %str_ptr7)
  call void @py_decref(ptr %str_obj6)
  %int_obj8 = call ptr @py_create_int(i32 1)
  call void @py_incref(ptr %list_obj)
  call void @py_incref(ptr %int_obj8)
  %calltmp9 = call ptr @returntest2(ptr %list_obj, ptr %int_obj8)
  %str_obj10 = call ptr @py_convert_to_string(ptr %calltmp9)
  %str_ptr11 = call ptr @py_extract_string(ptr %str_obj10)
  call void @py_print_string(ptr %str_ptr11)
  call void @py_decref(ptr %str_obj10)
  call void @py_incref(ptr %double_obj1)
  %calltmp12 = call ptr @whiletest(ptr %double_obj1)
  %str_obj13 = call ptr @py_convert_to_string(ptr %calltmp12)
  %str_ptr14 = call ptr @py_extract_string(ptr %str_obj13)
  call void @py_print_string(ptr %str_ptr14)
  call void @py_decref(ptr %str_obj13)
  %int_obj15 = call ptr @py_create_int(i32 1)
  %int_obj16 = call ptr @py_create_int(i32 999)
  call void @py_incref(ptr %int_obj15)
  call void @py_incref(ptr %int_obj16)
  %calltmp17 = call ptr @testrecursion(ptr %int_obj15, ptr %int_obj16)
  %calltmp18 = call ptr @teststring()
  %str_ptr19 = call ptr @py_extract_string(ptr %calltmp18)
  call void @py_print_string(ptr %str_ptr19)
  %int_obj20 = call ptr @py_create_int(i32 0)
  ret ptr %int_obj20
}

declare ptr @py_create_list(i32, i32)

declare void @py_list_set_item(ptr, i32, ptr)

!0 = !{i32 7}
!1 = !{i32 3}
