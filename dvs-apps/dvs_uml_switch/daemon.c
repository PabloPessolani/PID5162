/*
 * Copyright (C) 2001 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Copyright (C) 2001 Lennert Buytenhek (buytenh@gnu.org) and
 * James Leu (jleu@mindspring.net).
 * Copyright (C) 2001 by various other people who didn't put their name here.
 * Licensed under the GPL.
 */

#include "switch.h"
 
void rmttap_init(rmttap_t *rt_ptr)
{
	USRDEBUG("\n");

	strcpy(rt_ptr->rt_sock_type,"unix");
	sprintf(rt_ptr->rt_ctrl_path,"/tmp/uml%0.2d%0.2d.ctrl",sw_ptr->sw_dcid, rt_ptr->rt_index);
	sprintf(rt_ptr->rt_data_path,"/tmp/uml%0.2d%0.2d.data",sw_ptr->sw_dcid, rt_ptr->rt_index);
	rt_ptr->rt_ctrl_fd	= -1;
	rt_ptr->rt_data_fd	= -1;
	rt_ptr->rt_tap	 	= NULL;
	printf("rmttap_init (dvs_uml_switch version %d) - %s:%s\n",
	       SWITCH_VERSION, rt_ptr->rt_sock_type, rt_ptr->rt_ctrl_path);
}
 
