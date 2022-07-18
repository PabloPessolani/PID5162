/***********************************************************/
/*  DSP_RPROXY									*/
/***********************************************************/
#include "dsp_proxy.h"

/*----------------------------------------------*/
/*      PROXY RECEIVER FUNCTIONS               */
/*----------------------------------------------*/

int dsp_put2lcl(proxy_hdr_t *hdr_ptr, proxy_payload_t *pl_ptr)
{
	int dcid, rcode;
	sess_entry_t *sess_ptr;
	 proxy_t *px_ptr, *lcl_ptr; 

		if( local_nodeid == NODE1){ // PARA PRUEBA  
			
			dcid	= hdr_ptr->c_dcid;
			if( dcid < 0 || dcid >= dvs_ptr->d_nr_dcs)
				ERROR_RETURN(EDVSBADDCID);
	
			// TOMAMOS LA PRIMER SESION SOLO PARA PRUEBA 
			sess_ptr = (sess_entry_t *)sess_table[dcid].st_tab_ptr;
			
			MTX_LOCK(sess_table[dcid].st_mutex);
			if( (sess_ptr->se_dcid 			== hdr_ptr->c_dcid)
			&&  (sess_ptr->se_clt_nodeid 	== hdr_ptr->c_dnode)
			&& 	(sess_ptr->se_clt_ep	 	== hdr_ptr->c_dst)) {	
				PXYDEBUG("RPROXY(%d):  Existing Session with remote endpoint %d\n", 
					hdr_ptr->c_snode, hdr_ptr->c_src);				
				sess_ptr->se_svr_PID = hdr_ptr->c_pid;
				// Change the source nodeid as if was the Virtual Node
				hdr_ptr->c_snode = lb_ptr->lb_nodeid;
				MTX_UNLOCK(sess_table[dcid].st_mutex);
				lcl_ptr = &proxy_tab[local_nodeid];
				px_ptr 	= &proxy_tab[lb_ptr->lb_nodeid];
				// Copy header and payload to the virtual node RPROXY
				// And wakes it up 
				MTX_LOCK(px_ptr->px_recv_mtx);
				if( TEST_BIT(px_ptr->px_status, PX_BIT_WAIT2PUT)){
					memcpy(px_ptr->px_rdesc.td_header, hdr_ptr, sizeof(proxy_hdr_t));
					if( hdr_ptr->c_len > 0)
						memcpy(px_ptr->px_rdesc.td_payload, pl_ptr, hdr_ptr->c_len);	
					CLR_BIT(px_ptr->px_status, PX_BIT_WAIT2PUT);
					COND_SIGNAL(px_ptr->px_recv_cond);
				} else{
					SET_BIT(lcl_ptr->px_status, PX_BIT_WAIT2PUT);
					COND_WAIT(lcl_ptr->px_recv_cond, px_ptr->px_recv_mtx);
					// INCOMPLETO!!
					// el RPROXY del LB deberia copiar los datos antes de despertarlo !!! pero como??
				}
				MTX_UNLOCK(px_ptr->px_recv_mtx);
				return(OK);
			} else {
				MTX_UNLOCK(sess_table[dcid].st_mutex);
			}
		}
		rcode = dvk_put2lcl(hdr_ptr, pl_ptr);
		if( rcode < 0) ERROR_RETURN(rcode);
		return(rcode);
}

int decompress_payload( proxy_t *px_ptr, proxy_hdr_t *hd_ptr, proxy_payload_t *pl_ptr)
{
	LZ4F_errorCode_t lz4_rcode;
	size_t comp_len, raw_len;
	
	PXYDEBUG("RPROXY(%d): INPUT maxRsize=%d maxCsize=%d \n", px_ptr->px_nodeid, 
		px_ptr->px_rdesc.td_maxRsize, px_ptr->px_rdesc.td_maxCsize );

	comp_len = hd_ptr->c_len;
	raw_len  = px_ptr->px_rdesc.td_maxRsize;
	PXYDEBUG("RPROXY(%d): INPUT comp_len=%d  MAX raw_len=%d \n", px_ptr->px_nodeid,
		comp_len, raw_len );

	lz4_rcode = LZ4F_decompress(px_ptr->px_lz4Dctx,
                          px_ptr->px_rdesc.td_decomp_pl, &raw_len,
                          pl_ptr, &comp_len,
                          NULL);
		
	if (LZ4F_isError(lz4_rcode)) {
		fprintf(stderr ,"LZ4F_decompress failed: error %zu", lz4_rcode);
		ERROR_EXIT(lz4_rcode);
	}
	
	PXYDEBUG("RPROXY(%d): OUTPUT raw_len=%d comp_len=%d\n",	px_ptr->px_nodeid, raw_len, comp_len);
	return(raw_len);					  
						  
}
        
