/* Copyright 2001, 2002 Jeff Dike and others
 * Licensed under the GPL
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <unistd.h>
#include "switch.h"
#include "port.h"
#include "hash.h"
#ifdef TUNTAP
#include "tuntap.h"
#endif

#ifdef notdef
#include <stddef.h>
#endif

enum request_type { REQ_NEW_CONTROL };

struct request_v0 {
  enum request_type type;
  union {
    struct {
      unsigned char addr[ETH_ALEN];
      struct sockaddr_un name;
    } new_control;
  } u;
};

#define SWITCH_MAGIC 0xfeedface
#define SWITCH_VERSION 3

struct request_v1 {
  uint32_t magic;
  enum request_type type;
  union {
    struct {
      unsigned char addr[ETH_ALEN];
      struct sockaddr_un name;
    } new_control;
  } u;
};

struct request_v2 {
  uint32_t magic;
  uint32_t version;
  enum request_type type;
  struct sockaddr_un sock;
};

struct reply_v2 {
  unsigned char mac[ETH_ALEN];
  struct sockaddr_un sock;
};

struct request_v3 {
  uint32_t magic;
  uint32_t version;
  enum request_type type;
  struct sockaddr_un sock;
};

union request {
  struct request_v0 v0;
  struct request_v1 v1;
  struct request_v2 v2;
  struct request_v3 v3;
};

static char *ctl_path 	= "/tmp/uml.ctl";
static char *local_path = "/tmp/uml.local";
static struct sockaddr_un ctl_sun;
static struct sockaddr_un data_sun;

int main(int argc, char *argv[])
{
	struct request_v3 req;
	int fd_ctl, fd_data;
	int rcode, n;

	fd_ctl = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd_ctl < 0) {
		rcode = -errno;
		fprintf(stderr, "connect_to_switch: control socket failed, "
		       "errno = %d\n", -rcode);
		exit(rcode);
	}
	
	// se conecta con el socket de CONTROL del Switch 
	ctl_sun.sun_family = AF_UNIX;
	strncpy(ctl_sun.sun_path, ctl_path, strlen(ctl_path));
	printf("CTRL sun_path=%s \n", ctl_sun.sun_path);
	if (connect(fd_ctl, (struct sockaddr *) &ctl_sun, sizeof(struct sockaddr_un)) < 0) {
		rcode = -errno;
		fprintf(stderr,"connect_to_switch: control connect failed address, "
		       "errno = %d\n", -rcode);
		goto out;
	}

	// crea el socket de Datos local 
	fd_data = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd_data < 0) {
		rcode = -errno;
		fprintf(stderr, "connect_to_switch: data socket failed, "
		       "errno = %d\n", -rcode);
		goto out;
	}
	
	// hace el bind . - ME PARECE que el PATH no importa 
	data_sun.sun_family = AF_UNIX;
	strncpy(data_sun.sun_path, local_path, strlen(local_path));
	printf("DATA sun_path=%s \n", data_sun.sun_path);
	rcode = remove(data_sun.sun_path);
	if (bind(fd_data, (struct sockaddr *) &data_sun, sizeof(struct sockaddr_un)) < 0) {
		rcode = -errno;
		fprintf(stderr, "connect_to_switch: data bind failed, "
		       "errno = %d\n",  -rcode);
		goto out_close;
	}

	// Envia al SWITCH 
	req.magic 	= SWITCH_MAGIC;
	req.version = SWITCH_VERSION;
	req.type 	= REQ_NEW_CONTROL;
	req.sock 	= data_sun;
	
	n = write(fd_ctl, &req, sizeof(req));
	if (n != sizeof(req)) {
		fprintf(stderr, "connect_to_switch: control setup request "
		       "failed, rcode = %d\n", -errno);
		rcode = -ENOTCONN;
		goto out_free;
	}

	n = read(fd_ctl, &data_sun, sizeof(struct sockaddr_un));
	if (n != sizeof(struct sockaddr_un)) {
		fprintf(stderr, "connect_to_switch: read of data socket failed, "
		       "rcode = %d\n", -errno);
		rcode = -ENOTCONN;
		goto out_free;
	}

	printf("fd_data=%d\n",fd_data);
	printf("NEW DATA sun_path=%s \n", data_sun.sun_path);
	
  struct name_s {
    char zero;
    int pid;
    int usecs;
  } ;
  typedef struct name_s name_t;
  
	name_t  *n_ptr;
	n_ptr = data_sun.sun_path;
	printf("pid=%d usec=%d\n", n_ptr->pid, n_ptr->usecs);
	
	printf("sleep for a minute\n");
	sleep(60);
	
 out_free:
 out_close:
	close(fd_data);
 out:
	close(fd_ctl);
	printf("rcode=%d\n",rcode);
	exit(rcode);
}
