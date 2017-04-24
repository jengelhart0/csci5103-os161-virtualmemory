/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _VM_H_
#define _VM_H_

#include <types.h>
#include <addrspace.h>
#include <spinlock.h>
#include <machine/vm.h>

/*
 * VM system-related definitions.
 *
 */

/* Defines coremap to map and maintain physical memory. */

/* Fault-type arguments to vm_fault() */
#define VM_FAULT_READ        0    /* A read was attempted */
#define VM_FAULT_WRITE       1    /* A write was attempted */
#define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/

struct coremap_entry {
	pageTableEntry_t *pte; 	// pointer to second level page table pte
	/* Generously assumes 2^24 coremap entries exist.
	 * 25th bit allows -1 value for index.
	 */
	int prev_allocated:25;
	int next_allocated:25;
	int tlb_idx:7;
	/* bit fields: can only take values 0 or 1 with 1-bit fields */
	uint32_t allocated:1;
	uint32_t dirty:1; 		// needed?
	uint32_t more_contig_frames:1;  // 1 if contig-alloc'ed frames remain
	uint32_t kern:1;
};

struct coremap {
	struct spinlock cm_lock;
	/* Array of coremap entries */
	struct coremap_entry *entries;
	/* First addr managed by coremap */
	paddr_t first_mapped_paddr;
	/* Indices needed for FIFO eviction */
	int last_allocated:25;
	int oldest:25;

	unsigned num_frames:25;
};

/* Coremap initialization function */
void init_coremap(void);

/* Initialization function */
void vm_bootstrap(void);

/* Used to get pages for allocation. If kernel allocation, pte
 * should be NULL. Returns physical address of available frame,
 * or 0 if no frame is available.
 */
paddr_t getppages(pageTableEntry_t *pte, unsigned long npages);

/* wrapper for single page allocation by non-kernel functions */
paddr_t cm_alloc_frame(pageTableEntry_t *pte);

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(unsigned npages);

void free_kpages(vaddr_t addr);

/* Free contiguously allocated frames starting at pa */
int cm_free_frames(paddr_t pa);

/*
 * Selects best candidate for eviction. Sets idxptr to frame index to evict,
 * updates allocation chain to reflect victim selection. Returns -1 if no
 * suitable victim is found (can happen if memory is full of kernel pages).
 */
int select_victim(unsigned *idxptr);

/*
 * Uses coremap to evict next victim. Sets pte's value to be the pte whose
 * frame was evicted.
 */
int evict_frame(pageTableEntry_t **pte, unsigned *swap_idx);

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress);

/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown(const struct tlbshootdown *);


#endif /* _VM_H_ */
