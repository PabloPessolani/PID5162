/* Copyright (c) 2005 Russ Cox, MIT; see COPYRIGHT */

#ifndef _LIBTASK_H_
#define _LIBTASK_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sched.h>
#include <signal.h>
#include <sys/syscall.h> 
#include <pthread.h>
#include <sys/queue.h>

#if USE_UCONTEXT
#include <ucontext.h>
#endif
#include <sys/utsname.h>
#include <inttypes.h>

#include <inttypes.h>

#define DVS_USERSPACE	1
#define _GNU_SOURCE
#include <sched.h>
#define cpumask_t cpu_set_t

#include "/usr/src/dvs/include/com/dvs_config.h"
#include "/usr/src/dvs/include/com/config.h"
#include "/usr/src/dvs/include/com/const.h"
#include "/usr/src/dvs/include/com/types.h"
#include "/usr/src/dvs/include/com/timers.h"
#include "/usr/src/dvs/include/com/dvs_usr.h"
#include "/usr/src/dvs/include/com/dc_usr.h"
#include "/usr/src/dvs/include/com/node_usr.h"
#include "/usr/src/dvs/include/com/proc_usr.h"
#include "/usr/src/dvs/include/com/proc_sts.h"
#include "/usr/src/dvs/include/com/com.h"
#include "/usr/src/dvs/include/com/ipc.h"
#include "/usr/src/dvs/include/com/cmd.h"
#include "/usr/src/dvs/include/com/proxy_usr.h"
#include "/usr/src/dvs/include/com/proxy_sts.h"
#include "/usr/src/dvs/include/com/dvs_errno.h"
#include "/usr/src/dvs/include/com/endpoint.h"
#include "/usr/src/dvs/include/com/priv_usr.h"
#include "/usr/src/dvs/include/com/priv.h"
#include "/usr/src/dvs/include/com/stub_dvkcall.h"
#include "/usr/src/dvs/include/com/kipc.h"
#include "/usr/src/dvs/include/generic/configfile.h"


#include "../debug.h"
#include "../macros.h"
#include "/usr/src/dvs/vos/muk2/mutex.h"


typedef long unsigned int update_t;
	
/*
 * basic procs and threads
 */

#define MUK_STACK_SIZE		32768
#define MUK_DVK_INTERVAL	1000
#define MUK_BIT_DONE		15     

#define USE_UCONTEXT 1

#if USE_UCONTEXT
#include <ucontext.h>
#endif

typedef struct Task Task;
typedef struct Tasklist Tasklist;

int		anyready(void);
int		taskcreate(void (*f)(void *arg), void *arg, unsigned int stacksize);
void		taskexit(int);
void		taskexitall(int);
void		taskmain(int argc, char *argv[]);
int		taskyield(void);
void**		taskdata(void);
void		needstack(int);
void		taskname(char*, ...);
void		taskstate(char*, ...);
char*		taskgetname(void);
char*		taskgetstate(void);
void		tasksystem(void);
unsigned int	taskdelay(unsigned int);
unsigned int	taskid(void);
Task * 		current_task(void);


struct Tasklist	/* used internally */
{
	Task	*head;
	Task	*tail;
};

/*
 * queuing locks
 */
typedef struct QLock QLock;
struct QLock
{
	Task	*owner;
	Tasklist waiting;
};

void	qlock(QLock*);
int	canqlock(QLock*);
void	qunlock(QLock*);

/*
 * reader-writer locks
 */
typedef struct RWLock RWLock;
struct RWLock
{
	int	readers;
	Task	*writer;
	Tasklist rwaiting;
	Tasklist wwaiting;
};

void	rlock(RWLock*);
int	canrlock(RWLock*);
void	runlock(RWLock*);

void	wlock(RWLock*);
int	canwlock(RWLock*);
void	wunlock(RWLock*);

/*
 * sleep and wakeup (condition variables)
 */
typedef struct Rendez Rendez;

struct Rendez
{
	QLock	*l;
	Tasklist waiting;
};

void	tasksleep(Rendez*);
int	taskwakeup(Rendez*);
int	taskwakeupall(Rendez*);

/*
 * channel communication
 */
typedef struct Alt Alt;
typedef struct Altarray Altarray;
typedef struct Channel Channel;

enum
{
	CHANEND,
	CHANSND,
	CHANRCV,
	CHANNOP,
	CHANNOBLK,
};

struct Alt
{
	Channel		*c;
	void		*v;
	unsigned int	op;
	Task		*task;
	Alt		*xalt;
};

struct Altarray
{
	Alt		**a;
	unsigned int	n;
	unsigned int	m;
};

struct Channel
{
	unsigned int	bufsize;
	unsigned int	elemsize;
	unsigned char	*buf;
	unsigned int	nbuf;
	unsigned int	off;
	Altarray	asend;
	Altarray	arecv;
	char		*name;
};

int		chanalt(Alt *alts);
Channel*	chancreate(int elemsize, int elemcnt);
void		chanfree(Channel *c);
int		chaninit(Channel *c, int elemsize, int elemcnt);
int		channbrecv(Channel *c, void *v);
void*		channbrecvp(Channel *c);
unsigned long	channbrecvul(Channel *c);
int		channbsend(Channel *c, void *v);
int		channbsendp(Channel *c, void *v);
int		channbsendul(Channel *c, unsigned long v);
int		chanrecv(Channel *c, void *v);
void*		chanrecvp(Channel *c);
unsigned long	chanrecvul(Channel *c);
int		chansend(Channel *c, void *v);
int		chansendp(Channel *c, void *v);
int		chansendul(Channel *c, unsigned long v);

