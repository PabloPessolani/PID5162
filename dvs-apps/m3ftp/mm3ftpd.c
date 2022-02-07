//##################################################
// Multi thread - Multi client M3 FTPD
//##################################################

#include "m3ftp.h"

int local_nodeid;
int ftpd_lpid, ftpd_ep;
int min_ep, max_ep;
dvs_usr_t dvs, *dvs_ptr;
dc_usr_t  dcu, *dc_ptr;
proc_usr_t m3ftpd, *m3ftpd_ptr;	
int dcid;
pthread_mutex_t ftpd_mutex; 
int nr_servers, max_servers;
message m;
unsigned int bm_servers;		
char path[MNX_PATH_MAX];
char logname[MNX_PATH_MAX];
pthread_cond_t  ftpd_cond; 
	
struct server_s{
	int svr_ep;				// server Endpoint
	double svr_start_secs; /* timestamp when transfer start */
	double svr_stop_secs;  /*timestamp when transfer start */
	long int	svr_byte_count;	 /* transfer byte count */
	char *svr_fname;			/* the name of the file to transfer */
	char *svr_buf;			/* pointer to the transfer buffer */
	message *svr_msg;			/* pointer to the server message */
    pthread_t 	svr_thread;
	pthread_mutex_t svr_mutex; 
	pthread_cond_t  svr_cond;  
};
typedef struct server_s server_t;

server_t server_tab[NR_SYS_PROCS-NR_TASKS];

double dwalltime()
{
	double sec;
	struct timeval tv;

	gettimeofday(&tv,NULL);
	sec = tv.tv_sec + tv.tv_usec/1000000.0;
	return sec;
}

////////////////////////////////////////////////////////////
//		get_free_server()
////////////////////////////////////////////////////////////
int get_free_server(void)
{
	int i;
	
	USRDEBUG("MM3FTPD: max_servers=%d bm_servers=%0X\n", max_servers, bm_servers);
	MTX_LOCK(ftpd_mutex);
	for(i = 0; i <= max_servers; i++){
		if( TEST_BIT(bm_servers, i) == 0){
			SET_BIT(bm_servers, i);
			nr_servers++;
			MTX_UNLOCK(ftpd_mutex);
			USRDEBUG("MM3FTPD found free server:%d\n", i);
			return(i);
		}
	}
	MTX_UNLOCK(ftpd_mutex);
	ERROR_RETURN(EDVSNOSPC);
}
	
////////////////////////////////////////////////////////////
//		allocate_memory()
////////////////////////////////////////////////////////////
int allocate_memory(int svr_id)
{
	server_t *svr_ptr;
	
	USRDEBUG("MM3FTPD[%d]\n",svr_id);

	svr_ptr = &server_tab[svr_id];
	
		/*---------------- Allocate memory for message  ---------------*/
//	posix_memalign( (void **) &svr_ptr->svr_msg, getpagesize(), sizeof(message) );
	svr_ptr->svr_msg = (message *) malloc(sizeof(message));
	if (svr_ptr->svr_msg == NULL) {
   		ERROR_RETURN(-errno);
	}
	USRDEBUG("MM3FTPD[%d] svr_msg=%p\n", svr_id, svr_ptr->svr_msg);
		
	/*---------------- Allocate memory for data  ---------------*/
//	posix_memalign( (void **) &svr_ptr->svr_buf, getpagesize(), dvs_ptr->d_max_copylen );
	svr_ptr->svr_buf = (char *) malloc(dvs_ptr->d_max_copylen);
	if (svr_ptr->svr_buf == NULL) {
   		ERROR_EXIT(-errno);
	}
	USRDEBUG("MM3FTPD[%d] svr_buf=%p\n",svr_id, svr_ptr->svr_buf);
		
	/*---------------- Allocate memory for filename  ---------------*/
//	posix_memalign( (void **) &svr_ptr->svr_fname, getpagesize(), MNX_PATH_MAX );
	svr_ptr->svr_fname = (char *) malloc(MNX_PATH_MAX);
	if (svr_ptr->svr_fname == NULL) {
   		ERROR_RETURN(-errno);
	}
	USRDEBUG("MM3FTPD[%d] svr_fname=%p\n", svr_id, svr_ptr->svr_fname);

	return(OK);
}

