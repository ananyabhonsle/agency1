/* AGENCY-1 Linker Script
 * Kernel linked at 0x10000.
 * _start MUST be the first byte of the binary.
 */

ENTRY(_start)

SECTIONS
{
    . = 0x10000;

    /* _start's section first - KEEP ensures it isn't garbage collected */
    .text : {
        KEEP(*(.text._start))
        *(.text)
        *(.text.*)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
        *(.data.*)
    }

    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    . = ALIGN(512);
}
