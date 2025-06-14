/*
 * Copyright (C), 2018-2019, Arm Technology (China) Co., Ltd.
 * All rights reserved
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Arm Technology (China) Co., Ltd. It is subject to the terms of a
 * License Agreement between Licensee and Arm Technology (China) Co., Ltd
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and,,
 * any derivative work shall retain this copyright notice.
 */

#include "soc/bk7258/reg_base.h"
#include "partitions.h"
#include "sdkconfig.h"

OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")

__MSP_STACK_SIZE = (4 << 10);
__MIN_HEAP_SIZE  = (32 << 10);
__MPU_PROTECT_SIZE = 0x0;

__SWAP_SIZE = (2048);

#if CONFIG_CPU0_SRAM_BASE
__PRIMARY_APP_RAM_BASE = CONFIG_CPU0_SRAM_BASE;
#else
__PRIMARY_APP_RAM_BASE = SOC_SRAM0_DATA_BASE;
#endif

#if CONFIG_LV_ATTRIBUTE_FAST_MEM_L2
__LVGL_SRAM_CODE_SIZE = 0x1000;
#else
__LVGL_SRAM_CODE_SIZE = 0;
#endif

MEMORY
{
  FLASH (rx)            : ORIGIN = (SOC_FLASH_DATA_BASE + CONFIG_PRIMARY_CPU1_APP_VIRTUAL_CODE_START), LENGTH = CONFIG_PRIMARY_CPU1_APP_VIRTUAL_CODE_SIZE
  RAM (rwx)             : ORIGIN = __PRIMARY_APP_RAM_BASE + CONFIG_CPU0_SPE_RAM_SIZE, LENGTH = CONFIG_CPU1_APP_RAM_SIZE
#if CONFIG_LV_ATTRIBUTE_FAST_MEM_L2
  LVGL_SRAM_CODE (rx)   : ORIGIN = SOC_SRAM5_DATA_BASE, LENGTH = __LVGL_SRAM_CODE_SIZE
#endif
  LVGL (rwx)            : ORIGIN = (SOC_SRAM5_DATA_BASE + __LVGL_SRAM_CODE_SIZE), LENGTH = 0x20000 - __LVGL_SRAM_CODE_SIZE

  ITCM (rwx)            : ORIGIN = SOC_ITCM_DATA_BASE + __MPU_PROTECT_SIZE, LENGTH = CONFIG_ITCM_SIZE - __MPU_PROTECT_SIZE
  DTCM (rwx)            : ORIGIN = SOC_DTCM_DATA_BASE,     LENGTH = CONFIG_DTCM_SIZE
  SWAP (rwx)            : ORIGIN = __PRIMARY_APP_RAM_BASE + CONFIG_CPU0_SPE_RAM_SIZE - __SWAP_SIZE, LENGTH = __SWAP_SIZE
  PSRAM (rwx)           : ORIGIN = SOC_PSRAM_DATA_BASE, LENGTH = 0x4000000
#if CONFIG_LV_CODE_LOAD_PSRAM
  LVGL_PSRAM_CODE (rx)  : ORIGIN = 0x60900000,  LENGTH = 0x50000
#endif
}

ENTRY(Reset_Handler)

