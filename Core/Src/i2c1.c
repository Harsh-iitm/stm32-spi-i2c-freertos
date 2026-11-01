#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app.h"
#include <stddef.h>

#define I2C1_WAIT_LIMIT 100000u
#define MPU6050_ADDR 0x68u
static volatile uint32_t recovery_count;

enum { IRQ_IDLE, IRQ_START_WRITE, IRQ_ADDR_WRITE, IRQ_REG_TX,
       IRQ_START_READ, IRQ_ADDR_READ, IRQ_RX, IRQ_RX_LAST_TWO,
       IRQ_RX_LAST_ONE, IRQ_RX_SINGLE };
static struct {
    TaskHandle_t volatile owner;
    uint8_t address, reg, *data, length, index;
    volatile int result;
    volatile uint8_t phase;
} i2c_irq;

static void i2c_irq_finish(int result, BaseType_t *higher_priority_task_woken) {
    I2C1->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN);
    I2C1->CR1 |= I2C_CR1_ACK;
    i2c_irq.result = result;
    i2c_irq.phase = IRQ_IDLE;
    TaskHandle_t owner = i2c_irq.owner;
    i2c_irq.owner = 0;
    if (owner != 0) vTaskNotifyGiveFromISR(owner, higher_priority_task_woken);
}

static int wait_sr1(uint32_t mask) {
    uint32_t timeout = I2C1_WAIT_LIMIT;
    while ((I2C1->SR1 & mask) == 0u) {
        uint32_t errors = I2C1->SR1 & (I2C_SR1_BERR | I2C_SR1_ARLO |
                                       I2C_SR1_AF | I2C_SR1_OVR);
        if (errors != 0u) { I2C1->SR1 &= ~errors; return -1; }
        if (--timeout == 0u) return -1;
    }
    return 0;
}

static void i2c1_configure(void) {
    I2C1->CR1 = I2C_CR1_SWRST;
    I2C1->CR1 = 0u;
    I2C1->CR2 = 16u;       /* PCLK1 = 16 MHz (HSI default clock). */
    I2C1->CCR = 80u;       /* 100 kHz standard mode. */
    I2C1->TRISE = 17u;
    I2C1->CR1 = I2C_CR1_PE | I2C_CR1_ACK;
}

/* GPIO bit-bang bus clear: open-drain release, up to nine SCL pulses, STOP. */
static void i2c1_recover(void) {
    I2C1->CR1 = 0u;
    GPIOB->MODER = (GPIOB->MODER & ~((3u << 16) | (3u << 18))) |
                    (1u << 16) | (1u << 18);
    GPIOB->OTYPER |= (1u << 8) | (1u << 9);
    GPIOB->BSRR = (1u << 8) | (1u << 9);
    for (unsigned i = 0; i < 9u && (GPIOB->IDR & (1u << 9)) == 0u; ++i) {
        GPIOB->BSRR = 1u << (8 + 16); for (volatile unsigned d=0; d<100; ++d) {}
        GPIOB->BSRR = 1u << 8;        for (volatile unsigned d=0; d<100; ++d) {}
    }
    GPIOB->BSRR = 1u << (9 + 16); GPIOB->BSRR = 1u << 8;
    for (volatile unsigned d=0; d<100; ++d) {}
    GPIOB->BSRR = 1u << 9;
    GPIOB->MODER = (GPIOB->MODER & ~((3u << 16) | (3u << 18))) |
                    (2u << 16) | (2u << 18);
    GPIOB->AFR[1] = (GPIOB->AFR[1] & ~0xFFu) | 0x44u;
    i2c1_configure();
    NVIC_SetPriority(I2C1_EV_IRQn, 6u);
    NVIC_SetPriority(I2C1_ER_IRQn, 6u);
    NVIC_EnableIRQ(I2C1_EV_IRQn);
    NVIC_EnableIRQ(I2C1_ER_IRQn);
    ++recovery_count;
}

void i2c1_init(void) {
    GPIOB->MODER = (GPIOB->MODER & ~((3u << 16) | (3u << 18))) |
                    (2u << 16) | (2u << 18);
    GPIOB->OTYPER |= (1u << 8) | (1u << 9);
    GPIOB->PUPDR = (GPIOB->PUPDR & ~((3u << 16) | (3u << 18))) |
                    (1u << 16) | (1u << 18);
    GPIOB->AFR[1] = (GPIOB->AFR[1] & ~0xFFu) | 0x44u;
    i2c1_configure();
}

/* Interrupt receive state. The address/register preamble is polled; each data
 * byte is drained by I2C1_EV_IRQHandler and completion wakes the caller task. */
enum { IRQ_IDLE, IRQ_RX, IRQ_RX_LAST_TWO, IRQ_RX_LAST_ONE, IRQ_RX_SINGLE };
static struct {
    TaskHandle_t owner;
    uint8_t *data, length, index;
    volatile int result;
    volatile uint8_t phase;
} i2c_irq;

