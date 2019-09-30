/* Function prototypes. */
/* Function prototypes. */

int pm_no_sys(void);
int pm_getset(void);
int pm_isokendpt(int ep, int *proc);
int tell_fs(int what, int p1, int p2, int p3);

/* main.c */
int main ( int argc, char *argv[] );
void setreply(int proc_nr, int result);

/* utility.c */
void mproc_init(int p_nr);
int sys_proctab(muk_proc_t *kp, int tab_len);
int sys_privtab(priv_usr_t *kp, int tab_len);
int sys_slotstab(slot_t *slots, int tab_len);
void mproc_init(int p_nr);
molclock_t pm_get_uptime(void);
// void get_dvs_params(void);
//void get_dc_params(int vmid);


/* forkexit.c */
int pm_fork(void);
int pm_pm_exit(void);
int pm_bindproc(void);
int pm_waitpid(void);
int pm_freeproc(void);
int pm_wait4fork(void);
int pm_unbind(void);

/* rexec.c*/
int pm_rexec(void);
int pm_migrproc(void);

/* timers.c */
int pm_set_timer(moltimer_t *tp, int delta, tmr_func_t watchdog, int arg);
int pm_expire_timers(molclock_t now);
int pm_cancel_timer(moltimer_t *tp);

/* misc.c */
int pm_getsysinfo(void);
int pm_getprocnr (void);
int pm_procstat (void);

/* time.c */
int pm_stime(void);
int pm_time(void);
int pm_times(void);

/* signal.c*/
int pm_alarm(void);

/* dump.c*/
int pm_dump(void);

#ifdef TEMPORAL

struct mproc;
struct stat;
struct mem_map;
struct memory;


/* alloc.c */
_PROTOTYPE( phys_clicks alloc_mem, (phys_clicks clicks)			);
_PROTOTYPE( void free_mem, (phys_clicks base, phys_clicks clicks)	);
_PROTOTYPE( void mem_init, (struct memory *chunks, phys_clicks *free)	);
#if ENABLE_SWAP
_PROTOTYPE( int swap_on, (char *file, u32_t offset, u32_t size)	);
_PROTOTYPE( int swap_off, (void)					);
_PROTOTYPE( void swap_in, (void)					);
_PROTOTYPE( void swap_inqueue, (struct mproc *rmp)			);
#else /* !SWAP */
#define swap_in()			((void)0)
#define swap_inqueue(rmp)		((void)0)
#endif /* !SWAP */
_PROTOTYPE(int mem_holes_copy, (struct hole *, size_t *, u32_t *)	);

/* break.c */
_PROTOTYPE( int adjust, (struct mproc *rmp,
			vir_clicks data_clicks, vir_bytes sp)		);
_PROTOTYPE( int pm_brk, (void)						);
_PROTOTYPE( int size_ok, (int file_type, vir_clicks tc, vir_clicks dc,
			vir_clicks sc, vir_clicks dvir, vir_clicks s_vir) );

/* devio.c */
_PROTOTYPE( int pm_dev_io, (void) );
_PROTOTYPE( int pm_dev_io, (void) );

/* dmp.c */
_PROTOTYPE( int pm_fkey_pressed, (void)						);

/* exec.c */
_PROTOTYPE( int pm_exec, (void)						);
_PROTOTYPE( void rw_seg, (int rw, int fd, int proc, int seg,
						phys_bytes seg_bytes)	);
_PROTOTYPE( struct mproc *find_share, (struct mproc *mp_ign, Ino_t ino,
			Dev_t dev, time_t ctime)			);

/* forkexit.c */
_PROTOTYPE( int pm_fork, (void)						);
_PROTOTYPE( int pm_pm_exit, (void)					);
_PROTOTYPE( void pm_exit, (struct mproc *rmp, int exit_status, int oper)	);


/* misc.c */
_PROTOTYPE( int pm_reboot, (void)					);
_PROTOTYPE( int pm_getsysinfo, (void)					);
_PROTOTYPE( int pm_svrctl, (void)					);
_PROTOTYPE( int pm_allocmem, (void)					);
_PROTOTYPE( int pm_freemem, (void)					);
_PROTOTYPE( int pm_getsetpriority, (void)					);
_PROTOTYPE( ssize_t _read_pm, (int _fd, void *_buf, size_t _n, int s, int e));
_PROTOTYPE( ssize_t _write_pm, (int _fd, void *_buf, size_t _n, int s, int e));


#if (MACHINE == MACINTOSH)
_PROTOTYPE( phys_clicks start_click, (void)				);
#endif

_PROTOTYPE( void setreply, (int proc_nr, int result)			);

/* signal.c */
_PROTOTYPE( int pm_kill, (void)						);
_PROTOTYPE( int ksig_pending, (void)					);
_PROTOTYPE( int pm_pause, (void)					);
_PROTOTYPE( int set_alarm, (int proc_nr, int sec)			);
_PROTOTYPE( int check_sig, (pid_t proc_id, int signo)			);
_PROTOTYPE( void sig_proc, (struct mproc *rmp, int sig_nr)		);
_PROTOTYPE( int pm_sigaction, (void)					);
_PROTOTYPE( int pm_sigpending, (void)					);
_PROTOTYPE( int pm_sigprocmask, (void)					);
_PROTOTYPE( int pm_sigreturn, (void)					);
_PROTOTYPE( int pm_sigsuspend, (void)					);
_PROTOTYPE( void check_pending, (struct mproc *rmp)			);

/* time.c */
_PROTOTYPE( int pm_stime, (void)					);
_PROTOTYPE( int pm_time, (void)						);
_PROTOTYPE( int pm_times, (void)					);
_PROTOTYPE( int pm_gettimeofday, (void)					);



/* trace.c */
_PROTOTYPE( int pm_trace, (void)					);
_PROTOTYPE( void stop_proc, (struct mproc *rmp, int sig_nr)		);

/* utility.c */
_PROTOTYPE( pid_t get_free_pid, (void)					);
_PROTOTYPE( int allowed, (char *name_buf, struct stat *s_buf, int mask)	);
_PROTOTYPE( int pm_no_sys, (void)						);
_PROTOTYPE( void panic, (char *who, char *mess, int num)		);
_PROTOTYPE( int get_stack_ptr, (int proc_nr, vir_bytes *sp)		);
_PROTOTYPE( int get_mem_map, (int proc_nr, struct mem_map *mem_map)	);
_PROTOTYPE( char *find_param, (const char *key));
_PROTOTYPE( int proc_from_pid, (pid_t p));

#endif /* TEMPORAL */

