/* This file deals with creating processes (via FORK) and deleting them (via
 * EXIT/WAIT).  When a process forks, a new slot in the 'pm_proc_table' table is
 * allocated for it, and a copy of the parent's core image is made for the
 * child.  Then the kernel and file system are informed.  A process is removed
 * from the 'pm_proc_table' table when two events have occurred: (1) it has exited or
 * been killed by a signal, and (2) the parent has done a WAIT.  If the process
 * exits first, it continues to occupy a slot until the parent does a WAIT.
 *
 * The entry points into this file are:
 *   pm_fork:	 perform the FORK system call
 *   pm_pm_exit: perform the EXIT system call (by calling pm_exit())
 *   pm_exit:	 actually do the exiting
 *   pm_wait:	 perform the WAITPID or WAIT system call
 */
#include "pm.h"

#undef DVK_GLOBAL_HERE
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

void cleanup(mproc_t *child);

#define FORK_WAIT_MS 1000
#define LAST_FEW            2	/* last few slots reserved for superuser */
#define BIND_TO_SYSTASK		1 	

/*===========================================================================*
 *				pm_waitpid				     *
 *===========================================================================*/
int pm_waitpid(void)
{
/* A process wants to wait for a child to terminate. If a child is already 
 * exited, go clean it up and let this WAIT call terminate.  Otherwise, 
 * really wait. 
 * A process calling WAIT never gets a reply in the usual way at the end
 * of the main loop (unless WNOHANG is set or no qualifying child exists).
 * If a child has already exited, the routine cleanup() sends the reply
 * to awaken the caller.
 * Both WAIT and WAITPID are handled by this code.
 */
  register mproc_t *rp;
  int pidarg, options, children;

MUKDEBUG("pm_who_e=%d pm_call_nr=%d pid=%d sig_nr=%d\n",pm_who_e,pm_call_nr,pm_m_in.pid,pm_m_in.sig_nr);

  /* Set internal variables, depending on whether this is WAIT or WAITPID. */
  pidarg  = (pm_call_nr == MOLWAIT ? -1 : pm_m_in.pid);	   /* 1st param of waitpid */
  options = (pm_call_nr == MOLWAIT ?  0 : pm_m_in.sig_nr);  /* 3rd param of waitpid */
  if (pidarg == 0) pidarg = -mp->mp_procgrp;	/* pidarg < 0 ==> proc grp */

  /* Is there a child waiting to be collected? At this point, pidarg != 0:
   *	pidarg  >  0 means pidarg is pid of a specific process to wait for
   *	pidarg == -1 means wait for any child
   *	pidarg  < -1 means wait for any child whose process group = -pidarg
   */
  children = 0;
  for (rp = &pm_proc_table[0]; rp < &pm_proc_table[dc_ptr->dc_nr_procs]; rp++) {
	if ( rp->mp_parent == pm_who_p) {
	MUKDEBUG(PM_PROC_FORMAT,PM_PROC_FIELDS(rp));
	if ( rp->mp_flags & IN_USE ) {
		MUKDEBUG("The value of pidarg determines which children qualify pidarg=%d\n", pidarg);
		/* The value of pidarg determines which children qualify. */
		if (pidarg  > 0 && pidarg != rp->mp_pid) continue;
		if (pidarg < -1 && -pidarg != rp->mp_procgrp) continue;

		MUKDEBUG("this child is acceptable =%d\n", rp->mp_pid);
		children++;		/* this child is acceptable */
		if (rp->mp_flags & ZOMBIE) {
			/* This child meets the pid test and has exited. */
			cleanup(rp);	/* this child has already exited */
			return(SUSPEND);
		}
		if ((rp->mp_flags & STOPPED) && rp->mp_sigstatus) {
			/* This child meets the pid test and is being traced.*/
			mp->mp_reply.reply_res2 = 0177|(rp->mp_sigstatus << 8);
			rp->mp_sigstatus = 0;
			return(rp->mp_pid);
		}
	}
	}
  }

MUKDEBUG("No qualifying child has exited.  Wait for one.\n");
  /* No qualifying child has exited.  Wait for one, unless none exists. */
  if (children > 0) {
	/* At least 1 child meets the pid test exists, but has not exited. */
	if (options & WNOHANG) return(0);    /* parent does not want to wait */
	mp->mp_flags |= WAITING;	     /* parent wants to wait */
	mp->mp_wpid = (pid_t) pidarg;	     /* save pid for later */
	MUKDEBUG("do not reply, let it wait.\n");
	return(SUSPEND);		     /* do not reply, let it wait */
  } else {
	/* No child even meets the pid test.  Return error immediately. */
	return(EDVSCHILD);			     /* no - parent has no children */
  }
}

