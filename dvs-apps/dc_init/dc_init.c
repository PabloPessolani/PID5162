/**********************************************************************/
/**********************************************************************/
// dc_init
// usage: %s <config_file> 
//dc DCNAME {
//	dcid			0;
//	nr_procs		256;
//	nr_tasks		34;
//	nr_sysprocs		64;
//	nr_nodes		32;
//	warn2proc		0;
//	warnmsg			1;
//	tracker			-2;
//	ip_addr			"192.168.10.100";
//	memory			512;
//	image			"DC0.img";
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

#define  USRDBG		1
#define  DVS_USERSPACE	1

#define DVK_GLOBAL_HERE
#include "dc_init.h"
#include "glo.h"

/* Container representation */

#define STACK_SIZE  65536

struct clone_stack {
    char stack[65536] __attribute__ ((aligned(16))); /* Stack for the clone call */
    char ptr[0]; /* This ppointer actually point to the top of the stack array. */
};

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

container_t ctnr, *c_ptr;
int nr_containers;

static int init_dc(void *arg);
int becomeDaemon(int flags);
char link_name[128];
char root_dir[128];

void  main ( int argc, char *argv[] )
{
	int c, nodeid, child_pid , status, pid, len ,rcode, ret;
	dc_usr_t *dcu_ptr;
    char *stack;                        /* Start of stack buffer area */
    char *stack_top;                     /* End of stack buffer area */
    const mode_t START_UMASK = S_IWOTH; /* Initial umask setting */
    struct sigaction sa;
    struct utsname uts;
	FILE *fp;
	
extern char *optarg;
extern int optind, optopt, opterr;

	dcu_ptr = &dcu;
	dc_ptr=&ctnr.c_dcu;
	*dc_ptr = *dcu_ptr;
	c_ptr = &ctnr;
	
	if( argc != 2){
		fprintf( stderr,"usage: %s <config_file>\n", argv[0]);
		fflush(stderr);
		exit(1);
	}
	
	rcode = dvk_open();
	if (rcode < 0)  ERROR_EXIT(rcode);
	local_nodeid = dvk_getdvsinfo(&dvs);
	if(local_nodeid < 0 )
		ERROR_EXIT(local_nodeid);
	dvs_ptr = &dvs;
	printf(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));
	printf("local_nodeid=%d\n", local_nodeid);
	
	nr_containers = 0;		
	dc_read_config(argv[1]);
	
	if (nr_containers == 0){
		fprintf( stderr,"\nERROR. No container entries in %s\n", argv[1] );
		fflush(stderr);
		exit(1);
	}
	USRDEBUG("nr_containers=%d\n",nr_containers);
	if (nr_containers > 1){
		fprintf( stderr,"\nERROR. Only one container can be defined in %s\n", argv[1] );
		fflush(stderr);
		exit(1);
	}
		
	if( dc_ptr->dc_nr_tasks  > dc_ptr->dc_nr_procs )  {
		fprintf (stderr, "Invalid nr_tasks: must be <= nr_procs(%d)\n", dc_ptr->dc_nr_procs);
		exit(EXIT_FAILURE);
	}

	if( dc_ptr->dc_nr_sysprocs  > dc_ptr->dc_nr_procs )  {
		fprintf (stderr, "Invalid nr_sys_tasks: must be <= nr_procs(%d)\n", dc_ptr->dc_nr_procs);
		exit(EXIT_FAILURE);
	}
	
	if( ( strcmp( dc_ptr->dc_name, "DC0") == 0 ) 
		&& dc_ptr->dc_dcid != 0)
		sprintf(dc_ptr->dc_name,"DC%d", dc_ptr->dc_dcid);

	USRDEBUG("PARENT " DC_USR1_FORMAT, DC_USR1_FIELDS(dc_ptr));
	USRDEBUG("PARENT " DC_USR2_FORMAT, DC_USR2_FIELDS(dc_ptr));
	USRDEBUG("PARENT " DC_WARN_FORMAT, DC_WARN_FIELDS(dc_ptr));
	
    /* Allocate stack for child */
    stack = malloc(STACK_SIZE);
    if (stack == NULL) ERROR_EXIT(-errno);
    stack_top = stack + STACK_SIZE;  /* Assume stack grows downward */
	
	/* Establish handler to catch child termination signal */
