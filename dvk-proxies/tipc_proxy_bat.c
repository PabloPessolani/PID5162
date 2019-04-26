/********************************************************/
/* 		TIPC PROXIES	WITH:					*/
/*   BATCHING MESSAGES		:  IMPLEMENTED/NOT TESTED	*/
/*   DATA BLOCKS COMPRESSION 					*/
/*   AUTOMATIC CLIENT BINDING  					*/
/********************************************************/

#include "proxy.h"
#include "debug.h"
#include "macros.h"

#include <lz4frame.h>
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

#define HEADER_SIZE sizeof(proxy_hdr_t)
#define SERVER_TYPE  18888
#define SERVER_INST  17
#define BASE_TYPE      	3000
#define BASE_OFFSET		1000

#define PWS_BUFSIZE 65536
#define ERROR 42
#define SORRY 43
#define LOG   44

// makes remotes clients automatic bind when they send a message to local receiver proxy 
// if local  SYSTASK is running, it does not work 
#define RMT_CLIENT_BIND	1
// it enables automating binding through SYSTASK 
// define SYSTASK_BIND 1

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
char 			*p_comp_pl;
char 			*p_decomp_pl;

LZ4F_errorCode_t p_lz4err;
size_t			p_offset;
size_t			p_maxCsize;		/* Maximum Compressed size */
size_t			p_maxRsize;		/* Maximum Raw size		 */
__attribute__((packed, aligned(4)))
LZ4F_compressionContext_t 	p_lz4Cctx __attribute__((aligned(8))); /* Compression context */
LZ4F_decompressionContext_t p_lz4Dctx __attribute__((aligned(8))); /* Decompression context */

int	batch_nr  = 0;		// number of batching commands
int	cmd_flag  = 0;		// signals a command to be sent 
int rmsg_ok   = 0;
int rmsg_fail = 0;
int smsg_ok   = 0;
int smsg_fail = 0; 
int pws_port;

pthread_mutex_t px_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t px_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t pws_cond = PTHREAD_COND_INITIALIZER;

static char pws_buffer[PWS_BUFSIZE+1]; /* static so zero filled */
static char pws_html[PWS_BUFSIZE+1]; /* static so zero filled */
static char tmp_buffer[1024]; 

dvs_usr_t dvs;   
struct hostent *rmthost;
proxies_usr_t px, *px_ptr;

struct sockaddr_tipc rmtclient_addr, rmtserver_addr;
int    rlisten_sd, rconn_sd;
int    sproxy_sd;
pthread_t pws_pth;

void main_pws(void *arg_port);
void wdmp_px_stats(void);

struct  px_stats_s {
	int					pst_snode;
	int					pst_dnode;
	int					pst_dcid;
//	int					pst_src_ep;
//	int					pst_dst_ep;
	unsigned long int 	pst_nr_msg;
	unsigned long int 	pst_nr_data;
	unsigned long int 	pst_nr_cmd;
};
typedef struct px_stats_s px_stats_t;
static px_stats_t px_stats[NR_NODES][NR_DCS];

#define PXS_FORMAT "snode=%d dnode=%d dcid=%d nr_msg=%uld nr_data=%uld nr_cmd=%uld\n" 
#define PXS_FIELDS(p) 	 p->pst_snode, p->pst_dnode, p->pst_dcid \
						, p->pst_nr_msg, p->pst_nr_data, p->pst_nr_cmd
	
/*----------------------------------------------*/
/*      PROXY RECEIVER FUNCTIONS               */
/*----------------------------------------------*/
        

int decompress_payload( proxy_hdr_t *hd_ptr, proxy_payload_t *pl_ptr)
{
	LZ4F_errorCode_t lz4_rcode;
	size_t comp_len, raw_len;
	
	PXYDEBUG("RPROXY: INPUT p_maxRsize=%d p_maxCsize=%d \n", 
		p_maxRsize, p_maxCsize );

	comp_len = hd_ptr->c_len;
	raw_len  = p_maxRsize;
	PXYDEBUG("RPROXY: INPUT comp_len=%d raw_len=%d \n", 
		comp_len, raw_len );

	lz4_rcode = LZ4F_decompress(p_lz4Dctx,
                          p_decomp_pl, &raw_len,
                          pl_ptr, &comp_len,
                          NULL);
		
	if (LZ4F_isError(lz4_rcode)) {
		fprintf(stderr ,"LZ4F_decompress failed: error %zu", lz4_rcode);
		ERROR_EXIT(lz4_rcode);
	}
	
	PXYDEBUG("RPROXY: OUTPUT raw_len=%d comp_len=%d\n",	raw_len, comp_len);
	return(raw_len);					  
						  
}
        
