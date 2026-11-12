// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F407 firmware written like an STM32CubeIDE project with the
// STM32CubeF4 HAL (experiment E-12, SN-298, gate G12c.4): the circuit of
// example 69 driven through the HAL instead of the registers, at 16 MHz
// from the HSI (HAL's default clock).
//   SPI1 (PA5 SCK, PA7 MOSI) with PA4 as the latch: a 74HC595 counts every
//     250 ms (HAL_SPI_Transmit).
//   SPI2 (PB13 SCK, PB15 MOSI, 16-bit frames) with PB12 as LOAD: a MAX7219
//     shows a smiley on its 8x8 matrix; the last row counts.
//   I2C1 (PB6 SCL, PB7 SDA, 100 kHz): writes "SimNodus" into a 24C02 at 0x50
//     (HAL_I2C_Mem_Write), waits for its write cycle (HAL_I2C_IsDeviceReady),
//     reads it back (HAL_I2C_Mem_Read); reads the LM75 at 0x48 every second.
//   USART1 (PA9, 115200 baud) through DMA2 stream 7 (HAL_UART_Transmit_DMA):
//     "f4_hal_buses", "eeprom SimNodus", then "lm75 23.5" every second.
#include "stm32f4xx_hal.h"

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_tx;

#define EEPROM (0x50u << 1)
#define LM75 (0x48u << 1)

void Error_Handler(void);

static char line[64];

// Sends `text` through DMA once the previous line has gone.
static void print(const char* text)
{
    while (HAL_UART_GetState(&huart1) != HAL_UART_STATE_READY) {
    }
    uint16_t length = 0;
    while (text[length] != '\0' && length < sizeof line) {
        line[length] = text[length];
        ++length;
    }
    HAL_UART_Transmit_DMA(&huart1, (const uint8_t*)line, length);
}

static char* put_text(char* out, const char* text)
{
    while (*text != '\0') {
        *out++ = *text++;
    }
    return out;
}

static char* put_number(char* out, uint32_t value)
{
    char digits[10];
    int n = 0;
    do {
        digits[n++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    while (n > 0) {
        *out++ = digits[--n];
    }
    return out;
}

static void shift_register(uint8_t value)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, &value, 1, 10);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET); // the rising edge latches
}

static void max7219(uint8_t reg, uint8_t value)
{
    uint16_t word = (uint16_t)((reg << 8) | value);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi2, (uint8_t*)&word, 1, 10);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
    GPIO_InitTypeDef pin = {0};
    pin.Pin = GPIO_PIN_4;
    pin.Mode = GPIO_MODE_OUTPUT_PP;
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &pin);
    pin.Pin = GPIO_PIN_12;
    HAL_GPIO_Init(GPIOB, &pin);
}

static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA2_CLK_ENABLE();
    HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);
}