/*===========================================================================*
 *				pm_fork			    			*
 * ONLY used to create USER PROCESSES CHILDREN		*
 * pm_m_in.PR_LPID: Linux PID of child			     		*
 *===========================================================================*/
int pm_fork(void)
{
	/* The process pointed to by 'mp' has forked.  Create a child process. */
  	mproc_t *rmp, *rmc;
  	proc_usr_t *rkp, *rkc;	
	int  child_lpid, new_pid, child_nr, child_ep;
	int rcode;
	message *pm_msg_ptr;
	
	rkp = (proc_usr_t *) PM_KPROC(pm_who_p);
	/* Tell SYSTASK copy the kernel entry to pm_kproc[parent_nr]  	*/
  	if((rcode =sys_procinfo(pm_who_p)) != OK) 
		ERROR_EXIT(rcode);
	MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(rkp));
	pm_msg_ptr  = &pm_m_in;
	MUKDEBUG(MSG3_FORMAT,MSG3_FIELDS(pm_msg_ptr));
	child_lpid = pm_m_in.M3_LPID;
	
	MUKDEBUG("pm_who_p=%d child_lpid=%d M3_ENDPT=%d\n", pm_who_p, child_lpid, pm_m_in.M3_ENDPT);
	if(!TEST_BIT( rkp->p_rts_flags,BIT_REMOTE)) {
		/* Tell SYSTASK  about the (now successful) FORK, waiting endpoint */
		child_ep = sys_fork(child_lpid);
		MUKDEBUG("child_ep=%d child_lpid=%d \n", child_ep, child_lpid);
		if(child_ep < 0) ERROR_RETURN(child_ep);
	}else{	
		/* Request REMOTE SYSTASK to LOCALLY bind the process an allocate an endpoint */
		child_ep = sys_rfork(child_lpid, rkp->p_nodeid);
		if( child_ep < OK) ERROR_RETURN(child_ep);
		/* Request LOCAL SYSTASK to REMOTE bind the process with the allocated endpoint */
		MUKDEBUG("child_ep=%d p_nodeid=%d \n", child_ep, rkp->p_nodeid);
		rcode = sys_bindproc(child_ep, rkp->p_nodeid, RMT_BIND);
		if(rcode < 0) ERROR_RETURN(rcode);
		rcode = sys_rsetpname(child_ep, pm_m_in.M3_NAME, local_nodeid);
		if(rcode < 0) ERROR_RETURN(rcode);
	}

	child_nr = _ENDPOINT_P(child_ep);	
		
	MUKDEBUG("child_nr=%d child_ep=%d child_lpid=%d\n", 
		child_nr, child_ep, child_lpid);

	if(TEST_BIT( rkp->p_rts_flags,BIT_REMOTE)) {
		rmp = &pm_proc_table[INIT_PROC_NR];
	}else{
		rmp = mp;	
	}
	rmc = &pm_proc_table[child_nr];
	rkc =  (proc_usr_t *) PM_KPROC(child_nr);
	
  	/* Set up the child ; copy its 'pm_proc_table' slot from parent. */
  	*rmc = *rmp;			/* copy parent's process slot to child's */
	
	if(TEST_BIT( rkp->p_rts_flags,BIT_REMOTE)) {
		rmc->mp_parent = INIT_PROC_NR;	/* record child's parent */				
	}else{
		rmc->mp_parent = pm_who_p;			/* record child's parent */		
	}

	if((child_nr+dc_ptr->dc_nr_tasks) < dc_ptr->dc_nr_sysprocs) 
		rmc->mp_flags = (IN_USE | PRIV_PROC | FORKWAIT);
	else 
		rmc->mp_flags = (IN_USE | FORKWAIT);
	
  	rmc->mp_child_utime 	= 0;		/* reset administration */
  	rmc->mp_child_stime 	= 0;		/* reset administration */
	rmc->mp_exitstatus 	= 0;
  	rmc->mp_sigstatus 	= 0;
	/* Find a free pid for the child and put it in the table. */
  	new_pid = get_free_pid();
  	rmc->mp_pid = new_pid;			/* assign pid to child */
	rmc->mp_endpoint = child_ep;
	/* Tell SYSTASK copy the kernel entry to pm_kproc[child_nr]   	*/
  	if((rcode =sys_procinfo(child_nr)) != OK) 
		ERROR_EXIT(rcode);
	
	if( child_ep != rkc->p_endpoint) ERROR_EXIT(EDVSINVAL);

  	/* Tell file system about the (now successful) FORK. */
	tell_fs(MOLFORK, pm_who_e, child_ep, rmc->mp_pid);

  	rmp->mp_reply.endpt = rkc->p_endpoint;	/* child's process endpoint */
	MUKDEBUG(PM_PROC_FORMAT,PM_PROC_FIELDS(rmc));

  	return(new_pid);		 	/* child's pid */
}

