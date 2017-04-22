/* 
 * Interface for initializing, accessing, reading and 
 * storing to memory swapping store 
 */

#include <swap.h>
#include <vfs.h>
#include <lib.h>
#include <uio.h>
#include <kern/fcntl.h>
#include <kern/errno.h>

int init_swapdisk(void) {
		
	spinlock_init(&swaplock);

	int result;

	swapmap = bitmap_create(NUM_BLOCKS);
	KASSERT(swapmap);

	/* Open swapdisk */
	char *filename = NULL;
	filename = kstrdup("lhd0raw:");
	result = vfs_open(filename, O_RDWR, 0, &swapdisk);

	if(result) {
		kprintf("Error: failed to open swap disk in init_swapdisk.\n");
	}
	KASSERT(swapdisk);

	return 0;
}

int swap_in(paddr_t pa, unsigned blocknum) {

	int result;
		
	result = bitmap_isset(swapmap, blocknum);

	if(!result) {
		return EINVAL;
	} 

	result = read_block(pa, (off_t) blocknum);	
	if(result) {
		kprintf("Failed to read swap disk block.\n");
		return result;
	}

	struct uio u;
	struct iovec iov;
	char zeroes[PAGE_SIZE];

	bzero(zeroes, PAGE_SIZE);

	uio_kinit(&iov, &u, (void *) zeroes, PAGE_SIZE,
		  (off_t) blocknum, UIO_WRITE);
	
	result = VOP_WRITE(swapdisk, &u);

	if(result) {
		kprintf("Failed to zero out deallocated disk block.\n");
		return result;
	}

	clear_map_block(blocknum);
	
	return 0;
}

int swap_out(paddr_t pa, unsigned *blocknum) {
	
	int result;
	
	result = get_free_block(blocknum);

	if(result) {
		kprintf("Swap out failed: No free blocks on swap disk.\n");
		return result;	
	}

	result = write_frame(pa, (off_t) *blocknum);
	if(result) {
		kprintf("Swap out failed: Writing frame failed.\n");
		return result;
	}

	cm_free_frames(pa);

	return 0;
}

int read_block(paddr_t pa, off_t blocknum) {
	
	int result;
	vaddr_t frame_loc = PADDR_TO_KVADDR(pa);
	
	struct uio u;
	struct iovec iov;

	uio_kinit(&iov, &u, (void *) frame_loc, PAGE_SIZE,
		  blocknum, UIO_READ);	

	spinlock_acquire(&swaplock);

	result = VOP_READ(swapdisk, &u);

	spinlock_release(&swaplock);
		
	return result;
}

int write_frame(paddr_t pa, off_t blocknum) {

	int result;
	vaddr_t frame_loc = PADDR_TO_KVADDR(pa);
		
	struct uio u;
	struct iovec iov;

	uio_kinit(&iov, &u, (void *) frame_loc, PAGE_SIZE,
		  (off_t) blocknum, UIO_WRITE);
	
	spinlock_acquire(&swaplock);

	result = VOP_WRITE(swapdisk, &u);

	spinlock_release(&swaplock);
	
	return result;
}

int get_free_block(unsigned *idxptr) {
	
	int result;

	spinlock_acquire(&swaplock);

	result =  bitmap_alloc(swapmap, idxptr);	

	spinlock_release(&swaplock);

	return result;
}

void clear_map_block(unsigned idx) {
		
	spinlock_acquire(&swaplock);

	bitmap_unmark(swapmap, idx);

	spinlock_release(&swaplock);
}
