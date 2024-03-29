/* This file handles advisory file locking as required by POSIX.
 *
 * The entry points into this file are
 *   lock_op:	perform locking operations for FCNTL system call
 *   lock_revive: revive processes when a lock is released
 */

// #define MUKDBG    1
#include "fs.h"

#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

/*===========================================================================*
 *				lock_op					     *
 *===========================================================================*/
int lock_op(struct filp *f, int req)
//struct filp *f;
//int req;			/* either F_SETLK or F_SETLKW */
{
/* Perform the advisory locking required by POSIX. */

  int r, ltype, i, conflict = 0, unlocking = 0;
  mnx_mode_t mo;
  mnx_off_t first, last;
  struct mnx_flock fs_flock;
  vir_bytes user_flock;
  struct mnx_file_lock *flp, *flp2, *empty;

  /* Fetch the fs_flock structure from user space. */
  user_flock = (vir_bytes) fs_m_in.name1;
 //  r = sys_datacopy(fs_who_e, (vir_bytes) user_flock,
	// fs_ep, (vir_bytes) &fs_flock, (phys_bytes) sizeof(fs_flock));
  //r = dvk_vcopy(fs_who_e, user_flock, fs_ep, &fs_flock, sizeof(fs_flock));    
  MUK_vcopy( r, fs_who_e, user_flock, fs_ep, &fs_flock, sizeof(fs_flock));    
  if (r < 0) ERROR_RETURN(r);

  /* Make some error checks. */
  ltype = fs_flock.l_type;
  mo = f->filp_mode;
  if (ltype != F_UNLCK && ltype != F_RDLCK && ltype != F_WRLCK) return(EDVSINVAL);
  if (req == F_GETLK && ltype == F_UNLCK) return(EINVAL);
  if ( (f->filp_ino->i_mode & I_TYPE) != I_REGULAR) return(EDVSINVAL);
  if (req != F_GETLK && ltype == F_RDLCK && (mo & R_BIT) == 0) return(EDVSBADF);
  if (req != F_GETLK && ltype == F_WRLCK && (mo & W_BIT) == 0) return(EDVSBADF);

  /* Compute the first and last bytes in the lock region. */
  switch (fs_flock.l_whence) {
	case SEEK_SET:	first = 0; break;
	case SEEK_CUR:	first = f->filp_pos; break;
	case SEEK_END:	first = f->filp_ino->i_size; break;
	default:	return(EINVAL);
  }
  /* Check for overflow. */
  if (((long)fs_flock.l_start > 0) && ((first + fs_flock.l_start) < first))
	return(EDVSINVAL);
  if (((long)fs_flock.l_start < 0) && ((first + fs_flock.l_start) > first))
	return(EDVSINVAL);
  first = first + fs_flock.l_start;
  last = first + fs_flock.l_len - 1;
  if (fs_flock.l_len == 0) last = MAX_FILE_POS;
  if (last < first) return(EDVSINVAL);

  /* Check if this region conflicts with any existing lock. */
  empty = (struct mnx_file_lock *) 0;
  for (flp = &file_lock[0]; flp < & file_lock[NR_LOCKS]; flp++) {
	if (flp->lock_type == 0) {
		if (empty == (struct mnx_file_lock *) 0) empty = flp;
		continue;	/* 0 means unused slot */
	}
	if (flp->lock_inode != f->filp_ino) continue;	/* different file */
	if (last < flp->lock_first) continue;	/* new one is in front */
	if (first > flp->lock_last) continue;	/* new one is afterwards */
	if (ltype == F_RDLCK && flp->lock_type == F_RDLCK) continue;
	if (ltype != F_UNLCK && flp->lock_pid == fs_proc->fp_pid) continue;
  
	/* There might be a conflict.  Process it. */
	conflict = 1;
	if (req == F_GETLK) break;

	/* If we are trying to set a lock, it just failed. */
	if (ltype == F_RDLCK || ltype == F_WRLCK) {
		if (req == F_SETLK) {
			/* For F_SETLK, just report back failure. */
			return(EDVSAGAIN);
		} else {
			/* For F_SETLKW, suspend the process. */
			suspend(XLOCK);
			return(SUSPEND);
		}
	}

	/* We are clearing a lock and we found something that overlaps. */
	unlocking = 1;
	if (first <= flp->lock_first && last >= flp->lock_last) {
		flp->lock_type = 0;	/* mark slot as unused */
		fs_nr_locks--;		/* number of locks is now 1 less */
		continue;
	}

	/* Part of a locked region has been unlocked. */
	if (first <= flp->lock_first) {
		flp->lock_first = last + 1;
		continue;
	}

	if (last >= flp->lock_last) {
		flp->lock_last = first - 1;
		continue;
	}
	
	/* Bad luck. A lock has been split in two by unlocking the middle. */
	if (fs_nr_locks == NR_LOCKS) return(EDVSNOLCK);
	for (i = 0; i < NR_LOCKS; i++)
		if (file_lock[i].lock_type == 0) break;
	flp2 = &file_lock[i];
	flp2->lock_type = flp->lock_type;
	flp2->lock_pid = flp->lock_pid;
	flp2->lock_inode = flp->lock_inode;
	flp2->lock_first = last + 1;
	flp2->lock_last = flp->lock_last;
	flp->lock_last = first - 1;
	fs_nr_locks++;
  }
  if (unlocking) lock_revive();

  if (req == F_GETLK) {
	if (conflict) {
		/* GETLK and conflict. Report on the conflicting lock. */
		fs_flock.l_type = flp->lock_type;
		fs_flock.l_whence = SEEK_SET;
		fs_flock.l_start = flp->lock_first;
		fs_flock.l_len = flp->lock_last - flp->lock_first + 1;
		fs_flock.l_pid = flp->lock_pid;

	} else {
		/* It is GETLK and there is no conflict. */
		fs_flock.l_type = F_UNLCK;
	}

	/* Copy the fs_flock structure back to the caller. */
	// r = sys_datacopy(fs_ep, (vir_bytes) &fs_flock,
	// 	fs_who_e, (vir_bytes) user_flock, (phys_bytes) sizeof(fs_flock));
 	// r = dvk_vcopy(fs_ep, &fs_flock, fs_who_e, user_flock, sizeof(fs_flock));	
	MUK_vcopy( r, fs_ep, &fs_flock, fs_who_e, user_flock, sizeof(fs_flock));	
    if( r < 0) ERROR_RETURN(r);
	return(OK);
  }

  if (ltype == F_UNLCK) return(OK);	/* unlocked a region with no locks */

  /* There is no conflict.  If space exists, store new lock in the table. */
  if (empty == (struct mnx_file_lock *) 0) return(EDVSNOLCK);	/* table full */
  empty->lock_type = ltype;
  empty->lock_pid = fs_proc->fp_pid;
  empty->lock_inode = f->filp_ino;
  empty->lock_first = first;
  empty->lock_last = last;
  fs_nr_locks++;
  return(OK);
}

/*===========================================================================*
 *				lock_revive				     *
 *===========================================================================*/
void lock_revive()
{
/* Go find all the processes that are waiting for any kind of lock and 
 * revive them all.  The ones that are still blocked will block again when 
 * they run.  The others will complete.  This strategy is a space-time 
 * tradeoff.  Figuring out exactly which ones to unblock now would take 
 * extra code, and the only thing it would win would be some performance in 
 * extremely rare circumstances (namely, that somebody actually used 
 * locking).
 */

  int task;
  fproc_t *fptr;

  for (fptr = &fs_proc_table[INIT_PROC_NR + 1]; fptr < &fs_proc_table[dc_ptr->dc_nr_procs]; fptr++){
	if(fptr->fp_pid == PID_FREE) continue;
	task = -fptr->fp_task;
	if (fptr->fp_suspended == SUSPENDED && task == XLOCK) {
		revive(fptr->fp_endpoint, 0);
	}
  }
}