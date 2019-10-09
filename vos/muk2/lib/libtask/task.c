/* Copyright (c) 2005 Russ Cox, MIT; see COPYRIGHT */

#include "taskimpl.h"
#include <fcntl.h>
#include <stdio.h>
#include <pthread.h>

extern int	local_nodeid;

int	taskdebuglevel;
int	taskcount;
int	tasknswitch;
int	taskexitval;
Task	*taskrunning;

Context	taskschedcontext;
Tasklist	taskrunqueue;

Task	**alltask;
int		nalltask;

Task  *pproc[NR_PROCS+NR_TASKS];

static char *argv0;
static	void		contextswitch(Context *from, Context *to);

static void init_task_rqst( Task *t)
{
	t->p_rqst.rq_task 		= t;
	t->p_rqst.rq_oper 		= (-1);
	t->p_rqst.rq_other_ep 	= NONE;
	t->p_rqst.rq_mptr		= NULL;
	t->p_rqst.rq_timeout	= TIMEOUT_FOREVER;
	t->p_rqst.rq_rcode 		= OK;
}

static void pth_dvk(void *t_arg)
{
	Task *t;
	dvk_request_t *r_ptr;
	vcopy_t		 *v_ptr;
	int rcode;
	
	LIBDEBUG("DVK thread for %s\n", t->p_proc->p_name);	
	PH_MTX_LOCK(t->p_mutex);
	PH_COND_WAIT(t->p_cond, t->p_mutex);
	PH_MTX_UNLOCK(t->p_mutex);
	
	t->p_proc->p_lpid = t->pid;
	t->p_proc->p_vpid = syscall(SYS_gettid);
	rcode = dvk_tbind(t->p_proc->p_dcid, t->p_proc->p_endpoint);
	if( rcode != t->p_proc->p_endpoint){
		ERROR_EXIT(rcode);
	}
	
	LIBDEBUG("DVK thread for %s READY to process requests on endpoint=%d\n", 
			t->p_proc->p_name, rcode);	
			
	while(TRUE){
		PH_MTX_LOCK(t->p_mutex);
		PH_COND_WAIT(t->p_cond, t->p_mutex);
		PH_MTX_UNLOCK(t->p_mutex);
		r_ptr = &t->p_rqst;
		LIBDEBUG("%s: " DVK_RQST_FORMAT, t->p_proc->p_name, DVK_RQST_FIELDS(r_ptr));
		switch(r_ptr->rq_oper) {
			case SEND:
				r_ptr->rq_rcode = dvk_send_T(r_ptr->rq_other_ep,r_ptr->rq_mptr,r_ptr->rq_timeout);
				break;
			case SENDREC:
				r_ptr->rq_rcode = dvk_sendrec_T(r_ptr->rq_other_ep,r_ptr->rq_mptr,r_ptr->rq_timeout);
				break;
			case RECEIVE:
				r_ptr->rq_rcode = dvk_receive_T(r_ptr->rq_other_ep,r_ptr->rq_mptr,r_ptr->rq_timeout);
				break;
			case NOTIFY:
				r_ptr->rq_rcode = dvk_notify(r_ptr->rq_other_ep);
				break;
			case VCOPY:
				v_ptr = &r_ptr->rq_vcopy;
				r_ptr->rq_rcode = dvk_vcopy(v_ptr->v_src,v_ptr->v_saddr,
										v_ptr->v_dst,v_ptr->v_daddr,
										v_ptr->v_bytes);
				break;
			default:
				r_ptr->rq_rcode = EDVSNOSYS;
				break;
		}
		if(r_ptr->rq_rcode < 0) ERROR_PRINT(r_ptr->rq_rcode);
		SET_BIT(r_ptr->rq_oper, MUK_BIT_DONE);
		pthread_kill(t->pid, SIGIO);
	}
}

static void
taskdebug(char *fmt, ...)
{
	va_list arg;
	char buf[128];
	Task *t;
	char *p;
	static int fd = -1;

return;
	va_start(arg, fmt);
	vfprint(1, fmt, arg);
	va_end(arg);
return;

	if(fd < 0){
		p = strrchr(argv0, '/');
		if(p)
			p++;
		else
			p = argv0;
		snprint(buf, sizeof buf, "/tmp/%s.tlog", p);
		if((fd = open(buf, O_CREAT|O_WRONLY, 0666)) < 0)
			fd = open("/dev/null", O_WRONLY);
	}

	va_start(arg, fmt);
	vsnprint(buf, sizeof buf, fmt, arg);
	va_end(arg);
	t = taskrunning;
	if(t)
		fprint(fd, "%d.%d: %s\n", getpid(), t->id, buf);
	else
		fprint(fd, "%d._: %s\n", getpid(), buf);
}

