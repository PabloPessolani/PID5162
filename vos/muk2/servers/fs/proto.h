/* Function prototypes. */

struct buf *get_block(mnx_dev_t dev, mnx_block_t block,int only_search);


/* link.c */
int fs_link(void);
int fs_unlink(void);
int fs_rename(void);
int fs_truncate(void);
int fs_ftruncate(void);
int truncate_inode(struct inode *rip, mnx_off_t newsize);
int freesp_inode(struct inode *rip, mnx_off_t start, mnx_off_t end);
// mnx_off_t nextblock(mnx_off_t pos, int zone_size);
// void zeroblock_half(struct inode *rip, mnx_off_t pos, int half);
// void zeroblock_range(struct inode *rip, mnx_off_t pos, mnx_off_t len);
// 

/* lock.c */
int lock_op(struct filp *f, int req);
void lock_revive(void);

/* inode.c */
struct inode *get_inode(mnx_dev_t dev, int numb);
void rw_inode(struct inode *rip, int rw_flag);
void update_times(struct inode *rip);
void old_icopy(struct inode *rip, d1_inode *dip, int direction, int norm);
void new_icopy(struct inode *rip, d2_inode *dip, int direction, int norm);
void free_inode(mnx_dev_t dev, mnx_ino_t inumb);
void put_inode(struct inode *rip);
void wipe_inode(struct inode *rip);
void dup_inode(struct inode *ip);
struct inode *alloc_inode(mnx_dev_t dev, mnx_mode_t bits);

/* main.c */
int main ( int argc, char *argv[] );
void fs_reply(int whom, int result);

/* open.c*/
int fs_creat(void);
int fs_open(void);
int fs_close(void);
int fs_lseek(void);
struct inode *new_node(struct inode **ldirp, char *path, mnx_mode_t bits, mnx_zone_t z0, int opaque, char *parsed);
int fs_mknod(void);
int fs_mkdir(void);
int fs_slink(void);

/* read.c*/
int fs_read(void);
int read_write(int rw_flag);
mnx_block_t read_map(struct inode *rip, mnx_off_t position);
mnx_zone_t rd_indir(struct buf *bp, int index);
int rw_chunk(register struct inode *rip, mnx_off_t position, unsigned off, int chunk, unsigned left, int rw_flag, char *buff,
int seg, int usr, int block_size, int *completed);
void read_ahead();
struct buf *rahead(register struct inode *rip, mnx_block_t baseblock, mnx_off_t position, unsigned bytes_ahead);

/*write.c*/
int fs_write(void);
int empty_indir(struct buf *bp, struct super_block *sb);
void wr_indir(struct buf *bp, int index, mnx_zone_t zone);
void zero_block(struct buf *bp);
int write_map(struct inode *rip, mnx_off_t position, mnx_zone_t new_zone, int op);
struct buf *new_block(struct inode *rip, mnx_off_t position);

/* utility.c */
int fs_no_sys(void);
void fproc_init(int p_nr);
int load_image(char *img_name);
int fetch_name(char *path, int len, int flag);
unsigned conv2(int norm, int w);
long conv4(int norm, long x);
mnx_time_t clock_time();
void panic(char *who, char *mess, int num);
int isokendpt_f(char *file, int line, int endpoint, int *proc, int fatal);

/* super.c */
int read_super(struct super_block *sp);
struct super_block *get_super(mnx_dev_t dev);
int get_block_size(mnx_dev_t dev);
void free_bit(struct super_block *sp, int map, mnx_bit_t bit_returned);
mnx_bit_t alloc_bit(struct super_block *sp, int map, mnx_bit_t origin);


/* time.c */
int fs_utime(void);
int fs_stime(void);

/* misc.c */
int fs_getsysinfo(void);
int fs_dup(void);
int fs_fcntl(void);
int fs_sync(void);
int fs_fsync(void);
int fs_set(void);
int fs_revive(void);
int fs_svrctl(void);

/* cache.c */
mnx_zone_t alloc_zone(mnx_dev_t dev, mnx_zone_t z);
void free_zone(mnx_dev_t dev, mnx_zone_t numb);
void flushall(mnx_dev_t dev);
struct buf *get_block(mnx_dev_t dev, mnx_block_t block, int only_search);
void rm_lru(struct buf *bp);
void put_block(struct buf *bp, int block_type);
int rw_block(struct buf *bp, int rw_flag);

/* dmap.c */
int fs_devctl();
int map_driver(int major, int proc_nr_e, int style);
void dmap_unmap_by_endpt(int proc_nr_e);
void build_dmap();
int dmap_driver_match(int proc, int major);
void dmap_endpt_up(int proc_e);

