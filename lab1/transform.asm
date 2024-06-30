section .data
    InvalidInput db "Error", 0
    NewLine db 0xA

section .bss
    inputString resb 40
    num resb 32
    result resb 128

section .text
    global _start

_start:
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx

    mov ecx, 40
    mov edi, inputString
    cld
    rep stosb

    mov ecx, 32
    mov edi, num
    cld
    rep stosb

    mov ecx, 128
    mov edi, result
    cld
    rep stosb    
    
    mov esi, inputString
    dec esi
_readLoop:
    inc esi
    mov eax, 3
    mov ebx, 0
    mov ecx, esi
    mov edx, 1
    int 0x80
    cmp byte[esi], 0xA
    jne _readLoop

    cmp byte[inputString], 'q'
    je _exit

    mov esi, inputString
    call _judgeInput


    call _convertNum
    jmp _outPut
    

_outPut:
    mov esi, result
    call _strlen
    mov edx, eax;这里到时候要填result的长度
    mov eax, 4
    mov ebx, 1
    mov ecx, result

    int 0x80

    ; 输出换行符
    mov eax, 4
    mov ebx, 1
    mov ecx, NewLine
    mov edx, 1
    int 0x80

    jmp _start

_invalidInput:
    ; 输出错误信息
    mov eax, 4
    mov ebx, 1
    mov ecx, InvalidInput
    mov edx, 5
    int 0x80

    ; 输出换行符
    mov eax, 4
    mov ebx, 1
    mov ecx, NewLine
    mov edx, 1
    int 0x80

    jmp _start

_exit:
    ;退出程序
    mov eax, 1
    int 0x80

_judgeInput:
    xor r10, r10 ;r10w用于存储进制
    xor r11, r11 ;r11b用于存储输入的长度

_judgeLoop:
        cmp byte[esi], ' '
        je _judgeDone

        cmp byte[esi], '9'
        jg _invalidInput
        cmp byte[esi], '0'
        jl _invalidInput

        mov dl, byte[esi]
        mov byte[num + r11], dl

        inc esi
        inc r11

        jmp _judgeLoop

_judgeDone:
        add esi, 1
        cmp byte[esi], 'b'
        je .bType
        cmp byte[esi], 'o'
        je .oType
        cmp byte[esi], 'h'
        je .hType

        jmp _invalidInput

    .bType:
        mov r10, 2
        add esi, 1
        cmp byte[esi], 10
        jne _invalidInput

        ret

    .oType:
        mov r10, 8
        add esi, 1
        cmp byte[esi], 10
        jne _invalidInput

        ret

    .hType:
        mov r10, 16
        add esi, 1
        cmp byte[esi], 10
        jne _invalidInput

        ret

_convertNum:
    ;r10存储了进制信息
    ;r11存储了输入的长度
    ;num存储了输入的整数
    ;r12用于num计数
    ;r13用于result计数

    xor r13, r13
    xor ax, ax
    xor dx, dx

_convertLoop:
    xor r12, r12
    xor dx, dx
    .inner_loop:
        mov ax, dx
        mov r9w, 10
        mul r9w
        movzx bx, byte[num + r12]
        sub bx, '0' ;转换成数字
        add ax, bx
        xor bx, bx
        xor cx, cx
        xor dx, dx
        div r10b
        movzx dx, ah
        add al, '0' ;转换成字符
        mov byte[num + r12], al

        inc r12
        cmp r12, r11
        jne .inner_loop

    cmp dl, 10
    jge _hStore

    jmp _normal

_hStore:
    sub dl, 10
    add dl, 'a'
    jmp _storeResult

_normal:
    add dl, '0' ;转换成字符
_storeResult:
    mov byte[result + r13], dl
    inc r13

    call _notZero
    test eax, eax
    jz _convertLoop

    cmp r10, 2
    je .addB

    cmp r10, 8
    je .addO

    cmp r10, 16
    je .addH

    .addB:
        ;push r13
        mov byte[result + r13], 'b'
        inc r13
        mov byte[result + r13], '0'
        inc r13
        jmp _convertRet

    .addO:
        ;push r13
        mov byte[result + r13], 'o'
        inc r13
        mov byte[result + r13], '0'
        inc r13
        jmp _convertRet

    .addH:
        ;push r13
        mov byte[result + r13], 'x'
        inc r13
        mov byte[result + r13], '0'
        inc r13
        jmp _convertRet

_convertRet:
    call _reverseResult
    ret


;获取字符串长度
_strlen:
        xor ecx, ecx
        mov edi, esi

    .strlenLoop:
        cmp byte [edi + ecx], 0
        je .strlenDone
        inc ecx
        jmp .strlenLoop

    .strlenDone:
        mov eax , ecx
        ret

_reverseResult:
    xor r8, r8 ;r8用于temp
    xor r9, r9 ;r9用于temp
    mov r14, r13 ;r14用于反转次数
    mov r15, 0 ;计数
    shr r14, 1

    ; 反转结果
    .reverseLoop:
        dec r13
        mov r8b, byte[result + r13]
        mov r9b, byte[result + r15]
        mov byte[result + r15], r8b
        mov byte[result + r13], r9b
        inc r15

        cmp r15, r14
        jne .reverseLoop

    ret

_notZero:
    xor r8b, r8b ;r8b计数
    sub r8b, 1
    xor r9b, r9b ;r9b用于temp

    .notZeroLoop:
        inc r8b
        cmp r8, r11
        je .notZeroDone

        cmp byte[num + r8], '0'
        je .notZeroLoop

        mov eax, 0
        ret

    .notZeroDone:
        mov eax, 1
        ret