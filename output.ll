; ModuleID = 'main'
source_filename = "llvmpy_module"

declare ptr @py_create_dict(i32, i32)

declare ptr @py_dict_get_item(ptr, ptr)

declare void @py_dict_set_item(ptr, ptr, ptr)

define ptr @ff(i32 %b) {
entry:
  %b1 = alloca i32, align 4
  store i32 %b, ptr %b1, align 4
  %loadtmp = load i32, ptr %b1, align 4
  %addtmp = add i32 %loadtmp, 1
  %int_obj = call ptr @py_create_int(i32 %addtmp)
  %obj_copy = call ptr @py_object_copy(ptr %int_obj, i32 5)
  ret ptr %obj_copy
}

declare ptr @py_create_int(i32)

declare ptr @py_object_copy(ptr, i32)

define i32 @main() {
entry:
  %calltmp = call ptr @ff(i32 1)
  %a = alloca ptr, align 8
  store ptr %calltmp, ptr %a, align 8
  call void @py_print_object(ptr %a)
  ret i32 0
}

declare void @py_print_object(ptr)
