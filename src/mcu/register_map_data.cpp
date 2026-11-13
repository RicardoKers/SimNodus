// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The register maps of the register view (SN-290) as text, with the names of
// RM0008 (STM32F103), RM0090 (STM32F407), PM0056 (Cortex-M3) and the ATmega8, ATmega32 and
// ATmega328P datasheets. Only registers the emulators have are listed, and
// write-only registers are left out (there is nothing to show).
//
// One statement per line; `| text` ends a line with a description:
//   chip NAME
//   type NAME [BASE]            registers shared by several peripherals (BASE: starts as a copy)
//   per NAME ADDRESS            a peripheral; its registers follow, or
//   use TYPE                      the registers of a type
//   reg NAME OFFSET BYTES       a register (1, 2 or 4 bytes)
//   f NAME LSB WIDTH            a field of the last register
//   fr PREFIX FIRST COUNT WIDTH LSB STEP   fields PREFIX<FIRST>.. every STEP bits (# in the text: the number)
//   bits B7 ... B0              a name for every bit, highest first ("-": none)
//   like REG                    the fields of an earlier register of the block
//
// The text is in pieces because a compiler limits one string literal.
#include "mcu/register_map.hpp"

namespace simnodus::mcu {

// Declared for register_map.cpp (a const array is local to its file otherwise).
extern const char* const stm32f103_registers[];
extern const char* const stm32f407_registers[];
extern const char* const atmega8_registers[];
extern const char* const atmega32_registers[];
extern const char* const atmega328p_registers[];

// Shared by the STM32F103 and the STM32F407: the Cortex-M core's registers, its
// system registers, and the USART.
const char cortex_core_registers[] =
R"(per CORE 0xFFFFF000 | Cortex-M core registers
reg R0 0x00 4 | General-purpose register (first argument and return value of a function)
reg R1 0x04 4 | General-purpose register (second argument)
reg R2 0x08 4 | General-purpose register (third argument)
reg R3 0x0C 4 | General-purpose register (fourth argument)
reg R4 0x10 4 | General-purpose register
reg R5 0x14 4 | General-purpose register
reg R6 0x18 4 | General-purpose register
reg R7 0x1C 4 | General-purpose register
reg R8 0x20 4 | General-purpose register
reg R9 0x24 4 | General-purpose register
reg R10 0x28 4 | General-purpose register
reg R11 0x2C 4 | General-purpose register
reg R12 0x30 4 | General-purpose register
reg SP 0x34 4 | Stack pointer
reg LR 0x38 4 | Link register: where the function returns to
reg PC 0x3C 4 | Program counter: the next instruction
reg xPSR 0x40 4 | Program status
f N 31 1 | Negative
f Z 30 1 | Zero
f C 29 1 | Carry
f V 28 1 | Overflow
f Q 27 1 | Saturation
f T 24 1 | Thumb state
f EXCEPTION 0 9 | Number of the exception running (0: thread mode)

)";

const char cortex_system_registers[] =
R"(per SysTick 0xE000E010 | Cortex-M system timer
reg CTRL 0x00 4 | Control and status
f COUNTFLAG 16 1 | Counted to 0 since the last read by the firmware
f CLKSOURCE 2 1 | Clock: 0 HCLK/8, 1 HCLK
f TICKINT 1 1 | Interrupt when it counts to 0
f ENABLE 0 1 | Counter enable
reg LOAD 0x04 4 | Reload value: the period is LOAD + 1
reg VAL 0x08 4 | Current value
reg CALIB 0x0C 4 | Calibration

per NVIC 0xE000E100 | Cortex-M interrupt controller
reg ISER0 0x000 4 | Interrupts 0 to 31 enabled
reg ISER1 0x004 4 | Interrupts 32 to 63 enabled
reg ISPR0 0x100 4 | Interrupts 0 to 31 pending
reg ISPR1 0x104 4 | Interrupts 32 to 63 pending
reg IABR0 0x200 4 | Interrupts 0 to 31 active
reg IABR1 0x204 4 | Interrupts 32 to 63 active

per SCB 0xE000ED00 | Cortex-M system control block
reg CPUID 0x00 4 | Processor identification
reg ICSR 0x04 4 | Interrupt control and state
f NMIPENDSET 31 1 | NMI pending
f PENDSVSET 28 1 | PendSV pending
f PENDSTSET 26 1 | SysTick pending
f ISRPENDING 22 1 | An interrupt is pending
f VECTPENDING 12 10 | Number of the highest pending exception
f RETTOBASE 11 1 | No other exception is active
f VECTACTIVE 0 9 | Number of the exception running (0: thread mode)
reg VTOR 0x08 4 | Vector table address
reg AIRCR 0x0C 4 | Application interrupt and reset control
f PRIGROUP 8 3 | Priority grouping
reg SCR 0x10 4 | System control
f SLEEPDEEP 2 1 | Deep sleep
f SLEEPONEXIT 1 1 | Sleep when returning to thread mode
reg CCR 0x14 4 | Configuration and control
reg SHCSR 0x24 4 | System handler control and state
reg CFSR 0x28 4 | Configurable fault status
f UFSR 16 16 | Usage fault: 1 undefined instruction, 2 invalid state, 4 invalid PC, 8 no coprocessor, 0x100 unaligned, 0x200 division by zero
f BFSR 8 8 | Bus fault: 1 instruction, 2 precise data, 4 imprecise data, 8 unstacking, 0x10 stacking, 0x80 BFAR valid
f MMFSR 0 8 | Memory management fault
reg HFSR 0x2C 4 | Hard fault status
f FORCED 30 1 | A fault that could not run its handler became a hard fault
f VECTTBL 1 1 | Bus fault reading the vector table
reg MMFAR 0x34 4 | Memory management fault address
reg BFAR 0x38 4 | Bus fault address
)";

const char stm32_usart_registers[] =
R"(type USART
reg SR 0x00 2 | Status
f CTS 9 1 | CTS changed
f LBD 8 1 | LIN break detected
f TXE 7 1 | Transmit data register empty
f TC 6 1 | Transmission complete
f RXNE 5 1 | Received data ready to be read
f IDLE 4 1 | Idle line detected
f ORE 3 1 | Overrun error
f NE 2 1 | Noise error
f FE 1 1 | Framing error
f PE 0 1 | Parity error
reg DR 0x04 2 | Data: the byte received (shown without taking it)
reg BRR 0x08 2 | Baud rate: PCLK divided by this number (mantissa and sixteenths)
f DIV_Mantissa 4 12
f DIV_Fraction 0 4
reg CR1 0x0C 2 | Control 1
f UE 13 1 | USART enable
f M 12 1 | Word length: 0 eight bits, 1 nine bits
f WAKE 11 1 | Wakeup method
f PCE 10 1 | Parity control enable
f PS 9 1 | Parity: 0 even, 1 odd
f PEIE 8 1 | Parity error interrupt enable
f TXEIE 7 1 | TXE interrupt enable
f TCIE 6 1 | Transmission complete interrupt enable
f RXNEIE 5 1 | RXNE interrupt enable
f IDLEIE 4 1 | IDLE interrupt enable
f TE 3 1 | Transmitter enable
f RE 2 1 | Receiver enable
f RWU 1 1 | Receiver in mute mode
f SBK 0 1 | Send break
reg CR2 0x10 2 | Control 2
f LINEN 14 1 | LIN mode enable
f STOP 12 2 | Stop bits: 0 one, 1 half, 2 two, 3 one and a half
f CLKEN 11 1 | Clock pin enable
f CPOL 10 1 | Clock polarity
f CPHA 9 1 | Clock phase
f LBCL 8 1 | Last bit clock pulse
f LBDIE 6 1 | LIN break interrupt enable
f LBDL 5 1 | LIN break detection length
f ADD 0 4 | Address of the node
reg CR3 0x14 2 | Control 3
f CTSIE 10 1 | CTS interrupt enable
f CTSE 9 1 | CTS enable
f RTSE 8 1 | RTS enable
f DMAT 7 1 | DMA for transmission
f DMAR 6 1 | DMA for reception
f SCEN 5 1 | Smartcard mode
f NACK 4 1 | Smartcard NACK
f HDSEL 3 1 | Half-duplex (single wire)
f IRLP 2 1 | IrDA low power
f IREN 1 1 | IrDA mode
f EIE 0 1 | Error interrupt enable
reg GTPR 0x18 2 | Guard time and prescaler

)";

const char* const stm32f103_registers[] = {
"chip stm32f103\n\n",
cortex_core_registers,
R"(
type GPIO
reg CRL 0x00 4 | Configuration of pins 0 to 7
fr MODE 0 8 2 0 4 | Pin #: 0 input, 1 output 10 MHz, 2 output 2 MHz, 3 output 50 MHz
fr CNF 0 8 2 2 4 | Pin #: input 0 analog, 1 floating, 2 pull-up/down; output 0 push-pull, 1 open drain, 2 alternate push-pull, 3 alternate open drain
reg CRH 0x04 4 | Configuration of pins 8 to 15
fr MODE 8 8 2 0 4 | Pin #: 0 input, 1 output 10 MHz, 2 output 2 MHz, 3 output 50 MHz
fr CNF 8 8 2 2 4 | Pin #: input 0 analog, 1 floating, 2 pull-up/down; output 0 push-pull, 1 open drain, 2 alternate push-pull, 3 alternate open drain
reg IDR 0x08 4 | Input data: the levels on the pins
fr IDR 0 16 1 0 1 | Level on pin #
reg ODR 0x0C 4 | Output data (for an input with pull: 1 pull-up, 0 pull-down)
fr ODR 0 16 1 0 1 | Output of pin #
reg LCKR 0x18 4 | Configuration lock
f LCKK 16 1 | Lock key
fr LCK 0 16 1 0 1 | Configuration of pin # locked

per GPIOA 0x40010800 | General-purpose I/O port A
use GPIO
per GPIOB 0x40010C00 | General-purpose I/O port B
use GPIO
per GPIOC 0x40011000 | General-purpose I/O port C
use GPIO
per GPIOD 0x40011400 | General-purpose I/O port D
use GPIO

per AFIO 0x40010000 | Alternate function I/O: remaps and the pins of the external interrupts
reg EVCR 0x00 4 | Event output
f EVOE 7 1 | Event output enable
f PORT 4 3 | Port of the event output
f PIN 0 4 | Pin of the event output
reg MAPR 0x04 4 | Remap of alternate functions and debug pins
f SWJ_CFG 24 3 | Debug pins: 0 JTAG and SWD, 1 without NJTRST, 2 SWD only, 4 none
f PD01_REMAP 15 1 | PD0 and PD1 on OSC_IN and OSC_OUT
f CAN_REMAP 13 2 | CAN pins
f TIM4_REMAP 12 1 | TIM4 on PD12 to PD15
f TIM3_REMAP 10 2 | TIM3: 0 PA6 PA7 PB0 PB1, 2 PB4 PB5 PB0 PB1, 3 PC6 to PC9
f TIM2_REMAP 8 2 | TIM2: 0 PA0 to PA3, 1 PA15 PB3 PA2 PA3, 2 PA0 PA1 PB10 PB11, 3 PA15 PB3 PB10 PB11
f TIM1_REMAP 6 2 | TIM1: 0 none, 1 partial, 3 full
f USART3_REMAP 4 2 | USART3: 0 PB10 PB11, 1 PC10 PC11, 3 PD8 PD9
f USART2_REMAP 3 1 | USART2 on PD5 PD6
f USART1_REMAP 2 1 | USART1 on PB6 PB7
f I2C1_REMAP 1 1 | I2C1 on PB8 PB9
f SPI1_REMAP 0 1 | SPI1 on PA15 PB3 PB4 PB5
reg EXTICR1 0x08 4 | Port of the external interrupt lines 0 to 3
fr EXTI 0 4 4 0 4 | Line #: 0 PA, 1 PB, 2 PC, 3 PD
reg EXTICR2 0x0C 4 | Port of the external interrupt lines 4 to 7
fr EXTI 4 4 4 0 4 | Line #: 0 PA, 1 PB, 2 PC, 3 PD
reg EXTICR3 0x10 4 | Port of the external interrupt lines 8 to 11
fr EXTI 8 4 4 0 4 | Line #: 0 PA, 1 PB, 2 PC, 3 PD
reg EXTICR4 0x14 4 | Port of the external interrupt lines 12 to 15
fr EXTI 12 4 4 0 4 | Line #: 0 PA, 1 PB, 2 PC, 3 PD

per EXTI 0x40010400 | External interrupts and events
reg IMR 0x00 4 | Interrupt mask
fr MR 0 19 1 0 1 | Interrupt of line # enabled
reg EMR 0x04 4 | Event mask
fr MR 0 19 1 0 1 | Event of line # enabled
reg RTSR 0x08 4 | Rising edge trigger
fr TR 0 19 1 0 1 | Rising edge of line #
reg FTSR 0x0C 4 | Falling edge trigger
fr TR 0 19 1 0 1 | Falling edge of line #
reg SWIER 0x10 4 | Software interrupt
fr SWIER 0 19 1 0 1 | Software interrupt on line #
reg PR 0x14 4 | Pending (written with 1 to clear)
fr PR 0 19 1 0 1 | Line # pending
)",
R"(
per RCC 0x40021000 | Reset and clock control
reg CR 0x00 4 | Clock control
f PLLRDY 25 1 | PLL locked
f PLLON 24 1 | PLL enable
f CSSON 19 1 | Clock security system enable
f HSEBYP 18 1 | External clock instead of a crystal
f HSERDY 17 1 | HSE (crystal) oscillator ready
f HSEON 16 1 | HSE (crystal) oscillator enable
f HSICAL 8 8 | HSI calibration
f HSITRIM 3 5 | HSI trimming
f HSIRDY 1 1 | HSI (internal 8 MHz) oscillator ready
f HSION 0 1 | HSI (internal 8 MHz) oscillator enable
reg CFGR 0x04 4 | Clock configuration
f MCO 24 3 | Clock output on PA8: 4 SYSCLK, 5 HSI, 6 HSE, 7 PLL/2
f USBPRE 22 1 | USB prescaler: 0 PLL/1.5, 1 PLL
f PLLMUL 18 4 | PLL multiplication: the value plus 2 (0 is x2, 7 is x9)
f PLLXTPRE 17 1 | HSE divided by 2 for the PLL
f PLLSRC 16 1 | PLL source: 0 HSI/2, 1 HSE
f ADCPRE 14 2 | ADC prescaler: PCLK2 divided by 2, 4, 6, 8
f PPRE2 11 3 | APB2 prescaler: 0 to 3 none, 4 /2, 5 /4, 6 /8, 7 /16
f PPRE1 8 3 | APB1 prescaler: 0 to 3 none, 4 /2, 5 /4, 6 /8, 7 /16
f HPRE 4 4 | AHB prescaler: 0 to 7 none, 8 /2 ... 15 /512
f SWS 2 2 | System clock in use: 0 HSI, 1 HSE, 2 PLL
f SW 0 2 | System clock chosen: 0 HSI, 1 HSE, 2 PLL
reg CIR 0x08 4 | Clock interrupts
f PLLRDYIE 12 1 | PLL ready interrupt enable
f HSERDYIE 11 1 | HSE ready interrupt enable
f HSIRDYIE 10 1 | HSI ready interrupt enable
f LSERDYIE 9 1 | LSE ready interrupt enable
f LSIRDYIE 8 1 | LSI ready interrupt enable
f CSSF 7 1 | Clock security system flag
f PLLRDYF 4 1 | PLL ready flag
f HSERDYF 3 1 | HSE ready flag
f HSIRDYF 2 1 | HSI ready flag
f LSERDYF 1 1 | LSE ready flag
f LSIRDYF 0 1 | LSI ready flag
reg APB2RSTR 0x0C 4 | APB2 peripheral reset
f USART1RST 14 1
f SPI1RST 12 1
f TIM1RST 11 1
f ADC2RST 10 1
f ADC1RST 9 1
f IOPDRST 5 1
f IOPCRST 4 1
f IOPBRST 3 1
f IOPARST 2 1
f AFIORST 0 1
reg APB1RSTR 0x10 4 | APB1 peripheral reset
f PWRRST 28 1
f BKPRST 27 1
f I2C2RST 22 1
f I2C1RST 21 1
f USART3RST 18 1
f USART2RST 17 1
f SPI2RST 14 1
f WWDGRST 11 1
f TIM4RST 2 1
f TIM3RST 1 1
f TIM2RST 0 1
reg AHBENR 0x14 4 | AHB peripheral clock enable
f CRCEN 6 1 | CRC clock
f FLITFEN 4 1 | Flash interface clock
f SRAMEN 2 1 | SRAM clock
f DMA1EN 0 1 | DMA1 clock
reg APB2ENR 0x18 4 | APB2 peripheral clock enable
f USART1EN 14 1 | USART1 clock
f SPI1EN 12 1 | SPI1 clock
f TIM1EN 11 1 | TIM1 clock
f ADC2EN 10 1 | ADC2 clock
f ADC1EN 9 1 | ADC1 clock
f IOPDEN 5 1 | Port D clock
f IOPCEN 4 1 | Port C clock
f IOPBEN 3 1 | Port B clock
f IOPAEN 2 1 | Port A clock
f AFIOEN 0 1 | Alternate function clock
reg APB1ENR 0x1C 4 | APB1 peripheral clock enable
f PWREN 28 1 | Power interface clock
f BKPEN 27 1 | Backup interface clock
f CANEN 25 1 | CAN clock
f USBEN 23 1 | USB clock
f I2C2EN 22 1 | I2C2 clock
f I2C1EN 21 1 | I2C1 clock
f USART3EN 18 1 | USART3 clock
f USART2EN 17 1 | USART2 clock
f SPI2EN 14 1 | SPI2 clock
f WWDGEN 11 1 | Window watchdog clock
f TIM4EN 2 1 | TIM4 clock
f TIM3EN 1 1 | TIM3 clock
f TIM2EN 0 1 | TIM2 clock
reg BDCR 0x20 4 | Backup domain control
f BDRST 16 1 | Backup domain reset
f RTCEN 15 1 | RTC clock enable
f RTCSEL 8 2 | RTC clock: 0 none, 1 LSE, 2 LSI, 3 HSE/128
f LSEBYP 2 1 | External clock instead of the 32768 Hz crystal
f LSERDY 1 1 | LSE (32768 Hz) oscillator ready
f LSEON 0 1 | LSE (32768 Hz) oscillator enable
reg CSR 0x24 4 | Control and status: what caused the last reset
f LPWRRSTF 31 1 | Low-power reset
f WWDGRSTF 30 1 | Window watchdog reset
f IWDGRSTF 29 1 | Independent watchdog reset
f SFTRSTF 28 1 | Software reset
f PORRSTF 27 1 | Power-on reset
f PINRSTF 26 1 | Reset by the NRST pin
f RMVF 24 1 | Remove the reset flags
f LSIRDY 1 1 | LSI (internal 40 kHz) oscillator ready
f LSION 0 1 | LSI (internal 40 kHz) oscillator enable

