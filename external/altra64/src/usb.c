//
// Copyright (c) 2017 The Altra64 project contributors
// Portions (c) 2013 saturnu (Alt64) based on libdragon, Neo64Menu, ED64IO, libn64-hkz, libmikmod
// See LICENSE file in the project root for full license information.
//

#include "everdrive.h"
#include "sys.h"
#include "rom.h"

u8 cmdTest();
u8 cmdFill();
u8 cmdReadRom();
u8 cmdWriteRom();

u64 usb_buff[128];
u8 *usb_buff8; // = (u8 *) usb_buff;


#define PI_BSD_DOM1_LAT_REG	(PI_BASE_REG+0x14)

/* PI dom1 pulse width (R/W): [7:0] domain 1 device R/W strobe pulse width */
#define PI_BSD_DOM1_PWD_REG	(PI_BASE_REG+0x18)

/* PI dom1 page size (R/W): [3:0] domain 1 device page size */
#define PI_BSD_DOM1_PGS_REG	(PI_BASE_REG+0x1C)    /*   page size */

/* PI dom1 release (R/W): [1:0] domain 1 device R/W release duration */
#define PI_BSD_DOM1_RLS_REG	(PI_BASE_REG+0x20)
/* PI dom2 latency (R/W): [7:0] domain 2 device latency */
#define PI_BSD_DOM2_LAT_REG	(PI_BASE_REG+0x24)    /* Domain 2 latency */

/* PI dom2 pulse width (R/W): [7:0] domain 2 device R/W strobe pulse width */
#define PI_BSD_DOM2_PWD_REG	(PI_BASE_REG+0x28)    /*   pulse width */

/* PI dom2 page size (R/W): [3:0] domain 2 device page size */
#define PI_BSD_DOM2_PGS_REG	(PI_BASE_REG+0x2C)    /*   page size */

/* PI dom2 release (R/W): [1:0] domain 2 device R/W release duration */
#define PI_BSD_DOM2_RLS_REG	(PI_BASE_REG+0x30)    /*   release duration */

#define	PHYS_TO_K1(x)	((u32)(x)|0xA0000000)	/* physical to kseg1 */
#define	IO_WRITE(addr,data)	(*(volatile u32*)PHYS_TO_K1(addr)=(u32)(data))
#define PI_BASE_REG		0x04600000

extern u8 system_cic;

u8 usbListener() {

    volatile u16 resp;
    volatile u8 cmd;
    usb_buff8 = (u8 *) usb_buff;


    if (evd_fifoRxf())return 0;

    resp = evd_fifoRd(usb_buff, 1);

    if (resp != 0) return 1;

    if (usb_buff8[0] != 'C' || usb_buff8[1] != 'M' || usb_buff8[2] != 'D')return 2;

    cmd = usb_buff8[3];


    switch (cmd) {

        case 'R':
            resp = cmdReadRom();
            if (resp)return 10;
            break;
        case 'W':
            resp = cmdWriteRom();
            if (resp)return 11;
            break;
        case 'T':
            resp = cmdTest();
            if (resp)return 12;
            break;
        case 'F':
            resp = cmdFill();
            if (resp)return 13;
            break;
        case 'S':
            //IO_WRITE(PI_BSD_DOM1_PGS_REG, 0x0c);
            //IO_WRITE(PI_BSD_DOM1_PGS_REG, 0x80);
            //evdSetESaveType(SAVE_TYPE_EEP16k);
            //system_cic = CIC_6102; //TODO: re-enable
            evd_lockRegs();
            IO_WRITE(PI_STATUS_REG, 3);
            sleep(2);
            pif_boot();
            break;
        case 'D':
            //TODO: initiate debug session                    
        break;
        default:
        break;

    }


    return 0;
}

u8 cmdTest() {

    u16 resp;
    usb_buff8[3] = 'k';
    resp = evd_fifoWr(usb_buff, 1);
    if (resp)return 1;

    return 0;

}

u8 cmdFill() {

    u16 resp;
    u32 i;
    //console_printf("fill...\n");

    for (i = 0; i < 512; i++) {
        usb_buff8[i] = 0;
    }
    //console_printf("buff prepared\n");
    //romFill(0, 0x200000, 0); //TODO: re-enable
    //console_printf("fill done\n");

    usb_buff8[3] = 'k';
    resp = evd_fifoWr(usb_buff, 1);

    if (resp)return 1;
    //console_printf("resp sent ok\n");

    return 0;
}

u8 cmdReadRom() {

    u16 resp;
    u16 ptr;
    u16 len;
    u32 addr;
    ptr = 4;


    addr = usb_buff8[ptr++];
    addr <<= 8;
    addr |= usb_buff8[ptr++];
    addr *= 2048;

    len = usb_buff8[ptr++];
    len <<= 8;
    len |= usb_buff8[ptr++];


    resp = evd_fifoWrFromCart(addr, len);
    if (resp)return 1;


    return 0;
}

u8 cmdWriteRom() {

    u16 resp;
    u16 ptr;
    u16 len;
    u32 addr;
    ptr = 4;

    addr = usb_buff8[ptr++];
    addr <<= 8;
    addr |= usb_buff8[ptr++];
    addr *= 2048; 

    len = usb_buff8[ptr++]; 
    len <<= 8;
    len |= usb_buff8[ptr++]; 					 

    resp = evd_fifoRdToCart(addr, len);
    if (resp)return 1;

    return 0;

}
