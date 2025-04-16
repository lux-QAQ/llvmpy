	.file	"llvmpy_module"
	.text
	.p2align	4                               # -- Begin function __llvmpy_runtime_init
	.type	__llvmpy_runtime_init,@function
__llvmpy_runtime_init:                  # @__llvmpy_runtime_init
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rax
	.cfi_def_cfa_offset 16
	callq	py_initialize_builtin_type_methods@PLT
	popq	%rax
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end0:
	.size	__llvmpy_runtime_init, .Lfunc_end0-__llvmpy_runtime_init
	.cfi_endproc
                                        # -- End function
	.globl	fib                             # -- Begin function fib
	.p2align	4
	.type	fib,@function
fib:                                    # @fib
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r15
	.cfi_def_cfa_offset 16
	pushq	%r14
	.cfi_def_cfa_offset 24
	pushq	%r12
	.cfi_def_cfa_offset 32
	pushq	%rbx
	.cfi_def_cfa_offset 40
	pushq	%rax
	.cfi_def_cfa_offset 48
	.cfi_offset %rbx, -40
	.cfi_offset %r12, -32
	.cfi_offset %r14, -24
	.cfi_offset %r15, -16
	movq	%rdx, %r15
	movq	%rsi, %r14
	movq	%rdi, %rbx
	movq	%rdx, %rsi
	movl	$2, %edx
	callq	py_object_compare@PLT
	movq	%rax, %rdi
	callq	py_object_to_bool@PLT
	testb	$1, %al
	je	.LBB1_1
.LBB1_4:                                # %then
	movq	%r14, %rdi
	movq	%rbx, %rsi
	callq	py_object_index@PLT
	addq	$8, %rsp
	.cfi_def_cfa_offset 40
	popq	%rbx
	.cfi_def_cfa_offset 32
	popq	%r12
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	retq
.LBB1_1:                                # %else
	.cfi_def_cfa_offset 48
	movq	%r15, %rdi
	callq	py_incref@PLT
	.p2align	4
.LBB1_2:                                # %while.cond
                                        # =>This Inner Loop Header: Depth=1
	movl	$1, %edi
	callq	py_create_int@PLT
	movq	%rbx, %rdi
	movq	%rax, %rsi
	callq	py_object_add@PLT
	movq	%r15, %rdi
	movq	%rax, %rsi
	movl	$2, %edx
	callq	py_object_compare@PLT
	movq	%rax, %rdi
	callq	py_object_to_bool@PLT
	testb	$1, %al
	je	.LBB1_4
# %bb.3:                                # %while.body
                                        #   in Loop: Header=BB1_2 Depth=1
	movl	$1, %edi
	callq	py_create_int@PLT
	movq	%r15, %rdi
	movq	%rax, %rsi
	callq	py_object_subtract@PLT
	movq	%r14, %rdi
	movq	%rax, %rsi
	callq	py_object_index@PLT
	movq	%rax, %r12
	movl	$2, %edi
	callq	py_create_int@PLT
	movq	%r15, %rdi
	movq	%rax, %rsi
	callq	py_object_subtract@PLT
	movq	%r14, %rdi
	movq	%rax, %rsi
	callq	py_object_index@PLT
	movq	%r12, %rdi
	movq	%rax, %rsi
	callq	py_object_add@PLT
	movq	%r14, %rdi
	movq	%r15, %rsi
	movq	%rax, %rdx
	callq	py_object_set_index@PLT
	movl	$1, %edi
	callq	py_create_int@PLT
	movq	%rax, %rdi
	movq	%r15, %rsi
	callq	py_object_add@PLT
	movq	%rax, %r12
	movq	%rax, %rdi
	callq	py_incref@PLT
	movq	%r15, %rdi
	callq	py_decref@PLT
	movq	%r12, %r15
	jmp	.LBB1_2
.Lfunc_end1:
	.size	fib, .Lfunc_end1-fib
	.cfi_endproc
                                        # -- End function
	.globl	main                            # -- Begin function main
	.p2align	4
	.type	main,@function