//    sigemptyset(&sa.sa_mask);
//    sa.sa_flags = SA_RESTART;
//    sa.sa_handler = grimReaper;
//    if (sigaction(CHILD_SIG, &sa, NULL) == -1) {
//		fprintf (stderr, "sigaction(errno=%d)\n", errno);
//		exit(EXIT_FAILURE);
//	}  

    /* Create a pipe for child parent synchronization some stuff needs
     * to be done by parent (networking, cgroups) before child can
     * proceed */
    if (pipe(ctnr.c_pipe_fd) == -1) ERROR_EXIT(-errno);

    /* Directory where the container filesystem will reside */
	USRDEBUG("PARENT c_mount=%s\n", ctnr.c_mount );

    /* If the IP address is provided, we want to run in new network
     * namespace and create the veth pair.  */
    if (ctnr.c_ip_addr[0] != '\0')  {
        create_peer(ctnr.c_dcu.dc_dcid);
    }

	if ( tracker_flag ) {
		init_spread( );
		connect_to_spread();
	}
	
    child_pid = clone(init_dc, stack_top, 
					DC_CLONE_FLAGS|DC_NAMESPACES|SIGCHLD, dc_ptr);
	if(child_pid < 0) ERROR_EXIT(-errno);

	   /* Retrieve and display hostname */
    if (uname(&uts) == -1) ERROR_EXIT(-errno);
	printf("PARENT Sysname:  %s\n", uts.sysname);
    printf("PARENT Nodename: %s\n", uts.nodename);
    printf("PARENT Release:  %s\n", uts.release);
    printf("PARENT Version:  %s\n", uts.version);
    printf("PARENT Machine:  %s\n", uts.machine);
			
	sprintf(link_name,"/proc/%d/ns/pid",getpid());
	if ((len = readlink(link_name, root_dir, sizeof(root_dir)-1)) != -1)
		root_dir[len] = '\0';
    printf("PARENT link_name:%s root_dir=%s\n", link_name, root_dir);
	
	sprintf(link_name ,"/proc/%d/ns/pid",child_pid);
	if ((len = readlink(link_name, root_dir, sizeof(root_dir)-1)) != -1)
		root_dir[len] = '\0';
    printf("CHILD link_name:%s root_dir=%s\n", link_name, root_dir);

	sprintf(link_name,"/proc/%d/ns/uts",getpid());
	if ((len = readlink(link_name, root_dir, sizeof(root_dir)-1)) != -1)
		root_dir[len] = '\0';
    printf("PARENT link_name:%s root_dir=%s\n", link_name, root_dir);
	
	sprintf(link_name ,"/proc/%d/ns/uts",child_pid);
	if ((len = readlink(link_name, root_dir, sizeof(root_dir)-1)) != -1)
		root_dir[len] = '\0';
    printf("CHILD link_name:%s root_dir=%s\n", link_name, root_dir);
	

#define  sh_filename 	link_name
#define  dc_filename 	root_dir
	
	sprintf(sh_filename,"%s.sh", dc_ptr->dc_name);
	fp = fopen(sh_filename,"w");
	if(fp == NULL ) ERROR_EXIT(-errno);
	fputs("#!/bin/bash\n",fp);
	fprintf(fp,"NODEID=%d\n", local_nodeid); 
	fprintf(fp,"%s=%d\n",dc_ptr->dc_name,child_pid); 
	fprintf(fp,"export NODEID\n"); 
	fprintf(fp,"export %s\n",dc_ptr->dc_name); 
