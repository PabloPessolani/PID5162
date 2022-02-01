/****************************************************************/
/*			DVK MACROS   			*/
/****************************************************************/

/*----------------------------------------------------------------*/
/*				ERROR CHECKING MACROS 				*/
/*----------------------------------------------------------------*/

#define RCU_READ_LOCK 	rcu_read_lock()
#define RCU_READ_UNLOCK rcu_read_unlock()

#define LOCK_TASK_LIST read_lock(tasklist_ptr)
#define UNLOCK_TASK_LIST read_unlock(tasklist_ptr)

#define CHECK_DCID(dcid) 	if( dcid < 0 || dcid >= dvs.d_nr_dcs) 	ERROR_RETURN(EDVSBADDCID);
#define CHECK_NODEID(nodeid) 	if( nodeid < 0 || nodeid >= dvs.d_nr_nodes) 	ERROR_RETURN(EDVSBADNODEID);
#define CHECK_PROXYID(pxid) 	if( pxid < 0 || pxid >= dvs.d_nr_nodes) 	ERROR_RETURN(EDVSPROXYID);

#define CHECK_PID(pid, p_ptr)	if (pid != p_ptr->p_usr.p_lpid) 		ERROR_RETURN(EDVSBADPID);

#define CHECK_IF_DC_RUN(d_ptr) 	if( d_ptr->dc_usr.dc_flags) 			ERROR_RUNLOCK_DC(d_ptr,EDVSDCNOTRUN);

#define DC_PROC(dc_ptr,i)	((struct proc *) (((char *) dc_ptr->dc_proc) + ( i << log2_proc_size)))
#define IT_IS_REMOTE(proc)	test_bit(BIT_REMOTE, &proc->p_usr.p_rts_flags)
#define IT_IS_LOCAL(proc)	!test_bit(BIT_REMOTE, &proc->p_usr.p_rts_flags)

#define ENDPOINT2PTR(dc_ptr, ep) DC_PROC(dc_ptr,(_ENDPOINT_P(ep)+dc_ptr->dc_usr.dc_nr_tasks))
#define NBR2PTR(dc_ptr, nr)	DC_PROC(dc_ptr,(nr+dc_ptr->dc_usr.dc_nr_tasks))
#define DVS_NOT_INIT()		(atomic_read(&local_nodeid) == DVS_NO_INIT)

#define DC_INCREF(d)	do { \
  	DVKDEBUG(DBGREFCOUNT ,"DC_INCREF counter=%d\n",atomic_read(&d->dc_kref.refcount));\
	kref_get(&d->dc_kref);\
}while(0);

#define DC_DECREF(d, function)	do { \
	kref_put(&d->dc_kref, function); \
  	DVKDEBUG(DBGREFCOUNT ,"DC_DECREF counter=%d\n",atomic_read(&d->dc_kref.refcount));\
}while(0);	

#define NODE2SPROXY(n) 	  &proxies[node[n].n_usr.n_proxies].px_sproxy
#define NODE2RPROXY(n) 	  &proxies[node[n].n_usr.n_proxies].px_rproxy
#define NODE2MAXBYTES(n)  proxies[node[n].n_usr.n_proxies].px_usr.px_maxbytes

#define KREF_GET(rc) do { \
  	DVKDEBUG(DBGREFCOUNT ,"KREF_GET counter=%d\n",atomic_read(rc.refcount));\
	kref_get(rc);\
}while(0);

#define KREF_PUT(rc, function) do { \
	kref_put(rc, function);\
  	DVKDEBUG(DBGREFCOUNT ,"KREF_PUT counter=%d\n",atomic_read(rc.refcount));\
}while(0);

/*----------------------------------------------------------------*/
/*				MACROS 				*/
/*----------------------------------------------------------------*/
#define ERROR_PRINT(rcode) \
 do { \
     	printk("ERROR: %d:%s:%u: rcode=%d\n",task_pid_nr(current), __FUNCTION__ ,__LINE__,rcode); \
 }while(0);
 
#define ERROR_RETURN(rcode) \
 do { \
     	printk("ERROR: %d:%s:%u: rcode=%d\n",task_pid_nr(current), __FUNCTION__ ,__LINE__,rcode); \
	return(rcode); \
 }while(0);


#define ERROR_RUNLOCK_DC(d,rcode) \
 do { \
	RUNLOCK_DC(d); \
     	printk("ERROR: %d:%s:%u: rcode=%d\n",task_pid_nr(current), __FUNCTION__ ,__LINE__, rcode); \
	return(rcode); \
 }while(0);

#define ERROR_WUNLOCK_DC(d,rcode) \
 do { \
	WUNLOCK_DC(d); \
     	printk("ERROR: %d:%s:%u: rcode=%d\n",task_pid_nr(current), __FUNCTION__ ,__LINE__, rcode); \
	return(rcode); \
 }while(0);
 
#define ERROR_RUNLOCK_PROC(p,rcode) \
 do { \
	RUNLOCK_PROC(p); \
     	printk("ERROR: %d:%s:%u: rcode=%d\n",task_pid_nr(current), __FUNCTION__ ,__LINE__, rcode); \
	return(rcode); \
 }while(0);

#define ERROR_WUNLOCK_PROC(p,rcode) \
 do { \
	WUNLOCK_PROC(p); \
     	printk("ERROR: %d:%s:%u: rcode=%d\n",task_pid_nr(current), __FUNCTION__ ,__LINE__, rcode); \
	return(rcode); \
 }while(0);

#define ERROR_RUNLOCK_TASK(task, rcode) \
 do { \
	RUNLOCK_TASK(task);\
	ERROR_RETURN(rcode);\
 }while(0);

#define ERROR_WUNLOCK_TASK(task, rcode) \
 do { \
	WUNLOCK_TASK(task);\
	ERROR_RETURN(rcode);\
 }while(0);
 
#define LOCAL_PROC_UP(proc, rcode) \
 do { \
	inherit_cpu(proc); \
 	proc->p_rcode = rcode; \
	DVKDEBUG(DBGPROCSEM,"BEFORE UP lpid=%d p_sem=%d rcode=%d\n",proc->p_usr.p_lpid,proc->p_pseudosem, rcode); \
	proc->p_pseudosem += 1; \
	wake_up_interruptible(&proc->p_wqhead); \
}while(0);

