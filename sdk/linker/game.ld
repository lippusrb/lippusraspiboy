/* Linker dei giochi. I numeri base devono restare uguali a
   sdk/include/lippus/layout.h (lo controlla tools/check_layout.py). */

ENTRY(lippus_game_entry)

OS_FLASH_LIMIT = 0x40000;
FLASH_SIZE = 0x200000;
INSTALL_RESERVE = 0x1000;
OS_RAM_LENGTH = 0x2D800;
SRAM_ORIGIN = 0x20000000;
SRAM_END = 0x20040000;

MEMORY {
    FLASH (rx) : ORIGIN = 0x10000000 + OS_FLASH_LIMIT, LENGTH = FLASH_SIZE - OS_FLASH_LIMIT - INSTALL_RESERVE
    RAM (rwx) : ORIGIN = SRAM_ORIGIN + OS_RAM_LENGTH, LENGTH = SRAM_END - SRAM_ORIGIN - OS_RAM_LENGTH
}

SECTIONS {
    .text : {
        KEEP(*(.text.lippus_game_entry))
        KEEP(*(.lippus_orient))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
    .ARM.exidx : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } > FLASH

    .data : {
        . = ALIGN(4);
        __data_start = .;
        *(.data*)
        . = ALIGN(4);
        __data_end = .;
    } > RAM AT > FLASH
    __data_load = LOADADDR(.data);

    .bss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM

    __heap_start = __bss_end;
    __heap_end = ORIGIN(RAM) + LENGTH(RAM);
    ASSERT(__bss_end <= ORIGIN(RAM) + LENGTH(RAM), "RAM del gioco esaurita")
    ASSERT(__data_end <= ORIGIN(RAM) + LENGTH(RAM), "RAM del gioco esaurita")

    /DISCARD/ : { *(.comment) *(.debug*) *(.ARM.attributes) *(.eh_frame*) }
}
