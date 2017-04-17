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

	 // Set the stack (grows up) to be -1
	 as->stackDirIdx = -1;
	 as->stackTblIdx = -1;

	 // Set the heap (grows down) to be PAGE_TABLE_ENTRIES
	 as->heapDirIdx = PAGE_TABLE_ENTRIES;
	 as->heapTblIdx = PAGE_TABLE_ENTRIES;

	 // set all pageTable pointers to -1
	 as->pgDirectoryPtr = (struct pageTableEntry[PAGE_TABLE_ENTRIES])kmalloc(PAGE_SIZE);
	 for (int dirIdx = 0; dirIdx < PAGE_TABLE_ENTRIES; dirIdx++)
	 	as->pgDirectoryPtr[dirIdx] = 0; // what permissions do we want here?

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

		// what is best a reference to the pointer? or a copy of the array?
	 newas->pgDirectoryPtr = old->pgDirectoryPtr;

	 newas->stackDirIdx = old->stackDirIdx;
	 newas->stackTblIdx = old->stackTblIdx;
	 newas->heapDirIdx = old->heapDirIdx;
	 newas->heapTblIdx = old->heapTblIdx;

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	// BEWARE -- TODO -- handle case where stack & heap share 1 page

	// TODO -- seems like these loops should be simpler.
	// Maybe just loop through all possible entries and skip -1 values?

	// Release stack tables (& pages?)
	for (int stackTblPages = 0; stackTblPages < PAGE_TABLE_ENTRIES; stackTblPages++)
	{
		struct pageTableEntry[PAGE_TABLE_ENTRIES] thisPage = as->pgDirectoryPtr[stackTblPages];
		for (int stackPages = 0; stackPage < PAGE_TABLE_ENTRIES; stackPages++)
		{
			thisPage[stackPages].sharedProcessCount--;
			// TODO if this is 0 we need to let coremap know nothing is using this memory
		}

		// free this page of the page table 
		kfree(as->pgDirectoryPtr[stackTblPages]);
	}
	
	
	for (int stackTblPages = as->stackDirIdx; stackTblPages > -1; stackTblPages--)
	{
		struct pageTableEntry[PAGE_TABLE_ENTRIES] thisPage = as->pgDirectoryPtr[stackTblPages];
		for (int stackPages = as->stackTblIdx; stackPages > -1; stackPages--)
		{
			thisPage[stackPages].sharedProcessCount--;
			// TODO if this is 0 we need to let coremap know nothing is using this memory
		}

		// free this page of the page table and reset the inner counter for the next
		kfree(as->pgDirectoryPtr[stackTblPages]);
		as->stackTblIdx = PAGE_TABLE_ENTRIES - 1;
	}

	// Release heap tables
	for (int heapTblPages = as->heapDirIdx; heapTblPages > -1; heapTblPages--)
	{
		kfree(as->pgDirectoryPtr[heapTblPages]);
	}

	// release directory
	kfree(as->pgDirectoryPtr);

	// release addrspace
	kfree(as);
}

void
as_activate(void)
{
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	/*
	 * Write this.
	 */
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
	/*
	 * Write this.
	 */

	(void)as;
	(void)vaddr;
	(void)memsize;
	(void)readable;
	(void)writeable;
	(void)executable;
	return ENOSYS;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}