#define READY_UP_SHUTDOWN(proc) \
 do { \
 	inherit_cpu(proc); \
   	proc->p_rcode = EDVSSHUTDOWN; \
	DVKDEBUG(DBGPROCSEM,"BEFORE UP lpid=%d p_sem=%d\n",proc->p_usr.p_lpid,proc->p_pseudosem); \
	proc->p_pseudosem += 1; \
	wake_up_interruptible(&proc->p_wqhead); \
	}\
 }while(0);

#define READY_UP_RCODE(proc, cmd, rcode) \
 do { \
 	inherit_cpu(proc); \
	proc->p_rcode = rcode; \
DVKDEBUG(DBGPROCSEM,"BEFORE UP lpid=%d p_sem=%d\n",proc->p_usr.p_lpid,proc->p_pseudosem); \
	proc->p_pseudosem += 1; \
	wake_up_interruptible(&proc->p_wqhead); \
 }while(0); 

#define BUILD_NOTIFY_MSG(c_ptr, s_ep, s_nr, d_ptr ) \
 do { \
    if(c_ptr == d_ptr) {\
        d_ptr->p_message.m_source = HARDWARE;\
    }else{\
        d_ptr->p_message.m_source = s_ep;\
    }\
	d_ptr->p_message.m_type = NOTIFY_FROM(s_nr);\
	d_ptr->p_message.NOTIFY_TIMESTAMP = current_kernel_time();\
	switch (s_nr) {\
		case HARDWARE:\
			d_ptr->p_message.NOTIFY_ARG = (long) d_ptr->p_priv.priv_int_pending;\
			d_ptr->p_priv.priv_int_pending = 0;\
			break;\
		case SYSTEM:\
			d_ptr->p_message.NOTIFY_ARG = (long) d_ptr->p_priv.priv_sig_pending;\
			d_ptr->p_priv.priv_sig_pending = 0;\
		case SLOTS:\
			d_ptr->p_message.NOTIFY_ARG = (long) d_ptr->p_priv.priv_updt_pending;\
			d_ptr->p_priv.priv_updt_pending = 0;\
			break;\
		}\
	}while(0); 

#define WLOCK_ORDERED2(a,b,ptr,qtr)		\
do {\
	if( a < b) { \
		WLOCK_PROC(ptr);\
		WLOCK_PROC(qtr);\
	}else{\
		WLOCK_PROC(qtr);\
		WLOCK_PROC(ptr);\
	}\
}while(0);

#ifdef ANULADO
#define WLOCK_PROC2(p,q)		\
do {\
	if( p->p_usr.p_nr < q->p_usr.p_nr) { \
		WLOCK_PROC(p);\
		WLOCK_PROC(q);\
	}else{\
		WLOCK_PROC(q);\
		WLOCK_PROC(p);\
	}\
}while(0);
#endif // ANULADO

#define WUNLOCK_PROC2(p,q)	\
do {\
	WUNLOCK_PROC(p);\
	WUNLOCK_PROC(q);\
}while(0); 

#define WLOCK_ORDERED3(p,q,r,ptr, qtr, rtr)		\
do {\
	if( p < q) { \
		if( r < q) {\
			WLOCK_ORDERED2(p,r, ptr, rtr);\
			WLOCK_PROC(qtr);\
		}else{ \
			WLOCK_ORDERED2(p,q, ptr, qtr);\
			WLOCK_PROC(rtr);\
		}\
	}else{\
		if( r < p) {\
			WLOCK_ORDERED2(q,r, qtr, rtr);\
			WLOCK_PROC(ptr);\
		}else{ \
			WLOCK_ORDERED2(q,p, qtr, ptr);\
			WLOCK_PROC(rtr);\
		}\
	}\
}while(0);

#define WUNLOCK_PROC3(p,q,r)	\
do {\
	WUNLOCK_PROC(p);\
	WUNLOCK_PROC(q);\
	WUNLOCK_PROC(r);\
}while(0);

#define COPY_KRN2USR(ret, endpoint,  srcaddr, dst, dstaddr, len )	\
do {\
		ret = copy_krn2usr(endpoint, srcaddr, dst, dstaddr, len);\
}while(0)

#define COPY_USR2KRN(ret, endpoint, src, srcaddr, dstaddr, len )	\
do {\
	ret = copy_usr2krn(endpoint, src, srcaddr, dstaddr, len);\
}while(0)
	
/*--------------------------------------------------------- USE_TASK_RWLOCKS ---------------------------------------*/
#if LOCK_TASK_TYPE == USE_TASK_RWLOCK

#define TASK_LOCK_INIT(task)	rwlock_init(&(task->task_rwlock));

#define RLOCK_TASK(task) 	\
do {\
		read_lock(&task->task_rwlock);\
	DVKDEBUG(DBGTASKLOCK ,"RLOCK_TASK pid=%d\n",task->tgid);\
}while(0);

#define RUNLOCK_TASK(task) 	\
do {\
	DVKDEBUG(DBGTASKLOCK ,"RUNLOCK_TASK pid=%d\n",task->tgid);\
		read_unlock(&task->task_rwlock);\
}while(0);

#define WLOCK_TASK(task) 	\
do {\
	write_lock(&task->task_rwlock);\
	DVKDEBUG(DBGTASKLOCK ,"WLOCK_TASK pid=%d\n",task->tgid);\
}while(0);

#define WUNLOCK_TASK(task) 	\
do {\
	DVKDEBUG(DBGTASKLOCK ,"WUNLOCK_TASK pid=%d\n",task->tgid);\
	write_unlock(&task->task_rwlock);\
}while(0);

/*--------------------------------------------------------- USE_TASK_MUTEX  ---------------------------------------*/
#elif LOCK_TASK_TYPE == USE_TASK_MUTEX

#define TASK_LOCK_INIT(task) mutex_init(&task->task_mutex)

#define RLOCK_TASK(task) 	\
do {\
	mutex_lock(&task->task_mutex);\
	DVKDEBUG(DBGTASKLOCK ,"RLOCK_TASK pid=%d count=%d\n",task->tgid,atomic_read(&task->task_mutex.count));\
}while(0);