void init_decompression( proxy_t *px_ptr) 
{
	LZ4F_errorCode_t lz4_rcode;

	PXYDEBUG("RPROXY(%d):\n", px_ptr->px_nodeid);

	px_ptr->px_rdesc.td_maxRsize = sizeof(proxy_payload_t);
	
	px_ptr->px_rdesc.td_maxCsize =  px_ptr->px_rdesc.td_maxRsize + LZ4_HEADER_SIZE + LZ4_FOOTER_SIZE;
	posix_memalign( (void**) &px_ptr->px_rdesc.td_decomp_pl, getpagesize(), px_ptr->px_rdesc.td_maxCsize );
	if (px_ptr->px_rdesc.td_decomp_pl== NULL) {
    		fprintf(stderr, "px_ptr->px_rdesc.td_decomp_pl posix_memalign");
			ERROR_EXIT(errno);
  	}
	PXYDEBUG("RPROXY(%d): px_rdesc.td_decomp_pl=%X\n", px_ptr->px_nodeid, px_ptr->px_rdesc.td_decomp_pl);
	
	lz4_rcode =  LZ4F_createDecompressionContext(&px_ptr->px_lz4Dctx, LZ4F_VERSION);
	if (LZ4F_isError(lz4_rcode)) {
		fprintf(stderr ,"LZ4F_createDecompressionContext: error %zu", lz4_rcode);
		ERROR_EXIT(lz4_rcode);
	}
	PXYDEBUG("RPROXY(%d): px_lz4Dctx=%X\n", px_ptr->px_nodeid, px_ptr->px_lz4Dctx);
}

void stop_decompression( proxy_t *px_ptr) 
{
	LZ4F_errorCode_t lz4_rcode;

	PXYDEBUG("RPROXY(%d):\n", px_ptr->px_nodeid);

	lz4_rcode = LZ4F_freeDecompressionContext(px_ptr->px_lz4Dctx);
	if (LZ4F_isError(lz4_rcode)) {
		fprintf(stderr ,"LZ4F_freeDecompressionContext: error %zu", lz4_rcode);
		ERROR_EXIT(lz4_rcode);
	}
}
		

		
/* pr_setup_connection: bind and setup a listening socket 
   This socket is not accepting connections yet after
   end of this call.
 */                
