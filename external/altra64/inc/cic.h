//
// Copyright (c) 2017 The Altra64 project contributors
// See LICENSE file in the project root for full license information.
//

#ifndef _CIC_H
#define	_CIC_H

#define CIC_6101 1
#define CIC_6102 2
#define CIC_6103 3
#define CIC_5101 4 //aleck64
//#define CIC_6104 6104 //Unused in any game so used for Aleck64 instead
#define CIC_6105 5
#define CIC_6106 6
#define CIC_5167 7 //64dd conv

int get_cic(unsigned char *rom_data);

#endif
