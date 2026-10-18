# RenderGraphCacheThunk -- GAS form of RenderGraphCacheThunk.asm.
#
# Both forms are built: CMake picks the MASM .asm for MSVC and this .s for
# clang-cl. They must stay in sync; tools/check_thunk_parity.py enforces the
# symbol contract. See docs/clang-cl-linux-build.md for why MASM exists at all.
#
# NOTE ON UNWIND: the MASM original used PROC FRAME, which emits .pdata/.xdata.
# Neither clang's assembler nor GNU as will translate .cfi_* into COFF unwind
# for hand-written assembly (for PE targets GCC emits .seh_* from the compiler,
# and the assembler only consumes it), so the .cfi_* below are advisory and the
# emitted .eh_frame is ignored by lld-link. The thunks therefore carry no
# Windows unwind record on this toolchain. That is safe because every C++
# callback they invoke is exception-contained at its definition site -- see
# the note in include/Stereo/ThunkCallbacks.hpp.

	.text

	.extern	CvrRenderGraphKey
	.extern	CvrRenderGraphCacheOriginal

	.globl	CvrRenderGraphCacheDetour
	.def	CvrRenderGraphCacheDetour;	.scl	2;	.type	32;	.endef
	.p2align 4
CvrRenderGraphCacheDetour:
	.cfi_startproc
	# No frame pointer: this thunk must return the caller's flags untouched, and
	# a LEA/POP epilogue would clobber them. The CFA record describes the 0xE8
	# local regardless.
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
	movl	%edx, %ecx
	movq	%r8, %rdx
	# 0xE8 above the new rsp is the caller's shadow slot for the fifth argument.
	movq	0xE8(%rsp), %r8
	call	CvrRenderGraphKey
	movq	%rax, 0x48(%rsp)
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
	jmp	*CvrRenderGraphCacheOriginal(%rip)
	.cfi_endproc
