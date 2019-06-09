/*
 * Copyright (C) 2002 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <init.h>
#include <as-layout.h>
#include <mm_id.h>
#include <os.h>
#include <ptrace_user.h>
#include <registers.h>
#include <skas.h>
#include <sysdep/ptrace.h>
#include <sysdep/stub.h>

#ifdef CONFIG_UML_DVK_NULO
#include "../../drivers/um_dvk.h"
#include "../../drivers/glo_dvk.h"

int do_stub_open(struct mm_id *mm_idp, char *dvk_dev, int open_flags, mode_t open_mode)
{
	long ret;
	void *addr; 
	
	printk("do_stub_open: dvk_dev = %s\n", dvk_dev);
	
	unsigned long args[] = { 
				dvk_dev, 
				open_flags,
				open_mode,
				0,
				0,
				0};
			 
	ret = run_syscall_stub(mm_idp, __NR_open, args, 0, &addr, 1);

	return ret;
}

int do_stub_ioctl(int fd, int cmd , void *parms)
{
	long ret = 0;
	void *addr; 

//	struct mm_id *mm_idp =  &current->mm->context.id;

	printk("do_stub_ioctl: fd=%d cmd=%X\n", fd, cmd);
#ifdef ANULADO	
	unsigned long args[] = { 
				fd, 
				cmd,
				parms,
				0,
				0,
				0};
			 
	ret = run_syscall_stub(mm_idp, __NR_ioctl, args, 0, &addr, 1);
	printk("do_stub_ioctl: ret=%d\n", ret);
#endif // ANULADO	

	return ret;
}
#endif // CONFIG_UML_DVK_NULO





