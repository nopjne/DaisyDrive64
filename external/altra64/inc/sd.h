//
// Copyright (c) 2017 The Altra64 project contributors
// Portions (c) 2011 KRIK
// See LICENSE file in the project root for full license information.
//

#ifndef _SD_H
#define	_SD_H

#include "types.h"

u8 sdGetInterface();
u8 sdInit();
u8 sdRead(u32 sector, u8 *buff, u16 count);
u8 sdWrite(u32 sector, const u8 *buff, u16 count);

void sdSetInterface(u32 interface);



#define WAIT 1024

#define DISK_IFACE_SPI 0
#define DISK_IFACE_SD 1


#endif	/* _SD_H */
