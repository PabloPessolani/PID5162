/* This file contains a collection of miscellaneous procedures.  Some of them
 * perform simple system calls.  Some others do a little part of system calls
 * that are mostly performed by the Memory Manager.
 *
 * The entry points into this file are
 *   fs_dup:    perform the DUP system call
 *   fs_fcntl:    perform the FCNTL system call
 *   fs_sync:   perform the SYNC system call
 *   fs_fsync:    perform the FSYNC system call
 *   fs_reboot:   sync disks and prepare for shutdown
 *   fs_fork:   adjust the tables after MM has performed a FORK system call
 *   fs_exec:   handle files with FD_CLOEXEC on after MM has done an EXEC
 *   fs_exit:   a process has exited; note that in the tables
 *   fs_set:    set uid or gid for some process
 *   fs_revive:   revive a process that was waiting for something (e.g. TTY)
 *   fs_svrctl:   file system control
 *   fs_getsysinfo: request copy of FS data structure
 */

#include "fs.h"

#undef DVK_GLOBAL_HERE
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

/*===========================================================================*
*        fs_getsysinfo            *
*===========================================================================*/
int fs_getsysinfo()
{
  struct fproc_table *proc_addr;
  vir_bytes src_addr, dst_addr;
  mnx_size_t len;
  int s;

  switch (fs_m_in.info_what) {
  case SI_FSPROC_ADDR:
    proc_addr = &fs_proc_table[0];
    src_addr = (vir_bytes) &proc_addr;
    len = sizeof(struct fproc_table *);
    break;
  case SI_FSPROC_TAB:
    src_addr = (vir_bytes) fs_proc_table;
    len = sizeof(fproc_t) * dc_ptr->dc_nr_procs;
    break;
  case SI_DMAP_TAB:
    src_addr = (vir_bytes) fs_dmap_tab;
    len = sizeof(dmap_t) * NR_DEVICES;
    break;
  default:
    return (EDVSINVAL);
  }

  dst_addr = fs_m_in.info_where;
  // if (OK != (s=sys_datacopy(fs_ep, src_addr, fs_who_e, dst_addr, len)))
  MUK_vcopy(s,fs_ep, src_addr, fs_who_e, dst_addr, len);
  if ( s < 0) ERROR_RETURN(s);
  return (OK);

}

/*===========================================================================*
 *        fs_dup               *
 *===========================================================================*/
int fs_dup()
{
/* Perform the dup(fd) or dup2(fd,fd2) system call. These system calls are
 * obsolete.  In fact, it is not even possible to invoke them using the
 * current library because the library routines call fcntl().  They are
 * provided to permit old binary programs to continue to run.
 */

  register int rfd;
  register struct filp *f;
  struct filp *dummy;
  int r;

  /* Is the file descriptor valid? */
  rfd = fs_m_in.fd & ~DUP_MASK;    /* kill off dup2 bit, if on */
  if ((f = get_filp(rfd)) == NIL_FILP) return(fs_err_code);

  /* Distinguish between dup and dup2. */
  if (fs_m_in.fd == rfd) {     /* bit not on */
  /* dup(fd) */
  if ( (r = get_fd(0, 0, &fs_m_in.fd2, &dummy)) != OK) return(r);
  } else {
  /* dup2(fd, fd2) */
  if (fs_m_in.fd2 < 0 || fs_m_in.fd2 >= MNX_OPEN_MAX) return(EDVSBADF);
  if (rfd == fs_m_in.fd2) return(fs_m_in.fd2);  /* ignore the call: dup2(x, x) */
  fs_m_in.fd = fs_m_in.fd2;   /* prepare to close fd2 */
  (void) fs_close();  /* cannot fail */
  }

  /* Success. Set up new file descriptors. */
  f->filp_count++;
  fs_proc->fp_filp[fs_m_in.fd2] = f;
  FD_SET(fs_m_in.fd2, &fs_proc->fp_filp_inuse);
  return(fs_m_in.fd2);
}

