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

/*
  42 block	Demo/sample use

		This number is intended for use in sample code, as
		well as a general "example" device number.  It
		should never be used for a device driver that is being
		distributed; either obtain an official number or use
		the local/experimental range.  The sudden addition or
		removal of a driver with this number should not cause
		ill effects to the system (bugs excepted.)
*/ 		
#define 	RD_MAJOR		42
#define 	RD_MINOR		0
#endif // __UM_RDISK_USER_H