static void
taskstart(uint y, uint x)
{
	Task *t;
	ulong z;

	z = x<<16;	/* hide undefined 32-bit shift from 32-bit compilers */
	z <<= 16;
	z |= y;
	t = (Task*)z;

//print("taskstart %p\n", t);
	t->startfn(t->startarg);
//print("taskexits %p\n", t);
	taskexit(0);
//print("not reacehd\n");
}

static int taskidgen;

int muk_getep(int id)
{
	Task *t;
	int i; 
	for( i=0; i < nalltask; i++) {
		t = alltask[i];
		if( t->id == id) {
			if(t->p_proc->p_endpoint != NONE) {
				return(t->p_proc->p_endpoint);
			} else {
				ERROR_RETURN(EDVSNOTBIND);
			}
		}
	}
	ERROR_RETURN(EDVSBADPID);
}

Task *get_task(int p_ep)
{
	int p_nr;
	Task *tptr;
	
	p_nr = _ENDPOINT_P(p_ep);
	LIBDEBUG("p_ep=%d p_nr=%d\n", p_ep, p_nr);
	if( p_nr < (-dc_ptr->dc_nr_tasks) || p_nr >= (dc_ptr->dc_nr_procs)){
//		ERROR_PRINT(EDVSBADVALUE);
		return(NULL);
	}
	if( pproc[p_nr+dc_ptr->dc_nr_tasks] == NULL) {
//		ERROR_PRINT(EDVSBADPROC);
		return(NULL);
	}
	LIBDEBUG("pproc=%X\n", &pproc[p_nr+dc_ptr->dc_nr_tasks]);

	tptr = pproc[p_nr+dc_ptr->dc_nr_tasks];
	assert(tptr->p_proc->p_nr == p_nr);
	return(tptr);
}


int muk_tbind(int dcid, int p_ep, char *name)
{
	int rcode;
	
	LIBDEBUG("dcid=%d p_ep=%d name=%s\n", dcid, p_ep, name);
	rcode = task_bind( taskrunning, dcid, p_ep, name);
	return(rcode);
}

int task_bind( Task* t, int dcid, int p_ep, char *name)
{
	int p_nr, rcode;
	proc_usr_t *p_ptr;
	
	assert(t != NULL);
	
	p_ptr = &t->p_proc;
	
	p_ptr = malloc(sizeof (proc_usr_t));
	if(p_ptr == NULL){
		ERROR_EXIT(-errno);
	}
	
	init_task_rqst(t);
			
	p_nr = _ENDPOINT_P(p_ep);
	LIBDEBUG("id=%d p_nr=%d p_ep=%d\n", t->id, p_nr, p_ep);
	if( p_nr < (-dc_ptr->dc_nr_tasks) || p_nr >= (dc_ptr->dc_nr_procs))
		ERROR_EXIT(EDVSBADVALUE);
	if( pproc[p_nr+dc_ptr->dc_nr_tasks] != NULL) 
		ERROR_EXIT(EDVSBUSY);			
	pproc[p_nr+dc_ptr->dc_nr_tasks] = t;
		
	p_ptr->p_dcid 		= dcid;
	p_ptr->p_nr	  		= p_nr;	
	p_ptr->p_endpoint 	= p_ep;	
	p_ptr->p_lpid 		= syscall(SYS_gettid);
	p_ptr->p_vpid 		= PROC_NO_PID;
	p_ptr->p_nodeid		= local_nodeid;
	p_ptr->p_rts_flags 	= PROC_RUNNING;		/* process is runnable only if zero 		*/
	p_ptr->p_misc_flags = MIS_UNIKERNEL;	/* miselaneous flags				*/
	p_ptr->p_getfrom	= NONE;				/* from whom does process want to receive?	*/
	p_ptr->p_sendto		= NONE;					/* to whom does process want to send? 	*/
	p_ptr->p_waitmigr	= NONE;
	p_ptr->p_proxy		= NONE;
	strncpy(p_ptr->p_name, name, MAXPROCNAME-1);
	taskname("%s",p_ptr->p_name, p_ep);
	LIBDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(p_ptr));
	LIBDEBUG("pproc=%X\n", &pproc[p_nr+dc_ptr->dc_nr_tasks]);
	
	rcode = pthread_create( &t->p_thread, NULL, pth_dvk, t);
	if(rcode) ERROR_EXIT(rcode);
	MUKDEBUG("t->p_thread=%u\n",t->p_thread);
		
	PH_MTX_LOCK(t->p_mutex);
	PH_COND_SIGNAL(t->p_cond);
	PH_MTX_UNLOCK(t->p_mutex);
		
	return(p_ep);
}

