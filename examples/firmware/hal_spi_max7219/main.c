// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Blue Pill firmware written like an STM32CubeIDE project with the STM32CubeF1
// HAL (SN-204): SPI1 drives a MAX7219 that scans an 8x8 LED matrix.
//   SPI1 (master, 8 bits, CPOL 0, CPHA 0, MSB first, 72 MHz / 16 = 4.5 MHz):
//     PA5 SCK -> CLK, PA7 MOSI -> DIN; PA4 (a GPIO, the chip select) -> LOAD.
//   Each MAX7219 word is two bytes: the register (1-8 for the rows), then the
//   data; LOAD rising stores it.
//   The matrix shows a heart and a smiley in turn, one second each; register
//   10 (intensity) steps up with every picture.
//   PC13 (on-board LED, lit when low): toggles with every picture.
// Between pictures the CPU sleeps (WFI) until the next SysTick interrupt
// instead of spinning in HAL_Delay: less power on a board, a faster simulation.
#include "stm32f1xx_hal.h"

SPI_HandleTypeDef hspi1;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
void Error_Handler(void);

static const uint8_t heart[8] = {0x00, 0x66, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C, 0x18};
static const uint8_t smiley[8] = {0x3C, 0x42, 0xA5, 0x81, 0xA5, 0x99, 0x42, 0x3C};

// One register write: LOAD low, two bytes, LOAD high (the rising edge stores them).
static void max7219_write(uint8_t reg, uint8_t data)
{
    const uint8_t word[2] = {reg, data};
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    if (HAL_SPI_Transmit(&hspi1, (uint8_t*)word, 2, 10) != HAL_OK) {
        Error_Handler();
    }
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
}

// HAL_Delay without the busy loop: sleep until each millisecond tick.
static void sleep_ms(uint32_t ms)
{
    const uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < ms) {
        __WFI();
    }
}

static void show(const uint8_t rows[8])
{
    for (uint8_t r = 0; r < 8; ++r) {
        max7219_write((uint8_t)(r + 1), rows[r]);
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();

    max7219_write(0x0F, 0x00); // display test off
    max7219_write(0x0C, 0x01); // leave shutdown
    max7219_write(0x0B, 0x07); // scan all eight digits (rows)
    max7219_write(0x09, 0x00); // no decoding: the bits are the LEDs
    uint8_t intensity = 0;
    while (1) {
        max7219_write(0x0A, intensity);
        intensity = (uint8_t)((intensity + 4u) & 0x0Fu);
        show(heart);
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        sleep_ms(1000);
        show(smiley);
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        sleep_ms(1000);
    }
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_SPI1_Init(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

// What STM32CubeIDE generates in stm32f1xx_hal_msp.c and stm32f1xx_it.c.
void HAL_MspInit(void)
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
}

void HAL_SPI_MspInit(SPI_HandleTypeDef* spi)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (spi->Instance == SPI1) {
        __HAL_RCC_SPI1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_7; // SCK, MOSI
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = GPIO_PIN_6; // MISO (unused by the MAX7219)
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}
