/* 
 * Declarations for swapping structures and interface 
 */

#ifndef _SWAP_H_
#define _SWAP_H_

#include <addrspace.h>

#define SWAPDISK_SIZE 	1048576
#define NUM_BLOCKS	SWAPDISK_SIZE / PAGE_SIZE

struct swapblock {
	struct addrspace *as;
	vaddr_t vaddr;
};

/* initializes swap disk for memory swapping */
int init_swapdisk(void);

/* Reads page from swap disk into physical memory if page on swap disk */
int read_block(paddr_t pa, vaddr_t va, struct addrspace *as);

/* Writes page from physical memory into swap disk */
int write_frame(paddr_t pa, vaddr_t va, struct addrspace *as);

/* To me, the logic of the following functions should be in the coremap/clean sweeper */
/* Writes page to disk if dirty and removes from memory */
//int evict_page();

/* Writes page in memory to swap disk to make clean */
/*int clean_page(???)*/ 

#endif /* _SWAP_H_ */
