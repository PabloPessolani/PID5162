/***********************************************************/
/*  DSP_SPROXY									*/
/***********************************************************/

#include "dsp_proxy.h"

/*----------------------------------------------*/
/*      PROXY SENDER FUNCTIONS                  */
/*----------------------------------------------*/
int compress_payload( proxy_t *px_ptr, proxy_hdr_t *hd_ptr, proxy_payload_t *pl_ptr )
{
	size_t comp_len;

	/* size_t LZ4F_compressFrame(void* dstBuffer, size_t dstMaxSize, const void* srcBuffer, 
				size_t srcSize, const LZ4F_preferences_t* preferencesPtr);
	*/	
	
	PXYDEBUG("SPROXY(%d): hd_ptr=%X pl_ptr=%X\n", 
		px_ptr->px_nodeid, hd_ptr, pl_ptr);
		
	comp_len = LZ4F_compressFrame(
				px_ptr->px_sdesc.td_comp_pl, px_ptr->px_sdesc.td_maxCsize,
				pl_ptr,	hd_ptr->c_len, 
				NULL);
				
	if (LZ4F_isError(comp_len)) {
		fprintf(stderr ,"LZ4F_compressFrame failed: error %zu", comp_len);
		ERROR_RETURN(comp_len);
	}

	PXYDEBUG("SPROXY(%d): raw_len=%d comp_len=%d\n", 
		px_ptr->px_nodeid, hd_ptr->c_len, comp_len);
		
	return(comp_len);
}

void init_compression( proxy_t *px_ptr) 
{
	size_t frame_size;

	PXYDEBUG("SPROXY(%d):\n", px_ptr->px_nodeid);

	px_ptr->px_sdesc.td_maxRsize = sizeof(proxy_payload_t);
	frame_size = LZ4F_compressBound(sizeof(proxy_payload_t), &lz4_preferences);
	px_ptr->px_sdesc.td_maxCsize =  frame_size + LZ4_HEADER_SIZE + LZ4_FOOTER_SIZE;
	posix_memalign( (void**) &px_ptr->px_sdesc.td_comp_pl, getpagesize(), px_ptr->px_sdesc.td_maxCsize );
	if (px_ptr->px_sdesc.td_comp_pl == NULL) {
    		fprintf(stderr, "px_ptr->px_sdesc.td_comp_pl posix_memalign");
			ERROR_EXIT(errno);
  	}
	PXYDEBUG("SPROXY(%d):td_comp_pl=%X\n", px_ptr->px_nodeid, px_ptr->px_sdesc.td_comp_pl);
}

void stop_compression( proxy_t *px_ptr) 
{
	PXYDEBUG("SPROXY(%d):\n", px_ptr->px_nodeid);
}

/* ps_send_header: send  header to remote receiver */
int  ps_send_header(proxy_t *px_ptr, proxy_hdr_t *hd_ptr ) 
{
    int sent = 0;        // how many bytes we've sent
    int bytesleft;
    int n, total;
	char *p_ptr;
	proxy_t *tmp_ptr;

	bytesleft = sizeof(proxy_hdr_t); // how many bytes we have left to send
	total = bytesleft;
	PXYDEBUG("SPROXY(%d): send bytesleft=%d px_sproxy_sd=%d\n", 
		px_ptr->px_nodeid, bytesleft, px_ptr->px_sproxy_sd);

    p_ptr = (char *) hd_ptr;
	PXYDEBUG("SPROXY(%d):" CMD_FORMAT, px_ptr->px_nodeid, CMD_FIELDS(hd_ptr));
	PXYDEBUG("SPROXY(%d):" CMD_XFORMAT, px_ptr->px_nodeid, CMD_XFIELDS(hd_ptr));
	PXYDEBUG("SPROXY(%d):" CMD_TSFORMAT, px_ptr->px_nodeid, CMD_TSFIELDS(hd_ptr));
	PXYDEBUG("SPROXY(%d):" CMD_PIDFORMAT, px_ptr->px_nodeid, CMD_PIDFIELDS(hd_ptr));

	if( TEST_BIT(hd_ptr->c_flags, FLAG_CHECK_HDR)){
		PXYDEBUG("SPROXY(%d): ps_send_header FLAG_CHECK_HDR\n", px_ptr->px_nodeid);
		tmp_ptr = &proxy_tab[hd_ptr->c_dnode];
	} else{
		tmp_ptr = px_ptr;
	}
	hd_ptr->c_snd_seq = px_ptr->px_snd_seq;
	hd_ptr->c_ack_seq = px_ptr->px_ack_seq;

	MTX_LOCK(tmp_ptr->px_send_mtx);
    while(sent < total) {
        n = send(tmp_ptr->px_sproxy_sd, p_ptr, bytesleft, MSG_DONTWAIT | MSG_NOSIGNAL );
        if (n < 0) {
			ERROR_PRINT(n);
			if(errno == EALREADY) {
				ERROR_PRINT(-errno);
				sleep(1);
				continue;
			}else{
				MTX_UNLOCK(tmp_ptr->px_send_mtx);
				ERROR_RETURN(-errno);
			}
		}
        sent += n;
		p_ptr += n; 
        bytesleft -= n;
    }
	MTX_UNLOCK(tmp_ptr->px_send_mtx);

	PXYDEBUG("SPROXY(%d): socket=%d sent header=%d \n", 
		tmp_ptr->px_nodeid, tmp_ptr->px_sproxy_sd, total);
    return(OK);
}

