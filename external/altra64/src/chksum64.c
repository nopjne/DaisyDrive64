/*
    checksum rom in psram, based on

    chksum64 V1.2, a program to calculate the ROM checksum of Nintendo64 ROMs.
    Copyright (C) 1997  Andreas Sterbenz (stan@sbox.tu-graz.ac.at)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
*/

#include <libdragon.h>

#include "sys.h"
#include "chksum64.h"


#define BUFSIZE 0x8000
#define SDRAM_START 0xb0000000

#define CHECKSUM_START 0x1000
#define CHECKSUM_LENGTH 0x100000L
#define CHECKSUM_HEADERPOS 0x10
#define CHECKSUM_END (CHECKSUM_START + CHECKSUM_LENGTH)

#define CHECKSUM_STARTVALUE 0xf8ca4ddc

#define ROL(i, b) (((i)<<(b)) | ((i)>>(32-(b))))

#define BYTES2LONG(b)    ( (((b)[0] & 0xffL) << 24) | \
                           (((b)[1] & 0xffL) << 16) | \
                           (((b)[2] & 0xffL) <<  8) | \
                           (((b)[3] & 0xffL)) )

#define LONG2BYTES(l, b)     (b)[0] = ((l)>>24)&0xff; \
                             (b)[1] = ((l)>>16)&0xff; \
                             (b)[2] = ((l)>> 8)&0xff; \
                             (b)[3] = ((l)    )&0xff;


static unsigned char __attribute__((aligned(16))) buffer1[BUFSIZE];


void checksum_sdram(void)
{
  unsigned int sum1, sum2, offs;

  {
    unsigned int i;
    unsigned int c1, k1, k2;
    unsigned int t1, t2, t3, t4;
    unsigned int t5, t6;
    unsigned int n;
    unsigned int clen = CHECKSUM_LENGTH;

    t1 = CHECKSUM_STARTVALUE;
    t2 = CHECKSUM_STARTVALUE;
    t3 = CHECKSUM_STARTVALUE;
    t4 = CHECKSUM_STARTVALUE;
    t5 = CHECKSUM_STARTVALUE;
    t6 = CHECKSUM_STARTVALUE;

    offs = CHECKSUM_START;

    for( ;; ) {
      n = (BUFSIZE < clen) ? BUFSIZE : clen;
      dma_read_s(buffer1, SDRAM_START+offs, n);
      data_cache_hit_writeback_invalidate(buffer1,n);	
      
      offs += n;

      for( i=0; i<n; i+=4 ) {
        c1 = BYTES2LONG(&buffer1[i]);
        k1 = t6 + c1;
        if( k1 < t6 ) t4++;
        t6 = k1;
        t3 ^= c1;
        k2 = c1 & 0x1f;
        k1 = ROL(c1, k2);
        t5 += k1;
        if( c1 < t2 ) {
          t2 ^= k1;
        } else {
          t2 ^= t6 ^ c1;
        }
        t1 += c1 ^ t5;
      }
      clen -= n;
      if (!clen) break;
    }
    sum1 = t6 ^ t4 ^ t3;
    sum2 = t5 ^ t2 ^ t1;
  }
  LONG2BYTES(sum1, &buffer1[0]);
  LONG2BYTES(sum2, &buffer1[4]);
  dma_write_s(buffer1, SDRAM_START+CHECKSUM_HEADERPOS, 8);
  return;
}
