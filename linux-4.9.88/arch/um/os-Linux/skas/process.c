/*
 * Copyright (C) 2015 Thomas Meyer (thomas@m3y3r.de)
 * Copyright (C) 2002- 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <asm/unistd.h>
#include <as-layout.h>
#include <init.h>
#include <kern_util.h>
#include <mem.h>
#include <os.h>
#include <ptrace_user.h>
#include <registers.h>
#include <skas.h>
#include <sysdep/stub.h>
#include <sys/reg.h>
#include <sys/types.h>
#include <asm/ptrace-abi.h>
#include <asm/ptrace.h>

#define  CONFIG_UML_DVK		1

#ifdef CONFIG_UML_DVK
#include <signal.h>

#define DVS_USERSPACE 1

#include "/usr/src/dvs/include/com/dvs_config.h"
#include "/usr/src/dvs/include/com/config.h"
#include "/usr/src/dvs/include/com/const.h"
#include "/usr/src/dvs/include/com/com.h"
#include "/usr/src/dvs/include/com/cmd.h"
#include "/usr/src/dvs/include/com/proc_sts.h"
#include "/usr/src/dvs/include/com/proc_usr.h"
#include "/usr/src/dvs/include/com/proxy_sts.h"
#include "/usr/src/dvs/include/com/proxy_usr.h"
#include "/usr/src/dvs/include/com/dc_usr.h"
#include "/usr/src/dvs/include/com/node_usr.h"
#include "/usr/src/dvs/include/com/priv_usr.h"
#include "/usr/src/dvs/include/com/dvs_usr.h"
#include "/usr/src/dvs/include/com/dvk_calls.h"
#include "/usr/src/dvs/include/com/dvk_ioctl.h"
#include "/usr/src/dvs/include/com/dvs_errno.h"
#include "/usr/src/dvs/include/com/ipc.h"
#include "/usr/src/dvs/include/dvk/dvk_ioparm.h"
#include "/usr/src/dvs/include/generic/tracker.h"
#include "/usr/src/dvs/include/com/stub_dvkcall.h"
#include "/usr/src/dvs/dvk-mod/dvk_debug.h"
#include "/usr/src/dvs/dvk-mod/dvk_macros.h"

#define DEVICE_NAME "dvk"
#define  UML_DVK_DEV		"/dev/dvk"
#define  UML_DVK_NAMELEN	8
#endif // CONFIG_UML_DVK


int is_skas_winch(int pid, int fd, void *data)
{
	return pid == getpgrp();
}

static int ptrace_dump_regs(int pid)
{
	unsigned long regs[MAX_REG_NR];
	int i;

	if (ptrace(PTRACE_GETREGS, pid, 0, regs) < 0)
		return -errno;

	printk(UM_KERN_ERR "Stub registers -\n");
	for (i = 0; i < ARRAY_SIZE(regs); i++)
		printk(UM_KERN_ERR "\t%d - %lx\n", i, regs[i]);

	return 0;
}

/*
 * Signals that are OK to receive in the stub - we'll just continue it.
 * SIGWINCH will happen when UML is inside a detached screen.
 */
#define STUB_SIG_MASK ((1 << SIGALRM) | (1 << SIGWINCH))

/* Signals that the stub will finish with - anything else is an error */
#define STUB_DONE_MASK (1 << SIGTRAP)

void wait_stub_done(int pid)
{
	int n, status, err;

	while (1) {
		CATCH_EINTR(n = waitpid(pid, &status, WUNTRACED | __WALL));
		if ((n < 0) || !WIFSTOPPED(status))
			goto bad_wait;

		if (((1 << WSTOPSIG(status)) & STUB_SIG_MASK) == 0)
			break;

		err = ptrace(PTRACE_CONT, pid, 0, 0);
		if (err) {
			printk(UM_KERN_ERR "wait_stub_done : continue failed, "
			       "errno = %d\n", errno);
			fatal_sigsegv();
		}
	}

	if (((1 << WSTOPSIG(status)) & STUB_DONE_MASK) != 0)
		return;

bad_wait:
	err = ptrace_dump_regs(pid);
	if (err)
		printk(UM_KERN_ERR "Failed to get registers from stub, "
		       "errno = %d\n", -err);
	printk(UM_KERN_ERR "wait_stub_done : failed to wait for SIGTRAP, "
	       "pid = %d, n = %d, errno = %d, status = 0x%x\n", pid, n, errno,
	       status);
	fatal_sigsegv();
}

extern unsigned long current_stub_stack(void);

static void get_skas_faultinfo(int pid, struct faultinfo *fi)
{
	int err;
	unsigned long fpregs[FP_SIZE];

	err = get_fp_registers(pid, fpregs);
	if (err < 0) {
		printk(UM_KERN_ERR "save_fp_registers returned %d\n",
		       err);
		fatal_sigsegv();
	}
	err = ptrace(PTRACE_CONT, pid, 0, SIGSEGV);
	if (err) {
		printk(UM_KERN_ERR "Failed to continue stub, pid = %d, "
		       "errno = %d\n", pid, errno);
		fatal_sigsegv();
	}
	wait_stub_done(pid);

	/*
	 * faultinfo is prepared by the stub-segv-handler at start of
	 * the stub stack page. We just have to copy it.
	 */
	memcpy(fi, (void *)current_stub_stack(), sizeof(*fi));

	err = put_fp_registers(pid, fpregs);
	if (err < 0) {
		printk(UM_KERN_ERR "put_fp_registers returned %d\n",
		       err);
		fatal_sigsegv();
	}
}

