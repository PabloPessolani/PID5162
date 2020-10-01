/* This file deals with protection in the file system.  It contains the code
 * for four system calls that relate to protection.
 *
 * The entry points into this file are
 *   fs_chmod:	perform the CHMOD system call
 *   fs_chown:	perform the CHOWN system call
 *   fs_umask:	perform the UMASK system call
 *   fs_access:	perform the ACCESS system call
 *   forbidden:	check to see if a given access is allowed on a given inode
 */

// #define MUKDBG    1

#include "fs.h"

/*===========================================================================*
 *        fs_chmod             *
 *===========================================================================*/
int fs_chmod()
{
/* Perform the chmod(name, mode) system call. */

  register struct inode *rip;
  register int r;

  /* Temporarily open the file. */
  if (fetch_name(fs_m_in.name, fs_m_in.name_length, M3) != OK) return(fs_err_code);
  if ( (rip = eat_path(user_path)) == NIL_INODE) return(fs_err_code);

  /* Only the owner or the super_user may change the mode of a file.
   * No one may change the mode of a file on a read-only file system.
   */
  if (rip->i_uid != fs_proc->fp_effuid && !super_user)
  r = EDVSPERM;
  else
  r = read_only(rip);

  /* If error, return inode. */
  if (r != OK)  {
  put_inode(rip);
  return(r);
  }

  /* Now make the change. Clear setgid bit if file is not in caller's grp */
  rip->i_mode = (rip->i_mode & ~ALL_MODES) | (fs_m_in.mode & ALL_MODES);
  if (!super_user && rip->i_gid != fs_proc->fp_effgid)rip->i_mode &= ~I_SET_GID_BIT;
  rip->i_update |= CTIME;
  rip->i_dirt = DIRTY;

  put_inode(rip);
  return(OK);
}

/*===========================================================================*
 *        fs_chown             *
 *===========================================================================*/
int fs_chown()
{
/* Perform the chown(name, owner, group) system call. */

  register struct inode *rip;
  register int r;

  /* Temporarily open the file. */
  if (fetch_name(fs_m_in.name1, fs_m_in.name1_length, M1) != OK) return(fs_err_code);
  if ( (rip = eat_path(user_path)) == NIL_INODE) return(fs_err_code);

  /* Not permitted to change the owner of a file on a read-only file sys. */
  r = read_only(rip);
  if (r == OK) {
  /* FS is R/W.  Whether call is allowed depends on ownership, etc. */
  if (super_user) {
    /* The super user can do anything. */
    rip->i_uid = fs_m_in.owner;  /* others later */
  } else {
    /* Regular users can only change groups of their own files. */
    if (rip->i_uid != fs_proc->fp_effuid) r = EDVSPERM;
    if (rip->i_uid != fs_m_in.owner) r = EDVSPERM;  /* no giving away */
    if (fs_proc->fp_effgid != fs_m_in.group) r = EDVSPERM;
  }
  }
  if (r == OK) {
  rip->i_gid = fs_m_in.group;
  rip->i_mode &= ~(I_SET_UID_BIT | I_SET_GID_BIT);
  rip->i_update |= CTIME;
  rip->i_dirt = DIRTY;
  }

  put_inode(rip);
  return(r);
}

/*===========================================================================*
 *        fs_umask             *
 *===========================================================================*/
int fs_umask()
{
/* Perform the umask(co_mode) system call. */
  register mnx_mode_t r;

  r = ~fs_proc->fp_umask;    /* set 'r' to complement of old mask */
  MUKDEBUG("Before r=%d, \n", r);
  MUKDEBUG("RWX_MODES =%d, \n", RWX_MODES);  
  MUKDEBUG("fs_m_in.co_mode=%d, \n", fs_m_in.co_mode);    
  MUKDEBUG("fs_m_in.co_mode & RWX_MODES=%d, \n", fs_m_in.co_mode & RWX_MODES);    
  MUKDEBUG("~(fs_m_in.co_mode & RWX_MODES)=%d, \n", ~(fs_m_in.co_mode & RWX_MODES));      
  fs_proc->fp_umask = ~(fs_m_in.co_mode & RWX_MODES);
  return(r);      /* return complement of old mask */
}

/*===========================================================================*
 *        fs_access            *
 *===========================================================================*/
