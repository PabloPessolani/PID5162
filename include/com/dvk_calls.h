#ifndef _COM_DVK_CALLS_H
#define DVK__COM_DVK_CALLS_H

#define DVK_VOID1		    0
#define DVK_DCINIT		   	1	
#define DVK_SEND		   	2	
#define DVK_RECEIVE			3	
#define DVK_NOTIFY		   	4	
#define DVK_SENDREC	 		5  	
#define DVK_RCVRQST			6
#define DVK_REPLY			7
#define DVK_DCEND			8	/* End a DC 			*/
#define DVK_BIND			9	/* Bind a process to IPC  	*/
#define DVK_UNBIND			10	/* UnBind a process to IPC  	*/
#define DVK_GETPRIV			11	/* Get process priviledges	*/
#define DVK_SETPRIV			12	/* Set process priviledges	*/
#define DVK_VCOPY			13   /* Virtual Copy			*/
#define DVK_GETDCINFO		14	/* Get DC information		*/
#define DVK_GETPROCINFO		15	/* Get Proc information		*/
#define DVK_RELAY			16	
#define DVK_PROXYBIND		17	
#define DVK_PROXYUNBIND		18	
#define DVK_GETNODEINFO		19	
#define DVK_PUT2LCL			20	/* Used by receiver proxy to send messages to local processes */	
#define DVK_GET2RMT			21	/* Used by sender  proxy to send local messages to remote  processes */	
#define DVK_ADDNODE			22	
#define DVK_DELNODE			23	
#define DVK_DVSINIT			24	
#define DVK_DVSEND			25
#define DVK_GETEP			26	/* Get process endpoint 	*/
#define DVK_GETDVSINFO		27	
#define DVK_PROXYCONN		28	
#define DVK_WAIT4BIND		29	
#define DVK_MIGRATE			30	
#define DVK_NODEUP			31	
#define DVK_NODEDOWN		32	
#define DVK_GETPROXYINFO	33	
#define DVK_WAKEUPEP		34	
#define DVK_NR_CALLS	 	35   /* Numero de IPCs/DRDCM Calls habilitadas */ 

#define NR_DVK_CALLS 		DVK_NR_CALLS

#endif /* _COM_DVK_CALLS_H */
