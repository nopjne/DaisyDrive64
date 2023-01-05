//
// Copyright (c) 2017 The Altra64 project contributors
// Portions (c) 2013 saturnu (Alt64) based on libdragon, Neo64Menu, ED64IO, libn64-hkz, libmikmod
// See LICENSE file in the project root for full license information.
//

#include "sram.h"
#include "everdrive.h"
#include "sys.h"
#include "rom.h"
#include "cic.h"

void pif_boot()
{
    //TODO: implement
}



int is_valid_rom(unsigned char *buffer) {
    /* Test if rom is a native .z64 image with header 0x80371240. [ABCD] */
    if((buffer[0]==0x80)&&(buffer[1]==0x37)&&(buffer[2]==0x12)&&(buffer[3]==0x40))
        return 0;
    /* Test if rom is a byteswapped .v64 image with header 0x37804012. [BADC] */
    else if((buffer[0]==0x37)&&(buffer[1]==0x80)&&(buffer[2]==0x40)&&(buffer[3]==0x12))
        return 1;
    /* Test if rom is a wordswapped .n64 image with header  0x40123780. [DCBA] */
    else if((buffer[0]==0x40)&&(buffer[1]==0x12)&&(buffer[2]==0x37)&&(buffer[3]==0x80))
        return 2;
    else
        return 0;
}

void swap_header(unsigned char* header, int loadlength) {
    unsigned char temp;
    int i;

    /* Btyeswap if .v64 image. */
    if( header[0]==0x37) {
        for (i = 0; i < loadlength; i+=2) {
            temp= header[i];
            header[i]= header[i+1];
            header[i+1]=temp;
            }
        }
    /* Wordswap if .n64 image. */
    else if( header[0]==0x40) {
        for (i = 0; i < loadlength; i+=4) {
            temp= header[i];
            header[i]= header[i+3];
            header[i+3]=temp;
            temp= header[i+1];
            header[i+1]= header[i+2];
            header[i+2]=temp;
        }
    }
}

u8 getCicType(u8 bios_cic) {
    u8 cic_buff[2048];
    volatile u8 cic_chip;
    volatile u32 val;
    if (bios_cic) {
        evd_setCfgBit(ED_CFG_SDRAM_ON, 0);
        sleep(10);
        val = *(u32 *) 0xB0000170;
        dma_read_s(cic_buff, 0xB0000040, 1024);
        cic_chip = get_cic(cic_buff);
        evd_setCfgBit(ED_CFG_SDRAM_ON, 1);
        sleep(10);
    }
    else {
        val = *(u32 *) 0xB0000170;
        dma_read_s(cic_buff, 0xB0000040, 1024);
        cic_chip = get_cic(cic_buff);
    }

    return cic_chip;
}