/* device.c */
int dev_open(mnx_dev_t dev, int proc, int flags);
void dev_close(mnx_dev_t dev);
int dev_io(int op, mnx_dev_t dev, int proc_e, void *buf, mnx_off_t pos, int bytes, int flags);
int r_gen_opcl(int op, mnx_dev_t dev, int proc_e, int flags);
int f_gen_opcl(int op, mnx_dev_t dev, int proc_e, int flags);
int v_gen_opcl(int op, mnx_dev_t dev, int proc_e, int flags);
int n_gen_opcl(int op, mnx_dev_t dev, int proc_e, int flags);
int d_gen_opcl(int op, mnx_dev_t dev, int proc_e, int flags);
int fs_setsid();
int fs_ioctl();
int r_gen_io(int task_nr, message *mess_ptr);
int f_gen_io(int task_nr, message *mess_ptr);
int v_gen_io(int task_nr, message *mess_ptr);
int n_gen_io(int task_nr, message *mess_ptr);
int d_gen_io(int task_nr, message *mess_ptr);
int no_dev(int op, mnx_dev_t dev, int proc, int flags);
int no_dev_io(int proc, message *m);
void dev_up(int maj);

/* dump .c*/
int fs_dump(void);

/*misc.c*/
int fs_fork(void);


/*mount.c*/
int fs_mount(void);
int fs_umount(void);
int unmount(mnx_dev_t dev);
mnx_dev_t name_to_dev(char *path);

/*path.c*/
//struct inode *parse_path(char *path, char string[NAME_MAX], int action);
//struct inode *eat_path(path);

/*protect.c*/
int fs_chown(void);
int fs_chmod(void);
int fs_umask(void);
int fs_access(void);
int read_only(struct inode *ip);
int forbidden(struct inode *rip, mnx_mode_t access_desired);

/*filedesc.c*/
int get_fd(int start, mnx_mode_t bits, int *k, struct filp **fpt);
struct filp *get_filp(int fild);
struct filp *find_filp(register struct inode *rip, mnx_mode_t bits);
int inval_filp(struct filp *fs_proc);

/*pipe.c*/
void release(struct inode *ip, int fs_call_nr, int count);
void revive(int proc_nr_e, int returned);
void suspend(int task);
//int pipe_check(register struct inode *rip, int rw_flag, int oflags, register int bytes, register mnx_off_t position, int *canwrite, int notouch);

/* select.c */
// int select_callback(struct filp *, int ops);

/*stadir.c*/
int fs_fchdir(void);
int fs_chdir(void);
int fs_chroot(void);
int fs_stat(void);
int fs_fstat(void);
int fs_fstatfs(void);
int fs_lstat(void);
int fs_rdlink(void);

/* Structs used in prototypes must be declared as such first. */
struct buf;
struct filp;		
struct inode;
struct super_block;

#define okendpt(e, p) isokendpt_f(__FILE__, __LINE__, (e), (p), 1)
#define isokendpt(e, p) isokendpt_f(__FILE__, __LINE__, (e), (p), 0)

#ifdef ANULADO

#include "timers.h"


/* cache.c */
_PROTOTYPE( zone_t alloc_zone, (Dev_t dev, zone_t z)			);
_PROTOTYPE( void flushall, (Dev_t dev)					);
_PROTOTYPE( void free_zone, (Dev_t dev, zone_t numb)			);
_PROTOTYPE( struct buf *get_block, (Dev_t dev, block_t block,int only_search));
_PROTOTYPE( void invalidate, (Dev_t device)				);
_PROTOTYPE( void put_block, (struct buf *bp, int block_type)		);
_PROTOTYPE( void rw_scattered, (Dev_t dev,
			struct buf **bufq, int bufqsize, int rw_flag)	);

#if ENABLE_CACHE2
/* cache2.c */
_PROTOTYPE( void init_cache2, (unsigned long size)			);
_PROTOTYPE( int get_block2, (struct buf *bp, int only_search)		);
_PROTOTYPE( void put_block2, (struct buf *bp)				);
_PROTOTYPE( void invalidate2, (Dev_t device)				);
#endif

/* device.c */
_PROTOTYPE( int dev_open, (Dev_t dev, int proc, int flags)		);
_PROTOTYPE( void dev_close, (Dev_t dev)					);
_PROTOTYPE( int dev_io, (int op, Dev_t dev, int proc, void *buf,
			off_t pos, int bytes, int flags)		);
_PROTOTYPE( int gen_opcl, (int op, Dev_t dev, int proc, int flags)	);

