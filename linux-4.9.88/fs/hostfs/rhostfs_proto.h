
long rmt_gethostname(char __user *name, int len);

long rmt_open(const char __user *filename, int flags, umode_t mode);
long rmt_access(const char __user *filename, umode_t mode);
long rmt_pread64(unsigned int fd, char __user *buf, size_t count, loff_t pos);
long rmt_pwrite64(unsigned int fd, char __user *buf, size_t count, loff_t pos);
long rmt_llseek(unsigned int fd, unsigned long offset_high,	unsigned long offset_low, 
				loff_t __user *result, unsigned int whence);
long rmt_fsync(unsigned int fd);
long rmt_dup2(unsigned int oldfd, unsigned int newfd);
long rmt_fchmod(unsigned int fd, umode_t mode);
long rmt_chmod(const char __user *filename, umode_t mode);
long rmt_fchown(unsigned int fd, uid_t user, gid_t group);
long rmt_chown(const char __user *filename, uid_t user, gid_t group);				
long rmt_ftruncate(unsigned int fd, unsigned long length);
long rmt_truncate(const char __user *filename, long length);
long rmt_fdatasync(unsigned int fd);
long rmt_close(unsigned int fd);
long rmt_symlink(const char __user *old, const char __user *new);
long rmt_unlink(const char __user *pathname);
long rmt_link(const char __user *oldname, const char __user *newname);	
long rmt_readlink(const char __user *path, char __user *buf, int bufsiz);				
long rmt_rename(const char __user *old, const char __user *new);
long rmt_rmdir(const char __user *pathname);
long rmt_mkdir(const char __user *pathname, umode_t mode);
long rmt_mknod(const char __user *filename, umode_t mode,unsigned dev);
long rmt_futimes(int dfd, const char __user *filename, struct timeval __user *utimes);
long rmt_utimes(char __user *filename, struct timeval __user *utimes);
long rmt_fstat64(unsigned long fd, struct stat64 __user *statbuf);
long rmt_statfs64(const char __user *path, struct statfs64 __user *buf);
long rmt_lstat64(const char __user *filename, struct stat64 __user *statbuf);
long rmt_renameat2(int olddfd, const char __user *oldname,
			      int newdfd, const char __user *newname,
			      unsigned int flags);
void *rmt_opendir(const char *name);				  
void rmt_seekdir(void *dirp, long loc);
struct dirent *rmt_readdir(void *dirp);
int rmt_closedir(void *dirp);