/*===========================================================================*
 *				pm_wait4fork			    			*
 * ONLY used by a child to be woken up to start running 		*
 * M3_LPID  = child_lpid;
 * M3_ENDPT = child_ep;
 * m3_ca1, program name
 *===========================================================================*/
int pm_wait4fork(void)
{
	/* The process pointed to by 'mp' has forked.  Create a child process. */
  	mproc_t  *rmc;
  	proc_usr_t *rkc;	
	int  child_lpid, child_nr, child_ep;
	int rcode;
	message *pm_msg_ptr;
	
	rkc =  (proc_usr_t *) PM_KPROC(pm_who_p);
	/* Tell SYSTASK copy the kernel entry to pm_kproc[parent_nr]  	*/
  	if((rcode =sys_procinfo(pm_who_p)) != OK) 
		ERROR_EXIT(rcode);
	MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(rkc));
	pm_msg_ptr  = &pm_m_in;
	MUKDEBUG(MSG3_FORMAT,MSG3_FIELDS(pm_msg_ptr));
	child_lpid = pm_m_in.M3_LPID;
	child_ep = pm_m_in.M3_ENDPT;
	child_nr = _ENDPOINT_P(pm_m_in.M3_ENDPT);	
	
	MUKDEBUG("pm_who_p=%d child_nr=%d\n", pm_who_p, child_nr);
	if( child_ep !=  pm_who_e)
		ERROR_RETURN(EDVSCHILD);
	
	rmc = &pm_proc_table[child_nr];
	if (!(rmc->mp_flags&FORKWAIT))
		ERROR_RETURN(EDVSPROCSTS);

	rmc->mp_flags &= ~FORKWAIT;

  	return(rmc->mp_pid);		 	/* child's pid */
}

/*===========================================================================*
 *				pm_pm_exit				     *
 * status		m1_i1						     *
 * lnx_pid		m1_i2	(-1 if who is the exiting process)	     *
 *===========================================================================*/
int pm_pm_exit(void)
{
/* Perform the taskexit(status) system call. The real work is done by pm_exit(),
 * which is also called when a process is killed by a signal.
 */
  	proc_usr_t *rkp;	
 	int i, ret;

	MUKDEBUG("pm_who_p=%d pm_who_e=%d lnx_pid=%d status=%d\n",
			pm_who_p, pm_who_e, pm_m_in.lnx_pid, pm_m_in.status);

	/* Reject with return OK any system task including PM0 */			
	if(pm_who_p <= 0) return(OK);

	if( pm_m_in.lnx_pid != (-1) ) { /*  OTHER PROCESS */
		for ( i = 0; i < (dc_ptr->dc_nr_procs); i++) {
			rkp =  (proc_usr_t *) PM_KPROC(i);
			if(rkp->p_lpid == pm_m_in.lnx_pid) {
				ret = pm_exit(&pm_proc_table[i], pm_m_in.status, MOLEXIT);
				if(ret != OK)
					ERROR_RETURN(ret);
				return(SUSPEND);
			}
		}
		ERROR_RETURN(EDVSBADPID);
	}else { 					/*  SELF PROCESS */
		ret = pm_exit(mp, pm_m_in.status, MOLEXIT);
		if(ret != OK)
			ERROR_RETURN(ret);
	}
	
    return(SUSPEND);
}

