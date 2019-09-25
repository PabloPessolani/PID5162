#define DVK_GLOBAL_HERE
#include "muk2.h"
#include "glo.h"

#define FORK_WAIT_MS 1000
#define	INIT_PID 		1

/*===========================================================================*
 *				dwalltime				     *
 *===========================================================================*/
double dwalltime()
{
	double sec;
	struct timeval tv;

	gettimeofday(&tv,NULL);
	sec = tv.tv_sec + tv.tv_usec/1000000.0;
	return sec;
}

/*===========================================================================*
 *				get_dvs_params				     *
 *===========================================================================*/
void get_dvs_params(void)
{
	MUKDEBUG("\n");
	local_nodeid = dvk_getdvsinfo(&dvs);
	MUKDEBUG("local_nodeid=%d\n",local_nodeid);
	if( local_nodeid < DVS_NO_INIT) ERROR_EXIT(local_nodeid);
	dvs_ptr = &dvs;
	MUKDEBUG(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));

}

/*===========================================================================*
 *				get_dc_params				     *
 *===========================================================================*/
void get_dc_params(int dcid)
{
	int rcode;

	MUKDEBUG("dcid=%d\n", dcid);
	if ( dcid < 0 || dcid >= dvs.d_nr_dcs) {
 	        printf( "Invalid dcid [0-%d]\n", dvs.d_nr_dcs );
 	        ERROR_EXIT(EDVSBADDCID);
	}
	rcode = dvk_getdcinfo(dcid, &dcu);
	if( rcode ) ERROR_EXIT(rcode);
	dc_ptr = &dcu;
	MUKDEBUG(DC_USR1_FORMAT, DC_USR1_FIELDS(dc_ptr));
	MUKDEBUG(DC_USR2_FORMAT, DC_USR2_FIELDS(dc_ptr));
}

/*===========================================================================*
 *				get_nodeinfo				     *
 *===========================================================================*/
void  get_nodeinfo(int nodeid)
{ 
	int rcode;
	node_ptr = &node;
	rcode = dvk_getnodeinfo(local_nodeid, &node);
	if(rcode) ERROR_EXIT(rcode);
	MUKDEBUG(NODE_USR_FORMAT, NODE_USR_FIELDS(node_ptr));
	return;
}

void print_usage(char *argv0){
	fprintf(stderr,"Usage: %s <config_file> \n", argv0 );
	ERROR_EXIT(EDVSINVAL);	
	return;
}

//-------------------------------------------------------------------------------------
//				tokenize
// INPUT:  string
// OUTPUT: 
// argc: # of arguments, 
// argv: vector of arguments
// args: sequence of tokens separated by zeros, pointed by argv[i]
// 		args space must be allocated by the caller 
//  Return value: len of argv 
//-------------------------------------------------------------------------------------
int tokenize(char *string, int *argc, char **argv, char *args)
{
	int was_space,arg_len, i;
	char *iptr, *optr, *tptr;
	
//	MUKDEBUG("string=%s\n", string);

	arg_len = 0;
	iptr = string;
	optr = args;
	tptr = args; 
	i = 0;
	
	// convert a string into a sequence of tokens (spaces replaced by zeros)
	for (was_space = TRUE; *iptr != 0; iptr++) {
		if (isspace(*iptr)) {
			if(was_space == FALSE){
				*optr = '\0';
				arg_len++;
				optr++;
				argv[i] = tptr;
//				MUKDEBUG("argv[%d]=%s\n", i, argv[i]);
				tptr = optr;
				i++;
			}
			was_space = TRUE;
		} else {
			*optr = *iptr;
			was_space = FALSE;
			optr++;
			arg_len++;
		}
	}
	*argc = i;

	// set a zero at the end of the sequence of strings 
	optr++;
	*optr = '\0';	
	arg_len++;
//	MUKDEBUG("argc=%d arg_len=%d\n", *argc, arg_len);
	return(arg_len);
}
	
#if	ENABLE_SYSTASK	
	
/*===========================================================================*
 *                          TASK SYSTASK					                                     *
 *===========================================================================*/
