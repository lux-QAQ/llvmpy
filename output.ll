; ModuleID = 'llvmpy'
source_filename = "llvmpy"

@fmt_double = private unnamed_addr constant [4 x i8] c"%f\0A\00", align 1

define double @f(double %x) {
entry:
  %multmp = fmul double %x, 2.000000e+00
  %multmp4 = fmul double %x, %multmp
  %addtmp = fadd double %x, %multmp4
  ret double %addtmp
}

define double @find() {
entry:
  %mid = alloca double, align 8
  %r = alloca double, align 8
  %l = alloca double, align 8
  store double -1.000000e+01, ptr %l, align 8
  store double 0.000000e+00, ptr %r, align 8
  br label %while.cond

while.cond:                                       ; preds = %ifcont, %entry
  %l4 = phi double [ %l419, %ifcont ], [ -1.000000e+01, %entry ]
  %r3 = phi double [ %r317, %ifcont ], [ 0.000000e+00, %entry ]
  %subtmp = fsub double %r3, %l4
  %cmptmp = fcmp ugt double %subtmp, 1.000000e-05
  br i1 %cmptmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %divtmp = fmul double %subtmp, 5.000000e-01
  %addtmp = fadd double %l4, %divtmp
  store double %addtmp, ptr %mid, align 8
  %calltmp = call double @f(double %addtmp)
  %cmptmp8 = fcmp ugt double %calltmp, 0.000000e+00
  br i1 %cmptmp8, label %then, label %else

then:                                             ; preds = %while.body
  store double %addtmp, ptr %l, align 8
  br label %ifcont

else:                                             ; preds = %while.body
  store double %addtmp, ptr %r, align 8
  br label %ifcont

ifcont:                                           ; preds = %else, %then
  %l419 = phi double [ %l4, %else ], [ %addtmp, %then ]
  %r317 = phi double [ %addtmp, %else ], [ %r3, %then ]
  br label %while.cond

while.end:                                        ; preds = %while.cond
  ret double %l4
}

define i32 @main() {
entry:
  %calltmp = call double @find()
  %0 = call i32 (ptr, ...) @printf(ptr nonnull @fmt_double, double %calltmp)
  ret i32 0
}

declare i32 @printf(ptr, ...)
