	push 2
	pop rax
	mov [v], rax
global _start
section .text
_start:
	pop rax
	mov [a], rax
	push qword [mon]
	push 2
	pop rax
	mov [b], rax
	mov rax, 60
	mov rdi, 0
	syscall

section .bss
getchar resq 1
b resq 1
mon resq 1
main resq 1
