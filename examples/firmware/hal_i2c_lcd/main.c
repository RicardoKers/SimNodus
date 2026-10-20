// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Blue Pill firmware written like an STM32CubeIDE project with the STM32CubeF1
// HAL (SN-206): a 16x2 character LCD (HD44780) behind a PCF8574 at I2C
// address 0x27, wired like the common I2C LCD backpacks:
//   P0 -> RS, P1 -> RW, P2 -> E, P3 -> backlight, P4-P7 -> D4-D7 (4-bit mode).
// I2C1 on PB6 (SCL) and PB7 (SDA) at 100 kHz with 4.7 kOhm pull-ups.
// Each nibble takes two PCF8574 writes: with E high, then with E low (the LCD
// takes the nibble at the falling edge of E).
// The first row shows a greeting, the second counts seconds; PC13 blinks.
#include "stm32f1xx_hal.h"

I2C_HandleTypeDef hi2c1;

#define LCD (0x27u << 1)
#define LCD_RS 0x01u
#define LCD_E 0x04u
#define LCD_BACKLIGHT 0x08u

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
void Error_Handler(void);

static void sleep_ms(uint32_t ms)
{
    const uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start <= ms) {
        __WFI();
    }
}

// One nibble (in the high four bits of `value`): E high, then E low.
static void lcd_nibble(uint8_t value, uint8_t rs)
{
    uint8_t bytes[2];
    bytes[0] = (uint8_t)((value & 0xF0u) | rs | LCD_BACKLIGHT | LCD_E);
    bytes[1] = (uint8_t)((value & 0xF0u) | rs | LCD_BACKLIGHT);
    HAL_I2C_Master_Transmit(&hi2c1, LCD, bytes, 2, 10);
}

static void lcd_byte(uint8_t value, uint8_t rs)
{
    lcd_nibble(value, rs);
    lcd_nibble((uint8_t)(value << 4), rs);
}

static void lcd_command(uint8_t command)
{
    lcd_byte(command, 0);
    if (command <= 0x03u) {
        sleep_ms(2); // clear and home take 1.52 ms
    }
    // Other instructions take 37 us: the next I2C transfer takes longer than that.
}

static void lcd_text(const char* text)
{
    while (*text != '\0') {
        lcd_byte((uint8_t)*text++, LCD_RS);
    }
}

static void lcd_init(void)
{
    sleep_ms(50); // after power-on
    // Three times "8-bit mode", then "4-bit mode": works from any state.
    lcd_nibble(0x30u, 0);
    sleep_ms(5);
    lcd_nibble(0x30u, 0);
    sleep_ms(1);
    lcd_nibble(0x30u, 0);
    sleep_ms(1);
    lcd_nibble(0x20u, 0);
    lcd_command(0x28); // 4-bit, two lines, 5x8 dots
    lcd_command(0x0C); // display on, no cursor
    lcd_command(0x01); // clear
    lcd_command(0x06); // move right after each character
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();

    lcd_init();
    lcd_command(0x80); // row 1, column 1
    lcd_text("Hello, SimNodus!");
    uint32_t seconds = 0;
    while (1) {
        char line[17] = "Seconds:        ";
        uint32_t value = seconds;
        int column = 15;
        do {
            line[column--] = (char)('0' + value % 10u);
            value /= 10u;
        } while (value != 0u && column > 8);
        lcd_command(0xC0); // row 2, column 1
        lcd_text(line);
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        sleep_ms(1000);
        ++seconds;
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

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

// What STM32CubeIDE generates in stm32f1xx_hal_msp.c and stm32f1xx_it.c.
void HAL_MspInit(void)
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
}

void HAL_I2C_MspInit(I2C_HandleTypeDef* i2c)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (i2c->Instance == I2C1) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7; // SCL, SDA
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
        __HAL_RCC_I2C1_CLK_ENABLE();
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
