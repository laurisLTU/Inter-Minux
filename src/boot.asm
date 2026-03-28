[org 0x7C00]
[bits 16]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [BOOT_DRIVE], dl

    mov si, loading_msg
.print_loading:
    lodsb
    or al, al
    jz .load_kernel
    mov ah, 0x0E
    int 0x10
    jmp .print_loading

.load_kernel:
    mov ax, 0x0013
    int 0x10

    mov bx, 0x1000
    mov dh, KERNEL_SECTORS
    mov dl, [BOOT_DRIVE]
    call disk_load

    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG:protected_mode_start

[bits 16]
disk_load:
    pusha

    mov ah, 0x02
    mov al, dh
    mov ch, 0x00
    mov dh, 0x00
    mov cl, 0x02
    int 0x13
    jc disk_error

    popa
    ret

disk_error:
    mov si, disk_error_msg
.print_error:
    lodsb
    or al, al
    jz $
    mov ah, 0x0E
    int 0x10
    jmp .print_error

[bits 32]
protected_mode_start:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    mov eax, 0x1000
    jmp eax

[bits 16]
loading_msg db 'Inter-Minux booting...', 0
disk_error_msg db 'Disk read error!', 0
BOOT_DRIVE db 0

gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ 0x08
DATA_SEG equ 0x10
%include "src/kernel_sectors.inc"

times 510-($-$$) db 0
dw 0xAA55
