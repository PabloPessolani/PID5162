
#ifdef CONFIG_RHOSTFS

#include <linux/fs.h>
#include <linux/magic.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/statfs.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/dirent.h>
#include <init.h>
#include <kern.h>
#include "rhostfs.h"

long rmt_syscall(int who, int syscallnr, message *mptr)
{
	int rcode;

	mptr->m_type = syscallnr;
	rcode = dvk_sendrec_T(who, mptr, TIMEOUT_RMTCALL);
	if (rcode < 0) ERROR_RETURN(rcode);
	if (mptr->m_type < 0) {
		rmt_errno = -mptr->m_type;
		ERROR_PRINT(rmt_errno);
		return (-1);
	}
	return (mptr->m_type);
}

long rmt_open(const char __user *filename, int flags, umode_t mode)
{
	message m;

	RHDEBUG("filename=%s flags=%X mode=%X\n", filename, flags, mode);
	m.m1_i1 = strlen(filename) + 1;
	m.m1_i2 = flags;
	m.m1_i3 = mode;
	m.m1_p1 = (char *) filename;
	return (rmt_syscall(rhs_ep, __NR_open, &m));	
}

long rmt_access(const char __user *filename, umode_t mode)
{
	message m;

	RHDEBUG("filename=%s mode=%X\n" , filename, mode);
	m.m1_i1 = strlen(filename) + 1;
	m.m1_i3 = mode;
	m.m1_p1 = (char *) filename;
	return (rmt_syscall(rhs_ep, __NR_access, &m));	
}

long rmt_pread64(unsigned int fd, char __user *buf, size_t count, loff_t pos)
{
	message m;
	long long *ptr;

	RHDEBUG("fd=%d count=%ld pos=%lld\n" , fd, count, pos);
	m.m2_i1 = fd;
	m.m2_i2 = count;
	m.m2_p1 = buf;
		
	ptr = (long long*) &m.m2_l1;  // using m2_l2 Too !!!!
	*ptr = pos;
		
	return (rmt_syscall(rhs_ep, __NR_pread64, &m));	
}

long rmt_pwrite64(unsigned int fd, char __user *buf, size_t count, loff_t pos)
{
	message m;
	long long *ptr;

	RHDEBUG("fd=%d count=%ld pos=%lld\n" , fd, count, pos);
	m.m2_i1 = fd;
	m.m2_i2 = count;
	m.m2_p1 = buf;
		
	ptr = (long long*) &m.m2_l1;  // using m2_l2 Too !!!!
	*ptr = pos;;
	return (rmt_syscall(rhs_ep, __NR_pwrite64, &m));	
}

long rmt_llseek(unsigned int fd, unsigned long offset_high,	unsigned long offset_low, 
				loff_t __user *result, unsigned int whence)
{
	message m;

	RHDEBUG("fd=%d high=%ld low=%ld whence=%d\n" , fd, offset_high, offset_low, whence);
	m.m2_i1 = fd;
	m.m2_i2 = whence;
	m.m2_l1 = offset_high;
	m.m2_l2 = offset_low;
	m.m2_p1 = (char *) result;
	return (rmt_syscall(rhs_ep, __NR_lseek, &m));	
}

long rmt_fsync(unsigned int fd)
{
	message m;

	RHDEBUG("fd=%d\n" , fd);
	m.m1_i1 = fd;
	return (rmt_syscall(rhs_ep, __NR_fsync, &m));	
}

long rmt_dup2(unsigned int oldfd, unsigned int newfd)
{
	message m;

	RHDEBUG("oldfd=%d newfd=%d\n" , oldfd, newfd);
	m.m1_i1 = oldfd;
	m.m1_i2 = newfd;
	return (rmt_syscall(rhs_ep, __NR_dup2, &m));	
}

long rmt_fchmod(unsigned int fd, umode_t mode)
{
	message m;

	RHDEBUG("fd=%d mode=%X\n" , fd, mode);
	m.m1_i1 = fd;
	m.m1_i2 = mode;
	return (rmt_syscall(rhs_ep, __NR_fchmod, &m));	
}

long rmt_chmod(const char __user *filename, umode_t mode)
{
	message m;

	RHDEBUG("filename=%s mode=%X\n" , filename, mode);
	m.m1_i1 = strlen(filename) + 1;
	m.m1_i2 = mode;
	m.m1_p1 = (char *) filename;
	return (rmt_syscall(rhs_ep, __NR_chmod, &m));
}

