#include <stdio.h> 
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
 
#include <sys/time.h>
#include <sys/queue.h>

#include "/usr/src/dvs/include/com/com.h"
#include "/usr/src/dvs/include/com/ipc.h"
#include "/usr/src/dvs/include/com/proc_sts.h"

#include "macros.h"
#include "debug.h"

#define TRUE	1
#define FALSE	0
#define OK		0

#define MAXPROCNAME	16

TAILQ_HEAD(waithead, muk_proc_s);
 
/* =================================*/
/* PROCESS DESCRIPTOR	             */
/* =================================*/
struct muk_proc_s {

  int p_nr;								/* process number					*/
  volatile unsigned long p_rts_flags;	/* process is runnable only if zero 		*/
  volatile unsigned long  p_misc_flags;	/* miselaneous flags				*/
  int p_getfrom;						/* from whom does process want to receive?	*/
  int p_sendto;							/* to whom does process want to send? 	*/
  TAILQ_ENTRY(muk_proc_s) p_shed;     	/* Tail queue.					 */
  TAILQ_ENTRY(muk_proc_s) p_send;     	/* Tail queue.					 */
  message	*p_msg;  
  struct waithead p_wlist;             
  char p_name[MAXPROCNAME]; 	  
};
typedef struct muk_proc_s muk_proc_t;

#define PROC_MUK_FORMAT "nr=%d flags=%lX misc=%lX name=%s\n"
#define PROC_MUK_FIELDS(p) p->p_nr, p->p_rts_flags, p->p_misc_flags, p->p_name

#define	MUK_SERVER		0
#define	MUK_CLIENT		1
#define	MUK_NR_PROCS 	2

#define	MUK_SVR_EP	MUK_SERVER
#define	MUK_CLT_EP	MUK_CLIENT

TAILQ_HEAD(tailhead, muk_proc_s) sched_head;
struct tailhead *proc_headp;                 /* Tail queue sched_head. */

muk_proc_t proc[MUK_NR_PROCS];
jmp_buf	proc_env[MUK_NR_PROCS];		
jmp_buf	sched_env;					
int current_nr;
muk_proc_t *current_ptr;
			
/*===========================================================================*
 *				muk_send				     *
 *===========================================================================*/
int muk_send(int dst_nr, message *mptr)
{
	muk_proc_t *dst_ptr, *src_ptr;
	
	MUKDEBUG("dst_nr:%d\n", dst_nr);
	assert(dst_nr != current_nr);
	assert(dst_nr >= 0 && dst_nr < MUK_NR_PROCS);
	
	dst_ptr =&proc[dst_nr];
	if( (TEST_BIT(dst_ptr->p_rts_flags,RECEIVING) != 0) 
	 && ((dst_ptr->p_getfrom == ANY) || (dst_ptr->p_getfrom == current_nr))){
		memcpy(dst_ptr->p_msg, mptr, sizeof(message));
		dst_ptr->p_msg->m_source = current_nr;
		dst_ptr->p_getfrom = NONE;
		CLR_BIT(dst_ptr->p_rts_flags,RECEIVING);
		if( dst_ptr->p_rts_flags == 0){
			TAILQ_INSERT_TAIL(&sched_head, dst_ptr, p_shed);	
		}		
	} else { // not receiving 
		current_ptr->p_sendto = dst_nr;
		SET_BIT(current_ptr->p_rts_flags,SENDING);
		current_ptr->p_msg = mptr;
		TAILQ_INSERT_TAIL(&dst_ptr->p_wlist, current_ptr, p_send);	
	    TAILQ_REMOVE(&sched_head, current_ptr, p_shed);
		return(SENDING);
	}
	return(OK);
}			

/*===========================================================================*
 *				muk_receive				     *
 *===========================================================================*/
