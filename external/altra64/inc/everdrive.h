//
// Copyright (c) 2017 The Altra64 project contributors
// Portions (c) 2011 KRIK
// See LICENSE file in the project root for full license information.
//

#ifndef _EVERDRIVE_H
#define	_EVERDRIVE_H

#include "types.h"

#define OS_VER "1.29"

#define ROM_LEN  0x4000000
#define ROM_ADDR 0xb0000000
#define ROM_END_ADDR (0xb0000000 + 0x4000000)


#define SAVE_TYPE_OFF 0
#define SAVE_TYPE_SRAM 1
#define SAVE_TYPE_SRAM96 2
#define SAVE_TYPE_EEP4k 3
#define SAVE_TYPE_EEP16k 4
#define SAVE_TYPE_FLASH 5

#define DMA_BUFF_ADDR (ROM_LEN - 0x100000)
#if 0
#define REG_CFG 0
#define REG_STATUS 1
#define REG_DMA_LEN 2
#define REG_DMA_RAM_ADDR 3

#else
// This is kept the same as the everdrive. No need to setup the TLB differently.
#define DAISY_BASE_REGISTER 0xA8040000 
enum DAISY_FW_FUNCTION {
    GET_FW_VERSION,
    ENABLE_MENU_FUNCTIONS,
    DISABLE_MENU_FUNCTIONS,
    SD_CARD_READ_SECTOR,
    SD_CARD_WRITE_SECTOR,
    UPLOAD_ROM,
    UPLOAD_ROM_EX,
    SET_SAVE_TYPE,
};

#define DAISY_STATUS_BIT_ROM_LOADING 0x00000001
#define DAISY_STATUS_BIT_SD_BUSY     0x00000002
#define DAISY_STATUS_BIT_MENU_MODE   0x00000004
#define DAISY_STATUS_BIT_DMA_BUSY    0x00000008
#define DAISY_STATUS_BIT_DMA_TIMEOUT 0x00000010
#define DCFG_FIFO_TO_RAM 0
#define DCFG_RAM_TO_FIFO 1

enum DAISY_REGISTERS {
    REG_STATUS,
    REG_EXECUTE_FUNCTION,
    REG_FUNCTION_PARAMETER,
    REG_DMA_CFG,
    REG_DMA_LEN,
    REG_DMA_RAM_ADDR, // There are 512 bytes past this register to receive or send DMA.
    REG_DMA_DATA
};
#endif

u8 daisyDrive_uploadRom(char **Path, uint32_t *Offsets, uint32_t Count);

#define REGS_BASE 0xA8040000
#define REG_CFG 0
#define REG_MSG 4
//#define REG_DMA_CFG 5
#define REG_SPI 6
#define REG_SPI_CFG 7
#define REG_KEY 8
#define REG_SAV_CFG 9
#define REG_SEC 10
#define REG_VER 11

#define REG_CFG_CNT 16
#define REG_CFG_DAT 17
#define REG_MAX_MSG 18
#define REG_CRC 19


#define DCFG_SD_TO_RAM 1
#define DCFG_RAM_TO_SD 2
//#define DCFG_FIFO_TO_RAM 3
//#define DCFG_RAM_TO_FIFO 4

#define ED_CFG_SDRAM_ON 0
#define ED_CFG_SWAP 1
#define ED_CFG_WR_MOD 2
#define ED_CFG_WR_ADDR_MASK 3

void evd_setCfgBit(u8 option, u8 state);

void evd_init();
u8 evd_fifoRxf();
u8 evd_fifoTxe();
u8 evd_isDmaBusy();
u8 evd_isDmaTimeout();
u8 evd_fifoRd(void *buff, u16 blocks);
u8 evd_fifoWr(void *buff, u16 blocks);
u8 evd_fifoRdToCart(u32 cart_addr, u16 blocks);
u8 evd_fifoWrFromCart(u32 cart_addr, u16 blocks);

u8 evd_SPI(u8 dat);
void evd_mmcSetDmaSwap(u8 state);
u8 evd_mmcReadToCart(u32 cart_addr, u32 len);

void evd_ulockRegs(void);
void evd_lockRegs();
u16 evd_readReg(u8 reg);
void evd_writeReg(u8 reg, u16 val);
void evd_setSaveType(u8 type);


u8 romLoadSettingsFromSD();
u8 romSaveInfoToLog();
void evd_writeMsg(u8 dat);
u8 evd_readMsg();
u16 evd_getFirmVersion();

void evd_spiSSOn();
void evd_spiSSOff();
u8 evd_isSpiBusy();
void evd_setSpiSpeed(u8 speed);

void evd_SDcmdWriteMode(u8 bit1_mode);
void evd_SDcmdReadMode(u8 bit1_mode);
void evd_SDdatWriteMode(u8 bit4_mode);
void evd_SDdatReadMode(u8 bit4_mode);
void evd_enableSDMode();
void evd_enableSPIMode();
u8 evd_isSDMode();
void evd_setDmaCallback(void (*callback)());


//firmware upload
extern u32 bi_reg_rd(u32 reg);
extern void bi_reg_wr(u32 reg, u32 data);
void bi_init();
void bi_load_firmware(u8 *firm);
void bi_speed25();
void bi_speed50();

/*
u8 evd_mmcWriteNextBlock(void *dat);
u8 evd_mmcOpenWrite(u32 addr);
u8 evd_mmcWriteBlock(void *dat, u32 addr);
u8 evd_mmcInit();
u8 evd_mmcReadBlock(void *dat, u32 addr);
u8 evd_mmcOpenRead(u32 addr);
u8 evd_mmcReadNextBlock(void *dat);
void evd_mmcCloseRW();
 */


#endif	/* _EVERDRIVE_H */