/*===========================================================================*
 *				pm_unbind				     *
 * p_ep		m1_i1						     *
 *===========================================================================*/
int pm_unbind(void)
{
/* Perform the taskexit(status) system call. The real work is done by pm_exit(),
 * which is also called when a process is killed by a signal.
 */
  	proc_usr_t *rkp;	
 	int  ret;

	MUKDEBUG("pm_who_p=%d pm_who_e=%d p_ep=%d\n",
			pm_who_p, pm_who_e, pm_m_in.m1_i1);

	/* Reject with return OK any system task including PM0 */
	if(pm_m_in.m1_i1 <= 0) return(EDVSBADPROC);

	ret = 0;
	if( pm_m_in.m1_i1 != SELF ) { /*  OTHER PROCESS */
		rkp =  (proc_usr_t *) PM_KPROC(_ENDPOINT_P(pm_m_in.m1_i1));
		ret = pm_exit(&pm_proc_table[rkp->p_nr], 0, MOLUNBIND);
		if(ret != OK)
			ERROR_RETURN(ret);
		return(ret);
	}else { 					/*  SELF PROCESS */
		ret = pm_exit(mp, 0, MOLUNBIND);
		if(ret != OK)
			ERROR_RETURN(ret);
	}
	
    return(ret);
}

/*===========================================================================*
 *				pm_exit					     *
 * mproc_t *rmp;	 pointer to the process to be terminated 	     *
 * exit_status;		 the process' taskexit status (for parent) 		     *
 *===========================================================================*/