#define RUNLOCK_TASK(task) 	\
do {\
	DVKDEBUG(DBGTASKLOCK ,"RUNLOCK_TASK pid=%d count=%d\n",task->tgid,atomic_read(&task->task_mutex.count));\
	mutex_unlock(&task->task_mutex);\
}while(0);

#define WLOCK_TASK(task) 	\
do {\
	mutex_lock(&task->task_mutex);\
	DVKDEBUG(DBGTASKLOCK ,"WLOCK_TASK pid=%d count=%d\n",task->tgid,atomic_read(&task->task_mutex.count));\
}while(0);

#define WUNLOCK_TASK(task) 	\
do {\
	DVKDEBUG(DBGTASKLOCK ,"WUNLOCK_TASK pid=%d count=%d\n",task->tgid,atomic_read(&task->task_mutex.count));\
	mutex_unlock(&task->task_mutex);\
}while(0);

/*--------------------------------------------------------- USE_TASK_SPINLOCK ---------------------------------------*/
#elif LOCK_TASK_TYPE == USE_TASK_SPINLOCK

#define TASK_LOCK_INIT(task) spin_lock_init(&task->task_spinlock);

#define RLOCK_TASK(task) 	\
do {\
	spin_lock(&task->task_spinlock);\
	DVKDEBUG(DBGTASKLOCK ,"RLOCK_TASK pid=%d\n",task->tgid);\
}while(0);

#define RUNLOCK_TASK(task) 	\
do {\
	DVKDEBUG(DBGTASKLOCK ,"RUNLOCK_TASK pid=%d\n",task->tgid);\
	spin_unlock(&task->task_spinlock);\
}while(0);

#define WLOCK_TASK(task) 	\
do {\
	spin_lock(&task->task_spinlock);\
	DVKDEBUG(DBGTASKLOCK ,"WLOCK_TASK pid=%d\n",task->tgid);\
}while(0);

#define WUNLOCK_TASK(task) 	\
do {\
DVKDEBUG(DBGTASKLOCK ,"WUNLOCK_TASK pid=%d\n",task->tgid);\
	spin_unlock(&task->task_spinlock);\
}while(0)
	
/*--------------------------------------------------------- USE_TASK_RWSEM ---------------------------------------*/
#elif LOCK_TASK_TYPE == USE_TASK_RWSEM

#define TASK_LOCK_INIT(task) init_rwsem(&task->task_rwsem);

#define RLOCK_TASK(task) 	\
do {\
	down_read(&task->task_rwsem);\
	DVKDEBUG(DBGPROCLOCK,"RLOCK_TASK pid=%d count=%ld\n",task->tgid, task->task_rwsem.count);\
}while(0);

#define RUNLOCK_TASK(task) 	\
do {\
	DVKDEBUG(DBGPROCLOCK,"RUNLOCK_TASK  pid=%d  count=%ld\n",task->tgid, task->task_rwsem.count);\
	up_read(&task->task_rwsem);\
}while(0);

#define WLOCK_TASK(task) 	\
do {\
	down_write(&task->task_rwsem);\
	DVKDEBUG(DBGPROCLOCK,"WLOCK_TASK  pid=%d  count=%ld\n",task->tgid, task->task_rwsem.count);\
}while(0);

#define WUNLOCK_TASK(task) 	\
do {\
	DVKDEBUG(DBGPROCLOCK,"WUNLOCK_TASK  pid=%d  count=%ld\n",task->tgid, task->task_rwsem.count);\
	up_write(&task->task_rwsem);\
}while(0)
	
/*--------------------------------------------------------- USE_TASK_RCU  ---------------------------------------*/
#elif LOCK_TASK_TYPE == USE_TASK_RCU  

#define TASK_LOCK_INIT(task) spin_lock_init(&task->task_spinlock);

#define WUNLOCK_TASK(task)	\
do {\
	DVKDEBUG(DBGTASKLOCK ,"WUNLOCK_TASK pid=%d\n",task->tgid);\
	spin_unlock(&task->task_spinlock);\
	rcu_read_unlock();\
	synchronize_rcu();\
}while(0); 

#define RUNLOCK_TASK(task)	\
do {\
	DVKDEBUG(DBGTASKLOCK ,"RUNLOCK_TASK pid=%d\n",task->tgid);\
	rcu_read_unlock();\
}while(0); 
	
#define WLOCK_TASK(task)		\
do {\
	rcu_read_lock();\
	spin_lock(&task->task_spinlock);\
	DVKDEBUG(DBGTASKLOCK ,"WLOCK_TASK pid=%d\n",task->tgid);\
}while(0);

#define RLOCK_DC(task)		\
do {\
	rcu_read_lock();\
	DVKDEBUG(DBGTASKLOCK ,"RLOCK_TASK pid=%d\n",task->tgid);\
}while(0);
	
#endif

/*--------------------------------------------------------- USE_PROC_RWLOCKS ---------------------------------------*/
#if LOCK_PROC_TYPE == USE_PROC_RWLOCK

#define RUNLOCK_PROC(p)	\
	do {\
		DVKDEBUG(DBGPROCLOCK,"RUNLOCK_PROC ep=%d\n",p->p_usr.p_endpoint);\
		read_unlock(&p->p_rwlock);\
	}while(0); 

#define WUNLOCK_PROC(p)	\
do {\
	DVKDEBUG(DBGPROCLOCK,"WUNLOCK_PROC ep=%d\n",p->p_usr.p_endpoint);\
	write_unlock(&p->p_rwlock);\
}while(0); 

#define WLOCK_PROC(p)		\
do {\
	write_lock(&p->p_rwlock);\
	DVKDEBUG(DBGPROCLOCK,"WLOCK_PROC ep=%d\n",p->p_usr.p_endpoint);\
}while(0);

#define RLOCK_PROC(p)		\
do {\
	read_lock(&p->p_rwlock);\
	DVKDEBUG(DBGPROCLOCK,"RLOCK_PROC ep=%d\n",p->p_usr.p_endpoint);\
}while(0);

