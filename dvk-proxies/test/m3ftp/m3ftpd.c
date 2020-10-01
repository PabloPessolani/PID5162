#include "m3ftp.h"

int local_nodeid;
int ftpd_lpid;
int ftpd_ep;
int ftp_ep;
int dcid;
FILE *fp;
message *m_ptr;
char *path_ptr;
char *data_ptr;
dc_usr_t dc, *dc_usr_ptr;
proc_usr_t proc, *proc_usr_ptr;
dvs_usr_t dvs, *dvs_usr_ptr;
	
#define WAIT4BIND_MS 	1000
#define SEND_RECV_MS 		10000

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
void ftpd_reply(int rcode)
{
	int ret;

	USRDEBUG("ftpd_reply=%d\n", rcode);

	m_ptr->m_type = rcode;
	ret = dvk_send_T(m_ptr->m_source, m_ptr, SEND_RECV_MS);
	if (rcode == EDVSTIMEDOUT) {
		USRDEBUG("M3FTPD: ftpd_reply rcode=%d\n", rcode);
		return;
	}
	if( rcode < 0) ERROR_PRINT(rcode);
	return;
}
/*===========================================================================*
 *				ftpd_init					     *
 *===========================================================================*/
void ftpd_init(void)
{
	int  ret;
	
	/*-------- Open of pseudo DVK device --------------*/
	ret = dvk_open();
	if (ret < 0)  ERROR_EXIT(ret);
	
	/*---------------- Get DVS info ---------------*/
	dvs_usr_ptr = &dvs;
	ret = dvk_getdvsinfo(dvs_usr_ptr);
	if(ret < 0) {
 	    fprintf(stderr,"dvk_getdvsinfo error=%d \n", ret );
 	    exit(1);
	}
	local_nodeid = ret;
	USRDEBUG("local_nodeid=%d\n",local_nodeid);
	USRDEBUG(DVS_USR_FORMAT,DVS_USR_FIELDS(dvs_usr_ptr));
	
	/*---------------- Get DC info ---------------*/
	dc_usr_ptr = &dc;
	ret = dvk_getdcinfo(dcid, dc_usr_ptr);
	if(ret < 0) {
 	    fprintf(stderr,"dvk_getdcinfo error=%d \n", ret );
 	    exit(1);
	}
	USRDEBUG(DC_USR1_FORMAT,DC_USR1_FIELDS(dc_usr_ptr));
	USRDEBUG(DC_USR2_FORMAT,DC_USR2_FIELDS(dc_usr_ptr));
	
	/*---------------- Check Arguments ---------------*/
	if( ftpd_ep < 0 || ftpd_ep > (dc_usr_ptr->dc_nr_sysprocs - dc_usr_ptr->dc_nr_tasks) ){
   		fprintf(stderr, "ftpd_ep must be > 0 and < %d\n", 
			(dc_usr_ptr->dc_nr_sysprocs - dc_usr_ptr->dc_nr_tasks));
   		exit(1);
  	}

	/*----------------  SERVER binding ---------------*/
	ftpd_lpid = getpid();
    ret =  dvk_bind(dcid, ftpd_ep);
	if( ret < 0 ) {
		fprintf(stderr, "BIND ERROR ret=%d\n",ret);
	}
   	USRDEBUG("BIND SERVER  dcid=%d ftpd_lpid=%d ftpd_ep=%d\n",
		dcid, ftpd_lpid, ftpd_ep);
		
	/*---------------- Get SERVER PROC info ---------------*/
	proc_usr_ptr = &proc;
	ret = dvk_getprocinfo(dcid, ftpd_ep, proc_usr_ptr);
	if(ret < 0) {
 	    fprintf(stderr,"SERVER dvk_getprocinfo error=%d \n", ret );
 	    exit(1);
	}
	USRDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_usr_ptr));		
	
	/*---------------- Allocate memory for message  ---------------*/
	posix_memalign( (void **) &m_ptr, getpagesize(), sizeof(message) );
	if (m_ptr== NULL) {
   		ERROR_EXIT(-errno);
	}
	USRDEBUG("M3FTPD m_ptr=%p\n",m_ptr);

	/*---------------- Allocate memory for filename  ---------------*/
	posix_memalign( (void **) &path_ptr, getpagesize(), PATH_MAX );
	if (path_ptr== NULL) {
   		ERROR_EXIT(-errno);
	}
	USRDEBUG("M3FTPD path_ptr=%p\n",path_ptr);

	/*---------------- Allocate memory for data  ---------------*/
	posix_memalign( (void **) &data_ptr, getpagesize(), dvs_usr_ptr->d_max_copylen );
	if (data_ptr== NULL) {
   		ERROR_EXIT(-errno);
	}
	USRDEBUG("M3FTPD data_ptr=%p\n",data_ptr);	
	
}

/*===========================================================================*
 *				main					     *
 *   ftpd  <ftpd_ep> <ftp_ep> 
 * REQUEST:
 *	FTPOPER = m_type => FTP_GET, FTP_PUT
 *      FTPPATH = m1p1 => pathname address
 *      FTPPLEN  = m1i1 => lenght of pathname 
 *      FTPDATA = m1p2 => DATA address
 *      FTPDLEN  = m1i2 => lenght of DATA
 *===========================================================================*/
