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

add_executable(siren-panel
    # Application (portable)
    firmware/app/app.c
    firmware/app/buttons.c
    firmware/app/player.c
    firmware/app/program.c
    # Board / target
    firmware/core/main.c
    firmware/core/port_stm32.c
    firmware/core/stm32f1xx_it.c
    firmware/core/syscalls.c
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
    ${HAL}/Src/stm32f1xx_hal_i2c.c
    ${HAL}/Src/stm32f1xx_hal_iwdg.c
    ${HAL}/Src/stm32f1xx_hal_pwr.c
    ${HAL}/Src/stm32f1xx_hal_rcc.c
    ${HAL}/Src/stm32f1xx_hal_rcc_ex.c
    ${HAL}/Src/stm32f1xx_hal_spi.c
)

target_include_directories(siren-panel PRIVATE
    firmware/app
    firmware/core
    firmware/drivers
    firmware/config
    ${HAL}/Inc
    ${DEV}/Include
    ${TP}/cmsis_core/CMSIS/Core/Include
    ${FATFS}
)

target_compile_definitions(siren-panel PRIVATE
    STM32F103xB
    USE_HAL_DRIVER
    $<$<CONFIG:Debug>:DEBUG>
)

target_compile_options(siren-panel PRIVATE
    ${MCU_FLAGS}
    -ffunction-sections -fdata-sections
    $<$<COMPILE_LANGUAGE:C>:-std=c11 -Wall -Wextra>
)

set(LDSCRIPT ${DEV}/Source/Templates/gcc/linker/STM32F103XB_FLASH.ld)
target_link_options(siren-panel PRIVATE
    ${MCU_FLAGS}
    -T${LDSCRIPT}
    --specs=nano.specs
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/siren-panel.map
    -Wl,--print-memory-usage
    -Wl,--no-warn-rwx-segments
)
set_target_properties(siren-panel PROPERTIES SUFFIX ".elf" LINK_DEPENDS ${LDSCRIPT})

# Vendor code is not ours to fix: silence its warnings.
set_source_files_properties(${FATFS}/ff.c PROPERTIES COMPILE_OPTIONS "-w")

add_custom_command(TARGET siren-panel POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:siren-panel> siren-panel.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:siren-panel> siren-panel.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:siren-panel>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
