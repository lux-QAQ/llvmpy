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

define ptr @returntest1(ptr %a) {
entry:
  %int_obj = call ptr @py_create_int(i32 1)
  %conversion_result = call ptr @py_extract_int_from_any(ptr %a)
  %binop_result = call ptr @py_object_add(ptr %conversion_result, ptr %int_obj), !py.type !0
  ret ptr %binop_result
}

declare ptr @py_extract_int_from_any(ptr)

define ptr @returntest2(ptr %a, ptr %b) {
entry:
  %index_result = call ptr @py_object_index(ptr %a, ptr %b)
  ret ptr %index_result
}

define ptr @returntest3(ptr %a) {
entry:
  %int_obj = call ptr @py_create_int(i32 2)
  %conversion_result = call ptr @py_extract_int_from_any(ptr %a)
  %binop_result = call ptr @py_object_add(ptr %conversion_result, ptr %int_obj), !py.type !0
  ret ptr %binop_result
}

define ptr @returntest4(ptr %a) {
entry:
  %int_obj = call ptr @py_create_int(i32 3)
  %conversion_result = call ptr @py_extract_int_from_any(ptr %a)
  %binop_result = call ptr @py_object_add(ptr %conversion_result, ptr %int_obj), !py.type !0
  ret ptr %binop_result
}

!0 = !{i32 1}
