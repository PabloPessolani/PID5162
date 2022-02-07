#include "m3ftp.h"

int local_nodeid;
int ftp_lpid;
int ftp_ep, ftpd_ep;
dvs_usr_t dvs, *dvs_ptr;
dc_usr_t  dcu, *dc_ptr;
proc_usr_t m3ftpd, *m3ftpd_ptr;	
proc_usr_t m3ftp, *m3ftp_ptr;	
FILE *fp;
message *m_ptr;
char *path_ptr;
char *data_ptr;
int dcid;

char path[MNX_PATH_MAX];
message m;
char data[MAXCOPYLEN];

/*===========================================================================*
 *				ftp_request					     *
 *===========================================================================*/
int ftp_request(int oper)
{
	int ret, retry;

	USRDEBUG("ftp_request=%d\n", oper);

	m_ptr->m_type = oper;
	USRDEBUG("M3FTP: request " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
	for( retry = 0; retry < MAX_RETRIES; retry++) {
//		ret = dvk_sendrec(ftpd_ep, m_ptr);
		ret = dvk_sendrec_T(ftpd_ep, m_ptr, SEND_RECV_MS);
		if(ret == OK) break;
		if(ret == EDVSTIMEDOUT) {ERROR_PRINT(ret); continue;}
		if ( ret < 0) {ERROR_PRINT(ret); break;}		
	}
	if( ret < 0) ERROR_RETURN(ret);
	USRDEBUG("M3FTP: reply " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
	return(OK);
}

/*===========================================================================*
 *				ftp_init					     *
 *===========================================================================*/
void ftp_init(void)
{
  	int rcode;

	ftp_lpid = getpid();
	USRDEBUG("ftp_lpid=%d\n", ftp_lpid);

	rcode = dvk_open();
	if (rcode < 0)  ERROR_EXIT(rcode);
	
	ftp_ep = dvk_bind(dcid,ftp_ep);
	if( ftp_ep < EDVSERRCODE) ERROR_EXIT(ftp_ep);
	
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
	
	
	USRDEBUG("Get M3FTP endpoint=%d info\n", ftp_ep);
	m3ftp_ptr = &m3ftp;
	USRDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(m3ftp_ptr));
	rcode = dvk_getprocinfo(dcid, ftp_ep, m3ftp_ptr);
	if( rcode < 0) ERROR_EXIT(rcode);
	USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(m3ftp_ptr));
	USRDEBUG(PROC_WAIT_FORMAT, PROC_WAIT_FIELDS(m3ftp_ptr));
	USRDEBUG(PROC_COUNT_FORMAT, PROC_COUNT_FIELDS(m3ftp_ptr));

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
	USRDEBUG("M3FTP m_ptr=%p\n",m_ptr);

	/*---------------- Allocate memory for filename  ---------------*/
	posix_memalign( (void **) &path_ptr, getpagesize(), MNX_PATH_MAX );
	if (path_ptr== NULL) {
   		ERROR_EXIT(-errno);
	}
	USRDEBUG("M3FTP path_ptr=%p\n",path_ptr);

	/*---------------- Allocate memory for data  ---------------*/
	posix_memalign( (void **) &data_ptr, getpagesize(), MAXCOPYBUF );
	if (data_ptr== NULL) {
   		ERROR_EXIT(-errno);
	}
	USRDEBUG("M3FTP data_ptr=%p\n",data_ptr);	
#else //  ANULADO 
	m_ptr=&m;
	path_ptr = path;
	data_ptr = data;
#endif  // ANULADO 	

	if(rcode) ERROR_EXIT(rcode);
	
}

void usage(void) {
	fprintf(stderr, "Usage: m3ftp {-p|-g} <dcid> <clt_ep> <srv_ep> <rmt_fname> <lcl_fname> \n");
	ERROR_EXIT(EDVSINVAL);
}

/*===========================================================================*
 *				main					     *
 * usage:    m3ftp {-p|-g} <dcid> <clt_ep> <srv_ep> <rmt_fname> <lcl_fname> * 
 * p: PUT
 * g: GET
  *===========================================================================*/