int muk_unbind(int dcid, int p_ep)
{
	Task *t_ptr;
	int p_nr;
	
	int rcode;
	p_nr = _ENDPOINT_P(p_ep);
	t_ptr = pproc[p_nr+dc_ptr->dc_nr_tasks];
	LIBDEBUG("dcid=%d p_ep=%d\n", p_ep);
	rcode = task_unbind(t_ptr, p_ep);
	return(rcode);
}

int task_unbind( Task* t, int p_ep)
{
	int p_nr, i;
	
	p_nr = _ENDPOINT_P(p_ep);
	assert(t != NULL);
	LIBDEBUG("id=%d p_nr=%d p_ep=%d\n", t->id, p_nr, p_ep);
	if( p_nr < (-dc_ptr->dc_nr_tasks) || p_nr >= (dc_ptr->dc_nr_procs))
		ERROR_EXIT(EDVSBADVALUE);
	if( pproc[p_nr+dc_ptr->dc_nr_tasks] == NULL) 
		ERROR_RETURN(EDVSBADPROC);	

	// wakeup all receivers waiting a message from the unbounding task	
	for( i = 0; i < (dc_ptr->dc_nr_procs+dc_ptr->dc_nr_tasks); i++) {
		if( pproc[i] == NULL) continue;
		if( pproc[i]->p_proc->p_endpoint == p_ep) continue;
		if( TEST_BIT(pproc[i]->p_proc->p_rts_flags, BIT_RECEIVING) == 0) continue;
		if( pproc[i]->p_proc->p_getfrom == p_ep) {
			pproc[i]->p_proc->p_getfrom = NONE;
			CLR_BIT(pproc[i]->p_proc->p_rts_flags, BIT_RECEIVING);
			if(pproc[i]->p_proc->p_rts_flags == 0) {
				taskwakeup(&pproc[i]->p_rendez);
			}
		}
	}
	
	free(t->p_proc);
	t->p_proc		= NULL;
	t->p_thread		= (-1);
//	t->p_mutex 		= PTHREAD_MUTEX_INITIALIZER;
//	t->p_cond		= PTHREAD_COND_INITIALIZER;
	pthread_mutex_init(&t->p_mutex, NULL);
	pthread_cond_init(&t->p_cond, NULL);
	init_task_rqst(t);

	t->p_caller_q	= NULL; 			/* head list of trying to send task to this task */
	t->p_q_link		= NULL; 			/* pointer to the next trying to send task    	*/
	t->p_msg		= NULL; 			/* pointer to application message buffer 	*/ 
	t->p_error 		= 0; 				/* returned error from IPC after block 	*/ 
	t->p_pending	= 0; 				/* bitmap of pending notifies 		 	*/ 
	t->p_timeout 	= TIMEOUT_FOREVER;
	t->p_to_link 	= NULL;
		
	pproc[p_nr+dc_ptr->dc_nr_tasks] = NULL;
	return(OK);
}