////////////////////////////////////////////////////////////
// 			free_memory
////////////////////////////////////////////////////////////
void free_memory(int  svr_id)
{
	server_t *svr_ptr;

	USRDEBUG("MM3FTPD[%d]\n",svr_id);
	
	svr_ptr = &server_tab[svr_id];
	
	/*---------------- Free message memory ---------------*/
	if (svr_ptr->svr_msg != NULL) {
   		free(svr_ptr->svr_msg);
	}

	/*---------------- Free  filename memory  ---------------*/
	if (svr_ptr->svr_fname != NULL) {
   		free(svr_ptr->svr_fname);
	}

	/*---------------- Free memory for data  ---------------*/
	if (svr_ptr->svr_buf != NULL) {
   		free(svr_ptr->svr_buf);
	}
}

/*===========================================================================*
 *				main_reply					     *
 *===========================================================================*/
int main_reply(int rcode)
{
	int ret, retry;
	message *m_ptr;
	
	m_ptr = &m;
	m_ptr->m_type = rcode;
	USRDEBUG("MM3FTPD: dst_ep=%d rcode=%d\n",m_ptr->m_source, rcode);
	USRDEBUG("MM3FTPD: reply " MSG1_FORMAT, MSG1_FIELDS(m_ptr));

	for( retry = 0; retry < MAX_RETRIES; retry++) {
		ret = dvk_send_T(m_ptr->m_source, m_ptr, SEND_RECV_MS);
		if(ret == OK) break;
		if(ret == EDVSTIMEDOUT) {ERROR_PRINT(ret); continue;}
		if ( ret < 0) {ERROR_PRINT(ret); break;}		
	}
	if( ret < 0) ERROR_PRINT(ret);
	return(OK);
}

/*===========================================================================*
 *				ftpd_reply					     *
 *===========================================================================*/
int ftpd_reply(int svr_id, int rcode)
{
	int ret, retry;
	message *m_ptr;
	
	m_ptr = server_tab[svr_id].svr_msg;
	m_ptr->m_type = rcode;
	USRDEBUG("MM3FTPD[%d]: dst_ep=%d rcode=%d\n", svr_id, m_ptr->m_source, rcode);

	USRDEBUG("MM3FTPD[%d]: reply " MSG1_FORMAT, svr_id, MSG1_FIELDS(m_ptr));
	for( retry = 0; retry < MAX_RETRIES; retry++) {
		ret = dvk_send_T(m_ptr->m_source, m_ptr, SEND_RECV_MS);
		if(ret == OK) break;
		if(ret == EDVSTIMEDOUT) {ERROR_PRINT(ret); continue;}
		if ( ret < 0) {ERROR_PRINT(ret); break;}		
	}
	if( ret < 0) ERROR_PRINT(ret);
	return(OK);
}

/*===========================================================================*
 *				ftpd_init					     *
 *===========================================================================*/
