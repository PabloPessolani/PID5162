/****************************************************************/
/* 				LB_cltpxy							*/
/* LOAD BALANCER CLIENT PROXIES 				 		*/
/****************************************************************/

#define _GNU_SOURCE     
#define _MULTI_THREADED
#include "lb_dvs.h"

sess_entry_t *clt_Rproxy_2server(client_t *clt_ptr, service_t *svc_ptr);
int clt_Rproxy_svrmq(client_t *clt_ptr, server_t *svr_ptr, sess_entry_t *sess_ptr);
int clt_Rproxy_error(client_t *clt_ptr, int errcode); 
server_t *select_server(client_t *clt_ptr, 
						sess_entry_t *sess_ptr, 
						service_t *svc_ptr,
						proxy_hdr_t *hdr_ptr);
						
/*----------------------------------------------*/
/*----------------------------------------------*/
/*----------------------------------------------*/
/*      PROXY RECEIVER FUNCTIONS        */
/*----------------------------------------------*/
/*----------------------------------------------*/
/*----------------------------------------------*/



/*===========================================================================*
 *				   clt_Rproxy 				    					 *
 *===========================================================================*/
void *clt_Rproxy(void *arg)
{
	int clt_id = (int) arg;
   	client_t *clt_ptr;

	clt_ptr = &client_tab[clt_id];
	USRDEBUG("CLIENT_RPROXY[%s]: Initializing...\n", clt_ptr->clt_name);
	clt_Rproxy_init(clt_ptr);
	clt_Rproxy_loop(clt_ptr);
	USRDEBUG("CLIENT_RPROXY[%s]: Exiting...\n", clt_ptr->clt_name);
}	

void clt_Rproxy_init(client_t *clt_ptr) 
{
    int rcode;
	lbpx_desc_t *cpx_ptr; 

	cpx_ptr = &clt_ptr->clt_rpx;

	USRDEBUG("CLIENT_RPROXY(%s): Initializing proxy receiver\n", clt_ptr->clt_name);
	
	// Allocate buffer for msgq message (type + header)
	rcode = posix_memalign( (void**) &cpx_ptr->lbp_mqbuf, getpagesize(), (sizeof(msgq_buf_t)));
	if (rcode != 0) ERROR_EXIT(rcode);
	cpx_ptr->lbp_header = &cpx_ptr->lbp_mqbuf->mb_header;

	// Allocate buffer for payload 
	rcode = posix_memalign( (void**) &cpx_ptr->lbp_payload, getpagesize(), MAXCOPYBUF);
	if (rcode != 0) ERROR_EXIT(rcode);
	USRDEBUG("CLIENT_RPROXY(%s): Payload address=%p\n", clt_ptr->clt_name, cpx_ptr->lbp_payload);
	
    if( clt_Rproxy_setup(clt_ptr) != OK) ERROR_EXIT(errno);
	return(OK);
}

