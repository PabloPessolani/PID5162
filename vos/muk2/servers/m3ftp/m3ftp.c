#include "m3ftp.h"

int local_nodeid;
int dcid;
int ftp_lpid;
int fs_ep = 1;
int pm_ep = 0;

int ftp_ep, ftpd_ep;
dvs_usr_t dvs, *dvs_ptr;
dc_usr_t  dcu, *dc_ptr;
proc_usr_t m3ftp, *m3ftp_ptr;	
proc_usr_t m3ftpd, *m3ftpd_ptr;	
FILE *fp;
message *m_ptr;
char *path_ptr;
char *data_ptr;

#define WAIT4BIND_MS 		1000
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
 *				ftp_reply					     *
 *===========================================================================*/
void ftp_reply(int rcode)
{
	int ret;
	
	m_ptr->m_type = rcode;
	ret = muk_send_T(m_ptr->m_source, m_ptr, SEND_RECV_MS);
	if (rcode == EDVSTIMEDOUT) {
		MUKDEBUG("M3FTP: ftp_reply rcode=%d\n", rcode);
		return;
	}
	if( rcode < 0) ERROR_EXIT(rcode);
	return;
}
/*===========================================================================*
 *				ftp_init					     *
 *===========================================================================*/
void ftp_init(void)
{
  	int rcode;

	ftp_lpid = getpid();
	MUKDEBUG("ftp_lpid=%d\n", ftp_lpid);

	rcode = muk_open();
	if (rcode < 0)  ERROR_EXIT(rcode);

#ifdef DEMONIZE	
	do { 
		rcode = muk_wait4bind_T(WAIT4BIND_MS);
		MUKDEBUG("M3FTP: muk_wait4bind_T  rcode=%d\n", rcode);
		if (rcode == EDVSTIMEDOUT) {
			MUKDEBUG("M3FTP: muk_wait4bind_T TIMEOUT\n");
			continue ;
		}else if( rcode < 0) 
			ERROR_EXIT(EXIT_FAILURE);
	} while	(rcode < OK);
#else // DEMONIZE	
	ftp_ep = FTP_CLT_EP;
	rcode = muk_bind(dcid, ftp_ep);
	MUKDEBUG("rcode=%d\n", rcode);
	if( rcode != ftp_ep) {
		ERROR_PRINT(EDVSENDPOINT);
		exit(-1);
	}
#endif // DEMONIZE	
	
	MUKDEBUG("Get the DVS info from SYSTASK\n");
    rcode = sys_getkinfo(&dvs);
	if(rcode) ERROR_EXIT(rcode);
	dvs_ptr = &dvs;
	MUKDEBUG(DVS_USR_FORMAT,DVS_USR_FIELDS(dvs_ptr));

	MUKDEBUG("Get the VM info from SYSTASK\n");
	rcode = sys_getmachine(&dcu);
	if(rcode) ERROR_EXIT(rcode);
	dc_ptr = &dcu;
	MUKDEBUG(DC_USR1_FORMAT,DC_USR1_FIELDS(dc_ptr));
	MUKDEBUG(DC_USR2_FORMAT,DC_USR2_FIELDS(dc_ptr));

	ftp_ep = muk_getep(ftp_lpid);
	MUKDEBUG("Get M3FTP endpoint=%d info from SYSTASK\n", ftp_ep);
	rcode = sys_getproc(&m3ftp, ftp_ep);
	if(rcode) ERROR_EXIT(rcode);
	m3ftp_ptr = &m3ftp;
	MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(m3ftp_ptr));
	
	MUKDEBUG("Get M3FTPD endpoint=%d info from SYSTASK\n", ftpd_ep);
	rcode = sys_getproc(&m3ftpd, ftpd_ep);
	if(rcode) ERROR_EXIT(rcode);
	m3ftpd_ptr = &m3ftpd;
	MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(m3ftpd_ptr));
	if( TEST_BIT(m3ftpd_ptr->p_rts_flags, BIT_SLOT_FREE)) {
		fprintf(stderr, "m3ftpd not started\n");	
	}
	
	/*---------------- Allocate memory for message  ---------------*/
	posix_memalign( (void **) &m_ptr, getpagesize(), sizeof(message) );
	if (m_ptr== NULL) {
   		ERROR_EXIT(-errno);
	}
	MUKDEBUG("M3FTP m_ptr=%p\n",m_ptr);

	/*---------------- Allocate memory for filename  ---------------*/
	posix_memalign( (void **) &path_ptr, getpagesize(), MNX_PATH_MAX );
	if (path_ptr== NULL) {
   		ERROR_EXIT(-errno);
	}
	MUKDEBUG("M3FTP path_ptr=%p\n",path_ptr);

	/*---------------- Allocate memory for data  ---------------*/
	posix_memalign( (void **) &data_ptr, getpagesize(), MAXCOPYBUF );
	if (data_ptr== NULL) {
   		ERROR_EXIT(-errno);
	}
	MUKDEBUG("M3FTP data_ptr=%p\n",data_ptr);	

	if(rcode) ERROR_EXIT(rcode);
	
}

void usage(void) {
	fprintf(stderr, "Usage: m3ftp {-p|-g} <dcid> <rmt_fname> <lcl_fname> \n");
	ERROR_EXIT(EDVSINVAL);
}

/*===========================================================================*
 *				main					     *
 * usage:    m3ftp {-p|-g} <dcid>  <rmt_fname> <lcl_fname> * 
 * p: PUT
 * g: GET
  *===========================================================================*/
