	push 2
global _start
section .text
_start:
	push 2
	mov rax, 60
	mov rdi, 0
	syscall
