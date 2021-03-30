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

static struct sockaddr_un sw_svr_sun;		
static struct sockaddr_un rt_data_sun;			

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

static int rtap_user_init(void *data, void *dev)
{
	int rcode;
	
	printk("rtap_user.c:rtap_user_init\n");
	
	return 0;
}

static int rtap_open(void *data)
{
	struct rtap_data *pri = data;
	printk("rtap_user.c:rtap_open\n");

	return pri->fd;
}

static void rtap_remove(void *data)
{
	struct rtap_data *pri = data;

	printk("rtap_user.c:rtap_remove\n");

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
	printk( "rtap_user.c:rtap_user_write: fd=%d len=%d\n",  fd, len );

#if RTAP_IPC == RTAP_PIPE
	int bytes;
	bytes = write(sw_ptr->sw_Sfdes[PIPE_WRITE], buf, len);
	if( bytes < len) {
		printk( "rtap_user.c:rtap_user_write: write errno=%d\n", errno);
		return(-errno);
	}
	return(bytes);
#else //  RTAP_IPC
	struct sockaddr_un *data_addr = pri->data_addr;
	return net_send(fd, buf, len);
#endif //  RTAP_IPC

}

int rtap_user_read(int fd, void *buf, int len)
{
	printk( "rtap_user.c:rtap_user_read: fd=%d len=%d\n",  fd, len );

#if RTAP_IPC == RTAP_PIPE
	int bytes;
	bytes = read(sw_ptr->sw_Rfdes[PIPE_READ], buf, len);
	if( bytes < len) {
		printk( "rtap_user.c:rtap_user_read: read errno=%d\n", errno);
		return(-errno);
	}
	return(bytes);
#else //  RTAP_IPC
	struct sockaddr_un *data_addr = pri->data_addr;
	return net_send(fd, buf, len);
#endif //  RTAP_IPC

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
	if( rcode == OK){
		send_kthread(arg);
	}
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

#if RTAP_IPC == RTAP_PIPE
	printk( "\n");
	
#else //  RTAP_IPC
	printk( "\n");
	if(unlink(sw_ptr->sw_ctrl_path) < 0){
		printf("Couldn't remove control socket '%s' : ", sw_ptr->sw_ctrl_path);
		perror("");
	}
	if( sw_ptr->sw_tap_fd != (-1))
		close(sw_ptr->sw_tap_fd);
#endif //  RTAP_IPC
	
	rtap_kcleanup();
	
}

int start_sw_threads(unsigned long sp)
{
	int err;
	int one = 1;
	pid_t tid;
	
	printk("start_sw_threads - sp=%ld\n", sp);

#if RTAP_IPC == RTAP_PIPE

	if( pipe2(sw_ptr->sw_Sfdes,O_DIRECT ) == -1){
		err = -errno;
		printk( "start_rd_thread - pipe2 sw_Sfdes failed : errno = %d\n", errno);
		goto ss_out;
	}

	if( pipe2(sw_ptr->sw_Rfdes,O_DIRECT | O_NONBLOCK) == -1){
		err = -errno;
		printk( "start_rd_thread - pipe2 sw_Rfdes failed : errno = %d\n", errno);
		goto ss_out;
	}

	printk( "start_rd_thread: " SW_FORMAT, SW_FIELDS(sw_ptr));
#endif // RTAP_IPC
	
	err = init_sync();
	if( err) goto ss_out_close;

	err = enter_sync();
	if( err) goto ss_out_close;
	
	// create send_thread 
#if RTAP_IPC == RTAP_PIPE
	tid = clone((int (*)(void *))send_thread, (void *) sp, CLONE_VM, NULL);
#else //  RTAP_IPC
	tid = clone((int (*)(void *))send_thread, (void *) sp, CLONE_FILES | CLONE_VM, NULL);
#endif // RTAP_IPC
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
#if RTAP_IPC == RTAP_PIPE
	tid = clone((int (*)(void *))recv_thread, (void *) sp, CLONE_VM, NULL);
#else //  RTAP_IPC
	tid = clone((int (*)(void *))recv_thread, (void *) sp, CLONE_FILES | CLONE_VM, NULL);
#endif // RTAP_IPC
	printk("start_rd_thread sw_recv_thread tid=%d\n", tid);
	if(tid < 0){
		err = -errno;
		printk( "start_rd_thread - clone failed : errno = %d\n", errno);
		goto ss_out_close;
	} else{
//		sleep(2);
		sw_ptr->sw_recv_tid = tid;
	}

ss_out_close:

	err = leave_sync();
	if( err) goto ss_out_close;

	err = end_sync();
	if( err) goto ss_out_close;
	
	if( close(sw_ptr->sw_Sfdes[PIPE_READ]) == -1) {
		err = -errno;
		printk( "start_rd_thread - close sw_Sfdes failed : errno = %d\n", errno);
		goto ss_out;
	}
	
	if( close(sw_ptr->sw_Rfdes[PIPE_WRITE]) == -1) {
		err = -errno;
		printk( "start_rd_thread - close sw_Rfdes failed : errno = %d\n", errno);
		goto ss_out;
	}	
	
	printk( "start_sw_threads: " SW_FORMAT, SW_FIELDS(sw_ptr));

	return(OK);

ss_out:
	return err;
}

