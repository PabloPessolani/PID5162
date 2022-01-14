/********************************************************/
/* 	MULTI-NODE MULTI-PROTOCOL PROXY 			*/
/*   BATCHING MESSAGES							*/
/*   DATA BLOCKS COMPRESSION 					*/
/*   AUTOMATIC CLIENT BINDING  					*/
/*  Kernel with support of Timestamp and sender PID insertion	*/
/********************************************************/

#define _TABLE

#include "proxy.h"
#include "debug.h"
#include "macros.h"
#include "multi_proxy.h"
#include "multi_glo.h"

extern int errno;

#define LZ4_HEADER_SIZE 19
#define LZ4_FOOTER_SIZE 4
#define BLOCK_16K	(16 * 1024)

static const LZ4F_preferences_t lz4_preferences = {
	{ LZ4F_max1MB, LZ4F_blockLinked, LZ4F_noContentChecksum, LZ4F_frame, 0, { 0, 0 } },
	0,   /* compression level */
	0,   /* autoflush */
	{ 0, 0, 0, 0 },  /* reserved, must be set to 0 */
};

int local_nodeid;

__attribute__((packed, aligned(4)))

clockid_t clk_id = CLOCK_REALTIME;

static char pws_buffer[PWS_BUFSIZE+1]; /* static so zero filled */
static char pws_html[PWS_BUFSIZE+1]; /* static so zero filled */
static char tmp_buffer[1024]; 

dvs_usr_t dvs;   
proxies_usr_t px;

void main_pws(void *arg_port);
void wdmp_px_stats(proxy_t *px_ptr);

#if PX_ADD_TIMESTAMP
int timespec2str(char *buf, uint len, struct timespec *ts) {
    int ret;
    struct tm t;

    tzset();
    if (localtime_r(&(ts->tv_sec), &t) == NULL)
        return 1;

    ret = strftime(buf, len, "%F %T", &t);
    if (ret == 0)
        return 2;
    len -= ret - 1;

    ret = snprintf(&buf[strlen(buf)], len, ".%09ld", ts->tv_nsec);
    if (ret >= len)
        return 3;

    return 0;
}
#endif // PX_ADD_TIMESTAMP

/*----------------------------------------------*/
/*      PROXY RECEIVER FUNCTIONS               */
/*----------------------------------------------*/
int decompress_payload( proxy_t *px_ptr, proxy_hdr_t *hd_ptr, proxy_payload_t *pl_ptr)
{
	LZ4F_errorCode_t lz4_rcode;
	size_t comp_len, raw_len;
	
	PXYDEBUG("RPROXY(%d): INPUT maxRsize=%d maxCsize=%d \n", px_ptr->px_proxyid, 
		px_ptr->px_rdesc.td_maxRsize, px_ptr->px_rdesc.td_maxCsize );

	comp_len = hd_ptr->c_len;
	raw_len  = px_ptr->px_rdesc.td_maxRsize;
	PXYDEBUG("RPROXY(%d): INPUT comp_len=%d  MAX raw_len=%d \n", px_ptr->px_proxyid,
		comp_len, raw_len );

	lz4_rcode = LZ4F_decompress(px_ptr->px_lz4Dctx,
                          px_ptr->px_rdesc.td_decomp_pl, &raw_len,
                          pl_ptr, &comp_len,
                          NULL);
		
	if (LZ4F_isError(lz4_rcode)) {
		fprintf(stderr ,"LZ4F_decompress failed: error %zu", lz4_rcode);
		ERROR_EXIT(lz4_rcode);
	}
	
	PXYDEBUG("RPROXY(%d): OUTPUT raw_len=%d comp_len=%d\n",	px_ptr->px_proxyid, raw_len, comp_len);
	return(raw_len);					  
						  
}
        
void init_decompression( proxy_t *px_ptr) 
{
	LZ4F_errorCode_t lz4_rcode;

	PXYDEBUG("RPROXY(%d):\n", px_ptr->px_proxyid);

	px_ptr->px_rdesc.td_maxRsize = sizeof(proxy_payload_t);
	
	px_ptr->px_rdesc.td_maxCsize =  px_ptr->px_rdesc.td_maxRsize + LZ4_HEADER_SIZE + LZ4_FOOTER_SIZE;
	posix_memalign( (void**) &px_ptr->px_rdesc.td_decomp_pl, getpagesize(), px_ptr->px_rdesc.td_maxCsize );
	if (px_ptr->px_rdesc.td_decomp_pl== NULL) {
    		fprintf(stderr, "px_ptr->px_rdesc.td_decomp_pl posix_memalign");
			ERROR_EXIT(errno);
  	}
	PXYDEBUG("RPROXY(%d): px_rdesc.td_decomp_pl=%X\n", px_ptr->px_proxyid, px_ptr->px_rdesc.td_decomp_pl);
	
	lz4_rcode =  LZ4F_createDecompressionContext(&px_ptr->px_lz4Dctx, LZ4F_VERSION);
	if (LZ4F_isError(lz4_rcode)) {
		fprintf(stderr ,"LZ4F_createDecompressionContext: error %zu", lz4_rcode);
		ERROR_EXIT(lz4_rcode);
	}
	PXYDEBUG("RPROXY(%d): px_lz4Dctx=%X\n", px_ptr->px_proxyid, px_ptr->px_lz4Dctx);
}