void init_decompression( void) 
{
	LZ4F_errorCode_t lz4_rcode;

	PXYDEBUG("RPROXY\n");

	p_maxRsize = sizeof(proxy_payload_t);
	
	p_maxCsize =  p_maxRsize + LZ4_HEADER_SIZE + LZ4_FOOTER_SIZE;
	posix_memalign( (void**) &p_decomp_pl, getpagesize(), p_maxCsize );
	if (p_decomp_pl== NULL) {
    		fprintf(stderr, "p_decomp_pl posix_memalign");
			ERROR_EXIT(errno);
  	}
	
	lz4_rcode =  LZ4F_createDecompressionContext(&p_lz4Dctx, LZ4F_VERSION);
	if (LZ4F_isError(lz4_rcode)) {
		fprintf(stderr ,"LZ4F_createDecompressionContext: error %zu", lz4_rcode);
		ERROR_EXIT(lz4_rcode);
	}
}

void stop_decompression( void) 
{
	LZ4F_errorCode_t lz4_rcode;

	PXYDEBUG("RPROXY\n");

	lz4_rcode = LZ4F_freeDecompressionContext(p_lz4Dctx);
	if (LZ4F_isError(lz4_rcode)) {
		fprintf(stderr ,"LZ4F_freeDecompressionContext: error %zu", lz4_rcode);
		ERROR_EXIT(lz4_rcode);
	}
}
		
		
/* pr_setup_connection: bind and setup a listening socket 
   This socket is not accepting connections yet after
   end of this call.
 */                
