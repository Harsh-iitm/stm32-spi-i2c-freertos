#ifndef APP_H
#define APP_H
#include <stdint.h>
typedef struct {
    int16_t accel_x, accel_y, accel_z;
    int16_t temperature;
    int16_t gyro_x, gyro_y, gyro_z;
    uint32_t sequence;
} imu_sample_t;
void board_init(void);
int mpu6050_init(void);
int mpu6050_read(imu_sample_t *sample);
int mpu6050_read_irq(imu_sample_t *sample);
void uart_write(const char *text);
void spi1_init(void);
uint8_t spi1_transfer(uint8_t value);
int spi1_transfer_irq(uint8_t value, uint8_t *received);
#endif
