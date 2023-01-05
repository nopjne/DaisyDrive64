/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2016        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"
#include <diskio.h>		/* FatFs lower layer API */
#include "diskio.h"
#if 1
#include "sd.h"
#include "assert.h"
#include "string.h"

DSTATUS disk_status_daisydrive (void);
DSTATUS disk_initialize_daisydrive (void);
DRESULT disk_read_daisydrive (BYTE *buff, LBA_t sector, UINT count);
DRESULT disk_write_daisydrive (
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
);
DRESULT disk_ioctl_daisydrive (
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
);


/* Definitions of physical drive number for each drive */
#define DEV_RAM		0	/* Example: Map Ramdisk to physical drive 0 */
#define DEV_MMC		1	/* Example: Map MMC/SD card to physical drive 1 */
#define DEV_USB		2	/* Example: Map USB MSD to physical drive 2 */

//extern struct fat_disk_t;
typedef struct
{
	DSTATUS (*disk_initialize)(void);
	DSTATUS (*disk_status)(void);
	DRESULT (*disk_read)(BYTE* buff, LBA_t sector, UINT count);
	DRESULT (*disk_write)(const BYTE* buff, LBA_t sector, UINT count);
	DRESULT (*disk_ioctl)(BYTE cmd, void* buff);
} fat_disk_t;

extern fat_disk_t fat_disks[1];

static fat_disk_t fat_disk_daisydrive =
{
	disk_initialize_daisydrive,
	disk_status_daisydrive,
	disk_read_daisydrive,
	disk_write_daisydrive,
	disk_ioctl_daisydrive
};

void AltraDiskInit(void)
{
	assert(sizeof(fat_disks) == sizeof(fat_disk_daisydrive));
	memcpy(fat_disks, &fat_disk_daisydrive, sizeof(fat_disks[0]));
}
/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status_daisydrive (
	void		/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat;
	// int result;

	// switch (pdrv) {
	// case DEV_RAM :
	// 	result = RAM_disk_status();

	// 	// translate the reslut code here

	// 	return stat;

	// case DEV_MMC :
	// 	result = MMC_disk_status();

	// 	// translate the reslut code here

	// 	return stat;

	// case DEV_USB :
	// 	result = USB_disk_status();

	// 	// translate the reslut code here

	// 	return stat;
	// }
	// return STA_NOINIT;

	//if(pdrv)
    //{
    //    return STA_NOINIT;  
    //}
    return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize_daisydrive (
	void				/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat;
	int result;

	// switch (pdrv) {
	// case DEV_RAM :
	// 	result = RAM_disk_initialize();

	// 	// translate the reslut code here

	// 	return stat;

	// case DEV_MMC :
	// 	result = MMC_disk_initialize();

	// 	// translate the reslut code here

	// 	return stat;

	// case DEV_USB :
	// 	result = USB_disk_initialize();

	// 	// translate the reslut code here

	// 	return stat;
	// }

	stat=sdInit();  //SD card initialization

	if(stat == STA_NODISK)
	{
		return STA_NODISK;
	}
	else if(stat != 0)
	{
		return STA_NOINIT;  
	}
	else
	{
		return 0;           
	}
	
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read_daisydrive (
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	DRESULT res;
	// int result;

	// switch (pdrv) {
	// case DEV_RAM :
	// 	// translate the arguments here

	// 	result = RAM_disk_read(buff, sector, count);

	// 	// translate the reslut code here

	// 	return res;

	// case DEV_MMC :
	// 	// translate the arguments here

	// 	result = MMC_disk_read(buff, sector, count);

	// 	// translate the reslut code here

	// 	return res;

	// case DEV_USB :
	// 	// translate the arguments here

	// 	result = USB_disk_read(buff, sector, count);

	// 	// translate the reslut code here

	// 	return res;
	// }

	// return RES_PARERR;
	res = sdRead((unsigned long)sector, buff, count);
		
    if(res == 0x00)
    {
        return RES_OK;
    }
    else
    {
        return RES_ERROR;
    }
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

DRESULT disk_write_daisydrive (
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	DRESULT res;
	// int result;

	// switch (pdrv) {
	// case DEV_RAM :
	// 	// translate the arguments here

	// 	result = RAM_disk_write(buff, sector, count);

	// 	// translate the reslut code here

	// 	return res;

	// case DEV_MMC :
	// 	// translate the arguments here

	// 	result = MMC_disk_write(buff, sector, count);

	// 	// translate the reslut code here

	// 	return res;

	// case DEV_USB :
	// 	// translate the arguments here

	// 	result = USB_disk_write(buff, sector, count);

	// 	// translate the reslut code here

	// 	return res;
	// }

	//return RES_PARERR;
	
	res = sdWrite(sector, buff, count);
	
	if(res == 0)
    {
        return RES_OK;
    }
    else
    {
        return RES_ERROR;
    }
}



/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl_daisydrive (
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res;
	// int result;

	// switch (pdrv) {
	// case DEV_RAM :

	// 	// Process of the command for the RAM drive

	// 	return res;

	// case DEV_MMC :

	// 	// Process of the command for the MMC/SD card

	// 	return res;

	// case DEV_USB :

	// 	// Process of the command the USB drive

	// 	return res;
	// }

	switch (cmd) {
		case CTRL_SYNC:
			return RES_OK;
		case GET_SECTOR_SIZE:
			*(WORD*)buff = 512;
			return RES_OK;
		case GET_SECTOR_COUNT:
			//*(DWORD*)buff = sdGetSectors();
			return RES_OK;
		case GET_BLOCK_SIZE:
			//*(DWORD*)buff = sdGetBlockSize();
			return RES_OK;
		}

	return RES_PARERR;
}

DWORD get_fattime (void)
{
	//TODO: can we use the V3 RTC?
	return 0;
}

#endif