void stop_decompression( proxy_t *px_ptr) 
{
	LZ4F_errorCode_t lz4_rcode;

	PXYDEBUG("RPROXY(%d):\n", px_ptr->px_proxyid);

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

    port_no = px_ptr->px_rport;
	PXYDEBUG("RPROXY(%d): for node %s running at port=%d\n", 
			px_ptr->px_proxyid, px_ptr->px_name, port_no);

    // Create server socket.
    if ( (px_ptr->px_rlisten_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
        ERROR_EXIT(errno);

    if( (ret = setsockopt(px_ptr->px_rlisten_sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) < 0)
       	ERROR_EXIT(errno);

    // Bind (attach) this process to the server socket.
    servaddr.sin_family = AF_INET;
	if( !strncmp(px_ptr->px_rname,"ANY", 3)){
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	}else{
		h_ptr = gethostbyname(px_ptr->px_rname);
		if( h_ptr == NULL) ERROR_EXIT(h_errno);
		unsigned int i=0;
		while ( h_ptr->h_addr_list[i] != NULL) {
          PXYDEBUG("RPROXY(%d): %s \n", px_ptr->px_proxyid, 
			inet_ntoa( *( struct in_addr*)( h_ptr->h_addr_list[i])));
          i++;
		}  
		PXYDEBUG("RPROXY(%d): px_rname=%s listening on IP address=%s\n", 
			px_ptr->px_proxyid, px_ptr->px_rname, inet_ntoa( *( struct in_addr*)(h_ptr->h_addr)));		
		servaddr.sin_addr.s_addr = inet_addr(inet_ntoa( *( struct in_addr*)(h_ptr->h_addr)));
	}
    servaddr.sin_port = htons(port_no);
	ret = bind(px_ptr->px_rlisten_sd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if(ret < 0) ERROR_EXIT(errno);

	PXYDEBUG("RPROXY(%d): is bound to port=%d socket=%d\n", 
			px_ptr->px_proxyid, port_no, px_ptr->px_rlisten_sd);

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

   	PXYDEBUG("RPROXY(%d): pl_size=%d\n",px_ptr->px_proxyid, pl_size);
   	p_ptr = (char*) pl_ptr;
   	while ((n = recv(px_ptr->px_rconn_sd, p_ptr, (pl_size-received), 0 )) > 0) {
	   	PXYDEBUG("RPROXY(%d): recv=%d\n",px_ptr->px_proxyid, n);
        received = received + n;
		PXYDEBUG("RPROXY(%d): n:%d | received:%d\n",px_ptr->px_proxyid , n,received);
        if (received >= pl_size) return(OK);
       	p_ptr += n;
   	}
    
    if(n < 0) ERROR_RETURN(-errno);
 	return(OK);
}

/* pr_receive_header: receives the header from remote sender */
int pr_receive_header(proxy_t *px_ptr) 
{
    int n, total,  received = 0;
    char *p_ptr;

   	PXYDEBUG("RPROXY(%d): socket=%d\n",px_ptr->px_proxyid , px_ptr->px_rconn_sd);
   	p_ptr = (char*) px_ptr->px_rdesc.td_header;
	total = sizeof(proxy_hdr_t);
   	while ((n = recv(px_ptr->px_rconn_sd, p_ptr, (total-received), 0 )) > 0) {
        received = received + n;
		PXYDEBUG("RPROXY(%d): n:%d | received:%d | HEADER_SIZE:%d\n", 
					px_ptr->px_proxyid, n,received,sizeof(proxy_hdr_t));
        if (received >= sizeof(proxy_hdr_t)) {  
			PXYDEBUG("RPROXY(%d): " CMD_FORMAT,px_ptr->px_proxyid, 
				CMD_FIELDS(px_ptr->px_rdesc.td_header));
			PXYDEBUG("RPROXY(%d):" CMD_XFORMAT,px_ptr->px_proxyid, 
				CMD_XFIELDS(px_ptr->px_rdesc.td_header));
        	return(OK);
        } else {
			PXYDEBUG("RPROXY(%d): Header partially received. There are %d bytes still to get\n", 
                  	px_ptr->px_proxyid, sizeof(proxy_hdr_t) - received);
        	p_ptr += n;
        }
   	}
    if( n < 0)  ERROR_RETURN(-errno);
	return(OK);
}

/* pr_process_message: receives header and payload if any. Then deliver the 
 * message to local */
int pr_process_message(proxy_t *px_ptr) 
{
    int rcode, payload_size, i, ret, raw_len;
 	message *m_ptr;
	proxy_hdr_t *bat_cmd, *bat_ptr;
	dc_usr_t dcu, *dcu_ptr;

    do {
		bzero(px_ptr->px_rdesc.td_header, sizeof(proxy_hdr_t));
		PXYDEBUG("RPROXY(%d): About to receive header\n",px_ptr->px_proxyid);
    	rcode = pr_receive_header(px_ptr);
    	if (rcode != 0) ERROR_RETURN(rcode);

		if( px_ptr->px_rdesc.td_header->c_cmd != CMD_NONE) {
			PXYDEBUG("RPROXY(%d):" CMD_FORMAT,px_ptr->px_proxyid,
				CMD_FIELDS(px_ptr->px_rdesc.td_header));
			PXYDEBUG("RPROXY(%d):" CMD_XFORMAT,px_ptr->px_proxyid,
				CMD_XFIELDS(px_ptr->px_rdesc.td_header));
			PXYDEBUG("RPROXY(%d): %d "CMD_TSFORMAT,px_ptr->px_proxyid, 
				px_ptr->px_rdesc.td_tid, CMD_TSFIELDS(px_ptr->px_rdesc.td_header)); 
			PXYDEBUG("RPROXY(%d): %d "CMD_PIDFORMAT,px_ptr->px_proxyid, 
				px_ptr->px_rdesc.td_tid, CMD_PIDFIELDS(px_ptr->px_rdesc.td_header)); 
		} else {
			PXYDEBUG("RPROXY(%d): %d NONE\n",px_ptr->px_proxyid, px_ptr->px_rdesc.td_tid); 			
			PXYDEBUG("RPROXY(%d):" CMD_FORMAT,px_ptr->px_proxyid,
				CMD_FIELDS(px_ptr->px_rdesc.td_header));
		}
		
		switch(px_ptr->px_rdesc.td_header->c_cmd){
			case CMD_SEND_MSG:
			case CMD_SNDREC_MSG:
			case CMD_REPLY_MSG:
				m_ptr = &px_ptr->px_rdesc.td_header->c_msg;
				PXYDEBUG("RPROXY(%d): " MSG1_FORMAT, 
					px_ptr->px_proxyid, MSG1_FIELDS(m_ptr));
				break;
			case CMD_COPYIN_DATA:
			case CMD_COPYOUT_DATA:
				PXYDEBUG("RPROXY(%d): " VCOPY_FORMAT, 
					px_ptr->px_proxyid, VCOPY_FIELDS(px_ptr->px_rdesc.td_header)); 
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
			if( px_ptr->px_batch == YES) { 
				if(!TEST_BIT(px_ptr->px_rdesc.td_header->c_flags, FLAG_BATCH_BIT)) {
					PXYDEBUG("RPROXY(%d): put2lcl raw_len=%d\n",px_ptr->px_proxyid, raw_len);
					rcode = dvk_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_decomp_pl);
				}else{
					PXYDEBUG("RPROXY(%d): put2lcl\n",px_ptr->px_proxyid);
					rcode = dvk_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_payload);
				}
			} else {
				PXYDEBUG("RPROXY(%d): put2lcl\n",px_ptr->px_proxyid);
				rcode = dvk_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_decomp_pl);
			}
		} else{
			PXYDEBUG("RPROXY(%d): put2lcl\n",px_ptr->px_proxyid);
			rcode = dvk_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_payload);
		}
	}else {
		PXYDEBUG("RPROXY(%d): put2lcl\n",px_ptr->px_proxyid);
		rcode = dvk_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_payload);
	}

	
	if( px_ptr->px_autobind == YES) {
		
		/******************** REMOTE CLIENT BINDING ************************************/
		ret = 0;
		if( rcode < 0) {
			/* rcode: the result of the las dvk_put2lcl 	*/
			/* ret: the result of the following operations	*/
			PXYDEBUG("RPROXY(%d): REMOTE CLIENT BINDING rcode=%d\n", px_ptr->px_proxyid, rcode);
			dcu_ptr = &dcu;
			ret = dvk_getdcinfo( px_ptr->px_rdesc.td_header->c_dcid, dcu_ptr);
			if(ret < 0) ERROR_RETURN(ret);
			PXYDEBUG("RPROXY(%d):", DC_USR1_FORMAT, px_ptr->px_proxyid, DC_USR1_FIELDS(dcu_ptr));
			if( px_ptr->px_rdesc.td_header->c_src <= (dcu_ptr->dc_nr_sysprocs - dcu_ptr->dc_nr_tasks)) {
				PXYDEBUG("RPROXY(%d): src=%d <= (dc_nr_sysprocs-dc_nr_tasks) %d \n",
					px_ptr->px_proxyid , px_ptr->px_rdesc.td_header->c_src,
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
			px_ptr->px_rdesc.td_pseudo->c_dst 	            = SYSTASK(localnodeid);
			px_ptr->px_rdesc.td_pseudo->c_snode 	            = px_ptr->px_rdesc.td_header->c_snode;
			px_ptr->px_rdesc.td_pseudo->c_dnode 	            = local_nodeid;
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
			ret = dvk_put2lcl(px_ptr->px_rdesc.td_pseudo, px_ptr->px_rdesc.td_payload);
			if( ret < 0 ) {
				ERROR_PRINT(ret);
				ERROR_RETURN(rcode);
			}
		
	#endif // SYSTASK_BIND

			/* PUT2LCL retry after REMOTE CLIENT AUTOMATIC  BINDING */
			PXYDEBUG("RPROXY(%d): retry put2lcl (autobind)\n",px_ptr->px_proxyid);
			if( px_ptr->px_compress == YES) {
				if( TEST_BIT(px_ptr->px_rdesc.td_header->c_flags, FLAG_LZ4_BIT)) {
					if( px_ptr->px_batch == YES) { 
						if(!TEST_BIT(px_ptr->px_rdesc.td_header->c_flags, FLAG_BATCH_BIT)) {
							PXYDEBUG("RPROXY(%d): put2lcl raw_len=%d\n",px_ptr->px_proxyid, raw_len);
							rcode = dvk_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_decomp_pl);
						}else{
							PXYDEBUG("RPROXY(%d): put2lcl\n",px_ptr->px_proxyid);
							rcode = dvk_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_payload);
						}
					} else {
						PXYDEBUG("RPROXY(%d): put2lcl\n",px_ptr->px_proxyid);
						rcode = dvk_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_decomp_pl);
					}
				} else{
					PXYDEBUG("RPROXY(%d): put2lcl\n",px_ptr->px_proxyid);
					rcode = dvk_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_payload);
				}
			}else {
				PXYDEBUG("RPROXY(%d): put2lcl\n",px_ptr->px_proxyid);
				rcode = dvk_put2lcl(px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_payload);
			}
			
			if( ret < 0) {
				ERROR_PRINT(ret);
				if( rcode < 0) ERROR_RETURN(rcode);
			}
			rcode = 0;
		}
	}
	
	PXYDEBUG("RPROXY(%d):" CMD_FORMAT, px_ptr->px_proxyid, 
		CMD_FIELDS(px_ptr->px_rdesc.td_header));
	PXYDEBUG("RPROXY(%d):" CMD_XFORMAT,px_ptr->px_proxyid, 
		CMD_XFIELDS(px_ptr->px_rdesc.td_header));
	
	if(px_ptr->px_batch == YES) {
		if( TEST_BIT(px_ptr->px_rdesc.td_header->c_flags, FLAG_BATCH_BIT)) {
			px_ptr->px_rdesc.td_batch_nr = px_ptr->px_rdesc.td_header->c_batch_nr;
			PXYDEBUG("RPROXY(%d): px_ptr->px_rdesc.td_batch_nr=%d\n",px_ptr->px_proxyid , px_ptr->px_rdesc.td_batch_nr);
			// check payload len
			if( (px_ptr->px_rdesc.td_batch_nr * sizeof(proxy_hdr_t)) != px_ptr->px_rdesc.td_header->c_len){
				PXYDEBUG("RPROXY(%d): batched messages \n",px_ptr->px_proxyid);
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
					px_ptr->px_proxyid, i, CMD_FIELDS(bat_ptr));
				PXYDEBUG("RPROXY(%d):" CMD_FORMAT,
					px_ptr->px_proxyid, CMD_FIELDS(bat_ptr));
				PXYDEBUG("RPROXY(%d):" CMD_XFORMAT,
					px_ptr->px_proxyid, CMD_XFIELDS(bat_ptr));
				PXYDEBUG("RPROXY(%d): %d "CMD_TSFORMAT, 
					px_ptr->px_proxyid, px_ptr->px_rdesc.td_tid, CMD_TSFIELDS(bat_ptr)); 
				PXYDEBUG("RPROXY(%d): %d "CMD_PIDFORMAT, 
					px_ptr->px_proxyid, px_ptr->px_rdesc.td_tid, CMD_PIDFIELDS(bat_ptr)); 
				m_ptr = &bat_ptr->c_msg;				
				PXYDEBUG("RPROXY(%d): " MSG1_FORMAT,px_ptr->px_proxyid, MSG1_FIELDS(m_ptr));								
				rcode = dvk_put2lcl(bat_ptr, px_ptr->px_rdesc.td_payload);
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
void pr_start_serving(proxy_t *px_ptr)
{
	int sender_addrlen;
    char ip4[INET_ADDRSTRLEN];  // space to hold the IPv4 string
    struct sockaddr_in sa;
    int rcode;

	if( px_ptr->px_compress == YES )
		init_decompression(px_ptr);
	
    while (1){
		do {
			sender_addrlen = sizeof(sa);
			PXYDEBUG("RPROXY(%d): Waiting for connection.\n",px_ptr->px_proxyid);
    		px_ptr->px_rconn_sd = accept(px_ptr->px_rlisten_sd, (struct sockaddr *) &sa, &sender_addrlen);
    		if(px_ptr->px_rconn_sd < 0) ERROR_PRINT(errno);
		}while(px_ptr->px_rconn_sd < 0);

		PXYDEBUG("RPROXY(%d): Remote sender [%s] connected on sd [%d]. Getting remote command.\n",
           		px_ptr->px_proxyid, inet_ntop(AF_INET, &(sa.sin_addr), ip4, INET_ADDRSTRLEN),
           		px_ptr->px_rconn_sd);
		rcode = dvk_proxy_conn(px_ptr->px_proxyid, CONNECT_RPROXY);

    	/* Serve Forever */
		do { 
	       	/* get a complete message and process it */
       		rcode = pr_process_message(px_ptr);
       		if (rcode == OK) {
				PXYDEBUG("RPROXY(%d): Message succesfully processed.\n",px_ptr->px_proxyid);
				px_ptr->px_rdesc.td_msg_ok++;
				px_ptr->px_ack_seq++;
        	} else {
				PXYDEBUG("RPROXY(%d): Message processing failure [%d]\n",px_ptr->px_proxyid, rcode);
            			px_ptr->px_rdesc.td_msg_fail++;
				if( rcode == EDVSNOTCONN) break;
			}	
		}while(1);

		rcode = dvk_proxy_conn(px_ptr->px_proxyid, DISCONNECT_RPROXY);
		close(px_ptr->px_rconn_sd);
   	}
	if( px_ptr->px_compress == YES )
		stop_decompression(px_ptr);
    /* never reached */
}

void *pr_thread(void *arg) 
{
    int rcode = 0;
	proxy_t *px_ptr;
	
	px_ptr= (proxy_t*) arg;
	PXYDEBUG("RPROXY(%d): Initializing...\n", px_ptr->px_proxyid);
    
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

	px_ptr->px_ack_seq = 0;
	
#ifdef ANULADO 	
	do { 
		rcode = dvk_wait4bind_T(RETRY_US);
		PXYDEBUG("RPROXY(%d): dvk_wait4bind_T  rcode=%d\n", px_ptr->px_proxyid, rcode);
		if (rcode == EDVSTIMEDOUT) {
			PXYDEBUG("RPROXY(%d): dvk_wait4bind_T TIMEOUT\n", px_ptr->px_proxyid);
			continue ;
		}else if(rcode == NONE) { /* proxies have not endpoint */
			break;	
		}else if(rcode == -EINTR) {
			continue;
		} if( rcode < 0) 
			exit(EXIT_FAILURE);
	} while	(rcode < OK);
	PXYDEBUG("RPROXY(%d): rcode=%d\n",px_ptr->px_proxyid, rcode);

	PXYDEBUG("RPROXY(%d): Initializing on TID:%d\n", px_ptr->px_proxyid, px_ptr->px_rdesc.td_tid);
	do { 
		rcode = dvk_wait4bind_T(RETRY_MS);
		PXYDEBUG("RPROXY(%d): dvk_wait4bind_T  rcode=%d\n",px_ptr->px_proxyid , rcode);
		if (rcode == EDVSTIMEDOUT) {
			PXYDEBUG("RPROXY(%d): dvk_wait4bind_T TIMEOUT\n",px_ptr->px_proxyid );
			continue ;
		}else if(rcode == NONE) { /* proxies have not endpoint */
			break;	
		} else if(rcode == -EINTR) {
			continue;
		} if( rcode < 0) 
			exit(EXIT_FAILURE);
	} while	(rcode < OK);
#endif // ANULADO 	
	
	posix_memalign( (void**) &px_ptr->px_rdesc.td_header, getpagesize(), (sizeof(proxy_hdr_t)));
	if (px_ptr->px_rdesc.td_header== NULL) {
    		perror(" px_ptr->px_rdesc.td_header posix_memalign");
    		exit(1);
  	}
	PXYDEBUG("RPROXY(%d): px_rdesc.td_header=%X\n", px_ptr->px_proxyid, px_ptr->px_rdesc.td_header);

	posix_memalign( (void**) &px_ptr->px_rdesc.td_payload, getpagesize(), (sizeof(proxy_payload_t)));
	if (px_ptr->px_rdesc.td_payload== NULL) {
    		perror(" px_ptr->px_rdesc.td_payload posix_memalign");
    		exit(1);
  	}
	PXYDEBUG("RPROXY(%d): px_rdesc.td_payload=%X\n", px_ptr->px_proxyid, px_ptr->px_rdesc.td_payload);

	posix_memalign( (void**) &px_ptr->px_rdesc.td_pseudo, getpagesize(), (sizeof(proxy_hdr_t)));
	if (px_ptr->px_rdesc.td_pseudo== NULL) {
    		perror(" px_ptr->px_rdesc.td_pseudo posix_memalign");
    		exit(1);
  	}
	PXYDEBUG("RPROXY(%d): px_rdesc.td_pseudo=%X\n", px_ptr->px_proxyid, px_ptr->px_rdesc.td_pseudo);
	
	if( px_ptr->px_batch == YES) {
		posix_memalign( (void**) &px_ptr->px_rdesc.td_batch, getpagesize(), (sizeof(proxy_payload_t)));
		if (px_ptr->px_rdesc.td_payload== NULL) {
				perror("px_ptr->px_rdesc.td_batch posix_memalign");
				exit(1);
		}
	}
	PXYDEBUG("RPROXY(%d): px_rdesc.td_batch=%X\n", px_ptr->px_proxyid, px_ptr->px_rdesc.td_batch);
	
	PXYDEBUG("RPROXY(%d): px_ptr->px_rdesc.td_header=%p px_ptr->px_rdesc.td_payload=%p diff=%d\n",
			px_ptr->px_proxyid, px_ptr->px_rdesc.td_header, px_ptr->px_rdesc.td_payload, 
			((char*) px_ptr->px_rdesc.td_payload - (char*) px_ptr->px_rdesc.td_header));

#if PWS_ENABLED 	
	px_ptr->px_pws_port = (BASE_PWS_RPORT +  px_ptr->px_proxyid);
	rcode = pthread_create( &px_ptr->px_pws_pth, NULL, main_pws, px_ptr );
	if(rcode) ERROR_EXIT(rcode);
	PXYDEBUG("RPROXY(%d): px_ptr->px_pws_pth=%u\n",px_ptr->px_proxyid ,px_ptr->px_pws_pth);		

	MTX_LOCK(px_ptr->px_mutex);
	COND_WAIT(px_ptr->px_rdesc.td_cond, px_ptr->px_mutex);
	COND_SIGNAL(px_ptr->px_rdesc.td_pws_cond);
	MTX_UNLOCK(px_ptr->px_mutex);
#endif // PWS_ENABLED 	
	
	px_ptr->px_rdesc.td_tid = px_ptr->px_rdesc.td_tid;
    if( pr_setup_connection(px_ptr) == OK) {
      	pr_start_serving(px_ptr);
 	} else {
       	ERROR_EXIT(errno);
    }
}

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
		px_ptr->px_proxyid, hd_ptr, pl_ptr);
		
	comp_len = LZ4F_compressFrame(
				px_ptr->px_sdesc.td_comp_pl, px_ptr->px_sdesc.td_maxCsize,
				pl_ptr,	hd_ptr->c_len, 
				NULL);
				
	if (LZ4F_isError(comp_len)) {
		fprintf(stderr ,"LZ4F_compressFrame failed: error %zu", comp_len);
		ERROR_RETURN(comp_len);
	}

	PXYDEBUG("SPROXY(%d): raw_len=%d comp_len=%d\n", 
		px_ptr->px_proxyid, hd_ptr->c_len, comp_len);
		
	return(comp_len);
}