per FLASH 0x40022000 | Flash memory interface
reg ACR 0x00 4 | Access control
f PRFTBS 5 1 | Prefetch buffer on
f PRFTBE 4 1 | Prefetch buffer enable
f HLFCYA 3 1 | Half cycle access
f LATENCY 0 3 | Wait states: 0 up to 24 MHz, 1 up to 48 MHz, 2 up to 72 MHz
reg SR 0x0C 4 | Status
f EOP 5 1 | End of operation
f WRPRTERR 4 1 | Write protection error
f PGERR 2 1 | Programming error
f BSY 0 1 | Busy
reg CR 0x10 4 | Control
f EOPIE 12 1 | End of operation interrupt enable
f ERRIE 10 1 | Error interrupt enable
f OPTWRE 9 1 | Option bytes write enable
f LOCK 7 1 | Locked
f STRT 6 1 | Start an erase
f OPTER 5 1 | Option byte erase
f OPTPG 4 1 | Option byte programming
f MER 2 1 | Mass erase
f PER 1 1 | Page erase
f PG 0 1 | Programming

per PWR 0x40007000 | Power control
reg CR 0x00 4 | Control
f DBP 8 1 | Write access to the backup domain (RTC, BKP)
f PLS 5 3 | Voltage detector level
f PVDE 4 1 | Voltage detector enable
f CSBF 3 1 | Clear the standby flag
f CWUF 2 1 | Clear the wakeup flag
f PDDS 1 1 | Deep sleep: 0 Stop, 1 Standby
f LPDS 0 1 | Regulator in low power in Stop
reg CSR 0x04 4 | Control and status
f EWUP 8 1 | WKUP pin enable
f PVDO 2 1 | Supply below the detector level
f SBF 1 1 | Was in Standby
f WUF 0 1 | Wakeup event
)",
R"(
type TIM
reg CR1 0x00 2 | Control 1
f CKD 8 2 | Clock division of the filters
f ARPE 7 1 | ARR buffered until the next update
f CMS 5 2 | Center-aligned mode
f DIR 4 1 | Direction: 0 up, 1 down
f OPM 3 1 | One-pulse mode
f URS 2 1 | Update request source
f UDIS 1 1 | Update disable
f CEN 0 1 | Counter enable
reg CR2 0x04 2 | Control 2
f TI1S 7 1 | TI1 from CH1, CH2 and CH3 (XOR)
f MMS 4 3 | Master mode: what the trigger output is
f CCDS 3 1 | DMA request on update instead of on compare
reg SMCR 0x08 2 | Slave mode control
f ETP 15 1 | External trigger polarity
f ECE 14 1 | External clock enable
f ETPS 12 2 | External trigger prescaler
f ETF 8 4 | External trigger filter
f MSM 7 1 | Master/slave mode
f TS 4 3 | Trigger selection
f SMS 0 3 | Slave mode: 0 off, 1 to 3 encoder, 4 reset, 5 gated, 6 trigger, 7 external clock
reg DIER 0x0C 2 | DMA and interrupt enable
f TDE 14 1 | Trigger DMA request enable
f CC4DE 12 1 | Capture/compare 4 DMA request enable
f CC3DE 11 1 | Capture/compare 3 DMA request enable
f CC2DE 10 1 | Capture/compare 2 DMA request enable
f CC1DE 9 1 | Capture/compare 1 DMA request enable
f UDE 8 1 | Update DMA request enable
f TIE 6 1 | Trigger interrupt enable
f CC4IE 4 1 | Capture/compare 4 interrupt enable
f CC3IE 3 1 | Capture/compare 3 interrupt enable
f CC2IE 2 1 | Capture/compare 2 interrupt enable
f CC1IE 1 1 | Capture/compare 1 interrupt enable
f UIE 0 1 | Update interrupt enable
reg SR 0x10 2 | Status
f CC4OF 12 1 | Capture 4 overcapture
f CC3OF 11 1 | Capture 3 overcapture
f CC2OF 10 1 | Capture 2 overcapture
f CC1OF 9 1 | Capture 1 overcapture
f TIF 6 1 | Trigger interrupt flag
f CC4IF 4 1 | Capture/compare 4 flag
f CC3IF 3 1 | Capture/compare 3 flag
f CC2IF 2 1 | Capture/compare 2 flag
f CC1IF 1 1 | Capture/compare 1 flag
f UIF 0 1 | Update flag
reg CCMR1 0x18 2 | Capture/compare mode of channels 1 and 2 (fields named as for outputs)
f OC2CE 15 1 | Output 2 clear enable
f OC2M 12 3 | Output 2 mode: 0 frozen, 1 active on match, 2 inactive on match, 3 toggle, 4 low, 5 high, 6 PWM 1, 7 PWM 2
f OC2PE 11 1 | CCR2 buffered until the next update
f OC2FE 10 1 | Output 2 fast enable
f CC2S 8 2 | Channel 2: 0 output, 1 input on TI2, 2 input on TI1, 3 input on TRC
f OC1CE 7 1 | Output 1 clear enable
f OC1M 4 3 | Output 1 mode: 0 frozen, 1 active on match, 2 inactive on match, 3 toggle, 4 low, 5 high, 6 PWM 1, 7 PWM 2
f OC1PE 3 1 | CCR1 buffered until the next update
f OC1FE 2 1 | Output 1 fast enable
f CC1S 0 2 | Channel 1: 0 output, 1 input on TI1, 2 input on TI2, 3 input on TRC
reg CCMR2 0x1C 2 | Capture/compare mode of channels 3 and 4 (fields named as for outputs)
f OC4CE 15 1 | Output 4 clear enable
f OC4M 12 3 | Output 4 mode: 0 frozen, 1 active on match, 2 inactive on match, 3 toggle, 4 low, 5 high, 6 PWM 1, 7 PWM 2
f OC4PE 11 1 | CCR4 buffered until the next update
f OC4FE 10 1 | Output 4 fast enable
f CC4S 8 2 | Channel 4: 0 output, 1 input on TI4, 2 input on TI3, 3 input on TRC
f OC3CE 7 1 | Output 3 clear enable
f OC3M 4 3 | Output 3 mode: 0 frozen, 1 active on match, 2 inactive on match, 3 toggle, 4 low, 5 high, 6 PWM 1, 7 PWM 2
f OC3PE 3 1 | CCR3 buffered until the next update
f OC3FE 2 1 | Output 3 fast enable
f CC3S 0 2 | Channel 3: 0 output, 1 input on TI3, 2 input on TI4, 3 input on TRC
reg CCER 0x20 2 | Capture/compare enable
f CC4P 13 1 | Channel 4 polarity (output: active low; input: falling edge)
f CC4E 12 1 | Channel 4 enable
f CC3P 9 1 | Channel 3 polarity
f CC3E 8 1 | Channel 3 enable
f CC2P 5 1 | Channel 2 polarity
f CC2E 4 1 | Channel 2 enable
f CC1P 1 1 | Channel 1 polarity
f CC1E 0 1 | Channel 1 enable
reg CNT 0x24 2 | Counter
reg PSC 0x28 2 | Prescaler: the counter's clock is the timer's clock divided by PSC + 1
reg ARR 0x2C 2 | Auto-reload: the counter's period is ARR + 1
reg CCR1 0x34 2 | Capture/compare 1
reg CCR2 0x38 2 | Capture/compare 2
reg CCR3 0x3C 2 | Capture/compare 3
reg CCR4 0x40 2 | Capture/compare 4

type TIM_ADVANCED TIM
reg RCR 0x30 2 | Repetition counter
reg BDTR 0x44 2 | Break and dead time
f MOE 15 1 | Main output enable
f AOE 14 1 | Automatic output enable
f BKP 13 1 | Break polarity
f BKE 12 1 | Break enable
f OSSR 11 1 | Off-state selection for Run mode
f OSSI 10 1 | Off-state selection for Idle mode
f LOCK 8 2 | Lock configuration
f DTG 0 8 | Dead time generator

per TIM1 0x40012C00 | Advanced-control timer (complementary outputs and break in BDTR; CCER also has CCxNE and CCxNP)
use TIM_ADVANCED
per TIM2 0x40000000 | General-purpose timer
use TIM
per TIM3 0x40000400 | General-purpose timer
use TIM
per TIM4 0x40000800 | General-purpose timer
use TIM
)",
stm32_usart_registers,
R"(
per USART1 0x40013800 | Serial port 1 (PA9 TX, PA10 RX)
use USART
per USART2 0x40004400 | Serial port 2 (PA2 TX, PA3 RX)
use USART
per USART3 0x40004800 | Serial port 3 (PB10 TX, PB11 RX)
use USART

type SPI
reg CR1 0x00 2 | Control 1
f BIDIMODE 15 1 | One data line for both directions
f BIDIOE 14 1 | Output enable on the one data line
f CRCEN 13 1 | CRC enable
f CRCNEXT 12 1 | The CRC goes next
f DFF 11 1 | Frame: 0 eight bits, 1 sixteen bits
f RXONLY 10 1 | Receive only
f SSM 9 1 | Slave select by software
f SSI 8 1 | Internal slave select
f LSBFIRST 7 1 | Lowest bit first
f SPE 6 1 | SPI enable
f BR 3 3 | Baud rate: PCLK divided by 2, 4, 8 ... 256
f MSTR 2 1 | Master
f CPOL 1 1 | Clock polarity: its level while idle
f CPHA 0 1 | Clock phase: 0 sample on the first edge, 1 on the second
reg CR2 0x04 2 | Control 2
f TXEIE 7 1 | TXE interrupt enable
f RXNEIE 6 1 | RXNE interrupt enable
f ERRIE 5 1 | Error interrupt enable
f SSOE 2 1 | NSS output enable
f TXDMAEN 1 1 | DMA for transmission
f RXDMAEN 0 1 | DMA for reception
reg SR 0x08 2 | Status
f BSY 7 1 | Busy
f OVR 6 1 | Overrun
f MODF 5 1 | Mode fault
f CRCERR 4 1 | CRC error
f TXE 1 1 | Transmit buffer empty
f RXNE 0 1 | Received data ready to be read
reg DR 0x0C 2 | Data: the last word received (shown without taking it)

per SPI1 0x40013000 | SPI 1 (PA5 SCK, PA6 MISO, PA7 MOSI)
use SPI
per SPI2 0x40003800 | SPI 2 (PB13 SCK, PB14 MISO, PB15 MOSI)
use SPI

type I2C
reg CR1 0x00 2 | Control 1
f SWRST 15 1 | Software reset
f ALERT 13 1 | SMBus alert
f PEC 12 1 | Packet error checking
f POS 11 1 | ACK and PEC position
f ACK 10 1 | Acknowledge the bytes received
f STOP 9 1 | Stop generation
f START 8 1 | Start generation
f NOSTRETCH 7 1 | No clock stretching (slave)
f ENGC 6 1 | General call enable
f ENPEC 5 1 | PEC enable
f ENARP 4 1 | ARP enable
f SMBTYPE 3 1 | SMBus type
f SMBUS 1 1 | SMBus mode
f PE 0 1 | Peripheral enable
reg CR2 0x04 2 | Control 2
f LAST 12 1 | DMA last transfer
f DMAEN 11 1 | DMA requests enable
f ITBUFEN 10 1 | Buffer interrupt enable
f ITEVTEN 9 1 | Event interrupt enable
f ITERREN 8 1 | Error interrupt enable
f FREQ 0 6 | PCLK1 in MHz
reg OAR1 0x08 2 | Own address 1
f ADDMODE 15 1 | 10-bit address
f ADD 0 10 | Address (7 bits in bits 7 to 1)
reg OAR2 0x0C 2 | Own address 2
reg DR 0x10 2 | Data: the byte received (shown without taking it)
reg SR1 0x14 2 | Status 1
f SMBALERT 15 1 | SMBus alert
f TIMEOUT 14 1 | Timeout
f PECERR 12 1 | PEC error
f OVR 11 1 | Overrun or underrun
f AF 10 1 | Acknowledge failure: nobody answered
f ARLO 9 1 | Arbitration lost
f BERR 8 1 | Bus error
f TXE 7 1 | Data register empty (transmitting)
f RXNE 6 1 | Data register not empty (receiving)
f STOPF 4 1 | Stop detected (slave)
f ADD10 3 1 | 10-bit header sent
f BTF 2 1 | Byte transfer finished
f ADDR 1 1 | Address sent (master) or matched (slave)
f SB 0 1 | Start condition generated
reg SR2 0x18 2 | Status 2
f PEC 8 8 | Packet error checking register
f DUALF 7 1 | Dual flag
f SMBHOST 6 1 | SMBus host header
f SMBDEFAULT 5 1 | SMBus device default address
f GENCALL 4 1 | General call address
f TRA 2 1 | Transmitter
f BUSY 1 1 | Bus busy
f MSL 0 1 | Master
reg CCR 0x1C 2 | Clock control
f F_S 15 1 | Fast mode (400 kHz)
f DUTY 14 1 | Fast mode duty cycle
f CCR 0 12 | Clock control: half an SCL period in PCLK1 periods (standard mode)
reg TRISE 0x20 2 | Maximum rise time

