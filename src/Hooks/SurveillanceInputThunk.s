# SurveillanceInputThunk -- GAS form of SurveillanceInputThunk.asm.
#
# See RenderGraphCacheThunk.s for the build arrangement and the unwind caveat.
# Symbol contract is checked by tools/check_thunk_parity.py.

	.text

	.extern	CvrSurveillanceInputOriginal
	.extern	CvrSurveillanceInputAfter
	.extern	CvrSurveillanceFinishInput

	.globl	CvrSurveillanceInputDetour
	.def	CvrSurveillanceInputDetour;	.scl	2;	.type	32;	.endef
	.p2align 4
CvrSurveillanceInputDetour:
	.cfi_startproc
	# Replaces only DeviceCameraControlComponent's input-reader call. The native
	# reader has six arguments; RDI is the live component in that verified caller.
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	leaq	-0xE0(%rsp), %rsp
	.cfi_def_cfa_offset 0xF0
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	%rax, 0xD8(%rsp)
	movq	%rdx, 0xD0(%rsp)
	movq	0x110(%rsp), %rax
	movq	%rax, 0x20(%rsp)
	movq	0x118(%rsp), %rax
	movq	%rax, 0x28(%rsp)
	movq	0xD8(%rsp), %rax
	call	*CvrSurveillanceInputOriginal(%rip)
	movq	%rax, 0x30(%rsp)
	movq	%rcx, 0x38(%rsp)
	movq	%rdx, 0x40(%rsp)
	movq	%r8, 0x48(%rsp)
	movq	%r9, 0x50(%rsp)
	movq	%r10, 0x58(%rsp)
	movq	%r11, 0x60(%rsp)
	pushfq
	popq	%rax
	movq	%rax, 0x68(%rsp)
	movdqu	%xmm0, 0x70(%rsp)
	movdqu	%xmm1, 0x80(%rsp)
	movdqu	%xmm2, 0x90(%rsp)
	movdqu	%xmm3, 0xA0(%rsp)
	movdqu	%xmm4, 0xB0(%rsp)
	movdqu	%xmm5, 0xC0(%rsp)
	movq	%rdi, %rcx
	movq	0xD0(%rsp), %rdx
	call	CvrSurveillanceInputAfter
	movdqu	0x70(%rsp), %xmm0
	movdqu	0x80(%rsp), %xmm1
	movdqu	0x90(%rsp), %xmm2
	movdqu	0xA0(%rsp), %xmm3
	movdqu	0xB0(%rsp), %xmm4
	movdqu	0xC0(%rsp), %xmm5
	pushq	0x68(%rsp)
	popfq
	movq	0x30(%rsp), %rax
	movq	0x38(%rsp), %rcx
	movq	0x40(%rsp), %rdx
	movq	0x48(%rsp), %r8
	movq	0x50(%rsp), %r9
	movq	0x58(%rsp), %r10
	movq	0x60(%rsp), %r11
	leaq	0xE0(%rbp), %rsp
	.cfi_def_cfa_register %rsp
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	retq
	.cfi_endproc

	.globl	CvrSurveillanceFinishDetour
	.def	CvrSurveillanceFinishDetour;	.scl	2;	.type	32;	.endef
	.p2align 4
CvrSurveillanceFinishDetour:
	.cfi_startproc
	# Replaces movss xmm0,[rsp+44h] after CameraSystem's automatic modifiers.
	# Native input Euler is [caller rsp+60h], alternative auto Euler is +40h.
	# Preserve the complete original register state, then replay that movss.
	leaq	-0xE8(%rsp), %rsp
	.cfi_def_cfa_offset 0xF0
	movq	%rax, 0x30(%rsp)
	movq	%rcx, 0x38(%rsp)
	movq	%rdx, 0x40(%rsp)
	movq	%r8, 0x48(%rsp)
	movq	%r9, 0x50(%rsp)
	movq	%r10, 0x58(%rsp)
	movq	%r11, 0x60(%rsp)
	pushfq
	popq	%rax
	movq	%rax, 0x68(%rsp)
	movdqu	%xmm0, 0x70(%rsp)
	movdqu	%xmm1, 0x80(%rsp)
	movdqu	%xmm2, 0x90(%rsp)
	movdqu	%xmm3, 0xA0(%rsp)
	movdqu	%xmm4, 0xB0(%rsp)
	movdqu	%xmm5, 0xC0(%rsp)
	movq	%rdi, %rcx
	leaq	0x130(%rsp), %rdx
	leaq	0x150(%rsp), %r8
	call	CvrSurveillanceFinishInput
	movdqu	0x70(%rsp), %xmm0
	movdqu	0x80(%rsp), %xmm1
	movdqu	0x90(%rsp), %xmm2
	movdqu	0xA0(%rsp), %xmm3
	movdqu	0xB0(%rsp), %xmm4
	movdqu	0xC0(%rsp), %xmm5
	pushq	0x68(%rsp)
	popfq
	movq	0x30(%rsp), %rax
	movq	0x38(%rsp), %rcx
	movq	0x40(%rsp), %rdx
	movq	0x48(%rsp), %r8
	movq	0x50(%rsp), %r9
	movq	0x58(%rsp), %r10
	movq	0x60(%rsp), %r11
	leaq	0xE8(%rsp), %rsp
	.cfi_def_cfa_offset 8
	movss	0x4C(%rsp), %xmm0
	retq
	.cfi_endproc
