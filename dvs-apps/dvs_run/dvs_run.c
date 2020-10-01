
#include "dvs_run.h"

#ifdef USRDBG
#pragma message ("USRDBG=YES")
#else //  USRDBG
#pragma message ("USRDBG=NO")
#endif  //  USRDBG

extern char *optarg;
extern int optind, opterr, optopt;

dc_usr_t  dcu, *dc_ptr;
dvs_usr_t dvs, *dvs_ptr;
int local_nodeid;
proc_usr_t proc_usr, *proc_ptr;
int dcid, proc_ep, proc_nr, bind_type;
int proc_pid, rmt_nodeid;

#define SELFTASK (-1) 
#define INIT_PID  1 

/*===========================================================================*
 *				get_dvs_params				     *
 *===========================================================================*/
void get_dvs_params(void)
{
	USRDEBUG("\n");
	local_nodeid = dvk_getdvsinfo(&dvs);
	USRDEBUG("local_nodeid=%d\n",local_nodeid);
	if( local_nodeid < DVS_NO_INIT) ERROR_EXIT(local_nodeid);
	dvs_ptr = &dvs;
	USRDEBUG(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));
}

/*===========================================================================*
 *				get_dc_params				     *
 *===========================================================================*/
void get_dc_params(int dcid)
{
	int rcode;

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
}

void print_usage(char *argv0){
fprintf(stderr,"Usage: %s -<l|r|b>  <dcid> <endpoint> [<rmt_nodeid>] <command> <args...> \n", argv0 );
	fprintf(stderr,"\t\t l: local bind \n");
	fprintf(stderr,"\t\t r: replica bind \n");
	fprintf(stderr,"\t\t b: backup bind \n");
	fprintf(stderr,"<dcid>: DC ID for the process\n");
	fprintf(stderr,"<endpoint>: Endpoint to allocate for the process\n");
	fprintf(stderr,"<rmt_nodeid>: Remote node ID of the Primary for a Backup process\n");
	ERROR_EXIT(EDVSINVAL);	
}

/*===========================================================================*
 *				main					     					*
 *===========================================================================*/
