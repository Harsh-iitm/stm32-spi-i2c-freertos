#include "stm32f4xx.h"
#include "app.h"

void board_init(void) {
    /* Nucleo-F401RE starts from the 16 MHz HSI clock; no PLL is assumed. */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN | RCC_APB1ENR_I2C1EN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC->APB2ENR;
    GPIOA->MODER = (GPIOA->MODER & ~((3u<<4)|(3u<<6))) | (2u<<4) | (2u<<6);
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~((15u<<8)|(15u<<12))) | (7u<<8) | (7u<<12);
    USART2->BRR = 139u; /* 16 MHz / 115200 */
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

void uart_write(const char *text) {
    if (text == 0) return;
    while (*text != '\0') {
        while ((USART2->SR & USART_SR_TXE) == 0u) {}
        USART2->DR = (uint8_t)*text++;
    }
    while ((USART2->SR & USART_SR_TC) == 0u) {}
}

void spi1_init(void) {
    /* PA4 GPIO CS; PA5/6/7 AF5 for SCK/MISO/MOSI. */
    GPIOA->MODER = (GPIOA->MODER & ~((3u<<8)|(3u<<10)|(3u<<12)|(3u<<14))) |
                   (1u<<8) | (2u<<10) | (2u<<12) | (2u<<14);
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~((15u<<20)|(15u<<24)|(15u<<28))) |
                    (5u<<20) | (5u<<24) | (5u<<28);
    GPIOA->BSRR = 1u << 4;
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_0;
    SPI1->CR1 |= SPI_CR1_SPE;
}

uint8_t spi1_transfer(uint8_t value) {
    while ((SPI1->SR & SPI_SR_TXE) == 0u) {}
    *(__IO uint8_t *)&SPI1->DR = value;
    while ((SPI1->SR & SPI_SR_RXNE) == 0u) {}
    return *(__IO uint8_t *)&SPI1->DR;
}