/* ps_send_payload: send payload to remote receiver */
int  ps_send_payload(proxy_t *px_ptr, proxy_hdr_t *hd_ptr, proxy_payload_t *pl_ptr ) 
{
    int sent = 0;        // how many bytes we've sent
    int bytesleft;
    int n, total;
	char *p_ptr;
	proxy_t *tmp_ptr;

	assert( hd_ptr->c_len > 0);
	
	bytesleft =  hd_ptr->c_len; // how many bytes we have left to send
	total = bytesleft;
	PXYDEBUG("SPROXY(%d): ps_send_payload bytesleft=%d \n", px_ptr->px_nodeid, bytesleft);

	if( TEST_BIT(hd_ptr->c_flags, FLAG_CHECK_HDR)){
		PXYDEBUG("SPROXY(%d): ps_send_payload FLAG_CHECK_HDR\n", px_ptr->px_nodeid);
		tmp_ptr = &proxy_tab[hd_ptr->c_dnode];
	} else{
		tmp_ptr = px_ptr;
	}

    p_ptr = (char *) pl_ptr;
	MTX_LOCK(tmp_ptr->px_send_mtx);
    while(sent < total) {
        n = send(tmp_ptr->px_sproxy_sd, p_ptr, bytesleft, MSG_DONTWAIT | MSG_NOSIGNAL );
		PXYDEBUG("SPROXY(%d): sent=%d \n", tmp_ptr->px_nodeid, n);
        if (n < 0) {
			if(errno == EALREADY || errno == EAGAIN) {
				ERROR_PRINT(errno);
				sleep(1);
				continue;
			}else{
				MTX_UNLOCK(tmp_ptr->px_send_mtx);
				ERROR_RETURN(-errno);
			}
		}
        sent += n;
		p_ptr += n; 
        bytesleft -= n;
    }
	MTX_UNLOCK(tmp_ptr->px_send_mtx);
	PXYDEBUG("SPROXY(%d): socket=%d sent payload=%d \n", 
		tmp_ptr->px_nodeid, tmp_ptr->px_sproxy_sd, total);
    return(OK);
}

/* 
 * ps_send_remote: send a message (header + payload if existing) 
 * to remote receiver
 */
