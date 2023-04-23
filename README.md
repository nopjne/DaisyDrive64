All rights reserved. Right now this project is in a development stage and not ready for any consumers.

DaisyDrive64 is a N64 Cartridge Emulator that uses a Daisy Seed MCU board utilizing an STM32H750 (ARM Cortex-M7).
(These MCU boards can be found here: https://www.electro-smith.com/daisy/daisy)

![alt text](https://github.com/nopjne/DaisyDrive64/blob/master/DaisyDrive64.jpg?raw=true) \
Note: The white DaisyDrive64 - PCB boards are not yet public, please join the discord server mentioned below if you are comfortable bringing up these boards on your own.
You will need basic to moderate soldering skills and it really helps to have an STM32 debugger. (stlinkv2 or v3)

To interface with the N64 the DaisyDrive64 currently can use the PicoCart64 with breakout wires or can be wired to the Game Pak port directly. Please use the below connection diagram, while boards are being finalized.

The DaisySeed (STM32 CM7) gpio-pins can be reconfigured through software. The currently used setup attempts to optimize for AD0-15 contiguity, so the amount of involvement from the CPU is less. Ideally it could be none and handled entirely through DMA, but the Daisy Seed configuration prevents this, it would be possible if developing a board from scratch and wiring the stm32 directly.

![alt text](https://github.com/nopjne/DaisyDrive64/blob/master/daisypinout.png?raw=true)

![alt text](https://github.com/nopjne/DaisyDrive64/blob/master/n64pinout.JPG?raw=true)

Setting up: \
	1. Follow the setup instructions from Setting Up Your (Daisy) Development Environment: https://github.com/electro-smith/DaisyWiki/wiki/1.-Setting-Up-Your-Development-Environment \
	2. Clone the DaisyDrive64 source somewhere.

Building the project: \
	1. make -C external/libdaisy \
        2. make -C src

Executing debugger probe: \
	1. Build \
	2. Use visual studio to reference the correct openocd executable. \
	3. Use F5 to upload and start the DaisyDrive.

Flashing release firmware and data through web programmer: https://electro-smith.github.io/Programmer/ \
	1. Put in DFU mode -- In web programmer use "advanced" and "flash bootloader" image. \
	2. Reset the daisy by disconnecting it and plugging it back in. (DO NOT PUT IT IN DFU MODE) \
	3. Click connect in web programmer and select "Daisy boot loader" \
	4. Choose file: "DaisyDrive64_data.bin", program it. \
	5. Put daisy in DFU mode again. \
	6. Choose file: "DaisyDrive64_fw.bin", program it.

Video of assembling the DaisyDrive64: (this video shows outdated flashing instructions, please follow the instructions above here) \
[![Assembling the DaisyDrive64](https://img.youtube.com/vi/Yn7m13Sy0nY/1.jpg)](https://www.youtube.com/watch?v=Yn7m13Sy0nY)

Video showing off emulator support: \
[![Emulator inception](https://img.youtube.com/vi/nDgXXXI7Gs8/0.jpg)](https://www.youtube.com/watch?v=nDgXXXI7Gs8)

Comparison video of loading roms on the DaisyDrive64 and the Everdrive64: \
[![Game load](https://img.youtube.com/vi/WPcANSvD16U/0.jpg)](https://www.youtube.com/watch?v=WPcANSvD16U)

Background:
This project started out with wanting to connect an SD card to the N64 directly. After playing with the SD protocol, it was quickly apparent that the SD access latency was too great to support the XIP access that the N64 needs. Originally I gave up on this project because it would require additional hardware to create a working cart emulator and there were already solutions on the market that have the expected hardware. I also was dreading to creating my own (first) PCB for the project. My interest in the project got reinvigorated when Konrad Beckmann sent me a PicoCart64 v2 PCB, but instead of focusing on a RPi Pico MCU, I wanted to use an MCU that had 64MB RAM and SD support. 

I found an MCU board (Daisy Seed) that hit most of the requirements and seemed a lot faster than what I needed.
Once I got the Daisy Seed board I learned the difference between the speed of the GPIO pins and the speed the CPU runs at. With an added bonus the Daisy Board is specifically designed with audio in mind and has an op-amp that can be connected to the N64 audio interface of the gamepak. 

![alt text](https://github.com/nopjne/DaisyDrive64/blob/master/wires.jpg?raw=true)

Things that are implemented: \
    1. SD card reading and writing. (Roms are to be stored on an SD card and are loaded by the FW) \
    2. ExFat support and support for SDHC cards. (64gb+) \
    3. N64 PI interface at SDK speed (Domain1 reads). Using DMA address latching, interrupts for RD responses. \
    4. EEPROM emulation for game saves. Using LPTIM3 and DMA. The emulation runs in an interrupt routine. \
    5. CIC emulation and detection. CIC is detected through a CRC, CIC_CLK is being monitored from an interrupt. \
    6. Early rom endianness detection. \
    7. Menu through Altra64 \
    8. Async SD load. (Only the first 1MB is needed to boot) The rest can be loaded over time during boot. This will enable instant boot. \
    9. Automatic eeprom and sram commit to sd card. \
   10. Spoken error messages through the analog cart output. (Requires the usage of Multi-Out, does not work with UltraHDMI, N64Digital or N64Advanced. These mods tap the signal from the RSP and completely neglect the analog audio mixing)

Things that need to be implemented: \
    1. PI RD through DMA. (There is still a device that can kick off DMA, HRTIM_EEV4 on Pin 8) \
    2. Flash Ram emulation (Domain2 reads + writes) This is tricky because there is only 92ns between the latch and wr/rd. \
    3. 64DD support. \
    4. RTC, Animal Forest needs the RTC. The MCU has support for an RTC but has no way of keeping time.
	
Things that would be nice to have: \
    1. Patch support. Allow patches to be applied on the fly and save to a different save file. \
    2. EEPROM revisions. Allow past eeprom state to be retrievable. 
	
Most commercial roms run and save without issues, however there are still issues due to the missing Flash RAM support. There are only ~30 titles that need Flash Ram support, these games will play but will not save.
As a bootstrap this project relied on the RPI Pico to do CIC emulation and Flash Emulation thanks to PicoCart64 by Konrad Beckmann https://github.com/kbeckmann/PicoCart64
Please join the following discord to participate in the DaisyDrive64 development: https://discord.gg/2Gb3jWqqja

Known issues: \
    1. Games that use FlashRam as savetype do not save. (Refer here: http://micro-64.com/database/gamesave.shtml) \
    2. Games that use SRAM as savetype may or may not save depending on how leanient the game code is. OOT and SMB64 save, others do not. (refer here for other games: http://micro-64.com/database/gamesave.shtml) \
    3. The DRAM has been tuned but it is unclear whether that is stable. \
    4. Currently the MCU is overclocked to 540Mhz this may not be needed and generates more heat than necessary.