static void handle_segv(int pid, struct uml_pt_regs * regs)
{
	get_skas_faultinfo(pid, &regs->faultinfo);
	segv(regs->faultinfo, 0, 1, NULL);
}

/*
 * To use the same value of using_sysemu as the caller, ask it that value
 * (in local_using_sysemu
 */
static void handle_trap(int pid, struct uml_pt_regs *regs,
			int local_using_sysemu)
{
	int err, status;

	if ((UPT_IP(regs) >= STUB_START) && (UPT_IP(regs) < STUB_END))
		fatal_sigsegv();

	if (!local_using_sysemu)
	{
		err = ptrace(PTRACE_POKEUSER, pid, PT_SYSCALL_NR_OFFSET,
			     __NR_getpid);
		if (err < 0) {
			printk(UM_KERN_ERR "handle_trap - nullifying syscall "
			       "failed, errno = %d\n", errno);
			fatal_sigsegv();
		}

		err = ptrace(PTRACE_SYSCALL, pid, 0, 0);
		if (err < 0) {
			printk(UM_KERN_ERR "handle_trap - continuing to end of "
			       "syscall failed, errno = %d\n", errno);
			fatal_sigsegv();
		}

		CATCH_EINTR(err = waitpid(pid, &status, WUNTRACED | __WALL));
		if ((err < 0) || !WIFSTOPPED(status) ||
		    (WSTOPSIG(status) != SIGTRAP + 0x80)) {
				
			if (err < 0) {
			printk(UM_KERN_ERR "errno = %d\n", -err);					
			} else if (!WIFSTOPPED(status)){
			printk(UM_KERN_ERR "WIFSTOPPED\n");									
			} else if ( (WSTOPSIG(status) != SIGTRAP + 0x80)) {
			printk(UM_KERN_ERR "WSTOPSIG(status)=%X SIGTRAP + 0x80=%X\n", 
			WSTOPSIG(status) , SIGTRAP + 0x80);									
			}	
				
			err = ptrace_dump_regs(pid);
			if (err)
				printk(UM_KERN_ERR "Failed to get registers "
				       "from process, errno = %d\n", -err);
			printk(UM_KERN_ERR "handle_trap - failed to wait at "
			       "end of syscall, errno = %d, status = %d\n",
			       errno, status);
			fatal_sigsegv();
		}
	}

	handle_syscall(regs);
}

extern char __syscall_stub_start[];

static int userspace_tramp(void *stack)
{
	void *addr;
	int fd;
	unsigned long long offset;

	ptrace(PTRACE_TRACEME, 0, 0, 0);

	signal(SIGTERM, SIG_DFL);
	signal(SIGWINCH, SIG_IGN);

	/*
	 * This has a pte, but it can't be mapped in with the usual
	 * tlb_flush mechanism because this is part of that mechanism
	 */
	fd = phys_mapping(to_phys(__syscall_stub_start), &offset);
	addr = mmap64((void *) STUB_CODE, UM_KERN_PAGE_SIZE,
		      PROT_EXEC, MAP_FIXED | MAP_PRIVATE, fd, offset);
	if (addr == MAP_FAILED) {
		printk(UM_KERN_ERR "mapping mmap stub at 0x%lx failed, "
		       "errno = %d\n", STUB_CODE, errno);
		exit(1);
	}

	if (stack != NULL) {
		fd = phys_mapping(to_phys(stack), &offset);
		addr = mmap((void *) STUB_DATA,
			    UM_KERN_PAGE_SIZE, PROT_READ | PROT_WRITE,
			    MAP_FIXED | MAP_SHARED, fd, offset);
		if (addr == MAP_FAILED) {
			printk(UM_KERN_ERR "mapping segfault stack "
			       "at 0x%lx failed, errno = %d\n",
			       STUB_DATA, errno);
			exit(1);
		}
	}
	if (stack != NULL) {
		struct sigaction sa;

		unsigned long v = STUB_CODE +
				  (unsigned long) stub_segv_handler -
				  (unsigned long) __syscall_stub_start;

		set_sigstack((void *) STUB_DATA, UM_KERN_PAGE_SIZE);
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_ONSTACK | SA_NODEFER | SA_SIGINFO;
		sa.sa_sigaction = (void *) v;
		sa.sa_restorer = NULL;
		if (sigaction(SIGSEGV, &sa, NULL) < 0) {
			printk(UM_KERN_ERR "userspace_tramp - setting SIGSEGV "
			       "handler failed - errno = %d\n", errno);
			exit(1);
		}
	}

	kill(os_getpid(), SIGSTOP);
	return 0;
}

/* Each element set once, and only accessed by a single processor anyway */
#undef NR_CPUS
#define NR_CPUS 1
int userspace_pid[NR_CPUS];

