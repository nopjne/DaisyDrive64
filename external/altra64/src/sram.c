//
// Copyright (c) 2017 The Altra64 project contributors
// Portions (c) 2013 saturnu (Alt64) based on libdragon, Neo64Menu, ED64IO, libn64-hkz, libmikmod
// See LICENSE file in the project root for full license information.
//

#include <malloc.h>
#include <string.h>

#include "sys.h"
#include "utils.h"
#include "sram.h"
#include "rom.h"


void PI_Init(void) {
	PI_DMAWait();
	IO_WRITE(PI_STATUS_REG, 0x03);
}

// Inits PI for sram transfer
void PI_Init_SRAM(void) {
	
	
	//IO_WRITE(PI_BSD_DOM2_LAT_REG, 0x05);
	//IO_WRITE(PI_BSD_DOM2_PWD_REG, 0x0C);
	//IO_WRITE(PI_BSD_DOM2_PGS_REG, 0x0D);
	//IO_WRITE(PI_BSD_DOM2_RLS_REG, 0x02);
	
	IO_WRITE(PI_BSD_DOM2_LAT_REG, 0x40);
	IO_WRITE(PI_BSD_DOM2_PWD_REG, 0x40);
	IO_WRITE(PI_BSD_DOM2_PGS_REG, 0x40);
	IO_WRITE(PI_BSD_DOM2_RLS_REG, 0x40);
}

void PI_DMAWait(void) {
	
	
	 /*PI DMA wait

    1. Read PI_STATUS_REG then AND it with 0x3, if its true... then wait until
       it is not true.
       */
       
	while (IO_READ(PI_STATUS_REG) & (PI_STATUS_IO_BUSY | PI_STATUS_DMA_BUSY));
}


void PI_DMAFromSRAM(void *dest, u32 offset, u32 size) {
	

	IO_WRITE(PI_DRAM_ADDR_REG, K1_TO_PHYS(dest));
	IO_WRITE(PI_CART_ADDR_REG, (0xA8000000 + offset));
	 asm volatile ("" : : : "memory");
	IO_WRITE(PI_WR_LEN_REG, (size - 1));
	 asm volatile ("" : : : "memory");
 
	 
	 /*
	PI_DMAWait();

	IO_WRITE(PI_STATUS_REG, 0x03);
	IO_WRITE(PI_DRAM_ADDR_REG, K1_TO_PHYS(dest));
	IO_WRITE(PI_CART_ADDR_REG, (0xA8000000 + offset));
	_data_cache_invalidate_all();
	IO_WRITE(PI_WR_LEN_REG, (size - 1));	
*/
}


void PI_DMAToSRAM(void *src, u32 offset, u32 size) { //void*
	PI_DMAWait();

	IO_WRITE(PI_STATUS_REG, 2);
	IO_WRITE(PI_DRAM_ADDR_REG, K1_TO_PHYS(src));
	IO_WRITE(PI_CART_ADDR_REG, (0xA8000000 + offset));
	_data_cache_invalidate_all();
	 //data_cache_hit_writeback_invalidate(src,size);
	 
	 	/* Write back . nusys - only writeback
	osWritebackDCache((void*)buf_ptr, (s32)size);
	 */
	 	//libdragon equivalent
	// data_cache_hit_writeback (src, size);
	 
	IO_WRITE(PI_RD_LEN_REG, (size - 1));
}

void PI_DMAFromCart(void* dest, void* src, u32 size) {
	PI_DMAWait();

	IO_WRITE(PI_STATUS_REG, 0x03);
	IO_WRITE(PI_DRAM_ADDR_REG, K1_TO_PHYS(dest));
	IO_WRITE(PI_CART_ADDR_REG, K0_TO_PHYS(src));
	//_data_cache_invalidate_all();
	IO_WRITE(PI_WR_LEN_REG, (size - 1));
}


void PI_DMAToCart(void* dest, void* src, u32 size) {
	PI_DMAWait();

	IO_WRITE(PI_STATUS_REG, 0x02);
	IO_WRITE(PI_DRAM_ADDR_REG, K1_TO_PHYS(src));
	IO_WRITE(PI_CART_ADDR_REG, K0_TO_PHYS(dest));
	//_data_cache_invalidate_all();
	IO_WRITE(PI_RD_LEN_REG, (size - 1));
}


// Wrapper to support unaligned access to memory
void PI_SafeDMAFromCart(void *dest, void *src, u32 size) {
	if (!dest || !src || !size) return;

	u32 unalignedSrc  = ((u32)src)  % 2;
	u32 unalignedDest = ((u32)dest) % 8;

	//FIXME: Do i really need to check if size is 16bit aligned?
	if (!unalignedDest && !unalignedSrc && !(size % 2)) {
		PI_DMAFromCart(dest, src, size);
		PI_DMAWait();

		return;
	}

	void* newSrc = (void*)(((u32)src) - unalignedSrc);
	u32 newSize = (size + unalignedSrc) + ((size + unalignedSrc) % 2);

	u8 *buffer = memalign(8, newSize);
	PI_DMAFromCart(buffer, newSrc, newSize);
	PI_DMAWait();

	memcpy(dest, (buffer + unalignedSrc), size);

	free(buffer);
}