SECTIONS
{

    ASSERT((. == ALIGN(512)), "vector table address align fault.")
    .vectors :
    {
        __vector_table = .;
        KEEP(*(.vectors))
        . = ALIGN(128);
    } > FLASH

    .gnu.sgstubs ALIGN(32) : ALIGN(32)
    {
        *(.gnu.sgstubs*)
        . = ALIGN(32);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        _stext = .;

        . = ALIGN(4);
        __devconfig_start = .;
        *(".devconfig.*")
        KEEP(*(SORT_BY_NAME(".devconfig*")))
        __devconfig_end = .;

        . = ALIGN(4);
        __apps_start = .;
        KEEP (*(.apps_data))
        __apps_end = .;
        _etext = .;
        . = ALIGN(4);
    } > FLASH

    .a_device_null :
    {
      KEEP(*(.a_deviceobj_null))
    } > FLASH

    .a_devices :
    {
      __device_start = .;
      KEEP(*(.a_deviceobj_*))
      __device_end = .;
    } > FLASH

    .a_init_entries :
    {
      __a_init_start = .;
      KEEP(*(.a_init_entry_*))
      __a_init_end = .;
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
        . = ALIGN(4);
    } > FLASH

    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > FLASH
    __exidx_end = .;

    .copy.table :
    {
        . = ALIGN(4);
        __copy_table_start__ = .;
        LONG (__etext)
        LONG (__iram_start__)
        LONG ((__data_end__ - __iram_start__) / 4)

        /* Add each additional data section here */
        LONG (__etext2)
        LONG (__data2_start__)
        LONG ((__data2_end__ - __data2_start__) / 4)

        LONG (__dtcm_data)
        LONG (__dtcm_start__)
        LONG ((__dtcm_end__ - __dtcm_start__) / 4)

        LONG (__itcm_text)
        LONG (__itcm_start__)
        LONG ((__itcm_end__ - __itcm_start__) / 4)
#if CONFIG_LV_CODE_LOAD_PSRAM
		LONG (__lvgl_psram_code)
        LONG (__lvgl_psram_code_start)
        LONG ((__lvgl_psram_code_end - __lvgl_psram_code_start) / 4)
#endif
#if CONFIG_LV_ATTRIBUTE_FAST_MEM_L2
        LONG (__lvgl_sram_code)
        LONG (__lvgl_sram_code_start)
        LONG ((__lvgl_sram_code_end - __lvgl_sram_code_start) / 4)
#endif
        __copy_table_end__ = .;
    } > FLASH

    .zero.table :
    {
        . = ALIGN(4);
        __zero_table_start__ = .;
        LONG (_bss_start)
        LONG ((_bss_end - _bss_start) / 4)
        LONG (_heap_start)
        LONG ((_heap_end - _heap_start) / 4)
        /* Add each additional bss section here */
        LONG (__bss2_start__)
        LONG ((__bss2_end__ - __bss2_start__) / 4)
		LONG (__lvgl_start)
        LONG ((__lvgl_end - __lvgl_start) / 4)
        __zero_table_end__ = .;
    } > FLASH

    .itcm :
    {
        . = ALIGN(4);
        PROVIDE(__itcm_text = LOADADDR(.itcm));
        __itcm_start__ = .;
        KEEP(*(.null_trap_handler))
        *(.itcm)
        *(.itcm_section*)
        *(.itcm_sec_code*)
        *(.interrupt)
        . = ALIGN(4);
        __itcm_end__ = .;
    } > ITCM AT > FLASH

    .dtcm :
    {
        . = ALIGN(4);
        PROVIDE(__dtcm_data = LOADADDR(.dtcm));
        __dtcm_start__ = .;

        *(.dtcm)
        *(.dtcm_sec_data*)
        . = ALIGN(4);
        __dtcm_end__ = .;
    } > DTCM AT > FLASH

    .data :
    {
        . = ALIGN(512);
        PROVIDE(__etext = LOADADDR(.data));
        PROVIDE(__iram_flash_begin = LOADADDR(.data));
        __iram_start__ = .;

        . = ALIGN(512);
        __vector_itcm_table = .;
        KEEP(*(.vectors_iram))
        . = ALIGN(128);


        *(.iram)
        . = ALIGN(4);
        __iram_end__ = .;

        . = ALIGN(4);
        __data_start__ = .;

        *(.data)
        *(".data.*")
        *(.sdata)

        *(.gnu.linkonce.d*)
        . = ALIGN(4);
        __data_end__ = .;
    } > RAM AT > FLASH
	
#if CONFIG_LV_CODE_LOAD_PSRAM
	.lvgl_psram_code : 
	{
        . = ALIGN(4);
        PROVIDE(__lvgl_psram_code = LOADADDR(.lvgl_psram_code));
		__lvgl_psram_code_start = .;
		*(.text.lv_*)
        . = ALIGN(4);		
		__lvgl_psram_code_end = .;
	} > LVGL_PSRAM_CODE AT > FLASH
#endif
	
#if CONFIG_LV_ATTRIBUTE_FAST_MEM_L2
	.lvgl_sram_code : 
	{
        . = ALIGN(4);
        PROVIDE(__lvgl_sram_code = LOADADDR(.lvgl_sram_code));
		__lvgl_sram_code_start = .;		
        *(.sram_sec_code*)
        . = ALIGN(4);		
		__lvgl_sram_code_end = .;
	} > LVGL_SRAM_CODE AT > FLASH
#endif

    _data_flash_begin = LOADADDR(.data);
    _data_ram_begin = ADDR(.data);
    _data_ram_end = .;

    s_rom_end = LOADADDR(.data) + SIZEOF(.data);

    .uninitialized (NOLOAD):
    {
        . = ALIGN(32);
        __uninitialized_start = .;
        *(.uninitialized)
        *(".uninitialized.*")
        KEEP(*(.keep.uninitialized))
        . = ALIGN(32);
        __uninitialized_end = .;
    } > RAM

    .bss (NOLOAD):
    {
        . = ALIGN(4);
        _bss_start = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > RAM

    bss_size = _bss_end - _bss_start;

    .heap (COPY) :
    {
        . = ALIGN(8);
        _heap_start = .;
        . = . + (ORIGIN(RAM) + LENGTH(RAM) - __MSP_STACK_SIZE - _heap_start - 8); /* 16 bytes for boundary protection */
        . = ALIGN(8);
        _heap_end = .;
    } > RAM

    .stack (ORIGIN(RAM) + LENGTH(RAM) - __MSP_STACK_SIZE) (COPY) :
    {
        . = ALIGN(8);
        _sstack = .;
        __StackLimit = .;
        . = . + __MSP_STACK_SIZE;
        . = ALIGN(8);
        __StackTop = .;
        _estack = .;
    } > RAM

    .swap ORIGIN(SWAP):
    {
        . = ALIGN(8);
        _swap_start = .;

        * (.swap_data)
        * (.swap_data*)
        . = ALIGN(4);
        _swap_end = .;
    } > SWAP AT > FLASH

	.lvgl ORIGIN(LVGL) (NOLOAD):
	{
		. = ALIGN(4);
		__lvgl_start = .;
		*(.lvgl_draw)
		*(.lvgl_draw*)
        . = ALIGN(4);
		__lvgl_end = .;
		
	} > LVGL AT > FLASH

    .video.cache.data :
    {
        . = ALIGN(4);
        PROVIDE(__etext2 = LOADADDR(.video.cache.data));
        __data2_start__ = .;

        *(.video_cache_data)
        *(.video_cache_data*)

        . = ALIGN(4);
        __data2_end__ = .;
    } > PSRAM AT > FLASH

    .video_cache_bss (NOLOAD):
    {
        . = ALIGN(4);
        __bss2_start__ = .;
        *(.video_cache_bss)
        *(.video_cache_bss*)
        . = ALIGN(4);
        __bss2_end__ = .;
    } > PSRAM
	


    /* Set stack top to end of RAM, and stack limit move down by
     * size of stack_dummy section */
    __MSPTop = ORIGIN(RAM) + LENGTH(RAM);
    __MSPLimit = __MSPTop - __MSP_STACK_SIZE;

    ASSERT((s_rom_end < ORIGIN(FLASH) + LENGTH(FLASH)), "ROM overflow!!!")
    ASSERT(((ORIGIN(RAM) + LENGTH(RAM)) > (_heap_start + __MSP_STACK_SIZE + 8)), "Stack overflowed with bss")
    ASSERT(((_heap_end - _heap_start) >= __MIN_HEAP_SIZE), "Heap smaller than minimize size 32K!!!")
}