static int connect_to_switch(rmttap_t *rt_ptr)
{
	int n, rcode;
	struct sockaddr_un sun;
	
	USRDEBUG("rt_name=%s\n", rt_ptr->rt_name);

	rt_ptr->rt_ctrl_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (rt_ptr->rt_ctrl_fd < 0) {
		rcode = -errno;
		fprintf(stderr, "connect_to_switch(%s) : control socket failed, "
		       "errno = %d\n", rt_ptr->rt_name,  rcode);
		ERROR_RETURN(rcode);
	}
	
	rt_ptr->rt_ctrl_sun.sun_family = AF_UNIX;
	strncpy(rt_ptr->rt_ctrl_sun.sun_path, sw_ptr->sw_ctrl_path, strlen(sw_ptr->sw_ctrl_path));
	USRDEBUG("CTRL sun_path=%s \n", rt_ptr->rt_ctrl_sun.sun_path);

	if (connect(rt_ptr->rt_ctrl_fd, (struct sockaddr *) &rt_ptr->rt_ctrl_sun,
		   sizeof(struct sockaddr_un)) < 0) {
		rcode = -errno;
		fprintf(stderr,"connect_to_switch(%s) : control connect failed, "
		       "errno = %d\n", rt_ptr->rt_name, -rcode);
		goto out;
	}

	rt_ptr->rt_data_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (rt_ptr->rt_data_fd < 0) {
		rcode = -errno;
		fprintf(stderr, "connect_to_switch(%s) : data socket failed, "
		       "errno = %d\n", rt_ptr->rt_name, rcode);
		goto out;
	}
	
	rt_ptr->rt_data_sun.sun_family = AF_UNIX;
	strncpy(rt_ptr->rt_data_sun.sun_path, rt_ptr->rt_data_path, strlen(rt_ptr->rt_data_path));
	USRDEBUG("DATA sun_path=%s \n", rt_ptr->rt_data_sun.sun_path);

	if (bind(rt_ptr->rt_data_fd, (struct sockaddr *) &rt_ptr->rt_data_sun, sizeof(struct sockaddr_un)) < 0) {
		rcode = -errno;
		fprintf(stderr, "connect_to_switch(%s) : data bind failed, "
		       "errno = %d\n", rt_ptr->rt_name, rcode);
		goto out_close;
	}

	rt_ptr->rt_req.magic 	= SWITCH_MAGIC;
	rt_ptr->rt_req.version = SWITCH_VERSION;
	rt_ptr->rt_req.type 	= REQ_NEW_CONTROL;
	rt_ptr->rt_req.sock 	= &rt_ptr->rt_data_sun;
	
	n = write(rt_ptr->rt_ctrl_fd, &rt_ptr->rt_req, sizeof(struct request_v3));
	if (n != sizeof(struct request_v3)) {
		fprintf(stderr, "connect_to_switch(%s) : control setup request "
		       "failed, rcode = %d\n", rt_ptr->rt_name, errno);
		rcode = -ENOTCONN;
		goto out_free;
	}

	n = read(rt_ptr->rt_ctrl_fd, sun, sizeof(*sun));
	if (n != sizeof(*sun)) {
		fprintf(stderr, "connect_to_switch(%s) : read of data socket failed, "
		       "rcode = %d\n", rt_ptr->rt_name, errno);
		rcode = -ENOTCONN;
		goto out_free;
	}

	rt_ptr->rt_data_sun = sun;
	return rt_ptr->rt_data_fd;

 out_free:
 out_close:
	close(rt_ptr->rt_data_fd);
 out:
	close(rt_ptr->rt_ctrl_fd);
	return rcode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////// 
#ifdef ANULADO 
////////////////////////////////////////////////////////////////////////////////////////////////////// 

static struct sockaddr_un *new_addr(void *name, int len)
{
	struct sockaddr_un *sun;

	USRDEBUG("name=%d len=%d\n", name, len );

	sun = malloc(sizeof(struct sockaddr_un));
	if (sun == NULL) {
		fprintf(stderr, "new_addr: allocation of sockaddr_un "
		       "failed\n");
		return NULL;
	}
	sun->sun_family = AF_UNIX;
	memcpy(sun->sun_path, name, len);
	USRDEBUG("sun_path=%s \n", sun->sun_path);
	if (remove(sun->sun_path) == -1 && errno != ENOENT) {
		fprintf(stderr, "new_addr: remove %s failed with errno=%d\n", 
			sun->sun_path, errno);
		return(NULL);
	}
	return sun;
}

int daemon_user_init(rmttap_t *rt_ptr)
{
	struct daemon_data *pri;
	struct timeval tv;
	struct {
		char zero;
		int pid;
		int usecs;
	} name;

	USRDEBUG("rt_name=%s\n", rt_ptr->rt_name);

	pri = &rt_ptr->rt_dd;

	if (!strcmp(pri->sock_type, "unix")) {
		pri->ctrl_addr = new_addr(pri->ctrl_sock,
							strlen(pri->ctrl_sock) + 1);
	}
	
	pri->local_addr = new_addr(&name, sizeof(name));
	pri->dev = (void *)rt_ptr;
	pri->fd = connect_to_switch(rt_ptr);
	if (pri->fd < 0) {
		free(pri->local_addr);
		pri->local_addr = NULL;
		return pri->fd;
	}
	return 0;
}

#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include "daemon.h"
#include <net_user.h>
#include <os.h>
#include <um_malloc.h>

enum request_type { REQ_NEW_CONTROL };

#define SWITCH_MAGIC 0xfeedface

struct request_v3 {
	uint32_t magic;
	uint32_t version;
	enum request_type type;
	struct sockaddr_un sock;
};

static struct sockaddr_un *new_addr(void *name, int len)
{
	struct sockaddr_un *sun;

	USRDEBUG("name=%s, len=%d\n", name, len);

	sun = uml_kmalloc(sizeof(struct sockaddr_un), UM_GFP_KERNEL);
	if (sun == NULL) {
		fprintf(stderr, "new_addr: allocation of sockaddr_un "
		       "failed\n");
		return NULL;
	}
	sun->sun_family = AF_UNIX;
	memcpy(sun->sun_path, name, len);
	return sun;
}

static int connect_to_switch(struct daemon_data *pri)
{
	struct sockaddr_un *ctrl_addr = pri->ctrl_addr;
	struct sockaddr_un *local_addr = pri->local_addr;
	struct sockaddr_un *sun;
	struct request_v3 req;
	int fd, n, rcode;

	USRDEBUG("\n");

	pri->control = socket(AF_UNIX, SOCK_STREAM, 0);
	if (pri->control < 0) {
		rcode = -errno;
		fprintf(stderr, "daemon_open : control socket failed, "
		       "errno = %d\n", -rcode);
		return rcode;
	}

	if (connect(pri->control, (struct sockaddr *) ctrl_addr,
		   sizeof(*ctrl_addr)) < 0) {
		rcode = -errno;
		fprintf(stderr, "daemon_open : control connect failed, "
		       "errno = %d\n", -rcode);
		goto out;
	}

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) {
		rcode = -errno;
		fprintf(stderr, "daemon_open : data socket failed, "
		       "errno = %d\n", -rcode);
		goto out;
	}
	
	USRDEBUG("fd=%d, local_addr=%s\n", fd, local_addr);
	if (bind(fd, (struct sockaddr *) local_addr, sizeof(*local_addr)) < 0) {
		rcode = -errno;
		fprintf(stderr, "daemon_open : data bind failed, "
		       "errno = %d\n", -rcode);
		goto out_close;
	}

	sun = uml_kmalloc(sizeof(struct sockaddr_un), UM_GFP_KERNEL);
	if (sun == NULL) {
		fprintf(stderr, "new_addr: allocation of sockaddr_un "
		       "failed\n");
		rcode = -ENOMEM;
		goto out_close;
	}

	req.magic = SWITCH_MAGIC;
	req.version = SWITCH_VERSION;
	req.type = REQ_NEW_CONTROL;
	req.sock = *local_addr;
	n = write(pri->control, &req, sizeof(req));
	if (n != sizeof(req)) {
		fprintf(stderr, "daemon_open : control setup request "
		       "failed, rcode = %d\n", -errno);
		rcode = -ENOTCONN;
		goto out_free;
	}

	n = read(pri->control, sun, sizeof(*sun));
	if (n != sizeof(*sun)) {
		fprintf(stderr, "daemon_open : read of data socket failed, "
		       "rcode = %d\n", -errno);
		rcode = -ENOTCONN;
		goto out_free;
	}

	pri->data_addr = sun;
	return fd;

 out_free:
	kfree(sun);
 out_close:
	close(fd);
 out:
	close(pri->control);
	return rcode;
}