int  ps_send_remote(proxy_t *px_ptr, proxy_hdr_t *hd_ptr, proxy_payload_t *pl_ptr ) 
{
	int rcode, comp_len;

	PXYDEBUG("SPROXY(%d):" CMD_FORMAT, px_ptr->px_nodeid, CMD_FIELDS(hd_ptr));
	PXYDEBUG("SPROXY(%d):" CMD_XFORMAT, px_ptr->px_nodeid, CMD_XFIELDS(hd_ptr));

	if( px_ptr->px_compress == YES ) {
		if( (hd_ptr->c_cmd == CMD_COPYIN_DATA) 
		 || (hd_ptr->c_cmd == CMD_COPYOUT_DATA) 
#ifdef NO_BATCH_COMPRESSION	
		 || TEST_BIT(hd_ptr->c_flags, FLAG_BATCH_BIT)  // do not compress BATCHED commands 
#endif // NO_BATCH_COMPRESSION	
		 ){

			PXYDEBUG("SPROXY(%d): c_len=%d\n", px_ptr->px_nodeid, hd_ptr->c_len);
			assert( hd_ptr->c_len > 0);	

			/* compress here */
			comp_len = compress_payload(px_ptr, hd_ptr, pl_ptr);
			PXYDEBUG("SPROXY(%d): c_len=%d comp_len=%d\n", 
				px_ptr->px_nodeid, hd_ptr->c_len, comp_len);
			if(comp_len < hd_ptr->c_len){
				/* change command header  */
				hd_ptr->c_len = comp_len; 
				SET_BIT(hd_ptr->c_flags, FLAG_LZ4_BIT); 
			}
			PXYDEBUG("SPROXY(%d): " CMD_XFORMAT, px_ptr->px_nodeid, CMD_XFIELDS(hd_ptr));		
		}else {
			assert( hd_ptr->c_len == 0);
		}
	} 
	
	/* send the header */
	rcode =  ps_send_header(px_ptr, hd_ptr);
	if ( rcode < 0)  ERROR_RETURN(rcode);

	if( hd_ptr->c_len > 0) {
		PXYDEBUG("SPROXY(%d): send payload len=%d\n", px_ptr->px_nodeid, hd_ptr->c_len );
		if( px_ptr->px_compress == YES) {
			if( TEST_BIT(hd_ptr->c_flags, FLAG_LZ4_BIT) )
				rcode =  ps_send_payload(px_ptr, hd_ptr, px_ptr->px_sdesc.td_comp_pl);
			else
				rcode =  ps_send_payload(px_ptr, hd_ptr, px_ptr->px_sdesc.td_payload);
		}else {
				rcode =  ps_send_payload(px_ptr, hd_ptr, pl_ptr);
		}
		if ( rcode < OK)  ERROR_RETURN(rcode);
	}

#if PWS_ENABLED 	
	update_stats(px_ptr, hd_ptr, pl_ptr, CONNECT_SPROXY) ;
#endif // PWS_ENABLED 	

    return(OK);
}

/* 
 * ps_start_serving: gets local message and sends it to remote receiver .
 * Do this forever.
 */
