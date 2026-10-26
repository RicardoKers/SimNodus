// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Blue Pill firmware written like an STM32CubeIDE project with the STM32CubeF1
// HAL (SN-223): three chips on I2C1 (PB6 SCL, PB7 SDA, 400 kHz, 4.7 kOhm
// pull-ups to 3.3 V).
//   SSD1306 128x64 OLED at 0x3C: initialized like the common libraries
//     (segment remap and COM scan reversed, horizontal addressing), a frame
//     buffer of 1024 bytes sent after each update.
//   BMP280 at 0x76: calibration read once, normal mode, temperature and
//     pressure compensated with the datasheet's integer formulas.
//   DS3231 at 0x68: time and date read in BCD.
// Every 500 ms the OLED shows the time (twice the size), the date, the
// temperature and the pressure; PC13 (on-board LED) toggles.
#include "stm32f1xx_hal.h"

I2C_HandleTypeDef hi2c1;

#define OLED (0x3Cu << 1)
#define BMP280 (0x76u << 1)
#define DS3231 (0x68u << 1)

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
void Error_Handler(void);

// 5x7 glyphs, one byte per column, bit 0 at the top.
static const char font_chars[] = " 0123456789:.-ACDEHILMNOPRSTU";
static const uint8_t font[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // ' '
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // '0'
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // '1'
    {0x42, 0x61, 0x51, 0x49, 0x46}, // '2'
    {0x41, 0x49, 0x49, 0x49, 0x36}, // '3'
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // '4'
    {0x27, 0x45, 0x45, 0x45, 0x39}, // '5'
    {0x3E, 0x49, 0x49, 0x49, 0x30}, // '6'
    {0x01, 0x71, 0x09, 0x05, 0x03}, // '7'
    {0x36, 0x49, 0x49, 0x49, 0x36}, // '8'
    {0x06, 0x49, 0x49, 0x49, 0x3E}, // '9'
    {0x00, 0x00, 0x36, 0x00, 0x00}, // ':'
    {0x00, 0x60, 0x60, 0x00, 0x00}, // '.'
    {0x08, 0x08, 0x08, 0x08, 0x08}, // '-'
    {0x7E, 0x09, 0x09, 0x09, 0x7E}, // 'A'
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 'C'
    {0x7F, 0x41, 0x41, 0x41, 0x3E}, // 'D'
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 'E'
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 'H'
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // 'I'
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // 'L'
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 'M'
    {0x7F, 0x02, 0x04, 0x08, 0x7F}, // 'N'
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 'O'
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // 'P'
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // 'R'
    {0x46, 0x49, 0x49, 0x49, 0x31}, // 'S'
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // 'T'
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 'U'
};

// Control byte 0x40 (display data) followed by the frame buffer: page 0 first.
static uint8_t frame[1 + 1024];
static uint8_t* const pixels = frame + 1;

static void oled_commands(const uint8_t* commands, uint16_t count)
{
    uint8_t buffer[32];
    buffer[0] = 0x00; // control byte: commands follow
    for (uint16_t i = 0; i < count; ++i) {
        buffer[1 + i] = commands[i];
    }
    HAL_I2C_Master_Transmit(&hi2c1, OLED, buffer, (uint16_t)(count + 1u), 50);
}

static void oled_init(void)
{
    static const uint8_t init[] = {
        0xAE,       // display off
        0xD5, 0x80, // clock
        0xA8, 0x3F, // 64 rows
        0xD3, 0x00, // no offset
        0x40,       // start line 0
        0x8D, 0x14, // charge pump on
        0x20, 0x00, // horizontal addressing
        0xA1,       // segment remap: column 127 on SEG0
        0xC8,       // COM scan from COM63
        0xDA, 0x12, // COM pins
        0x81, 0xCF, // contrast
        0xD9, 0xF1, // precharge
        0xDB, 0x40, // VCOMH
        0xA4,       // follow RAM
        0xA6,       // normal (not inverse)
        0xAF,       // display on
    };
    oled_commands(init, sizeof init);
}

static void oled_show(void)
{
    static const uint8_t window[] = {0x21, 0x00, 0x7F, 0x22, 0x00, 0x07}; // columns 0-127, pages 0-7
    oled_commands(window, sizeof window);
    frame[0] = 0x40;
    HAL_I2C_Master_Transmit(&hi2c1, OLED, frame, sizeof frame, 200);
}

static void set_pixel(int x, int y)
{
    if (x >= 0 && x < 128 && y >= 0 && y < 64) {
        pixels[(y / 8) * 128 + x] |= (uint8_t)(1u << (y % 8));
    }
}

// Text at (x, y), each glyph pixel `scale` x `scale` pixels, 6 x scale per character.
static void draw_text(int x, int y, const char* text, int scale)
{
    for (; *text != '\0'; ++text, x += 6 * scale) {
        int index = 0;
        while (font_chars[index] != '\0' && font_chars[index] != *text) {
            ++index;
        }
        if (font_chars[index] == '\0') {
            continue; // not in the font: a gap
        }
        for (int column = 0; column < 5; ++column) {
            for (int row = 0; row < 7; ++row) {
                if ((font[index][column] >> row) & 1u) {
                    for (int dx = 0; dx < scale; ++dx) {
                        for (int dy = 0; dy < scale; ++dy) {
                            set_pixel(x + column * scale + dx, y + row * scale + dy);
                        }
                    }
                }
            }
        }
    }
}

static char* put_text(char* out, const char* text)
{
    while (*text != '\0') {
        *out++ = *text++;
    }
    return out;
}