per I2C1 0x40005400 | I2C 1 (PB6 SCL, PB7 SDA)
use I2C
per I2C2 0x40005800 | I2C 2 (PB10 SCL, PB11 SDA)
use I2C
)",
R"(
type ADC
reg SR 0x00 4 | Status
f STRT 4 1 | Regular conversion started
f JSTRT 3 1 | Injected conversion started
f JEOC 2 1 | Injected conversions ended
f EOC 1 1 | Conversion ended
f AWD 0 1 | Analog watchdog
reg CR1 0x04 4 | Control 1
f AWDEN 23 1 | Analog watchdog on regular channels
f JAWDEN 22 1 | Analog watchdog on injected channels
f DUALMOD 16 4 | Dual mode
f DISCNUM 13 3 | Channels of a discontinuous group
f JDISCEN 12 1 | Discontinuous mode on injected channels
f DISCEN 11 1 | Discontinuous mode on regular channels
f JAUTO 10 1 | Automatic injected group conversion
f AWDSGL 9 1 | Analog watchdog on a single channel
f SCAN 8 1 | Scan mode: convert the whole sequence
f JEOCIE 7 1 | JEOC interrupt enable
f AWDIE 6 1 | Analog watchdog interrupt enable
f EOCIE 5 1 | EOC interrupt enable
f AWDCH 0 5 | Analog watchdog channel
reg CR2 0x08 4 | Control 2
f TSVREFE 23 1 | Temperature sensor and VREFINT enable
f SWSTART 22 1 | Start the regular conversion
f JSWSTART 21 1 | Start the injected conversion
f EXTTRIG 20 1 | External trigger for regular channels
f EXTSEL 17 3 | Regular trigger: 0 T1 CC1, 1 T1 CC2, 2 T1 CC3, 3 T2 CC2, 4 T3 TRGO, 5 T4 CC4, 6 EXTI 11, 7 SWSTART
f JEXTTRIG 15 1 | External trigger for injected channels
f JEXTSEL 12 3 | Injected trigger
f ALIGN 11 1 | Alignment: 0 right, 1 left
f DMA 8 1 | DMA mode
f RSTCAL 3 1 | Reset calibration
f CAL 2 1 | Calibration
f CONT 1 1 | Continuous conversion
f ADON 0 1 | Converter on
reg SMPR1 0x0C 4 | Sample time of channels 10 to 17
fr SMP 10 8 3 0 3 | Channel #: 0 1.5, 1 7.5, 2 13.5, 3 28.5, 4 41.5, 5 55.5, 6 71.5, 7 239.5 cycles
reg SMPR2 0x10 4 | Sample time of channels 0 to 9
fr SMP 0 10 3 0 3 | Channel #: 0 1.5, 1 7.5, 2 13.5, 3 28.5, 4 41.5, 5 55.5, 6 71.5, 7 239.5 cycles
reg JOFR1 0x14 4 | Offset of injected channel 1
reg JOFR2 0x18 4 | Offset of injected channel 2
reg JOFR3 0x1C 4 | Offset of injected channel 3
reg JOFR4 0x20 4 | Offset of injected channel 4
reg HTR 0x24 4 | Analog watchdog high threshold
reg LTR 0x28 4 | Analog watchdog low threshold
reg SQR1 0x2C 4 | Regular sequence 1
f L 20 4 | Length of the sequence minus 1
fr SQ 13 4 5 0 5 | Channel of conversion #
reg SQR2 0x30 4 | Regular sequence 2
fr SQ 7 6 5 0 5 | Channel of conversion #
reg SQR3 0x34 4 | Regular sequence 3
fr SQ 1 6 5 0 5 | Channel of conversion #
reg JSQR 0x38 4 | Injected sequence
f JL 20 2 | Length of the sequence minus 1
fr JSQ 1 4 5 0 5 | Channel of injected conversion #
reg JDR1 0x3C 4 | Injected data 1
reg JDR2 0x40 4 | Injected data 2
reg JDR3 0x44 4 | Injected data 3
reg JDR4 0x48 4 | Injected data 4
reg DR 0x4C 4 | Regular data: the last conversion (shown without clearing EOC)
f ADC2DATA 16 16 | ADC2's data in dual mode
f DATA 0 16 | Result (0 to 4095 right aligned)

per ADC1 0x40012400 | Analog-to-digital converter 1
use ADC
per ADC2 0x40012800 | Analog-to-digital converter 2
use ADC

per DMA1 0x40020000 | DMA controller
reg ISR 0x00 4 | Interrupt status
fr TEIF 1 7 1 3 4 | Channel # transfer error
fr HTIF 1 7 1 2 4 | Channel # half transfer
fr TCIF 1 7 1 1 4 | Channel # transfer complete
fr GIF 1 7 1 0 4 | Channel # global flag
reg CCR1 0x08 4 | Channel 1 configuration
f MEM2MEM 14 1 | Memory to memory
f PL 12 2 | Priority
f MSIZE 10 2 | Memory size: 0 byte, 1 half word, 2 word
f PSIZE 8 2 | Peripheral size: 0 byte, 1 half word, 2 word
f MINC 7 1 | Memory address increments
f PINC 6 1 | Peripheral address increments
f CIRC 5 1 | Circular mode
f DIR 4 1 | Direction: 0 from the peripheral, 1 from memory
f TEIE 3 1 | Transfer error interrupt enable
f HTIE 2 1 | Half transfer interrupt enable
f TCIE 1 1 | Transfer complete interrupt enable
f EN 0 1 | Channel enable
reg CNDTR1 0x0C 4 | Channel 1: data left to transfer
reg CPAR1 0x10 4 | Channel 1: peripheral address
reg CMAR1 0x14 4 | Channel 1: memory address
reg CCR2 0x1C 4 | Channel 2 configuration
like CCR1
reg CNDTR2 0x20 4 | Channel 2: data left to transfer
reg CPAR2 0x24 4 | Channel 2: peripheral address
reg CMAR2 0x28 4 | Channel 2: memory address
reg CCR3 0x30 4 | Channel 3 configuration
like CCR1
reg CNDTR3 0x34 4 | Channel 3: data left to transfer
reg CPAR3 0x38 4 | Channel 3: peripheral address
reg CMAR3 0x3C 4 | Channel 3: memory address
reg CCR4 0x44 4 | Channel 4 configuration
like CCR1
reg CNDTR4 0x48 4 | Channel 4: data left to transfer
reg CPAR4 0x4C 4 | Channel 4: peripheral address
reg CMAR4 0x50 4 | Channel 4: memory address
reg CCR5 0x58 4 | Channel 5 configuration
like CCR1
reg CNDTR5 0x5C 4 | Channel 5: data left to transfer
reg CPAR5 0x60 4 | Channel 5: peripheral address
reg CMAR5 0x64 4 | Channel 5: memory address
reg CCR6 0x6C 4 | Channel 6 configuration
like CCR1
reg CNDTR6 0x70 4 | Channel 6: data left to transfer
reg CPAR6 0x74 4 | Channel 6: peripheral address
reg CMAR6 0x78 4 | Channel 6: memory address
reg CCR7 0x80 4 | Channel 7 configuration
like CCR1
reg CNDTR7 0x84 4 | Channel 7: data left to transfer
reg CPAR7 0x88 4 | Channel 7: peripheral address
reg CMAR7 0x8C 4 | Channel 7: memory address
)",
R"(
per RTC 0x40002800 | Real-time clock
reg CRH 0x00 2 | Control high
f OWIE 2 1 | Overflow interrupt enable
f ALRIE 1 1 | Alarm interrupt enable
f SECIE 0 1 | Second interrupt enable
reg CRL 0x04 2 | Control low
f RTOFF 5 1 | Last write finished
f CNF 4 1 | Configuration mode
f RSF 3 1 | Registers synchronized
f OWF 2 1 | Overflow flag
f ALRF 1 1 | Alarm flag
f SECF 0 1 | Second flag
reg DIVH 0x10 2 | Prescaler divider, high part
reg DIVL 0x14 2 | Prescaler divider, low part
reg CNTH 0x18 2 | Counter (seconds), high part
reg CNTL 0x1C 2 | Counter (seconds), low part

per BKP 0x40006C00 | Backup registers (kept through a reset)
reg DR1 0x04 2 | Backup data 1
reg DR2 0x08 2 | Backup data 2
reg DR3 0x0C 2 | Backup data 3
reg DR4 0x10 2 | Backup data 4
reg DR5 0x14 2 | Backup data 5
reg DR6 0x18 2 | Backup data 6
reg DR7 0x1C 2 | Backup data 7
reg DR8 0x20 2 | Backup data 8
reg DR9 0x24 2 | Backup data 9
reg DR10 0x28 2 | Backup data 10
reg RTCCR 0x2C 2 | RTC clock calibration
reg CR 0x30 2 | Control
reg CSR 0x34 2 | Control and status

per IWDG 0x40003000 | Independent watchdog
reg PR 0x04 4 | Prescaler
f PR 0 3 | LSI divided by 4, 8, 16, 32, 64, 128, 256
reg RLR 0x08 4 | Reload value
f RL 0 12

per WWDG 0x40002C00 | Window watchdog
reg CR 0x00 4 | Control
f WDGA 7 1 | Watchdog active
f T 0 7 | Counter: a reset when it goes from 0x40 to 0x3F
reg CFR 0x04 4 | Configuration
f EWI 9 1 | Early wakeup interrupt enable
f WDGTB 7 2 | Timer base: PCLK1/4096 divided by 1, 2, 4, 8
f W 0 7 | Window: a refresh above it resets the chip
reg SR 0x08 4 | Status
f EWIF 0 1 | Early wakeup interrupt flag

per CRC 0x40023000 | CRC calculation unit
reg DR 0x00 4 | Data: the CRC so far
reg IDR 0x04 4 | Independent data (one free byte)

)",
cortex_system_registers,
nullptr};