int pm_exit(mproc_t *rmp, int exit_status, int oper )
{
	/* A process is done.  Release most of the process' possessions.  If its
 	* parent is waiting, release the rest, else keep the process slot and
 	* become a zombie.
 	*/
  	int proc_nr, proc_ep;
  	int parent_waiting, right_child, rcode;
 	int pidarg, procgrp;
  	mproc_t *p_mp, *rcp;
  	proc_usr_t *rkp;
  	molclock_t t[5];

  	proc_nr = (int) (rmp - pm_proc_table);	/* get process slot number */
	rkp =  (proc_usr_t *) PM_KPROC(proc_nr);
  	proc_ep = rkp->p_endpoint;
	MUKDEBUG("mnx_pid=%d status=%d proc_nr=%d proc_ep=%d oper=%d\n", 
		rmp->mp_pid, exit_status, proc_nr, proc_ep, oper);

  	/* Remember a session leader's process group. */
  	procgrp = (rmp->mp_pid == mp->mp_procgrp) ? mp->mp_procgrp : 0;

  	/* If the exited process has a timer pending, kill it. */
  	if (rmp->mp_flags & ALARM_ON) 
		set_alarm(proc_ep, (unsigned) 0);

  	/* Do accounting: fetch usage times and accumulate at parent. */
//  	if((rcode =sys_times(proc_ep, t)) != OK)
//  		ERROR_EXIT(rcode);
	
	p_mp = &pm_proc_table[rmp->mp_parent];				/* process' parent */
	MUKDEBUG("parent_pid=%d\n", p_mp->mp_pid);

  	p_mp->mp_child_utime += t[0] + rmp->mp_child_utime;	/* add user time */
 	p_mp->mp_child_stime += t[1] + rmp->mp_child_stime;	/* add system time */

  	/* Tell the kernel the process is no longer runnable to prevent it from 
   	* being scheduled in between the following steps. Then tell FS that it 
   	* the process has exited and finally, clean up the process at the kernel.
   	* This order is important so that FS can tell drivers to cancel requests
   	* such as copying to/ from the exiting process, before it is gone.
   	*/

//  	sys_nice(proc_ep, PRIO_STOP);			/* stop the process */

//	tell_fs(MOLEXIT, proc_ep, 0, 0);  		/* tell FS to free the slot */

	/* Tell SYSTASK to unbind the exiting process	*/
	MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(rkp));
	
	rcode = _sys_exit(rkp->p_endpoint, rkp->p_nodeid);
			
	if(rcode < 0 && rcode != EDVSNOTBIND  && rcode != EDVSDSTDIED)		
  		ERROR_EXIT(rcode);

	/* Tell SYSTASK copy the kernel entry to pm_kproc[proc_nr]   	*/
  	if((rcode =sys_procinfo(proc_nr)) != OK) 
		ERROR_EXIT(rcode);
		
	MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(rkp));
	
	if( TEST_BIT( rkp->p_rts_flags,BIT_SLOT_FREE)) {
		rmp->mp_flags &= ~REPLYPENDING;
	}
	
  	/* Pending reply messages for the LOCAL dead process cannot be delivered. */
	if( ! (rmp->mp_flags & RMT_PROC ))
		rmp->mp_flags &= ~REPLYPENDING;
    
	if( oper == MOLEXIT){
		/* The process slot can only be freed if the parent has done a WAIT. */
		rmp->mp_exitstatus = (char) exit_status;

		pidarg = p_mp->mp_wpid;				/* who's being waited for? */
		parent_waiting = p_mp->mp_flags & WAITING;
		right_child =				/* child meets one of the 3 tests? */
		(pidarg == -1 || pidarg == rmp->mp_pid || -pidarg == rmp->mp_procgrp);

		if ((parent_waiting && right_child)  || 
			(rmp->mp_flags & RMT_PROC )){
			cleanup(rmp);					/* tell parent and release child slot */
		} else {
			if(p_mp->mp_endpoint != pm_ep) {
				rmp->mp_flags = IN_USE|ZOMBIE;	/* parent not waiting, zombify child */
				sig_proc(p_mp, SIGCHLD);		/* send parent a "child died" signal */
			}
		}
	}else{ // MOLUNBIND 
		cleanup(rmp);					/* tell parent and release child slot */
	}
	
  	/* If the process has children, disinherit them.  INIT is the new parent. */
  	for (rcp = &pm_proc_table[0]; rcp < &pm_proc_table[dc_ptr->dc_nr_procs]; rcp++) {
		if (rcp->mp_parent == proc_nr) {
			MUKDEBUG(PM_PROC_FORMAT,PM_PROC_FIELDS(rcp));
			if (rcp->mp_flags & IN_USE ) {
				/* 'rcp' now points to a child to be disinherited. */
				rcp->mp_parent = INIT_PROC_NR;
				parent_waiting = pm_proc_table[INIT_PROC_NR].mp_flags & WAITING;
				if (parent_waiting && (rcp->mp_flags & ZOMBIE)) cleanup(rcp);
			}
		}
  	}

  	/* Send a hangup to the process' process group if it was a session leader. */
  	if (procgrp != 0) check_sig(-procgrp, SIGHUP);
	
	/* WARNING: on MOL, Servers and tasks do not have parents (????????) */
	if( (proc_nr+dc_ptr->dc_nr_tasks) < (dc_ptr->dc_nr_sysprocs))
		rmp->mp_flags = 0;
	MUKDEBUG(PM_PROC1_FORMAT,PM_PROC1_FIELDS(rmp));
	
	return(OK);
	
}

/*===========================================================================*
 *				cleanup					     *
 * mproc_t *child;	tells which process is exiting 			     *
 *===========================================================================*/