static void i2c_irq_finish(int result, BaseType_t *higher_priority_task_woken) {
    I2C1->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN);
    I2C1->CR1 |= I2C_CR1_ACK;
    i2c_irq.result = result;
    i2c_irq.phase = IRQ_IDLE;
    TaskHandle_t owner = i2c_irq.owner;
    i2c_irq.owner = 0;
    if (owner != 0) vTaskNotifyGiveFromISR(owner, higher_priority_task_woken);
}

static int i2c1_read_regs_irq(uint8_t address, uint8_t reg,
                              uint8_t *data, uint8_t length) {
    if (data == NULL || length == 0u || length == 2u || i2c_irq.owner != 0)
        return -1;
    if (I2C1->SR2 & I2C_SR2_BUSY) i2c1_recover();
    I2C1->CR1 |= I2C_CR1_ACK | I2C_CR1_START;
    if (wait_sr1(I2C_SR1_SB)) goto error;
    (void)I2C1->SR1; I2C1->DR = (uint8_t)(address << 1);
    if (wait_sr1(I2C_SR1_ADDR)) goto error;
    (void)I2C1->SR1; (void)I2C1->SR2; I2C1->DR = reg;
    if (wait_sr1(I2C_SR1_BTF)) goto error;
    I2C1->CR1 |= I2C_CR1_START;
    if (wait_sr1(I2C_SR1_SB)) goto error;
    (void)I2C1->SR1; I2C1->DR = (uint8_t)((address << 1) | 1u);
    if (wait_sr1(I2C_SR1_ADDR)) goto error;

    (void)ulTaskNotifyTake(pdTRUE, 0u);
    i2c_irq.owner = xTaskGetCurrentTaskHandle();
    i2c_irq.data = data; i2c_irq.length = length; i2c_irq.index = 0u;
    i2c_irq.result = -1;
    if (length == 1u) {
        I2C1->CR1 &= ~I2C_CR1_ACK;
        i2c_irq.phase = IRQ_RX_SINGLE;
    } else {
        I2C1->CR1 |= I2C_CR1_ACK;
        i2c_irq.phase = IRQ_RX;
    }
    (void)I2C1->SR1; (void)I2C1->SR2;
    if (length == 1u) I2C1->CR1 |= I2C_CR1_STOP;
    I2C1->CR2 |= I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN;

    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20u)) == 0u) {
        I2C1->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN);
        i2c_irq.owner = 0; i2c_irq.phase = IRQ_IDLE;
        I2C1->CR1 |= I2C_CR1_STOP | I2C_CR1_ACK;
        i2c1_recover();
        return -1;
    }
    int result = i2c_irq.result;
    if (result != 0) i2c1_recover();
    return result;
error:
    I2C1->CR1 |= I2C_CR1_STOP | I2C_CR1_ACK;
    i2c1_recover();
    return -1;
}

