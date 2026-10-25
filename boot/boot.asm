; AGENCY-1 Boot Sector - simple CHS, 64 sectors (32 KiB)
[BITS 16]
[ORG 0x7C00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    mov [bios_drive], dl
    mov dx, 0x3F8 + 5
.w1:
    in al, dx
    test al, 0x20
    jz .w1
    mov dx, 0x3F8
    mov al, 0x42
    out dx, al
    mov ax, 0x1000
    mov es, ax
    xor bx, bx
    mov ah, 0x02
    mov al, 63
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [bios_drive]
    int 0x13
    jc disk_error
    mov dx, 0x3F8 + 5
.w2:
    in al, dx
    test al, 0x20
    jz .w2
    mov dx, 0x3F8
    mov al, 0x4B
    out dx, al
    in al, 0x92
    or al, 2
    out 0x92, al
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG:init_pm
[BITS 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    mov dx, 0x3F8 + 5
.w3:
    in al, dx
    test al, 0x20
    jz .w3
    mov dx, 0x3F8
    mov al, 0x50
    out dx, al
    jmp CODE_SEG:0x10000
disk_error:
    mov si, disk_err_msg
    mov dx, 0x3F8
.pl:
    lodsb
    test al, al
    jz .hang
    push dx
    add dx, 5
.w:
    in al, dx
    test al, 0x20
    jz .w
    pop dx
    mov al, [si - 1]
    out dx, al
    jmp .pl
.hang:
    hlt
    jmp .hang
bios_drive: db 0
disk_err_msg: db "DISK_ERR", 10, 0
gdt_start:
gdt_null:
    dd 0x0
    dd 0x0
gdt_code:
    dw 0xFFFF
    dw 0x0
    db 0x0
    db 10011010b
    db 11001111b
    db 0x0
gdt_data:
    dw 0xFFFF
    dw 0x0
    db 0x0
    db 10010010b
    db 11001111b
    db 0x0
gdt_end:
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start
CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start
times 510 - ($ - $$) db 0
dw 0xAA55
