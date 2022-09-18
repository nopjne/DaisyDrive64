#include "daisy_seed.h"
#include "stm32h7xx_hal.h"
#include "sys/system.h"
#include "stm32h7xx_hal_dma.h"
#include "n64common.h"
#include "daisydrive64.h"

#define SI_RESET 0xFF // Tx:1 Rx:3
#define SI_INFO  0x00 // Tx:1 Rx:3
#define EEPROM_READ 0x04 // Tx:2 Rx:8
#define EEPROM_STORE  0x05 // Tx:10 Rx:1 

BYTE *EepromInputLog = (BYTE*)(LogBuffer - 1024 * 1024);
DTCM_DATA uint32_t EepLogIdx = 0;

#define ITCM_FUNCTION __attribute__((long_call, section(".itcm_text")))

// 0  = 0,0,0,1
// 1  = 0,1,1,1
// DS = 0,0,1,1
// CS = 0,1,1 
bool ITCM_FUNCTION SIGetBytes(BYTE *Out, uint32_t ExpectedBytes, bool Block)
{
    volatile uint32_t TimeStamp;
    volatile uint32_t OldTimeStamp;
    uint32_t TransferTime = 540; // 540 * 1000 * 1000 / 1000 * 1000 = 1us.
    volatile BYTE BitCount = 0;
    *Out = 0;

    // wait for sdat low
    while (((GPIOC->IDR & (S_DAT_LINE)) != 0)) {}

    TimeStamp = DWT->CYCCNT;
    OldTimeStamp = TimeStamp;
    while ((Running != false) && (BitCount < 8)) {
        // Wait for 2us.
        while (((TimeStamp - OldTimeStamp) < (TransferTime * 2))) {
            TimeStamp = DWT->CYCCNT;
        }

        *Out <<= 1;
        *Out |= ((GPIOC->IDR & (S_DAT_LINE)) != 0) ? 1 : 0;
        BitCount += 1;
        if ((BitCount % 8) == 0) {
#ifdef LOG_EEPROM_BYTES
            EepromInputLog[EepLogIdx++ % (1024*1024)] = *Out;
#endif
            Out += 1;
        }

        if (BitCount == (8 * ExpectedBytes)) {
            return true;
        }

        // Wait for next bit.
        uint32_t timeout = 300;
        while (((GPIOC->IDR & (S_DAT_LINE)) == 0) && (timeout--)) {}

        if (timeout != 0) {
            timeout = 300;
            while (((GPIOC->IDR & (S_DAT_LINE)) != 0) && (timeout--)) {}
        }

        if (timeout == 0) {
            return false;
        }

        OldTimeStamp = TimeStamp = DWT->CYCCNT;
    }

#ifdef LOG_EEPROM_BYTES
    EepromInputLog[EepLogIdx++ % (1024*1024)] = *Out;
#endif

    return true;
}

bool ITCM_FUNCTION SIGetConsoleTerminator(void)
{
    volatile uint32_t TimeStamp;
    volatile uint32_t OldTimeStamp;
    const uint32_t TransferTime = 540; // 540 * 1000 * 1000 / 1000 * 1000 = 1us.
    // Skip the terminator for now.
    TimeStamp = OldTimeStamp = DWT->CYCCNT;
    while ((Running != false) && ((TimeStamp - OldTimeStamp) < (TransferTime * 3))) {
        TimeStamp = DWT->CYCCNT;
    }

    return true;
}

volatile uint32_t OldTimeStamp;
bool ITCM_FUNCTION SIPutDeviceTerminator(void)
{
    const BYTE SIDeviceTerminator = 0xC;
    volatile uint32_t TimeStamp;
    BYTE Out = SIDeviceTerminator;

    const uint32_t TransferTime = 555; // 540 * 1000 * 1000 / 1000 * 1000 = 1us.
    //OldTimeStamp = DWT->CYCCNT;
    while ((Running != false) && (Out != 0)) {
        TimeStamp = DWT->CYCCNT;
        volatile uint32_t OverTime = (TimeStamp - OldTimeStamp);
        if (OverTime > TransferTime) {
            GPIOC->BSRR = (((Out & 1) != 0) ? S_DAT_LINE : (S_DAT_LINE << 16));
            Out >>= 1;
            OldTimeStamp = (TimeStamp - (OverTime - TransferTime));
        }
    }

    GPIOC->BSRR = S_DAT_LINE; // Set line to high to enable input.

    return true;
}

bool ITCM_FUNCTION SIPutByte(BYTE In)
{
    const BYTE SIZero = 0x8;
    const BYTE SIOne = 0xE;
    volatile uint32_t TimeStamp;
    
    BYTE Out = 0;

    const uint32_t TransferTime = 553; // 540 * 1000 * 1000 / 1000 * 1000 = 1us.
    uint BitCount = 0;
    //OldTimeStamp = DWT->CYCCNT;
    while ((Running != false) && (BitCount <= 8)) {
        if (Out == 0) {
            Out = ((In & 0x80) != 0) ? SIOne : SIZero;
            In <<= 1;
            BitCount += 1;
        }

        TimeStamp = DWT->CYCCNT;
        volatile uint32_t OverTime = (TimeStamp - OldTimeStamp);
        if (OverTime > TransferTime) {
            GPIOC->BSRR = (((Out & 1) != 0) ? S_DAT_LINE : (S_DAT_LINE << 16));
            Out >>= 1;
            OldTimeStamp = (TimeStamp - (OverTime - TransferTime));
        }
    }

    return true;
}

BYTE EEPROMStore[2048]; // 16KiBit
BYTE EEPROMType = 0x80;
void ITCM_FUNCTION RunEEPROMEmulator(void)
{
    BYTE Command = 0;

    // Check state 
    bool result = SIGetBytes(&Command, 1, true);
    if ((Running == false) || (result == false)) {
        return;
    }

    // 
    if ((Command == SI_RESET) || (Command == SI_INFO)) {
        SIGetConsoleTerminator();
        // Return the info on either 4KiBit or 16KiBit EEPROM, it should be safe to always return 16kbit.
        // 0x0080	N64	4 Kbit EEPROM	Bitfield: 0x80=Write in progress
        // 0x00C0	N64	16 Kbit EEPROM	Bitfield: 0x80=Write in progress
        OldTimeStamp = DWT->CYCCNT;
        SIPutByte(0x00);
        SIPutByte(EEPROMType);
        SIPutByte(0x00);
        SIPutDeviceTerminator();
    }

    if (Command == EEPROM_READ) {
        BYTE AddressByte;
        SIGetBytes(&AddressByte, 1, true);
        SIGetConsoleTerminator();
        uint32_t Address = AddressByte * 8;
        OldTimeStamp = DWT->CYCCNT;
        for (int i = 0; i < 8; i += 1) {
            SIPutByte(EEPROMStore[Address + i]);
        }

        SIPutDeviceTerminator();
    }

    if (Command == EEPROM_STORE) {
        BYTE AddressByte;
        SIGetBytes(&AddressByte, 1, true);
        uint32_t Address = AddressByte * 8;
        for (int i = 0; i < 8; i += 1) {
            SIGetBytes(&(EEPROMStore[Address + i]), 8, true);
        }

        SIGetConsoleTerminator();
        OldTimeStamp = DWT->CYCCNT;
        SIPutByte(0x00); // Output not busy, coz we fast.
        SIPutDeviceTerminator();
    }
}