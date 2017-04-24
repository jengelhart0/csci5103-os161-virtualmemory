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

#ifndef _ADDRSPACE_H_
#define _ADDRSPACE_H_

/*
 * Address space structure and operations.
 */


#include <vm.h>
#include "opt-dumbvm.h"

struct vnode;


/*
 * Address space - data structure associated with the virtual memory
 * space of a process.
 *
 * You write this.
 */

// We can change this, but it's MAX size is PAGE_SIZE / PAGE_TABLE_ENTRIES = 32 bits
struct pageTableEntryMap_t {
   paddr_t physicalAddress:20;
   uint32_t notUsed:7;
   uint32_t isRead:1;
   uint32_t isWrite:1;
   uint32_t isExecute:1;
   uint32_t isOnDisc:1;
   uint32_t isUsed:1;
 };
 // masks from /src/kern/arch/mips/include/vm.h
 #define PG_ADRS(pageTableEntry) (paddr_t)((pageTableEntry_t)pageTableEntry & PAGE_FRAME) // first 20 - 0 out the last 12
 // Can use bits in the middle if we need more flags
 #define IS_READ_PAGE(pageTableEntry) ((pageTableEntry_t)pageTableEntry & READ_BIT)
 #define IS_WRITE_PAGE(pageTableEntry) ((pageTableEntry_t)pageTableEntry & WRITE_BIT)
 #define IS_EXE_PAGE(pageTableEntry) ((pageTableEntry_t)pageTableEntry & EXECUTE_BIT)
 #define IS_ON_DISK(pageTableEntry) ((pageTableEntry_t)pageTableEntry & DISK_BIT)
 #define IS_USED_PAGE(pageTableEntry) ((pageTableEntry_t)pageTableEntry & USED_BIT)

// get pieces from vaddr_t or pageTableEntry_t
#define DIR_TBL_OFFSET(pageTableEntry) ((pageTableEntry_t)pageTableEntry & DIRECTORY_OFFSET)>>22 // first 10 - throw away the last 22
#define PG_TBL_OFFSET(pageTableEntry) ((pageTableEntry_t)pageTableEntry & DIRECTORY_PAGE_OFFSET)>>12 // middle 10 - throw away the first 10 & last 12
//#define PG_OFFSET(pageTableEntry) ((pageTableEntry_t)pageTableEntry & OFFSET_BITS) // last 12 - throw away the first 20

 // put pieces back together
 #define MAKE_PTE_ADDR(dirIdx,pgTblIdx,psyOffset) (pageTableEntry_t *)(((dirIdx)<<22) + ((pgTblIdx)<<12) + psyOffset) // 10 from dirIdx; 10 from pgTblIdx; 12 phsy ofset
 #define MAKE_VADDR(dirIdx,pgTblIdx,psyOffset)               (vaddr_t)(((dirIdx)<<22) + ((pgTblIdx)<<12) + psyOffset) // 10 from dirIdx; 10 from pgTblIdx; 12 phsy ofset

 // Make a PTE or PG_TBL_ADDR
 #define MAKE_PG_TBL_ADDR(dirIdx) (pageTableEntry_t *)((dirIdx)<<12) // dirIdx == 0 is unused by load_elf -- use these addresses for the page tables
 #define MAKE_PTE(paddr, otherBits) (pageTableEntry_t)(paddr + otherBits);


struct addrspace {
#if OPT_DUMBVM
        vaddr_t as_vbase1;
        paddr_t as_pbase1;
        size_t as_npages1;
        vaddr_t as_vbase2;
        paddr_t as_pbase2;
        size_t as_npages2;
        paddr_t as_stackpbase;
#else
  // a directory table entry with entries for the pageTables
  pageTableEntry_t *pgDirectoryPtr;//[PAGE_TABLE_ENTRIES];
  vaddr_t stackPtr;
  vaddr_t textTopPtr;
  vaddr_t heapPtr;
#endif
};

/*
 * Functions in addrspace.c:
 *
 *    as_create - create a new empty address space. You need to make
 *                sure this gets called in all the right places. You
 *                may find you want to change the argument list. May
 *                return NULL on out-of-memory error.
 *
 *    as_copy   - create a new address space that is an exact copy of
 *                an old one. Probably calls as_create to get a new
 *                empty address space and fill it in, but that's up to
 *                you.
 *
 *    as_activate - make curproc's address space the one currently
 *                "seen" by the processor.
 *
 *    as_deactivate - unload curproc's address space so it isn't
 *                currently "seen" by the processor. This is used to
 *                avoid potentially "seeing" it while it's being
 *                destroyed.
 *
 *    as_destroy - dispose of an address space. You may need to change
 *                the way this works if implementing user-level threads.
 *
 *    as_define_region - set up a region of memory within the address
 *                space.
 *
 *    as_prepare_load - this is called before actually loading from an
 *                executable into the address space.
 *
 *    as_complete_load - this is called when loading from an executable
 *                is complete.
 *
 *    as_define_stack - set up the stack region in the address space.
 *                (Normally called *after* as_complete_load().) Hands
 *                back the initial stack pointer for the new process.
 *
 * Note that when using dumbvm, addrspace.c is not used and these
 * functions are found in dumbvm.c.
 */

struct addrspace *as_create(void);
int               as_copy(struct addrspace *src, struct addrspace **ret);
void              as_activate(void);
void              as_deactivate(void);
void              as_destroy(struct addrspace *);

int               as_define_region(struct addrspace *as,
                                   vaddr_t vaddr, size_t sz,
                                   int readable,
                                   int writeable,
                                   int executable);
int               as_prepare_load(struct addrspace *as);
int               as_complete_load(struct addrspace *as);
int               as_define_stack(struct addrspace *as, vaddr_t *initstackptr);


/*
 * Functions in loadelf.c
 *    load_elf - load an ELF user program executable into the current
 *               address space. Returns the entry point (initial PC)
 *               in the space pointed to by ENTRYPOINT.
 */

int load_elf(struct vnode *v, vaddr_t *entrypoint);


#endif /* _ADDRSPACE_H_ */
