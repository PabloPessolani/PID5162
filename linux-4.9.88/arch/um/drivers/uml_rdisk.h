/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Copyright (C) 2001 RidgeRun, Inc (glonnon@ridgerun.com)
 * Licensed under the GPL
 */

#ifndef __UM_RDISK_USER_H
#define __UM_RDISK_USER_H

extern int start_rd_thread(unsigned long sp, int *fds_out);
extern int rd_thread(void *arg);
extern int kernel_fd;

#define 	RD_MAJOR		3
#define 	RD_MINOR		0
#endif // __UM_RDISK_USER_H