int start_userspace(unsigned long stub_stack)
{
	void *stack;
	unsigned long sp;
	int pid, status, n, flags, err;

	stack = mmap(NULL, UM_KERN_PAGE_SIZE,
		     PROT_READ | PROT_WRITE | PROT_EXEC,
		     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (stack == MAP_FAILED) {
		err = -errno;
		printk(UM_KERN_ERR "start_userspace : mmap failed, "
		       "errno = %d\n", errno);
		return err;
	}

	sp = (unsigned long) stack + UM_KERN_PAGE_SIZE - sizeof(void *);

	flags = CLONE_FILES | SIGCHLD;

	pid = clone(userspace_tramp, (void *) sp, flags, (void *) stub_stack);
	if (pid < 0) {
		err = -errno;
		printk(UM_KERN_ERR "start_userspace : clone failed, "
		       "errno = %d\n", errno);
		return err;
	}

	do {
		CATCH_EINTR(n = waitpid(pid, &status, WUNTRACED | __WALL));
		if (n < 0) {
			err = -errno;
			printk(UM_KERN_ERR "start_userspace : wait failed, "
			       "errno = %d\n", errno);
			goto out_kill;
		}
	} while (WIFSTOPPED(status) && (WSTOPSIG(status) == SIGALRM));

	if (!WIFSTOPPED(status) || (WSTOPSIG(status) != SIGSTOP)) {
		err = -EINVAL;
		printk(UM_KERN_ERR "start_userspace : expected SIGSTOP, got "
		       "status = %d\n", status);
		goto out_kill;
	}

	if (ptrace(PTRACE_OLDSETOPTIONS, pid, NULL,
		   (void *) PTRACE_O_TRACESYSGOOD) < 0) {
		err = -errno;
		printk(UM_KERN_ERR "start_userspace : PTRACE_OLDSETOPTIONS "
		       "failed, errno = %d\n", errno);
		goto out_kill;
	}

	if (munmap(stack, UM_KERN_PAGE_SIZE) < 0) {
		err = -errno;
		printk(UM_KERN_ERR "start_userspace : munmap failed, "
		       "errno = %d\n", errno);
		goto out_kill;
	}

	return pid;

 out_kill:
	os_kill_ptraced_process(pid, 1);
	return err;
}


#ifdef CONFIG_UML_DVK

#ifdef __x86_64__
#define SC_NUMBER  (8 * ORIG_RAX)
#define SC_RETCODE (8 * RAX)
#else
#define SC_NUMBER  (4 * ORIG_EAX)
#define SC_RETCODE (4 * EAX)
#endif

void return_from_dvkcall(int dvk_retcode,  struct uml_pt_regs *r);

#define SI_INFO_FORMAT 		"si_signo=%d si_errno=%d si_code=%d si_pid=%d si_uid=%d si_syscall=%d\n"
#define SI_INFO_FIELDS(p) 	p->si_signo,p->si_errno, p->si_code, p->si_pid, p->si_uid, p->si_syscall


static void handle_dvk_wait(int pid, struct uml_pt_regs *regs)
{
	int err, status, dvk_retcode;
	siginfo_t si;
	siginfo_t *sptr;
	sigset_t ss;
	sigset_t *ss_ptr;
	
	printk("DVK handle_dvk_wait pid=%d\n", pid);
	
	if ((UPT_IP(regs) >= STUB_START) && (UPT_IP(regs) < STUB_END))
		fatal_sigsegv();
	
	ss_ptr = &ss;
	err = ptrace(PTRACE_GETSIGMASK, pid, sizeof(sigset_t), ss_ptr);
	if (err < 0) {
		printk(UM_KERN_ERR "handle_dvk_wait - PTRACE_GETSIGMASK failed, errno = %d\n", errno);
		fatal_sigsegv();
	}
	
	err = sigaddset(ss_ptr, SIGALRM);
	if (err < 0) {
		printk(UM_KERN_ERR "handle_dvk_wait - sigaddset failed, errno = %d\n", errno);
		fatal_sigsegv();
	}
	
	err = ptrace(PTRACE_SETSIGMASK, pid, sizeof(sigset_t), ss_ptr);
	if (err < 0) {
		printk(UM_KERN_ERR "handle_dvk_wait - PTRACE_SETSIGMASK failed, errno = %d\n", errno);
		fatal_sigsegv();
	}


#ifdef ANULADO

	// resumes the DVK_CALL entry  
//	err = ptrace(PTRACE_CONT, pid, 0, 0);
	err = ptrace(PTRACE_SYSCALL, pid, 0, 0);
	if (err < 0) {
		printk(UM_KERN_ERR "handle_dvk_wait - continuing to end of "
			   "syscall failed, errno = %d\n", errno);
		fatal_sigsegv();
	}

	
	///////////////////////////////////////////////////////////////////////////////////
	// FROM HERE, the USER process execute by itself the DVK_CALL, then, when it finished it 
	// it calls   raise(SIGSTOP);
	// The UML_KERNEL process remains running in the while(1) loop of userspace() function
	// There it captures the TRAP of the USER process 
	///////////////////////////////////////////////////////////////////////////////////
	
	// waits for the DVK_CALL exit  
	CATCH_EINTR(err = waitpid(pid, &status, WUNTRACED | WCONTINUED));
	if (err < 0) {
		printk(UM_KERN_ERR "errno = %d\n", -err);					
		printk(UM_KERN_ERR "handle_dvk_wait - failed to wait at "
			   "end of syscall, errno = %d, status = %d\n",
			   errno, status);
		fatal_sigsegv();
	}

   if (WIFEXITED(status)) {
		printk("exited, status=%d\n", WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		printk("killed by signal %d\n", WTERMSIG(status));
	} else if (WIFSTOPPED(status)) {
		printk("stopped by signal %d\n", WSTOPSIG(status));
	} else if (WIFCONTINUED(status)) {
		printk("continued\n");
	}
#endif // ANULADO

	// return_from_dvkcall(dvk_retcode, regs);
}

static void handle_dvk_exit(int pid, struct uml_pt_regs *regs)
{
	int err, status, dvk_retcode;
	siginfo_t si;
	siginfo_t *sptr;
	
	printk("DVK handle_dvk_exit pid=%d\n", pid);
	
	if ((UPT_IP(regs) >= STUB_START) && (UPT_IP(regs) < STUB_END))
		fatal_sigsegv();
	
	// resumes the DVK_CALL entry  
	err = ptrace(PTRACE_SYSCALL, pid, 0, 0);
	if (err < 0) {
		printk(UM_KERN_ERR "handle_dvk_exit - continuing to end of "
			   "syscall failed, errno = %d\n", errno);
		fatal_sigsegv();
	}

	need_uml_resched();
	pid = userspace_pid[0];
	
	// waits for the DVK_CALL exit  
	CATCH_EINTR(err = waitpid(pid, &status, WUNTRACED | __WALL));

    dvk_retcode = ptrace(PTRACE_PEEKUSER, pid, SC_RETCODE, NULL);
	if( dvk_retcode == (-ENOSYS))
		printk("handle_dvk_exit WARNING dvk_retcode=%d == -ENOSYS\n", dvk_retcode);
	else 
		printk("handle_dvk_exit dvk_retcode=%d\n", dvk_retcode);
		
	if ((err < 0) || !WIFSTOPPED(status) ||
		(WSTOPSIG(status) != SIGTRAP + 0x80)) {
		if (err < 0) {
			printk(UM_KERN_ERR "errno = %d\n", -err);					
		} else if (!WIFSTOPPED(status)){
			printk(UM_KERN_ERR "WIFSTOPPED\n");									
		} else if ( (WSTOPSIG(status) != SIGTRAP + 0x80)) {
			printk(UM_KERN_ERR "WSTOPSIG(status)=%X SIGTRAP + 0x80=%X\n", 
				WSTOPSIG(status) , SIGTRAP + 0x80);									
		}
		err = ptrace_dump_regs(pid);
		if (err)
			printk(UM_KERN_ERR "handle_dvk_exit Failed to get registers "
				   "from process, errno = %d\n", -err);
		printk(UM_KERN_ERR "handle_dvk_exit - failed to wait at "
			   "end of syscall, errno = %d, status = %d\n",
			   errno, status);
		fatal_sigsegv();
	}
	
#define ERESTARTSYS 	512 
	if( dvk_retcode == (-ERESTARTSYS)){
		sptr = &si;
		err = ptrace(PTRACE_GETSIGINFO, pid, 0, (struct siginfo *)sptr);
		if( err) 
			printk(UM_KERN_ERR "handle_dvk_exit Failed to get signal info err=%d\n", err);
		printk(SI_INFO_FORMAT, SI_INFO_FIELDS(sptr));
	}
	
	return_from_dvkcall(dvk_retcode, regs);
}

#define		DVK_NOCMD			0
#define		DVK_CMD_NOWAIT		1
#define		DVK_CMD_WAIT		2

int dvk_call_wait[] = {
	DVK_CMD_NOWAIT, // DVK_VOID1		   
	DVK_CMD_NOWAIT, // DVK_DCINIT		   
	DVK_CMD_WAIT, // DVK_SEND		   
	DVK_CMD_WAIT, // DVK_RECEIVE			
	DVK_CMD_WAIT, // DVK_NOTIFY		   
	DVK_CMD_WAIT, // DVK_SENDREC	 		
	DVK_CMD_WAIT, // DVK_RCVRQST			
	DVK_CMD_WAIT, // DVK_REPLY			
	DVK_CMD_NOWAIT, // DVK_DCEND			
	DVK_CMD_NOWAIT, // DVK_BIND			
	DVK_CMD_NOWAIT, // DVK_UNBIND			
	DVK_CMD_NOWAIT, // DVK_GETPRIV			
	DVK_CMD_NOWAIT, // DVK_SETPRIV			
	DVK_CMD_WAIT, // DVK_VCOPY			
	DVK_CMD_NOWAIT, // DVK_GETDCINFO		
	DVK_CMD_NOWAIT, // DVK_GETPROCINFO		
	DVK_CMD_WAIT, // DVK_RELAY			
	DVK_CMD_NOWAIT, // DVK_PROXYBIND		
	DVK_CMD_NOWAIT, // DVK_PROXYUNBIND		
	DVK_CMD_NOWAIT, // DVK_GETNODEINFO		
	DVK_CMD_NOWAIT, // DVK_PUT2LCL			
	DVK_CMD_WAIT, // DVK_GET2RMT			
	DVK_CMD_NOWAIT, // DVK_ADDNODE			
	DVK_CMD_NOWAIT, // DVK_DELNODE			
	DVK_CMD_NOWAIT, // DVK_DVSINIT			
	DVK_CMD_NOWAIT, // DVK_DVSEND			
	DVK_CMD_NOWAIT, // DVK_GETEP			
	DVK_CMD_NOWAIT, // DVK_GETDVSINFO		
	DVK_CMD_NOWAIT, // DVK_PROXYCONN		
	DVK_CMD_WAIT, // DVK_WAIT4BIND		
	DVK_CMD_WAIT, // DVK_MIGRATE			
	DVK_CMD_NOWAIT, // DVK_NODEUP			
	DVK_CMD_NOWAIT, // DVK_NODEDOWN		
	DVK_CMD_NOWAIT, // DVK_GETPROXYINFO	
	DVK_CMD_NOWAIT, // DVK_WAKEUPEP		
};

int handle_dvk_enter(int pid, struct uml_pt_regs *r)
{
	int syscall;
	extern int dvk_fd; 
	int lpid, fd, cmd, rcode, tmp_pid, tmp_cmd;
	int dvk_call;
	char *open_path;
static	char *tmp_path = "01234567890123456789";
	int open_flags;
	mode_t open_mode;
	void *args;
//	static int count;
	long *tmp_ptr;
	
	/* Initialize the syscall number and default return value. */
//	UPT_SYSCALL_NR(r) = PT_SYSCALL_NR(r->gp);
//	syscall = UPT_SYSCALL_NR(r);
	syscall = ptrace(PTRACE_PEEKUSER, pid, sizeof(long)*ORIG_EAX, 0);
	 if( syscall == __NR_ioctl){ 	// DETECT IOCTL //////////////////////////
		//  int ioctl(int fd, unsigned long cmd, void *args);
		fd 		= UPT_SYSCALL_ARG1(r);
		cmd 	= UPT_SYSCALL_ARG2(r);
		args 	= UPT_SYSCALL_ARG3(r);
//		lpid    = stub_syscall0(__NR_getpid);
//					if( (cmd == DVK_IOCSDVKBIND) ||  (cmd == DVK_IOCQGETEP)){
		// AQUI SE DEBERIA COMPARAR EL FD DEL SYSCALL CON EL FD DEL DVK ASIGNADO AL PROCESO 
//					printk("DVK userspace __NR_ioctl tracee_lpid=%d tracer_lpid=%d fd=%d cmd=%X DVK_IOCSDVKBIND=%X\n", 
//							pid, lpid, fd, cmd, DVK_IOCSDVKBIND);
		if (cmd == DVK_IOCQGETEP) {
			printk("DVK handle_dvk_enter DVK_IOCQGETEP BEFORE fd=%d cmd=%X args=%d\n", fd, cmd, args);		
			// change the UML_USER PID by the HOST allocated PID 
			UPT_SYSCALL_ARG3(r) = pid;
			args 	= UPT_SYSCALL_ARG3(r);
			printk("DVK handle_dvk_enter DVK_IOCQGETEP AFTER  fd=%d cmd=%X args=%d\n", fd, cmd, args);
			if (ptrace(PTRACE_SETREGS, pid, 0, r->gp)){
				printk("DVK handle_dvk_enter DVK_IOCQGETEP PTRACE_SETREGS errno=%d\n", errno);			
				fatal_sigsegv();
			}
			return(DVK_CMD_NOWAIT);
		} else if (cmd == DVK_IOCSDVKBIND) {
			args 	= UPT_SYSCALL_ARG3(r);
		
			tmp_pid = ptrace(PTRACE_PEEKDATA, pid, args+(0*sizeof(int)), NULL);
			printk("DVK handle_dvk_enter bind parm_cmd=%d\n",tmp_pid);
			tmp_pid = ptrace(PTRACE_PEEKDATA, pid, args+(1*sizeof(int)), NULL);
			printk("DVK handle_dvk_enter bind parm_dcid=%d\n",tmp_pid);
			tmp_pid = ptrace(PTRACE_PEEKDATA, pid, args+(2*sizeof(int)), NULL);
			printk("DVK handle_dvk_enter bind parm_pid=%d\n",tmp_pid);
			tmp_pid = ptrace(PTRACE_PEEKDATA, pid, args+(3*sizeof(int)), NULL);
			printk("DVK handle_dvk_enter bind parm_ep=%d\n",tmp_pid);
			tmp_pid = ptrace(PTRACE_PEEKDATA, pid, args+(4*sizeof(int)), NULL);
			printk("DVK handle_dvk_enter bind parm_nodeid=%d\n",tmp_pid);	

			tmp_cmd = ptrace(PTRACE_PEEKDATA, pid, args+(0*sizeof(int)), NULL);
			if( tmp_cmd == RMT_BIND) return(DVK_CMD_NOWAIT);
			
			if( tmp_cmd == SELF_BIND) {
				// change the UML_USER PID by the HOST allocated PID 
				// PID offset 3rd argument in parm_bind_t structure 
				tmp_pid = ptrace(PTRACE_PEEKDATA, pid, args+2*sizeof(int), NULL);
				printk("DVK handle_dvk_enter DVK_IOCSDVKBIND BEFORE fd=%d cmd=%X pid=%d\n", fd, cmd, tmp_pid);
				tmp_pid = ptrace(PTRACE_POKEDATA, pid, args+2*sizeof(int), pid);
				printk("DVK handle_dvk_enter DVK_IOCSDVKBIND AFTER  fd=%d cmd=%X pid=%d\n", fd, cmd, tmp_pid);
			} else {   // LCL_BIND,  BKUP_BIND, REPLICA_BIND
				// ??? Como se obtiene le  PID_HOST a partir del PID_UML ?????
			}
			
			if (ptrace(PTRACE_SETREGS, pid, 0, r->gp)){
				printk("DVK handle_dvk_enter DVK_IOCSDVKBIND PTRACE_SETREGS errno=%d\n", errno);			
				fatal_sigsegv();
			}
			return(DVK_CMD_NOWAIT);
		} else if ( _IOC_TYPE(cmd)	== DVK_IOC_MAGIC ) {
			dvk_call = _IOC_NR(cmd);
			printk("DVK handle_dvk_enter __NR_ioctl fd=%d cmd=%X lpid=%d dvk_call=%d wait=%d\n", 
				fd, cmd, lpid, dvk_call, dvk_call_wait[dvk_call]);
			return(dvk_call_wait[dvk_call]);
		}
	}else if( syscall == __NR_open ){ // DETECT OPEN ////////////////////////// 
			// int open(const char *pathname, int flags, mode_t mode);
			open_path = (char  *) ptrace(PTRACE_PEEKUSER, pid, sizeof(long)*EBX, 0);
	//		open_path	= UPT_SYSCALL_ARG1(r);
			open_flags  = UPT_SYSCALL_ARG2(r);
			open_mode   = UPT_SYSCALL_ARG3(r);
	//					memset(tmp_path, 0, len+1);
			tmp_ptr = (long *) &tmp_path[0];
			*tmp_ptr = (long) ptrace(PTRACE_PEEKDATA, pid, open_path+0, NULL ); 
			tmp_ptr = (long *) &tmp_path[4];
			*tmp_ptr = (long) ptrace(PTRACE_PEEKDATA, pid, open_path+sizeof(long), NULL); 	
	//		tmp_path[4] = ptrace(PTRACE_PEEKDATA, pid, open_path+4, &rcode); 
			tmp_path[2*sizeof(long)] = 0;
	//		if(count > 0) {
	//			count--;
	//			printk("DVK handle_dvk_enter __NR_open tmp_path=[%s] UML_DVK_DEV=[%s]\n", tmp_path, UML_DVK_DEV);
	//		}
			if ( strncmp(tmp_path, UML_DVK_DEV, UML_DVK_NAMELEN) == 0) { // string MATCH!
					lpid    = stub_syscall0(__NR_getpid);
					printk("DVK handle_dvk_enter MATCH __NR_open tracee_lpid=%d tracer_lpid=%d tmp_path=%s flags=%X mode=%X\n", 
					pid, lpid, tmp_path, open_flags, open_mode);
				return(DVK_CMD_NOWAIT);
			} 
	} 
	return(DVK_NOCMD);
}
#endif // 	CONFIG_UML_DVK

void userspace(struct uml_pt_regs *regs)
{
	int err, status, op, pid = userspace_pid[0];
	/* To prevent races if using_sysemu changes under us.*/
	int local_using_sysemu;
	siginfo_t si;
	/* Handle any immediate reschedules or signals */
	interrupt_end();

	while (1) {
		/*
		 * This can legitimately fail if the process loads a
		 * bogus value into a segment register.  It will
		 * segfault and PTRACE_GETREGS will read that value
		 * out of the process.  However, PTRACE_SETREGS will
		 * fail.  In this case, there is nothing to do but
		 * just kill the process.
		 */
		  
		if (ptrace(PTRACE_SETREGS, pid, 0, regs->gp)){
			printk(UM_KERN_ERR "userspace - PTRACE_SETREGS errno=%d\n", errno);			
			fatal_sigsegv();
		}
		
		if (put_fp_registers(pid, regs->fp)){
			printk(UM_KERN_ERR "userspace - PTRACE_SETREGS errno=%d\n", errno);			
			fatal_sigsegv();
		}

		/* Now we set local_using_sysemu to be used for one loop */
		local_using_sysemu = get_using_sysemu();
		op = SELECT_PTRACE_OPERATION(local_using_sysemu, singlestepping(NULL));
		if (ptrace(op, pid, 0, 0)) {
			printk(UM_KERN_ERR "userspace - ptrace continue "
			       "failed, op = %d, errno = %d\n", op, errno);
			fatal_sigsegv();
		}

#ifdef CONFIG_UML_DVK
userspace_loop:
#endif // 	CONFIG_UML_DVK

		CATCH_EINTR(err = waitpid(pid, &status, WUNTRACED | __WALL));
		if (err < 0) {
			printk(UM_KERN_ERR "userspace - wait failed, "
			       "errno = %d\n", errno);
			fatal_sigsegv();
		}

		regs->is_user = 1;
		if (ptrace(PTRACE_GETREGS, pid, 0, regs->gp)) {
			printk(UM_KERN_ERR "userspace - PTRACE_GETREGS failed, "
			       "errno = %d\n", errno);
			fatal_sigsegv();
		}

		if (get_fp_registers(pid, regs->fp)) {
			printk(UM_KERN_ERR "userspace -  get_fp_registers failed, "
			       "errno = %d\n", errno);
			fatal_sigsegv();
		}

#ifdef CONFIG_UML_DVK
		extern int dvk_fd; 
		int sg, rcode, err;
		int dvk_retcode;
		// only works without PTRACE_SYSEMU 
//		if( local_using_sysemu) goto userspace_sysemu;
		
		if (WIFSTOPPED(status)) {
			sg = WSTOPSIG(status);
			ptrace(PTRACE_GETSIGINFO, pid, 0, (struct siginfo *)&si);
			if( sg == SIGTRAP + 0x80) { 
				dvk_retcode = ptrace(PTRACE_PEEKUSER, pid, SC_RETCODE, NULL);
				if( dvk_retcode != (-ENOSYS))
					printk("userspace WARNING dvk_retcode=%d != -ENOSYS\n", dvk_retcode);
				rcode = handle_dvk_enter(pid, regs);
				if( rcode < 0) goto userspace_sysemu;				
				if( rcode == DVK_CMD_NOWAIT) { // NON-WAITING DVKCALL ENTER  
					handle_dvk_exit( pid, regs);
					pid = userspace_pid[0];
					interrupt_end();
					/* Avoid -ERESTARTSYS handling in host */
					if (PT_SYSCALL_NR_OFFSET != PT_SYSCALL_RET_OFFSET)
						PT_SYSCALL_NR(regs->gp) = -1;
					continue;
				} else  if( rcode == DVK_CMD_WAIT) { // WAITING  DVKCALL ENTER  
					handle_dvk_wait( pid, regs);
					handle_dvk_exit( pid, regs);
					pid = userspace_pid[0];
					interrupt_end();
					if (PT_SYSCALL_NR_OFFSET != PT_SYSCALL_RET_OFFSET)
						PT_SYSCALL_NR(regs->gp) = -1;
					continue;
//					err = ptrace(PTRACE_CONT, pid, 0, 0);
//					goto userspace_loop;
				}
			}
		}
userspace_sysemu:		
#endif // 	CONFIG_UML_DVK

		UPT_SYSCALL_NR(regs) = -1; /* Assume: It's not a syscall */

		if (WIFSTOPPED(status)) {
			int sig = WSTOPSIG(status);

			ptrace(PTRACE_GETSIGINFO, pid, 0, (struct siginfo *)&si);

			switch (sig) {
			case SIGSEGV:
				if (PTRACE_FULL_FAULTINFO) {
					get_skas_faultinfo(pid,
							   &regs->faultinfo);
					(*sig_info[SIGSEGV])(SIGSEGV, (struct siginfo *)&si,
							     regs);
				}
				else handle_segv(pid, regs);
				break;
			case SIGTRAP + 0x80:
			        handle_trap(pid, regs, local_using_sysemu);
				break;
			case SIGTRAP:
					relay_signal(SIGTRAP, (struct siginfo *)&si, regs);
				break;		
			case SIGALRM:
				break;
			case SIGIO:
			case SIGILL:
			case SIGBUS:
			case SIGFPE:
			case SIGWINCH:
				block_signals();
				(*sig_info[sig])(sig, (struct siginfo *)&si, regs);
				unblock_signals();
				break;
			default:
				printk(UM_KERN_ERR "userspace - child stopped "
				       "with signal %d\n", sig);
				fatal_sigsegv();
			}
			pid = userspace_pid[0];
			interrupt_end();

			/* Avoid -ERESTARTSYS handling in host */
			if (PT_SYSCALL_NR_OFFSET != PT_SYSCALL_RET_OFFSET)
				PT_SYSCALL_NR(regs->gp) = -1;
			
		} 
	}
}

static unsigned long thread_regs[MAX_REG_NR];
static unsigned long thread_fp_regs[FP_SIZE];

static int __init init_thread_regs(void)
{
	get_safe_registers(thread_regs, thread_fp_regs);
	/* Set parent's instruction pointer to start of clone-stub */
	thread_regs[REGS_IP_INDEX] = STUB_CODE +
				(unsigned long) stub_clone_handler -
				(unsigned long) __syscall_stub_start;
	thread_regs[REGS_SP_INDEX] = STUB_DATA + UM_KERN_PAGE_SIZE -
		sizeof(void *);
#ifdef __SIGNAL_FRAMESIZE
	thread_regs[REGS_SP_INDEX] -= __SIGNAL_FRAMESIZE;
#endif
	return 0;
}

__initcall(init_thread_regs);

int copy_context_skas0(unsigned long new_stack, int pid)
{
	int err;
	unsigned long current_stack = current_stub_stack();
	struct stub_data *data = (struct stub_data *) current_stack;
	struct stub_data *child_data = (struct stub_data *) new_stack;
	unsigned long long new_offset;
	int new_fd = phys_mapping(to_phys((void *)new_stack), &new_offset);

	/*
	 * prepare offset and fd of child's stack as argument for parent's
	 * and child's mmap2 calls
	 */
	*data = ((struct stub_data) {
			.offset	= MMAP_OFFSET(new_offset),
			.fd     = new_fd
	});

	err = ptrace_setregs(pid, thread_regs);
	if (err < 0) {
		err = -errno;
		printk(UM_KERN_ERR "copy_context_skas0 : PTRACE_SETREGS "
		       "failed, pid = %d, errno = %d\n", pid, -err);
		return err;
	}

	err = put_fp_registers(pid, thread_fp_regs);
	if (err < 0) {
		printk(UM_KERN_ERR "copy_context_skas0 : put_fp_registers "
		       "failed, pid = %d, err = %d\n", pid, err);
		return err;
	}

	/* set a well known return code for detection of child write failure */
	child_data->err = 12345678;

	/*
	 * Wait, until parent has finished its work: read child's pid from
	 * parent's stack, and check, if bad result.
	 */
	err = ptrace(PTRACE_CONT, pid, 0, 0);
	if (err) {
		err = -errno;
		printk(UM_KERN_ERR "Failed to continue new process, pid = %d, "
		       "errno = %d\n", pid, errno);
		return err;
	}
	
	// WAIT FOR PARENT 
	wait_stub_done(pid);
	
#ifdef CONFIG_UML_DVK_NULO
	int stub_fd, stub_fd_addr;
	stub_fd_addr = data->fd;
#endif // CONFIG_UML_DVK_NULO

	// GET CHILD PID 
	pid = data->err;
	if (pid < 0) {
		printk(UM_KERN_ERR "copy_context_skas0 - stub-parent reports "
		       "error %d\n", -pid);
		return pid;
	}

	/*
	 * Wait, until child has finished too: read child's result from
	 * child's stack and check it.
	 */
	// WAIT FOR CHILD 
	wait_stub_done(pid);
	if (child_data->err != STUB_DATA) {
		printk(UM_KERN_ERR "copy_context_skas0 - stub-child reports "
		       "error %ld\n", child_data->err);
		err = child_data->err;
		goto out_kill;
	}

#ifdef CONFIG_UML_DVK_NULO
	stub_fd = ptrace(PTRACE_PEEKDATA, pid, stub_fd_addr, 0);
	if (stub_fd < 0) {
		printk(UM_KERN_ERR "Failed PTRACE_PEEKDATA Child pid=%d stub_fd=%d errno=%d stub_fd_addr=%X\n", pid, stub_fd, errno, stub_fd_addr);
	} else {
		printk("SUCCESS!! pid=%d stub_fd=%d\n", pid, stub_fd);
	}
#endif // CONFIG_UML_DVK_NULO

	if (ptrace(PTRACE_OLDSETOPTIONS, pid, NULL,
		   (void *)PTRACE_O_TRACESYSGOOD) < 0) {
		err = -errno;
		printk(UM_KERN_ERR "copy_context_skas0 : PTRACE_OLDSETOPTIONS "
		       "failed, errno = %d\n", errno);
		goto out_kill;
	}

	return pid;

 out_kill:
	os_kill_ptraced_process(pid, 1);
	return err;
}

void new_thread(void *stack, jmp_buf *buf, void (*handler)(void))
{
	(*buf)[0].JB_IP = (unsigned long) handler;
	(*buf)[0].JB_SP = (unsigned long) stack + UM_THREAD_SIZE -
		sizeof(void *);
}

#define INIT_JMP_NEW_THREAD 0
#define INIT_JMP_CALLBACK 1
#define INIT_JMP_HALT 2
#define INIT_JMP_REBOOT 3

void switch_threads(jmp_buf *me, jmp_buf *you)
{
	if (UML_SETJMP(me) == 0)
		UML_LONGJMP(you, 1);
}

static jmp_buf initial_jmpbuf;

/* XXX Make these percpu */
static void (*cb_proc)(void *arg);
static void *cb_arg;
static jmp_buf *cb_back;

int start_idle_thread(void *stack, jmp_buf *switch_buf)
{
	int n;

	set_handler(SIGWINCH);

	/*
	 * Can't use UML_SETJMP or UML_LONGJMP here because they save
	 * and restore signals, with the possible side-effect of
	 * trying to handle any signals which came when they were
	 * blocked, which can't be done on this stack.
	 * Signals must be blocked when jumping back here and restored
	 * after returning to the jumper.
	 */
	n = setjmp(initial_jmpbuf);
	switch (n) {
	case INIT_JMP_NEW_THREAD:
		(*switch_buf)[0].JB_IP = (unsigned long) uml_finishsetup;
		(*switch_buf)[0].JB_SP = (unsigned long) stack +
			UM_THREAD_SIZE - sizeof(void *);
		break;
	case INIT_JMP_CALLBACK:
		(*cb_proc)(cb_arg);
		longjmp(*cb_back, 1);
		break;
	case INIT_JMP_HALT:
		kmalloc_ok = 0;
		return 0;
	case INIT_JMP_REBOOT:
		kmalloc_ok = 0;
		return 1;
	default:
		printk(UM_KERN_ERR "Bad sigsetjmp return in "
		       "start_idle_thread - %d\n", n);
		fatal_sigsegv();
	}
	longjmp(*switch_buf, 1);
}

void initial_thread_cb_skas(void (*proc)(void *), void *arg)
{
	jmp_buf here;

	cb_proc = proc;
	cb_arg = arg;
	cb_back = &here;

	block_signals();
	if (UML_SETJMP(&here) == 0)
		UML_LONGJMP(&initial_jmpbuf, INIT_JMP_CALLBACK);
	unblock_signals();

	cb_proc = NULL;
	cb_arg = NULL;
	cb_back = NULL;
}

void halt_skas(void)
{
	block_signals();
	UML_LONGJMP(&initial_jmpbuf, INIT_JMP_HALT);
}

void reboot_skas(void)
{
	block_signals();
	UML_LONGJMP(&initial_jmpbuf, INIT_JMP_REBOOT);
}

void __switch_mm(struct mm_id *mm_idp)
{
	userspace_pid[0] = mm_idp->u.pid;
}
