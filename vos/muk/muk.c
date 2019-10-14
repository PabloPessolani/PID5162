#define DVK_GLOBAL_HERE
#include "muk.h"
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
void get_nodeinfo(int nodeid)
{ 
	int rcode;
	node_ptr = &node;
	rcode = dvk_getnodeinfo(local_nodeid, &node);
	if(rcode) ERROR_EXIT(rcode);
	MUKDEBUG(NODE_USR_FORMAT, NODE_USR_FIELDS(node_ptr));
}

print_usage(char *argv0){
	fprintf(stderr,"Usage: %s <config_file> \n", argv0 );
	ERROR_EXIT(EDVSINVAL);	
}

//-------------------------------------------------------------------------------------
//				tokeninze
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
	
/*===========================================================================*
 *                          THREAD SYSTASK					                                     *
 *===========================================================================*/
static void pth_systask(void *str_arg)
{
	pthread_t my_pth;
#define MAX_ARG_NR 20
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
	
	my_pth = pthread_self();
	MUKDEBUG("my_pth=%u\n", my_pth);
	
	// call SYSTASK !!
	rcode = main_systask(argc, argv);

	pthread_exit(&rcode);
}

/*===========================================================================*
 *                          THREAD PM					                                     *
 *===========================================================================*/
static void pth_pm(void *str_arg)
{
	pthread_t my_pth;
#define MAX_ARG_NR 20
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
	
	my_pth = pthread_self();
	MUKDEBUG("my_pth=%u\n", my_pth);

	rcode = main_pm(argc, argv);

	pthread_exit(&rcode);
}

/*===========================================================================*
 *                          THREAD RDISK					                                     *
 *===========================================================================*/
static void pth_rd(void *str_arg)
{
	pthread_t my_pth;
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
	
	my_pth = pthread_self();
	MUKDEBUG("my_pth=%u\n", my_pth);
	
	rcode = main_rdisk(argc, argv);

	pthread_exit(&rcode);
}

/*===========================================================================*
 *                          THREAD FS					                                     *
 *===========================================================================*/
static void pth_fs(void *str_arg)
{
	pthread_t my_pth;
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
	
	my_pth = pthread_self();
	MUKDEBUG("my_pth=%u\n", my_pth);

	rcode = main_fs(argc, argv);

	pthread_exit(&rcode);
}

/*===========================================================================*
 *                          THREAD IS					                                     *
 *===========================================================================*/
static void pth_is(void *str_arg)
{
	pthread_t my_pth;
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
	
	my_pth = pthread_self();
	MUKDEBUG("my_pth=%u\n", my_pth);
	
	rcode = main_is(argc, argv);

	pthread_exit(&rcode);
}


/*===========================================================================*
 *                          THREAD NW					                                     *
 *===========================================================================*/
static void pth_web(void *str_arg)
{
	pthread_t my_pth;
	static char *argv[MAX_ARG_NR];
	static char args[ARG_MAX];
	int argc;
	int len;
	int i;
	int rcode;
	proc_usr_t *tmp_ptr;

#define		NW_PROC_NR		INIT_PROC_NR

	MUKDEBUG("str_arg=%s\n", str_arg);
	len = tokenize(str_arg, &argc, argv, args);
	MUKDEBUG("len=%d argc=%d\n", len, argc);
	for(i = 0; i < argc; i++){
		MUKDEBUG("argv[%d]=%s\n", i,  argv[i]);
	}
	
	my_pth = pthread_self();
	MUKDEBUG("my_pth=%u\n", my_pth);
			
	rcode = main_nweb(argc, argv);

	pthread_exit(&rcode);
}

/*===========================================================================*
 *                          THREAD FTP					                                     *
 *===========================================================================*/
static void pth_ftp(void *str_arg)
{
	pthread_t my_pth;
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
	
	my_pth = pthread_self();
	MUKDEBUG("my_pth=%u\n", my_pth);
	

	rcode = main_ftpd(argc, argv);

	pthread_exit(&rcode);
}

/*===========================================================================*
 *				main					     					*
 *===========================================================================*/
