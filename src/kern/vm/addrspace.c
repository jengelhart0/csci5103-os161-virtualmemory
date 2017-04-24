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
#include <copyinout.h>

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
	as->textTopPtr = (vaddr_t)MAKE_PG_TBL_ADDR(PAGE_TABLE_ENTRIES-1);
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
	newas->pgDirectoryPtr = (pageTableEntry_t *)kmalloc(PAGE_SIZE);
	for (int dirIdx = 0; dirIdx < PAGE_TABLE_ENTRIES; dirIdx++)
	{
		if (old->pgDirectoryPtr[dirIdx].isUsed)
		{
				new->pgDirectoryPtr[dirIdx].physicalAddress = cm_alloc_frame(vaddr_t);
				new->pgDirectoryPtr[dirIdx].isUsed = 1;
				for (int pgIdx = 0; pgIdx < PAGE_TABLE_ENTRIES; pgIdx++)
				{
					pageTableEntry_t *pageTablePtr = MAKE_VADDR(dirIdx,pgIdx,0);
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

static
pageTableEntry_t *
as_new_directory_frame(struct addrspace *as, vaddr_t vaddr)
{
		int32_t dirIdx = DIR_TBL_OFFSET(vaddr);

		// if this dirTbl entry isn't initialized -- set it
		if (!IS_USED_PAGE(as->pgDirectoryPtr[dirIdx]))
		{
			paddr_t paddr = cm_alloc_frame(MAKE_PG_TBL_ADDR(dirIdx));
			as->pgDirectoryPtr[dirIdx] = MAKE_PTE(paddr, USED_BIT);

			// copy this into every entry of the page
			pageTableEntry_t *pgTblPtr = PTE_TO_KPG_TBL(as->pgDirectoryPtr[dirIdx]);
			for (int idx = 0; idx < PAGE_TABLE_ENTRIES; idx++)
				 pgTblPtr[idx] = 0;
		}

		return PTE_TO_KPG_TBL(as->pgDirectoryPtr[dirIdx]);
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

	// freak out if this memory is already used for page tables
	if (vaddr < (vaddr_t)MAKE_PG_TBL_ADDR(PAGE_TABLE_ENTRIES-1))
		return ENOSYS;

	// create a PTE with permissions set - not used yet
	pageTableEntry_t pte = 0;
	if (readable) pte += READ_BIT;
	if (writeable) pte += WRITE_BIT;
	if (executable) pte += EXECUTE_BIT;

	// heap grows up
	for (size_t idx = 0; idx < npages; idx++)
	{
		// if this dirTbl entry isn't initialized -- set it
		pageTableEntry_t *pgTblPtr = as_new_directory_frame(as, vaddr);
		int32_t pgIdx = PG_TBL_OFFSET(vaddr);

		// freak out if this pte is already used
		if (IS_USED_PAGE(pgTblPtr[pgIdx]))
			return ENOSYS;

		// add the permission bits to the pgTbl entry
		// - even though it doesn't have it's paddr_t part yet
		pgTblPtr[pgIdx] = pte;

		// and recalc for the next page
		vaddr += PAGE_SIZE;
	}

	// keep track of where this section ends
	 int32_t dirIdx = DIR_TBL_OFFSET(vaddr);
	 int32_t pgIdx = PG_TBL_OFFSET(vaddr);
	 as->textTopPtr = MAKE_VADDR(dirIdx, pgIdx, 0);
	 return 0;
}

static
void
as_allocate_page(struct addrspace *as, int dirIdx, int pgIdx)
{
		// grab the right page table
		pageTableEntry_t *pgTblPtr = PTE_TO_KPG_TBL(as->pgDirectoryPtr[dirIdx]);

		// mark this page as used (add to existing permissions bytes)
		pgTblPtr[pgIdx] += USED_BIT;

		// get virtual page # & save assigned paddr_t
		pageTableEntry_t *page = MAKE_PTE_ADDR(dirIdx, pgIdx, 0);
		paddr_t physicalAddress = cm_alloc_frame(page);
		pgTblPtr[pgIdx] += physicalAddress;
}

/* assumes text region starts at 0 and grows to textTopPtr */
int
as_prepare_load(struct addrspace *as)
{
	// get starting values in our directory & page tables
	int32_t dirMax = DIR_TBL_OFFSET(as->textTopPtr);
	int32_t pgMax = PG_TBL_OFFSET(as->textTopPtr);

	// dir 0 reserved for page table allocation - skip it
	for (int32_t dirIdx = 1; dirIdx < dirMax; dirIdx++)
	{
		if (!IS_USED_PAGE(as->pgDirectoryPtr[dirIdx]))
			continue;

		// allocate each frame
		for (int32_t pgIdx = 0; pgIdx < PAGE_TABLE_ENTRIES; pgIdx++)
			as_allocate_page(as, dirIdx, pgIdx);
	}

	// allocate each frame - final pgTbl may not use all entries
	for (int32_t pgIdx = 0; pgIdx <= pgMax; pgIdx++)
		as_allocate_page(as, dirMax, pgIdx);

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
	// stack grows down - allocate a page table for it to start.
	*stackptr = as->stackPtr;

	// if this dirTbl entry isn't initialized -- set it
	pageTableEntry_t *pgTblPtr = as_new_directory_frame(as, *stackptr);

	// freak out if this pte is already used
	uint32_t pgIdx = PG_TBL_OFFSET(*stackptr);
	if (IS_USED_PAGE(pgTblPtr[pgIdx]))
		return ENOSYS;

	return 0;
}
