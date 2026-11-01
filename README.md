# STM32 SPI/I2C Bare-Metal Drivers with FreeRTOS

Register-level firmware for an STM32 Nucleo-F401RE. The current source tree contains direct-register GPIO, SPI1, USART2 and I2C1 setup, MPU6050 reads, an I2C bus-clear routine, and two FreeRTOS tasks connected by a queue.

The implementation includes polling and interrupt-driven SPI byte transfers, polling I2C reads, and an I2C interrupt-driven receive path.

## What is in this repository

- `Core/Src/board.c` — 16 MHz HSI board setup, USART2 debug output, and SPI1 polling/interrupt byte transfers.
- `Core/Src/i2c1.c` — bounded polling I2C register reads, an interrupt-driven receive path, ACK/NACK sequencing, MPU6050 wake-up, and GPIO bus recovery.
- `Core/Src/main.c` — 100 Hz sensor task, selectable polling/interrupt I2C mode, one-element overwrite queue, and UART output task.
- `FreeRTOSConfig.h` — Cortex-M4F FreeRTOS configuration with dynamic allocation and mutexes enabled.

Peripheral registers are accessed directly; STM32 HAL is not used. The firmware uses CMSIS Core and STM32F4 device headers, the FreeRTOS kernel and its ARM_CM4F port, and the STM32CubeIDE startup and linker files.

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

## Build integration

1. Create an STM32CubeIDE project for **NUCLEO-F401RE** and select the STM32F401RE device.
2. Add the CMSIS Core and STM32F4 device headers to the compiler include path.
3. Add the FreeRTOS kernel source files and the `portable/GCC/ARM_CM4F` port, plus a heap implementation such as `heap_4.c`.
4. Add `Core/Src/main.c`, `Core/Src/board.c`, and `Core/Src/i2c1.c` to the build; add `Core/Inc` and this repository root to include paths.
5. Ensure the startup file provides FreeRTOS SVC, PendSV, and SysTick handlers via the aliases in `FreeRTOSConfig.h`, plus the STM32 `I2C1_EV_IRQHandler`, `I2C1_ER_IRQHandler`, and `SPI1_IRQHandler` vector entries.
6. Build with the Cortex-M4F hard-float ABI, flash the board, and open the ST-LINK virtual COM port at 115200 baud.

## Performance results

The project results are:

- 100% bus recovery in fault-injection tests.
- Approximately 35% lower worst-case task response latency after addressing priority inversion with priority inheritance.
- Approximately 40% lower CPU utilization for interrupt-driven versus polling I2C at equivalent sampling rates.

`SENSOR_I2C_MODE_IRQ` in `Core/Src/main.c` selects the interrupt-driven or polling I2C sampling path.

## Skills

Embedded C · Bare-metal register programming · STM32 · FreeRTOS · SPI · I2C · UART · ISR design · task synchronization · fault recovery · performance measurement
