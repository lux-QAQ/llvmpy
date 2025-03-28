; ModuleID = 'llvmpy'
source_filename = "llvmpy"

@str = private unnamed_addr constant [37 x i8] c"IndexError: list index out of range\0A\00", align 1
@str.1 = private unnamed_addr constant [4 x i8] c"%f\0A\00", align 1

define i32 @main() {
entry:
  %a = alloca ptr, align 8
  %list = alloca { i32, ptr }, align 8
  %length_ptr = getelementptr inbounds nuw { i32, ptr }, ptr %list, i32 0, i32 0
  store i32 2, ptr %length_ptr, align 4
  %list_data = call ptr @malloc(i32 16)
  %data_ptr_ptr = getelementptr inbounds nuw { i32, ptr }, ptr %list, i32 0, i32 1
  store ptr %list_data, ptr %data_ptr_ptr, align 8
  %elem_ptr_0 = getelementptr double, ptr %list_data, i32 0
  store double 1.000000e+00, ptr %elem_ptr_0, align 8
  %elem_ptr_1 = getelementptr double, ptr %list_data, i32 1
  store double 2.000000e+00, ptr %elem_ptr_1, align 8
  store ptr %list, ptr %a, align 8
  %a1 = load double, ptr %a, align 8
  %a_ptr = load ptr, ptr %a, align 8
  br label %bounds_check

bounds_check:                                     ; preds = %entry
  %length_ptr2 = getelementptr inbounds nuw { i32, ptr }, ptr %a_ptr, i32 0, i32 0
  %length = load i32, ptr %length_ptr2, align 4
  %is_too_large = icmp sge i32 0, %length
  %is_out_of_bounds = or i1 false, %is_too_large
  br i1 %is_out_of_bounds, label %out_of_bounds, label %in_bounds

in_bounds:                                        ; preds = %out_of_bounds, %bounds_check
  %safe_index = phi i32 [ 0, %out_of_bounds ], [ 0, %bounds_check ]
  %data_ptr_ptr3 = getelementptr inbounds nuw { i32, ptr }, ptr %a_ptr, i32 0, i32 1
  %data_ptr = load ptr, ptr %data_ptr_ptr3, align 8
  %a_elem_ptr = getelementptr double, ptr %data_ptr, i32 %safe_index
  %elem_value = load double, ptr %a_elem_ptr, align 8
  %0 = call i32 (ptr, ...) @printf(ptr @str.1, double %elem_value)
  ret i32 0

out_of_bounds:                                    ; preds = %bounds_check
  %1 = call i32 (ptr, ...) @printf(ptr @str)
  br label %in_bounds
}

declare ptr @malloc(i32)

declare i32 @printf(ptr, ...)
