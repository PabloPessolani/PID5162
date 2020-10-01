/* This file takes care of those system calls that deal with time.
 *
 * The entry points into this file are
 *   fs_utime:		perform the UTIME system call
 *   fs_stime:		PM informs FS about STIME system call
 */

#include "fs.h"
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"


/*===========================================================================*
 *				fs_utime				     *
 *===========================================================================*/
int fs_utime()
{
/* Perform the utime(name, timep) system call. */

  register struct inode *rip;
  register int len, r;

  /* Adjust for case of 'timep' being NULL;
   * utime_strlen then holds the actual size: strlen(name)+1.
   */
  len = fs_m_in.utime_length;
  if (len == 0) len = fs_m_in.utime_strlen;

MUKDEBUG("len=%d\n", len);

  /* Temporarily open the file. */
  if (fetch_name(fs_m_in.utime_file, len, M1) != OK) return(fs_err_code);
  if ( (rip = eat_path(user_path)) == NIL_INODE) return(fs_err_code);

  /* Only the owner of a file or the super_user can change its time. */
  r = OK;
MUKDEBUG("r=%d\n", r);
MUKDEBUG("rip->i_uid=%d\n", rip->i_uid);
MUKDEBUG("fs_proc->fp_effuid=%d\n", fs_proc->fp_effuid);
MUKDEBUG("super_user=%d\n", super_user);   
  if (rip->i_uid != fs_proc->fp_effuid && !super_user) r = EDVSPERM; // VER DE ANULAR ACA O ACOMODAR ESTO
  if (fs_m_in.utime_length == 0 && r != OK) r = forbidden(rip, W_BIT);
  if (read_only(rip) != OK) r = EDVSROFS;	/* not even su can touch if R/O */
  if (r == OK) {
	if (fs_m_in.utime_length == 0) {
		rip->i_atime = clock_time();
		rip->i_mtime = rip->i_atime;
	} else {
		rip->i_atime = fs_m_in.utime_actime;
		rip->i_mtime = fs_m_in.utime_modtime;
	}
	rip->i_update = CTIME;	/* discard any stale ATIME and MTIME flags */
	rip->i_dirt = DIRTY;
  }

  // MUKDEBUG("ANTES DE putinode\n");  

  put_inode(rip);
  return(r);
}

/*===========================================================================*
 *				fs_stime				     *
 *===========================================================================*/
int fs_stime()
{
/* Perform the stime(tp) system call. */
  boottime = (long) fs_m_in.pm_stime; 
  return(OK);
}