int  main ( int argc, char *argv[] )
{
	int rcode, ret, rlen, oper, cmd;
	double t_start, t_stop, t_total;
	long total_bytes;
	FILE *fp;
	char *dir_ptr;

	USRDEBUG("M3FTPD\n");
	if( argc != 3)  {
 	    fprintf(stderr,"Usage: ftpd <dcid> <ftpd_ep>\n");
 	    exit(1);
	}
		
	dcid 	= atoi(argv[1]);
	ftpd_ep = atoi(argv[2]);
	ftpd_init();

	dir_ptr = dirname(argv[0]);
	USRDEBUG("M3FTPD old cwd=%s\n", dir_ptr);
	rcode =  chdir(dir_ptr);
	if(rcode)ERROR_EXIT(-errno);
	dir_ptr = getcwd(path_ptr, PATH_MAX);
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

		cmd = m_ptr->FTPOPER;

		if (cmd != FTP_GET && cmd != FTP_PUT){
			fprintf(stderr,"M3FTPD: invalid request %d\n", m_ptr->FTPOPER);
			ftpd_reply(EDVSINVAL);
			continue;
		}

		if (m_ptr->FTPPLEN < 0 || m_ptr->FTPPLEN > (PATH_MAX-1)){
			fprintf(stderr,"M3FTPD: FTPPLEN=%d\n",  m_ptr->FTPPLEN);
			ftpd_reply(EDVSNAMESIZE);
			continue;			
		}
		
		if (m_ptr->FTPDLEN < 0 || m_ptr->FTPDLEN > (dvs_usr_ptr->d_max_copylen)){
			fprintf(stderr,"M3FTPD: FTDPLEN=%d\n",  m_ptr->FTPDLEN);
			ftpd_reply(EDVSMSGSIZE);
			continue;			
		}
		ftp_ep =  m_ptr->m_source;
		rcode = dvk_vcopy(ftp_ep, m_ptr->FTPPATH,
							SELF, path_ptr, m_ptr->FTPPLEN);
		if( rcode < 0) break;
		path_ptr[m_ptr->FTPPLEN] = 0;
		USRDEBUG("M3FTPD: path >%s<\n", path_ptr);
		
		m_ptr->m_type = OK;	
		rcode = dvk_send_T(ftp_ep, m_ptr, SEND_RECV_MS);
		if(rcode < 0) {
			ERROR_PRINT(rcode); 
			break;
		}
		
		switch(cmd){
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
				do	{	
					rcode = dvk_receive_T(ftp_ep, m_ptr, SEND_RECV_MS);
					if(rcode < 0) {
						ERROR_PRINT(rcode); 
						break;
					}	
					oper = m_ptr->FTPOPER;		
					if( oper != FTP_NEXT) {
						if( oper != FTP_CANCEL) {
							rcode = EDVSINVAL;
							ERROR_PRINT(rcode); 
						}
						break;
					}

					if ( (rlen = fread(data_ptr, 1, m_ptr->FTPDLEN, fp)) > 0) {
						USRDEBUG("M3FTPD: FTPDLEN=%d rlen=%d\n", m_ptr->FTPDLEN, rlen);			
						rcode = dvk_vcopy(SELF, data_ptr
								,ftp_ep, m_ptr->FTPDATA ,rlen);
						if(rcode < 0) {
							ERROR_PRINT(rcode); 
							break;
						}
						m_ptr->FTPDLEN = rlen;
						m_ptr->m_type = OK;					
						rcode = dvk_send_T(ftp_ep, m_ptr, SEND_RECV_MS);
						if(rcode < 0) {
							ERROR_PRINT(rcode); 
							break;
						}
						total_bytes += rlen;
					}
				} while ( rlen > 0);
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
				do	{	
					rcode = dvk_receive_T(ftp_ep,m_ptr, SEND_RECV_MS);
					if(rcode < 0) {
						ERROR_PRINT(rcode); 
						break;
					}			
					oper = m_ptr->FTPOPER;	
					if( oper != FTP_NEXT) {
						if( oper != FTP_CANCEL) {
							rcode = EDVSINVAL;
							ERROR_PRINT(rcode); 
						}
						break;
					}
					rcode = dvk_vcopy(ftp_ep, m_ptr->FTPDATA
									, SELF, data_ptr,m_ptr->FTPDLEN);
					if(rcode < 0) {
						ERROR_PRINT(rcode); 
						break;
					}
					
					if( m_ptr->FTPDLEN == 0 ) break;
					if( m_ptr->FTPDLEN > 0 ) {
						rlen = fwrite( data_ptr,1, m_ptr->FTPDLEN, fp);
						USRDEBUG("M3FTPD: FTPDLEN=%d rlen=%d\n", m_ptr->FTPDLEN, rlen);			
						m_ptr->FTPDLEN = rlen;
						m_ptr->m_type = OK;					
						total_bytes += rlen;
					} 
					rcode = dvk_send_T(ftp_ep, m_ptr, SEND_RECV_MS);
					if(rcode < 0) {
						ERROR_PRINT(rcode); 
						break;
					}
				} while ( m_ptr->FTPDLEN > 0);	
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
		ftpd_reply(rcode);
		t_stop = dwalltime();
		if( rcode) continue;
		/*--------- Report statistics  ---------*/
		t_total = (t_stop-t_start);
		fp = fopen("m3ftpd.log","a+");
		fprintf(fp, "t_start=%.2f t_stop=%.2f t_total=%.2f\n",t_start, t_stop, t_total);
		fprintf(fp, "total_bytes = %ld\n", total_bytes);
		if( cmd == FTP_PUT)
			fprintf(fp, "FTP_PUT [%s] Throuhput = %f [bytes/s]\n", path_ptr, (double)(total_bytes)/t_total);
		else 
			fprintf(fp, "FTP_GET [%s] Throuhput = %f [bytes/s]\n", path_ptr, (double)(total_bytes)/t_total);
		fclose(fp);
	}	
 }


	
