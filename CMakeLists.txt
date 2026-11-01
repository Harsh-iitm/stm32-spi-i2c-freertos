cmake_minimum_required(VERSION 3.22)
project(stm32_spi_i2c_freertos C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(FREERTOS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/FreeRTOS-Kernel)
set(CMSIS_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/CMSIS_5/CMSIS/Core/Include)
set(CMSIS_DEVICE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/cmsis-device-f4)
set(MCU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)

add_executable(${PROJECT_NAME}.elf
    Core/Src/main.c
    Core/Src/board.c
    Core/Src/i2c1.c
    ${CMSIS_DEVICE_DIR}/Source/Templates/system_stm32f4xx.c
    ${CMSIS_DEVICE_DIR}/Source/Templates/gcc/startup_stm32f401xe.s
    ${FREERTOS_DIR}/tasks.c
    ${FREERTOS_DIR}/queue.c
    ${FREERTOS_DIR}/list.c
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F/port.c
    ${FREERTOS_DIR}/portable/MemMang/heap_4.c
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMSIS_CORE_DIR}
    ${CMSIS_DEVICE_DIR}/Include
    ${FREERTOS_DIR}/include
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F
)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE STM32F401xE)
target_compile_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
    -Wno-unused-parameter
)
target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -T${CMAKE_CURRENT_SOURCE_DIR}/STM32F401RETX_FLASH.ld
    -nostartfiles
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map
    --specs=nosys.specs
)
set_target_properties(${PROJECT_NAME}.elf PROPERTIES LINKER_LANGUAGE C)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    BYPRODUCTS ${PROJECT_NAME}.hex ${PROJECT_NAME}.bin
    COMMENT "Creating Intel HEX and raw binary images"
)