int pr_setup_connection(void) 
{
    int server_type;
	
	struct sockaddr_tipc server_addr;
	
    server_type = (BASE_TYPE + px.px_id);
	PXYDEBUG("RPROXY: for node %s running at type=%d ,inst=%d\n", px.px_name, server_type, SERVER_INST);
	
    // Create server socket.
    if ( (rlisten_sd = socket(AF_TIPC, SOCK_STREAM , 0)) < 0) 
        ERROR_EXIT(errno);

/*     if( (ret = setsockopt(rlisten_sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) < 0)
        	ERROR_EXIT(errno); */

    // Bind (attach) this process to the server socket.
	server_addr.family             = AF_TIPC;
	server_addr.addrtype           = TIPC_ADDR_NAMESEQ;
	server_addr.addr.nameseq.type  = server_type;
	server_addr.addr.nameseq.lower = SERVER_INST;
	server_addr.addr.nameseq.upper = SERVER_INST;
	server_addr.scope              = TIPC_ZONE_SCOPE;

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
int pr_receive_payload(proxy_payload_t *pl_ptr, int pl_size) 
{
    int n, received = 0;
    char *p_ptr;

   	PXYDEBUG("pl_size=%d\n",pl_size);
   	p_ptr = (char*) pl_ptr;
   	while ((n = recv(rconn_sd, p_ptr, (pl_size-received), 0 )) > 0) {
	   	PXYDEBUG("recv=%d\n",n);
        received = received + n;
		PXYDEBUG("RPROXY: n:%d | received:%d\n", n,received);
        if (received >= pl_size) return(OK);
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
			PXYDEBUG("RPROXY:" CMD_XFORMAT, CMD_XFIELDS(p_header));
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
    int rcode, payload_size, i, ret, raw_len;
 	message *m_ptr;
	proxy_hdr_t *bat_cmd, *bat_ptr;

    do {
		bzero(p_header, sizeof(proxy_hdr_t));
		PXYDEBUG("RPROXY: About to receive header\n");
    	rcode = pr_receive_header();
    	if (rcode != 0) ERROR_RETURN(rcode);
		PXYDEBUG("RPROXY:" CMD_FORMAT,CMD_FIELDS(p_header));
		PXYDEBUG("RPROXY:" CMD_XFORMAT,CMD_XFIELDS(p_header));
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
		}
   	}while(p_header->c_cmd == CMD_NONE);
    
	/* now we have a proxy header in the buffer. Cast it.*/
    payload_size = p_header->c_len;
    
    /* payload could be zero */
    if(payload_size > 0) {
        bzero(p_payload, payload_size);
        rcode = pr_receive_payload(p_payload, payload_size);
        if (rcode != 0) ERROR_RETURN(rcode);
		
		if( TEST_BIT(p_header->c_flags, FLAG_LZ4_BIT)) {
				raw_len = decompress_payload(p_header, p_payload); 
				if(raw_len < 0)	ERROR_RETURN(raw_len);
				if(!TEST_BIT(p_header->c_flags, FLAG_BATCH_BIT)) { // ONLY CMD_COPYIN_DATA AND  CMD_COPYOUT_DATA  
					if( raw_len != p_header->c_u.cu_vcopy.v_bytes){
							fprintf(stderr,"raw_len=%d " VCOPY_FORMAT, raw_len, VCOPY_FIELDS(p_header));
							ERROR_RETURN(EDVSBADVALUE);
					}
				}
				p_header->c_len = raw_len;
		}
	}

	PXYDEBUG("RPROXY: put2lcl\n");
	if( TEST_BIT(p_header->c_flags, FLAG_LZ4_BIT)) {
		rcode = dvk_put2lcl(p_header, p_decomp_pl);
	} else{
		rcode = dvk_put2lcl(p_header, p_payload);
	}

#ifdef RMT_CLIENT_BIND	
	ret = 0;
	if( rcode < 0) {
		/******************** REMOTE CLIENT BINDING ************************************/
		/* rcode: the result of the las dvk_put2lcl 	*/
		/* ret: the result of the following operations	*/
		PXYDEBUG("RPROXY: REMOTE CLIENT BINDING rcode=%d\n", rcode);
		
		if( p_header->c_src <= NR_SYS_PROCS) {
			PXYDEBUG("RPROXY: src=%d <= NR_SYS_PROCS\n", p_header->c_src);
			ERROR_RETURN(rcode);
		}
		
		switch(rcode){
			case EDVSNONODE:	/* local node register other node for this endpoint   */
			case EDVSENDPOINT:	/* local node registers other endpoint using the slot */
				ret = dvk_unbind(p_header->c_dcid,p_header->c_src);
				if(ret != 0) ERROR_RETURN(rcode);
				// fall down 
			case EDVSNOTBIND:	/* the slot is free */
				ret = dvk_rmtbind(p_header->c_dcid,"rclient",p_header->c_src,p_header->c_snode);
				if( ret != p_header->c_src) ERROR_RETURN(rcode);
				break;
			default:
				ERROR_RETURN(rcode);
		} 

#ifdef SYSTASK_BIND
		/*Build a pseudo header */
		p_pseudo->c_cmd 	            = CMD_SNDREC_MSG;
		p_pseudo->c_dcid	            = p_header->c_dcid;
		p_pseudo->c_src	 	            = PM_PROC_NR;
		p_pseudo->c_dst 	            = SYSTASK(localnodeid);
		p_pseudo->c_snode 	            = p_header->c_snode;
		p_pseudo->c_dnode 	            = local_nodeid;
		p_pseudo->c_rcode	            = 0;
		p_pseudo->c_len		            = 0;
		p_pseudo->c_flags	            = 0;
		p_pseudo->c_batch_nr            = 0;
		p_pseudo->c_snd_seq             = 0;
		p_pseudo->c_ack_seq             = 0;
		p_pseudo->c_timestamp           =  p_header->c_timestamp;
		p_pseudo->c_u.cu_msg.m_source 	= PM_PROC_NR;
		p_pseudo->c_u.cu_msg.m_type 	= SYS_BINDPROC;
		p_pseudo->c_u.cu_msg.M3_ENDPT 	= p_header->c_src;
		p_pseudo->c_u.cu_msg.M3_NODEID 	= p_header->c_snode;
		p_pseudo->c_u.cu_msg.M3_OPER 	= RMT_BIND;
		sprintf(&p_pseudo->c_u.cu_msg.m3_ca1,"RClient%d", p_header->c_snode);
		
		/* send PSEUDO message to local SYSTASK */	
		ret = dvk_put2lcl(p_pseudo, p_payload);
		if( ret < 0 ) {
			ERROR_PRINT(ret);
			ERROR_RETURN(rcode);
		}
	
#endif // SYSTASK_BIND

		/* PUT2LCL retry after REMOTE CLIENT AUTOMATIC  BINDING */
		PXYDEBUG("RPROXY: put2lcl (autobind)\n");

		if( TEST_BIT(p_header->c_flags, FLAG_LZ4_BIT)) {
			rcode = dvk_put2lcl(p_header, p_decomp_pl);
		} else{
			rcode = dvk_put2lcl(p_header, p_payload);
		}

		if( ret < 0) {
			ERROR_PRINT(ret);
			if( rcode < 0) ERROR_RETURN(rcode);
		}
		rcode = 0;
	}
#else /* RMT_CLIENT_BIND	*/
	if( rcode < 0 ) ERROR_RETURN(rcode);

#endif /* RMT_CLIENT_BIND	*/
	PXYDEBUG("RPROXY:" CMD_FORMAT, CMD_FIELDS(p_header));
	PXYDEBUG("RPROXY:" CMD_XFORMAT, CMD_XFIELDS(p_header));
	
	if( TEST_BIT(p_header->c_flags, FLAG_BATCH_BIT)) {
		batch_nr = p_header->c_batch_nr;
		PXYDEBUG("RPROXY: batch_nr=%d\n", batch_nr);
		// check payload len
		if( (batch_nr * sizeof(proxy_hdr_t)) != p_header->c_len){
			PXYDEBUG("RPROXY: batched messages \n");
			ERROR_RETURN(EDVSBADVALUE);
		}
		// put the batched commands
		if( TEST_BIT(p_header->c_flags, FLAG_LZ4_BIT)) 
			bat_cmd = (proxy_hdr_t *) p_decomp_pl;
		else 
			bat_cmd = (proxy_hdr_t *) p_payload;
			
		for( i = 0; i < batch_nr; i++){
			bat_ptr = &bat_cmd[i];
			PXYDEBUG("RPROXY: bat_cmd[%d]:" CMD_FORMAT, i, CMD_FIELDS(bat_ptr));
			rcode = dvk_put2lcl(bat_ptr, p_payload);
			if( rcode < 0) {
				ERROR_RETURN(rcode);
			}
		}
	}
	
	if( TEST_BIT(p_header->c_flags, FLAG_LZ4_BIT)) 
		update_stats(p_header,  p_decomp_pl, CONNECT_RPROXY);
	else 
		update_stats(p_header,  p_payload, CONNECT_RPROXY);

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

	init_decompression();
	
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
    stop_decompression();
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
		} if( rcode < 0) 
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
	
	pws_port = ((BASE_TYPE+BASE_OFFSET) +  px.px_id);
	rcode = pthread_create( &pws_pth, NULL, main_pws, pws_port );
	if(rcode) ERROR_EXIT(rcode);
	PXYDEBUG("pws_pth=%u\n",pws_pth);		

	MTX_LOCK(px_mutex);
	COND_WAIT(px_cond, px_mutex);
	COND_SIGNAL(pws_cond);
	MTX_UNLOCK(px_mutex);
	
	// MUTEX REMAINS LOCKED 
    if( pr_setup_connection() == OK) {
      	pr_start_serving();
 	} else {
       	ERROR_EXIT(errno);
    }
}

