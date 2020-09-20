
#define _GNU_SOURCE
#include "rhs.h"
#include "rhostfs_glo.h"
#include <syscall.h> 
#include "/usr/src/linux/arch/sh/include/uapi/asm/unistd_32.h"
#include "rhostfs_proto.h"

long lcl_get_rootpath(message *mptr)
{
	int rcode;
	int len; 

	RHSDEBUG("rhs_dir=%s\n",rhs_dir);
	rcode = dvk_vcopy(rhs_ep, rhs_dir, mptr->m_source, mptr->m1_p1, strlen(rhs_dir)+1);
	if( rcode < 0) ERROR_RETURN(rcode);
	return(OK);
}

long lcl_reply(int dest, int err, message *mptr)
{
	int rcode;
	mptr->m_type = err;
	rcode = dvk_send_T(dest, mptr, TIMEOUT_RMTCALL);
	if (rcode < 0) ERROR_RETURN(rcode);
	return(OK);
}

long lcl_gethostname(message *mptr)
{
	int rcode;
	int len; 
	char hname[HOST_NAME_MAX+1]; 

	len = mptr->m1_i1;
	RHSDEBUG("len=%d\n", len);
	rcode = gethostname(hname, len);
	if( rcode < 0) ERROR_RETURN(-errno);
	RHSDEBUG("hname=%s\n", hname);
	rcode = dvk_vcopy(rhs_ep, hname, mptr->m_source, mptr->m1_p1, strlen(hname)+1);
	if( rcode < 0) ERROR_RETURN(rcode);
	return(OK);
}

long lcl_open(message *mptr)
{
	int rcode;
	char name[PATH_MAX+1]; 
	
	int len 	= mptr->m1_i1;
	int flags 	= mptr->m1_i2;
	int mode 	= mptr->m1_i3;
	RHSDEBUG("len=%d flags=%X mode=%X\n" , len, flags, mode);
	
	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p1, rhs_ep, name, mptr->m1_i1);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("name=%s\n", name);

	rcode = open(name, flags, mode);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);	
}

long lcl_access(message *mptr)
{
	int rcode;
	char name[PATH_MAX+1]; 

	int len 	= mptr->m1_i1;
	int mode 	= mptr->m1_i2;
	RHSDEBUG("len=%d mode=%X\n" , len, mode);
	
	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p1, rhs_ep, name, mptr->m1_i1);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("name=%s\n", name);

	rcode = access(name, mode);
	if( rcode < 0)
		ERROR_RETURN(-errno);
	return(rcode);
}


long lcl_pread(message *mptr)
{
	int rcode;
	char *buf;
	long lpos[2];
	long long pos, *ptr;
	
	int		fd 		= mptr->m2_i1;
	long 	count 	= mptr->m2_i2;
	lpos[0] 		= mptr->m2_l1;
	lpos[1] 		= mptr->m2_l2;
	ptr = (long long *) lpos;
	pos = *ptr;
	RHSDEBUG("fd=%d count=%ld pos=%lld\n" , fd, count, pos);

	buf = calloc( 1, count);
	if (buf == NULL) ERROR_RETURN(-errno);
	
	rcode = pread(fd, buf, count, pos);
	if( rcode < 0){
		rcode = -errno;
		goto free_buf;
	} 

	rcode = dvk_vcopy(rhs_ep, buf, mptr->m_source, mptr->m2_p1, count);
	if( rcode < 0) goto free_buf;
	
free_buf:	
	free(buf);
	if( rcode < 0) ERROR_RETURN(rcode);
	return(rcode);
}

long lcl_pwrite(message *mptr)
{
	int rcode;
	char *buf;
	long lpos[2];
	long long pos, *ptr;
	
	int		fd 		= mptr->m2_i1;
	long 	count 	= mptr->m2_i2;
	lpos[0] 		= mptr->m2_l1;
	lpos[1] 		= mptr->m2_l2;
	ptr = (long long *) lpos;
	pos = *ptr;
	RHSDEBUG("fd=%d count=%ld pos=%lld\n" , fd, count, pos);

	buf = calloc( 1, count);
	if (buf == NULL) ERROR_RETURN(-errno);

	rcode = dvk_vcopy( mptr->m_source, mptr->m2_p1, rhs_ep, buf,  count);
	if( rcode < 0) goto free_buf;
	if(rcode != count) RHSDEBUG("WARNING: (rcode=%d) != (count=%d)\n",rcode, count);
	if(rcode < count) count = rcode;
	
	rcode = pwrite(fd, buf, count, pos);
	if( rcode < 0){
		rcode = -errno;
		goto free_buf;
	} 
	
free_buf:	
	free(buf);
	if( rcode < 0) ERROR_RETURN(rcode);
	return(rcode);
}