int muk_receive(int src_nr, message *mptr)
{
	muk_proc_t *src_ptr, *dst_ptr;
	
	MUKDEBUG("src_nr:%d\n", src_nr);
	assert(src_nr != current_nr);
	assert( (src_nr >= 0 && src_nr < MUK_NR_PROCS) || (src_nr == ANY) );
	
	if(src_nr == ANY){
		src_ptr = TAILQ_FIRST(&current_ptr->p_wlist);
		if( src_ptr != NULL){
			CLR_BIT(src_ptr->p_rts_flags,SENDING);
			src_ptr->p_sendto = NONE;
		    TAILQ_REMOVE(&current_ptr->p_wlist, src_ptr, p_send);
			TAILQ_INSERT_TAIL(&sched_head, src_ptr, p_shed);
			return(OK);
		} else {
			SET_BIT(current_ptr->p_rts_flags,RECEIVING);
			current_ptr->p_getfrom = src_nr;
		    TAILQ_REMOVE(&sched_head, current_ptr, p_shed);
			return(RECEIVING);			
		}
	}
	
	TAILQ_FOREACH(src_ptr, &current_ptr->p_wlist, p_send){
		if ( src_ptr->p_nr == src_nr){
			CLR_BIT(src_ptr->p_rts_flags, SENDING);
			src_ptr->p_sendto = NONE;
		    TAILQ_REMOVE(&current_ptr->p_wlist, src_ptr, p_send);
			TAILQ_INSERT_TAIL(&sched_head, src_ptr, p_shed);
			return(OK);			
		} 
	}
	
	SET_BIT(current_ptr->p_rts_flags, RECEIVING);
	current_ptr->p_getfrom = src_nr;
	TAILQ_REMOVE(&sched_head, current_ptr, p_shed);
	return(RECEIVING);
}	
/*===========================================================================*
 *				init_main				     *
 *===========================================================================*/
void init_main(void )
{
	MUKDEBUG("\n");
	TAILQ_INIT(&sched_head);                      /* Initialize the queue. */
}

/*===========================================================================*
 *				init_server				     *
 *===========================================================================*/
void init_server(void)
{
	muk_proc_t *muk_proc_t;
	
	MUKDEBUG("\n");
	muk_proc_t = &proc[MUK_SERVER];
	
	muk_proc_t->p_nr 			= MUK_SERVER;	/* process number				*/
	muk_proc_t->p_rts_flags 	= PROC_RUNNING;	/* process is runnable only if zero 	*/
	muk_proc_t->p_misc_flags	= 0;			/* miselaneous flags			*/
	muk_proc_t->p_getfrom		= NONE;			/* from whom does process want to receive?	*/
	muk_proc_t->p_sendto		= NONE;			/* to whom does process want to send? 		*/
//	p_shed		= ;         /* Tail queue. */
//	muk_proc_t->p_env 		= NULL;					
	strcpy(muk_proc_t->p_name,"SERVER"); 
	TAILQ_INIT(&muk_proc_t->p_wlist);
	MUKDEBUG(PROC_MUK_FORMAT, PROC_MUK_FIELDS(muk_proc_t));
}

/*===========================================================================*
 *				init_client				     *
 *===========================================================================*/
void init_client(void)
{
	muk_proc_t *proc_ptr;
	
	MUKDEBUG("\n");

	proc_ptr = &proc[MUK_CLIENT];
	
	proc_ptr->p_nr 			= MUK_CLIENT;	/* process number				*/
	proc_ptr->p_rts_flags 	= PROC_RUNNING;	/* process is runnable only if zero 	*/
	proc_ptr->p_misc_flags	= 0;			/* miselaneous flags			*/
	proc_ptr->p_getfrom		= NONE;			/* from whom does process want to receive?	*/
	proc_ptr->p_sendto		= NONE;			/* to whom does process want to send? 		*/
//	p_shed		= ;         /* Tail queue. */
//	proc_ptr->p_env 		= NULL;					
	strcpy(proc_ptr->p_name,"CLIENT"); 
	TAILQ_INIT(&proc_ptr->p_wlist);
	MUKDEBUG(PROC_MUK_FORMAT, PROC_MUK_FIELDS(proc_ptr));
}

/*===========================================================================*
 *				muk_server				     *
 *===========================================================================*/
int muk_server(int stage)
{
	int rcode;
	muk_proc_t *proc_ptr;
	message 	msg, *mptr;
	
	MUKDEBUG("stage=%d\n",stage);
	
	rcode = setjmp(proc_env[MUK_SERVER]);
	if(rcode == 0) MUK2_RETURN(MUK_SERVER+1);
	
	MUKDEBUG("Starting SERVER\n");
	while(TRUE){
		MUKDEBUG("before receive\n");
		mptr = &msg;
		rcode = muk_receive(ANY, mptr);
		if( rcode == RECEIVING){
			rcode = setjmp(proc_env[MUK_SERVER]);
			if ( rcode == 0) MUK2_RETURN(MUK_SERVER+1);
		}
		MUKDEBUG(MSG1_FORMAT,MSG1_FIELDS(mptr));
		
		MUKDEBUG("PROCESSING...\n");	
		sleep(1);

		MUKDEBUG("before send\n");
		mptr = &msg;
		mptr->m1_i1 = 0x0A;
		mptr->m1_i2 = 0x0B;
		mptr->m1_i3 = 0x0C;
		rcode = muk_send(ANY, mptr);
		MUKDEBUG(MSG1_FORMAT,MSG1_FIELDS(mptr));
		if( rcode == SENDING){
			rcode = setjmp(proc_env[MUK_SERVER]);
			if ( rcode == 0) MUK2_RETURN(MUK_SERVER+1);
		}		
	}	
	return(OK);
}

