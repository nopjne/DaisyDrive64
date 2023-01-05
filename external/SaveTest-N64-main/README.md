# N64 Cartridge Save Type Detection Test ROM

This is a simple test ROM for Nintendo 64 that can detect all known (and a few theoretical) cartridge-based save capabilities.

![Screenshot of SRAM 768Kb detection result](./screenshot-sram768k.png?raw=true)

## Supported save types

* EEPROM 4Kbit (512 bytes)
* EEPROM 16Kbit (2048 bytes)
* FlashRAM 1Mbit (128 KiB)
* SRAM 256Kbit (32 KiB)
* SRAM 768Kbit Banked (96 KiB)
  * Only used by one officially-licensed cartridge: Dezaemon 3D
  * Supported on EverDrive64 using `SRAM 768K`/`SRAM 96K` save type
* SRAM 1Mbit Banked (128 KiB)
  * Not used by any officially-licensed cartridge
  * Theoretically-possible: no known emulator/flashcart implementation exists
* SRAM 768Kbit Contiguous (96 KiB)
  * Not used by any officially-licensed cartridge
  * Only supported on EverDrive64 V3/X5/X7 using `SRAM 1M`/`SRAM 128K` save type
* SRAM 1Mbit Contiguous (128 KiB)
  * Not used by any officially-licensed cartridge
  * Only supported on EverDrive64 V3/X5/X7 using `SRAM 1M`/`SRAM 128K` save type

## Multiple save types

* If a larger capacity is supported, any compatible smaller capacities will also be detected:
  * EEPROM 16Kbit implies that EEPROM 4Kbit is also supported.
  * SRAM 1Mbit Banked implies that both SRAM 768Kbit Banked and SRAM 256Kbit are supported.
  * SRAM 1Mbit Contiguous implies that both SRAM 768Kbit Contiguous and SRAM 256Kbit are supported.
* It is technically possible for a cartridge to contain EEPROM alongside SRAM or FlashRAM. No emulators or flash carts currently support this configuration.
* It is not possible for SRAM to co-exist with FlashRAM. This is because both SRAM and FlashRAM are accessed through Cartridge Domain 2 Address 2 memory space.

## Run the test ROM

[Download](./savetest.z64?raw=true) or [compile](#build-the-rom) the ROM file and load it as you would any other N64 ROM.

This ROM file has been tested to work on real Nintendo 64 hardware using the [EverDrive-64 by krikzz](http://krikzz.com/) and [64drive by retroactive](http://64drive.retroactive.be/).

This ROM file should also be compatible with low-level, accuracy-focused Nintendo 64 emulators such as [Ares](https://ares-emulator.github.io/), [CEN64](https://cen64.com/) and [MAME](http://mamedev.org/).

## Build the ROM

1. [Install LibDragon](https://github.com/DragonMinded/libdragon) and make sure you export `N64_INST` as the path to your N64 compiler toolchain.
2. Run `make` to produce `savetest.z64`

## License

This project is [Unlicensed public domain software](./LICENSE.md?raw=true) written by [Christopher Bonhage](https://github.com/meeq).

LibDragon is [Unlicensed public domain software](https://github.com/DragonMinded/libdragon/blob/trunk/LICENSE.md?raw=true).

"Nintendo 64" is a registered trademark of Nintendo used for informational purposes without permission.