static int daemon_user_init(void *data, void *dev)
{
	struct daemon_data *pri = data;
	struct timeval tv;
	struct {
		char zero;
		int pid;
		int usecs;
	} name;

	USRDEBUG("\n");

	if (!strcmp(pri->sock_type, "unix"))
		pri->ctrl_addr = new_addr(pri->ctrl_sock,
					 strlen(pri->ctrl_sock) + 1);
	name.zero = 0;
	name.pid = os_getpid();
	gettimeofday(&tv, NULL);
	name.usecs = tv.tv_usec;
	pri->local_addr = new_addr(&name, sizeof(name));
	USRDEBUG("name=%s \n", name);

	pri->dev = dev;
	pri->fd = connect_to_switch(pri);
	if (pri->fd < 0) {
		kfree(pri->local_addr);
		pri->local_addr = NULL;
		return pri->fd;
	}

	return 0;
}

static int daemon_open(void *data)
{
	struct daemon_data *pri = data;
	return pri->fd;
}

static void daemon_remove(void *data)
{
	struct daemon_data *pri = data;

	close(pri->fd);
	pri->fd = -1;
	close(pri->control);
	pri->control = -1;

	kfree(pri->data_addr);
	pri->data_addr = NULL;
	kfree(pri->ctrl_addr);
	pri->ctrl_addr = NULL;
	kfree(pri->local_addr);
	pri->local_addr = NULL;
}

int daemon_user_write(int fd, void *buf, int len, struct daemon_data *pri)
{
	struct sockaddr_un *data_addr = pri->data_addr;

	return net_sendto(fd, buf, len, data_addr, sizeof(*data_addr));
}

const struct net_user_info daemon_user_info = {
	.init		= daemon_user_init,
	.open		= daemon_open,
	.close	 	= NULL,
	.remove	 	= daemon_remove,
	.add_address	= NULL,
	.delete_address = NULL,
	.mtu		= ETH_MAX_PACKET,
	.max_packet	= ETH_MAX_PACKET + ETH_HEADER_OTHER,
};

#endif // ANULADO