static void tsk_systask(void *str_arg)
{
	Task *my_tsk;
#define MAX_ARG_NR 20
	static char *argv[MAX_ARG_NR];
	static char args[ARG_MAX];
	int argc;
	int len;
	int i;
	int rcode;
	
	MUKDEBUG("str_arg=%s\n", str_arg);
	len = tokenize(str_arg, &argc, argv, args);
	MUKDEBUG("len=%d argc=%d\n", len, argc);
	for(i = 0; i < argc; i++){
		MUKDEBUG("argv[%d]=%s\n", i,  argv[i]);
	}
	
	my_tsk = current_task();
	MUKDEBUG("my_tsk->id=%u\n", my_tsk->id);
	
	// call SYSTASK !!
	rcode = main_systask(argc, argv);

	taskexit(rcode);
}
#endif // ENABLE_SYSTASK	

#if	ENABLE_PM	

/*===========================================================================*
 *                          TASK PM					                                     *
 *===========================================================================*/
static void tsk_pm(void *str_arg)
{
	Task *my_tsk;
#define MAX_ARG_NR 20
	static char *argv[MAX_ARG_NR];
	static char args[ARG_MAX];
	int argc;
	int len;
	int i;
	int rcode;

	MUKDEBUG("str_arg=%s\n", str_arg);
	len = tokenize(str_arg, &argc, argv, args);
	MUKDEBUG("len=%d argc=%d\n", len, argc);
	for(i = 0; i < argc; i++){
		MUKDEBUG("argv[%d]=%s\n", i,  argv[i]);
	}
	
	my_tsk = current_task();
	MUKDEBUG("my_tsk->id=%u\n", my_tsk->id);
	
	rcode = main_pm(argc, argv);

	taskexit(rcode);
}
#endif	// ENABLE_PM	

#if	ENABLE_RDISK
/*===========================================================================*
 *                          TASK RDISK					                                     *
 *===========================================================================*/
static void tsk_rd(void *str_arg)
{
	Task *my_tsk;
	static char *argv[MAX_ARG_NR];
	static char args[ARG_MAX];
	int argc;
	int len;
	int i;
	int rcode;

	MUKDEBUG("str_arg=%s\n", str_arg);
	len = tokenize(str_arg, &argc, argv, args);
	MUKDEBUG("len=%d argc=%d\n", len, argc);
	for(i = 0; i < argc; i++){
		MUKDEBUG("argv[%d]=%s\n", i,  argv[i]);
	}
	
	my_tsk = current_task();
	MUKDEBUG("my_tsk->id=%u\n", my_tsk->id);
	
	rcode = main_rdisk(argc, argv);

	taskexit(rcode);
}
#endif	// ENABLE_RDISK

#if	ENABLE_FS
/*===========================================================================*
 *                          TASK FS					                                     *
 *===========================================================================*/
static void tsk_fs(void *str_arg)
{
	Task *my_tsk;
	static char *argv[MAX_ARG_NR];
	static char args[ARG_MAX];
	int argc;
	int len;
	int i;
	int rcode;
	proc_usr_t *tmp_ptr;

	MUKDEBUG("str_arg=%s\n", str_arg);
	len = tokenize(str_arg, &argc, argv, args);
	MUKDEBUG("len=%d argc=%d\n", len, argc);
	for(i = 0; i < argc; i++){
		MUKDEBUG("argv[%d]=%s\n", i,  argv[i]);
	}
	
	my_tsk = current_task();
	MUKDEBUG("my_tsk->id=%u\n", my_tsk->id);

	rcode = main_fs(argc, argv);

	taskexit(rcode);
}
#endif // ENABLE_FS

#if	ENABLE_IS
/*===========================================================================*
 *                          TASK IS					                                     *
 *===========================================================================*/
static void tsk_is(void *str_arg)
{
	Task *my_tsk;
	static char *argv[MAX_ARG_NR];
	static char args[ARG_MAX];
	int argc;
	int len;
	int i;
	int rcode;

	MUKDEBUG("str_arg=%s\n", str_arg);
	len = tokenize(str_arg, &argc, argv, args);
	MUKDEBUG("len=%d argc=%d\n", len, argc);
	for(i = 0; i < argc; i++){
		MUKDEBUG("argv[%d]=%s\n", i,  argv[i]);
	}
	
	my_tsk = current_task();
	MUKDEBUG("my_tsk->id=%u\n", my_tsk->id);
	
	rcode = main_is(argc, argv);

	taskexit(rcode);
}
#endif	//ENABLE_IS

#if	ENABLE_NW
/*===========================================================================*
 *                          TASK NW					                                     *
 *===========================================================================*/
