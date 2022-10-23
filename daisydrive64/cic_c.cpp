/**************************************************************
 * Generic CIC implementation for N64                         *
 * ---------------------------------------------------------- *
 * This should run on every MCU which is fast enough to       *
 * handle the IO operations.                                  *
 * You just have to implement the low level gpio functions:   *
 *     - ReadBit() and                                        *
 *     - WriteBit().                                          *
 *                                                            *
 * Hardware connections                                       *
 * Data Clock Input (DCLK): CIC Pin 14                        *
 * Data Line, Bidir (DIO):  CIC Pin 15                        *
 *                                                            *
 **************************************************************/

#include "daisy_seed.h"
#include "daisydrive64.h"
#include "n64common.h"
#define REGION_NTSC (0)
#define REGION_PAL  (1)

#define GET_REGION() (REGION_NTSC)
#define CIC_USE_INTERRUPTS 1
/* SEEDs */ 

// 6102/7101
#define CIC6102_SEED 0x3F

// 6101
#define CIC6101_SEED 0x3F

// 6103/7103
#define CIC6103_SEED 0x78

// 6105/7105
#define CIC6105_SEED 0x91

// 6106/7106
#define CIC6106_SEED 0x85

// 7102
#define CIC7102_SEED 0x3F

/* CHECKSUMs */

// 6102/7101
#define CIC6102_CHECKSUM 0xa, 0x5, 0x3, 0x6, 0xc, 0x0, 0xf, 0x1, 0xd, 0x8, 0x5, 0x9

// 6101
#define CIC6101_CHECKSUM 0x4, 0x5, 0xC, 0xC, 0x7, 0x3, 0xE, 0xE, 0x3, 0x1, 0x7, 0xA

// 6103/7103
#define CIC6103_CHECKSUM 0x5, 0x8, 0x6, 0xf, 0xd, 0x4, 0x7, 0x0, 0x9, 0x8, 0x6, 0x7

// 6105/7105
#define CIC6105_CHECKSUM 0x8, 0x6, 0x1, 0x8, 0xA, 0x4, 0x5, 0xB, 0xC, 0x2, 0xD, 0x3

// 6106/7106
#define CIC6106_CHECKSUM 0x2, 0xB, 0xB, 0xA, 0xD, 0x4, 0xE, 0x6, 0xE, 0xB, 0x7, 0x4

// 7102
#define CIC7102_CHECKSUM 0x4, 0x4, 0x1, 0x6, 0x0, 0xE, 0xC, 0x5, 0xD, 0x9, 0xA, 0xF

void EncodeRound(unsigned char index);
void CicRound(unsigned char *);
void Cic6105Algo(void);

/* Select SEED and CHECKSUM here */
unsigned char _CicSeed = CIC6102_SEED;

const unsigned char _CicChecksumBytes[] = {
        CIC6102_CHECKSUM,
        CIC6101_CHECKSUM,
        CIC6103_CHECKSUM,
        CIC6105_CHECKSUM,
        CIC6106_CHECKSUM,
        CIC7102_CHECKSUM,
    };

const unsigned char *_CicChecksum;


/* NTSC initial RAM */
const unsigned char _CicRamInitNtsc[] = {
    0xE, 0x0, 0x9, 0xA, 0x1, 0x8, 0x5, 0xA, 0x1, 0x3, 0xE, 0x1, 0x0, 0xD, 0xE, 0xC,
    0x0, 0xB, 0x1, 0x4, 0xF, 0x8, 0xB, 0x5, 0x7, 0xC, 0xD, 0x6, 0x1, 0xE, 0x9, 0x8
};

/* PAL initial RAM */
const unsigned char _CicRamInitPal[] = {
    0xE, 0x0, 0x4, 0xF, 0x5, 0x1, 0x2, 0x1, 0x7, 0x1, 0x9, 0x8, 0x5, 0x7, 0x5, 0xA,
    0x0, 0xB, 0x1, 0x2, 0x3, 0xF, 0x8, 0x2, 0x7, 0x1, 0x9, 0x8, 0x1, 0x1, 0x5, 0xC
};

/* Memory for the CIC algorithm */
unsigned char _CicMem[32];

/* Memory for the 6105 algorithm */
unsigned char _6105Mem[32];

/* YOU HAVE TO IMPLEMENT THE LOW LEVEL GPIO FUNCTIONS ReadBit() and WriteBit() */

#define CIC_DCLK_PIN ((GPIOG->IDR) & CIC_CLK)
#define CIC_DATA_PIN ((GPIOG->IDR) & CIC_DAT)
#define CIC_DATA_PIN_SET ((GPIOG->BSRR) = CIC_DAT)
#define CIC_DATA_PIN_CLEAR ((GPIOG->BSRR) = (CIC_DAT << 16))

