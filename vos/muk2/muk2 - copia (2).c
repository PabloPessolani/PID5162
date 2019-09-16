#include <stdio.h> 
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
 
#include <sys/time.h>
#include <sys/queue.h>

#include "/usr/src/dvs/include/com/com.h"
#include "/usr/src/dvs/include/com/proc_sts.h"

#include "macros.h"
#include "debug.h"

#define TRUE	1
#define FALSE	0
#define OK		0

#define MAXPROCNAME	16

TAILQ_HEAD(tailhead, muk_proc_s) proc_head;
struct tailhead *proc_headp;                 /* Tail queue proc_head. */


/* =================================*/
/* PROCESS DESCRIPTOR	             */
/* =================================*/
struct muk_proc_s {

  int p_nr;								/* process number					*/
  int p_endpoint;						/* process endpoint				*/
  volatile unsigned long p_rts_flags;	/* process is runnable only if zero 		*/
  volatile unsigned long  p_misc_flags;	/* miselaneous flags				*/
  int p_getfrom;						/* from whom does process want to receive?	*/
  int p_sendto;							/* to whom does process want to send? 	*/
  TAILQ_ENTRY(muk_proc_s) p_entry;     	/* Tail queue.					 */
  jmp_buf	p_env;					
  char p_name[MAXPROCNAME]; 	  
};
typedef struct muk_proc_s muk_proc_t;

#define PROC_MUK_FORMAT "nr=%d endp=%d flags=%lX misc=%lX name=%s\n"
#define PROC_MUK_FIELDS(p) p->p_nr,p->p_endpoint, p->p_rts_flags, p->p_misc_flags, p->p_name

#define	MUK_SERVER		0
#define	MUK_CLIENT		1
#define	MUK_NR_PROCS 	2

#define	MUK_SVR_EP	MUK_SERVER
#define	MUK_CLT_EP	MUK_CLIENT


muk_proc_t proc[MUK_NR_PROCS];
jmp_buf	proc_env[MUK_NR_PROCS];		
jmp_buf	main_env;					
			

/*===========================================================================*
 *				init_main				     *
 *===========================================================================*/
void init_main(void )
{
	MUKDEBUG("\n");
	TAILQ_INIT(&proc_head);                      /* Initialize the queue. */
}

/*===========================================================================*
 *				init_server				     *
 *===========================================================================*/
void init_server(void)
{
	muk_proc_t *proc_ptr;
	
	MUKDEBUG("\n");
	proc_ptr = &proc[MUK_SERVER];
	
	proc_ptr->p_nr 			= MUK_SERVER;	/* process number				*/
	proc_ptr->p_endpoint 	= MUK_SVR_EP;	/* process endpoint			*/
	proc_ptr->p_rts_flags 	= PROC_RUNNING;	/* process is runnable only if zero 	*/
	proc_ptr->p_misc_flags	= 0;			/* miselaneous flags			*/
	proc_ptr->p_getfrom		= NONE;			/* from whom does process want to receive?	*/
	proc_ptr->p_sendto		= NONE;			/* to whom does process want to send? 		*/
//	p_entry		= ;         /* Tail queue. */
//	proc_ptr->p_env 		= NULL;					
	strcpy(proc_ptr->p_name,"SERVER"); 
	MUKDEBUG(PROC_MUK_FORMAT, PROC_MUK_FIELDS(proc_ptr));
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
	proc_ptr->p_endpoint 	= MUK_CLT_EP;	/* process endpoint			*/
	proc_ptr->p_rts_flags 	= PROC_RUNNING;	/* process is runnable only if zero 	*/
	proc_ptr->p_misc_flags	= 0;			/* miselaneous flags			*/
	proc_ptr->p_getfrom		= NONE;			/* from whom does process want to receive?	*/
	proc_ptr->p_sendto		= NONE;			/* to whom does process want to send? 		*/
//	p_entry		= ;         /* Tail queue. */
//	proc_ptr->p_env 		= NULL;					
	strcpy(proc_ptr->p_name,"CLIENT"); 
	MUKDEBUG(PROC_MUK_FORMAT, PROC_MUK_FIELDS(proc_ptr));
}

/*===========================================================================*
 *				muk_server				     *
 *===========================================================================*/
