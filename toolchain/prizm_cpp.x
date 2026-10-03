OUTPUT_FORMAT(binary)
OUTPUT_ARCH(sh3)

ENTRY(initialize)

MEMORY
{

        rom (rx) : o = 0x00300000, l = 1024k
        ram (rwx) : o = 0x08100004, l = 64k
        ilram (rwx) : o = 0xE5200000, l = 4k
}

SECTIONS
{

        .text : {
                *(.pretext)
                *(.text)
                *(.text.*)
        } > rom

        .ilram : {
                _ilramld = LOADADDR(.data) ;
                _silram = . ;
                *(.ilram)
                *(.ilram.*)
                _eilram = . ;
        } > ilram AT> rom

        .rodata : {
                *(.rodata)
                *(.rodata.*)
        } > rom

        .init_array : ALIGN(4) {
                ___init_array_start = . ;
                __init_array_start = . ;
                KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
                KEEP(*(.init_array))
                KEEP(*(SORT_BY_INIT_PRIORITY(.ctors.*)))
                KEEP(*(.ctors))
                ___init_array_end = . ;
                __init_array_end = . ;
        } > rom

        .data : ALIGN(4) {
                _datald = LOADADDR(.data) ;
                _sdata = . ;
                *(.data)
                *(.data.*);
                _edata = . ;
        } >ram AT>rom

        .bss : ALIGN(4) {
                _bbss = . ;
                *(.bss)
                *(.bss*)
                *(COMMON)
                _ebss = . ;
        } >ram
}
