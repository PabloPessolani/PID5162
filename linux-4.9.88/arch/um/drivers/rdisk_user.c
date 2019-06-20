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
 

#include "um_dvk.h"
#include "uml_rdisk.h"
#include "glo_dvk.h"

extern int rdk_fd;

int start_rd_thread(unsigned long sp, int *fd_out)
{
	int pid, fds[2], err;

	err = os_pipe(fds, 1, 1);
	if(err < 0){
		printk("start_rd_thread - os_pipe failed, err = %d\n", -err);
		goto rd_out;
	}

	rdk_fd = fds[0];
	*fd_out = fds[1];

	err = os_set_fd_block(*fd_out, 0);
	if (err) {
		printk("start_rd_thread - failed to set nonblocking I/O.\n");
		goto rd_out_close;
	}

	pid = clone(rd_thread, (void *) sp, CLONE_FILES | CLONE_VM, NULL);
	if(pid < 0){
		err = -errno;
		printk("start_rd_thread - clone failed : errno = %d\n", errno);
		goto rd_out_close;
	}

	return(pid);

 rd_out_close:
	os_close_file(fds[0]);
	os_close_file(fds[1]);
	rdk_fd = -1;
	*fd_out = -1;
 rd_out:
	return err;
}
//#endif // CONFIG_UML_RDISK 