long rmt_fchown(unsigned int fd, uid_t user, gid_t group)
{
	message m;

	RHDEBUG("fd=%d user=%d group=%d\n" , fd, user, group);
	m.m1_i1 = fd;
	m.m1_i2 = user;
	m.m1_i3 = group;
	return (rmt_syscall(rhs_ep, __NR_fchown, &m));
}

long rmt_chown(const char __user *filename, uid_t user, gid_t group)
{
	message m;

	RHDEBUG("filename=%s user=%d group=%d\n", filename, user, group);
	m.m1_i1 = strlen(filename) + 1;
	m.m1_i2 = user;
	m.m1_i3 = group;
	m.m1_p1 = (char *) filename;
	return (rmt_syscall(rhs_ep, __NR_chown, &m));
}

long rmt_ftruncate(unsigned int fd, unsigned long length)
{
	message m;

	RHDEBUG("fd=%d length=%uld\n" , fd, length);
	m.m2_i1 = fd;
	m.m2_l1 = length;
	return (rmt_syscall(rhs_ep, __NR_ftruncate, &m));
}

long rmt_truncate(const char __user *filename, long length)
{
	message m;

	RHDEBUG("filename=%s  length=%uld\n", filename, length);
	m.m2_i1 = strlen(filename) + 1;
	m.m2_l1 = length;
	m.m2_p1 = (char *) filename;
	return (rmt_syscall(rhs_ep, __NR_truncate, &m));
}

long rmt_fdatasync(unsigned int fd)
{
	message m;

	RHDEBUG("fd=%d\n", fd);
	m.m1_i1 = fd;
	return (rmt_syscall(rhs_ep, __NR_fdatasync, &m));
}

long rmt_close(unsigned int fd)
{
	message m;

	RHDEBUG("fd=%d\n", fd);
	m.m1_i1 = fd;
	return (rmt_syscall(rhs_ep, __NR_close, &m));
}

long rmt_symlink(const char __user *old, const char __user *new)
{
	message m;

	RHDEBUG("old=%s new=%s\n", old, new);
	m.m1_i1 = strlen(old) + 1;
	m.m1_i2 = strlen(new) + 1;
	m.m1_p1 = (char *) old;
	m.m1_p2 = (char *) new;
	return (rmt_syscall(rhs_ep, __NR_symlink, &m));
}

long rmt_unlink(const char __user *filename)
{
	message m;

	RHDEBUG("filename=%s\n", filename);
	m.m1_i1 = strlen(filename) + 1;
	m.m1_p1 = (char *) filename;
	return (rmt_syscall(rhs_ep, __NR_unlink, &m));
}

long rmt_link(const char __user *old, const char __user *new)
{
	message m;

	RHDEBUG("old=%s new=%s\n", old, new);
	m.m1_i1 = strlen(old) + 1;
	m.m1_i2 = strlen(new) + 1;
	m.m1_p1 = (char *) old;
	m.m1_p2 = (char *) new;
	return (rmt_syscall(rhs_ep, __NR_link, &m));
}

long rmt_readlink(const char __user *filename, char __user *buf, int bufsiz)
{
	message m;

	RHDEBUG("filename=%s bufsiz=%d\n", filename, bufsiz);
	m.m1_i1 = strlen(filename) + 1;
	m.m1_i2 = bufsiz;
	m.m1_p1 = (char *) filename;
	return (rmt_syscall(rhs_ep, __NR_readlink, &m));
}

long rmt_rename(const char __user *old, const char __user *new)
{
	message m;

	RHDEBUG("old=%s new=%s\n", old, new);
	m.m1_i1 = strlen(old) + 1;
	m.m1_i2 = strlen(new) + 1;
	m.m1_p1 = (char *) old;
	m.m1_p2 = (char *) new;
	return (rmt_syscall(rhs_ep, __NR_rename, &m));
}

long rmt_rmdir(const char __user *filename)
{
	message m;

	RHDEBUG("filename=%s\n", filename);
	m.m1_i1 = strlen(filename) + 1;
	m.m1_p1 = (char *) filename;
	return (rmt_syscall(rhs_ep, __NR_rmdir, &m));
}

long rmt_mkdir(const char __user *filename, umode_t mode)
{
	message m;

	RHDEBUG("filename=%s mode=%X\n" , filename, mode);
	m.m1_i1 = strlen(filename) + 1;
	m.m1_i2 = mode;
	m.m1_p1 = (char *) filename;
	return (rmt_syscall(rhs_ep, __NR_mkdir, &m));
}