volatile bool CicOut;
volatile bool CicBitIn;
volatile bool CicWait;

extern "C"
ITCM_FUNCTION
void EXTI9_5_IRQHandler(void)
{
    EXTI->PR1 = CIC_CLK;
    if (CIC_DCLK_PIN == 0) {
        if (CicOut == false) {
            CicBitIn = CIC_DATA_PIN;
        }

    } else {
        if (CicOut != false) {
            CIC_DATA_PIN_SET;
        }
    }

    CicWait = false;
}

/* Read a bit synchronized by DCLK */
unsigned char ReadBit(void)
{
    unsigned char res;
#if (CIC_USE_INTERRUPTS == 0)
    // wait for DCLK to go low
    while ((CIC_DCLK_PIN != 0) && (Running != false)) { }

    // Read the data bit
    res = CIC_DATA_PIN;

    // wait for DCLK to go high
    while ((CIC_DCLK_PIN == 0) && (Running != false)) { }
#else
    CicWait = true;
    CicOut = false;
    while ((CicWait != false) && (Running != false)) {}
    CicWait = true;
    while ((CicWait != false) && (Running != false)) {}
    res = CicBitIn;
#endif

    return res;
}

/* Write a bit synchronized by DCLK */
void WriteBit(unsigned char b)
{
#if (CIC_USE_INTERRUPTS == 0)
    // wait for DCLK to go low
    while ((CIC_DCLK_PIN != 0) && (Running != false)) { }

    if (b == 0) {
        // drive the output low
        // OUTPUT_TRISTATE = 0;
        // OUTPUT_DATA = 0;
        CIC_DATA_PIN_CLEAR;
    }

    // wait for DCLK to go high
    while ((CIC_DCLK_PIN == 0) && (Running != false)) { }

    // tristate the output
    //  OUTPUT_TRISTATE = 1;
    CIC_DATA_PIN_SET;

#else
    CicWait = true;
    CicOut = true;
    while ((CicWait != false) && (Running != false)) {}
    if (b == 0) {
        CIC_DATA_PIN_CLEAR;
    }
    
    CicWait = true;
    while ((CicWait != false) && (Running != false)) {}
    
#endif
}

/* Writes the lowes 4 bits of the byte */
void WriteNibble(unsigned char n)
{
    WriteBit(n & 0x08);
    WriteBit(n & 0x04);
    WriteBit(n & 0x02);
    WriteBit(n & 0x01);
}

// Write RAM nibbles until index hits a 16 Byte border
void WriteRamNibbles(unsigned char index)
{
    do {
        WriteNibble(_CicMem[index]);
        index++;
    } while ((index & 0x0f) != 0);
}

/* Reads 4 bits and returns them in the lowes 4 bits of a byte */
unsigned char ReadNibble(void)
{
    unsigned char res = 0;
    unsigned char i;
    for (i = 0; i < 4; i++) {
        res <<= 1;
        res |= ReadBit();
    }
    return res;
}

/* Encrypt and output the seed */
void WriteSeed(void)
{
    _CicMem[0x0a] = 0xb;
    _CicMem[0x0b] = 0x5;
    _CicMem[0x0c] = _CicSeed >> 4;
    _CicMem[0x0d] = _CicSeed;
    _CicMem[0x0e] = _CicSeed >> 4;
    _CicMem[0x0f] = _CicSeed;

    EncodeRound(0x0a);
    EncodeRound(0x0a);
    WriteRamNibbles(0x0a);
}

/* Encrypt and output the checksum */
void WriteChecksum(void)
{
    unsigned char i;
    for (i = 0; i < 12; i++) {
        _CicMem[i + 4] = _CicChecksum[i];
    }

    // wait for DCLK to go low
    // (doesn't seem to be necessary)

    // "encrytion" key
    // initial value doesn't matter
    //_CicMem[0x00] = 0;
    //_CicMem[0x01] = 0xd;
    //_CicMem[0x02] = 0;
    //_CicMem[0x03] = 0;

    EncodeRound(0x00);
    EncodeRound(0x00);
    EncodeRound(0x00);
    EncodeRound(0x00);

    // signal that we are done to the pif
    // (test with WriteBit(1) also worked)
    WriteBit(0);

    // Write 16 nibbles
    WriteRamNibbles(0);
}

/* seed and checksum encryption algorithm */
void EncodeRound(unsigned char index)
{
    unsigned char a;

    a = _CicMem[index];
    index++;

    do {
        a = (a + 1) & 0x0f;
        a = (a + _CicMem[index]) & 0x0f;
        _CicMem[index] = a;
        index++;
    } while ((index & 0x0f) != 0);
}

