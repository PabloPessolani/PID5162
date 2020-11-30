/*
 * Copyright (C) 2001 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Copyright (C) 2001 Lennert Buytenhek (buytenh@gnu.org) and
 * James Leu (jleu@mindspring.net).
 * Copyright (C) 2001 by various other people who didn't put their name here.
 * Licensed under the GPL.
 */

#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <net_user.h>
#include <os.h>
#include <um_malloc.h>
#include <pthread.h>

#define  CONFIG_UML_DVK
#define  DVS_USERSPACE
#include "rtap.h"
#include "rtap_user.h"

pthread_mutex_t rt_main_mutex; 	
// PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rt_thread_mutex;

static char *data_socket = NULL;
static struct sockaddr_un data_sun;

unsigned int os_sleep_secs(unsigned int seconds)
{
	return(sleep(seconds));
}

int rtap_block_signals(void)
{
	sigset_t sigs;

	sigfillset(&sigs);
	/* Block all signals possible. */
	if (sigprocmask(SIG_SETMASK, &sigs, NULL) < 0) {
		printk(UM_KERN_ERR "winch_thread : sigprocmask failed, "
		       "errno = %d\n", errno);
		return(errno);
	}
	return(OK);
}

static struct sockaddr_un *new_addr(void *name, int len)
{
	struct sockaddr_un *sun;

	sun = uml_kmalloc(sizeof(struct sockaddr_un), UM_GFP_KERNEL);
	if (sun == NULL) {
		printk( "new_addr: allocation of sockaddr_un "
		       "failed\n");
		return NULL;
	}
	sun->sun_family = AF_UNIX;
	memcpy(sun->sun_path, name, len);
	return sun;
}

static int connect_to_switch(struct rtap_data *pri)
{
	struct sockaddr_un *ctl_addr = pri->ctl_addr;
	struct sockaddr_un *local_addr = pri->local_addr;
	struct sockaddr_un *sun;
	struct request_v3 req;
	int fd, n, err;

	pri->control = socket(AF_UNIX, SOCK_STREAM, 0);
	if (pri->control < 0) {
		err = -errno;
		printk( "rtap_open : control socket failed, "
		       "errno = %d\n", -err);
		return err;
	}

	if (connect(pri->control, (struct sockaddr *) ctl_addr,
		   sizeof(*ctl_addr)) < 0) {
		err = -errno;
		printk( "rtap_open : control connect failed, "
		       "errno = %d\n", -err);
		goto out;
	}

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) {
		err = -errno;
		printk( "rtap_open : data socket failed, "
		       "errno = %d\n", -err);
		goto out;
	}
	if (bind(fd, (struct sockaddr *) local_addr, sizeof(*local_addr)) < 0) {
		err = -errno;
		printk( "rtap_open : data bind failed, "
		       "errno = %d\n", -err);
		goto out_close;
	}

	sun = uml_kmalloc(sizeof(struct sockaddr_un), UM_GFP_KERNEL);
	if (sun == NULL) {
		printk( "new_addr: allocation of sockaddr_un "
		       "failed\n");
		err = -ENOMEM;
		goto out_close;
	}

	req.magic = SWITCH_MAGIC;
	req.version = SWITCH_VERSION;
	req.type = REQ_NEW_CONTROL;
	req.sock = *local_addr;
	n = write(pri->control, &req, sizeof(req));
	if (n != sizeof(req)) {
		printk( "rtap_open : control setup request "
		       "failed, err = %d\n", -errno);
		err = -ENOTCONN;
		goto out_free;
	}

	n = read(pri->control, sun, sizeof(*sun));
	if (n != sizeof(*sun)) {
		printk( "rtap_open : read of data socket failed, "
		       "err = %d\n", -errno);
		err = -ENOTCONN;
		goto out_free;
	}

	printk( "rtap_user.c:connect_to_switch: pri->control=%d fd=%d\n", pri->control, fd );
	pri->data_addr = sun;
	return fd;

 out_free:
	kfree(sun);
 out_close:
	close(fd);
 out:
	close(pri->control);
	return err;
}

static int rtap_user_init(void *data, void *dev)
{
	struct rtap_data *pri = data;
	struct timeval tv;
	struct {
		char zero;
		int pid;
		int usecs;
	} name;

	printk( "rtap_user.c:rtap_user_init\n");

	if (!strcmp(pri->sock_type, "unix"))
		pri->ctl_addr = new_addr(pri->ctl_sock,
					 strlen(pri->ctl_sock) + 1);
					 
	name.zero = 0;
	name.pid = os_getpid();
	gettimeofday(&tv, NULL);
	name.usecs = tv.tv_usec;
	pri->local_addr = new_addr(&name, sizeof(name));
	pri->dev = dev;

	printk( RTD_FORMAT, RTD_FIELDS(pri));

	pri->fd = connect_to_switch(pri);
	if (pri->fd < 0) {
		kfree(pri->local_addr);
		pri->local_addr = NULL;
		return pri->fd;
	}

	return 0;
}

