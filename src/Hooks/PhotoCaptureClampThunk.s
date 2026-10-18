# PhotoCaptureClampThunk -- GAS form of PhotoCaptureClampThunk.asm.
#
# See RenderGraphCacheThunk.s for the build arrangement and the unwind caveat.
# Symbol contract is checked by tools/check_thunk_parity.py.

	.text

	.extern	CvrPhotoClampOriginal
	.extern	CvrPhotoClampSkip

# RestoreIncoming -- put every volatile register and the flags back exactly as
# they were on entry. Spelled as a macro so the two exits below cannot drift.
	.macro	RESTORE_INCOMING
	movdqu	0x60(%rsp), %xmm0
	movdqu	0x70(%rsp), %xmm1
	movdqu	0x80(%rsp), %xmm2
	movdqu	0x90(%rsp), %xmm3
	movdqu	0xA0(%rsp), %xmm4
	movdqu	0xB0(%rsp), %xmm5
	pushq	0x58(%rsp)
	popfq
	movq	0x20(%rsp), %rax
	movq	0x28(%rsp), %rcx
	movq	0x30(%rsp), %rdx
	movq	0x38(%rsp), %r8
	movq	0x40(%rsp), %r9
	movq	0x48(%rsp), %r10
	movq	0x50(%rsp), %r11
	.endm

	.globl	CvrPhotoClampDetour
	.def	CvrPhotoClampDetour;	.scl	2;	.type	32;	.endef
	.p2align 4
CvrPhotoClampDetour:
	.cfi_startproc
	# Only the screenshot readback CALL site is redirected here. The shared native
	# clamp entry remains untouched: world-widget callers depend on live R8/R9.
	# Preserve all incoming volatile registers and flags around the C++ predicate.
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	leaq	-0xC0(%rsp), %rsp
	.cfi_def_cfa_offset 0xD0
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	%rax, 0x20(%rsp)
	movq	%rcx, 0x28(%rsp)
	movq	%rdx, 0x30(%rsp)
	movq	%r8, 0x38(%rsp)
	movq	%r9, 0x40(%rsp)
	movq	%r10, 0x48(%rsp)
	movq	%r11, 0x50(%rsp)
	pushfq
	popq	%rax
	movq	%rax, 0x58(%rsp)
	movdqu	%xmm0, 0x60(%rsp)
	movdqu	%xmm1, 0x70(%rsp)
	movdqu	%xmm2, 0x80(%rsp)
	movdqu	%xmm3, 0x90(%rsp)
	movdqu	%xmm4, 0xA0(%rsp)
	movdqu	%xmm5, 0xB0(%rsp)
	call	CvrPhotoClampSkip
	testb	%al, %al
	je	.Lnative_clamp
	RESTORE_INCOMING
	leaq	0xC0(%rbp), %rsp
	.cfi_def_cfa_register %rsp
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	retq
.Lnative_clamp:
	RESTORE_INCOMING
	leaq	0xC0(%rbp), %rsp
	.cfi_def_cfa_register %rsp
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	jmp	*CvrPhotoClampOriginal(%rip)
	.cfi_endproc
