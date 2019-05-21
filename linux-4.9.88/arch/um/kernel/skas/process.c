/*
 * Copyright (C) 2002 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <as-layout.h>
#include <kern.h>
#include <os.h>
#include <skas.h>

#ifdef CONFIG_UML_DVK
#include "../../drivers/um_dvk.h"
extern int dvk_fd;
#endif // CONFIG_UML_DVK

extern void start_kernel(void);


static int __init start_kernel_proc(void *unused)
{
	int pid, tid;

	block_signals();
	
#ifdef CONFIG_UML_DVK
	pid = os_getpid();
	tid = os_gettid();
	printf("UML-kernel PID=%d  UML-kernel TID=%d\n", pid, tid);
	DVKDEBUG(INTERNAL, "UML-kernel PID=%d  UML-kernel TID=%d\n", pid, tid);
	os_flush_stdout();
	
	dvk_fd = (-1);
	dvk_fd = os_open_file(UML_DVK_DEV, of_set_rw(OPENFLAGS(), 1, 1), 0);
	if ( dvk_fd < 0){
		ERROR_PRINT(dvk_fd);
	}else{
		DVKDEBUG(INTERNAL, "DVK device file successfully opened!! dvk_fd=%d\n", dvk_fd);
		printf("DVK device file successfully opened!! dvk_fd=%d\n", dvk_fd);
	}
	os_flush_stdout();
#endif // CONFIG_UML_DVK
		
	cpu_tasks[0].pid = pid;
	cpu_tasks[0].task = current;

	start_kernel();
	return 0;
}

extern int userspace_pid[];

extern char cpu0_irqstack[];

int __init start_uml(void)
{
	stack_protections((unsigned long) &cpu0_irqstack);
	set_sigstack(cpu0_irqstack, THREAD_SIZE);

	init_new_thread_signals();

	init_task.thread.request.u.thread.proc = start_kernel_proc;
	init_task.thread.request.u.thread.arg = NULL;


	return start_idle_thread(task_stack_page(&init_task),
				 &init_task.thread.switch_buf);
}

unsigned long current_stub_stack(void)
{
	if (current->mm == NULL)
		return 0;

	return current->mm->context.id.stack;
}