static void tsk_web(void *str_arg)
{
	Task *my_tsk;
	static char *argv[MAX_ARG_NR];
	static char args[ARG_MAX];
	int argc;
	int len;
	int i;
	int rcode;

#define		NW_PROC_NR		INIT_PROC_NR

	MUKDEBUG("str_arg=%s\n", str_arg);
	len = tokenize(str_arg, &argc, argv, args);
	MUKDEBUG("len=%d argc=%d\n", len, argc);
	for(i = 0; i < argc; i++){
		MUKDEBUG("argv[%d]=%s\n", i,  argv[i]);
	}
	
	my_tsk = current_task();
	MUKDEBUG("my_tsk->id=%u\n", my_tsk->id);
			
	rcode = main_nweb(argc, argv);

	taskexit(rcode);
}
#endif //ENABLE_NW

#if	ENABLE_FTP
/*===========================================================================*
 *                          TASK FTP					                                     *
 *===========================================================================*/
static void tsk_ftp(void *str_arg)
{
	Task *my_tsk;
	static char *argv[MAX_ARG_NR];
	static char args[ARG_MAX];
	int argc;
	int len;
	int i;
	int rcode;

	MUKDEBUG("str_arg=%s\n", str_arg);
	len = tokenize(str_arg, &argc, argv, args);
	MUKDEBUG("len=%d argc=%d\n", len, argc);
	for(i = 0; i < argc; i++){
		MUKDEBUG("argv[%d]=%s\n", i,  argv[i]);
	}
	
	my_tsk = current_task();
	MUKDEBUG("my_tsk->id=%u\n", my_tsk->id);
	rcode = main_ftpd(argc, argv);

	taskexit(rcode);
}
#endif // ENABLE_FTP


/*===========================================================================*
 *                          TASK MAIN 					                                     *
 *===========================================================================*/
