# CameraPoseLeafThunks -- GAS form of CameraPoseLeafThunks.asm.
#
# See RenderGraphCacheThunk.s for the build arrangement and the unwind caveat.
# Symbol contract is checked by tools/check_thunk_parity.py.

	.text

	.extern	CvrPoseSetupDefaultOriginal
	.extern	CvrPoseSetupCopyOriginal
	.extern	CvrPoseSetupDefaultBefore
	.extern	CvrPoseSetupCopyBefore
	.extern	CvrPoseSetupCopyAfter
	.extern	CvrWorldBlendBeforeNotify

# SaveVolatile / RestoreVolatile -- preserve the engine leaf routines' actual
# register contract, including flags and XMM outputs. The C++ callbacks are
# ordinary Win64 callees and may clobber every volatile register. Both original
# routines take register arguments only.

	.macro	SAVE_VOLATILE
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
	.endm

	.macro	RESTORE_VOLATILE
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

	.globl	CvrPoseSetupDefaultDetour
	.def	CvrPoseSetupDefaultDetour;	.scl	2;	.type	32;	.endef
	.p2align 4
CvrPoseSetupDefaultDetour:
	.cfi_startproc
	# A frame pointer gives Windows a standard LEA/POP epilogue while keeping
	# incoming/result flags intact (ADD/SUB RSP would overwrite them).
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	leaq	-0xC0(%rsp), %rsp
	.cfi_def_cfa_offset 0xD0
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	SAVE_VOLATILE
	call	CvrPoseSetupDefaultBefore
	RESTORE_VOLATILE
	leaq	0xC0(%rbp), %rsp
	.cfi_def_cfa_register %rsp
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	jmp	*CvrPoseSetupDefaultOriginal(%rip)
	.cfi_endproc

	.globl	CvrPoseSetupCopyDetour
	.def	CvrPoseSetupCopyDetour;	.scl	2;	.type	32;	.endef
	.p2align 4
CvrPoseSetupCopyDetour:
	.cfi_startproc
	# C0/C8 retain the arguments. D0..2CF is a per-call PoseReceipt (512 bytes),
	# whose size/alignment/trivial destruction are checked in C++.
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	leaq	-0x2D0(%rsp), %rsp
	.cfi_def_cfa_offset 0x2E0
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	SAVE_VOLATILE
	movq	%rcx, 0xC0(%rsp)
	movq	%rdx, 0xC8(%rsp)
	leaq	0xD0(%rsp), %rcx
	call	CvrPoseSetupCopyBefore
	RESTORE_VOLATILE
	call	*CvrPoseSetupCopyOriginal(%rip)
	# Save the ORIGINAL's outputs, not the incoming values: the copy updates
	# RAX and XMM0/1. Commit ancestry only after the copied bytes exist.
	SAVE_VOLATILE
	leaq	0xD0(%rsp), %rcx
	movq	0xC0(%rsp), %rdx
	call	CvrPoseSetupCopyAfter
	RESTORE_VOLATILE
	leaq	0x2D0(%rbp), %rsp
	.cfi_def_cfa_register %rsp
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	retq
	.cfi_endproc

	.globl	CvrWorldBlendNotifyDetour
	.def	CvrWorldBlendNotifyDetour;	.scl	2;	.type	32;	.endef
	.p2align 4
CvrWorldBlendNotifyDetour:
	.cfi_startproc
	# Replaces only WorldSpaceBlendCamera's call [rax+240h]. RCX/RDX and
	# the vtable in RAX must reach the original notification unchanged.
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	leaq	-0xC0(%rsp), %rsp
	.cfi_def_cfa_offset 0xD0
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	SAVE_VOLATILE
	call	CvrWorldBlendBeforeNotify
	RESTORE_VOLATILE
	leaq	0xC0(%rbp), %rsp
	.cfi_def_cfa_register %rsp
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	jmp	*0x240(%rax)
	.cfi_endproc
