// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Blue Pill firmware written like an STM32CubeIDE project with the STM32CubeF1
// HAL (SN-205): three chips on I2C1 (PB6 SCL, PB7 SDA, 100 kHz, 4.7 kOhm
// pull-ups to 3.3 V), reported on USART1 (PA9, 115200 baud).
//   24C02 EEPROM at 0x50: writes "SimNodus" at word address 0x10, waits for
//     the write cycle by acknowledge polling (HAL_I2C_IsDeviceReady), reads it
//     back (HAL_I2C_Mem_Write / Mem_Read).
//   LM75 temperature sensor at 0x48: reads register 0 (two bytes) every
//     500 ms and prints the temperature with half degrees.
//   PCF8574 I/O expander at 0x20: P4-P7 are inputs with push buttons to GND,
//     P0-P3 drive LEDs from 3.3 V (low = on); each LED follows its button
//     (HAL_I2C_Master_Receive / Master_Transmit of one byte).
//   PC13 (on-board LED, lit when low): toggles every 500 ms.
#include "stm32f1xx_hal.h"

I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart1;

#define EEPROM (0x50u << 1)
#define LM75 (0x48u << 1)
#define PCF8574 (0x20u << 1)

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
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

static void sleep_ms(uint32_t ms)
{
    const uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < ms) {
        __WFI();
    }
}

static void eeprom_test(void)
{
    const uint8_t text[8] = {'S', 'i', 'm', 'N', 'o', 'd', 'u', 's'};
    if (HAL_I2C_Mem_Write(&hi2c1, EEPROM, 0x10, I2C_MEMADD_SIZE_8BIT, (uint8_t*)text, 8, 50) != HAL_OK) {
        print("EEPROM: no answer\r\n");
        return;
    }
    // The write cycle takes up to 5 ms; meanwhile the EEPROM does not acknowledge.
    // Each trial takes about 0.2 ms at 100 kHz, so 100 trials cover it.
    if (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM, 100, 50) != HAL_OK) {
        print("EEPROM: still busy\r\n");
        return;
    }
    uint8_t back[9] = {0};
    if (HAL_I2C_Mem_Read(&hi2c1, EEPROM, 0x10, I2C_MEMADD_SIZE_8BIT, back, 8, 50) != HAL_OK) {
        print("EEPROM: read failed\r\n");
        return;
    }
    char line[32];
    char* p = put_text(line, "EEPROM: ");
    p = put_text(p, (const char*)back);
    p = put_text(p, "\r\n");
    *p = '\0';
    print(line);
}

static void print_temperature(void)
{
    uint8_t raw[2];
    if (HAL_I2C_Mem_Read(&hi2c1, LM75, 0x00, I2C_MEMADD_SIZE_8BIT, raw, 2, 50) != HAL_OK) {
        print("LM75: no answer\r\n");
        return;
    }
    const int16_t halves = (int16_t)((int16_t)((raw[0] << 8) | raw[1]) >> 7); // half degrees
    char line[32];
    char* p = put_text(line, "Temperature: ");
    uint32_t magnitude = (uint32_t)(halves < 0 ? -halves : halves);
    if (halves < 0) {
        *p++ = '-';
    }
    p = put_number(p, magnitude / 2u);
    p = put_text(p, (magnitude & 1u) ? ".5 C\r\n" : ".0 C\r\n");
    *p = '\0';
    print(line);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_I2C1_Init();

    print("I2C devices example\r\n");
    eeprom_test();
    uint8_t last_buttons = 0xFF;
    uint32_t next_report = HAL_GetTick();
    while (1) {
        // PCF8574: read the pins, then write the LEDs (low = on) and keep P4-P7 high (inputs).
        uint8_t pins = 0xFF;
        if (HAL_I2C_Master_Receive(&hi2c1, PCF8574, &pins, 1, 50) == HAL_OK) {
            const uint8_t buttons = (uint8_t)((pins >> 4) & 0x0Fu); // 0 = pressed
            uint8_t out = (uint8_t)(0xF0u | buttons);
            HAL_I2C_Master_Transmit(&hi2c1, PCF8574, &out, 1, 50);
            if (buttons != last_buttons) {
                last_buttons = buttons;
                char line[24];
                char* p = put_text(line, "Buttons: ");
                for (int i = 0; i < 4; ++i) {
                    *p++ = ((buttons >> i) & 1u) ? '-' : (char)('1' + i);
                }
                p = put_text(p, "\r\n");
                *p = '\0';
                print(line);
            }
        }
        if ((int32_t)(HAL_GetTick() - next_report) >= 0) {
            next_report += 500u;
            print_temperature();
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }
        sleep_ms(10);
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