static int rtap_open(void *data)
{
	struct rtap_data *pri = data;
	return pri->fd;
}

static void rtap_remove(void *data)
{
	struct rtap_data *pri = data;

	close(pri->fd);
	pri->fd = -1;
	close(pri->control);
	pri->control = -1;

	kfree(pri->data_addr);
	pri->data_addr = NULL;
	kfree(pri->ctl_addr);
	pri->ctl_addr = NULL;
	kfree(pri->local_addr);
	pri->local_addr = NULL;
}

int rtap_user_write(int fd, void *buf, int len, struct rtap_data *pri)
{
	struct sockaddr_un *data_addr = pri->data_addr;
	printk( "rtap_user.c:connect_to_switch: fd=%d len=%d\n",  fd, len );

	return net_send(fd, buf, len);
}

const struct net_user_info rtap_user_info = {
	.init		= rtap_user_init,
	.open		= rtap_open,
	.close	 	= NULL,
	.remove	 	= rtap_remove,
	.add_address	= NULL,
	.delete_address = NULL,
	.mtu		= ETH_MAX_PACKET,
	.max_packet	= ETH_MAX_PACKET + ETH_HEADER_OTHER,
};


long os_receive_T(int src, message *m_ptr, long int timeout)
{
	return(dvk_receive_T(src, m_ptr, timeout));
} 

long os_send_T( int dst, message *m_ptr, long int timeout)
{
	return(dvk_send_T(dst, m_ptr, timeout));	
}

long os_sendrec_T(int dst, message *m_ptr, long int timeout)
{
	return(dvk_sendrec_T(dst, m_ptr, timeout));		
}

/*===========================================================================*
 *				send_thread		     
 *===========================================================================*/	
void *send_thread(void *arg)
{
	int rcode;
	rcode = init_send_kthread(arg);
	printk( "send_thread init_send_kthread rcode=%d\n", rcode);
	if( rcode == OK) send_kthread(arg);
	return(1);
}

/*===========================================================================*
 *				recv_thread			     
 *===========================================================================*/	
void *recv_thread(void *arg)
{
	int rcode;

	rcode = init_recv_kthread(arg);
	printk( "recv_thread init_recv_kthread rcode=%d\n", rcode);
	if( rcode == OK) recv_kthread(arg);
	return(1);
}

void rmttap_cleanup(void)
{
	int i, rcode, svr_ep, clt_ep; 
	
	printk( "\n");

	if(unlink(sw_ptr->sw_ctrl_path) < 0){
		printf("Couldn't remove control socket '%s' : ", sw_ptr->sw_ctrl_path);
		perror("");
	}
	if((data_socket != NULL) && (unlink(sw_ptr->sw_ctrl_path) < 0)){
		printf("Couldn't remove data socket '%s' : ", sw_ptr->sw_ctrl_path);
		perror("");
	}
	if( sw_ptr->sw_tap_fd != (-1))
		close(sw_ptr->sw_tap_fd);
	
	rtap_kcleanup();
#ifdef ANULADO 	
	for( i = 0; i < nr_rmttap; i++) {
		rt_ptr = &rmttap_cfg[i];
		if( rt_ptr->rt_rmttap_fd != (-1)){
			printk( "close %s fd=%d\n", rt_ptr->rt_name, rt_ptr->rt_rmttap_fd);			
			os_close(rt_ptr->rt_rmttap_fd);
		}
	}
#endif  // ANULADO 	
	
}

int still_used(struct sockaddr_un *sun)
{
	int test_fd, ret = 1;

	printk( "still_used\n");

	if((test_fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0){
		printk( "socket\n");
		return(1);
	}
	if(connect(test_fd, (struct sockaddr *) sun, sizeof(*sun)) < 0){
		if(errno == ECONNREFUSED){
			if(unlink(sun->sun_path) < 0){
				printk( "Failed to removed unused socket '%s': \n", 
				sun->sun_path);
			printk( "\n");
			}
		ret = 0;
	  }
	  else printk( "connect\n");
	}
	close(test_fd);
	return(ret);
}

int bind_socket(int fd, const char *name, struct sockaddr_un *sock_out)
{
	struct sockaddr_un sun;

	printk( "fd=%d name=%s\n",fd, name);
		
	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, name, sizeof(sun.sun_path));
	printk( "sun_path=%s \n", sun.sun_path);

	if(bind(fd, (struct sockaddr *) &sun, sizeof(sun)) < 0){
		if((errno  == EADDRINUSE) && still_used(&sun)) return(EADDRINUSE);
		else if(bind(fd, (struct sockaddr *) &sun, sizeof(sun)) < 0){
			printk( "bind");
		return(EPERM);
		}
	}
	if(sock_out != NULL) *sock_out = sun;
	return(0);
}

