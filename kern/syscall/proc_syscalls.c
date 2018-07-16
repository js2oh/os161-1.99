#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <synch.h>
#include <mips/trapframe.h>
#include <mips/vm.h>
#include <limits.h>
#include <test.h>
#include "opt-A2.h"

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {
#if OPT_A2
  KASSERT(curproc != NULL);

  struct addrspace *as;
  struct proc *p = curproc;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  p->p_dead = true;
  p->p_exitcode = _MKWAIT_EXIT(exitcode);
  lock_acquire(p->p_lk);
  cv_broadcast(p->p_cv,p->p_lk);
  lock_release(p->p_lk);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  if (p->p_parent == NULL) {
    proc_destroy(p);
  }
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
#else
  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
#endif /* OPT_A2 */
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
#if OPT_A2
  KASSERT(curproc != NULL);
  *retval = curproc->p_id;
  return(0);
#else
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = 1;
  return(0);
#endif /* OPT_A2 */
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

#if OPT_A2
  if (options != 0) {
    *retval = -1;
    return(EINVAL);
  }

  if (!proc_exist(pid)) {
    *retval = -1;
    return(ESRCH);
  }

  struct proc *cProc = get_child_proc(curproc,pid);
  if (cProc == NULL) {
    *retval = -1;
    return(ECHILD);
  }

  lock_acquire(cProc->p_lk);
  while(!cProc->p_dead){
    cv_wait(cProc->p_cv, cProc->p_lk);
  }
  lock_release(cProc->p_lk);

  exitstatus = cProc->p_exitcode;

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    *retval = -1;
    return(result);
  }

#else
  if (options != 0) {
    return(EINVAL);
  }
  
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;


  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
#endif /* OPT_A2 */

  *retval = pid;
  return(0);
}

#if OPT_A2
int sys_fork(struct trapframe *tf, pid_t *retval) {

  KASSERT(curproc != NULL);

  // create process structure for child process
  struct proc *cProc = proc_create_runprogram(curproc->p_name);
  if (cProc == NULL) {
    *retval = -1;
    return(ENOMEM);
  }

  // create and copy address space
  int errCode = 0;
  spinlock_acquire(&cProc->p_lock);
  errCode = as_copy(curproc->p_addrspace, &cProc->p_addrspace);
  spinlock_release(&cProc->p_lock);
  if (errCode) {
    proc_destroy(cProc);
    *retval = -1;
    return(errCode);
  }

  // assign PID to child process and create the parent/child relationship
  if (cProc->p_id == -1) {
    proc_destroy(cProc);
    *retval = -1;
    return(EMPROC);
  }
  lock_acquire(curproc->p_lk);
  cProc->p_parent = curproc;
  array_add(curproc->p_children,cProc,NULL);
  lock_release(curproc->p_lk);

  // copy trapframe (required to put it onto the new thread's stack)
  struct trapframe *ctf = kmalloc(sizeof(struct trapframe));
  if (ctf == NULL) {
    proc_destroy(cProc);
    *retval = -1;
    return(ENOMEM);
  }
  memcpy(ctf,tf,sizeof(struct trapframe));

  // create thread for child process
  // child thread needs to put the trapframe onto the stack and modify it so that it returns the current value & call mips_usermode in the child
  errCode = thread_fork(curthread->t_name,cProc,enter_forked_process,(void *)ctf,0);
  if (errCode) {
    proc_destroy(cProc);
    kfree(ctf);
    *retval = -1;
    return(errCode);
  }

  *retval = cProc->p_id;
  return(0);
}

int sys_execv(const_userptr_t progname, userptr_t args, int *retval) {
  int result;

  // Count the number of arguments and copy them into the kernel
  int argc = 0;
  size_t tLength;
  int index = 0;
  while(((userptr_t *) args)[argc] != NULL) {
    argc++;
    tLength += strlen(((char **)args)[index]) + 1;
  }
  if (tLength > ARG_MAX) {
    *retval = -1;
    return E2BIG;
  }
  size_t aLength;
  char *argv[argc];
  char cArg[tLength];
  index = 0;
  for(int i = 0; i < argc; i++) {
    size_t mLength = strlen(((char **)args)[i]) + 1;
    result = copyinstr(((userptr_t *)args)[i], (char *)(cArg + index), mLength, &aLength);
    if (result) {
      *retval = -1;
      return result;
    }
    argv[i] = cArg + index;
    index += aLength;
  }

  // Copy the program path into the kernel
  char cPath[PATH_MAX];
  size_t pLength;
  result = copyinstr(progname, cPath, PATH_MAX, &pLength);
  if (result) {
    *retval = -1;
    return result;
  }
  if (pLength <= 1) {
    *retval = -1;
    return ENOENT;
  }

  // Call runprogram to do the rest of steps
  result = runprogram(cPath,argv,argc);

  *retval = -1;
  return result;
}

#endif /* OPT_A2 */
