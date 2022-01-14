/********************************************************/
/* 		TCP PROXIES	WITH BATCHING MESSAGES	*/
/********************************************************/

#include "proxy.h"
#include "debug.h"
#include "macros.h"

#define HEADER_SIZE sizeof(proxy_hdr_t)
#define BASE_PORT      3000

#define RETRY_US		2000 /* Microseconds */
#define BIND_RETRIES	3
#define BATCH_PENDING 	1
#define CMD_PENDING 	1

int local_nodeid;

proxy_hdr_t 	*p_header;
proxy_hdr_t 	*p_header2;
proxy_hdr_t 	*p_header3;
proxy_hdr_t 	*p_pseudo;
proxy_payload_t *p_payload;
proxy_payload_t *p_payload2;
proxy_payload_t *p_batch;

int	batch_nr = 0;		// number of batching commands
int	cmd_flag = 0;		// signals a command to be sent 
int rmsg_ok = 0;
int rmsg_fail = 0;
int smsg_ok = 0;
int smsg_fail = 0; 

dvs_usr_t dvs;   
struct hostent *rmthost;
proxies_usr_t px, *px_ptr;

struct sockaddr_in rmtclient_addr, rmtserver_addr;
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
    short int port_no;
    struct sockaddr_in servaddr;
    int optval = 1;

    port_no = (BASE_PORT+px.px_id);
	PXYDEBUG("RPROXY: for node %s running at port=%d\n", px.px_name, port_no);

    // Create server socket.
    if ( (rlisten_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
        ERROR_EXIT(errno);

    if( (ret = setsockopt(rlisten_sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) < 0)
        	ERROR_EXIT(errno);

    // Bind (attach) this process to the server socket.
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port_no);
	ret = bind(rlisten_sd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if(ret < 0) ERROR_EXIT(errno);

	PXYDEBUG("RPROXY: is bound to port=%d socket=%d\n", port_no, rlisten_sd);

// Turn 'rproxy_sd' to a listening socket. Listen queue size is 1.
	ret = listen(rlisten_sd, 0);
    if(ret < 0) ERROR_EXIT(errno);

    return(OK);
   
}

/* pr_receive_payloadr: receives the header from remote sender */
int pr_receive_payload(int payload_size) 
{
    int n, received = 0;
    char *p_ptr;

   	PXYDEBUG("payload_size=%d\n",payload_size);
   	p_ptr = (char*) p_payload;
   	while ((n = recv(rconn_sd, p_ptr, (payload_size-received), 0 )) > 0) {
        received = received + n;
		PXYDEBUG("RPROXY: n:%d | received:%d\n", n,received);
        if (received >= payload_size) return(OK);
       	p_ptr += n;
   	}
    
    if(n < 0) ERROR_RETURN(errno);
 	return(OK);
}

/* pr_receive_header: receives the header from remote sender */
int pr_receive_header(void) 
{
    int n, total,  received = 0;
    char *p_ptr;

   	PXYDEBUG("socket=%d\n", rconn_sd);
   	p_ptr = (char*) p_header;
	total = sizeof(proxy_hdr_t);
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
   	}
    
    ERROR_RETURN(errno);
}

/* pr_process_message: receives header and payload if any. Then deliver the 
 * message to local */
int pr_process_message(void) {
    int rcode, payload_size, i;
 	message *m_ptr;
	proxy_hdr_t *bat_cmd, *bat_ptr;

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
				m_ptr = &p_header->c_msg;
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
	if( rcode < 0) ERROR_RETURN(rcode);	
		
	if( TEST_BIT( p_header->c_flags,  FLAG_BATCH_BIT)) {
		batch_nr = p_header->c_batch_nr;
		PXYDEBUG("RPROXY: batch_nr=%d\n", batch_nr);
		// check payload len
		if( (batch_nr * sizeof(proxy_hdr_t)) != p_header->c_len){
			PXYDEBUG("RPROXY: put2lcl\n");
			ERROR_RETURN(EDVSBADVALUE);
		}
		// put the batched commands
		bat_cmd = &p_payload->pay_cmd;
		for( i = 0; i < batch_nr; i++){
			bat_ptr = &bat_cmd[i];
			PXYDEBUG("RPROXY: bat_cmd[%d]:" CMD_FORMAT, i, CMD_FIELDS(bat_ptr));
			rcode = dvk_put2lcl(bat_ptr, p_payload);
			if( rcode < 0) ERROR_RETURN(rcode);		
		}
	}
	
    return(OK);    
}

/* pr_start_serving: accept connection from remote sender
   and loop receiving and processing messages
 */
void pr_start_serving(void)
{
	int sender_addrlen;
    char ip4[INET_ADDRSTRLEN];  // space to hold the IPv4 string
    struct sockaddr_in sa;
    int rcode;

    while (1){
		do {
			sender_addrlen = sizeof(sa);
			PXYDEBUG("RPROXY: Waiting for connection.\n");
    		rconn_sd = accept(rlisten_sd, (struct sockaddr *) &sa, &sender_addrlen);
    		if(rconn_sd < 0) ERROR_PRINT(errno);
		}while(rconn_sd < 0);

		PXYDEBUG("RPROXY: Remote sender [%s] connected on sd [%d]. Getting remote command.\n",
           		inet_ntop(AF_INET, &(sa.sin_addr), ip4, INET_ADDRSTRLEN),
           		rconn_sd);
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
				if( rcode == EDVSNOTCONN) break;
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
    int rcode;

	PXYDEBUG("RPROXY: Initializing proxy receiver. PID: %d\n", getpid());
    
	do { 
		rcode = dvk_wait4bind_T(RETRY_US);
		PXYDEBUG("RPROXY: dvk_wait4bind_T  rcode=%d\n", rcode);
		if (rcode == EDVSTIMEDOUT) {
			PXYDEBUG("RPROXY: dvk_wait4bind_T TIMEOUT\n");
			continue ;
		}else if(rcode == NONE) { /* proxies have not endpoint */
			break;	
		} else if(rcode == -EINTR) {
			continue;
		}if( rcode < 0) 
			exit(EXIT_FAILURE);
	} while	(rcode < OK);
	
	posix_memalign( (void**) &p_header, getpagesize(), (sizeof(proxy_hdr_t)));
	if (p_header== NULL) {
    		perror(" p_header posix_memalign");
    		exit(1);
  	}

	posix_memalign( (void**) &p_payload, getpagesize(), (sizeof(proxy_payload_t)));
	if (p_payload== NULL) {
    		perror(" p_payload posix_memalign");
    		exit(1);
  	}

	posix_memalign( (void**) &p_pseudo, getpagesize(), (sizeof(proxy_hdr_t)));
	if (p_pseudo== NULL) {
    		perror(" p_pseudo posix_memalign");
    		exit(1);
  	}
	
	posix_memalign( (void**) &p_batch, getpagesize(), (sizeof(proxy_payload_t)));
	if (p_payload== NULL) {
    		perror("p_batch posix_memalign");
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
int  ps_send_header(proxy_hdr_t *ptr_hdr ) 
{
    int sent = 0;        // how many bytes we've sent
    int bytesleft;
    int n, total;
	char *p_ptr;

	bytesleft = sizeof(proxy_hdr_t); // how many bytes we have left to send
	total = bytesleft;
	PXYDEBUG("SPROXY: send header=%d \n", bytesleft);

    p_ptr = (char *) ptr_hdr;
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
int  ps_send_payload(proxy_hdr_t *ptr_hdr, proxy_payload_t *ptr_pay ) 
{
    int sent = 0;        // how many bytes we've sent
    int bytesleft;
    int n, total;
	char *p_ptr;

	if( ptr_hdr->c_len <= 0) ERROR_EXIT(EDVSINVAL);
	
	bytesleft =  ptr_hdr->c_len; // how many bytes we have left to send
	total = bytesleft;
	PXYDEBUG("SPROXY: send header=%d \n", bytesleft);

    p_ptr = (char *) ptr_pay;
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
int  ps_send_remote(proxy_hdr_t *ptr_hdr, proxy_payload_t *ptr_pay ) 
{
	int rcode;

	PXYDEBUG("SPROXY:" CMD_FORMAT,CMD_FIELDS(ptr_hdr));

	/* send the header */
	rcode =  ps_send_header(ptr_hdr);
	if ( rcode != OK)  ERROR_RETURN(rcode);

	if( ptr_hdr->c_len > 0) {
		PXYDEBUG("SPROXY: send payload len=%d\n", ptr_hdr->c_len );
		rcode =  ps_send_payload( ptr_hdr, ptr_pay);
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
	proxy_hdr_t *bat_vect;
    int rcode;
    message *m_ptr;
    int pid, ret;

    pid = getpid();
	
    while(1) {
		cmd_flag = 0;
		batch_nr = 0;		// count the number of batching commands in the buffer
			
		PXYDEBUG("SPROXY %d: Waiting a message\n", pid);
		ret = dvk_get2rmt(p_header, p_payload);            
		switch(ret) {
			case OK:
				break;
			case EDVSTIMEDOUT:
				PXYDEBUG("SPROXY: Sending HELLO \n");
				p_header->c_cmd = CMD_NONE;
				p_header->c_len = 0;
				p_header->c_rcode = 0;
				rcode =  ps_send_remote(p_header, p_payload);
				cmd_flag = 0;
				if (rcode == 0) smsg_ok++;
				else smsg_fail++;
				break;
			case EDVSNOTCONN:
				ERROR_RETURN(EDVSNOTCONN);
			default:
				ERROR_RETURN(ret);
				break;
		}
		if( ret == EDVSTIMEDOUT) continue;
		
		//------------------------ BATCHEABLE COMMAND -------------------------------
		PXYDEBUG("SPROXY: %d "HDR_FORMAT,pid, HDR_FIELDS(p_header)); 
		if(  (p_header->c_cmd != CMD_COPYIN_DATA )     // is a batcheable command ??
		  && (p_header->c_cmd != CMD_COPYOUT_DATA)){	// YESSSSS

			if ( (p_header->c_cmd == CMD_SEND_MSG) 
				||(p_header->c_cmd == CMD_SNDREC_MSG)
				||(p_header->c_cmd == CMD_REPLY_MSG)){
				m_ptr = &p_header->c_msg;
				PXYDEBUG("SPROXY: " MSG1_FORMAT,  MSG1_FIELDS(m_ptr));
			}
			// store original header into batched header 
			memcpy( (char*) p_header3, p_header, sizeof(proxy_hdr_t));
			bat_vect = (proxy_hdr_t*) p_batch;		
			do{
				PXYDEBUG("SPROXY %d: Getting more messages\n", pid);
				ret = dvk_get2rmt_T(p_header2, p_payload2, TIMEOUT_NOWAIT);            
				if( ret != OK) {
					switch(ret) {
						case EDVSTIMEDOUT:
						case EDVSAGAIN:
							ERROR_PRINT(ret); 
							break;
						case EDVSNOTCONN: 
							ERROR_RETURN(EDVSNOTCONN);
							break;
						default:
							ERROR_PRINT(ret);
							break;
					}
					break;
				} 
			
				if(  (p_header2->c_cmd == CMD_COPYIN_DATA )    // is a batcheable command ??
					|| (p_header2->c_cmd == CMD_COPYOUT_DATA)){// NOOOOOOO
					cmd_flag = CMD_PENDING;
					break;
				}
			
				// Get another command 
				memcpy( (char*) &bat_vect[batch_nr], (char*) p_header2, sizeof(proxy_hdr_t));
				batch_nr++;		
				PXYDEBUG("SPROXY: new BATCHED COMMAND batch_nr=%d\n", batch_nr);		

			}while ( batch_nr < MAXBATCMD);
		}
		
		if( batch_nr > 0) { 			// is batching in course??	
			PXYDEBUG("SPROXY: sending BATCHED COMMANDS batch_nr=%d\n", batch_nr);
			p_header3->c_flags = 0;
			SET_BIT(p_header3->c_flags, FLAG_BATCH_BIT);
			p_header3->c_batch_nr = batch_nr;
			p_header3->c_len = batch_nr * sizeof(proxy_hdr_t);
			rcode =  ps_send_remote(p_header3, p_batch);
			if (rcode == 0) smsg_ok++;
			else smsg_fail++;
			batch_nr = 0;		// count the number of batching commands in the buffer
		} else {
			rcode =  ps_send_remote(p_header, p_payload);
			if (rcode == 0) smsg_ok++;
			else smsg_fail++;		
		}

		// now, send the non batcheable command plus payload 
		if( cmd_flag == CMD_PENDING) { 
			rcode =  ps_send_remote(p_header2, p_payload2);
			cmd_flag = 0;
			if (rcode == 0) smsg_ok++;
			else smsg_fail++;
		}
	}

    /* never reached */
    exit(1);
}

/* ps_connect_to_remote: connects to the remote receiver */
int ps_connect_to_remote(void) 
{
    int port_no, rcode, i;
    char rmt_ipaddr[INET_ADDRSTRLEN+1];

    port_no = (BASE_PORT+local_nodeid);
	PXYDEBUG("SPROXY: for node %s running at port=%d\n", px.px_name, port_no);    

	// Connect to the server client	
    rmtserver_addr.sin_family = AF_INET;  
    rmtserver_addr.sin_port = htons(port_no);  

    rmthost = gethostbyname(px.px_name);
	if( rmthost == NULL) ERROR_EXIT(h_errno);
	for( i =0; rmthost->h_addr_list[i] != NULL; i++) {
		PXYDEBUG("SPROXY: remote host address %i: %s\n", 
			i, inet_ntoa( *( struct in_addr*)(rmthost->h_addr_list[i])));
	}

    if((inet_pton(AF_INET,inet_ntoa( *( struct in_addr*)(rmthost->h_addr_list[0])), (struct sockaddr*) &rmtserver_addr.sin_addr)) <= 0)
    	ERROR_RETURN(errno);

    inet_ntop(AF_INET, (struct sockaddr*) &rmtserver_addr.sin_addr, rmt_ipaddr, INET_ADDRSTRLEN);
	PXYDEBUG("SPROXY: for node %s running at  IP=%s\n", px.px_name, rmt_ipaddr);    

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

	PXYDEBUG("SPROXY: Initializing on PID:%d\n", getpid());
    
	do { 
		rcode = dvk_wait4bind_T(RETRY_US);
		PXYDEBUG("SPROXY: dvk_wait4bind_T  rcode=%d\n", rcode);
		if (rcode == EDVSTIMEDOUT) {
			PXYDEBUG("SPROXY: dvk_wait4bind_T TIMEOUT\n");
			continue ;
		}else if(rcode == NONE) { /* proxies have not endpoint */
			break;	
		} else if(rcode == -EINTR) {
			continue;
		} if( rcode < 0) 
			exit(EXIT_FAILURE);
	} while	(rcode < OK);
		
	posix_memalign( (void**) &p_header, getpagesize(), (sizeof(proxy_hdr_t)));
	if (p_header== NULL) {
    		perror("p_header posix_memalign");
    		exit(1);
  	}

	posix_memalign( (void**) &p_header2, getpagesize(), (sizeof(proxy_hdr_t)));
	if (p_header== NULL) {
    		perror("p_header2 posix_memalign");
    		exit(1);
  	}

	posix_memalign( (void**) &p_header3, getpagesize(), (sizeof(proxy_hdr_t)));
	if (p_header== NULL) {
    		perror("p_header2 posix_memalign");
    		exit(1);
  	}
	
	posix_memalign( (void**) &p_payload, getpagesize(), (sizeof(proxy_payload_t)));
	if (p_payload== NULL) {
    		perror("p_payload posix_memalign");
    		exit(1);
  	}

	posix_memalign( (void**) &p_payload2, getpagesize(), (sizeof(proxy_payload_t)));
	if (p_payload== NULL) {
    		perror("p_payload2 posix_memalign");
    		exit(1);
  	}
	
	posix_memalign( (void**) &p_batch, getpagesize(), (sizeof(proxy_payload_t)));
	if (p_payload== NULL) {
    		perror("p_batch posix_memalign");
    		exit(1);
  	}
	
    // Create server socket.
    if ( (sproxy_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
       	ERROR_EXIT(errno)
    }
	
	/* try to connect many times */
	batch_nr = 0;		// number of batching commands
	cmd_flag = 0;		// signals a command to be sent 
	while(1) {
		do {
			if (rcode < 0)
				PXYDEBUG("SPROXY: Could not connect to %d"
                    			" Sleeping for a while...\n", px.px_id);
			sleep(4);
			rcode = ps_connect_to_remote();
		} while (rcode != 0);
		
		rcode = dvk_proxy_conn(px.px_id, CONNECT_SPROXY);
		if(rcode){
			ERROR_EXIT(rcode);
		} 
		ps_start_serving();
	
		rcode = dvk_proxy_conn(px.px_id, DISCONNECT_SPROXY);
		if(rcode){
			ERROR_EXIT(rcode);
		} 	
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
    ret = dvk_proxies_bind(px.px_name, px.px_id, spid, rpid,MAXCOPYBUF);
    if( ret < 0) ERROR_EXIT(ret);
	
	px_ptr = &px;
	PXYDEBUG(PX_USR_FORMAT , PX_USR_FIELDS(px_ptr));
	PXYDEBUG("binded to (%d,%d)\n", spid, rpid);

	ret= dvk_node_up(px.px_name, px.px_id, px.px_id);	
	
   	wait(&status);
    wait(&status);
    exit(0);
}

