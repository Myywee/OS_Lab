strlen:
        xor rax, rax

        cond:
            cmp BYTE[rdi] ,0
            je end
            inc rax
            inc rdi
            jmp cond
        end:
            ret

global print

print:
        xor rdx, rdx

        push rdi
        call strlen
        pop rdi

        mov rsi, rdi 
        mov rdx, rax 
        mov rax, 1 
        mov rdi, 1 
        syscall
ret

