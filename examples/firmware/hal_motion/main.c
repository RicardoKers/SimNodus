// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Blue Pill firmware written like an STM32CubeIDE project with the STM32CubeF1
// HAL (SN-226): a rotary encoder sets one position (0-20 detents) that three
// motors follow, reported on USART1 (PA9, 115200 baud) twice a second:
//   Encoder: A on PB6, B on PB7 (internal pull-ups), C to GND, decoded from
//     the quadrature states every millisecond.
//   Servo: TIM2 CH1 on PA0, 50 Hz, 0.5-2.5 ms: 9 degrees per detent.
//   28BYJ-48 stepper through a ULN2003 on PB12-PB15: half steps, 2 ms each,
//     towards 205 half steps per detent (a tenth of a turn).
//   DC motor through an L298: IN1/IN2 on PB0/PB1 choose the direction (forward
//     above detent 10, backward below), TIM2 CH2 on PA1 (ENA) the speed, 10 %
//     per detent away from 10.
#include "stm32f1xx_hal.h"

TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart1;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
void HAL_TIM_MspPostInit(TIM_HandleTypeDef* htim);
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

// Quadrature decoding: the change of the two-bit state gives +1, -1 or 0;
// a detent is four changes (one full cycle).
static int32_t position = 10; // detents
static int32_t quarter = 0;
static uint8_t last_state = 3;

static void encoder_poll(void)
{
    static const int8_t table[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    const uint8_t state = (uint8_t)((HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) == GPIO_PIN_SET ? 2u : 0u)
        | (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_SET ? 1u : 0u));
    quarter += table[(last_state << 2) | state];
    last_state = state;
    if (state == 3) { // at rest: whole detents
        if (quarter >= 4 && position < 20) {
            ++position;
        } else if (quarter <= -4 && position > 0) {
            --position;
        }
        quarter = 0;
    }
}

// Half-step sequence of the 28BYJ-48 coils A, B, C, D (PB12-PB15).
static const uint8_t half_steps[8] = {0x1, 0x3, 0x2, 0x6, 0x4, 0xC, 0x8, 0x9};
static int32_t stepper_at = 0;

static void stepper_step(int direction)
{
    stepper_at += direction;
    const uint8_t coils = half_steps[(uint32_t)stepper_at & 7u];
    GPIOB->BSRR = ((uint32_t)coils << 12) | ((uint32_t)(~coils & 0xFu) << (12 + 16));
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_TIM2_Init();
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

    print("Motion example\r\n");
    uint32_t last_ms = HAL_GetTick();
    uint32_t next_report = last_ms;
    while (1) {
        const uint32_t now = HAL_GetTick();
        if (now == last_ms) {
            __WFI();
            continue;
        }
        last_ms = now;
        encoder_poll();
        // Servo: 500 us + 100 us per detent (9 degrees).
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 500u + (uint32_t)position * 100u);
        // DC motor: direction and speed from the distance to detent 10.
        const int32_t offset = position - 10;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, offset > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, offset < 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, (uint32_t)(offset < 0 ? -offset : offset) * 2000u);
        // Stepper: one half step every 2 ms towards its target.
        const int32_t target = (position - 10) * 205;
        if ((now & 1u) == 0 && stepper_at != target) {
            stepper_step(stepper_at < target ? 1 : -1);
        }
        if ((int32_t)(now - next_report) >= 0) {
            next_report += 500u;
            char line[64];
            char* p = put_text(line, "POS ");
            p = put_number(p, position);
            p = put_text(p, " SERVO ");
            p = put_number(p, position * 9);
            p = put_text(p, " STEP ");
            p = put_number(p, stepper_at);
            p = put_text(p, " MOTOR ");
            p = put_number(p, offset * 10);
            p = put_text(p, " %\r\n");
            *p = '\0';
            print(line);
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }
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

// TIM2: 1 MHz counter, 20 ms period (50 Hz); CH1 the servo pulse, CH2 the motor's PWM.
static void MX_TIM2_Init(void)
{
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 71;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 19999;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
        Error_Handler();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) {
        Error_Handler();
    }
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 1500;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }
    sConfigOC.Pulse = 0;
    if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) {
        Error_Handler();
    }
    HAL_TIM_MspPostInit(&htim2);
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

    // L298 IN1/IN2 and the ULN2003 inputs: outputs, low.
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15,
        GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    // Encoder A and B: inputs with pull-ups (its contacts close to GND).
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

// What STM32CubeIDE generates in stm32f1xx_hal_msp.c and stm32f1xx_it.c.
void HAL_MspInit(void)
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* htim)
{
    if (htim->Instance == TIM2) {
        __HAL_RCC_TIM2_CLK_ENABLE();
    }
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef* htim)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (htim->Instance == TIM2) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1; // CH1 servo, CH2 L298 ENA
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
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