/*===========================================================================*
 *				muk_client				     *
 *===========================================================================*/
int muk_client(int stage)
{
	int rcode;
	muk_proc_t *proc_ptr;
	message 	msg, *mptr;
	
	MUKDEBUG("stage=%d\n",stage);
	
	rcode = setjmp(proc_env[MUK_CLIENT]);
	if(rcode == 0) MUK2_RETURN(MUK_CLIENT+1)
	
	MUKDEBUG("Starting CLIENT\n");
	while(TRUE){
		MUKDEBUG("before send\n");
		mptr = &msg;
		mptr->m1_i1 = 0x01;
		mptr->m1_i2 = 0x02;
		mptr->m1_i3 = 0x03;
		rcode = muk_send(ANY, mptr);
		MUKDEBUG(MSG1_FORMAT,MSG1_FIELDS(mptr));
		if( rcode == SENDING){
			rcode = setjmp(proc_env[MUK_CLIENT]);
			if ( rcode == 0) MUK2_RETURN(MUK_CLIENT+1);
		}	
		
		MUKDEBUG("before receive\n");
		mptr = &msg;
		rcode = muk_receive(ANY, mptr);
		if( rcode == RECEIVING){
			rcode = setjmp(proc_env[MUK_CLIENT]);
			if ( rcode == 0) MUK2_RETURN(MUK_CLIENT+1);
		}
		MUKDEBUG(MSG1_FORMAT,MSG1_FIELDS(mptr));
		
		MUKDEBUG("PROCESSING...\n");	
		sleep(1);		
	}	
	
	return(OK);
}

/*===========================================================================*
 *				pick_proc				     *
 *===========================================================================*/
int pick_proc(void)
{
	muk_proc_t *proc_ptr;

	MUKDEBUG("\n");	
	proc_ptr = TAILQ_FIRST(&sched_head);
	if(proc_ptr == NULL) return(-1);
	MUKDEBUG("p_nr=%d\n", proc_ptr->p_nr);
	return(proc_ptr->p_nr);
}

/*===========================================================================*
 *				muk_sched				     *
 *===========================================================================*/
int muk_sched(stage )
{
		int rcode; 
		
		MUKDEBUG("stage=%d\n",stage);
	
		rcode = setjmp(sched_env);
		if(rcode == 0) return(OK);
		
		current_nr = pick_proc(); // obtiene la proxima funcion a ejecutar de una cola de funciones.
		MUKDEBUG("current_nr=%d\n", current_nr);
		if( current_nr == (-1) ){
			sleep(1);
			return(OK);
		}
		assert( current_nr >= 0 || current_nr < MUK_NR_PROCS );
		current_ptr = &proc[current_nr];
		rcode = setjmp(sched_env); 
		if( rcode == 0) {
			MUKDEBUG("before longjmp current_nr=%d\n", current_nr);
			longjmp(proc_env[current_nr], 1);
			// NOT RETURN HERE - Return at setjmp with rcode != 0
		} 
		return(OK);
}

/*===========================================================================*
 *				main					     					*
 *===========================================================================*/
int main ( int argc, char *argv[] )
{
	int rcode;
	int stage;
	
	init_main();
	init_server();
	init_client();

	current_ptr = &proc[MUK_SERVER];
	TAILQ_INSERT_HEAD(&sched_head, current_ptr, p_shed);
	current_ptr = &proc[MUK_CLIENT];
	TAILQ_INSERT_TAIL(&sched_head, current_ptr, p_shed);

	stage = 0;
	while(TRUE) {
		rcode = muk_sched(stage);
		rcode = muk_client(stage);
		rcode = muk_server(stage);
		stage = 1;
	}
	exit(0);
}