_PROTOTYPE( int gen_io, (int task_nr, message *mess_ptr)		);
_PROTOTYPE( int no_dev, (int op, Dev_t dev, int proc, int flags)	);
_PROTOTYPE( int no_dev_io, (int, message *)				);
_PROTOTYPE( int tty_opcl, (int op, Dev_t dev, int proc, int flags)	);
_PROTOTYPE( int ctty_opcl, (int op, Dev_t dev, int proc, int flags)	);
_PROTOTYPE( int clone_opcl, (int op, Dev_t dev, int proc, int flags)	);
_PROTOTYPE( int ctty_io, (int task_nr, message *mess_ptr)		);
_PROTOTYPE( int fs_ioctl, (void)					);
_PROTOTYPE( int fs_setsid, (void)					);
_PROTOTYPE( void dev_status, (message *)				);
_PROTOTYPE( void dev_up, (int major)					);

/* dmp.c */
_PROTOTYPE( int fs_fkey_pressed, (void)					);

/* dmap.c */
_PROTOTYPE( int fs_devctl, (void)					);
_PROTOTYPE( void build_dmap, (void)					);
_PROTOTYPE( int map_driver, (int major, int proc_nr, int dev_style)	);
_PROTOTYPE( int dmap_driver_match, (int proc, int major)		);
_PROTOTYPE( void dmap_unmap_by_endpt, (int proc_nr)			);
_PROTOTYPE( void dmap_endpt_up, (int proc_nr)				);

/* filedes.c */
_PROTOTYPE( struct filp *find_filp, (struct inode *rip, mode_t bits)	);
_PROTOTYPE( int get_fd, (int start, mode_t bits, int *k, struct filp **fpt) );
_PROTOTYPE( struct filp *get_filp, (int fild)				);
_PROTOTYPE( int inval_filp, (struct filp *)				);

/* inode.c */
_PROTOTYPE( struct inode *alloc_inode, (dev_t dev, mode_t bits)		);
_PROTOTYPE( void dup_inode, (struct inode *ip)				);
_PROTOTYPE( void free_inode, (Dev_t dev, Ino_t numb)			);
_PROTOTYPE( struct inode *get_inode, (Dev_t dev, int numb)		);
_PROTOTYPE( void put_inode, (struct inode *rip)				);
_PROTOTYPE( void update_times, (struct inode *rip)			);
_PROTOTYPE( void rw_inode, (struct inode *rip, int rw_flag)		);
_PROTOTYPE( void wipe_inode, (struct inode *rip)			);

/* link.c */
_PROTOTYPE( int fs_link, (void)						);
_PROTOTYPE( int fs_unlink, (void)					);
_PROTOTYPE( int fs_rename, (void)					);
_PROTOTYPE( int fs_truncate, (void)					);
_PROTOTYPE( int fs_ftruncate, (void)					);
_PROTOTYPE( int truncate_inode, (struct inode *rip, off_t len)		);
_PROTOTYPE( int freesp_inode, (struct inode *rip, off_t st, off_t end)	);

/* lock.c */
_PROTOTYPE( int lock_op, (struct filp *f, int req)			);
_PROTOTYPE( void lock_revive, (void)					);



/* misc.c */
_PROTOTYPE( int fs_dup, (void)						);
_PROTOTYPE( int fs_exit, (void)						);
_PROTOTYPE( int fs_fcntl, (void)					);
_PROTOTYPE( int fs_fork, (void)						);
_PROTOTYPE( int fs_exec, (void)						);
_PROTOTYPE( int fs_revive, (void)					);
_PROTOTYPE( int fs_set, (void)						);
_PROTOTYPE( int fs_sync, (void)						);
_PROTOTYPE( int fs_fsync, (void)					);
_PROTOTYPE( int fs_reboot, (void)					);
_PROTOTYPE( int fs_svrctl, (void)					);
_PROTOTYPE( int fs_getsysinfo, (void)					);

/* mount.c */
_PROTOTYPE( int fs_mount, (void)					);
_PROTOTYPE( int fs_umount, (void)					);
_PROTOTYPE( int unmount, (Dev_t dev)					);

/* open.c */
_PROTOTYPE( int fs_close, (void)					);
_PROTOTYPE( int fs_creat, (void)					);
_PROTOTYPE( int fs_lseek, (void)					);
_PROTOTYPE( int fs_mknod, (void)					);
_PROTOTYPE( int fs_mkdir, (void)					);
_PROTOTYPE( int fs_open, (void)						);
_PROTOTYPE( int fs_slink, (void)                                       );

/* path.c */
_PROTOTYPE( struct inode *advance,(struct inode **dirp, char string[NAME_MAX]));
_PROTOTYPE( int search_dir, (struct inode *ldir_ptr,
			char string [NAME_MAX], ino_t *numb, int flag)	);
_PROTOTYPE( struct inode *eat_path, (char *path)			);
_PROTOTYPE( struct inode *last_dir, (char *path, char string [NAME_MAX]));
_PROTOTYPE( struct inode *parse_path, (char *path, char string[NAME_MAX], 
                                                       int action)     );