int pr_setup_connection(proxy_t *px_ptr) 
{
    int ret;
    short int port_no;
    struct sockaddr_in servaddr;
	struct hostent *h_ptr;
    int optval = 1;
    struct timeval timeout;      

    timeout.tv_sec = MP_RECV_TIMEOUT;
    timeout.tv_usec = 0;

    port_no = px_ptr->px_rport;
	PXYDEBUG("RPROXY(%d): for node %s running at port=%d\n", 
			px_ptr->px_nodeid, px_ptr->px_name, port_no);

    // Create server socket.
    if ( (px_ptr->px_rlisten_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
        ERROR_EXIT(errno);

    if( (ret = setsockopt(px_ptr->px_rlisten_sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) < 0)
       	ERROR_EXIT(errno);

    if( (ret = setsockopt(px_ptr->px_rlisten_sd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))) < 0)
       	ERROR_EXIT(errno);

//	enable_keepalive(px_ptr->px_rlisten_sd);

    // Bind (attach) this process to the server socket.
    servaddr.sin_family = AF_INET;
	if( !strncmp(px_ptr->px_rname,"ANY", 3)){
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	}else{
		h_ptr = gethostbyname(px_ptr->px_rname);
		if( h_ptr == NULL) ERROR_EXIT(h_errno);
		unsigned int i=0;
		while ( h_ptr->h_addr_list[i] != NULL) {
          PXYDEBUG("RPROXY(%d): %s \n", px_ptr->px_nodeid, 
			inet_ntoa( *( struct in_addr*)( h_ptr->h_addr_list[i])));
          i++;
		}  
		PXYDEBUG("RPROXY(%d): px_rname=%s listening on IP address=%s\n", 
			px_ptr->px_nodeid, px_ptr->px_rname, inet_ntoa( *( struct in_addr*)(h_ptr->h_addr)));		
		servaddr.sin_addr.s_addr = inet_addr(inet_ntoa( *( struct in_addr*)(h_ptr->h_addr)));
	}
    servaddr.sin_port = htons(port_no);
	ret = bind(px_ptr->px_rlisten_sd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if(ret < 0) ERROR_EXIT(errno);

	PXYDEBUG("RPROXY(%d): is bound to port=%d socket=%d\n", 
			px_ptr->px_nodeid, port_no, px_ptr->px_rlisten_sd);

// Turn 'rproxy_sd' to a listening socket. Listen queue size is 1.
	ret = listen(px_ptr->px_rlisten_sd, 0);
    if(ret < 0) ERROR_EXIT(errno);

    return(OK);
   
}

/* pr_receive_payload: receives the payload from remote sender */
int pr_receive_payload(proxy_t *px_ptr, proxy_payload_t *pl_ptr, int pl_size) 
{
    int n, received = 0;
    char *p_ptr;

   	PXYDEBUG("RPROXY(%d): pl_size=%d\n",px_ptr->px_nodeid, pl_size);
   	p_ptr = (char*) pl_ptr;
   	while ((n = recv(px_ptr->px_rconn_sd, p_ptr, (pl_size-received), 0 )) > 0) {
	   	PXYDEBUG("RPROXY(%d): recv=%d\n",px_ptr->px_nodeid, n);
        received = received + n;
		PXYDEBUG("RPROXY(%d): n:%d | received:%d\n",px_ptr->px_nodeid , n,received);
        if (received >= pl_size) return(OK);
       	p_ptr += n;
   	}
   	if( n == 0) ERROR_RETURN(EDVSNOCONN);
    if(n < 0) ERROR_RETURN(-errno);
 	return(OK);
}

/* pr_receive_header: receives the header from remote sender */
int pr_receive_header(proxy_t *px_ptr) 
{
    int n, total,  received = 0;
    char *p_ptr;

   	PXYDEBUG("RPROXY(%d): socket=%d\n",px_ptr->px_nodeid , px_ptr->px_rconn_sd);
   	p_ptr = (char*) px_ptr->px_rdesc.td_header;
	total = sizeof(proxy_hdr_t);
   	while ((n = recv(px_ptr->px_rconn_sd, p_ptr, (total-received), 0 )) > 0) {
        received = received + n;
		PXYDEBUG("RPROXY(%d): n:%d | received:%d | HEADER_SIZE:%d\n", 
					px_ptr->px_nodeid, n,received,sizeof(proxy_hdr_t));
        if (received >= sizeof(proxy_hdr_t)) {  
			PXYDEBUG("RPROXY(%d): " CMD_FORMAT,px_ptr->px_nodeid, 
				CMD_FIELDS(px_ptr->px_rdesc.td_header));
			PXYDEBUG("RPROXY(%d):" CMD_XFORMAT,px_ptr->px_nodeid, 
				CMD_XFIELDS(px_ptr->px_rdesc.td_header));
        	return(OK);
        } else {
			PXYDEBUG("RPROXY(%d): Header partially received. There are %d bytes still to get\n", 
                  	px_ptr->px_nodeid, sizeof(proxy_hdr_t) - received);
        	p_ptr += n;
        }
   	}
	if( n == 0) ERROR_RETURN(EDVSNOCONN);	
    if( n < 0)  ERROR_RETURN(-errno);
	return(OK);
}

/* pr_process_message: receives header and payload if any. Then deliver the 
 * message to local */
int pr_process_message(proxy_t *px_ptr) 
{
    int rcode, payload_size, i, ret, raw_len, retry, dcid;
 	message *m_ptr;
	proxy_hdr_t *bat_cmd, *bat_ptr;
	dc_usr_t dcu, *dcu_ptr;
		
    do {
		bzero(px_ptr->px_rdesc.td_header, sizeof(proxy_hdr_t));
		PXYDEBUG("RPROXY(%d): About to receive header\n",px_ptr->px_nodeid);
    	rcode = pr_receive_header(px_ptr);
    	if (rcode != 0) ERROR_RETURN(rcode);

		PXYDEBUG("RPROXY(%d):" CMD_FORMAT,px_ptr->px_nodeid,
			CMD_FIELDS(px_ptr->px_rdesc.td_header));
		PXYDEBUG("RPROXY(%d):" CMD_XFORMAT,px_ptr->px_nodeid,
			CMD_XFIELDS(px_ptr->px_rdesc.td_header));
		PXYDEBUG("RPROXY(%d): %d " CMD_TSFORMAT, px_ptr->px_nodeid, 
			px_ptr->px_rdesc.td_tid, CMD_TSFIELDS(px_ptr->px_rdesc.td_header)); 
		PXYDEBUG("RPROXY(%d): %d " CMD_PIDFORMAT, px_ptr->px_nodeid, 
				px_ptr->px_rdesc.td_tid, CMD_PIDFIELDS(px_ptr->px_rdesc.td_header)); 
				
		if( px_ptr->px_rdesc.td_header->c_cmd == CMD_NONE) {
			PXYDEBUG("RPROXY(%d): HELLO %d CMD_NONE\n",px_ptr->px_nodeid, px_ptr->px_rdesc.td_tid); 			
			m_ptr = &px_ptr->px_rdesc.td_header->c_msg;
			PXYDEBUG("RPROXY(%d): m_type=%d\n", px_ptr->px_nodeid, m_ptr->m_type);
			switch(m_ptr->m_type){
				case MT_LOAD_THRESHOLDS:
					PXYDEBUG("RPROXY(%d): MT_LOAD_THRESHOLDS\n", px_ptr->px_nodeid);
					PXYDEBUG("RPROXY(%d):\n" MSG1_FORMAT, px_ptr->px_nodeid, MSG1_FIELDS(m_ptr));
					
					MTX_LOCK(lb_ptr->lb_mtx);
					assert( lb_ptr->lb_period	> 0);
					lb_ptr->lb_lowwater	= m_ptr->m1_i1;
					lb_ptr->lb_highwater	= m_ptr->m1_i2;
					lb_ptr->lb_period		= m_ptr->m1_i3;
					MTX_UNLOCK(lb_ptr->lb_mtx);
					
					MTX_LOCK(px_ptr->px_send_mtx);
#ifdef GET_METRICS
					build_load_level(px_ptr, MT_LOAD_THRESHOLDS | MT_ACKNOWLEDGE);
#endif //  GET_METRICS
					rcode = send_hello_msg(px_ptr);
					if( rcode < 0 ) {
						ERROR_PRINT(rcode);
					}else{
						px_ptr->px_sdesc.td_msg_ok++;
					}
					MTX_UNLOCK(px_ptr->px_send_mtx);
					
					break;
				case MT_RMTBIND:			
					PXYDEBUG("RPROXY(%d): MT_RMTBIND\n", px_ptr->px_nodeid);
////////////////// TEMPORARIO 		
					dcid = m_ptr->m3_i1;
					dcu_ptr = &dcu;
					do {
						ret = dvk_getdcinfo( m_ptr->m3_i1, dcu_ptr);
						if(ret < 0) ERROR_PRINT(ret);
						PXYDEBUG("RPROXY(%d): " DC_USR1_FORMAT, px_ptr->px_nodeid, DC_USR1_FIELDS(dcu_ptr));
						PXYDEBUG("RPROXY(%d): " DC_USR2_FORMAT, px_ptr->px_nodeid, DC_USR2_FIELDS(dcu_ptr));
						sleep(1);
					} while( TEST_BIT( dcu_ptr->dc_nodes, dcid ) ==0);
////////////////// TEMPORARIO 									
					PXYDEBUG("RPROXY(%d):" MSG3_FORMAT, px_ptr->px_nodeid, MSG3_FIELDS(m_ptr));
					rcode = dvk_rmtbind(m_ptr->m3_i1,m_ptr->m3_ca1,m_ptr->m3_i2, (int) m_ptr->m3_p1);
					PXYDEBUG("RPROXY(%d): MT_RMTBIND rcode=%d\n", px_ptr->px_nodeid, rcode);
					if( rcode < EDVSERRCODE)
						ERROR_PRINT(rcode);
					
					MTX_LOCK(px_ptr->px_send_mtx);
					build_reply_msg(px_ptr, (MT_RMTBIND | MT_ACKNOWLEDGE), rcode); 
					rcode = send_hello_msg(px_ptr);
					if( rcode < 0 ) {
						ERROR_PRINT(rcode);
					}else{
						px_ptr->px_sdesc.td_msg_ok++;
					}
					MTX_UNLOCK(px_ptr->px_send_mtx);		
					break;
				default:
					PXYDEBUG("RPROXY(%d): m_type=%d\n", px_ptr->px_nodeid, m_ptr->m_type);
					break;
			}
//			rcode = send_load_level(px_ptr);
//			if(rcode < 0) ERROR_PRINT(rcode);
		}
		
		switch(px_ptr->px_rdesc.td_header->c_cmd){
			case CMD_SEND_MSG:
			case CMD_SNDREC_MSG:
			case CMD_REPLY_MSG:
				m_ptr = &px_ptr->px_rdesc.td_header->c_msg;
				PXYDEBUG("RPROXY(%d): " MSG1_FORMAT, 
					px_ptr->px_nodeid, MSG1_FIELDS(m_ptr));
				break;
			case CMD_COPYIN_DATA:
			case CMD_COPYOUT_DATA:
				PXYDEBUG("RPROXY(%d): " VCOPY_FORMAT, 
					px_ptr->px_nodeid, VCOPY_FIELDS(px_ptr->px_rdesc.td_header)); 
				break;
		}
   	}while(px_ptr->px_rdesc.td_header->c_cmd == CMD_NONE);
    
	/* now we have a proxy header in the buffer. Cast it.*/
    payload_size = px_ptr->px_rdesc.td_header->c_len;
    
    /* payload could be zero */
    if(payload_size > 0) {
        bzero(px_ptr->px_rdesc.td_payload, payload_size);
        rcode = pr_receive_payload(px_ptr, px_ptr->px_rdesc.td_payload, payload_size);
        if (rcode != 0) ERROR_RETURN(rcode);
		if(px_ptr->px_compress == YES ){
			if( TEST_BIT(px_ptr->px_rdesc.td_header->c_flags, FLAG_LZ4_BIT)) {
					raw_len = decompress_payload(px_ptr, px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_payload); 
					if(raw_len < 0)	ERROR_RETURN(raw_len);
					if(!TEST_BIT(px_ptr->px_rdesc.td_header->c_flags, FLAG_BATCH_BIT)) { // ONLY CMD_COPYIN_DATA AND  CMD_COPYOUT_DATA  
						if( raw_len != px_ptr->px_rdesc.td_header->c_vcopy.v_bytes){
								fprintf(stderr,"raw_len=%d " VCOPY_FORMAT, raw_len, VCOPY_FIELDS(px_ptr->px_rdesc.td_header));
								ERROR_RETURN(EDVSBADVALUE);
						}
					}
					px_ptr->px_rdesc.td_header->c_len = raw_len;
			}
		}
	}

	if( px_ptr->px_compress == YES) {
		if( TEST_BIT(px_ptr->px_rdesc.td_header->c_flags, FLAG_LZ4_BIT)) {
#ifdef NO_BATCH_COMPRESSION
			if( px_ptr->px_batch == YES) { 
				if(!TEST_BIT(px_ptr->px_rdesc.td_header->c_flags, FLAG_BATCH_BIT)) {
					PXYDEBUG("RPROXY(%d): put2lcl raw_len=%d\n",px_ptr->px_nodeid, raw_len);
					rcode = dsp_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_decomp_pl);
				}else{
					PXYDEBUG("RPROXY(%d): put2lcl\n",px_ptr->px_nodeid);
					rcode = dsp_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_payload);
				}
			} else {
				PXYDEBUG("RPROXY(%d): put2lcl\n",px_ptr->px_nodeid);
				rcode = dsp_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_decomp_pl);
			}
#else // NO_BATCH_COMPRESSION
				PXYDEBUG("RPROXY(%d): put2lcl\n",px_ptr->px_nodeid);
				rcode = dsp_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_decomp_pl);
		} else{
#endif // NO_BATCH_COMPRESSION
			PXYDEBUG("RPROXY(%d): put2lcl\n",px_ptr->px_nodeid);
			rcode = dsp_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_payload);
		}
	}else {
		PXYDEBUG("RPROXY(%d): put2lcl\n",px_ptr->px_nodeid);
		rcode = dsp_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_payload);
	}

	
	if( px_ptr->px_autobind == YES) {
		
		/******************** REMOTE CLIENT BINDING ************************************/
		ret = 0;
		if( rcode < 0) {
			/* rcode: the result of the las dsp_put2lcl 	*/
			/* ret: the result of the following operations	*/
			dcu_ptr = &dcu;
			ret = dvk_getdcinfo( px_ptr->px_rdesc.td_header->c_dcid, dcu_ptr);
			if(ret < 0) ERROR_RETURN(ret);
			PXYDEBUG("RPROXY(%d):", DC_USR1_FORMAT, px_ptr->px_nodeid, DC_USR1_FIELDS(dcu_ptr));
			PXYDEBUG("RPROXY(%d): REMOTE CLIENT BINDING rcode=%d\n", px_ptr->px_nodeid, rcode);
			if( px_ptr->px_rdesc.td_header->c_src <= (dcu_ptr->dc_nr_sysprocs - dcu_ptr->dc_nr_tasks)) {
				PXYDEBUG("RPROXY(%d): src=%d <= (dc_nr_sysprocs-dc_nr_tasks) %d \n",
					px_ptr->px_nodeid , px_ptr->px_rdesc.td_header->c_src,
					(dcu_ptr->dc_nr_sysprocs - dcu_ptr->dc_nr_tasks));
				ERROR_RETURN(rcode);
			}
			
			switch(rcode){
				case EDVSNONODE:	/* local node register other node for this endpoint   */
				case EDVSENDPOINT:	/* local node registers other endpoint using the slot */
					ret = dvk_unbind(px_ptr->px_rdesc.td_header->c_dcid,px_ptr->px_rdesc.td_header->c_src);
					if(ret != 0) ERROR_RETURN(rcode);
					// fall down 
				case EDVSNOTBIND:	/* the slot is free */
					ret = dvk_rmtbind(px_ptr->px_rdesc.td_header->c_dcid,"rclient",px_ptr->px_rdesc.td_header->c_src,px_ptr->px_rdesc.td_header->c_snode);
					if( ret != px_ptr->px_rdesc.td_header->c_src) ERROR_RETURN(rcode);
					break;
				default:
					ERROR_RETURN(rcode);
			} 

	#ifdef SYSTASK_BIND
			/*Build a pseudo header */
			px_ptr->px_rdesc.td_pseudo->c_cmd 	            = CMD_SNDREC_MSG;
			px_ptr->px_rdesc.td_pseudo->c_dcid	            = px_ptr->px_rdesc.td_header->c_dcid;
			px_ptr->px_rdesc.td_pseudo->c_src	 	        = PM_PROC_NR;
			px_ptr->px_rdesc.td_pseudo->c_dst 	            = SYSTASK(lb_ptr->lb_nodeid);
			px_ptr->px_rdesc.td_pseudo->c_snode 	        = px_ptr->px_rdesc.td_header->c_snode;
			px_ptr->px_rdesc.td_pseudo->c_dnode 	        = lb_ptr->lb_nodeid;
			px_ptr->px_rdesc.td_pseudo->c_rcode	            = 0;
			px_ptr->px_rdesc.td_pseudo->c_len		        = 0;
			px_ptr->px_rdesc.td_pseudo->c_flags	            = 0;
			px_ptr->px_rdesc.td_pseudo->c_batch_nr           = 0;
			px_ptr->px_rdesc.td_pseudo->c_snd_seq            = 0;
			px_ptr->px_rdesc.td_pseudo->c_ack_seq            = 0;
			px_ptr->px_rdesc.td_pseudo->c_timestamp          = px_ptr->px_rdesc.td_header->c_timestamp;
			px_ptr->px_rdesc.td_pseudo->c_msg.m_source 		= PM_PROC_NR;
			px_ptr->px_rdesc.td_pseudo->c_msg.m_type 		= SYS_BINDPROC;
			px_ptr->px_rdesc.td_pseudo->c_msg.M3_ENDPT 		= px_ptr->px_rdesc.td_header->c_src;
			px_ptr->px_rdesc.td_pseudo->c_msg.M3_NODEID 		= px_ptr->px_rdesc.td_header->c_snode;
			px_ptr->px_rdesc.td_pseudo->c_msg.M3_OPER 		= RMT_BIND;
			sprintf(&px_ptr->px_rdesc.td_pseudo->c_msg.m3_ca1,"RClient%d", px_ptr->px_rdesc.td_header->c_snode);
			
			/* send PSEUDO message to local SYSTASK */	
			ret = dsp_put2lcl(px_ptr->px_rdesc.td_pseudo, px_ptr->px_rdesc.td_payload);
			if( ret < 0 ) {
				ERROR_PRINT(ret);
				ERROR_RETURN(rcode);
			}
		
	#endif // SYSTASK_BIND

			/* PUT2LCL retry after REMOTE CLIENT AUTOMATIC  BINDING */
			PXYDEBUG("RPROXY(%d): retry put2lcl (autobind)\n",px_ptr->px_nodeid);
			if( px_ptr->px_compress == YES) {
				if( TEST_BIT(px_ptr->px_rdesc.td_header->c_flags, FLAG_LZ4_BIT)) {
#ifdef NO_BATCH_COMPRESSION					
					if( px_ptr->px_batch == YES) { 
						if(!TEST_BIT(px_ptr->px_rdesc.td_header->c_flags, FLAG_BATCH_BIT)) {
							PXYDEBUG("RPROXY(%d): put2lcl raw_len=%d\n",px_ptr->px_nodeid, raw_len);
							rcode = dsp_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_decomp_pl);
						}else{
							PXYDEBUG("RPROXY(%d): put2lcl\n",px_ptr->px_nodeid);
							rcode = dsp_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_payload);
						}
					} else {
						PXYDEBUG("RPROXY(%d): put2lcl\n",px_ptr->px_nodeid);
						rcode = dsp_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_decomp_pl);
					}
#else // NO_BATCH_COMPRESSION					
					PXYDEBUG("RPROXY(%d): put2lcl\n",px_ptr->px_nodeid);
					rcode = dsp_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_decomp_pl);
				} else{
					PXYDEBUG("RPROXY(%d): put2lcl\n",px_ptr->px_nodeid);
					rcode = dsp_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_payload);
				}
