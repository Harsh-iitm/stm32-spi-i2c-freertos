# Firmware source layout

`Core/Src/main.c` defines the sensor and UART output tasks. `board.c` configures USART2 and SPI1 using direct peripheral registers and provides polling and RX-interrupt SPI byte transfers. `i2c1.c` provides polling reads, an interrupt-driven receive path, and GPIO bus recovery. Hardware validation has not been performed in this session.

The CMSIS device header (`stm32f4xx.h`) and FreeRTOS kernel/ARM_CM4F port are external dependencies. See the root README before building.
