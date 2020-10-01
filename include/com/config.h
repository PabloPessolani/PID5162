#ifndef _COM_CONFIG_H
#define _COM_CONFIG_H

#define DVS_CAPABILITIES 1

#define DVSPROFILING	0

#define DVS_VERSION 5

#define MAXCOPYBUF	65536 
#define MAXCOPYLEN	(16 * MAXCOPYBUF)

#define USE_DVS_RWLOCK		0
#define USE_DVS_MUTEX		1
#define USE_DVS_SPINLOCK	2
#define USE_DVS_RWSEM		3
#define USE_DVS_RCU			4

#define USE_DC_RWLOCK		0
#define USE_DC_MUTEX		1
#define USE_DC_SPINLOCK		2
#define USE_DC_RWSEM		3
#define USE_DC_RCU			4

#define USE_PROC_RWLOCK		0
#define USE_PROC_MUTEX		1
#define USE_PROC_SPINLOCK	2
#define USE_PROC_RWSEM	3
#define USE_PROC_RCU		4

#define USE_NODE_RWLOCK		0
#define USE_NODE_MUTEX		1
#define USE_NODE_SPINLOCK	2
#define USE_NODE_RWSEM		3
#define USE_NODE_RCU		4

#define USE_PROXY_RWLOCK	0
#define USE_PROXY_MUTEX		1
#define USE_PROXY_SPINLOCK	2
#define USE_PROXY_RWSEM 	3
#define USE_PROXY_RCU		4

#define USE_TASK_RWLOCK		0
#define USE_TASK_MUTEX		1
#define USE_TASK_SPINLOCK	2
#define USE_TASK_RWSEM		3
#define USE_TASK_RCU		4

#define USE_LIST_NORMAL		0
#define USE_LIST_RCU		1

#define DEBUGALL	0xFFFFFFFF	/* Set all debug levels */

//#define LOCK_DVS_TYPE	USE_DVS_MUTEX
//#define LOCK_DC_TYPE		USE_DC_MUTEX
//#define LOCK_NODE_TYPE	USE_NODE_MUTEX
//#define LOCK_PROXY_TYPE	USE_PROXY_MUTEX

/*****************************************************/
/* WARNING: To use RCU: DVS, VM, NODE and PROXY 	*/
/* must use RCU , and PROC must use RCU or SPINLOCK */
/*****************************************************/
#define LOCK_DVS_TYPE	USE_DVS_MUTEX
#define LOCK_DC_TYPE	USE_DC_MUTEX
#define LOCK_NODE_TYPE	USE_NODE_MUTEX
#define LOCK_PROXY_TYPE	USE_PROXY_MUTEX
#define LOCK_PROC_TYPE	USE_PROC_MUTEX
#define LOCK_TASK_TYPE	USE_TASK_MUTEX
#define LIST_TYPE		USE_LIST_NORMAL

#define NR_FIXED_TASKS  _NR_FIXED_TASKS 
#define NR_NODES 		_NR_NODES 
#define NR_SYSTASKS		_NR_SYSTASKS
#define NR_TASKS		_NR_TASKS
#define NR_SERVERS		_NR_SERVERS
#define NR_SYS_PROCS	_NR_SYS_PROCS
#define NR_USR_PROCS    _NR_USR_PROCS
#define NR_PROCS		_NR_PROCS
#define NR_DCS 	  		_NR_DCS 

#endif /* _COM_CONFIG_H */
