#include "m3ftp.h"

int local_nodeid;
int ftpd_lpid;
int ftpd_ep, ftp_ep;
dvs_usr_t dvs, *dvs_ptr;
dc_usr_t  dcu, *dc_ptr;
proc_usr_t m3ftpd, *m3ftpd_ptr;	
FILE *fp;
message *m_ptr;
char *path_ptr;
char *data_ptr;
int dcid;

char path[MNX_PATH_MAX];
message m;
char data[MAXCOPYLEN];
char logname[MNX_PATH_MAX];

double dwalltime()
{
	double sec;
	struct timeval tv;

	gettimeofday(&tv,NULL);
	sec = tv.tv_sec + tv.tv_usec/1000000.0;
	return sec;
}

/*===========================================================================*
 *				ftpd_reply					     *
 *===========================================================================*/
int ftpd_reply(int rcode)
{
	int ret, retry;

	USRDEBUG("ftpd_reply=%d\n", rcode);
	m_ptr->m_type = rcode;
	USRDEBUG("M3FTPD: reply " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
	for( retry = 0; retry < MAX_RETRIES; retry++) {
		ret = dvk_send_T(m_ptr->m_source, m_ptr, SEND_RECV_MS);
		if(ret == OK) break;
		if(ret == EDVSTIMEDOUT) {ERROR_PRINT(ret); continue;}
		if ( ret < 0) {ERROR_PRINT(ret); break;}		
	}
	if( rcode < 0) ERROR_PRINT(rcode);
	return(OK);
}
/*===========================================================================*
 *				ftpd_init					     *
 *===========================================================================*/
void ftpd_init(void)
{
  	int rcode;

	ftpd_lpid = getpid();
	USRDEBUG("ftpd_lpid=%d\n", ftpd_lpid);
	
	// habilito el uso de las APIs del DVK para el padre. Los hijos heredan 
	rcode = dvk_open();
	if (rcode < 0)  ERROR_EXIT (rcode);
	ftpd_lpid = getpid();
	
	ftpd_ep = dvk_bind(dcid,ftpd_ep);
	USRDEBUG("SERVER bind with endpoint: %d\n",ftpd_ep);
	if( ftpd_ep < EDVSERRCODE) ERROR_EXIT(ftpd_ep);
	USRDEBUG("binds SERVER: dcid=%d ftpd_lpid=%d ftpd_ep=%d\n",
			 dcid, ftpd_lpid, ftpd_ep);
	
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

	USRDEBUG("Get M3FTPD endpoint=%d info\n", ftpd_ep);
	m3ftpd_ptr = &m3ftpd;
	USRDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(m3ftpd_ptr));
	rcode = dvk_getprocinfo(dcid, ftpd_ep, m3ftpd_ptr);
	if( rcode < 0) ERROR_EXIT(rcode);
	USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(m3ftpd_ptr));
	USRDEBUG(PROC_WAIT_FORMAT, PROC_WAIT_FIELDS(m3ftpd_ptr));
	USRDEBUG(PROC_COUNT_FORMAT, PROC_COUNT_FIELDS(m3ftpd_ptr));

#ifdef ANULADO 
	/*---------------- Allocate memory for message  ---------------*/
	posix_memalign( (void **) &m_ptr, getpagesize(), sizeof(message) );
	if (m_ptr== NULL) {
   		ERROR_EXIT(-errno);
	}
	USRDEBUG("M3FTPD m_ptr=%p\n",m_ptr);

	/*---------------- Allocate memory for filename  ---------------*/
	posix_memalign( (void **) &path_ptr, getpagesize(), MNX_PATH_MAX );
	if (path_ptr== NULL) {
   		ERROR_EXIT(-errno);
	}
	USRDEBUG("M3FTPD path_ptr=%p\n",path_ptr);

	/*---------------- Allocate memory for data  ---------------*/
	posix_memalign( (void **) &data_ptr, getpagesize(), MAXCOPYBUF );
	if (data_ptr== NULL) {
   		ERROR_EXIT(-errno);
	}
	USRDEBUG("M3FTPD data_ptr=%p\n",data_ptr);	
#else //  ANULADO 
	m_ptr=&m;
	path_ptr = path;
	data_ptr = data;
