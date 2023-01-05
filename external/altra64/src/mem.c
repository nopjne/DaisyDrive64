//
// Copyright (c) 2017 The Altra64 project contributors
// Portions (c) 2013 saturnu (Alt64) based on libdragon, Neo64Menu, ED64IO, libn64-hkz, libmikmod
// See LICENSE file in the project root for full license information.
//

#include "everdrive.h"
#include "sys.h"
#include "errors.h"
#include "mem.h"

u8 spi_dma;

u8 memSpiReadDma(void *dst, u16 slen);
u8 memSpiReadPio(void *dst, u16 slen);
u8 mem_buff[512];

void memSpiSetDma(u8 mode) {
    spi_dma = mode;
}

void memcopy(void *src, void *dst, u16 len) {

    u8 *s = (u8 *) src;
    u8 *d = (u8 *) dst;
    while (len--)*d++ = *s++;
}

void memfill(void *dst, u8 val, u16 len) {
    u8 *d = (u8 *) dst;
    while (len--)*d++ = val;
}


// buff len
u8 memSpiRead(void *dst, u16 slen) {

    u8 copy_to_rom = 0;
    u32 addr = (u32) dst;  //if ROM_ADDR 0xb0000000
    if (addr >= ROM_ADDR && addr < ROM_END_ADDR)copy_to_rom = 1;

    //if (copy_to_rom || spi_dma) {
    if ((copy_to_rom || spi_dma) && addr % 4 == 0) {
        return memSpiReadDma(dst, slen);
    } else {
        return memSpiReadPio(dst, slen);

    }

}

u8 memSpiReadPio(void *dst, u16 slen) {

    u16 i;
    u16 u;
    u8 *ptr8 = (u8 *) dst;



    for (i = 0; i < slen; i++) {

        evd_SDdatReadMode(1);
        for (u = 0; u < 65535; u++)if ((evd_SPI(0xff) & 0xf1) == 0xf0)break;
        evd_SDdatReadMode(0);
        if (u == 65535) {
            evd_spiSSOff();
            return DISK_RD_FE_TOUT;
        }

        for (u = 0; u < 512; u++)*ptr8++ = evd_SPI(0xff);

        u = evd_isSDMode() ? 8 : 2;

        while (u--) {
            // console_printf("XRC: %02X", evd_SPI(0xff));
            //console_printf("%02X\n", evd_SPI(0xff));
            u--;
            evd_SPI(0xff);
            evd_SPI(0xff);
        }
        //evd_SPI(0xff);
        //evd_SPI(0xff);
    }


    return 0;
}

u8 memSpiReadDma(void *dst, u16 slen) {

    u8 resp = 0;
    u8 copy_to_rom = 0;
    u32 addr = (u32) dst;

    evd_SDdatReadMode(0);
    // console_printf("dma\n");
    if (addr >= ROM_ADDR && addr < ROM_END_ADDR)copy_to_rom = 1;

    if (copy_to_rom) {

        return evd_mmcReadToCart(addr, slen);

    } else {

        resp = evd_mmcReadToCart(0, slen);
        dma_read_s(dst, ROM_ADDR, slen * 512);
    }


    return resp;
}

u8 memSpiWrite(const void *src) {

    u16 i;

    u8 *ptr8 = (u8 *) src;

    if ((u32) src >= ROM_ADDR && (u32) src < ROM_END_ADDR) {
        dma_read_s(mem_buff, (u32) src, 512);
        for (i = 0; i < 512; i++)mem_spi(mem_buff[i]);
    } else {
        for (i = 0; i < 512; i++)mem_spi(*ptr8++);
    }


    return 0;
}

void memSpiBusy() {

    while (evd_isSpiBusy());

}

void memRomWrite32(u32 addr, u32 val) {

    vu32 *ptr = (u32 *) (addr + ROM_ADDR);
    vu8 tmp;

    tmp = *ptr;
    *ptr = val;
    tmp = *ptr;
}

u32 memRomRead32(u32 addr) {

    vu32 *ptr = (u32 *) (addr + ROM_ADDR);
    vu8 tmp;
    vu32 val;

    tmp = *ptr;
    val = *ptr;

    return val;
}


/*
u8 mem_spi(u8 dat) {
    return evd_SPI(dat);
}

 */
