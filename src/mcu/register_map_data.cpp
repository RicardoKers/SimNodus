// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The register maps of the register view (SN-290) as text, with the names of
// RM0008 (STM32F103), PM0056 (Cortex-M3) and the ATmega8, ATmega32 and
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
extern const char* const atmega8_registers[];
extern const char* const atmega32_registers[];
extern const char* const atmega328p_registers[];

const char* const stm32f103_registers[] = {
R"(chip stm32f103

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
R"(
type USART
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

per SysTick 0xE000E010 | Cortex-M3 system timer
reg CTRL 0x00 4 | Control and status
f COUNTFLAG 16 1 | Counted to 0 since the last read by the firmware
f CLKSOURCE 2 1 | Clock: 0 HCLK/8, 1 HCLK
f TICKINT 1 1 | Interrupt when it counts to 0
f ENABLE 0 1 | Counter enable
reg LOAD 0x04 4 | Reload value: the period is LOAD + 1
reg VAL 0x08 4 | Current value
reg CALIB 0x0C 4 | Calibration

per NVIC 0xE000E100 | Cortex-M3 interrupt controller
reg ISER0 0x000 4 | Interrupts 0 to 31 enabled
reg ISER1 0x004 4 | Interrupts 32 to 63 enabled
reg ISPR0 0x100 4 | Interrupts 0 to 31 pending
reg ISPR1 0x104 4 | Interrupts 32 to 63 pending
reg IABR0 0x200 4 | Interrupts 0 to 31 active
reg IABR1 0x204 4 | Interrupts 32 to 63 active

per SCB 0xE000ED00 | Cortex-M3 system control block
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
)",
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