/* Exchange value of a and b */
void Exchange(unsigned char *a, unsigned char *b)
{
    unsigned char tmp;
    tmp = *a;
    *a = *b;
    *b = tmp;
}

// translated from PIC asm code (thx to Mike Ryan for the PIC implementation)
// this implementation saves program memory in LM8

/* CIC compare mode memory alternation algorithm */
void CicRound(unsigned char * m)
{
    unsigned char a;
    unsigned char b, x;

    x = m[15];
    a = x;

    do {
        b = 1;
        a += m[b] + 1;
        m[b] = a;
        b++;
        a += m[b] + 1;
        Exchange(&a, &m[b]);
        m[b] = ~m[b];
        b++;
        a &= 0xf;
        a += (m[b] & 0xf) + 1;
        if (a < 16) {
            Exchange(&a, &m[b]);
            b++;
        }
        a += m[b];
        m[b] = a;
        b++;
        a += m[b];
        Exchange(&a, &m[b]);
        b++;
        a &= 0xf;
        a += 8;
        if (a < 16) {
            a += m[b];
        }

        Exchange(&a, &m[b]);
        b++;
        do
        {
            a += m[b] + 1;
            m[b] = a;
            b++;
            b &= 0xf;
        } while (b != 0);
        a = x + 0xf;
        x = a & 0xf;
    } while (x != 15);
}

// Big Thx to Mike Ryan, John McMaster, marshallh for publishing their work

/* CIC 6105 algorithm */
void Cic6105Algo(void)
{
    unsigned char A = 5;
    unsigned char carry = 1;
    unsigned char i;
    for (i = 0; i < 30; ++i) {
        if (!(_6105Mem[i] & 1)) {
            A += 8;
        }

        if (!(A & 2)) {
            A += 4;
        }

        A = (A + _6105Mem[i]) & 0xf;
        _6105Mem[i] = A;
        if (carry == false) {
            A += 7;
        }

        A = (A + _6105Mem[i]) & 0xF;
        A = A + _6105Mem[i] + carry;
        if (A >= 0x10) {
            carry = 1;
            A -= 0x10;

        } else {
            carry = 0;
        }

        A = (~A) & 0xf;
        _6105Mem[i] = A;
    }
}

/* Wait for reset */
void Die(void)
{
    // never return
    while (1)
    { }
}

/* CIC compare mode */
void CompareMode(unsigned char isPal)
{
    unsigned char ramPtr;
    // don't care about the low ram as we don't check this
//  CicRound(&_CicMem[0x00]);
//  CicRound(&_CicMem[0x00]);
//  CicRound(&_CicMem[0x00]);

    // only need to calculate the high ram
    CicRound(&_CicMem[0x10]);
    CicRound(&_CicMem[0x10]);
    CicRound(&_CicMem[0x10]);

    // 0x17 determines the start index (but never 0)
    ramPtr = _CicMem[0x17] & 0xf;
    if (ramPtr == 0) {
        ramPtr = 1;
    }

    ramPtr |= 0x10;

    do
    {
        // read the bit from PIF (don't care about the value)
        ReadBit();

        // send out the lowest bit of the currently indexed ram
        WriteBit(_CicMem[ramPtr] & 0x01);

        // PAL or NTSC?
        if (isPal == false) {
            // NTSC
            ramPtr++;
        } else {
            // PAL
            ramPtr--;
        }

        // repeat until the end is reached
    } while (ramPtr & 0xf);
}

/* CIC 6105 mode */
void Cic6105Mode(void)
{
    unsigned char ramPtr;

    // write 0xa 0xa
    WriteNibble(0xa);
    WriteNibble(0xa);

    // receive 30 nibbles
    for (ramPtr = 0; ramPtr < 30; ramPtr++) {
        _6105Mem[ramPtr] = ReadNibble();
    }

    // execute the algorithm
    Cic6105Algo();

    // send bit 0
    WriteBit(0);

    // send 30 nibbles
    for (ramPtr = 0; ramPtr < 30; ramPtr++)
    {
        WriteNibble(_6105Mem[ramPtr]);
    }
}

/* Load initial ram depending on region */
void InitRam(unsigned char isPal)
{
    unsigned char i;

    if (isPal == false) {
        for (i = 0; i < 32; i++) {
            _CicMem[i] = _CicRamInitNtsc[i];
        }

    } else {
        for (i = 0; i < 32; i++) {
            _CicMem[i] = _CicRamInitPal[i];
        }
    }
}

unsigned char isPal;

#define CIC6102_TYPE 0
#define CIC6101_TYPE 1
#define CIC6103_TYPE 2
#define CIC6105_TYPE 3
#define CIC6106_TYPE 4
#define CIC7102_TYPE 5

