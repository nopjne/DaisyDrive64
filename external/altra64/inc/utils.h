//
// Copyright (c) 2017 The Altra64 project contributors
// Portions (c) 2013 saturnu (Alt64) based on libdragon, Neo64Menu, ED64IO, libn64-hkz, libmikmod
// See LICENSE file in the project root for full license information.
//


#ifndef _UTILS_H
#define	_UTILS_H

#if !defined(MIN)
    #define MIN(a, b) ({ \
        __typeof__ (a) _a = (a); \
        __typeof__ (b) _b = (b); \
        _a < _b ? _a : _b; \
    })
#endif

#if !defined(MAX)
    #define MAX(a, b) ({ \
        __typeof__ (a) _a = (a); \
        __typeof__ (b) _b = (b); \
        _a > _b ? _a : _b; \
    })
#endif


void _sync_bus(void);
void _data_cache_invalidate_all(void);

// End ...


void restoreTiming(void);

void simulate_boot(u32 boot_cic, u8 bios_cic, u32 *cheat_list[2]);


int get_cic_save(char *cartid, int *cic, int *save);
//const char* saveTypeToExtension(int type);
const char* saveTypeToExtension(int type, int etype);
int saveTypeToSize(int type);
int getSaveFromCart(int stype, uint8_t *buffer);
int pushSaveToCart(int stype, uint8_t *buffer);

int getSRAM(  uint8_t *buffer,int size);
int getSRAM32(  uint8_t *buffer);
int getSRAM96(  uint8_t *buffer);
int getEeprom4k(  uint8_t *buffer);
int getEeprom16k(  uint8_t *buffer);
int getFlashRAM(  uint8_t *buffer);

int setSRAM(uint8_t *buffer,int size);
int setSRAM32(  uint8_t *buffer);
int setSRAM96(  uint8_t *buffer);
int setEeprom4k(  uint8_t *buffer);
int setEeprom16k( uint8_t *buffer);
int setFlashRAM(  uint8_t *buffer);

#endif