void ftpd_init(void)
{
  	int rcode, ep;
	server_t *svr_ptr;

	ftpd_lpid = getpid();
	USRDEBUG("ftpd_lpid=%d\n", ftpd_lpid);
	
	// habilito el uso de las APIs del DVK para el padre. Los hijos heredan 
	rcode = dvk_open();
	if (rcode < 0)  ERROR_EXIT (rcode);
	ftpd_lpid = getpid();
	
	USRDEBUG("Get the DVS info\n");
	local_nodeid = dvk_getdvsinfo(&dvs);
	USRDEBUG("local_nodeid=%d\n",local_nodeid);
	if( local_nodeid < DVS_NO_INIT) ERROR_EXIT(local_nodeid);
	dvs_ptr = &dvs;
	USRDEBUG(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));
	
	USRDEBUG("Get the DC info\n");
	USRDEBUG("dcid=%d\n", dcid);
	if ( dcid < 0 || dcid >= dvs.d_nr_dcs) {
 	        fprintf(stderr,"Invalid dcid [0-%d]\n", dvs.d_nr_dcs );
 	        ERROR_EXIT(EDVSBADDCID);
	}
	rcode = dvk_getdcinfo(dcid, &dcu);
	if( rcode ) ERROR_EXIT(rcode);
	dc_ptr = &dcu;
	USRDEBUG(DC_USR1_FORMAT, DC_USR1_FIELDS(dc_ptr));
	USRDEBUG(DC_USR2_FORMAT, DC_USR2_FIELDS(dc_ptr));
	
	if( (ftpd_ep <= 0) 
	||  (ftpd_ep >= dc_ptr->dc_nr_sysprocs)) {
 	    fprintf(stderr,"Invalid ftpd_ep (0 < ftpd_ep < %d\n", 
				dc_ptr->dc_nr_sysprocs);
 	    ERROR_EXIT(EDVSENDPOINT);
	} 
	
	if( (min_ep <= 0) 
	||  (min_ep >= (dc_ptr->dc_nr_sysprocs - dc_ptr->dc_nr_tasks))) {
 	    fprintf(stderr,"Invalid min_ep (0 < min_ep < %d\n", 
				(dc_ptr->dc_nr_sysprocs - dc_ptr->dc_nr_tasks));
 	    ERROR_EXIT(EDVSENDPOINT);
	} 

	if( (max_ep <= 0) 
	||  (max_ep >= (dc_ptr->dc_nr_sysprocs - dc_ptr->dc_nr_tasks))) {
 	    fprintf(stderr,"Invalid max_ep (0 < max_ep < %d)\n", 
				(dc_ptr->dc_nr_sysprocs - dc_ptr->dc_nr_tasks));
 	    ERROR_EXIT(EDVSENDPOINT);
	} 

	if( min_ep > max_ep) {
 	    fprintf(stderr,"Invalid: min_ep must be greater than max_ep\n");
 	    ERROR_EXIT(EDVSENDPOINT);
	} 

	if( (ftpd_ep >= min_ep) && (ftpd_ep <= min_ep) ) {
 	    fprintf(stderr,"Invalid: ftpd_ep must be out of the range of [min_ep,max_ep]\n");
 	    ERROR_EXIT(EDVSENDPOINT);
	} 
	
	max_servers = (max_ep - min_ep) + 1;
	if( max_servers > (sizeof(bm_servers)*8)) {
 	    fprintf(stderr,"Invalid: endpoint range [min_ep, max_ep] must be lower than %d\n", 
			(sizeof(bm_servers)*8));
 	    ERROR_EXIT(EDVSENDPOINT);
	} 
	nr_servers  = 0;
	bm_servers  = 0;

#ifdef ANULADO	
	/*---------------- Allocate memory for server table  ---------------*/
	posix_memalign( (void **) &server_tab, getpagesize(), nr_servers * sizeof(server_t));
	if (server_tab == NULL) {
   		ERROR_RETURN(-errno);
	}
#endif //  ANULADO	
	
	ftpd_ep = dvk_bind(dcid,ftpd_ep);
	USRDEBUG("SERVER bind with endpoint: %d\n",ftpd_ep);
	if( ftpd_ep < EDVSERRCODE) ERROR_EXIT(ftpd_ep);
	USRDEBUG("binds SERVER: dcid=%d ftpd_lpid=%d ftpd_ep=%d\n",
			 dcid, ftpd_lpid, ftpd_ep);

	USRDEBUG("Get MM3FTPD endpoint=%d info\n", ftpd_ep);
	m3ftpd_ptr = &m3ftpd;
	USRDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(m3ftpd_ptr));
	rcode = dvk_getprocinfo(dcid, ftpd_ep, m3ftpd_ptr);
	if( rcode < 0) ERROR_EXIT(rcode);
	USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(m3ftpd_ptr));
	USRDEBUG(PROC_WAIT_FORMAT, PROC_WAIT_FIELDS(m3ftpd_ptr));
	USRDEBUG(PROC_COUNT_FORMAT, PROC_COUNT_FIELDS(m3ftpd_ptr));

	for( ep = min_ep; ep <= max_ep; ep++){
		svr_ptr = &server_tab[ep-min_ep];
		svr_ptr->svr_ep = ep;
		pthread_mutex_init(&svr_ptr->svr_mutex, NULL);
		pthread_cond_init(&svr_ptr->svr_cond, NULL);
	}

	pthread_mutex_init(&ftpd_mutex, NULL);
	pthread_cond_init(&ftpd_cond, NULL);

	if(rcode) ERROR_EXIT(rcode);
	
}

