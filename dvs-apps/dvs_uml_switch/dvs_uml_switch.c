/* Copyright 2001, 2002 Jeff Dike and others
 * Licensed under the GPL
 * Modified by Pablo Pessolani 2020 to support DVS
 */
#define _GLOBAL_VARS_HERE	1

#include "switch.h"
char *ctl_path = "/tmp/dvs_uml.ctl";

static int hub = 0;
static int compat_v0 = 0;


static char *data_socket = NULL;
static struct sockaddr_un data_sun;

static void cleanup(void)
{
	int i;

	USRDEBUG("\n");

	if(unlink(ctl_path) < 0){
		printf("Couldn't remove control socket '%s' : ", ctl_path);
		perror("");
	}
	if((data_socket != NULL) && (unlink(data_socket) < 0)){
		printf("Couldn't remove data socket '%s' : ", data_socket);
		perror("");
	}
	if( sw_ptr->sw_tap_fd != (-1))
		close(sw_ptr->sw_tap_fd);
	rmttap_cleanup();
}

static struct pollfd *fds = NULL;
static int maxfds = 0;
static int nfds = 0;

void add_fd(int fd)
{
	struct pollfd *p;
	USRDEBUG("fd=%d\n",fd);

	if(nfds == maxfds){
		maxfds = maxfds ? 2 * maxfds : 4;
		if((fds = realloc(fds, maxfds * sizeof(struct pollfd))) == NULL){
			error("realloc");
			cleanup();
			exit(1);
		}
	}
	p = &fds[nfds++];
	p->fd = fd;
	p->events = POLLIN;
}

static void remove_fd(int fd)
{
	int i;
	USRDEBUG("fd=%d\n",fd);

	for(i = 0; i < nfds; i++){
		if(fds[i].fd == fd) break;
	}
	if(i == nfds){
		fprintf(stderr, "remove_fd : Couldn't find descriptor %d\n", fd);
	}
	memmove(&fds[i], &fds[i + 1], (maxfds - i - 1) * sizeof(struct pollfd));
	nfds--;
}

static void sig_handler(int sig)
{
	printf("Caught signal %d, cleaning up and exiting\n", sig);
	cleanup();
	signal(sig, SIG_DFL);
	kill(getpid(), sig);
}

static void close_descriptor(int fd)
{
	USRDEBUG("fd=%d\n",fd);
	remove_fd(fd);
	close(fd);
	close_port(fd);
}

static void new_port_v0(int fd, struct request_v0 *req, int data_fd)
{
	USRDEBUG("fd=%d data_fd=%d\n",fd, data_fd);

	switch(req->type){
		case REQ_NEW_CONTROL:
			setup_sock_port(fd, &req->u.new_control.name, data_fd);
			break;
		default:
			printf("Bad request type : %d\n", req->type);
			close_descriptor(fd);
	}
}

static void new_port_v1_v3(int fd, enum request_type type, 
		 struct sockaddr_un *sock, int data_fd)
{
	int n, err;

	USRDEBUG("ctrl_fd=%d type=%d data_fd=%d\n",fd, type, data_fd);

	switch(type){
		case REQ_NEW_CONTROL:
			err = setup_sock_port(fd, sock, data_fd);
			if(err) return;
			USRDEBUG("fd=%d sun.sun_path=%s\n",fd, data_sun.sun_path);
			n = write(fd, &data_sun, sizeof(data_sun));
			if(n != sizeof(data_sun)){
				perror("Sending data socket name");
				close_descriptor(fd);
			}
			break;
		default:
			printf("Bad request type : %d\n", type);
			close_descriptor(fd);
	}
}

static void new_port_v2(int fd)
{
	fprintf(stderr, "Version 2 is not supported\n");
	close_descriptor(fd);
}

static void new_port(int fd, int data_fd)
{
	union request req;
	int len;

	USRDEBUG("ctrl_fd=%d data_fd=%d\n",fd, data_fd);

	len = read(fd, &req, sizeof(req));
	if(len < 0){
		if(errno != EAGAIN){
			perror("Reading request");
			close_descriptor(fd);
		}
		return;
	}
	else if(len == 0){
		printf("EOF from new port\n");
		close_descriptor(fd);
		return;
	}
	if(req.v1.magic == SWITCH_MAGIC){
		if(req.v2.version == 2) new_port_v2(fd);
		if(req.v3.version == 3) 
			new_port_v1_v3(fd, req.v3.type, &req.v3.sock, data_fd);
		else if(req.v2.version > 2) 
			fprintf(stderr, "Request for a version %d port, which this "
			"dvs_uml_switch doesn't support\n", req.v2.version);
		else new_port_v1_v3(fd, req.v1.type, &req.v1.u.new_control.name, data_fd);
	}
		else new_port_v0(fd, &req.v0, data_fd);
}

