/* EXTERN should be extern except in table.c */
#ifdef DVK_GLOBAL_HERE
#define EXTERN
#else
#define EXTERN extern
#endif


EXTERN muk_proc_t *kp;	/* Kernel process table (in userspace)	*/
EXTERN mproc_t *mp;		/* PM process table			*/
EXTERN mproc_t *pm_proc_table;		/* PM process table			*/

/* Global variables. */
EXTERN int procs_in_use;	/* how many processes are marked as IN_USE */

/* The parameters of the call are kept here. */
EXTERN message pm_m_in __attribute__((aligned(0x1000)));		/* the incoming message itself is kept here. */
EXTERN message pm_m_out __attribute__((aligned(0x1000))) ;		/* the outgoing message is kept here. */
EXTERN message *pm_msg_ptr;		/* pointer to message */

EXTERN int pm_who_p, pm_who_e;	/* caller's proc number, endpoint */
EXTERN int pm_call_nr;		/* system call number */

extern _PROTOTYPE (int (*pm_call_vec[]), (void) );	/* system call handlers */
extern char core_name[];	/* file name where core images are produced */
EXTERN sigset_t core_sset;	/* which signals cause core images */
EXTERN sigset_t ign_sset;	/* which signals are by default ignored */

EXTERN muk_proc_t *pm_kproc;	/* Kernel process table (in userspace)	*/
EXTERN priv_usr_t *pm_kpriv;	/* Kernel priviledge table (in userspace)*/

EXTERN int pm_id;		
EXTERN int pm_ep;		
EXTERN int pm_nr;	

EXTERN muk_proc_t *pm_proc_ptr;
EXTERN priv_usr_t *pm_priv_ptr;

EXTERN struct sysinfo pm_info;
EXTERN int pm_next_child;

EXTERN long clockTicks;





	