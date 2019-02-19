/* This file contains the code for performing four system calls relating to
 * status and directories.
 *
 * The entry points into this file are
 *   do_chdir:	perform the CHDIR system call
 *   do_chroot:	perform the CHROOT system call
 *   do_stat:	perform the STAT system call
 *   do_fstat:	perform the FSTAT system call
 *   do_fstatfs: perform the FSTATFS system call
 *   do_lstat:  perform the LSTAT system call
 *   do_rdlink: perform the RDLNK system call
 */

#include "fs.h"

int change(struct inode **iip, char *name_ptr, int len);
int change_into(struct inode **iip, struct inode *rip);
int stat_inode(register struct inode *rip, struct filp *fil_ptr, char *user_addr);

/*===========================================================================*
 *        do_fchdir            *
 *===========================================================================*/
int do_fchdir()
{
  /* Change directory on already-opened fd. */
  struct filp *rfilp;

  /* Is the file descriptor valid? */
  if ( (rfilp = get_filp(m_in.fd)) == NIL_FILP) return(err_code);
  dup_inode(rfilp->filp_ino);
  return change_into(&fp->fp_workdir, rfilp->filp_ino);
}

/*===========================================================================*
 *        do_chdir             *
 *===========================================================================*/
int do_chdir()
{
/* Change directory.  This function is  also called by MM to simulate a chdir
 * in order to do EXEC, etc.  It also changes the root directory, the uids and
 * gids, and the umask. 
 */

  int r;
  register struct fproc *rfp;

  if (who_e == PM_PROC_NR) {
  int slot;
  if(isokendpt(m_in.endpt1, &slot) != OK)
    return EDVSINVAL;
  rfp = &fproc[slot];
  put_inode(fp->fp_rootdir);
  dup_inode(fp->fp_rootdir = rfp->fp_rootdir);
  put_inode(fp->fp_workdir);
  dup_inode(fp->fp_workdir = rfp->fp_workdir);

  /* MM uses access() to check permissions.  To make this work, pretend
   * that the user's real ids are the same as the user's effective ids.
   * FS calls other than access() do not use the real ids, so are not
   * affected.
   */
  fp->fp_realuid =
  fp->fp_effuid = rfp->fp_effuid;
  fp->fp_realgid =
  fp->fp_effgid = rfp->fp_effgid;
  fp->fp_umask = rfp->fp_umask;
  return(OK);
  }

  /* Perform the chdir(name) system call. */
  r = change(&fp->fp_workdir, m_in.name, m_in.name_length);
  return(r);
}

/*===========================================================================*
 *        do_chroot            *
 *===========================================================================*/
int do_chroot()
{
/* Perform the chroot(name) system call. */

  register int r;

  if (!super_user) return(EDVSPERM); /* only su may chroot() */
  r = change(&fp->fp_rootdir, m_in.name, m_in.name_length);
  return(r);
}

/*===========================================================================*
 *        change               *
 *===========================================================================*/
int change(struct inode **iip, char *name_ptr, int len)
//struct inode **iip;   /* pointer to the inode pointer for the dir */
//char *name_ptr;     /* pointer to the directory name to change to */
//int len;      /* length of the directory name string */
{
/* Do the actual work for chdir() and chroot(). */
  struct inode *rip;

  /* Try to open the new directory. */
  if (fetch_name(name_ptr, len, M3) != OK) return(err_code);
  if ( (rip = eat_path(user_path)) == NIL_INODE) return(err_code);
  return change_into(iip, rip);
}


/*===========================================================================*
 *        change_into            *
 *===========================================================================*/
int change_into(struct inode **iip, struct inode *rip)
//struct inode **iip;   /* pointer to the inode pointer for the dir */
//struct inode *rip;    /* this is what the inode has to become */
{
  register int r;

  /* It must be a directory and also be searchable. */
  if ( (rip->i_mode & I_TYPE) != I_DIRECTORY)
  r = EDVSNOTDIR;
  else
  r = forbidden(rip, X_BIT);  /* check if dir is searchable */

  /* If error, return inode. */
  if (r != OK) {
  put_inode(rip);
  return(r);
  }

  /* Everything is OK.  Make the change. */
  put_inode(*iip);    /* release the old directory */
  *iip = rip;     /* acquire the new one */
  return(OK);
}

/*===========================================================================*
 *        do_stat              *
 *===========================================================================*/
int do_stat()
{
/* Perform the stat(name, buf) system call. */

  register struct inode *rip;
  register int r;

  /* Both stat() and fstat() use the same routine to do the real work.  That
   * routine expects an inode, so acquire it temporarily.
   */
  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  if ( (rip = eat_path(user_path)) == NIL_INODE) return(err_code);
  r = stat_inode(rip, NIL_FILP, m_in.name2);  /* actually do the work.*/
  put_inode(rip);   /* release the inode */
  return(r);
}

/*===========================================================================*
 *        do_fstat             *
 *===========================================================================*/
int do_fstat()
{
/* Perform the fstat(fd, buf) system call. */

  register struct filp *rfilp;

  /* Is the file descriptor valid? */
  if ( (rfilp = get_filp(m_in.fd)) == NIL_FILP) return(err_code);

  return(stat_inode(rfilp->filp_ino, rfilp, m_in.buffer));
}

/*===========================================================================*
 *        stat_inode             *
 *===========================================================================*/