// The STM32F407 (RM0090): what the machine of experiment E-12 has.
const char* const stm32f407_registers[] = {
"chip stm32f407\n\n",
cortex_core_registers,
R"(
per FPU 0xFFFFF054 | The FPU's single-precision registers (D0 to D15 are pairs of them)
reg S0 0x00 4 | Single-precision register (first floating-point argument and result)
reg S1 0x04 4 | Single-precision register
reg S2 0x08 4 | Single-precision register
reg S3 0x0C 4 | Single-precision register
reg S4 0x10 4 | Single-precision register
reg S5 0x14 4 | Single-precision register
reg S6 0x18 4 | Single-precision register
reg S7 0x1C 4 | Single-precision register
reg S8 0x20 4 | Single-precision register
reg S9 0x24 4 | Single-precision register
reg S10 0x28 4 | Single-precision register
reg S11 0x2C 4 | Single-precision register
reg S12 0x30 4 | Single-precision register
reg S13 0x34 4 | Single-precision register
reg S14 0x38 4 | Single-precision register
reg S15 0x3C 4 | Single-precision register (S0 to S15 are saved on exception entry)
reg S16 0x40 4 | Single-precision register (S16 to S31 are kept by a function)
reg S17 0x44 4 | Single-precision register
reg S18 0x48 4 | Single-precision register
reg S19 0x4C 4 | Single-precision register
reg S20 0x50 4 | Single-precision register
reg S21 0x54 4 | Single-precision register
reg S22 0x58 4 | Single-precision register
reg S23 0x5C 4 | Single-precision register
reg S24 0x60 4 | Single-precision register
reg S25 0x64 4 | Single-precision register
reg S26 0x68 4 | Single-precision register
reg S27 0x6C 4 | Single-precision register
reg S28 0x70 4 | Single-precision register
reg S29 0x74 4 | Single-precision register
reg S30 0x78 4 | Single-precision register
reg S31 0x7C 4 | Single-precision register
reg FPSCR 0x80 4 | Floating-point status and control
f N 31 1 | Negative (of the last comparison)
f Z 30 1 | Zero
f C 29 1 | Carry
f V 28 1 | Overflow (unordered)
f AHP 26 1 | Alternative half-precision format
f DN 25 1 | Default NaN mode
f FZ 24 1 | Flush-to-zero mode
f RMode 22 2 | Rounding: 0 to nearest, 1 toward plus infinity, 2 toward minus infinity, 3 toward zero
f IDC 7 1 | Input denormal (cumulative)
f IXC 4 1 | Inexact (cumulative)
f UFC 3 1 | Underflow (cumulative)
f OFC 2 1 | Overflow (cumulative)
f DZC 1 1 | Division by zero (cumulative)
f IOC 0 1 | Invalid operation (cumulative)

per FPU_CONTROL 0xE000ED88 | The FPU's access and context control
reg CPACR 0x000 4 | Coprocessor access control: the FPU is off until CP10 and CP11 are set
f CP11 22 2 | 0 no access, 1 privileged only, 3 full access
f CP10 20 2 | 0 no access, 1 privileged only, 3 full access
reg FPCCR 0x1AC 4 | Floating-point context control
f ASPEN 31 1 | The FPU's use is tracked, and its registers are saved on exception entry
f LSPEN 30 1 | Lazy saving: room is reserved, the registers are written only if the handler uses the FPU
f THREAD 3 1 | The reserved frame belongs to thread mode
f USER 1 1 | The reserved frame belongs to unprivileged code
f LSPACT 0 1 | A reserved frame waits to be written
reg FPCAR 0x1B0 4 | Address of the reserved frame's floating-point part
reg FPDSCR 0x1B4 4 | The FPSCR control bits a new context starts with

type GPIO
reg MODER 0x00 4 | Mode of each pin
fr MODER 0 16 2 0 2 | Pin #: 0 input, 1 output, 2 alternate function, 3 analog
reg OTYPER 0x04 4 | Output type
fr OT 0 16 1 0 1 | Pin #: 0 push-pull, 1 open drain
reg OSPEEDR 0x08 4 | Output speed (kept; not simulated)
fr OSPEEDR 0 16 2 0 2 | Pin #: 0 low, 1 medium, 2 fast, 3 high
reg PUPDR 0x0C 4 | Pull-up and pull-down
fr PUPDR 0 16 2 0 2 | Pin #: 0 none, 1 pull-up, 2 pull-down
reg IDR 0x10 4 | Input data: the levels on the pins
fr IDR 0 16 1 0 1 | Level on pin #
reg ODR 0x14 4 | Output data
fr ODR 0 16 1 0 1 | Output of pin #
reg LCKR 0x1C 4 | Configuration lock
f LCKK 16 1 | Lock key
fr LCK 0 16 1 0 1 | Configuration of pin # locked
reg AFRL 0x20 4 | Alternate function of pins 0 to 7
fr AFRL 0 8 4 0 4 | Function number of pin # (7: USART1 to USART3; 8: UART4, UART5, USART6)
reg AFRH 0x24 4 | Alternate function of pins 8 to 15
fr AFRH 8 8 4 0 4 | Function number of pin # (7: USART1 to USART3; 8: UART4, UART5, USART6)

per GPIOA 0x40020000 | General-purpose I/O port A
use GPIO
per GPIOB 0x40020400 | General-purpose I/O port B
use GPIO
per GPIOC 0x40020800 | General-purpose I/O port C
use GPIO
per GPIOD 0x40020C00 | General-purpose I/O port D
use GPIO
per GPIOE 0x40021000 | General-purpose I/O port E
use GPIO
per GPIOH 0x40021C00 | General-purpose I/O port H (PH0 and PH1, the HSE crystal's pins)
use GPIO

per SYSCFG 0x40013800 | System configuration: the pins of the external interrupts
reg MEMRMP 0x00 4 | Memory remap
f MEM_MODE 0 2 | What is at address 0: 0 flash, 1 system memory, 2 FSMC, 3 SRAM
reg PMC 0x04 4 | Peripheral mode configuration
reg EXTICR1 0x08 4 | Port of the external interrupt lines 0 to 3
fr EXTI 0 4 4 0 4 | Line #: 0 PA, 1 PB, 2 PC, 3 PD, 4 PE, 7 PH
reg EXTICR2 0x0C 4 | Port of the external interrupt lines 4 to 7
fr EXTI 4 4 4 0 4 | Line #: 0 PA, 1 PB, 2 PC, 3 PD, 4 PE, 7 PH
reg EXTICR3 0x10 4 | Port of the external interrupt lines 8 to 11
fr EXTI 8 4 4 0 4 | Line #: 0 PA, 1 PB, 2 PC, 3 PD, 4 PE, 7 PH
reg EXTICR4 0x14 4 | Port of the external interrupt lines 12 to 15
fr EXTI 12 4 4 0 4 | Line #: 0 PA, 1 PB, 2 PC, 3 PD, 4 PE, 7 PH

per EXTI 0x40013C00 | External interrupts and events
reg IMR 0x00 4 | Interrupt mask
fr MR 0 23 1 0 1 | Interrupt of line # enabled
reg EMR 0x04 4 | Event mask
fr MR 0 23 1 0 1 | Event of line # enabled
reg RTSR 0x08 4 | Rising edge trigger
fr TR 0 23 1 0 1 | Rising edge of line #
reg FTSR 0x0C 4 | Falling edge trigger
fr TR 0 23 1 0 1 | Falling edge of line #
reg SWIER 0x10 4 | Software interrupt
fr SWIER 0 23 1 0 1 | Software interrupt on line #
reg PR 0x14 4 | Pending (written with 1 to clear)
fr PR 0 23 1 0 1 | Line # pending
)",
R"(
per RCC 0x40023800 | Reset and clock control
reg CR 0x00 4 | Clock control
f PLLI2SRDY 27 1 | PLLI2S locked
f PLLI2SON 26 1 | PLLI2S enable
f PLLRDY 25 1 | Main PLL locked
f PLLON 24 1 | Main PLL enable
f CSSON 19 1 | Clock security system enable
f HSEBYP 18 1 | External clock instead of a crystal
f HSERDY 17 1 | HSE (crystal) oscillator ready: never without a crystal on PH0 and PH1
f HSEON 16 1 | HSE (crystal) oscillator enable
f HSICAL 8 8 | HSI calibration
f HSITRIM 3 5 | HSI trimming
f HSIRDY 1 1 | HSI (internal 16 MHz) oscillator ready
f HSION 0 1 | HSI (internal 16 MHz) oscillator enable
reg PLLCFGR 0x04 4 | PLL configuration: VCO = input / PLLM * PLLN, SYSCLK = VCO / PLLP
f PLLQ 24 4 | Division for USB, SDIO and the random number generator (48 MHz)
f PLLSRC 22 1 | PLL input: 0 HSI, 1 HSE
f PLLP 16 2 | Division for the system clock: 0 is 2, 1 is 4, 2 is 6, 3 is 8
f PLLN 6 9 | Multiplication (50 to 432)
f PLLM 0 6 | Division of the input (to 1 or 2 MHz)
reg CFGR 0x08 4 | Clock configuration
f MCO2 30 2 | Clock output 2
f MCO2PRE 27 3 | Clock output 2 prescaler
f MCO1PRE 24 3 | Clock output 1 prescaler
f I2SSRC 23 1 | I2S clock source
f MCO1 21 2 | Clock output 1
f RTCPRE 16 5 | HSE division for the RTC
f PPRE2 13 3 | APB2 prescaler: 0 to 3 none, 4 /2, 5 /4, 6 /8, 7 /16
f PPRE1 10 3 | APB1 prescaler: 0 to 3 none, 4 /2, 5 /4, 6 /8, 7 /16
f HPRE 4 4 | AHB prescaler: 0 to 7 none, 8 /2 ... 15 /512
f SWS 2 2 | System clock in use: 0 HSI, 1 HSE, 2 PLL
f SW 0 2 | System clock chosen: 0 HSI, 1 HSE, 2 PLL
reg CIR 0x0C 4 | Clock interrupts
reg AHB1ENR 0x30 4 | AHB1 peripheral clock enable
f OTGHSEN 29 1 | USB OTG HS clock
f ETHMACEN 25 1 | Ethernet clock
f DMA2EN 22 1 | DMA2 clock
f DMA1EN 21 1 | DMA1 clock
f CCMDATARAMEN 20 1 | CCM data RAM clock
f BKPSRAMEN 18 1 | Backup SRAM clock
f CRCEN 12 1 | CRC clock
f GPIOIEN 8 1 | Port I clock
f GPIOHEN 7 1 | Port H clock
f GPIOGEN 6 1 | Port G clock
f GPIOFEN 5 1 | Port F clock
f GPIOEEN 4 1 | Port E clock
f GPIODEN 3 1 | Port D clock
f GPIOCEN 2 1 | Port C clock
f GPIOBEN 1 1 | Port B clock
f GPIOAEN 0 1 | Port A clock
reg AHB2ENR 0x34 4 | AHB2 peripheral clock enable
f OTGFSEN 7 1 | USB OTG FS clock
f RNGEN 6 1 | Random number generator clock
f DCMIEN 0 1 | Camera interface clock
reg APB1ENR 0x40 4 | APB1 peripheral clock enable
f DACEN 29 1 | DAC clock
f PWREN 28 1 | Power interface clock
f CAN2EN 26 1 | CAN2 clock
f CAN1EN 25 1 | CAN1 clock
f I2C3EN 23 1 | I2C3 clock
f I2C2EN 22 1 | I2C2 clock
f I2C1EN 21 1 | I2C1 clock
f UART5EN 20 1 | UART5 clock
f UART4EN 19 1 | UART4 clock
f USART3EN 18 1 | USART3 clock
f USART2EN 17 1 | USART2 clock
f SPI3EN 15 1 | SPI3 clock
f SPI2EN 14 1 | SPI2 clock
f WWDGEN 11 1 | Window watchdog clock
f TIM14EN 8 1 | TIM14 clock
f TIM13EN 7 1 | TIM13 clock
f TIM12EN 6 1 | TIM12 clock
f TIM7EN 5 1 | TIM7 clock
f TIM6EN 4 1 | TIM6 clock
f TIM5EN 3 1 | TIM5 clock
f TIM4EN 2 1 | TIM4 clock
f TIM3EN 1 1 | TIM3 clock
f TIM2EN 0 1 | TIM2 clock
reg APB2ENR 0x44 4 | APB2 peripheral clock enable
f TIM11EN 18 1 | TIM11 clock
f TIM10EN 17 1 | TIM10 clock
f TIM9EN 16 1 | TIM9 clock
f SYSCFGEN 14 1 | System configuration clock
f SPI1EN 12 1 | SPI1 clock
f SDIOEN 11 1 | SDIO clock
f ADC3EN 10 1 | ADC3 clock
f ADC2EN 9 1 | ADC2 clock
f ADC1EN 8 1 | ADC1 clock
f USART6EN 5 1 | USART6 clock
f USART1EN 4 1 | USART1 clock
f TIM8EN 1 1 | TIM8 clock
f TIM1EN 0 1 | TIM1 clock
reg BDCR 0x70 4 | Backup domain control
f BDRST 16 1 | Backup domain reset
f RTCEN 15 1 | RTC clock enable
f RTCSEL 8 2 | RTC clock: 0 none, 1 LSE, 2 LSI, 3 HSE divided
f LSEBYP 2 1 | External clock instead of the 32768 Hz crystal
f LSERDY 1 1 | LSE (32768 Hz) oscillator ready: never without a crystal on PC14 and PC15
f LSEON 0 1 | LSE (32768 Hz) oscillator enable
reg CSR 0x74 4 | Control and status: what caused the last reset
f LPWRRSTF 31 1 | Low-power reset
f WWDGRSTF 30 1 | Window watchdog reset
f IWDGRSTF 29 1 | Independent watchdog reset
f SFTRSTF 28 1 | Software reset
f PORRSTF 27 1 | Power-on reset
f PINRSTF 26 1 | Reset by the NRST pin
f BORRSTF 25 1 | Brown-out reset
f RMVF 24 1 | Remove the reset flags
f LSIRDY 1 1 | LSI (internal 32 kHz) oscillator ready
f LSION 0 1 | LSI (internal 32 kHz) oscillator enable

per FLASH 0x40023C00 | Flash memory interface
reg ACR 0x00 4 | Access control
f DCEN 10 1 | Data cache enable
f ICEN 9 1 | Instruction cache enable
f PRFTEN 8 1 | Prefetch enable
f LATENCY 0 3 | Wait states: one more for every 30 MHz of HCLK (5 at 168 MHz)
reg SR 0x0C 4 | Status
reg CR 0x10 4 | Control
f LOCK 31 1 | Locked
f STRT 16 1 | Start an erase
f MER 2 1 | Mass erase
f SER 1 1 | Sector erase
f PG 0 1 | Programming
reg OPTCR 0x14 4 | Option bytes

per PWR 0x40007000 | Power control
reg CR 0x00 4 | Control
f VOS 14 1 | Regulator voltage scale: 1 for the highest frequencies
f DBP 8 1 | Write access to the backup domain (RTC, backup registers)
f PLS 5 3 | Voltage detector level
f PVDE 4 1 | Voltage detector enable
f PDDS 1 1 | Deep sleep: 0 Stop, 1 Standby
f LPDS 0 1 | Regulator in low power in Stop
reg CSR 0x04 4 | Control and status
f VOSRDY 14 1 | Regulator voltage scale ready
f BRE 9 1 | Backup regulator enable
f EWUP 8 1 | WKUP pin enable
f BRR 3 1 | Backup regulator ready
f PVDO 2 1 | Supply below the detector level
f SBF 1 1 | Was in Standby
f WUF 0 1 | Wakeup event

)",
R"(type F4_TIM
reg CR1 0x00 4 | Control 1
f CKD 8 2 | Clock division for the dead time and the filters: 0 1, 1 2, 2 4
f ARPE 7 1 | ARR buffered until the next update
f CMS 5 2 | Center-aligned mode (not simulated: edge-aligned)
f DIR 4 1 | Direction: 0 up, 1 down
f OPM 3 1 | One-pulse mode
f URS 2 1 | Update request source
f UDIS 1 1 | Update disable
f CEN 0 1 | Counter enable
reg CR2 0x04 4 | Control 2
f TI1S 7 1 | TI1 from CH1, CH2 and CH3 (XOR)
f MMS 4 3 | Master mode, the trigger output (TRGO) for the ADC, the DAC and other timers: 0 reset, 1 enable, 2 update, 3 compare pulse, 4 to 7 OC1REF to OC4REF
f CCDS 3 1 | DMA request on update instead of on compare
reg SMCR 0x08 4 | Slave mode control
f ETP 15 1 | External trigger polarity
f ECE 14 1 | External clock enable
f ETPS 12 2 | External trigger prescaler
f ETF 8 4 | External trigger filter
f MSM 7 1 | Master/slave mode
f TS 4 3 | Trigger selection
f SMS 0 3 | Slave mode: 0 off, 1 to 3 encoder (simulated), 4 reset, 5 gated, 6 trigger, 7 external clock
reg DIER 0x0C 4 | DMA and interrupt enable
f TDE 14 1 | Trigger DMA request enable
f CC4DE 12 1 | Capture/compare 4 DMA request enable
f CC3DE 11 1 | Capture/compare 3 DMA request enable
f CC2DE 10 1 | Capture/compare 2 DMA request enable
f CC1DE 9 1 | Capture/compare 1 DMA request enable
f UDE 8 1 | Update DMA request enable
f BIE 7 1 | Break interrupt enable (TIM1, TIM8)
f TIE 6 1 | Trigger interrupt enable
f COMIE 5 1 | Commutation interrupt enable (TIM1, TIM8)
f CC4IE 4 1 | Capture/compare 4 interrupt enable
f CC3IE 3 1 | Capture/compare 3 interrupt enable
f CC2IE 2 1 | Capture/compare 2 interrupt enable
f CC1IE 1 1 | Capture/compare 1 interrupt enable
f UIE 0 1 | Update interrupt enable
reg SR 0x10 4 | Status
f CC4OF 12 1 | Capture 4 overcapture
f CC3OF 11 1 | Capture 3 overcapture
f CC2OF 10 1 | Capture 2 overcapture
f CC1OF 9 1 | Capture 1 overcapture
f BIF 7 1 | Break flag (TIM1, TIM8)
f TIF 6 1 | Trigger flag
f COMIF 5 1 | Commutation flag (TIM1, TIM8)
f CC4IF 4 1 | Capture/compare 4 flag
f CC3IF 3 1 | Capture/compare 3 flag
f CC2IF 2 1 | Capture/compare 2 flag
f CC1IF 1 1 | Capture/compare 1 flag
f UIF 0 1 | Update flag
reg CCMR1 0x18 4 | Capture/compare mode of channels 1 and 2 (fields named as for outputs)
f OC2CE 15 1 | Output 2 clear enable
f OC2M 12 3 | Output 2 mode: 0 frozen, 1 active on match, 2 inactive on match, 3 toggle, 4 low, 5 high, 6 PWM 1, 7 PWM 2
f OC2PE 11 1 | CCR2 buffered until the next update
f OC2FE 10 1 | Output 2 fast enable
f CC2S 8 2 | Channel 2: 0 output, 1 input on TI2, 2 input on TI1, 3 input on TRC
f OC1CE 7 1 | Output 1 clear enable
f OC1M 4 3 | Output 1 mode: 0 frozen, 1 active on match, 2 inactive on match, 3 toggle, 4 low, 5 high, 6 PWM 1, 7 PWM 2
f OC1PE 3 1 | CCR1 buffered until the next update
f OC1FE 2 1 | Output 1 fast enable
f CC1S 0 2 | Channel 1: 0 output, 1 input on TI1, 2 input on TI2, 3 input on TRC
reg CCMR2 0x1C 4 | Capture/compare mode of channels 3 and 4 (fields named as for outputs)
f OC4CE 15 1 | Output 4 clear enable
f OC4M 12 3 | Output 4 mode: 0 frozen, 1 active on match, 2 inactive on match, 3 toggle, 4 low, 5 high, 6 PWM 1, 7 PWM 2
f OC4PE 11 1 | CCR4 buffered until the next update
f OC4FE 10 1 | Output 4 fast enable
f CC4S 8 2 | Channel 4: 0 output, 1 input on TI4, 2 input on TI3, 3 input on TRC
f OC3CE 7 1 | Output 3 clear enable
f OC3M 4 3 | Output 3 mode: 0 frozen, 1 active on match, 2 inactive on match, 3 toggle, 4 low, 5 high, 6 PWM 1, 7 PWM 2
f OC3PE 3 1 | CCR3 buffered until the next update
f OC3FE 2 1 | Output 3 fast enable
f CC3S 0 2 | Channel 3: 0 output, 1 input on TI3, 2 input on TI4, 3 input on TRC
reg CCER 0x20 4 | Capture/compare enable (CCxNE, CCxNP: the complementary outputs of TIM1 and TIM8)
f CC4NP 15 1 | Channel 4 input: with CC4P, both edges
f CC4P 13 1 | Channel 4 polarity (output: active low; input: falling edge)
f CC4E 12 1 | Channel 4 enable
f CC3NP 11 1 | Complementary output 3 polarity; input: with CC3P, both edges
f CC3NE 10 1 | Complementary output 3 enable
f CC3P 9 1 | Channel 3 polarity
f CC3E 8 1 | Channel 3 enable
f CC2NP 7 1 | Complementary output 2 polarity; input: with CC2P, both edges
f CC2NE 6 1 | Complementary output 2 enable
f CC2P 5 1 | Channel 2 polarity
f CC2E 4 1 | Channel 2 enable
f CC1NP 3 1 | Complementary output 1 polarity; input: with CC1P, both edges
f CC1NE 2 1 | Complementary output 1 enable
f CC1P 1 1 | Channel 1 polarity
f CC1E 0 1 | Channel 1 enable
reg CNT 0x24 4 | Counter (32 bits on TIM2 and TIM5, 16 on the others)
reg PSC 0x28 4 | Prescaler: the counter's clock is the timer's clock divided by PSC + 1
reg ARR 0x2C 4 | Auto-reload: the counter's period is ARR + 1
reg CCR1 0x34 4 | Capture/compare 1
reg CCR2 0x38 4 | Capture/compare 2
reg CCR3 0x3C 4 | Capture/compare 3
reg CCR4 0x40 4 | Capture/compare 4

