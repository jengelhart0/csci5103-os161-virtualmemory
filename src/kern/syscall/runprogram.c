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

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than runprogram() does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <uio.h>

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname, char **args, unsigned long nargs)
{
	int result = 0;

	/* set up a single buffer for argv pointers and contents */
	int char_count = 0;
	int eachparam[nargs];
	for (unsigned long idx = 0; idx < nargs; idx++)
	{ /* arg + null terminator */
		eachparam[idx] = strlen(args[idx]) + 1;
		char_count += eachparam[idx];
		//kprintf("Args %d = %s - %d\n", (int)idx, args[idx], eachparam[idx]);
	}

	int argv_offset =  (nargs+1)*sizeof(char*);
	int actual_size = argv_offset + char_count;
	int align_size = actual_size % sizeof(char*) == 0 ? 0 :
		4 - actual_size % sizeof(char*);
	int total_size = actual_size + align_size;
	char total[total_size];

	/* add contents but not argv pointers yet */
	int kargs_offset = argv_offset;
	for (unsigned long idx = 0; idx < nargs; idx++)
	{
			/* copy value */
			char *argc_ptr = total + kargs_offset;
			int size = eachparam[idx];
			memcpy(argc_ptr, args[idx], size);
			//kprintf("Args %d: %s\n", idx, argc_ptr);
			kargs_offset += size;
	}

	/* Open the file. */
	struct vnode *v;
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* Create a new address space. */
	struct addrspace *as  = as_create();
	if (as == NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	struct addrspace *old_as = proc_setas(as);
	as_activate();

	/* We should be a new process only when called directly */
	/* from execv we already have the forked process */
	if (old_as != NULL)
		as_destroy(old_as);

	/* Load the executable. */
	vaddr_t entrypoint;
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	vaddr_t stackptr;
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}

	/* set up argv now that we have the stackptr */
	char *user_stack = (char *)stackptr - total_size;
	kargs_offset = argv_offset;
	for (unsigned long idx = 0; idx <= nargs; idx++)
	{
			/* set location in argv */
			char **argv_ptr = (char**)(total + idx*sizeof(char*));

			/* copy ptr or NULL */
			char *uargs_ptr = idx == nargs ? NULL : user_stack + kargs_offset;
			memcpy(argv_ptr, &uargs_ptr, sizeof(char*));
			//kprintf("Args %d: %p/%p\n", (int)idx, argv_ptr, *argv_ptr);

			/* increment arg pointer value */
			kargs_offset += eachparam[idx];
	}

	/* copy that buffer to userspace */
	struct iovec iov;
	iov.iov_ubase = (userptr_t)user_stack;
	iov.iov_len = total_size;

	struct uio u;
	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_offset = 0;
	u.uio_resid = actual_size;
	u.uio_segflg = UIO_USERSPACE;
	u.uio_rw = UIO_READ;
	u.uio_space = as;

	//kprintf("-> uiomove params size (%d/%d) from %p to %p\n",
	//	actual_size, total_size, total, user_stack);
	result = uiomove(total, actual_size, &u);
	if (result)
		return result;

	/* Warp to user mode. */
	enter_new_process(nargs /*argc*/,
				(userptr_t)user_stack /*userptr_t-userspace addr of argv*/,
			  NULL /*userspace addr of environment*/,
			  (vaddr_t)(stackptr - total_size), //stackptr
				entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}