#define COPY_TO_USER_PROC(ret, kaddr, uaddr, len )		\
do {\
		preempt_enable();\
	 	ret = copy_to_user(uaddr, kaddr, len);\
		preempt_disable();\
}while(0)

#define COPY_FROM_USER_PROC(ret, kaddr, uaddr, len )		\
do {\
		preempt_enable();\
 	ret = copy_from_user(kaddr, uaddr, len);\
		preempt_disable();\
}while(0)

#define COPY_USR2USR_PROC_XXXX(ret, endpoint, src, saddr, dst, daddr, len)		\
do {\
		preempt_enable();\
	ret = copy_usr2usr(endpoint, src, saddr, dst, daddr, len);\
		preempt_disable();\
}while(0)
	
#define COPY_USR2USR_PROC(ret, endpoint, src, saddr, dst, daddr, len)		\
	ret = copy_usr2usr(endpoint, src, saddr, dst, daddr, len);

#define PROC_LOCK_INIT(p)	rwlock_init(&(p->p_rwlock));

#define PLOCK_PROC(p)		\
do {\
	DVKDEBUG(DBGPROCLOCK,"PLOCK_PROC ep=%d\n",p->p_usr.p_endpoint);\
}while(0);

/*--------------------------------------------------------- USE_PROC_MUTEX  ---------------------------------------*/
#elif LOCK_PROC_TYPE == USE_PROC_MUTEX

#define RLOCK_PROC(p)		\
do {\
	mutex_lock(&p->p_mutex);\
	DVKDEBUG(DBGPROCLOCK,"RLOCK_PROC ep=%d count=%d\n",p->p_usr.p_endpoint,atomic_read(&p->p_mutex.count));\
}while(0);

#define RUNLOCK_PROC(p)	\
	do {\
		DVKDEBUG(DBGPROCLOCK,"RUNLOCK_PROC ep=%d count=%d\n",p->p_usr.p_endpoint, atomic_read(&p->p_mutex.count));\
		mutex_unlock(&p->p_mutex);\
	}while(0); 

#define WUNLOCK_PROC(p)	\
do {\
	DVKDEBUG(DBGPROCLOCK,"WUNLOCK_PROC ep=%d count=%d\n",p->p_usr.p_endpoint,atomic_read(&p->p_mutex.count));\
	mutex_unlock(&p->p_mutex);\
}while(0); 

#define WLOCK_PROC(p)		\
do {\
	mutex_lock(&p->p_mutex);\
	DVKDEBUG(DBGPROCLOCK,"WLOCK_PROC ep=%d count=%d\n",p->p_usr.p_endpoint,atomic_read(&p->p_mutex.count));\
}while(0);

#define wunlock_proc(p)	\
do {\
	mutex_unlock(&p->p_mutex);\
}while(0); 

#define wlock_proc(p)		\
do {\
	mutex_lock(&p->p_mutex);\
}while(0);


#define COPY_TO_USER_PROC(ret, kaddr, uaddr, len )		\
do {\
	 	ret = copy_to_user(uaddr, kaddr, len);\
}while(0)

#define COPY_FROM_USER_PROC(ret, kaddr, uaddr, len )		\
do {\
 	ret = copy_from_user(kaddr, uaddr, len);\
}while(0)

#define COPY_USR2USR_PROC(ret, endpoint, src, srcaddr, dst, dstaddr, len )		\
do {\
	ret = copy_usr2usr(endpoint, src, srcaddr, dst, dstaddr, len);\
}while(0)

#define PROC_LOCK_INIT(p)	mutex_init(&(p->p_mutex));

#define PLOCK_PROC(p)		\
do {\
	DVKDEBUG(DBGPROCLOCK,"PLOCK_PROC ep=%d count=%d\n",p->p_usr.p_endpoint,atomic_read(&p->p_mutex.count));\
}while(0);

/*--------------------------------------------------------- USE_PROC_SPINLOCK ---------------------------------------*/
#elif LOCK_PROC_TYPE == USE_PROC_SPINLOCK

#define RLOCK_PROC(p)		\
do {\
	spin_lock(&p->p_spinlock);\
	DVKDEBUG(DBGPROCLOCK,"RLOCK_PROC ep=%d\n",p->p_usr.p_endpoint);\
}while(0);

#define RUNLOCK_PROC(p)	\
do {\
	DVKDEBUG(DBGPROCLOCK,"RUNLOCK_PROC ep=%d\n",p->p_usr.p_endpoint);\
	spin_unlock(&p->p_spinlock);\
}while(0); 

#define WUNLOCK_PROC(p)	\
do {\
	DVKDEBUG(DBGPROCLOCK,"WUNLOCK_PROC ep=%d\n",p->p_usr.p_endpoint);\
	spin_unlock(&p->p_spinlock);\
}while(0); 

#define WLOCK_PROC(p)		\
do {\
	spin_lock(&p->p_spinlock);\
	DVKDEBUG(DBGPROCLOCK,"WLOCK_PROC ep=%d\n",p->p_usr.p_endpoint);\
}while(0);

#define wunlock_proc(p)	\
do {\
	spin_unlock(&p->p_spinlock);\
}while(0); 

#define wlock_proc(p)		\
do {\
	spin_lock(&p->p_spinlock);\
}while(0);


#define COPY_TO_USER_PROC(ret, kaddr, uaddr, len )		\
do {\
	 	ret = copy_to_user(uaddr, kaddr, len);\
}while(0)

#define COPY_FROM_USER_PROC(ret, kaddr, uaddr, len )		\
do {\
 	ret = copy_from_user(kaddr, uaddr, len);\
}while(0)

#define COPY_USR2USR_PROC(ret, endpoint, src, srcaddr, dst, dstaddr, len )		\
do {\
	ret = copy_usr2usr(endpoint, src, srcaddr, dst, dstaddr, len);\
}while(0)

#define PROC_LOCK_INIT(p)	spin_lock_init(&(p->p_spinlock));

#define PLOCK_PROC(p)		\
do {\
	DVKDEBUG(DBGPROCLOCK,"PLOCK_PROC ep=%d\n",p->p_usr.p_endpoint);\
}while(0);

