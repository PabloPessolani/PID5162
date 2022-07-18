/***********************************************************/
/* 	MULTI-NODE MULTI-PROTOCOL  				*/
/* 	DISTRIBUTED SERVICE PROXY 		  			*/
/*   BATCHING MESSAGES							*/
/*   DATA BLOCKS COMPRESSION 					*/
/*   AUTOMATIC CLIENT BINDING  					*/
/*  Kernel with support of Timestamp and sender PID insertion	*/
/***********************************************************/

#define _TABLE

#include "dsp_proxy.h"


void usage(void)
{
   	fprintf(stderr,"Usage: dsp_proxy <config_file>\n");
	exit(1);
}

/*----------------------------------------------*/
/*		MAIN: 			*/
/*----------------------------------------------*/
void  main ( int argc, char *argv[] )
{
    int pid, status;
    int ret, c, i;
	proxy_t *px_ptr;
	client_t *clt_ptr;
	server_t *svr_ptr;
	service_t *svc_ptr;
	sess_entry_t *sess_ptr;


	if( argc != 2) 	{
		usage();
		exit(EXIT_FAILURE);
	}
	
	ret = dvk_open();
	if (ret < 0)  ERROR_EXIT(ret);

    local_nodeid = dvk_getdvsinfo(&dvs);
	if(local_nodeid < 0) ERROR_EXIT(local_nodeid);
    dvs_ptr=&dvs;
	PXYDEBUG(DVS_USR_FORMAT,DVS_USR_FIELDS(dvs_ptr));
	lb_ptr = &lb;
    lb_ptr->lb_pid = syscall (SYS_gettid);
	PXYDEBUG("MAIN: lb_pid=%d local_nodeid=%d\n", lb_ptr->lb_pid , local_nodeid);
	 
	// Clear server and client tables 
    PXYDEBUG("Clear Proxy\n");
	for( i = 0;  i < dvs_ptr->d_nr_nodes; i++ ){
		px_ptr = &proxy_tab[i];
		px_ptr->px_nodeid = i;
		init_proxy(px_ptr);
		/* creates SENDER and RECEIVER Proxies as Treads */
		pthread_mutex_init(&px_ptr->px_conn_mtx, NULL);  /* init mutex */
		pthread_cond_init(&px_ptr->px_conn_scond, NULL);  /* init condition */
		pthread_cond_init(&px_ptr->px_conn_rcond, NULL);  /* init condition */

		pthread_mutex_init(&px_ptr->px_send_mtx, NULL);  /* init mutex */
		pthread_cond_init(&px_ptr->px_send_cond, NULL);  /* init condition */

		pthread_mutex_init(&px_ptr->px_recv_mtx, NULL);  /* init mutex */
		pthread_cond_init(&px_ptr->px_recv_cond, NULL);  /* init condition */

		// starting RECEIVER thread synchronization mutex and condition 
		pthread_cond_init(&px_ptr->px_rdesc.td_cond, NULL);  /* init condition */
		pthread_cond_init(&px_ptr->px_rdesc.td_tcond, NULL);  /* init condition */
		pthread_mutex_init(&px_ptr->px_rdesc.td_mtx, NULL);  /* init mutex */
		
		// starting SENDING thread synchronization mutex and condition 
		pthread_cond_init(&px_ptr->px_sdesc.td_cond, NULL);  /* init condition */
		pthread_cond_init(&px_ptr->px_sdesc.td_tcond, NULL);  /* init condition */
		pthread_mutex_init(&px_ptr->px_sdesc.td_mtx, NULL);  /* init mtx */
	}

	// Clear service table  
    PXYDEBUG("Clear service table\n");	
	for( i = 0;  i < MAX_SVC_NR; i++ ){
		svc_ptr = &service_tab[i];
		svc_ptr->svc_index = i;
		init_service(svc_ptr);
	}
 
    // one session table for each DC 
    PXYDEBUG("Building Session Tables\n");
	nr_sess_entries = dvs_ptr->d_nr_procs - dvs_ptr->d_nr_sysprocs;
	for (int i = 0;i < dvs_ptr->d_nr_dcs; i++){ 
        sess_table[i].st_tab_ptr = (void*) 
			malloc(	sizeof(sess_entry_t) * nr_sess_entries ); 
        if( sess_table[i].st_tab_ptr == NULL) ERROR_EXIT(EDVSNOMEM);
		sess_ptr = (sess_entry_t *)sess_table[i].st_tab_ptr;
		// clear 
		sess_table[i].st_nr_sess = 0;	// clear # of used sessions 
		// MUTEX initialization
		pthread_mutex_init(&sess_table[i].st_mutex, NULL);
		for( int j = 0; j < nr_sess_entries; j++){
			sess_ptr->se_dcid = i;
			init_session(sess_ptr);
			sess_ptr->se_rmtcmd    = malloc(sizeof(MAXCMDLEN));
			if(sess_ptr->se_rmtcmd == NULL) {
				ret = -errno;
				ERROR_EXIT(ret);
			}
			sess_ptr++;
		}
    }
  
    lb_config(argv[1]);  //Reads Config File

    if( lb_ptr->lb_compress_opt == YES){
		if( (BLOCK_16K << lz4_preferences.frameInfo.blockSizeID) < MAXCOPYBUF)  {
			fprintf(stderr, "MAXCOPYBUF(%d) must be greater than (BLOCK_16K <<"
				"lz4_preferences.frameInfo.blockSizeID)(%d)\n",MAXCOPYBUF,
				(BLOCK_16K << lz4_preferences.frameInfo.blockSizeID));
			exit(1);
		}
	}

	// SETUP de VIRTUAL LOAD BALANCER proxy fields
	PXYDEBUG("MAIN: lb_nodeid=%d\n", lb_ptr->lb_nodeid);
	pthread_mutex_init(&lb_ptr->lb_mtx, NULL);
	px_ptr = &proxy_tab[lb_ptr->lb_nodeid];
	px_ptr->px_type		= PX_LOADBAL;
	px_ptr->px_rname	= lb_ptr->lb_name;	
	px_ptr->px_name		= lb_ptr->lb_name;	

	// START ALL COMMUNICATION PROXY  THREADS 
	for( i = 0;  i < dvs_ptr->d_nr_nodes; i++){
        PXYDEBUG("MAIN: i=%d \n", i);
		px_ptr = &proxy_tab[i];

		if( px_ptr->px_nodeid == local_nodeid) continue;
		if( px_ptr->px_type   == LB_INVALID) continue;

        PXYDEBUG(PROXY_FORMAT, PROXY_FIELDS(px_ptr));		

		// START RPROXY 
		MTX_LOCK(px_ptr->px_rdesc.td_mtx); 
		PXYDEBUG("MAIN(%d): pthread_create RPROXY\n", px_ptr->px_nodeid);
		if ( (ret = pthread_create(&px_ptr->px_rdesc.td_thread, NULL, pr_thread,(void*)px_ptr )) != 0) {
			ERROR_EXIT(ret);
		}
		COND_WAIT(px_ptr->px_rdesc.td_cond, px_ptr->px_rdesc.td_mtx);
		PXYDEBUG("MAIN(%d): td_tid=%d\n", px_ptr->px_nodeid, px_ptr->px_rdesc.td_tid);
		
		// START SPROXY 
		MTX_LOCK(px_ptr->px_sdesc.td_mtx);
		PXYDEBUG("MAIN(%d): pthread_create SPROXY\n", px_ptr->px_nodeid);
		if ((ret = pthread_create(&px_ptr->px_sdesc.td_thread, NULL, ps_thread,(void*)px_ptr )) != 0) {
			ERROR_EXIT(ret);
		}
		COND_WAIT(px_ptr->px_sdesc.td_cond, px_ptr->px_sdesc.td_mtx);
		PXYDEBUG("MAIN(%d): td_tid=%d\n", px_ptr->px_nodeid, px_ptr->px_sdesc.td_tid);
	
		// BIND THE COMMUNICATION THREADS TO NODES 
		if( px_ptr->px_nodeid == lb_ptr->lb_nodeid) { // VIRTUAL NODE PROXIES BIND 
			ret = dvk_proxies_bind(lb_ptr->lb_name, lb_ptr->lb_nodeid, 
							px_ptr->px_sdesc.td_tid, px_ptr->px_rdesc.td_tid, MAXCOPYBUF);
			if( ret < 0) ERROR_EXIT(ret);
			ret= dvk_node_up(lb_ptr->lb_name, lb_ptr->lb_nodeid, lb_ptr->lb_nodeid);	
			if( ret < 0) ERROR_EXIT(ret);
		} else {									// REAL NODE PROXIES BIND 
			ret = dvk_proxies_bind(px_ptr->px_name, px_ptr->px_nodeid, 
					px_ptr->px_sdesc.td_tid, px_ptr->px_rdesc.td_tid, MAXCOPYBUF);
			if( ret < 0) ERROR_EXIT(ret);
			ret= dvk_node_up(px_ptr->px_name, px_ptr->px_nodeid, px_ptr->px_nodeid);	
			if( ret < 0) ERROR_EXIT(ret);			
		}
		
		COND_SIGNAL(px_ptr->px_rdesc.td_tcond);
		COND_SIGNAL(px_ptr->px_sdesc.td_tcond);
		MTX_UNLOCK(px_ptr->px_rdesc.td_mtx);
		MTX_UNLOCK(px_ptr->px_sdesc.td_mtx);
	}
	
	// PERIODICALLY,  GET LOAD METRICS 
	while(TRUE){
		PXYDEBUG("MAIN: lb_period=%d\n", lb_ptr->lb_period);		
		sleep(lb_ptr->lb_period);
#ifdef GET_METRICS
		get_metrics();
#endif // GET_METRICS
	}
	
	PXYDEBUG("MAIN: joining threads\n");		
	for( i = 0; i < dvs_ptr->d_nr_nodes; i++){
		px_ptr = &proxy_tab[i];
		if( px_ptr->px_type   == LB_INVALID) continue;
		if( px_ptr->px_nodeid == lb_ptr->lb_nodeid) continue;
		if( px_ptr->px_nodeid == local_nodeid) continue;

        PXYDEBUG(PROXY_FORMAT, PROXY_FIELDS(px_ptr));	
		pthread_join (&px_ptr->px_rdesc.td_thread, NULL );
		pthread_join (&px_ptr->px_sdesc.td_thread, NULL );
		pthread_mutex_destroy(&px_ptr->px_rdesc.td_mtx);
		pthread_cond_destroy(&px_ptr->px_rdesc.td_cond);
		pthread_mutex_destroy(&px_ptr->px_sdesc.td_mtx);
		pthread_cond_destroy(&px_ptr->px_sdesc.td_cond);
		pthread_mutex_destroy(&px_ptr->px_send_mtx);
	}
	
    exit(0);
}

