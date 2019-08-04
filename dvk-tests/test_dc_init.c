/**********************************************************************/
// dc_init
// usage: %s <config_file> 
//dc DCNAME {
//	dcid			0;
//	nr_procs		256;
//	nr_tasks		32;
//	nr_sysprocs		64;
//	nr_nodes		32;
//	warn2proc		0;
//	warnmsg			1;
//	ip_addr			"192.168.10.100";
//	memory			512;
//};
//  The specified DC is started an initilized.
//  All namespace are created, and a deamon is created inside the NS to keep 
//  at least one process in the NS
//  As a result a shell script is created called <dc_name>.sh in the current directory
//  The shell creates an environment variable called <dc_name> with the PID of 
//  the deamon process of the DC in the init(root) NS.
//  WARINING: To execute the script it must be done without a shell fork
//   #> . ./DC12.sh  <<<<< the first dot is to avoid a fork
//    The variable DC12, now has the PID as value. Then, it can be used to start new
// Processes within the NS.
//   #> nsenter -p -t$DC12 test_bind 12 3
/**********************************************************************/

#include "tests.h"

dvs_usr_t dvs, *dvs_ptr;
extern int local_nodeid;

dc_usr_t dcu = {
	.dc_dcid = 0,
	.dc_flags = 0,
    .dc_nr_procs = NR_PROCS,
	.dc_nr_tasks = NR_TASKS,
	.dc_nr_sysprocs = NR_SYS_PROCS,
	.dc_nr_nodes = NR_NODES,
	.dc_warn2proc = NONE,
	.dc_warnmsg = KERNEL,
	.dc_name = "DC0"
};

#define DC_NAMESPACES (CLONE_NEWUTS|CLONE_NEWPID|CLONE_NEWIPC|CLONE_NEWNS|CLONE_NEWNET)
#define DC_CLONE_FLAGS (CLONE_FILES|CLONE_SIGHAND|CLONE_VM)

static int init_dc(void *arg);
int becomeDaemon(int flags);
char link_name[128];
char file_name[128];

