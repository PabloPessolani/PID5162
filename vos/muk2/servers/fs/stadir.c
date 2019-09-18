/* This file contains the code for performing four system calls relating to
 * status and directories.
 *
 * The entry points into this file are
 *   fs_chdir:	perform the CHDIR system call
 *   fs_chroot:	perform the CHROOT system call
 *   fs_stat:	perform the STAT system call
 *   fs_fstat:	perform the FSTAT system call
 *   fs_fstatfs: perform the FSTATFS system call
 *   fs_lstat:  perform the LSTAT system call
 *   fs_rdlink: perform the RDLNK system call
 */

#include "fs.h"
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

int change(struct inode **iip, char *name_ptr, int len);
int change_into(struct inode **iip, struct inode *rip);
int stat_inode(register struct inode *rip, struct filp *fil_ptr, char *user_addr);

/*===========================================================================*
 *        fs_fchdir            *
 *===========================================================================*/
int fs_fchdir()
{
  /* Change directory on already-opened fd. */
  struct filp *rfilp;

  /* Is the file descriptor valid? */
  if ( (rfilp = get_filp(fs_m_in.fd)) == NIL_FILP) return(fs_err_code);
  dup_inode(rfilp->filp_ino);
  return change_into(&fs_proc->fp_workdir, rfilp->filp_ino);
}

/*===========================================================================*
 *        fs_chdir             *
 *===========================================================================*/
int fs_chdir()
{
/* Change directory.  This function is  also called by MM to simulate a chdir
 * in order to do EXEC, etc.  It also changes the root directory, the uids and
 * gids, and the umask. 
 */

  int r;
  register fproc_t *rfp;

  if (fs_who_e == pm_ep) {
  int slot;
  if(isokendpt(fs_m_in.endpt1, &slot) != OK)
    return EDVSINVAL;
  rfp = &fs_proc_table[slot];
  put_inode(fs_proc->fp_rootdir);
  dup_inode(fs_proc->fp_rootdir = rfp->fp_rootdir);
  put_inode(fs_proc->fp_workdir);
  dup_inode(fs_proc->fp_workdir = rfp->fp_workdir);

  /* MM uses access() to check permissions.  To make this work, pretend
   * that the user's real ids are the same as the user's effective ids.
   * FS calls other than access() do not use the real ids, so are not
   * affected.
   */
  fs_proc->fp_realuid =
  fs_proc->fp_effuid = rfp->fp_effuid;
  fs_proc->fp_realgid =
  fs_proc->fp_effgid = rfp->fp_effgid;
  fs_proc->fp_umask = rfp->fp_umask;
  return(OK);
  }

  /* Perform the chdir(name) system call. */
  r = change(&fs_proc->fp_workdir, fs_m_in.name, fs_m_in.name_length);
  return(r);
}

/*===========================================================================*
 *        fs_chroot            *
 *===========================================================================*/
