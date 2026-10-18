# StoryAttentionThunk -- GAS form of StoryAttentionThunk.asm.
#
# See RenderGraphCacheThunk.s for the build arrangement and the unwind caveat.
# Symbol contract is checked by tools/check_thunk_parity.py.

	.text

	.extern	CvrStoryAttentionAfter

	.globl	CvrStoryAttentionDetour
	.def	CvrStoryAttentionDetour;	.scl	2;	.type	32;	.endef
	.p2align 4
CvrStoryAttentionDetour:
	.cfi_startproc
	# Inserted CALL at +3F90F7 (caller RSP aligned). Preserve all volatile state;
	# the original source virtual call can leave registers live past this site.
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
	movq	%rbx, %rcx
	leaq	-0x50(%rbp), %rdx
	call	CvrStoryAttentionAfter
	movdqu	0x70(%rsp), %xmm0
	movdqu	0x80(%rsp), %xmm1
	movdqu	0x90(%rsp), %xmm2
	movdqu	0xA0(%rsp), %xmm3
	movdqu	0xB0(%rsp), %xmm4
	movdqu	0xC0(%rsp), %xmm5
	pushq	0x68(%rsp)
	popfq
	# RAX and RDX are deliberately not restored: the replay below overwrites both
	# with the values the native site would have produced.
	movq	0x38(%rsp), %rcx
	movq	0x48(%rsp), %r8
	movq	0x50(%rsp), %r9
	movq	0x58(%rsp), %r10
	movq	0x60(%rsp), %r11
	leaq	0xE8(%rsp), %rsp
	.cfi_def_cfa_offset 8
	# Replay exactly: mov rax,[rbp-50]; lea rdx,[caller rsp+50].
	movq	-0x50(%rbp), %rax
	leaq	0x58(%rsp), %rdx
	retq
	.cfi_endproc