static char* put_number(char* out, uint32_t value, int digits)
{
    char reversed[10];
    int n = 0;
    do {
        reversed[n++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u || n < digits);
    while (n > 0) {
        *out++ = reversed[--n];
    }
    return out;
}

static char* put_bcd(char* out, uint8_t value)
{
    *out++ = (char)('0' + (value >> 4));
    *out++ = (char)('0' + (value & 0x0Fu));
    return out;
}

// BMP280 calibration (dig_T1..dig_T3, dig_P1..dig_P9) and t_fine, as in the datasheet.
static uint16_t dig_t1, dig_p1;
static int16_t dig_t2, dig_t3, dig_p[10]; // dig_p[2]..dig_p[9] (dig_P1 is unsigned, apart)
static int32_t t_fine;

static int bmp280_init(void)
{
    uint8_t c[24];
    if (HAL_I2C_Mem_Read(&hi2c1, BMP280, 0x88, I2C_MEMADD_SIZE_8BIT, c, sizeof c, 50) != HAL_OK) {
        return 0;
    }
    dig_t1 = (uint16_t)(c[0] | (c[1] << 8));
    dig_t2 = (int16_t)(c[2] | (c[3] << 8));
    dig_t3 = (int16_t)(c[4] | (c[5] << 8));
    dig_p1 = (uint16_t)(c[6] | (c[7] << 8));
    for (int i = 0; i < 8; ++i) {
        dig_p[i + 2] = (int16_t)(c[8 + 2 * i] | (c[9 + 2 * i] << 8));
    }
    uint8_t mode[2] = {0xF4, 0x27}; // temperature and pressure x1, normal mode
    return HAL_I2C_Master_Transmit(&hi2c1, BMP280, mode, 2, 50) == HAL_OK;
}

// Temperature in 0.01 degrees C.
static int32_t bmp280_temperature(int32_t adc)
{
    const int32_t var1 = ((((adc >> 3) - ((int32_t)dig_t1 << 1))) * ((int32_t)dig_t2)) >> 11;
    const int32_t var2 = (((((adc >> 4) - ((int32_t)dig_t1)) * ((adc >> 4) - ((int32_t)dig_t1))) >> 12)
                             * ((int32_t)dig_t3))
        >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8;
}

// Pressure in Pa x 256.
static uint32_t bmp280_pressure(int32_t adc)
{
    int64_t var1 = ((int64_t)t_fine) - 128000;
    int64_t var2 = var1 * var1 * (int64_t)dig_p[6];
    var2 = var2 + ((var1 * (int64_t)dig_p[5]) << 17);
    var2 = var2 + (((int64_t)dig_p[4]) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_p[3]) >> 8) + ((var1 * (int64_t)dig_p[2]) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_p1) >> 33;
    if (var1 == 0) {
        return 0;
    }
    int64_t p = 1048576 - adc;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_p[9]) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_p[8]) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_p[7]) << 4);
    return (uint32_t)p;
}

static void update(int bmp_ok)
{
    for (int i = 0; i < 1024; ++i) {
        pixels[i] = 0;
    }
    draw_text(25, 0, "SIMNODUS OLED", 1);
    char line[24];
    uint8_t t[7];
    if (HAL_I2C_Mem_Read(&hi2c1, DS3231, 0x00, I2C_MEMADD_SIZE_8BIT, t, sizeof t, 50) == HAL_OK) {
        char* p = put_bcd(line, t[2] & 0x3Fu);
        *p++ = ':';
        p = put_bcd(p, t[1]);
        *p++ = ':';
        p = put_bcd(p, t[0] & 0x7Fu);
        *p = '\0';
        draw_text(16, 12, line, 2);
        p = put_text(line, "DATE 20");
        p = put_bcd(p, t[6]);
        *p++ = '-';
        p = put_bcd(p, t[5] & 0x1Fu);
        *p++ = '-';
        p = put_bcd(p, t[4]);
        *p = '\0';
        draw_text(0, 32, line, 1);
    } else {
        draw_text(0, 16, "NO DS3231", 1);
    }
    uint8_t raw[6];
    if (bmp_ok && HAL_I2C_Mem_Read(&hi2c1, BMP280, 0xF7, I2C_MEMADD_SIZE_8BIT, raw, sizeof raw, 50) == HAL_OK) {
        const int32_t adc_p = (int32_t)((raw[0] << 12) | (raw[1] << 4) | (raw[2] >> 4));
        const int32_t adc_t = (int32_t)((raw[3] << 12) | (raw[4] << 4) | (raw[5] >> 4));
        const int32_t centi = bmp280_temperature(adc_t);
        const uint32_t pa = (bmp280_pressure(adc_p) / 256u + 5u) / 10u; // tenths of hPa, rounded
        char* p = put_text(line, "TEMP ");
        uint32_t magnitude = (uint32_t)(centi < 0 ? -centi : centi);
        if (centi < 0) {
            *p++ = '-';
        }
        p = put_number(p, magnitude / 100u, 1);
        *p++ = '.';
        p = put_number(p, (magnitude % 100u) / 10u, 1);
        p = put_text(p, " C");
        *p = '\0';
        draw_text(0, 42, line, 1);
        p = put_text(line, "PRES ");
        p = put_number(p, pa / 10u, 1);
        *p++ = '.';
        p = put_number(p, pa % 10u, 1);
        p = put_text(p, " HPA");
        *p = '\0';
        draw_text(0, 52, line, 1);
    } else {
        draw_text(0, 46, "NO BMP280", 1);
    }
    oled_show();
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();

    oled_init();
    const int bmp_ok = bmp280_init();
    uint32_t next = HAL_GetTick();
    while (1) {
        if ((int32_t)(HAL_GetTick() - next) >= 0) {
            next += 500u;
            update(bmp_ok);
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

static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000;
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
