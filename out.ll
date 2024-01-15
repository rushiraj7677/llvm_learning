; ModuleID = 'EvaLLVM'
source_filename = "EvaLLVM"

@VERSION = global i32 43, align 4
@0 = private unnamed_addr constant [12 x i8] c"IS X : %d \0A\00", align 1

declare i32 @printf(ptr, ...)

define i32 @main() {
entry:
  %z = alloca i32, align 4
  store i32 32, ptr %z, align 4
  %z1 = load i32, ptr %z, align 4
  %tmpadd = add i32 %z1, 10
  %x = alloca i32, align 4
  store i32 %tmpadd, ptr %x, align 4
  %x2 = load i32, ptr %x, align 4
  %tmpcmp = icmp ne i32 %x2, 42
  br i1 %tmpcmp, label %then, label %else

then:                                             ; preds = %entry
  store i32 100, ptr %x, align 4
  br label %ifend

else:                                             ; preds = %entry
  store i32 200, ptr %x, align 4
  br label %ifend

ifend:                                            ; preds = %else, %then
  %tmpif = phi i32 [ 100, %then ], [ 200, %else ]
  %x3 = load i32, ptr %x, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @0, i32 %x3)
  ret i32 0
}