int fs_access()
{
/* Perform the access(name, mode) system call. */

  struct inode *rip;
  register int r;

  MUKDEBUG("R_OK=%d, \n", R_OK);
  MUKDEBUG("W_OK=%d, \n", W_OK);
  MUKDEBUG("X_OK=%d, \n", X_OK);
  MUKDEBUG("F_OK=%d, \n", F_OK);
  MUKDEBUG("fs_m_in.mode=%d, \n", fs_m_in.mode);
  /* First check to see if the mode is correct. */
  if ( (fs_m_in.mode & ~(R_OK | W_OK | X_OK)) != 0 && fs_m_in.mode != F_OK)
  return(EDVSINVAL);

  /* Temporarily open the file whose access is to be checked. */
  if (fetch_name(fs_m_in.name, fs_m_in.name_length, M3) != OK) return(fs_err_code);
  if ( (rip = eat_path(user_path)) == NIL_INODE) return(fs_err_code);

  /* Now check the permissions. */
  r = forbidden(rip, (mnx_mode_t) fs_m_in.mode);
  put_inode(rip);
  return(r);
}

/*===========================================================================*
 *        forbidden            *
 *===========================================================================*/
int forbidden(struct inode *rip, mnx_mode_t access_desired)
{
/* Given a pointer to an inode, 'rip', and the access desired, determine
 * if the access is allowed, and if not why not.  The routine looks up the
 * caller's uid in the 'fs_proc_table' table.  If access is allowed, OK is returned
 * if it is forbidden, EACCES is returned.
 */

  struct inode *old_rip = rip;
  struct super_block *sp;
  mnx_mode_t bits, perm_bits;
  int r, shift, test_uid, test_gid, type;

  if (rip->i_mount == I_MOUNT)  /* The inode is mounted on. */
  for (sp = &super_block[1]; sp < &super_block[NR_SUPERS]; sp++)
    if (sp->s_imount == rip) {
      rip = get_inode(sp->s_dev, ROOT_INODE);
      break;
    } /* if */

  /* Isolate the relevant rwx bits from the mode. */
  bits = rip->i_mode;
  test_uid = (fs_call_nr == EDVSACCES ? fs_proc->fp_realuid : fs_proc->fp_effuid);
  test_gid = (fs_call_nr == EDVSACCES ? fs_proc->fp_realgid : fs_proc->fp_effgid);
  if (test_uid == SU_UID) {
  /* Grant read and write permission.  Grant search permission for
   * directories.  Grant execute permission (for non-directories) if
   * and only if one of the 'X' bits is set.
   */
  if ( (bits & I_TYPE) == I_DIRECTORY ||
       bits & ((X_BIT << 6) | (X_BIT << 3) | X_BIT))
    perm_bits = R_BIT | W_BIT | X_BIT;
  else
    perm_bits = R_BIT | W_BIT;
  } else {
  if (test_uid == rip->i_uid) shift = 6;    /* owner */
  else if (test_gid == rip->i_gid ) shift = 3;  /* group */
  else shift = 0;         /* other */
  perm_bits = (bits >> shift) & (R_BIT | W_BIT | X_BIT);
  }

  /* If access desired is not a subset of what is allowed, it is refused. */
  r = OK;
MUKDEBUG("access_desired=%d \n", access_desired);
MUKDEBUG("perm_bits=%d perm_bits(OCT)=%#o\n", perm_bits, perm_bits); 
MUKDEBUG("access_desired=%d access_desired(OCT)=%#o\n", access_desired, access_desired);     

	if ((perm_bits | access_desired) != perm_bits) r = EACCES;


  /* Check to see if someone is trying to write on a file system that is
   * mounted read-only.
   */
  type = rip->i_mode & I_TYPE;
  if (r == OK)
  if (access_desired & W_BIT)
    r = read_only(rip);

  if (rip != old_rip) put_inode(rip);

	MUKDEBUG("r=%d\n", r);
// 
  return(r);
}

/*===========================================================================*
 *        read_only            *
 *===========================================================================*/
int read_only(struct inode *ip)
//struct inode *ip;   /* ptr to inode whose file sys is to be cked */
{
/* Check to see if the file system on which the inode 'ip' resides is mounted
 * read only.  If so, return EROFS, else return OK.
 */

  struct super_block *sp;
  // MUKDEBUG("READONLY\n"); 
  sp = ip->i_sp;
  return(sp->s_rd_only ? EDVSROFS : OK);
}