static Task*
taskalloc(void (*fn)(void*), void *arg, uint stack)
{
	Task *t;
	sigset_t zero;
	uint x, y;
	ulong z;

	/* allocate the task and stack together */
	t = malloc(sizeof *t+stack);
	if(t == nil){
		fprint(2, "taskalloc malloc: %r\n");
		abort();
	}
	memset(t, 0, sizeof *t);
	t->stk = (uchar*)(t+1);
	t->stksize = stack;
	t->id = ++taskidgen;
	t->startfn = fn;
	t->startarg = arg;
	
	t->p_proc		= NULL;
	t->p_thread		= (-1);
//	t->p_mutex 		= PTHREAD_MUTEX_INITIALIZER;
//	t->p_cond		= PTHREAD_COND_INITIALIZER;
	pthread_mutex_init(&t->p_mutex, NULL);
	pthread_cond_init(&t->p_cond, NULL);

	init_task_rqst(t);
		
	t->p_caller_q	= NULL; 			/* head list of trying to send msg to this task */
	t->p_q_link		= NULL; 			/* pointer to the next trying to send task    	*/
	t->p_msg		= NULL; 			/* pointer to application message buffer 	*/ 
	t->p_error 		= 0; 				/* returned error from IPC after block 	*/ 
	t->p_pending	= 0; 				/* bitmap of pending notifies 		 	*/ 
	t->p_timeout 	= TIMEOUT_FOREVER;
	t->p_to_link 	= NULL;				/* link to other process on timeout list */
	
	/* do a reasonable initialization */
	memset(&t->context.uc, 0, sizeof t->context.uc);
	sigemptyset(&zero);
	sigprocmask(SIG_BLOCK, &zero, &t->context.uc.uc_sigmask);

	/* must initialize with current context */
	if(getcontext(&t->context.uc) < 0){
		fprint(2, "getcontext: %r\n");
		abort();
	}

	/* call makecontext to do the real work. */
	/* leave a few words open on both ends */
	t->context.uc.uc_stack.ss_sp = t->stk+8;
	t->context.uc.uc_stack.ss_size = t->stksize-64;
#if defined(__sun__) && !defined(__MAKECONTEXT_V2_SOURCE)		/* sigh */
#warning "doing sun thing"
	/* can avoid this with __MAKECONTEXT_V2_SOURCE but only on SunOS 5.9 */
	t->context.uc.uc_stack.ss_sp = 
		(char*)t->context.uc.uc_stack.ss_sp
		+t->context.uc.uc_stack.ss_size;
#endif
	/*
	 * All this magic is because you have to pass makecontext a
	 * function that takes some number of word-sized variables,
	 * and on 64-bit machines pointers are bigger than words.
	 */
//print("make %p\n", t);
	z = (ulong)t;
	y = z;
	z >>= 16;	/* hide undefined 32-bit shift from 32-bit compilers */
	x = z>>16;
	makecontext(&t->context.uc, (void(*)())taskstart, 2, y, x);

	return t;
}

int
taskcreate(void (*fn)(void*), void *arg, uint stack)
{
	int id;
	Task *t;

	t = taskalloc(fn, arg, stack);
	taskcount++;
	id = t->id;
	if(nalltask%64 == 0){
		alltask = realloc(alltask, (nalltask+64)*sizeof(alltask[0]));
		if(alltask == nil){
			fprint(2, "out of memory\n");
			abort();
		}
	}
	t->pid = syscall (SYS_gettid);
	t->alltaskslot = nalltask;
	alltask[nalltask++] = t;
	taskready(t);
	return id;
}


void
tasksystem(void)
{
	if(!taskrunning->system){
		taskrunning->system = 1;
		--taskcount;
	}
}

void
taskswitch(void)
{
	needstack(0);
	contextswitch(&taskrunning->context, &taskschedcontext);
}

void
taskready(Task *t)
{
	t->ready = 1;
	addtask(&taskrunqueue, t);
}

int
taskyield(void)
{
	int n;
	
	n = tasknswitch;
	taskready(taskrunning);
	taskstate("yield");
	taskswitch();
	return tasknswitch - n - 1;
}

int
anyready(void)
{
	return taskrunqueue.head != nil;
}

void
taskexitall(int val)
{
	exit(val);
}

void
taskexit(int val)
{
	taskexitval = val;
	taskrunning->exiting = 1;
	taskswitch();
}


static void
contextswitch(Context *from, Context *to)
{
	if(swapcontext(&from->uc, &to->uc) < 0){
		fprint(2, "swapcontext failed: %r\n");
		assert(0);
	}
}

static void
taskscheduler(void)
{
	int i;
	Task *t;

	taskdebug("scheduler enter");
	for(;;){
		if(taskcount == 0)
			exit(taskexitval);
		t = taskrunqueue.head;
		if(t == nil){
			fprint(2, "no runnable tasks! %d tasks stalled\n", taskcount);
			exit(1);
		}
		deltask(&taskrunqueue, t);
		t->ready = 0;
		taskrunning = t;
		tasknswitch++;
		taskdebug("run %d (%s)", t->id, t->name);
		contextswitch(&taskschedcontext, &t->context);
//print("back in scheduler\n");
		taskrunning = nil;
		if(t->exiting){
			if(!t->system)
				taskcount--;
			i = t->alltaskslot;
			alltask[i] = alltask[--nalltask];
			alltask[i]->alltaskslot = i;
			free(t);
		}
	}
}

void**
taskdata(void)
{
	return &taskrunning->udata;
}

/*
 * debugging
 */
void
taskname(char *fmt, ...)
{
	va_list arg;
	Task *t;

	t = taskrunning;
	va_start(arg, fmt);
	vsnprint(t->name, sizeof t->name, fmt, arg);
	va_end(arg);
}

