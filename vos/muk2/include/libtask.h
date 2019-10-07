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
#if USE_UCONTEXT
#include <ucontext.h>
#endif
#include <sys/utsname.h>
#include <inttypes.h>

#include <inttypes.h>
#include "/usr/src/dvs/vos/muk2/lib/libtask/task.h"

typedef long unsigned int update_t;
	
/*
 * basic procs and threads
 */

#define MUK_STACK_SIZE		32768
#define MUK_DVK_INTERVAL	1000

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
	
	int p_endpoint;						/* process endpoint					*/
	int p_nr;							/* process number					*/
	volatile unsigned long p_rts_flags;	/* process is runnable only if zero 		*/
	volatile unsigned long p_misc_flags;	/* miselaneous flags			*/
	int p_getfrom;						/* from whom does process want to receive?	*/
	int p_sendto;						/* to whom does process want to send? 	*/
	Task	*p_caller_q; 				/* head list of trying to send task to this task */
	Task	*p_q_link; 					/* pointer to the next trying to send task    	*/
	Rendez	p_rendez;
	int		p_error;
	unsigned long	p_pending;			/* bitmap of pending notifies 			*/
	
	long	p_next_timeout;
	Task	*p_t_link;
	
	dvktimer_t p_alarm_timer;
	message	*p_msg;  
};

#define PROC_MUK_FORMAT "nr=%d id=%d flags=%0.8lX misc=%0.8lX p_getfrom=%d p_sendto=%d name=%s \n"
#define PROC_MUK_FIELDS(p) p->p_nr,p->id, p->p_rts_flags, p->p_misc_flags, p->p_getfrom, p->p_sendto, p->name

#define PROC_TO_FORMAT "id=%d p_next_timeout=%ld link=%d\n"
#define PROC_TO_FIELDS(p) p->id, p->p_next_timeout, ((p->p_t_link == NULL)?(-1):p->p_t_link->id)
			 
typedef struct Task muk_proc_t;

int task_bind( Task* t, int p_ep, char *name);
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