type F4_TIM_ADVANCED F4_TIM
reg RCR 0x30 4 | Repetition counter: an update every RCR + 1 periods
reg BDTR 0x44 4 | Break and dead time
f MOE 15 1 | Main output enable: the outputs are off without it
f AOE 14 1 | Automatic output enable
f BKP 13 1 | Break polarity
f BKE 12 1 | Break enable (not simulated)
f OSSR 11 1 | Off-state selection for Run mode
f OSSI 10 1 | Off-state selection for Idle mode
f LOCK 8 2 | Lock configuration
f DTG 0 8 | Dead time between an output and its complement, in clock periods (scaled when above 127)

)",
R"(type F4_TIM_TWO
reg CR1 0x00 4 | Control 1
f CKD 8 2 | Clock division for the filters
f ARPE 7 1 | ARR buffered until the next update
f OPM 3 1 | One-pulse mode
f URS 2 1 | Update request source
f UDIS 1 1 | Update disable
f CEN 0 1 | Counter enable
reg SMCR 0x08 4 | Slave mode control
f MSM 7 1 | Master/slave mode
f TS 4 3 | Trigger selection
f SMS 0 3 | Slave mode: 0 off, 4 reset, 5 gated, 6 trigger, 7 external clock
reg DIER 0x0C 4 | Interrupt enable
f TIE 6 1 | Trigger interrupt enable
f CC2IE 2 1 | Capture/compare 2 interrupt enable
f CC1IE 1 1 | Capture/compare 1 interrupt enable
f UIE 0 1 | Update interrupt enable
reg SR 0x10 4 | Status
f CC2OF 10 1 | Capture 2 overcapture
f CC1OF 9 1 | Capture 1 overcapture
f TIF 6 1 | Trigger flag
f CC2IF 2 1 | Capture/compare 2 flag
f CC1IF 1 1 | Capture/compare 1 flag
f UIF 0 1 | Update flag
reg CCMR1 0x18 4 | Capture/compare mode of channels 1 and 2 (fields named as for outputs)
f OC2M 12 3 | Output 2 mode: 0 frozen, 1 active on match, 2 inactive on match, 3 toggle, 4 low, 5 high, 6 PWM 1, 7 PWM 2
f OC2PE 11 1 | CCR2 buffered until the next update
f CC2S 8 2 | Channel 2: 0 output, 1 input on TI2, 2 input on TI1, 3 input on TRC
f OC1M 4 3 | Output 1 mode: 0 frozen, 1 active on match, 2 inactive on match, 3 toggle, 4 low, 5 high, 6 PWM 1, 7 PWM 2
f OC1PE 3 1 | CCR1 buffered until the next update
f CC1S 0 2 | Channel 1: 0 output, 1 input on TI1, 2 input on TI2, 3 input on TRC
reg CCER 0x20 4 | Capture/compare enable
f CC2NP 7 1 | Channel 2 input: with CC2P, both edges
f CC2P 5 1 | Channel 2 polarity
f CC2E 4 1 | Channel 2 enable
f CC1NP 3 1 | Channel 1 input: with CC1P, both edges
f CC1P 1 1 | Channel 1 polarity
f CC1E 0 1 | Channel 1 enable
reg CNT 0x24 4 | Counter (16 bits)
reg PSC 0x28 4 | Prescaler: the counter's clock is the timer's clock divided by PSC + 1
reg ARR 0x2C 4 | Auto-reload: the counter's period is ARR + 1
reg CCR1 0x34 4 | Capture/compare 1
reg CCR2 0x38 4 | Capture/compare 2

type F4_TIM_ONE
reg CR1 0x00 4 | Control 1
f ARPE 7 1 | ARR buffered until the next update
f URS 2 1 | Update request source
f UDIS 1 1 | Update disable
f CEN 0 1 | Counter enable
reg DIER 0x0C 4 | Interrupt enable
f CC1IE 1 1 | Capture/compare 1 interrupt enable
f UIE 0 1 | Update interrupt enable
reg SR 0x10 4 | Status
f CC1OF 9 1 | Capture 1 overcapture
f CC1IF 1 1 | Capture/compare 1 flag
f UIF 0 1 | Update flag
reg CCMR1 0x18 4 | Capture/compare mode of channel 1 (fields named as for an output)
f OC1M 4 3 | Output 1 mode: 0 frozen, 1 active on match, 2 inactive on match, 3 toggle, 4 low, 5 high, 6 PWM 1, 7 PWM 2
f OC1PE 3 1 | CCR1 buffered until the next update
f CC1S 0 2 | Channel 1: 0 output, 1 input on TI1
reg CCER 0x20 4 | Capture/compare enable
f CC1NP 3 1 | Channel 1 input: with CC1P, both edges
f CC1P 1 1 | Channel 1 polarity
f CC1E 0 1 | Channel 1 enable
reg CNT 0x24 4 | Counter (16 bits)
reg PSC 0x28 4 | Prescaler: the counter's clock is the timer's clock divided by PSC + 1
reg ARR 0x2C 4 | Auto-reload: the counter's period is ARR + 1
reg CCR1 0x34 4 | Capture/compare 1

type F4_TIM_BASIC
reg CR1 0x00 4 | Control 1
f ARPE 7 1 | ARR buffered until the next update
f OPM 3 1 | One-pulse mode
f URS 2 1 | Update request source
f UDIS 1 1 | Update disable
f CEN 0 1 | Counter enable
reg CR2 0x04 4 | Control 2
f MMS 4 3 | Master mode, the trigger output (TRGO) for the DAC and the ADC: 0 reset, 1 enable, 2 update
reg DIER 0x0C 4 | Interrupt enable
f UIE 0 1 | Update interrupt enable
reg SR 0x10 4 | Status
f UIF 0 1 | Update flag
reg CNT 0x24 4 | Counter (16 bits)
reg PSC 0x28 4 | Prescaler: the counter's clock is the timer's clock divided by PSC + 1
reg ARR 0x2C 4 | Auto-reload: the counter's period is ARR + 1

per TIM1 0x40010000 | Advanced-control timer 1 (function 1: CH1 PA8 or PE9, CH1N PA7, PB13 or PE8; APB2)
use F4_TIM_ADVANCED
per TIM8 0x40010400 | Advanced-control timer 8 (function 3: CH1 to CH4 PC6 to PC9; APB2)
use F4_TIM_ADVANCED
per TIM2 0x40000000 | General-purpose timer 2, 32 bits (function 1: CH1 PA0, PA5 or PA15)
use F4_TIM
per TIM3 0x40000400 | General-purpose timer 3 (function 2: CH1 PA6, PB4 or PC6)
use F4_TIM
per TIM4 0x40000800 | General-purpose timer 4 (function 2: CH1 to CH4 PB6 to PB9 or PD12 to PD15)
use F4_TIM
per TIM5 0x40000C00 | General-purpose timer 5, 32 bits (function 2: CH1 to CH4 PA0 to PA3)
use F4_TIM
per TIM9 0x40014000 | General-purpose timer 9, two channels (function 3: PA2, PA3 or PE5, PE6; APB2)
use F4_TIM_TWO
per TIM12 0x40001800 | General-purpose timer 12, two channels (function 9: PB14, PB15)
use F4_TIM_TWO
per TIM10 0x40014400 | General-purpose timer 10, one channel (function 3: PB8; APB2)
use F4_TIM_ONE
per TIM11 0x40014800 | General-purpose timer 11, one channel (function 3: PB9; APB2)
use F4_TIM_ONE
per TIM13 0x40001C00 | General-purpose timer 13, one channel (function 9: PA6)
use F4_TIM_ONE
per TIM14 0x40002000 | General-purpose timer 14, one channel (function 9: PA7)
use F4_TIM_ONE
per TIM6 0x40001000 | Basic timer 6 (the DAC's trigger; no pins)
use F4_TIM_BASIC
per TIM7 0x40001400 | Basic timer 7 (no pins)
use F4_TIM_BASIC

)",
R"(type F4_ADC
reg SR 0x00 4 | Status
f OVR 5 1 | Overrun: a result was lost because DR was not read
f STRT 4 1 | Regular conversion started
f JSTRT 3 1 | Injected conversion started
f JEOC 2 1 | Injected conversions ended
f EOC 1 1 | Regular conversion ended (cleared by reading DR)
f AWD 0 1 | Analog watchdog: a result outside HTR and LTR
reg CR1 0x04 4 | Control 1
f OVRIE 26 1 | Overrun interrupt enable
f RES 24 2 | Resolution: 0 12 bits (15 cycles), 1 10 bits (13), 2 8 bits (11), 3 6 bits (9)
f AWDEN 23 1 | Analog watchdog on the regular channels
f JAWDEN 22 1 | Analog watchdog on the injected channels
f DISCNUM 13 3 | Discontinuous mode count (not simulated)
f JDISCEN 12 1 | Discontinuous mode on the injected channels (not simulated)
f DISCEN 11 1 | Discontinuous mode on the regular channels (not simulated)
f JAUTO 10 1 | Injected conversions after the regular ones
f AWDSGL 9 1 | Analog watchdog on one channel only
f SCAN 8 1 | Scan mode: the whole sequence
f JEOCIE 7 1 | Injected end of conversion interrupt enable
f AWDIE 6 1 | Analog watchdog interrupt enable
f EOCIE 5 1 | End of conversion interrupt enable
f AWDCH 0 5 | Analog watchdog channel
reg CR2 0x08 4 | Control 2
f EXTEN 28 2 | Regular trigger edge: 0 none, 1 rising, 2 falling, 3 both
f EXTSEL 24 4 | Regular trigger: 6 TIM2 TRGO, 8 TIM3 TRGO, 14 TIM8 TRGO, ... (RM0090 table 69)
f JEXTEN 20 2 | Injected trigger edge
f JEXTSEL 16 4 | Injected trigger (RM0090 table 70)
f ALIGN 11 1 | Alignment: 0 right, 1 left
f EOCS 10 1 | EOC after each conversion instead of after the sequence
f DDS 9 1 | DMA requests continue after the stream's last transfer
f DMA 8 1 | DMA mode: each regular result asks the DMA for a transfer
f CONT 1 1 | Continuous conversion
f ADON 0 1 | Converter on
reg SMPR1 0x0C 4 | Sample time of channels 10 to 18
fr SMP 10 9 3 0 3 | Channel #: 0 3, 1 15, 2 28, 3 56, 4 84, 5 112, 6 144, 7 480 cycles
reg SMPR2 0x10 4 | Sample time of channels 0 to 9
fr SMP 0 10 3 0 3 | Channel #: 0 3, 1 15, 2 28, 3 56, 4 84, 5 112, 6 144, 7 480 cycles
reg JOFR1 0x14 4 | Offset of injected channel 1
reg JOFR2 0x18 4 | Offset of injected channel 2
reg JOFR3 0x1C 4 | Offset of injected channel 3
reg JOFR4 0x20 4 | Offset of injected channel 4
reg HTR 0x24 4 | Analog watchdog high threshold
reg LTR 0x28 4 | Analog watchdog low threshold
reg SQR1 0x2C 4 | Regular sequence 1
f L 20 4 | Length of the sequence minus 1
fr SQ 13 4 5 0 5 | Channel of conversion #
reg SQR2 0x30 4 | Regular sequence 2
fr SQ 7 6 5 0 5 | Channel of conversion #
reg SQR3 0x34 4 | Regular sequence 3
fr SQ 1 6 5 0 5 | Channel of conversion #
reg JSQR 0x38 4 | Injected sequence
f JL 20 2 | Length of the sequence minus 1
fr JSQ 1 4 5 0 5 | Channel of injected conversion #
reg JDR1 0x3C 4 | Injected data 1
reg JDR2 0x40 4 | Injected data 2
reg JDR3 0x44 4 | Injected data 3
reg JDR4 0x48 4 | Injected data 4
reg DR 0x4C 4 | Regular data: the last conversion (shown without clearing EOC)

per ADC1 0x40012000 | Analog-to-digital converter 1 (channels 0 to 15: PA0 to PA7, PB0, PB1, PC0 to PC5; 16 temperature, 17 VREFINT, 18 VBAT)
use F4_ADC
per ADC2 0x40012100 | Analog-to-digital converter 2 (the same pins as ADC1)
use F4_ADC
per ADC3 0x40012200 | Analog-to-digital converter 3 (on this package: PA0 to PA3, PC0 to PC3)
use F4_ADC
per ADC_COMMON 0x40012300 | Registers the three converters share
reg CSR 0x00 4 | The flags of ADC1 (bits 0 to 5), ADC2 (8 to 13) and ADC3 (16 to 21)
reg CCR 0x04 4 | Common control
f TSVREFE 23 1 | Temperature sensor and VREFINT on
f VBATE 22 1 | VBAT / 2 on channel 18
f ADCPRE 16 2 | ADC clock: PCLK2 divided by 2, 4, 6 or 8
f DMA 14 2 | DMA in the multi-ADC modes (not simulated)
f DELAY 8 4 | Delay between two sampling phases (multi-ADC modes)
f MULTI 0 5 | Multi-ADC mode (not simulated: 0 independent)
reg CDR 0x08 4 | ADC1's data (bits 0 to 15) and ADC2's (16 to 31)

per DAC 0x40007400 | Digital-to-analog converter (channel 1 PA4, channel 2 PA5)
reg CR 0x00 4 | Control
f DMAUDRIE2 29 1 | Channel 2 DMA underrun interrupt enable
f DMAEN2 28 1 | Channel 2 DMA: a request at each trigger
f MAMP2 24 4 | Channel 2 amplitude: 2^(MAMP + 1) - 1 codes for the triangle
f WAVE2 22 2 | Channel 2 wave: 0 none, 1 noise (not simulated), 2 triangle
f TSEL2 19 3 | Channel 2 trigger: 0 TIM6, 1 TIM8, 2 TIM7, 3 TIM5, 4 TIM2, 5 TIM4, 6 EXTI 9, 7 software
f TEN2 18 1 | Channel 2 trigger enable
f BOFF2 17 1 | Channel 2 output buffer off
f EN2 16 1 | Channel 2 enable
f DMAUDRIE1 13 1 | Channel 1 DMA underrun interrupt enable
f DMAEN1 12 1 | Channel 1 DMA: a request at each trigger
f MAMP1 8 4 | Channel 1 amplitude: 2^(MAMP + 1) - 1 codes for the triangle
f WAVE1 6 2 | Channel 1 wave: 0 none, 1 noise (not simulated), 2 triangle
f TSEL1 3 3 | Channel 1 trigger: 0 TIM6, 1 TIM8, 2 TIM7, 3 TIM5, 4 TIM2, 5 TIM4, 6 EXTI 9, 7 software
f TEN1 2 1 | Channel 1 trigger enable
f BOFF1 1 1 | Channel 1 output buffer off
f EN1 0 1 | Channel 1 enable
reg DHR12R1 0x08 4 | Channel 1 data, 12 bits right aligned
reg DHR12L1 0x0C 4 | Channel 1 data, 12 bits left aligned
reg DHR8R1 0x10 4 | Channel 1 data, 8 bits
reg DHR12R2 0x14 4 | Channel 2 data, 12 bits right aligned
reg DHR12L2 0x18 4 | Channel 2 data, 12 bits left aligned
reg DHR8R2 0x1C 4 | Channel 2 data, 8 bits
reg DHR12RD 0x20 4 | Both channels' data, 12 bits right aligned
reg DHR12LD 0x24 4 | Both channels' data, 12 bits left aligned
reg DHR8RD 0x28 4 | Both channels' data, 8 bits
reg DOR1 0x2C 4 | Channel 1 output: the code on PA4 (VREF+ * DOR / 4095)
reg DOR2 0x30 4 | Channel 2 output: the code on PA5
reg SR 0x34 4 | Status
f DMAUDR2 29 1 | Channel 2 DMA underrun: a trigger came before the DMA served the last request
f DMAUDR1 13 1 | Channel 1 DMA underrun
)",
stm32_usart_registers,
R"(
per USART1 0x40011000 | Serial port 1 (PA9 or PB6 TX, PA10 or PB7 RX; function 7)
use USART
per USART2 0x40004400 | Serial port 2 (PA2 or PD5 TX, PA3 or PD6 RX; function 7)
use USART
per USART3 0x40004800 | Serial port 3 (PB10, PC10 or PD8 TX; PB11, PC11 or PD9 RX; function 7)
use USART
per UART4 0x40004C00 | Serial port 4 (PA0 or PC10 TX, PA1 or PC11 RX; function 8)
use USART
per UART5 0x40005000 | Serial port 5 (PC12 TX, PD2 RX; function 8)
use USART
per USART6 0x40011400 | Serial port 6 (PC6 TX, PC7 RX; function 8)
use USART

)",
R"(
type F4_SPI
reg CR1 0x00 2 | Control 1
f BIDIMODE 15 1 | One data line for both directions
f BIDIOE 14 1 | Output enable on the one data line
f CRCEN 13 1 | CRC enable
f CRCNEXT 12 1 | The CRC goes next
f DFF 11 1 | Frame: 0 eight bits, 1 sixteen bits
f RXONLY 10 1 | Receive only
f SSM 9 1 | Slave select by software
f SSI 8 1 | Internal slave select
f LSBFIRST 7 1 | Lowest bit first
f SPE 6 1 | SPI enable
f BR 3 3 | Baud rate: PCLK divided by 2, 4, 8 ... 256
f MSTR 2 1 | Master
f CPOL 1 1 | Clock polarity: its level while idle
f CPHA 0 1 | Clock phase: 0 sample on the first edge, 1 on the second
reg CR2 0x04 2 | Control 2
f TXEIE 7 1 | TXE interrupt enable
f RXNEIE 6 1 | RXNE interrupt enable
f ERRIE 5 1 | Error interrupt enable
f FRF 4 1 | Frame format: 0 Motorola, 1 TI (not simulated)
f SSOE 2 1 | NSS output enable
f TXDMAEN 1 1 | DMA for transmission
f RXDMAEN 0 1 | DMA for reception
reg SR 0x08 2 | Status
f FRE 8 1 | TI frame format error
f BSY 7 1 | Busy
f OVR 6 1 | Overrun
f MODF 5 1 | Mode fault
f CRCERR 4 1 | CRC error
f TXE 1 1 | Transmit buffer empty
f RXNE 0 1 | Received data ready to be read
reg DR 0x0C 2 | Data: the last word received (shown without taking it)
reg CRCPR 0x10 2 | CRC polynomial (CRC not simulated)
reg I2SCFGR 0x1C 2 | I2S configuration
f I2SMOD 11 1 | I2S mode (not simulated)
reg I2SPR 0x20 2 | I2S prescaler

