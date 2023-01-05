//
// Copyright (c) 2017 The Altra64 project contributors
// Portions (c) 2013 saturnu (Alt64) based on libdragon, Neo64Menu, ED64IO, libn64-hkz, libmikmod
// See LICENSE file in the project root for full license information.
//

#include "strlib.h"
#include "types.h"

char *strcpytrim(char *d, // destination
                 char *s, // source
                 int mode,
                 char *delim
                 ) {
    char *o = d; // save orig
    char *e = 0; // end space ptr.
    char dtab[256] = {0};
    if (!s || !d) return 0;

    if (!delim) delim = " \t\n\f";
    while (*delim) 
        dtab[*delim++] = 1;

    while ( (*d = *s++) != 0 ) { 
        if (!dtab[*d]) { // Not a match char
            e = 0;       // Reset end pointer
        } else {
            if (!e) e = d;  // Found first match.

            if ( mode == STRLIB_MODE_ALL || ((mode != STRLIB_MODE_RIGHT) && (d == o)) ) 
                continue;
        }
        d++;
    }
    if (mode != STRLIB_MODE_LEFT && e) { // for everything but trim_left, delete trailing matches.
        *e = 0;
    }
    return o;
}

// perhaps these could be inlined in strlib.h
char *strtriml(char *d, char *s) { return strcpytrim(d, s, STRLIB_MODE_LEFT, 0); }
char *strtrimr(char *d, char *s) { return strcpytrim(d, s, STRLIB_MODE_RIGHT, 0); }
char *strtrim(char *d, char *s) { return strcpytrim(d, s, STRLIB_MODE_BOTH, 0); }
char *strstrlibkill(char *d, char *s) { return strcpytrim(d, s, STRLIB_MODE_ALL, 0); }

char *triml(char *s) { return strcpytrim(s, s, STRLIB_MODE_LEFT, 0); }
char *trimr(char *s) { return strcpytrim(s, s, STRLIB_MODE_RIGHT, 0); }
char *trim(char *s) { return strcpytrim(s, s, STRLIB_MODE_BOTH, 0); }
char *strlibkill(char *s) { return strcpytrim(s, s, STRLIB_MODE_ALL, 0); }

u16 strcon(u8 *str1, u8 *str2, u8 *dst, u16 max_len) {
    
        u16 len = 0;
        max_len -= 1;
    
        while (*str1 != 0 && len < max_len) {
            *dst++ = *str1++;
            len++;
        }
    
        while (*str2 != 0 && len < max_len) {
            *dst++ = *str2++;
            len++;
        }
        *dst++ = 0;
        return len;
    
    }

u8 streq(u8 *str1, u8 *str2) {
    
        u8 s1;
        u8 s2;
    
        for (;;) {
            s1 = *str1++;
            s2 = *str2++;
            if (s1 >= 'a' && s1 <= 'z')s1 -= 0x20;
            if (s2 >= 'a' && s2 <= 'z')s2 -= 0x20;
    
            if (s1 != s2) return 0;
    
            if (*str1 == 0 && *str2 == 0)return 1;
        }
    }
    
    u8 streql(u8 *str1, u8 *str2, u8 len) {
    
        u8 s1;
        u8 s2;
        while (len--) {
    
            s1 = *str1++;
            s2 = *str2++;
            if (s1 >= 'a' && s1 <= 'z')s1 -= 0x20;
            if (s2 >= 'a' && s2 <= 'z')s2 -= 0x20;
    
            if (s1 != s2) return 0;
        }
    
        return 1;
    }
    
    u16 strContain(u8 *target, u8 *str) {
    
        u16 targ_len = slen(target);
        u16 eq_len;
    
    
        for (eq_len = 0; eq_len < targ_len;) {
    
            if (*str == 0)return 0;
            if (*str++ == target[eq_len]) {
                eq_len++;
            } else {
                eq_len = 0;
            }
        }
    
        if (eq_len != targ_len)return 0;
        return 1;
    
    }
    
    u8 slen(u8 *str) {
    
        u8 len = 0;
        while (*str++)len++;
        return len;
    }
    
    u8 scopy(u8 *src, u8 *dst) {
    
        u8 len = 0;
        while (*src != 0) {
            *dst++ = *src++;
            len++;
        }
        *dst = 0;
        return len;
    }
    
    void strhicase(u8 *str, u8 len) {
    
        if (len) {
            while (len--) {
                if (*str >= 'a' && *str <= 'z')*str -= 0x20;
                str++;
            }
        } else {
            while (*str != 0) {
                if (*str >= 'a' && *str <= 'z')*str -= 0x20;
                str++;
            }
        }
    
    }