int fs_chroot()
{
/* Perform the chroot(name) system call. */

  register int r;

  if (!super_user) return(EDVSPERM); /* only su may chroot() */
  r = change(&fs_proc->fp_rootdir, fs_m_in.name, fs_m_in.name_length);
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
  if (fetch_name(name_ptr, len, M3) != OK) return(fs_err_code);
  if ( (rip = eat_path(user_path)) == NIL_INODE) return(fs_err_code);
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
 *        fs_stat              *
 *===========================================================================*/
int fs_stat()
{
/* Perform the stat(name, buf) system call. */

  register struct inode *rip;
  register int r;

  /* Both stat() and fstat() use the same routine to do the real work.  That
   * routine expects an inode, so acquire it temporarily.
   */
  if (fetch_name(fs_m_in.name1, fs_m_in.name1_length, M1) != OK) return(fs_err_code);
  if ( (rip = eat_path(user_path)) == NIL_INODE) return(fs_err_code);
  r = stat_inode(rip, NIL_FILP, fs_m_in.name2);  /* actually do the work.*/
  put_inode(rip);   /* release the inode */
  return(r);
}

/*===========================================================================*
 *        fs_fstat             *
 *===========================================================================*/
int fs_fstat()
{
/* Perform the fstat(fd, buf) system call. */

  register struct filp *rfilp;

  /* Is the file descriptor valid? */
  if ( (rfilp = get_filp(fs_m_in.fd)) == NIL_FILP) return(fs_err_code);

  return(stat_inode(rfilp->filp_ino, rfilp, fs_m_in.buffer));
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

  statbuf.mnx_st_dev = rip->i_dev;
  statbuf.mnx_st_ino = rip->i_num;
  statbuf.mnx_st_mode = rip->i_mode;
  statbuf.mnx_st_nlink = rip->i_nlinks;
  statbuf.mnx_st_uid = rip->i_uid;
  statbuf.mnx_st_gid = rip->i_gid;
  statbuf.mnx_st_rdev = (dev_t) (s ? rip->i_zone[0] : NO_DEV);
  statbuf.mnx_st_size = rip->i_size;

  if (rip->i_pipe == I_PIPE) {
  statbuf.mnx_st_mode &= ~I_REGULAR;  /* wipe out I_REGULAR bit for pipes */
  if (fil_ptr != NIL_FILP && fil_ptr->filp_mode & R_BIT) 
    statbuf.mnx_st_size -= fil_ptr->filp_pos;
  }

  statbuf.mnx_st_atime = rip->i_atime;
  statbuf.mnx_st_mtime = rip->i_mtime;
  statbuf.mnx_st_ctime = rip->i_ctime;

	/* Copy the struct to user space. */
	// r = sys_datacopy(fs_ep, (vir_bytes) &statbuf,
	//     fs_who_e, (vir_bytes) user_addr, (phys_bytes) sizeof(statbuf));	
	//  r = muk_vcopy(fs_ep, &statbuf, fs_who_e, user_addr, sizeof(statbuf));  
	MUK_vcopy(r, fs_ep, &statbuf, fs_who_e, 
		user_addr, sizeof(statbuf));  
	if( r < 0) ERROR_RETURN(r);
	return(OK);
}

/*===========================================================================*
 *        fs_fstatfs             *
 *===========================================================================*/
int fs_fstatfs()
{
  /* Perform the fstatfs(fd, buf) system call. */
  struct statfs st;
  register struct filp *rfilp;
  int r;

  /* Is the file descriptor valid? */
  if ( (rfilp = get_filp(fs_m_in.fd)) == NIL_FILP) return(fs_err_code);

  st.f_bsize = rfilp->filp_ino->i_sp->s_block_size;

  // r = sys_datacopy(fs_ep, (vir_bytes) &st,
  //     fs_who_e, (vir_bytes) fs_m_in.buffer, (phys_bytes) sizeof(st));

  MUK_vcopy(r, fs_ep, &st, fs_who_e, fs_m_in.buffer, sizeof(st));    
  if( r < 0) ERROR_RETURN(r);
  return(OK);
}

/*===========================================================================*
 *                             fs_lstat                                     *
 *===========================================================================*/
int fs_lstat()
{
/* Perform the lstat(name, buf) system call. */

  register int r;              /* return value */
  register struct inode *rip;  /* target inode */

  if (fetch_name(fs_m_in.name1, fs_m_in.name1_length, M1) != OK) return(fs_err_code);
  if ((rip = parse_path(user_path, (char *) 0, EAT_PATH_OPAQUE)) == NIL_INODE)
       return(fs_err_code);
  r = stat_inode(rip, NIL_FILP, fs_m_in.name2);
  put_inode(rip);
  return(r);
}

/*===========================================================================*
 *                             fs_rdlink                                    *
 *===========================================================================*/
int fs_rdlink()
{
/* Perform the readlink(name, buf) system call. */

  register int r;              /* return value */
  mnx_block_t b;                   /* block containing link text */
  struct buf *bp;              /* buffer containing link text */
  register struct inode *rip;  /* target inode */
  int copylen;
  copylen = fs_m_in.m1_i2;
  // MUKDEBUG("copylen=%d, \n", copylen);
  if(copylen < 0) return EDVSINVAL;

  if (fetch_name(fs_m_in.name1, fs_m_in.name1_length, M1) != OK) return(fs_err_code);
  if ((rip = parse_path(user_path, (char *) 0, EAT_PATH_OPAQUE)) == NIL_INODE)
       return(fs_err_code);

  r = EDVSACCES;
  // MUKDEBUG("rip->i_mode=%d, \n", rip->i_mode);
  // MUKDEBUG("S_ISLNK(rip->i_mode)=%d, \n", S_ISLNK(rip->i_mode));
  // MUKDEBUG("fs_m_in.name2_length=%d, \n", fs_m_in.name2_length);
  // MUKDEBUG("copylen=%d, \n", copylen);
  if (S_ISLNK(rip->i_mode) && (b = read_map(rip, (mnx_off_t) 0)) != NO_BLOCK) {
       if (fs_m_in.name2_length <= 0) r = EDVSINVAL;
       else if (fs_m_in.name2_length < rip->i_size) r = EDVSRANGE;
       else {
         if(rip->i_size < copylen) copylen = rip->i_size;
               bp = get_block(rip->i_dev, b, NORMAL);
               // MUKDEBUG("DESPUES get_block\n");
    //            r = sys_vircopy(SELF, D, (vir_bytes) bp->b_data,
    // fs_who_e, D, (vir_bytes) fs_m_in.name2, (vir_bytes) copylen);
               // MUKDEBUG("ANTES muk_vcopy\n");     
               // MUKDEBUG("bp->b_data=%s, \n", bp->b_data);
               // MUKDEBUG("fs_m_in.name2=%s, \n", fs_m_in.name2);
               MUK_vcopy(r, SELF, bp->b_data, fs_who_e, fs_m_in.name2, copylen);                
			   if( r < 0) ERROR_RETURN(r);
               r = copylen;
               put_block(bp, DIRECTORY_BLOCK);
       }
  }

  put_inode(rip);
  return(r);
}