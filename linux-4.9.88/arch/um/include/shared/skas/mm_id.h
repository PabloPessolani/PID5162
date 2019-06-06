/*
 * Copyright (C) 2005 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#ifndef __MM_ID_H
#define __MM_ID_H

struct mm_id {
	union {
		int mm_fd;
		int pid;
	} u;
	unsigned long stack;
//#ifdef CONFIG_UML_DVK
	int  mm_dvk_fd;
//#endif // CONFIG_UML_DVK

};

#endif
