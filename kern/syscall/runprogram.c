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
 * that execv() needs to do more than this function does.
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
#include <copyinout.h>
#include "opt-A2.h"

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname, char **args, unsigned long argc)
{
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr, argv;
	int result;

	/* Open the file. */
	char *fname_temp;
	fname_temp = kstrdup(progname);
	result = vfs_open(fname_temp, O_RDONLY, 0, &v);
	kfree(fname_temp);
	if (result) {
		return result;
	}

	/* We should be a new process. */
	// KASSERT(curproc_getas() == NULL);

	/* Create a new address space. */
	as = as_create();
	if (as ==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	#if OPT_A2
	struct addrspace *oldas = curproc_setas(as);
	#else
	curproc_setas(as);
	#endif /* OPT_A2 */
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		#if OPT_A2
		as_deactivate();
    	as = curproc_setas(oldas);
    	as_destroy(as);
    	as_activate();
    	#endif /* OPT_A2 */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		#if OPT_A2
		as_deactivate();
    	as = curproc_setas(oldas);
    	as_destroy(as);
    	as_activate();
    	#endif /* OPT_A2 */
		return result;
	}

	#if OPT_A2
	// Need to copy the arguments into the new address space
	char ** argPtr = kmalloc((argc + 1)*sizeof(char *));
	for (unsigned int i = 0; i < argc; i++) {
		size_t argLength = strlen(args[i]) + 1;
		stackptr -= argLength;
		result = copyout(args[i],(userptr_t)stackptr,argLength);
		if (result) {
			as_deactivate();
    		as = curproc_setas(oldas);
    		as_destroy(as);
    		as_activate();
			kfree(argPtr);
			return result;
		}
		argPtr[i] = (char *)stackptr;
	}
	argPtr[argc] = NULL;
	int pad = stackptr % 4;
	stackptr -= pad;
	size_t argPtrLength = (argc + 1) * sizeof(char *);
	stackptr -= argPtrLength;
	result = copyout(argPtr,(userptr_t)stackptr,argPtrLength);
	if (result) {
		as_deactivate();
    	as = curproc_setas(oldas);
    	as_destroy(as);
    	as_activate();
		kfree(argPtr);
		return result;
	}
	argv = stackptr;
	pad = stackptr % 8;
	stackptr -= pad;

	kfree(argPtr);

	// Delete old address space
  	as_deactivate();
  	as_destroy(oldas);
  	as_activate();

  	/* Warp to user mode. */
	enter_new_process(argc, (userptr_t)argv,
			  stackptr, entrypoint);
	#else
	/* Warp to user mode. */
	enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
			  stackptr, entrypoint);
	#endif /* OPT_A2 */
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}
