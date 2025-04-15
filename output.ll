; ModuleID = 'llvmpy_module'
source_filename = "llvmpy_module"

@llvm.global_ctors = appending global [1 x { i32, ptr, ptr }] [{ i32, ptr, ptr } { i32 65535, ptr @__llvmpy_runtime_init, ptr null }]

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

define ptr @main() {
entry:
  %int_obj = call ptr @py_create_int(i32 2)
  br label %while.cond

while.cond:                                       ; preds = %while.body, %entry
  %a.phi = phi ptr [ %int_obj, %entry ], [ %any_op_result, %while.body ]
  %int_obj1 = call ptr @py_create_int(i32 10)
  %any_cmp_result = call ptr @py_object_compare(ptr %a.phi, ptr %int_obj1, i32 2), !py.type !0
  %condval = call i1 @py_object_to_bool(ptr %any_cmp_result)
  br i1 %condval, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %int_obj2 = call ptr @py_create_int(i32 1)
  %any_op_result = call ptr @py_object_add(ptr %a.phi, ptr %int_obj2), !py.type !1
  call void @py_incref(ptr %any_op_result)
  br label %while.cond

while.end:                                        ; preds = %while.cond
  %str_obj = call ptr @py_convert_to_string(ptr %a.phi)
  %str_ptr = call ptr @py_extract_string(ptr %str_obj)
  call void @py_print_string(ptr %str_ptr)
  call void @py_decref(ptr %str_obj)
  %int_obj3 = call ptr @py_create_int(i32 0)
  ret ptr %int_obj3
}

declare ptr @py_object_compare(ptr, ptr, i32)

declare i1 @py_object_to_bool(ptr)

declare ptr @py_convert_to_string(ptr)

declare ptr @py_extract_string(ptr)

!0 = !{i32 3}
!1 = !{i32 7}
