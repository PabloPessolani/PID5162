/*
 * Copyright (C) 2000 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */
//#ifdef CONFIG_RHOSTFS

#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <sys/syscall.h>
#include <signal.h>

#define DVS_USERSPACE	1

#define CONFIG_RHOSTFS	1 
#include "rhostfs.h"
#include <utime.h>

void print_sigset(void)
{
    int sig, cnt, rcode;
	sigset_t sigset;

	rcode =  sigpending(&sigset);
	if(rcode == 0) {
		ERROR_PRINT(rcode);
		return;
	}
    cnt = 0;
    for (sig = 1; sig < sizeof(sigset)*8; sig++) {
        if (sigismember(&sigset, sig)) {
            cnt++;
            printf("%d:%s\n", sig, strsignal(sig));
        }
    }
    if (cnt == 0)
        printf("<empty signal set>\n");
}

static void rh_ustat_to_hostfs( struct USR_stat *buf, struct hostfs_stat *p)
{
	RHDEBUG("st_ino=%ld\n",buf->ust_ino);	
	p->ino = buf->ust_ino;
	p->mode = buf->ust_mode;
	p->nlink = buf->ust_nlink;
	p->uid = buf->ust_uid;
	p->gid = buf->ust_gid;
	RHDEBUG("st_size=%ld\n",buf->ust_size);	
	p->size = buf->ust_size;
	p->atime.tv_sec = buf->ust_atime;
	p->atime.tv_nsec = 0;
	p->ctime.tv_sec = buf->ust_ctime;
	p->ctime.tv_nsec = 0;
	p->mtime.tv_sec = buf->ust_mtime;
	p->mtime.tv_nsec = 0;
	p->blksize = buf->ust_blksize;
	p->blocks = buf->ust_blocks;
	p->maj = os_major(buf->ust_rdev);
	p->min = os_minor(buf->ust_rdev);
	RHDEBUG("ino=%ld maj=%d min=%d\n",p->ino, p->maj, p->min);	
}


int rh_stat_file(const char *path, struct hostfs_stat *p, int fd)
{
	struct USR_stat statbuf, *usb_ptr;

	usb_ptr = &statbuf;
	
	if (fd >= 0) {
		if (rmt_fstat64(fd, usb_ptr) < 0)
			ERROR_RETURN(-rmt_errno);
	} else if (rmt_lstat64(path, usb_ptr) < 0) {
		ERROR_RETURN(-rmt_errno);
	}
	rh_ustat_to_hostfs(&statbuf, p);
	return 0;
}

int rh_access_file(char *path, int r, int w, int x)
{
	int mode = 0;

	RHDEBUG("path=%s r=%d w=%d x=%d\n",path, r, w, x);	

//	if( strcmp(path,"//") == 0) return(access_file(path, r, w, x));
	if (r)
		mode = R_OK;
	if (w)
		mode |= W_OK;
	if (x)
		mode |= X_OK;
	if (rmt_access(path, mode) != 0) {
		ERROR_RETURN(-rmt_errno);
	}else 
		return 0;
}

int rh_open_file(char *path, int r, int w, int append)
{
	int mode = 0, flags = 0 , fd;

	if (r && !w)
		mode = O_RDONLY;
	else if (!r && w)
		mode = O_WRONLY;
	else if (r && w)
		mode = O_RDWR;
	else panic("Impossible mode in rh_open_file");

	if (append)
		mode |= O_APPEND;
	fd = rmt_open64(path, flags, mode);
	if (fd < 0){
		ERROR_RETURN(-rmt_errno);
	} else 
		return fd;
}

void *rh_open_dir(char *path, int *err_out)
{
	DIR *dir;

	RHDEBUG("path=%s\n",path);	

	dir = rmt_opendir(path);
	*err_out = -rmt_errno;

	return dir;
}

void rh_seek_dir(void *stream, unsigned long long pos)
{
	DIR *dir = stream;
	RHDEBUG("\n");	
	rmt_seekdir(dir, pos);
}

char *rh_read_dir(void *stream, unsigned long long *pos_out,
	       unsigned long long *ino_out, int *len_out,
	       unsigned int *type_out)
{
	DIR *dir = stream;
//	struct dirent *dire_ptr;
	static	struct UML_dire_info UML_di;
	struct UML_dire_info *udi_ptr;
	
	RHDEBUG("\n");	

 	udi_ptr = (struct UML_dire_info *) rmt_readdir(dir);
	if (udi_ptr == NULL) {
		ERROR_PRINT(EDVSNOENT);
		return NULL;
	}
	
	memcpy(&UML_di, udi_ptr, sizeof(struct USR_dirent));
	udi_ptr = &UML_di;
	RHDEBUG(UDIRE_FORMAT, UDIRE_FIELDS(udi_ptr));
	
	*ino_out  = udi_ptr->ino_dire;
	*pos_out  = udi_ptr->pos_dire;
	*type_out = udi_ptr->type_dire;
	*len_out = strlen(udi_ptr->name_dire);	
		
	return udi_ptr->name_dire;
}