#endif // NO_BATCH_COMPRESSION					
			}else {
				PXYDEBUG("RPROXY(%d): put2lcl\n",px_ptr->px_nodeid);
				rcode = dsp_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_payload);
			}
			
			if( ret < 0) {
				ERROR_PRINT(ret);
				if( rcode < 0) ERROR_RETURN(rcode);
			}
			rcode = 0;
		}
	}
	if( rcode < 0) ERROR_RETURN(rcode);
	
	PXYDEBUG("RPROXY(%d):" CMD_FORMAT, px_ptr->px_nodeid, 
		CMD_FIELDS(px_ptr->px_rdesc.td_header));
	PXYDEBUG("RPROXY(%d):" CMD_XFORMAT,px_ptr->px_nodeid, 
		CMD_XFIELDS(px_ptr->px_rdesc.td_header));
	
	if(px_ptr->px_batch == YES) {
		if( TEST_BIT(px_ptr->px_rdesc.td_header->c_flags, FLAG_BATCH_BIT)) {
			px_ptr->px_rdesc.td_batch_nr = px_ptr->px_rdesc.td_header->c_batch_nr;
			PXYDEBUG("RPROXY(%d): px_ptr->px_rdesc.td_batch_nr=%d\n",px_ptr->px_nodeid , px_ptr->px_rdesc.td_batch_nr);
			// check payload len
			if( (px_ptr->px_rdesc.td_batch_nr * sizeof(proxy_hdr_t)) != px_ptr->px_rdesc.td_header->c_len){
				PXYDEBUG("RPROXY(%d): batched messages \n",px_ptr->px_nodeid);
				ERROR_RETURN(EDVSBADVALUE);
			}
			// put the batched commands
			if( TEST_BIT(px_ptr->px_rdesc.td_header->c_flags, FLAG_LZ4_BIT)) 
				bat_cmd = (proxy_hdr_t *) px_ptr->px_rdesc.td_decomp_pl;
			else 
				bat_cmd = (proxy_hdr_t *) px_ptr->px_rdesc.td_payload;
				
			for( i = 0; i < px_ptr->px_rdesc.td_batch_nr; i++){
				bat_ptr = &bat_cmd[i];
				PXYDEBUG("RPROXY(%d): bat_cmd[%d]:" CMD_FORMAT,
					px_ptr->px_nodeid, i, CMD_FIELDS(bat_ptr));
				PXYDEBUG("RPROXY(%d):" CMD_FORMAT,
					px_ptr->px_nodeid, CMD_FIELDS(bat_ptr));
				PXYDEBUG("RPROXY(%d):" CMD_XFORMAT,
					px_ptr->px_nodeid, CMD_XFIELDS(bat_ptr));
				PXYDEBUG("RPROXY(%d): %d "CMD_TSFORMAT, 
					px_ptr->px_nodeid, px_ptr->px_rdesc.td_tid, CMD_TSFIELDS(bat_ptr)); 
				PXYDEBUG("RPROXY(%d): %d "CMD_PIDFORMAT, 
					px_ptr->px_nodeid, px_ptr->px_rdesc.td_tid, CMD_PIDFIELDS(bat_ptr)); 
				m_ptr = &bat_ptr->c_msg;				
				PXYDEBUG("RPROXY(%d): " MSG1_FORMAT,px_ptr->px_nodeid, MSG1_FIELDS(m_ptr));								
				rcode = dsp_put2lcl(bat_ptr, px_ptr->px_rdesc.td_payload);
				if( rcode < 0) {
					ERROR_RETURN(rcode);
				}
			}
		}
	}

