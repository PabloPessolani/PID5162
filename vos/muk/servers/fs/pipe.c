/* This file deals with the suspension and revival of processes.  A process can
 * be suspended because it wants to read or write from a pipe and can't, or
 * because it wants to read or write from a special file and can't.  When a
 * process can't continue it is suspended, and revived later when it is able
 * to continue.
 *
 * The entry points into this file are
 *   fs_pipe:	  perform the PIPE system call
 *   pipe_check:  check to see that a read or write on a pipe is feasible now
 *   suspend:	  suspend a process that cannot do a requested read or write
 *   release:	  check to see if a suspended process can be released and do
 *                it
 *   revive:	  mark a suspended process as able to run again
 *   unsuspend_by_endpt: revive all processes blocking on a given process
 *   fs_unpause:  a signal has been sent to a process; see if it suspended
 */

// #define MUKDBG    1

#include "fs.h"
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"


 /*===========================================================================*
 *				suspend					     *
 *===========================================================================*/
void suspend(int task)
//int task;			/* who is proc waiting for? (PIPE = pipe) */
{
/* Take measures to suspend the processing of the present system call.
 * Store the parameters to be used upon resuming in the process table.
 * (Actually they are not used when a process is waiting for an I/O device,
 * but they are needed for pipes, and it is not worth making the distinction.)
 * The SUSPEND pseudo error should be returned after calling suspend().
 */

  if (task == XPIPE || task == XPOPEN) susp_count++;/* #procs susp'ed on pipe*/
  fs_proc->fp_suspended = SUSPENDED;
  fs_proc->fp_fd = fs_m_in.fd << 8 | fs_call_nr;
  if(task == NONE)
	panic(__FILE__,"suspend on NONE",NO_NUM);
  fs_proc->fp_task = -task;
  if (task == XLOCK) {
	fs_proc->fp_buffer = (char *) fs_m_in.name1;	/* third arg to fcntl() */
	fs_proc->fp_nbytes = fs_m_in.request;		/* second arg to fcntl() */
  } else {
	fs_proc->fp_buffer = fs_m_in.buffer;		/* for reads and writes */
	fs_proc->fp_nbytes = fs_m_in.nbytes;
  }
}

/*===========================================================================*
 *				release					     *
 *===========================================================================*/
void release(struct inode *ip, int fs_call_nr, int count)
{
//register struct inode *ip;	/* inode of pipe */
//int fs_call_nr;			/* READ, WRITE, OPEN or CREAT */
//int count;			/* max number of processes to release */
/* Check to see if any process is hanging on the pipe whose inode is in 'ip'.
 * If one is, and it was trying to perform the call indicated by 'fs_call_nr',
 * release it.
 */

  register fproc_t *rp;
  struct filp *f;

  /* Trying to perform the call also includes SELECTing on it with that
   * operation.
   */
  if (fs_call_nr == MOLREAD || fs_call_nr == MOLWRITE) {
  	  int op;
  	  if (fs_call_nr == MOLREAD)
  	  	op = MOL_SEL_RD;
  	  else
  	  	op = MOL_SEL_WR;
	  for(f = &filp[0]; f < &filp[NR_FILPS]; f++) {
  		if (f->filp_count < 1 || !(f->filp_pipe_select_ops & op) ||
  		   f->filp_ino != ip)
  			continue;
  		 //select_callback(f, op); //VER ESTO para que este completo. Da error de compilacion en int select_callback(struct filp *fs_proc, int ops) cuando se la agrega a proto.h
		f->filp_pipe_select_ops &= ~op;
  	}
  }

  /* Search the proc table. */
  for (rp = &fs_proc_table[0]; rp < &fs_proc_table[dc_ptr->dc_nr_procs]; rp++) {
	if (rp->fp_pid != PID_FREE && rp->fp_suspended == SUSPENDED &&
			rp->fp_revived == NOT_REVIVING &&
			(rp->fp_fd & BYTE) == fs_call_nr &&
			rp->fp_filp[rp->fp_fd>>8]->filp_ino == ip) {
		revive(rp->fp_endpoint, 0);
		susp_count--;	/* keep track of who is suspended */
		if (--count == 0) return;
	}
  }
}

/*===========================================================================*
 *				revive					     *
 *===========================================================================*/
void revive(int proc_nr_e, int returned)
//int proc_nr_e;			/* process to revive */
//int returned;			/* if hanging on task, how many bytes read */
{
/* Revive a previously blocked process. When a process hangs on tty, this
 * is the way it is eventually released.
 */

  register fproc_t *rfp;
  register int task;
  int proc_nr;

	MUKDEBUG("proc_nr_e=%d  returned\n", proc_nr_e, returned);

  if(isokendpt(proc_nr_e, &proc_nr) != OK)
	return;

  rfp = &fs_proc_table[proc_nr];
  if (rfp->fp_suspended == NOT_SUSPENDED || rfp->fp_revived == REVIVING)return;

  /* The 'reviving' flag only applies to pipes.  Processes waiting for TTY get
   * a message right away.  The revival process is different for TTY and pipes.
   * For select and TTY revival, the work is already done, for pipes it is not:
   *  the proc must be restarted so it can try again.
   */
  task = -rfp->fp_task;
  if (task == XPIPE || task == XLOCK) {
	/* Revive a process suspended on a pipe or lock. */
	rfp->fp_revived = REVIVING;
	reviving++;		/* process was waiting on pipe or lock */
  } else {
	rfp->fp_suspended = NOT_SUSPENDED;
	if (task == XPOPEN) /* process blocked in open or create */
		fs_reply(proc_nr_e, rfp->fp_fd>>8);
	else if (task == XSELECT) {
		fs_reply(proc_nr_e, returned);
	} else {
		/* Revive a process suspended on TTY or other device. */
		rfp->fp_nbytes = returned;	/*pretend it wants only what there is*/
		fs_reply(proc_nr_e, returned);	/* unblock the process */
	}
  }
}