/*--------------------------------------------------------- USE_PROC_RWSEM  ---------------------------------------*/
#elif LOCK_PROC_TYPE == USE_PROC_RWSEM

#define RLOCK_PROC(p)		\
do {\
	down_read(&p->p_rwsem);\
	DVKDEBUG(DBGPROCLOCK,"RLOCK_PROC ep=%d count=%ld\n",p->p_usr.p_endpoint,p->p_rwsem.count);\
}while(0);

#define RUNLOCK_PROC(p)	\
	do {\
		DVKDEBUG(DBGPROCLOCK,"RUNLOCK_PROC ep=%d count=%ld\n",p->p_usr.p_endpoint, p->p_rwsem.count);\
		up_read(&p->p_rwsem);\
	}while(0); 

#define WUNLOCK_PROC(p)	\
do {\
	DVKDEBUG(DBGPROCLOCK,"WUNLOCK_PROC ep=%d count=%ld\n",p->p_usr.p_endpoint,p->p_rwsem.count);\
	up_write(&p->p_rwsem);\
}while(0); 

#define WLOCK_PROC(p)		\
do {\
	down_write(&p->p_rwsem);\
	DVKDEBUG(DBGPROCLOCK,"WLOCK_PROC ep=%d count=%ld\n",p->p_usr.p_endpoint,p->p_rwsem.count);\
}while(0);

#define wunlock_proc(p)	\
do {\
	up_write(&p->p_rwsem);\
}while(0); 

#define wlock_proc(p)		\
do {\
	down_write(&p->p_rwsem);\
}while(0);

#define COPY_TO_USER_PROC(ret, kaddr, uaddr, len )		\
do {\
	 	ret = copy_to_user(uaddr, kaddr, len);\
}while(0)

#define COPY_FROM_USER_PROC(ret, kaddr, uaddr, len )		\
do {\
 	ret = copy_from_user(kaddr, uaddr, len);\
}while(0)

#define COPY_USR2USR_PROC(ret, endpoint, src, srcaddr, dst, dstaddr, len )		\
do {\
	ret = copy_usr2usr(endpoint, src, srcaddr, dst, dstaddr, len);\
}while(0)

#define PROC_LOCK_INIT(p)	init_rwsem(&(p->p_rwsem));

#endif

#define LOCK_ALL_PROCS(dc_ptr, tmp_ptr, i) do { \
	DVKDEBUG(INTERNAL,"Locking all processes of DC=%d\n",dc_ptr->dc_usr.dc_dcid); \
	for (i = 0; i < (dc_ptr->dc_usr.dc_nr_tasks + dc_ptr->dc_usr.dc_nr_procs); i++) {\
		tmp_ptr = DC_PROC(dc_ptr,i);\
		WLOCK_PROC(tmp_ptr); \
	}\
}while(0)

#define UNLOCK_ALL_PROCS(dc_ptr, tmp_ptr,i) do{ \
	DVKDEBUG(INTERNAL,"Unlocking all processes of DC=%d\n",dc_ptr->dc_usr.dc_dcid);\
	for (i = 0; i < (dc_ptr->dc_usr.dc_nr_tasks + dc_ptr->dc_usr.dc_nr_procs); i++) {\
		tmp_ptr = DC_PROC(dc_ptr,i);\
		WUNLOCK_PROC(tmp_ptr); \
	}\
}while(0)

#define FOR_EACH_PROC(dc_ptr, i) \
	for (i = 0; i < (dc_ptr->dc_usr.dc_nr_tasks + dc_ptr->dc_usr.dc_nr_procs); i++) \

#define FOR_EACH_SYSPROC(dc_ptr, i) \
	for (i = 0; i < (dc_ptr->dc_usr.dc_nr_sysprocs); i++) \

#define PLOCK_PROC(p)		\
do {\
	DVKDEBUG(DBGPROCLOCK,"PLOCK_PROC ep=%d count=%ld\n",p->p_usr.p_endpoint,p->p_rwsem.count);\
}while(0);

/*--------------------------------------------------------- USE_DC_RWLOCKS ---------------------------------------*/
#if LOCK_DC_TYPE == USE_DC_RWLOCK
#define WUNLOCK_DC(d)	\
do {\
	write_unlock(&d->dc_rwlock);\
}while(0); 


#define RUNLOCK_DC(d)	\
do {\
	read_unlock(&d->dc_rwlock);\
}while(0); 
	
#define WLOCK_DC(d)		\
do {\
	write_lock(&d->dc_rwlock);\
}while(0);

#define RLOCK_DC(d)		\
do {\
	read_lock(&d->dc_rwlock);\
}while(0);

#define COPY_TO_USER_DC(ret, kaddr, uaddr, len )		\
do {\
	preempt_enable();\
 	ret = copy_to_user(uaddr, kaddr, len);\
	preempt_disable();\
}while(0)

#define DC_LOCK_INIT(d)	rwlock_init(&(d->dc_rwlock));

/*--------------------------------------------------------- USE_DC_MUTEX  ---------------------------------------*/
#elif LOCK_DC_TYPE == USE_DC_MUTEX  
#define WUNLOCK_DC(d)	\
do {\
	DVKDEBUG(DBGDCLOCK,"WUNLOCK_DC dc=%d count=%d\n",dc->dc_usr.dc_dcid, atomic_read(&d->dc_mutex.count));\
	mutex_unlock(&d->dc_mutex);\
}while(0); 

#define RUNLOCK_DC(d)	\
do {\
	DVKDEBUG(DBGDCLOCK,"RUNLOCK_DC dc=%d count=%d\n",dc->dc_usr.dc_dcid, atomic_read(&d->dc_mutex.count));\
	mutex_unlock(&d->dc_mutex);\
}while(0); 
	
#define WLOCK_DC(d)		\
do {\
	mutex_lock(&d->dc_mutex);\
	DVKDEBUG(DBGDCLOCK,"WLOCK_DC dc=%d count=%d\n",dc->dc_usr.dc_dcid, atomic_read(&d->dc_mutex.count));\
}while(0);

