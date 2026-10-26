// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Blue Pill firmware written like an STM32CubeIDE project with the STM32CubeF1
// HAL (SN-224), reported on USART1 (PA9, 115200 baud):
//   SPI1 (PA5 SCK, PA6 MISO, PA7 MOSI, mode 0, 1.125 MHz) with three chips:
//     MCP3208 A/D converter, CS on PA4: CH0 reads a potentiometer, CH1 the
//       wiper of the digital potentiometer, VREF 3.3 V.
//     MCP41010 digital potentiometer, CS on PB0, between 3.3 V and GND: its
//       wiper follows the potentiometer (CH0 / 16), so CH1 tracks CH0.
//     W25Q32 flash, CS on PB1: JEDEC ID; "SimNodus" written to address 0
//       once (erase, program, busy polling) and read back.
//   HX711 with a 5 kg load cell (1 mV/V): PB10 drives SCK, PB11 reads DT; the
//     weight in grams.
// Every 500 ms one line: the two converter readings and the weight; PC13
// toggles.
#include "stm32f1xx_hal.h"

SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart1;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
void Error_Handler(void);

static void print(const char* text)
{
    uint16_t length = 0;
    while (text[length] != '\0') {
        ++length;
    }
    HAL_UART_Transmit(&huart1, (const uint8_t*)text, length, 100);
}

static char* put_text(char* out, const char* text)
{
    while (*text != '\0') {
        *out++ = *text++;
    }
    return out;
}

static char* put_number(char* out, int32_t value)
{
    if (value < 0) {
        *out++ = '-';
        value = -value;
    }
    char digits[10];
    int n = 0;
    do {
        digits[n++] = (char)('0' + (uint32_t)value % 10u);
        value /= 10;
    } while (value != 0);
    while (n > 0) {
        *out++ = digits[--n];
    }
    return out;
}

static char* put_hex(char* out, uint8_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    *out++ = hex[value >> 4];
    *out++ = hex[value & 0x0Fu];
    return out;
}

// One SPI transaction with CS low on `port`/`pin`.
static void spi(GPIO_TypeDef* port, uint16_t pin, uint8_t* bytes, uint16_t count)
{
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
    if (HAL_SPI_TransmitReceive(&hspi1, bytes, bytes, count, 10) != HAL_OK) {
        Error_Handler();
    }
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
}

// MCP3208: start bit, single-ended, channel; 12 bits back.
static uint16_t adc_read(uint8_t channel)
{
    uint8_t b[3] = {(uint8_t)(0x06u | (channel >> 2)), (uint8_t)((channel & 3u) << 6), 0};
    spi(GPIOA, GPIO_PIN_4, b, 3);
    return (uint16_t)(((b[1] & 0x0Fu) << 8) | b[2]);
}

// MCP41010: write pot 0.
static void dpot_write(uint8_t value)
{
    uint8_t b[2] = {0x11, value};
    spi(GPIOB, GPIO_PIN_0, b, 2);
}

static void flash_wait(void)
{
    uint8_t b[2];
    do {
        b[0] = 0x05;
        b[1] = 0;
        spi(GPIOB, GPIO_PIN_1, b, 2);
    } while (b[1] & 1u); // BUSY
}

// The buffer receives the answer, so each command gets a fresh one.
static void flash_write_enable(void)
{
    uint8_t wren = 0x06;
    spi(GPIOB, GPIO_PIN_1, &wren, 1);
}

static void flash_test(void)
{
    uint8_t id[4] = {0x9F, 0, 0, 0};
    spi(GPIOB, GPIO_PIN_1, id, 4);
    char line[48];
    char* p = put_text(line, "W25Q32 ID ");
    p = put_hex(p, id[1]);
    p = put_hex(p, id[2]);
    p = put_hex(p, id[3]);
    p = put_text(p, "\r\n");
    *p = '\0';
    print(line);

    uint8_t read[12] = {0x03, 0, 0, 0};
    spi(GPIOB, GPIO_PIN_1, read, 12);
    if (read[4] == 0xFF) { // erased: write the text once
        flash_write_enable();
        uint8_t erase[4] = {0x20, 0, 0, 0};
        spi(GPIOB, GPIO_PIN_1, erase, 4);
        flash_wait();
        flash_write_enable();
        uint8_t program[12] = {0x02, 0, 0, 0, 'S', 'i', 'm', 'N', 'o', 'd', 'u', 's'};
        spi(GPIOB, GPIO_PIN_1, program, 12);
        flash_wait();
        for (int i = 0; i < 12; ++i) {
            read[i] = 0;
        }
        read[0] = 0x03;
        spi(GPIOB, GPIO_PIN_1, read, 12);
    }
    p = put_text(line, "W25Q32 reads ");
    for (int i = 4; i < 12; ++i) {
        *p++ = (char)read[i];
    }
    p = put_text(p, "\r\n");
    *p = '\0';
    print(line);
}

static void short_delay(void)
{
    for (volatile int i = 0; i < 10; ++i) {
    }
}

// HX711: wait for DT low, 24 bits, one more pulse for channel A x128 next time.
static int hx711_read(int32_t* value)
{
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11) != GPIO_PIN_RESET) {
        return 0; // not ready
    }
    uint32_t raw = 0;
    for (int i = 0; i < 25; ++i) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
        short_delay();
        if (i < 24) {
            raw = (raw << 1) | (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11) == GPIO_PIN_SET ? 1u : 0u);
        }
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
        short_delay();
    }
    *value = (int32_t)(raw << 8) >> 8; // sign-extend 24 bits
    return 1;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_SPI1_Init();

    print("SPI sensors example\r\n");
    flash_test();
    int32_t grams = 0;
    uint32_t next = HAL_GetTick();
    while (1) {
        int32_t code;
        if (hx711_read(&code)) {
            // 1 mV/V at full load (5 kg) and gain 128: 0.256 x 2^23 counts for 5000 g.
            grams = (int32_t)(((int64_t)code * 5000) / 2147484);
        }
        if ((int32_t)(HAL_GetTick() - next) >= 0) {
            next += 500u;
            const uint16_t pot = adc_read(0);
            dpot_write((uint8_t)(pot >> 4));
            const uint16_t wiper = adc_read(1);
            char line[64];
            char* p = put_text(line, "CH0 ");
            p = put_number(p, pot);
            p = put_text(p, " CH1 ");
            p = put_number(p, wiper);
            p = put_text(p, " LOAD ");
            p = put_number(p, grams);
            p = put_text(p, " g\r\n");
            *p = '\0';
            print(line);
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }
        __WFI();
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
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
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

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // Chip selects high (idle), HX711 SCK low.
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
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
        GPIO_InitStruct.Pin = GPIO_PIN_6; // MISO
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (huart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitStruct.Pin = GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = GPIO_PIN_10;
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
