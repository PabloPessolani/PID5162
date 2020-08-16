#include "m3ftp.h"

int local_nodeid;
int ftp_pid;
int dcid;
int ftp_ep, ftpd_ep;
dc_usr_t dc, *dc_usr_ptr;
proc_usr_t proc, *proc_usr_ptr;
dvs_usr_t dvs, *dvs_usr_ptr;
FILE *fp;
message *m_ptr;
char *path_ptr;
char *data_ptr;

#define WAIT4BIND_MS 	1000
#define SEND_RECV_MS 	10000

/*===========================================================================*
 *				ftp_reply					     *
 *===========================================================================*/
void ftp_reply(int rcode)
{
	int ret;
	
	m_ptr->m_type = rcode;
	ret = dvk_send_T(m_ptr->m_source, m_ptr, SEND_RECV_MS);
	if (rcode == EDVSTIMEDOUT) {
		USRDEBUG("M3FTP: ftp_reply rcode=%d\n", rcode);
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
	
	/*---------------- Get SERVER PROC info ---------------*/
	proc_usr_ptr = &proc;
	ret = dvk_getprocinfo(dcid, ftpd_ep, proc_usr_ptr);
	if(ret < 0) {
 	    fprintf(stderr,"SERVER dvk_getprocinfo error=%d \n", ret );
 	    exit(1);
	}
	USRDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_usr_ptr));
	if( proc_usr_ptr->p_rts_flags == SLOT_FREE) {
 	    fprintf(stderr,"SERVER not bound=%d \n", EDVSNOTBIND );
 	    exit(1);
	}

	/*----------------  CLIENT binding ---------------*/
	ftp_pid = getpid();
    ret =  dvk_bind(dcid, ftp_ep);
	if( ret < 0 ) {
		fprintf(stderr, "BIND ERROR ret=%d\n",ret);
	}
   	USRDEBUG("BIND CLIENT  dcid=%d svr_pid=%d ftp_ep=%d\n",
		dcid, ftp_pid, ftp_ep);
	
	/*---------------- Allocate memory for message  ---------------*/
	posix_memalign( (void **) &m_ptr, getpagesize(), sizeof(message) );
	if (m_ptr== NULL) {
   		ERROR_EXIT(-errno);
	}
	USRDEBUG("M3FTP m_ptr=%p\n",m_ptr);

	/*---------------- Allocate memory for filename  ---------------*/
	posix_memalign( (void **) &path_ptr, getpagesize(), PATH_MAX );
	if (path_ptr== NULL) {
   		ERROR_EXIT(-errno);
	}
	USRDEBUG("M3FTP path_ptr=%p\n",path_ptr);

	/*---------------- Allocate memory for data  ---------------*/
	posix_memalign( (void **) &data_ptr, getpagesize(), dvs_usr_ptr->d_max_copylen );
	if (data_ptr== NULL) {
   		ERROR_EXIT(-errno);
	}
	USRDEBUG("M3FTP data_ptr=%p\n",data_ptr);	

	
}

void usage(void) {
	fprintf(stderr, "Usage:  m3ftp {-p|-g} <dcid> <srv_ep> <clt_ep> <rmt_fname> <lcl_fname> \n");
	ERROR_EXIT(EDVSINVAL);
}

/*===========================================================================*
 *				main					     *
 * usage:    m3ftp {-p|-g} <dcid> <srv_ep> <clt_ep> <rmt_fname> <lcl_fname> * 
 * p: PUT
 * g: GET
  *===========================================================================*/
