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

#ifdef 	CONFIG_UML_DVK

#include "/usr/src/dvs/include/com/dvk_calls.h"
//#include "/usr/src/dvs/include/com/dvk_ioctl.h"
#define  UML_DVK_DEV		"/dev/dvk"
#define  UML_DVK_NAMELEN	8


extern int userspace_pid[];
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

static inline long stub_syscall4(long syscall, long arg1, long arg2, long arg3,	 long arg4)
{
	long ret;

	__asm__ volatile ("int $0x80" : "=a" (ret) : "0" (syscall), "b" (arg1),
			"c" (arg2), "d" (arg3), "S" (arg4));

	return ret;
}

#endif // 	CONFIG_UML_DVK

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

#ifdef CONFIG_UML_DVK_NULO
		int lpid, fd, user, cmd, sg, err, rcode, len, tmp_fd, pid;
		char *open_path;
		char *tmp_path = "/dev/xxxxxxxxx";
		int open_flags;
		mode_t open_mode;
		void *args;
			
		if( syscall == __NR_ioctl){ // DETECT IOCTL //////////////////////////
			//  int ioctl(int fd, unsigned long cmd, void *args);
			fd 		= UPT_SYSCALL_ARG1(r);
			cmd 	= UPT_SYSCALL_ARG2(r);
			args 	= UPT_SYSCALL_ARG3(r);
			lpid    = stub_syscall0(__NR_getpid);
			// AQUI SE DEBERIA COMPARAR EL FD DEL SYSCALL CON EL FD DEL DVK ASIGNADO AL PROCESO 
//					printk("DVK userspace __NR_ioctl tracee_lpid=%d tracer_lpid=%d fd=%d cmd=%X DVK_IOCSDVKBIND=%X\n", 
//							pid, lpid, fd, cmd, DVK_IOCSDVKBIND);	
			if( (cmd == DVK_IOCSDVKBIND) ||  (cmd == DVK_IOCQGETEP)){
				printk("DVK userspace __NR_ioctl fd=%d cmd=%X\n", fd, cmd);
				pid = userspace_pid[0];
//				interrupt_end();
//				err = ptrace(PTRACE_SYSCALL, pid, 0, 0);
				err = stub_syscall4(__NR_ptrace, PTRACE_SYSCALL, pid, 0, 0);
				goto out;
			}
		} else if( syscall == __NR_open ){ // DETECT OPEN ////////////////////////// 
			// int open(const char *pathname, int flags, mode_t mode);
			len =  strlen(UML_DVK_DEV);
			open_path	= UPT_SYSCALL_ARG1(r);
			open_flags  = UPT_SYSCALL_ARG2(r);
			open_mode   = UPT_SYSCALL_ARG3(r);
			memset(tmp_path, 0, len+1);
			rcode = copy_from_user_proc(tmp_path, open_path, len);
			if ( !strcmp(tmp_path, UML_DVK_DEV)) { // string MATCH!
				lpid    = stub_syscall0(__NR_getpid);
				printk("DVK userspace MATCH __NR_open user=%d tracee_lpid=%d tracer_lpid=%d tmp_path=%s flags=%X mode=%X\n", 
				user, pid, lpid, tmp_path, open_flags, open_mode);	
				pid = userspace_pid[0];
//				interrupt_end();
//				err = ptrace(PTRACE_SYSCALL, pid, 0, 0);
				err = stub_syscall4(__NR_ptrace, PTRACE_SYSCALL, pid, 0, 0);
				goto out;
			} 
		}			
#endif // 	CONFIG_UML_DVK_NULO
	

#ifdef 	ANULADO
	// Este funcion handle_syscall se ejecutan en el PROCESO DEL UML_KERNEL, no de un proceso de USUARIO
	// Esto que viene a continauaciòn NO ES UTIL !!!! 
	static int pid, fd;
	static int cmd, arg, rcode;
	if( syscall == __NR_ioctl){
		fd 		=  UPT_SYSCALL_ARG1(r);
		cmd 	=  UPT_SYSCALL_ARG2(r);
		arg   	=  UPT_SYSCALL_ARG3(r);
		printk("handle_syscall __NR_ioctl pid=%d fd=%d cmd=%X\n", pid, fd, cmd);
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
	
#ifdef 	ANULADO
	if( syscall == __NR_open){
		printk("handle_syscall pid=%d syscall=%d __NR_open=%d\n", pid, syscall, __NR_open);
	}	
#endif // 	ANULADO
	
	if (syscall >= 0 && syscall <= __NR_syscall_max)
		PT_REGS_SET_SYSCALL_RETURN(regs,
				EXECUTE_SYSCALL(syscall, regs));
#ifdef 	CONFIG_UML_DVK
	else {
		printk("syscall=%d __NR_syscall_max=%d\n", syscall, __NR_syscall_max);
	}	
#endif // 	CONFIG_UML_DVK

	
out:
	syscall_trace_leave(regs);
}

#ifdef CONFIG_UML_DVK
void return_from_dvkcall(int dvk_retcode,  struct uml_pt_regs *r)
{
	struct pt_regs *regs = container_of(r, struct pt_regs, regs);
	PT_REGS_SET_SYSCALL_RETURN(regs, dvk_retcode);
}

#endif // CONFIG_UML_DVK


