"""Writes the digital devices of resources/library/devices (SN-203, ADR 0102).

These chips work together with a microcontroller board: their logic runs in
the board's runner, in step with the firmware (src/devices), and the nets
they share with the board are resolved there at once. The SPICE template only
gives each output pin a driver controlled by the runner, like a board pin
({ext:<pin>_g} conductance and {ext:<pin>_i} current, against the GND pin),
and every signal pin a 100 MOhm path to GND so no node floats. The supply pin
must be wired: the outputs drive to its voltage, and without it the chip does
nothing.

- 74HC595: 8-bit shift register with output latches (NXP pin names: DS, SHCP,
  STCP, MR, OE, Q0-Q7, Q7S).
- 24C02: 256-byte I2C EEPROM (SCL, SDA, A0-A2, WP).
- LM75: I2C temperature sensor (SCL, SDA, A0-A2, OS); the temperature is a
  parameter.
- PCF8574: I2C 8-bit quasi-bidirectional I/O expander (SCL, SDA, A0-A2, INT,
  P0-P7); its pins read the circuit (buttons), so they work as inputs too.
- HD44780 character LCD (SN-206): the module's 16 pins on top as on a real
  module (VSS VDD V0 RS RW E D0-D7 A K), rows and columns as parameters; the
  editor draws its text in the screen area; A and K are the backlight LED.
- ROM and RAM (SN-212): generic parallel memories of 16 to 64K bytes (A0-A15,
  D0-D7, CE, OE and the RAM's WE, active low); the size is a parameter and the
  address pins beyond it are ignored; contents from a .bin or Intel .hex file.
  Category *Memories*.
- BMP280, MPU6050, DS3231, TCS34725 and the SSD1306 OLED (SN-223): I2C
  sensors, clock and display with their registers; measured values are live
  parameters; the OLED's pixels appear on the schematic.
- MCP3208, MCP41010, W25Q32, microSD card and HX711 (SN-224): SPI chips and
  the load cell amplifier; the converters read the circuit's voltages
  (device.analog_pins), the digital potentiometer sets a resistance through
  an output inside the part (device.internal_pins).
- DS18B20, DHT22/DHT11, HC-SR04 and the WS2812B strip (SN-225): sensors
  and LEDs with timed one-wire protocols, decoded from the edge times.
- Hobby servo (SN-226): its horn follows the pulse width on SIG and is drawn
  on the schematic.
- Serial Port (SN-237): a serial port of the computer (a USB-serial adapter)
  on RX, TX and GND at TTL levels; the level (3.3 or 5 V) is a parameter
  instead of a supply pin (device.supply_parameter), the port a text the
  editor offers from the computer's list. Category *External I/O*.
- MAX7219: 8-digit LED display driver (DIN, CLK, LOAD, DOUT, DIG0-DIG7, SEGA-SEGG,
  SEGDP); its segment outputs source about 40 mA at 5 V like the chip with its
  current-setting resistor, so the LEDs need no series resistors.

Run from the repository root: python tools/generate_device_parts.py
"""
import json
import os

FOLDER = "resources/library/devices"
CATEGORY = "Chips for microcontrollers"