void accept_connection(int fd)
{
	struct sockaddr addr;
	unsigned int len;
	int new;

	USRDEBUG("fd=%d\n",fd);

	len = sizeof(addr);
	new = accept(fd, &addr, &len);
	if(new < 0){
		perror("accept");
		return;
	}
	if(fcntl(new, F_SETFL, O_NONBLOCK) < 0){
		perror("fcntl - setting O_NONBLOCK");
		close(new);
		return;
	}
	add_fd(new);
}

int still_used(struct sockaddr_un *sun)
{
	int test_fd, ret = 1;

	USRDEBUG("\n");

	if((test_fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0){
		perror("socket");
		exit(1);
	}
	if(connect(test_fd, (struct sockaddr *) sun, sizeof(*sun)) < 0){
		if(errno == ECONNREFUSED){
			if(unlink(sun->sun_path) < 0){
				fprintf(stderr, "Failed to removed unused socket '%s': ", 
				sun->sun_path);
			perror("");
			}
		ret = 0;
	  }
	  else perror("connect");
	}
	close(test_fd);
	return(ret);
}

int bind_socket(int fd, const char *name, struct sockaddr_un *sock_out)
{
	struct sockaddr_un sun;

	USRDEBUG("fd=%d name=%s\n",fd, name);
		
	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, name, sizeof(sun.sun_path));
	USRDEBUG("sun_path=%s \n", sun.sun_path);

	if(bind(fd, (struct sockaddr *) &sun, sizeof(sun)) < 0){
		if((errno  == EADDRINUSE) && still_used(&sun)) return(EADDRINUSE);
		else if(bind(fd, (struct sockaddr *) &sun, sizeof(sun)) < 0){
			perror("bind");
		return(EPERM);
		}
	}
	if(sock_out != NULL) *sock_out = sun;
	return(0);
}

static char *prog;

void bind_sockets_v0(int ctl_fd, const char *ctl_name, 
	   int data_fd, const char *data_name)
{
	int ctl_err, ctl_present = 0, ctl_used = 0;
	int data_err, data_present = 0, data_used = 0;
	int try_remove_ctl, try_remove_data;

	USRDEBUG("ctl_fd=%d ctl_name=%s data_fd=%d data_name=%s\n", 
		ctl_fd, ctl_name, data_fd, data_name);

	ctl_err = bind_socket(ctl_fd, ctl_name, NULL);
	if(ctl_err != 0) ctl_present = 1;
	if(ctl_err == EADDRINUSE) ctl_used = 1;

	data_err = bind_socket(data_fd, data_name, &data_sun);
	if(data_err != 0) data_present = 1;
	if(data_err == EADDRINUSE) data_used = 1;

	if(!ctl_err && !data_err){
	  return;
	}

	try_remove_ctl = ctl_present;
	try_remove_data = data_present;
	if(ctl_present && ctl_used){
		fprintf(stderr, "The control socket '%s' has another server "
		"attached to it\n", ctl_name);
		try_remove_ctl = 0;
	}
	else if(ctl_present && !ctl_used)
		fprintf(stderr, "The control socket '%s' exists, isn't used, but couldn't "
		"be removed\n", ctl_name);
	if(data_present && data_used){
		fprintf(stderr, "The data socket '%s' has another server "
		"attached to it\n", data_name);
		try_remove_data = 0;
	}
	else if(data_present && !data_used)
		fprintf(stderr, "The data socket '%s' exists, isn't used, but couldn't "
		"be removed\n", data_name);
	if(try_remove_ctl || try_remove_data){
		fprintf(stderr, "You can either\n");
		if(try_remove_ctl && !try_remove_data) 
			fprintf(stderr, "\tremove '%s'\n", ctl_path);
		else if(!try_remove_ctl && try_remove_data) 
			fprintf(stderr, "\tremove '%s'\n", data_socket);
		else fprintf(stderr, "\tremove '%s' and '%s'\n", ctl_path, data_socket);
			fprintf(stderr, "\tor rerun with different, unused filenames for "
				"sockets:\n");
		fprintf(stderr, "\t\t%s -unix <control> <data>\n", prog);
		fprintf(stderr, "\t\tand run the UMLs with "
			"'eth0=daemon,,unix,<control>,<data>\n");
		exit(1);
	}
	else {
		fprintf(stderr, "You should rerun with different, unused filenames for "
			"sockets:\n");
		fprintf(stderr, "\t%s -unix <control> <data>\n", prog);
		fprintf(stderr, "\tand run the UMLs with "
			"'eth0=daemon,,unix,<control>,<data>'\n");
		exit(1);
	}
}