int rh_read_file(int fd, unsigned long long *offset, char *buf, int len)
{
	int n;

	RHDEBUG("\n");	

	n = rmt_pread64(fd, buf, len, *offset);
	if (n < 0)
		ERROR_RETURN(-rmt_errno);
	*offset += n;
	return n;
}

int rh_write_file(int fd, unsigned long long *offset, const char *buf, int len)
{
	int n;

	RHDEBUG("\n");	

	n = rmt_pwrite64(fd, buf, len, *offset);
	if (n < 0)
		ERROR_RETURN(-rmt_errno);
	*offset += n;
	return n;
}

int rh_lseek_file(int fd, long long offset, int whence)
{
	int ret;

	RHDEBUG("\n");	

	ret = rmt_lseek64(fd, offset, whence);
	if (ret < 0)
		ERROR_RETURN(-rmt_errno);
	return 0;
}

int rh_fsync_file(int fd, int datasync)
{
	int ret;
	RHDEBUG("\n");	

	if (datasync)
		ret = rmt_fdatasync(fd);
	else
		ret = rmt_fsync(fd);

	if (ret < 0)
		ERROR_RETURN(-rmt_errno);
	return 0;
}

int rh_replace_file(int oldfd, int fd)
{
	RHDEBUG("\n");	

	return rmt_dup2(oldfd, fd);
}

void rh_close_file(void *stream)
{
	RHDEBUG("\n");	

	rmt_close(*((int *) stream));
}

void rh_close_dir(void *stream)
{
	RHDEBUG("\n");	
	rmt_closedir(stream);
}

int rh_file_create(char *name, int mode)
{
	int fd;

	RHDEBUG("name=%s mode=%X\n", name, mode);	

	fd = rmt_open64(name, O_CREAT | O_RDWR, mode);
	if (fd < 0) ERROR_RETURN(-rmt_errno);
	RHDEBUG("returned fd=%d\n", fd);	
	return fd;
}

int rh_set_attr(const char *file, struct hostfs_iattr *attrs, int fd)
{
	struct hostfs_stat st;
	struct timeval times[2];
	int err, ma;

	RHDEBUG("file=%s fd=%d\n", file, fd);	

	if (attrs->ia_valid & HOSTFS_ATTR_MODE) {
		if (fd >= 0) {
			if (rmt_fchmod(fd, attrs->ia_mode) != 0)
				ERROR_RETURN(-rmt_errno);
		} else if (rmt_chmod(file, attrs->ia_mode) != 0) {
			ERROR_RETURN(-rmt_errno);
		}
	}
	if (attrs->ia_valid & HOSTFS_ATTR_UID) {
		if (fd >= 0) {
			if (rmt_fchown(fd, attrs->ia_uid, -1))
				ERROR_RETURN(-rmt_errno);
		} else if (rmt_chown(file, attrs->ia_uid, -1)) {
			ERROR_RETURN(-rmt_errno);
		}
	}
	if (attrs->ia_valid & HOSTFS_ATTR_GID) {
		if (fd >= 0) {
			if (rmt_fchown(fd, -1, attrs->ia_gid))
				ERROR_RETURN(-rmt_errno);
		} else if (rmt_chown(file, -1, attrs->ia_gid)) {
			ERROR_RETURN(-rmt_errno);
		}
	}
	if (attrs->ia_valid & HOSTFS_ATTR_SIZE) {
		if (fd >= 0) {
			if (rmt_ftruncate(fd, attrs->ia_size))
				ERROR_RETURN(-rmt_errno);
		} else if (rmt_truncate(file, attrs->ia_size)) {
			ERROR_RETURN(-rmt_errno);
		}
	}

	/*
	 * Update accessed and/or modified time, in two parts: first set
	 * times according to the changes to perform, and then call futimes()
	 * or utimes() to apply them.
	 */
	ma = (HOSTFS_ATTR_ATIME_SET | HOSTFS_ATTR_MTIME_SET);
	if (attrs->ia_valid & ma) {
		err = rh_stat_file(file, &st, fd);
		if (err != 0)
			return err;

		times[0].tv_sec = st.atime.tv_sec;
		times[0].tv_usec = st.atime.tv_nsec / 1000;
		times[1].tv_sec = st.mtime.tv_sec;
		times[1].tv_usec = st.mtime.tv_nsec / 1000;

		if (attrs->ia_valid & HOSTFS_ATTR_ATIME_SET) {
			times[0].tv_sec = attrs->ia_atime.tv_sec;
			times[0].tv_usec = attrs->ia_atime.tv_nsec / 1000;
		}
		if (attrs->ia_valid & HOSTFS_ATTR_MTIME_SET) {
			times[1].tv_sec = attrs->ia_mtime.tv_sec;
			times[1].tv_usec = attrs->ia_mtime.tv_nsec / 1000;
		}

		if (fd >= 0) {
			if (rmt_futimes(fd, times) != 0)
				ERROR_RETURN(-rmt_errno);
		} else if (rmt_utimes(file, times) != 0) {
			ERROR_RETURN(-rmt_errno);
		}
	}

	/* Note: ctime is not handled */
	if (attrs->ia_valid & (HOSTFS_ATTR_ATIME | HOSTFS_ATTR_MTIME)) {
		err = rh_stat_file(file, &st, fd);
		attrs->ia_atime = st.atime;
		attrs->ia_mtime = st.mtime;
		if (err != 0)
			ERROR_RETURN(err);
	}
	return 0;
}