//	fprintf(fp,"exit 0\n"); 
	fclose(fp);
	
	rcode = chmod(sh_filename, S_IRWXU);
	if(rcode) ERROR_EXIT(-errno);
	
	
	// If we have new network namespace add the veth1 to child's namespace.
    if (ctnr.c_ip_addr[0] != '\0')  {
        network_setup(child_pid);
    }
	
    /* If limiting the memory, create the cgroup group and add the child. */
    if (ctnr.c_memory) cgroup_setup(child_pid, ctnr.c_memory);

    /* Close the write end of the pipe, to signal to the child that we   are ready. */
    close(ctnr.c_pipe_fd[1]);
		
	if ( tracker_flag == 1) {
		MTX_LOCK(trk_mutex);
		COND_WAIT(trk_cond, trk_mutex);
//		COND_SIGNAL(sys_cond);
		MTX_UNLOCK(trk_mutex);
		if( fork() == 0){
			ret = tracker_main();
			if(ret < 0) ERROR_EXIT(ret);
			exit(0);
		}
	}
    printf("PARENT exiting - child_pid=%ld\n", (long) child_pid);
	exit(0);
 }

static int init_dc(void *arg)
{
	dc_usr_t *dc_ptr;
    struct utsname uts;
	int nodeid;
	long rcode;
    node_usr_t node_usr, *node_usr_ptr;
	sigset_t set_old,set_new;
	char *ovfs_opts;
    char *upper;
    char *work;
    char *merged;
	char ch;
	 
	USRDEBUG("Waiting for parent to finish setup\n");

    if (read(c_ptr->c_pipe_fd[0], &ch, 1) != 0) {
        ERROR_EXIT(-errno);
    }
	
	USRDEBUG("CHILD  PID=%d PPID=%d\n", getpid(), getppid());
	
	dc_ptr = (dc_usr_t *) arg;
	USRDEBUG("CHILD before " DC_USR1_FORMAT, DC_USR1_FIELDS(dc_ptr));
	USRDEBUG("CHILD before " DC_USR2_FORMAT, DC_USR2_FIELDS(dc_ptr));
	USRDEBUG("CHILD before " DC_WARN_FORMAT, DC_WARN_FIELDS(dc_ptr));
	
    /* Become leader of new session */
	if (setsid() == -1)  ERROR_EXIT(-errno);
	sigemptyset(&set_old);
	sigemptyset(&set_new);
	sigfillset(&set_new);

	// mask ALL SIGNALS
	rcode = sigprocmask(SIG_SETMASK, &set_new, &set_old);
	if (rcode) ERROR_EXIT(-errno);
		
	USRDEBUG("CHILD I am a daemon\n");
	// init DC 
	nodeid = dvk_dc_init(dc_ptr);
	if( nodeid < 0) ERROR_EXIT(nodeid);
	printf("DC%d has been initialized on node %d\n", dc_ptr->dc_dcid, nodeid);
	
	node_usr_ptr = &node_usr;
	rcode = dvk_getnodeinfo(nodeid, node_usr_ptr);
	if( rcode < 0) ERROR_EXIT(rcode);
	USRDEBUG("CHILD " NODE_USR_FORMAT, NODE_USR_FIELDS(node_usr_ptr));
	
	rcode = dvk_getdcinfo(dc_ptr->dc_dcid, dc_ptr);
	if( rcode < 0) ERROR_EXIT(rcode);
	USRDEBUG("CHILD after  " DC_USR1_FORMAT, DC_USR1_FIELDS(dc_ptr));
	USRDEBUG("CHILD after  " DC_USR2_FORMAT, DC_USR2_FIELDS(dc_ptr));
	USRDEBUG("CHILD after  " DC_WARN_FORMAT, DC_WARN_FIELDS(dc_ptr));

	/* Change hostname in UTS namespace of child */
    if (sethostname(dc_ptr->dc_name, strlen(dc_ptr->dc_name)) == -1)
		ERROR_EXIT(-errno);
	
    /* Retrieve and display hostname */
    if (uname(&uts) == -1) ERROR_EXIT(-errno);
	printf("CHILD Sysname:  %s\n", uts.sysname);
    printf("CHILD Nodename: %s\n", uts.nodename);
    printf("CHILD Release:  %s\n", uts.release);
    printf("CHILD Version:  %s\n", uts.version);
    printf("CHILD Machine:  %s\n", uts.machine);
	
	printf("CHILD chroot to:  %s\n", c_ptr->c_mount);
    if (chroot(c_ptr->c_mount) == -1) ERROR_PRINT(-errno);
	
	/* remount / as private, on some systems / is shared */
#ifdef REMOUNT 	
    if (mount("/", "/", "none", MS_PRIVATE | MS_REC, NULL) < 0 ) {
        ERROR_EXIT(-errno);
    }
#endif // REMOUNT 	

	/* Create all the directories needed for overlayFS
     * Whats basically happening is:

       mount -t overlay overlay -o lowerdir=<image>,\
       upperdir=containers/<container>/upper,\
       workdir=containers/<container>/work \
       containers/<container>/merged
     */

#ifdef OVERLAYFS 	
	 USRDEBUG("CHILD c_mount  %s\n", c_ptr->c_mount);

    asprintf(&upper, "%s/upper", c_ptr->c_mount);
	USRDEBUG("CHILD upper  %s\n", upper);
	
    asprintf(&work, "%s/work", c_ptr->c_mount);
	USRDEBUG("CHILD work  %s\n", work);

    asprintf(&merged, "%s/merged", c_ptr->c_mount);
	USRDEBUG("CHILD merged  %s\n", merged);
	
    if (mkdir(c_ptr->c_mount, 0700) < 0 && errno != -EDVSEXIST) ERROR_EXIT(-errno);
    if (mkdir(upper, 0700) < 0 && errno != -EDVSEXIST) 		ERROR_EXIT(-errno);
    if (mkdir(work, 0700) < 0 && errno != -EDVSEXIST) 		ERROR_EXIT(-errno);
    if (mkdir(merged, 0700) < 0 && errno != -EDVSEXIST)		ERROR_EXIT(-errno);

    asprintf(&ovfs_opts, "lowerdir=%s,upperdir=%s,workdir=%s",c_ptr->c_image, upper, work);
	USRDEBUG("Overlayfs opts: %s\n", ovfs_opts);
    USRDEBUG("root on host: %s\n", merged);
//    if (mount("", merged, "overlay", MS_RELATIME, ovfs_opts) < 0) ERROR_EXIT(-errno);

    /* Unmount old proc as otherwise it'll be still showing all the host info. */
    if (umount2("/proc", MNT_DETACH) < 0  && errno != -EDVSEXIST) ERROR_EXIT(-errno);

    change_root(merged);
    USRDEBUG("Root changed\n");

    free(ovfs_opts);
    free(upper);
    free(work);
    free(merged);

#endif //  OVERLAYFS 	

#ifdef PROC_DEV  	

    /* Mount new /proc so commands like ps show correct information */
    if (mkdir("/proc", 0700) < 0 && errno != EEXIST) {
		ERROR_EXIT(-errno);
    }
	
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RELATIME, NULL) < 0) 
        ERROR_EXIT(-errno);
    USRDEBUG("/proc mounted\n");
		
	/* Mount new /dev, here we can actually create just some subset of
     * devices, but for the sake of the simplicity just create a new
     * one.*/
    if (mkdir("/dev", 0700) < 0 && errno != EEXIST) {
		ERROR_EXIT(-errno);
	}
	
	if (mount("udev", "/dev", "devtmpfs", MS_NOSUID | MS_RELATIME, NULL) < 0 ) {
        ERROR_EXIT(-errno);
    }
    USRDEBUG("/dev mounted\n");
