/* 
 * Copyright (C) 2000, 2001, 2002 Jeff Dike (jdike@karaya.com)
 * Copyright (C) 2001 Ridgerun,Inc (glonnon@ridgerun.com)
 * Licensed under the GPL
 */

// WARNING the configuration setting CONFIG_UML_RDISK is not seeing here!!!!!!!
// #ifdef CONFIG_UML_RDISK 

#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <endian.h>
#include <byteswap.h>

#include <os.h>
 
#define  CONFIG_UML_DVK
#define  DVS_USERSPACE
#include "um_dvk.h"
#include "uml_rdisk.h"
#include "glo_dvk.h"
#include "usr_dvk.h"

extern int umlkrn_fd;

int start_rd_thread(unsigned long sp, int *fd_out)
{
	int pid, fds[2], err;

	printk("start_rd_thread - sp=%ld\n", sp);

	// socketpair - create a pair of connected sockets fds[0] , fds[1]  
	err = os_pipe(fds, 1, 1); // first 1 means stream => SOCK_STREAM (AF_UNIX)
								// second 1 means close_on_exec = YES
	if(err < 0){
		printk("start_rd_thread - os_pipe failed, err = %d\n", -err);
		goto rd_out;
	}

	// Kernel socket FD 
	umlkrn_fd = fds[0];
	// RDISK thread socket FD rd_thread_fd
	*fd_out = fds[1];

	// set the FD as Non-blocking 
	err = os_set_fd_block(*fd_out, 0);
	if (err) {
		printk("start_rd_thread - failed to set nonblocking I/O.\n");
		goto rd_out_close;
	}

	// create rd_thread 
	pid = clone(rd_thread, (void *) sp, CLONE_FILES | CLONE_VM, NULL);
	printk("start_rd_thread pid=%d\n", pid);
	if(pid < 0){
		err = -errno;
		printk("start_rd_thread - clone failed : errno = %d\n", errno);
		goto rd_out_close;
	}
// ATENCION, ESTO ES PARA "SINCRONIZAR"	
sleep(2);
	return(pid);

 rd_out_close:
	os_close_file(fds[0]);
	os_close_file(fds[1]);
	umlkrn_fd = -1;
	*fd_out = -1;
 rd_out:
	return err;
}
//#endif // CONFIG_UML_RDISK 