void init_compression( proxy_t *px_ptr) 
{
	size_t frame_size;

	PXYDEBUG("SPROXY(%d):\n", px_ptr->px_proxyid);

	px_ptr->px_sdesc.td_maxRsize = sizeof(proxy_payload_t);
	frame_size = LZ4F_compressBound(sizeof(proxy_payload_t), &lz4_preferences);
	px_ptr->px_sdesc.td_maxCsize =  frame_size + LZ4_HEADER_SIZE + LZ4_FOOTER_SIZE;
	posix_memalign( (void**) &px_ptr->px_sdesc.td_comp_pl, getpagesize(), px_ptr->px_sdesc.td_maxCsize );
	if (px_ptr->px_sdesc.td_comp_pl == NULL) {
    		fprintf(stderr, "px_ptr->px_sdesc.td_comp_pl posix_memalign");
			ERROR_EXIT(errno);
  	}
	PXYDEBUG("SPROXY(%d):td_comp_pl=%X\n", px_ptr->px_proxyid, px_ptr->px_sdesc.td_comp_pl);
}

void stop_compression( proxy_t *px_ptr) 
{
	PXYDEBUG("SPROXY(%d):\n", px_ptr->px_proxyid);
}

/* ps_send_header: send  header to remote receiver */
int  ps_send_header(proxy_t *px_ptr, proxy_hdr_t *hd_ptr ) 
{
    int sent = 0;        // how many bytes we've sent
    int bytesleft;
    int n, total;
	char *p_ptr;

	bytesleft = sizeof(proxy_hdr_t); // how many bytes we have left to send
	total = bytesleft;
	PXYDEBUG("SPROXY(%d): send header=%d \n", px_ptr->px_proxyid, bytesleft);

    p_ptr = (char *) hd_ptr;
	PXYDEBUG("SPROXY(%d):" CMD_FORMAT, px_ptr->px_proxyid, CMD_FIELDS(hd_ptr));
	PXYDEBUG("SPROXY(%d):" CMD_XFORMAT, px_ptr->px_proxyid, CMD_XFIELDS(hd_ptr));

	hd_ptr->c_snd_seq = px_ptr->px_snd_seq;
	hd_ptr->c_ack_seq = px_ptr->px_ack_seq;
    while(sent < total) {
        n = send(px_ptr->px_sproxy_sd, p_ptr, bytesleft, 0);
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
	PXYDEBUG("SPROXY(%d): socket=%d sent header=%d \n", 
		px_ptr->px_proxyid, px_ptr->px_sproxy_sd, total);
    return(OK);
}

/* ps_send_payload: send payload to remote receiver */
int  ps_send_payload(proxy_t *px_ptr, proxy_hdr_t *hd_ptr, proxy_payload_t *pl_ptr ) 
{
    int sent = 0;        // how many bytes we've sent
    int bytesleft;
    int n, total;
	char *p_ptr;

	assert( hd_ptr->c_len > 0);
	
	bytesleft =  hd_ptr->c_len; // how many bytes we have left to send
	total = bytesleft;
	PXYDEBUG("SPROXY(%d): ps_send_payload bytesleft=%d \n", px_ptr->px_proxyid, bytesleft);

    p_ptr = (char *) pl_ptr;
    while(sent < total) {
        n = send(px_ptr->px_sproxy_sd, p_ptr, bytesleft, 0);
		PXYDEBUG("SPROXY(%d): sent=%d \n", px_ptr->px_proxyid, n);
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
	PXYDEBUG("SPROXY(%d): socket=%d sent payload=%d \n", 
		px_ptr->px_proxyid, px_ptr->px_sproxy_sd, total);
    return(OK);
}

#if PWS_ENABLED
void update_stats(proxy_t *px_ptr, proxy_hdr_t *hd_ptr, proxy_payload_t *pl_ptr, int px_type)
{
	px_stats_t *pxs_ptr;
	proxy_hdr_t *bat_cmd, *bat_ptr;
	int i;

	PXYDEBUG("PWS(%d): px_type=%d\n", px_ptr->px_proxyid, px_type);

	MTX_LOCK(px_ptr->px_mutex);
	pxs_ptr = &px_ptr->px_stat[hd_ptr->c_dcid];
	pxs_ptr->pst_nr_cmd++;
	pxs_ptr->pst_dcid 	=  hd_ptr->c_dcid;
	if( px_type == CONNECT_RPROXY){
		pxs_ptr->pst_dnode	=  hd_ptr->c_dnode;
		pxs_ptr->pst_snode	=  hd_ptr->c_snode;		
	}else{
		pxs_ptr->pst_snode	=  local_nodeid;
		pxs_ptr->pst_dnode	=  hd_ptr->c_dnode;
	}
	
	switch(hd_ptr->c_cmd){
		case CMD_COPYIN_DATA:
		case CMD_COPYOUT_DATA:
			pxs_ptr->pst_nr_data += hd_ptr->c_len;
			break;
		case CMD_SEND_MSG:
		case CMD_SNDREC_MSG:
		case CMD_REPLY_MSG:
			pxs_ptr->pst_nr_msg++;
			// fall down 
		default:	
			pxs_ptr->pst_nr_cmd += hd_ptr->c_batch_nr;
			if( hd_ptr->c_batch_nr != 0) {
				bat_cmd = (proxy_hdr_t *) pl_ptr;
				for( i = 0; i < hd_ptr->c_batch_nr; i++){
					bat_ptr = &bat_cmd[i];
					if( (bat_ptr->c_cmd == CMD_SEND_MSG)
					||  (bat_ptr->c_cmd == CMD_SNDREC_MSG)
					||  (bat_ptr->c_cmd == CMD_REPLY_MSG)){
						pxs_ptr->pst_nr_msg++;
					}		  
				}
			}			
			break;
	}
	PXYDEBUG(PXS_FORMAT, PXS_FIELDS(pxs_ptr));
	MTX_UNLOCK(px_ptr->px_mutex);
}
#endif  // PWS_ENABLED

/* 
 * ps_send_remote: send a message (header + payload if existing) 
 * to remote receiver
 */
int  ps_send_remote(proxy_t *px_ptr, proxy_hdr_t *hd_ptr, proxy_payload_t *pl_ptr ) 
{
	int rcode, comp_len;

	PXYDEBUG("SPROXY(%d):" CMD_FORMAT, px_ptr->px_proxyid, CMD_FIELDS(hd_ptr));
	PXYDEBUG("SPROXY(%d):" CMD_XFORMAT, px_ptr->px_proxyid, CMD_XFIELDS(hd_ptr));

	if( px_ptr->px_compress == YES ) {
		if( (hd_ptr->c_cmd == CMD_COPYIN_DATA) || 
			(hd_ptr->c_cmd == CMD_COPYOUT_DATA) || 
			TEST_BIT(hd_ptr->c_flags, FLAG_BATCH_BIT) ){

			PXYDEBUG("SPROXY(%d): c_len=%d\n", px_ptr->px_proxyid, hd_ptr->c_len);
			assert( hd_ptr->c_len > 0);	

			/* compress here */
			comp_len = compress_payload(px_ptr, hd_ptr, pl_ptr);
			PXYDEBUG("SPROXY(%d): c_len=%d comp_len=%d\n", 
				px_ptr->px_proxyid, hd_ptr->c_len, comp_len);
			if(comp_len < hd_ptr->c_len){
				/* change command header  */
				hd_ptr->c_len = comp_len; 
				SET_BIT(hd_ptr->c_flags, FLAG_LZ4_BIT); 
			}
			PXYDEBUG("SPROXY(%d): " CMD_XFORMAT, px_ptr->px_proxyid, CMD_XFIELDS(hd_ptr));		
		}else {
			assert( hd_ptr->c_len == 0);
		}
	} 
	
	/* send the header */
	rcode =  ps_send_header(px_ptr, hd_ptr);
	if ( rcode < 0)  ERROR_RETURN(rcode);

	if( hd_ptr->c_len > 0) {
		PXYDEBUG("SPROXY(%d): send payload len=%d\n", px_ptr->px_proxyid, hd_ptr->c_len );
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

	if( px_ptr->px_compress == YES )
		init_compression(px_ptr);
	
    while(1) {
		px_ptr->px_sdesc.td_cmd_flag = 0;
		px_ptr->px_sdesc.td_batch_nr = 0;		// nr_cmd the number of batching commands in the buffer
			
		PXYDEBUG("SPROXY(%d): Waiting a message\n", px_ptr->px_proxyid);
		ret = dvk_get2rmt(px_ptr->px_sdesc.td_header, px_ptr->px_sdesc.td_payload); 

//		px_ptr->px_sdesc.td_header->c_flags   = 0;
//		px_ptr->px_sdesc.td_header->c_snd_seq = 0;
//		px_ptr->px_sdesc.td_header->c_ack_seq = 0;
		switch(ret) {
			case OK:
				break;
			case EDVSTIMEDOUT:
				PXYDEBUG("SPROXY(%d): Sending HELLO \n", px_ptr->px_proxyid);
				px_ptr->px_sdesc.td_header->c_cmd   = CMD_NONE;
				px_ptr->px_sdesc.td_header->c_len   = 0;
				px_ptr->px_sdesc.td_header->c_rcode = 0;
				rcode =  ps_send_remote(px_ptr, px_ptr->px_sdesc.td_header, px_ptr->px_sdesc.td_payload);
				px_ptr->px_sdesc.td_cmd_flag = 0;
				if (rcode == 0) {
					px_ptr->px_sdesc.td_msg_ok++;
					px_ptr->px_snd_seq++;					
				}else{
					px_ptr->px_sdesc.td_msg_fail++;
					ERROR_RETURN(rcode);
				}
				break;
			case EDVSNOTCONN:
				ERROR_RETURN(EDVSNOTCONN);
			default:
				ERROR_RETURN(ret);
				break;
		}
		if( ret == EDVSTIMEDOUT) continue;

		PXYDEBUG("SPROXY(%d): %d "CMD_FORMAT,px_ptr->px_proxyid
			, px_ptr->px_sdesc.td_tid, CMD_FIELDS(px_ptr->px_sdesc.td_header)); 
		PXYDEBUG("SPROXY(%d): %d "CMD_XFORMAT,px_ptr->px_proxyid
			,px_ptr->px_sdesc.td_tid, CMD_XFIELDS(px_ptr->px_sdesc.td_header)); 
		PXYDEBUG("SPROXY(%d): %d "CMD_TSFORMAT,px_ptr->px_proxyid
			, px_ptr->px_sdesc.td_tid, CMD_TSFIELDS(px_ptr->px_sdesc.td_header)); 
		PXYDEBUG("SPROXY(%d): %d "CMD_PIDFORMAT,px_ptr->px_proxyid
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
						px_ptr->px_proxyid, MSG1_FIELDS(m_ptr));
				}			
				// store original header into batched header 
				memcpy( (char*) px_ptr->px_sdesc.td_header3, px_ptr->px_sdesc.td_header, sizeof(proxy_hdr_t));
				bat_vect = (proxy_hdr_t*) px_ptr->px_sdesc.td_batch;
				px_ptr->px_sdesc.td_batch_nr = 0;		
				do{
					PXYDEBUG("SPROXY(%d): %d Getting more messages px_ptr->px_sdesc.td_batch_nr=%d \n", 
						px_ptr->px_proxyid, px_ptr->px_sdesc.td_tid, px_ptr->px_sdesc.td_batch_nr);
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
						PXYDEBUG("SPROXY(%d): next cmd=%d\n", px_ptr->px_proxyid, px_ptr->px_sdesc.td_header2->c_cmd);
						px_ptr->px_sdesc.td_cmd_flag = CMD_PENDING;
						break;
					}
					
					// Get another command 
					memcpy( (char*) &bat_vect[px_ptr->px_sdesc.td_batch_nr], (char*) px_ptr->px_sdesc.td_header2, sizeof(proxy_hdr_t));
					bat_ptr = &bat_vect[px_ptr->px_sdesc.td_batch_nr];
					PXYDEBUG("SPROXY(%d): bat_vect[%d]:" CMD_FORMAT,	
						px_ptr->px_proxyid, px_ptr->px_sdesc.td_batch_nr, CMD_FIELDS(bat_ptr));
					PXYDEBUG("SPROXY(%d): new BATCHED COMMAND px_ptr->px_sdesc.td_batch_nr=%d\n", 
						px_ptr->px_proxyid ,px_ptr->px_sdesc.td_batch_nr);		
					px_ptr->px_sdesc.td_batch_nr++;		
				}while ( px_ptr->px_sdesc.td_batch_nr < MAXBATCMD);
				PXYDEBUG("SPROXY(%d): px_ptr->px_sdesc.td_batch_nr=%d ret=%d\n", 
					px_ptr->px_proxyid ,px_ptr->px_sdesc.td_batch_nr ,ret);				
			}
		} else {
			px_ptr->px_sdesc.td_batch_nr = 0;
		}
	
		if ((px_ptr->px_batch == YES) 
		&&  ( px_ptr->px_sdesc.td_batch_nr > 0)) { 			// is batching in course??	
			PXYDEBUG("SPROXY(%d): sending BATCHED COMMANDS px_ptr->px_sdesc.td_batch_nr=%d\n",
				px_ptr->px_proxyid , px_ptr->px_sdesc.td_batch_nr);
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
			PXYDEBUG("SPROXY(%d):" CMD_FORMAT, px_ptr->px_proxyid, 
				CMD_FIELDS(px_ptr->px_sdesc.td_header));
			PXYDEBUG("SPROXY(%d):" CMD_XFORMAT, px_ptr->px_proxyid, 
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

    port_no = (BASE_PORT+local_nodeid);
	PXYDEBUG("SPROXY(%d): for node %s running at port=%d\n", 
		px_ptr->px_proxyid, px_ptr->px_name, port_no);    

	// Connect to the server client	
    px_ptr->px_rmtserver_addr.sin_family = AF_INET;  
    px_ptr->px_rmtserver_addr.sin_port = htons(port_no);  

    px_ptr->px_rmthost = gethostbyname(px_ptr->px_name);
	if( px_ptr->px_rmthost == NULL) ERROR_EXIT(h_errno);
	for( i =0; px_ptr->px_rmthost->h_addr_list[i] != NULL; i++) {
		PXYDEBUG("SPROXY(%d): remote host address %i: %s\n", px_ptr->px_proxyid, 
			i, inet_ntoa( *( struct in_addr*)(px_ptr->px_rmthost->h_addr_list[i])));
	}

    if((inet_pton(AF_INET,inet_ntoa( *( struct in_addr*)(px_ptr->px_rmthost->h_addr_list[0])), 
		(struct sockaddr*) &px_ptr->px_rmtserver_addr.sin_addr)) <= 0)
    	ERROR_RETURN(-errno);

    inet_ntop(AF_INET, (struct sockaddr*) &px_ptr->px_rmtserver_addr.sin_addr, rmt_ipaddr, INET_ADDRSTRLEN);
	PXYDEBUG("SPROXY(%d): for node %s running at  IP=%s\n", 
		px_ptr->px_proxyid, px_ptr->px_name, rmt_ipaddr);    

	rcode = connect(px_ptr->px_sproxy_sd, (struct sockaddr *) &px_ptr->px_rmtserver_addr, sizeof(px_ptr->px_rmtserver_addr));
    if (rcode != 0) ERROR_RETURN(-errno);
    return(OK);
}

void *ps_thread(void *arg) 
{
    int rcode = 0, ret = 0;
	proxy_t *px_ptr;
	
	px_ptr= (proxy_t*) arg;
	PXYDEBUG("SPROXY(%d): Initializing...\n", px_ptr->px_proxyid);
    
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
	
	px_ptr->px_snd_seq = 0;
#ifdef ANULADO 	
	do { 
		rcode = dvk_wait4bind_T(RETRY_US);
		PXYDEBUG("SPROXY(%d): dvk_wait4bind_T  rcode=%d\n", px_ptr->px_proxyid, rcode);
		if (rcode == EDVSTIMEDOUT) {
			PXYDEBUG("SPROXY(%d): dvk_wait4bind_T TIMEOUT\n", px_ptr->px_proxyid);
			continue ;
		}else if(rcode == NONE) { /* proxies have not endpoint */
			break;	
		}else if(rcode == -EINTR) {
			continue;
		} if( rcode < 0) 
			exit(EXIT_FAILURE);
	} while	(rcode < OK);
	PXYDEBUG("SPROXY(%d): rcode=%d\n",px_ptr->px_proxyid, rcode);

	PXYDEBUG("SPROXY(%d): Initializing on TID:%d\n", px_ptr->px_proxyid, px_ptr->px_sdesc.td_tid);  
	do { 
		rcode = dvk_wait4bind_T(RETRY_MS);
		PXYDEBUG("SPROXY(%d): dvk_wait4bind_T  rcode=%d\n",px_ptr->px_proxyid, rcode);
		if (rcode == EDVSTIMEDOUT) {
			PXYDEBUG("SPROXY(%d): dvk_wait4bind_T TIMEOUT\n",px_ptr->px_proxyid);
			continue ;
		}else if(rcode == NONE) { /* proxies have not endpoint */
			break;	
		} else if(rcode == -EINTR) {
			continue;
		} if( rcode < 0) 
			exit(EXIT_FAILURE);
	} while	(rcode < OK);
#endif // ANULADO 	
		
	posix_memalign( (void**) &px_ptr->px_sdesc.td_header, getpagesize(), (sizeof(proxy_hdr_t)));
	if (px_ptr->px_sdesc.td_header== NULL) {
    		perror("px_sdesc.td_header posix_memalign");
    		exit(1);
  	}
	PXYDEBUG("SPROXY(%d): px_sdesc.td_header=%X\n", px_ptr->px_proxyid, px_ptr->px_sdesc.td_header);  

	posix_memalign( (void**) &px_ptr->px_sdesc.td_header2, getpagesize(), (sizeof(proxy_hdr_t)));
	if (px_ptr->px_sdesc.td_header== NULL) {
    		perror("px_sdesc.td_header2 posix_memalign");
    		exit(1);
  	}
	PXYDEBUG("SPROXY(%d): px_sdesc.td_header2=%X\n", px_ptr->px_proxyid, px_ptr->px_sdesc.td_header2);  

	posix_memalign( (void**) &px_ptr->px_sdesc.td_header3, getpagesize(), (sizeof(proxy_hdr_t)));
	if (px_ptr->px_sdesc.td_header== NULL) {
    		perror("px_sdesc.td_header3 posix_memalign");
    		exit(1);
  	}
	PXYDEBUG("SPROXY(%d): px_sdesc.td_header3=%X\n", px_ptr->px_proxyid, px_ptr->px_sdesc.td_header3);  
	
	posix_memalign( (void**) &px_ptr->px_sdesc.td_payload, getpagesize(), (sizeof(proxy_payload_t)));
	if (px_ptr->px_sdesc.td_payload== NULL) {
    		perror("px_sdesc.td_payload posix_memalign");
    		exit(1);
  	}
	PXYDEBUG("SPROXY(%d): px_sdesc.td_payload=%X\n", px_ptr->px_proxyid, px_ptr->px_sdesc.td_payload);  

	posix_memalign( (void**) &px_ptr->px_sdesc.td_payload2, getpagesize(), (sizeof(proxy_payload_t)));
	if (px_ptr->px_sdesc.td_payload== NULL) {
    		perror("px_sdesc.td_payload2 posix_memalign");
    		exit(1);
  	}
	PXYDEBUG("SPROXY(%d): px_sdesc.td_payload2=%X\n", px_ptr->px_proxyid, px_ptr->px_sdesc.td_payload2);  
	
	if( px_ptr->px_batch == YES) {
		posix_memalign( (void**) &px_ptr->px_sdesc.td_batch, getpagesize(), (sizeof(proxy_payload_t)));
		if (px_ptr->px_sdesc.td_payload== NULL) {
				perror("px_batch posix_memalign");
				exit(1);
		}
	}
	PXYDEBUG("SPROXY(%d): px_sdesc.td_batch=%X\n", px_ptr->px_proxyid, px_ptr->px_sdesc.td_batch);  

    // Create server socket.
    if ( (px_ptr->px_sproxy_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
       	ERROR_EXIT(errno)
    }
	
#if PWS_ENABLED 	
	px_ptr->px_pws_port = (BASE_PWS_SPORT + px_ptr->px_proxyid);
	rcode = pthread_create( &px_ptr->px_pws_pth, NULL, main_pws, px_ptr );
	if(rcode) ERROR_EXIT(rcode);
	PXYDEBUG("SPROXY(%d):px_ptr->px_pws_pth=%u\n",px_ptr->px_proxyid,px_ptr->px_pws_pth);
	
	/* try to connect many times */
	px_ptr->px_sdesc.td_batch_nr = 0;		// number of batching commands
	px_ptr->px_sdesc.td_cmd_flag = 0;		// signals a command to be sent 

	MTX_LOCK(px_ptr->px_mutex);
	COND_WAIT(px_ptr->px_sdesc.td_cond, px_ptr->px_mutex);
	COND_SIGNAL(px_ptr->px_sdesc.td_pws_cond);	
	MTX_UNLOCK(px_ptr->px_mutex);
#endif //PWS_ENABLED 	

	while(1) {
		do {
			if (rcode < 0)
				PXYDEBUG("SPROXY(%d): Could not connect to %d"
                    			" Sleeping for a while...\n",px_ptr->px_proxyid, px_ptr->px_proxyid);
			sleep(4);

			rcode = ps_connect_to_remote(px_ptr);
		} while (rcode != 0);
		
		rcode = dvk_proxy_conn(px_ptr->px_proxyid, CONNECT_SPROXY);
		if(rcode){
			ERROR_EXIT(rcode);
		} 
		
		ret = ps_start_serving(px_ptr);
	
		rcode = dvk_proxy_conn(px_ptr->px_proxyid, DISCONNECT_SPROXY);
		if(rcode){
			ERROR_EXIT(rcode);
		} 
		ERROR_PRINT(ret);
		close(px_ptr->px_rconn_sd);
		close(px_ptr->px_sproxy_sd);
	}


    /* code never reaches here */
}

void usage(void)
{
   	fprintf(stderr,"Usage: multi_proxy <config_file>\n");
	exit(1);
}

/*----------------------------------------------*/
/*		MAIN: 			*/
/*----------------------------------------------*/
void  main ( int argc, char *argv[] )
{
    int pid, status;
    int ret, c, i;
    dvs_usr_t *d_ptr;
	proxy_t *px_ptr;

	if( argc != 2) 	{
		usage();
		exit(EXIT_FAILURE);
	}
	
	ret = dvk_open();
	if (ret < 0)  ERROR_EXIT(ret);
	
    local_nodeid = dvk_getdvsinfo(&dvs);
	if(local_nodeid < 0) ERROR_EXIT(local_nodeid);
    d_ptr=&dvs;
	PXYDEBUG(DVS_USR_FORMAT,DVS_USR_FIELDS(d_ptr));

    pid = syscall (SYS_gettid);
	PXYDEBUG("MAIN: pid=%d local_nodeid=%d\n", pid, local_nodeid);
	
    multi_config(argv[1]);  //Reads Config File
	
    if( compress_opt == YES){
		if( (BLOCK_16K << lz4_preferences.frameInfo.blockSizeID) < MAXCOPYBUF)  {
			fprintf(stderr, "MAXCOPYBUF(%d) must be greater than (BLOCK_16K <<"
				"lz4_preferences.frameInfo.blockSizeID)(%d)\n",MAXCOPYBUF,
				(BLOCK_16K << lz4_preferences.frameInfo.blockSizeID));
			exit(1);
		}
	}

	PXYDEBUG("MAIN: nr_proxies=%d\n", nr_proxies);

	for( i = 0; i < nr_proxies; i++){
        PXYDEBUG("MAIN: i=%d \n", i);
		px_ptr = &proxy_tab[i];
		if( px_ptr->px_proxyid == PX_INVALID) {
			fprintf(stderr, "px_proxyid(%d) must not be PX_INVALID\n",i);
			ERROR_EXIT(EDVSINVAL);
			continue;
		}
        PXYDEBUG(PROXY_FORMAT, PROXY_FIELDS(px_ptr));
		
		pthread_mutex_init(&px_ptr->px_mutex, NULL);  /* init mutex */

		/* creates SENDER and RECEIVER Proxies as Treads */
		PXYDEBUG("MAIN(%d): pthread_create RPROXY\n", px_ptr->px_proxyid);
		pthread_cond_init(&px_ptr->px_rdesc.td_cond, NULL);  /* init condition */
		pthread_cond_init(&px_ptr->px_rdesc.td_tcond, NULL);  /* init condition */
		pthread_mutex_init(&px_ptr->px_rdesc.td_mtx, NULL);  /* init mutex */
		MTX_LOCK(px_ptr->px_rdesc.td_mtx); 
		if ( (ret = pthread_create(&px_ptr->px_rdesc.td_thread, NULL, pr_thread,(void*)px_ptr )) != 0) {
			ERROR_EXIT(ret);
		}
		COND_WAIT(px_ptr->px_rdesc.td_cond, px_ptr->px_rdesc.td_mtx);
		PXYDEBUG("MAIN(%d): RPROXY td_tid=%d\n", px_ptr->px_proxyid, px_ptr->px_rdesc.td_tid);

		PXYDEBUG("MAIN(%d):: pthread_create SPROXY\n", px_ptr->px_proxyid);
		pthread_cond_init(&px_ptr->px_sdesc.td_cond, NULL);  /* init condition */
		pthread_cond_init(&px_ptr->px_sdesc.td_tcond, NULL);  /* init condition */
		pthread_mutex_init(&px_ptr->px_sdesc.td_mtx, NULL);  /* init mutex */
		MTX_LOCK(px_ptr->px_sdesc.td_mtx);
		if ((ret = pthread_create(&px_ptr->px_sdesc.td_thread, NULL, ps_thread,(void*)px_ptr )) != 0) {
			ERROR_EXIT(ret);
		}
		COND_WAIT(px_ptr->px_sdesc.td_cond, px_ptr->px_sdesc.td_mtx);
		PXYDEBUG("MAIN(%d): SPROXY td_tid=%d\n", px_ptr->px_proxyid, px_ptr->px_sdesc.td_tid);
		
		/* register the proxies */
		ret = dvk_proxies_bind(px_ptr->px_name, px_ptr->px_proxyid, 
					px_ptr->px_sdesc.td_tid, px_ptr->px_rdesc.td_tid, MAXCOPYBUF);
		if( ret < 0) ERROR_EXIT(ret);

		ret= dvk_node_up(px_ptr->px_name, px_ptr->px_proxyid, px_ptr->px_proxyid);	
		if( ret < 0) ERROR_EXIT(ret);


		COND_SIGNAL(px_ptr->px_rdesc.td_tcond);
		COND_SIGNAL(px_ptr->px_sdesc.td_tcond);
		MTX_UNLOCK(px_ptr->px_rdesc.td_mtx);
		MTX_UNLOCK(px_ptr->px_sdesc.td_mtx);
	}
	
	while(1)
		sleep(5);
	
	for( i = 0; i < nr_proxies; i++){
		px_ptr = &proxy_tab[i];
		if( px_ptr->px_proxyid == PX_INVALID) continue;
        PXYDEBUG(PROXY_FORMAT, PROXY_FIELDS(px_ptr));	
		pthread_join (&px_ptr->px_rdesc.td_thread, NULL );
		pthread_join (&px_ptr->px_sdesc.td_thread, NULL );
		pthread_mutex_destroy(&px_ptr->px_rdesc.td_mtx);
		pthread_cond_destroy(&px_ptr->px_rdesc.td_cond);
		pthread_mutex_destroy(&px_ptr->px_sdesc.td_mtx);
		pthread_cond_destroy(&px_ptr->px_sdesc.td_cond);
	}
	
    exit(0);
}

#if PWS_ENABLED
/*===========================================================================*
 *				PWS web server					     *
 *===========================================================================*/
void pws_server(proxy_t *px_ptr, int fd, int hit, int px_type)
{
	int j, file_fd, buflen, len;
	long i, ret;
	char * fstr;

	PXYDEBUG("fd:%d hit:%d\n", fd, hit);

	ret =read(fd, pws_buffer,PWS_BUFSIZE); 	/* read Web request in one go */
	if(ret == 0 || ret == -1) {	/* read failure stop now */
		PXYDEBUG("Failed to read browser request: %s",fd);
	}
	if(ret > 0 && ret < PWS_BUFSIZE)	/* return code is valid chars */
		pws_buffer[ret]=0;		/* terminate the pws_buffer */
	else pws_buffer[0]=0;

	PXYDEBUG("pws_buffer:%s\n", pws_buffer);

	for(i=0;i<ret;i++)	/* remove CF and LF characters */
		if(pws_buffer[i] == '\r' || pws_buffer[i] == '\n')
			pws_buffer[i]='*';

	if( strncmp(pws_buffer,"GET ",4) && strncmp(pws_buffer,"get ",4) )
		printf("Only simple GET operation supported: %s",fd);

	for(i=4;i<PWS_BUFSIZE;i++) { /* null terminate after the second space to ignore extra stuff */
		if(pws_buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
			pws_buffer[i] = 0;
			break;
		}
	}

	/* Start building the HTTP response */
	memset(pws_buffer, 0, PWS_BUFSIZE);
	(void)strcat(pws_buffer,"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
//	(void)write(fd,pws_buffer,strlen(pws_buffer));

	/* Start building the HTML page */
	if(px_type == CONNECT_SPROXY)
		(void)strcat(pws_buffer,"<html><head><title>PWS - SENDER   PROXY  Server</title>");
	else 
		(void)strcat(pws_buffer,"<html><head><title>PWS - RECEIVER PROXY  Server</title>");
		
	/* Add some CSS style */
	(void)strcat(pws_buffer,"<style type=\"text/css\">\n");
	(void)strcat(pws_buffer,"body { font-family: Verdana,Arial,sans-serif; }\n");
	(void)strcat(pws_buffer,"body,p,blockquote,li,th,td { font-size: 10pt; }\n");
	(void)strcat(pws_buffer,"table { border-spacing: 0px; background: #E9E9F3; border: 0.5em solid #E9E9F3; }\n");
	(void)strcat(pws_buffer,"table th { text-align: left; font-weight: normal; padding: 0.1em 0.5em; border: 0px; border-bottom: 1px solid #9999AA; }\n");
	(void)strcat(pws_buffer,"table td { text-align: right; border: 0px; border-bottom: 1px solid #9999AA; border-left: 1px solid #9999AA; padding: 0.1em 0.5em; }\n");
	(void)strcat(pws_buffer,"table thead th { text-align: center; font-weight: bold; color: #6C6C9A; border-left: 1px solid #9999AA; }\n");
	(void)strcat(pws_buffer,"table th.Corner { text-align: left; border-left: 0px; }\n");
	(void)strcat(pws_buffer,"table tr.Odd { background: #F6F4E4; }\n");
	(void)strcat(pws_buffer,"</style></head>");
	
	/* Start body tag */
	(void)strcat(pws_buffer,"<body>");

	/* Add the main top table with links to the functions */
	if(px_type == CONNECT_SPROXY)
		(void)strcat(pws_buffer,"<table><thead><tr><th colspan=\"10\">SENDER Proxy Web Server .</th></tr></thead><tbody><tr>");
	else
		(void)strcat(pws_buffer,"<table><thead><tr><th colspan=\"10\">RECEIVER Proxy Web Server .</th></tr></thead><tbody><tr>");
		
	(void)strcat(pws_buffer,"</tr></tbody></table><br/>");
	
// (void)write(fd,pws_buffer,strlen(pws_buffer));
	/* Write the HTML generated in functions to network socket */
	wdmp_px_stats(px_ptr);
	if( strlen(pws_html) != 0){
		(void)strcat(pws_buffer,pws_html);
//		(void)write(fd,pws_html,strlen(pws_html));
//		PXYDEBUG("%s\n",pws_buffer);	
		PXYDEBUG("strlen(pws_buffer)=%d\n", strlen(pws_buffer));
	}
	
	/* Finish HTML code */
	(void)strcat(pws_buffer,"</body></html>");
// (void)sprintf(pws_buffer,"</body></html>");
	(void)write(fd,pws_buffer,strlen(pws_buffer));
		
}

/*===========================================================================*
 *				main_pws                                         *
 *===========================================================================*/
void main_pws(void *arg)
{
	int i, pws_pid, pws_listenfd, pws_sfd, pws_hit, px_type;
	size_t length;
	int rcode;
	proxy_t *px_ptr;

	static struct sockaddr_in pws_cli_addr; /* static = initialised to zeros */
	static struct sockaddr_in serv_addr; /* static = initialised to zeros */
	
	PXYDEBUG("PWS service starting.\n");
	pws_pid = syscall (SYS_gettid);
	PXYDEBUG("pws_pid=%d\n", pws_pid);
	
			/* Setup the network socket */
	if((pws_listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
			ERROR_PT_EXIT(errno);
		
	/* PWS service Web server px_ptr->px_pws_port */
	px_ptr = (proxy_t *) arg;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(px_ptr->px_pws_port);
	PXYDEBUG("px_ptr->px_pws_port=%d\n", px_ptr->px_pws_port);
	
	if(bind(pws_listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0){
		ERROR_PT_EXIT(errno);
	}
		
	if( listen(pws_listenfd,64) <0){
		ERROR_PT_EXIT(errno);
	}
	
	MTX_LOCK(px_ptr->px_mutex);
	if( px_ptr->px_pws_port < BASE_PWS_RPORT){
		COND_SIGNAL(px_ptr->px_sdesc.td_cond);
		px_type = CONNECT_SPROXY;
		COND_WAIT(px_ptr->px_sdesc.td_pws_cond, px_ptr->px_mutex);
	}else{
		COND_SIGNAL(px_ptr->px_rdesc.td_cond);
		px_type = CONNECT_RPROXY;
		COND_WAIT(px_ptr->px_rdesc.td_pws_cond, px_ptr->px_mutex);
	}
	MTX_UNLOCK(px_ptr->px_mutex);
	
	if(px_type == CONNECT_SPROXY){
		PXYDEBUG("Starting SENDER PWS server main loop\n");
	}else{
		PXYDEBUG("Starting RECEIVER PWS server main loop\n");
	}
	
	for(pws_hit=1; ;pws_hit++) {
		length = sizeof(pws_cli_addr);
		PXYDEBUG("Conection accept\n");
		pws_sfd = accept(pws_listenfd, (struct sockaddr *)&pws_cli_addr, &length);
		if (pws_sfd < 0){
			rcode = -errno;
			ERROR_PT_EXIT(rcode);
		}
		pws_server(px_ptr, pws_sfd, pws_hit, px_type);
		close(pws_sfd);
	}
	
	return(OK);		/* shouldn't come here */
}


/*===========================================================================*
 *				wdmp_px_stats  					    				     *
 *===========================================================================*/
void wdmp_px_stats(proxy_t *px_ptr)
{
	int n, d;
	char *page_ptr;
	px_stats_t	*pxs_ptr;

	PXYDEBUG("\n");
	page_ptr = pws_html;
	*page_ptr = 0;

	(void)strcat(page_ptr,"<table>\n");
	(void)strcat(page_ptr,"<thead>\n");
	(void)strcat(page_ptr,"<tr>\n");
	(void)strcat(page_ptr,"<th>---snode--</th>\n");
	(void)strcat(page_ptr,"<th>---dnode--</th>\n");
	(void)strcat(page_ptr,"<th>---dcid---</th>\n");
	(void)strcat(page_ptr,"<th>--nr_msgs-</th>\n");
	(void)strcat(page_ptr,"<th>--nr_data-</th>\n");
	(void)strcat(page_ptr,"<th>--nr_cmd--</th>\n");
	(void)strcat(page_ptr,"</tr>\n");
	(void)strcat(page_ptr,"</thead>\n");
	(void)strcat(page_ptr,"<tbody>\n");

	MTX_LOCK(px_ptr->px_mutex);
	for(n = 0; n < (dvs.d_nr_nodes-1); n++) {
		for(d = 0; d < dvs.d_nr_dcs; d++) {
			pxs_ptr = &px_ptr->px_stat[d];
			if ((pxs_ptr->pst_nr_cmd == 0) || // have no traffic
			    (pxs_ptr->pst_snode == pxs_ptr->pst_dnode)) {  
				continue;
			}
			(void)strcat(page_ptr,"<tr>\n");
			sprintf(tmp_buffer, "<td>%10d</td> <td>%10d</td> <td>%10d</td>"
								" <td>%10ld</td> <td>%10ld</td> <td>%10ld</td>\n",
					pxs_ptr->pst_snode,
					pxs_ptr->pst_dnode,
					pxs_ptr->pst_dcid,
					pxs_ptr->pst_nr_msg,
					pxs_ptr->pst_nr_data,
					pxs_ptr->pst_nr_cmd);	
			(void)strcat(page_ptr,tmp_buffer);
			(void)strcat(page_ptr,"</tr>\n");
			}
	}
	MTX_UNLOCK(px_ptr->px_mutex);

	(void)strcat(page_ptr,"</tbody></table>\n");
   	PXYDEBUG("%s\n",page_ptr);	
	PXYDEBUG("strlen(page_ptr)=%d\n", strlen(page_ptr));	
}

#endif //  PWS_ENABLED