char*
taskgetname(void)
{
	return taskrunning->name;
}

void
taskstate(char *fmt, ...)
{
	va_list arg;
	Task *t;

	t = taskrunning;
	va_start(arg, fmt);
	vsnprint(t->state, sizeof t->name, fmt, arg);
	va_end(arg);
}

char*
taskgetstate(void)
{
	return taskrunning->state;
}


void
needstack(int n)
{
	Task *t;

	t = taskrunning;

	if((char*)&t <= (char*)t->stk
	|| (char*)&t - (char*)t->stk < 256+n){
		fprint(2, "task stack overflow: &t=%p tstk=%p n=%d\n", &t, t->stk, 256+n);
		abort();
	}
}


static void
taskinfo(int s)
{
	int i;
	Task *t;
	char *extra;
	proc_usr_t *p_ptr;

	fprint(2, "task list:\n");
	for(i=0; i<nalltask; i++){
		t = alltask[i];
		if(t == taskrunning)
			extra = " (running)";
		else if(t->ready)
			extra = " (ready)";
		else
			extra = "";
		fprint(2, "%6d%c %-20s %s%s\n", 
			t->id, t->system ? 's' : ' ', 
			t->name, t->state, extra);
	}
	
	fprintf(2, "DC pnr -endp -lpid/vpid- nd flag misc -getf -sndt -wmig -prxy name\n");	
	for(i=0; i<nalltask; i++){
		t = alltask[i];
		if( t->p_proc == NULL) continue;
		p_ptr = &t->p_proc;
		fprintf(2, "%2d %3d %5d %5ld/%-5ld %2d %4lX %4lX %5d %5d %5d %5d %-15.15s\n",
					p_ptr->p_dcid,
					p_ptr->p_nr,
					p_ptr->p_endpoint,
					p_ptr->p_lpid,
					p_ptr->p_vpid,
					p_ptr->p_nodeid,
					p_ptr->p_rts_flags,
					p_ptr->p_misc_flags,
					p_ptr->p_getfrom,
					p_ptr->p_sendto,
					p_ptr->p_waitmigr,
					p_ptr->p_proxy,
					p_ptr->p_name);
	}			
}


void dvk_handler(int s)
{
	int i;
	dvk_request_t *r_ptr;
	Task *t;
	
	for(i=0; i<nalltask; i++){
		t = alltask[i];
		if( t->p_proc == NULL) continue;
		r_ptr = &t->p_rqst;
		if( TEST_BIT(r_ptr->rq_oper, MUK_BIT_DONE)) {
			CLR_BIT(r_ptr->rq_oper, MUK_BIT_DONE);
			LIBDEBUG("wakeup %s: oper=%d rcode=%d\n", 
				t->name, r_ptr->rq_oper, r_ptr->rq_rcode);
			taskwakeup(&t->p_rendez);
		}
	}
}

/*
 * startup
 */

static int taskargc;
static char **taskargv;
int mainstacksize;

void
taskmainstart(void *v)
{
	taskname("taskmain");
	taskmain(taskargc, taskargv);
}

int
main(int argc, char **argv)
{
	int i;
	struct sigaction sa, osa;

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = taskinfo;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGQUIT, &sa, &osa);

	sa.sa_handler = dvk_handler;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGIO, &sa, &osa);
	
	
#ifdef SIGINFO
	sigaction(SIGINFO, &sa, &osa);
#endif

	argv0 = argv[0];
	taskargc = argc;
	taskargv = argv;
	
	for( i = 0; i < (NR_PROCS+NR_TASKS); i++) {
		pproc[i] = NULL;
	}
	
	if(mainstacksize == 0)
		mainstacksize = 256*1024;
	taskcreate(taskmainstart, nil, mainstacksize);
	taskscheduler();
	fprint(2, "taskscheduler returned in main!\n");
	abort();
	return 0;
}

/*
 * hooray for linked lists
 */
void addtask(Tasklist *l, Task *t)
{
	if(l->tail){
		l->tail->next = t;
		t->prev = l->tail;
	}else{
		l->head = t;
		t->prev = nil;
	}
	l->tail = t;
	t->next = nil;
}

void
deltask(Tasklist *l, Task *t)
{
	if(t->prev)
		t->prev->next = t->next;
	else
		l->head = t->next;
	if(t->next)
		t->next->prev = t->prev;
	else
		l->tail = t->prev;
}

unsigned int
taskid(void)
{
	return taskrunning->id;
}

Task*  current_task(void)
{
	return taskrunning;
}