per SPI1 0x40013000 | SPI 1 (function 5: PA5 or PB3 SCK, PA6 or PB4 MISO, PA7 or PB5 MOSI, PA4 or PA15 NSS)
use F4_SPI
per SPI2 0x40003800 | SPI 2 (function 5: PB10 or PB13 SCK, PB14 or PC2 MISO, PB15 or PC3 MOSI, PB9 or PB12 NSS)
use F4_SPI
per SPI3 0x40003C00 | SPI 3 (function 6: PB3 or PC10 SCK, PB4 or PC11 MISO, PB5 or PC12 MOSI, PA4 or PA15 NSS)
use F4_SPI

type F4_I2C
reg CR1 0x00 2 | Control 1
f SWRST 15 1 | Software reset
f ALERT 13 1 | SMBus alert
f PEC 12 1 | Packet error checking
f POS 11 1 | ACK and PEC position
f ACK 10 1 | Acknowledge the bytes received
f STOP 9 1 | Stop generation
f START 8 1 | Start generation
f NOSTRETCH 7 1 | No clock stretching (slave)
f ENGC 6 1 | General call enable
f ENPEC 5 1 | PEC enable
f ENARP 4 1 | ARP enable
f SMBTYPE 3 1 | SMBus type
f SMBUS 1 1 | SMBus mode
f PE 0 1 | Peripheral enable
reg CR2 0x04 2 | Control 2
f LAST 12 1 | DMA last transfer
f DMAEN 11 1 | DMA requests enable
f ITBUFEN 10 1 | Buffer interrupt enable
f ITEVTEN 9 1 | Event interrupt enable
f ITERREN 8 1 | Error interrupt enable
f FREQ 0 6 | PCLK1 in MHz
reg OAR1 0x08 2 | Own address 1
f ADDMODE 15 1 | 10-bit address
f ADD 0 10 | Address (7 bits in bits 7 to 1)
reg OAR2 0x0C 2 | Own address 2
reg DR 0x10 2 | Data: the byte received (shown without taking it)
reg SR1 0x14 2 | Status 1
f SMBALERT 15 1 | SMBus alert
f TIMEOUT 14 1 | Timeout
f PECERR 12 1 | PEC error
f OVR 11 1 | Overrun or underrun
f AF 10 1 | Acknowledge failure: nobody answered
f ARLO 9 1 | Arbitration lost
f BERR 8 1 | Bus error
f TXE 7 1 | Data register empty (transmitting)
f RXNE 6 1 | Data register not empty (receiving)
f STOPF 4 1 | Stop detected (slave)
f ADD10 3 1 | 10-bit header sent
f BTF 2 1 | Byte transfer finished
f ADDR 1 1 | Address sent (master) or matched (slave)
f SB 0 1 | Start condition generated
reg SR2 0x18 2 | Status 2
f PEC 8 8 | Packet error checking register
f DUALF 7 1 | Dual flag
f SMBHOST 6 1 | SMBus host header
f SMBDEFAULT 5 1 | SMBus device default address
f GENCALL 4 1 | General call address
f TRA 2 1 | Transmitter
f BUSY 1 1 | Bus busy
f MSL 0 1 | Master
reg CCR 0x1C 2 | Clock control
f F_S 15 1 | Fast mode (400 kHz)
f DUTY 14 1 | Fast mode duty cycle
f CCR 0 12 | Clock control: half an SCL period in PCLK1 periods (standard mode)
reg TRISE 0x20 2 | Maximum rise time
reg FLTR 0x24 2 | Noise filters (kept, not simulated)
f ANOFF 4 1 | Analog filter off
f DNF 0 4 | Digital filter: 0 off, else this many PCLK1 periods

per I2C1 0x40005400 | I2C 1 (function 4: PB6 or PB8 SCL, PB7 or PB9 SDA)
use F4_I2C
per I2C2 0x40005800 | I2C 2 (function 4: PB10 SCL, PB11 SDA)
use F4_I2C
per I2C3 0x40005C00 | I2C 3 (function 4: PA8 SCL, PC9 SDA)
use F4_I2C

)",
R"(
type F4_DMA
reg LISR 0x00 4 | Status of streams 0 to 3
f TCIF0 5 1 | Stream 0 transfer complete
f HTIF0 4 1 | Stream 0 half transfer
f TEIF0 3 1 | Stream 0 transfer error
f DMEIF0 2 1 | Stream 0 direct mode error (not raised)
f FEIF0 0 1 | Stream 0 FIFO error (not raised)
f TCIF1 11 1 | Stream 1 transfer complete
f HTIF1 10 1 | Stream 1 half transfer
f TEIF1 9 1 | Stream 1 transfer error
f DMEIF1 8 1 | Stream 1 direct mode error (not raised)
f FEIF1 6 1 | Stream 1 FIFO error (not raised)
f TCIF2 21 1 | Stream 2 transfer complete
f HTIF2 20 1 | Stream 2 half transfer
f TEIF2 19 1 | Stream 2 transfer error
f DMEIF2 18 1 | Stream 2 direct mode error (not raised)
f FEIF2 16 1 | Stream 2 FIFO error (not raised)
f TCIF3 27 1 | Stream 3 transfer complete
f HTIF3 26 1 | Stream 3 half transfer
f TEIF3 25 1 | Stream 3 transfer error
f DMEIF3 24 1 | Stream 3 direct mode error (not raised)
f FEIF3 22 1 | Stream 3 FIFO error (not raised)
reg HISR 0x04 4 | Status of streams 4 to 7
f TCIF4 5 1 | Stream 4 transfer complete
f HTIF4 4 1 | Stream 4 half transfer
f TEIF4 3 1 | Stream 4 transfer error
f DMEIF4 2 1 | Stream 4 direct mode error (not raised)
f FEIF4 0 1 | Stream 4 FIFO error (not raised)
f TCIF5 11 1 | Stream 5 transfer complete
f HTIF5 10 1 | Stream 5 half transfer
f TEIF5 9 1 | Stream 5 transfer error
f DMEIF5 8 1 | Stream 5 direct mode error (not raised)
f FEIF5 6 1 | Stream 5 FIFO error (not raised)
f TCIF6 21 1 | Stream 6 transfer complete
f HTIF6 20 1 | Stream 6 half transfer
f TEIF6 19 1 | Stream 6 transfer error
f DMEIF6 18 1 | Stream 6 direct mode error (not raised)
f FEIF6 16 1 | Stream 6 FIFO error (not raised)
f TCIF7 27 1 | Stream 7 transfer complete
f HTIF7 26 1 | Stream 7 half transfer
f TEIF7 25 1 | Stream 7 transfer error
f DMEIF7 24 1 | Stream 7 direct mode error (not raised)
f FEIF7 22 1 | Stream 7 FIFO error (not raised)
reg LIFCR 0x08 4 | Clears the flags of streams 0 to 3 (write-only)
reg HIFCR 0x0C 4 | Clears the flags of streams 4 to 7 (write-only)
reg S0CR 0x10 4 | Stream 0 configuration
f CHSEL 25 3 | Channel: which peripheral request the stream serves (RM0090 tables 42, 43)
f MBURST 23 2 | Memory burst (not simulated)
f PBURST 21 2 | Peripheral burst (not simulated)
f CT 19 1 | Current target in double-buffer mode: 0 M0AR, 1 M1AR
f DBM 18 1 | Double-buffer mode
f PL 16 2 | Priority: 0 low to 3 very high
f PINCOS 15 1 | Peripheral increment of 4 bytes
f MSIZE 13 2 | Memory size: 0 byte, 1 half word, 2 word
f PSIZE 11 2 | Peripheral size: 0 byte, 1 half word, 2 word
f MINC 10 1 | Memory address increments
f PINC 9 1 | Peripheral address increments
f CIRC 8 1 | Circular mode
f DIR 6 2 | Direction: 0 peripheral to memory, 1 memory to peripheral, 2 memory to memory (DMA2)
f PFCTRL 5 1 | Peripheral flow controller (not simulated)
f TCIE 4 1 | Transfer complete interrupt enable
f HTIE 3 1 | Half transfer interrupt enable
f TEIE 2 1 | Transfer error interrupt enable
f DMEIE 1 1 | Direct mode error interrupt enable
f EN 0 1 | Stream enable
reg S0NDTR 0x14 4 | Stream 0: items left to transfer
reg S0PAR 0x18 4 | Stream 0: peripheral address (the source from memory to memory)
reg S0M0AR 0x1C 4 | Stream 0: memory address 0
reg S0M1AR 0x20 4 | Stream 0: memory address 1 (double-buffer mode)
reg S0FCR 0x24 4 | Stream 0 FIFO control
f FEIE 7 1 | FIFO error interrupt enable
f FS 3 3 | FIFO status (4: empty)
f DMDIS 2 1 | Direct mode off: the FIFO is used
f FTH 0 2 | FIFO threshold (not simulated)
reg S1CR 0x28 4 | Stream 1 configuration
like S0CR
reg S1NDTR 0x2C 4 | Stream 1: items left to transfer
reg S1PAR 0x30 4 | Stream 1: peripheral address (the source from memory to memory)
reg S1M0AR 0x34 4 | Stream 1: memory address 0
reg S1M1AR 0x38 4 | Stream 1: memory address 1 (double-buffer mode)
reg S1FCR 0x3C 4 | Stream 1 FIFO control
like S0FCR
reg S2CR 0x40 4 | Stream 2 configuration
like S0CR
reg S2NDTR 0x44 4 | Stream 2: items left to transfer
reg S2PAR 0x48 4 | Stream 2: peripheral address (the source from memory to memory)
reg S2M0AR 0x4C 4 | Stream 2: memory address 0
reg S2M1AR 0x50 4 | Stream 2: memory address 1 (double-buffer mode)
reg S2FCR 0x54 4 | Stream 2 FIFO control
like S0FCR
reg S3CR 0x58 4 | Stream 3 configuration
like S0CR
reg S3NDTR 0x5C 4 | Stream 3: items left to transfer
reg S3PAR 0x60 4 | Stream 3: peripheral address (the source from memory to memory)
reg S3M0AR 0x64 4 | Stream 3: memory address 0
reg S3M1AR 0x68 4 | Stream 3: memory address 1 (double-buffer mode)
reg S3FCR 0x6C 4 | Stream 3 FIFO control
like S0FCR
reg S4CR 0x70 4 | Stream 4 configuration
like S0CR
reg S4NDTR 0x74 4 | Stream 4: items left to transfer
reg S4PAR 0x78 4 | Stream 4: peripheral address (the source from memory to memory)
reg S4M0AR 0x7C 4 | Stream 4: memory address 0
reg S4M1AR 0x80 4 | Stream 4: memory address 1 (double-buffer mode)
reg S4FCR 0x84 4 | Stream 4 FIFO control
like S0FCR
reg S5CR 0x88 4 | Stream 5 configuration
like S0CR
reg S5NDTR 0x8C 4 | Stream 5: items left to transfer
reg S5PAR 0x90 4 | Stream 5: peripheral address (the source from memory to memory)
reg S5M0AR 0x94 4 | Stream 5: memory address 0
reg S5M1AR 0x98 4 | Stream 5: memory address 1 (double-buffer mode)
reg S5FCR 0x9C 4 | Stream 5 FIFO control
like S0FCR
reg S6CR 0xA0 4 | Stream 6 configuration
like S0CR
reg S6NDTR 0xA4 4 | Stream 6: items left to transfer
reg S6PAR 0xA8 4 | Stream 6: peripheral address (the source from memory to memory)
reg S6M0AR 0xAC 4 | Stream 6: memory address 0
reg S6M1AR 0xB0 4 | Stream 6: memory address 1 (double-buffer mode)
reg S6FCR 0xB4 4 | Stream 6 FIFO control
like S0FCR
reg S7CR 0xB8 4 | Stream 7 configuration
like S0CR
reg S7NDTR 0xBC 4 | Stream 7: items left to transfer
reg S7PAR 0xC0 4 | Stream 7: peripheral address (the source from memory to memory)
reg S7M0AR 0xC4 4 | Stream 7: memory address 0
reg S7M1AR 0xC8 4 | Stream 7: memory address 1 (double-buffer mode)
reg S7FCR 0xCC 4 | Stream 7 FIFO control
like S0FCR

per DMA1 0x40026000 | DMA controller 1 (serial ports 2 to 5, SPI2, SPI3, the I2Cs, the DAC)
use F4_DMA
per DMA2 0x40026400 | DMA controller 2 (USART1, USART6, SPI1, the ADCs; memory to memory)
use F4_DMA
)",
R"(

per RTC 0x40002800 | Real-time clock with its backup registers (kept through a reset)
reg TR 0x00 4 | Time (BCD)
f PM 22 1 | PM in the 12-hour format
f HT 20 2 | Hour tens
f HU 16 4 | Hour units
f MNT 12 3 | Minute tens
f MNU 8 4 | Minute units
f ST 4 3 | Second tens
f SU 0 4 | Second units
reg DR 0x04 4 | Date (BCD)
f YT 20 4 | Year tens
f YU 16 4 | Year units
f WDU 13 3 | Weekday: 1 Monday to 7 Sunday
f MT 12 1 | Month tens
f MU 8 4 | Month units
f DT 4 2 | Day tens
f DU 0 4 | Day units
reg CR 0x08 4 | Control
f COE 23 1 | Calibration output (not simulated)
f OSEL 21 2 | Output: alarm A, alarm B or wakeup (not simulated)
f BKP 18 1 | Summer time already applied (a note for the firmware)
f SUB1H 17 1 | Subtract one hour
f ADD1H 16 1 | Add one hour
f WUTIE 14 1 | Wakeup interrupt enable
f ALRBIE 13 1 | Alarm B interrupt enable
f ALRAIE 12 1 | Alarm A interrupt enable
f WUTE 10 1 | Wakeup timer enable
f ALRBE 9 1 | Alarm B enable
f ALRAE 8 1 | Alarm A enable
f FMT 6 1 | Hour format: 0 24 hours, 1 AM/PM
f WUCKSEL 0 3 | Wakeup clock: 0 RTCCLK/16, 1 /8, 2 /4, 3 /2, 4 1 Hz, 6 1 Hz + 2^16
reg ISR 0x0C 4 | Initialization and status
f WUTF 10 1 | Wakeup flag
f ALRBF 9 1 | Alarm B flag
f ALRAF 8 1 | Alarm A flag
f INIT 7 1 | Initialization mode requested
f INITF 6 1 | In initialization mode: TR, DR and PRER can be written
f RSF 5 1 | Shadow registers synchronized
f INITS 4 1 | The calendar was initialized (the year is not 0)
f WUTWF 2 1 | WUTR can be written
f ALRBWF 1 1 | ALRMBR can be written
f ALRAWF 0 1 | ALRMAR can be written
reg PRER 0x10 4 | Prescalers: RTCCLK / (PREDIV_A + 1) / (PREDIV_S + 1) is the 1 Hz clock
f PREDIV_A 16 7 | Asynchronous prescaler
f PREDIV_S 0 15 | Synchronous prescaler
reg WUTR 0x14 4 | Wakeup timer: an event every WUTR + 1 periods of its clock
reg ALRMAR 0x1C 4 | Alarm A (BCD; a mask bit set leaves its field out)
f MSK4 31 1 | Leave the date or weekday out
f WDSEL 30 1 | DU is a weekday
f DT 28 2 | Day tens
f DU 24 4 | Day units or weekday
f MSK3 23 1 | Leave the hours out
f HT 20 2 | Hour tens
f HU 16 4 | Hour units
f MSK2 15 1 | Leave the minutes out
f MNT 12 3 | Minute tens
f MNU 8 4 | Minute units
f MSK1 7 1 | Leave the seconds out
f ST 4 3 | Second tens
f SU 0 4 | Second units
reg ALRMBR 0x20 4 | Alarm B
like ALRMAR
reg SSR 0x28 4 | Subseconds: PREDIV_S down to 0 in each second
reg BKP0R 0x50 4 | Backup register 0
reg BKP1R 0x54 4 | Backup register 1
reg BKP2R 0x58 4 | Backup register 2
reg BKP3R 0x5C 4 | Backup register 3
reg BKP4R 0x60 4 | Backup register 4
reg BKP5R 0x64 4 | Backup register 5
reg BKP6R 0x68 4 | Backup register 6
reg BKP7R 0x6C 4 | Backup register 7
reg BKP8R 0x70 4 | Backup register 8
reg BKP9R 0x74 4 | Backup register 9
reg BKP10R 0x78 4 | Backup register 10
reg BKP11R 0x7C 4 | Backup register 11
reg BKP12R 0x80 4 | Backup register 12
reg BKP13R 0x84 4 | Backup register 13
reg BKP14R 0x88 4 | Backup register 14
reg BKP15R 0x8C 4 | Backup register 15
reg BKP16R 0x90 4 | Backup register 16
reg BKP17R 0x94 4 | Backup register 17
reg BKP18R 0x98 4 | Backup register 18
reg BKP19R 0x9C 4 | Backup register 19

