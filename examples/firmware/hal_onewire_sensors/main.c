// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Blue Pill firmware written like an STM32CubeIDE project with the STM32CubeF1
// HAL (SN-225): timed one-wire protocols made with the DWT cycle counter
// (72 cycles per microsecond), reported on USART1 (PA9, 115200 baud):
//   DS18B20 on PB5 (open drain, 4.7 kOhm pull-up): skip ROM, convert, wait,
//     read the scratchpad, check its CRC.
//   DHT22 on PB4 (open drain, pull-up): start signal, 40 bits by the length of
//     their high pulses, checksum.
//   HC-SR04: TRIG on PB3 (10 us pulse), ECHO on PB6: 58 us per centimetre.
//   WS2812B strip of 8 LEDs on PA8: a bar of the distance, 25 cm per LED,
//     green to red.
// One line every second; PC13 toggles.
#include "stm32f1xx_hal.h"

UART_HandleTypeDef huart1;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
void Error_Handler(void);

#define CYCLES_PER_US 72u

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

// Tenths as "12.3".
static char* put_tenths(char* out, int32_t tenths)
{
    if (tenths < 0) {
        *out++ = '-';
        tenths = -tenths;
    }
    out = put_number(out, tenths / 10);
    *out++ = '.';
    *out++ = (char)('0' + tenths % 10);
    return out;
}

static void cycles_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
    const uint32_t start = DWT->CYCCNT;
    while (DWT->CYCCNT - start < us * CYCLES_PER_US) {
    }
}

// Microseconds until `pin` reads `level`, or -1 after `timeout_us`.
static int32_t wait_level(GPIO_TypeDef* port, uint16_t pin, GPIO_PinState level, uint32_t timeout_us)
{
    const uint32_t start = DWT->CYCCNT;
    while (HAL_GPIO_ReadPin(port, pin) != level) {
        if (DWT->CYCCNT - start > timeout_us * CYCLES_PER_US) {
            return -1;
        }
    }
    return (int32_t)((DWT->CYCCNT - start) / CYCLES_PER_US);
}

// ---- DS18B20 on PB5 ----------------------------------------------------------

#define DQ_LOW() (GPIOB->BRR = GPIO_PIN_5)
#define DQ_RELEASE() (GPIOB->BSRR = GPIO_PIN_5)
#define DQ_READ() ((GPIOB->IDR & GPIO_PIN_5) != 0u)

static int ow_reset(void)
{
    DQ_LOW();
    delay_us(500);
    DQ_RELEASE();
    delay_us(70);
    const int presence = !DQ_READ();
    delay_us(430);
    return presence;
}

static void ow_write(uint8_t byte)
{
    for (int k = 0; k < 8; ++k) {
        __disable_irq();
        DQ_LOW();
        if (byte & (1u << k)) {
            delay_us(5);
            DQ_RELEASE();
            delay_us(60);
        } else {
            delay_us(60);
            DQ_RELEASE();
            delay_us(5);
        }
        __enable_irq();
    }
}

static uint8_t ow_read(void)
{
    uint8_t value = 0;
    for (int k = 0; k < 8; ++k) {
        __disable_irq();
        DQ_LOW();
        delay_us(2);
        DQ_RELEASE();
        delay_us(10);
        if (DQ_READ()) {
            value |= (uint8_t)(1u << k);
        }
        __enable_irq();
        delay_us(55);
    }
    return value;
}

static uint8_t crc8(const uint8_t* bytes, int count)
{
    uint8_t crc = 0;
    for (int i = 0; i < count; ++i) {
        uint8_t b = bytes[i];
        for (int k = 0; k < 8; ++k) {
            const uint8_t mix = (uint8_t)((crc ^ b) & 1u);
            crc >>= 1;
            if (mix) {
                crc ^= 0x8C;
            }
            b >>= 1;
        }
    }
    return crc;
}

// Tenths of a degree, or INT32_MIN without a sensor.
static int32_t ds18b20_read(void)
{
    if (!ow_reset()) {
        return INT32_MIN;
    }
    ow_write(0xCC); // skip ROM
    ow_write(0x44); // convert
    uint32_t waited = 0;
    while (ow_read() == 0 && waited < 1000) { // 0 while converting
        HAL_Delay(10);
        waited += 10;
    }
    if (!ow_reset()) {
        return INT32_MIN;
    }
    ow_write(0xCC);
    ow_write(0xBE); // read scratchpad
    uint8_t s[9];
    for (int i = 0; i < 9; ++i) {
        s[i] = ow_read();
    }
    if (crc8(s, 9) != 0) {
        return INT32_MIN;
    }
    const int16_t sixteenths = (int16_t)(s[0] | (s[1] << 8));
    return (int32_t)sixteenths * 10 / 16;
}

// ---- DHT22 on PB4 ---------------------------------------------------------------