#endif // PROC_DEV  	

    /* Setting env variables here just to make sure that the shell in
     * container works correctly, otherwise ther PATH and others ENV
     * variables are the same as from the parent process. Not really
     * making any effort here to clean it up, which we otherwise
     * should.*/
    USRDEBUG("Setting env variables\n");
    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin", 1);
    USRDEBUG("unsetenv LC_ALL\n");
    unsetenv("LC_ALL");

#ifdef NETWORK_SETTINGS

    USRDEBUG("Setting up network\n");

    /* If we have an IP address setup the network, assign IP and add
     * route to the gateway, bridge diyc0 which has by default
     * 172.16.0.1. Again using ip tool and system() to avoid netlink
     * code. */
    if (c_ptr->c_ip_addr[0] != '\0') {
        char *ip_cmd;
        system("ip link set veth1 up");
        asprintf(&ip_cmd, "ip addr add %s/24 dev veth1", c_ptr->c_ip_addr);
        system(ip_cmd);
        free(ip_cmd);
        system("ip route add default via 172.16.0.1");
    }
#endif // NETWORK_SETTINGS

#ifdef ACCESS_RIGHTS
    USRDEBUG("changing access rights\n");
    if (access(c_ptr->c_args[0], R_OK | X_OK) != 0) {
        ERROR_EXIT(-errno);
    }