#if PWS_ENABLED 	
	if( px_ptr->px_compress == YES ) {
		if( TEST_BIT(px_ptr->px_rdesc.td_header->c_flags, FLAG_LZ4_BIT)) 
			update_stats(px_ptr->px_rdesc.td_header,  px_ptr->px_rdesc.td_payload, CONNECT_RPROXY);
		else 
			update_stats(px_ptr->px_rdesc.td_header,  px_ptr->px_rdesc.td_payload, CONNECT_RPROXY);
	}else {
		update_stats(px_ptr->px_rdesc.td_header,  px_ptr->px_rdesc.td_payload, CONNECT_RPROXY);
	}
#endif // PWS_ENABLED 	
	
    return(OK);    
}

/* pr_start_serving: accept connection from remote sender
   and loop receiving and processing messages
 */
int pr_start_serving(proxy_t *px_ptr)
{
    int rcode;
	
	/* Serve Forever */
	do { 
		/* get a complete message and process it */
		rcode = pr_process_message(px_ptr);
		if (rcode == OK) {
			PXYDEBUG("RPROXY(%d): Message succesfully processed.\n",px_ptr->px_nodeid);
			px_ptr->px_rdesc.td_msg_ok++;
			px_ptr->px_ack_seq++;
		} else {
			PXYDEBUG("RPROXY(%d): Message processing failure [%d]\n",px_ptr->px_nodeid, rcode);
			px_ptr->px_rdesc.td_msg_fail++;
			if( rcode != EDVSAGAIN) break;
		}	
	}while(1);
	
    /* never reached */

}

