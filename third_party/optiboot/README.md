# Optiboot for the Arduino UNO

The Arduino UNO part (SN-261) has this bootloader in its ATmega328P's boot
section, as the board leaves the factory, so that the Arduino IDE and avrdude
upload to it unchanged.

- `optiboot_atmega328.hex`: the binary SimNodus loads, unmodified, SHA-256
  `3fe5f1e25110ba20ca8ec7b520c1146417bcf864c80ebd960edeb16efa5b9a61`.
- `optiboot.c`, `boot.h`, `pin_defs.h`, `stk500.h`, `Makefile`,
  `README.TXT`: its complete source, unmodified.
- Origin: the Arduino AVR core 1.8.8 (`bootloaders/optiboot`), Optiboot
  version 4.4 as Arduino maintains it; upstream
  <https://github.com/Optiboot/optiboot>. Copyright Peter Knight, Jason P
  Kyle, Spiff, Ladyada, Bill Westfield and contributors.
- License: GNU General Public License version 2 or later (see the header of
  `optiboot.c`); the text is in `COPYING`. SimNodus does not link it: it is
  firmware the emulated chip runs, shipped with its source.
