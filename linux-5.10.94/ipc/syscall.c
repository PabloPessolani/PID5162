// SPDX-License-Identifier: GPL-2.0
/*
 * sys_ipc() is the old de-multiplexer for the SysV IPC calls.
 *
 * This is really horribly ugly, and new architectures should just wire up
 * the individual syscalls instead.
 */
#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <linux/security.h>
#include <linux/ipc_namespace.h>
#include "util.h"

#ifdef __ARCH_WANT_SYS_IPC
#include <linux/errno.h>
#include <linux/ipc.h>
#include <linux/shm.h>
#include <linux/uaccess.h>

#ifdef  CONFIG_DVKIPC

#include "dvk_ipc.h"

unsigned int dvk_ipc_on;

char *dvk_routine_names[DVK_NR_CALLS] = {
    "ipc_void0",
    "ipc_dc_init",
    "ipc_mini_send",
    "ipc_mini_receive",
    "ipc_mini_notify",
    "ipc_mini_sendrec",
    "ipc_mini_rcvrqst",
    "ipc_mini_reply",
    "ipc_dc_end",
    "ipc_bind_X",
    "ipc_unbind",
    "ipc_getpriv",
    "ipc_setpriv",
    "ipc_vcopy",
    "ipc_getdcinfo",
    "ipc_getprocinfo",
    "ipc_mini_relay",
    "ipc_proxies_bind",
    "ipc_proxies_unbind",
    "ipc_getnodeinfo",
    "ipc_put2lcl",
    "ipc_get2rmt",
    "ipc_add_node",
    "ipc_del_node",
    "ipc_dvs_init",
    "ipc_dvs_end",
    "ipc_getep",
    "ipc_getdvsinfo",
    "ipc_proxy_conn",
    "ipc_wait4bind_X",
    "ipc_migrate",   
    "ipc_node_up",
    "ipc_node_down",
    "ipc_getproxyinfo",
	"ipc_wakeup",
};   /* the ones we are gonna replace */

  long ipc_void0( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_dc_init( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_mini_send( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_mini_receive( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_mini_notify( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_mini_sendrec( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_mini_rcvrqst( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_mini_reply( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_dc_end( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_bind_X( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_unbind( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_getpriv( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_setpriv( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_vcopy( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_getdcinfo( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_getprocinfo( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_mini_relay( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_proxies_bind( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_proxies_unbind( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_getnodeinfo( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_put2lcl( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_get2rmt( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_add_node( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_del_node( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_dvs_init( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_dvs_end( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_getep( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_getdvsinfo( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_proxy_conn( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_wait4bind_X( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_migrate( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_node_up( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_node_down( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_getproxyinfo( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
  long ipc_wakeup( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth);
	

// int first, unsigned long second, unsigned long third, void __user * ptr, long fifth
 long (*dvk_routine[DVK_NR_CALLS])( int first, unsigned long second,unsigned long third, void __user *ptr, long fifth) = {
    ipc_void0,
    ipc_dc_init,
    ipc_mini_send,
    ipc_mini_receive,
    ipc_mini_notify,
    ipc_mini_sendrec,
    ipc_mini_rcvrqst,
    ipc_mini_reply,
    ipc_dc_end,
    ipc_bind_X,
    ipc_unbind,
    ipc_getpriv,
    ipc_setpriv,
    ipc_vcopy,
    ipc_getdcinfo,
    ipc_getprocinfo,
    ipc_mini_relay,
    ipc_proxies_bind,
    ipc_proxies_unbind,
    ipc_getnodeinfo,
    ipc_put2lcl,
    ipc_get2rmt,
    ipc_add_node,
    ipc_del_node,
    ipc_dvs_init,
    ipc_dvs_end,
    ipc_getep,
    ipc_getdvsinfo,
    ipc_proxy_conn,
    ipc_wait4bind_X,
    ipc_migrate,
    ipc_node_up,
    ipc_node_down,
    ipc_getproxyinfo,
    ipc_wakeup,
};

long ipc_void0( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
{
	return(ENOSYS);
}
//int  dvk_mod_loaded = 0;
//EXPORT_SYMBOL(dvk_mod_loaded);
long sc_dvk_ipc(int call, int first, unsigned long second,unsigned long third, void __user *ptr, long fifth)
{	
	return(-ENOSYS);
}
EXPORT_SYMBOL(sc_dvk_ipc);
#endif //CONFIG_DVKIPC 

int ksys_ipc(unsigned int call, int first, unsigned long second,
	unsigned long third, void __user * ptr, long fifth)
{
	int version, ret;

	version = call >> 16; /* hack for backward compatibility */
	call &= 0xffff;

#ifdef  CONFIG_DVKIPC 
#pragma message ("SYSCALL_DEFINE6=ipc")
	int rcode, dvk_call;
	if( call > 0xFF) { // DVK_CALL are formed as call = NR00;
//		if( dvk_mod_loaded == 0) ERROR_RETURN(EDVSNOSYS);
		dvk_call = call >> 8;
		if( dvk_call < 0 || dvk_call > DVK_NR_CALLS)
			ERROR_RETURN(EDVSNOSYS);
		DVKDEBUG(DBGPARAMS, "%s: call=%d first=%d second=%d third=%d fifth=%d \n", 
			dvk_routine_names[dvk_call],call, first, second, third, fifth); 
		rcode = (*dvk_routine[dvk_call])(first, second, third, ptr, fifth);
		DVKDEBUG(DBGPARAMS, "%s: call=%d rcode=%d\n", 
			dvk_routine_names[dvk_call],call, rcode);
		ERROR_RETURN(rcode);
	}	
#endif //CONFIG_DVKIPC 

	switch (call) {
	case SEMOP:
		return ksys_semtimedop(first, (struct sembuf __user *)ptr,
				       second, NULL);
	case SEMTIMEDOP:
		if (IS_ENABLED(CONFIG_64BIT))
			return ksys_semtimedop(first, ptr, second,
			        (const struct __kernel_timespec __user *)fifth);
		else if (IS_ENABLED(CONFIG_COMPAT_32BIT_TIME))
			return compat_ksys_semtimedop(first, ptr, second,
			        (const struct old_timespec32 __user *)fifth);
		else
			return -ENOSYS;

	case SEMGET:
		return ksys_semget(first, second, third);
	case SEMCTL: {
		unsigned long arg;
		if (!ptr)
			return -EINVAL;
		if (get_user(arg, (unsigned long __user *) ptr))
			return -EFAULT;
		return ksys_old_semctl(first, second, third, arg);
	}

	case MSGSND:
		return ksys_msgsnd(first, (struct msgbuf __user *) ptr,
				  second, third);
	case MSGRCV:
		switch (version) {
		case 0: {
			struct ipc_kludge tmp;
			if (!ptr)
				return -EINVAL;

			if (copy_from_user(&tmp,
					   (struct ipc_kludge __user *) ptr,
					   sizeof(tmp)))
				return -EFAULT;
			return ksys_msgrcv(first, tmp.msgp, second,
					   tmp.msgtyp, third);
		}
		default:
			return ksys_msgrcv(first,
					   (struct msgbuf __user *) ptr,
					   second, fifth, third);
		}
	case MSGGET:
		return ksys_msgget((key_t) first, second);
	case MSGCTL:
		return ksys_old_msgctl(first, second,
				   (struct msqid_ds __user *)ptr);

	case SHMAT:
		switch (version) {
		default: {
			unsigned long raddr;
			ret = do_shmat(first, (char __user *)ptr,
				       second, &raddr, SHMLBA);
			if (ret)
				return ret;
			return put_user(raddr, (unsigned long __user *) third);
		}
		case 1:
			/*
			 * This was the entry point for kernel-originating calls
			 * from iBCS2 in 2.2 days.
			 */
			return -EINVAL;
		}
	case SHMDT:
		return ksys_shmdt((char __user *)ptr);
	case SHMGET:
		return ksys_shmget(first, second, third);
	case SHMCTL:
		return ksys_old_shmctl(first, second,
				   (struct shmid_ds __user *) ptr);
	default:
		return -ENOSYS;
	}
}

SYSCALL_DEFINE6(ipc, unsigned int, call, int, first, unsigned long, second,
		unsigned long, third, void __user *, ptr, long, fifth)
{
	return ksys_ipc(call, first, second, third, ptr, fifth);
}
#endif

#ifdef CONFIG_COMPAT
#include <linux/compat.h>

#ifndef COMPAT_SHMLBA
#define COMPAT_SHMLBA	SHMLBA
#endif

struct compat_ipc_kludge {
	compat_uptr_t msgp;
	compat_long_t msgtyp;
};

#ifdef CONFIG_ARCH_WANT_OLD_COMPAT_IPC
int compat_ksys_ipc(u32 call, int first, int second,
	u32 third, compat_uptr_t ptr, u32 fifth)
{
	int version;
	u32 pad;

	version = call >> 16; /* hack for backward compatibility */
	call &= 0xffff;

	switch (call) {
	case SEMOP:
		/* struct sembuf is the same on 32 and 64bit :)) */
		return ksys_semtimedop(first, compat_ptr(ptr), second, NULL);
	case SEMTIMEDOP:
		if (!IS_ENABLED(CONFIG_COMPAT_32BIT_TIME))
			return -ENOSYS;
		return compat_ksys_semtimedop(first, compat_ptr(ptr), second,
						compat_ptr(fifth));
	case SEMGET:
		return ksys_semget(first, second, third);
	case SEMCTL:
		if (!ptr)
			return -EINVAL;
		if (get_user(pad, (u32 __user *) compat_ptr(ptr)))
			return -EFAULT;
		return compat_ksys_old_semctl(first, second, third, pad);

	case MSGSND:
		return compat_ksys_msgsnd(first, ptr, second, third);

	case MSGRCV: {
		void __user *uptr = compat_ptr(ptr);

		if (first < 0 || second < 0)
			return -EINVAL;

		if (!version) {
			struct compat_ipc_kludge ipck;
			if (!uptr)
				return -EINVAL;
			if (copy_from_user(&ipck, uptr, sizeof(ipck)))
				return -EFAULT;
			return compat_ksys_msgrcv(first, ipck.msgp, second,
						 ipck.msgtyp, third);
		}
		return compat_ksys_msgrcv(first, ptr, second, fifth, third);
	}
	case MSGGET:
		return ksys_msgget(first, second);
	case MSGCTL:
		return compat_ksys_old_msgctl(first, second, compat_ptr(ptr));

	case SHMAT: {
		int err;
		unsigned long raddr;

		if (version == 1)
			return -EINVAL;
		err = do_shmat(first, compat_ptr(ptr), second, &raddr,
			       COMPAT_SHMLBA);
		if (err < 0)
			return err;
		return put_user(raddr, (compat_ulong_t __user *)compat_ptr(third));
	}
	case SHMDT:
		return ksys_shmdt(compat_ptr(ptr));
	case SHMGET:
		return ksys_shmget(first, (unsigned int)second, third);
	case SHMCTL:
		return compat_ksys_old_shmctl(first, second, compat_ptr(ptr));
	}

	return -ENOSYS;
}

COMPAT_SYSCALL_DEFINE6(ipc, u32, call, int, first, int, second,
	u32, third, compat_uptr_t, ptr, u32, fifth)
{
	return compat_ksys_ipc(call, first, second, third, ptr, fifth);
}
#endif
#endif