#endif // ACCESS_RIGHTS

	if( tracker_flag ){
		MTX_LOCK(trk_mutex);
		COND_SIGNAL(trk_cond);
//		COND_WAIT(is_cond, muk_mutex);
		MTX_UNLOCK(trk_mutex);
	}

	USRDEBUG("sleep looping\n");
	while(1) {
//		printf("CHILD pid:%d\n", getpid());
		sleep(60);
	}

    exit(EXIT_SUCCESS);      /* Child terminates now */
}

static int copy_file(char *src, char *dst)
{
    int in, out;
    off_t bytes = 0;
    struct stat fileinfo = {0};
    int result = 0;

    if ((in = open(src, O_RDONLY)) == -1) {
		ERROR_EXIT(-errno);
    }

    if ((out = open(dst, O_RDWR | O_CREAT)) == -1)  {
        close(in);
		ERROR_EXIT(-errno);
    }

    fstat(in, &fileinfo);
    result = sendfile(out, in, &bytes, fileinfo.st_size);

    close(in);
    close(out);

    return(result);
}


/* Wrapper for pivot root syscall.
 * see pivot_root(2)
 */
static int pivot_root(char *new, char *old)
{
    USRDEBUG("Setting up pivot_root\nold: %s\nnew: %s\n", old, new);

    if (mount(new, new, "bind", MS_BIND | MS_REC, "") < 0) {
		ERROR_EXIT(-errno);
    }

    if (mkdir(old, 0700) < 0 && errno != EEXIST) {
		ERROR_EXIT(-errno);
    }

    return syscall(SYS_pivot_root, new, old);
}



/* Change the root and prepare the filesystem for the container, in
 * this case we just copy resolv.conf from the host so that dns
 * resolving works in the container.
 */
int change_root(char *path)
{
    char oldpath[PATH_MAX + 1];
    char oldroot[PATH_MAX + 1];
    char newroot[PATH_MAX + 1];

    USRDEBUG("change_root: %s\n", path);

    realpath(path, newroot);
//	strcpy(newroot, path);
    USRDEBUG("newroot: %s\n", newroot);
    snprintf(oldpath, PATH_MAX, "%s/.pivot_root", newroot);
    USRDEBUG("oldpath: %s\n", oldpath);
	
    realpath(oldpath, oldroot);
//	strcpy(oldroot, oldpath);
    USRDEBUG("oldroot: %s\n", oldroot);

    USRDEBUG("Calling pivot root\n");
    if (pivot_root(newroot, oldroot) < 0) 
		ERROR_EXIT(-errno);

    /* Change to new root so we can safely remove the old root*/
    chdir("/");

    if (copy_file("/.pivot_root/etc/resolv.conf", "/etc/resolv.conf") < 0) 
		ERROR_EXIT(-errno);
    if (copy_file("/.pivot_root/etc/nsswitch.conf", "/etc/nsswitch.conf") < 0)
		ERROR_EXIT(-errno);

    /* Unmount the old root and remove it so it is not accessible from
     * the container */
    if (umount2("/.pivot_root", MNT_DETACH) < 0) ERROR_EXIT(-errno);

    if (rmdir("/.pivot_root") < 0) ERROR_EXIT(-errno);

    return 0;
}

/* Setup cgroups for memory limit.
 * Cgroups are accessed through he /sys/fs/cgroup/<subgroup>
 * filesystem so we just create a subdir in /sys/fs/cgroup/memory/
 * which gets then populated by the kernel so we then just write the
 * desired values.
 */