void bind_data_socket(int fd, struct sockaddr_un *sun)
{
	struct {
		char zero;
		int pid;
		int usecs;
	} name;
	struct timeval tv;

	USRDEBUG("fd=%d \n", fd);

	name.zero = 0;
	name.pid = getpid();
	gettimeofday(&tv, NULL);
	name.usecs = tv.tv_usec;
	sun->sun_family = AF_UNIX;
	memcpy(sun->sun_path, &name, sizeof(name));
	USRDEBUG("sun_path=%s \n", sun->sun_path);
	if(bind(fd, (struct sockaddr *) sun, sizeof(*sun)) < 0){
		perror("Binding to data socket");
		exit(1);
	}
}

void bind_sockets(int ctl_fd, const char *ctl_name, int data_fd)
{
	int err, used = 0;

	USRDEBUG("ctl_fd=%d ctl_name=%s data_fd=%d\n", ctl_fd, ctl_name, data_fd);

	err = bind_socket(ctl_fd, ctl_name, NULL);
	if(err == 0){
		bind_data_socket(data_fd, &data_sun);
		return;
	}
	else if(err == EADDRINUSE) used = 1;

	if(used){
		fprintf(stderr, "The control socket '%s' has another server "
			"attached to it\n", ctl_name);
		fprintf(stderr, "You can either\n");
		fprintf(stderr, "\tremove '%s'\n", ctl_name);
		fprintf(stderr, "\tor rerun with a different, unused filename for a "
			"socket\n");
	}
	else
		fprintf(stderr, "The control socket '%s' exists, isn't used, but couldn't "
			"be removed\n", ctl_name);
	exit(1);
}

static void Usage(void)
{
  char *tap_str = "";
  fprintf(stderr, "Usage : %s  <config.file> \n", prog);
  exit(1);
}


/*===========================================================================*
 *				switch_init				     *
 *===========================================================================*/
 void switch_init(void)
{
	int i;
	
    //Global glo.h
    nr_rmttap = 0; 
    nr_lcltap = 0; 
    nr_switch = 0;
    
	sw_ptr = &switch_cfg;
	sw_ptr->sw_hub 		= NO; 
	sw_ptr->sw_recv_ep 	= NONE;
	sw_ptr->sw_send_ep 	= NONE;
	sw_ptr->sw_recv_tid = PROC_NO_PID;
	sw_ptr->sw_send_tid = PROC_NO_PID;
	sw_ptr->sw_dcid  	= NO_DCID;
	sw_ptr->sw_daemon	= NO;
	sw_ptr->sw_tap_fd	= (-1);
	sw_ptr->sw_tap		= NULL;
	sw_ptr->sw_ctrl_path= ctl_path;
	sw_ptr->sw_name 	= "DVS_UML_SWITCH"; 
	
	for( i = 0; i <  NR_RMTTAP; i++){
		rt_ptr = &rmttap_cfg[i];
		rt_ptr->rt_index 	= i;
		rt_ptr->rt_nodeid 	= PROC_NO_PID;
		rt_ptr->rt_ctrl_fd 	= (-1);
		rt_ptr->rt_data_fd 	= (-1);
		rt_ptr->rt_poll_idx = (-1);
		rt_ptr->rt_rmttap_fd= (-1);
		rt_ptr->rt_tap		= NULL;
		rt_ptr->rt_name  	= NULL;
	}
}