long lcl_llseek(message *mptr)
{
	int rcode;
	long result; 

	int fd = mptr->m2_i1;
	int whence = mptr->m2_i2;
	long offset_high = mptr->m2_l1;
	long offset_low = mptr->m2_l2;
	RHSDEBUG("fd=%d high=%ld low=%ld whence=%d\n" , fd, offset_high, offset_low, whence);
	
//	rcode = _llseek(fd, offset_high, offset_low, &result, whence);
	rcode = syscall(SYS__llseek, fd, offset_high, offset_low, &result, whence);
	if( rcode < 0) ERROR_RETURN(-errno);
	
	mptr->m2_p1 = result;
	return(rcode);
}

long lcl_fsync(message *mptr)
{
	int rcode;

	int fd = mptr->m1_i1;
	RHSDEBUG("fd=%d\n" , fd);
	rcode = fsync(fd);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

long lcl_dup2(message *mptr)
{
	int rcode;

	int oldfd = mptr->m1_i1;
	int newfd = mptr->m1_i2;
	RHSDEBUG("oldfd=%d newfd=%d\n" , oldfd, newfd);

	rcode = dup2( oldfd, newfd);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);	
}

long lcl_fchmod(message *mptr)
{
	int rcode;

	int fd 	= mptr->m1_i1;
	int mode= mptr->m1_i2;	
	RHSDEBUG("fd=%d mode=%X\n" , fd, mode);

	rcode = fchmod(fd, mode);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);	
}

long lcl_chmod(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1]; 
	
	int len 	= mptr->m1_i1;
	int mode 	= mptr->m1_i2;
	RHSDEBUG("len=%d mode=%X\n" , len, mode);
	
	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p1, rhs_ep, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("pathname=%s\n", pathname);

	rcode = chmod(pathname, mode);	
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

long lcl_fchown(message *mptr)
{
	int rcode;

	int fd 	= mptr->m1_i1;
	int user= mptr->m1_i2;
	int group= mptr->m1_i3;
	RHSDEBUG("fd=%d user=%d group=%d\n" , fd, user, group);

	rcode = fchown(fd, user, group);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);	
}

long lcl_chown(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1]; 
	
	int len = mptr->m1_i1;
	int user = mptr->m1_i2;
	int group = mptr->m1_i3;
	RHSDEBUG("len=%d user=%d group=%d\n" , len, user, group);

	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p1, rhs_ep, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("pathname=%s\n", pathname);	
	
	rcode = chown(pathname, user, group);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

long lcl_ftruncate(message *mptr)
{
	int rcode;

	int fd 		= mptr->m2_i1;
	int length 	= mptr->m2_l1;
	RHSDEBUG("fd=%d length=%uld\n", fd, length);

	rcode = ftruncate(fd, length);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

long lcl_truncate(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1];
	
	int len 	= mptr->m2_i1;
	long length = mptr->m2_l1;
	RHSDEBUG("len=%d length=%uld\n", len, length);

	rcode = dvk_vcopy(mptr->m_source, mptr->m2_p1, rhs_ep, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("pathname=%s\n", pathname);	
	
	rcode =  truncate(pathname, length);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

long lcl_fdatasync(message *mptr)
{
	int rcode;

	int fd 	= mptr->m1_i1;
	RHSDEBUG("fd=%d\n", fd);

	rcode = fdatasync(fd);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}


long lcl_close(message *mptr)
{
	int rcode;

	int fd 	= mptr->m1_i1;
	RHSDEBUG("fd=%d\n", fd);

	rcode = close(fd);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

long lcl_symlink(message *mptr)
{
	int rcode;
	char oldname[PATH_MAX+1];
	char newname[PATH_MAX+1];

	int olen = mptr->m1_i1;
	int nlen = mptr->m1_i2;
	
	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p1, rhs_ep, oldname, olen);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("oldname=%s\n", oldname);

	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p2, rhs_ep, newname, nlen);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("newname=%s\n", newname);

	rcode = symlink(oldname, newname);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);	
}

