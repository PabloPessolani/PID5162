/****************************************************************/
/* 				LB_svrpxy							*/
/* LOAD BALANCER SERVER PROXIES 				 		*/
/****************************************************************/

#define _GNU_SOURCE     
#define _MULTI_THREADED
#include "lb_dvs.h"

 sess_entry_t *svr_Rproxy_2server(server_t *svr_ptr);

/*----------------------------------------------*/
/*----------------------------------------------*/
/*----------------------------------------------*/
/*      PROXY RECEIVER FUNCTIONS        */
/*----------------------------------------------*/
/*----------------------------------------------*/
/*----------------------------------------------*/
/*===========================================================================*
 *				   svr_Rproxy 				    					 *
 *===========================================================================*/
void *svr_Rproxy(void *arg)
{
	int svr_id = (int) arg;
   	server_t *svr_ptr;

	svr_ptr = &server_tab[svr_id];
	USRDEBUG("SERVER_RPROXY[%s]: Initializing...\n", svr_ptr->svr_name);
	svr_Rproxy_init(svr_ptr);
	svr_Rproxy_loop(svr_ptr);
	USRDEBUG("SERVER_RPROXY[%s]: Exiting...\n", svr_ptr->svr_name);
}	

void svr_Rproxy_init(server_t *svr_ptr) 
{
    int rcode;
	lbpx_desc_t *spx_ptr; 

	spx_ptr = &svr_ptr->svr_rpx;

	USRDEBUG("SERVER_RPROXY(%s): Initializing proxy receiver\n", svr_ptr->svr_name);
	
	rcode = posix_memalign( (void**) &spx_ptr->lbp_mqbuf, getpagesize(), (sizeof(msgq_buf_t)));
	if (rcode != 0) ERROR_EXIT(rcode);
	spx_ptr->lbp_header = &spx_ptr->lbp_mqbuf->mb_header;

	// Allocate buffer for payload 
	rcode = posix_memalign( (void**) &spx_ptr->lbp_payload, getpagesize(), MAXCOPYBUF);
	if (rcode != 0) ERROR_EXIT(rcode);
	USRDEBUG("SERVER_RPROXY(%s): Payload address=%p\n", svr_ptr->svr_name, spx_ptr->lbp_payload);

    if( svr_Rproxy_setup(svr_ptr) != OK) ERROR_EXIT(errno);

	return(OK);
}

