
#include "tests.h"

dvs_usr_t  dvs = {
		.d_name = "DEFAULT",
		.d_nr_dcs=NR_DCS,
		.d_nr_nodes=NR_NODES,
		.d_nr_procs=NR_PROCS,
		.d_nr_tasks=NR_TASKS,
		.d_nr_sysprocs=NR_SYS_PROCS,
		.d_max_copybuf=MAXCOPYBUF,
		.d_max_copylen=MAXCOPYLEN,

		.d_dbglvl=0x00000000, /* DEFAULT IS 520966 = 0X7F306 */
		.d_version=DVS_VERSION,
		.d_flags=0,
		0,
		};

   
void  main ( int argc, char *argv[] )
{
	int c, nodeid, ret;
	dvs_usr_t  *d_ptr; 
extern char *optarg;
extern int optind, optopt, opterr;

	nodeid = (-1);

	while ((c = getopt(argc, argv, "n:V:N:P:T:S:B:L:D:C:")) != -1) {
		switch(c) {
			case 'n':
				nodeid = atoi(optarg);
				if( nodeid < 0 || nodeid >= NR_NODES) {
					fprintf (stderr, "Invalid nodeid [0-%d]\n", NR_NODES-1 );
					exit(EXIT_FAILURE);
				}
				break;
			case 'V':
				dvs.d_nr_dcs = atoi(optarg);
				if( dvs.d_nr_dcs <= 0 || dvs.d_nr_dcs  > NR_DCS) {
					fprintf (stderr, "Invalid nr_dcs [1-%d]\n", NR_DCS);
					exit(EXIT_FAILURE);
				}
				break;
			case 'N':
				dvs.d_nr_nodes = atoi(optarg);
				if(dvs.d_nr_nodes <= 0 || dvs.d_nr_nodes  > NR_NODES ) {
					fprintf (stderr, "Invalid nr_nodes [1-%d]\n", dvs.d_nr_nodes );
					exit(EXIT_FAILURE);
				}
				break;
			case 'P':
				dvs.d_nr_procs = atoi(optarg);
				if( dvs.d_nr_procs <= 0 || dvs.d_nr_procs > NR_PROCS ) {
					fprintf (stderr, "Invalid nr_procs [1-%d]\n", NR_PROCS);
					exit(EXIT_FAILURE);
				}
				break;					
			case 'T':
				dvs.d_nr_tasks = atoi(optarg);
				if( dvs.d_nr_tasks <= 0 || dvs.d_nr_tasks  > NR_TASKS ) {
					fprintf (stderr, "Invalid nr_tasks [1-%d]\n", NR_TASKS);
					exit(EXIT_FAILURE);
				}
				break;	
			case 'S':
				dvs.d_nr_sysprocs = atoi(optarg);
				if( dvs.d_nr_sysprocs <= 0 || dvs.d_nr_sysprocs > NR_SYS_PROCS ) {
					fprintf (stderr, "Invalid nr_sysprocs [1-%d]\n", NR_SYS_PROCS);
					exit(EXIT_FAILURE);
				}
				break;

			case 'B':
				dvs.d_max_copybuf = atoi(optarg);
				if( dvs.d_max_copybuf <= 0 || dvs.d_max_copybuf > MAXCOPYBUF ) {
					fprintf (stderr, "Invalid 0 < max_copybuf < %d\n", MAXCOPYBUF);
					exit(EXIT_FAILURE);
				}
				break;
			case 'L':
				dvs.d_max_copylen = atoi(optarg);
				if( dvs.d_max_copylen <= 0 || dvs.d_max_copylen > MAXCOPYLEN ) {
					fprintf (stderr, "Invalid 0 < max_copylen < %d\n", MAXCOPYLEN);
					exit(EXIT_FAILURE);
				}
				break;
			case 'D':
				dvs.d_dbglvl = atol(optarg);
				break;
			case 'Q':
				dvs.d_dbglvl = atol(optarg);
				break;
			case 'C':
				if (strlen (optarg) > (MAXPROCNAME-1)){
					fprintf (stderr, "DVS Cluster name too long (max length = %d)\n", MAXPROCNAME-1);
					exit(EXIT_FAILURE);
				}
				strncpy(dvs.d_name, optarg, MAXPROCNAME-1);
				break;
			default:
				fprintf (stderr,"usage: %s \n\t [-C dvs_name] [-n nodeid] [-V nr_dcs] [-N nr_nodes] [-P nr_procs] [-T nr_tasks] \n\t"
						" [-B max_copybuf] [-L max_copylen] [-S nr_sysprocs] [-D dbglvl] \n"
						, argv[0]);
				exit(EXIT_FAILURE);
		}
	}
	

	if( argv[optind] != NULL || nodeid == (-1)){
		fprintf (stderr,"usage: %s \n\t [-C dvs_name] [-n nodeid] [-V nr_dcs] [-N nr_nodes] [-P nr_procs] [-T nr_tasks] \n\t"
				" [-B max_copybuf] [-L max_copylen] [-S nr_sysprocs] [-D dbglvl] \n"
				, argv[0]);
		exit(EXIT_FAILURE);
	}	

	d_ptr = &dvs;
	if(d_ptr->d_nr_dcs <= 0  || d_ptr->d_nr_dcs > NR_DCS ) {
		fprintf (stderr,"ERROR(%u): must be 0 < nr_dcs < %d\n",__LINE__,NR_DCS);
		exit(EXIT_FAILURE);
	}

	if(d_ptr->d_nr_nodes <= 0 || d_ptr->d_nr_nodes > NR_NODES) {
		fprintf (stderr,"ERROR(%u): must be 0 < nr_nodes < %d\n",__LINE__,NR_NODES);
		exit(EXIT_FAILURE);
	}

	if(d_ptr->d_nr_procs <= 0 || d_ptr->d_nr_procs > NR_PROCS ) {
		fprintf (stderr,"ERROR(%u): must be 0 < nr_procs < %d\n",__LINE__, NR_PROCS);
		exit(EXIT_FAILURE);
	}
		
	if(d_ptr->d_nr_tasks <= 0 || d_ptr->d_nr_tasks > NR_TASKS ) {
		fprintf (stderr,"ERROR(%u): must be 0 < nr_tasks < %d\n",__LINE__, NR_TASKS);
		exit(EXIT_FAILURE);
	}

	if(d_ptr->d_nr_sysprocs <= 0 || d_ptr->d_nr_sysprocs > NR_SYS_PROCS ) {
		fprintf (stderr,"ERROR(%u): must be 0 < nr_sysprocs <= %d\n",__LINE__, NR_SYS_PROCS);
		exit(EXIT_FAILURE);
	}

	if(d_ptr->d_nr_tasks < ((d_ptr->d_nr_nodes)+NR_FIXED_TASKS)) {
		fprintf (stderr,"ERROR(%u): must be nr_tasks >= (nr_nodes)+%d\n",__LINE__,NR_FIXED_TASKS);
		exit(EXIT_FAILURE);
	}
	
	if(d_ptr->d_nr_sysprocs >= (d_ptr->d_nr_procs+ d_ptr->d_nr_tasks)) {
		fprintf (stderr,"ERROR(%u): must be nr_sysprocs < (nr_procs+nr_tasks)\n");
		exit(EXIT_FAILURE);
	}

	if(d_ptr->d_nr_sysprocs <= d_ptr->d_nr_tasks) {
		fprintf (stderr,"ERROR(%u): must be nr_sysprocs > (nr_tasks)\n");
		exit(EXIT_FAILURE);
	}

	if(d_ptr->d_max_copybuf > MAXCOPYBUF) {
		fprintf (stderr,"ERROR(%u): must be max_copybuf <= %d\n",__LINE__, MAXCOPYBUF);
		exit(EXIT_FAILURE);
	}
	
	if(d_ptr->d_max_copylen > MAXCOPYLEN) {
		fprintf (stderr,"ERROR(%u): must be max_copylen <= %d\n",__LINE__, MAXCOPYLEN);
		exit(EXIT_FAILURE);
	}

	if(d_ptr->d_max_copylen < d_ptr->d_max_copybuf) {
		fprintf (stderr,"ERROR(%u): must be max_copylen > %d\n",__LINE__, d_ptr->d_max_copybuf);
		exit(EXIT_FAILURE);
	}

	ret = dvk_open();
	if (ret < 0)  ERROR_PRINT(ret);
	
    printf("Initializing DVS. Local node ID %d... \n", nodeid);
	ret = dvk_dvs_init(nodeid, &dvs);
	if(ret<0)  ERROR_PRINT(ret);
	d_ptr = &dvs;
	printf(DVS_USR_FORMAT, DVS_USR_FIELDS(d_ptr));
	printf(DVS_MAX_FORMAT, DVS_MAX_FIELDS(d_ptr));
	printf(DVS_VER_FORMAT, DVS_VER_FIELDS(d_ptr));
	
    printf("Get DVS info\n");
    nodeid = dvk_getdvsinfo(&dvs);
    printf("local node ID %d... \n", nodeid);
	printf(DVS_USR_FORMAT, DVS_USR_FIELDS(d_ptr));
	printf(DVS_MAX_FORMAT, DVS_MAX_FIELDS(d_ptr));
	printf(DVS_VER_FORMAT, DVS_VER_FIELDS(d_ptr));

}