int main ( int argc, char *argv[] )
{
	int rcode, ret, rlen , oper, opt, retry, pos;
	char *dir_ptr;
	
	USRDEBUG("M3FTP argc=%d\n", argc);

	if( argc != 7) usage();
		
	dcid = atoi(argv[2]);
	if ( dcid < 0 || dcid >= NR_DCS) {
		fprintf(stderr,  "Invalid dcid [0-%d]\n", NR_DCS-1 );
		exit(EXIT_FAILURE);
	}
	
	ftp_ep = atoi(argv[3]);
	USRDEBUG("M3FTP ftp_ep=%d\n", ftp_ep);

	ftpd_ep = atoi(argv[4]);
	USRDEBUG("M3FTP ftpd_ep=%d\n", ftpd_ep);

	ftp_init();
	dir_ptr = dirname(argv[0]);
	USRDEBUG("M3FTP old cwd=%s\n", dir_ptr);
	rcode =  chdir(dir_ptr);
	if(rcode)ERROR_EXIT(-errno);
	dir_ptr = getcwd(path_ptr, MNX_PATH_MAX);
	USRDEBUG("M3FTP new cwd=%s\n", dir_ptr);

	oper = FTP_NONE;
    while ((opt = getopt(argc, argv, "pg")) != -1) {
        switch (opt) {
			case 'p':
				oper = FTP_PUT;
				break;
			case 'g':
				oper = FTP_GET;
				break;
			default: /* '?' */
				usage();
				break;
        }
    }
	USRDEBUG("oper=%d\n", oper);
	pos = 0;
	m_ptr->FTPOPER = oper;
	m_ptr->FTPPATH = argv[5];
	m_ptr->FTPPLEN = strlen(argv[5]);
	m_ptr->FTPDATA = data_ptr;
	m_ptr->FTPDLEN = MAXBUFLEN;	
	m_ptr->FTPPOS  = pos;	
	
	rcode = ftp_request(oper);
	if( rcode < 0) ERROR_EXIT(rcode);
	
	switch(oper){
		case FTP_PUT:
			USRDEBUG("M3FTP FTP_PUT %s->%s\n", argv[5], argv[6]);
			fp = fopen(argv[6], "r");
			if(fp == NULL) ERROR_EXIT(-errno);
			do { 
				rlen = fread(data_ptr, 1, MAXBUFLEN, fp);
				m_ptr->FTPDATA = data_ptr;
				m_ptr->FTPDLEN = rlen;
				m_ptr->FTPPOS  = pos;	
				rcode = ftp_request(FTP_PNEXT);
				if(rcode < 0) break;
				if (m_ptr->m_type != OK) {
					rcode = m_ptr->m_type;
					ERROR_PRINT(rcode); 
					break;
				}
				pos += rlen;
				m_ptr->FTPOPER = FTP_PNEXT;
				m_ptr->FTPDLEN = MAXBUFLEN;	
			}while(rlen > 0);
			break;
		case FTP_GET:
			USRDEBUG("M3FTP FTP_GET %s->%s\n", argv[5], argv[6]);
			fp = fopen(argv[6], "w");
			if(fp == NULL){
				ERROR_PRINT(-errno);
				rcode = ftp_request(FTP_CANCEL);
				ERROR_EXIT(EDVSGENERIC);
			}
			do {
				m_ptr->FTPDATA = data_ptr;
				m_ptr->FTPDLEN = MAXBUFLEN;
				m_ptr->FTPPOS  = pos;	
				rcode = ftp_request(FTP_GNEXT);
				if(rcode < 0) break;
				if (m_ptr->m_type != OK) {
					rcode = m_ptr->m_type;
					ERROR_EXIT(rcode); 
				}
				if( m_ptr->FTPDLEN < 0 || m_ptr->FTPDLEN > MAXBUFLEN)
					ERROR_EXIT(EDVSMSGSIZE);
				pos += m_ptr->FTPDLEN;
				m_ptr->FTPOPER = FTP_GNEXT;
				rcode = fwrite(data_ptr, 1, m_ptr->FTPDLEN, fp);
				if(rcode < 0 ) {ERROR_EXIT(rcode);}
				if( m_ptr->FTPDLEN < MAXBUFLEN ) break; // EOF 				
			}while(TRUE);
			break;
		default:
			ERROR_EXIT(EDVSINVAL);
			break;
	}

	USRDEBUG("M3FTP: CLOSE\n");
	fclose(fp);
	if ( rcode < 0) ERROR_PRINT(rcode);
}
	