void taskmain(int argc, char **argv)
{
	int rcode, to_id, i;
	Task *t;
	static char muk_cmd[_POSIX_ARG_MAX];
 	proc_usr_t *mukproc_ptr;		/*  process pointer */
    config_t *cfg;
	double t_start, t_stop, t_total; 

	MUKDEBUG("\n");

	t_start =  dwalltime();
	if ( argc != 2) {
		print_usage(argv[0]);
 	    exit(1);
    }

	dump_fd = stdout;
	
	get_dvs_params();
	
#define MAIN_NR 	(-3)
	main_ep 	= MAIN_NR; 
	dcid 		= HARDWARE;
	pm_ep		= HARDWARE;
	fs_ep		= HARDWARE;
	is_ep		= HARDWARE;
	rd_ep		= HARDWARE;
	web_ep		= HARDWARE;
	ftp_ep		= HARDWARE;
//	ds_ep		= HARDWARE;
//	tty_ep		= HARDWARE;
//	eth_ep		= HARDWARE;
//	tap_ep		= HARDWARE;
//	inet_ep		= HARDWARE;

	cfg= nil;
	MUKDEBUG("cfg_file=%s\n", argv[1]);
	cfg = config_read(argv[1], CFG_ESCAPED, cfg);
	
	MUKDEBUG("before muk_search_config\n");	
	rcode = muk_search_config(cfg);
	if(rcode) ERROR_TSK_EXIT(rcode);

	if( dcid == HARDWARE)  ERROR_EXIT(EDVSBADDCID);
	get_dc_params(dcid);
	
	muk_mutex = 1; // initialize pseudo-mutex 

	to_id = taskcreate(muk_timeout_task, 0, 32768);

#if 	ENABLE_SYSTASK
	/*Create SYSTASK Task  */
	MUKDEBUG("Starting SYSTASK task -----------------------------------\n");
	sprintf(muk_cmd,"systask\n");
	sys_id = taskcreate(tsk_systask, muk_cmd, 32768);
	MUKDEBUG("sys_id=%d\n",sys_id);
	MTX_LOCK(muk_mutex);
	COND_WAIT(muk_cond, muk_mutex);
	COND_SIGNAL(sys_cond);
	MTX_UNLOCK(muk_mutex);	
#endif // ENABLE_SYSTASK
	
#if 	ENABLE_RDISK
	/*Create RDISK Task  */
	if( rd_ep != HARDWARE) {
		MUKDEBUG("Starting RDISK Task--------------------------------------\n");
		sprintf(muk_cmd,"rdisk -c %s\n", rd_cfg);
		rd_id = taskcreate(tsk_rd, muk_cmd, 32768);
		MUKDEBUG("rd_id=%d\n",rd_id);
		MTX_LOCK(muk_mutex);
		COND_WAIT(muk_cond, muk_mutex);
		COND_SIGNAL(rd_cond);
		MTX_UNLOCK(muk_mutex);
	}
#endif // ENABLE_RDISK 

#if 	ENABLE_PM
	/*Create PM Task  */
	if( pm_ep != HARDWARE) {
		MUKDEBUG("Starting PM Task ---------------------------------------\n");
		sprintf(muk_cmd,"pm\n");
		pm_id = taskcreate(tsk_pm, muk_cmd, 32768);
		MUKDEBUG("pm_id=%d\n",pm_id);
		MTX_LOCK(muk_mutex);
		COND_WAIT(muk_cond, muk_mutex);
		mukproc_ptr = (proc_usr_t *) get_task(pm_ep);
		SET_BIT( mukproc_ptr->p_misc_flags, MIS_BIT_UNIKERNEL);
		COND_SIGNAL(pm_cond);
		MTX_UNLOCK(muk_mutex);
	}
#endif // ENABLE_PM

#if 	ENABLE_FS
	/*Create FS Task  */
	if( fs_ep != HARDWARE) {
		MUKDEBUG("Starting FS Task--------------------------------------\n");
		sprintf(muk_cmd,"fs %s\n", fs_cfg);
		fs_id = taskcreate(tsk_fs, muk_cmd, 32768);
		MUKDEBUG("fs_id=%d\n",fs_id);
		MTX_LOCK(muk_mutex);
		COND_WAIT(muk_cond, muk_mutex);
		mukproc_ptr = (proc_usr_t *) get_task(fs_ep);
		SET_BIT( mukproc_ptr->p_misc_flags, MIS_BIT_UNIKERNEL);
		COND_SIGNAL(fs_cond);
		MTX_UNLOCK(muk_mutex);
	}
#endif // ENABLE_FS

#if 	ENABLE_IS
	/*Create IS Task  */
	if( is_ep != HARDWARE) {
		MUKDEBUG("Starting IS Task--------------------------------------\n");
		sprintf(muk_cmd,"is\n");
		is_id = taskcreate(tsk_is, muk_cmd, 32768);
		MUKDEBUG("is_id=%d\n",is_id);
		MTX_LOCK(muk_mutex);
		COND_WAIT(muk_cond, muk_mutex);
		COND_SIGNAL(is_cond);
		MTX_UNLOCK(muk_mutex);
	} 
#endif // ENABLE_IS 
		
#if ENABLE_NW
	/*Create NW Task  */
	if( web_ep != HARDWARE) {
		MUKDEBUG("Starting NW Task--------------------------------------\n");
	//	pthread_attr_init(&attrs);
	//	pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE);
		sprintf(muk_cmd,"nw %s\n", web_cfg);
		web_id = taskcreate(tsk_web, muk_cmd, 32768);
		MUKDEBUG("web_id=%d\n",web_id);
		MTX_LOCK(muk_mutex);
		COND_WAIT(muk_cond, muk_mutex);
		COND_SIGNAL(nw_cond);
		MTX_UNLOCK(muk_mutex);
	}
#endif // ENABLE_NW

#if ENABLE_FTP
	if( ftp_ep != HARDWARE) {
		/*Create FTP Task  */
		MUKDEBUG("Starting FTP Task--------------------------------------\n");
		sprintf(muk_cmd,"m3ftp\n");
		ftp_id = taskcreate(tsk_ftp, muk_cmd, 32768);
		MUKDEBUG("ftp_id=%d\n",ftp_id);
		MTX_LOCK(muk_mutex);
		COND_WAIT(muk_cond, muk_mutex);
		COND_SIGNAL(ftp_cond);
		MTX_UNLOCK(muk_mutex);
	}
#endif // ENABLE_FTP

	t_stop =  dwalltime();
	t_total = (t_stop-t_start);
 	printf("BOOT TIME: t_start=%.2f t_stop=%.2f t_total=%.2f [s]\n",t_start, t_stop, t_total);
	MUKDEBUG("JOINING ALL TASKS -------------------------------------\n");
	
	for( i = -dc_ptr->dc_nr_tasks; i < (dc_ptr->dc_nr_procs); i++) {
		t = get_task(i);
		if( t == NULL) continue;
		LIBDEBUG(PROC_MUK_FORMAT,PROC_MUK_FIELDS(t));
	} 
	
    fflush(stdout);
}

