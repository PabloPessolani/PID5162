/* This file takes care of those system calls that deal with time.
 *
 * The entry points into this file are
 *   do_utime:		perform the UTIME system call
 *   do_stime:		PM informs FS about STIME system call
 */

#include "fs.h"

/*===========================================================================*
 *				do_utime				     *
 *===========================================================================*/
int do_utime()
{
/* Perform the utime(name, timep) system call. */

  register struct inode *rip;
  register int len, r;

  /* Adjust for case of 'timep' being NULL;
   * utime_strlen then holds the actual size: strlen(name)+1.
   */
  len = m_in.utime_length;
  if (len == 0) len = m_in.utime_strlen;

SVRDEBUG("len=%d\n", len);

  /* Temporarily open the file. */
  if (fetch_name(m_in.utime_file, len, M1) != OK) ERROR_RETURN(err_code);
  if ( (rip = eat_path(user_path)) == NIL_INODE) ERROR_RETURN(err_code);

  /* Only the owner of a file or the super_user can change its time. */
  r = OK;
SVRDEBUG("r=%d\n", r);
SVRDEBUG("rip->i_uid=%d\n", rip->i_uid);
SVRDEBUG("fp->fp_effuid=%d\n", fp->fp_effuid);
SVRDEBUG("super_user=%d\n", super_user);   
  if (rip->i_uid != fp->fp_effuid && !super_user) r = EMOLPERM; // VER DE ANULAR ACA O ACOMODAR ESTO
  if (m_in.utime_length == 0 && r != OK) r = forbidden(rip, W_BIT);
  if (read_only(rip) != OK) r = EMOLROFS;	/* not even su can touch if R/O */
  if (r == OK) {
	if (m_in.utime_length == 0) {
		rip->i_atime = clock_time();
		rip->i_mtime = rip->i_atime;
	} else {
		rip->i_atime = m_in.utime_actime;
		rip->i_mtime = m_in.utime_modtime;
	}
	rip->i_update = CTIME;	/* discard any stale ATIME and MTIME flags */
	rip->i_dirt = DIRTY;
  }

  // SVRDEBUG("ANTES DE putinode\n");  

  put_inode(rip);
  return(r);
}

/*===========================================================================*
 *				do_stime				     *
 *===========================================================================*/
int do_stime()
{
/* Perform the stime(tp) system call. */
  boottime = (long) m_in.pm_stime; 
  return(OK);
}

