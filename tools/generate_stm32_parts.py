"""Generate the STM32F407VET6 chip part (experiment E-12, SN-295).

resources/library/mcu/stm32f407vet6_lqfp100.json: the bare chip in LQFP-100
with every pin of the package, named as on the chip (PA0..PE15, PH0 and PH1
for the HSE crystal, NRST, BOOT0, the supply pins). The symbol is not the
package's square: the pins are in two columns in the package's order (1 to 50
down the left, 100 to 51 down the right), with their numbers.

The pinout is the STM32F405xx/407xx datasheet's LQFP-100 one as known when
this was written; it is to be checked against the datasheet's text
(docs/experiments/E-12-results.md).

    python tools/generate_stm32_parts.py
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# LQFP-100 pinout: pin number -> name (unique in the part) and the label drawn.
PINOUT = [
    (1, "PE2"), (2, "PE3"), (3, "PE4"), (4, "PE5"), (5, "PE6"), (6, "VBAT"), (7, "PC13"),
    (8, "PC14", "PC14/OSC32_IN"), (9, "PC15", "PC15/OSC32_OUT"), (10, "VSS"), (11, "VDD"),
    (12, "PH0", "PH0/OSC_IN"), (13, "PH1", "PH1/OSC_OUT"), (14, "NRST"), (15, "PC0"), (16, "PC1"), (17, "PC2"),
    (18, "PC3"), (19, "VDD2", "VDD"), (20, "VSSA"), (21, "VREF", "VREF+"), (22, "VDDA"), (23, "PA0", "PA0/WKUP"),
    (24, "PA1"), (25, "PA2"), (26, "PA3"), (27, "VSS2", "VSS"), (28, "VDD3", "VDD"), (29, "PA4"), (30, "PA5"),
    (31, "PA6"), (32, "PA7"), (33, "PC4"), (34, "PC5"), (35, "PB0"), (36, "PB1"), (37, "PB2", "PB2/BOOT1"),
    (38, "PE7"), (39, "PE8"), (40, "PE9"), (41, "PE10"), (42, "PE11"), (43, "PE12"), (44, "PE13"), (45, "PE14"),
    (46, "PE15"), (47, "PB10"), (48, "PB11"), (49, "VCAP1", "VCAP_1"), (50, "VDD4", "VDD"),
    (51, "PB12"), (52, "PB13"), (53, "PB14"), (54, "PB15"), (55, "PD8"), (56, "PD9"), (57, "PD10"), (58, "PD11"),
    (59, "PD12"), (60, "PD13"), (61, "PD14"), (62, "PD15"), (63, "PC6"), (64, "PC7"), (65, "PC8"), (66, "PC9"),
    (67, "PA8"), (68, "PA9"), (69, "PA10"), (70, "PA11"), (71, "PA12"), (72, "PA13", "PA13/SWDIO"),
    (73, "VCAP2", "VCAP_2"), (74, "VSS3", "VSS"), (75, "VDD5", "VDD"), (76, "PA14", "PA14/SWCLK"), (77, "PA15"),
    (78, "PC10"), (79, "PC11"), (80, "PC12"), (81, "PD0"), (82, "PD1"), (83, "PD2"), (84, "PD3"), (85, "PD4"),
    (86, "PD5"), (87, "PD6"), (88, "PD7"), (89, "PB3", "PB3/SWO"), (90, "PB4"), (91, "PB5"), (92, "PB6"),
    (93, "PB7"), (94, "BOOT0"), (95, "PB8"), (96, "PB9"), (97, "PE0"), (98, "PE1"), (99, "VSS4", "VSS"),
    (100, "VDD6", "VDD"),
]


def is_gpio(name):
    return len(name) >= 3 and name[0] == "P" and name[1] in "ABCDE" and name[2:].isdigit()


def chip():
    half = 50
    top = -(half - 1)  # the first pin's row; rows are two grid units apart
    graphics = [{"rect": [-11, top - 2, 22, 2 * half + 2]}, {"arc": [0, top - 2, 0.8, 180, 180]},
                {"text": "STM32F407VET6", "at": [0, -2], "size": 1.0}, {"text": "LQFP-100", "at": [0, 0], "size": 0.7}]
    pins = []
    names = []
    for entry in PINOUT:
        number, name = entry[0], entry[1]
        label = entry[2] if len(entry) > 2 else name
        names.append(name)
        left = number <= half
        y = top + 2 * ((number - 1) if left else (100 - number))
        x_end, x_body = (-13, -11) if left else (13, 11)
        graphics.append({"line": [x_end, y, x_body, y]})
        graphics.append({"text": label, "at": [-6.6 if left else 6.6, y], "size": 0.55})
        graphics.append({"text": str(number), "at": [-12.0 if left else 12.0, y - 0.5], "size": 0.45})
        pins.append({"name": name, "at": [x_end, y], "dir": "left" if left else "right"})
    assert len(set(names)) == 100
    gpio = [n for n in names if is_gpio(n)]
    assert len(gpio) == 80, len(gpio)

    spice = []
    # The supply pins of one rail are one node inside the chip.
    for other in ("VSS2", "VSS3", "VSS4", "VSSA"):
        spice.append(f"R_{{ref}}_{other.lower()} {{node:VSS}} {{node:{other}}} 1m")
    for other in ("VDD2", "VDD3", "VDD4", "VDD5", "VDD6"):
        spice.append(f"R_{{ref}}_{other.lower()} {{node:VDD}} {{node:{other}}} 1m")
    spice += [
        # About 10 mA of supply current at 3.3 V (the chip takes 2 to 90 mA with its clock).
        "R_{ref}_vdd {node:VDD} {node:VSS} 330",
        "R_{ref}_vdda {node:VDDA} {node:VSS} 10k",
        "R_{ref}_vref {node:VREF} {node:VSS} 15k",
        "R_{ref}_vbat {node:VBAT} {node:VSS} 3.3Meg",
        # The regulator's capacitor pins: the regulator is not simulated.
        "R_{ref}_vcap1 {node:VCAP1} {node:VSS} 100Meg",
        "R_{ref}_vcap2 {node:VCAP2} {node:VSS} 100Meg",
        "R_{ref}_ph0 {node:PH0} {node:VSS} 100Meg",
        "R_{ref}_ph1 {node:PH1} {node:VSS} 100Meg",
        "R_{ref}_nrst {node:NRST} {node:VDD} 40k",  # the chip's own pull-up
        "R_{ref}_boot0 {node:BOOT0} {node:VSS} 100Meg",
    ]
    for name in gpio:
        low = name.lower()
        spice.append(f"R_{{ref}}_{low} {{node:{name}}} {{node:VSS}} 100Meg")
        spice.append(f"B_{{ref}}_{low} {{node:VSS}} {{node:{name}}} "
                     f"I=V({{ext:{low}_i}})-V({{ext:{low}_g}})*(V({{node:{name}}})-V({{node:VSS}}))")
    return {
        "type": "stm32f407vet6_lqfp100",
        "name": "STM32F407VET6 (LQFP-100, experimental)",
        "category": "Microcontrollers",
        "prefix": "U",
        "description": "STMicroelectronics STM32F407VET6 in LQFP-100 (experimental until compared with a real chip): "
                       "a Cortex-M4 with FPU, 512 KB flash, 192 KB RAM, up to 168 MHz. Runs an ELF file from "
                       "STM32CubeIDE in the SimNodus emulator. So far: clocks, GPIO, external interrupts and the six "
                       "serial ports; timers, ADC, DAC, SPI, I2C, DMA and RTC are not simulated yet. A Crystal "
                       "between PH0 and PH1 is the HSE (none: the HSE never starts); tie BOOT0 to ground and wire "
                       "the VDD and VSS pins (3.3 V)",
        "symbol": {"graphics": graphics, "pins": pins},
        "designator_offset": [-2, top - 4],
        "value_offset": [-2, -top + 2],
        "parameters": [
            {"name": "firmware", "label": "Firmware", "kind": "file", "filter": "*.elf", "default": "",
             "description": "ELF file built for the STM32F407VE (for example with STM32CubeIDE); stored relative to "
                            "the project"},
        ],
        "mcu": {
            "chip": "stm32f407",
            "firmware_parameter": "firmware",
            "ground_pin": "VSS",
            # The Blue Pill's measured thresholds and drive stand in until the
            # STM32F407's pins are compared with a real chip (SN-296): the
            # datasheet gives VIL 0.3 VDD and VIH 0.7 VDD as limits, and about
            # 40 kOhm for the pull resistors.
            "pin_model": {"supply": 3.3, "output_resistance": 40, "pull_resistance": 40000,
                          "input_high": 1.83, "input_low": 1.16},
            "uarts": [
                {"name": "usart1", "label": "USART1 (PA9 or PB6 TX, PA10 or PB7 RX)", "tx_pin": "PA9", "rx_pin": "PA10"},
                {"name": "usart2", "label": "USART2 (PA2 or PD5 TX, PA3 or PD6 RX)", "tx_pin": "PA2", "rx_pin": "PA3"},
                {"name": "usart3", "label": "USART3 (PB10, PC10 or PD8 TX)", "tx_pin": "PB10", "rx_pin": "PB11"},
                {"name": "uart4", "label": "UART4 (PA0 or PC10 TX, PA1 or PC11 RX)", "tx_pin": "PC10", "rx_pin": "PC11"},
                {"name": "uart5", "label": "UART5 (PC12 TX, PD2 RX)", "tx_pin": "PC12", "rx_pin": "PD2"},
                {"name": "usart6", "label": "USART6 (PC6 TX, PC7 RX)", "tx_pin": "PC6", "rx_pin": "PC7"},
            ],
        },
        "spice": spice,
    }


def main():
    path = os.path.join(ROOT, "resources", "library", "mcu", "stm32f407vet6_lqfp100.json")
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        json.dump(chip(), out, indent=2, ensure_ascii=False)
        out.write("\n")
    print("wrote", os.path.relpath(path, ROOT))


if __name__ == "__main__":
    main()
