cmake_minimum_required(VERSION 3.20)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake)
endif()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE MinSizeRel)
endif()

project(siren-panel C ASM)

set(TP  ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
set(HAL ${TP}/stm32f1xx_hal_driver)
set(DEV ${TP}/cmsis_device_f1)
set(FATFS ${TP}/stm32_mw_fatfs/source)

if(NOT EXISTS ${HAL}/Src/stm32f1xx_hal.c)
    message(FATAL_ERROR "Vendor code missing. Run: git submodule update --init --depth 1")
endif()

set(MCU_FLAGS -mcpu=cortex-m3 -mthumb)
set(LDSCRIPT ${DEV}/Source/Templates/gcc/linker/STM32F103XB_FLASH.ld)

# ---------------------------------------------------------------------------
# Compiler settings shared by everything
# ---------------------------------------------------------------------------
add_library(fw_options INTERFACE)
target_include_directories(fw_options INTERFACE
    firmware/app
    firmware/core
    firmware/drivers
    firmware/config
    ${HAL}/Inc
    ${DEV}/Include
    ${TP}/cmsis_core/CMSIS/Core/Include
    ${FATFS}
)
target_compile_definitions(fw_options INTERFACE
    STM32F103xB
    USE_HAL_DRIVER
    $<$<CONFIG:Debug>:DEBUG>
)
target_compile_options(fw_options INTERFACE
    ${MCU_FLAGS}
    -ffunction-sections -fdata-sections
    $<$<COMPILE_LANGUAGE:C>:-std=c11 -Wall -Wextra>
)

# ---------------------------------------------------------------------------
# Board support shared by the main and bring-up firmware
# ---------------------------------------------------------------------------
add_library(board_common OBJECT
    # Portable modules also used by the port layer
    firmware/app/buttons.c
    firmware/app/program.c
    # Board / target
    firmware/core/port_stm32.c
    firmware/core/system.c
    firmware/core/stm32f1xx_it.c
    firmware/core/syscalls.c
    firmware/drivers/hc165.c
    firmware/drivers/lcd1602.c
    firmware/drivers/sd_spi.c
    firmware/drivers/diskio_sd.c
    # FatFs
    ${FATFS}/ff.c
    # CMSIS device
    ${DEV}/Source/Templates/system_stm32f1xx.c
    ${DEV}/Source/Templates/gcc/startup_stm32f103xb.s
    # ST HAL
    ${HAL}/Src/stm32f1xx_hal.c
    ${HAL}/Src/stm32f1xx_hal_cortex.c
    ${HAL}/Src/stm32f1xx_hal_dma.c
    ${HAL}/Src/stm32f1xx_hal_flash.c
    ${HAL}/Src/stm32f1xx_hal_flash_ex.c
    ${HAL}/Src/stm32f1xx_hal_gpio.c
    ${HAL}/Src/stm32f1xx_hal_gpio_ex.c
    ${HAL}/Src/stm32f1xx_hal_iwdg.c
    ${HAL}/Src/stm32f1xx_hal_pwr.c
    ${HAL}/Src/stm32f1xx_hal_rcc.c
    ${HAL}/Src/stm32f1xx_hal_rcc_ex.c
    ${HAL}/Src/stm32f1xx_hal_spi.c
)
target_link_libraries(board_common PUBLIC fw_options)

# Vendor code is not ours to fix: silence its warnings.
set_source_files_properties(${FATFS}/ff.c PROPERTIES COMPILE_OPTIONS "-w")

# ---------------------------------------------------------------------------
# Firmware images
# ---------------------------------------------------------------------------
function(add_firmware name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE board_common fw_options)
    target_link_options(${name} PRIVATE
        ${MCU_FLAGS}
        -T${LDSCRIPT}
        --specs=nano.specs
        -Wl,--gc-sections
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${name}.map
        -Wl,--print-memory-usage
        -Wl,--no-warn-rwx-segments
    )
    set_target_properties(${name} PROPERTIES SUFFIX ".elf" LINK_DEPENDS ${LDSCRIPT})
    add_custom_command(TARGET ${name} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${name}> ${name}.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${name}> ${name}.hex
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${name}>
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endfunction()

# The siren control panel.
add_firmware(siren-panel
    firmware/core/main.c
    firmware/app/app.c
    firmware/app/player.c
)

# Hardware bring-up / test firmware (see docs/bringup.md).
add_firmware(siren-bringup
    firmware/bringup/bringup.c
    firmware/drivers/uart_log.c
)
