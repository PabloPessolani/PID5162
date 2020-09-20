#ifndef _COM_COM_H
#define _COM_COM_H 

#define 	OK		0

#define 	TRUE	1
#define 	FALSE	0

#define	DVK_FILE_NAME	"/dev/dvk"

#define TIMEOUT_NOWAIT			0	
#define TIMEOUT_FOREVER			(-1)
#define TIMEOUT_MOLCALL  30000 //TIMEOUT for MOLSYSCALL. 30 sec

#define MIGR_START		0
#define MIGR_COMMIT		1
#define MIGR_ROLLBACK	2

#define SELF_BIND		0
#define LCL_BIND		1
#define RMT_BIND		2
#define BKUP_BIND		3
#define REPLICA_BIND	4
#define USERMODE_BIND	5
#define MAX_BIND_TYPE	USERMODE_BIND

#ifdef DVS_CAPABILITIES
#define CAP_DVS_ADMIN	CAP_SYS_ADMIN
#define CAP_VOS_ADMIN	CAP_SYS_ADMIN
#endif 

#define WAIT_BIND		0
#define WAIT_UNBIND		1

#define MAXPROCNAME	16
#define PROC_NO_PID	(-1)


/*===========================================================================*
 *          	    		Magic process numbers			     *
 *===========================================================================*/


/* These may not be any valid endpoint (see <minix/endpoint.h>). */
#define ANY			0x7ace	/* used to indicate 'any process' */
#define NONE 		0x6ace  /* used to indicate 'no process at all' */
#define SELF		0x8ace 	/* used to indicate 'own process' */
#define _MAX_MAGIC_PROC (SELF)	/* used by <minix/endpoint.h> 
				   to determine generation size */

#define LOCALNODE		(-1) /* The node id of processes running local */

#define MIN(x,y) 	(x<y)?x:y;
#define MAX(x,y) 	(x>y)?x:y;

#define SET_BIT(bitmap, bit_nr)    (bitmap |= (1 << bit_nr))
#define CLR_BIT(bitmap, bit_nr)    (bitmap &= ~(1 << bit_nr))
#define TEST_BIT(bitmap, bit_nr)   (bitmap & (1 << bit_nr))

/*===========================================================================*
 *            	Process numbers of processes in the system image	     *
 *===========================================================================*/

/* The values of several task numbers depend on whether they or other tasks
 * are enabled. They are defined as (PREVIOUS_TASK - ENABLE_TASK) in general.
 * ENABLE_TASK is either 0 or 1, so a task either gets a new number, or gets
 * the same number as the previous task and is further unused. Note that the
 * order should correspond to the order in the task table defined in table.c. 
 */


/* Kernel tasks. These all run in the same address space. */
#define KERNEL           -1			/* pseudo-process for IPC and scheduling */
#define HARDWARE     KERNEL			/* for hardware interrupt handlers */
#define SYSTEM           -2			/* request system functionality */
#define SLOTS  		 (SYSTEM-_NR_SYSTASKS)	/* process number until NR_NODES System tasks */
#define CLOCK  		 (SLOTS-1)
#define IDLE         (CLOCK-1)		/* runs when no one else can run */
#define PROXY 		IDLE			/* All node proxies share temporary this proc number */

#define SYSTASK(x)  (SYSTEM - x)

/* Number of tasks. Note that NR_PROCS is defined in <minix/config.h>. */
/* !!!!!!!!!!!!!!! IT MUST BE LOWER THAN  (-EMOLBUSY) = 16 !!!!!!!!!!!!!!!!!!!!!!!!!!!*/
#define NR_TASKS	  _NR_TASKS 

/*===========================================================================*
 *                	   Kernel notification types                         *
 *===========================================================================*/

/* Kernel notification types. In principle, these can be sent to any process,
 * so make sure that these types do not interfere with other message types.
 * Notifications are prioritized because of the way they are unhold() and
 * blocking notifications are delivered. The lowest numbers go first. The
 * offset are used for the per-process notification bit maps. 
 */
#define NOTIFY_MESSAGE		  0x1000
#define NOTIFY_FROM(p)	 (NOTIFY_MESSAGE | (p)) 

/* Shorthands for message parameters passed with notifications. */
#define NOTIFY_SOURCE	m_source
#define NOTIFY_TYPE		m_type
#define NOTIFY_FLAGS	m9_i1
#define NOTIFY_ARG		m9_l1
#define NOTIFY_TIMESTAMP m9_t1  

#endif /* _COM_COM_H */ 