int pr_allocmem(proxy_t *px_ptr) 
{
	int rcode;
	
	posix_memalign( (void**) &px_ptr->px_rdesc.td_header, getpagesize(), (sizeof(proxy_hdr_t)));
	if (px_ptr->px_rdesc.td_header== NULL) {
    		perror(" px_ptr->px_rdesc.td_header posix_memalign");
    		exit(1);
  	}
	PXYDEBUG("RPROXY(%d): px_rdesc.td_header=%X\n", px_ptr->px_nodeid, px_ptr->px_rdesc.td_header);

	posix_memalign( (void**) &px_ptr->px_rdesc.td_payload, getpagesize(), (sizeof(proxy_payload_t)));
	if (px_ptr->px_rdesc.td_payload== NULL) {
    		perror(" px_ptr->px_rdesc.td_payload posix_memalign");
    		exit(1);
  	}
	PXYDEBUG("RPROXY(%d): px_rdesc.td_payload=%X\n", px_ptr->px_nodeid, px_ptr->px_rdesc.td_payload);

	posix_memalign( (void**) &px_ptr->px_rdesc.td_pseudo, getpagesize(), (sizeof(proxy_hdr_t)));
	if (px_ptr->px_rdesc.td_pseudo== NULL) {
    		perror(" px_ptr->px_rdesc.td_pseudo posix_memalign");
    		exit(1);
  	}
	PXYDEBUG("RPROXY(%d): px_rdesc.td_pseudo=%X\n", px_ptr->px_nodeid, px_ptr->px_rdesc.td_pseudo);
	
	if( px_ptr->px_batch == YES) {
		posix_memalign( (void**) &px_ptr->px_rdesc.td_batch, getpagesize(), (sizeof(proxy_payload_t)));
		if (px_ptr->px_rdesc.td_payload== NULL) {
				perror("px_ptr->px_rdesc.td_batch posix_memalign");
				exit(1);
		}
	}
	PXYDEBUG("RPROXY(%d): px_rdesc.td_batch=%X\n", px_ptr->px_nodeid, px_ptr->px_rdesc.td_batch);

	PXYDEBUG("RPROXY(%d): px_ptr->px_rdesc.td_header=%p px_ptr->px_rdesc.td_payload=%p diff=%d\n",
			px_ptr->px_nodeid, px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_payload, 
			((char*) px_ptr->px_rdesc.td_payload - (char*) px_ptr->px_rdesc.td_header));
	return(OK);
}	