void  main ( int argc, char *argv[] )
{
	int c, nodeid, child_pid , status, pid, len ,rcode, ret;
	dc_usr_t *dcu_ptr;
    const int STACK_SIZE = 65536;       /* Stack size for cloned child */
    char *stack;                        /* Start of stack buffer area */
    char *stack_top;                     /* End of stack buffer area */
    const mode_t START_UMASK = S_IWOTH; /* Initial umask setting */
    struct sigaction sa;
    struct utsname uts;
	FILE *fp;
	
extern char *optarg;
extern int optind, optopt, opterr;

	dcu_ptr = &dcu;
	
	while ((c = getopt(argc, argv, "d:p:t:s:n:N:m:P:")) != -1) {
		switch(c) {
			case 'd':
				dcu_ptr->dc_dcid = atoi(optarg);
				if( dcu_ptr->dc_dcid < 0 || dcu_ptr->dc_dcid >= NR_DCS) {
					fprintf (stderr, "Invalid dcid [0-%d]\n", NR_DCS-1 );
					exit(EXIT_FAILURE);
				}
				break;
			case 'p':
				dcu_ptr->dc_nr_procs = atoi(optarg);
				if( dcu_ptr->dc_nr_procs <= 0 || dcu_ptr->dc_nr_procs  > NR_PROCS) {
					fprintf (stderr, "Invalid nr_procs [1-%d]\n", NR_PROCS);
					exit(EXIT_FAILURE);
				}
				break;				
			case 't':
				dcu_ptr->dc_nr_tasks = atoi(optarg);
				if( dcu_ptr->dc_nr_tasks <= 0 || dcu_ptr->dc_nr_tasks  > NR_TASKS ) {
					fprintf (stderr, "Invalid nr_tasks [1-%d]\n", NR_TASKS);
					exit(EXIT_FAILURE);
				}
				break;	
			case 's':
				dcu_ptr->dc_nr_sysprocs = atoi(optarg);
				if( dcu_ptr->dc_nr_sysprocs <= 0 || dcu_ptr->dc_nr_sysprocs > NR_SYS_PROCS ) {
					fprintf (stderr, "Invalid nr_sysprocs [1-%d]\n", NR_SYS_PROCS);
					exit(EXIT_FAILURE);
				}
				break;
			case 'n':
				dcu_ptr->dc_nr_nodes = atoi(optarg);
				if( dcu_ptr->dc_nr_nodes <= 0 || dcu_ptr->dc_nr_nodes > NR_NODES ) {
					fprintf (stderr, "Invalid nr_nodes [1-%d]\n", NR_NODES);
					exit(EXIT_FAILURE);
				}
				break;
			case 'N':
				if( strlen(optarg) > (MAXDCNAME-1) ) {
					fprintf (stderr, "DC name too long [%d]\n", (MAXDCNAME-1));
					exit(EXIT_FAILURE);
				}
				strncpy(dcu_ptr->dc_name,optarg,(MAXDCNAME-1));
				dcu_ptr->dc_name[MAXDCNAME-1]= '\0';

				break;
			case 'P':
				dcu_ptr->dc_warn2proc = atoi(optarg);
				break;
			case 'm':
				dcu_ptr->dc_warnmsg = atoi(optarg);
				break;
			default:
				fprintf (stderr,"usage: %s [-d dcid] [-p nr_procs] [-t nr_tasks] [-s nr_sysprocs][-n nr_nodes] [-P warn2proc] [-m warnmsg][-N DCname]\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}
	
	if( argv[optind] != NULL){
		fprintf (stderr,"usage: %s [-d dcid] [-p nr_procs] [-t nr_tasks] [-s nr_sysprocs][-n nr_nodes] [-P warn2proc] [-m warnmsg][-N DCname]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	if( dcu_ptr->dc_nr_tasks  > dcu_ptr->dc_nr_procs )  {
		fprintf (stderr, "Invalid nr_tasks: must be <= nr_procs(%d)\n", dcu_ptr->dc_nr_procs);
		exit(EXIT_FAILURE);
	}

	if( dcu_ptr->dc_nr_sysprocs  > dcu_ptr->dc_nr_procs )  {
		fprintf (stderr, "Invalid nr_sys_tasks: must be <= nr_procs(%d)\n", dcu_ptr->dc_nr_procs);
		exit(EXIT_FAILURE);
	}
	
	if( ( strcmp( dcu_ptr->dc_name, "DC0") == 0 ) 
		&& dcu_ptr->dc_dcid != 0)
		sprintf(dcu_ptr->dc_name,"DC%d", dcu_ptr->dc_dcid);

	dcu_ptr =&dcu;
	printf( DC_USR1_FORMAT, DC_USR1_FIELDS(dcu_ptr));
	printf( DC_USR2_FORMAT, DC_USR2_FIELDS(dcu_ptr));
	printf( DC_WARN_FORMAT, DC_WARN_FIELDS(dcu_ptr));
	
	ret = dvk_open();
	if (ret < 0)  ERROR_PRINT(ret);

    ret = init_dc(dcu_ptr);
	if(ret < 0) ERROR_PRINT(-errno);

	sleep(5);

	local_nodeid = dvk_getdvsinfo(&dvs);
	if(local_nodeid < 0 )
		ERROR_EXIT(local_nodeid);
	dvs_ptr = &dvs;
	printf(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));
	printf("local_nodeid=%d\n", local_nodeid);
	
	exit(0);
 }

static int init_dc(void *arg)
{
	dc_usr_t *dcu_ptr;
    struct utsname uts;
	int nodeid;
	long rcode;
    node_usr_t node_usr, *node_usr_ptr;
	sigset_t set_old,set_new;
	 
	dcu_ptr = (dc_usr_t *) arg;
	printf("before " DC_USR1_FORMAT, DC_USR1_FIELDS(dcu_ptr));
	printf("before " DC_USR2_FORMAT, DC_USR2_FIELDS(dcu_ptr));
	printf("before " DC_WARN_FORMAT, DC_WARN_FIELDS(dcu_ptr));
	
	// init DC 
	nodeid = dvk_dc_init(dcu_ptr);
	if( nodeid < 0) ERROR_PRINT(nodeid);
	printf("DC%d has been initialized on node %d\n", dcu_ptr->dc_dcid, nodeid);
	
	node_usr_ptr = &node_usr;
	rcode = dvk_getnodeinfo(nodeid, node_usr_ptr);
	if( rcode < 0) ERROR_PRINT(rcode);
	printf("" NODE_USR_FORMAT, NODE_USR_FIELDS(node_usr_ptr));
	
	rcode = dvk_getdcinfo(dcu_ptr->dc_dcid, dcu_ptr);
	if( rcode < 0) ERROR_PRINT(rcode);
	printf("after  " DC_USR1_FORMAT, DC_USR1_FIELDS(dcu_ptr));
	printf("after  " DC_USR2_FORMAT, DC_USR2_FIELDS(dcu_ptr));
	printf("after  " DC_WARN_FORMAT, DC_WARN_FIELDS(dcu_ptr));

    exit(EXIT_SUCCESS);      /* Child terminates now */
}