/*===========================================================================*
 *        fs_fcntl             *
 *===========================================================================*/
int fs_fcntl()
{
  /* Perform the fcntl(fd, request, ...) system call. */

  register struct filp *f;
  int new_fd, r, fl;
  long cloexec_mask;    /* bit map for the FD_CLOEXEC flag */
  long clo_value;   /* FD_CLOEXEC flag in proper position */
  struct filp *dummy;

  MUKDEBUG("fs_m_in.fd is=%d\n", fs_m_in.fd);
  /* Is the file descriptor valid? */
  if ((f = get_filp(fs_m_in.fd)) == NIL_FILP) ERROR_RETURN(fs_err_code);

  switch (fs_m_in.request) {
		case F_DUPFD:
			/* This replaces the old dup() system call. */
			if (fs_m_in.addr < 0 || fs_m_in.addr >= MNX_OPEN_MAX) 
				ERROR_RETURN (EDVSINVAL);
			if ((r = get_fd(fs_m_in.addr, 0, &new_fd, &dummy)) != OK) 
				ERROR_RETURN (r);
			f->filp_count++;
			fs_proc->fp_filp[new_fd] = f;
			return (new_fd);

		case F_GETFD:
			/* Get close-on-exec flag (FD_CLOEXEC in POSIX Table 6-2). */
			return ( ((fs_proc->fp_cloexec >> fs_m_in.fd) & 01) ? FD_CLOEXEC : 0);

		case F_SETFD:
			/* Set close-on-exec flag (FD_CLOEXEC in POSIX Table 6-2). */
			cloexec_mask = 1L << fs_m_in.fd; /* singleton set position ok */
			clo_value = (fs_m_in.addr & FD_CLOEXEC ? cloexec_mask : 0L);
			fs_proc->fp_cloexec = (fs_proc->fp_cloexec & ~cloexec_mask) | clo_value;
			return (OK);
		case F_GETFL:
			/* Get file status flags (O_NONBLOCK and O_APPEND). */
			fl = f->filp_flags & (O_NONBLOCK | O_APPEND | O_ACCMODE);
			return (fl);
		case F_SETFL:
			/* Set file status flags (O_NONBLOCK and O_APPEND). */
			fl = O_NONBLOCK | O_APPEND;
			f->filp_flags = (f->filp_flags & ~fl) | (fs_m_in.addr & fl);
			return (OK);
		case F_GETLK:
		case F_SETLK:
		case F_SETLKW:
			/* Set or clear a file lock. */
			r = lock_op(f, fs_m_in.request);
			return (r);

		case F_FREESP:	{
			/* Free a section of a file. Preparation is done here,
			 * actual freeing in freesp_inode().
			 */
			mnx_off_t start, end;
			struct mnx_flock flock_arg;
			signed long offset;

			/* Check if it's a regular file. */
			if ((f->filp_ino->i_mode & I_TYPE) != I_REGULAR) {
				ERROR_RETURN(EDVSINVAL);
			}

			/* Copy fs_flock data from userspace. */
			// if((r = sys_datacopy(fs_who_e, (vir_bytes) fs_m_in.name1,
			//   fs_ep, (vir_bytes) &flock_arg,
			//   (phys_bytes) sizeof(flock_arg))) != OK)
			MUK_vcopy(r, fs_who_e, fs_m_in.name1, fs_ep, &flock_arg, sizeof(flock_arg))
			if (r < 0) ERROR_RETURN(r);

			/* Convert starting offset to signed. */
			offset = (signed long) flock_arg.l_start;

			/* Figure out starting position base. */
			switch (flock_arg.l_whence) {
				case SEEK_SET: 
					start = 0; 
					if (offset < 0) 
						ERROR_RETURN(EDVSINVAL); 
					break;
				case SEEK_CUR: 
					start = f->filp_pos; 
					break;
				case SEEK_END: 
					start = f->filp_ino->i_size; 
					break;
				default: 
					ERROR_RETURN(EDVSINVAL);
			}

			/* Check for overflow or underflow. */
			if (offset > 0 && start + offset < start)
				ERROR_RETURN(EDVSINVAL);
			if (offset < 0 && start + offset > start) 
				ERROR_RETURN(EDVSINVAL);
			start += offset;
			if (flock_arg.l_len > 0) {
				end = start + flock_arg.l_len;
				if (end <= start) {
					ERROR_RETURN(EDVSINVAL);
				}
				r = freesp_inode(f->filp_ino, start, end);
			} else {
				r = truncate_inode(f->filp_ino, start);
			}
			return r;
		}
		default:
			ERROR_RETURN(EDVSINVAL);
  }
}

