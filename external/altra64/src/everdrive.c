// DaisyDrive64 interface implementation.

#include "everdrive.h"
#include "sys.h"
#include "n64sys.h"
#include "errors.h"
#include "dma.h"
#include <string.h>

u8 evd_isSDBusy();

volatile u32 *daisy_regs = (u32 *) DAISY_BASE_REGISTER;
u8 sdRead(u32 sector, u8 *buff, u16 count) 
{
    u8* tempBuff = buff;
    for (u32 repeats = count; repeats > 0; repeats -= 1) {
        // Reading a chunk of SDmmc to local memory.
        io_write((uint32_t)&(daisy_regs[REG_DMA_LEN]), count);
        io_write((uint32_t)&(daisy_regs[REG_DMA_RAM_ADDR]), sector);
        io_write((uint32_t)&(daisy_regs[REG_DMA_CFG]), DCFG_FIFO_TO_RAM);
        io_write((uint32_t)&(daisy_regs[REG_EXECUTE_FUNCTION]), SD_CARD_READ_SECTOR);
        while (evd_isSDBusy()) {}
        dma_read_s(tempBuff, (unsigned long)&(daisy_regs[REG_DMA_DATA]), 512);
        dma_wait();
        while (evd_isDmaBusy()) {}
        tempBuff += 512;
    }

    return 0;
}

u8 sdWrite(u32 sector, const u8 *buff, u16 count)
{
    const u8* tempBuff = buff;
    for (u32 repeats = count; repeats > 0; repeats -= 1) {
        io_write((uint32_t)&(daisy_regs[REG_DMA_LEN]), count);
        io_write((uint32_t)&(daisy_regs[REG_DMA_RAM_ADDR]), sector);
        io_write((uint32_t)&(daisy_regs[REG_DMA_CFG]), DCFG_FIFO_TO_RAM);
        dma_write_s(tempBuff, (unsigned long)&(daisy_regs[REG_DMA_DATA]), 512);
        dma_wait();
        io_write((uint32_t)&(daisy_regs[REG_EXECUTE_FUNCTION]), SD_CARD_WRITE_SECTOR);
        while (evd_isDmaBusy()) {}
        tempBuff += 512;
    }

    return 0;
}

void evd_setCfgBit(u8 option, u8 state)
{
}

void evd_init() {
    daisy_regs[REG_EXECUTE_FUNCTION] = ENABLE_MENU_FUNCTIONS;
    while (evd_isSDBusy()) {}
}

// USB functions are unsupported for now. The DaisyDrive64 does have support for USB, however this has not been implemented yet here.
u8 evd_fifoRxf() { return 0; }
u8 evd_fifoTxe() { return 0; }
u8 evd_fifoRd(void *buff, u16 blocks) { return 0; }
u8 evd_fifoWr(void *buff, u16 blocks) { return 0; }
u8 evd_fifoRdToCart(u32 cart_addr, u16 blocks) { return 0; }
u8 evd_fifoWrFromCart(u32 cart_addr, u16 blocks) { return 0; }

u8 evd_isSDBusy()
{
    if ((io_read((uint32_t)&(daisy_regs[REG_STATUS])) & DAISY_STATUS_BIT_SD_BUSY) != 0) {
        return 1;
    } else {
        return 0;
    }
}

u8 evd_isDmaBusy()
{
    if ((io_read((uint32_t)&(daisy_regs[REG_STATUS])) & DAISY_STATUS_BIT_DMA_BUSY) != 0) {
        return 1;
    } else {
        return 0;
    }
}

u8 evd_isDmaTimeout()
{
    return ((io_read((uint32_t)&(daisy_regs[REG_STATUS])) & DAISY_STATUS_BIT_DMA_TIMEOUT) != 0) ? 0 : 1;
}

u8 evd_SPI(u8 dat);
void evd_mmcSetDmaSwap(u8 state);
u8 evd_mmcReadToCart(u32 cart_addr, u32 len);
void evd_ulockRegs(void) {}
void evd_lockRegs() {}
u16 evd_readReg(u8 reg)
{
    volatile u32 x;
    x = daisy_regs[REG_STATUS];
    return (u16)(daisy_regs[reg]); // Converts to u16? Weird.
}

void evd_writeReg(u8 reg, u16 val)
{
    //volatile u8 x;
    //x = daisy_regs[REG_STATUS];
    //daisy_regs[reg] = val;
}
void evd_setSaveType(u8 type)
{
    io_write((uint32_t)&(daisy_regs[REG_FUNCTION_PARAMETER]), type);
    io_write((uint32_t)&(daisy_regs[REG_EXECUTE_FUNCTION]), SET_SAVE_TYPE);
}

u16 evd_getFirmVersion() {
    io_write((uint32_t)&(daisy_regs[REG_EXECUTE_FUNCTION]), GET_FW_VERSION);
    while (evd_isDmaBusy()) {}
    return (u16)io_read((uint32_t)&((daisy_regs[REG_FUNCTION_PARAMETER])));
}

void evd_spiSSOn();
void evd_spiSSOff();
u8 evd_isSpiBusy();
void evd_setSpiSpeed(u8 speed);
void evd_setDmaCallback(void (*callback)());

//firmware upload
void bi_load_firmware(u8 *firm) {
    // DaisyDrive64 does not support firmware updates. Keeping the function here so main.c does not need to change.
}

void bi_speed25() {
    // DaisyDrive64 only works with max SD speed. Keeping the function here so main.c does not need to change.
}

void bi_speed50() {
    // DaisyDrive64 only works with max SD speed. Keeping the function here so main.c does not need to change.
}

void evd_mmcSetDmaSwap(u8 state) {}


void evd_SDdatReadMode(u8 bit4mode) {}
u8 evd_SPI(u8 dat) { return 0; }
void evd_spiSSOff() {}
u8 evd_mmcReadToCart(u32 cart, u32 len) { return 0; }
u8 evd_isSpiBusy() {return 0;}
u8 evd_isSDMode() { return 0;}

u8 daisyDrive_uploadRom(char *Path)
{
    dma_write_s(Path, (unsigned long)&(daisy_regs[REG_DMA_DATA]), strlen(Path) + 1);
    dma_wait();
    io_write((uint32_t)&(daisy_regs[REG_EXECUTE_FUNCTION]), UPLOAD_ROM);
    while (evd_isDmaBusy()) {}
}