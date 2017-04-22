/* 
 * Declarations for swapping structures and interface 
 */

#ifndef _SWAP_H_
#define _SWAP_H_

#include <types.h>
#include <vnode.h>
#include <addrspace.h>
#include <bitmap.h>

/* TODO: design a more refined size system here */
#define SWAPDISK_SIZE 	1048576
#define NUM_BLOCKS	SWAPDISK_SIZE / PAGE_SIZE

/* Structures for organizing backing store */
static struct vnode *swapdisk; 
static struct spinlock swaplock; // needed?
static struct bitmap *swapmap;

/* initializes swap disk for memory swapping */
int init_swapdisk(void);

/* Swaps data in swap block blocknum into memory at pa */
int swap_in(paddr_t pa, unsigned blocknum);

/* Swaps data at pa onto disk, storing block index in blocknum. */ 
int swap_out(paddr_t pa, unsigned *blocknum);

/* Reads page from swap disk into physical memory if page on swap disk */
int read_block(paddr_t pa, off_t blocknum);

/* Writes page from physical memory into swap disk */
int write_frame(paddr_t pa, off_t blocknum);

/* Finds a free block on swap disk and sets it as allocated */
int get_free_block(unsigned *idxptr);

/* Sets block at idx as unallocated */
void clear_map_block(unsigned idx);

/* To me, the logic of the following functions should be in the coremap/clean sweeper */
/* Writes page to disk if dirty and removes from memory */
//int evict_page();

/* Writes page in memory to swap disk to make clean */
/*int clean_page(???)*/ 

#endif /* _SWAP_H_ */