#define RLOCK_DC(d)		\
do {\
	mutex_lock(&d->dc_mutex);\
	DVKDEBUG(DBGDCLOCK,"RLOCK_DC dc=%d count=%d\n",dc->dc_usr.dc_dcid, atomic_read(&d->dc_mutex.count));\
}while(0);

#define COPY_TO_USER_DC(ret, kaddr, uaddr, len )		\
do {\
 	ret = copy_to_user(uaddr, kaddr, len);\
}while(0)

#define DC_LOCK_INIT(d)	mutex_init(&(d->dc_mutex));

/*--------------------------------------------------------- USE_DC_RWSEM  ---------------------------------------*/
#elif LOCK_DC_TYPE == USE_DC_RWSEM  
#define WUNLOCK_DC(d)	\
do {\
	DVKDEBUG(DBGDCLOCK,"WUNLOCK_DC dc=%d count=%ld\n",d->dc_usr.dc_dcid, d->dc_rwsem.count);\
	up_write(&d->dc_rwsem);\
}while(0); 

#define RUNLOCK_DC(d)	\
do {\
	DVKDEBUG(DBGDCLOCK,"RUNLOCK_DC dc=%d count=%ld\n",d->dc_usr.dc_dcid, d->dc_rwsem.count);\
	up_read(&d->dc_rwsem);\
}while(0); 
	
#define WLOCK_DC(d)		\
do {\
	down_write(&d->dc_rwsem);\
	DVKDEBUG(DBGDCLOCK,"WLOCK_DC dc=%d count=%ld\n",d->dc_usr.dc_dcid, d->dc_rwsem.count);\
}while(0);

#define RLOCK_DC(d)		\
do {\
	down_read(&d->dc_rwsem);\
	DVKDEBUG(DBGDCLOCK,"RLOCK_DC dc=%d count=%ld\n",d->dc_usr.dc_dcid, d->dc_rwsem.count);\
}while(0);

#define COPY_TO_USER_DC(ret, kaddr, uaddr, len )		\
do {\
 	ret = copy_to_user(uaddr, kaddr, len);\
}while(0)

#define DC_LOCK_INIT(d)	init_rwsem(&(d->dc_rwsem));



/*--------------------------------------------------------- USE_DC_RCU  ---------------------------------------*/
#elif LOCK_DC_TYPE == USE_DC_RCU  
#define WUNLOCK_DC(d)	\
do {\
	DVKDEBUG(DBGDCLOCK,"WUNLOCK_DC dc=%d\n",dc->dc_usr.dc_dcid);\
	spin_unlock(&d->dc_spinlock);\
	rcu_read_unlock();\
	synchronize_rcu();\
}while(0); 

#define RUNLOCK_DC(d)	\
do {\
	DVKDEBUG(DBGDCLOCK,"RUNLOCK_DC dc=%d\n",dc->dc_usr.dc_dcid);\
	rcu_read_unlock();\
}while(0); 
	
#define WLOCK_DC(d)		\
do {\
	rcu_read_lock();\
	spin_lock(&dc->dc_spinlock);\
	DVKDEBUG(DBGDCLOCK,"WLOCK_DC dc=%d\n",dc->dc_usr.dc_dcid);\
}while(0);

#define RLOCK_DC(d)		\
do {\
	rcu_read_lock();\
	DVKDEBUG(DBGDCLOCK,"RLOCK_DC dc=%d\n",dc->dc_usr.dc_dcid);\
}while(0);

#define COPY_TO_USER_DC(ret, kaddr, uaddr, len )		\
do {\
 	ret = copy_to_user(uaddr, kaddr, len);\
}while(0)

#define DC_LOCK_INIT(d)	spin_lock_init(&(d->dc_spinlock));

#endif

/*--------------------------------------------------------- USE_DVS_RWLOCKS ---------------------------------------*/
#if LOCK_DVS_TYPE == USE_DVS_RWLOCK
#define WUNLOCK_DVS	\
do {\
	write_unlock(&dvs_rwlock);\
}while(0); 

#define RUNLOCK_DVS	\
do {\
	read_unlock(&dvs_rwlock);\
}while(0); 
	
#define WLOCK_DVS		\
do {\
	write_lock(&dvs_rwlock);\
}while(0);

#define RLOCK_DVS		\
do {\
	read_lock(&dvs_rwlock);\
}while(0);

/*--------------------------------------------------------- USE_DVS_MUTEX  ---------------------------------------*/
#elif  LOCK_DVS_TYPE == USE_DVS_MUTEX
#define WUNLOCK_DVS	\
do {\
	mutex_unlock(&dvs_mutex);\
}while(0); 

#define RUNLOCK_DVS	\
do {\
	mutex_unlock(&dvs_mutex);\
}while(0); 
	
#define WLOCK_DVS		\
do {\
	mutex_lock(&dvs_mutex);\
}while(0);

#define RLOCK_DVS		\
do {\
	mutex_lock(&dvs_mutex);\
}while(0);

/*--------------------------------------------------------- USE_DVS_RWSEM  ---------------------------------------*/
#elif  LOCK_DVS_TYPE == USE_DVS_RWSEM
#define WUNLOCK_DVS	\
do {\
	up_write(&dvs_rwsem);\
}while(0); 

#define RUNLOCK_DVS	\
do {\
	up_read(&dvs_rwsem);\
}while(0); 
	
#define WLOCK_DVS		\
do {\
	down_write(&dvs_rwsem);\
}while(0);

#define RLOCK_DVS		\
do {\
	down_read(&dvs_rwsem);\
}while(0);

/*--------------------------------------------------------- USE_DVS_RCU  ---------------------------------------*/
#elif LOCK_DVS_TYPE == USE_DVS_RCU
#define WUNLOCK_DVS	\
do {\
	spin_unlock(&dvs_spinlock);\
	rcu_read_unlock();\
	synchronize_rcu();\
}while(0); 

#define RUNLOCK_DVS	\
do {\
	rcu_read_unlock();\
}while(0); 
	
#define WLOCK_DVS		\
do {\
	rcu_read_lock();\
	spin_lock(&dvs_spinlock);\
}while(0);

#define RLOCK_DVS		\
do {\
	rcu_read_lock();\
}while(0);

#endif