uint32_t CrcTable[256];
bool TableBuilt = false;
uint32_t si_crc32(const uint8_t *data, size_t size) {
    unsigned n, k;
    uint32_t c;

    // No need to recompute the table on every invocation.
    if (TableBuilt == false) {
        for (n = 0; n < 256; n++) {
            c = (uint32_t) n;
            for (k = 0; k < 8; k++) {
                if (c & 1) {
                    c = 0xEDB88320L ^ (c >> 1);
                    
                } else {
                    c = c >> 1;
                }

                CrcTable[n] = c;
            }
        }

        TableBuilt = true;
    }

    c = 0L ^ 0xFFFFFFFF;
    for (n = 0; n < size; n++) {
        c = CrcTable[(c ^ data[n]) & 0xFF] ^ (c >> 8);
    }

  return c ^ 0xFFFFFFFF;
}

int cic_init(unsigned int Region, unsigned int Type)
{
    switch (Type) {
        case CIC6102_TYPE:
            _CicSeed = CIC6102_SEED;
        break;
        case CIC6101_TYPE:
            _CicSeed = CIC6101_SEED;
        break;
        case CIC6103_TYPE:
            _CicSeed = CIC6103_SEED;
        break;
        case CIC6105_TYPE:
            _CicSeed = CIC6105_SEED;
        break;
        case CIC6106_TYPE:
            _CicSeed = CIC6106_SEED;
        break;
        case CIC7102_TYPE:
            _CicSeed = CIC7102_SEED;
        break;
        default:
            _CicSeed = CIC6102_SEED;
            Type = CIC6102_TYPE;
        break;
    }

    _CicChecksum = &(_CicChecksumBytes[Type * 12]);

    // read the region setting
    isPal = Region;

    return 0;
}

#define CRC_NUS_5101 0x587BD543 // ??
#define CRC_NUS_6101 0x9AF30466 //0x6170A4A1
#define CRC_NUS_7102 0x009E9EA3 // ??
#define CRC_NUS_6102 0x6D089C64 //0x90BB6CB5
#define CRC_NUS_6103 0x211BA9FB //0x0B050EE0
#define CRC_NUS_6105 0x520D9ABB //0x98BC2C86
#define CRC_NUS_6106 0x266C376C //0xACC8580A
#define CRC_NUS_8303 0x0E018159 // ??
#define CRC_iQue_1 0xCD19FEF1
#define CRC_iQue_2 0xB98CED9A
#define CRC_iQue_3 0xE71C2766
void CICEmulatorInit(void)
{
    volatile uint32_t crc = si_crc32(ram + 0x40, 0x1000 - 0x40);
    switch (crc) {
    case CRC_NUS_6101:
    case CRC_NUS_7102:
    case CRC_iQue_1:
    case CRC_iQue_2:
    case CRC_iQue_3:
      cic_init(REGION_NTSC, CIC6101_TYPE);
      break;

    case CRC_NUS_6102:
      cic_init(REGION_NTSC, CIC6102_TYPE);
      break;

    case CRC_NUS_6103:
      cic_init(REGION_NTSC, CIC6103_TYPE);
      break;

    case CRC_NUS_6105:
      cic_init(REGION_NTSC, CIC6105_TYPE);
      break;

    case CRC_NUS_6106:
      cic_init(REGION_NTSC, CIC6106_TYPE);
      break;

    case CRC_NUS_8303:
      cic_init(REGION_NTSC, CIC6102_TYPE);
      break;

    // TODO do the PAL regions too.
    default:
      cic_init(REGION_NTSC, CIC6102_TYPE);
  }
}

void StartCICEmulator()
{
    // send out the corresponding id
    unsigned char hello = 0x1;
    if (isPal) {
        hello |= 0x4;
    }

    CIC_DATA_PIN_SET;
    WriteNibble(hello);

    // encode and send the seed
    WriteSeed();

    // encode and send the checksum
    WriteChecksum();
    
    // init the ram corresponding to the region
    InitRam(isPal);
    
    // read the initial values from the PIF
    _CicMem[0x01] = ReadNibble();
    _CicMem[0x11] = ReadNibble();

}
int RunCICEmulator(void)
{
    // read mode (2 bit)
    unsigned char cmd = 0;
    cmd |= (ReadBit() << 1);
    cmd |= ReadBit();
    switch (cmd)
    {
    case 0:
        // 00 (compare)
        CompareMode(isPal);
        break;

    case 2:
        // 10 (6105)
        Cic6105Mode();
        break;

    case 3:
        // 11 (reset)
        WriteBit(0);
        break;

    case 1:
        // 01 (die)
    default:
        Die();
    }

    return 0;
}
