// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F407 RTC, backup registers and watchdog (experiment E-12, SN-298),
// at 16 MHz from the HSI.
//   The RTC runs on the LSE (a 32.768 kHz crystal on PC14 and PC15; without
//   one, after about a second of waiting, on the LSI). On the first start it
//   sets the calendar to 2026-11-12 12:00:00, a Thursday; after a reset it
//   keeps counting. Backup register 1 counts the starts.
//   The wakeup timer interrupts every second (EXTI line 22): the firmware
//   prints the time on USART1 (PA9, 115200 baud) and toggles PC13.
//   Alarm A fires whenever the seconds are 05 (EXTI line 17): "alarm".
//   The independent watchdog (2 s) is reloaded every second, but not while
//   PA0 is high (a button to 3.3 V): then it resets the chip, and the next
//   start says so.
#define F4_RTC_WKUP RTC_WKUP_IRQHandler
#define F4_RTC_ALARM RTC_Alarm_IRQHandler
#include "../f4_common/f4_peripherals.h"

#define IWDG_KR REG(0x40003000u)
#define IWDG_PR REG(0x40003004u)
#define IWDG_RLR REG(0x40003008u)

static volatile uint32_t ticks, alarms;

void RTC_WKUP_IRQHandler(void)
{
    rtc_clear(1u << 10); // WUTF
    EXTI_PR = 1u << 22;
    GPIO_ODR(PORT_C) ^= 1u << 13;
    ++ticks;
}

void RTC_Alarm_IRQHandler(void)
{
    rtc_clear(1u << 8); // ALRAF
    EXTI_PR = 1u << 17;
    ++alarms;
}

// Polled output: put_text from f4.h.
static void put_bcd(uint32_t value)
{
    put_char((char)('0' + ((value >> 4) & 15u)));
    put_char((char)('0' + (value & 15u)));
}

static void put_time(void)
{
    const uint32_t tr = RTC_TR;
    const uint32_t dr = RTC_DR;
    put_bcd((tr >> 16) & 0x3Fu);
    put_char(':');
    put_bcd((tr >> 8) & 0x7Fu);
    put_char(':');
    put_bcd(tr & 0x7Fu);
    put_text(" 20");
    put_bcd((dr >> 16) & 0xFFu);
    put_char('-');
    put_bcd((dr >> 8) & 0x1Fu);
    put_char('-');
    put_bcd(dr & 0x3Fu);
    put_text("\r\n");
}

int main(void)
{
    RCC_AHB1ENR |= (1u << 0) | (1u << 2); // GPIOAEN, GPIOCEN
    usart1_start(139);
    GPIO_MODER(PORT_C) = (GPIO_MODER(PORT_C) & ~(3u << 26)) | (1u << 26); // PC13 output
    GPIO_PUPDR(PORT_A) = (GPIO_PUPDR(PORT_A) & ~3u) | 2u;                 // PA0 input, pull-down

    const uint32_t csr = RCC_CSR;
    RCC_CSR |= 1u << 24; // RMVF: the next start sees only its own cause
    put_text("f4_rtc\r\n");
    if (csr & (1u << 29)) { // IWDGRSTF
        put_text("reset by the watchdog\r\n");
    }
    const int set = rtc_open(0x120000u, 0x00269112u); // 12:00:00, Thursday 2026-11-12
    put_text(set ? "calendar set" : "calendar kept");
    put_text(RTC_BKP(2) == 1u ? " (LSE)\r\n" : " (LSI)\r\n");
    RTC_BKP(1) = RTC_BKP(1) + 1u;
    put_text("start ");
    put_decimal(RTC_BKP(1));
    put_text("\r\n");

    // The wakeup timer every second on the 1 Hz clock, alarm A at second 05.
    RTC_WPR = 0xCAu;
    RTC_WPR = 0x53u;
    RTC_CR &= ~((1u << 10) | (1u << 8)); // WUTE, ALRAE off: their registers can be written
    while ((RTC_ISR & 5u) != 5u) {      // WUTWF, ALRAWF
    }
    RTC_WUTR = 0;
    RTC_ALRMAR = (1u << 31) | (1u << 23) | (1u << 15) | 0x05u; // only the seconds count
    rtc_clear((1u << 10) | (1u << 8));
    RTC_CR = (RTC_CR & ~7u) | 4u | (1u << 10) | (1u << 14) | (1u << 8) | (1u << 12); // ck_spre, WUTE, WUTIE, ALRAE, ALRAIE
    RTC_WPR = 0xFFu;
    EXTI_IMR |= (1u << 17) | (1u << 22);
    EXTI_RTSR |= (1u << 17) | (1u << 22);
    NVIC_ISER0 = 1u << 3; // RTC_WKUP
    NVIC_ISER1 = 1u << 9; // RTC_Alarm (41)

    // The independent watchdog: LSI / 32, 2000 counts: 2 s.
    IWDG_KR = 0x5555u;
    IWDG_PR = 3u;
    IWDG_RLR = 1999u;
    IWDG_KR = 0xCCCCu;

    uint32_t printed = 0, rung = 0;
    for (;;) {
        __asm__ volatile("wfi");
        if (ticks != printed) {
            printed = ticks;
            put_time();
            if (!(GPIO_IDR(PORT_A) & 1u)) {
                IWDG_KR = 0xAAAAu; // reloaded while the button is not pressed
            }
        }
        if (alarms != rung) {
            rung = alarms;
            put_text("alarm\r\n");
        }
    }
}