per IWDG 0x40003000 | Independent watchdog (LSI, 32 kHz)
reg PR 0x04 4 | Prescaler
f PR 0 3 | LSI divided by 4, 8, 16, 32, 64, 128, 256
reg RLR 0x08 4 | Reload value
f RL 0 12

per WWDG 0x40002C00 | Window watchdog
reg CR 0x00 4 | Control
f WDGA 7 1 | Watchdog active
f T 0 7 | Counter: a reset when it goes from 0x40 to 0x3F
reg CFR 0x04 4 | Configuration
f EWI 9 1 | Early wakeup interrupt enable
f WDGTB 7 2 | Timer base: PCLK1/4096 divided by 1, 2, 4, 8
f W 0 7 | Window: a refresh above it resets the chip
reg SR 0x08 4 | Status
f EWIF 0 1 | Early wakeup interrupt flag

per CRC 0x40023000 | CRC calculation unit
reg DR 0x00 4 | Data: the CRC so far
reg IDR 0x04 4 | Independent data (one free byte)

per RNG 0x50060800 | Random number generator (a fixed sequence in SimNodus)
reg CR 0x00 4 | Control
f IE 3 1 | Interrupt enable
f RNGEN 2 1 | Generator enable
reg SR 0x04 4 | Status
f DRDY 0 1 | A number is ready
reg DR 0x08 4 | Data: the number (shown without taking it)

per NVIC_64_81 0xE000E100 | Cortex-M interrupt controller: interrupts 64 to 81
reg ISER2 0x008 4 | Interrupts 64 to 81 enabled
reg ISPR2 0x108 4 | Interrupts 64 to 81 pending
reg IABR2 0x208 4 | Interrupts 64 to 81 active

)",
cortex_system_registers,
nullptr};

// The AVR chips: addresses are in the data space (the I/O address plus 0x20).
const char* const atmega8_registers[] = {
R"(chip atmega8

per CPU 0 | Status, stack pointer and the chip's control registers
reg SREG 0x5F 1 | Status register
bits I T H S V N Z C
reg SP 0x5D 2 | Stack pointer
reg MCUCR 0x55 1 | MCU control: sleep and the sense of INT0 and INT1
bits SE SM2 SM1 SM0 ISC11 ISC10 ISC01 ISC00
reg MCUCSR 0x54 1 | What caused the last reset
bits - - - - WDRF BORF EXTRF PORF
reg GICR 0x5B 1 | External interrupt enable
bits INT1 INT0 - - - - IVSEL IVCE
reg GIFR 0x5A 1 | External interrupt flags
bits INTF1 INTF0 - - - - - -
reg SFIOR 0x50 1 | Special functions
bits - - - - ACME PUD PSR2 PSR10
reg OSCCAL 0x51 1 | Calibration of the internal oscillator
reg SPMCR 0x57 1 | Store program memory control
bits SPMIE RWWSB - RWWSRE BLBSET PGWRT PGERS SPMEN
reg WDTCR 0x41 1 | Watchdog control
bits - - - WDCE WDE WDP2 WDP1 WDP0

per CORE 0xFFFFF000 | Program counter
reg PC 0x88 4 | Program counter, as a byte address in the flash

per REGISTERS 0 | General-purpose registers R0 to R31
reg R0 0x00 1 | General-purpose register
reg R1 0x01 1 | General-purpose register
reg R2 0x02 1 | General-purpose register
reg R3 0x03 1 | General-purpose register
reg R4 0x04 1 | General-purpose register
reg R5 0x05 1 | General-purpose register
reg R6 0x06 1 | General-purpose register
reg R7 0x07 1 | General-purpose register
reg R8 0x08 1 | General-purpose register
reg R9 0x09 1 | General-purpose register
reg R10 0x0A 1 | General-purpose register
reg R11 0x0B 1 | General-purpose register
reg R12 0x0C 1 | General-purpose register
reg R13 0x0D 1 | General-purpose register
reg R14 0x0E 1 | General-purpose register
reg R15 0x0F 1 | General-purpose register
reg R16 0x10 1 | General-purpose register
reg R17 0x11 1 | General-purpose register
reg R18 0x12 1 | General-purpose register
reg R19 0x13 1 | General-purpose register
reg R20 0x14 1 | General-purpose register
reg R21 0x15 1 | General-purpose register
reg R22 0x16 1 | General-purpose register
reg R23 0x17 1 | General-purpose register
reg R24 0x18 1 | General-purpose register
reg R25 0x19 1 | General-purpose register
reg R26 0x1A 1 | General-purpose register (X low)
reg R27 0x1B 1 | General-purpose register (X high)
reg R28 0x1C 1 | General-purpose register (Y low)
reg R29 0x1D 1 | General-purpose register (Y high)
reg R30 0x1E 1 | General-purpose register (Z low)
reg R31 0x1F 1 | General-purpose register (Z high)

per PORTB 0 | Port B
reg PORTB 0x38 1 | Output (for an input: its pull-up)
bits PB7 PB6 PB5 PB4 PB3 PB2 PB1 PB0
reg DDRB 0x37 1 | Direction: 1 output
bits DDB7 DDB6 DDB5 DDB4 DDB3 DDB2 DDB1 DDB0
reg PINB 0x36 1 | The levels on the pins
bits PINB7 PINB6 PINB5 PINB4 PINB3 PINB2 PINB1 PINB0

per PORTC 0 | Port C
reg PORTC 0x35 1 | Output (for an input: its pull-up)
bits - PC6 PC5 PC4 PC3 PC2 PC1 PC0
reg DDRC 0x34 1 | Direction: 1 output
bits - DDC6 DDC5 DDC4 DDC3 DDC2 DDC1 DDC0
reg PINC 0x33 1 | The levels on the pins
bits - PINC6 PINC5 PINC4 PINC3 PINC2 PINC1 PINC0

per PORTD 0 | Port D
reg PORTD 0x32 1 | Output (for an input: its pull-up)
bits PD7 PD6 PD5 PD4 PD3 PD2 PD1 PD0
reg DDRD 0x31 1 | Direction: 1 output
bits DDD7 DDD6 DDD5 DDD4 DDD3 DDD2 DDD1 DDD0
reg PIND 0x30 1 | The levels on the pins
bits PIND7 PIND6 PIND5 PIND4 PIND3 PIND2 PIND1 PIND0

per TIMERS 0 | Interrupt masks and flags of the three timers
reg TIMSK 0x59 1 | Timer interrupt enable
bits OCIE2 TOIE2 TICIE1 OCIE1A OCIE1B TOIE1 - TOIE0
reg TIFR 0x58 1 | Timer interrupt flags
bits OCF2 TOV2 ICF1 OCF1A OCF1B TOV1 - TOV0

per TIMER0 0 | Timer/Counter 0 (8 bits)
reg TCCR0 0x53 1 | Control: the clock
bits - - - - - CS02 CS01 CS00
reg TCNT0 0x52 1 | Counter

per TIMER1 0 | Timer/Counter 1 (16 bits)
reg TCCR1A 0x4F 1 | Control A: outputs and waveform
bits COM1A1 COM1A0 COM1B1 COM1B0 FOC1A FOC1B WGM11 WGM10
reg TCCR1B 0x4E 1 | Control B: capture, waveform and clock
bits ICNC1 ICES1 - WGM13 WGM12 CS12 CS11 CS10
reg TCNT1 0x4C 2 | Counter
reg OCR1A 0x4A 2 | Output compare A
reg OCR1B 0x48 2 | Output compare B
reg ICR1 0x46 2 | Input capture

per TIMER2 0 | Timer/Counter 2 (8 bits, asynchronous)
reg TCCR2 0x45 1 | Control: output, waveform and clock
bits FOC2 WGM20 COM21 COM20 WGM21 CS22 CS21 CS20
reg TCNT2 0x44 1 | Counter
reg OCR2 0x43 1 | Output compare
reg ASSR 0x42 1 | Asynchronous status
bits - - - - AS2 TCN2UB OCR2UB TCR2UB

per USART 0 | Serial port
reg UDR 0x2C 1 | Data: the byte received (shown without taking it)
reg UCSRA 0x2B 1 | Control and status A
bits RXC TXC UDRE FE DOR PE U2X MPCM
reg UCSRB 0x2A 1 | Control and status B
bits RXCIE TXCIE UDRIE RXEN TXEN UCSZ2 RXB8 TXB8
reg UBRRL 0x29 1 | Baud rate, low byte
reg UBRRH_UCSRC 0x40 1 | Baud rate high byte, or control C (one address for both)

per SPI 0 | Serial peripheral interface
reg SPCR 0x2D 1 | Control
bits SPIE SPE DORD MSTR CPOL CPHA SPR1 SPR0
reg SPSR 0x2E 1 | Status
bits SPIF WCOL - - - - - SPI2X
reg SPDR 0x2F 1 | Data

per TWI 0 | Two-wire interface (I2C)
reg TWCR 0x56 1 | Control
bits TWINT TWEA TWSTA TWSTO TWWC TWEN - TWIE
reg TWSR 0x21 1 | Status and prescaler
bits TWS7 TWS6 TWS5 TWS4 TWS3 - TWPS1 TWPS0
reg TWDR 0x23 1 | Data
reg TWAR 0x22 1 | Own address
bits TWA6 TWA5 TWA4 TWA3 TWA2 TWA1 TWA0 TWGCE
reg TWBR 0x20 1 | Bit rate

per ADC 0 | Analog-to-digital converter and comparator
reg ADMUX 0x27 1 | Reference and channel
bits REFS1 REFS0 ADLAR - MUX3 MUX2 MUX1 MUX0
reg ADCSRA 0x26 1 | Control and status
bits ADEN ADSC ADFR ADIF ADIE ADPS2 ADPS1 ADPS0
reg ADC 0x24 2 | Result
reg ACSR 0x28 1 | Analog comparator
bits ACD ACBG ACO ACI ACIE ACIC ACIS1 ACIS0

per EEPROM 0 | EEPROM access
reg EEAR 0x3E 2 | Address
reg EEDR 0x3D 1 | Data
reg EECR 0x3C 1 | Control
bits - - - - EERIE EEMWE EEWE EERE
)",
nullptr};

