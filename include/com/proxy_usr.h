
#ifndef _COM_PROXY_USR_H
#define _COM_PROXY_USR_H

#ifndef _COM_CMD_H
#include "cmd.h"
#endif // _COM_CMD_H

#define MAXPROXYNAME	16
#define	CONNECT_SPROXY		1
#define	CONNECT_RPROXY		2
#define	DISCONNECT_SPROXY	3
#define	DISCONNECT_RPROXY	4

#define PROXY_NO_DC				(-1)	/* The proxy has not a DC		*/

#define	proxy_hdr_t	cmd_t 

#define ETHERNET_MTU	1500
#define IPV4_HEADER		20
#define UPD_HEADER		8
#define PROXY_HEADER	(sizeof(proxy_hdr_t)+sizeof(int))
#define MAXMTUSIZE	(ETHERNET_MTU - (IPV4_HEADER+UPD_HEADER) )
#define MAXBUFSIZE	(ETHERNET_MTU - (IPV4_HEADER+UPD_HEADER+PROXY_HEADER) ) 

#define PROXY_NO_REPLY 		0
#define PROXY_REPLY		1

#define BIT_ACKNOWLEDGE		13
#define MASK_ACKNOWLEDGE 	(1<<BIT_ACKNOWLEDGE)

#define FLAG_BATCH_BIT		0
#define FLAG_LZ4_BIT		1
#define FLAG_CHECK_HDR		2		// This flags signal that the true header was modified by a Load Balancer o Proxy an must be analyzed by the Proxy Receiver.

#define HELLO_PERIOD		(30*1000)	/* 30 Seconds */

#define	NO_PROXIES		(-1)

#define MAXBATCMD		(MAXCOPYBUF/sizeof(cmd_t))	

#ifndef _COM_PROC_USR_H
#include "../include/com/proc_usr.h"
#endif // 
	
typedef union {
	cmd_t 	pay_cmd[MAXBATCMD];		/* Proxies commands (ONLY FOR MESSAGE)  */
									/* can be used to transport multiple batch messages 			*/
	char 	pay_data[MAXCOPYBUF];	/* buffer space to copy data	*/  
	proc_usr_t 	pay_proc_usr;		/* process descriptor	*/  
} proxy_payload_t;

#define TIME_FORMAT "TIMESTAMP sec=%ld nsec=%ld\n"
#define TIME_FIELDS(p) p->tv_sec, p->tv_nsec

struct proxies_usr_s {
	unsigned int	px_id; 		/* The number of pair of proxies	*/
unsigned long int	px_flags; 	/* The status of the pair of proxies 	*/
	int				px_maxbytes;/*	Maximum number of bytes the proxies support (<= MAXCOPYBUF) */	
	int				px_maxbcmds; /*	Maximum number of batched messages commands the proxies support (<= MAXBATCMD) */	
	char			px_name[MAXPROXYNAME];

};
typedef struct proxies_usr_s proxies_usr_t;

#define PX_USR_FORMAT 		"px_id=%d px_flags=%d px_maxbytes=%d px_maxbcmds=%d px_name=%s\n" 
#define PX_USR_FIELDS(p) 	p->px_id, p->px_flags, p->px_maxbytes, p->px_maxbcmds, p->px_name
 
#endif // _COM_PROXY_USR_H
