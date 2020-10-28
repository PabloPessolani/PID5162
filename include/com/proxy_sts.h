
#ifndef _COM_PROXY_STS_H
#define _COM_PROXY_STS_H

#define MIS_BIT_PROXY		0
#define MIS_BIT_CONNECTED	1
#define MIS_BIT_NOTIFY		2
#define MIS_BIT_NEEDMIGR	3

#define MIS_BIT_RMTBACKUP	4
#define MIS_BIT_GRPLEADER	5
#define MIS_BIT_KTHREAD		6
#define MIS_BIT_REPLICATED 	7

#define MIS_BIT_KILLED	 	8
#define MIS_BIT_WOKENUP		9
#define MIS_BIT_USERMODE	10
#define MIS_BIT_RMTINFO		11

#define MIS_BIT_NOMIGRATE 	12
#define MIS_BIT_ATOMIC 		13		// equivalent to REPLY_PENDING in MINIX3
#define MIS_BIT_ENQUEUED 	14		// The process descriptor is enqueued in other' process queue

enum mis_status {
		MIS_PROXY		= (1<<MIS_BIT_PROXY),		/* the process is a proxy 			*/
		MIS_CONNECTED	= (1<<MIS_BIT_CONNECTED),	/* The proxy is connected 			*/
		MIS_NOTIFY		= (1<<MIS_BIT_NOTIFY),		/* A notify is pending 	 			*/
		MIS_NEEDMIG		= (1<<MIS_BIT_NEEDMIGR), 	/* The proccess need to migrate		*/
		MIS_RMTBACKUP	= (1<<MIS_BIT_RMTBACKUP), 	/* The proccess is a remote process' backup	*/
		MIS_GRPLEADER	= (1<<MIS_BIT_GRPLEADER), 	/* The proccess is the thread group leader 	*/	
		MIS_KTHREAD		= (1<<MIS_BIT_KTHREAD), 	/* The proccess is a KERNEL thread 	*/	
		MIS_REPLICATED	= (1<<MIS_BIT_REPLICATED), 	/* The ep is LOCAL but it is replicated on other nodes */	
		MIS_KILLED		= (1<<MIS_BIT_KILLED), 		/* The process has been killed 		*/
		MIS_WOKENUP 	= (1<<MIS_BIT_WOKENUP), 	/* The process has been signaled 	*/
		MIS_USERMODE 	= (1<<MIS_BIT_USERMODE), 	/* The process is running inside a local USERMODE VOS */
		MIS_RMTINFO 	= (1<<MIS_BIT_RMTINFO), 	/* The process is waiting remote information */
		MIS_NOMIGRATE	= (1<<MIS_BIT_NOMIGRATE), 	/* Do not migrate this process */
		MIS_ENQUEUED	= (1<<MIS_BIT_ENQUEUED), 	/* The process descriptor is enqueued in other' process queue */
};

#define PX_BIT_INUSE		0
#define PX_BIT_SCONNECTED	1
#define PX_BIT_RCONNECTED	2

#define PROXIES_FREE		0x0000
enum px_status {
		PROXIES_INUSE		= (1<<PX_BIT_INUSE),		/* the proxy pair is in use 		*/
		PROXIES_SCONNECTED	= (1<<PX_BIT_SCONNECTED),	
		PROXIES_RCONNECTED	= (1<<PX_BIT_RCONNECTED),	
};

#endif // _COM_PROXY_STS_H