long lcl_unlink(message *mptr)
{
	int rcode;
	char *name[PATH_MAX+1]; 
	
	int len 	= mptr->m1_i1;
	RHSDEBUG("len=%d\n", len);
	
	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p1, rhs_ep, name, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("name=%s\n", name);

	rcode = unlink(name);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

long lcl_link(message *mptr)
{
	int rcode;
	char oldname[PATH_MAX+1];
	char newname[PATH_MAX+1];

	int olen = mptr->m1_i1;
	int nlen = mptr->m1_i2;
	
	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p1, rhs_ep, oldname, olen);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("oldname=%s\n", oldname);

	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p2, rhs_ep, newname, nlen);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("newname=%s\n", newname);

	rcode = link(oldname, newname);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);	
}


long lcl_readlink(message *mptr)
{
	int rcode;
	char filename[PATH_MAX+1];
	char *buf;
	
	int len = mptr->m1_i1;
	int bufsiz = mptr->m1_i2;
	RHSDEBUG("len=%D bufsiz=%d\n", len, bufsiz);

	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p1, rhs_ep, filename, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("filename=%s\n", filename);
		
	buf = calloc( 1, bufsiz);
	if (buf == NULL) ERROR_RETURN(-errno);
	
	rcode = readlink(filename, buf, bufsiz);
	if( rcode < 0){
		rcode = -errno;
		goto free_buf;
	} 

	rcode = dvk_vcopy(rhs_ep, buf, mptr->m_source, mptr->m1_p2, rcode+1);
	if( rcode < 0) goto free_buf;
	
free_buf:	
	free(buf);
	if( rcode < 0) ERROR_RETURN(rcode);
	return(rcode);
}

long lcl_rename(message *mptr)
{
	int rcode;
	char oldname[PATH_MAX+1];
	char newname[PATH_MAX+1];

	int olen = mptr->m1_i1;
	int nlen = mptr->m1_i2;
	
	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p1, rhs_ep, oldname, olen);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("oldname=%s\n", oldname);

	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p1, rhs_ep, newname, nlen);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("newname=%s\n", newname);

	rcode = rename(oldname, newname);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);	
}

long lcl_rmdir(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1]; 
	
	int len 	= mptr->m1_i1;
	RHSDEBUG("len=%d\n", len);
	
	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p1, rhs_ep, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("pathname=%s\n", pathname);

	rcode = rmdir(pathname);	
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

long lcl_mkdir(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1]; 
	
	int len 	= mptr->m1_i1;
	int mode 	= mptr->m1_i2;
	RHSDEBUG("len=%d mode=%X\n" , len, mode);
	
	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p1, rhs_ep, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("pathname=%s\n", pathname);

	rcode = mkdir(pathname, mode);	
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

long lcl_mknod(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1]; 
	
	int len 	= mptr->m1_i1;
	int mode 	= mptr->m1_i2;
	int dev 	= mptr->m1_i3;
	RHSDEBUG("len=%d mode=%X dev=%d\n" , len, mode, dev);
	
	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p1, rhs_ep, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("pathname=%s\n", pathname);

	rcode = mknod(pathname, mode, dev);	
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

long lcl_futimes(message *mptr)
{
	int rcode;
	const struct timeval tv[2];
	
	int fd 	= mptr->m1_i1;
	RHSDEBUG("fd=%d\n", fd);
	
	rcode = futimes(fd, tv);
	if( rcode < 0) ERROR_RETURN(-errno);
	
	rcode = dvk_vcopy(rhs_ep, tv, mptr->m_source, mptr->m1_p1, sizeof(struct timeval));
	if( rcode < 0) ERROR_RETURN(rcode);

	return(OK);
}

long lcl_utimes(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1]; 
	const struct timeval tv[2];
	
	int len 	= mptr->m1_i1;
	RHSDEBUG("len=%d\n", len);
	
	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p1, rhs_ep, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("pathname=%s\n", pathname);
	
	rcode = lutimes(pathname, tv);
	if( rcode < 0) ERROR_RETURN(-errno);
	
	rcode = dvk_vcopy(rhs_ep, tv, mptr->m_source, mptr->m1_p2, sizeof(struct timeval));
	if( rcode < 0) ERROR_RETURN(rcode);

	return(OK);
}