void cleanup(mproc_t *child)
{
	/* Finish off the taskexit of a process.  The process has exited or been killed
 	* by a signal, and its parent is waiting.
 	*/
  	int exitstatus;
  	mproc_t *parent = &pm_proc_table[child->mp_parent];

	MUKDEBUG("parent_pid=%d\n", parent->mp_pid);
	if( ! (child->mp_flags & RMT_PROC )) {
		/* Wake up the parent by sending the reply message. */
		exitstatus = (child->mp_exitstatus << 8) | (child->mp_sigstatus & 0377);
		parent->mp_reply.reply_res2 = exitstatus;
		setreply(child->mp_parent, child->mp_pid);
		parent->mp_flags &= ~WAITING;		/* parent no longer waiting */
	}
	
  	/* Release the process table entry and reinitialize some field. */
  	child->mp_pid = 0;
  	child->mp_flags = 0;
  	child->mp_child_utime = 0;
  	child->mp_child_stime = 0;
	child->mp_endpoint = NONE;

  	procs_in_use--;

}

/*===========================================================================*
 *				pm_bindproc				     *
 * pm_m_in.M7_PID: stablished MINIX PID o (PROC_NO_PID)			     *
 * pm_m_in.M7_LPID: Linux PID 						     *
 * pm_m_in.M7_SLOT < dc_ptr->dc_nr_sysprocs-dc_ptr->dc_nr_tasks *
 * pm_m_in.M7_OPER: oper (bind type as in minix/com.h) 						     *
 *===========================================================================*/
int pm_bindproc(void)
{
	/* The process pointed to by 'mp' binded to the kernel but not to 
	PM and SYSTASK.  Register the process (sysproc) into PM */
	message *pm_msg_ptr;
  	mproc_t *msp;
  	proc_usr_t *ksp, *psp;	
	char *sig_ptr;
	static char mess_sigs[] = { SIGTERM, SIGHUP, SIGABRT, SIGQUIT };
	int  lpid, new_pid, sysproc_nr, sysproc_ep, rcode, oper;

	MUKDEBUG("\n");

	pm_msg_ptr 	= &pm_m_in;
	lpid 		= pm_m_in.M7_LPID;
	new_pid 	= pm_m_in.M7_PID;
	sysproc_nr 	= pm_m_in.M7_SLOT;
	oper		= pm_m_in.M7_OPER;
	MUKDEBUG(MSG7_FORMAT, MSG7_FIELDS(pm_msg_ptr));

	if( (oper < 0) | (oper > MAX_BIND_TYPE) )
		ERROR_RETURN(EDVSBINDTYPE);
	
	if ( (sysproc_nr+dc_ptr->dc_nr_tasks) >= (dc_ptr->dc_nr_sysprocs)) 	
		ERROR_RETURN(EDVSBADPROC);

	/* Tell SYSTASK to copy the kernel entry to pm_kproc[sysproc_nr]   */
  	if((rcode = sys_procinfo(sysproc_nr)) != OK) 
		ERROR_EXIT(rcode);
	
	msp = &pm_proc_table[sysproc_nr];
	ksp =  (proc_usr_t *) PM_KPROC(sysproc_nr);
	if( ksp->p_rts_flags != SLOT_FREE) {
		MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(ksp));
		MUKDEBUG(PM_PROC_FORMAT,PM_PROC_FIELDS(msp));
		if(! (ksp->p_rts_flags & BIT_REMOTE)){
			if(ksp->p_lpid != lpid) {
				ERROR_RETURN(EDVSEXIST);
			}else{
				assert( !(msp->mp_flags & IN_USE));
			}
		}	
	}
//	else {
		/* Bind the sysproc into SYSTASK, wait for endpoint */
		if((sysproc_ep = sys_bindproc(sysproc_nr, lpid, oper)) < 0 ){
			mproc_init(sysproc_nr);
			ERROR_RETURN(sysproc_ep);
		}
		MUKDEBUG("sysproc_nr=%d sysproc_ep=%d new_pid=%d\n", sysproc_nr, sysproc_ep, new_pid);
