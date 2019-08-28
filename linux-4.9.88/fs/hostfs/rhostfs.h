#ifndef __UM_FS_RHOSTFS
#define __UM_FS_FHOSTFS

#define _GNU_SOURCE

#include "/usr/src/dvs/include/com/dvs_config.h"
#include "/usr/src/dvs/include/com/config.h"
#include "/usr/src/dvs/include/com/const.h"
#include "/usr/src/dvs/include/com/com.h"
#include "/usr/src/dvs/include/com/cmd.h"
#include "/usr/src/dvs/include/com/proc_sts.h"
#include "/usr/src/dvs/include/com/proc_usr.h"
#include "/usr/src/dvs/include/com/proxy_sts.h"
#include "/usr/src/dvs/include/com/proxy_usr.h"
#include "/usr/src/dvs/include/com/dc_usr.h"
#include "/usr/src/dvs/include/com/node_usr.h"
#include "/usr/src/dvs/include/com/priv_usr.h"
#include "/usr/src/dvs/include/com/dvs_usr.h"
#include "/usr/src/dvs/include/com/dvk_calls.h"
#include "/usr/src/dvs/include/com/dvk_ioctl.h"
#include "/usr/src/dvs/include/com/dvs_errno.h"
#include "/usr/src/dvs/include/com/ipc.h"
#include "/usr/src/dvs/include/dvk/dvk_ioparm.h"
#include "/usr/src/dvs/include/generic/tracker.h"
#include "/usr/src/dvs/include/com/stub_dvkcall.h"

#include "/usr/src/linux/arch/sh/include/uapi/asm/unistd_32.h"

#include "hostfs.h"

#include "rh_debug.h"
#include "rh_macros.h"

#ifndef DVK_MAJOR
#define DVK_MAJOR 33   /* dynamic major by default */
#endif
#define UML_DVK_DEV "/dev/dvk"
#define DEVICE_NAME "dvk"

#include "rhostfs_proto.h"
#include "rhostfs_glo.h"
#include "rh_syscall.h"

#define HOSTNAME_MAX	64
#define	TIMEOUT_RMTCALL	TIMEOUT_MOLCALL
#define RHOSTFS_PROC_NR 1

#include <os.h>

/*
 * These are exactly the same definitions as in fs.h, but the names are
 * changed so that this file can be included in both kernel and user files.
 */

#define HOSTFS_ATTR_MODE	1
#define HOSTFS_ATTR_UID 	2
#define HOSTFS_ATTR_GID 	4
#define HOSTFS_ATTR_SIZE	8
#define HOSTFS_ATTR_ATIME	16
#define HOSTFS_ATTR_MTIME	32
#define HOSTFS_ATTR_CTIME	64
#define HOSTFS_ATTR_ATIME_SET	128
#define HOSTFS_ATTR_MTIME_SET	256

/* These two are unused by hostfs. */
#define HOSTFS_ATTR_FORCE	512	/* Not a change, but a change it */
#define HOSTFS_ATTR_ATTR_FLAG	1024


		   

extern int rh_stat_file(const char *path, struct hostfs_stat *p, int fd);
extern int rh_access_file(char *path, int r, int w, int x);
extern int rh_open_file(char *path, int r, int w, int append);
extern void *rh_open_dir(char *path, int *err_out);
extern void  rh_seek_dir(void *stream, unsigned long long pos);
extern char *rh_read_dir(void *stream, unsigned long long *pos_out,
		      unsigned long long *ino_out, int *len_out,
		      unsigned int *type_out);
extern void  rh_close_file(void *stream);
extern int rh_replace_file(int oldfd, int fd);
extern void  rh_close_dir(void *stream);
extern int rh_read_file(int fd, unsigned long long *offset, char *buf, int len);
extern int rh_write_file(int fd, unsigned long long *offset, const char *buf,
		      int len);
extern int rh_lseek_file(int fd, long long offset, int whence);
extern int rh_fsync_file(int fd, int datasync);
extern int rh_file_create(char *name, int mode);
extern int rh_set_attr(const char *file, struct hostfs_iattr *attrs, int fd);
extern int rh_make_symlink(const char *from, const char *to);
extern int rh_unlink_file(const char *file);
extern int rh_do_mkdir(const char *file, int mode);
extern int rh_do_rmdir(const char *file);
extern int rh_do_mknod(const char *file, int mode, unsigned int major,
		    unsigned int minor);
extern int rh_link_file(const char *from, const char *to);
extern int rh_hostfs_do_readlink(char *file, char *buf, int size);
extern int rh_rename_file(char *from, char *to);
extern int rh_rename2_file(char *from, char *to, unsigned int flags);
extern int rh_do_statfs(char *root, long *bsize_out, long long *blocks_out,
		     long long *bfree_out, long long *bavail_out,
		     long long *files_out, long long *ffree_out,
		     void *fsid_out, int fsid_size, long *namelen_out);
extern int rh_get_rootpath(char *to);

		   
#endif