int muk_server(void)
{
	int rcode;
	muk_proc_t *proc_ptr;
	
	MUKDEBUG("\n");
	proc_ptr = &proc[MUK_SERVER];

	rcode = setjmp(proc_env[MUK_SERVER]);
	if(rcode == 0) return(OK);

	while(TRUE){
		MUKDEBUG("stop 1\n");
		rcode = setjmp(proc_env[MUK_SERVER]);
		proc_ptr = &proc[MUK_SERVER];
		if(rcode == 0) {MUK2_RETURN(1);}
		sleep(1);
		MUKDEBUG("stop 2\n");
		rcode = setjmp(proc_env[MUK_SERVER]);
		proc_ptr = &proc[MUK_SERVER];
		if(rcode == 0) {MUK2_RETURN(2);}
		sleep(1);
		MUKDEBUG("stop 3\n");
		rcode = setjmp(proc_env[MUK_SERVER]);
		proc_ptr = &proc[MUK_SERVER];
		if(rcode == 0) {MUK2_RETURN(3);}
		sleep(1);
	}	
	return(OK);
}

/*===========================================================================*
 *				muk_client				     *
 *===========================================================================*/
int muk_client(void)
{
	int rcode;
	muk_proc_t *proc_ptr;

	MUKDEBUG("\n");
	proc_ptr = &proc[MUK_CLIENT];
	
	rcode = setjmp(proc_env[MUK_CLIENT]);
	if(rcode == 0) return(OK);
	
	while(TRUE){
		MUKDEBUG("stop 1\n");
		rcode = setjmp(proc_env[MUK_CLIENT]);
		proc_ptr = &proc[MUK_CLIENT];
		if(rcode == 0){MUK2_RETURN(1);}
		sleep(1);
		MUKDEBUG("stop 2\n");
		rcode = setjmp(proc_env[MUK_CLIENT]);
		proc_ptr = &proc[MUK_CLIENT];
		if(rcode == 0) {MUK2_RETURN(2);}
		sleep(1);
		MUKDEBUG("stop 3\n");
		rcode = setjmp(proc_env[MUK_CLIENT]);
		proc_ptr = &proc[MUK_CLIENT];
		if(rcode == 0){MUK2_RETURN(3);}
		sleep(1);
	}	
	
	return(OK);
}

/*===========================================================================*
 *				get_next_proc				     *
 *===========================================================================*/
int get_next_proc(void)
{
	muk_proc_t *proc_ptr;

	proc_ptr = TAILQ_FIRST(&proc_head);
	if(proc_ptr == NULL) return(-1);

	MUKDEBUG("after TAILQ_FIRST %d\n", proc_ptr->p_nr);
    TAILQ_REMOVE(&proc_head, proc_ptr, p_entry);
	MUKDEBUG("after TAILQ_REMOVE %d\n", proc_ptr->p_nr);
	MUKDEBUG("p_nr=%d\n", proc_ptr->p_nr);
	return(proc_ptr->p_nr);
}


/*===========================================================================*
 *				main					     					*
 *===========================================================================*/
int main ( int argc, char *argv[] )
{
	int rcode;
	muk_proc_t *next_ptr;
	int next_proc;

	init_main();

	init_server();
	init_client();

	next_ptr = &proc[MUK_SERVER];
	TAILQ_INSERT_HEAD(&proc_head, next_ptr, p_entry);

	next_ptr = &proc[MUK_CLIENT];
	TAILQ_INSERT_TAIL(&proc_head, next_ptr, p_entry);
	
	rcode = setjmp(main_env); 
	if( rcode == 0) {
		rcode = muk_server();
		rcode = muk_client();
	}
	
	while(TRUE) {
		next_proc = get_next_proc(); // obtiene la proxima funcion a ejecutar de una cola de funciones.
		MUKDEBUG("next_proc=%d\n", next_proc);
		if( next_proc == (-1) ){
			sleep(1);
			continue;
		}
		assert( next_proc >= 0 || next_proc < MUK_NR_PROCS );
		next_ptr = &proc[next_proc];
		MUKDEBUG("before longjmp =%d\n", next_proc);
		rcode = setjmp(main_env); 
		if( rcode == 0) {
			longjmp(proc_env[next_proc], 1);
		} 
/*
		else {
			MUKDEBUG("after longjmp =%d\n", next_proc);
			TAILQ_INSERT_TAIL(&proc_head, next_ptr, p_entry);
			MUKDEBUG("after TAILQ_INSERT_TAIL =%d\n", next_proc);
		}
*/
	}
	exit(0);
}