int main ( int argc, char *argv[] )
{
	int rcode;
	static char muk_cmd[_POSIX_ARG_MAX];
 	proc_usr_t *mukproc_ptr;		/*  process pointer */
    config_t *cfg;
	int ret_st;
	int ret_pm;
	int ret_rd;
	int ret_fs;
	int ret_is;
	int ret_web;
	int ret_ftp;
	double t_start, t_stop, t_total; 
	
	pthread_attr_t attrs;
	
	t_start =  dwalltime();
	
	if ( argc != 2) {
		print_usage(argv[0]);
 	    exit(1);
    }

	dump_fd = stdout;
	
	rcode = dvk_open();
	if (rcode < 0)  ERROR_EXIT(rcode);
	
	get_dvs_params();

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

	// initialize synchronization semaphores ALL IN ZERO 
	MTX_LOCK(muk_mutex);
	MTX_LOCK(sys_mutex);
	MTX_LOCK(pm_mutex);
	MTX_LOCK(rd_mutex);
	MTX_LOCK(fs_mutex);
	MTX_LOCK(is_mutex);
	MTX_LOCK(nw_mutex);
	MTX_LOCK(ftp_mutex);  
    
	cfg= nil;
	MUKDEBUG("cfg_file=%s\n", argv[1]);
	cfg = config_read(argv[1], CFG_ESCAPED, cfg);
	
	MUKDEBUG("before muk_search_config\n");	
	rcode = muk_search_config(cfg);
	if(rcode) ERROR_PT_EXIT(rcode);

	if( dcid == HARDWARE)  ERROR_EXIT(EDVSBADDCID);
	
#if 	ENABLE_SYSTASK
	/*Create SYSTASK Thread  */
	MUKDEBUG("Starting SYSTASK thread -----------------------------------\n");
//	pthread_attr_init(&attrs);
//	pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE);
	sprintf(muk_cmd,"systask\n");
	rcode = pthread_create( &sys_pth, NULL, pth_systask, muk_cmd );
	if(rcode)ERROR_EXIT(rcode);
	MUKDEBUG("sys_pth=%u\n",sys_pth);
	mukproc_ptr = (proc_usr_t *) PROC_MAPPED(sys_ep);
	SET_BIT( mukproc_ptr->p_misc_flags, MIS_BIT_UNIKERNEL);
	MTX_UNLOCK(sys_mutex);
	MTX_LOCK(muk_mutex);
	
#endif // ENABLE_SYSTASK
	
#if 	ENABLE_RDISK
	/*Create RDISK Thread  */
	if( rd_ep != HARDWARE) {
		MUKDEBUG("Starting RDISK thread--------------------------------------\n");
	//	pthread_attr_init(&attrs);
	//	pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE);
		sprintf(muk_cmd,"rdisk -c %s\n", rd_cfg);
		rcode = pthread_create( &rd_pth, NULL, pth_rd, muk_cmd);
		if(rcode) ERROR_EXIT(rcode);
		MUKDEBUG("rd_pth=%u\n",rd_pth);
		mukproc_ptr = (proc_usr_t *) PROC_MAPPED(rd_ep);
		SET_BIT( mukproc_ptr->p_misc_flags, MIS_BIT_UNIKERNEL);
		MTX_UNLOCK(rd_mutex);
		MTX_LOCK(muk_mutex);
	}
#endif // ENABLE_RDISK 

#if 	ENABLE_PM
	/*Create PM Thread  */
	if( pm_ep != HARDWARE) {
		MUKDEBUG("Starting PM thread ---------------------------------------\n");
	//	pthread_attr_init(&attrs);
	//	pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE);
		sprintf(muk_cmd,"pm\n");
		rcode = pthread_create( &pm_pth, NULL, pth_pm, muk_cmd );
		if(rcode)ERROR_EXIT(rcode);
		MUKDEBUG("pm_pth=%u\n",pm_pth);
		mukproc_ptr = (proc_usr_t *) PROC_MAPPED(pm_ep);
		SET_BIT( mukproc_ptr->p_misc_flags, MIS_BIT_UNIKERNEL);
		MTX_UNLOCK(pm_mutex);
		MTX_LOCK(muk_mutex);
	}
#endif // ENABLE_PM

#if 	ENABLE_FS
	/*Create FS Thread  */
	if( fs_ep != HARDWARE) {
		MUKDEBUG("Starting FS thread--------------------------------------\n");
	//	pthread_attr_init(&attrs);
	//	pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE);
		sprintf(muk_cmd,"fs %s\n", fs_cfg);
		rcode = pthread_create( &fs_pth, NULL, pth_fs, muk_cmd);
		if(rcode) ERROR_EXIT(rcode);
		MUKDEBUG("fs_pth=%u\n",fs_pth);
		mukproc_ptr = (proc_usr_t *) PROC_MAPPED(fs_ep);
		SET_BIT( mukproc_ptr->p_misc_flags, MIS_BIT_UNIKERNEL);	
		MTX_UNLOCK(fs_mutex);
		MTX_LOCK(muk_mutex);
	}
#endif // ENABLE_FS

#if 	ENABLE_IS
	/*Create IS Thread  */
	if( is_ep != HARDWARE) {
		MUKDEBUG("Starting IS thread--------------------------------------\n");
	//	pthread_attr_init(&attrs);
	//	pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE);
		sprintf(muk_cmd,"is\n");
		rcode = pthread_create( &is_pth, NULL, pth_is, muk_cmd);
		if(rcode) ERROR_EXIT(rcode);
		MUKDEBUG("is_pth=%u\n",is_pth);
		mukproc_ptr = (proc_usr_t *) PROC_MAPPED(is_ep);
		SET_BIT( mukproc_ptr->p_misc_flags, MIS_BIT_UNIKERNEL);	
		MTX_UNLOCK(is_mutex);
		MTX_LOCK(muk_mutex);
	} 
#endif // ENABLE_IS 
		
#if ENABLE_NW
	/*Create NW Thread  */
	if( web_ep != HARDWARE) {
		MUKDEBUG("Starting NW thread--------------------------------------\n");
	//	pthread_attr_init(&attrs);
	//	pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE);
		sprintf(muk_cmd,"nw %s\n", web_cfg);
		rcode = pthread_create( &web_pth, NULL, pth_web, muk_cmd);
		if(rcode) ERROR_EXIT(rcode);
		MUKDEBUG("web_pth=%u\n",web_pth);
		mukproc_ptr = (proc_usr_t *) PROC_MAPPED(web_ep);
		SET_BIT( mukproc_ptr->p_misc_flags, MIS_BIT_UNIKERNEL);	
		MTX_UNLOCK(nw_mutex);
		MTX_LOCK(muk_mutex);
	}
#endif // ENABLE_NW

#if ENABLE_FTP
	if( ftp_ep != HARDWARE) {
		/*Create FTP Thread  */
		MUKDEBUG("Starting FTP thread--------------------------------------\n");
	//	pthread_attr_init(&attrs);
	//	pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE);
		sprintf(muk_cmd,"m3ftp\n");
		rcode = pthread_create( &ftp_pth, NULL, pth_ftp, muk_cmd );
		if(rcode) ERROR_EXIT(rcode);
		MUKDEBUG("ftp_pth=%u\n",ftp_pth);
		mukproc_ptr = (proc_usr_t *) PROC_MAPPED(ftp_ep);
		SET_BIT( mukproc_ptr->p_misc_flags, MIS_BIT_UNIKERNEL);	
		MTX_UNLOCK(ftp_mutex);
		MTX_LOCK(muk_mutex);
	}
#endif // ENABLE_FTP

	t_stop =  dwalltime();
	t_total = (t_stop-t_start);
 	printf("BOOT TIME: t_start=%.2f t_stop=%.2f t_total=%.2f [s]\n",t_start, t_stop, t_total);
	MUKDEBUG("JOINING ALL THREADS -------------------------------------\n");
    fflush(stdout);

		// JOINING ALL THREADS 
#if ENABLE_FTP
	if( ftp_ep != HARDWARE) {
		pthread_join(ftp_pth, &ret_ftp);
		MUKDEBUG("ret_ftp=%d\n",ret_ftp);
	}
#endif // ENABLE_FTP

#if ENABLE_NW
	if( web_ep != HARDWARE) {	
		pthread_join(web_pth, &ret_web);
		MUKDEBUG("ret_web=%d\n",ret_web);
	}
#endif // ENABLE_NW
	
#if ENABLE_IS
	if( is_ep != HARDWARE) {
		pthread_join(is_pth, &ret_is);
		MUKDEBUG("ret_is=%d\n",ret_is);
	}
#endif // ENABLE_IS
	
#if ENABLE_FS
	if( fs_ep != HARDWARE) {
		pthread_join(fs_pth, &ret_fs);
		MUKDEBUG("ret_fs=%d\n",ret_fs);
	}
#endif // ENABLE_FS
		
#if ENABLE_RD
	if( rd_ep != HARDWARE) {
		pthread_join(rd_pth, &ret_rd);
		MUKDEBUG("ret_rd=%d\n",ret_rd);
	}
#endif // ENABLE_RD 

#if ENABLE_PM
	if( pm_ep != HARDWARE) {
		pthread_join(pm_pth, &ret_pm);
		MUKDEBUG("ret_pm=%d\n",ret_pm);
	}
#endif // ENABLE_PM
	
#if ENABLE_SYSTASK
	if( sys_ep != HARDWARE) {
		pthread_join(sys_pth, &ret_st);
		MUKDEBUG("ret_st=%d\n",ret_st);
	}
#endif // ENABLE_SYSTASK
	
	exit(0);
}

