#define RHS_GLOBAL_HERE
#include "rhs.h"
#include "rhostfs_glo.h"
#include <syscall.h> 
#include "/usr/src/linux/arch/sh/include/uapi/asm/unistd_32.h"
#include "rhostfs_proto.h"

int (*call_vec[NR_syscalls])(message *m_ptr);
#define map(call_nr, handler)  do { \
	RHSDEBUG("%s->%s\n", #call_nr, #handler);\
	call_vec[call_nr] = (handler); \
	}while(0)
	
int (*rh_call_vec[RH_MAX_CALL])(message *m_ptr);
#define rh_map(call_nr, handler) do { \
	RHSDEBUG("%s->%s\n", #call_nr, #handler);\
	rh_call_vec[call_nr-RH_SYS_CALL] = (handler);\
	}while(0)


/*===========================================================================*
 *				get_dvs_params				     *
 *===========================================================================*/
void get_dvs_params(void)
{
	RHSDEBUG("\n");
	local_nodeid = dvk_getdvsinfo(&dvs);
	RHSDEBUG("local_nodeid=%d\n",local_nodeid);
	if( local_nodeid < DVS_NO_INIT) ERROR_EXIT(local_nodeid);
	dvs_ptr = &dvs;
	RHSDEBUG(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));

}

/*===========================================================================*
 *				get_dc_params				     *
 *===========================================================================*/
void get_dc_params()
{
	int rcode;

	RHSDEBUG("dcid=%d\n", dcid);
	if ( dcid < 0 || dcid >= dvs.d_nr_dcs) {
 	        printf( "Invalid dcid [0-%d]\n", dvs.d_nr_dcs );
 	        ERROR_EXIT(EDVSBADDCID);
	}
	rcode = dvk_getdcinfo(dcid, &dcu);
	if(rcode < EDVSERRCODE) ERROR_EXIT(rcode);
	dc_ptr = &dcu;
	RHSDEBUG(DC_USR1_FORMAT, DC_USR1_FIELDS(dc_ptr));
	RHSDEBUG(DC_USR2_FORMAT, DC_USR2_FIELDS(dc_ptr));
}

/*===========================================================================*
 *				init_rhostfsd				     *
 *===========================================================================*/
void init_rhostfsd()
{ 
	int rcode;

	RHSDEBUG("rhs_ep=%d\n", rhs_ep);

	rcode = dvk_bind(dcid,rhs_ep);
	if(rcode < EDVSERRCODE) ERROR_EXIT(rcode);

	rhs_ptr = &rhs; 
	rcode = dvk_getprocinfo(dcid, rhs_ep, rhs_ptr);
	RHSDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(rhs_ptr));
}


print_usage(char *argv0){
	fprintf(stderr,"Usage: %s <config_file> \n", argv0 );
	ERROR_EXIT(EDVSINVAL);	
}

/*===========================================================================*
 *			          rhs_unused				     *
 *===========================================================================*/
int rhs_unused(message *m)
{
  printf("RHOSTFSD: got unused request %d from %d\n", m->m_type, m->m_source);
  return(EDVSBADREQUEST);			/* illegal message type */
}

/*===========================================================================*
 *				main					     					*
 *===========================================================================*/