int svr_Rproxy_setup(server_t *svr_ptr) 
{
    int rcode;
    int optval = 1;
	struct hostent *h_ptr;
	lbpx_desc_t *spx_ptr; 

	spx_ptr = &svr_ptr->svr_rpx;
	
    spx_ptr->lbp_port = (LBP_BASE_PORT+svr_ptr->svr_nodeid);
	USRDEBUG("SERVER_RPROXY(%s): for node %d running at port=%d\n", 
		svr_ptr->svr_name, svr_ptr->svr_nodeid, spx_ptr->lbp_port);

    // Create server socket.
    if ( (spx_ptr->lbp_lsd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
        ERROR_EXIT(errno);

    if( (rcode = setsockopt(spx_ptr->lbp_lsd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) < 0)
        	ERROR_EXIT(errno);

    // Bind (attach) this process to the server socket.
    spx_ptr->lbp_lclsvr_addr.sin_family = AF_INET;
	
	if( !strncmp(lb.lb_svrname,"ANY", 3)){
		spx_ptr->lbp_lclsvr_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}else{
		h_ptr = gethostbyname(lb.lb_svrname);
		if( h_ptr == NULL) ERROR_EXIT(h_errno);
		unsigned int i=0;
		while ( h_ptr->h_addr_list[i] != NULL) {
          USRDEBUG("SERVER_RPROXY(%s): %s\n",
			svr_ptr->svr_name,  inet_ntoa( *( struct in_addr*)( h_ptr->h_addr_list[i])));
          i++;
		}  
		USRDEBUG("SERVER_RPROXY(%s): svrname=%s listening on IP address=%s\n", 
			svr_ptr->svr_name, lb.lb_svrname, inet_ntoa( *( struct in_addr*)(h_ptr->h_addr)));		
		spx_ptr->lbp_lclsvr_addr.sin_addr.s_addr = inet_addr(inet_ntoa( *( struct in_addr*)(h_ptr->h_addr)));
	}
		
    spx_ptr->lbp_lclsvr_addr.sin_port = htons(spx_ptr->lbp_port);
	rcode = bind(spx_ptr->lbp_lsd, (struct sockaddr *) &spx_ptr->lbp_lclsvr_addr, 
					sizeof(spx_ptr->lbp_lclsvr_addr));
    if(rcode < 0) ERROR_EXIT(errno);

	USRDEBUG("SERVER_RPROXY(%s): is bound to port=%d socket=%d\n", 
		svr_ptr->svr_name, spx_ptr->lbp_port, spx_ptr->lbp_lsd);
		
// Turn 'rproxy_sd' to a listening socket. Listen queue size is 1.
	rcode = listen(spx_ptr->lbp_lsd, 0);
    if(rcode < 0) ERROR_EXIT(errno);

    return(OK);
   
}

void svr_Rproxy_loop(server_t *svr_ptr)
{
	int sender_addrlen;
    char ip4[INET_ADDRSTRLEN];  // space to hold the IPv4 string
    struct sockaddr_in sa;
    int rcode, dcid;
	client_t *clt_ptr;
	lbpx_desc_t *spx_ptr; 
	sess_entry_t *sess_ptr;

	spx_ptr = &svr_ptr->svr_rpx;
	
    while (1){
		do {
			sender_addrlen = sizeof(sa);
			USRDEBUG("SERVER_RPROXY(%s): Waiting for connection.\n",
				svr_ptr->svr_name);
    		spx_ptr->lbp_csd = accept(spx_ptr->lbp_lsd, (struct sockaddr *) &sa, &sender_addrlen);
    		if(spx_ptr->lbp_csd < 0) ERROR_PRINT(errno);
		}while(spx_ptr->lbp_csd < 0);

		USRDEBUG("SERVER_RPROXY(%s): [%s]. Getting remote command.\n",
           		svr_ptr->svr_name, inet_ntop(AF_INET, &(sa.sin_addr), ip4, INET_ADDRSTRLEN));

    	/* Serve Forever */
		do { 
	       	/* get a complete message and process it */
       		rcode = svr_Rproxy_getcmd(svr_ptr);
       		if (rcode == OK) {
				USRDEBUG("SERVER_RPROXY(%s):Message succesfully processed.\n",
					svr_ptr->svr_name);
				spx_ptr->lbp_msg_ok++;
				sess_ptr = svr_Rproxy_2server(svr_ptr);
				if( sess_ptr == NULL) {
					ERROR_PRINT(EDVSAGAIN);
					continue;
				}
				dcid = spx_ptr->lbp_header->c_dcid;
				MTX_LOCK(sess_table[dcid].st_mutex);
				clt_ptr = &client_tab[sess_ptr->se_clt_nodeid];	
				MTX_UNLOCK(sess_table[dcid].st_mutex);

				rcode 	= svr_Rproxy_cltmq(svr_ptr, clt_ptr, sess_ptr);
        	} else {
				USRDEBUG("SERVER_RPROXY(%s): Message processing failure [%d]\n",
					svr_ptr->svr_name,rcode);
            	spx_ptr->lbp_msg_err++;
				if( rcode == EDVSNOTCONN) break;
			}	
		}while(1);
		close(spx_ptr->lbp_csd);
   	}
    /* never reached */
}

sess_entry_t *svr_Rproxy_2server(server_t *svr_ptr)
{
	int rcode, i, max_sessions, dcid;
	int svr_nodeid, svr_endpoint;
	lbpx_desc_t *spx_ptr;
	proxy_hdr_t *hdr_ptr;
	client_t *clt_ptr;
	sess_entry_t *sess_ptr;
	service_t *svc_ptr;
	int raw_time;
    struct timespec cmd_ts = {0, 0};

	spx_ptr = &svr_ptr->svr_rpx;
	assert(spx_ptr->lbp_header != NULL);

	hdr_ptr = spx_ptr->lbp_header;
	dcid	= hdr_ptr->c_dcid;
	if( dcid < 0 || dcid >= NR_DCS){
		ERROR_PRINT(EDVSBADDCID);
		return(NULL);
	}
	
	max_sessions =(NR_PROCS-NR_SYS_PROCS);
	
	USRDEBUG("SERVER_RPROXY(%s): \n", svr_ptr->svr_name);
	sess_ptr = (sess_entry_t *)sess_table[dcid].st_tab_ptr;
	
	// check that src endpoint be a CLIENT ENDPOINT
	// and dst endpoint be a SERVER  ENDPOINT
	if(hdr_ptr->c_dst < (NR_SYS_PROCS-NR_TASKS)
	|| hdr_ptr->c_src < 0  
	|| hdr_ptr->c_src >= (NR_SYS_PROCS-NR_TASKS)) {
		rcode = svr_Rproxy_error(svr_ptr, EDVSENDPOINT);
		if( rcode < 0) ERROR_PRINT(rcode);
		return(NULL);
	}	
		
	// Search for an existing Session 
	MTX_LOCK(sess_table[dcid].st_mutex);
	for( i = 0; i < max_sessions; i++, sess_ptr++){
		if ( sess_ptr->se_clt_nodeid == LB_INVALID) {
			continue;
		}
#ifdef TEMPORARY
			USRDEBUG("XXX_SERVER_RPROXY(%s):" SESE_CLT_FORMAT, 
					svr_ptr->svr_name, SESE_CLT_FIELDS(sess_ptr));
			USRDEBUG("XXX_SERVER_RPROXY(%s):" SESE_SVR_FORMAT, 
					svr_ptr->svr_name, SESE_SVR_FIELDS(sess_ptr));
			USRDEBUG("XXX_SERVER_RPROXY(%s):" SESE_LB_FORMAT, 
					svr_ptr->svr_name, SESE_LB_FIELDS(sess_ptr));

			USRDEBUG("XXX_SERVER_RPROXY(%s):" CMD_FORMAT, 
					svr_ptr->svr_name, CMD_FIELDS(hdr_ptr));
			USRDEBUG("XXX_SERVER_RPROXY(%s):" CMD_PIDFORMAT, 
					svr_ptr->svr_name, CMD_PIDFIELDS(hdr_ptr));
#endif // TEMPORARY					
 
		// Search for an Active Session 
		if( (sess_ptr->se_svr_nodeid == hdr_ptr->c_snode) 
		&&  (sess_ptr->se_svr_ep     == hdr_ptr->c_src)  
		&&  (sess_ptr->se_lbsvr_ep	 == hdr_ptr->c_dst)	
		&&  (lb.lb_nodeid			 == hdr_ptr->c_dnode)){
			// ACTIVE SESSION FOUND 
			clt_ptr = &client_tab[sess_ptr->se_clt_nodeid];
			// the SERVER PID must be the same 
			if(sess_ptr->se_svr_PID != LB_INVALID) {
				if(sess_ptr->se_svr_PID != hdr_ptr->c_pid) {
					USRDEBUG("SERVER_RPROXY(%s): Expired Session Found se_svr_PID=%d c_pid=%d\n", 
						svr_ptr->svr_name, sess_ptr->se_svr_PID, hdr_ptr->c_pid);

#ifdef  REPORT_ERROR 					
					// Send ERROR To CLIENT
					// session locked at start of function MTX_LOCK
					if( !(hdr_ptr->c_cmd & MASK_ACKNOWLEDGE))
						hdr_ptr->c_cmd  |= MASK_ACKNOWLEDGE;
					hdr_ptr->c_snode = lb.lb_nodeid;
					hdr_ptr->c_src   = sess_ptr->se_lbclt_ep;
					hdr_ptr->c_dnode = sess_ptr->se_clt_nodeid;
					hdr_ptr->c_dst   = sess_ptr->se_clt_ep;
					hdr_ptr->c_len   = 0;
					hdr_ptr->c_rcode = EDVSDEADSRCDST;
					MTX_UNLOCK(sess_table[dcid].st_mutex);

					rcode 	= svr_Rproxy_cltmq(svr_ptr, clt_ptr, sess_ptr);
					if( rcode < 0) ERROR_PRINT(rcode);
					
					// Send ERROR To SERVER  
					MTX_LOCK(sess_table[dcid].st_mutex);
					hdr_ptr->c_src   = sess_ptr->se_lbsvr_ep;
					hdr_ptr->c_dnode = sess_ptr->se_svr_nodeid;
					hdr_ptr->c_dst   = sess_ptr->se_svr_ep;
					hdr_ptr->c_len   = 0;
					hdr_ptr->c_rcode = EDVSDEADSRCDST;
					MTX_UNLOCK(sess_table[dcid].st_mutex);

					rcode 	= clt_Rproxy_svrmq(clt_ptr, svr_ptr, sess_ptr);
					if( rcode < 0) ERROR_PRINT(rcode);
					MTX_LOCK(sess_table[dcid].st_mutex);
#endif //  REPORT_ERROR 	
					svc_ptr = sess_ptr->se_service;
					assert(svc_ptr != NULL);
					if( svc_ptr->svc_bind != PROG_BIND ){
#ifdef USE_SSHPASS												
						sprintf(sess_ptr->se_rmtcmd,	
							"sshpass -p \'root\' ssh root@%s "
							"-o UserKnownHostsFile=/dev/null "
							"-o StrictHostKeyChecking=no "
							"-o LogLevel=ERROR "
							" kill -s SIGKILL %d > /tmp/%s.out 2> /tmp/%s.err",
							svr_ptr->svr_name,								
							sess_ptr->se_svr_PID,
							svr_ptr->svr_name,								
							svr_ptr->svr_name								
						);
						USRDEBUG("SERVER_RPROXY(%s): se_rmtcmd=%s\n", 		
								svr_ptr->svr_name, sess_ptr->se_rmtcmd);
						rcode = system(sess_ptr->se_rmtcmd);
						if(rcode < 0){
							ERROR_PRINT(rcode);
							ERROR_PRINT(-errno);				
						}						
#else // USE_SSHPASS								
						sprintf(sess_ptr->se_rmtcmd,	
							"kill -9 %d > /tmp/%s.out 2> /tmp/%s.err",
							sess_ptr->se_svr_PID,
							svr_ptr->svr_name,								
							svr_ptr->svr_name
						);
						USRDEBUG("SERVER_RPROXY(%s): se_rmtcmd=%s\n", 		
								svr_ptr->svr_name, sess_ptr->se_rmtcmd);
						rcode = ucast_cmd(svr_ptr->svr_nodeid, 
											svr_ptr->svr_name, sess_ptr->se_rmtcmd);								
						if( rcode < 0) ERROR_PRINT(rcode);
						
						// Send to AGENT to wait until server process is unbound									
						rcode = ucast_wait4bind(MT_WAIT_UNBIND, svr_ptr->svr_nodeid, svr_ptr->svr_name,
										sess_ptr->se_dcid, sess_ptr->se_svr_ep, LB_TIMEOUT_5SEC);
						if( rcode < 0) ERROR_PRINT(rcode);
						
						// Wait for AGENT Notification or timeout 
						MTX_LOCK(svr_ptr->svr_tail_mtx);
						MTX_LOCK(svr_ptr->svr_agent_mtx);
						// insert the client proxy descriptor into server list 
						TAILQ_INSERT_TAIL(&svr_ptr->svr_svr_head,
										svr_ptr,
										svr_tail_entry);

						SET_BIT(svr_ptr->svr_bm_sts, SVR_WAIT_STOP);		// set bit in status bitmap that a client is waiting for stop  
						raw_time = clock_gettime(CLOCK_REALTIME, &cmd_ts);
						if (raw_time) ERROR_PRINT(raw_time);
						cmd_ts.tv_sec += LB_TIMEOUT_5SEC;				// wait start notification from agent
						COND_WAIT_T(rcode, svr_ptr->svr_agent_cond, svr_ptr->svr_agent_mtx, &cmd_ts);
						if(rcode != 0 ) ERROR_PRINT(rcode);
						
						if (svr_ptr->svr_svr_head.tqh_first != NULL){
							TAILQ_REMOVE(&svr_ptr->svr_svr_head, 
								svr_ptr->svr_svr_head.tqh_first, 
								svr_tail_entry);
							// only remove the bit from the bitmap if it was the last waiting client
							if (svr_ptr->svr_svr_head.tqh_first == NULL)
								CLR_BIT(svr_ptr->svr_bm_sts, CLT_WAIT_STOP);								
						}
						MTX_UNLOCK(svr_ptr->svr_agent_mtx);	
						MTX_UNLOCK(svr_ptr->svr_tail_mtx);
							
						
#endif// USE_SSHPASS								
					}					
					// Delete Session 
					sess_ptr->se_clt_nodeid = LB_INVALID;
					sess_ptr->se_clt_PID	= LB_INVALID;
					sess_ptr->se_svr_PID	= LB_INVALID;
					sess_ptr->se_service	= NULL;	
					// Clear service on that endpoint : The server PID has changed
					CLR_BIT(svr_ptr->svr_bm_svc, hdr_ptr->c_src); 
					sess_table[dcid].st_nr_sess--;
					////////////////////////////////////////////////
					// DEBERIA ENVIAR A LA COLA DE MENSAJES DEL
					// SERVER PROXY SENDER UN MENSAJE DE TIPO 
					//  cmd  | MASK_ACKNOWLEDGE
					//  CON CODIGO DE ERROR  EDVSDEADSRCDST
					////////////////////////////////////////////////
					break; 
				}
			} else {
				sess_ptr->se_svr_PID = hdr_ptr->c_pid;
			}

			MTX_UNLOCK(sess_table[dcid].st_mutex);
			USRDEBUG("SERVER_RPROXY(%s): Active Session Found with client %s\n", 
				svr_ptr->svr_name, clt_ptr->clt_name);
			USRDEBUG("SERVER_RPROXY(%s):" SESE_CLT_FORMAT, 
					svr_ptr->svr_name, SESE_CLT_FIELDS(sess_ptr));
			USRDEBUG("SERVER_RPROXY(%s):" SESE_SVR_FORMAT, 
					svr_ptr->svr_name, SESE_SVR_FIELDS(sess_ptr));
			USRDEBUG("SERVER_RPROXY(%s):" SESE_LB_FORMAT, 
					svr_ptr->svr_name, SESE_LB_FIELDS(sess_ptr));
			return(sess_ptr);
		}
	}
	MTX_UNLOCK(sess_table[dcid].st_mutex);
	ERROR_PRINT(EDVSCONNREFUSED);

	rcode = svr_Rproxy_error(svr_ptr, rcode);
	if ( rcode < 0) ERROR_PRINT(rcode);

	return(NULL);
}

int svr_Rproxy_error(server_t *svr_ptr, int errcode)
{
	int rcode, dst;
	lbpx_desc_t *spx_ptr;
	lbpx_desc_t *svr_send_ptr;
	
	spx_ptr 	= &svr_ptr->svr_rpx;
	svr_send_ptr= &svr_ptr->svr_spx;

	clock_gettime(clk_id, &spx_ptr->lbp_header->c_timestamp);
	USRDEBUG("SERVER_RPROXY(%s): Replying %d\n",svr_ptr->svr_name, errcode);
	spx_ptr->lbp_mqbuf->mb_type  = LBP_SVR2SVR;
	
	// if it is an ACKNOWLEDGE, ignore
	if (spx_ptr->lbp_header->c_cmd & MASK_ACKNOWLEDGE) 
		ERROR_RETURN(EDVSAGAIN);
	spx_ptr->lbp_header->c_cmd 	 = (spx_ptr->lbp_header->c_cmd | MASK_ACKNOWLEDGE);
	spx_ptr->lbp_header->c_rcode = errcode;
	spx_ptr->lbp_header->c_dnode = spx_ptr->lbp_header->c_snode;
	spx_ptr->lbp_header->c_snode = lb.lb_nodeid;
	dst 						 = spx_ptr->lbp_header->c_dst;
	spx_ptr->lbp_header->c_dst   = spx_ptr->lbp_header->c_src;
	spx_ptr->lbp_header->c_src   = dst;
	rcode = msgsnd(svr_send_ptr->lbp_mqid, (char*) spx_ptr->lbp_mqbuf, 
								sizeof(proxy_hdr_t), 0); 
	if(rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

int svr_Rproxy_getcmd(server_t *svr_ptr) 
{
    int rcode, pl_size, i, ret, raw_len;
	lbpx_desc_t *spx_ptr, *svr_send_ptr;
	
	spx_ptr = &svr_ptr->svr_rpx;
	
	USRDEBUG("SERVER_RPROXY(%s): \n", svr_ptr->svr_name);
	assert(spx_ptr->lbp_header != NULL);
    do {
		bzero(spx_ptr->lbp_header, sizeof(proxy_hdr_t));
		USRDEBUG("SERVER_RPROXY(%s): About to receive header\n",
			svr_ptr->svr_name);
    	rcode = svr_Rproxy_rcvhdr(svr_ptr);
    	if (rcode != 0) ERROR_RETURN(rcode);

		if( spx_ptr->lbp_header->c_cmd != CMD_NONE) {
			USRDEBUG("SERVER_RPROXY(%s): " CMD_FORMAT,
				svr_ptr->svr_name, CMD_FIELDS(spx_ptr->lbp_header));
			USRDEBUG("SERVER_RPROXY(%s): " CMD_XFORMAT,
				svr_ptr->svr_name, CMD_XFIELDS(spx_ptr->lbp_header));
			USRDEBUG("SERVER_RPROXY(%s): " CMD_TSFORMAT, 
				svr_ptr->svr_name, CMD_TSFIELDS(spx_ptr->lbp_header)); 
			USRDEBUG("SERVER_RPROXY(%s): "CMD_PIDFORMAT, 
				svr_ptr->svr_name, CMD_PIDFIELDS(spx_ptr->lbp_header)); 
		} else {
				USRDEBUG("SERVER_RPROXY(%s): NONE\n", svr_ptr->svr_name); 
				// reverse the node fields 
				//	spx_ptr->lbp_header->c_dnode = spx_ptr->lbp_header->c_snode;
				//	spx_ptr->lbp_header->c_snode = lb.lb_nodeid;
				clock_gettime(clk_id, &spx_ptr->lbp_header->c_timestamp);
				USRDEBUG("SERVER_RPROXY(%s): Replying NONE\n",svr_ptr->svr_name);
				spx_ptr->lbp_mqbuf->mb_type = LBP_SVR2SVR;
				svr_send_ptr = &svr_ptr->svr_spx; 						
				rcode = msgsnd(svr_send_ptr->lbp_mqid, (char*) spx_ptr->lbp_mqbuf, 
								sizeof(proxy_hdr_t), 0); 
				if( rcode < 0) ERROR_PRINT(-errno);
				continue;
		}
		
		switch(spx_ptr->lbp_header->c_cmd){
			case CMD_SEND_MSG:
			case CMD_SNDREC_MSG:
			case CMD_REPLY_MSG:
				break;
			case CMD_COPYIN_DATA:
			case CMD_COPYOUT_DATA:
			case CMD_COPYOUT_RQST:
			case CMD_COPYLCL_RQST:
			case CMD_COPYRMT_RQST:
			case CMD_COPYIN_RQST:
				USRDEBUG("SERVER_RPROXY(%s):" VCOPY_FORMAT, 
					svr_ptr->svr_name, VCOPY_FIELDS(spx_ptr->lbp_header)); 
				break;
		}
   	}while(spx_ptr->lbp_header->c_cmd == CMD_NONE);
    
	/* now we have a proxy header in the buffer. Cast it.*/
    pl_size = spx_ptr->lbp_header->c_len;
    
    /* payload could be zero */
    if(pl_size > 0) {
        bzero(spx_ptr->lbp_payload, pl_size);
        rcode = svr_Rproxy_rcvpay(svr_ptr);
        if (rcode != 0) ERROR_RETURN(rcode);
	}

	if( rcode < 0 ) ERROR_RETURN(rcode);
    return(OK);
}

int svr_Rproxy_rcvhdr(server_t *svr_ptr)
{
    int n, total,  received = 0;
    char *p_ptr;
	lbpx_desc_t *spx_ptr; 

	spx_ptr = &svr_ptr->svr_rpx;
	
   	p_ptr = (char*) spx_ptr->lbp_header;
	total = sizeof(proxy_hdr_t);
   	while ((n = recv(spx_ptr->lbp_csd, p_ptr, (total-received), 0 )) > 0) {
        received = received + n;
		USRDEBUG("SERVER_RPROXY (%s): n:%d | received:%d | HEADER_SIZE:%d\n", 
				svr_ptr->svr_name, n,received,sizeof(proxy_hdr_t));
        if (received >= sizeof(proxy_hdr_t)) {  
			USRDEBUG("SERVER_RPROXY (%s): " CMD_FORMAT,
				svr_ptr->svr_name, CMD_FIELDS(spx_ptr->lbp_header));
			USRDEBUG("SERVER_RPROXY (%s): " CMD_XFORMAT, 
				svr_ptr->svr_name, CMD_XFIELDS(spx_ptr->lbp_header));
        	return(OK);
        } else {
			USRDEBUG("SERVER_RPROXY (%s): Header partially received. There are %d bytes still to get\n", 
                  	svr_ptr->svr_name, sizeof(proxy_hdr_t) - received);
        	p_ptr += n;
        }
   	}
    if( n < 0)  ERROR_RETURN(-errno);
	return(OK);
}

int svr_Rproxy_rcvpay(server_t *svr_ptr)
{
    int n, pl_size ,received = 0;
    char *p_ptr;
	lbpx_desc_t *spx_ptr; 

	spx_ptr = &svr_ptr->svr_rpx;
	pl_size = spx_ptr->lbp_header->c_len;
   	USRDEBUG("SERVER_RPROXY (%s): pl_size=%d\n",svr_ptr->svr_name, pl_size);
   	p_ptr = (char*) spx_ptr->lbp_payload;
   	while ((n = recv(spx_ptr->lbp_csd, p_ptr, (pl_size-received), 0 )) > 0) {
        received = received + n;
		USRDEBUG("SERVER_RPROXY (%s): n:%d | received:%d\n",
			svr_ptr->svr_name, n,received);
        if (received >= pl_size) return(OK);
       	p_ptr += n;
   	}
    
    if(n < 0) ERROR_RETURN(-errno);
 	return(OK);
}

int svr_Rproxy_cltmq(server_t *svr_ptr, client_t *clt_ptr, sess_entry_t *sess_ptr)
{
    int rcode = 0, maxbytes, ret, cmd;
	long my_bytes, dcid;
	lbpx_desc_t *spx_ptr; 
	lbpx_desc_t *cpx_ptr; 
	proxy_hdr_t *hdr_ptr;
	proxy_payload_t *pay_ptr;
	message *m_ptr;

	spx_ptr = &svr_ptr->svr_rpx;
	cpx_ptr = &clt_ptr->clt_spx;
	maxbytes = sizeof(proxy_hdr_t);

	// modify SERVER RECEIVER header 
	hdr_ptr = spx_ptr->lbp_header;
	hdr_ptr->c_snode = lb.lb_nodeid;
	m_ptr   = &hdr_ptr->c_msg;
	dcid = spx_ptr->lbp_header->c_dcid;
	USRDEBUG("SERVER_RPROXY(%s) BEFORE: " CMD_FORMAT, svr_ptr->svr_name, CMD_FIELDS(hdr_ptr));
	USRDEBUG("SERVER_RPROXY(%s) BEFORE: " CMD_XFORMAT, svr_ptr->svr_name, CMD_XFIELDS(hdr_ptr));
	MTX_LOCK(sess_table[dcid].st_mutex);
	hdr_ptr->c_dnode = sess_ptr->se_clt_nodeid;
	
	// modify VCOPY subheader fields
	cmd = (hdr_ptr->c_cmd & ~MASK_ACKNOWLEDGE);
	if( cmd >= CMD_COPYIN_DATA && cmd <=CMD_COPYIN_RQST){
		USRDEBUG("SERVER_RPROXY(%s) VCOPY BEFORE: " VCOPY_FORMAT, 
			svr_ptr->svr_name, VCOPY_FIELDS(hdr_ptr));	
		// MODIFY THE VCOPY SOURCE
		if( hdr_ptr->c_vcopy.v_rqtr == hdr_ptr->c_src){
			hdr_ptr->c_vcopy.v_rqtr = sess_ptr->se_lbclt_ep; 
		} 
		if( hdr_ptr->c_vcopy.v_src == hdr_ptr->c_src){
			hdr_ptr->c_vcopy.v_src = sess_ptr->se_lbclt_ep; 
		} 
		if( hdr_ptr->c_vcopy.v_dst == hdr_ptr->c_src){
			hdr_ptr->c_vcopy.v_dst = sess_ptr->se_lbclt_ep; 
		} 
		// MODIFY THE VCOPY DESTINATION
		if( hdr_ptr->c_vcopy.v_rqtr == hdr_ptr->c_dst){
			hdr_ptr->c_vcopy.v_rqtr = sess_ptr->se_clt_ep; 
		} 
		if( hdr_ptr->c_vcopy.v_src == hdr_ptr->c_dst){
			hdr_ptr->c_vcopy.v_src = sess_ptr->se_clt_ep; 
		} 
		if( hdr_ptr->c_vcopy.v_dst == hdr_ptr->c_dst){
			hdr_ptr->c_vcopy.v_dst = sess_ptr->se_clt_ep; 
		} 
		
		USRDEBUG("SERVER_RPROXY(%s) VCOPY AFTER: " VCOPY_FORMAT, 
			svr_ptr->svr_name, VCOPY_FIELDS(hdr_ptr));		
	}
	m_ptr->m_source = sess_ptr->se_lbclt_ep;

	// MODIFY THE HEADER SOURCE AND DESTINATION 
	hdr_ptr->c_src   = sess_ptr->se_lbclt_ep;
	hdr_ptr->c_dst   = sess_ptr->se_clt_ep;
	
	if( hdr_ptr->c_len > 0)
		hdr_ptr->c_pl_ptr	= (unsigned long) spx_ptr->lbp_payload; //  changed field  c_pl_ptr is really c_snd_seq 
	MTX_UNLOCK(sess_table[dcid].st_mutex);

	USRDEBUG("SERVER_RPROXY(%s) ARTER : " CMD_FORMAT, svr_ptr->svr_name, CMD_FIELDS(hdr_ptr));
	USRDEBUG("SERVER_RPROXY(%s) ARTER : " CMD_XFORMAT, svr_ptr->svr_name, CMD_XFIELDS(hdr_ptr));
	
	USRDEBUG("SERVER_RPROXY(%s): sending HEADER %d bytes to %s from mqid=%d\n", 
		svr_ptr->svr_name,  sizeof(proxy_hdr_t), clt_ptr->clt_name, cpx_ptr->lbp_mqid);
	spx_ptr->lbp_mqbuf->mb_type = LBP_SVR2CLT;	
	rcode = msgsnd(cpx_ptr->lbp_mqid, (char*) spx_ptr->lbp_mqbuf,  sizeof(proxy_hdr_t), 0); 

	if( hdr_ptr->c_len > 0){
		USRDEBUG("SERVER_RPROXY(%s): getting new Payload buffer\n", svr_ptr->svr_name);
		// Allocate a NEW buffer for payload 
		ret = posix_memalign( (void**) &spx_ptr->lbp_payload, getpagesize(), MAXCOPYBUF);
		if (ret != 0) ERROR_EXIT(ret);
		USRDEBUG("SERVER_RPROXY(%s): Payload address=%p\n", clt_ptr->clt_name, spx_ptr->lbp_payload);
	}

	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);

}

/*----------------------------------------------*/
/*----------------------------------------------*/
/*----------------------------------------------*/
/*      PROXY SENDER FUNCTIONS          */
/*----------------------------------------------*/
/*----------------------------------------------*/
/*----------------------------------------------*/

/*===========================================================================*
 *				   svr_Sproxy 				    					 *
 *===========================================================================*/
void *svr_Sproxy(void *arg)
{
	int svr_id = (int) arg;
   	server_t *svr_ptr;
	
	USRDEBUG("SERVER_SPROXY[%d]: Initializing...\n", svr_id);
	svr_ptr = &server_tab[svr_id];
	svr_Sproxy_init(svr_ptr);
	svr_Sproxy_loop(svr_ptr);
	USRDEBUG("SERVER_SPROXY[%d]: Exiting...\n", svr_id);
}	

void  svr_Sproxy_init(	server_t *svr_ptr) 
{
    int rcode = 0;
	int optval;

	lbpx_desc_t *spx_ptr; 
	spx_ptr = &svr_ptr->svr_spx;

	USRDEBUG("SERVER_SPROXY(%s): Initializing proxy sender\n", svr_ptr->svr_name);

	rcode = posix_memalign( (void**) &spx_ptr->lbp_mqbuf, getpagesize(), (sizeof(msgq_buf_t)));
	if (rcode != 0) ERROR_EXIT(rcode);

	spx_ptr->lbp_header  = &spx_ptr->lbp_mqbuf->mb_header;
	spx_ptr->lbp_payload = NULL;

    // Create server socket.
    if ( (spx_ptr->lbp_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
       	ERROR_EXIT(errno);
    }

	// set SO_REUSEADDR on a socket to true (1):
	optval = 1;
	rcode = setsockopt(spx_ptr->lbp_sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (rcode < 0) ERROR_EXIT(errno);

	// bind a socket to a device name (might not work on all systems):
	rcode = setsockopt(spx_ptr->lbp_sd, SOL_SOCKET, SO_BINDTODEVICE, lb.lb_svrdev, strlen(lb.lb_svrdev));
	if (rcode < 0) ERROR_EXIT(errno);	
}

void  svr_Sproxy_loop(server_t *svr_ptr)  
{
    int rcode = 0;
	lbpx_desc_t *spx_ptr; 
	spx_ptr = &svr_ptr->svr_spx;

	while(1) {
		do {
			if (rcode < 0)
				USRDEBUG("SERVER_SPROXY(%s): Could not connect. "
                    			"Sleeping for a while...\n",svr_ptr->svr_name);
			sleep(LB_TIMEOUT_5SEC);
			rcode = svr_Sproxy_connect(svr_ptr);
		} while (rcode != 0);
		
		rcode = svr_Sproxy_serving(svr_ptr);
		ERROR_PRINT(rcode);
		close(spx_ptr->lbp_sd);
	}
   /* code never reaches here */
}

void  svr_Sproxy_connect(server_t *svr_ptr)  
{
    int rcode, i;
    char rmt_ipaddr[INET_ADDRSTRLEN+1];
	lbpx_desc_t *spx_ptr; 

	spx_ptr = &svr_ptr->svr_spx;

    spx_ptr->lbp_port = (LBP_BASE_PORT+lb.lb_nodeid);
	USRDEBUG("SERVER_SPROXY(%s): remote host port=%d\n", svr_ptr->svr_name, spx_ptr->lbp_port);    

	// Connect to the server client	
    spx_ptr->lbp_rmtsvr_addr.sin_family = AF_INET;  
    spx_ptr->lbp_rmtsvr_addr.sin_port 	= htons(spx_ptr->lbp_port);  
    spx_ptr->lbp_rmthost = gethostbyname(svr_ptr->svr_name);
	USRDEBUG("SERVER_SPROXY(%s): rmthost %s IP %s\n", svr_ptr->svr_name, 
			spx_ptr->lbp_rmthost->h_name, 
			inet_ntoa(*(struct in_addr*)spx_ptr->lbp_rmthost->h_addr));   
	
//	if( spx_ptr->lbp_rmthost == NULL) ERROR_EXIT(h_errno);
//	for( i =0; spx_ptr->lbp_rmthost->h_addr_list[i] != NULL; i++) {
//		USRDEBUG("SERVER_SPROXY(%s): remote host address %i: %s\n", svr_ptr->svr_name,
//			i, inet_ntoa( *( struct in_addr*)(spx_ptr->lbp_rmthost->h_addr_list[i])));
//	}

//    if((inet_pton(AF_INET,inet_ntoa( *( struct in_addr*)(spx_ptr->lbp_rmthost->h_addr_list[0])), 
//		(struct sockaddr*) &spx_ptr->lbp_rmtsvr_addr.sin_addr)) <= 0)
//     	ERROR_RETURN(-errno);

    if((inet_pton(AF_INET,inet_ntoa( *( struct in_addr*)(spx_ptr->lbp_rmthost->h_addr)), 
		(struct sockaddr*) &spx_ptr->lbp_rmtsvr_addr.sin_addr)) <= 0)
    	ERROR_RETURN(-errno);
		
    inet_ntop(AF_INET, (struct sockaddr*) &spx_ptr->lbp_rmtsvr_addr.sin_addr, rmt_ipaddr, INET_ADDRSTRLEN);
	USRDEBUG("SERVER_SPROXY(%s): running at IP=%s\n", svr_ptr->svr_name, rmt_ipaddr);    

	rcode = connect(spx_ptr->lbp_sd, (struct sockaddr *) &spx_ptr->lbp_rmtsvr_addr, 
				sizeof(spx_ptr->lbp_rmtsvr_addr));
    if (rcode != 0) ERROR_RETURN(-errno);
	USRDEBUG("SERVER_SPROXY(%s): connected to IP=%s on socket=%d\n", 
		svr_ptr->svr_name, rmt_ipaddr, spx_ptr->lbp_sd);    
    return(OK);
}

void  svr_Sproxy_serving(server_t *svr_ptr) 
{
    int rcode = 0;
	lbpx_desc_t *spx_ptr; 

	spx_ptr = &svr_ptr->svr_spx;

    while(1) {
		USRDEBUG("SERVER_SPROXY(%s): Reading message queue..\n", svr_ptr->svr_name);
		
		// Get message from message queue  
		rcode = svr_Sproxy_mqrcv(svr_ptr);
		if( rcode < 0) continue;
		assert(spx_ptr->lbp_header != NULL);
		USRDEBUG("SERVER_SPROXY(%s): " CMD_FORMAT ,svr_ptr->svr_name, 
			CMD_FIELDS(spx_ptr->lbp_header)); 
		USRDEBUG("SERVER_SPROXY(%s): " CMD_XFORMAT,svr_ptr->svr_name, 
			CMD_XFIELDS(spx_ptr->lbp_header)); 
	
#ifdef PROXY_TIMESTAMP	
		timestamp_opt = 1;
		if( timestamp_opt){
			clock_gettime(clk_id, &spx_ptr->lbp_header->c_timestamp);
			USRDEBUG("SERVER_SPROXY(%s): " CMD_TSFORMAT, svr_ptr->svr_name, 
				CMD_TSFIELDS(spx_ptr->lbp_header)); 
		}
#endif // PROXY_TIMESTAMP	

		rcode =  svr_Sproxy_send(svr_ptr);
	}
}

void  svr_Sproxy_send(server_t *svr_ptr) 
{
    int rcode = 0;
	lbpx_desc_t *spx_ptr; 
	char *p_ptr;

	spx_ptr = &svr_ptr->svr_spx;

	USRDEBUG("SERVER_SPROXY(%s): \n", svr_ptr->svr_name);

	/* send the header */
    p_ptr = (char *)spx_ptr->lbp_header->c_pl_ptr; // Get the pointer from header 
	rcode =  svr_Sproxy_sndhdr(svr_ptr);
	if ( rcode < 0) {
		if( spx_ptr->lbp_header->c_len > 0)
			FREE(p_ptr);		
		ERROR_RETURN(rcode);
	}
	
	if( spx_ptr->lbp_header->c_len > 0) {
		USRDEBUG("SERVER_SPROXY(%s): send payload len=%d\n", 
			svr_ptr->svr_name, spx_ptr->lbp_header->c_len);
		rcode =  svr_Sproxy_sndpay(svr_ptr);
		// Free the Client Proxy input buffer for payload  
		FREE(p_ptr);
		if ( rcode < OK)  ERROR_RETURN(rcode);
	}

    return(OK);
}
	
void  svr_Sproxy_sndhdr(server_t *svr_ptr) 
{
    int rcode = 0;
	int sent = 0;        // how many bytes we've sent
    int bytesleft;
    int n, total;
	char *p_ptr;
	lbpx_desc_t *spx_ptr;

	spx_ptr = &svr_ptr->svr_spx;

	bytesleft = sizeof(proxy_hdr_t); // how many bytes we have left to send
	total = bytesleft;
	USRDEBUG("SERVER_SPROXY(%s): send header=%d \n", svr_ptr->svr_name, bytesleft);

    p_ptr = (char *) spx_ptr->lbp_header;
	USRDEBUG("SERVER_SPROXY(%s): " CMD_FORMAT, svr_ptr->svr_name, 
		CMD_FIELDS(spx_ptr->lbp_header));
	USRDEBUG("SERVER_SPROXY(%s): " CMD_XFORMAT, svr_ptr->svr_name, 
		CMD_XFIELDS(spx_ptr->lbp_header));

    while(sent < total) {
        n = send(spx_ptr->lbp_sd, p_ptr, bytesleft, 0);
        if (n < 0) {
			ERROR_PRINT(n);
			if(errno == EALREADY) {
				ERROR_PRINT(errno);
				sleep(1);
				continue;
			}else{
				ERROR_RETURN(-errno);
			}
		}
        sent += n;
		p_ptr += n; 
        bytesleft -= n;
    }
	USRDEBUG("SERVER_SPROXY(%s): sent header=%d \n", svr_ptr->svr_name, total);
    return(OK);

}	

int  svr_Sproxy_sndpay(server_t *svr_ptr) 
{
    int rcode = 0;
    int sent = 0;        // how many bytes we've sent
    int bytesleft;
    int n, total;
	char *p_ptr;
	lbpx_desc_t *spx_ptr;
	proxy_hdr_t *hdr_ptr;

	spx_ptr = &svr_ptr->svr_spx;

	assert( spx_ptr->lbp_header->c_len > 0);
	
	bytesleft =  spx_ptr->lbp_header->c_len; // how many bytes we have left to send
	total = bytesleft;
	USRDEBUG("SERVER_SPROXY(%s): svr_Sproxy_sndpay bytesleft=%d \n", 
		svr_ptr->svr_name, bytesleft);

    hdr_ptr = spx_ptr->lbp_header;
	USRDEBUG("SERVER_SPROXY(%s): " VCOPY_FORMAT, 
			svr_ptr->svr_name, VCOPY_FIELDS(hdr_ptr));
	
    p_ptr = (char *)spx_ptr->lbp_header->c_pl_ptr; // Get the pointer from header 
    while(sent < total) {
        n = send(spx_ptr->lbp_sd, p_ptr, bytesleft, 0);
		USRDEBUG("SERVER_SPROXY(%s): payload sent=%d \n",svr_ptr->svr_name, n);
        if (n < 0) {
			if(errno == EALREADY) {
				ERROR_PRINT(errno);
				sleep(1);
				continue;
			}else{
				ERROR_RETURN(-errno);
			}
		}
        sent += n;
		p_ptr += n; 
        bytesleft -= n;
    }
	USRDEBUG("SERVER_SPROXY(%s): sent payload=%d \n", svr_ptr->svr_name, total);
    return(OK);
}	

int svr_Sproxy_mqrcv(server_t *svr_ptr)
{
    int rcode = 0;
	lbpx_desc_t *spx_ptr;

	spx_ptr = &svr_ptr->svr_spx;
	USRDEBUG("SERVER_SPROXY(%s): reading from mqid=%d\n", 
		svr_ptr->svr_name, spx_ptr->lbp_mqid);
	rcode = msgrcv(spx_ptr->lbp_mqid, spx_ptr->lbp_mqbuf, sizeof(proxy_hdr_t), 0, 0 );
	if( rcode < 0) ERROR_RETURN(-errno);
	USRDEBUG("SERVER_SPROXY(%s): %d bytes received\n", 
			svr_ptr->svr_name, rcode);
	return(rcode);
}