/*===========================================================================*
 *        fs_sync              *
 *===========================================================================*/
int fs_sync()
{
  /* Perform the sync() system call.  Flush all the tables.
   * The order in which the various tables are flushed is critical.  The
   * blocks must be flushed last, since rw_inode() leaves its results in
   * the block cache.
   */
  register struct inode *rip;
  register struct buf *bp;

  /* Write all the dirty inodes to the disk. */
  for (rip = &inode[0]; rip < &inode[NR_INODES]; rip++)
    if (rip->i_count > 0 && rip->i_dirt == DIRTY) rw_inode(rip, WRITING);

// MUKDEBUG("Llamada a FLUSHALL desde fs_sync ------//////////////////////////////// ");

  /* Write all the dirty blocks to the disk, one drive at a time. */
  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++)
    if (bp->b_dev != NO_DEV && bp->b_dirt == DIRTY) flushall(bp->b_dev);

  return (OK);  /* sync() can't fail */
}

/*===========================================================================*
 *        fs_fsync             *
 *===========================================================================*/
int fs_fsync()
{
  /* Perform the fsync() system call. For now, don't be unnecessarily smart. */

  fs_sync();

  return (OK);
}


/*===========================================================================*
 *        fs_set               *
 *===========================================================================*/
int fs_set()
{
  /* Set uid_t or gid_t field. */

  register fproc_t *tfp;
  int proc;

  /* Only PM may make this call directly. */
  if (fs_who_e != pm_ep) return (EDVSGENERIC);

  okendpt(fs_m_in.endpt1, &proc);
  tfp = &fs_proc_table[proc];
  if (fs_call_nr == MOLSETUID) {
    tfp->fp_realuid = (mnx_uid_t) fs_m_in.real_user_id;
    tfp->fp_effuid =  (mnx_uid_t) fs_m_in.eff_user_id;
  }
  if (fs_call_nr == MOLSETGID) {
    tfp->fp_effgid =  (mnx_gid_t) fs_m_in.eff_grp_id;
    tfp->fp_realgid = (mnx_gid_t) fs_m_in.real_grp_id;
  }
  return (OK);
}

/*===========================================================================*
 *        fs_revive            *
 *===========================================================================*/
int fs_revive()
{
  /* A driver, typically TTY, has now gotten the characters that were needed for
   * a previous read.  The process did not get a fs_reply when it made the call.
   * Instead it was suspended.  Now we can send the fs_reply to wake it up.  This
   * business has to be done carefully, since the incoming message is from
   * a driver (to which no fs_reply can be sent), and the fs_reply must go to a process
   * that blocked earlier.  The fs_reply to the caller is inhibited by returning the
   * 'SUSPEND' pseudo error, and the fs_reply to the blocked process is done
   * explicitly in revive().
   */
  revive(fs_m_in.REP_ENDPT, fs_m_in.REP_STATUS);
  return (SUSPEND);   /* don't fs_reply to the TTY task */
}

/*===========================================================================*
 *        fs_svrctl            *
 *===========================================================================*/