/*--------------------------------------------------------- USE_NODE_RWLOCKS ---------------------------------------*/
#if LOCK_NODE_TYPE == USE_NODE_RWLOCK
#define WUNLOCK_NODE(n)	\
do {\
	write_unlock(&n->n_rwlock);\
}while(0); 

#define RUNLOCK_NODE(n)	\
do {\
	read_unlock(&n->n_rwlock);\
}while(0); 
	
#define WLOCK_NODE(n)		\
do {\
	write_lock(&n->n_rwlock);\
}while(0);

#define RLOCK_NODE(n)		\
do {\
	read_lock(&n->n_rwlock);\
}while(0);

#define COPY_TO_USER_NODE(ret, kaddr, uaddr, len )		\
do {\
		preempt_enable();\
 	ret = copy_to_user(uaddr, kaddr, len);\
		preempt_disable();\
}while(0)
#define NODE_LOCK_INIT(n)	rwlock_init(&(n->n_rwlock));

/*--------------------------------------------------------- USE_NODE_MUTEX  ---------------------------------------*/
#elif LOCK_NODE_TYPE == USE_NODE_MUTEX  

#define WUNLOCK_NODE(n)	\
do {\
DVKDEBUG(DBGNODELOCK,"WUNLOCK_NODE node=%d count=%d\n",n->n_usr.n_nodeid, atomic_read(&n->n_mutex.count));\
	mutex_unlock(&n->n_mutex);\
}while(0); 

#define RUNLOCK_NODE(n)	\
do {\
	DVKDEBUG(DBGNODELOCK,"RUNLOCK_NODE node=%d count=%d\n",n->n_usr.n_nodeid, atomic_read(&n->n_mutex.count));\
	mutex_unlock(&n->n_mutex);\
}while(0); 
	
#define WLOCK_NODE(n)		\
do {\
	mutex_lock(&n->n_mutex);\
	DVKDEBUG(DBGNODELOCK,"WLOCK_NODE node=%d count=%d\n",n->n_usr.n_nodeid, atomic_read(&n->n_mutex.count));\
}while(0);

#define RLOCK_NODE(n)		\
do {\
	mutex_lock(&n->n_mutex);\
	DVKDEBUG(DBGNODELOCK,"RLOCK_NODE node=%d count=%d\n",n->n_usr.n_nodeid, atomic_read(&n->n_mutex.count));\
}while(0);

#define COPY_TO_USER_NODE(ret, kaddr, uaddr, len )		\
do {\
 	ret = copy_to_user(uaddr, kaddr, len);\
}while(0)

#define NODE_LOCK_INIT(n)	mutex_init(&(n->n_mutex))

/*--------------------------------------------------------- USE_NODE_RWSEM  ---------------------------------------*/
#elif LOCK_NODE_TYPE == USE_NODE_RWSEM

#define WUNLOCK_NODE(n)	\
do {\
DVKDEBUG(DBGNODELOCK,"WUNLOCK_NODE node=%d count=%ld\n",n->n_usr.n_nodeid, n->n_rwsem.count);\
	up_write(&n->n_rwsem);\
}while(0); 

#define RUNLOCK_NODE(n)	\
do {\
	DVKDEBUG(DBGNODELOCK,"RUNLOCK_NODE node=%d count=%ld\n",n->n_usr.n_nodeid, n->n_rwsem.count);\
	up_read(&n->n_rwsem);\
}while(0); 
	
#define WLOCK_NODE(n)		\
do {\
	down_write(&n->n_rwsem);\
	DVKDEBUG(DBGNODELOCK,"WLOCK_NODE node=%d count=%ld\n",n->n_usr.n_nodeid, n->n_rwsem.count);\
}while(0);

#define RLOCK_NODE(n)		\
do {\
	down_read(&n->n_rwsem);\
	DVKDEBUG(DBGNODELOCK,"RLOCK_NODE node=%d count=%ld\n",n->n_usr.n_nodeid, n->n_rwsem.count);\
}while(0);

#define COPY_TO_USER_NODE(ret, kaddr, uaddr, len )		\
do {\
 	ret = copy_to_user(uaddr, kaddr, len);\
}while(0)

#define NODE_LOCK_INIT(n)	init_rwsem(&(n->n_rwsem))

/*--------------------------------------------------------- USE_NODE_RCU ---------------------------------------*/
#elif LOCK_NODE_TYPE == USE_NODE_RCU
#define WUNLOCK_NODE(n)	\
do {\
	DVKDEBUG(DBGNODELOCK,"WUNLOCK_NODE node=%d\n",n->n_usr.n_nodeid);\
	spin_unlock(&n->n_spinlock);\
	rcu_read_unlock();\
	synchronize_rcu();\
}while(0); 

#define RUNLOCK_NODE(n)	\
do {\
	DVKDEBUG(DBGNODELOCK,"RUNLOCK_NODE node=%d\n",n->n_usr.n_nodeid);\
	rcu_read_unlock();\
}while(0); 
	
#define WLOCK_NODE(n)		\
do {\
	rcu_read_lock();\
	spin_lock(&n->n_spinlock);\
	DVKDEBUG(DBGNODELOCK,"WLOCK_NODE node=%d\n",n->n_usr.n_nodeid);\
}while(0);

#define RLOCK_NODE(n)		\
do {\
	rcu_read_lock();\
	DVKDEBUG(DBGNODELOCK,"RLOCK_NODE node=%d\n",n->n_usr.n_nodeid);\
}while(0);

#define COPY_TO_USER_NODE(ret, kaddr, uaddr, len )		\
do {\
 	ret = copy_to_user(uaddr, kaddr, len);\
}while(0)

#define NODE_LOCK_INIT(n)	spin_lock_init(&(n->n_spinlock));

#endif

/*--------------------------------------------------------- USE_PROXY_RWLOCKS ---------------------------------------*/
#if LOCK_PROXY_TYPE == USE_PROXY_RWLOCK
#define WUNLOCK_PROXY(px)	\
do {\
	write_unlock(&px->px_rwlock);\
}while(0); 

#define RUNLOCK_PROXY(px)	\
do {\
	read_unlock(&px->px_rwlock);\
}while(0); 
	