/*
 * Threaded I/O.
 */
int		fdread(int, void*, int);
int		fdread1(int, void*, int);	/* always uses fdwait */
int		fdwrite(int, void*, int);
void		fdwait(int, int);
int		fdnoblock(int);

void		fdtask(void*);

/*
 * Network dialing - sets non-blocking automatically
 */
enum
{
	UDP = 0,
	TCP = 1,
};

int		netannounce(int, char*, int);
int		netaccept(int, char*, int*);
int		netdial(int, char*, int);
int		netlookup(char*, uint32_t*);	/* blocks entire program! */
int		netdial(int, char*, int);

typedef struct Context Context;

struct Context
{
	ucontext_t	uc;
};

typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned long long uvlong;
typedef long long vlong;

struct	dvk_request_s {
	Task	*rq_task;
	int		rq_oper;
	int		rq_other_ep;
	message *rq_mptr;
	long	rq_timeout;
	vcopy_t rq_vcopy;
	int		rq_rcode;
};
typedef struct dvk_request_s dvk_request_t;
#define DVK_RQST_FORMAT "id=%d rq_oper=%d rq_other_ep=%d rq_timeout=%ld\n"
#define DVK_RQST_FIELDS(r) r->rq_task->id,r->rq_oper, r->rq_other_ep, r->rq_timeout

#define MUK_VCOPY_FORMAT	VCOPY_FORMAT
#define MUK_VCOPY_FIELDS(p) p->v_src, p->v_dst, p->v_rqtr,p->v_saddr, p->v_daddr, p->v_bytes

struct Task
{
	char	name[256];	// offset known to acid
	char	state[256];
	Task	*next;
	Task	*prev;
	Task	*allnext;
	Task	*allprev;
	Context	context;
	uvlong	alarmtime;
	uint	id;
	uchar	*stk;
	uint	stksize;
	int		exiting;
	int		alltaskslot;
	int		system;
	int		ready;
	void	(*startfn)(void*);
	void	*startarg;
	void	*udata;
	
	pid_t			pid;
	
	proc_usr_t 		*p_proc;		/* pointer to a proc_usr_t allocated struct 	*/
	pthread_t		p_thread;
	pthread_mutex_t p_mutex;
	pthread_cond_t	p_th_cond;		/* condition variable for DVK ipc thread */ 
	pthread_cond_t	p_tsk_cond;		/* condition variable for the task to synchronize with DVK ipc thread */ 
	dvk_request_t	p_rqst;			
	
	Task			*p_caller_q;	/* head list of trying to send task to this task*/
	Task			*p_q_link;		/* pointer to the next trying to send task    	*/
	Rendez			p_rendez;
	int				p_error;
	unsigned long	p_pending;		/* bitmap of pending notifies 					*/
	long			p_timeout;
	Task			*p_to_link;
	dvktimer_t		p_alarm_timer;
	message			*p_msg;  
	
    TAILQ_ENTRY(Task) p_entries;         /* Tail queue. */

};	 

typedef struct Task muk_proc_t;

int task_bind( Task* t, int dcid,  int p_ep, char *name);
int task_unbind( Task* t, int p_ep);
int muk_tbind(int dcid, int p_ep, char *name);
int muk_unbind(int dcid, int p_ep);
Task *get_task(int p_ep);

int muk_send_T(int dst_ep, message *mptr, long timeout);
int muk_receive_T(int src_ep, message *mptr, long timeout);
int muk_sendrec_T(int srcdst_ep, message* m_ptr, long timeout_ms);
int muk_notify_X(int src_nr, int dst_ep);

#define muk_sendrec(srcdst_ep, m_ptr)		muk_sendrec_T(srcdst_ep, m_ptr, TIMEOUT_FOREVER)
#define muk_send(dst_ep, m_ptr)				muk_send_T(dst_ep, m_ptr, TIMEOUT_FOREVER)
#define muk_receive(src_ep, m_ptr)			muk_receive_T(src_ep, m_ptr, TIMEOUT_FOREVER)

#define muk_notify(dst_ep)						muk_notify_X(SELF, dst_ep)
#define muk_src_notify(src_nr, dst_ep)			muk_notify_X(src_nr, dst_ep)

#define muk_wait4bind()						muk_wait4bind_X(WAIT_BIND,  SELF,  TIMEOUT_FOREVER)
#define muk_wait4bindep(endpoint)			muk_wait4bind_X(WAIT_BIND,  endpoint,  TIMEOUT_FOREVER)
#define muk_wait4unbind(endpoint)			muk_wait4bind_X(WAIT_UNBIND, endpoint, TIMEOUT_FOREVER)
#define muk_wait4bind_T(to_ms)				muk_wait4bind_X(WAIT_BIND, SELF, to_ms)
#define muk_wait4bindep_T(endpoint, to_ms)	muk_wait4bind_X(WAIT_BIND,  endpoint, to_ms)
#define muk_wait4unbind_T(endpoint, to_ms)	muk_wait4bind_X(WAIT_UNBIND, endpoint, to_ms)

int muk_timeout_task(void );

#ifdef __cplusplus
}
#endif
#endif