int fs_svrctl()
{
  switch (fs_m_in.svrctl_req) {
  case FSSIGNON: {
    /* A server in user space calls in to manage a device. */
    struct fssignon device;
    int r, major, proc_nr_n;

    if (fs_proc->fp_effuid != SU_UID && fs_proc->fp_effuid != SERVERS_UID)
      return (EDVSPERM);

    /* Try to copy request structure to FS. */
    // if ((r = sys_datacopy(fs_who_e, (vir_bytes) fs_m_in.svrctl_argp,
    //                       fs_ep, (vir_bytes) &device,
    //                       (phys_bytes) sizeof(device))) != OK)
	MUK_vcopy(r, fs_who_e, fs_m_in.svrctl_argp,
                          fs_ep, &device, sizeof(device));
    if (r < 0) ERROR_RETURN(r);

    if (isokendpt(fs_who_e, &proc_nr_n) != OK)
      return (EDVSINVAL);

    /* Try to update device mapping. */
    major = (device.dev >> MNX_MAJOR) & BYTE;
    r = map_driver(major, fs_who_e, device.style);
    if (r == OK)
    {
      /* If a driver has completed its exec(), it can be announced
       * to be up.
      */
      if (fs_proc_table[proc_nr_n].fp_execced) {
        dev_up(major);
      } else {
        fs_dmap_tab[major].dmap_flags |= DMAP_BABY;
      }
    }

    return (r);
  }
  case FSDEVUNMAP: {
    struct fsdevunmap fdu;
    int r, major;
    /* Try to copy request structure to FS. */
	MUK_vcopy(r, fs_who_e,  fs_m_in.svrctl_argp,
                       fs_ep, &fdu,
                       sizeof(fdu));
    if (r < OK) ERROR_RETURN(r);
    major = (fdu.dev >> MNX_MAJOR) & BYTE;
    r = map_driver(major, NONE, 0);
	if( r < 0) ERROR_RETURN(r);
	return (r);
  }
  default:
    ERROR_RETURN(EDVSINVAL);
  }
}


/*===========================================================================*
 *				fs_fork					     *
 *===========================================================================*/
int fs_fork(void)
{
  fproc_t *cp;
  int i, parentno, childno;

   MUKDEBUG("parent_endpt=%d parent_endpt=%d pid=%d\n",
		fs_m_in.parent_endpt,
		fs_m_in.child_endpt,
		fs_m_in.pid);

  /* Only PM may make this call directly. */
  if (fs_who_e != pm_ep) ERROR_RETURN(EDVSGENERIC);

  /* Check up-to-dateness of fproc. */
  okendpt(fs_m_in.parent_endpt, &parentno);

  /* PM gives child endpoint, which implies process slot information.
   * Don't call isokendpt, because that will verify if the endpoint
   * number is correct in fproc, which it won't be.
   */
  childno = _ENDPOINT_P(fs_m_in.child_endpt);
  assert(childno > 0 && childno < NR_PROCS);
  MUKDEBUG("fs_proc_table[%d].fp_pid=%d\n", childno, fs_proc_table[childno].fp_pid);
  if ( fs_proc_table[childno].fp_pid != fs_m_in.pid)
	assert(fs_proc_table[childno].fp_pid == PID_FREE);
  
  /* Copy the parent's fproc struct to the child. */
  fs_proc_table[childno] = fs_proc_table[parentno];

  /* Increase the counters in the 'filp' table. */
  cp = &fs_proc_table[childno];
  for (i = 0; i < OPEN_MAX; i++)
	if (cp->fp_filp[i] != NIL_FILP) cp->fp_filp[i]->filp_count++;

  /* Fill in new process and endpoint id. */
  cp->fp_pid = fs_m_in.pid;
  cp->fp_endpoint = fs_m_in.child_endpt;

  /* A child is not a process leader. */
  cp->fp_sesldr = 0;

  /* This child has not exec()ced yet. */
  cp->fp_execced = 0;

  /* Record the fact that both root and working dir have another user. */
  dup_inode(cp->fp_rootdir);
  dup_inode(cp->fp_workdir);
  return(OK);
}


