
#ifndef _COM_CMD_H
#define _COM_CMD_H

// headers needed
#ifndef _COM_IPC_H
#include "ipc.h"
#endif // _COM_IPC_H

/* COMMANDS 	*/
  enum proxy_cmd{
        CMD_NONE      =  0,	/* NO COMMAND  							*/
		CMD_SEND_MSG,		/*  Send a message to a process 					*/
		CMD_NTFY_MSG,		/* Send a NOTIFY message to remote proces  		*/
		CMD_SNDREC_MSG,		/*  Send a message to a process and wait for reply 		*/
		
		CMD_REPLY_MSG,		/*  Send a REPLY message to a process 				*/
		CMD_COPYIN_DATA,	/* Request and DATA to copy data to remote process 		*/
		CMD_COPYOUT_RQST,	/* The remote process send to local process the data requested 	*/
		CMD_COPYLCL_RQST,
		
		CMD_COPYRMT_RQST,	/* REQUESTER to SENDER to copy data out to RECEIVER */
		CMD_COPYIN_RQST,	/* SENDER to RECEIVER */
        CMD_HELLO,			/* HELLO COMMAND used by proxies 				*/
		CMD_SHUTDOWN,		/* Exit the waiting loop with error	EMOLINTR */

		CMD_PROCINFO,		/* Request remote RPROXY info about a REMOTE endpont */
		CMD_PROCBIND,		/* Request remote RPROXY to BIND an endpoint */		
		CMD_PROCUNBIND,		/* Request remote RPROXY to UNBIND an endpoint  */
		CMD_LAST_CMD		/* THIS MUST BE THE LAST COMMAND */
  };

#define CMD_SEND_ACK		(CMD_SEND_MSG     | MASK_ACKNOWLEDGE) 
#define CMD_NTFY_ACK		(CMD_NTFY_MSG     | MASK_ACKNOWLEDGE)
#define CMD_SNDREC_ACK  	(CMD_SNDREC_MSG   | MASK_ACKNOWLEDGE)
#define	CMD_REPLY_ACK   	(CMD_REPLY_MSG    | MASK_ACKNOWLEDGE)
#define	CMD_COPYIN_ACK 		(CMD_COPYIN_DATA  | MASK_ACKNOWLEDGE)
#define	CMD_COPYOUT_DATA 	(CMD_COPYOUT_RQST | MASK_ACKNOWLEDGE)
#define	CMD_COPYLCL_ACK		(CMD_COPYLCL_RQST | MASK_ACKNOWLEDGE)
#define CMD_COPYRMT_ACK 	(CMD_COPYRMT_RQST | MASK_ACKNOWLEDGE) /* From RECEIVER to REQUESTER */
#define CMD_PROCINFO_ACK	(CMD_PROCINFO     | MASK_ACKNOWLEDGE) 
#define CMD_PROCBIND_ACK	(CMD_PROCBIND     | MASK_ACKNOWLEDGE) 
#define CMD_PROCUNBIND_ACK	(CMD_PROCUNBIND   | MASK_ACKNOWLEDGE) 
  
struct vcopy_s {
	int	v_src;		    /* source endpoint		*/
	int	v_dst;		    /* destination endpoint	*/
  	int	v_rqtr;		    /* requester endpoint		*/
	void 	*v_saddr;	/* virtual address copy from */
  	void 	*v_daddr;	/* virtual address copy to 	*/
  	int	v_bytes;	    /* bytes to copy			*/
};
typedef struct vcopy_s vcopy_t;

struct cmd_s {
	int	c_cmd;		
	int c_dcid;		                /* DC ID					*/
	int	c_src;		                /* source endpoint			*/
	int	c_dst;		                /* destination endpoint			*/
	int	c_snode;	                /* source node				*/
	int	c_dnode;	                /* destination node			*/
	int c_rcode;	                /* return code 				*/
  	int c_len;		                /* payload len 				*/
	int c_pid;						/* source PID 				*/
	int c_batch_nr;	                /* # of batched commands in payload 								*/
  	unsigned long c_flags;			/* generic field for flags filled and controled by proxies not by M3-IPC 		*/
  	unsigned long c_snd_seq;		/* send sequence #  - filled and controled by proxies not by M3-IPC 			*/
  	unsigned long c_ack_seq;		/* acknowledge sequence #  - filled and controled by proxies not by M3-IPC 	*/
	struct timespec c_timestamp;	/* timestamp												*/
	message c_msg;					/* embedded message 			*/
	vcopy_t c_vcopy;				/* struct used only for remote vcopy 	*/
};
typedef struct cmd_s cmd_t;

#define CMD_FORMAT "cmd=0x%X dcid=%d src=%d dst=%d snode=%d dnode=%d rcode=%d len=%d PID=%d\n" 
#define CMD_FIELDS(p) 	p->c_cmd, p->c_dcid, p->c_src, p->c_dst, p->c_snode \
	, p->c_dnode, p->c_rcode, p->c_len, p->c_pid 

#define CMD_XFORMAT "c_batch_nr=%d c_flags=0x%lX c_snd_seq=%ld c_ack_seq=%ld c_timestamp.tv_sec=%ld\n" 
#define CMD_XFIELDS(p) 	p->c_batch_nr, p->c_flags, p->c_snd_seq, p->c_ack_seq, p->c_timestamp.tv_sec 
	
#define VCOPY_FORMAT "src=%d dst=%d rqtr=%d saddr=%p daddr=%p bytes=%d \n"
#define VCOPY_FIELDS(p) p->c_vcopy.v_src, p->c_vcopy.v_dst, p->c_vcopy.v_rqtr,\
	 p->c_vcopy.v_saddr, p->c_vcopy.v_daddr, p->c_vcopy.v_bytes

// #define PCOPY_FORMAT "src=%d dst=%d rqtr=%d saddr=%p daddr=%p bytes=%d \n"
// #define PCOPY_FIELDS(p) p->p_vcopy.v_src, p->p_vcopy.v_dst, p->p_vcopy.v_rqtr,\
//	 p->p_vcopy.v_saddr, p->p_vcopy.v_daddr, p->p_vcopy.v_bytes	 

#define HDR_FORMAT 		CMD_FORMAT
#define HDR_FIELDS(p)	CMD_FIELDS(p)


#endif // _COM_CMD_H



