#define DVK_GLOBAL_HERE
#include "m3ftp.h"

#undef DVK_GLOBAL_HERE
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

int ftpd_fd;
message ftp_msg, *ftp_mptr;
char *ftp_path_ptr;
char *ftp_data_ptr;
int clt_ep;
int oper;


#define SEND_RECV_MS 	10000


/*===========================================================================*
 *				ftpd_reply					     *
 *===========================================================================*/
void ftpd_reply(int rcode)
{
	int ret;

	MUKDEBUG("ftpd_reply=%d\n", rcode);

	ftp_mptr->m_type = rcode;
	ret = muk_send_T(clt_ep, ftp_mptr, SEND_RECV_MS);
	if (rcode == EDVSTIMEDOUT) {
		MUKDEBUG("M3FTPD: ftpd_reply rcode=%d\n", rcode);
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
  	int rcode;
	int  lcl_ep;
	

	MUKDEBUG("dcid=%d ftp_ep=%d\n",dcid, ftp_ep);
		
	MUKDEBUG(DVS_USR_FORMAT,DVS_USR_FIELDS(dvs_ptr));
	MUKDEBUG(DC_USR1_FORMAT,DC_USR1_FIELDS(dc_ptr));
	MUKDEBUG(DC_USR2_FORMAT,DC_USR2_FIELDS(dc_ptr));
	
	ftp_pid = syscall (SYS_gettid);
	MUKDEBUG("ftp_pid=%d\n", ftp_pid);
	
	rcode = muk_tbind(dcid, ftp_ep);
	MUKDEBUG("rcode=%d\n", rcode);
	if( rcode != ftp_ep) {
		ERROR_PRINT(EDVSENDPOINT);
		taskexit(&rcode);
	}

	rcode = mol_bindproc(ftp_ep, ftp_ep, ftp_pid, LCL_BIND);
	MUKDEBUG("rcode=%d\n", rcode);
	if( rcode < 0) {
		ERROR_PRINT(rcode);
		taskexit(&ftp_ep);
	}
		
	ftp_mptr = &ftp_msg;
	
	/*---------------- Allocate memory for filename  ---------------*/
	posix_memalign( (void **) &ftp_path_ptr, getpagesize(), MNX_PATH_MAX );
	if (ftp_path_ptr== NULL) {
   		ERROR_EXIT(-errno);
	}
	MUKDEBUG("M3FTPD ftp_path_ptr=%p\n",ftp_path_ptr);

	/*---------------- Allocate memory for data  ---------------*/
	posix_memalign( (void **) &ftp_data_ptr, getpagesize(), MAXCOPYBUF );
	if (ftp_data_ptr== NULL) {
   		ERROR_EXIT(-errno);
	}
	MUKDEBUG("M3FTPD ftp_data_ptr=%p\n",ftp_data_ptr);	

	ftp_pid = syscall (SYS_gettid);
	MUKDEBUG("ftp_pid=%d\n", ftp_pid);
	
	MUKDEBUG("Get ftp_ep info\n");
	ftp_ptr = (proc_usr_t *) get_task(ftp_ep);
	MUKDEBUG(PROC_MUK_FORMAT,PROC_MUK_FIELDS(ftp_ptr));
	
	// set the name of FTP 
	rcode = sys_rsetpname(ftp_ep, "m3ftp", local_nodeid);
	if(rcode < 0) ERROR_TSK_EXIT(rcode);
	
	
	if(rcode) ERROR_EXIT(rcode);
	
}

/*===========================================================================*
 *				main_ftpd					     *
 * REQUEST:
 *	FTPOPER = m_type => FTP_GET, FTP_PUT
 *      FTPPATH = m1p1 => pathname address
 *      FTPPLEN  = m1i1 => lenght of pathname 
 *      FTPDATA = m1p2 => DATA address
 *      FTPDLEN  = m1i2 => lenght of DATA
 *===========================================================================*/
int  main_ftpd ( int argc, char *argv[] )
{
	int rcode, ret, rlen ;
	double t_start, t_stop, t_total;
	long total_bytes;
	FILE *ftpd_fd;
	

	if( argc != 1) {
		ERROR_PRINT(EDVSENDPOINT);
		taskexit(&ftp_ep);
	}
	
	ftpd_init();

	// SYNCHRONIZE WITH MUK
	MTX_LOCK(muk_mutex);
	COND_SIGNAL(muk_cond);
	COND_WAIT(ftp_cond, muk_mutex);
	MTX_UNLOCK(muk_mutex);
		
	while(TRUE){
		// Wait for file request 
		do { 
			rcode = muk_receive_T(ANY,ftp_mptr, SEND_RECV_MS);
			MUKDEBUG("M3FTPD: muk_receive_T  rcode=%d\n", rcode);
			if (rcode == EDVSTIMEDOUT) {
				MUKDEBUG("M3FTPD: muk_receive_T TIMEOUT\n");
				continue ;
			}else if( rcode < 0) 
				ERROR_EXIT(EXIT_FAILURE);
		} while	(rcode < OK);
		MUKDEBUG("M3FTPD: " MSG1_FORMAT, MSG1_FIELDS(ftp_mptr));

		if (ftp_mptr->FTPOPER != FTP_GET && ftp_mptr->FTPOPER != FTP_PUT){
			fprintf(stderr,"M3FTPD: invalid request %d\n", ftp_mptr->FTPOPER);
			ftpd_reply(EDVSINVAL);
			continue;
		}

		if (ftp_mptr->FTPPLEN < 0 || ftp_mptr->FTPPLEN > (MNX_PATH_MAX-1)){
			fprintf(stderr,"M3FTPD: FTPPLEN=%d\n",  ftp_mptr->FTPPLEN);
			ftpd_reply(EDVSNAMESIZE);
			continue;			
		}
		
		if (ftp_mptr->FTPDLEN < 0 || ftp_mptr->FTPDLEN > (MAXCOPYBUF)){
			fprintf(stderr,"M3FTPD: FTDPLEN=%d\n",  ftp_mptr->FTPDLEN);
			ftpd_reply(EDVSMSGSIZE);
			continue;			
		}
		clt_ep = ftp_mptr->m_source;
		oper = ftp_mptr->FTPOPER;
		
		MUK_vcopy(rcode,clt_ep, ftp_mptr->FTPPATH,
							SELF, ftp_path_ptr, ftp_mptr->FTPPLEN);
		if( rcode < 0) break;
		ftp_path_ptr[ftp_mptr->FTPPLEN] = 0;
		MUKDEBUG("M3FTPD: path >%s<\n", ftp_path_ptr);
			
		ftp_mptr->m_type = OK;	
		
		rcode = muk_send_T(clt_ep, ftp_mptr, SEND_RECV_MS);
		if(rcode < 0) {
			ERROR_PRINT(rcode); 
			break;
		}
		
		switch(oper){
			case FTP_GET:
				MUKDEBUG("M3FTPD: FTP_GET filename:%s\n", ftp_path_ptr);
				if(( ftpd_fd = mol_open(ftp_path_ptr,O_RDONLY)) == -1) {  
					rcode = (-errno);
					ERROR_PRINT(rcode); 
					break;
				}
				MUKDEBUG("M3FTPD: READ LOOP\n");
				t_start = dwalltime();
				total_bytes = 0;
					
				do	{	
					rcode = muk_receive_T(clt_ep,ftp_mptr, SEND_RECV_MS);
					if(rcode < 0) {
						ERROR_PRINT(rcode); 
						break;
					}			
					oper = ftp_mptr->FTPOPER;		
					if ( (rlen = mol_read(ftpd_fd, ftp_data_ptr, ftp_mptr->FTPDLEN)) > 0) {
						MUKDEBUG("M3FTPD: FTPDLEN=%d rlen=%d\n", ftp_mptr->FTPDLEN, rlen);			
						MUK_vcopy(rcode, SELF, ftp_data_ptr
								,clt_ep, ftp_mptr->FTPDATA ,rlen);
						if(rcode < 0) {
							ERROR_PRINT(rcode); 
							break;
						}
						ftp_mptr->FTPDLEN = rlen;
						ftp_mptr->m_type = OK;					
						rcode = muk_send_T(clt_ep, ftp_mptr, SEND_RECV_MS);
						if(rcode < 0) {
							ERROR_PRINT(rcode); 
							break;
						}
						total_bytes += rlen;

						if( oper != FTP_NEXT) {
							if( oper != FTP_CANCEL) {
								rcode = EDVSINVAL;
								ERROR_PRINT(rcode); 
							}
							break;
						}
					}
				} while ( rlen > 0);
				
				MUKDEBUG("M3FTPD: CLOSE \n");
				ret = mol_close(ftpd_fd);		
				if(rcode < 0) {
					ERROR_PRINT(rcode); 
					break;
				}
				rcode = ret;
				ftp_mptr->FTPDLEN = 0;
				break;
			case FTP_PUT:
				MUKDEBUG("M3FTPD: FTP_PUT\n");
				if(( ftpd_fd = mol_open(ftp_path_ptr,O_RDWR | O_CREAT)) == -1) {  
					rcode = (-errno);
					ERROR_PRINT(rcode); 
					break;
				}
				MUKDEBUG("M3FTPD: WRITE LOOP\n");
				t_start = dwalltime();
				total_bytes = 0;
				
				do	{	
					rcode = muk_receive_T(clt_ep,ftp_mptr, SEND_RECV_MS);
					if(rcode < 0) {
						ERROR_PRINT(rcode); 
						break;
					}			
					oper = ftp_mptr->FTPOPER;	
					if( oper != FTP_NEXT) {
						if( oper != FTP_CANCEL) {
							rcode = EDVSINVAL;
							ERROR_PRINT(rcode); 
						}
						break;
					}
					if (ftp_mptr->FTPDATA == 0) // EOF 
						break; 
					MUK_vcopy(rcode, clt_ep, ftp_mptr->FTPDATA
									, SELF, ftp_data_ptr,rlen);
					if(rcode < 0) {
						ERROR_PRINT(rcode); 
						break;
					}
					if( ftp_mptr->FTPDLEN > 0 ) {
						rlen = mol_write(ftpd_fd, ftp_data_ptr, ftp_mptr->FTPDLEN);
						MUKDEBUG("M3FTPD: FTPDLEN=%d rlen=%d\n", ftp_mptr->FTPDLEN, rlen);			
						ftp_mptr->FTPDLEN = rlen;
						ftp_mptr->m_type = OK;					
						rcode = muk_send_T(clt_ep, ftp_mptr, SEND_RECV_MS);
						if(rcode < 0) {
							ERROR_PRINT(rcode); 
							break;
						}
						total_bytes += rlen;
					}
				} while ( rlen > 0);	
				MUKDEBUG("M3FTPD: CLOSE \n");
				ret = mol_close(ftpd_fd);		
				if(rcode < 0) {
					ERROR_PRINT(rcode); 
					break;
				}
				rcode = ret;
				break;
			default:
				fprintf(stderr,"M3FTPD: invalid request %d\n", ftp_mptr->FTPOPER);
				ERROR_EXIT(EDVSINVAL);
		}
		ftpd_reply(rcode);
		t_stop = dwalltime();
		/*--------- Report statistics  ---------*/
		t_total = (t_stop-t_start);
		ftpd_fd = fopen("m3ftpd.txt","a+");
		fprintf(ftpd_fd, "t_start=%.2f t_stop=%.2f t_total=%.2f\n",t_start, t_stop, t_total);
		fprintf(ftpd_fd, "total_bytes = %ld\n", total_bytes);
		fprintf(ftpd_fd, "Throuhput = %f [bytes/s]\n", (double)(total_bytes)/t_total);
		fclose(ftpd_fd);
	}	
 }


	