/*----------------------------------------------*/
/*      PROXY SENDER FUNCTIONS                  */
/*----------------------------------------------*/

int compress_payload( proxy_hdr_t *hd_ptr, proxy_payload_t *pl_ptr )
{
	size_t comp_len;

	/* size_t LZ4F_compressFrame(void* dstBuffer, size_t dstMaxSize, const void* srcBuffer, 
				size_t srcSize, const LZ4F_preferences_t* preferencesPtr);
	*/	
	comp_len = LZ4F_compressFrame(
				p_comp_pl,	p_maxCsize,
				pl_ptr,	hd_ptr->c_len, 
				NULL);
				
	if (LZ4F_isError(comp_len)) {
		fprintf(stderr ,"LZ4F_compressFrame failed: error %zu", comp_len);
		ERROR_RETURN(comp_len);
	}

	PXYDEBUG("SPROXY: raw_len=%d comp_len=%d\n", hd_ptr->c_len, comp_len);
		
	return(comp_len);
}

void init_compression( void) 
{
	size_t frame_size;

	PXYDEBUG("SPROXY\n");

	p_maxRsize = sizeof(proxy_payload_t);
	frame_size = LZ4F_compressBound(sizeof(proxy_payload_t), &lz4_preferences);
	p_maxCsize =  frame_size + LZ4_HEADER_SIZE + LZ4_FOOTER_SIZE;
	posix_memalign( (void**) &p_comp_pl, getpagesize(), p_maxCsize );
	if (p_comp_pl == NULL) {
    		fprintf(stderr, "p_comp_pl posix_memalign");
			ERROR_EXIT(errno);
  	}
}

