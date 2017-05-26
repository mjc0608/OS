#include <inc/mmu.h>
#include <inc/memlayout.h>

.text
.globl _divzero_upcall
_divzero_upcall:
	// Call the C page fault handler.
	pushl %esp			// function argument: pointer to UTF
	movl _divzero_handler, %eax
	call *%eax
	addl $4, %esp			// pop function argument
	movl 0x28(%esp), %eax  // eip
	movl 0x30(%esp), %ecx  // esp
	subl $4, %ecx
	movl %eax, (%ecx)
	movl %ecx, 0x30(%esp)
	addl $8, %esp
	popal
	add $4, %esp
	popfl
	popl %esp
	ret


.text
.globl _gpflt_upcall
_gpflt_upcall:
	// Call the C page fault handler.
	pushl %esp			// function argument: pointer to UTF
	movl _gpflt_handler, %eax
	call *%eax
	addl $4, %esp			// pop function argument
	movl 0x28(%esp), %eax  // eip
	movl 0x30(%esp), %ecx  // esp
	subl $4, %ecx
	movl %eax, (%ecx)
	movl %ecx, 0x30(%esp)
	addl $8, %esp
	popal
	add $4, %esp
	popfl
	popl %esp
	ret


.text
.globl _illop_upcall
_illop_upcall:
	// Call the C page fault handler.
	pushl %esp			// function argument: pointer to UTF
	movl _illop_handler, %eax
	call *%eax
	addl $4, %esp			// pop function argument
	movl 0x28(%esp), %eax  // eip
	movl 0x30(%esp), %ecx  // esp
	subl $4, %ecx
	movl %eax, (%ecx)
	movl %ecx, 0x30(%esp)
	addl $8, %esp
	popal
	add $4, %esp
	popfl
	popl %esp
	ret
