//
// Copyright (c) 2017 The Altra64 project contributors
// Portions (c) 2011 KRIK
// See LICENSE file in the project root for full license information.
//

#ifndef _MEM_H
#define	_MEM_H

#define SPI_SPEED_INIT 2
#define SPI_SPEED_25 1
#define SPI_SPEED_50 0

#define mem_spi evd_SPI
#define memSpiSetSpeed evd_setSpiSpeed
#define memSpiIsBusy evd_isSpiBusy
#define memSpiSSOff evd_spiSSOff
#define memSpiSSOn evd_spiSSOn


void memSpiSSOn();
void memSpiSSOff();
void memSpiBusy();
u8 memSpiIsBusy();
void memSpiSetSpeed(u8 speed);
void spiReadBlock(void *dat);
void spiWriteBlock(void *dat);
u8 memSpiRead(void *dst, u16 slen);
u8 memSpiWrite(const void *src);
//u8 mem_spi(u8 dat);
void memfill(void *dst, u8 val, u16 len);
void memcopy(void *src, void *dst, u16 len);
void memSpiSetDma(u8 mode);
void memRomWrite32(u32 addr, u32 val);
u32 memRomRead32(u32 addr);

#endif	/* _MEM_H */
