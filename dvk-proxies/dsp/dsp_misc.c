/***********************************************************/
/*  DSP_MISC									*/
/***********************************************************/

#include "dsp_proxy.h"

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

int enable_keepalive(int sock) 
{
    int yes = 1;
    if(setsockopt(
        sock, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int)) != -1)
		ERROR_RETURN(-errno);

    int idle = 1;
    if(setsockopt(
        sock, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(int)) != -1)
		ERROR_RETURN(-errno);

    int interval = 1;
	if(setsockopt(
        sock, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(int)) != -1)
		ERROR_RETURN(-errno);

    int maxpkt = 10;
    if(setsockopt(
        sock, IPPROTO_TCP, TCP_KEEPCNT, &maxpkt, sizeof(int)) != -1)
				ERROR_RETURN(-errno);
	return(OK);
}

void sig_alrm(int sig)
{
	PXYDEBUG("sig=%d\n",sig);
}

// #define GET_METRICS 1
#ifdef GET_METRICS

/*===========================================================================*
*				get_metrics				     *
*===========================================================================*/
void get_metrics(void)
{
	int period;
	int cpu_usage;
	int load_lvl;
	
	PXYDEBUG(DSP1_FORMAT, DSP1_FIELDS(lb_ptr));
		
	// wait until be initiliazed 
	PXYDEBUG("Check initialization... \n");
	MTX_LOCK(lb_ptr->lb_mtx);
	if(lb_ptr->lb_lowwater == LB_INVALID){
		MTX_UNLOCK(lb_ptr->lb_mtx);
		return;
	}
	
	if(lb_ptr->lb_lowwater == LB_INVALID){
		MTX_UNLOCK(lb_ptr->lb_mtx);
		return;
	}
	MTX_UNLOCK(lb_ptr->lb_mtx);

	cpu_usage = get_CPU_usage();
	if( cpu_usage == LB_INVALID) return;
	assert( cpu_usage >= 0 && cpu_usage <= 100);
	
	MTX_LOCK(lb_ptr->lb_mtx);
	if( cpu_usage < lb_ptr->lb_lowwater){
		lb_ptr->lb_load_lvl = LVL_IDLE;
	} else	if( cpu_usage >= lb_ptr->lb_highwater){
		lb_ptr->lb_load_lvl = LVL_SATURATED;
	} else {
		lb_ptr->lb_load_lvl = LVL_BUSY;
	}
	// update agent values 
	lb_ptr->lb_cpu_usage = cpu_usage;
	PXYDEBUG(DSP3_FORMAT, DSP3_FIELDS(lb_ptr));

	MTX_UNLOCK(lb_ptr->lb_mtx);
}

int get_CPU_usage(void)
{
	int rcode;
	float idle_float;
	unsigned int  cpu_usage, cpu_idle;
	char *ptr;
	FILE *fp;
	static char cpu_string[128];
	jiff duse, dsys, didl, diow, dstl, Div, divo2;

	PXYDEBUG("\n");

	/* Open the command for reading. */
to_popen:	
	fp = popen("head -n1 /proc/stat", "r");
	if (fp == NULL)	{
		fprintf( stderr,"get_CPU_usage: error popen errno=%d\n", errno);
		ERROR_RETURN(LB_INVALID);
	}

	/* Read the output a line at a time - output it. */
	ptr = fgets(cpu_string, sizeof(cpu_string), fp);
	if( ptr == NULL) {
		fprintf( stderr,"get_CPU_usage: error fgets errno=%d\n", errno);
		fclose(fp);
		ERROR_RETURN(LB_INVALID);
	}
//	PXYDEBUG("cpu_string=%s\n", cpu_string);

	rcode = sscanf(cpu_string,  "cpu  %llu %llu %llu %llu %llu %llu %llu %llu", 
				&cuse[idx], &cice[idx], &csys[idx], &cide[idx], &ciow[idx], &cxxx[idx], &cyyy[idx], &czzz[idx]);
	PXYDEBUG("cpu  %llu %llu %llu %llu %llu %llu %llu %llu\n", 
				cuse[idx], cice[idx], csys[idx], cide[idx], ciow[idx], cxxx[idx], cyyy[idx], czzz[idx]);
	if (idx == 0){
		idx = 1;
		pclose(fp);
		sleep(1);
		goto to_popen;
	}
			
	duse = (cuse[1] + cice[1]) - (cuse[0] + cice[0]);
	dsys = (csys[1] + cxxx[1] + cyyy[1]) - (csys[0] + cxxx[0] + cyyy[0]);
	didl = cide[1]-cide[0];
	diow = ciow[1]-ciow[0];
	dstl = czzz[1]-czzz[0];
	Div = cuse[1] + cice[1] + csys[1] + cide[1] + ciow[1] + cxxx[1] + cyyy[1] + czzz[1];
	Div -= cuse[0] + cice[0] + csys[0] + cide[0] + ciow[0] + cxxx[0] + cyyy[0] + czzz[0];
	if (!Div) Div = 1, didl = 1;
	divo2 = Div / 2UL;
	
	PXYDEBUG("didl=%llu Div=%llu\n", didl, Div);

//	cpu_idle =(unsigned)( (100*didl	+ divo2) / Div );
	idle_float  = (float) didl/Div;
	idle_float *= 100;
	cpu_idle  = (unsigned) idle_float;
	cpu_usage = 100 - cpu_idle;
	
	PXYDEBUG("cpu_usage=%d cpu_idle=%d\n", cpu_usage, cpu_idle);
	pclose(fp);
	
	cuse[0] = cuse[1]; 
	cice[0] = cice[1];
	csys[0] = csys[1];
	cide[0] = cide[1];
	ciow[0] = ciow[1];
	cxxx[0] = cxxx[1];
	cyyy[0] = cyyy[1];
	czzz[0] = czzz[1];
	
	return(cpu_usage);
}

