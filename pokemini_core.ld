/*
 * PokeMini core linker script — RAM_EMU + ITCM hot segment.
 *
 * ITCM holds CPU-hot .text (Minx CPU/opcodes, timers, IRQ, IO, PRC, LCD,
 * audio, video blit). Emulator working buffers that used to live in ITCM
 * via itc_malloc are now allocated from DTCM (dtc_malloc) at runtime.
 */

INCLUDE gnw_ram_emu.ld
INCLUDE gnw_itcm_core.ld

MEMORY
{
  RAM_EMU (xrw) : ORIGIN = __RAM_EMU_START__, LENGTH = __RAM_EMU_LENGTH__
  ITCM    (xrw) : ORIGIN = __ITCM_CORE_START__, LENGTH = __ITCM_CORE_LENGTH__
}

/* Object paths match BUILD_DIR=build/core (flat names under that dir). */

SECTIONS
{
  .core_entry __RAM_EMU_START__ :
  {
    KEEP(*(.core_entry))
    KEEP(*(.gw_core_bridge_probe))

    . = ALIGN(4);
    EXCLUDE_FILE(
      build/core/MinxCPU.o
      build/core/MinxCPU_CE.o
      build/core/MinxCPU_CF.o
      build/core/MinxCPU_SP.o
      build/core/MinxCPU_XX.o
      build/core/MinxTimers.o
      build/core/MinxIRQ.o
      build/core/MinxIO.o
      build/core/MinxPRC.o
      build/core/MinxColorPRC.o
      build/core/MinxLCD.o
      build/core/MinxAudio.o
      build/core/Hardware.o
      build/core/Video.o
      build/core/Video_x3.o
      build/core/PokeMini.o
    ) *(.text*)
    *(.rodata*)
    *(.glue_7)
    *(.glue_7t)

    . = ALIGN(4);
    __CORE_INIT_ARRAY_START__ = .;
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    __CORE_INIT_ARRAY_END__ = .;
  } >RAM_EMU

  .data :
  {
    *(.data*)
  } >RAM_EMU

  __CORE_CODE_END__ = .;

  .bss (NOLOAD) :
  {
    . = ALIGN(8);
    *(.bss*)
    *(COMMON)
    . = ALIGN(8);
  } >RAM_EMU

  __CORE_BSS_END__ = .;

  ASSERT(__CORE_BSS_END__ <= __RAM_EMU_END__,
         "Error: PokeMini RAM_EMU segment (code+data+bss) overflows RAM_EMU")

  .core_itcm __ITCM_CORE_START__ :
  {
    __CORE_ITCM_CODE_START__ = .;
    build/core/MinxCPU.o(.text*)
    build/core/MinxCPU_CE.o(.text*)
    build/core/MinxCPU_CF.o(.text*)
    build/core/MinxCPU_SP.o(.text*)
    build/core/MinxCPU_XX.o(.text*)
    build/core/MinxTimers.o(.text*)
    build/core/MinxIRQ.o(.text*)
    build/core/MinxIO.o(.text*)
    build/core/MinxPRC.o(.text*)
    build/core/MinxColorPRC.o(.text*)
    build/core/MinxLCD.o(.text*)
    build/core/MinxAudio.o(.text*)
    build/core/Hardware.o(.text*)
    build/core/Video.o(.text*)
    build/core/Video_x3.o(.text*)
    build/core/PokeMini.o(.text*)
    . = ALIGN(4);
    __CORE_ITCM_CODE_END__ = .;
  } >ITCM AT>RAM_EMU

  __CORE_ITCM_BSS_END__ = __CORE_ITCM_CODE_END__;

  ASSERT(__CORE_ITCM_BSS_END__ <= __ITCM_CORE_START__ + __ITCM_CORE_LENGTH__,
         "Error: PokeMini ITCM segment overflows the 64K ITCM region")

  __exidx_start = .;
  __exidx_end = .;

  /DISCARD/ :
  {
    *(.fini_array*)
    *(.preinit_array*)
    *(.ARM.exidx*)
    *(.ARM.extab*)
    *(.comment)
    *(.ARM.attributes)
  }
}