//	}
	/* Tell SYSTASK to copy the kernel entry to pm_kproc[sysproc_nr]   */
	if((rcode = sys_procinfo(sysproc_nr)) != OK) 
		ERROR_EXIT(rcode);
	assert( sysproc_nr == ksp->p_nr);
		
	MUKDEBUG("sysproc_nr=%d\n", sysproc_nr);
	
  	/* Set up the sysproc */
	psp = (proc_usr_t *) PM_KPROC(pm_ep);
  	msp->mp_parent = psp->p_endpoint;	/* PM is the parent of all sysprocs */
  	/* inherit only these flags */
 	msp->mp_flags 		= (IN_USE|PRIV_PROC);
  	msp->mp_child_utime = 0;		/* reset administration */
  	msp->mp_child_stime = 0;		/* reset administration */
	msp->mp_exitstatus 	= 0;
  	msp->mp_sigstatus 	= 0;
	msp->mp_nice = PRIO_SERVER;
	msp->mp_endpoint = ksp->p_endpoint;

	sigemptyset(&msp->mp_sig2mess);
  	sigemptyset(&msp->mp_ignore);	
  	sigemptyset(&msp->mp_sigmask);
  	sigemptyset(&msp->mp_catch);
	for (sig_ptr = mess_sigs; 
	     sig_ptr < mess_sigs+sizeof(mess_sigs); 
	     sig_ptr++) {
			sigaddset(&mp->mp_sig2mess, *sig_ptr);
	}
	sigfillset(&msp->mp_ignore); 	/* guard against signals */
	MUKDEBUG(PM_PROC_FORMAT,PM_PROC_FIELDS(msp));
	procs_in_use++;

	/* Find a free pid for the sysproc and put it in the table. */
	if( new_pid == PROC_NO_PID) {
	  	new_pid = get_free_pid();
	}
  	msp->mp_pid = new_pid;			/* assign pid to sysproc */
	
  	/* Tell kernel and file system about the (now successful) FORK. */
	// FORK, parent_ep, child_ep, pid 
	if( sysproc_ep != fs_ep) 
		tell_fs(MOLFORK, pm_ep, sysproc_ep, msp->mp_pid);

	/* Reply to sysproc to wake it up. */
// ANULADO POR AHORA 	setreply(sysproc_nr, 0);/* only parent gets details */
  	msp->mp_reply.endpt = ksp->p_endpoint;	/* sysproc's process endpoint */

  	return(new_pid);		 	/* sysproc's pid */
}

/*===========================================================================*
 *				pm_freeproc				     *
 * A parent process has done a fork(), but needs the child_pr to bind it     *
 * PM gets a free slot, sets it as RESERVED and returns to parent	     *
 * m.PR_SLOT: minix slot number						     *
 *===========================================================================*/
int pm_freeproc(void)
{
	/* The process pointed to by 'mp' has forked.  Create a child process. */
  	mproc_t *rmp, *rmc;
	int  child_nr;
	int next_free;

	MUKDEBUG("pm_who_p=%d\n", pm_who_p);

 	/* If tables might fill up during FORK, don't even start since recovery half
  	* way through is such a nuisance.
  	*/
  	rmp = mp;
  	if ((procs_in_use == dc_ptr->dc_nr_procs) || 
  		(procs_in_use >= (dc_ptr->dc_nr_procs-LAST_FEW) && rmp->mp_effuid != 0))
  		{
  		printf("PM: warning, process table is full!\n");
  		ERROR_RETURN(EDVSAGAIN);
  	}

	/* Find a slot in 'pm_proc_table' for the child process.  A slot must exist. */
//    next_free = get_free_proc();
	next_free = 0; /* PARA EVITAR ERROR DE LINKEDICION */
	if(next_free < 0) ERROR_RETURN(next_free);

MUKDEBUG("next_free=%d\n", next_free);
		
	rmc = &pm_proc_table[next_free];

  	/* Set up the child ; copy its 'pm_proc_table' slot from parent. */
  	child_nr = (int)(rmc - pm_proc_table);	/* slot number of the child */
  	procs_in_use++;
  	*rmc = *rmp;			/* copy parent's process slot to child's */
  	rmc->mp_parent = pm_who_p;		/* record child's parent */
  	/* inherit only these flags */
 	rmc->mp_flags &= (IN_USE|PRIV_PROC|FORKWAIT);

  	rmp->mp_reply.PR_SLOT = child_nr;	/* child's process endpoint */

MUKDEBUG("child_nr=%d\n",  child_nr);

  	return( child_nr);		 	/* child's pid */
}





