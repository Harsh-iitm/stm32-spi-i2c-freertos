# STM32 SPI/I2C Bare-Metal Drivers with FreeRTOS

Register-level firmware for an STM32 Nucleo-F401RE. The source tree contains direct-register GPIO, SPI1, USART2 and I2C1 setup, MPU6050 reads, an I2C bus-clear routine, and two FreeRTOS tasks connected by a queue.

The implementation includes polling and interrupt-driven SPI byte transfers, polling I2C reads, and an I2C interrupt-driven receive path.

## What is in this repository

- `Core/Src/board.c` — 16 MHz HSI board setup, USART2 debug output, and SPI1 polling/interrupt byte transfers.
- `Core/Src/i2c1.c` — bounded polling I2C register reads, an interrupt-driven receive path, ACK/NACK sequencing, MPU6050 wake-up, and GPIO bus recovery.
- `Core/Src/main.c` — 100 Hz sensor task, selectable polling/interrupt I2C mode, one-element overwrite queue, and UART output task.
- `FreeRTOSConfig.h` — Cortex-M4F FreeRTOS configuration with dynamic allocation and mutexes enabled.

Peripheral registers are accessed directly; STM32 HAL is not used. CMSIS Core, the STM32F4 device package, and FreeRTOS Kernel are pinned as Git submodules. The repository includes its startup integration, linker script, CMake project, and Arm GNU toolchain file.

## Board connections

| Nucleo-F401RE | Signal | MPU6050 / host |
| --- | --- | --- |
| PB8 | I2C1 SCL | SCL |
| PB9 | I2C1 SDA | SDA |
| PA2 / PA3 | USART2 TX / RX | ST-LINK virtual COM port |
| PA5 / PA6 / PA7 | SPI1 SCK / MISO / MOSI | SPI peripheral |
| PA4 | SPI chip-select GPIO | CS |
| 3V3 / GND | Power | MPU6050 VCC / GND |

Use external I2C pull-up resistors if they are not present on the sensor breakout. The default I2C address is `0x68` (AD0 low). The code assumes the MCU remains on the reset-default 16 MHz HSI clock and configures I2C for 100 kHz.

## Build

Install CMake, Ninja, and the Arm GNU Toolchain. Clone with the pinned dependencies, then build:

```sh
git clone --recurse-submodules https://github.com/Harsh-iitm/stm32-spi-i2c-freertos.git
cd stm32-spi-i2c-freertos
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
cmake --build build
```

The build produces `stm32_spi_i2c_freertos.elf`, `.hex`, and `.bin` files. The project uses the included STM32F401RE linker script and startup file from the pinned CMSIS device package. Flash the ELF or HEX using STM32CubeProgrammer, then open the ST-LINK virtual COM port at 115200 baud.

## Transfer modes and scheduling

`SENSOR_I2C_MODE_IRQ` in `Core/Src/main.c` selects the interrupt-driven or polling I2C sampling path. The sensor task publishes the latest successful sample to a one-element overwrite queue; the output task consumes samples and writes formatted records over UART. A FreeRTOS mutex protects UART writes.

## Skills

Embedded C · Bare-metal register programming · STM32 · FreeRTOS · SPI · I2C · UART · ISR design · task synchronization · fault recovery