void stop_compression( void) 
{
	PXYDEBUG("SPROXY\n");
}

/* ps_send_header: send  header to remote receiver */
int  ps_send_header(proxy_hdr_t *hd_ptr ) 
{
    int sent = 0;        // how many bytes we've sent
    int bytesleft;
    int n, total;
	char *p_ptr;

	bytesleft = sizeof(proxy_hdr_t); // how many bytes we have left to send
	total = bytesleft;
	PXYDEBUG("SPROXY: send header=%d \n", bytesleft);

    p_ptr = (char *) hd_ptr;
	PXYDEBUG("SPROXY:" CMD_FORMAT, CMD_FIELDS(hd_ptr));
	PXYDEBUG("SPROXY:" CMD_XFORMAT, CMD_XFIELDS(hd_ptr));

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
int  ps_send_payload(proxy_hdr_t *hd_ptr, proxy_payload_t *pl_ptr ) 
{
    int sent = 0;        // how many bytes we've sent
    int bytesleft;
    int n, total;
	char *p_ptr;

	assert( hd_ptr->c_len > 0);
	
	bytesleft =  hd_ptr->c_len; // how many bytes we have left to send
	total = bytesleft;
	PXYDEBUG("SPROXY: ps_send_payload bytesleft=%d \n", bytesleft);

    p_ptr = (char *) pl_ptr;
    while(sent < total) {
        n = send(sproxy_sd, p_ptr, bytesleft, 0);
		PXYDEBUG("SPROXY: sent=%d \n", n);
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

void update_stats(proxy_hdr_t *hd_ptr, proxy_payload_t *pl_ptr, int px_type)
{
	px_stats_t *pxs_ptr;
	proxy_hdr_t *bat_cmd, *bat_ptr;
	int i;

	PXYDEBUG("px_type=%d\n", px_type);

	MTX_LOCK(px_mutex);
	pxs_ptr = &px_stats[hd_ptr->c_dnode][hd_ptr->c_dcid];
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
	MTX_UNLOCK(px_mutex);
}

/* 
 * ps_send_remote: send a message (header + payload if existing) 
 * to remote receiver
 */
int  ps_send_remote(proxy_hdr_t *hd_ptr, proxy_payload_t *pl_ptr ) 
{
	int rcode, comp_len;

	PXYDEBUG("SPROXY:" CMD_FORMAT, CMD_FIELDS(hd_ptr));
	PXYDEBUG("SPROXY:" CMD_XFORMAT, CMD_XFIELDS(hd_ptr));
	
	if( (hd_ptr->c_cmd == CMD_COPYIN_DATA) || 
		(hd_ptr->c_cmd == CMD_COPYOUT_DATA) || 
		TEST_BIT(hd_ptr->c_flags, FLAG_BATCH_BIT) ){

		PXYDEBUG("SPROXY: c_len=%d\n", hd_ptr->c_len);
		assert( hd_ptr->c_len > 0);	

		/* compress here */
		comp_len = compress_payload(hd_ptr, pl_ptr);
		PXYDEBUG("SPROXY: c_len=%d comp_len=%d\n", hd_ptr->c_len, comp_len);
		if(comp_len < hd_ptr->c_len){
			/* change command header  */
			hd_ptr->c_len = comp_len; 
			SET_BIT(hd_ptr->c_flags, FLAG_LZ4_BIT); 
		}
		PXYDEBUG(CMD_XFORMAT, CMD_XFIELDS(hd_ptr));		
	}else {
		assert( hd_ptr->c_len == 0);
	}
	
	/* send the header */
	rcode =  ps_send_header(hd_ptr);
	if ( rcode != OK)  ERROR_RETURN(rcode);

	if( hd_ptr->c_len > 0) {
		PXYDEBUG("SPROXY: send payload len=%d\n", hd_ptr->c_len );
		if( TEST_BIT(hd_ptr->c_flags, FLAG_LZ4_BIT) )
			rcode =  ps_send_payload(hd_ptr, p_comp_pl);
		else
			rcode =  ps_send_payload(hd_ptr, p_payload);
		if ( rcode != OK)  ERROR_RETURN(rcode);
	}

	update_stats(hd_ptr, pl_ptr, CONNECT_SPROXY) ;

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

	init_compression();
    pid = getpid();
	
    while(1) {
		cmd_flag = 0;
		batch_nr = 0;		// nr_cmd the number of batching commands in the buffer
			
		PXYDEBUG("SPROXY %d: Waiting a message\n", pid);
		ret = dvk_get2rmt(p_header, p_payload); 

//		p_header->c_flags   = 0;
//		p_header->c_snd_seq = 0;
//		p_header->c_ack_seq = 0;
		switch(ret) {
			case OK:
				break;
			case EDVSTIMEDOUT:
				PXYDEBUG("SPROXY: Sending HELLO \n");
				p_header->c_cmd   = CMD_NONE;
				p_header->c_len   = 0;
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

		PXYDEBUG("SPROXY: %d "CMD_FORMAT,pid, CMD_FIELDS(p_header)); 
		PXYDEBUG("SPROXY: %d "CMD_XFORMAT,pid, CMD_XFIELDS(p_header)); 
		//------------------------ BATCHEABLE COMMAND -------------------------------
		if(  (p_header->c_cmd != CMD_COPYIN_DATA )     // is a batcheable command ??
		  && (p_header->c_cmd != CMD_COPYOUT_DATA)){	// YESSSSS

			if ( (p_header->c_cmd  == CMD_SEND_MSG) 
				||(p_header->c_cmd == CMD_SNDREC_MSG)
				||(p_header->c_cmd == CMD_REPLY_MSG)){
				m_ptr = &p_header->c_u.cu_msg;
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
			p_header3->c_flags 	  = 0;
			SET_BIT(p_header3->c_flags,FLAG_BATCH_BIT);
			p_header3->c_batch_nr = batch_nr;
			p_header3->c_len      = batch_nr * sizeof(proxy_hdr_t);
			rcode =  ps_send_remote(p_header3, p_batch);
			if (rcode == 0) smsg_ok++;
			else smsg_fail++;
			batch_nr = 0;		// nr_cmd the number of batching commands in the buffer
		} else {
			PXYDEBUG("SPROXY:" CMD_FORMAT, CMD_FIELDS(p_header));
			PXYDEBUG("SPROXY:" CMD_XFORMAT, CMD_XFIELDS(p_header));
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
	stop_compression();
    exit(1);
}

void wait_for_server(__u32 name_type, __u32 name_instance, int wait)
{
	struct sockaddr_tipc topsrv;
	struct tipc_subscr subscr;
	struct tipc_event event;

	int sd = socket(AF_TIPC, SOCK_SEQPACKET, 0);

	memset(&topsrv, 0, sizeof(topsrv));
	topsrv.family                  = AF_TIPC;
	topsrv.addrtype                = TIPC_ADDR_NAME;
	topsrv.addr.name.name.type     = TIPC_TOP_SRV;
	topsrv.addr.name.name.instance = TIPC_TOP_SRV;

	/* Connect to topology server */

	if (0 > connect(sd, (struct sockaddr *)&topsrv, sizeof(topsrv))) {
		perror("Client: failed to connect to topology server");
		exit(1);
	}

	subscr.seq.type  = htonl(name_type);
	subscr.seq.lower = htonl(name_instance);
	subscr.seq.upper = htonl(name_instance);
	subscr.timeout   = htonl(wait);
	subscr.filter    = htonl(TIPC_SUB_SERVICE);

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
    int server_type, rcode ;
	
	server_type = (BASE_TYPE + local_nodeid);
	wait_for_server(server_type, SERVER_INST, 10000);

	sproxy_sd = socket(AF_TIPC, SOCK_STREAM, 0);

	rmtserver_addr.family                  = AF_TIPC;
	rmtserver_addr.addrtype                = TIPC_ADDR_NAME;
	rmtserver_addr.addr.name.name.type     = server_type;
	rmtserver_addr.addr.name.name.instance = SERVER_INST;
	rmtserver_addr.addr.name.domain        = 0;

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
    int rcode = 0, n, d;

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
    		perror("p_header3 posix_memalign");
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
    if ( (sproxy_sd = socket(AF_TIPC, SOCK_STREAM, 0)) < 0){
       	ERROR_EXIT(errno);
    }
	
	pws_port = (BASE_TYPE + px.px_id);
	rcode = pthread_create( &pws_pth, NULL, main_pws, pws_port );
	if(rcode) ERROR_EXIT(rcode);
	PXYDEBUG("pws_pth=%u\n",pws_pth);
		
	/* try to connect many times */
	batch_nr = 0;		// number of batching commands
	cmd_flag = 0;		// signals a command to be sent 
	
	MTX_LOCK(px_mutex);
	COND_WAIT(px_cond, px_mutex);
	COND_SIGNAL(pws_cond);	
	MTX_UNLOCK(px_mutex);
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

	if( (BLOCK_16K << lz4_preferences.frameInfo.blockSizeID) < MAXCOPYBUF)  {
		fprintf(stderr, "MAXCOPYBUF(%d) must be greater than (BLOCK_16K <<"
			"lz4_preferences.frameInfo.blockSizeID)(%d)\n",MAXCOPYBUF,
			(BLOCK_16K << lz4_preferences.frameInfo.blockSizeID));
		exit(1);
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

/*===========================================================================*
 *				PWS web server					     *
 *===========================================================================*/
void pws_server(int fd, int hit, int px_type)
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
	wdmp_px_stats();
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
void main_pws(void *arg_port)
{
	int i, pws_pid, pws_listenfd, pws_sfd, pws_hit, px_type;
	size_t length;
	int rcode;
	static struct sockaddr_in pws_cli_addr; /* static = initialised to zeros */
	static struct sockaddr_in serv_addr; /* static = initialised to zeros */
	
	PXYDEBUG("PWS service starting.\n");
	pws_pid = syscall (SYS_gettid);
	PXYDEBUG("pws_pid=%d\n", pws_pid);
	
			/* Setup the network socket */
	if((pws_listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
			ERROR_PT_EXIT(errno);
		
	/* PWS service Web server pws_port */
	pws_port = (int) arg_port;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(pws_port);
		
	if(bind(pws_listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0){
		ERROR_PT_EXIT(errno);
	}
		
	if( listen(pws_listenfd,64) <0){
		ERROR_PT_EXIT(errno);
	}
	
	MTX_LOCK(px_mutex);
	COND_SIGNAL(px_cond);
	COND_WAIT(pws_cond, px_mutex);
	MTX_UNLOCK(px_mutex);
	
	PXYDEBUG("Starting PWS server main loop\n");
	for(pws_hit=1; ;pws_hit++) {
		length = sizeof(pws_cli_addr);
		PXYDEBUG("Conection accept\n");
		pws_sfd = accept(pws_listenfd, (struct sockaddr *)&pws_cli_addr, &length);
		if (pws_sfd < 0){
			rcode = -errno;
			ERROR_PT_EXIT(rcode);
		}
		px_type = (pws_port < (BASE_TYPE+BASE_OFFSET))?CONNECT_SPROXY:CONNECT_RPROXY;
		pws_server(pws_sfd, pws_hit, px_type);
		close(pws_sfd);
	}
	
	return(OK);		/* shouldn't come here */
}


/*===========================================================================*
 *				wdmp_px_stats  					    				     *
 *===========================================================================*/
void wdmp_px_stats(void)
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

	MTX_LOCK(px_mutex);
	for(n = 0; n < NR_NODES; n++) {
		for(d = 0; d < NR_DCS; d++) {
			pxs_ptr = &px_stats[n][d];
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
	MTX_UNLOCK(px_mutex);

	(void)strcat(page_ptr,"</tbody></table>\n");
   	PXYDEBUG("%s\n",page_ptr);	
	PXYDEBUG("strlen(page_ptr)=%d\n", strlen(page_ptr));	
}