main:                                   # @main
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r15
	.cfi_def_cfa_offset 16
	pushq	%r14
	.cfi_def_cfa_offset 24
	pushq	%rbx
	.cfi_def_cfa_offset 32
	.cfi_offset %rbx, -32
	.cfi_offset %r14, -24
	.cfi_offset %r15, -16
	movl	$8, %edi
	movl	$1, %esi
	callq	py_create_dict@PLT
	movq	%rax, %rbx
	xorl	%edi, %edi
	callq	py_create_int@PLT
	movq	%rax, %r14
	xorl	%edi, %edi
	callq	py_create_int@PLT
	movq	%rbx, %rdi
	movq	%r14, %rsi
	movq	%rax, %rdx
	callq	py_dict_set_item@PLT
	movl	$1, %edi
	callq	py_create_int@PLT
	movq	%rax, %r14
	movl	$1, %edi
	callq	py_create_int@PLT
	movq	%rbx, %rdi
	movq	%r14, %rsi
	movq	%rax, %rdx
	callq	py_dict_set_item@PLT
	movl	$2, %edi
	callq	py_create_int@PLT
	movq	%rax, %r14
	movl	$1, %edi
	callq	py_create_int@PLT
	movq	%rbx, %rdi
	movq	%r14, %rsi
	movq	%rax, %rdx
	callq	py_dict_set_item@PLT
	movl	$3, %edi
	callq	py_create_int@PLT
	movq	%rax, %r14
	movl	$10, %edi
	callq	py_create_int@PLT
	movq	%rax, %r15
	movq	%rax, %rdi
	callq	py_incref@PLT
	movq	%rbx, %rdi
	callq	py_incref@PLT
	movq	%r14, %rdi
	callq	py_incref@PLT
	movq	%r15, %rdi
	movq	%rbx, %rsi
	movq	%r14, %rdx
	callq	fib@PLT
	movq	%rax, %rdi
	callq	py_convert_to_string@PLT
	movq	%rax, %r14
	movq	%rax, %rdi
	callq	py_extract_string@PLT
	movq	%rax, %rdi
	callq	py_print_string@PLT
	movq	%r14, %rdi
	callq	py_decref@PLT
	movl	$11, %edi
	callq	py_create_int@PLT
	movq	%rax, %r14
	movl	$20, %edi
	callq	py_create_int@PLT
	movq	%rax, %r15
	movq	%rax, %rdi
	callq	py_incref@PLT
	movq	%rbx, %rdi
	callq	py_incref@PLT
	movq	%r14, %rdi
	callq	py_incref@PLT
	movq	%r15, %rdi
	movq	%rbx, %rsi
	movq	%r14, %rdx
	callq	fib@PLT
	movq	%rax, %rdi
	callq	py_convert_to_string@PLT
	movq	%rax, %r14
	movq	%rax, %rdi
	callq	py_extract_string@PLT
	movq	%rax, %rdi
	callq	py_print_string@PLT
	movq	%r14, %rdi
	callq	py_decref@PLT
	movl	$21, %edi
	callq	py_create_int@PLT
	movq	%rax, %r14
	movl	$30, %edi
	callq	py_create_int@PLT
	movq	%rax, %r15
	movq	%rax, %rdi
	callq	py_incref@PLT
	movq	%rbx, %rdi
	callq	py_incref@PLT
	movq	%r14, %rdi
	callq	py_incref@PLT
	movq	%r15, %rdi
	movq	%rbx, %rsi
	movq	%r14, %rdx
	callq	fib@PLT
	movq	%rax, %rdi
	callq	py_convert_to_string@PLT
	movq	%rax, %rbx
	movq	%rax, %rdi
	callq	py_extract_string@PLT
	movq	%rax, %rdi
	callq	py_print_string@PLT
	movq	%rbx, %rdi
	callq	py_decref@PLT
	xorl	%edi, %edi
	callq	py_create_int@PLT
	popq	%rbx
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end2:
	.size	main, .Lfunc_end2-main
	.cfi_endproc
                                        # -- End function
	.section	.init_array,"aw",@init_array
	.p2align	3, 0x0
	.quad	__llvmpy_runtime_init
	.section	".note.GNU-stack","",@progbits