static int dht22_read(int32_t* tenths_c, int32_t* tenths_rh)
{
    GPIOB->BRR = GPIO_PIN_4;
    delay_us(1200);
    __disable_irq();
    GPIOB->BSRR = GPIO_PIN_4;
    // Response: low 80 us, high 80 us.
    int ok = wait_level(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET, 100) >= 0 && wait_level(GPIOB, GPIO_PIN_4, GPIO_PIN_SET, 100) >= 0
        && wait_level(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET, 100) >= 0;
    uint8_t b[5] = {0};
    for (int i = 0; ok && i < 40; ++i) {
        if (wait_level(GPIOB, GPIO_PIN_4, GPIO_PIN_SET, 80) < 0) {
            ok = 0;
            break;
        }
        const int32_t high = wait_level(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET, 100);
        if (high < 0) {
            ok = 0;
            break;
        }
        b[i / 8] = (uint8_t)((b[i / 8] << 1) | (high > 40 ? 1u : 0u));
    }
    __enable_irq();
    if (!ok || (uint8_t)(b[0] + b[1] + b[2] + b[3]) != b[4]) {
        return 0;
    }
    *tenths_rh = (b[0] << 8) | b[1];
    const int32_t t = ((b[2] & 0x7F) << 8) | b[3];
    *tenths_c = (b[2] & 0x80) ? -t : t;
    return 1;
}

// ---- HC-SR04: TRIG PB3, ECHO PB6 -----------------------------------------------------

// Centimetres, or -1 without an echo.
static int32_t hcsr04_read(void)
{
    GPIOB->BSRR = GPIO_PIN_3;
    delay_us(12);
    GPIOB->BRR = GPIO_PIN_3;
    if (wait_level(GPIOB, GPIO_PIN_6, GPIO_PIN_SET, 2000) < 0) {
        return -1;
    }
    const int32_t us = wait_level(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET, 40000);
    return us < 0 || us >= 30000 ? -1 : us / 58;
}

// ---- WS2812B strip on PA8 ---------------------------------------------------------------

#define LEDS 8

// 24 bits per LED, green, red, blue, MSB first: 0.4 us high for a 0, 0.8 us for a 1, 1.25 us each.
static void ws2812_show(const uint32_t* rgb)
{
    __disable_irq();
    for (int i = 0; i < LEDS; ++i) {
        const uint32_t grb = ((rgb[i] >> 8) & 0xFFu) << 16 | ((rgb[i] >> 16) & 0xFFu) << 8 | (rgb[i] & 0xFFu);
        for (int k = 23; k >= 0; --k) {
            const uint32_t start = DWT->CYCCNT;
            GPIOA->BSRR = GPIO_PIN_8;
            const uint32_t high = (grb >> k) & 1u ? 58u : 29u; // 0.8 or 0.4 us
            while (DWT->CYCCNT - start < high) {
            }
            GPIOA->BRR = GPIO_PIN_8;
            while (DWT->CYCCNT - start < 90u) { // 1.25 us
            }
        }
    }
    __enable_irq();
    delay_us(80); // reset: the colors show
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    cycles_init();

    print("One-wire sensors example\r\n");
    uint32_t next = HAL_GetTick();
    while (1) {
        if ((int32_t)(HAL_GetTick() - next) < 0) {
            __WFI();
            continue;
        }
        next += 1000u;
        char line[96];
        char* p = put_text(line, "DS18B20 ");
        const int32_t t1 = ds18b20_read();
        if (t1 == INT32_MIN) {
            p = put_text(p, "none");
        } else {
            p = put_tenths(p, t1);
            p = put_text(p, " C");
        }
        int32_t t2 = 0, rh = 0;
        p = put_text(p, "  DHT22 ");
        if (dht22_read(&t2, &rh)) {
            p = put_tenths(p, t2);
            p = put_text(p, " C ");
            p = put_tenths(p, rh);
            p = put_text(p, " %");
        } else {
            p = put_text(p, "none");
        }
        const int32_t cm = hcsr04_read();
        p = put_text(p, "  HC-SR04 ");
        if (cm < 0) {
            p = put_text(p, "no echo");
        } else {
            p = put_number(p, cm);
            p = put_text(p, " cm");
        }
        p = put_text(p, "\r\n");
        *p = '\0';
        print(line);
        // Distance bar: one LED per 25 cm, green near, red far.
        uint32_t rgb[LEDS];
        const int lit = cm < 0 ? 0 : (cm + 24) / 25;
        for (int i = 0; i < LEDS; ++i) {
            const uint32_t red = (uint32_t)(i * 255 / (LEDS - 1));
            rgb[i] = i < lit ? (red << 16) | ((255u - red) << 8) : 0;
        }
        ws2812_show(rgb);
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
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

    // DS18B20 (PB5) and DHT22 (PB4): open drain, released.
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    // HC-SR04 TRIG (PB3) and the strip (PA8): push-pull, low.
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    // HC-SR04 ECHO (PB6): input.
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

// What STM32CubeIDE generates in stm32f1xx_hal_msp.c and stm32f1xx_it.c.
void HAL_MspInit(void)
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG(); // frees PB3 and PB4
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