int  ps_start_serving(proxy_t *px_ptr)
{
	proxy_hdr_t *bat_vect, *bat_ptr;
    int rcode;
    message *m_ptr;
    int ret;

	PXYDEBUG("SPROXY(%d): Waiting a message\n", px_ptr->px_nodeid);

	if( lb_ptr->lb_compress_opt == YES )
		init_compression(px_ptr);
	
	MTX_LOCK(px_ptr->px_sdesc.td_mtx);
    while(TRUE) {
		px_ptr->px_sdesc.td_cmd_flag = 0;
		px_ptr->px_sdesc.td_batch_nr = 0;		// nr_cmd the number of batching commands in the buffer
			
		PXYDEBUG("SPROXY(%d): Waiting a message\n", px_ptr->px_nodeid);
		ret = dvk_get2rmt(px_ptr->px_sdesc.td_header, px_ptr->px_sdesc.td_payload); 
		if( ret < 0) ERROR_PRINT(ret);
//		px_ptr->px_sdesc.td_header->c_flags   = 0;
//		px_ptr->px_sdesc.td_header->c_snd_seq = 0;
//		px_ptr->px_sdesc.td_header->c_ack_seq = 0;
		switch(ret) {
			case OK:
				break;
			case EDVSTIMEDOUT:
//			case EDVSINTR:
				if( px_ptr->px_nodeid != lb_ptr->lb_nodeid) {
					if( TEST_BIT(px_ptr->px_status, PX_BIT_SCONNECTED) 
						&& TEST_BIT(px_ptr->px_status, PX_BIT_RCONNECTED)) {
						PXYDEBUG("SPROXY(%d): Sending HELLO\n", px_ptr->px_nodeid);
#ifdef GET_METRICS
						build_load_level(px_ptr, MT_LOAD_LEVEL);
#endif // GET_METRICS
						rcode = send_hello_msg(px_ptr);
						ERROR_PRINT(rcode);
					}
				}
				break;
			default:
				sleep(MP_TIMEOUT_5SEC);
				ERROR_RETURN(ret);
				break;
		}
		if( ret == EDVSTIMEDOUT) continue;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// AQUI DECIDIR EL DESTINO DEL MENSAJE !!!!!

int dcid, max_sessions, i;
sess_entry_t *sess_ptr;
proxy_hdr_t *hdr_ptr;

		if( local_nodeid == NODE1 && px_ptr->px_nodeid == lb_ptr->lb_nodeid){
			// AQUI DECIDIR EL DESTINO DEL MENSAJE !!!!!
			// PARA PRUEBA NODE0 es NODE2 
			px_ptr->px_sdesc.td_header->c_dnode = NODE2  ;
			SET_BIT(px_ptr->px_sdesc.td_header->c_flags, FLAG_CHECK_HDR);
			// AQUI SE DEBERIA CREAR UNA SESION O BUSCAR UNA EXISTENTE
			
			hdr_ptr = px_ptr->px_sdesc.td_header;
			dcid	= hdr_ptr->c_dcid;
			assert( dcid >= 0 && dcid < dvs_ptr->d_nr_dcs);
	
			// TOMAMOS LA PRIMER SESION SOLO PARA PRUEBA 
			sess_ptr = (sess_entry_t *)sess_table[dcid].st_tab_ptr;
			
			MTX_LOCK(sess_table[dcid].st_mutex);

			// A session is needed because when receiving reply packets in any RPROXY, 
			// they must be identified if they belong to a session 
			// And modified the header if they are 
			if (sess_ptr->se_clt_PID != hdr_ptr->c_pid) { // ASUMIMOS QUE DISTINTO PID ES NUEVA SESION SOLO PRUEBA 
				PXYDEBUG("SPROXY(%d):  NEW Session with endpoint %d\n", 
					px_ptr->px_nodeid, hdr_ptr->c_dst);				
				sess_ptr->se_dcid 		= hdr_ptr->c_dcid;
				sess_ptr->se_clt_nodeid	= hdr_ptr->c_snode;
				sess_ptr->se_clt_ep		= hdr_ptr->c_src;
				sess_ptr->se_clt_PID	= hdr_ptr->c_pid;
				
				sess_ptr->se_lbclt_ep	= hdr_ptr->c_dst;
				sess_ptr->se_lbsvr_ep	= hdr_ptr->c_src; 	// could be different

				sess_ptr->se_svr_nodeid	= NODE2;	
				sess_ptr->se_svr_ep		= hdr_ptr->c_dst;
				sess_ptr->se_svr_PID	= LB_INVALID; 				

			} else {
				PXYDEBUG("SPROXY(%d):  Existing Session with remote endpoint %d\n", 
					px_ptr->px_nodeid, hdr_ptr->c_dst);				
			}
			
			MTX_UNLOCK(sess_table[dcid].st_mutex);
		}
		
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

		PXYDEBUG("SPROXY(%d): %d "CMD_FORMAT,px_ptr->px_nodeid
			, px_ptr->px_sdesc.td_tid, CMD_FIELDS(px_ptr->px_sdesc.td_header)); 
		PXYDEBUG("SPROXY(%d): %d "CMD_XFORMAT,px_ptr->px_nodeid
			,px_ptr->px_sdesc.td_tid, CMD_XFIELDS(px_ptr->px_sdesc.td_header)); 
		PXYDEBUG("SPROXY(%d): %d "CMD_TSFORMAT,px_ptr->px_nodeid
			, px_ptr->px_sdesc.td_tid, CMD_TSFIELDS(px_ptr->px_sdesc.td_header)); 
		PXYDEBUG("SPROXY(%d): %d "CMD_PIDFORMAT,px_ptr->px_nodeid
			, px_ptr->px_sdesc.td_tid, CMD_PIDFIELDS(px_ptr->px_sdesc.td_header)); 
			
		//------------------------ BATCHEABLE COMMAND -------------------------------
		if (px_ptr->px_batch == YES) {
			if(  (px_ptr->px_sdesc.td_header->c_cmd != CMD_COPYIN_DATA )     // is a batcheable command ??
			  && (px_ptr->px_sdesc.td_header->c_cmd != CMD_COPYOUT_DATA)){	// YESSSSS
				if ( (px_ptr->px_sdesc.td_header->c_cmd  == CMD_SEND_MSG) 
					||(px_ptr->px_sdesc.td_header->c_cmd == CMD_SNDREC_MSG)
					||(px_ptr->px_sdesc.td_header->c_cmd == CMD_REPLY_MSG)){
					m_ptr = &px_ptr->px_sdesc.td_header->c_msg;
					PXYDEBUG("SPROXY(%d): " MSG1_FORMAT, 
						px_ptr->px_nodeid, MSG1_FIELDS(m_ptr));
				}			
				// store original header into batched header 
				memcpy( (char*) px_ptr->px_sdesc.td_header3, px_ptr->px_sdesc.td_header, sizeof(proxy_hdr_t));
				bat_vect = (proxy_hdr_t*) px_ptr->px_sdesc.td_batch;
				px_ptr->px_sdesc.td_batch_nr = 0;		
				do{
					PXYDEBUG("SPROXY(%d): %d Getting more messages px_ptr->px_sdesc.td_batch_nr=%d \n", 
						px_ptr->px_nodeid, px_ptr->px_sdesc.td_tid, px_ptr->px_sdesc.td_batch_nr);
					ret = dvk_get2rmt_T(px_ptr->px_sdesc.td_header2, px_ptr->px_sdesc.td_payload2, TIMEOUT_NOWAIT);            
					if( ret < OK) {
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
				
					if(  (px_ptr->px_sdesc.td_header2->c_cmd == CMD_COPYIN_DATA )    // is a batcheable command ??
						|| (px_ptr->px_sdesc.td_header2->c_cmd == CMD_COPYOUT_DATA)){// NOOOOOOO
						PXYDEBUG("SPROXY(%d): next cmd=%d\n", px_ptr->px_nodeid, px_ptr->px_sdesc.td_header2->c_cmd);
						px_ptr->px_sdesc.td_cmd_flag = CMD_PENDING;
						break;
					}
					
					// Get another command 
					memcpy( (char*) &bat_vect[px_ptr->px_sdesc.td_batch_nr], (char*) px_ptr->px_sdesc.td_header2, sizeof(proxy_hdr_t));
					bat_ptr = &bat_vect[px_ptr->px_sdesc.td_batch_nr];
					PXYDEBUG("SPROXY(%d): bat_vect[%d]:" CMD_FORMAT,	
						px_ptr->px_nodeid, px_ptr->px_sdesc.td_batch_nr, CMD_FIELDS(bat_ptr));
					PXYDEBUG("SPROXY(%d): new BATCHED COMMAND px_ptr->px_sdesc.td_batch_nr=%d\n", 
						px_ptr->px_nodeid ,px_ptr->px_sdesc.td_batch_nr);		
					px_ptr->px_sdesc.td_batch_nr++;		
				}while ( px_ptr->px_sdesc.td_batch_nr < MAXBATCMD);
				PXYDEBUG("SPROXY(%d): px_ptr->px_sdesc.td_batch_nr=%d ret=%d\n", 
					px_ptr->px_nodeid ,px_ptr->px_sdesc.td_batch_nr ,ret);				
			}
		} else {
			px_ptr->px_sdesc.td_batch_nr = 0;
		}
	
		if ((px_ptr->px_batch == YES) 
		&&  ( px_ptr->px_sdesc.td_batch_nr > 0)) { 			// is batching in course??	
			PXYDEBUG("SPROXY(%d): sending BATCHED COMMANDS px_ptr->px_sdesc.td_batch_nr=%d\n",
				px_ptr->px_nodeid , px_ptr->px_sdesc.td_batch_nr);
			px_ptr->px_sdesc.td_header3->c_flags 	  = 0;
			SET_BIT(px_ptr->px_sdesc.td_header3->c_flags,FLAG_BATCH_BIT);
			px_ptr->px_sdesc.td_header3->c_batch_nr = px_ptr->px_sdesc.td_batch_nr;
			px_ptr->px_sdesc.td_header3->c_len      = px_ptr->px_sdesc.td_batch_nr * sizeof(proxy_hdr_t);
			rcode =  ps_send_remote(px_ptr, px_ptr->px_sdesc.td_header3, px_ptr->px_sdesc.td_batch);
			px_ptr->px_sdesc.td_batch_nr = 0;		// nr_cmd the number of batching commands in the buffer
			if (rcode == 0) {
				px_ptr->px_sdesc.td_msg_ok++;
				px_ptr->px_snd_seq++;					
			}else{
				px_ptr->px_sdesc.td_msg_fail++;
				ERROR_RETURN(rcode);
			}
		} else {
			PXYDEBUG("SPROXY(%d):" CMD_FORMAT, px_ptr->px_nodeid, 
				CMD_FIELDS(px_ptr->px_sdesc.td_header));
			PXYDEBUG("SPROXY(%d):" CMD_XFORMAT, px_ptr->px_nodeid, 
				CMD_XFIELDS(px_ptr->px_sdesc.td_header));
			rcode =  ps_send_remote(px_ptr, px_ptr->px_sdesc.td_header, px_ptr->px_sdesc.td_payload);
			if (rcode == 0) {
				px_ptr->px_sdesc.td_msg_ok++;
				px_ptr->px_snd_seq++;					
			}else {
				px_ptr->px_sdesc.td_msg_fail++;
			}
		}

		// now, send the non batcheable command plus payload 
		if( px_ptr->px_sdesc.td_cmd_flag == CMD_PENDING) { 
			rcode =  ps_send_remote(px_ptr, px_ptr->px_sdesc.td_header2, px_ptr->px_sdesc.td_payload2);
			px_ptr->px_sdesc.td_cmd_flag = 0;
			if (rcode == 0) {
				px_ptr->px_sdesc.td_msg_ok++;
				px_ptr->px_snd_seq++;					
			}else{
				px_ptr->px_sdesc.td_msg_fail++;
				ERROR_RETURN(rcode);
			}
		}
	}
	MTX_UNLOCK(px_ptr->px_sdesc.td_mtx);

    /* never reached */
	if( px_ptr->px_compress == YES )
		stop_compression(px_ptr);
	
    exit(1);
}

/* ps_connect_to_remote: connects to the remote receiver */
int ps_connect_to_remote(proxy_t *px_ptr) 
{
    int port_no, rcode, i;
    char rmt_ipaddr[INET_ADDRSTRLEN+1];

    port_no = px_ptr->px_sport;
	PXYDEBUG("SPROXY(%d): for node %s running at port=%d\n", 
		px_ptr->px_nodeid, px_ptr->px_name, port_no);    

	// Connect to the server client	
    px_ptr->px_rmtserver_addr.sin_family = AF_INET;  
    px_ptr->px_rmtserver_addr.sin_port = htons(port_no);  

    px_ptr->px_rmthost = gethostbyname(px_ptr->px_name);
	if( px_ptr->px_rmthost == NULL) ERROR_EXIT(h_errno);
	for( i =0; px_ptr->px_rmthost->h_addr_list[i] != NULL; i++) {
		PXYDEBUG("SPROXY(%d): remote host address %i: %s\n", px_ptr->px_nodeid, 
			i, inet_ntoa( *( struct in_addr*)(px_ptr->px_rmthost->h_addr_list[i])));
	}

    if((inet_pton(AF_INET,inet_ntoa( *( struct in_addr*)(px_ptr->px_rmthost->h_addr_list[0])), 
		(struct sockaddr*) &px_ptr->px_rmtserver_addr.sin_addr)) <= 0)
    	ERROR_RETURN(-errno);

    inet_ntop(AF_INET, (struct sockaddr*) &px_ptr->px_rmtserver_addr.sin_addr, rmt_ipaddr, INET_ADDRSTRLEN);
	PXYDEBUG("SPROXY(%d): for node %s running at  IP=%s\n", 
		px_ptr->px_nodeid, px_ptr->px_name, rmt_ipaddr);    

	rcode = connect(px_ptr->px_sproxy_sd, (struct sockaddr *) &px_ptr->px_rmtserver_addr, sizeof(px_ptr->px_rmtserver_addr));
    if (rcode != 0) ERROR_RETURN(-errno);
    return(OK);
}

int ps_allocmem(proxy_t *px_ptr) 
{
	int rcode; 
	
	posix_memalign( (void**) &px_ptr->px_sdesc.td_header, getpagesize(), (sizeof(proxy_hdr_t)));
	if (px_ptr->px_sdesc.td_header== NULL) {
    		perror("px_sdesc.td_header posix_memalign");
    		exit(1);
  	}
	PXYDEBUG("SPROXY(%d): px_sdesc.td_header=%X\n", px_ptr->px_nodeid, px_ptr->px_sdesc.td_header);  

	posix_memalign( (void**) &px_ptr->px_sdesc.td_header2, getpagesize(), (sizeof(proxy_hdr_t)));
	if (px_ptr->px_sdesc.td_header== NULL) {
    		perror("px_sdesc.td_header2 posix_memalign");
    		exit(1);
  	}
	PXYDEBUG("SPROXY(%d): px_sdesc.td_header2=%X\n", px_ptr->px_nodeid, px_ptr->px_sdesc.td_header2);  

	posix_memalign( (void**) &px_ptr->px_sdesc.td_header3, getpagesize(), (sizeof(proxy_hdr_t)));
	if (px_ptr->px_sdesc.td_header== NULL) {
    		perror("px_sdesc.td_header3 posix_memalign");
    		exit(1);
  	}
	PXYDEBUG("SPROXY(%d): px_sdesc.td_header3=%X\n", px_ptr->px_nodeid, px_ptr->px_sdesc.td_header3);  
	
	posix_memalign( (void**) &px_ptr->px_sdesc.td_payload, getpagesize(), (sizeof(proxy_payload_t)));
	if (px_ptr->px_sdesc.td_payload== NULL) {
    		perror("px_sdesc.td_payload posix_memalign");
    		exit(1);
  	}
	PXYDEBUG("SPROXY(%d): px_sdesc.td_payload=%X\n", px_ptr->px_nodeid, px_ptr->px_sdesc.td_payload);  

	posix_memalign( (void**) &px_ptr->px_sdesc.td_payload2, getpagesize(), (sizeof(proxy_payload_t)));
	if (px_ptr->px_sdesc.td_payload== NULL) {
    		perror("px_sdesc.td_payload2 posix_memalign");
    		exit(1);
  	}
	PXYDEBUG("SPROXY(%d): px_sdesc.td_payload2=%X\n", px_ptr->px_nodeid, px_ptr->px_sdesc.td_payload2);  
	
	if( px_ptr->px_batch == YES) {
		posix_memalign( (void**) &px_ptr->px_sdesc.td_batch, getpagesize(), (sizeof(proxy_payload_t)));
		if (px_ptr->px_sdesc.td_payload== NULL) {
				perror("px_batch posix_memalign");
				exit(1);
		}
	}
	PXYDEBUG("SPROXY(%d): px_sdesc.td_batch=%X\n", px_ptr->px_nodeid, px_ptr->px_sdesc.td_batch);  
	return(OK);
}

void *ps_thread(void *arg) 
{
    int rcode = 0, ret = 0;
	proxy_t *px_ptr;
    struct sigaction sa;
	struct timeval timeout;      

    timeout.tv_sec = MP_SEND_TIMEOUT;
    timeout.tv_usec = 0;
	
	sa.sa_handler 	= sig_alrm;
	sa.sa_flags 	= SA_RESTART;
	if(sigaction(SIGALRM, &sa, NULL) < 0){
		ERROR_EXIT(-errno);
	}	
	
	px_ptr= (proxy_t*) arg;
	PXYDEBUG("SPROXY(%d): Initializing...\n", px_ptr->px_nodeid);
		
	/* Lock mutex... */
	MTX_LOCK(px_ptr->px_sdesc.td_mtx);
	/* Get and save TID and ready flag.. */
	px_ptr->px_sdesc.td_tid = syscall(SYS_gettid);
	/* and signal main thread that we're ready */
	COND_SIGNAL(px_ptr->px_sdesc.td_cond);
	/* wait MAIN signal */
	COND_WAIT(px_ptr->px_sdesc.td_tcond, px_ptr->px_sdesc.td_mtx);	
	/* ..then unlock when we're done. */
	MTX_UNLOCK(px_ptr->px_sdesc.td_mtx);
	
	PXYDEBUG("SPROXY(%d): td_tid=%d\n", 
		px_ptr->px_nodeid, px_ptr->px_sdesc.td_tid);
		
	px_ptr->px_snd_seq = 0;

	// ALLOCATE MEMORY BUFFERS
	rcode = ps_allocmem(px_ptr);
	
    // Create socket.
	px_ptr->px_sproxy_sd = LB_INVALID;
    if ( (px_ptr->px_sproxy_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
       	ERROR_EXIT(errno)
    }

#ifdef ANULADO	
	rcode = setsockopt (px_ptr->px_sproxy_sd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
	if (rcode < 0) ERROR_EXIT(-errno);
#endif // ANULADO	
	
#if PWS_ENABLED 	
	px_ptr->px_pws_port = (BASE_PWS_SPORT + px_ptr->px_nodeid);
	rcode = pthread_create( &px_ptr->px_pws_pth, NULL, main_pws, px_ptr );
	if(rcode) ERROR_EXIT(rcode);
	PXYDEBUG("SPROXY(%d):px_ptr->px_pws_pth=%u\n",px_ptr->px_nodeid,px_ptr->px_pws_pth);
	
	/* try to connect many times */
	px_ptr->px_sdesc.td_batch_nr = 0;		// number of batching commands
	px_ptr->px_sdesc.td_cmd_flag = 0;		// signals a command to be sent 

	MTX_LOCK(px_ptr->px_mutex);
	COND_WAIT(px_ptr->px_sdesc.td_cond, px_ptr->px_mutex);
	COND_SIGNAL(px_ptr->px_sdesc.td_pws_cond);	
	MTX_UNLOCK(px_ptr->px_mutex);
#endif //PWS_ENABLED 	

		
	while(TRUE) {
		// TRY to connnec to remote node 
		if( px_ptr->px_nodeid != lb_ptr->lb_nodeid) {
			do {
				if (rcode < 0)
					PXYDEBUG("SPROXY(%d): Could not connect to %d"
									" Sleeping for a while...\n",px_ptr->px_nodeid, px_ptr->px_nodeid);
				sleep(MP_TIMEOUT_5SEC);

				rcode = ps_connect_to_remote(px_ptr);
			} while (rcode != 0);
		}
		
		// Synchronize connection with RPROXY 
		MTX_LOCK(px_ptr->px_conn_mtx);
		// EJECUTAR UN JOIN SI ES LA PRIMERA VEZ QUE SE CONECTA - ESTO SE SABE CON EL NRO DE SECUENCIA !!!!!! 
		rcode = dvk_proxy_conn(px_ptr->px_nodeid, CONNECT_SPROXY);
		SET_BIT(px_ptr->px_status, PX_BIT_SCONNECTED);
		SET_BIT(lb_ptr->lb_bm_sconn, px_ptr->px_nodeid);
		if( SET_BIT(px_ptr->px_status, PX_BIT_RCONNECTED)){ // if RPROXY is just connected, wakeup it 
			COND_SIGNAL(px_ptr->px_conn_rcond);
		}else {
			COND_WAIT(px_ptr->px_conn_scond, px_ptr->px_conn_mtx); // wait until RPROXY is connected 
		}
		// UP px_conn_mtx and wait for a connection failure
		MTX_UNLOCK(px_ptr->px_conn_mtx);
		PXYDEBUG("SPROXY(%d): px_status=%X\n",px_ptr->px_nodeid, px_ptr->px_status);

		// READY, CONNECTED, start serving 
		ret = ps_start_serving(px_ptr);
			
		// Something WRONG has occurred. SPROXY not connected 	
		PXYDEBUG("SPROXY(%d):Something WRONG has occurred. SPROXY not connected\n",px_ptr->px_nodeid);
		MTX_LOCK(px_ptr->px_conn_mtx);
		// EJECUTAR UN LEAVE DE APLICACION !!!!!! 
		// SIGNAL AL SPROXY PARA QUE DESCONNECTE 
		CLR_BIT(px_ptr->px_status, PX_BIT_SCONNECTED);
		CLR_BIT(lb_ptr->lb_bm_sconn, px_ptr->px_nodeid);
		rcode = dvk_proxy_conn(px_ptr->px_nodeid, DISCONNECT_SPROXY);
		MTX_UNLOCK(px_ptr->px_conn_mtx);		
		PXYDEBUG("SPROXY(%d): px_status=%X\n",px_ptr->px_nodeid, px_ptr->px_status);
	
		// Close the communication SOCKET 
		if( px_ptr->px_nodeid != lb_ptr->lb_nodeid) {
			PXYDEBUG("SPROXY(%d): close=%d\n",px_ptr->px_nodeid ,px_ptr->px_sproxy_sd);		
			close(px_ptr->px_sproxy_sd);
		}
	}
    /* code never reaches here */
}