int main ( int argc, char *argv[] )
{
	int rcode, ret, rlen , oper, opt;
	char *dir_ptr;
	long total_bytes;

	USRDEBUG("M3FTP argc=%d\n", argc);

	if( argc != 7) usage();
	
	dcid = atoi(argv[2]);
	USRDEBUG("M3FTP dcid=%d\n", dcid);
	ftpd_ep = atoi(argv[3]);
	USRDEBUG("M3FTP ftpd_ep=%d\n", ftpd_ep);
	ftp_ep = atoi(argv[4]);
	USRDEBUG("M3FTP ftp_ep=%d\n", ftp_ep);

	ftp_init();
	dir_ptr = dirname(argv[0]);
	USRDEBUG("M3FTP old cwd=%s\n", dir_ptr);
	rcode =  chdir(dir_ptr);
	if(rcode)ERROR_EXIT(-errno);
	dir_ptr = getcwd(path_ptr, PATH_MAX);
	USRDEBUG("M3FTP new cwd=%s\n", dir_ptr);

	oper = FTP_NONE;
    while ((opt = getopt(2, argv, "pg")) != -1) {
        switch (opt) {
			case 'p':
				oper = FTP_PUT;
				m_ptr->FTPOPER 	= FTP_PUT;
				m_ptr->FTPPATH 	= argv[5];
				m_ptr->FTPPLEN  = strlen(argv[5]);
				break;
			case 'g':
				oper = FTP_GET;
				m_ptr->FTPOPER = FTP_GET;
				m_ptr->FTPPATH = argv[5];
				m_ptr->FTPPLEN = strlen(argv[5]);
				break;
			default: /* '?' */
				usage();
				break;
        }
    }

	USRDEBUG("oper=%d\n", oper);
	// SEND PATHNAME 
	USRDEBUG("M3FTP: %s\n", argv[5]);
	USRDEBUG("M3FTP: request " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
	rcode = dvk_sendrec(ftpd_ep, m_ptr);
	if( rcode < 0 ) ERROR_EXIT(rcode);
	USRDEBUG("M3FTP: reply " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
	if (m_ptr->m_type < 0 ) {
		ERROR_EXIT(rcode); 
	}
	
			
	switch(oper){
		case FTP_PUT:
			USRDEBUG("M3FTP FTP_PUT %s->%s\n", argv[5], argv[6]);
			fp = fopen(argv[6], "r");
			if(fp == NULL) ERROR_EXIT(-errno);
			do {
				rlen = fread(data_ptr, 1, dvs_usr_ptr->d_max_copylen, fp);
				if(rlen < 0 ) {
					m_ptr->FTPOPER = FTP_CANCEL;
					break;
				}else {
					m_ptr->FTPOPER = FTP_NEXT;
					m_ptr->FTPDATA = data_ptr;
					m_ptr->FTPDLEN = rlen; // # bytem ns read 
				}
				USRDEBUG("M3FTP: request " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
				rcode = dvk_sendrec(ftpd_ep, m_ptr);
				if ( rcode < 0) {ERROR_PRINT(rcode); break;}
				if ( rlen < 0)   {ERROR_PRINT(-errno); break;}
				USRDEBUG("M3FTP: reply   " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
				if (m_ptr->m_type != OK) {
					rcode = m_ptr->m_type;
					ERROR_PRINT(rcode); 
					break;
				}
				total_bytes += rlen;
				putchar('>');
			}while(rlen > 0);
			break;
		case FTP_GET:
			USRDEBUG("M3FTP FTP_GET %s->%s\n", argv[5], argv[6]);
			fp = fopen(argv[6], "w");
			if(fp == NULL) ERROR_EXIT(-errno);
			m_ptr->FTPOPER = FTP_GET;
			do {
				m_ptr->FTPOPER = FTP_NEXT;
				m_ptr->FTPDATA = data_ptr;
				m_ptr->FTPDLEN = dvs_usr_ptr->d_max_copylen;
				USRDEBUG("M3FTP: request " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
				rcode = dvk_sendrec(ftpd_ep, m_ptr);
				if ( rcode < 0) {ERROR_PRINT(rcode); break;}
				USRDEBUG("M3FTP: reply   " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
				if (m_ptr->m_type != OK) {
					rcode = m_ptr->m_type;
					ERROR_PRINT(rcode); 
					break;
				}
				if( m_ptr->FTPDLEN < 0 || m_ptr->FTPDLEN > dvs_usr_ptr->d_max_copylen)
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
	USRDEBUG("M3FTP: CLOSE\n");
	fclose(fp);
	if ( rcode < 0) ERROR_PRINT(rcode);
}
	