/*===========================================================================*
 *				main				     *
 *===========================================================================*/
 int main(int argc, char **argv)
{
	int n, i, j, new, one = 1, daemonize = 0;
	int timeout; 
	rmttap_t *rt_ptr;
	int rcode;
	#ifdef TUNTAP
	int tap_fd  = -1;
	#endif

	prog = argv[0];
	if( argc != 2) Usage();
	
	switch_init();
    switch_config(argv[1]);  //Reads Config File

	sw_ptr 		= &switch_cfg;
	ctl_path 	= sw_ptr->sw_ctrl_path;
	hub    		= sw_ptr->sw_hub;
	daemonize	= sw_ptr->sw_daemon;
	USRDEBUG(SW_FORMAT, SW_FIELDS(sw_ptr));

	if( nr_rmttap > 0) { 
		rcode = dvk_open();     //load dvk
		if (rcode < 0)  ERROR_EXIT(rcode);
		get_dvs_params();
		get_dc_params(sw_ptr->sw_dcid);
		
		max_nr_rmttap = (dc_ptr->dc_nr_sysprocs - dc_ptr->dc_nr_tasks);
		if( nr_rmttap > max_nr_rmttap){
			fprintf(stderr, "CONFIGURATION ERROR: nr_rmttap(%d) >  max_nr_rmttap(%d)\n",
				nr_rmttap, max_nr_rmttap);
			return(EDVSNOSPC);								
		}
	}
	
	if((sw_ptr->sw_conn_fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0){
		perror("socket");
		exit(1);
	}
	USRDEBUG("sw_ptr->sw_conn_fd=%d\n", sw_ptr->sw_conn_fd);

	if(setsockopt(sw_ptr->sw_conn_fd, SOL_SOCKET, SO_REUSEADDR, (char *) &one, 
		sizeof(one)) < 0){
		perror("setsockopt");
		exit(1);
	}
	if(fcntl(sw_ptr->sw_conn_fd, F_SETFL, O_NONBLOCK) < 0){
		perror("Setting O_NONBLOCK on connection fd");
		exit(1);
	}

	if((sw_ptr->sw_data_fd = socket(PF_UNIX, SOCK_DGRAM, 0)) < 0){
		perror("socket");
		exit(1);
	}
	USRDEBUG("sw_ptr->sw_data_fd=%d\n", sw_ptr->sw_data_fd);

	if(fcntl(sw_ptr->sw_data_fd, F_SETFL, O_NONBLOCK) < 0){
		perror("Setting O_NONBLOCK on data fd");
		exit(1);
	}
   
	if(compat_v0) bind_sockets_v0(sw_ptr->sw_conn_fd, ctl_path, sw_ptr->sw_data_fd, data_socket);
	else bind_sockets(sw_ptr->sw_conn_fd, ctl_path, sw_ptr->sw_data_fd);

	if(listen(sw_ptr->sw_conn_fd, LISTEN_BACKLOG) < 0){
		perror("listen");
		exit(1);
	}

	if(signal(SIGINT, sig_handler) == SIG_ERR)
		perror("Setting handler for SIGINT");
	hash_init();

	if(compat_v0) 
		printf("%s attached to unix sockets '%s' and '%s'", prog, ctl_path,
			data_socket);
	else printf("%s attached to unix socket '%s'", prog, ctl_path);

	if(isatty(0))
		add_fd(0);
	add_fd(sw_ptr->sw_conn_fd);
	add_fd(sw_ptr->sw_data_fd);

#ifdef TUNTAP
	if(sw_ptr->sw_tap != NULL) {
		USRDEBUG("sw_ptr->sw_tap=%s\n", sw_ptr->sw_tap);
		sw_ptr->sw_tap_fd = open_tap(sw_ptr->sw_tap);
	}
	USRDEBUG("SWITCH TAP %s sw_tap_fd=%d\n", sw_ptr->sw_tap, sw_ptr->sw_tap_fd);
	if(sw_ptr->sw_tap_fd > -1) add_fd(sw_ptr->sw_tap_fd);
	USRDEBUG("\n");
#endif

	if (daemonize && daemon(0, 1)) {
		perror("daemon");
		exit(1);
	}
	USRDEBUG("\n");
	
	MTX_LOCK(sw_mutex);
	if( nr_rmttap > 0) {
		init_rt_threads();
	}

//while(TRUE)
//	sleep(5);

	USRDEBUG("\n");
	timeout = 0;
	
	while(1){
		char buf[128];
		n = poll(fds, nfds, timeout);
		if(n == 0){
			USRDEBUG("Timeout\n");
			if ( timeout == 0){
				timeout = POLL_TIMEOUT;
				MTX_UNLOCK(sw_mutex);		
			}
			continue;
		}
		if(n < 0){
			if(errno == EINTR) {
				// USRDEBUG("EINTR\n");
				continue;
			}
			perror("poll");
			break;
		}
		for(i = 0; i < nfds; i++){
			if(fds[i].revents == 0) continue;
			if(fds[i].fd == 0){      // STDIN 
				if(fds[i].revents & POLLHUP){
					printf("EOF on stdin, cleaning up and exiting\n");
					goto out;
				}
				n = read(0, buf, sizeof(buf));
				if(n < 0){
					perror("Reading from stdin");
					break;
				}
				else if(n == 0){
					printf("EOF on stdin, cleaning up and exiting\n");
					goto out;
				}
			} else if(fds[i].fd == sw_ptr->sw_conn_fd){ // ALQUIEN QUIERE CONECTARSE 
				if(fds[i].revents & POLLHUP){
					printf("Error on connection fd\n");
					continue;
				}
				accept_connection(sw_ptr->sw_conn_fd);
			}
			else if(fds[i].fd == sw_ptr->sw_data_fd) 
				handle_sock_data(sw_ptr->sw_data_fd, hub);
#ifdef TUNTAP
			else if(fds[i].fd == sw_ptr->sw_tap_fd) 
				handle_tap_data(sw_ptr->sw_tap_fd, hub);
#endif
			else {		
				// NEW PORT 
				new = handle_port(fds[i].fd);
				if(new) new_port(fds[i].fd, sw_ptr->sw_data_fd);
				else close_descriptor(fds[i].fd);
			}
		}
	}
out:
	cleanup();
	return 0;
}

