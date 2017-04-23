#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <swap.h>

/*
 *
 */

/* under dumbvm, always have 72k of user stack */
/* (this must be > 64K so argument blocks of size ARG_MAX will fit) */
#define DUMBVM_STACKPAGES    18


static struct coremap cm;
static unsigned vm_bootstrapped = 0;

/*
 * Wrap ram_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

void init_coremap(void) {

	spinlock_init(&cm.cm_lock);
	
	cm.last_allocated = -1;
	cm.oldest = -1;

	uint32_t memsize = ram_getsize();
	unsigned max_coremap_entries = memsize / PAGE_SIZE;
	cm.entries = (struct coremap_entry *)
			  kmalloc(max_coremap_entries * sizeof(struct coremap_entry));
	KASSERT(cm.entries);
	/*
	 * At this stage in the bootstrap process, kmalloc will be ram_steal()ing if
	 * it needs a fresh page, meaning firstpaddr will be changing.
	 * Our call to kmalloc for the coremap can cause the result of
	 * ram_getsize() to change (if coremap allocation
	 * cannot be handled by an existing subpage), so we call it again to
	 * get the updated memsize. This works only because we modified
	 * ram_getsize(), which wasn't being used by anything.
	 */
	memsize = ram_getsize();
	/* num_frames will now track how many coremap entries to manage/allocate */
	cm.num_frames = memsize / PAGE_SIZE;
	cm.first_mapped_paddr = ram_getfirstfree();

	unsigned i;
	for(i = 0; i < cm.num_frames; i++) {
		//(coremap.entries + i)->pte = NULL;
		(cm.entries + i)->tlb_idx = -1;
		(cm.entries + i)->next_allocated = -1;
		(cm.entries + i)->allocated = 0;
		(cm.entries + i)->dirty = 0;
		(cm.entries + i)->more_contig_frames = 0;
		(cm.entries + i)->kern = 0;
	}
}
void
vm_bootstrap(void)
{
	init_swapdisk();	
	init_coremap();
	vm_bootstrapped = 1;
}

/*
 * Check if we're in a context that can sleep. While most of the
 * operations in dumbvm don't in fact sleep, in a real VM system many
 * of them would. In those, assert that sleeping is ok. This helps
 * avoid the situation where syscall-layer code that works ok with
 * dumbvm starts blowing up during the VM assignment.
 */
static
void
vm_can_sleep(void)
{
	if (CURCPU_EXISTS()) {
		/* must not hold spinlocks */
		KASSERT(curcpu->c_spinlocks == 0);

		/* must not be in an interrupt handler */
		KASSERT(curthread->t_in_interrupt == 0);
	}
}

/* Used to get npages physical pages for kernel allocation,
 * or 1 page for non-kernel allocation. If kernel pages, 
 * pte should be NULL. 
 */
static
paddr_t
getppages(struct pageTableEntry_t *pte, unsigned long npages)
{
	paddr_t addr;
	/* Before vm_bootstrapped, we are stealing ram. After, coremap manages mem */
	if(vm_bootstrapped) {

		unsigned i, j;
		int entry_found = 0;
		int entry_idx;

		spinlock_acquire(&cm.cm_lock);

		for(i = 0; i < cm.num_frames && !entry_found; i++) {
			if((cm.entries + i)->allocated) {
				continue;
			}

			entry_found = 1;
			entry_idx = i;
			for(j = 0; j < npages; j++) {
				if((cm.entries + i + j)->allocated) {
					entry_found = 0;
					break;
				}
			}
		}
		/* If no free frames, begin eviction routines */
		if(!entry_found) {
	// UNCOMMENT ONCE evict_frame() written
	//		entry_idx = evict_frame();
	//		if(entry_idx == -1) {	
				kprintf("No memory available!\n");
				spinlock_release(&cm.cm_lock);
				return 0;
	//		}
		}

		struct coremap_entry *return_entry;
		return_entry = (cm.entries + entry_idx);

		/* Only kernel pages have no pte */
		if(!pte) {
			for(j = 0; j < npages; j++) {
				(return_entry + j)->allocated = 1;
					(return_entry + j)->kern = 1;
				if(j < npages - 1) {
					(return_entry + j)->more_contig_frames = 1;
				}
			}
		/* Non-kernel pages are allocated 1 at a time: no need to loop */
		} else {
			/* Set entry pte */	
			return_entry->pte = pte;

			/* Update allocation order chain */
			cm.entries[cm.last_allocated].next_allocated = entry_idx;	
			cm.last_allocated = entry_idx;
		}

		addr = cm.first_mapped_paddr + (entry_idx * PAGE_SIZE);

		spinlock_release(&cm.cm_lock);

	} else {
		spinlock_acquire(&stealmem_lock);

		addr = ram_stealmem(npages);

		spinlock_release(&stealmem_lock);
	}
	return addr;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t
alloc_kpages(unsigned npages)
{
	paddr_t pa;
	vm_can_sleep();
	/* Get npages for kernel */
	pa = getppages(NULL, npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void
free_kpages(vaddr_t addr)
{
	if(vm_bootstrapped) {
		cm_free_frames(KVADDR_TO_PADDR(addr));
	}
}

int
cm_free_frames(paddr_t pa)
{
	unsigned cm_idx;
	/* pa guaranteed to be page aligned, so truncation should not be
	 * a concern here.
	 */
	KASSERT(pa >= cm.first_mapped_paddr);
	KASSERT(pa % PAGE_SIZE == 0);
	cm_idx = (pa - cm.first_mapped_paddr) / PAGE_SIZE;

	/* verify within coremap bounds */
	KASSERT(cm_idx < cm.num_frames);

	spinlock_acquire(&cm.cm_lock);

	struct coremap_entry *to_free;
	to_free = (cm.entries + cm_idx);

	int more_to_free = 1;
	while(more_to_free) {
		to_free->pte = NULL;
		to_free->tlb_idx = -1;
		to_free->allocated = 0;
		to_free->kern = 0;
		more_to_free = to_free->more_contig_frames;
		to_free->more_contig_frames = 0;
		
		/*
		 * NOTE: leave next_allocated as is: we need ghosts to remain
		 * until they are discovered as unallocated so we don't break
		 * the allocation order chain
		 */

		to_free = to_free + 1;
	}

	spinlock_release(&cm.cm_lock);

	return 0;

}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	return 0;
}