const char* const atmega32_registers[] = {
R"(chip atmega32

per CPU 0 | Status, stack pointer and the chip's control registers
reg SREG 0x5F 1 | Status register
bits I T H S V N Z C
reg SP 0x5D 2 | Stack pointer
reg MCUCR 0x55 1 | MCU control: sleep and the sense of INT0 and INT1
bits SE SM2 SM1 SM0 ISC11 ISC10 ISC01 ISC00
reg MCUCSR 0x54 1 | JTAG disable, the sense of INT2 and what caused the last reset
bits JTD ISC2 - JTRF WDRF BORF EXTRF PORF
reg GICR 0x5B 1 | External interrupt enable
bits INT1 INT0 INT2 - - - IVSEL IVCE
reg GIFR 0x5A 1 | External interrupt flags
bits INTF1 INTF0 INTF2 - - - - -
reg SFIOR 0x50 1 | Special functions: the ADC's trigger, pull-ups, prescaler resets
bits ADTS2 ADTS1 ADTS0 - ACME PUD PSR2 PSR10
reg OSCCAL 0x51 1 | Calibration of the internal oscillator
reg SPMCR 0x57 1 | Store program memory control
bits SPMIE RWWSB - RWWSRE BLBSET PGWRT PGERS SPMEN
reg WDTCR 0x41 1 | Watchdog control
bits - - - WDTOE WDE WDP2 WDP1 WDP0

per CORE 0xFFFFF000 | Program counter
reg PC 0x88 4 | Program counter, as a byte address in the flash

per REGISTERS 0 | General-purpose registers R0 to R31
reg R0 0x00 1 | General-purpose register
reg R1 0x01 1 | General-purpose register
reg R2 0x02 1 | General-purpose register
reg R3 0x03 1 | General-purpose register
reg R4 0x04 1 | General-purpose register
reg R5 0x05 1 | General-purpose register
reg R6 0x06 1 | General-purpose register
reg R7 0x07 1 | General-purpose register
reg R8 0x08 1 | General-purpose register
reg R9 0x09 1 | General-purpose register
reg R10 0x0A 1 | General-purpose register
reg R11 0x0B 1 | General-purpose register
reg R12 0x0C 1 | General-purpose register
reg R13 0x0D 1 | General-purpose register
reg R14 0x0E 1 | General-purpose register
reg R15 0x0F 1 | General-purpose register
reg R16 0x10 1 | General-purpose register
reg R17 0x11 1 | General-purpose register
reg R18 0x12 1 | General-purpose register
reg R19 0x13 1 | General-purpose register
reg R20 0x14 1 | General-purpose register
reg R21 0x15 1 | General-purpose register
reg R22 0x16 1 | General-purpose register
reg R23 0x17 1 | General-purpose register
reg R24 0x18 1 | General-purpose register
reg R25 0x19 1 | General-purpose register
reg R26 0x1A 1 | General-purpose register (X low)
reg R27 0x1B 1 | General-purpose register (X high)
reg R28 0x1C 1 | General-purpose register (Y low)
reg R29 0x1D 1 | General-purpose register (Y high)
reg R30 0x1E 1 | General-purpose register (Z low)
reg R31 0x1F 1 | General-purpose register (Z high)

per PORTA 0 | Port A
reg PORTA 0x3B 1 | Output (for an input: its pull-up)
bits PA7 PA6 PA5 PA4 PA3 PA2 PA1 PA0
reg DDRA 0x3A 1 | Direction: 1 output
bits DDA7 DDA6 DDA5 DDA4 DDA3 DDA2 DDA1 DDA0
reg PINA 0x39 1 | The levels on the pins
bits PINA7 PINA6 PINA5 PINA4 PINA3 PINA2 PINA1 PINA0

per PORTB 0 | Port B
reg PORTB 0x38 1 | Output (for an input: its pull-up)
bits PB7 PB6 PB5 PB4 PB3 PB2 PB1 PB0
reg DDRB 0x37 1 | Direction: 1 output
bits DDB7 DDB6 DDB5 DDB4 DDB3 DDB2 DDB1 DDB0
reg PINB 0x36 1 | The levels on the pins
bits PINB7 PINB6 PINB5 PINB4 PINB3 PINB2 PINB1 PINB0

per PORTC 0 | Port C
reg PORTC 0x35 1 | Output (for an input: its pull-up)
bits PC7 PC6 PC5 PC4 PC3 PC2 PC1 PC0
reg DDRC 0x34 1 | Direction: 1 output
bits DDC7 DDC6 DDC5 DDC4 DDC3 DDC2 DDC1 DDC0
reg PINC 0x33 1 | The levels on the pins
bits PINC7 PINC6 PINC5 PINC4 PINC3 PINC2 PINC1 PINC0

per PORTD 0 | Port D
reg PORTD 0x32 1 | Output (for an input: its pull-up)
bits PD7 PD6 PD5 PD4 PD3 PD2 PD1 PD0
reg DDRD 0x31 1 | Direction: 1 output
bits DDD7 DDD6 DDD5 DDD4 DDD3 DDD2 DDD1 DDD0
reg PIND 0x30 1 | The levels on the pins
bits PIND7 PIND6 PIND5 PIND4 PIND3 PIND2 PIND1 PIND0

per TIMERS 0 | Interrupt masks and flags of the three timers
reg TIMSK 0x59 1 | Timer interrupt enable
bits OCIE2 TOIE2 TICIE1 OCIE1A OCIE1B TOIE1 OCIE0 TOIE0
reg TIFR 0x58 1 | Timer interrupt flags
bits OCF2 TOV2 ICF1 OCF1A OCF1B TOV1 OCF0 TOV0

per TIMER0 0 | Timer/Counter 0 (8 bits)
reg TCCR0 0x53 1 | Control: output, waveform and clock
bits FOC0 WGM00 COM01 COM00 WGM01 CS02 CS01 CS00
reg TCNT0 0x52 1 | Counter
reg OCR0 0x5C 1 | Output compare

per TIMER1 0 | Timer/Counter 1 (16 bits)
reg TCCR1A 0x4F 1 | Control A: outputs and waveform
bits COM1A1 COM1A0 COM1B1 COM1B0 FOC1A FOC1B WGM11 WGM10
reg TCCR1B 0x4E 1 | Control B: capture, waveform and clock
bits ICNC1 ICES1 - WGM13 WGM12 CS12 CS11 CS10
reg TCNT1 0x4C 2 | Counter
reg OCR1A 0x4A 2 | Output compare A
reg OCR1B 0x48 2 | Output compare B
reg ICR1 0x46 2 | Input capture

per TIMER2 0 | Timer/Counter 2 (8 bits, asynchronous)
reg TCCR2 0x45 1 | Control: output, waveform and clock
bits FOC2 WGM20 COM21 COM20 WGM21 CS22 CS21 CS20
reg TCNT2 0x44 1 | Counter
reg OCR2 0x43 1 | Output compare
reg ASSR 0x42 1 | Asynchronous status
bits - - - - AS2 TCN2UB OCR2UB TCR2UB

per USART 0 | Serial port
reg UDR 0x2C 1 | Data: the byte received (shown without taking it)
reg UCSRA 0x2B 1 | Control and status A
bits RXC TXC UDRE FE DOR PE U2X MPCM
reg UCSRB 0x2A 1 | Control and status B
bits RXCIE TXCIE UDRIE RXEN TXEN UCSZ2 RXB8 TXB8
reg UBRRL 0x29 1 | Baud rate, low byte
reg UBRRH_UCSRC 0x40 1 | Baud rate high byte, or control C (one address for both)

per SPI 0 | Serial peripheral interface
reg SPCR 0x2D 1 | Control
bits SPIE SPE DORD MSTR CPOL CPHA SPR1 SPR0
reg SPSR 0x2E 1 | Status
bits SPIF WCOL - - - - - SPI2X
reg SPDR 0x2F 1 | Data

per TWI 0 | Two-wire interface (I2C)
reg TWCR 0x56 1 | Control
bits TWINT TWEA TWSTA TWSTO TWWC TWEN - TWIE
reg TWSR 0x21 1 | Status and prescaler
bits TWS7 TWS6 TWS5 TWS4 TWS3 - TWPS1 TWPS0
reg TWDR 0x23 1 | Data
reg TWAR 0x22 1 | Own address
bits TWA6 TWA5 TWA4 TWA3 TWA2 TWA1 TWA0 TWGCE
reg TWBR 0x20 1 | Bit rate

per ADC 0 | Analog-to-digital converter and comparator
reg ADMUX 0x27 1 | Reference and channel
bits REFS1 REFS0 ADLAR MUX4 MUX3 MUX2 MUX1 MUX0
reg ADCSRA 0x26 1 | Control and status
bits ADEN ADSC ADATE ADIF ADIE ADPS2 ADPS1 ADPS0
reg ADC 0x24 2 | Result
reg ACSR 0x28 1 | Analog comparator
bits ACD ACBG ACO ACI ACIE ACIC ACIS1 ACIS0

per EEPROM 0 | EEPROM access
reg EEAR 0x3E 2 | Address
reg EEDR 0x3D 1 | Data
reg EECR 0x3C 1 | Control
bits - - - - EERIE EEMWE EEWE EERE
)",
nullptr};

const char* const atmega328p_registers[] = {
R"(chip atmega328p

per CPU 0 | Status, stack pointer and the chip's control registers
reg SREG 0x5F 1 | Status register
bits I T H S V N Z C
reg SP 0x5D 2 | Stack pointer
reg MCUCR 0x55 1 | MCU control
bits - BODS BODSE PUD - - IVSEL IVCE
reg MCUSR 0x54 1 | What caused the last reset
bits - - - - WDRF BORF EXTRF PORF
reg SMCR 0x53 1 | Sleep mode
bits - - - - SM2 SM1 SM0 SE
reg CLKPR 0x61 1 | Clock prescaler
bits CLKPCE - - - CLKPS3 CLKPS2 CLKPS1 CLKPS0
reg PRR 0x64 1 | Power reduction: 1 turns the part off
bits PRTWI PRTIM2 PRTIM0 - PRTIM1 PRSPI PRUSART0 PRADC
reg OSCCAL 0x66 1 | Calibration of the internal oscillator
reg SPMCSR 0x57 1 | Store program memory control
bits SPMIE RWWSB SIGRD RWWSRE BLBSET PGWRT PGERS SPMEN
reg WDTCSR 0x60 1 | Watchdog control
bits WDIF WDIE WDP3 WDCE WDE WDP2 WDP1 WDP0
reg GPIOR0 0x3E 1 | General-purpose register 0
reg GPIOR1 0x4A 1 | General-purpose register 1
reg GPIOR2 0x4B 1 | General-purpose register 2

per CORE 0xFFFFF000 | Program counter
reg PC 0x88 4 | Program counter, as a byte address in the flash

per REGISTERS 0 | General-purpose registers R0 to R31
reg R0 0x00 1 | General-purpose register
reg R1 0x01 1 | General-purpose register
reg R2 0x02 1 | General-purpose register
reg R3 0x03 1 | General-purpose register
reg R4 0x04 1 | General-purpose register
reg R5 0x05 1 | General-purpose register
reg R6 0x06 1 | General-purpose register
reg R7 0x07 1 | General-purpose register
reg R8 0x08 1 | General-purpose register
reg R9 0x09 1 | General-purpose register
reg R10 0x0A 1 | General-purpose register
reg R11 0x0B 1 | General-purpose register
reg R12 0x0C 1 | General-purpose register
reg R13 0x0D 1 | General-purpose register
reg R14 0x0E 1 | General-purpose register
reg R15 0x0F 1 | General-purpose register
reg R16 0x10 1 | General-purpose register
reg R17 0x11 1 | General-purpose register
reg R18 0x12 1 | General-purpose register
reg R19 0x13 1 | General-purpose register
reg R20 0x14 1 | General-purpose register
reg R21 0x15 1 | General-purpose register
reg R22 0x16 1 | General-purpose register
reg R23 0x17 1 | General-purpose register
reg R24 0x18 1 | General-purpose register
reg R25 0x19 1 | General-purpose register
reg R26 0x1A 1 | General-purpose register (X low)
reg R27 0x1B 1 | General-purpose register (X high)
reg R28 0x1C 1 | General-purpose register (Y low)
reg R29 0x1D 1 | General-purpose register (Y high)
reg R30 0x1E 1 | General-purpose register (Z low)
reg R31 0x1F 1 | General-purpose register (Z high)

per PORTB 0 | Port B
reg PORTB 0x25 1 | Output (for an input: its pull-up)
bits PB7 PB6 PB5 PB4 PB3 PB2 PB1 PB0
reg DDRB 0x24 1 | Direction: 1 output
bits DDB7 DDB6 DDB5 DDB4 DDB3 DDB2 DDB1 DDB0
reg PINB 0x23 1 | The levels on the pins
bits PINB7 PINB6 PINB5 PINB4 PINB3 PINB2 PINB1 PINB0

per PORTC 0 | Port C
reg PORTC 0x28 1 | Output (for an input: its pull-up)
bits - PC6 PC5 PC4 PC3 PC2 PC1 PC0
reg DDRC 0x27 1 | Direction: 1 output
bits - DDC6 DDC5 DDC4 DDC3 DDC2 DDC1 DDC0
reg PINC 0x26 1 | The levels on the pins
bits - PINC6 PINC5 PINC4 PINC3 PINC2 PINC1 PINC0

per PORTD 0 | Port D
reg PORTD 0x2B 1 | Output (for an input: its pull-up)
bits PD7 PD6 PD5 PD4 PD3 PD2 PD1 PD0
reg DDRD 0x2A 1 | Direction: 1 output
bits DDD7 DDD6 DDD5 DDD4 DDD3 DDD2 DDD1 DDD0
reg PIND 0x29 1 | The levels on the pins
bits PIND7 PIND6 PIND5 PIND4 PIND3 PIND2 PIND1 PIND0

per EXINT 0 | External and pin change interrupts
reg EICRA 0x69 1 | The sense of INT0 and INT1
bits - - - - ISC11 ISC10 ISC01 ISC00
reg EIMSK 0x3D 1 | External interrupt enable
bits - - - - - - INT1 INT0
reg EIFR 0x3C 1 | External interrupt flags
bits - - - - - - INTF1 INTF0
reg PCICR 0x68 1 | Pin change interrupt enable
bits - - - - - PCIE2 PCIE1 PCIE0
reg PCIFR 0x3B 1 | Pin change interrupt flags
bits - - - - - PCIF2 PCIF1 PCIF0
reg PCMSK0 0x6B 1 | Pin change mask of port B
bits PCINT7 PCINT6 PCINT5 PCINT4 PCINT3 PCINT2 PCINT1 PCINT0
reg PCMSK1 0x6C 1 | Pin change mask of port C
bits - PCINT14 PCINT13 PCINT12 PCINT11 PCINT10 PCINT9 PCINT8
reg PCMSK2 0x6D 1 | Pin change mask of port D
bits PCINT23 PCINT22 PCINT21 PCINT20 PCINT19 PCINT18 PCINT17 PCINT16

per TIMER0 0 | Timer/Counter 0 (8 bits)
reg TCCR0A 0x44 1 | Control A: outputs and waveform
bits COM0A1 COM0A0 COM0B1 COM0B0 - - WGM01 WGM00
reg TCCR0B 0x45 1 | Control B: waveform and clock
bits FOC0A FOC0B - - WGM02 CS02 CS01 CS00
reg TCNT0 0x46 1 | Counter
reg OCR0A 0x47 1 | Output compare A
reg OCR0B 0x48 1 | Output compare B
reg TIMSK0 0x6E 1 | Interrupt enable
bits - - - - - OCIE0B OCIE0A TOIE0
reg TIFR0 0x35 1 | Interrupt flags
bits - - - - - OCF0B OCF0A TOV0
reg GTCCR 0x43 1 | General timer control: prescaler resets
bits TSM - - - - - PSRASY PSRSYNC

per TIMER1 0 | Timer/Counter 1 (16 bits)
reg TCCR1A 0x80 1 | Control A: outputs and waveform
bits COM1A1 COM1A0 COM1B1 COM1B0 - - WGM11 WGM10
reg TCCR1B 0x81 1 | Control B: capture, waveform and clock
bits ICNC1 ICES1 - WGM13 WGM12 CS12 CS11 CS10
reg TCCR1C 0x82 1 | Control C: force output compare
bits FOC1A FOC1B - - - - - -
reg TCNT1 0x84 2 | Counter
reg ICR1 0x86 2 | Input capture
reg OCR1A 0x88 2 | Output compare A
reg OCR1B 0x8A 2 | Output compare B
reg TIMSK1 0x6F 1 | Interrupt enable
bits - - ICIE1 - - OCIE1B OCIE1A TOIE1
reg TIFR1 0x36 1 | Interrupt flags
bits - - ICF1 - - OCF1B OCF1A TOV1

per TIMER2 0 | Timer/Counter 2 (8 bits, asynchronous)
reg TCCR2A 0xB0 1 | Control A: outputs and waveform
bits COM2A1 COM2A0 COM2B1 COM2B0 - - WGM21 WGM20
reg TCCR2B 0xB1 1 | Control B: waveform and clock
bits FOC2A FOC2B - - WGM22 CS22 CS21 CS20
reg TCNT2 0xB2 1 | Counter
reg OCR2A 0xB3 1 | Output compare A
reg OCR2B 0xB4 1 | Output compare B
reg ASSR 0xB6 1 | Asynchronous status
bits - EXCLK AS2 TCN2UB OCR2AUB OCR2BUB TCR2AUB TCR2BUB
reg TIMSK2 0x70 1 | Interrupt enable
bits - - - - - OCIE2B OCIE2A TOIE2
reg TIFR2 0x37 1 | Interrupt flags
bits - - - - - OCF2B OCF2A TOV2

per USART0 0 | Serial port
reg UDR0 0xC6 1 | Data: the byte received (shown without taking it)
reg UCSR0A 0xC0 1 | Control and status A
bits RXC0 TXC0 UDRE0 FE0 DOR0 UPE0 U2X0 MPCM0
reg UCSR0B 0xC1 1 | Control and status B
bits RXCIE0 TXCIE0 UDRIE0 RXEN0 TXEN0 UCSZ02 RXB80 TXB80
reg UCSR0C 0xC2 1 | Control and status C
bits UMSEL01 UMSEL00 UPM01 UPM00 USBS0 UCSZ01 UCSZ00 UCPOL0
reg UBRR0 0xC4 2 | Baud rate

per SPI 0 | Serial peripheral interface
reg SPCR 0x4C 1 | Control
bits SPIE SPE DORD MSTR CPOL CPHA SPR1 SPR0
reg SPSR 0x4D 1 | Status
bits SPIF WCOL - - - - - SPI2X
reg SPDR 0x4E 1 | Data

per TWI 0 | Two-wire interface (I2C)
reg TWCR 0xBC 1 | Control
bits TWINT TWEA TWSTA TWSTO TWWC TWEN - TWIE
reg TWSR 0xB9 1 | Status and prescaler
bits TWS7 TWS6 TWS5 TWS4 TWS3 - TWPS1 TWPS0
reg TWDR 0xBB 1 | Data
reg TWAR 0xBA 1 | Own address
bits TWA6 TWA5 TWA4 TWA3 TWA2 TWA1 TWA0 TWGCE
reg TWAMR 0xBD 1 | Own address mask
bits TWAM6 TWAM5 TWAM4 TWAM3 TWAM2 TWAM1 TWAM0 -
reg TWBR 0xB8 1 | Bit rate

per ADC 0 | Analog-to-digital converter and comparator
reg ADMUX 0x7C 1 | Reference and channel
bits REFS1 REFS0 ADLAR - MUX3 MUX2 MUX1 MUX0
reg ADCSRA 0x7A 1 | Control and status A
bits ADEN ADSC ADATE ADIF ADIE ADPS2 ADPS1 ADPS0
reg ADCSRB 0x7B 1 | Control and status B: the trigger
bits - ACME - - - ADTS2 ADTS1 ADTS0
reg ADC 0x78 2 | Result
reg DIDR0 0x7E 1 | Digital inputs of the ADC pins off
bits - - ADC5D ADC4D ADC3D ADC2D ADC1D ADC0D
reg DIDR1 0x7F 1 | Digital inputs of the comparator pins off
bits - - - - - - AIN1D AIN0D
reg ACSR 0x50 1 | Analog comparator
bits ACD ACBG ACO ACI ACIE ACIC ACIS1 ACIS0

per EEPROM 0 | EEPROM access
reg EEAR 0x41 2 | Address
reg EEDR 0x40 1 | Data
reg EECR 0x3F 1 | Control
bits - - EEPM1 EEPM0 EERIE EEMPE EEPE EERE
)",
nullptr};

} // namespace simnodus::mcu