int main ( int argc, char *argv[] )
{
	int rcode, ret, rlen , oper, opt;
	char *dir_ptr;
	double t_start, t_stop, t_total;
	long total_bytes;
	
	MUKDEBUG("M3FTP argc=%d\n", argc);

	if( argc != 5) usage();
	
	dcid = atoi(argv[2]);
	MUKDEBUG("M3FTP dcid=%d\n", dcid);
	
	ftpd_ep = FTP_SVR_EP; 
	MUKDEBUG("M3FTP ftpd_ep=%d\n", ftpd_ep);

	ftp_init();
//	dir_ptr = dirname(argv[0]);
//	MUKDEBUG("M3FTP old cwd=%s\n", dir_ptr);
//	rcode =  chdir(dir_ptr);
//	if(rcode)ERROR_EXIT(-errno);
	dir_ptr = getcwd(path_ptr, MNX_PATH_MAX);
	MUKDEBUG("M3FTP new cwd=%s\n", dir_ptr);

	oper = FTP_NONE;
    while ((opt = getopt(argc, argv, "pg")) != -1) {
        switch (opt) {
			case 'p':
				oper = FTP_PUT;
				m_ptr->FTPOPER 	= FTP_PUT;
				m_ptr->FTPPATH 	= argv[3];
				m_ptr->FTPPLEN  = strlen(argv[3]);
				break;
			case 'g':
				oper = FTP_GET;
				m_ptr->FTPOPER = FTP_GET;
				m_ptr->FTPPATH = argv[3];
				m_ptr->FTPPLEN = strlen(argv[3]);
				break;
			default: /* '?' */
				usage();
				break;
        }
    }
	t_start = dwalltime();
	total_bytes = 0;
	MUKDEBUG("oper=%d\n", oper);
	// SEND PATHNAME 
	MUKDEBUG("M3FTP: %s\n", argv[3]);
	MUKDEBUG("M3FTP: request " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
	rcode = muk_sendrec(ftpd_ep, m_ptr);
	if( rcode < 0 ) ERROR_EXIT(rcode);
	MUKDEBUG("M3FTP: reply " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
	if (m_ptr->m_type < 0 ) {
		ERROR_EXIT(rcode); 
	}
	switch(oper){
		case FTP_PUT:
			MUKDEBUG("M3FTP FTP_PUT %s->%s\n", argv[4], argv[3]);
			fp = fopen(argv[4], "r");
			if(fp == NULL) ERROR_EXIT(-errno);
			m_ptr->FTPOPER = FTP_GET;
			do {
				rcode = fread(data_ptr, 1, m_ptr->FTPDLEN, fp);
				if(rcode < 0 ) {ERROR_PRINT(rcode); break;};
				m_ptr->FTPOPER = FTP_NEXT;
				m_ptr->FTPDATA = data_ptr;
				m_ptr->FTPDLEN = MAXCOPYBUF;
				MUKDEBUG("M3FTP: request " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
				rcode = muk_sendrec(ftpd_ep, m_ptr);
				if ( rcode < 0) {ERROR_PRINT(rcode); break;}
				MUKDEBUG("M3FTP: reply   " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
				if (m_ptr->m_type != OK) {
					rcode = m_ptr->m_type;
					ERROR_PRINT(rcode); 
					break;
				}
				if( m_ptr->FTPDLEN < 0 || m_ptr->FTPDLEN > MAXCOPYBUF)
					ERROR_EXIT(EDVSMSGSIZE);
				if( m_ptr->FTPDLEN == 0) break; // EOF 			
				total_bytes += m_ptr->FTPDLEN;
				putchar('>');
			}while(TRUE);
			break;
		case FTP_GET:
			MUKDEBUG("M3FTP FTP_GET %s->%s\n", argv[3], argv[4]);
			fp = fopen(argv[4], "w");
			if(fp == NULL) ERROR_EXIT(-errno);
			m_ptr->FTPOPER = FTP_GET;
			do {
				m_ptr->FTPOPER = FTP_NEXT;
				m_ptr->FTPDATA = data_ptr;
				m_ptr->FTPDLEN = MAXCOPYBUF;
				MUKDEBUG("M3FTP: request " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
				rcode = muk_sendrec(ftpd_ep, m_ptr);
				if ( rcode < 0) {ERROR_PRINT(rcode); break;}
				MUKDEBUG("M3FTP: reply   " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
				if (m_ptr->m_type != OK) {
					rcode = m_ptr->m_type;
					ERROR_PRINT(rcode); 
					break;
				}
				if( m_ptr->FTPDLEN < 0 || m_ptr->FTPDLEN > MAXCOPYBUF)
					ERROR_EXIT(EDVSMSGSIZE);
				if( m_ptr->FTPDLEN == 0) break; // EOF 			
				total_bytes += m_ptr->FTPDLEN;
				rcode = fwrite(data_ptr, 1, m_ptr->FTPDLEN, fp);
				if(rcode < 0 ) {ERROR_PRINT(rcode); break;};
				putchar('<');
			}while(TRUE);
			break;
		default:
			ERROR_EXIT(EDVSINVAL);
	}
	MUKDEBUG("M3FTP: CLOSE\n");
	fclose(fp);
	t_stop = dwalltime();
	/*--------- Report statistics  ---------*/
	t_total = (t_stop-t_start);
	printf("t_start=%.2f t_stop=%.2f t_total=%.2f\n",t_start, t_stop, t_total);
	printf("total_bytes = %ld\n", total_bytes);
	printf("Throuhput = %f [bytes/s]\n", (double)(total_bytes)/t_total);
		
	if ( rcode < 0) ERROR_PRINT(rcode);
}
	