int rtap_receiver_kinit(void)
{
	int err;
	printk( "rtap_receiver_kinit\n");

#if RTAP_IPC == RTAP_PIPE
	if( close(sw_ptr->sw_Rfdes[PIPE_READ]) == -1) {
		err = -errno;
		printk( "rtap_receiver_kinit - close sw_Rfdes failed : errno = %d\n", errno);
		return(err);
	}

	if( close(sw_ptr->sw_Sfdes[PIPE_READ]) == -1) {
		err = -errno;
		printk( "rtap_receiver_kinit - close sw_Sfdes failed : errno = %d\n", errno);
		return(err);
	}
	
	if( close(sw_ptr->sw_Sfdes[PIPE_WRITE]) == -1) {
		err = -errno;
		printk( "rtap_receiver_kinit - close sw_Sfdes failed : errno = %d\n", errno);
		return(err);
	}

	printk( "rtap_receiver_kinit: " SW_FORMAT, SW_FIELDS(sw_ptr));

#else //  RTAP_IPC

	rt_ptr->rt_clt_fd = socket(AF_UNIX, SOCK_STREAM, 0);  
    if (rt_ptr->rt_clt_fd == -1){
		printk("rtap_receiver_kinit socket errno:%d\n", errno );
		return(-errno);
	}
	
	printk( "rtap_receiver_kinit: rt_clt_fd=%d\n",rt_ptr->rt_clt_fd);

    memset(&rt_data_sun, 0, sizeof(struct sockaddr_un));
	rt_data_sun.sun_family = AF_UNIX;
    strncpy(rt_data_sun.sun_path, sw_ptr->sw_svr_sock, sizeof(rt_data_sun.sun_path) - 1);

	printk( "rtap_receiver_kinit: sun_path=%s\n",rt_data_sun.sun_path);
    if (connect(rt_ptr->rt_clt_fd, (struct sockaddr *) &rt_data_sun,
                sizeof(struct sockaddr_un)) == -1){
        printk( "rtap_receiver_kinit connect errno:%d\n", errno );
		return(-errno);
	}
	printk( "rtap_receiver_kinit OK!\n");
#endif //  RTAP_IPC
	
	return(OK);			
}

int  rtap_sender_kinit( void)
{
	int err;

	printk( "rtap_sender_kinit\n");

#if RTAP_IPC == RTAP_PIPE
	if( close(sw_ptr->sw_Sfdes[PIPE_WRITE]) == -1) {
		err = -errno;
		printk( "rtap_sender_kinit - close sw_Sfdes failed : errno = %d\n", errno);
		return(err);
	}

	if( close(sw_ptr->sw_Rfdes[PIPE_READ]) == -1) {
		err = -errno;
		printk( "rtap_sender_kinit - close sw_Rfdes failed : errno = %d\n", errno);
		return(err);
	}
	
	if( close(sw_ptr->sw_Rfdes[PIPE_WRITE]) == -1) {
		err = -errno;
		printk( "rtap_sender_kinit - close sw_Rfdes failed : errno = %d\n", errno);
		return(err);
	}

	printk( "rtap_sender_kinit: " SW_FORMAT, SW_FIELDS(sw_ptr));

#else //  RTAP_IPC

    sw_ptr->sw_svr_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sw_ptr->sw_svr_fd  == -1){
		printk("rtap_sender_kinit socket errno:%d\n", errno );
		return(-errno);
	}

    if (remove(sw_ptr->sw_svr_sock) == -1 && errno != ENOENT)
        printk("rtap_sender_kinit remove-%s errno:%d\n",sw_ptr->sw_svr_sock, errno);

    memset(&sw_svr_sun, 0, sizeof(struct sockaddr_un));
    sw_svr_sun.sun_family = AF_UNIX;
    strncpy(sw_svr_sun.sun_path, sw_ptr->sw_svr_sock, sizeof(sw_svr_sun.sun_path) - 1);
    if (bind(sw_ptr->sw_svr_fd, (struct sockaddr *) &sw_svr_sun, sizeof(struct sockaddr_un)) == -1){
        printk("rtap_sender_kinit bind errno:%d\n", errno);
		return(-errno);
	}
	printk( "rtap_sender_kinit bind OK: sw_svr_fd=%d\n",sw_ptr->sw_svr_fd);

    if (listen(sw_ptr->sw_svr_fd, LISTEN_BACKLOG) == -1){
        printk("rtap_sender_kinit listen errno:%d\n", errno);
		return(-errno);
	}
	printk( "rtap_sender_kinit: listen OK\n");
#endif //  RTAP_IPC
	
	return(OK);
}


#if RTAP_IPC == RTAP_UNIX
int  rtap_server_wait( void)
{
	printk( "rtap_server_wait: sw_svr_fd=%d\n",sw_ptr->sw_svr_fd);

	printk( "rtap_server_wait: os_accept_connection\n");
//	sw_ptr->sw_clt_fd = os_accept_connection(sw_ptr->sw_svr_fd);
    sw_ptr->sw_clt_fd = accept(sw_ptr->sw_svr_fd, NULL, NULL);
	if (sw_ptr->sw_clt_fd  == -1){
        printk("rtap_server_wait accept errno:%d\n", errno);
		return(-errno);
	}
	
	printk( "rtap_server_wait OK: sw_clt_fd=%d\n",sw_ptr->sw_clt_fd);
	return(OK);
}
#endif //  RTAP_IPC
