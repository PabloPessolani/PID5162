
long rmt_gethostname(char  *name, int len);
long rmt_open64(const char  *filename, int flags, mode_t mode);
long rmt_access(const char  *filename, mode_t mode);
long rmt_pread64(unsigned int fd, char  *buf, size_t count, loff_t pos);
long rmt_pwrite64(unsigned int fd, char  *buf, size_t count, loff_t pos);
long rmt_llseek(unsigned int fd, unsigned long offset_high,	unsigned long offset_low, 
				loff_t  *result, unsigned int whence);
long rmt_lseek64(unsigned int fd, loff_t *result, unsigned int whence);			
long rmt_fsync(unsigned int fd);
long rmt_dup2(unsigned int oldfd, unsigned int newfd);
long rmt_fchmod(unsigned int fd, mode_t mode);
long rmt_chmod(const char  *filename, mode_t mode);
long rmt_fchown(unsigned int fd, uid_t user, gid_t group);
long rmt_chown(const char  *filename, uid_t user, gid_t group);				
long rmt_ftruncate(unsigned int fd, unsigned long length);
long rmt_truncate(const char  *filename, long length);
long rmt_fdatasync(unsigned int fd);
long rmt_close(unsigned int fd);
long rmt_symlink(const char  *old, const char  *new);
long rmt_unlink(const char  *pathname);
long rmt_link(const char  *oldname, const char  *newname);	
long rmt_readlink(const char  *path, char  *buf, int bufsiz);				
long rmt_rename(const char  *old, const char  *new);
long rmt_rmdir(const char  *pathname);
long rmt_mkdir(const char  *pathname, mode_t mode);
long rmt_mknod(const char  *filename, mode_t mode,unsigned dev);
long rmt_futimes(int dfd, struct timeval  *utimes);
long rmt_utimes(char  *filename, struct timeval  *utimes);
long rmt_fstat64(unsigned long fd, struct stat64  *statbuf);
long rmt_statfs64(const char  *path, struct statfs64  *buf);
long rmt_lstat64(const char  *filename, struct stat64  *statbuf);
long rmt_renameat2(int olddfd, const char  *oldname,
			      int newdfd, const char  *newname,
			      unsigned int flags);
void *rmt_opendir(const char *name);				  
void rmt_seekdir(void *dirp, long loc);
struct dirent *rmt_readdir(void *dirp);
int rmt_closedir(void *dirp);
long rmt_syscall(int who, int syscallnr, message *mptr);



