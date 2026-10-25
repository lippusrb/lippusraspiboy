.syntax unified
.cpu cortex-m0plus
.thumb

.section .text.lippus_game_entry, "ax", %progbits
.align 2
.global lippus_game_entry
.thumb_func
lippus_game_entry:
    push {r4, lr}
    mov r4, r0

    ldr r1, =__data_load
    ldr r2, =__data_start
    ldr r3, =__data_end
1:
    cmp r2, r3
    bcs 2f
    ldr r0, [r1]
    adds r1, r1, #4
    str r0, [r2]
    adds r2, r2, #4
    b 1b
2:
    ldr r2, =__bss_start
    ldr r3, =__bss_end
    movs r0, #0
3:
    cmp r2, r3
    bcs 4f
    str r0, [r2]
    adds r2, r2, #4
    b 3b
4:
    ldr r0, =g_lippus_api
    str r4, [r0]
    bl lippus_runtime_start
    pop {r4, pc}
    .ltorg
