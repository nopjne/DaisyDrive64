All rights reserved. Right now this project is in a development stage and not ready for any consumers.

DaisyDrive64 is a N64 Cartridge Emulator that uses a Daisy Seed MCU board utilizing an STM32H750 (ARM Cortex-M7).
(These MCU boards can be found here: https://www.electro-smith.com/daisy/daisy)

To interface with the N64 the DaisyDrive64 currently can use the PicoCart64 with breakout wires or can be wired to the Game Pak port directly. Please use the below connection diagram, while boards are being finalized.

For now the DaisyDrive64 needs to be powered by USB and is not to be powered by the N64, this is also necessary for the Cortex M7 to load a rom off the SD card and into the 64MB ram that is connected through an FMC interface. It's something that will be less important when a menu is created/ported for the DaisyDrive64.

The DaisySeed (STM32 CM7) gpio-pins can be reconfigured through software. The currently used setup attempts to optimize for AD0-15 contiguity, so the amount of involvement from the CPU is less. Ideally it could be none and handled entirely through DMA, but the Daisy Seed configuration prevents this, it would be possible if developing a board from scratch and wiring the stm32 directly.

![alt text](https://github.com/nopjne/DaisyDrive64/blob/master/daisypinout.png?raw=true)

![alt text](https://github.com/nopjne/DaisyDrive64/blob/master/n64pinout.JPG?raw=true)
Gamepak slot pinout. LAUDIO and RAUDIO can be connected to Pin18 and Pin19 respectively, however there may be some hardware required to interface these safely. The values for the input are currently unknown.

Setting up:
	1. Follow the setup instructions from 1. Setting Up Your Development Environment · https://github.com/electro-smith/DaisyWiki/wiki/1.-Setting-Up-Your-Development-Environment
	2. Clone the DaisyDrive64 source somewhere.

Building the project:
	1. make -C external/libdaisy
        2. make -C src

Executing (USB uploading):
	1. Build
	2. Upload the src/build/daisydrive64.elf to the USB drive through the method mentioned in the electro-smith environment setup.

Executing debugger probe
	1. Build
	2. Use visual studio to reference the correct openocd executable.
	3. Use F5 to upload and start the DaisyDrive.

Background:
This project started out with wanting to connect an SD card to the N64 directly. After playing with the SD protocol, it was quickly apparent that the SD access latency was too great to support the XIP access that the N64 needs. Originally I gave up on this project because it would require additional hardware to create a working cart emulator and there were already solutions on the market that have the expected hardware. I also was dreading to creating my own (first) PCB for the project. My interest in the project got reinvigorated when Konrad Beckmann sent me a PicoCart64 v2 PCB, but instead of focusing on a RPi Pico MCU, I wanted to use an MCU that had 64MB RAM and SD support. 

I found an MCU board (Daisy Seed) that hit most of the requirements and seemed a lot faster than what I needed.
Once I got the Daisy Seed board I learned the difference between the speed of the GPIO pins and the speed the CPU runs at. With an added bonus the Daisy Board is specifically designed with audio in mind and has an op-amp that can be connected to the N64 audio interface of the gamepak. 

![alt text](https://github.com/nopjne/DaisyDrive64/blob/master/wires.jpg?raw=true)

Things that are implemented:
	1. SD card reading and writing. (Roms are to be stored on an SD card and are loaded by the FW)
	2. N64 PI interface at 1/4th the speed (Domain1 reads). Using DMA address latching, interrupts for RD responses.
	3. EEPROM emulation for game saves. Using LPTIM3 and DMA. The emulation runs in an interrupt routine.
	4. CIC emulation and detection. CIC is detected through a CRC, CIC_CLK is being monitored from an interrupt.
	5. Early rom endianness detection.

Things that need to be implemented:
	1. PI RD through DMA. (There is still a device that can kick off DMA, HRTIM_EEV4 on Pin 8)
	2. Flash Ram emulation (Domain2 reads + writes) This is tricky because there is only 92ns between the latch and wr/rd.
	3. Run CIC in an interrupt entirely, should allow the core to be used for other work.
	4. Reduce NDTR reads for the EEPROM emulator, the emulation should not spin in the interrupt but have a state machine.
	
Things that would be nice to have:
	1. Implement a menu. (an N64 binary that will help select which rom to use)
	2. Patch support. Allow patches to be applied on the fly and save to a different save file.
	3. EEPROM revisions. Allow past eeprom state to be retrievable.
	4. Improve rom endianness detection.
	
Most commercial roms run and save without issues, however there are still issues due to running the bus 1/4th the speed and the missing Flash RAM support. There are only ~30 titles that need Flash Ram support.
As a bootstrap this project relied on the RPI Pico to do CIC emulation and Flash Emulation thanks to PicoCart64 by Konrad Beckmann https://github.com/kbeckmann/PicoCart64

Known issues:
	1. DK64 does not boot.
	2. SF64 has the tendency to lock up.
	3. OOT random freezes.
        4. SMB Freezes when reading Flash.
        5. Stability issues when CIC is emulated from the daisy.