int main ( int argc, char *argv[] )
{
	int rcode, i , call_nr, result, last_nr;
    config_t *cfg;
	message *m_ptr;

	if ( argc != 2) {
		print_usage(argv[0]);
 	    exit(1);
    }

	rcode = dvk_open();
	if (rcode < 0)  ERROR_EXIT(rcode);
	
	get_dvs_params();

	dcid 		= HARDWARE;
	rhs_ep		= HARDWARE;
	rhc_ep		= HARDWARE;

	cfg= nil;
	RHSDEBUG("cfg_file=%s\n", argv[1]);
	cfg = config_read(argv[1], CFG_ESCAPED, cfg);
	
	RHSDEBUG("before rhs_search_config\n");	
	rcode = rhs_search_config(cfg);
	if(rcode) ERROR_EXIT(rcode);

	if( dcid == HARDWARE)  ERROR_EXIT(EDVSBADDCID);
	
	get_dc_params();
	
	init_rhostfsd();
	
	RHSDEBUG( "Initialize the LINUX call vector to a safe default handler.\n");
  	for (i=0; i < NR_syscalls; i++) {
		putchar('.');
   		call_vec[i] = rhs_unused;
  	}
	putchar('\n');

	map(SYS_lstat, 	 lcl_lstat);
	map(SYS_lstat64, lcl_lstat);
	map(SYS_fstat, 	 lcl_fstat);
	map(SYS_access,  lcl_access);
	map(SYS_open,	 lcl_open);
	map(SYS_unlink,  lcl_unlink);
	map(SYS_close,   lcl_close);
	map(SYS_pwrite64, lcl_pwrite);
	map(SYS_pread64, lcl_pread);
	map(SYS_link, 	 lcl_link);
	map(SYS_symlink, lcl_symlink);
	map(SYS_mkdir, 	lcl_mkdir);
	map(SYS_rmdir, 	lcl_rmdir);
	map(SYS_fchown, lcl_fchown);
	map(SYS_chown, 	lcl_chown);
	map(SYS_fchmod, lcl_fchmod);
	map(SYS_chmod, 	lcl_chmod);
	map(SYS_ftruncate, lcl_ftruncate);
	map(SYS_truncate, lcl_truncate);
	map(SYS_renameat2, lcl_renameat2);
	map(SYS__llseek, lcl_llseek);
	map(SYS_fsync, lcl_fsync);
	map(SYS_dup2, lcl_dup2);
	map(SYS_fdatasync, lcl_fdatasync);
	map(SYS_readlink, lcl_readlink);
	map(SYS_rename, lcl_rename);
	map(SYS_mknod, lcl_mknod);
	map(SYS_futimesat, lcl_futimes);
	map(SYS_utimes, lcl_utimes);
	map(SYS_statfs, lcl_statfs);
	
	RHSDEBUG( "Initialize the RHOSTFS own call vector to a safe default handler.\n");
  	for (i=0; i < RH_MAX_CALL; i++) {
		putchar('#');
   		rh_call_vec[i] = rhs_unused;
  	}
	putchar('\n');

    rh_map(RMT_get_rootpath, lcl_get_rootpath); 	
    rh_map(RMT_readdir , lcl_readdir); 	
    rh_map(RMT_opendir , lcl_opendir); 	
    rh_map(RMT_seekdir , lcl_seekdir); 	
    rh_map(RMT_closedir , lcl_closedir); 	
	
	
	rcode = chroot(rhs_dir);
	if(rcode < 0 ) ERROR_EXIT(rcode);
	
	last_nr = SYS_restart_syscall;
	
	while(TRUE) {

		/*------------------------------------
		* Receive Request 
	 	*------------------------------------*/
		RHSDEBUG("RHOSTFSD is waiting for requests\n");
		rcode = dvk_receive(ANY,&rhs_m);
		RHSDEBUG("dvk_receive rcode=%d\n", rcode);
		if(rcode < 0 ) ERROR_EXIT(rcode);

		m_ptr = &rhs_m;
   		RHSDEBUG("RECEIVE msg:"MSG2_FORMAT,MSG2_FIELDS(m_ptr));
		
		/*------------------------------------
		* Process the Request 
	 	*------------------------------------*/
		call_nr = (unsigned) m_ptr->m_type;	
		rhs_who_e = m_ptr->m_source;
		rhs_who_p = _ENDPOINT_P(rhs_who_e);
		RHSDEBUG("call_nr=%d rhs_who_e=%d\n", call_nr, rhs_who_e);
		if (call_nr < 0) {	/* check call number 	*/
			ERROR_PRINT(EDVSBADREQUEST);
			result = EDVSBADREQUEST;		/* illegal message type */
		} else if(call_nr >= NR_syscalls){
			if( (call_nr < RH_SYS_CALL) 
				|| call_nr >= (RH_MAX_CALL + RH_SYS_CALL)){
				ERROR_PRINT(EDVSBADREQUEST);
				result = EDVSBADREQUEST;		/* illegal message type */					
			}
			call_nr -= RH_SYS_CALL;
			RHSDEBUG("RHOSTFS Calling vector %d\n",call_nr);
			result = (*rh_call_vec[call_nr])(&rhs_m);	/* handle the system call*/				
		}else {	
			if( call_nr == SYS_restart_syscall)
				call_nr = last_nr;	// restore interrupted system call 
			RHSDEBUG("LINUX Calling vector %d\n",call_nr);
			result = (*call_vec[call_nr])(&rhs_m);	/* handle the system call*/
			last_nr = call_nr; 		// save last system call 
		}
		
		/*------------------------------------
	 	* Send Reply 
 		*------------------------------------*/
		if (result != EDVSDONTREPLY) {
  	  		m_ptr->m_type = result;		/* report status of call */
			RHSDEBUG("REPLY msg:"MSG2_FORMAT,MSG2_FIELDS(m_ptr));
    		rcode = dvk_send_T(rhs_m.m_source, (long) &rhs_m, TIMEOUT_RMTCALL);
			if( rcode < 0) ERROR_PRINT(rcode);
		}
	}		

	if(rcode < 0) ERROR_EXIT(rcode);

	exit(0);
}