static void MX_SPI_Init(SPI_HandleTypeDef* spi, SPI_TypeDef* instance, uint32_t data_size, uint32_t prescaler)
{
    spi->Instance = instance;
    spi->Init.Mode = SPI_MODE_MASTER;
    spi->Init.Direction = SPI_DIRECTION_2LINES;
    spi->Init.DataSize = data_size;
    spi->Init.CLKPolarity = SPI_POLARITY_LOW;
    spi->Init.CLKPhase = SPI_PHASE_1EDGE;
    spi->Init.NSS = SPI_NSS_SOFT;
    spi->Init.BaudRatePrescaler = prescaler;
    spi->Init.FirstBit = SPI_FIRSTBIT_MSB;
    spi->Init.TIMode = SPI_TIMODE_DISABLE;
    spi->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    spi->Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(spi) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

int main(void)
{
    HAL_Init();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_SPI_Init(&hspi1, SPI1, SPI_DATASIZE_8BIT, SPI_BAUDRATEPRESCALER_16);
    MX_SPI_Init(&hspi2, SPI2, SPI_DATASIZE_16BIT, SPI_BAUDRATEPRESCALER_8);
    MX_I2C1_Init();
    MX_USART1_UART_Init();

    print("f4_hal_buses\r\n");
    shift_register(0);
    static const uint8_t smiley[8] = {0x3C, 0x42, 0xA5, 0x81, 0xA5, 0x99, 0x42, 0x3C};
    max7219(0x0F, 0);
    max7219(0x09, 0);
    max7219(0x0B, 7);
    max7219(0x0A, 8);
    for (uint8_t row = 0; row < 8u; ++row) {
        max7219((uint8_t)(row + 1u), smiley[row]);
    }
    max7219(0x0C, 1);

    static const char text[8] = {'S', 'i', 'm', 'N', 'o', 'd', 'u', 's'};
    char out[48];
    char* p = out;
    uint8_t back[9] = {0};
    if (HAL_I2C_Mem_Write(&hi2c1, EEPROM, 0x10, I2C_MEMADD_SIZE_8BIT, (uint8_t*)text, 8, 50) != HAL_OK
        || HAL_I2C_IsDeviceReady(&hi2c1, EEPROM, 100, 50) != HAL_OK
        || HAL_I2C_Mem_Read(&hi2c1, EEPROM, 0x10, I2C_MEMADD_SIZE_8BIT, back, 8, 50) != HAL_OK) {
        p = put_text(p, "eeprom: no answer");
    } else {
        p = put_text(p, "eeprom ");
        p = put_text(p, (const char*)back);
    }
    p = put_text(p, "\r\n");
    *p = '\0';
    print(out);

    uint32_t count = 0;
    uint32_t stepped = HAL_GetTick(), measured = HAL_GetTick();
    for (;;) {
        const uint32_t now = HAL_GetTick();
        if (now - stepped >= 250u) {
            stepped += 250u;
            ++count;
            shift_register((uint8_t)count);
            max7219(8, (uint8_t)count);
        }
        if (now - measured >= 1000u) {
            measured += 1000u;
            uint8_t raw[2];
            p = out;
            if (HAL_I2C_Mem_Read(&hi2c1, LM75, 0x00, I2C_MEMADD_SIZE_8BIT, raw, 2, 50) != HAL_OK) {
                p = put_text(p, "lm75: no answer");
            } else {
                const int32_t halves = (int16_t)(uint16_t)((raw[0] << 8) | raw[1]) / 128;
                const uint32_t magnitude = (uint32_t)(halves < 0 ? -halves : halves);
                p = put_text(p, halves < 0 ? "lm75 -" : "lm75 ");
                p = put_number(p, magnitude / 2u);
                p = put_text(p, (magnitude & 1u) ? ".5" : ".0");
            }
            p = put_text(p, "\r\n");
            *p = '\0';
            print(out);
        }
        __WFI(); // until the next SysTick
    }
}

// The MSP functions STM32CubeMX generates in stm32f4xx_hal_msp.c.
void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

void HAL_SPI_MspInit(SPI_HandleTypeDef* spi)
{
    GPIO_InitTypeDef pin = {0};
    pin.Mode = GPIO_MODE_AF_PP;
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    if (spi->Instance == SPI1) {
        __HAL_RCC_SPI1_CLK_ENABLE();
        pin.Pin = GPIO_PIN_5 | GPIO_PIN_7;
        pin.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOA, &pin);
    } else if (spi->Instance == SPI2) {
        __HAL_RCC_SPI2_CLK_ENABLE();
        pin.Pin = GPIO_PIN_13 | GPIO_PIN_15;
        pin.Alternate = GPIO_AF5_SPI2;
        HAL_GPIO_Init(GPIOB, &pin);
    }
}

void HAL_I2C_MspInit(I2C_HandleTypeDef* i2c)
{
    if (i2c->Instance != I2C1) {
        return;
    }
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef pin = {0};
    pin.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    pin.Mode = GPIO_MODE_AF_OD;
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    pin.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &pin);
    __HAL_RCC_I2C1_CLK_ENABLE();
}

void HAL_UART_MspInit(UART_HandleTypeDef* uart)
{
    if (uart->Instance != USART1) {
        return;
    }
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef pin = {0};
    pin.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    pin.Mode = GPIO_MODE_AF_PP;
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    pin.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &pin);

    hdma_usart1_tx.Instance = DMA2_Stream7;
    hdma_usart1_tx.Init.Channel = DMA_CHANNEL_4;
    hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_tx.Init.Mode = DMA_NORMAL;
    hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_usart1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(uart, hdmatx, hdma_usart1_tx);
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

// The interrupt handlers of stm32f4xx_it.c.
void SysTick_Handler(void)
{
    HAL_IncTick();
}

void DMA2_Stream7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

void Error_Handler(void)
{
    __disable_irq();
    for (;;) {
    }
}