int bind_data_socket(int fd, struct sockaddr_un *sun)
{
	struct {
		char zero;
		int pid;
		int usecs;
	} name;
	struct timeval tv;

	printk( "bind_data_socket fd=%d \n", fd);

	name.zero = 0;
	name.pid = os_getpid();
	gettimeofday(&tv, NULL);
	name.usecs = tv.tv_usec;
	sun->sun_family = AF_UNIX;
	memcpy(sun->sun_path, &name, sizeof(name));
	printk( "bind_data_socket sun_path=%s \n", sun->sun_path);
	if(bind(fd, (struct sockaddr *) sun, sizeof(*sun)) < 0){
		printk( "bind_data_socket Binding to data socket");
		return(-errno);
	}
	return(OK);
}

int bind_sockets(int ctl_fd, const char *ctl_name, int data_fd)
{
	int rcode;

	printk( "ctl_fd=%d ctl_name=%s data_fd=%d\n", ctl_fd, ctl_name, data_fd);

	rcode = bind_socket(ctl_fd, ctl_name, NULL);
	if(rcode == 0){
		rcode = bind_data_socket(data_fd, &data_sun);
		return(rcode);
	}
	if(rcode == EADDRINUSE) {
		printk( "The control socket '%s' has another server "
			"attached to it\n", ctl_name);
		printk( "You can either\n");
		printk( "\tremove '%s'\n", ctl_name);
		printk( "\tor rerun with a different, unused filename for a "
			"socket\n");
	} else {
		printk( "The control socket '%s' exists, isn't used, but couldn't "
			"be removed\n", ctl_name);
	}
	return(rcode);
}

int start_sw_threads(unsigned long sp, int *fd_out)
{
	int err;
	int one = 1;
	pid_t tid;
	
	printk("start_sw_threads - sp=%ld\n", sp);

	if((sw_ptr->sw_conn_fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0){
		printk( "socket\n");
		goto ss_out;
	}
	printk( "sw_ptr->sw_conn_fd=%d\n", sw_ptr->sw_conn_fd);

	if(setsockopt(sw_ptr->sw_conn_fd, SOL_SOCKET, SO_REUSEADDR, (char *) &one, 
		sizeof(one)) < 0){
		printk( "setsockopt\n");
		goto ss_out_close_conn;
	}
	
	if(fcntl(sw_ptr->sw_conn_fd, F_SETFL, O_NONBLOCK) < 0){
		printk( "Setting O_NONBLOCK on connection fd\n");
		goto ss_out_close_conn;
	}

	if((sw_ptr->sw_data_fd = socket(PF_UNIX, SOCK_DGRAM, 0)) < 0){
		printk( "socket\n");
		goto ss_out_close;
	}
	printk( "sw_ptr->sw_data_fd=%d\n", sw_ptr->sw_data_fd);

	if(fcntl(sw_ptr->sw_data_fd, F_SETFL, O_NONBLOCK) < 0){
		printk( "Setting O_NONBLOCK on data fd\n");
		goto ss_out_close;
	}
   
    sprintf(sw_ptr->sw_ctrl_path,"/tmp/rtap%02d.ctl",dcid);
	
	bind_sockets(sw_ptr->sw_conn_fd, sw_ptr->sw_ctrl_path, sw_ptr->sw_data_fd);

	if(listen(sw_ptr->sw_conn_fd, LISTEN_BACKLOG) < 0){
		printk( "listen\n");
		goto ss_out_close;
	}

	err = init_sync();
	if( err) goto ss_out_close;

	err = enter_sync();
	if( err) goto ss_out_close;
	
	// create send_thread 
	tid = clone((int (*)(void *))send_thread, (void *) sp, CLONE_FILES | CLONE_VM, NULL);
	printk("start_rd_thread sw_send_thread tid=%d\n", tid);
	if(tid < 0){
		err = -errno;
		printk( "start_rd_thread - clone failed : errno = %d\n", errno);
		goto ss_out_close;
	} else{
//		sleep(2);
		sw_ptr->sw_send_tid = tid;
	}

	err = leave_sync();
	if( err) goto ss_out_close;
	
	err = enter_sync();
	if( err) goto ss_out_close;
	
	// create recv_thread 
	tid = clone((int (*)(void *))recv_thread, (void *) sp, CLONE_FILES | CLONE_VM, NULL);
	printk("start_rd_thread sw_recv_thread tid=%d\n", tid);
	if(tid < 0){
		err = -errno;
		printk( "start_rd_thread - clone failed : errno = %d\n", errno);
		goto ss_out_close;
	} else{
//		sleep(2);
		sw_ptr->sw_recv_tid = tid;
	}

	err = leave_sync();
	if( err) goto ss_out_close;

	err = end_sync();
	if( err) goto ss_out_close;
	
	return(OK);
	
 	
ss_out_close:
	os_close_file(sw_ptr->sw_data_fd);
ss_out_close_conn:
	os_close_file(sw_ptr->sw_conn_fd);
	*fd_out = -1;
ss_out:
	return err;
}