/* 
struct stat {
    dev_t     st_dev;     		     
    ino_t     st_ino;     
    mode_t    st_mode;    
    nlink_t   st_nlink;   
    uid_t     st_uid;     
    gid_t     st_gid;     
    dev_t     st_rdev;    
    off_t     st_size;    
    blksize_t st_blksize; 
    blkcnt_t  st_blocks;  
    time_t    st_atime;   
    time_t    st_mtime;   
    time_t    st_ctime;   
};
*/ 

long lcl_fstat(message *mptr)
{
	int rcode;
	struct stat statbuf;
	
	int fd 	= mptr->m1_i1;
	RHSDEBUG("fd=%d\n", fd);
	
	rcode = fstat(fd, &statbuf);
	if( rcode < 0) ERROR_RETURN(-errno);
	
	rcode = dvk_vcopy(rhs_ep, &statbuf, mptr->m_source, mptr->m1_p1, sizeof(struct statfs));
	if( rcode < 0) ERROR_RETURN(rcode);

	return(OK);
}

long lcl_statfs(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1]; 
	struct statfs statbuf;
	struct statfs64 statbuf64;
	
	int len 	= mptr->m1_i1;
	RHSDEBUG("len=%d\n", len);
	
	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p1, rhs_ep, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("pathname=%s\n", pathname);
	  
	rcode = statfs(pathname, &statbuf);
	if( rcode < 0) ERROR_RETURN(-errno);
	
	statbuf64.f_type 	= (long int)statbuf.f_type; /* type of file system (see below) */
	statbuf64.f_bsize	= (long int)statbuf.f_bsize; /* optimal transfer block size */
	statbuf64.f_blocks	= statbuf.f_blocks; /* total data blocks in file system */
	statbuf64.f_bfree	= statbuf.f_bfree; /* free blocks in fs */
	statbuf64.f_bavail	= statbuf.f_bavail; /* free blocks available to	unprivileged user */
	statbuf64.f_files	= statbuf.f_files; /* total file nodes in file system */
	statbuf64.f_ffree	= statbuf.f_ffree; /* free file nodes in fs */
	statbuf64.f_fsid	= statbuf.f_fsid; /* file system id */
	statbuf64.f_namelen	= (long int)statbuf.f_namelen; /* maximum length of filenames */
	statbuf64.f_frsize	= (long int)statbuf.f_frsize; /* fragment size (since Linux 2.6) */
	*statbuf64.f_spare	= (long int)*statbuf.f_spare;
	
	rcode = dvk_vcopy(rhs_ep, &statbuf64, mptr->m_source, mptr->m1_p2, sizeof(struct statfs));
	if( rcode < 0) ERROR_RETURN(rcode);

	return(OK);
}

long lcl_lstat(message *mptr)
{
	int rcode;
	struct stat lcl_stat, *ls_ptr;
	struct USR_stat usb, *usb_ptr;
	char pathname[PATH_MAX+1]; 

	int len 	= mptr->m1_i1;
	RHSDEBUG("len=%d\n", len);
	
	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p1, rhs_ep, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("pathname=%s\n", pathname);
	
	rcode = lstat(pathname, &lcl_stat);
	if( rcode < 0) ERROR_RETURN(-errno);
	ls_ptr = &lcl_stat;
	RHSDEBUG(STAT_FORMAT, STAT_FIELDS(ls_ptr));
	RHSDEBUG(STAT2_FORMAT, STAT2_FIELDS(ls_ptr));
	
	usb_ptr = &usb;
	usb_ptr->ust_dev= ls_ptr->st_dev;     /* ID of device containing file */
    usb_ptr->ust_ino= ls_ptr->st_ino;     /* inode number */
    usb_ptr->ust_mode= ls_ptr->st_mode;    /* protection */
    usb_ptr->ust_nlink= ls_ptr->st_nlink;  /* number of hard links */
    usb_ptr->ust_uid= ls_ptr->st_uid;     /* user ID of owner */
    usb_ptr->ust_gid= ls_ptr->st_gid;     /* group ID of owner */

    usb_ptr->ust_rdev= ls_ptr->st_rdev;    /* device ID (if special file) */
    usb_ptr->ust_size= ls_ptr->st_size;   /* total size, in bytes */
	usb_ptr->ust_blksize= ls_ptr->st_blksize;/* blocksize for file system I/O */
    usb_ptr->ust_blocks= ls_ptr->st_blocks;  /* number of 512B blocks allocated */
    usb_ptr->ust_atime= ls_ptr->st_atime;   /* time of last access */
	usb_ptr->ust_mtime= ls_ptr->st_mtime;   /* time of last modification */
    usb_ptr->ust_ctime= ls_ptr->st_ctime;   /* time of last status change */

	RHSDEBUG(USTAT_FORMAT, USTAT_FIELDS(usb_ptr));
	RHSDEBUG(USTAT2_FORMAT, USTAT2_FIELDS(usb_ptr));
	
	rcode = dvk_vcopy(rhs_ep, &usb, mptr->m_source, mptr->m1_p2, sizeof(struct USR_stat));
	if( rcode < 0) ERROR_RETURN(rcode);

	return(OK);
}