////////////////////////////////////////////////////////////		
//			server_ftp	
////////////////////////////////////////////////////////////		
void *server_ftp(void *arg)
{		
	int svr_id, ftp_ep, ftpd_ep, rcode, oper, rlen, retry;
	FILE *fp;
	double	t_total;
  	server_t *svr_ptr;
	message *m_ptr;
	char *data_ptr;
	pid_t tid;
	
	svr_id = (int) arg;
 	
	svr_ptr = &server_tab[svr_id];
	tid = (pid_t) syscall (SYS_gettid);
	USRDEBUG("MM3FTPD[%d] tid=%d svr_ep=%d\n",svr_id, tid, svr_ptr->svr_ep);

	MTX_LOCK(svr_ptr->svr_mutex);
	ftpd_ep = dvk_lclbind(dcid,tid,svr_ptr->svr_ep); 
	USRDEBUG("MM3FTPD[%d] bind with endpoint: %d\n",svr_id, ftpd_ep);
	if( ftpd_ep < EDVSERRCODE){
		ERROR_PRINT(ftpd_ep);
		MTX_UNLOCK(svr_ptr->svr_mutex);
		goto svr_free_resources;
	}
	USRDEBUG("MM3FTPD[%d]: BIND dcid=%d ftpd_ep=%d\n", dcid, ftpd_ep);

	m_ptr	= svr_ptr->svr_msg;
	data_ptr= svr_ptr->svr_buf;
	USRDEBUG("MM3FTPD[%d]: " MSG1_FORMAT, svr_id, MSG1_FIELDS(m_ptr));

	ftp_ep = m_ptr->m_source;
	oper   = m_ptr->FTPOPER;
	rcode = dvk_vcopy(ftp_ep, m_ptr->FTPPATH,
						svr_ptr->svr_ep, svr_ptr->svr_fname, m_ptr->FTPPLEN);
	if( rcode < 0){
		ERROR_PRINT(rcode);
		ftpd_reply(svr_id, rcode);
		goto svr_free_resources;
	}
	svr_ptr->svr_fname[m_ptr->FTPPLEN] = 0;
	USRDEBUG("MM3FTPD[%d]: svr_fname >%s<\n", svr_id, svr_ptr->svr_fname);
	
//	ftpd_reply(svr_id, OK);
			
	COND_SIGNAL(ftpd_cond);
	COND_WAIT(svr_ptr->svr_cond, svr_ptr->svr_mutex);
	MTX_UNLOCK(svr_ptr->svr_mutex);
	
	switch(oper){
		case FTP_GET:
			USRDEBUG("MM3FTPD[%d]: FTP_GET\n", svr_id);
			fp = fopen(svr_ptr->svr_fname, "r");
			if (fp == NULL){
				rcode = (-errno);
				ERROR_PRINT(rcode); 
				ftpd_reply(svr_id, rcode);
				goto svr_free_resources;
				break;
			}
			USRDEBUG("MM3FTPD[%d]: FTP_GET READ LOOP\n", svr_id );
			svr_ptr->svr_start_secs = dwalltime();
			svr_ptr->svr_byte_count = 0;
			do 	{
				for( retry = 0; retry < MAX_RETRIES; retry++) {
					rcode = dvk_receive_T(ftp_ep,m_ptr, SEND_RECV_MS);
					if(rcode == OK) break;
					if(rcode == EDVSTIMEDOUT) {ERROR_PRINT(rcode); continue;}
					if ( rcode < 0) break;
				}
				if ( rcode < 0) {
					ERROR_PRINT(rcode); 
					ftpd_reply(svr_id, rcode);
					goto svr_close_fd;
				}
				USRDEBUG("MM3FTPD[%d]: " MSG1_FORMAT, svr_id, MSG1_FIELDS(m_ptr));
				if( m_ptr->FTPOPER == FTP_CANCEL){
					ftpd_reply(svr_id, FTP_CANCEL);
					goto svr_close_fd;
				}
				if( m_ptr->FTPOPER != FTP_GNEXT){
					ftpd_reply(svr_id, FTP_CANCEL);
					goto svr_close_fd;
				}
				if( m_ptr->FTPDLEN == 0) {
					ftpd_reply(svr_id, OK);
					goto svr_close_fd;
				}
				if( (rlen = fread(data_ptr, 1, m_ptr->FTPDLEN, fp)) > 0) {
					USRDEBUG("MM3FTPD[%d]: FTPDLEN=%d rlen=%d\n", svr_id, m_ptr->FTPDLEN, rlen);			
					rcode = dvk_vcopy(svr_ptr->svr_ep, data_ptr
								,ftp_ep, m_ptr->FTPDATA
								,rlen);
					if(rcode < 0) {
						ERROR_PRINT(rcode); 
						goto svr_close_fd;
					}
					m_ptr->FTPDLEN = rlen;
				}
				if( rlen <  m_ptr->FTPDLEN) {
					if( ferror(fp)){
						m_ptr->FTPDLEN = rlen;
						ftpd_reply(svr_id, EDVSGENERIC);
						goto svr_close_fd;
					}
				} else if (rlen == 0) {
					break; 
				} else {	
					ftpd_reply(svr_id, OK);
				}
				svr_ptr->svr_byte_count += rlen;
			} while(rlen == m_ptr->FTPDLEN);
			if(rcode < 0) {
				ERROR_PRINT(rcode); 
			} else {
				rcode = OK;
			}
			m_ptr->FTPDLEN = 0;
			break;
		case FTP_PUT:
			USRDEBUG("MM3FTPD[%d]: FTP_PUT\n", svr_id);
			fp = fopen(svr_ptr->svr_fname, "w");					
			if (fp == NULL){
				rcode = -errno;
				ERROR_PRINT(rcode); 
				ftpd_reply(svr_id, rcode);
				goto svr_free_resources;			
				break;
			}
			USRDEBUG("MM3FTPD[%d]: WRITE LOOP\n", svr_id);
			svr_ptr->svr_start_secs = dwalltime();
			svr_ptr->svr_byte_count = 0;
			while( m_ptr->FTPDLEN > 0) {
				for( retry = 0; retry < MAX_RETRIES; retry++) {
					rcode = dvk_receive_T(ftp_ep,m_ptr, SEND_RECV_MS);
					if(rcode == OK) break;
					if(rcode == EDVSTIMEDOUT) {ERROR_PRINT(rcode); continue;}
					if ( rcode < 0) break;	
				}		
				if ( rcode < 0) {
					ERROR_PRINT(rcode); 
					ftpd_reply(svr_id, rcode);
					goto svr_close_fd;
				}
				USRDEBUG("MM3FTPD[%d]: " MSG1_FORMAT, svr_id, MSG1_FIELDS(m_ptr));
				if( m_ptr->FTPOPER == FTP_CANCEL){
					ftpd_reply(svr_id, FTP_CANCEL);
					goto svr_close_fd;
				}
				if( m_ptr->FTPOPER != FTP_PNEXT){
					ftpd_reply(svr_id, FTP_CANCEL);
					goto svr_close_fd;
				}
				rcode = dvk_vcopy(ftp_ep, m_ptr->FTPDATA
							,svr_ptr->svr_ep, data_ptr
							,m_ptr->FTPDLEN);
				if(rcode < 0) {
					ERROR_PRINT(rcode);
					ftpd_reply(svr_id, rcode);
					goto svr_close_fd;
				}
				rlen = fwrite(data_ptr, 1, m_ptr->FTPDLEN, fp);
				USRDEBUG("MM3FTPD: FTPDLEN=%d rlen=%d\n", m_ptr->FTPDLEN, rlen);			
				m_ptr->FTPDLEN = rlen;
				ftpd_reply(svr_id, OK);
				svr_ptr->svr_byte_count += rlen;
			}
			rcode = OK;
			break;
		default:
			fprintf(stderr,"MM3FTPD: invalid request %d\n", m_ptr->FTPOPER);
			ERROR_EXIT(EDVSINVAL);
	}
	if( rcode < 0){
		ERROR_PRINT(rcode);
	}
	ftpd_reply(svr_id, rcode);
	fclose(fp);
	
	svr_ptr->svr_stop_secs = dwalltime();
	/*--------- Report statistics  ---------*/
	t_total = (svr_ptr->svr_stop_secs - svr_ptr->svr_start_secs);
	sprintf(logname, "m3ftpd_%d.log",svr_ptr->svr_ep);
	fp = fopen(logname,"a+");
	if(oper == FTP_PUT){ 
		fprintf(fp, "FTP_PUT[%d]: t_start=%.2f t_stop=%.2f t_total=%.2f\n", svr_id,
			svr_ptr->svr_start_secs, svr_ptr->svr_stop_secs, t_total);
		fprintf(fp, "FTP_PUT[%d]: total_bytes=%ld Throughput=%f [bytes/s]\n",svr_id,
					svr_ptr->svr_byte_count,(double)(svr_ptr->svr_byte_count)/t_total);
	} if(oper == FTP_GET){
		fprintf(fp, "FTP_GET[%d]: t_start=%.2f t_stop=%.2f t_total=%.2f\n",svr_id,
			svr_ptr->svr_start_secs, svr_ptr->svr_stop_secs, t_total);
		fprintf(fp, "FTP_GET[%d]: total_bytes=%ld Throughput=%f [bytes/s]\n",svr_id,
					svr_ptr->svr_byte_count,(double)(svr_ptr->svr_byte_count)/t_total);
	}
	
svr_close_fd:
	fclose(fp);	
svr_free_resources:
	free_memory(svr_id);
svr_free_server:
	MTX_LOCK(ftpd_mutex);
	CLR_BIT(bm_servers, svr_id);
	nr_servers--;
	MTX_UNLOCK(ftpd_mutex);
	pthread_exit(NULL);
 }