int main ( int argc, char *argv[] )
{
	int rcode, opt, i, first_arg, arg_c;
	char **arg_v;
	pid_t child_pid;
	
	if ( argc < 5 ) {
		USRDEBUG("argc=%d\n", argc);
		for( i = 0; i < argc ; i++) {
			USRDEBUG("argv[%d]=%s\n", i, argv[i]);
		}
		print_usage(argv[0]);
 	    exit(1);
    }

	rcode = dvk_open();
	if (rcode < 0)  ERROR_EXIT(rcode);
	
	bind_type = SELF_BIND;
    while ((opt = getopt(argc, argv, "lrb")) != -1) {
        switch (opt) {
			case 'l':
				if( bind_type != SELF_BIND)
					print_usage(argv[0]);
				bind_type = LCL_BIND;
				break;
			case 'r':
				if( bind_type != SELF_BIND)
					print_usage(argv[0]);
				bind_type = REPLICA_BIND;
				break;
			case 'b':
				if( bind_type != SELF_BIND)
					print_usage(argv[0]);
				rmt_nodeid = atoi(argv[4]);
				USRDEBUG("rmt_nodeid=%d\n", rmt_nodeid)
				bind_type = BKUP_BIND;
				break;
			default: /* '?' */
				print_usage(argv[0]);
				break;
        }
    }
	
	USRDEBUG("getopt bind_type=%d\n", bind_type);
	if( bind_type == SELF_BIND ) {
		print_usage(argv[0]);
	}
		
	get_dvs_params();

	dcid = atoi(argv[2]);
	USRDEBUG("dcid=%d\n", dcid);
	get_dc_params(dcid);
			
	proc_ep = atoi(argv[3]);
	USRDEBUG("proc_ep=%d\n", proc_ep);
	proc_nr = _ENDPOINT_P(proc_ep);
	if ( (proc_nr < (-dc_ptr->dc_nr_tasks)) 
	|| (proc_nr >= (dc_ptr->dc_nr_procs-dc_ptr->dc_nr_tasks))) {
 	    fprintf(stderr, "Invalid <endpoint> [%d-%d]\n", 
			(-dc_ptr->dc_nr_tasks), 
			(dc_ptr->dc_nr_procs-dc_ptr->dc_nr_tasks));
		ERROR_EXIT(EDVSBADPROC);
	}

	if( !TEST_BIT(dc_ptr->dc_nodes, rmt_nodeid)) {
 	    fprintf(stderr, "DC %d cannot run on node %d\n", dcid, rmt_nodeid);
		ERROR_EXIT(EDVSDCNODE);
	}
	
	USRDEBUG("argc=%d \n",argc);
	
	if ((child_pid = fork()) == 0) {     /* CHILD  */	
		detach_proc();
		proc_pid = getpid();
		USRDEBUG("proc_pid:%d bind_type=%d\n", proc_pid, bind_type);
		// dvs_run -<l|r|b>  <dcid> <endpoint> [<rmt_nodeid>] <command> <args...> 
		switch(bind_type){
			case LCL_BIND:
				rcode = dvk_lclbind(dc_ptr->dc_dcid,proc_pid,proc_ep);
				first_arg = 4;
				break;
			case REPLICA_BIND:
				rcode = dvk_replbind(dc_ptr->dc_dcid,proc_pid,proc_ep); 
				first_arg = 4;
				break;
			case BKUP_BIND:
				rcode = dvk_bkupbind(dc_ptr->dc_dcid,proc_pid,proc_ep,rmt_nodeid);
				first_arg = 5;
				break;
			default:
				USRDEBUG("NEVER HERE!!\n");
				rcode = EDVSINVAL;
				break;
		}
		if(rcode < 0) ERROR_RETURN(rcode);
		arg_c	= argc - first_arg;
		arg_v	= &argv[first_arg];	
		
		USRDEBUG("arg_c=%d \n",arg_c);
		for( i = 0; i < arg_c ; i++) {
			USRDEBUG("argv[%d]=%s\n", i, arg_v[i]);
		}
		rcode = execvpe(argv[first_arg], arg_v, NULL);
		
		/* this code only executes if execvp() failure */
		USRDEBUG("CHILD: execvpe rcode=%d errno=%d\n", rcode, errno);
		USRDEBUG("CHILD: execvpe %s\n", *strerror(errno));
		ERROR_RETURN(errno);
	}

	USRDEBUG("PARENT waiting child exiting\n");
	wait(&child_pid);
	USRDEBUG("exiting child=%d\n", child_pid);
	exit(0);
}

	
void detach_proc(void)
{
	int i,lfp;
	char str[10];
	
	USRDEBUG("\n");

	if(getppid()== INIT_PID) return; /* already a daemon */
	i=fork();
	if (i<0) exit(1); /* fork error */
	if (i>0) exit(0); /* parent exits */
	
	USRDEBUG("pid=%d\n", getpid());

	/* child (daemon) continues */
	setsid(); /* obtain a new process group */
//	for (i=getdtablesize();i>=0;--i) close(i); /* close all descriptors */
//	i=open("/dev/null",O_RDWR); dup(i); dup(i); /* handle standart I/O */
	umask(027); /* set newly created file permissions */
//	chdir(RUNNING_DIR); /* change running directory */
//	lfp=open(LOCK_FILE,O_RDWR|O_CREAT,0640);
//	if (lfp<0) exit(1); /* can not open */
//	if (lockf(lfp,F_TLOCK,0)<0) exit(0); /* can not lock */
	/* first instance continues */
//	sprintf(str,"%d\n",getpid());
//	write(lfp,str,strlen(str)); /* record pid to lockfile */
	signal(SIGCHLD,SIG_IGN); /* ignore child */
	signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
	signal(SIGHUP,SIG_IGN); /* catch hangup signal */
	signal(SIGTERM,SIG_IGN); /* catch kill signal */
}	
