/********************************************************/
/* 		TIPC  PROXIES				*/
/********************************************************/

#include "proxy.h"
#include "debug.h"
#include "macros.h"

#define HEADER_SIZE sizeof(proxy_hdr_t)
#define BASE_TYPE      3000

#define RETRY_US		2000 /* Microseconds */
#define BIND_RETRIES	3

#define SERVER_TYPE  18888
#define SERVER_INST  17
           
int local_nodeid;

proxy_hdr_t 	*p_header;
proxy_payload_t *p_payload;

int rmsg_ok = 0;
int rmsg_fail = 0;
int smsg_ok = 0;
int smsg_fail = 0; 
dvs_usr_t dvs;   
struct hostent *rmthost;
proxies_usr_t px, *px_ptr;

struct sockaddr_tipc rmtclient_addr, rmtserver_addr;
int    rlisten_sd, rconn_sd;
int    sproxy_sd;

/*----------------------------------------------*/
/*      PROXY RECEIVER FUNCTIONS               */
/*----------------------------------------------*/
                
/* pr_setup_connection: bind and setup a listening socket 
   This socket is not accepting connections yet after
   end of this call.
 */                
int pr_setup_connection(void) 
{
    int ret;
    int server_type;
    int optval = 1;
	
	struct sockaddr_tipc server_addr;
	
    server_type = (BASE_TYPE + px.px_id);
	PXYDEBUG("RPROXY: for node %s running at type=%d ,inst=%d\n", px.px_name, server_type, SERVER_INST);
	
    // Create server socket.
    if ( (rlisten_sd = socket(AF_TIPC, SOCK_STREAM , 0)) < 0) 
        ERROR_EXIT(errno);

/*     if( (ret = setsockopt(rlisten_sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) < 0)
        	ERROR_EXIT(errno); */

    // Bind (attach) this process to the server socket.
	server_addr.family = AF_TIPC;
	server_addr.addrtype = TIPC_ADDR_NAMESEQ;
	server_addr.addr.nameseq.type = server_type;
	server_addr.addr.nameseq.lower = SERVER_INST;
	server_addr.addr.nameseq.upper = SERVER_INST;
	server_addr.scope = TIPC_ZONE_SCOPE;

	if (0 != bind(rlisten_sd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
		ERROR_EXIT(errno);
	}

	PXYDEBUG("RPROXY: is bound to type=%d ,inst=%d, socket=%d\n", server_type, SERVER_INST, rlisten_sd);
	
// Turn 'rproxy_sd' to a listening socket.
	if (0 != listen(rlisten_sd, 0)) {
		ERROR_EXIT(errno);
	}

    return(OK);
   
}

/* pr_receive_payloadr: receives the header from remote sender */
int pr_receive_payload(int payload_size) 
{
    int n, len, total,  received = 0;
    char *p_ptr;

   	PXYDEBUG("payload_size=%d\n",payload_size);
   	p_ptr = (char*) p_payload;
   	len = sizeof(struct sockaddr_in);
   	while ((n = recv(rconn_sd, p_ptr, (payload_size-received), 0 )) > 0) {
        received = received + n;
		PXYDEBUG("RPROXY: n:%d | received:%d\n", n,received);
        if (received >= payload_size) return(OK);
       	p_ptr += n;
		len = sizeof(struct sockaddr_in);
   	}
    
    if(n < 0) ERROR_RETURN(errno);
 	return(OK);
}

/* pr_receive_header: receives the header from remote sender */
int pr_receive_header(void) 
{
    int n, len, total,  received = 0;
    char *p_ptr;

   	PXYDEBUG("socket=%d\n", rconn_sd);
   	p_ptr = (char*) p_header;
	total = sizeof(proxy_hdr_t);
   	len = sizeof(struct sockaddr_in);
   	while ((n = recv(rconn_sd, p_ptr, (total-received), 0 )) > 0) {
        received = received + n;
		PXYDEBUG("RPROXY: n:%d | received:%d | HEADER_SIZE:%d\n", n,received,sizeof(proxy_hdr_t));
        if (received >= sizeof(proxy_hdr_t)) {  
			PXYDEBUG("RPROXY: " CMD_FORMAT,CMD_FIELDS(p_header));
        	return(OK);
        } else {
			PXYDEBUG("RPROXY: Header partially received. There are %d bytes still to get\n", 
                  	sizeof(proxy_hdr_t) - received);
        	p_ptr += n;
        }
		len = sizeof(struct sockaddr_in);
   	}
    
    ERROR_RETURN(errno);
}

/* pr_process_message: receives header and payload if any. Then deliver the 
 * message to local */
int pr_process_message(void) {
    int rcode, payload_size, ret, retry;
	proc_usr_t kproc, *kp_ptr;
	message *m_ptr;
	
    do {
		bzero(p_header, sizeof(proxy_hdr_t));
		PXYDEBUG("RPROXY: About to receive header\n");
    	rcode = pr_receive_header();
    	if (rcode != 0) ERROR_RETURN(rcode);
		PXYDEBUG("RPROXY:" CMD_FORMAT,CMD_FIELDS(p_header));
   	}while(p_header->c_cmd == CMD_NONE);
    
	/* now we have a proxy header in the buffer. Cast it.*/
    payload_size = p_header->c_len;
    
    /* payload could be zero */
    if(payload_size != 0) {
        bzero(p_payload, payload_size);
        rcode = pr_receive_payload(payload_size);
        if (rcode != 0){
			PXYDEBUG("RPROXY: No payload to receive.\n");
		}
    }else{
		switch(p_header->c_cmd){
			case CMD_SEND_MSG:
			case CMD_SNDREC_MSG:
			case CMD_REPLY_MSG:
				m_ptr = &p_header->c_u.cu_msg;
				PXYDEBUG("RPROXY: " MSG1_FORMAT,  MSG1_FIELDS(m_ptr));
				break;
			case CMD_COPYIN_DATA:
			case CMD_COPYOUT_DATA:
				PXYDEBUG("RPROXY: "VCOPY_FORMAT, VCOPY_FIELDS(p_header)); 
				break;
			default:
				break;
		}
	}
	
	PXYDEBUG("RPROXY: put2lcl\n");
	rcode = dvk_put2lcl(p_header, p_payload);
#ifdef RMT_CLIENT_BIND	
	if( rcode == OK) return (OK);
	/******************** REMOTE CLIENT BINDING ************************************/
	/* rcode: the result of the las dvk_put2lcl 	*/
	/* ret: the result of the following operations	*/
	PXYDEBUG("RPROXY: REMOTE CLIENT BINDING\n");
	switch(rcode){
		case EDVSNOTBIND:	/* the slot is free */	
		case EDVSENDPOINT:	/* local node registers other endpoint using the slot */
		case EDVSNONODE:	/* local node register other node for this endpoint   */
			break;
		default:
			ERROR_RETURN(rcode);
	} 
	
	/* Try to bind the PROXY to the M3-IPC kernel with  "PROXY" endpoint number */
	retry = BIND_RETRIES;
	do {
		ret = dvk_bind(p_header->c_dcid, PROXY);
		if(ret < PROXY) {
			ERROR_PRINT(ret);
			ERROR_RETURN(rcode); /* !!!! return the original return code !!!! */
		}
		if( ret != PROXY) {
			retry--;
			usleep(RETRY_US);  /* [0,2000000] */
		}
	}while(ret != PROXY && retry > 0);
	if ( retry == 0) {
		PXYDEBUG("RPROXY: can bind after %d retries\n",BIND_RETRIES);
		ERROR_RETURN(rcode); /* !!!! return the original return code !!!! */
	}
	
    s_ptr = &slot;
	ret = sys_getslot(&s_ptr, p_header->c_src);
	PXYDEBUG("RPROXY: " SLOTS_FORMAT, SLOTS_FIELDS(s_ptr));
	
	/* check if the source node match the slot owner */
	if( s_ptr->s_owner !=  p_header->c_snode){
		fprintf(stderr,"RPROXY:endpoint %d source node %d don't match slot owner %d\n"
				,p_header->c_src, s_ptr->s_owner, p_header->c_snode );
		dvk_unbind( p_header->c_dcid, PROXY);	
		ERROR_RETURN(rcode); /* !!!! return the original return code !!!! */		
	}
	
	if(rcode == EDVSNONODE || rcode == EDVSENDPOINT){
		/* free the slot first */
		ret = sys_exit(p_header->c_src);
		if(ret) 
			ERROR_PRINT(ret);
	}
	
	/* bind the remote process */
	ret = sys_bindrproc(p_header->c_src, p_header->c_snode);
	if(ret != p_header->c_src) 
		ERROR_PRINT(ret);
	dvk_unbind( p_header->c_dcid, PROXY);	
    if(ret) ERROR_RETURN(rcode); /* !!!! return the original return code !!!! */

	kp_ptr = &kproc;
	sys_getproc(kp_ptr,p_header->c_src);
	PXYDEBUG("RPROXY: " PROC_USR_FORMAT , PROC_USR_FIELDS(kp_ptr));

	if( rcode ){
		ret = dvk_put2lcl(p_header, p_payload);
		if( ret) {
			ERROR_PRINT(ret);
			ERROR_RETURN(rcode);
		}
	}
#else /* RMT_CLIENT_BIND	*/
    if( rcode) ERROR_RETURN(rcode);	
#endif /* RMT_CLIENT_BIND	*/
	
    return(OK);    
}

/* pr_start_serving: accept connection from remote sender
   and loop receiving and processing messages
 */
void pr_start_serving(void)
{
	int sender_addrlen;
    char ip4[INET_ADDRSTRLEN];  // space to hold the IPv4 string
    struct sockaddr_tipc sa;
    int rcode;

    while (1){
		do {
			sender_addrlen = sizeof(sa);
			PXYDEBUG("RPROXY: Waiting for connection.\n");
    		rconn_sd = accept(rlisten_sd, (struct sockaddr *) &sa, &sender_addrlen);
    		if(rconn_sd < 0) ERROR_PRINT(errno);
		}while(rconn_sd < 0);

//		PXYDEBUG("RPROXY: Remote sender connected from [%d.%d.%d] on sd [%d]. Getting remote command.\n",
//			tipc_zone(sa.addr), tipc_cluster(sa.addr), tipc_node(sa.addr), rconn_sd);
		
		rcode = dvk_proxy_conn(px.px_id, CONNECT_RPROXY);

    	/* Serve Forever */
		do { 
	       	/* get a complete message and process it */
       		rcode = pr_process_message();
       		if (rcode == OK) {
				PXYDEBUG("RPROXY: Message succesfully processed.\n");
				rmsg_ok++;
        	} else {
				PXYDEBUG("RPROXY: Message processing failure [%d]\n",rcode);
            			rmsg_fail++;
				if( rcode == EDVSNOTCONN 
				 || rcode == EDVSCALLDENIED
				 || rcode == EDVSBADREQUEST) break;
			}	
		}while(1);

		rcode = dvk_proxy_conn(px.px_id, DISCONNECT_RPROXY);
		close(rconn_sd);
   	}
    
    /* never reached */
}

/* pr_init: creates socket */
void pr_init(void) 
{
    int receiver_sd, rcode;

	PXYDEBUG("RPROXY: Initializing proxy receiver. PID: %d\n", getpid());
    
	do { 
		rcode = dvk_wait4bind_T(RETRY_US);
		PXYDEBUG("RPROXY: dvk_wait4bind_T  rcode=%d\n", rcode);
		if (rcode == EDVSTIMEDOUT) {
			PXYDEBUG("RPROXY: dvk_wait4bind_T TIMEOUT\n");
			continue ;
		}else if(rcode == NONE) { /* proxies have not endpoint */
			break;	
		} if( rcode < 0) 
			exit(EXIT_FAILURE);
	} while	(rcode < OK);
	
	posix_memalign( (void**) &p_header, getpagesize(), (sizeof(proxy_hdr_t)+ getpagesize()));
	if (p_header== NULL) {
    		perror(" p_header posix_memalign");
    		exit(1);
  	}

	posix_memalign( (void**) &p_payload, getpagesize(), (sizeof(proxy_payload_t)+ getpagesize()));
	if (p_payload== NULL) {
    		perror(" p_payload posix_memalign");
    		exit(1);
  	}
	
	PXYDEBUG("p_header=%p p_payload=%p diff=%d\n", 
			p_header, p_payload, ((char*) p_payload - (char*) p_header));
    
    if( pr_setup_connection() == OK) {
      	pr_start_serving();
 	} else {
       	ERROR_EXIT(errno);
    }
}

/*----------------------------------------------*/
/*      PROXY SENDER FUNCTIONS                  */
/*----------------------------------------------*/

/* ps_send_header: send  header to remote receiver */
int  ps_send_header(void ) 
{
    int sent = 0;        // how many bytes we've sent
    int bytesleft;
    int n, total;
	char *p_ptr;

	bytesleft = sizeof(proxy_hdr_t); // how many bytes we have left to send
	total = bytesleft;
	PXYDEBUG("SPROXY: send header=%d \n", bytesleft);

    p_ptr = (char *) p_header;
    while(sent < total) {
        n = send(sproxy_sd, p_ptr, bytesleft, 0);
        if (n < 0) {
			if(errno == EALREADY) {
				ERROR_PRINT(errno);
				sleep(1);
				continue;
			}else{
				ERROR_RETURN(errno);
			}
		}
        sent += n;
		p_ptr += n; 
        bytesleft -= n;
    }
	PXYDEBUG("SPROXY: socket=%d sent header=%d \n", sproxy_sd, total);
    return(OK);
}

/* ps_send_payload: send payload to remote receiver */
int  ps_send_payload(void ) 
{
    int sent = 0;        // how many bytes we've sent
    int bytesleft;
    int n, total;
	char *p_ptr;

	if( p_header->c_len <= 0) ERROR_EXIT(EDVSINVAL);
	
	bytesleft =  p_header->c_len; // how many bytes we have left to send
	total = bytesleft;
	PXYDEBUG("SPROXY: send header=%d \n", bytesleft);

    p_ptr = (char *) p_payload;
    while(sent < total) {
        n = send(sproxy_sd, p_ptr, bytesleft, 0);
        if (n < 0) {
			if(errno == EALREADY) {
				ERROR_PRINT(errno);
				sleep(1);
				continue;
			}else{
				ERROR_RETURN(errno);
			}
		}
        sent += n;
		p_ptr += n; 
        bytesleft -= n;
    }
	PXYDEBUG("SPROXY: socket=%d sent payload=%d \n", sproxy_sd, total);
    return(OK);
}

/* 
 * ps_send_remote: send a message (header + payload if existing) 
 * to remote receiver
 */
int  ps_send_remote(void) 
{
	int rcode;

	PXYDEBUG("SPROXY:" CMD_FORMAT,CMD_FIELDS(p_header));

	/* send the header */
	rcode =  ps_send_header();
	if ( rcode != OK)  ERROR_RETURN(rcode);

	if( p_header->c_len > 0) {
		PXYDEBUG("SPROXY: send payload len=%d\n", p_header->c_len );
		rcode =  ps_send_payload();
		if ( rcode != OK)  ERROR_RETURN(rcode);
	}
	
    return(OK);
}

/* 
 * ps_start_serving: gets local message and sends it to remote receiver .
 * Do this forever.
 */
int  ps_start_serving(void)
{
    int rcode, i;
    message *m_ptr;
    int pid, ret;
   	char *ptr; 

    pid = getpid();
	
    while(1) {
		PXYDEBUG("SPROXY %d: Waiting a message\n", pid);
        
		ret = dvk_get2rmt(p_header, p_payload);       
      
		if( ret != OK) {
			switch(ret) {
				case EDVSTIMEDOUT:
					PXYDEBUG("SPROXY: Sending HELLO \n");
					p_header->c_cmd = CMD_NONE;
					p_header->c_len = 0;
					p_header->c_rcode = 0;
					break;
				case EDVSNOTCONN:
					ERROR_RETURN(EDVSNOTCONN);
				default:
					PXYDEBUG("ERROR  dvk_get2rmt %d\n", ret);
					continue;
			}
		}

		PXYDEBUG("SPROXY: %d "HDR_FORMAT,pid, HDR_FIELDS(p_header)); 

		switch(p_header->c_cmd){
			case CMD_SEND_MSG:
			case CMD_SNDREC_MSG:
			case CMD_REPLY_MSG:
				m_ptr = &p_header->c_u.cu_msg;
				PXYDEBUG("SPROXY: " MSG1_FORMAT,  MSG1_FIELDS(m_ptr));
				break;
			case CMD_COPYIN_DATA:
			case CMD_COPYOUT_DATA:
				PXYDEBUG("SPROXY: "VCOPY_FORMAT, VCOPY_FIELDS(p_header)); 
				break;
			default:
				break;
		}

		/* send the message to remote */
		rcode =  ps_send_remote();
		if (rcode == 0) {
			smsg_ok++;
		} else {
			smsg_fail++;
		}	
    }

    /* never reached */
    exit(1);
}

void wait_for_server(__u32 name_type, __u32 name_instance, int wait)
{
	struct sockaddr_tipc topsrv;
	struct tipc_subscr subscr;
	struct tipc_event event;

	int sd = socket(AF_TIPC, SOCK_SEQPACKET, 0);

	memset(&topsrv, 0, sizeof(topsrv));
	topsrv.family = AF_TIPC;
	topsrv.addrtype = TIPC_ADDR_NAME;
	topsrv.addr.name.name.type = TIPC_TOP_SRV;
	topsrv.addr.name.name.instance = TIPC_TOP_SRV;

	/* Connect to topology server */

	if (0 > connect(sd, (struct sockaddr *)&topsrv, sizeof(topsrv))) {
		perror("Client: failed to connect to topology server");
		exit(1);
	}

	subscr.seq.type = htonl(name_type);
	subscr.seq.lower = htonl(name_instance);
	subscr.seq.upper = htonl(name_instance);
	subscr.timeout = htonl(wait);
	subscr.filter = htonl(TIPC_SUB_SERVICE);

	do {
		if (send(sd, &subscr, sizeof(subscr), 0) != sizeof(subscr)) {
			perror("Client: failed to send subscription");
			exit(1);
		}
		/* Now wait for the subscription to fire */

		if (recv(sd, &event, sizeof(event), 0) != sizeof(event)) {
			perror("Client: failed to receive event");
			exit(1);
		}
		if (event.event == htonl(TIPC_PUBLISHED)) {
			close(sd);
			return;
		}
		PXYDEBUG("Client: server {%u,%u} not published within %u [s]\n",
	       name_type, name_instance, wait/1000);	     
	} while(1);
	
}

/* ps_connect_to_remote: connects to the remote receiver */
int ps_connect_to_remote(void) 
{
    int server_type, rcode, i;
    char rmt_ipaddr[INET_ADDRSTRLEN+1];
	
	int rec_num;
	int rec_size;
	int tot_size;
	int sent_size;
	int msg_size;
	
	server_type = (BASE_TYPE + local_nodeid);
	wait_for_server(server_type, SERVER_INST, 10000);

	sproxy_sd = socket(AF_TIPC, SOCK_STREAM, 0);

	rmtserver_addr.family = AF_TIPC;
	rmtserver_addr.addrtype = TIPC_ADDR_NAME;
	rmtserver_addr.addr.name.name.type = server_type;
	rmtserver_addr.addr.name.name.instance = SERVER_INST;
	rmtserver_addr.addr.name.domain = 0;

 	rcode = connect(sproxy_sd, (struct sockaddr *) &rmtserver_addr, sizeof(rmtserver_addr));
    if (rcode != 0) ERROR_RETURN(errno);
    return(OK);
}

/* 
 * ps_init: creates sender socket, the connect to remote and
 * start sending messages to remote 
 */
void  ps_init(void) 
{
    int rcode = 0;
	char *p_buffer;

	PXYDEBUG("SPROXY: Initializing on PID:%d\n", getpid());
    
	do { 
		rcode = dvk_wait4bind_T(RETRY_US);
		PXYDEBUG("SPROXY: dvk_wait4bind_T  rcode=%d\n", rcode);
		if (rcode == EDVSTIMEDOUT) {
			PXYDEBUG("SPROXY: dvk_wait4bind_T TIMEOUT\n");
			continue ;
		}else if(rcode == NONE) { /* proxies have not endpoint */
			break;	
		} if( rcode < 0) 
			exit(EXIT_FAILURE);
	} while	(rcode < OK);
		
	posix_memalign( (void**) &p_header, getpagesize(), (sizeof(proxy_hdr_t)+ getpagesize()));
	if (p_header== NULL) {
    		perror("p_header posix_memalign");
    		exit(1);
  	}

	posix_memalign( (void**) &p_payload, getpagesize(), (sizeof(proxy_payload_t)+ getpagesize()));
	if (p_payload== NULL) {
    		perror("p_payload posix_memalign");
    		exit(1);
  	}
	
    // Create server socket.
    if ( (sproxy_sd = socket(AF_TIPC, SOCK_STREAM, 0)) < 0){
       	ERROR_EXIT(errno);
    }

    /* try to connect many times */
	while(1) {
		do {
			if (rcode < 0)
				PXYDEBUG("SPROXY: Could not connect to %d"
                    			" Sleeping for a while...\n", px.px_id);
			sleep(4);
			rcode = ps_connect_to_remote();
		} while (rcode != 0);
		
		rcode = dvk_proxy_conn(px.px_id, CONNECT_SPROXY);
		if(rcode) ERROR_EXIT(rcode);
	
		ps_start_serving();
	
		rcode = dvk_proxy_conn(px.px_id, DISCONNECT_SPROXY);
		if(rcode) ERROR_EXIT(rcode);
	
		close(sproxy_sd);
	}


    /* code never reaches here */
}

extern int errno;

/*----------------------------------------------*/
/*		MAIN: 			*/
/*----------------------------------------------*/
void  main ( int argc, char *argv[] )
{
	int spid, rpid, pid, status;
	int ret;
	dvs_usr_t *d_ptr;    

	if (argc != 3) {
		fprintf(stderr,"Usage: %s <px_name> <px_id> \n", argv[0]);
		exit(0);
	}

	strncpy(px.px_name,argv[1], MAXPROXYNAME);
	px.px_name[MAXPROCNAME-1]= '\0';
	printf("TCP Proxy Pair name: %s\n",px.px_name);

	px.px_id = atoi(argv[2]);
	printf("Proxy Pair id: %d\n",px.px_id);

	ret = dvk_open();
	if (ret < 0)  ERROR_PRINT(ret);

	local_nodeid = dvk_getdvsinfo(&dvs);
	d_ptr=&dvs;
	PXYDEBUG(DVS_USR_FORMAT,DVS_USR_FIELDS(d_ptr));

	pid = getpid();
	PXYDEBUG("MAIN: pid=%d local_nodeid=%d\n", pid, local_nodeid);

	/* creates SENDER and RECEIVER Proxies as children */
	if ( (spid = fork()) == 0) ps_init();
	if ( (rpid = fork()) == 0) pr_init();

	/* register the proxies */
	ret = dvk_proxies_bind(px.px_name, px.px_id, spid, rpid, MAXCOPYBUF);
	if( ret < 0) ERROR_EXIT(ret);
	
	px_ptr = &px;
	PXYDEBUG(PX_USR_FORMAT , PX_USR_FIELDS(px_ptr));
	PXYDEBUG("binded to (%d,%d)\n", spid, rpid);

	ret= dvk_node_up(px.px_name, px.px_id, px.px_id);	
	
   	wait(&status);
    wait(&status);
    exit(0);
}

