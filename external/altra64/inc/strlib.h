//
// Copyright (c) 2017 The Altra64 project contributors
// Portions (c) 2013 saturnu (Alt64) based on libdragon, Neo64Menu, ED64IO, libn64-hkz, libmikmod
// See LICENSE file in the project root for full license information.
//

#ifndef _STRLIB_H
#define	_STRLIB_H

#include "types.h"

enum strtrim_mode_t {
    STRLIB_MODE_ALL       = 0, 
    STRLIB_MODE_RIGHT     = 0x01, 
    STRLIB_MODE_LEFT      = 0x02, 
    STRLIB_MODE_BOTH      = 0x03
};

char *strcpytrim(char *d, // destination
                 char *s, // source
                 int mode,
                 char *delim
                 );

char *strtriml(char *d, char *s);
char *strtrimr(char *d, char *s);
char *strtrim(char *d, char *s); 
char *strstrlibkill(char *d, char *s);

char *triml(char *s);
char *trimr(char *s);
char *trim(char *s);
char *strlibkill(char *s);

void strhicase(u8 *str, u8 len);
u16 strcon(u8 *str1, u8 *str2, u8 *dst, u16 max_len);
u8 slen(u8 *str);
u8 scopy(u8 *src, u8 *dst);

u8 streq(u8 *str1, u8 *str2);
u8 streql(u8 *str1, u8 *str2, u8 len);

u16 strContain(u8 *target, u8 *str);

#endif