/* pipe.c */
_PROTOTYPE( int fs_pipe, (void)						);
_PROTOTYPE( int fs_unpause, (void)					);
_PROTOTYPE( int pipe_check, (struct inode *rip, int rw_flag,
			int oflags, int bytes, off_t position, int *canwrite, int notouch));
_PROTOTYPE( void release, (struct inode *ip, int fs_call_nr, int count)	);
_PROTOTYPE( void revive, (int proc_nr, int bytes)			);
_PROTOTYPE( void suspend, (int task)					);
_PROTOTYPE( int select_request_pipe, (struct filp *f, int *ops, int bl)	);
_PROTOTYPE( int select_cancel_pipe, (struct filp *f)			);
_PROTOTYPE( int select_match_pipe, (struct filp *f)			);
_PROTOTYPE( void unsuspend_by_endpt, (int)				);

/* protect.c */
_PROTOTYPE( int fs_access, (void)					);
_PROTOTYPE( int fs_chmod, (void)					);
_PROTOTYPE( int fs_chown, (void)					);
_PROTOTYPE( int fs_umask, (void)					);
_PROTOTYPE( int forbidden, (struct inode *rip, mode_t access_desired)	);
_PROTOTYPE( int read_only, (struct inode *ip)				);

/* read.c */
_PROTOTYPE( int fs_read, (void)						);
_PROTOTYPE( struct buf *rahead, (struct inode *rip, block_t baseblock,
			off_t position, unsigned bytes_ahead)		);
_PROTOTYPE( void read_ahead, (void)					);
_PROTOTYPE( block_t read_map, (struct inode *rip, off_t pos)		);
_PROTOTYPE( int read_write, (int rw_flag)				);
_PROTOTYPE( zone_t rd_indir, (struct buf *bp, int index)		);

/* stadir.c */
_PROTOTYPE( int fs_chdir, (void)					);
_PROTOTYPE( int fs_fchdir, (void)					);
_PROTOTYPE( int fs_chroot, (void)					);
_PROTOTYPE( int fs_fstat, (void)					);
_PROTOTYPE( int fs_stat, (void)						);
_PROTOTYPE( int fs_fstatfs, (void)					);
_PROTOTYPE( int fs_rdlink, (void)                                      );
_PROTOTYPE( int fs_lstat, (void)                                       );

/* super.c */
_PROTOTYPE( bit_t alloc_bit, (struct super_block *sp, int map, bit_t origin));
_PROTOTYPE( void free_bit, (struct super_block *sp, int map,
						bit_t bit_returned)	);
_PROTOTYPE( struct super_block *get_super, (Dev_t dev)			);
_PROTOTYPE( int mounted, (struct inode *rip)				);
_PROTOTYPE( int read_super, (struct super_block *sp)			);
_PROTOTYPE( int get_block_size, (dev_t dev)				);

/* time.c */
_PROTOTYPE( int fs_stime, (void)					);
_PROTOTYPE( int fs_utime, (void)					);

/* utility.c */
_PROTOTYPE( time_t clock_time, (void)					);
_PROTOTYPE( unsigned conv2, (int norm, int w)				);
_PROTOTYPE( long conv4, (int norm, long x)				);
_PROTOTYPE( int fetch_name, (char *path, int len, int flag)		);
_PROTOTYPE( int isokendpt_f, (char *f, int l, int e, int *p, int ft));
_PROTOTYPE( void panic, (char *who, char *mess, int num)		);


/* write.c */
_PROTOTYPE( void clear_zone, (struct inode *rip, off_t pos, int flag)	);
_PROTOTYPE( int fs_write, (void)					);
_PROTOTYPE( struct buf *new_block, (struct inode *rip, off_t position)	);
_PROTOTYPE( void zero_block, (struct buf *bp)				);
_PROTOTYPE( int write_map, (struct inode *, off_t, zone_t, int)		);

/* select.c */
_PROTOTYPE( int fs_select, (void)					);
_PROTOTYPE( int select_callback, (struct filp *, int ops)		);
_PROTOTYPE( void select_forget, (int fs_proc_table)				);
_PROTOTYPE( void select_timeout_check, (timer_t *)			);
_PROTOTYPE( void init_select, (void)					);
_PROTOTYPE( void select_unsuspend_by_endpt, (int proc)			);
_PROTOTYPE( int select_notified, (int major, int minor, int ops)	);

/* timers.c */
_PROTOTYPE( void fs_set_timer, (timer_t *tp, int delta, tmr_func_t watchdog, int arg));
_PROTOTYPE( void fs_expire_timers, (clock_t now)			);
_PROTOTYPE( void fs_cancel_timer, (timer_t *tp)				);
_PROTOTYPE( void fs_init_timer, (timer_t *tp)				);

/* cdprobe.c */
_PROTOTYPE( int cdprobe, (void)						);

#endif /* ANULADO */