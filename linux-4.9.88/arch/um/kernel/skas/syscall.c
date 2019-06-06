/*
 * Copyright (C) 2002 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#include <linux/kernel.h>
#include <linux/ptrace.h>
#include <linux/seccomp.h>
#include <kern_util.h>
#include <sysdep/ptrace.h>
#include <sysdep/ptrace_user.h>
#include <sysdep/syscalls.h>
#include <shared/os.h>

#include <asm/unistd.h>
#include <as-layout.h>
//#include <ptrace_user.h>
//#include <stub-data.h>
//#include <sysdep/stub.h>

#ifdef 	ANULADO
extern int dvk_fd; 
static inline long stub_syscall0(long syscall)
{
	long ret;

	__asm__ volatile ("int $0x80" : "=a" (ret) : "0" (syscall));

	return ret;
}

static inline long stub_syscall3(long syscall, long arg1, long arg2, long arg3)
{
	long ret;

	__asm__ volatile ("int $0x80" : "=a" (ret) : "0" (syscall), "b" (arg1),
			"c" (arg2), "d" (arg3));

	return ret;
}
#endif // 	ANULADO

void handle_syscall(struct uml_pt_regs *r)
{
	struct pt_regs *regs = container_of(r, struct pt_regs, regs);
	int syscall;

#ifdef 	ANULADO
	static int pid, vpid;
	static int cmd, arg, rcode;
	if( vpid != get_current_pid()) {
		pid = os_getpid();
		vpid = get_current_pid();
	}
#endif // 	ANULADO
	
	/* Initialize the syscall number and default return value. */
	UPT_SYSCALL_NR(r) = PT_SYSCALL_NR(r->gp);
	PT_REGS_SET_SYSCALL_RETURN(regs, -ENOSYS);

	if (syscall_trace_enter(regs))
		goto out;

	/* Do the seccomp check after ptrace; failures should be fast. */
	if (secure_computing(NULL) == -1)
		goto out;

	syscall = UPT_SYSCALL_NR(r);

#ifdef 	ANULADO
	// Este funcion handle_syscall se ejecutan en el PROCESO DEL UML_KERNEL, no de un proceso de USUARIO
	// Esto que viene a continauaciòn NO ES UTIL !!!! 
	if( syscall == __NR_ioctl){
		pid = stub_syscall0(__NR_getpid);
		if( dvk_fd >= 0) {
			if( dvk_fd ==  UPT_SYSCALL_ARG1(r) ) {
        		cmd 	=  UPT_SYSCALL_ARG2(r);
				arg   	=   UPT_SYSCALL_ARG3(r);
				printk("UML DVK CALL pid=%d dvk_fd=%d cmd=%X\n", pid, dvk_fd, cmd);
				rcode = stub_syscall3(__NR_ioctl , dvk_fd, cmd,  arg );
				printk("UML DVK CALL rcode=%d\n", rcode);
				PT_REGS_SET_SYSCALL_RETURN(regs,rcode);
			}
		}
	}
#endif // 	ANULADO
	
	if (syscall >= 0 && syscall <= __NR_syscall_max)
		PT_REGS_SET_SYSCALL_RETURN(regs,
				EXECUTE_SYSCALL(syscall, regs));

out:
	syscall_trace_leave(regs);
}