int clt_Rproxy_setup(client_t *clt_ptr) 
{
    int rcode;
    int optval = 1;
	lbpx_desc_t *cpx_ptr; 
	struct hostent *h_ptr;

	cpx_ptr = &clt_ptr->clt_rpx;
	
    cpx_ptr->lbp_port = (LBP_BASE_PORT+clt_ptr->clt_nodeid);
	USRDEBUG("CLIENT_RPROXY(%s): for node %d running at port=%d\n", 
		clt_ptr->clt_name, clt_ptr->clt_nodeid, cpx_ptr->lbp_port);

    // Create server socket.
    if ( (cpx_ptr->lbp_lsd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
        ERROR_EXIT(errno);

    if( (rcode = setsockopt(cpx_ptr->lbp_lsd, SOL_SOCKET, SO_REUSEADDR, 
							&optval, sizeof(optval))) < 0)
        	ERROR_EXIT(errno);

    // Bind (attach) this process to the server socket.
    cpx_ptr->lbp_rmtclt_addr.sin_family = AF_INET;
	
	if( !strncmp(lb.lb_cltname,"ANY", 3)){
		cpx_ptr->lbp_rmtclt_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}else{
		h_ptr = gethostbyname(lb.lb_cltname);
		if( h_ptr == NULL) ERROR_EXIT(h_errno);
		unsigned int i=0;
		while ( h_ptr->h_addr_list[i] != NULL) {
          USRDEBUG("CLIENT_RPROXY(%s): %s\n",
			clt_ptr->clt_name,  inet_ntoa( *( struct in_addr*)( h_ptr->h_addr_list[i])));
          i++;
		}  
		USRDEBUG("CLIENT_RPROXY(%s): cltname=%s listening on IP address=%s\n", 
			clt_ptr->clt_name, lb.lb_cltname, inet_ntoa( *( struct in_addr*)(h_ptr->h_addr)));		
		cpx_ptr->lbp_rmtclt_addr.sin_addr.s_addr = inet_addr(inet_ntoa( *( struct in_addr*)(h_ptr->h_addr)));
	}
		
    cpx_ptr->lbp_rmtclt_addr.sin_port = htons(cpx_ptr->lbp_port);
	rcode = bind(cpx_ptr->lbp_lsd, (struct sockaddr *) &cpx_ptr->lbp_rmtclt_addr, 
					sizeof(cpx_ptr->lbp_rmtclt_addr));
    if(rcode < 0) ERROR_EXIT(errno);

	USRDEBUG("CLIENT_RPROXY(%s): is bound to port=%d socket=%d\n", 
		clt_ptr->clt_name, cpx_ptr->lbp_port, cpx_ptr->lbp_lsd);
		
// Turn 'rproxy_sd' to a listening socket. Listen queue size is 1.
	rcode = listen(cpx_ptr->lbp_lsd, 0);
    if(rcode < 0) ERROR_EXIT(errno);

    return(OK);
   
}

void clt_Rproxy_loop(client_t *clt_ptr)
{
	int sender_addrlen, i, ep;
    char ip4[INET_ADDRSTRLEN];  // space to hold the IPv4 string
    struct sockaddr_in sa;
    int rcode, dcid;
	server_t *svr_ptr;
	lbpx_desc_t *cpx_ptr;
	sess_entry_t *sess_ptr;
	service_t *svc_ptr;
	

	cpx_ptr = &clt_ptr->clt_rpx;
	
    while (1){
		do {
			sender_addrlen = sizeof(sa);
			USRDEBUG("CLIENT_RPROXY(%s): Waiting for connection.\n",
				clt_ptr->clt_name);
    		cpx_ptr->lbp_csd = accept(cpx_ptr->lbp_lsd, (struct sockaddr *) &sa, &sender_addrlen);
    		if(cpx_ptr->lbp_csd < 0) ERROR_PRINT(errno);
		}while(cpx_ptr->lbp_csd < 0);

    	/* Serve Forever */
		do { 
			USRDEBUG("CLIENT_RPROXY(%s): [%s]. Getting remote command.\n",
           		clt_ptr->clt_name, inet_ntop(AF_INET, &(sa.sin_addr), ip4, INET_ADDRSTRLEN));
	       	/* get a complete proxy message and process it */
       		rcode = clt_Rproxy_getcmd(clt_ptr);
       		if (rcode == OK) {
				USRDEBUG("CLIENT_RPROXY(%s):Message succesfully processed.\n",
					clt_ptr->clt_name);
				cpx_ptr->lbp_msg_ok++;
				
				// Search for a valid service 
				for(i = 0; i < lb.lb_nr_services; i++){
					svc_ptr = &service_tab[i];
					USRDEBUG("CLIENT_RPROXY(%s): " SERVICE_FORMAT, 
						clt_ptr->clt_name, SERVICE_FIELDS(svc_ptr));	
					// check for matching DCID
					if( (svc_ptr->svc_dcid == LB_INVALID) // It means all DCIDs 
					||  (svc_ptr->svc_dcid == cpx_ptr->lbp_header->c_dcid)) { 
						// check if there is a service which listen in the destination endpoint
						
						USRDEBUG("CLIENT_RPROXY(%s): extep=%d minep=%d  maxep=%d c_dst=%d\n",
							clt_ptr->clt_name, svc_ptr->svc_extep, 
							svc_ptr->svc_minep, svc_ptr->svc_maxep,
							cpx_ptr->lbp_header->c_dst);	
#define SINGLE_ENDPOINT												
#ifdef SINGLE_ENDPOINT						

				
						// Service NOT configured endpoint
//						if( TEST_BIT(svr_ptr->svr_bm_svc, cpx_ptr->lbp_header->c_dst) == 0) continue;
						if(cpx_ptr->lbp_header->c_dst == svc_ptr->svc_extep) break;
						
						// OUT of Service endpoint range ?	
						if( (cpx_ptr->lbp_header->c_dst >= svc_ptr->svc_minep)
						&&  (cpx_ptr->lbp_header->c_dst <= svc_ptr->svc_maxep)) break;
#else // SINGLE_ENDPOINT								
						// Search a free endpoint inside Service endpont range 
						for( ep = svc_ptr->svc_minep; ep <= svc_ptr->svc_maxep; ep++){
							USRDEBUG("CLIENT_RPROXY(%s): ep=%d\n", clt_ptr->clt_name, ep);	
							if(ep == cpx_ptr->lbp_header->c_dst) break;
						}
						if( ep > svc_ptr->svc_maxep) continue;
						USRDEBUG("CLIENT_RPROXY(%s): service endpoint found ep=%d\n",clt_ptr->clt_name,ep);						
						break;		
#endif // SINGLE_ENDPOINT						
					}
				}
				// invalid destination endpoint -> send EDVSDSTDIED
				if( i == lb.lb_nr_services){
					rcode = clt_Rproxy_error(clt_ptr, EDVSDSTDIED);
					if( rcode < 0) ERROR_PRINT(-errno);
					continue;
				}
			
				// search for a existing session or create a new one 
				sess_ptr = clt_Rproxy_2server(clt_ptr, svc_ptr);
				if( sess_ptr == NULL) {
					ERROR_PRINT(EDVSAGAIN);
					continue;
				}

				dcid = cpx_ptr->lbp_header->c_dcid;
				MTX_LOCK(sess_table[dcid].st_mutex);
				svr_ptr = &server_tab[sess_ptr->se_svr_nodeid];
				MTX_UNLOCK(sess_table[dcid].st_mutex);

				rcode 	= clt_Rproxy_svrmq(clt_ptr, svr_ptr, sess_ptr);
				if( rcode < 0) ERROR_PRINT(rcode);
        	} else {
				USRDEBUG("CLIENT_RPROXY(%s): Message processing failure [%d]\n",
					clt_ptr->clt_name,rcode);
            	cpx_ptr->lbp_msg_err++;
				if( rcode == EDVSNOTCONN) break;
			}	
		}while(1);
		close(cpx_ptr->lbp_csd);
   	}
    /* never reached */
}

int clt_Rproxy_error(client_t *clt_ptr, int errcode)
{
	int rcode;
	lbpx_desc_t *cpx_ptr;
	lbpx_desc_t *clt_send_ptr;
	
	cpx_ptr 	= &clt_ptr->clt_rpx;
	clt_send_ptr= &clt_ptr->clt_spx;

	clock_gettime(clk_id, &cpx_ptr->lbp_header->c_timestamp);
	USRDEBUG("CLIENT_RPROXY(%s): Replying %d\n",clt_ptr->clt_name, errcode);
	cpx_ptr->lbp_mqbuf->mb_type  = LBP_CLT2CLT;
	cpx_ptr->lbp_header->c_cmd 	 = (cpx_ptr->lbp_header->c_cmd | MASK_ACKNOWLEDGE);
	cpx_ptr->lbp_header->c_rcode = errcode;
	cpx_ptr->lbp_header->c_dnode = cpx_ptr->lbp_header->c_snode;
	cpx_ptr->lbp_header->c_snode = lb.lb_nodeid;
	rcode 						 = cpx_ptr->lbp_header->c_dst;
	cpx_ptr->lbp_header->c_dst   = cpx_ptr->lbp_header->c_src;
	cpx_ptr->lbp_header->c_src   = rcode;
	rcode = msgsnd(clt_send_ptr->lbp_mqid, (char*) cpx_ptr->lbp_mqbuf, 
								sizeof(proxy_hdr_t), 0); 
	if(rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}
								
sess_entry_t *clt_Rproxy_2server(client_t *clt_ptr, service_t *svc_ptr)
{
	int rcode, i, max_sessions, dcid, new_ep;
	lbpx_desc_t *cpx_ptr;
	proxy_hdr_t *hdr_ptr;
	server_t *svr_ptr;
	sess_entry_t *sess_ptr;
	int raw_time;
    struct timespec cmd_ts = {0, 0};

	cpx_ptr = &clt_ptr->clt_rpx;
	assert(cpx_ptr->lbp_header != NULL);

	hdr_ptr = cpx_ptr->lbp_header;
	dcid	= hdr_ptr->c_dcid;
	if( dcid < 0 || dcid >= NR_DCS){
		ERROR_PRINT(EDVSBADDCID);
		return(NULL);
	}
	
	max_sessions =(NR_PROCS-NR_SYS_PROCS);
	
	USRDEBUG("CLIENT_RPROXY(%s): \n", clt_ptr->clt_name);
	sess_ptr = (sess_entry_t *)sess_table[dcid].st_tab_ptr;
	MTX_LOCK(sess_table[dcid].st_mutex);
	for( i = 0; i < max_sessions; i++, sess_ptr++){
//		MTX_LOCK(sess_table[dcid].st_mutex);
		if ( sess_ptr->se_clt_nodeid == LB_INVALID){
//			MTX_UNLOCK(sess_table[dcid].st_mutex);
			continue;
		}
		USRDEBUG("CLIENT_RPROXY(%s) se_clt_ep=%d c_src=%d\n", 
				clt_ptr->clt_name, sess_ptr->se_clt_ep, hdr_ptr->c_src);
		if (sess_ptr->se_clt_ep == hdr_ptr->c_src) {
			USRDEBUG("CLIENT_RPROXY(%s): ACTIVE CLIENT ENDPOINT FOUND se_clt_ep=%d \n", 
				clt_ptr->clt_name, sess_ptr->se_clt_ep);
			// Search for an Active Session 
			if( (sess_ptr->se_clt_nodeid == hdr_ptr->c_snode)
			&&  (sess_ptr->se_lbclt_ep	 == hdr_ptr->c_dst)
			&&  (lb.lb_nodeid			 == hdr_ptr->c_dnode)){
				// ACTIVE SESSION FOUND 
				USRDEBUG("CLIENT_RPROXY(%s): SESSION FOUND se_clt_ep=%d \n",
					clt_ptr->clt_name, sess_ptr->se_clt_ep);
				svr_ptr = &server_tab[sess_ptr->se_svr_nodeid];
				MTX_LOCK(svr_ptr->svr_mutex);
				// the CLIENT PID must be the same 
				if(sess_ptr->se_clt_PID != hdr_ptr->c_pid) {
					USRDEBUG("CLIENT_RPROXY(%s): Expired Session Found se_clt_PID=%d c_pid=%d\n", 
						clt_ptr->clt_name, sess_ptr->se_clt_PID, hdr_ptr->c_pid);
					// Create a new Session 	
					if( svc_ptr->svc_bind != PROG_BIND ){
#ifdef USE_SSHPASS								
						sprintf(sess_ptr->se_rmtcmd,	
							"sshpass -p \'root\' ssh root@%s "
							"-o UserKnownHostsFile=/dev/null "
							"-o StrictHostKeyChecking=no "
							"-o LogLevel=ERROR "
							" kill -s SIGKILL %d >> /tmp/%s.out 2>> /tmp/%s.err",
							svr_ptr->svr_name,								
							sess_ptr->se_svr_PID,
							svr_ptr->svr_name,								
							svr_ptr->svr_name							
						);
						rcode = system(sess_ptr->se_rmtcmd);	
						USRDEBUG("CLIENT_RPROXY(%s): se_rmtcmd=%s\n", 		
								clt_ptr->clt_name, sess_ptr->se_rmtcmd);
						if(rcode < 0){
							ERROR_PRINT(rcode);
							ERROR_PRINT(-errno);				
						}
#else // USE_SSHPASS								
						sprintf(sess_ptr->se_rmtcmd,	
							" kill -9 %d >> /tmp/%s.out 2>> /tmp/%s.err",
							sess_ptr->se_svr_PID,
							svr_ptr->svr_name,								
							svr_ptr->svr_name							
						);
						USRDEBUG("CLIENT_RPROXY(%s): se_rmtcmd=%s\n", 		
								clt_ptr->clt_name, sess_ptr->se_rmtcmd);
						rcode = ucast_cmd(svr_ptr->svr_nodeid, 
											svr_ptr->svr_name, sess_ptr->se_rmtcmd);
						if( rcode < 0) ERROR_PRINT(rcode);	
#endif // USE_SSHPASS								
						// Send to AGENT to wait until server process is unbound									
						rcode = ucast_wait4bind(MT_CLT_WAIT_UNBIND, (char *) clt_ptr, 
										svr_ptr->svr_nodeid, svr_ptr->svr_name,
										sess_ptr->se_dcid, sess_ptr->se_svr_ep, LB_TIMEOUT_5SEC);
						if( rcode < 0) ERROR_PRINT(rcode);
						
						// Wait for AGENT Notification or timeout 
//						MTX_LOCK(svr_ptr->svr_tail_mtx);
						MTX_LOCK(clt_ptr->clt_agent_mtx);
						// insert the client proxy descriptor into server list 
//						TAILQ_INSERT_TAIL(&svr_ptr->svr_clt_head,
//										clt_ptr,
//										clt_tail_entry);
//						SET_BIT(svr_ptr->svr_bm_sts, CLT_WAIT_STOP);		// set bit in status bitmap that a client is waiting for stop  
						raw_time = clock_gettime(CLOCK_REALTIME, &cmd_ts);
						if (raw_time) ERROR_PRINT(raw_time);
						cmd_ts.tv_sec += LB_TIMEOUT_5SEC;				// wait start notification from agent
						COND_WAIT_T(rcode, clt_ptr->clt_agent_cond, clt_ptr->clt_agent_mtx, &cmd_ts);
						if(rcode != 0 ) ERROR_PRINT(rcode);
//						if (svr_ptr->svr_clt_head.tqh_first != NULL){
//							TAILQ_REMOVE(&svr_ptr->svr_clt_head, 
//								svr_ptr->svr_clt_head.tqh_first, 
//								clt_tail_entry);
							// only remove the bit from the bitmap if it was the last waiting client
//							if (svr_ptr->svr_clt_head.tqh_first == NULL)
//								CLR_BIT(svr_ptr->svr_bm_sts, CLT_WAIT_STOP);								
//						}
						MTX_UNLOCK(clt_ptr->clt_agent_mtx);	
//						MTX_UNLOCK(svr_ptr->svr_tail_mtx);			
					}
					// Delete Session 
					sess_ptr->se_clt_nodeid = LB_INVALID;
					sess_ptr->se_clt_PID	= LB_INVALID;
					sess_ptr->se_svr_PID	= LB_INVALID;
					sess_ptr->se_service	= NULL;	
					sess_table[dcid].st_nr_sess--;
					CLR_BIT(svr_ptr->svr_bm_svc, sess_ptr->se_svr_ep);
					MTX_UNLOCK(svr_ptr->svr_mutex);
	//				MTX_UNLOCK(sess_table[dcid].st_mutex);
					break;
				}
				USRDEBUG("CLIENT_RPROXY(%s): Active Session Found with server %s\n", 
					clt_ptr->clt_name, svr_ptr->svr_name);
				USRDEBUG("CLIENT_RPROXY(%s):" SESE_CLT_FORMAT, 
						clt_ptr->clt_name, SESE_CLT_FIELDS(sess_ptr));
				USRDEBUG("CLIENT_RPROXY(%s):" SESE_SVR_FORMAT, 
						clt_ptr->clt_name, SESE_SVR_FIELDS(sess_ptr));
				USRDEBUG("CLIENT_RPROXY(%s):" SESE_LB_FORMAT, 
						clt_ptr->clt_name, SESE_LB_FIELDS(sess_ptr));			
				MTX_UNLOCK(svr_ptr->svr_mutex);	
				MTX_UNLOCK(sess_table[dcid].st_mutex);
				return(sess_ptr);
			}
//			MTX_UNLOCK(sess_table[dcid].st_mutex);
		} 
#ifdef ANULADO
		else {

			// Same destination endpoint from the same source node ??
			if( (sess_ptr->se_clt_nodeid == hdr_ptr->c_snode)
			&&  (sess_ptr->se_lbclt_ep	 == hdr_ptr->c_dst) ){
				rcode = clt_Rproxy_error(clt_ptr, EDVSRSCBUSY);
				MTX_UNLOCK(sess_table[dcid].st_mutex);
				return(NULL);
			}
		}
#endif // ANULADO
	}
	MTX_UNLOCK(sess_table[dcid].st_mutex);

	// check that src endpoint be a CLIENT ENDPOINT
	if( hdr_ptr->c_src < (NR_SYS_PROCS-NR_TASKS)
	||  hdr_ptr->c_dst < 0  
	||  hdr_ptr->c_dst >= (NR_SYS_PROCS-NR_TASKS)) {
		rcode = clt_Rproxy_error(clt_ptr, EDVSENDPOINT);
		if( rcode < 0) ERROR_PRINT(EDVSENDPOINT);
		return(NULL);
	}			
	
	// NEW SESSION ------------------------------------ 

	// Search for a FREE session entry 
	sess_ptr = (sess_entry_t *)sess_table[dcid].st_tab_ptr;
	MTX_LOCK(sess_table[dcid].st_mutex);
	for( i = 0; i < max_sessions; i++, sess_ptr++){
		if ( sess_ptr->se_clt_nodeid == LB_INVALID) break;
	}
	if( i == max_sessions){
		rcode = clt_Rproxy_error(clt_ptr, EDVSNOSPC);
		if( rcode < 0) ERROR_PRINT(EDVSNOSPC);		
		MTX_UNLOCK(sess_table[dcid].st_mutex);
		return(NULL);
	}
	
	// sess_ptr LOCKED 
	svr_ptr = select_server(clt_ptr, sess_ptr, svc_ptr, hdr_ptr);
	if( svr_ptr == NULL) {
		rcode = clt_Rproxy_error(clt_ptr, EDVSAGAIN);
		if( rcode < 0) 	ERROR_PRINT(rcode);
		MTX_UNLOCK(sess_table[dcid].st_mutex);		
		return(NULL);
	}
	

		
	sess_ptr->se_dcid 		= hdr_ptr->c_dcid;
	sess_ptr->se_clt_nodeid	= hdr_ptr->c_snode;
	sess_ptr->se_clt_ep		= hdr_ptr->c_src;
	sess_ptr->se_clt_PID	= hdr_ptr->c_pid;
	
	sess_ptr->se_lbclt_ep	= hdr_ptr->c_dst;
	sess_ptr->se_lbsvr_ep	= hdr_ptr->c_src; 	// could be different
	
//	new_ep = svc_ptr->svc_minep; // could be any from minep to maxep
//	sess_ptr->se_svr_ep		= new_ep; 
	
	sess_ptr->se_svr_nodeid	= svr_ptr->svr_nodeid;	
	sess_ptr->se_svr_PID	= LB_INVALID; 		
	sess_ptr->se_service 	= svc_ptr; 			
	sess_table[dcid].st_nr_sess++;

	USRDEBUG("CLIENT_RPROXY(%s): NEW Session with server %s\n", 
		clt_ptr->clt_name, svr_ptr->svr_name);
	USRDEBUG("CLIENT_RPROXY(%s):" SESE_CLT_FORMAT, 
			clt_ptr->clt_name, SESE_CLT_FIELDS(sess_ptr));
	USRDEBUG("CLIENT_RPROXY(%s):" SESE_SVR_FORMAT, 
			clt_ptr->clt_name, SESE_SVR_FIELDS(sess_ptr));
	USRDEBUG("CLIENT_RPROXY(%s):" SESE_LB_FORMAT, 
			clt_ptr->clt_name, SESE_LB_FIELDS(sess_ptr));

	MTX_UNLOCK(sess_table[dcid].st_mutex);
	
	return(sess_ptr);
}

// CHOOSE the first server with a load STATUS <  SATURATED 
server_t *select_server(client_t *clt_ptr, 
						sess_entry_t *sess_ptr, 
						service_t *svc_ptr,
						proxy_hdr_t *hdr_ptr)
{
	int i, new_ep, rcode;
	server_t *svr_ptr;
	int raw_time;
	lb_t* lb_ptr;
    struct timespec cmd_ts = {0, 0};

	lb_ptr = &lb;
	MTX_LOCK(lb_ptr->lb_mtx);				
	for( i = 0; i < dvs_ptr->d_nr_nodes; i++){
		// TEMPORARY : CHOOSE THE FIRST INITIALIZED SERVER 
		if( i == lb_ptr->lb_nodeid) continue;
		if( TEST_BIT(lb_ptr->lb_bm_init,i) ) {
//		if( svr_ptr->svr_nodeid != LB_INVALID) {
			svr_ptr = &server_tab[i]; 
			MTX_LOCK(svr_ptr->svr_mutex);
			// If server is SATURATED get the next
			if( svr_ptr->svr_level == LVL_SATURATED) {
				MTX_UNLOCK(svr_ptr->svr_mutex);
				ERROR_PRINT(EDVSBUSY);
				continue;
			}
			// If server is not Initialized 
//			if( TEST_BIT(lb_ptr->lb_bm_init, i) == 0) {
//				MTX_UNLOCK(svr_ptr->svr_mutex);
//				continue;
//			}
			
			// check COMPRESSION and BATCHING in both ends
			USRDEBUG("CLIENT_RPROXY(%s): " SERVER1_FORMAT, 
							clt_ptr->clt_name, SERVER1_FIELDS(svr_ptr));
			USRDEBUG("CLIENT_RPROXY(%s): " CLIENT_FORMAT, 
							clt_ptr->clt_name, CLIENT_FIELDS(clt_ptr));
			if( svr_ptr->svr_compress != clt_ptr->clt_compress 
			||  svr_ptr->svr_batch    != clt_ptr->clt_batch) {
				rcode = clt_Rproxy_error(clt_ptr, EDVSBADPROXY);
				if( rcode < 0) 	ERROR_PRINT(EDVSBADPROXY);
				continue;
			}
	
			if( svc_ptr->svc_bind == PROG_BIND) {
				MTX_UNLOCK(svr_ptr->svr_mutex);
				new_ep = hdr_ptr->c_dst; 
				break;
			}

			// ALLOCATE FREE SERVER ENDPOINT		
			for ( new_ep = svc_ptr->svc_minep; 
				  new_ep <= svc_ptr->svc_maxep; new_ep++){

				USRDEBUG("CLIENT_RPROXY(%s): svr_bm_svc=%0lX bit=%d\n", 	
						clt_ptr->clt_name, svr_ptr->svr_bm_svc, new_ep);					  
					  
				if( TEST_BIT(svr_ptr->svr_bm_svc, new_ep) == 0) {
					SET_BIT(svr_ptr->svr_bm_svc, new_ep);
					if( svc_ptr->svc_bind != PROG_BIND 
					&&	strncmp(svc_ptr->svc_prog,nonprog, strlen(nonprog)) != 0 ){
						// sshpass [-ffilename|-dnum|-ppassword|-e] [options] command arguments
						// run_server.sh <dcid> <svr_ep> <clt_node> <clt_ep>"
						// sshpass -p 'root' ssh root@node1 -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=ERROR "bash -c \"
#ifdef USE_SSHPASS						
						sprintf(sess_ptr->se_rmtcmd, "sshpass -p \'root\' ssh root@%s "
													  "-o UserKnownHostsFile=/dev/null "
													  "-o StrictHostKeyChecking=no "
													  "-o LogLevel=ERROR "
													  " %s %d %d %d %d >> /tmp/%s.out 2>> /tmp/%s.err",
									svr_ptr->svr_name,				  
									svc_ptr->svc_prog, 								
									svc_ptr->svc_dcid, 
									new_ep,
									lb_ptr->lb_nodeid, 
									hdr_ptr->c_src,
									svr_ptr->svr_name,				  
									svr_ptr->svr_name				  
									);
						USRDEBUG("CLIENT_RPROXY(%s): se_rmtcmd=%s\n", 		
								clt_ptr->clt_name, sess_ptr->se_rmtcmd);
						rcode = system(sess_ptr->se_rmtcmd);
						if(rcode < 0){
							CLR_BIT(svr_ptr->svr_bm_svc, new_ep);
							ERROR_PRINT(rcode);
							ERROR_PRINT(-errno);				
						}
#else //  USE_SSHPASS	
						sprintf(sess_ptr->se_rmtcmd, "%s %d %d %d %d >> /tmp/%s.out 2>> /tmp/%s.err", 
									svc_ptr->svc_prog, 								
									svc_ptr->svc_dcid, 
									new_ep,
									lb_ptr->lb_nodeid, 
									hdr_ptr->c_src,
									svr_ptr->svr_name,				  
									svr_ptr->svr_name				  
									);
						USRDEBUG("CLIENT_RPROXY(%s): se_rmtcmd=%s\n", 		
								clt_ptr->clt_name, sess_ptr->se_rmtcmd);
						rcode = ucast_cmd(svr_ptr->svr_nodeid, 
											svr_ptr->svr_name, sess_ptr->se_rmtcmd);			
						if( rcode < 0) {
							CLR_BIT(svr_ptr->svr_bm_svc, new_ep);
							ERROR_PRINT(rcode);
						}
#endif  //  USE_SSHPASS													
						// Send to AGENT to wait until server process is bound									
						rcode = ucast_wait4bind(MT_CLT_WAIT_BIND, (char *) clt_ptr, 
										svr_ptr->svr_nodeid, svr_ptr->svr_name,
										sess_ptr->se_dcid, new_ep, LB_TIMEOUT_5SEC);
						if( rcode < 0) ERROR_PRINT(rcode);
						// Wait for AGENT Notification or timeout 
						MTX_LOCK(clt_ptr->clt_agent_mtx);
						raw_time = clock_gettime(CLOCK_REALTIME, &cmd_ts);
						if (raw_time) ERROR_PRINT(raw_time);
						cmd_ts.tv_sec += LB_TIMEOUT_5SEC;
						COND_WAIT_T(rcode, clt_ptr->clt_agent_cond, clt_ptr->clt_agent_mtx, &cmd_ts);
						MTX_UNLOCK(clt_ptr->clt_agent_mtx);						
						if(rcode < 0 ){
							ERROR_PRINT(rcode);
							MTX_UNLOCK(svr_ptr->svr_mutex);
							MTX_UNLOCK(lb_ptr->lb_mtx);				
							return(NULL);
						}
					}
					break;
				}
			}
			if( new_ep <= svc_ptr->svc_maxep){
				MTX_UNLOCK(svr_ptr->svr_mutex);
				break;
			}
			MTX_UNLOCK(svr_ptr->svr_mutex);
		}
	}
	USRDEBUG("CLIENT_RPROXY(%s): i=%d d_nr_nodes=%d new_ep=%d\n", 		
				clt_ptr->clt_name, i,  dvs_ptr->d_nr_nodes, new_ep);
								
	// Is a non SATURATED SERVER RUNNING server found?
	if ((i == dvs_ptr->d_nr_nodes)
	&&  (new_ep > svc_ptr->svc_maxep)) { 
		// Are there any defined node to start ??
		USRDEBUG("CLIENT_RPROXY(%s): lb_nr_init=%d lb_nr_svrpxy=%d\n", 		
								clt_ptr->clt_name, lb_ptr->lb_nr_init,lb_ptr->lb_nr_svrpxy);
		if( lb_ptr->lb_nr_init < lb_ptr->lb_nr_svrpxy) {
			///////////////////////////////////////////////////////////////////
			// HERE, START A VM WITH A NEW NODE 
			///////////////////////////////////////////////////////////////////
			start_new_node(sess_ptr->se_rmtcmd);
			ERROR_PRINT(EDVSAGAIN);
		}else {
			ERROR_PRINT(EDVSNOSPC);
		}
		MTX_UNLOCK(lb_ptr->lb_mtx);				
		return(NULL);	
	} 
	MTX_UNLOCK(lb_ptr->lb_mtx);				

	USRDEBUG("CLIENT_RPROXY(%s): server name=%s new_ep=%d\n", 		
								clt_ptr->clt_name, svr_ptr->svr_name, new_ep);
	sess_ptr->se_svr_ep	= new_ep;
	return(svr_ptr);
}

int clt_Rproxy_getcmd(client_t *clt_ptr) 
{
    int rcode, pl_size, bytes;
	lbpx_desc_t *cpx_ptr, *clt_send_ptr; 

	cpx_ptr = &clt_ptr->clt_rpx;
	
	USRDEBUG("CLIENT_RPROXY(%s): \n", clt_ptr->clt_name);
	assert(cpx_ptr->lbp_header != NULL);
    do {
		bzero(cpx_ptr->lbp_header, sizeof(proxy_hdr_t));
		USRDEBUG("CLIENT_RPROXY(%s): About to receive header\n",
			clt_ptr->clt_name);
    	bytes = clt_Rproxy_rcvhdr(clt_ptr);
		USRDEBUG("CLIENT_RPROXY(%s): header bytes=%d\n",clt_ptr->clt_name, bytes); 
    	if (bytes < 0) ERROR_RETURN(bytes);
		if ( bytes == 0) {
			cpx_ptr->lbp_header->c_cmd == CMD_NONE;
			continue;
		}
		if( cpx_ptr->lbp_header->c_cmd != CMD_NONE) {
			USRDEBUG("CLIENT_RPROXY(%s): " CMD_FORMAT,
				clt_ptr->clt_name, CMD_FIELDS(cpx_ptr->lbp_header));
			USRDEBUG("CLIENT_RPROXY(%s): " CMD_XFORMAT,
				clt_ptr->clt_name, CMD_XFIELDS(cpx_ptr->lbp_header));
			USRDEBUG("CLIENT_RPROXY(%s): " CMD_TSFORMAT, 
				clt_ptr->clt_name, CMD_TSFIELDS(cpx_ptr->lbp_header)); 
			USRDEBUG("CLIENT_RPROXY(%s): "CMD_PIDFORMAT, 
				clt_ptr->clt_name, CMD_PIDFIELDS(cpx_ptr->lbp_header)); 
		} else {
				USRDEBUG("CLIENT_RPROXY(%s): NONE\n", clt_ptr->clt_name);
				// reverse the node fields 
				//	cpx_ptr->lbp_header->c_dnode = cpx_ptr->lbp_header->c_snode;
				//	cpx_ptr->lbp_header->c_snode = lb.lb_nodeid;
				clock_gettime(clk_id, &cpx_ptr->lbp_header->c_timestamp);
				USRDEBUG("CLIENT_RPROXY(%s): Replying NONE\n",clt_ptr->clt_name);
				cpx_ptr->lbp_mqbuf->mb_type = LBP_CLT2CLT;
				clt_send_ptr = &clt_ptr->clt_spx; 		
				rcode = msgsnd(clt_send_ptr->lbp_mqid, (char*) cpx_ptr->lbp_mqbuf, 
								sizeof(proxy_hdr_t), 0); 
				if( rcode < 0) ERROR_PRINT(-errno);
				continue;
		}
		
		switch(cpx_ptr->lbp_header->c_cmd){
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
				USRDEBUG("CLIENT_RPROXY(%s):" VCOPY_FORMAT, 
					clt_ptr->clt_name, VCOPY_FIELDS(cpx_ptr->lbp_header)); 
				break;
			default:
				break;
		}
   	}while(cpx_ptr->lbp_header->c_cmd == CMD_NONE);
    
	/* now we have a proxy header in the buffer. Cast it.*/
    pl_size = cpx_ptr->lbp_header->c_len;
    
    /* payload could be zero */
    if(pl_size > 0) {
		assert(cpx_ptr->lbp_payload != NULL);
        bzero(cpx_ptr->lbp_payload, pl_size);
        bytes = clt_Rproxy_rcvpay(clt_ptr);
		USRDEBUG("CLIENT_RPROXY(%s): payload bytes=%d\n",clt_ptr->clt_name, bytes); 
        if (bytes < 0) ERROR_RETURN(bytes);
		return(OK);
	}
    return(OK);
}

int clt_Rproxy_rcvhdr(client_t *clt_ptr)
{
    int n, total,  received = 0;
    char *p_ptr;
	lbpx_desc_t *cpx_ptr; 

	cpx_ptr = &clt_ptr->clt_rpx;
	USRDEBUG("CLIENT_RPROXY(%s): lbp_csd=%d \n", 
			clt_ptr->clt_name, cpx_ptr->lbp_csd);
   	p_ptr = (char*) cpx_ptr->lbp_header;
	assert(p_ptr != NULL);
	total = sizeof(proxy_hdr_t);
   	while ((n = recv(cpx_ptr->lbp_csd, p_ptr, (total-received), 0 )) > 0) {
        received = received + n;
		USRDEBUG("CLIENT_RPROXY (%s): n:%d | received:%d | HEADER_SIZE:%d\n", 
				clt_ptr->clt_name, n,received,sizeof(proxy_hdr_t));
        if (received >= sizeof(proxy_hdr_t)) {  
			USRDEBUG("CLIENT_RPROXY (%s): " CMD_FORMAT,
				clt_ptr->clt_name, CMD_FIELDS(cpx_ptr->lbp_header));
			USRDEBUG("CLIENT_RPROXY (%s): " CMD_XFORMAT, 
				clt_ptr->clt_name, CMD_XFIELDS(cpx_ptr->lbp_header));
        	return(received);
        } else {
			USRDEBUG("CLIENT_RPROXY (%s): Header partially received. There are %d bytes still to get\n", 
                  	clt_ptr->clt_name, sizeof(proxy_hdr_t) - received);
        	p_ptr += n;
        }
   	}
    if( n < 0)  ERROR_RETURN(-errno);
	return(received);
}

int clt_Rproxy_rcvpay(client_t *clt_ptr)
{
    int n, pl_size ,received = 0;
    char *p_ptr;
	lbpx_desc_t *cpx_ptr; 

	cpx_ptr = &clt_ptr->clt_rpx;
	pl_size = cpx_ptr->lbp_header->c_len;
   	USRDEBUG("CLIENT_RPROXY (%s): pl_size=%d\n",clt_ptr->clt_name, pl_size);
   	p_ptr = (char*) cpx_ptr->lbp_payload;
   	while ((n = recv(cpx_ptr->lbp_csd, p_ptr, (pl_size-received), 0 )) > 0) {
        received = received + n;
		USRDEBUG("CLIENT_RPROXY (%s): n:%d | received:%d\n",
			clt_ptr->clt_name, n,received);
        if (received >= pl_size) return(received);
       	p_ptr += n;
   	}
    
    if(n < 0) ERROR_RETURN(-errno);
 	return(received);
}

int clt_Rproxy_svrmq(client_t *clt_ptr, server_t *svr_ptr, 
					sess_entry_t *sess_ptr)
{
    int rcode = 0, svr_ep, ret, cmd, dcid;
	lbpx_desc_t *cpx_ptr; 
	lbpx_desc_t *spx_ptr; 
	proxy_hdr_t *hdr_ptr;
	proxy_payload_t *pay_ptr;
	message *m_ptr;
	lb_t* lb_ptr;
	
	lb_ptr = &lb;	
	cpx_ptr = &clt_ptr->clt_rpx;
	spx_ptr = &svr_ptr->svr_spx;

	
	// Modify CLIENT RECEIVER header 
	hdr_ptr = cpx_ptr->lbp_header;
	hdr_ptr->c_snode = lb_ptr->lb_nodeid;
	USRDEBUG("CLIENT_RPROXY(%s) BEFORE: " CMD_FORMAT, clt_ptr->clt_name, CMD_FIELDS(hdr_ptr));
	USRDEBUG("CLIENT_RPROXY(%s) BEFORE: " CMD_XFORMAT, clt_ptr->clt_name, CMD_XFIELDS(hdr_ptr));
	
	m_ptr   = &hdr_ptr->c_msg;
	dcid = spx_ptr->lbp_header->c_dcid;
	MTX_LOCK(sess_table[dcid].st_mutex);
	hdr_ptr->c_dnode = sess_ptr->se_svr_nodeid;
	
	// modify VCOPY subheader fields
	cmd = (hdr_ptr->c_cmd & ~MASK_ACKNOWLEDGE);
	if( cmd >= CMD_COPYIN_DATA && cmd <= CMD_COPYIN_RQST){
		USRDEBUG("CLIENT_RPROXY(%s) VCOPY BEFORE: " VCOPY_FORMAT, 
			clt_ptr->clt_name, VCOPY_FIELDS(hdr_ptr));
		// MODIFY THE VCOPY DESTINATION
		if( hdr_ptr->c_vcopy.v_rqtr == hdr_ptr->c_dst){
			hdr_ptr->c_vcopy.v_rqtr = sess_ptr->se_svr_ep; 
		} 
		if( hdr_ptr->c_vcopy.v_src == hdr_ptr->c_dst){
			hdr_ptr->c_vcopy.v_src = sess_ptr->se_svr_ep; 
		} 
		if( hdr_ptr->c_vcopy.v_dst == hdr_ptr->c_dst){
			hdr_ptr->c_vcopy.v_dst = sess_ptr->se_svr_ep; 
		}
		// MODIFY THE VCOPY SOURCE
		if( hdr_ptr->c_vcopy.v_rqtr == hdr_ptr->c_src){
			hdr_ptr->c_vcopy.v_rqtr = sess_ptr->se_lbsvr_ep; 
		} 
		if( hdr_ptr->c_vcopy.v_src == hdr_ptr->c_src){
			hdr_ptr->c_vcopy.v_src = sess_ptr->se_lbsvr_ep; 
		} 
		if( hdr_ptr->c_vcopy.v_dst == hdr_ptr->c_src){
			hdr_ptr->c_vcopy.v_dst = sess_ptr->se_lbsvr_ep; 
		}		
		USRDEBUG("CLIENT_RPROXY(%s) VCOPY AFTER: " VCOPY_FORMAT, 
			clt_ptr->clt_name, VCOPY_FIELDS(hdr_ptr));		
	} 
	m_ptr->m_source = sess_ptr->se_lbsvr_ep;
	hdr_ptr->c_src = sess_ptr->se_lbsvr_ep;
	hdr_ptr->c_dst = sess_ptr->se_svr_ep; 

	if( hdr_ptr->c_len > 0)
		hdr_ptr->c_pl_ptr	= (unsigned long) cpx_ptr->lbp_payload; //  changed field  c_pl_ptr is really c_snd_seq 
	MTX_UNLOCK(sess_table[dcid].st_mutex);
	// End Modify 
	
	USRDEBUG("CLIENT_RPROXY(%s) AFTER: " CMD_FORMAT, clt_ptr->clt_name, CMD_FIELDS(hdr_ptr));
	USRDEBUG("CLIENT_RPROXY(%s) AFTER: " CMD_XFORMAT, clt_ptr->clt_name, CMD_XFIELDS(hdr_ptr));

	USRDEBUG("CLIENT_RPROXY(%s): sending HEADER %d bytes to server %s by mqid=%d\n", 
		clt_ptr->clt_name, sizeof(proxy_hdr_t), svr_ptr->svr_name, spx_ptr->lbp_mqid);
	cpx_ptr->lbp_mqbuf->mb_type = LBP_CLT2SVR;	
	rcode = msgsnd(spx_ptr->lbp_mqid, (char*) cpx_ptr->lbp_mqbuf, sizeof(proxy_hdr_t), 0); 
	if( hdr_ptr->c_len > 0){
		USRDEBUG("CLIENT_RPROXY(%s): getting new Payload buffer\n", clt_ptr->clt_name);
		// Allocate a NEW buffer for payload 
		ret = posix_memalign( (void**) &cpx_ptr->lbp_payload, getpagesize(), MAXCOPYBUF);
		if (ret != 0) ERROR_EXIT(ret);
		USRDEBUG("CLIENT_RPROXY(%s): Payload address=%p\n", clt_ptr->clt_name, cpx_ptr->lbp_payload);
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
 *				   clt_Sproxy 				    					 *
 *===========================================================================*/
void *clt_Sproxy(void *arg)
{
	int clt_id = (int) arg;
   	client_t *clt_ptr;
	
	USRDEBUG("CLIENT_SPROXY[%d]: Initializing...\n", clt_id);
	clt_ptr = &client_tab[clt_id];
	clt_Sproxy_init(clt_ptr);
	clt_Sproxy_loop(clt_ptr);
	USRDEBUG("CLIENT_SPROXY[%d]: Exiting...\n", clt_id);
}	

void  clt_Sproxy_init(	client_t *clt_ptr) 
{
    int rcode = 0;
	int optval;

	lbpx_desc_t *cpx_ptr; 
	cpx_ptr = &clt_ptr->clt_spx;

	USRDEBUG("CLIENT_SPROXY(%s): Initializing proxy sender\n", clt_ptr->clt_name);

	rcode = posix_memalign( (void**) &cpx_ptr->lbp_mqbuf, getpagesize(), (sizeof(msgq_buf_t)));
	if (rcode != 0) ERROR_EXIT(rcode);

	cpx_ptr->lbp_header = &cpx_ptr->lbp_mqbuf->mb_header;
	cpx_ptr->lbp_payload = NULL;

//	rcode = posix_memalign( (void**) &cpx_ptr->lbp_payload, getpagesize(), (sizeof(proxy_payload_t)));
//	if (rcode != 0) ERROR_EXIT(rcode);

    // Create server socket.
    if ( (cpx_ptr->lbp_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
       	ERROR_EXIT(errno);
    }
	
	// set SO_REUSEADDR on a socket to true (1):
	optval = 1;
	rcode = setsockopt(cpx_ptr->lbp_sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (rcode < 0) ERROR_EXIT(errno);

	// bind a socket to a device name (might not work on all systems):
	rcode = setsockopt(cpx_ptr->lbp_sd, SOL_SOCKET, SO_BINDTODEVICE, lb.lb_cltdev, strlen(lb.lb_cltdev));
	if (rcode < 0) ERROR_EXIT(errno);

}

void  clt_Sproxy_loop(client_t *clt_ptr)  
{
    int rcode = 0;
	lbpx_desc_t *cpx_ptr; 
	cpx_ptr = &clt_ptr->clt_spx;

	while(1) {
		do {
			if (rcode < 0)
				USRDEBUG("CLIENT_SPROXY(%s): Could not connect. "
                    			"Sleeping for a while...\n",clt_ptr->clt_name);
			sleep(LB_TIMEOUT_5SEC);
			rcode = clt_Sproxy_connect(clt_ptr);
		} while (rcode != 0);
		
		rcode = clt_Sproxy_serving(clt_ptr);
		ERROR_PRINT(rcode);
		close(cpx_ptr->lbp_sd);
	}
   /* code never reaches here */
}

void  clt_Sproxy_connect(client_t *clt_ptr)  
{
    int rcode, i;
    char rmt_ipaddr[INET_ADDRSTRLEN+1];
	lbpx_desc_t *cpx_ptr; 

	cpx_ptr = &clt_ptr->clt_spx;

    cpx_ptr->lbp_port = (LBP_BASE_PORT+lb.lb_nodeid);
	USRDEBUG("CLIENT_SPROXY(%s): remote host port=%d\n", clt_ptr->clt_name, cpx_ptr->lbp_port);    

	// Connect to the  client  
    cpx_ptr->lbp_rmtclt_addr.sin_family = AF_INET;  
    cpx_ptr->lbp_rmtclt_addr.sin_port 	= htons(cpx_ptr->lbp_port);  
    cpx_ptr->lbp_rmthost = gethostbyname(clt_ptr->clt_name);
	if( cpx_ptr->lbp_rmthost == NULL) ERROR_EXIT(h_errno);
	USRDEBUG("CLIENT_SPROXY(%s): rmthost %s IP %s\n", clt_ptr->clt_name, 
		cpx_ptr->lbp_rmthost->h_name, 
		inet_ntoa(*(struct in_addr*)cpx_ptr->lbp_rmthost->h_addr));    

//	if( cpx_ptr->lbp_rmthost == NULL) ERROR_EXIT(h_errno);
//	for( i =0; cpx_ptr->lbp_rmthost->h_addr_list[i] != NULL; i++) {
//		USRDEBUG("CLIENT_SPROXY(%s): remote host address %i: %s\n", clt_ptr->clt_name,
//			i, inet_ntoa( *( struct in_addr*)(cpx_ptr->lbp_rmthost->h_addr_list[i])));
//	}

//    if((inet_pton(AF_INET,inet_ntoa( *( struct in_addr*)(cpx_ptr->lbp_rmthost->h_addr_list[0])), 
//		(struct sockaddr*) &cpx_ptr->lbp_rmtclt_addr.sin_addr)) <= 0)
//    	ERROR_RETURN(-errno);

    if((inet_pton(AF_INET,inet_ntoa( *( struct in_addr*)(cpx_ptr->lbp_rmthost->h_addr)), 
		(struct sockaddr*) &cpx_ptr->lbp_rmtclt_addr.sin_addr)) <= 0)
    	ERROR_RETURN(-errno);

    inet_ntop(AF_INET, (struct sockaddr*) &cpx_ptr->lbp_rmtclt_addr.sin_addr, rmt_ipaddr, INET_ADDRSTRLEN);
	USRDEBUG("CLIENT_SPROXY(%s): running at IP=%s\n", clt_ptr->clt_name, rmt_ipaddr);    

	rcode = connect(cpx_ptr->lbp_sd, (struct sockaddr *) &cpx_ptr->lbp_rmtclt_addr, 
				sizeof(cpx_ptr->lbp_rmtclt_addr));
    if (rcode != 0) ERROR_RETURN(-errno);
	USRDEBUG("CLIENT_SPROXY(%s): connected to IP=%s on socket=%d\n", 
		clt_ptr->clt_name, rmt_ipaddr, cpx_ptr->lbp_sd);    
    return(OK);
}

void  clt_Sproxy_serving(client_t *clt_ptr) 
{
    int rcode = 0;
	lbpx_desc_t *cpx_ptr; 

	cpx_ptr = &clt_ptr->clt_spx;

    while(1) {
		USRDEBUG("CLIENT_SPROXY(%s): Reading message queue..\n", clt_ptr->clt_name);
		
		// Get message from message queue  
		rcode = clt_Sproxy_mqrcv(clt_ptr);
		if( rcode < 0) {
			ERROR_PRINT(rcode);
			sleep(1);
			continue;
		}
		USRDEBUG("CLIENT_SPROXY(%s): " CMD_FORMAT ,clt_ptr->clt_name, 
			CMD_FIELDS(cpx_ptr->lbp_header)); 
		USRDEBUG("CLIENT_SPROXY(%s): " CMD_XFORMAT,clt_ptr->clt_name, 
			CMD_XFIELDS(cpx_ptr->lbp_header)); 
	
#ifdef PROXY_TIMESTAMP	
		timestamp_opt = 1;
		if( timestamp_opt){
			clock_gettime(clk_id, &cpx_ptr->lbp_header->c_timestamp);
			USRDEBUG("CLIENT_SPROXY(%s): " CMD_TSFORMAT, clt_ptr->clt_name, 
				CMD_TSFIELDS(cpx_ptr->lbp_header)); 
		}
#endif // PROXY_TIMESTAMP	

		rcode =  clt_Sproxy_send(clt_ptr);
	}
}

void  clt_Sproxy_send(client_t *clt_ptr) 
{
    int rcode = 0;
	lbpx_desc_t *cpx_ptr; 
	char *p_ptr;

	cpx_ptr = &clt_ptr->clt_spx;

	USRDEBUG("CLIENT_SPROXY(%s): " CMD_FORMAT, clt_ptr->clt_name, 
		CMD_FIELDS(cpx_ptr->lbp_header));
	USRDEBUG("CLIENT_SPROXY(%s): " CMD_XFORMAT, clt_ptr->clt_name, 
		CMD_XFIELDS(cpx_ptr->lbp_header));
		
	/* send the header */
    p_ptr = (char *)cpx_ptr->lbp_header->c_pl_ptr; // Get the pointer from header 
	rcode =  clt_Sproxy_sndhdr(clt_ptr);
	if ( rcode < 0) {
		if( cpx_ptr->lbp_header->c_len > 0)
			FREE(p_ptr);		
		ERROR_RETURN(rcode);
	}
	
	if( cpx_ptr->lbp_header->c_len > 0) {
		USRDEBUG("CLIENT_SPROXY(%s): send payload len=%d\n", 
			clt_ptr->clt_name, cpx_ptr->lbp_header->c_len);
		rcode =  clt_Sproxy_sndpay(clt_ptr);
		// Free the Client Proxy input buffer for payload  
		FREE(p_ptr);
		if ( rcode < OK) ERROR_RETURN(rcode);
	}
    return(OK);
}
	
void  clt_Sproxy_sndhdr(client_t *clt_ptr) 
{
    int rcode = 0;
	int sent = 0;        // how many bytes we've sent
    int bytesleft;
    int n, total;
	char *p_ptr;
	lbpx_desc_t *cpx_ptr;

	cpx_ptr = &clt_ptr->clt_spx;
	bytesleft = sizeof(proxy_hdr_t); // how many bytes we have left to send
	total = bytesleft;
	USRDEBUG("CLIENT_SPROXY(%s): send header=%d \n", clt_ptr->clt_name, bytesleft);

    p_ptr = (char *) cpx_ptr->lbp_header;
	USRDEBUG("CLIENT_SPROXY(%s): " CMD_FORMAT, clt_ptr->clt_name, 
		CMD_FIELDS(cpx_ptr->lbp_header));
	USRDEBUG("CLIENT_SPROXY(%s): " CMD_XFORMAT, clt_ptr->clt_name, 
		CMD_XFIELDS(cpx_ptr->lbp_header));

    while(sent < total) {
        n = send(cpx_ptr->lbp_sd, p_ptr, bytesleft, 0);
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
	USRDEBUG("CLIENT_SPROXY(%s): sent header=%d \n", clt_ptr->clt_name, total);
    return(OK);

}	

void  clt_Sproxy_sndpay(client_t *clt_ptr) 
{
    int rcode = 0;
    int sent = 0;        // how many bytes we've sent
    int bytesleft;
    int n, total;
	char *p_ptr;
	lbpx_desc_t *cpx_ptr;
	proxy_hdr_t *hdr_ptr;


	cpx_ptr = &clt_ptr->clt_spx;

	assert( cpx_ptr->lbp_header->c_len > 0);

	bytesleft =  cpx_ptr->lbp_header->c_len; // how many bytes we have left to send
	total = bytesleft;
	USRDEBUG("CLIENT_SPROXY(%s): clt_Sproxy_sndpay bytesleft=%d \n", 
		clt_ptr->clt_name, bytesleft);

    hdr_ptr = (char *) cpx_ptr->lbp_header;
	USRDEBUG("CLIENT_SPROXY(%s): " VCOPY_FORMAT, 
			clt_ptr->clt_name, VCOPY_FIELDS(hdr_ptr));
			
//    p_ptr = (char *) cpx_ptr->lbp_payload;
    p_ptr = (char *)cpx_ptr->lbp_header->c_pl_ptr; // Get the pointer from header 
	USRDEBUG("CLIENT_SPROXY(%s): " VCOPY_FORMAT, 
		clt_ptr->clt_name, VCOPY_FIELDS(cpx_ptr->lbp_header));
		
    while(sent < total) {
        n = send(cpx_ptr->lbp_sd, p_ptr, bytesleft, 0);
		USRDEBUG("CLIENT_SPROXY(%s): payload sent=%d \n",clt_ptr->clt_name, n);
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
	USRDEBUG("CLIENT_SPROXY(%s): sent payload=%d \n", clt_ptr->clt_name, total);
    return(OK);
}	

int clt_Sproxy_mqrcv(client_t *clt_ptr)
{
    int rcode = 0;
	lbpx_desc_t *cpx_ptr;

	cpx_ptr = &clt_ptr->clt_spx;
	USRDEBUG("CLIENT_SPROXY(%s): reading from mqid=%d \n", 
		clt_ptr->clt_name, cpx_ptr->lbp_mqid);
	rcode = msgrcv(cpx_ptr->lbp_mqid, cpx_ptr->lbp_mqbuf, sizeof(proxy_hdr_t), 0, 0 );
	if( rcode < 0) ERROR_RETURN(-errno);
	USRDEBUG("CLIENT_SPROXY(%s): %d bytes received\n", 
		clt_ptr->clt_name, rcode);
	return(rcode);
}