/*===========================================================================*
 *				main					     *
 * REQUEST:
 *	FTPOPER = m_type => FTP_GET, FTP_PUT
 *      FTPPATH = m1p1 => pathname address
 *      FTPPLEN  = m1i1 => lenght of pathname 
 *      FTPDATA = m1p2 => DATA address
 *      FTPDLEN  = m1i2 => lenght of DATA
 *===========================================================================*/
int  main ( int argc, char *argv[] )
{
	int rcode, ret, rlen, oper, retry, svr_id;
	double t_start, t_stop, t_total;
	long total_bytes;
	FILE *fp;
	char *dir_ptr;
	message *m_ptr;
	server_t *svr_ptr;
	
	if ( argc != 5) {
		fprintf(stderr,  "Usage: %s <dcid> <ftpd_ep> <min_ep> <max_ep>\n", argv[0] );
		exit(EXIT_FAILURE);
	}
	
	dcid = atoi(argv[1]);
	if ( dcid < 0 || dcid >= NR_DCS) {
		fprintf(stderr,  "Invalid dcid [0-%d]\n", NR_DCS-1 );
		exit(EXIT_FAILURE);
	}
	
	ftpd_ep = atoi(argv[2]);
	min_ep 	= atoi(argv[3]);
	max_ep 	= atoi(argv[4]);

	ftpd_init();
	
	dir_ptr = dirname(argv[0]);
	USRDEBUG("MM3FTPD old cwd=%s\n", dir_ptr);
	rcode =  chdir(dir_ptr);
	if(rcode)ERROR_EXIT(-errno);
	dir_ptr = getcwd(path, MNX_PATH_MAX-1);
	USRDEBUG("MM3FTPD new cwd=%s\n", dir_ptr);
	
	m_ptr = &m;
	while(TRUE){
		// Wait for file request 
		do { 
			// busca un servidor libre 
			svr_id = get_free_server();
			if(svr_id < 0){
				fprintf(stderr, "Not free server endpoint\n");
				sleep(SEND_RECV_MS);
				continue;
			}
			rcode  = allocate_memory(svr_id);
			if(rcode < 0){
				fprintf(stderr, "Can't allocate enoght memory: %d\n", rcode);
				sleep(SEND_RECV_MS);
				goto free_server;
			}
			
			svr_ptr = &server_tab[svr_id];
			do {
				rcode = dvk_receive_T(ANY, m_ptr, SEND_RECV_MS);
				USRDEBUG("MM3FTPD: dvk_receive_T  rcode=%d\n", rcode);
				if (rcode == EDVSTIMEDOUT) {
					USRDEBUG("MM3FTPD: dvk_receive_T TIMEOUT\n");		
					continue ;
				}else if( rcode < OK) 
					ERROR_EXIT(EXIT_FAILURE);
			} while (rcode < OK);
		} while	(rcode < OK);
		USRDEBUG("MM3FTPD: " MSG1_FORMAT, MSG1_FIELDS(m_ptr));

		if (m_ptr->FTPOPER != FTP_GET && m_ptr->FTPOPER != FTP_PUT){
			fprintf(stderr,"MM3FTPD: invalid request %d\n", m_ptr->FTPOPER);
			main_reply(EDVSINVAL);
			goto free_resources;
			continue;
		}

		if (m_ptr->FTPPLEN < 0 || m_ptr->FTPPLEN > (MNX_PATH_MAX-1)){
			fprintf(stderr,"MM3FTPD: FTPPLEN=%d\n",  m_ptr->FTPPLEN);
			main_reply(EDVSNAMESIZE);
			goto free_resources;
		}
		
		if (m_ptr->FTPDLEN < 0 || m_ptr->FTPDLEN > dvs_ptr->d_max_copylen){
			fprintf(stderr,"MM3FTPD: FTDPLEN=%d\n",  m_ptr->FTPDLEN);
			main_reply(EDVSMSGSIZE);
			goto free_resources;
			continue;			
		}
		
		memcpy(svr_ptr->svr_msg, m_ptr, sizeof(message));
		MTX_LOCK(svr_ptr->svr_mutex);
		rcode = pthread_create( &svr_ptr->svr_thread, NULL, server_ftp, svr_id);
		COND_WAIT(ftpd_cond, svr_ptr->svr_mutex);
		USRDEBUG("MM3FTPD: rcode=%d svr_id=%d\n", rcode, svr_id);
		if(rcode < 0) {
			ERROR_PRINT(rcode);
			main_reply(rcode);
		} else {
			// reply with the server endpoint means OK and that for the next communications will use this endpoint
			main_reply(svr_id+min_ep);
		}
		COND_SIGNAL(svr_ptr->svr_cond);
		MTX_UNLOCK(svr_ptr->svr_mutex);
		continue;
		
free_resources:
		USRDEBUG("MM3FTPD: free_resources svr_id=%d\n", svr_id);
		free_memory(svr_id);
free_server:
		USRDEBUG("MM3FTPD: free_server svr_id=%d\n", svr_id);
		MTX_LOCK(ftpd_mutex);
		CLR_BIT(bm_servers, svr_id);
		nr_servers--;
		MTX_UNLOCK(ftpd_mutex);
		USRDEBUG("MM3FTPD: nr_servers=%d bm_servers=%0X\n", nr_servers, bm_servers);
	}
}
	
#ifdef ANULADO 
		// wait for child server binding  
		for( retry = 0; retry < MAX_RETRIES; retry++){
			USRDEBUG("MM3FTPD: dvk_wait4bindep_T svr_ep=%d retry=%d\n", 
				svr_ptr->svr_ep, retry);
			rcode = dvk_wait4bindep_T(svr_ptr->svr_ep, BIND_TIMEOUT);
			if( rcode >= EDVSERRCODE) break;
		}
		if(rcode < EDVSERRCODE) {
			ERROR_PRINT(rcode);
			main_reply(rcode);
			goto free_resources;
		}
#endif // ANULADO 
	



