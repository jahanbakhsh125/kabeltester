ENTRY(_start)
PHDRS {
    code PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}
SECTIONS {
    . = 0x80000000;
    .text : { *(.text.start) *(.text*) } :code
    .rodata : { *(.rodata*) } :code
    . = ALIGN(4096);
    .data : { *(.data*) *(.sdata*) } :data
    PROVIDE(__global_pointer$ = ADDR(.data) + 0x800);
    .bss : { __bss_start = .; *(.bss*) *(.sbss*) *(COMMON) __bss_end = .; } :data
    . = ALIGN(16);
    _end = .;
    PROVIDE(end = .);
    __stack_top = 0x81000000;
}
