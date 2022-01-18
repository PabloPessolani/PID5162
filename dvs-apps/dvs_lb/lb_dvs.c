/****************************************************************/
/* 				DVS_LB							*/
/* Load Balancing & autoscaling for DVS 				 		*/
/****************************************************************/

#define _GNU_SOURCE     
#define _MULTI_THREADED
#define _TABLE
#include "lb_dvs.h"

static const LZ4F_preferences_t lz4_preferences = {
	{ LZ4F_max1MB, LZ4F_blockLinked, LZ4F_noContentChecksum, LZ4F_frame, 0, { 0, 0 } },
	0,   /* compression level */
	0,   /* autoflush */
	{ 0, 0, 0, 0 },  /* reserved, must be set to 0 */
};

__attribute__((packed, aligned(4)))

void stop_compression( void) 
{
	USRDEBUG("DVS_LB\n");
}

/*===========================================================================*
 *				   main 				    					 *
 *===========================================================================*/
int main (int argc, char *argv[] )
{
    int i, rcode;
    sess_entry_t *sess_ptr;
	client_t *clt_ptr;
	server_t *svr_ptr;
	lbpx_desc_t *spx_ptr;
	lbpx_desc_t *cpx_ptr;
		
    if ( argc != 2) {
        fprintf( stderr,"Usage: %s <config_file> \n", argv[0] );
		fprintf(stderr,"sizeof(cmd_t)= %d\n", sizeof(cmd_t) );
		fprintf(stderr,"sizeof(message)= %d\n", sizeof(message) );		
        exit(1);
    }

	clk_id = CLOCK_REALTIME;

    init_spread( );
	
    // one session table for each DC 
	nr_sess_entries = NR_PROCS - NR_SYS_PROCS;
	for (int i = 0;i < NR_DCS; i++){ 
        sess_table[i].st_tab_ptr = (void*) 
			malloc(	sizeof(sess_entry_t) * nr_sess_entries ); 
        if( sess_table[i].st_tab_ptr == NULL) ERROR_EXIT(EDVSNOMEM);
		sess_ptr = (sess_entry_t *)sess_table[i].st_tab_ptr;
		// clear 
		sess_table[i].st_nr_sess = 0;	// clear # of used sessions 
		// MUTEX initialization
		sess_table[i].st_mutex; 
		pthread_mutex_init(&sess_table[i].st_mutex, NULL);
		for( int j = 0; j < nr_sess_entries; j++){
			sess_ptr->se_dcid 		= i;
			sess_ptr->se_clt_nodeid	= LB_INVALID;
			sess_ptr->se_clt_ep		= LB_INVALID;
			sess_ptr->se_clt_PID	= LB_INVALID;
			sess_ptr->se_lbclt_ep	= LB_INVALID;
			sess_ptr->se_lbsvr_ep	= LB_INVALID; 
			sess_ptr->se_svr_nodeid	= LB_INVALID;
			sess_ptr->se_svr_ep		= LB_INVALID;
			sess_ptr->se_svr_PID	= LB_INVALID;
			sess_ptr->se_service	= NULL; 
			sess_ptr->se_rmtcmd    = malloc(sizeof(MAXCMDLEN));
			if(sess_ptr->se_rmtcmd == NULL) ERROR_EXIT(errno);
			sess_ptr++;
		}
    }
    
	// Clear server and client tables 
	for( i = 0;  i < NR_NODES; i++ ){
		server_tab[i].svr_nodeid = LB_INVALID;
		client_tab[i].clt_nodeid = LB_INVALID;
		server_tab[i].svr_compress = LB_INVALID;
		client_tab[i].clt_compress = LB_INVALID;
		server_tab[i].svr_batch = LB_INVALID;
		client_tab[i].clt_batch = LB_INVALID;
		
		server_tab[i].svr_bm_sts= 0;
		server_tab[i].svr_start = NULL;
		server_tab[i].svr_stop 	= NULL;
		server_tab[i].svr_image = NULL;

		TAILQ_INIT(&server_tab[i].svr_tail_head);                      /* Initialize the queue. */
	
	}

	// Clear service table  
	for( i = 0;  i < MAX_SVC_NR; i++ ){
		service_tab[i].svc_extep 	= HARDWARE;
		service_tab[i].svc_minep 	= HARDWARE;
		service_tab[i].svc_maxep 	= HARDWARE;
		service_tab[i].svc_bind 	= LB_INVALID;
		service_tab[i].svc_dcid 	= LB_INVALID;
	}
	
    lb_config(argv[1]);  //Reads Config File

	if( (BLOCK_16K << lz4_preferences.frameInfo.blockSizeID) < MAXCOPYBUF)  {
		fprintf(stderr, "MAXCOPYBUF(%d) must be greater than (BLOCK_16K <<"
			"lz4_preferences.frameInfo.blockSizeID)(%d)\n",MAXCOPYBUF,
			(BLOCK_16K << lz4_preferences.frameInfo.blockSizeID));
		exit(1);
	}
		
	msgctl ( SVR_QUEUEBASE, IPC_RMID, 0);	
	// Creates One pair of proxy Servers for each Server 
	for( i = 0;  i < NR_NODES; i++ ){
		svr_ptr = &server_tab[i];
		if( svr_ptr->svr_nodeid == (-1)) continue;
		
		// Initialize Server Service Endpoints in use bitmap 
		svr_ptr->svr_bm_svc = 0;
		
		pthread_mutex_init(&svr_ptr->svr_mutex, NULL);
		
		pthread_mutex_init(&svr_ptr->svr_agent_mtx, NULL);
		pthread_cond_init(&svr_ptr->svr_agent_cond, NULL);
		
		pthread_mutex_init(&svr_ptr->svr_node_mtx, NULL);
		pthread_cond_init(&svr_ptr->svr_node_cond, NULL);

		pthread_mutex_init(&svr_ptr->svr_tail_mtx, NULL);

		svr_ptr->svr_level  = LVL_NOTINIT;
		svr_ptr->svr_load   = LVL_NOTINIT;
        USRDEBUG(SERVER_FORMAT, SERVER_FIELDS(svr_ptr));
        USRDEBUG(SERVER1_FORMAT, SERVER1_FIELDS(svr_ptr));

		spx_ptr = &svr_ptr->svr_spx;		
		// Creates Proxy Sender Message Queue
		spx_ptr->lbp_mqkey = (SVR_QUEUEBASE+i);
		spx_ptr->lbp_mqid   = msgget(spx_ptr->lbp_mqkey, IPC_CREAT | 0x660);
		if ( spx_ptr->lbp_mqid < 0) {
			if ( errno != EEXIST) {
				ERROR_EXIT(errno);
			}
			printf( "The queue with key=%d already exists\n",spx_ptr->lbp_mqkey);
			spx_ptr->lbp_mqid = msgget(spx_ptr->lbp_mqkey, 0);
			if(spx_ptr->lbp_mqid < 0) ERROR_EXIT(spx_ptr->lbp_mqid);
		}
		msgctl(spx_ptr->lbp_mqid , IPC_STAT, &spx_ptr->lbp_mqds);
        USRDEBUG("BEFORE " LBPX_MSGQ_FORMAT, LBPX_MSGQ_FIELDS(spx_ptr));
		spx_ptr->lbp_mqds.msg_qbytes =  (sizeof(msgq_buf_t) * NR_NODES);
		msgctl(spx_ptr->lbp_mqid, IPC_SET, &spx_ptr->lbp_mqds);
		msgctl(spx_ptr->lbp_mqid, IPC_STAT, &spx_ptr->lbp_mqds);
        USRDEBUG("AFTER  " LBPX_MSGQ_FORMAT, LBPX_MSGQ_FIELDS(spx_ptr));

		// Allocate memory for the Proxy Sender message queue buffer
		spx_ptr->lbp_mqbuf = malloc(sizeof(msgq_buf_t));
		if( spx_ptr->lbp_mqbuf == NULL) ERROR_EXIT(errno);
		
		// SERVER PROXY Sender Thread 
		rcode = pthread_create( &svr_ptr->svr_spx.lbp_thread, NULL, svr_Sproxy, i);
		if( rcode) ERROR_EXIT(rcode);
	
		// SERVER PROXY Receiver Thread 
		rcode = pthread_create( &svr_ptr->svr_rpx.lbp_thread, NULL, svr_Rproxy, i);
		if( rcode) ERROR_EXIT(rcode);
	}

	msgctl (CLT_QUEUEBASE, IPC_RMID, 0);
	// Creates One pair of proxy Servers for each Client 
	for( i = 0;  i < NR_NODES; i++ ){
		clt_ptr = &client_tab[i];
		if( clt_ptr->clt_nodeid == (-1)) continue;
        USRDEBUG(CLIENT_FORMAT, CLIENT_FIELDS(clt_ptr));

		pthread_mutex_init(&clt_ptr->clt_agent_mtx, NULL);
		pthread_cond_init(&clt_ptr->clt_agent_cond, NULL);

		pthread_mutex_init(&clt_ptr->clt_node_mtx, NULL);
		pthread_cond_init(&clt_ptr->clt_node_cond, NULL);

		cpx_ptr = &clt_ptr->clt_spx;		
		// Creates Proxy Sender Message Queue
		cpx_ptr->lbp_mqkey = (CLT_QUEUEBASE+i);
		cpx_ptr->lbp_mqid   = msgget(cpx_ptr->lbp_mqkey, IPC_CREAT | 0x660);
		if ( cpx_ptr->lbp_mqid  < 0) {
			if ( errno != EEXIST) {
				ERROR_EXIT(errno);
			}
			printf( "The queue with key=%d already exists\n",cpx_ptr->lbp_mqkey);
			cpx_ptr->lbp_mqid = msgget( cpx_ptr->lbp_mqkey, 0);
			if(cpx_ptr->lbp_mqid < 0) ERROR_EXIT(cpx_ptr->lbp_mqid);
		}
		msgctl(cpx_ptr->lbp_mqid , IPC_STAT, &cpx_ptr->lbp_mqds);
        USRDEBUG("BEFORE " LBPX_MSGQ_FORMAT, LBPX_MSGQ_FIELDS(cpx_ptr));
		cpx_ptr->lbp_mqds.msg_qbytes =  (sizeof(msgq_buf_t) * NR_NODES);
		msgctl(cpx_ptr->lbp_mqid, IPC_SET, &cpx_ptr->lbp_mqds);
		msgctl(cpx_ptr->lbp_mqid, IPC_STAT, &cpx_ptr->lbp_mqds);
        USRDEBUG("AFTER  " LBPX_MSGQ_FORMAT, LBPX_MSGQ_FIELDS(cpx_ptr));
		
		// Allocate memory for the Proxy Sender message queue buffer
		cpx_ptr->lbp_mqbuf = malloc(sizeof(msgq_buf_t));
		if( cpx_ptr->lbp_mqbuf == NULL) ERROR_EXIT(errno);

		// CLIENT PROXY Sender Thread 
		rcode = pthread_create( &clt_ptr->clt_spx.lbp_thread, NULL, clt_Sproxy, (void * restrict) i);
		if( rcode) ERROR_EXIT(rcode);
	
		// CLIENT PROXY Receiver Thread 
		rcode = pthread_create( &clt_ptr->clt_rpx.lbp_thread, NULL, clt_Rproxy, (void * restrict) i);
		if( rcode) ERROR_EXIT(rcode);
	}

	// Creates LB Monitor thread 
    rcode = pthread_create( &lb.lb_thread, NULL, lb_monitor, (void * restrict)lb.lb_nodeid);
    if( rcode) ERROR_EXIT(rcode);

#ifdef ONLY4TESTING
	sleep(10);
	rcode = unicast_cmd(1, "node1", "ls -l ");
	if( rcode != 0) ERROR_PRINT(rcode);
#endif // ONLY4TESTING

    // JOIN LB monitor
    rcode = pthread_join(lb.lb_thread, NULL);	
    USRDEBUG("LB Monitor exiting..\n");
	
	// A join for each pair of Client proxies  
	for( i = 0;  i < NR_NODES; i++){
		clt_ptr = &client_tab[i];
		if( clt_ptr->clt_nodeid == (-1)) continue;
		rcode = pthread_join(clt_ptr->clt_rpx.lbp_thread, NULL);	
		rcode = pthread_join(clt_ptr->clt_spx.lbp_thread, NULL);	
        USRDEBUG("Client[%d] proxies exiting..\n",i);
	}
	
	// A join for each pair of Server proxies  
	for( i = 0;  i < NR_NODES; i++ ){
		svr_ptr = &server_tab[i];
		if( svr_ptr->svr_nodeid == (-1)) continue;
		rcode = pthread_join(svr_ptr->svr_rpx.lbp_thread, NULL);	
		rcode = pthread_join(svr_ptr->svr_spx.lbp_thread, NULL);
        USRDEBUG("Server[%d] proxies exiting..\n",i);
	}
	stop_compression();
 
	return(OK);				
}




