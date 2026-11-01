#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "app.h"
#include <stdio.h>

#define SAMPLE_PERIOD_MS 10u
#define SENSOR_I2C_MODE_IRQ 1
static QueueHandle_t samples;
static SemaphoreHandle_t uart_lock;

static void log_line(const char *line) {
    if (xSemaphoreTake(uart_lock, portMAX_DELAY) == pdPASS) {
        uart_write(line);
        xSemaphoreGive(uart_lock);
    }
}

static void sensor_task(void *argument) {
    (void)argument;
    TickType_t wake = xTaskGetTickCount();
    imu_sample_t sample = {0};
    uint32_t sequence = 0;
    for (;;) {
#if SENSOR_I2C_MODE_IRQ
        int read_status = mpu6050_read_irq(&sample);
#else
        int read_status = mpu6050_read(&sample);
#endif
        if (read_status == 0) {
            sample.sequence = ++sequence;
            (void)xQueueOverwrite(samples, &sample);
        } else log_line("WARN: MPU6050 read failed; check I2C bus\r\n");
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}

static void output_task(void *argument) {
    (void)argument;
    imu_sample_t sample;
    char line[128];
    for (;;) {
        if (xQueueReceive(samples, &sample, portMAX_DELAY) == pdPASS) {
            int n = snprintf(line, sizeof line,
                "n=%lu accel=%d,%d,%d gyro=%d,%d,%d temp_raw=%d\r\n",
                (unsigned long)sample.sequence, sample.accel_x, sample.accel_y,
                sample.accel_z, sample.gyro_x, sample.gyro_y, sample.gyro_z,
                sample.temperature);
            if (n > 0 && (size_t)n < sizeof line) log_line(line);
        }
    }
}

int main(void) {
    board_init();
    spi1_init();
    samples = xQueueCreate(1, sizeof(imu_sample_t));
    uart_lock = xSemaphoreCreateMutex();
    configASSERT(samples != NULL && uart_lock != NULL);
    if (mpu6050_init() != 0) {
        uart_write("MPU6050 not detected; check power, address and I2C wiring\r\n");
    }
    configASSERT(xTaskCreate(sensor_task, "sensor", 512, NULL, 2, NULL) == pdPASS);
    configASSERT(xTaskCreate(output_task, "output", 512, NULL, 1, NULL) == pdPASS);
    vTaskStartScheduler();
    for (;;) { __WFI(); }
}

void vApplicationMallocFailedHook(void) {
    taskDISABLE_INTERRUPTS();
    for (;;) { __WFI(); }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name) {
    (void)task;
    (void)task_name;
    taskDISABLE_INTERRUPTS();
    for (;;) { __WFI(); }
}