int stat_inode(register struct inode *rip, struct filp *fil_ptr, char *user_addr)
//register struct inode *rip; /* pointer to inode to stat */
//struct filp *fil_ptr;   /* filp pointer, supplied by 'fstat' */
//char *user_addr;    /* user space address where stat buf goes */
{
/* Common code for stat and fstat system calls. */

  struct mnx_stat statbuf;
  mode_t mo;
  int r, s;

  /* Update the atime, ctime, and mtime fields in the inode, if need be. */
  if (rip->i_update) update_times(rip);

  /* Fill in the statbuf struct. */
  mo = rip->i_mode & I_TYPE;

  /* true iff special */
  s = (mo == I_CHAR_SPECIAL || mo == I_BLOCK_SPECIAL);

  statbuf.st_dev = rip->i_dev;
  statbuf.st_ino = rip->i_num;
  statbuf.st_mode = rip->i_mode;
  statbuf.st_nlink = rip->i_nlinks;
  statbuf.st_uid = rip->i_uid;
  statbuf.st_gid = rip->i_gid;
  statbuf.st_rdev = (dev_t) (s ? rip->i_zone[0] : NO_DEV);
  statbuf.st_size = rip->i_size;

  if (rip->i_pipe == I_PIPE) {
  statbuf.st_mode &= ~I_REGULAR;  /* wipe out I_REGULAR bit for pipes */
  if (fil_ptr != NIL_FILP && fil_ptr->filp_mode & R_BIT) 
    statbuf.st_size -= fil_ptr->filp_pos;
  }

  statbuf.st_atime = rip->i_atime;
  statbuf.st_mtime = rip->i_mtime;
  statbuf.st_ctime = rip->i_ctime;

  /* Copy the struct to user space. */
  // r = sys_datacopy(FS_PROC_NR, (vir_bytes) &statbuf,
  //     who_e, (vir_bytes) user_addr, (phys_bytes) sizeof(statbuf));
  r = dvk_vcopy(FS_PROC_NR, &statbuf, who_e, user_addr, sizeof(statbuf));  
  if( r < 0) ERROR_RETURN(r);
  return(OK);
}

/*===========================================================================*
 *        do_fstatfs             *
 *===========================================================================*/
int do_fstatfs()
{
  /* Perform the fstatfs(fd, buf) system call. */
  struct statfs st;
  register struct filp *rfilp;
  int r;

  /* Is the file descriptor valid? */
  if ( (rfilp = get_filp(m_in.fd)) == NIL_FILP) return(err_code);

  st.f_bsize = rfilp->filp_ino->i_sp->s_block_size;

  // r = sys_datacopy(FS_PROC_NR, (vir_bytes) &st,
  //     who_e, (vir_bytes) m_in.buffer, (phys_bytes) sizeof(st));

  r = dvk_vcopy(FS_PROC_NR, &st, who_e, m_in.buffer, sizeof(st));    
  if( r < 0) ERROR_RETURN(r);
  return(OK);
}

/*===========================================================================*
 *                             do_lstat                                     *
 *===========================================================================*/
int do_lstat()
{
/* Perform the lstat(name, buf) system call. */

  register int r;              /* return value */
  register struct inode *rip;  /* target inode */

  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  if ((rip = parse_path(user_path, (char *) 0, EAT_PATH_OPAQUE)) == NIL_INODE)
       return(err_code);
  r = stat_inode(rip, NIL_FILP, m_in.name2);
  put_inode(rip);
  return(r);
}

/*===========================================================================*
 *                             do_rdlink                                    *
 *===========================================================================*/
int do_rdlink()
{
/* Perform the readlink(name, buf) system call. */

  register int r;              /* return value */
  mnx_block_t b;                   /* block containing link text */
  struct buf *bp;              /* buffer containing link text */
  register struct inode *rip;  /* target inode */
  int copylen;
  copylen = m_in.m1_i2;
  // SVRDEBUG("copylen=%d, \n", copylen);
  if(copylen < 0) return EDVSINVAL;

  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  if ((rip = parse_path(user_path, (char *) 0, EAT_PATH_OPAQUE)) == NIL_INODE)
       return(err_code);

  r = EDVSACCES;
  // SVRDEBUG("rip->i_mode=%d, \n", rip->i_mode);
  // SVRDEBUG("S_ISLNK(rip->i_mode)=%d, \n", S_ISLNK(rip->i_mode));
  // SVRDEBUG("m_in.name2_length=%d, \n", m_in.name2_length);
  // SVRDEBUG("copylen=%d, \n", copylen);
  if (S_ISLNK(rip->i_mode) && (b = read_map(rip, (mnx_off_t) 0)) != NO_BLOCK) {
       if (m_in.name2_length <= 0) r = EDVSINVAL;
       else if (m_in.name2_length < rip->i_size) r = EDVSRANGE;
       else {
         if(rip->i_size < copylen) copylen = rip->i_size;
               bp = get_block(rip->i_dev, b, NORMAL);
               // SVRDEBUG("DESPUES get_block\n");
    //            r = sys_vircopy(SELF, D, (vir_bytes) bp->b_data,
    // who_e, D, (vir_bytes) m_in.name2, (vir_bytes) copylen);
               // SVRDEBUG("ANTES dvk_vcopy\n");     
               // SVRDEBUG("bp->b_data=%s, \n", bp->b_data);
               // SVRDEBUG("m_in.name2=%s, \n", m_in.name2);
                  r = dvk_vcopy(SELF, bp->b_data, who_e, m_in.name2, copylen);                
               // SVRDEBUG("DESPUES dvk_vcopy\n");                      
			   if( r < 0) ERROR_RETURN(r);
               r = copylen;
               put_block(bp, DIRECTORY_BLOCK);
       }
  }

  put_inode(rip);
  return(r);
}