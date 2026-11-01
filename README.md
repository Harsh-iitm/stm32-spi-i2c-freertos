# STM32 SPI/I2C Bare-Metal Drivers with FreeRTOS

Bare-metal SPI and I2C peripheral-driver work for the **STM32 Nucleo-F401RE**, integrating an **MPU6050 IMU**, UART debug output, and a small FreeRTOS task scheduler. Peripheral access is implemented with direct register manipulation, without STM32 HAL.

## Project highlights

- Implemented SPI and I2C transfers in both polling and interrupt-driven modes.
- Connected an MPU6050 over I2C and used UART for debug logging.
- Diagnosed an I2C bus lockup related to ACK-flag handling and corrected the recovery path. Fault-injection tests reported 100% bus recovery.
- Split sensor acquisition and data output across two FreeRTOS tasks, synchronized with a queue.
- Resolved a priority inversion issue using priority inheritance, reducing worst-case task response latency by approximately 35%.
- Benchmarked polling and interrupt-driven I2C at equivalent sampling rates; the interrupt-driven design used approximately 40% less CPU.

## Hardware and software

| Component | Details |
| --- | --- |
| MCU board | STM32 Nucleo-F401RE |
| Sensor | MPU6050 IMU over I2C |
| Interfaces | SPI, I2C, UART |
| Language | Embedded C |
| Scheduler | FreeRTOS |

## Design overview

The sensor task acquires IMU data through the I2C driver and passes readings to the output task through a FreeRTOS queue. The output task handles data reporting over UART. The peripheral drivers support polling and interrupt-driven transfers; the interrupt-driven I2C path was compared against polling at the same sampling rate.

```text
MPU6050 --I2C--> I2C driver --> Sensor task --FreeRTOS queue--> Output task --> UART logs
                         STM32 Nucleo-F401RE
```

## Results

| Measure | Reported result |
| --- | --- |
| I2C bus recovery under fault injection | 100% |
| Worst-case task response latency after priority-inversion fix | ~35% lower |
| CPU utilization, interrupt-driven vs polling I2C at equivalent sampling rates | ~40% lower |

These figures are from the project's reported tests and benchmarks. Exact test setup, sample counts, and timing parameters were not included in the project summary.

## Skills demonstrated

Embedded C · Bare-metal register programming · STM32 · FreeRTOS · SPI · I2C · UART · ISR design · Task synchronization · Priority inheritance · Fault diagnosis and benchmarking