int cgroup_setup(pid_t pid, unsigned int limit)
{
    char cgroup_dir[PATH_MAX + 1];
    char cgroup_file[PATH_MAX + 1];
    unsigned long mem = limit * (1024 * 1024);

    USRDEBUG("Setting up cgroups with memory limit %d MB (%lu)\n", limit, mem);

    snprintf(cgroup_dir, PATH_MAX, "/sys/fs/cgroup/memory/%u", pid);
    USRDEBUG("cgroup_dir %s\n", cgroup_dir);
    if (mkdir(cgroup_dir, 0700) < 0 && errno != EEXIST) 
		ERROR_EXIT(-errno);

    /* Maximum allowed memory for the container */
    snprintf(cgroup_file, PATH_MAX, "%s/memory.limit_in_bytes", cgroup_dir);
    USRDEBUG("cgroup_file %s\n", cgroup_file);
    FILE *fp = fopen(cgroup_file, "w+");
    if (NULL == fp) ERROR_EXIT(-errno);
    fprintf(fp, "%lu\n", mem);
    fclose(fp);

    /* No swap */
#ifdef MEMSW	
    snprintf(cgroup_file, PATH_MAX, "%s/memory.memsw.limit_in_bytes", cgroup_dir);
    USRDEBUG("cgroup_file %s\n", cgroup_file);
    fp = fopen(cgroup_file, "w");
    if (NULL == fp) ERROR_EXIT(-errno);
    fprintf(fp, "0\n");
    fclose(fp);
#endif // MEMSW	

    /* Add the container pid to the group */
    snprintf(cgroup_file, PATH_MAX, "%s/cgroup.procs", cgroup_dir);
    USRDEBUG("cgroup_file %s\n", cgroup_file);
    fp = fopen(cgroup_file, "a");
    if (NULL == fp) ERROR_EXIT(-errno);
    fprintf(fp, "%d\n", pid);
    fclose(fp);

    return 0;
}


/* Create the veth pair and bring it up.
 * Using the system() function and ip tool (iproute2 package) to avoid
 * the netlink code which would have been pretty complex and would
 * obscure the main parts.
 */ 
int create_peer(int dcid)
{
    char *set_br_int; char *add_bridge;
    char *set_int; char *set_int_up; char *add_to_bridge;

#ifdef ANULADO	
    asprintf(&add_bridge, "brctl addbr br%d", dcid);
	USRDEBUG( "PARENT add_bridge=%s\n", add_bridge);
    system(add_bridge);
    free(add_bridge);
	
    asprintf(&set_br_int, "ifconfig br%d 172.16.1.3 netmask 255.255.255.0", dcid);
	USRDEBUG( "PARENT set_br_int=%s\n", set_br_int);
    system(set_br_int);
    free(set_br_int);

#endif // ANULADO	

	asprintf(&set_int, "ip link add veth%d type veth peer name veth1", dcid);
	USRDEBUG( "PARENT set_int=%s\n", set_int);
    system(set_int);
    free(set_int);

    asprintf(&set_int_up, "ip link set veth%d up", dcid);
	USRDEBUG( "PARENT set_int_up=%s\n", set_int_up);
    system(set_int_up);
    free(set_int_up);

    asprintf(&add_to_bridge, "ip link set veth%d master br%d", dcid, dcid );
	USRDEBUG( "PARENT add_to_bridge=%s\n", add_to_bridge);
    system(add_to_bridge);
    free(add_to_bridge);

    return 0;
}

/* Move the veth1 device to the childs new netwrok namespace,
 * effectivelly connecting the parent's and child's namespaces. Again
 * using just system() to avoid netlink code.
 */
int network_setup(pid_t pid)
{
    char *set_pid_ns;

    asprintf(&set_pid_ns,"ip link set veth1 netns %d", pid);
	USRDEBUG( "set_pid_ns=%s\n", set_pid_ns);
    system(set_pid_ns);
    free(set_pid_ns);
    return 0;
}