// typedef struct {int m7i1, m7i2, m7i3, m7i4; char *m7p1, *m7p2;} mess_7;
long lcl_renameat2(message *mptr)
{
	int rcode;
	char oldname[PATH_MAX+1];
	char newname[PATH_MAX+1];

	int olen = mptr->m7_i1;
	int nlen = mptr->m7_i2;
	int flags= mptr->m7_i4;
	int oldfd= (mptr->m7_i3) >> sizeof(short int);
	int newfd= (mptr->m7_i3) & ((1 <<  sizeof(short int))-1);
	RHSDEBUG(" olen=%d nlen=%d oldfd=%d newfd=%d flags=%X\n",
		olen, nlen, oldfd, newfd, flags);
		
	rcode = dvk_vcopy(mptr->m_source, mptr->m7_p1, rhs_ep, oldname, olen);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("oldname=%s\n", oldname);

	rcode = dvk_vcopy(mptr->m_source, mptr->m7_p2, rhs_ep, newname, nlen);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("newname=%s\n", newname);
	
//	rcode = renameat2(oldfd, oldname, newfd, newname, flags);
	rcode = syscall(SYS_renameat2,oldfd, oldname, newfd, newname, flags);
	if( rcode < 0) ERROR_RETURN(-errno);				 
	return(rcode);
}
	
int lcl_opendir(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1]; 
	
	int len 	= mptr->m1_i1;
	RHSDEBUG("len=%d\n", len);
	
	rcode = dvk_vcopy(mptr->m_source, mptr->m1_p1, rhs_ep, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHSDEBUG("pathname=%s\n", pathname);

	DIR *dirp = opendir(pathname);
	RHSDEBUG("dirp=%p\n", dirp);
	if( dirp == NULL) ERROR_RETURN(-errno);

	mptr->m1_p2 = dirp; // must be returned for future operations on DIR
	
	return(rcode);
}


int lcl_seekdir(message *mptr)
{
	long loc = mptr->m2_l1;
	DIR *dirp= mptr->m2_p1;
	RHSDEBUG("dirp=%p\n", dirp);

	RHSDEBUG("loc=%ld\n", loc);

	seekdir(dirp, loc);
	return(OK);
}

//       If the end of the directory stream is reached, NULL is returned and
//       errno is not changed.  If an error occurs, NULL is returned and errno
//       is set appropriately.  To distinguish end of stream from an error,
//       set errno to zero before calling readdir() and then check the value
//       of errno if NULL is returned.
int lcl_readdir(message *mptr)
{
	message m;
	int rcode;
	static struct dirent *dire_ptr;

	RHSDEBUG("\n");
	DIR *dirp = mptr->m1_p1;
	RHSDEBUG("dirp=%p\n", dirp);
	
	errno = 0;
	dire_ptr = readdir(dirp);
	if( dire_ptr == NULL && errno != 0) ERROR_RETURN(-errno);

	if(  dire_ptr != NULL ){
		RHSDEBUG(DIRE_FORMAT, DIRE_FIELDS(dire_ptr));
		rcode = dvk_vcopy(rhs_ep, dire_ptr, mptr->m_source, 
						mptr->m1_p2, sizeof(struct dirent));
		if( rcode < 0) ERROR_RETURN(rcode);
	}
	mptr->m1_p3 = dire_ptr;
	ERROR_RETURN(0); // End of directory
}

int lcl_closedir(message *mptr)
{
	int rcode;
	RHSDEBUG("\n");
	DIR *dirp= mptr->m1_p1;
	RHSDEBUG("dirp=%p\n", dirp);
	rcode = closedir(dirp);
	if( rcode < 0) ERROR_RETURN(rcode);
	return(rcode);
}



		




























































































































				





