int build_load_level(proxy_t *px_ptr, int mtype)
{
	int rcode; 
	message *m_ptr; 
	proxy_hdr_t *hdr_ptr;

	PXYDEBUG("SPROXY(%d): mtype=%X\n", px_ptr->px_nodeid, mtype);

	hdr_ptr = px_ptr->px_sdesc.td_header;
	m_ptr =  &hdr_ptr->c_msg;

	m_ptr->m_type	=  mtype;
	m_ptr->m_source	=  lb_ptr->lb_nodeid;
	m_ptr->m1_i1	=  lb_ptr->lb_cpu_usage;
	m_ptr->m1_i2	=  lb_ptr->lb_load_lvl; 
	m_ptr->m1_i3	=  lb_ptr->lb_period;
	
	PXYDEBUG("SPROXY(%d): " MSG1_FORMAT, px_ptr->px_nodeid , MSG1_FIELDS(m_ptr));
	return(OK);
}

#endif // GET_METRICS

int build_reply_msg(proxy_t *px_ptr, int mtype, int ret)
{
	int rcode; 
	message *m_ptr; 
	proxy_hdr_t *hdr_ptr;

	PXYDEBUG("SPROXY(%d): mtype=%X\n", px_ptr->px_nodeid, mtype);

	hdr_ptr = px_ptr->px_sdesc.td_header;
	m_ptr =  &hdr_ptr->c_msg;

	m_ptr->m_type	=  mtype;
	m_ptr->m_source	=  local_nodeid;
	m_ptr->m1_i1	=  ret;
	
	PXYDEBUG("SPROXY(%d): " MSG1_FORMAT, px_ptr->px_nodeid , MSG1_FIELDS(m_ptr));
	return(OK);
}

int send_hello_msg(proxy_t *px_ptr)
{
	int rcode; 
	proxy_hdr_t *hdr_ptr;
	proxy_payload_t *pl_ptr;
	message *m_ptr;

	hdr_ptr = px_ptr->px_sdesc.td_header;
	pl_ptr  = px_ptr->px_sdesc.td_payload;
	m_ptr   = &hdr_ptr->c_msg;
	clock_gettime(clk_id, &ts);

	PXYDEBUG("SPROXY(%d): send a HELLO message. m_type=%d\n",px_ptr->px_nodeid, m_ptr->m_type);
	switch(m_ptr->m_type){
		case MT_LOAD_LEVEL:
			PXYDEBUG("SPROXY(%d):" MSG1_FORMAT,px_ptr->px_nodeid, MSG1_FIELDS(m_ptr));
			break;
		case MT_RMTBIND | MT_ACKNOWLEDGE:
			PXYDEBUG("SPROXY(%d):" MSG3_FORMAT,px_ptr->px_nodeid, MSG3_FIELDS(m_ptr));
			break;
		default:
			PXYDEBUG("SPROXY(%d):" MSG1_FORMAT,px_ptr->px_nodeid, MSG1_FIELDS(m_ptr));
			break;
	}

	hdr_ptr->c_cmd 		= CMD_NONE;
	hdr_ptr->c_dcid 	= LB_INVALID;
	hdr_ptr->c_src 		= NONE;
	hdr_ptr->c_dst 		= NONE;
	hdr_ptr->c_snode	= local_nodeid;
	hdr_ptr->c_dnode	= px_ptr->px_nodeid;
	hdr_ptr->c_len		= 0;
	hdr_ptr->c_pid		= px_ptr->px_sdesc.td_tid;
	hdr_ptr->c_batch_nr	= 0;
	hdr_ptr->c_flags	= 0;
	hdr_ptr->c_snd_seq	= 0;
	hdr_ptr->c_ack_seq	= 0;
	hdr_ptr->c_timestamp=ts;
	
	PXYDEBUG("SPROXY(%d): " CMD_FORMAT, px_ptr->px_nodeid, CMD_FIELDS(hdr_ptr));
	PXYDEBUG("SPROXY(%d): " CMD_XFORMAT, px_ptr->px_nodeid, CMD_XFIELDS(hdr_ptr));
	
	rcode =  ps_send_remote(px_ptr, hdr_ptr, pl_ptr);
//	rcode = ps_send_header(px_ptr, hdr_ptr); 
	if( rcode < 0 ) ERROR_RETURN(rcode);
	
	return(rcode);
}	