def ic_symbol(title, left, right, top, bottom, inverted=(), optional=()):
    rows = max(len(left), len(right))
    height = 2 * rows + 2
    upper = -(height // 2)
    graphics = [{"rect": [-4, upper, 8, height]}, {"text": title, "at": [0, upper + 1.2], "size": 0.8}]
    pins = []
    for names, x_tip, x_edge, direction in ((left, -6, -4, "left"), (right, 6, 4, "right")):
        first = -(len(names) - 1)
        for i, name in enumerate(names):
            y = first + 2 * i
            graphics.append({"line": [x_tip, y, x_edge, y]})
            graphics.append({"text": name, "at": [x_edge + (1.3 if x_edge < 0 else -1.2), y], "size": 0.6})
            if name in inverted:  # active low: a bubble at the body
                cx = x_edge - 0.3 if x_edge < 0 else x_edge + 0.3
                graphics.append({"circle": [cx, y, 0.3]})
            pin = {"name": name, "at": [x_tip, y], "dir": direction}
            if name in optional:
                pin["optional"] = True
            pins.append(pin)
    graphics += [{"line": [0, upper - 2, 0, upper]}, {"text": top, "at": [0, upper + 0.5], "size": 0.55},
                 {"line": [0, -upper, 0, -upper + 2]}, {"text": bottom, "at": [0, -upper - 0.5], "size": 0.55}]
    pins += [{"name": top, "at": [0, upper - 2], "dir": "up"}, {"name": bottom, "at": [0, -upper + 2], "dir": "down"}]
    return {"graphics": graphics, "pins": pins}, upper


def device_spice(signals, outputs, supply="VCC", ground="GND"):
    spice = [f"R_{{ref}}_h{p.lower()} {{node:{p}}} {{node:{ground}}} 100Meg" for p in signals]
    for p in outputs:
        n = p.lower()
        spice.append(f"B_{{ref}}_{n} {{node:{ground}}} {{node:{p}}} "
                     f"I=V({{ext:{n}_i}})-V({{ext:{n}_g}})*(V({{node:{p}}})-V({{node:{ground}}}))")
    spice.append(f"R_{{ref}}_q {{node:{supply}}} {{node:{ground}}} 100k")  # quiescent current
    return spice


def part(type_, name, description, symbol, upper, kind, spice, output_resistance=50, parameters=None, driven=(),
         category=CATEGORY):
    result = {
        "type": type_,
        "name": name,
        "category": category,
        "prefix": "U",
        "description": description,
        "symbol": symbol,
        "designator_offset": [5, upper - 1],
        "value_offset": [5, -upper + 1],
        "device": {"kind": kind, "supply_pin": "VCC", "ground_pin": "GND", "output_resistance": output_resistance},
        "spice": spice,
    }
    if driven:
        result["device"]["driven_pins"] = list(driven)  # push-pull outputs that always drive
    if parameters:
        result["parameters"] = parameters
        result["value_parameter"] = parameters[0]["name"]
    return result


def hc595():
    inputs = ["DS", "SHCP", "STCP", "MR", "OE"]
    outputs = [f"Q{i}" for i in range(8)] + ["Q7S"]
    symbol, upper = ic_symbol("74HC595", inputs, outputs, "VCC", "GND", inverted=("MR", "OE"),
                              optional=tuple(outputs))
    return part("74hc595", "Shift register 74HC595",
                "8-bit serial-in shift register with output latches (74HC595, NXP pin names): DS shifts in at each "
                "rising edge of SHCP (Q0 first), a rising edge of STCP copies the shift register to Q0-Q7, MR low "
                "clears the shift register, OE high turns Q0-Q7 off (high impedance); Q7S, the last stage, feeds the "
                "DS of a second 74HC595. Tie MR to VCC and OE to GND for normal use; supply 2-6 V on VCC and GND. "
                "Works together with a microcontroller board.",
                symbol, upper, "74hc595", device_spice(inputs + outputs, outputs), driven=["Q7S"])


I2C_NOTE = ("SDA is open drain: SCL and SDA need pull-up resistors (4.7 kOhm to the supply), as on a real board; "
            "A0-A2 set the low address bits (tie them to GND or VCC).")


def eeprom_24c02():
    inputs = ["SCL", "A0", "A1", "A2", "WP"]
    symbol, upper = ic_symbol("24C02", ["SCL", "SDA", "A0", "A1", "A2"], ["WP"], "VCC", "GND")
    p = part("24c02", "EEPROM 24C02 (I2C)",
             "2 Kbit (256-byte) serial EEPROM like the AT24C02 at I2C address 0x50 + A2 A1 A0: write a word address "
             "and up to 8 bytes of one page, stored at STOP in 5 ms (the chip does not acknowledge its address "
             "meanwhile); reads start at the word address and continue through the memory; WP high protects it. "
             "Starts erased (0xFF) or with its contents file. " + I2C_NOTE + " Supply 1.8-5.5 V. Works together with "
             "a microcontroller board.",
             symbol, upper, "24c02", device_spice(inputs + ["SDA"], ["SDA"]),
             parameters=[{"name": "contents", "label": "Contents file", "kind": "file", "filter": "*.bin *.hex",
                          "default": "", "description": "Contents the EEPROM starts each run with (.bin or Intel "
                                                        ".hex, stored relative to the project); empty: erased"}])
    p["device"].update({"memory_size": 256, "contents_parameter": "contents"})
    del p["value_parameter"]  # the file name would crowd the symbol
    return p


def lm75():
    inputs = ["SCL", "A0", "A1", "A2"]
    symbol, upper = ic_symbol("LM75", ["SCL", "SDA", "A0", "A1", "A2"], ["OS"], "VCC", "GND", optional=("OS",))
    return part("lm75", "Temperature sensor LM75 (I2C)",
                "Digital temperature sensor (LM75) at I2C address 0x48 + A2 A1 A0: register 0 reads the temperature as "
                "two bytes (whole degrees, then bit 7 for half a degree), 1 is the configuration, 2 and 3 are THYST "
                "(75 C) and TOS (80 C); the open-drain OS output goes low above TOS and is released again below THYST. "
                + I2C_NOTE + " Supply 2.8-5.5 V. Works together with a microcontroller board.",
                symbol, upper, "lm75", device_spice(inputs + ["SDA", "OS"], ["SDA", "OS"]),
                parameters=[{"name": "temperature", "label": "Temperature", "unit": "°C", "default": "25",
                             "min": "-55", "max": "125", "live": True,
                             "description": "Temperature the sensor measures; change it while running to test the "
                                            "circuit"}])


def pcf8574():
    ports = [f"P{i}" for i in range(8)]
    inputs = ["SCL", "A0", "A1", "A2"]
    symbol, upper = ic_symbol("PCF8574", ["SCL", "SDA", "A0", "A1", "A2", "INT"], ports, "VCC", "GND",
                              inverted=("INT",), optional=tuple(ports) + ("INT",))
    return part("pcf8574", "I/O expander PCF8574 (I2C)",
                "8-bit quasi-bidirectional I/O expander (PCF8574) at I2C address 0x20 + A2 A1 A0 (PCF8574A: 0x38): a "
                "written byte sets P0-P7, a 0 pulls the pin low (an LED from VCC lights), a 1 leaves it high through "
                "a weak pull-up so it can be read as an input (a push button to GND); a read returns the pin levels; "
                "INT (open drain) goes low when an input changes. Drives an HD44780 LCD as on the common I2C LCD "
                "backpacks. " + I2C_NOTE + " Supply 2.5-6 V. Works together with a microcontroller board.",
                symbol, upper, "pcf8574", device_spice(inputs + ["SDA", "INT"] + ports, ["SDA", "INT"] + ports),
                parameters=[{"name": "variant", "label": "Variant", "unit": "", "default": "PCF8574",
                             "choices": ["PCF8574", "PCF8574A"],
                             "description": "PCF8574 answers at 0x20-0x27, PCF8574A at 0x38-0x3F"}])


def lcd_hd44780():
    names = ["VSS", "VDD", "V0", "RS", "RW", "E"] + [f"D{i}" for i in range(8)] + ["A", "K"]
    graphics = [{"rect": [-17, -3, 34, 14]}]
    pins = []
    for i, name in enumerate(names):
        x = -15 + 2 * i
        pin = {"name": name, "at": [x, -5], "dir": "up"}
        if name in ("V0", "A", "K") or name in [f"D{k}" for k in range(4)]:
            pin["optional"] = True  # contrast, backlight, and D0-D3 in 4-bit mode
        pins.append(pin)
        graphics.append({"line": [x, -5, x, -3]})
        graphics.append({"text": name, "at": [x, -2.2], "size": 0.45})
    outputs = [f"D{i}" for i in range(8)]
    spice = device_spice(["V0", "RS", "RW", "E"] + outputs, outputs, supply="VDD", ground="VSS")
    # Backlight: a white LED with its series resistor between A and K.
    spice += ["R_{ref}_bl {node:A} bl_{ref} 47", "D_{ref}_bl bl_{ref} {node:K} LCDBL"]
    p = part("lcd_hd44780", "Character LCD (HD44780)",
             "Character LCD module with an HD44780 controller, 1, 2 or 4 rows of 8 to 40 characters (set in its "
             "properties): RS, RW and E with D0-D7 (8-bit) or D4-D7 (4-bit) from a microcontroller, or from a "
             "PCF8574 as on the common I2C backpacks; supply 5 V or 3.3 V on VDD and VSS; A and K are the backlight. "
             "The text appears on the schematic while running. Works together with a microcontroller board.",
             {"graphics": graphics, "pins": pins}, -3, "hd44780", spice,
             parameters=[{"name": "rows", "label": "Rows", "unit": "", "default": "2", "choices": ["1", "2", "4"],
                          "description": "Rows of characters"},
                         {"name": "columns", "label": "Columns", "unit": "", "default": "16",
                          "choices": ["8", "16", "20", "40"], "description": "Characters per row"}])
    p["category"] = "Displays"
    p["device"].update({"supply_pin": "VDD", "ground_pin": "VSS"})
    p["screen"] = {"rect": [-14, 0, 28, 8], "rows_parameter": "rows", "columns_parameter": "columns"}
    p["models"] = [".model LCDBL D(IS=6.7e-28 N=2 RS=8)"]
    p["designator_offset"] = [-17, -8]
    p["value_offset"] = [10, -8]
    del p["value_parameter"]  # rows and columns show on the screen itself
    return p


def max7219():
    inputs = ["DIN", "CLK", "LOAD"]
    digits = [f"DIG{i}" for i in range(8)]
    segments = [f"SEG{s}" for s in "ABCDEFG"] + ["SEGDP"]
    outputs = ["DOUT"] + digits + segments
    symbol, upper = ic_symbol("MAX7219", inputs + ["DOUT"], digits + segments, "VCC", "GND",
                              optional=tuple(outputs))
    return part("max7219", "LED display driver MAX7219",
                "Serially interfaced 8-digit LED display driver (MAX7219): 16-bit words shift in on DIN at rising CLK "
                "(MSB first: 4 address bits, 8 data bits) and a rising LOAD stores them (registers 1-8 digits, 9 "
                "decode mode, 10 intensity, 11 scan limit, 12 shutdown, 15 display test). It scans the digits about "
                "800 times a second: the active DIG pin sinks the digit's current, SEG A-G and DP source the segment "
                "current (about 40 mA at 5 V, no series resistors needed). Drives eight common-cathode digits or an "
                "8x8 LED matrix (SEG to the anodes, DIG to the cathodes); DOUT feeds the DIN of a second MAX7219. "
                "Starts in shutdown: write 1 to register 12. Supply 4-5.5 V on VCC and GND. Works together with a "
                "microcontroller board.",
                symbol, upper, "max7219", device_spice(inputs + outputs, outputs), output_resistance=75,
                driven=outputs)


def live(name, label, unit, default, minimum, maximum, description):
    return {"name": name, "label": label, "unit": unit, "default": default, "min": minimum, "max": maximum,
            "live": True, "description": description + "; change it while running to test the circuit"}


MODULE_NOTE = ("SDA is open drain: SCL and SDA need pull-up resistors (4.7 kOhm to the supply; breakout modules "
               "usually have them, here they are wired in the circuit). Supply 3.3 or 5 V on VCC and GND (a module "
               "with its regulator). Works together with a microcontroller board.")


def bmp280():
    symbol, upper = ic_symbol("BMP280", ["SCL", "SDA", "SDO", "CSB"], [], "VCC", "GND", optional=("SDO", "CSB"))
    return part("bmp280", "Pressure sensor BMP280 (I2C)",
                "Barometric pressure and temperature sensor (Bosch BMP280) at I2C address 0x76, or 0x77 with SDO high: "
                "chip ID 0x58, calibration words, forced and normal modes; the raw values give the set temperature "
                "and pressure through the datasheet's compensation, as the usual libraries compute it. CSB is not "
                "used (I2C only). " + MODULE_NOTE,
                symbol, upper, "bmp280", device_spice(["SCL", "SDA", "SDO", "CSB"], ["SDA"]),
                parameters=[live("pressure", "Pressure", "hPa", "1013.25", "300", "1100",
                                 "Air pressure (1013.25 hPa at sea level, about 12 hPa less per 100 m up)"),
                            live("temperature", "Temperature", "°C", "25", "-40", "85", "Temperature of the sensor")])


def mpu6050():
    symbol, upper = ic_symbol("MPU6050", ["SCL", "SDA", "AD0", "INT"], [], "VCC", "GND", optional=("AD0", "INT"))
    axes = []
    for axis in "xyz":
        axes.append(live(f"accel_{axis}", f"Acceleration {axis.upper()}", "g", "1" if axis == "z" else "0", "-16",
                         "16", f"Acceleration along {axis.upper()} (lying flat: Z = 1 g)"))
    for axis in "xyz":
        axes.append(live(f"gyro_{axis}", f"Rotation {axis.upper()}", "°/s", "0", "-2000", "2000",
                         f"Rotation rate about {axis.upper()}"))
    axes.append(live("temperature", "Temperature", "°C", "25", "-40", "85", "Temperature of the chip"))
    p = part("mpu6050", "Accelerometer and gyroscope MPU6050 (I2C)",
             "6-axis motion sensor (InvenSense MPU-6050) at I2C address 0x68, or 0x69 with AD0 high: WHO_AM_I 0x68, "
             "starts asleep (clear SLEEP in PWR_MGMT_1), accelerations and rotation rates scaled by the full-scale "
             "ranges of ACCEL_CONFIG and GYRO_CONFIG, and the temperature. INT, the FIFO and the motion processor "
             "are not simulated. " + MODULE_NOTE,
             symbol, upper, "mpu6050", device_spice(["SCL", "SDA", "AD0", "INT"], ["SDA"]), parameters=axes)
    p["value_parameter"] = "accel_z"
    return p


def ds3231():
    symbol, upper = ic_symbol("DS3231", ["SCL", "SDA"], ["SQW", "32K"], "VCC", "GND", optional=("SQW", "32K"))
    return part("ds3231", "Real-time clock DS3231 (I2C)",
                "Real-time clock (DS3231) at I2C address 0x68: seconds to year in BCD (24- or 12-hour mode), two "
                "alarms, control and status registers and the temperature. The clock runs with the simulation from "
                "the start time. SQW (open drain, needs a pull-up) gives a 1 Hz to 8.192 kHz square wave while INTCN "
                "is 0, or goes low on an enabled alarm while INTCN is 1. 32K is not driven. " + MODULE_NOTE,
                symbol, upper, "ds3231", device_spice(["SCL", "SDA", "SQW", "32K"], ["SDA", "SQW"]),
                parameters=[{"name": "start", "label": "Start time", "kind": "text", "default": "now",
                             "description": "Date and time when the run starts: now (the computer's clock) or "
                                            "YYYY-MM-DD HH:MM:SS"},
                            live("temperature", "Temperature", "°C", "25", "-40", "85", "Temperature of the chip")])


def tcs34725():
    symbol, upper = ic_symbol("TCS34725", ["SCL", "SDA", "INT", "LED"], [], "VCC", "GND", optional=("INT", "LED"))
    color = [live(c, c.capitalize(), "", "255", "0", "255", f"{c.capitalize()} part of the color in front of the "
                  "sensor (0-255)") for c in ("red", "green", "blue")]
    color.append(live("light", "Light", "lux", "300", "0", "10000", "Light on the object"))
    p = part("tcs34725", "Color sensor TCS34725 (I2C)",
             "RGB color sensor (ams TCS34725) at I2C address 0x29: command byte with auto-increment, ID 0x44, ENABLE "
             "(PON, AEN), integration time (ATIME) and gain (CONTROL); clear, red, green and blue counts follow the "
             "color and the light set in its properties. INT and the module's LED are not simulated. " + MODULE_NOTE,
             symbol, upper, "tcs34725", device_spice(["SCL", "SDA", "INT", "LED"], ["SDA"]), parameters=color)
    p["value_parameter"] = "light"
    return p


def ssd1306():
    names = ["GND", "VCC", "SCL", "SDA"]
    graphics = [{"rect": [-10, -6, 20, 14]}, {"text": "SSD1306 OLED", "at": [0, 6.8], "size": 0.6}]
    pins = []
    for i, name in enumerate(names):
        x = -6 + 4 * i
        pins.append({"name": name, "at": [x, -8], "dir": "up"})
        graphics.append({"line": [x, -8, x, -6]})
        graphics.append({"text": name, "at": [x, -5.2], "size": 0.45})
    p = part("ssd1306", "OLED display SSD1306 (I2C)",
             "Monochrome OLED module, 128x64 or 128x32 pixels, with an SSD1306 controller at I2C address 0x3C (or "
             "0x3D): commands and display data as from the Adafruit or u8g2 libraries (addressing modes, page and "
             "column windows, segment remap, COM scan, inverse, display on/off). The picture appears on the "
             "schematic while running. " + MODULE_NOTE,
             {"graphics": graphics, "pins": pins}, -6, "ssd1306", device_spice(["SCL", "SDA"], ["SDA"]),
             parameters=[{"name": "height", "label": "Height", "unit": "", "default": "64", "choices": ["64", "32"],
                          "description": "Pixel rows: 128x64 (0.96 inch) or 128x32 (0.91 inch)"},
                         {"name": "address", "label": "I2C address", "unit": "", "default": "0x3C",
                          "choices": ["0x3C", "0x3D"], "description": "Address set on the module"},
                         {"name": "color", "label": "Color", "unit": "", "default": "white",
                          "choices": ["white", "blue", "yellow-blue"],
                          "description": "Color of the pixels (yellow-blue: the top 16 rows yellow)"}])
    p["category"] = "Displays"
    p["screen"] = {"rect": [-8, -4, 16, 8], "pixels": True, "color_parameter": "color"}
    p["designator_offset"] = [-10, -8]
    p["value_offset"] = [6, -8]
    del p["value_parameter"]
    return p


SPI_NOTE = ("SPI modes 0 and 3 (data in at rising SCK, out at falling SCK); the data output floats while CS is high. "
            "Works together with a microcontroller board.")


def mcp3208():
    channels = [f"CH{i}" for i in range(8)]
    right = ["CS", "CLK", "DIN", "DOUT", "VREF", "AGND"]
    symbol, upper = ic_symbol("MCP3208", channels, right, "VDD", "DGND", inverted=("CS",),
                              optional=tuple(channels) + ("AGND",))
    p = part("mcp3208", "A/D converter MCP3208 (SPI)",
             "8-channel 12-bit A/D converter (Microchip MCP3208) on SPI: with CS low, a start bit, SGL/DIFF and three "
             "channel bits on DIN choose a single-ended input (CHn against AGND) or a differential pair; DOUT then "
             "gives a null bit and the 12-bit result 4096 x VIN / VREF, MSB first. The inputs and VREF are read from "
             "the circuit. Supply 2.7-5.5 V on VDD and DGND; AGND belongs to ground. " + SPI_NOTE,
             symbol, upper, "mcp3208", device_spice(channels + right, ["DOUT"], supply="VDD", ground="DGND"))
    p["device"].update({"supply_pin": "VDD", "ground_pin": "DGND", "analog_pins": channels + ["VREF"]})
    return p


def mcp41010():
    symbol, upper = ic_symbol("MCP41010", ["CS", "SCK", "SI"], ["PA0", "PW0", "PB0"], "VDD", "VSS",
                              inverted=("CS",))
    wiper = "50*V({ext:wiper_g})"  # the device's output strength / output resistance (50 Ohm)
    spice = device_spice(["CS", "SCK", "SI"], [], supply="VDD", ground="VSS") + [
        f"B_{{ref}}_a {{node:PA0}} {{node:PW0}} I=V({{node:PA0}},{{node:PW0}})/({{R}}*max(1-{wiper},0.001)+52)",
        f"B_{{ref}}_b {{node:PW0}} {{node:PB0}} I=V({{node:PW0}},{{node:PB0}})/({{R}}*max({wiper},0.001)+52)",
    ]
    p = part("mcp41010", "Digital potentiometer MCP41010 (SPI)",
             "Digital potentiometer (Microchip MCP41010/MCP41050/MCP41100) on SPI: a command byte (0x11 writes pot 0, "
             "0x21 shuts it down) and a data byte D taken when CS rises after 16 clocks put the wiper PW0 at D/256 "
             "of the track from PB0 towards PA0 (52 ohm wiper resistance). Starts at mid-scale. Supply 2.7-5.5 V on "
             "VDD and VSS; keep PA0, PW0 and PB0 between VSS and VDD. " + SPI_NOTE,
             symbol, upper, "mcp41010", spice,
             parameters=[{"name": "R", "label": "Resistance", "unit": "Ohm", "default": "10k",
                          "choices": ["10k", "50k", "100k"],
                          "description": "Track resistance from PA0 to PB0: MCP41010, MCP41050 or MCP41100"}])
    p["device"].update({"supply_pin": "VDD", "ground_pin": "VSS", "internal_pins": ["WIPER"]})
    return p


def w25q32():
    symbol, upper = ic_symbol("W25Q32", ["CS", "CLK", "DI", "DO"], ["WP", "HOLD"], "VCC", "GND", inverted=("CS",),
                              optional=("WP", "HOLD"))
    p = part("w25q32", "SPI flash W25Q32",
             "32 Mbit (4 MB) SPI NOR flash (Winbond W25Q32): read (03, 0B), write enable (06), page program (02, up "
             "to 256 bytes, only clearing bits), erase of 4 KB, 32 KB, 64 KB or the whole chip (20, 52, D8, C7), "
             "status (05: BUSY, WEL), JEDEC ID (9F: EF 40 16), power-down; programming and erasing keep BUSY for the "
             "datasheet's typical times (0.7 ms a page, 45 ms a sector, 10 s the chip). Starts erased or with its "
             "contents file; the Memory Viewer shows it. WP and HOLD are not simulated. Supply 2.7-3.6 V. " + SPI_NOTE,
             symbol, upper, "w25q32", device_spice(["CS", "CLK", "DI", "DO", "WP", "HOLD"], ["DO"]),
             parameters=[{"name": "contents", "label": "Contents file", "kind": "file", "filter": "*.bin *.hex",
                          "default": "", "description": "Contents the flash starts each run with (.bin or Intel "
                                                        ".hex, stored relative to the project); empty: erased"}])
    p["device"].update({"memory_size": 4194304, "contents_parameter": "contents"})
    del p["value_parameter"]
    return p


def sdcard():
    names = ["CS", "SCK", "MOSI", "MISO", "VCC", "GND"]
    graphics = [{"rect": [-6, -6, 12, 12]}, {"text": "microSD", "at": [0, 2], "size": 0.8},
                {"polygon": [-3, -3, 2, -3, 3, -2, 3, 0, -3, 0]}]
    pins = []
    for i, name in enumerate(names):
        y = -5 + 2 * i
        pins.append({"name": name, "at": [-8, y], "dir": "left"})
        graphics.append({"line": [-8, y, -6, y]})
        graphics.append({"text": name, "at": [-4.6, y], "size": 0.5})
    p = part("sdcard", "microSD card (SPI)",
             "microSD card in SPI mode, as on the common card modules: CMD0, CMD8, ACMD41, CMD58, CMD9/10, CMD16, "
             "block reads (CMD17, CMD18 with CMD12) and writes (CMD24, CMD25) of 512 bytes with block addresses "
             "(SDHC), as the FatFs drivers use them; CRCs are not checked. The card is as large as its size "
             "property and starts empty or with an image file (format it with FatFs f_mkfs, or load a FAT image); "
             "writes stay in the simulation, and the Memory Viewer can save them. " + SPI_NOTE,
             {"graphics": graphics, "pins": pins}, -6, "sdcard", device_spice(["CS", "SCK", "MOSI", "MISO"], ["MISO"]),
             parameters=[{"name": "size", "label": "Size (bytes)", "unit": "", "default": "8M",
                          "choices": ["1M", "2M", "4M", "8M", "16M", "32M", "64M"],
                          "description": "Capacity of the card"},
                         {"name": "contents", "label": "Image file", "kind": "file", "filter": "*.img *.bin",
                          "default": "", "description": "Card image the run starts with (raw blocks, stored relative "
                                                        "to the project); empty: all zeros"}])
    p["device"].update({"memory_size_parameter": "size", "memory_fill": 0, "contents_parameter": "contents"})
    p["designator_offset"] = [-6, -8]
    p["value_offset"] = [-6, 7]
    return p


def hx711():
    symbol, upper = ic_symbol("HX711", ["E+", "E-", "A-", "A+", "B-", "B+"], ["DT", "SCK"], "VCC", "GND",
                              optional=("B-", "B+"))
    spice = device_spice(["A-", "A+", "B-", "B+", "DT", "SCK"], ["DT"]) + [
        "R_{ref}_ep {node:VCC} {node:E+} 2",  # excitation for the bridge from the supply
        "R_{ref}_en {node:E-} {node:GND} 1",
    ]
    p = part("hx711", "Load cell amplifier HX711",
             "24-bit A/D converter for load cells (Avia HX711 module): E+ and E- excite the bridge, A+ and A- (gain "
             "128 or 64) or B+ and B- (gain 32) take its output. DT goes low when a conversion is ready (10 or 80 "
             "per second); 24 pulses on SCK shift the result out MSB first, 1-3 more choose the next input and "
             "gain; SCK high for over 60 us powers it down. The code is (A+ - A-) x gain / (E+ / 2) x 2^23. Supply "
             "2.6-5.5 V. Works together with a microcontroller board.",
             symbol, upper, "hx711", spice, driven=["DT"],
             parameters=[{"name": "rate", "label": "Rate", "unit": "", "default": "10", "choices": ["10", "80"],
                          "description": "Conversions per second (the module's RATE pin)"}])
    p["device"]["analog_pins"] = ["E+", "A+", "A-", "B+", "B-"]
    return p


ONE_WIRE_NOTE = ("The data pin is open drain: it needs a pull-up resistor (4.7 kOhm to the supply). Works together with a "
                 "microcontroller board, whose firmware makes the timed pulses (microsecond delays).")


def ds18b20():
    symbol, upper = ic_symbol("DS18B20", ["DQ"], [], "VDD", "GND")
    p = part("ds18b20", "Temperature sensor DS18B20 (1-Wire)",
             "Digital thermometer on a 1-Wire bus (Maxim DS18B20): reset and presence pulses, the ROM commands "
             "(read, match, skip, search, alarm search) and the function commands (convert, read and write "
             "scratchpad, copy, recall, power supply), 9 to 12 bits with conversions of 94 to 750 ms; it reads +85 C "
             "until the first conversion. Each sensor has its own ROM code (from the serial parameter or its "
             "designator), so several share a bus. Supply 3-5.5 V on VDD (not parasitic power). " + ONE_WIRE_NOTE,
             symbol, upper, "ds18b20", device_spice(["DQ"], ["DQ"], supply="VDD"),
             parameters=[live("temperature", "Temperature", "°C", "25", "-55", "125", "Temperature of the sensor"),
                         {"name": "serial", "label": "Serial number", "kind": "text", "default": "auto",
                          "description": "48-bit serial number in hex (auto: from the designator)"}])
    p["device"].update({"supply_pin": "VDD"})
    return p


def dht22():
    symbol, upper = ic_symbol("DHT22", ["DATA"], [], "VCC", "GND")
    return part("dht22", "Humidity sensor DHT22 / DHT11",
                "Humidity and temperature sensor on one wire (Aosong DHT22/AM2302 or DHT11): the microcontroller pulls "
                "DATA low for at least 1 ms (18 ms for the DHT11) and releases it; the sensor answers with 80 us low "
                "and 80 us high, then 40 bits of 50 us low and 26 us (0) or 70 us (1) high: humidity, temperature "
                "and a checksum. Supply 3.3-5.5 V. " + ONE_WIRE_NOTE,
                symbol, upper, "dht22", device_spice(["DATA"], ["DATA"]),
                parameters=[{"name": "model", "label": "Model", "unit": "", "default": "DHT22",
                             "choices": ["DHT22", "DHT11"],
                             "description": "DHT22/AM2302 (tenths, -40 to 80 C) or DHT11 (0 to 50 C)"},
                            live("temperature", "Temperature", "°C", "25", "-40", "80", "Air temperature"),
                            live("humidity", "Humidity", "%", "50", "0", "100", "Relative humidity of the air")])


def hcsr04():
    symbol, upper = ic_symbol("HC-SR04", ["TRIG", "ECHO"], [], "VCC", "GND")
    p = part("hcsr04", "Ultrasonic sensor HC-SR04",
             "Ultrasonic distance module: a pulse of at least 10 us on TRIG starts a measurement; about 0.25 ms later "
             "ECHO goes high for the round trip of the sound, 58 us per centimetre of the distance set in its "
             "properties (2 to 400 cm; beyond that 38 ms, no echo). Supply 5 V; ECHO is high at the supply voltage "
             "(a 5 V-tolerant pin or a divider on a 3.3 V board). Works together with a microcontroller board.",
             symbol, upper, "hcsr04", device_spice(["TRIG", "ECHO"], ["ECHO"]), driven=["ECHO"],
             parameters=[live("distance", "Distance", "cm", "100", "2", "450", "Distance to the obstacle")])
    return p


def serial_port():
    graphics = [{"rect": [-4, -3, 8, 6]}, {"text": "SERIAL", "at": [0, -1.9], "size": 0.7},
                {"text": "PORT", "at": [0, -1.0], "size": 0.7}]
    pins = []
    for name, y in (("RX", 0), ("TX", 2)):
        graphics += [{"line": [-6, y, -4, y]}, {"text": name, "at": [-2.8, y], "size": 0.6}]
        pins.append({"name": name, "at": [-6, y], "dir": "left"})
    graphics += [{"line": [0, 3, 0, 5]}, {"text": "GND", "at": [0, 2.5], "size": 0.55}]
    pins.append({"name": "GND", "at": [0, 5], "dir": "down"})
    spice = device_spice(["RX", "TX"], ["TX"])[:-1]  # no supply pin, so no quiescent current
    p = part("serial_port", "Serial Port (computer)",
             "A serial port of this computer in the circuit, like a USB-serial adapter: bytes the circuit sends to "
             "RX go out of the port, bytes arriving at the port come out on TX. Connect TX to the board's RX, RX to "
             "its TX and GND to ground. Choose the port, baud rate and format in its properties; the levels are TTL "
             "(3.3 or 5 V, idle high). The port opens when the simulation starts; run at real-time speed so that "
             "the timing matches the other program. 'loopback' sends back what it receives.",
             {"graphics": graphics, "pins": pins}, -3, "serial_port", spice, driven=["TX"],
             parameters=[
                 {"name": "port", "label": "Port", "kind": "text", "serial_port": True, "default": "loopback",
                  "description": "The computer's serial port (COM3, /dev/ttyUSB0); loopback sends back what it "
                                 "receives"},
                 {"name": "baud", "label": "Baud rate", "choices": ["300", "1200", "2400", "4800", "9600", "19200",
                                                                    "38400", "57600", "115200", "230400", "460800",
                                                                    "921600"],
                  "default": "115200", "description": "Bits per second, the same on the port and on the circuit"},
                 {"name": "data_bits", "label": "Data bits", "choices": ["5", "6", "7", "8"], "default": "8",
                  "description": "Data bits per frame"},
                 {"name": "parity", "label": "Parity", "choices": ["None", "Even", "Odd"], "default": "None",
                  "description": "Parity bit after the data bits"},
                 {"name": "stop_bits", "label": "Stop bits", "choices": ["1", "2"], "default": "1",
                  "description": "Stop bits per frame"},
                 {"name": "level", "label": "Level", "choices": ["3.3 V", "5 V"], "default": "3.3 V",
                  "description": "High level of TX, like the adapter's 3.3 V or 5 V setting"}],
             category="External I/O")
    p["device"].pop("supply_pin")
    p["device"]["supply_parameter"] = "level"
    p["prefix"] = "J"
    p["value_offset"] = [5, 4]
    p["designator_offset"] = [5, -4]
    return p


def ws2812():
    names = ["DIN", "DOUT", "VDD", "GND"]
    graphics = [{"rect": [-14, -3, 28, 6]}]
    pins = []
    for name, x, y, direction in (("DIN", -16, 0, "left"), ("DOUT", 16, 0, "right"), ("VDD", -10, -5, "up"),
                                  ("GND", -10, 5, "down")):
        pin = {"name": name, "at": [x, y], "dir": direction}
        if name == "DOUT":
            pin["optional"] = True
        pins.append(pin)
    graphics += [{"line": [-16, 0, -14, 0]}, {"text": "DIN", "at": [-12.6, -2.2], "size": 0.45},
                 {"line": [16, 0, 14, 0]}, {"text": "DOUT", "at": [12.4, -2.2], "size": 0.45},
                 {"line": [-10, -5, -10, -3]}, {"text": "VDD", "at": [-8.4, -2.2], "size": 0.45},
                 {"line": [-10, 5, -10, 3]}, {"text": "GND", "at": [-8.4, 2.2], "size": 0.45}]
    del names
    p = part("ws2812", "RGB LED strip WS2812B",
             "Strip of WS2812B addressable RGB LEDs (1 to 60): 24 bits per LED on DIN (green, red, blue, MSB first, "
             "about 800 kHz; a high pulse over 0.6 us is a 1), the first LED takes the first 24 bits; DIN low for "
             "over 50 us shows the colors and starts again; bits beyond the strip go out on DOUT to the next one. "
             "The colors appear on the schematic while running. Supply 5 V on VDD and GND (no current drawn by the "
             "LEDs is simulated). Works together with a microcontroller board, whose firmware makes the 0.4/0.8 us "
             "pulses.",
             {"graphics": graphics, "pins": pins}, -3, "ws2812", device_spice(["DIN", "DOUT"], ["DOUT"], supply="VDD"),
             driven=["DOUT"],
             parameters=[{"name": "count", "label": "LEDs", "unit": "", "default": "8",
                          "choices": ["1", "8", "12", "16", "24", "30", "60"], "description": "Number of LEDs"}])
    p["device"]["supply_pin"] = "VDD"
    p["category"] = "Displays"
    p["screen"] = {"rect": [-13, -2, 26, 4], "pixels": True}
    p["designator_offset"] = [-14, -5]
    p["value_offset"] = [0, -5]
    return p


def servo():
    graphics = [{"rect": [-6, -4, 12, 9]}, {"text": "SERVO", "at": [0, 4], "size": 0.55}]
    pins = []
    for i, name in enumerate(["SIG", "V+", "GND"]):
        y = -2 + 2 * i
        pins.append({"name": name, "at": [-8, y], "dir": "left"})
        graphics.append({"line": [-8, y, -6, y]})
        graphics.append({"text": name, "at": [-4.9, y - 0.5], "size": 0.45})
    p = part("servo", "Servo motor (hobby)",
             "Hobby servo like the SG90 or MG90S: the width of the pulses on SIG (usually 50 per second) sets the "
             "horn's angle, from the minimum pulse (0 degrees) to the maximum (180 degrees); it turns at 600 degrees "
             "a second and holds its angle without pulses. The horn is drawn on the schematic while running. Supply "
             "5 V on V+ and GND (the motor's current is not simulated). Works together with a microcontroller board "
             "(a timer's PWM output).",
             {"graphics": graphics, "pins": pins}, -4, "servo", device_spice(["SIG"], [], supply="V+"),
             parameters=[{"name": "min", "label": "Pulse for 0°", "unit": "us", "default": "500", "min": "300",
                          "max": "1500", "description": "Pulse width for 0 degrees (SG90: 500 us; some servos 1000 us)"},
                         {"name": "max", "label": "Pulse for 180°", "unit": "us", "default": "2500", "min": "1500",
                          "max": "3000", "description": "Pulse width for 180 degrees (SG90: 2500 us; some servos 2000 us)"}])
    p["device"]["supply_pin"] = "V+"
    p["category"] = "Motors & motion"
    p["screen"] = {"rect": [-5, -3, 10, 6], "pixels": True}
    p["designator_offset"] = [-6, -6]
    p["value_offset"] = [-6, 6]
    del p["value_parameter"]
    return p


MEMORY_SIZES = ["16", "32", "64", "128", "256", "512", "1K", "2K", "4K", "8K", "16K", "32K", "64K"]


def memory(ram):
    address = [f"A{i}" for i in range(16)]
    data = [f"D{i}" for i in range(8)]
    control = ["CE", "OE"] + (["WE"] if ram else [])
    title = "RAM" if ram else "ROM"
    symbol, upper = ic_symbol(title, address, data + control, "VCC", "GND", inverted=tuple(control),
                              optional=tuple(address + data))
    what = ("Static RAM: with CE and OE low and WE high it drives D0-D7 with the addressed byte; with CE and WE low "
            "it takes D0-D7 and stores them when WE (or CE) rises. Starts with every byte 0, or with its contents "
            "file." if ram else
            "ROM: with CE and OE low it drives D0-D7 with the addressed byte, otherwise D0-D7 are off (high "
            "impedance). Its contents come from a .bin or Intel .hex file; without one every byte is 0xFF.")
    p = part(title.lower(), f"{title} (parallel)",
             f"Generic parallel {title} of 16 bytes to 64 KB (the size is a property): A0-A15 address a byte, the "
             "address pins beyond the size are ignored. " + what + " Ideal timing: no access or write time. Works "
             "with a microcontroller board's pins or on its own in a circuit; Window > Memory Viewer shows its "
             "contents. Supply on VCC and GND.",
             symbol, upper, title.lower(), device_spice(address + data + control, data),
             parameters=[{"name": "size", "label": "Size (bytes)", "choices": MEMORY_SIZES,
                          "default": "2K" if ram else "256", "description": "Number of 8-bit words; A0 up to the "
                                                                           "address pins the size needs are used"},
                         {"name": "contents", "label": "Contents file", "kind": "file", "filter": "*.bin *.hex",
                          "default": "", "description": "Contents the memory starts each run with (.bin or Intel "
                                                        ".hex, stored relative to the project)"}],
             category="Memories")
    p["device"].update({"memory_size_parameter": "size", "memory_fill": 0 if ram else 255,
                        "contents_parameter": "contents"})
    return p


def main():
    os.makedirs(FOLDER, exist_ok=True)
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "part_descriptions.json"), encoding="utf-8") as fh:
        short = json.load(fh)
    for p in [hc595(), max7219(), eeprom_24c02(), lm75(), pcf8574(), lcd_hd44780(), memory(False), memory(True),
              bmp280(), mpu6050(), ds3231(), tcs34725(), ssd1306(), mcp3208(), mcp41010(), w25q32(), sdcard(),
              hx711(), ds18b20(), dht22(), hcsr04(), ws2812(), servo(), serial_port()]:
        p["description"] = short.get(p["type"], p["description"])
        with open(os.path.join(FOLDER, p["type"] + ".json"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(json.dumps(p, indent=2, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