void I2C1_EV_IRQHandler(void) {
    BaseType_t higher_priority_task_woken = pdFALSE;
    uint32_t status = I2C1->SR1;
    if ((status & I2C_SR1_BTF) && i2c_irq.phase == IRQ_RX &&
        i2c_irq.index == (uint8_t)(i2c_irq.length - 3u)) {
        I2C1->CR1 &= ~I2C_CR1_ACK;
        i2c_irq.data[i2c_irq.index++] = (uint8_t)I2C1->DR;
        I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
        i2c_irq.phase = IRQ_RX_LAST_TWO;
    } else if ((status & I2C_SR1_BTF) && i2c_irq.phase == IRQ_RX_LAST_TWO) {
        I2C1->CR1 |= I2C_CR1_STOP;
        i2c_irq.data[i2c_irq.index++] = (uint8_t)I2C1->DR;
        i2c_irq.phase = IRQ_RX_LAST_ONE;
        I2C1->CR2 |= I2C_CR2_ITBUFEN;
    } else if ((status & I2C_SR1_RXNE) &&
               (I2C1->CR2 & I2C_CR2_ITBUFEN)) {
        if (i2c_irq.phase == IRQ_RX &&
            i2c_irq.index < (uint8_t)(i2c_irq.length - 3u)) {
            i2c_irq.data[i2c_irq.index++] = (uint8_t)I2C1->DR;
            if (i2c_irq.index == (uint8_t)(i2c_irq.length - 3u))
                I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
        } else if (i2c_irq.phase == IRQ_RX_LAST_ONE ||
                   i2c_irq.phase == IRQ_RX_SINGLE) {
            i2c_irq.data[i2c_irq.index++] = (uint8_t)I2C1->DR;
            i2c_irq_finish(i2c_irq.index == i2c_irq.length ? 0 : -1,
                           &higher_priority_task_woken);
        }
    }
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

void I2C1_ER_IRQHandler(void) {
    BaseType_t higher_priority_task_woken = pdFALSE;
    uint32_t errors = I2C1->SR1 & (I2C_SR1_BERR | I2C_SR1_ARLO |
                                    I2C_SR1_AF | I2C_SR1_OVR);
    I2C1->SR1 &= ~errors;
    I2C1->CR1 |= I2C_CR1_STOP;
    i2c_irq_finish(-1, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

/* Register read using bounded polling. ACK is restored on every exit path. */
static int i2c1_read_regs(uint8_t address, uint8_t reg,
                          uint8_t *data, uint8_t length) {
    if (data == NULL || length == 0u || length == 2u) return -1;
    if (I2C1->SR2 & I2C_SR2_BUSY) i2c1_recover();
    I2C1->CR1 |= I2C_CR1_ACK | I2C_CR1_START;
    if (wait_sr1(I2C_SR1_SB)) goto error;
    (void)I2C1->SR1; I2C1->DR = (uint8_t)(address << 1);
    if (wait_sr1(I2C_SR1_ADDR)) goto error;
    (void)I2C1->SR1; (void)I2C1->SR2;
    I2C1->DR = reg;
    if (wait_sr1(I2C_SR1_BTF)) goto error;
    I2C1->CR1 |= I2C_CR1_START;
    if (wait_sr1(I2C_SR1_SB)) goto error;
    (void)I2C1->SR1; I2C1->DR = (uint8_t)((address << 1) | 1u);
    if (wait_sr1(I2C_SR1_ADDR)) goto error;
    if (length == 1u) {
        I2C1->CR1 &= ~I2C_CR1_ACK;
        __disable_irq(); (void)I2C1->SR1; (void)I2C1->SR2;
        I2C1->CR1 |= I2C_CR1_STOP; __enable_irq();
        if (wait_sr1(I2C_SR1_RXNE)) goto error;
        data[0] = (uint8_t)I2C1->DR;
    } else {
        (void)I2C1->SR1; (void)I2C1->SR2;
        uint8_t i = 0u;
        while (i < (uint8_t)(length - 3u)) {
            if (wait_sr1(I2C_SR1_RXNE)) goto error;
            data[i++] = (uint8_t)I2C1->DR;
        }
        if (wait_sr1(I2C_SR1_BTF)) goto error;
        I2C1->CR1 &= ~I2C_CR1_ACK;
        data[i++] = (uint8_t)I2C1->DR;
        if (wait_sr1(I2C_SR1_BTF)) goto error;
        I2C1->CR1 |= I2C_CR1_STOP;
        data[i++] = (uint8_t)I2C1->DR;
        data[i] = (uint8_t)I2C1->DR;
    }
    I2C1->CR1 |= I2C_CR1_ACK;
    return 0;
error:
    I2C1->CR1 |= I2C_CR1_STOP | I2C_CR1_ACK;
    i2c1_recover();
    return -1;
}

int mpu6050_init(void) {
    uint8_t id = 0u, wake = 0u;
    i2c1_init();
    if (i2c1_read_regs(MPU6050_ADDR, 0x75u, &id, 1u) != 0 || id != 0x68u)
        return -1;
    /* Write PWR_MGMT_1 via a short direct transaction. */
    I2C1->CR1 |= I2C_CR1_START;
    if (wait_sr1(I2C_SR1_SB)) return -1;
    (void)I2C1->SR1; I2C1->DR = MPU6050_ADDR << 1;
    if (wait_sr1(I2C_SR1_ADDR)) return -1;
    (void)I2C1->SR1; (void)I2C1->SR2;
    I2C1->DR = 0x6Bu;
    if (wait_sr1(I2C_SR1_TXE)) return -1;
    I2C1->DR = wake;
    if (wait_sr1(I2C_SR1_BTF)) return -1;
    I2C1->CR1 |= I2C_CR1_STOP;
    return 0;
}

int mpu6050_read(imu_sample_t *sample) {
    uint8_t raw[14];
    if (sample == NULL || i2c1_read_regs(MPU6050_ADDR, 0x3Bu, raw, 14u)) return -1;
    sample->accel_x=(int16_t)((raw[0]<<8)|raw[1]);
    sample->accel_y=(int16_t)((raw[2]<<8)|raw[3]);
    sample->accel_z=(int16_t)((raw[4]<<8)|raw[5]);
    sample->temperature=(int16_t)((raw[6]<<8)|raw[7]);
    sample->gyro_x=(int16_t)((raw[8]<<8)|raw[9]);
    sample->gyro_y=(int16_t)((raw[10]<<8)|raw[11]);
    sample->gyro_z=(int16_t)((raw[12]<<8)|raw[13]);
    return 0;
}

int mpu6050_read_irq(imu_sample_t *sample) {
    uint8_t raw[14];
    if (sample == NULL || i2c1_read_regs_irq(MPU6050_ADDR, 0x3Bu,
                                              raw, 14u)) return -1;
    sample->accel_x=(int16_t)((raw[0]<<8)|raw[1]);
    sample->accel_y=(int16_t)((raw[2]<<8)|raw[3]);
    sample->accel_z=(int16_t)((raw[4]<<8)|raw[5]);
    sample->temperature=(int16_t)((raw[6]<<8)|raw[7]);
    sample->gyro_x=(int16_t)((raw[8]<<8)|raw[9]);
    sample->gyro_y=(int16_t)((raw[10]<<8)|raw[11]);
    sample->gyro_z=(int16_t)((raw[12]<<8)|raw[13]);
    return 0;
}