int rh_make_symlink(const char *from, const char *to)
{
	int err;

	RHDEBUG("from=%s to=%s\n", from, to);	

	err = rmt_symlink(to, from);
	if (err)
		ERROR_RETURN(-rmt_errno);
	return 0;
}

int rh_unlink_file(const char *file)
{
	int err;
	RHDEBUG("file=%s\n", file);	

	err = rmt_unlink(file);
	if (err)
		ERROR_RETURN(-rmt_errno);
	return 0;
}

int rh_do_mkdir(const char *file, int mode)
{
	int err;

	RHDEBUG("file=%s mode=%X\n", file, mode);	

	err = rmt_mkdir(file, mode);
	if (err)
		ERROR_RETURN(-rmt_errno);
	return 0;
}

int rh_do_rmdir(const char *file)
{
	int err;
	RHDEBUG("file=%s\n", file);	

	err = rmt_rmdir(file);
	if (err)
		ERROR_RETURN(-rmt_errno);
	return 0;
}

int rh_do_mknod(const char *file, int mode, unsigned int major, unsigned int minor)
{
	int err;

	RHDEBUG("file=%s mode=%X major=%d minor=%d \n", file, mode, major, minor );	

	err = rmt_mknod(file, mode, os_makedev(major, minor));
	if (err)
		ERROR_RETURN(-rmt_errno);
	return 0;
}

int rh_link_file(const char *to, const char *from)
{
	int err;

	RHDEBUG("from=%s to=%s\n", from, to);	

	err = rmt_link(to, from);
	if (err)
		ERROR_RETURN(-rmt_errno);
	return 0;
}

int rh_do_readlink(char *file, char *buf, int size)
{
	int n;

	n = rmt_readlink(file, buf, size);
	if (n < 0)
		ERROR_RETURN(-rmt_errno);
	if (n < size)
		buf[n] = '\0';
	return n;
}

int rh_rename_file(char *from, char *to)
{
	int err;

	err = rmt_rename(from, to);
	if (err < 0)
		ERROR_RETURN(-rmt_errno);
	return 0;
}

int rh_rename2_file(char *from, char *to, unsigned int flags)
{
	int err;

#ifndef SYS_renameat2
#  ifdef __x86_64__
#    define SYS_renameat2 316
#  endif
#  ifdef __i386__
#    define SYS_renameat2 353
#  endif
#endif

#ifdef SYS_renameat2
	err = rmt_renameat2(AT_FDCWD, from, AT_FDCWD, to, flags);
	if (err < 0) {
		if (errno != ENOSYS){
			ERROR_RETURN(-rmt_errno);
		}else{
			ERROR_RETURN(-EINVAL);
		}
	}
	return 0;
#else
	ERROR_RETURN(-EINVAL);
#endif
}

int rh_do_statfs(char *root, long *bsize_out, long long *blocks_out,
	      long long *bfree_out, long long *bavail_out,
	      long long *files_out, long long *ffree_out,
	      void *fsid_out, int fsid_size, long *namelen_out)
{
	struct statfs64 buf;
	int err;

	err = rmt_statfs64(root, &buf);
	if (err < 0)
		ERROR_RETURN(-rmt_errno);

	*bsize_out = buf.f_bsize;
	*blocks_out = buf.f_blocks;
	*bfree_out = buf.f_bfree;
	*bavail_out = buf.f_bavail;
	*files_out = buf.f_files;
	*ffree_out = buf.f_ffree;
	memcpy(fsid_out, &buf.f_fsid,
	       sizeof(buf.f_fsid) > fsid_size ? fsid_size :
	       sizeof(buf.f_fsid));
	*namelen_out = buf.f_namelen;

	return 0;
}
// #endif // CONFIG_RHOSTFS