#endif  // ANULADO 	

	if(rcode) ERROR_EXIT(rcode);
	
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
	int rcode, ret, rlen, oper, retry;
	double t_start, t_stop, t_total;
	long total_bytes;
	FILE *fp;
	char *dir_ptr;

	if ( argc != 3) {
		fprintf(stderr,  "Usage: %s <dcid> <ftpd_ep>\n", argv[0] );
		exit(EXIT_FAILURE);
	}
	
	dcid = atoi(argv[1]);
	if ( dcid < 0 || dcid >= NR_DCS) {
		fprintf(stderr,  "Invalid dcid [0-%d]\n", NR_DCS-1 );
		exit(EXIT_FAILURE);
	}
	
	ftpd_ep = atoi(argv[2]);

	ftpd_init();
	dir_ptr = dirname(argv[0]);
	USRDEBUG("M3FTPD old cwd=%s\n", dir_ptr);
	rcode =  chdir(dir_ptr);
	if(rcode)ERROR_EXIT(-errno);
	dir_ptr = getcwd(path_ptr, MNX_PATH_MAX-1);
	USRDEBUG("M3FTPD new cwd=%s\n", dir_ptr);
		
	while(TRUE){
		// Wait for file request 
		do { 
			rcode = dvk_receive_T(ANY,m_ptr, SEND_RECV_MS);
			USRDEBUG("M3FTPD: dvk_receive_T  rcode=%d\n", rcode);
			if (rcode == EDVSTIMEDOUT) {
				USRDEBUG("M3FTPD: dvk_receive_T TIMEOUT\n");
				continue ;
			}else if( rcode < 0) 
				ERROR_EXIT(EXIT_FAILURE);
		} while	(rcode < OK);
		USRDEBUG("M3FTPD: " MSG1_FORMAT, MSG1_FIELDS(m_ptr));

		if (m_ptr->FTPOPER != FTP_GET && m_ptr->FTPOPER != FTP_PUT){
			fprintf(stderr,"M3FTPD: invalid request %d\n", m_ptr->FTPOPER);
			ftpd_reply(EDVSINVAL);
			continue;
		}

		if (m_ptr->FTPPLEN < 0 || m_ptr->FTPPLEN > (MNX_PATH_MAX-1)){
			fprintf(stderr,"M3FTPD: FTPPLEN=%d\n",  m_ptr->FTPPLEN);
			ftpd_reply(EDVSNAMESIZE);
			continue;			
		}
		
		if (m_ptr->FTPDLEN < 0 || m_ptr->FTPDLEN > (MAXBUFLEN)){
			fprintf(stderr,"M3FTPD: FTDPLEN=%d\n",  m_ptr->FTPDLEN);
			ftpd_reply(EDVSMSGSIZE);
			continue;			
		}
		ftp_ep = m_ptr->m_source;
		oper   = m_ptr->FTPOPER;
		rcode = dvk_vcopy(ftp_ep, m_ptr->FTPPATH,
							ftpd_ep, path_ptr, m_ptr->FTPPLEN);
		if( rcode < 0){
			ERROR_PRINT(rcode);
			continue;
		}
		path_ptr[m_ptr->FTPPLEN] = 0;
		USRDEBUG("M3FTPD: path >%s<\n", path_ptr);
		
		// reply with its endpoint means OK and that for the next communications will use this endpoint
		ftpd_reply(ftpd_ep);
		
		switch(oper){
			case FTP_GET:
				USRDEBUG("M3FTPD: FTP_GET\n");
				fp = fopen(path_ptr, "r");
				if (fp == NULL){
					rcode = (-errno);
					ERROR_PRINT(rcode); 
					break;
				}
				USRDEBUG("M3FTPD: READ LOOP\n");
				t_start = dwalltime();
				total_bytes = 0;
				do 	{
					for( retry = 0; retry < MAX_RETRIES; retry++) {
						rcode = dvk_receive_T(ftp_ep,m_ptr, SEND_RECV_MS);
						if(rcode == OK) break;
						if(rcode == EDVSTIMEDOUT) {ERROR_PRINT(rcode); continue;}
						if ( rcode < 0) {ERROR_PRINT(rcode); break;}
					}
					if ( rcode < 0) break;
					USRDEBUG("M3FTPD: " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
					if( m_ptr->FTPOPER == FTP_CANCEL){
						ftpd_reply(FTP_CANCEL);
						break;
					}
					if( m_ptr->FTPOPER != FTP_GNEXT) break;
					
					if( (rlen = fread(data_ptr, 1, m_ptr->FTPDLEN, fp)) > 0) {
						USRDEBUG("M3FTPD: FTPDLEN=%d rlen=%d\n", m_ptr->FTPDLEN, rlen);			
						rcode = dvk_vcopy(ftpd_ep, data_ptr
									,ftp_ep, m_ptr->FTPDATA
									,rlen);
						if(rcode < 0) {
							ERROR_PRINT(rcode); 
							break;
						}
						m_ptr->FTPDLEN = rlen;
					}
					if( rlen <  m_ptr->FTPDLEN) {
						if( ferror(fp)){
							m_ptr->FTPDLEN = rlen;
							ftpd_reply(EDVSGENERIC);
							break;
						}
					} else {
						ftpd_reply(OK);
					}
					total_bytes += rlen;
				} while(rlen == m_ptr->FTPDLEN);
				USRDEBUG("M3FTPD: CLOSE \n");
				ret = fclose(fp);		
				if(rcode < 0) {
					ERROR_PRINT(rcode); 
					break;
				}
				rcode = ret;
				m_ptr->FTPDLEN = 0;
				break;
			case FTP_PUT:
				USRDEBUG("M3FTPD: FTP_PUT\n");
				fp = fopen(path_ptr, "w");					
				if (fp == NULL){
					rcode = -errno;
					ERROR_PRINT(rcode); 
					break;
				}
				USRDEBUG("M3FTPD: WRITE LOOP\n");
				t_start = dwalltime();
				total_bytes = 0;
				while( m_ptr->FTPDLEN > 0) {
					for( retry = 0; retry < MAX_RETRIES; retry++) {
						rcode = dvk_receive_T(ftp_ep,m_ptr, SEND_RECV_MS);
						if(rcode == OK) break;
						if(rcode == EDVSTIMEDOUT) {ERROR_PRINT(rcode); continue;}
						if ( rcode < 0) {ERROR_PRINT(rcode); break;}
					}		
					if ( rcode < 0) break;	
					
					if( m_ptr->FTPOPER != FTP_PNEXT) {
						if( m_ptr->FTPOPER != FTP_CANCEL) {
							rcode = EDVSINVAL;
							ERROR_PRINT(rcode); 
						}
						break;
					}		
					if( m_ptr->FTPDLEN == 0) {
						ftpd_reply(OK);
						break;
					}
					rcode = dvk_vcopy(ftp_ep, m_ptr->FTPDATA
								,ftpd_ep, data_ptr
								,m_ptr->FTPDLEN);
					if(rcode < 0) {
						ERROR_PRINT(rcode); 
						break;
					}
					rlen = fwrite(data_ptr, 1, m_ptr->FTPDLEN, fp);
					USRDEBUG("M3FTPD: FTPDLEN=%d rlen=%d\n", m_ptr->FTPDLEN, rlen);			
					m_ptr->FTPDLEN = rlen;
					m_ptr->m_type = OK;		
					for( retry = 0; retry < MAX_RETRIES; retry++) {
						rcode = dvk_send_T(ftp_ep, m_ptr, SEND_RECV_MS);
						if(rcode == OK) break;
						if(rcode == EDVSTIMEDOUT) {ERROR_PRINT(rcode); continue;}
						if ( rcode < 0) {ERROR_PRINT(rcode); break;}
					}		
					if ( rcode < 0) break;						

					total_bytes += rlen;
				}
				USRDEBUG("M3FTPD: CLOSE \n");
				ret = fclose(fp);		
				if(rcode < 0) {
					ERROR_PRINT(rcode); 
					break;
				}
				rcode = ret;
				break;
			default:
				fprintf(stderr,"M3FTPD: invalid request %d\n", m_ptr->FTPOPER);
				ERROR_EXIT(EDVSINVAL);
		}
		if( rcode < 0){
			ERROR_PRINT(rcode);
			fclose(fp);	
			continue;
		}
		ftpd_reply(rcode);
		t_stop = dwalltime();
		/*--------- Report statistics  ---------*/
		t_total = (t_stop-t_start);
		sprintf(logname, "m3ftpd_%d.log",ftpd_ep);
		fp = fopen(logname,"a+");
		if(oper == FTP_PUT){ 
			fprintf(fp, "FTP_PUT: t_start=%.2f t_stop=%.2f t_total=%.2f\n",t_start, t_stop, t_total);
			fprintf(fp, "FTP_PUT: total_bytes=%ld Throughput=%f [bytes/s]\n",
						total_bytes,(double)(total_bytes)/t_total);
		} if(oper == FTP_GET){
			fprintf(fp, "FTP_GET: t_start=%.2f t_stop=%.2f t_total=%.2f\n",t_start, t_stop, t_total);
			fprintf(fp, "FTP_GET: total_bytes=%ld Throughput=%f [bytes/s]\n",
						total_bytes,(double)(total_bytes)/t_total);;		
		}
		fclose(fp);	
	}
 }


	