void *pr_thread(void *arg) 
{
    int ret;
	int rcode = 0;
	proxy_t *px_ptr;
	int sender_addrlen;
    struct sockaddr_in sa;
    char ip4[INET_ADDRSTRLEN];  // space to hold the IPv4 string

	px_ptr= (proxy_t*) arg;
	PXYDEBUG("RPROXY(%d): Initializing...\n", px_ptr->px_nodeid);
    
	/* Lock mutex... */
	MTX_LOCK(px_ptr->px_rdesc.td_mtx);
	/* Get and save TID and ready flag.. */
	px_ptr->px_rdesc.td_tid = syscall(SYS_gettid);
	/* and signal main thread that we're ready */
	COND_SIGNAL(px_ptr->px_rdesc.td_cond);
	/* wait MAIN signal */
	COND_WAIT(px_ptr->px_rdesc.td_tcond, px_ptr->px_rdesc.td_mtx);
	/* ..then unlock when we're done. */
	MTX_UNLOCK(px_ptr->px_rdesc.td_mtx);

	PXYDEBUG("RPROXY(%d): td_tid=%d\n", 
		px_ptr->px_nodeid, px_ptr->px_rdesc.td_tid);
		
	px_ptr->px_ack_seq = 0;
	
	rcode = pr_allocmem(px_ptr);

#if PWS_ENABLED 	
	px_ptr->px_pws_port = (BASE_PWS_RPORT +  px_ptr->px_nodeid);
	rcode = pthread_create( &px_ptr->px_pws_pth, NULL, main_pws, px_ptr );
	if(rcode) ERROR_EXIT(rcode);
	PXYDEBUG("RPROXY(%d): px_ptr->px_pws_pth=%u\n",px_ptr->px_nodeid ,px_ptr->px_pws_pth);		

	MTX_LOCK(px_ptr->px_mutex);
	COND_WAIT(px_ptr->px_rdesc.td_cond, px_ptr->px_mutex);
	COND_SIGNAL(px_ptr->px_rdesc.td_pws_cond);
	MTX_UNLOCK(px_ptr->px_mutex);
#endif // PWS_ENABLED 	
	
	if( px_ptr->px_compress == YES )
		init_decompression(px_ptr);
	
	if( px_ptr->px_nodeid == lb_ptr->lb_nodeid) {
		MTX_LOCK(px_ptr->px_conn_mtx);
		rcode = dvk_proxy_conn(px_ptr->px_nodeid, CONNECT_RPROXY);
		SET_BIT(px_ptr->px_status, PX_BIT_RCONNECTED);
		SET_BIT(lb_ptr->lb_bm_rconn, px_ptr->px_nodeid);
		if( SET_BIT(px_ptr->px_status, PX_BIT_SCONNECTED)){ // if SPROXY is just connected, wakeup it 
			COND_SIGNAL(px_ptr->px_conn_scond);
		}else {
			COND_WAIT(px_ptr->px_conn_rcond, px_ptr->px_conn_mtx); // wait until RPROXY is connected 
		}
		MTX_UNLOCK(px_ptr->px_conn_mtx);
		PXYDEBUG("RPROXY(%d): px_status=%X\n",px_ptr->px_nodeid, px_ptr->px_status);

		px_ptr = &proxy_tab[lb_ptr->lb_nodeid];
		MTX_LOCK(px_ptr->px_recv_mtx);
		while(TRUE) {
			SET_BIT(px_ptr->px_status, PX_BIT_WAIT2PUT);
			PXYDEBUG("RPROXY(%d): Waiting for dvk_put2lcl. px_status=%X\n",
				px_ptr->px_nodeid, px_ptr->px_status);
			COND_WAIT(px_ptr->px_recv_cond, px_ptr->px_recv_mtx);
			rcode = dvk_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_payload);
			ERROR_PRINT(rcode);
		}
		MTX_UNLOCK(px_ptr->px_recv_mtx);
	}
	
	rcode = pr_setup_connection(px_ptr);
	if(rcode != OK) ERROR_EXIT(rcode);
	while(TRUE) {
		do {
			sender_addrlen = sizeof(sa);
			PXYDEBUG("RPROXY(%d): Waiting for connection.\n",px_ptr->px_nodeid);
			px_ptr->px_rconn_sd = accept(px_ptr->px_rlisten_sd, (struct sockaddr *) &sa, &sender_addrlen);
			if(px_ptr->px_rconn_sd < 0) ERROR_PRINT(-errno);
		}while(px_ptr->px_rconn_sd < 0);

		PXYDEBUG("RPROXY(%d): Remote sender [%s] connected on sd [%d]. Getting remote command.\n",
					px_ptr->px_nodeid, inet_ntop(AF_INET, &(sa.sin_addr), ip4, INET_ADDRSTRLEN),
					px_ptr->px_rconn_sd);
		
		MTX_LOCK(px_ptr->px_conn_mtx);
		// EJECUTAR UN JOIN SI ES LA PRIMERA VEZ QUE SE CONECTA - ESTO SE SABE CON EL NRO DE SECUENCIA !!!!!! 
		rcode = dvk_proxy_conn(px_ptr->px_nodeid, CONNECT_RPROXY);
		SET_BIT(px_ptr->px_status, PX_BIT_RCONNECTED);
		SET_BIT(lb_ptr->lb_bm_rconn, px_ptr->px_nodeid);
 		if( SET_BIT(px_ptr->px_status, PX_BIT_SCONNECTED)){ // if SPROXY is just connected, wakeup it 
			COND_SIGNAL(px_ptr->px_conn_scond);
		}else {
			COND_WAIT(px_ptr->px_conn_rcond, px_ptr->px_conn_mtx);
		}
		// UP px_conn_mtx and wait for a connection failure
		MTX_UNLOCK(px_ptr->px_conn_mtx);
		PXYDEBUG("RPROXY(%d): px_status=%X\n",px_ptr->px_nodeid, px_ptr->px_status);

		pr_start_serving(px_ptr);
		
		PXYDEBUG("RPROXY(%d):Something WRONG has occurred. RPROXY not connected\n",px_ptr->px_nodeid);
		MTX_LOCK(px_ptr->px_conn_mtx);
		// EJECUTAR UN LEAVE DE APLICACION !!!!!! 
		// SIGNAL AL SPROXY PARA QUE DESCONNECTE 
		CLR_BIT(px_ptr->px_status, PX_BIT_RCONNECTED);
		CLR_BIT(lb_ptr->lb_bm_rconn, px_ptr->px_nodeid);
		rcode = dvk_proxy_conn(px_ptr->px_nodeid, DISCONNECT_RPROXY);
		MTX_UNLOCK(px_ptr->px_conn_mtx);
		PXYDEBUG("RPROXY(%d): px_status=%X\n",px_ptr->px_nodeid, px_ptr->px_status);
		
		PXYDEBUG("RPROXY(%d): close=%d\n",px_ptr->px_nodeid ,px_ptr->px_rconn_sd);		
		close(px_ptr->px_rconn_sd);
	}
	if( px_ptr->px_compress == YES )
		stop_decompression(px_ptr);
}