#define WLOCK_PROXY(px)		\
do {\
	write_lock(&px->px_rwlock);\
}while(0);

#define RLOCK_PROXY(px)		\
do {\
	read_lock(&px->px_rwlock);\
}while(0);

#define PROXY_LOCK_INIT(px)	rwlock_init(&(px->px_rwlock))

/*--------------------------------------------------------- USE_PROXY_MUTEX  ---------------------------------------*/
#elif LOCK_PROXY_TYPE == USE_PROXY_MUTEX
#define WUNLOCK_PROXY(px)	\
do {\
	DVKDEBUG(DBGPROXYLOCK,"WUNLOCK_PROXY pxid=%d count=%d\n",px->px_usr.px_id, atomic_read(&px->px_mutex.count));\
	mutex_unlock(&px->px_mutex);\
}while(0); 

#define RUNLOCK_PROXY(px)	\
do {\
	DVKDEBUG(DBGPROXYLOCK,"RUNLOCK_PROXY pxid=%d count=%d\n",px->px_usr.px_id, atomic_read(&px->px_mutex.count));\
	mutex_unlock(&px->px_mutex);\
}while(0); 
	
#define WLOCK_PROXY(px)		\
do {\
	mutex_lock(&px->px_mutex);\
	DVKDEBUG(DBGPROXYLOCK,"WLOCK_PROXY pxid=%d count=%d\n",px->px_usr.px_id, atomic_read(&px->px_mutex.count));\
}while(0);

#define RLOCK_PROXY(px)		\
do {\
	mutex_lock(&px->px_mutex);\
	DVKDEBUG(DBGPROXYLOCK,"RLOCK_PROXY pxid=%d count=%d\n",px->px_usr.px_id, atomic_read(&px->px_mutex.count));\
}while(0);

#define PROXY_LOCK_INIT(px)	mutex_init(&(px->px_mutex))

/*--------------------------------------------------------- USE_PROXY_RWSEM  ---------------------------------------*/
#elif LOCK_PROXY_TYPE == USE_PROXY_RWSEM
#define WUNLOCK_PROXY(px)	\
do {\
	DVKDEBUG(DBGPROXYLOCK,"WUNLOCK_PROXY pxid=%d count=%ld\n",px->px_usr.px_id, px->px_rwsem.count);\
	up_write(&px->px_rwsem);\
}while(0); 

#define RUNLOCK_PROXY(px)	\
do {\
	DVKDEBUG(DBGPROXYLOCK,"RUNLOCK_PROXY pxid=%d count=%ld\n",px->px_usr.px_id, px->px_rwsem.count);\
	up_read(&px->px_rwsem);\
}while(0); 
	
#define WLOCK_PROXY(px)		\
do {\
	down_write(&px->px_rwsem);\
	DVKDEBUG(DBGPROXYLOCK,"WLOCK_PROXY pxid=%d count=%ld\n",px->px_usr.px_id, px->px_rwsem.count);\
}while(0);

#define RLOCK_PROXY(px)		\
do {\
	down_read(&px->px_rwsem);\
	DVKDEBUG(DBGPROXYLOCK,"RLOCK_PROXY pxid=%d count=%ld\n",px->px_usr.px_id, px->px_rwsem.count);\
}while(0);

#define PROXY_LOCK_INIT(px)	init_rwsem(&(px->px_rwsem))

/*--------------------------------------------------------- USE_PROXY_RCU  ---------------------------------------*/
#elif LOCK_PROXY_TYPE == USE_PROXY_RCU
#define WUNLOCK_PROXY(px)	\
do {\
	DVKDEBUG(DBGPROXYLOCK,"WUNLOCK_PROXY pxid=%d\n",px->px_usr.px_id);\
	spin_unlock(&px->px_spinlock);\
	rcu_read_unlock();\
	synchronize_rcu();\
}while(0); 

#define RUNLOCK_PROXY(px)	\
do {\
	DVKDEBUG(DBGPROXYLOCK,"RUNLOCK_PROXY pxid=%d\n",px->px_usr.px_id);\
	rcu_read_unlock();\
}while(0); 
	
#define WLOCK_PROXY(px)		\
do {\
	rcu_read_lock();\
	spin_lock(&px->px_spinlock);\
	DVKDEBUG(DBGPROXYLOCK,"WLOCK_PROXY pxid=%d\n",px->px_usr.px_id);\
}while(0);

#define RLOCK_PROXY(px)		\
do {\
	rcu_read_lock();\
	DVKDEBUG(DBGPROXYLOCK,"RLOCK_PROXY pxid=%d\n",px->px_usr.px_id);\
}while(0);

#define PROXY_LOCK_INIT(px)	spin_lock_init(&(px->px_spinlock));

#endif

/*--------------------------------------------------------- LISTS ---------------------------------------*/
#if LIST_TYPE == USE_LIST_RCU

#define LIST_ADD(x,y) 						list_add_rcu(x, y)
#define LIST_ADD_TAIL(x,y) 					list_add_tail_rcu(x, y)
#define LIST_DEL(x) 						list_del_rcu(x)
#define LIST_DEL_INIT(x) 					list_del_init_rcu(x)
#define LIST_FOR_EACH_ENTRY_SAFE(x,y,z,t) 	list_for_each_entry_safe_rcu(x,y,z,t)

#else

#define LIST_ADD(x,y) 						do {\
		DVKDEBUG(DBGPROCSEM,"LIST_ADD\n"); \
		list_add(x, y);\
}while(0);		
		
#define LIST_ADD_TAIL(x,y) 					do {\
		DVKDEBUG(DBGPROCSEM,"LIST_ADD_TAIL\n"); \
		list_add_tail(x, y);\
}while(0);		

#define LIST_DEL(x) 						do {\
		DVKDEBUG(DBGPROCSEM,"LIST_DEL\n"); \
		list_del(x);\
}while(0);

#define LIST_DEL_INIT(x) 			\
do {\
		DVKDEBUG(DBGPROCSEM,"LIST_DEL_INIT\n"); \
		list_del_init(x);\
}while(0);

#define LIST_FOR_EACH_ENTRY_SAFE(x,y,z,t) 	list_for_each_entry_safe(x,y,z,t)
#endif

