global _start
section .text
_start:
	push 2
	push 6
	pop rbx
	pop rax
	sub rax, rbx
	push rax
	mov rax, 60
	mov rdi, 0
	syscall