long rmt_mknod(const char __user *filename, umode_t mode,unsigned dev)
{
	message m;

	RHDEBUG("filename=%s mode=%X dev=%d\n" , filename, mode, dev);
	m.m1_i1 = strlen(filename) + 1;
	m.m1_i2 = mode;
	m.m1_i3 = dev;
	m.m1_p1 = (char *) filename;
	return (rmt_syscall(rhs_ep, __NR_mknod, &m));
}

long rmt_futimes(int fd, const char __user *filename, struct timeval __user *utimes)
{
	message m;

	RHDEBUG("filename=%s fd=%d\n" , filename, fd);
	m.m1_i1 = strlen(filename) + 1;
	m.m1_i2 = fd;
	m.m1_p1 = (char *) filename;
	m.m1_p2 = (char *) utimes;
	return (rmt_syscall(rhs_ep, __NR_futimesat, &m));
}

long rmt_utimes(char __user *filename, struct timeval __user *utimes)
{
	message m;

	RHDEBUG("filename=%s\n" , filename);
	m.m1_i1 = strlen(filename) + 1;
	m.m1_p1 = (char *) filename;
	m.m1_p2 = (char *) utimes;
	return (rmt_syscall(rhs_ep, __NR_utimes, &m));
}

long rmt_fstat64(unsigned long fd, struct stat64 __user *statbuf)
{
	message m;

	RHDEBUG("fd=%d\n", fd);
	m.m1_i1 = fd;
	m.m1_p1 = (char *) statbuf;
	return (rmt_syscall(rhs_ep, __NR_fstat64, &m));
}

long rmt_statfs64(const char __user *filename, struct statfs64 __user *buf)
{
	message m;

	RHDEBUG("filename=%s\n" , filename);
	m.m1_i1 = strlen(filename) + 1;
	m.m1_p1 = (char *) filename;
	m.m1_p2 = (char *) buf;
	return (rmt_syscall(rhs_ep, __NR_statfs64, &m));
}

long rmt_lstat64(const char __user *filename, struct stat64 __user *statbuf)
{
	message m;

	RHDEBUG("filename=%s\n" , filename);
	m.m1_i1 = strlen(filename) + 1;
	m.m1_p1 = (char *) filename;
	m.m1_p2 = (char *) statbuf;
	return (rmt_syscall(rhs_ep, __NR_lstat64, &m));
}

// typedef struct {int m7i1, m7i2, m7i3, m7i4; char *m7p1, *m7p2;} mess_7;
long rmt_renameat2(int olddfd, const char __user *oldname,
			      int newdfd, const char __user *newname,
			      unsigned int flags)
{
	message m;
	int oldfd, newfd;
	
	RHDEBUG("olddfd=%d oldname=%s newdfd=%d newname=%s flags=%X\n",
		olddfd, oldname,
		newdfd, newname,
		flags);
		
	m.m7_i1 = strlen(oldname) + 1;
	m.m7_p1 = (char *) oldname;
	m.m7_i2 = strlen(newname) + 1;
	m.m7_p2 = (char *) newname;
	// two parameters in the same message field 
	olddfd  = olddfd << sizeof(short int);
	m.m7_i3 = oldfd | newfd; 
	
	m.m7_i4 = flags;

	return (rmt_syscall(rhs_ep, __NR_renameat2, &m));
}
		
void *rmt_opendir(const char *name)
{
	message m;
	int rcode;

	RHDEBUG("name=%s\n", name);
	m.m1_i1 = strlen(name) + 1;
	m.m1_p1 = (char *) name;
	rcode = rmt_syscall(rhs_ep, RMT_opendir, &m);
	if (rcode < 0) return(NULL);
	return (&m.m1_p2);
}

void rmt_seekdir(void *dirp, long loc)
{
	message m;
	int rcode;

	RHDEBUG("loc=%ld\n", loc);
	m.m2_l1 = loc;
	m.m2_p1 = (char *) dirp;
	rcode = rmt_syscall(rhs_ep, RMT_seekdir, &m);
	if (rcode < 0) ERROR_PRINT(rcode);
	return;
}


struct dirent *rmt_readdir(void *dirp)
{
	message m;
	int rcode;
	static struct UML_dirent dire;

	RHDEBUG("\n");
	m.m1_p1 = (char *) dirp;
	m.m1_p2 = (char *) &dire;
	rcode = rmt_syscall(rhs_ep, RMT_readdir, &m);
	if (rcode < 0) return(NULL);
	return ((struct dirent *)&dire);
}

int rmt_closedir(void *dirp)
{
	message m;
	int rcode;

	RHDEBUG("\n");
	m.m1_p1 = (char *) dirp;
	return(rmt_syscall(rhs_ep, RMT_closedir, &m));
}

#endif // CONFIG_RHOSTFS


		




























































































































				




















