/* 
 *Interface for initializing, accessing, reading and storing to memory swapping store */

#include <swap.h>

static struct swapblock swapmap[NUM_BLOCKS];
static struct vnode *swapdisk;
 
static struct spinlock swaplock; // needed?

int init_swapdisk(void) {
		
	spinlock_init(&swaplock);

	int i, result;

	/* Initialize clean swapmap */
	for(i = 0; i < NUM_BLOCKS; i++) {
		swapmap[i].as = NULL;
		swapmap[i].vaddr = NULL;
	}

	/* Open swapdisk */
	result = vfs_open("lhd0raw:", O_RDWR, &swapdisk);

	if(result) {
		kprintf("Error: failed to open swap disk in init_swapdisk.\n");
	}

	KASSERT(swapdisk != NULL);

	

	return 0;
}

int read_block(paddr_t pa, vaddr_t va, struct addrspace *as) {
	
	int result;

	spinlock_acquire(&swaplock);
	
	struct iovec iov;
	struct uio u;

	
}

int write_frame(paddr_t pa, vaddr_t va, struct addrspace *as) {

}