void init_session( sess_entry_t *sess_ptr)
{
	sess_ptr->se_clt_nodeid	= LB_INVALID;
	sess_ptr->se_clt_ep		= LB_INVALID;
	sess_ptr->se_clt_PID	= LB_INVALID;
	sess_ptr->se_lbclt_ep	= LB_INVALID;
	sess_ptr->se_lbsvr_ep	= LB_INVALID; 
	sess_ptr->se_svr_nodeid	= LB_INVALID;
	sess_ptr->se_svr_ep		= LB_INVALID;
	sess_ptr->se_svr_PID	= LB_INVALID;
	sess_ptr->se_service	= NULL; 
}

void init_proxy(proxy_t *px_ptr)
{
	PXYDEBUG("px_nodeid=%d\n", px_ptr->px_nodeid);
	px_ptr->px_proto	= PX_PROTO_TCP;	
	px_ptr->px_rport	= LBP_BASE_PORT+px_ptr->px_nodeid;	
	px_ptr->px_sport	= LBP_BASE_PORT+local_nodeid;	
	px_ptr->px_batch	= NO;	
	px_ptr->px_compress	= NO;
	px_ptr->px_autobind	= NO;
	px_ptr->px_status	= 0;
	px_ptr->px_type		= LB_INVALID;
	px_ptr->px_rname	= rname_any;
    PXYDEBUG(PROXY_FORMAT, PROXY_FIELDS(px_ptr));		
}

void init_server(server_t *svr_ptr)
{
	PXYDEBUG("svr_index=%d\n", svr_ptr->svr_index);

	// Initialize Server Service Endpoints in use bitmap 
	svr_ptr->svr_bm_svc = 0;
	svr_ptr->svr_idle_ts.tv_sec = 0;
	clock_gettime(clk_id, &svr_ptr->svr_last_cmd);
	
	svr_ptr->svr_idle_count = 0;
	svr_ptr->svr_bm_sts		= 0;
	svr_ptr->svr_bm_svc 	= 0;
	svr_ptr->svr_image 		= NULL;

	svr_ptr->svr_bm_params 		= 0;
	svr_ptr->svr_icmp_retry 	= FD_MAXRETRIES;
	svr_ptr->svr_max_retries	= FD_MAXRETRIES;

	TAILQ_INIT(&svr_ptr->svr_clt_head);                      /* Initialize the queue. */
	TAILQ_INIT(&svr_ptr->svr_svr_head);                      /* Initialize the queue. */
}

void init_client(client_t *clt_ptr)
{
	PXYDEBUG("clt_index=%d\n", clt_ptr->clt_index);

	clock_gettime(clk_id, &clt_ptr->clt_last_cmd);
	clt_ptr->clt_bm_params	= 0;
	
}

void init_service(service_t *svc_ptr)
{
	PXYDEBUG("svc_index=%d\n", svc_ptr->svc_index);

	svc_ptr->svc_extep 	= LB_INVALID;
	svc_ptr->svc_minep 	= LB_INVALID;
	svc_ptr->svc_maxep 	= LB_INVALID;
	svc_ptr->svc_bind 	= LB_INVALID;
	svc_ptr->svc_dcid 	= LB_INVALID;
	svc_ptr->svc_bm_params 	= 0;

}

void init_lb(void )
{
	PXYDEBUG("\n");
	lb.lb_nodeid 	= LOCALNODE;		
	lb.lb_nr_cltpxy	= 0;	
	lb.lb_nr_svrpxy	= 0;	
	lb.lb_bm_svrpxy	= 0;	
	lb.lb_bm_cltpxy	= 0;	
	lb.lb_nr_services 	= 0;
	lb.lb_lowwater 	= LB_INVALID;	
	lb.lb_highwater	= LB_INVALID;
	lb.lb_min_servers = 0;	
	lb.lb_max_servers = dvs_ptr->d_nr_nodes;
	lb.lb_period   	= LB_PERIOD_DEFAULT;	
	lb.lb_start   	= LB_START_DEFAULT;	
	lb.lb_stop 		= LB_SHUTDOWN_DEFAULT;
	lb.lb_hellotime = LB_PERIOD_DEFAULT;	
		
	lb.lb_pid = getpid();
	lb.lb_nr_active = 0;
	lb.lb_bm_active = 0;
	lb.lb_nr_proxy  = 0;
	lb.lb_bm_proxy  = 0;
	lb.lb_nr_init   = 0;
	lb.lb_bm_init   = 0;
	lb.lb_bm_tested = 0;

	lb.lb_bm_sconn  = 0;
	lb.lb_bm_rconn  = 0;
	
	lb.lb_cltname   = NULL;		
	lb.lb_svrname   = NULL;		
	lb.lb_cltdev    = NULL;		
	lb.lb_svrdev    = NULL;		
	lb.lb_ssh_host  = NULL;	
	lb.lb_ssh_user  = NULL;	
	lb.lb_ssh_pass  = NULL;		
	lb.lb_vm_start  = NULL;	
	lb.lb_vm_stop   = NULL;		
	lb.lb_vm_status = NULL;		
		
	lb.lb_bm_params	= 0;	

	lb.lb_bm_suspect= 0;	
	
}


