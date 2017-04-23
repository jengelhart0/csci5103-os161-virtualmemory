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

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>

#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	// Set the stack (grows down) to be PAGE_TABLE_ENTRIES
	as->stackPtr =  USERSTACK;
	as->textTopPtr = 0;
	as->heapPtr = 0;

	 // set all pageTable pointers to -1
	 as->pgDirectoryPtr = (pageTableEntry_t *)kmalloc(PAGE_SIZE);
	 for (int dirIdx = 0; dirIdx < PAGE_TABLE_ENTRIES; dirIdx++)
	 	as->pgDirectoryPtr[dirIdx] = 0;

	//	kprintf("A new addrspace! (%p)\n", as->pgDirectoryPtr);
	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	/* not really sure how to make a deep copy given that I'm using the same vaddr_t
	newas->pgDirectoryPtr = (struct pageTableEntry_t *)kmalloc(PAGE_SIZE);
	for (int dirIdx = 0; dirIdx < PAGE_TABLE_ENTRIES; dirIdx++)
	{
		if (old->pgDirectoryPtr[dirIdx].isUsed)
		{
				new->pgDirectoryPtr[dirIdx].physicalAddress = cm_alloc_frames(1);
				new->pgDirectoryPtr[dirIdx].isUsed = 1;
				for (int pgIdx = 0; pgIdx < PAGE_TABLE_ENTRIES; pgIdx++)
				{
					struct pageTableEntry_t *pageTablePtr = MAKE_VADDR(dirIdx,pgIdx,0);
					if (pageTablePtr[pgIdx].isUsed)
					{
						// get paddr_t for allocating actual pageTable
						// save it in the page table entry
						// memcpy the actual contents of the page from the original location
					} // check if pageTable entry used
				} // loop over pageTable entires
		} // check if directoryTable entry used
	} // loop over directoryTable entries
	*/
	 newas->pgDirectoryPtr = old->pgDirectoryPtr;

	 newas->stackPtr = old->stackPtr;
	 newas->textTopPtr = old->textTopPtr;
	 newas->heapPtr = old->heapPtr;

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	for (int32_t dirIdx = 0; dirIdx < PAGE_TABLE_ENTRIES; dirIdx++)
	{
		if (!IS_USED_PAGE(as->pgDirectoryPtr[dirIdx]))
			continue;

		// release each frame
		for (int32_t pgIdx = 0; pgIdx < PAGE_TABLE_ENTRIES; pgIdx++)
		{
				pageTableEntry_t *pgTbl = MAKE_PTE_ADDR(dirIdx, pgIdx, 0);
				if (!IS_USED_PAGE(*pgTbl))
					continue;

				cm_free_frames(PG_ADRS(*pgTbl));
		}

		// release each pageTable
		pageTableEntry_t *pgTbl = MAKE_PTE_ADDR(dirIdx, 0, 0);
		cm_free_frames(PG_ADRS(*pgTbl));
	}

	// release directory
	kfree(as->pgDirectoryPtr);

	// release addrspace
	kfree(as);
}

/* copied from dumbvm - just clears TLB */
void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{
	// Align the region. First, the base...
	memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	// ...and now the length.
	memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;
	size_t npages = memsize / PAGE_SIZE;

	// create a PTE with permissions set - not used yet
	pageTableEntry_t pte = 0;
	if (readable) pte += READ_BIT;
	if (writeable) pte += WRITE_BIT;
	if (executable) pte += EXECUTE_BIT;

	// get starting values in our directory & page tables
	int32_t dirIdx = DIR_TBL_OFFSET(vaddr);
	int32_t pgIdx = PG_TBL_OFFSET(vaddr);

	// heap grows up
	for (size_t idx = 0; idx < npages; idx++)
	{
		// if we've reached the end of the pgTbl, get the next dirTbl entry
		if (pgIdx == PAGE_TABLE_ENTRIES)
		{
			pgIdx = 0;
			dirIdx++;
		}

		// if this dirTbl entry isn't initialized -- set it
		if (!IS_USED_PAGE(as->pgDirectoryPtr[dirIdx]))
			as->pgDirectoryPtr[dirIdx] = cm_alloc_frames(1) + USED_BIT;

		pageTableEntry_t *pgTbl = MAKE_PTE_ADDR(dirIdx, pgIdx, 0);
		// freak out if this pte is already used
		if (IS_USED_PAGE(*pgTbl))
			return ENOSYS;

		// add the permission bits to the pgTbl entry
		// - even though it doesn't have it's paddr_t part yet
		*pgTbl = pte;
		pgIdx++;
	}

	 as->textTopPtr = MAKE_VADDR(dirIdx, pgIdx, 0);
	 return 0;
}

/* assumes text region starts at 0 and grows to textTopPtr */
int
as_prepare_load(struct addrspace *as)
{
	// get starting values in our directory & page tables
	int32_t dirMax = DIR_TBL_OFFSET(as->textTopPtr);
	int32_t pgMax = PG_TBL_OFFSET(as->textTopPtr);

	for (int32_t dirIdx = 0; dirIdx < dirMax; dirIdx++)
	{
		if (!IS_USED_PAGE(as->pgDirectoryPtr[dirIdx]))
			continue;

		// allocate each frame
		for (int32_t pgIdx = 0; pgIdx < PAGE_TABLE_ENTRIES; pgIdx++)
		{
				pageTableEntry_t *pgTbl = MAKE_PTE_ADDR(dirIdx, pgIdx, 0);
				*pgTbl = cm_alloc_frames(1) + USED_BIT;
		}
	}

	// allocate each frame - final directory entry
	for (int32_t pgIdx = 0; pgIdx <= pgMax; pgIdx++)
	{
			pageTableEntry_t *pgTbl = MAKE_PTE_ADDR(dirMax, pgIdx, 0);
			*pgTbl = cm_alloc_frames(1) + USED_BIT;
	}

 return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this. - not actually sure there is anything to do here
	 */

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/* Initial user-level stack pointer */
	*stackptr = as->stackPtr;

	uint32_t dirIdx = DIR_TBL_OFFSET(*stackptr);
	uint32_t pgIdx = PG_TBL_OFFSET(*stackptr);

	// stack grows down - allocate a page table for it to start.

	// if this dirTbl entry isn't initialized -- set it
	if (!IS_USED_PAGE(as->pgDirectoryPtr[dirIdx]))
		as->pgDirectoryPtr[dirIdx] = alloc_kpages(1) + USED_BIT;

	pageTableEntry_t *pgTbl = MAKE_PTE_ADDR(dirIdx, pgIdx, 0);
	// freak out if this pte is already used
	if (IS_USED_PAGE(*pgTbl))
		return ENOSYS;

	return 0;
}
