/* This file contains the main program of the process manager and some related
 * procedures.  When MINIX starts up, the kernel runs for a little while,
 * initializing itself and its tasks, and then it runs PM and FS.  Both PM
 * and FS initialize themselves as far as they can. PM asks the kernel for
 * all free memory and starts serving requests.
 *
 * The entry points into this file are:
 *   main:	starts PM running
 *   setreply:	set the reply to be sent to process making an PM system call
 */
 
#define DVK_GLOBAL_HERE
#include "pm.h"

#undef DVK_GLOBAL_HERE
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"


void pm_init(void);
void pm_get_work(void);
		
/*===========================================================================*
 *				main_pm					     *
 *===========================================================================*/
int main_pm ( int argc, char *argv[] )
{
	int rcode, result, proc_nr;
	mnxsigset_t sigset;
	mproc_t *rmp;	
	muk_proc_t *rkp;	
	struct timespec t; 
	long long tt, td;

	if( argc != 1) {
		ERROR_PRINT(EDVSENDPOINT);
		taskexit(&pm_ep);
	}
	
	pm_init();
	
	// SYNCHRONIZE WITH MUK
	MTX_LOCK(muk_mutex);
	COND_SIGNAL(muk_cond);
	COND_WAIT(pm_cond, muk_mutex);
	MTX_UNLOCK(muk_mutex);
	
MUKDEBUG("This is PM's main loop-  get work and do it, forever and forever.\n");
  	while (TRUE) {
	
		pm_get_work();		/* wait for an PM system call */

		/* Check for system notifications first. Special cases. */
		if (pm_call_nr == SYN_ALARM) {
MUKDEBUG("SYN_ALARM\n");
			t = pm_m_in.NOTIFY_TIMESTAMP;
			td = 1000000000/clockTicks;
			tt =  t.tv_nsec / td;
			tt += (t.tv_sec * clockTicks);
			pm_expire_timers((molclock_t) tt);
			result = SUSPEND;		/* don't reply */
		} else if (pm_call_nr == NOTIFY_FROM(LOCAL_SYSTASK) ) {	/* signals pending */
			sigset = pm_m_in.NOTIFY_ARG;
//			if (sigismember(&sigset, SIGKSIG))  {
//				(void) ksig_pending();
//			} 
			result = SUSPEND;		/* don't reply */
		}else if ((unsigned) pm_call_nr >= NCALLS) {
			result = ENOSYS;
		} else {
			result = (*pm_call_vec[pm_call_nr])();
		}
MUKDEBUG("pm_call_nr=%d result=%d\n", pm_call_nr, result);

		/* Send the results back to the user to indicate completion. */
		if (result != SUSPEND) setreply(pm_who_p, result);

		/* Send out all pending reply messages, including the answer to
	 	* the call just made above.  
	 	*/
		/*!!!!!!!!!!!!!!!!!!!!!!!! ESTO ES UNA BURRADA !!!!!!!!!!!!!!*/
		/* Hay que hacer lista enlazada de procesos */
		MUKDEBUG("Send out all pending reply messages\n");
		for (proc_nr=0, rmp=pm_proc_table; 
			proc_nr < dc_ptr->dc_nr_procs; 
			proc_nr++, rmp++) {

			rkp= get_task(proc_nr);

			/* In the meantime, the process may have been killed by a
			 * signal (e.g. if a lethal pending signal was unblocked)
		 	* without the PM realizing it. If the slot is no longer in
		 	* use or just a zombie, don't try to reply.
		 	*/
			if ((rmp->mp_flags & (REPLYPENDING | IN_USE | ZOMBIE)) == (REPLYPENDING | IN_USE)) {
				MUKDEBUG("Replying to %d\n",rkp->p_proc->p_endpoint);		   
				if ((rcode = muk_send(rkp->p_proc->p_endpoint, &rmp->mp_reply)) != OK) {
					MUKDEBUG("PM can't reply to %d (%s)\n",rkp->p_proc->p_endpoint, rkp->name);
					switch(rcode){  /* Auto Unbind the failed process */
						case	EDVSSRCDIED:
						case	EDVSDSTDIED:
						case	EDVSNOPROXY:
						case	EDVSNOTCONN:
						case	EDVSDEADSRCDST:
							pm_exit(mp, rcode);
							break;
						default:
							break;
					}
					ERROR_PRINT(rcode);
				}
				rmp->mp_flags &= ~REPLYPENDING;
			}
		}	
	}

return(OK);
}

/*===========================================================================*
 *				pm_get_work				     *
 *===========================================================================*/
void pm_get_work(void)
{
	int ret;
	
	/* Wait for the next message and extract useful information from it. */
	while( TRUE) {
		MUKDEBUG("Wait for the next message and extract useful information from it.\n");

		if ( (ret=muk_receive(ANY, &pm_m_in)) != OK) {
			ERROR_TSK_EXIT(ret);
		}
		pm_who_e = pm_m_in.m_source;	/* who sent the message */
		pm_call_nr = pm_m_in.m_type;	/* system call number */
		MUKDEBUG("Request received from pm_who_e=%d, pm_call_nr=%d\n", pm_who_e, pm_call_nr);

		if( (pm_who_e == CLOCK) && (pm_call_nr & NOTIFY_MESSAGE) ) return;

		if(( ret = pm_isokendpt(pm_who_e, &pm_who_p)) != OK) {
			MUKDEBUG("BAD ENDPOINT endpoint=%d, pm_call_nr=%d\n", pm_who_e, pm_call_nr);
//			pm_m_in.m_type = EDVSENDPOINT;
//			ret = muk_send(pm_who_e, &pm_m_in);
			continue;
		} 

		/* Process slot of caller. Misuse PM's own process slot if the kernel is
		* calling. This can happen in case of synchronous alarms (CLOCK) or or 
		* event like pending kernel signals (SYSTASK(local_nodeid)).
		*/
		kp = get_task((pm_who_p < 0 ? pm_ep : pm_who_p));
		mp = &pm_proc_table[pm_who_p < 0 ? pm_ep : pm_who_p];
		MUKDEBUG(PM_PROC_FORMAT, PM_PROC_FIELDS(mp));

		if(pm_who_p >= 0 && kp->p_proc->p_endpoint != pm_who_e) {
			MUKDEBUG("PM endpoint number out of sync with kernel endpoint=%d.\n",kp->p_proc->p_endpoint);
			continue;
		}
		return;
	}
 
}


/*===========================================================================*
 *				pm_init					     *
 *===========================================================================*/
void pm_init(void)
{
  	int i, rcode, tab_len;
	char *sig_ptr;
    proc_usr_t *proc_ptr;

	static char mess_sigs[] = { SIGTERM, SIGHUP, SIGABRT, SIGQUIT };

	MUKDEBUG("dcid=%d pm_ep=%d\n",dcid, pm_ep);
		
	MUKDEBUG(DVS_USR_FORMAT,DVS_USR_FIELDS(dvs_ptr));
	MUKDEBUG(DC_USR1_FORMAT,DC_USR1_FIELDS(dc_ptr));
	MUKDEBUG(DC_USR2_FORMAT,DC_USR2_FIELDS(dc_ptr));
	
	pm_id = taskid();
	MUKDEBUG("pm_id=%d\n", pm_id);
	
	rcode = muk_tbind(dcid, pm_ep, "pm");
	MUKDEBUG("rcode=%d\n", rcode);
	if( rcode != pm_ep) {
		ERROR_PRINT(EDVSENDPOINT);
		taskexit(&rcode);
	}
	pm_ptr = current_task();
	proc_ptr = pm_ptr->p_proc;
	MUKDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));

	pm_msg_ptr = &pm_m_in;
	
	/* Register into SYSTASK (as an autofork) */
	MUKDEBUG("Register PM into SYSTASK pm_id=%d\n",pm_id);
	pm_ep = sys_bindproc(pm_ep, pm_id, LCL_BIND);
	if(pm_ep < 0) ERROR_TSK_EXIT(pm_ep);
	
	// set the name of PM 
	rcode = sys_rsetpname(pm_ep, "pm", local_nodeid);
	if(rcode < 0) ERROR_TSK_EXIT(rcode);

	// set the name of SYSTASK  
	rcode = sys_rsetpname(sys_ep, "systask", local_nodeid);
	if(rcode < 0) ERROR_TSK_EXIT(rcode);
	
	MUKDEBUG(DVS_USR_FORMAT,DVS_USR_FIELDS(dvs_ptr));
//	MUKDEBUG(DC_USR1_FORMAT,DC_USR1_FIELDS(dc_ptr));
//	MUKDEBUG(DC_USR2_FORMAT,DC_USR2_FIELDS(dc_ptr));
	
#ifdef ALLOC_LOCAL_TABLE 			
	/* alloc dynamic memory for the KERNEL process table */
	MUKDEBUG("Alloc dynamic memory for the Kernel process table nr_procs+nr_tasks=%d\n", (dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs));
//	pm_kproc = malloc((dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs)*sizeof(muk_proc_t));
	posix_memalign( (void**) &pm_kproc, getpagesize(), (dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs) * dvs_ptr->d_size_proc);
	if(pm_kproc == NULL) ERROR_TSK_EXIT(rcode);
#else // ALLOC_LOCAL_TABLE 		
//	pm_kproc = kproc_map; 
#endif // ALLOC_LOCAL_TABLE 			

	/* alloc dynamic memory for the PM process table */
	MUKDEBUG("Alloc dynamic memory for the PM process table nr_procs=%d\n", dc_ptr->dc_nr_procs);
//	pm_proc_table = malloc((dc_ptr->dc_nr_procs)*sizeof(mproc_t));
	posix_memalign( (void**) &pm_proc_table, getpagesize(), (dc_ptr->dc_nr_procs)*sizeof(mproc_t));
	if(pm_proc_table == NULL) ERROR_TSK_EXIT(rcode);

	/* alloc dynamic memory for the KERNEL priviledge table */
	MUKDEBUG("Alloc dynamic memory for the Kernel priviledge table nr_procs+nr_tasks=%d\n", (dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs));
//	pm_kpriv = malloc((dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs)*sizeof(priv_usr_t));
	posix_memalign( (void**) &pm_kpriv, getpagesize(), (dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs)*sizeof(priv_usr_t));
	if(pm_kpriv == NULL) ERROR_TSK_EXIT(rcode);

#ifdef ALLOC_LOCAL_TABLE 			
	/* get kernel PROC TABLE */
	tab_len =  ((dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs)*dvs_ptr->d_size_proc);
	MUKDEBUG("tab_len=%d\n", tab_len);
	rcode = sys_proctab(pm_kproc, tab_len);
	if(rcode < 0) ERROR_EXIT(rcode);
#endif // ALLOC_LOCAL_TABLE 			
	
	MUKDEBUG("Initialize the PM's process table\n");
	for (i = 0; i < dc_ptr->dc_nr_procs; i++) 
		mproc_init(i);

	MUKDEBUG("Initialize PM entry in the PM's process table\n");
 	/* Initialize PM entry in the PM's process table */
 	procs_in_use = 1;			/* start populating table */
	/* Set process details found in the PM table. */
	mp = &pm_proc_table[pm_ep];	
	mp->mp_nice = PRIO_SERVER;
	sigemptyset(&mp->mp_sig2mess);
  	sigemptyset(&mp->mp_ignore);	
  	sigemptyset(&mp->mp_sigmask);
  	sigemptyset(&mp->mp_catch);
	mp->mp_pid = PM_PID;		/* PM has magic pid */
	mp->mp_flags |= IN_USE | DONT_SWAP | PRIV_PROC; 
	for (sig_ptr = mess_sigs; 
	     sig_ptr < mess_sigs+sizeof(mess_sigs); 
	     sig_ptr++) {
			sigaddset(&mp->mp_sig2mess, *sig_ptr);
	}
	sigfillset(&pm_proc_table[pm_ep].mp_ignore); 	/* guard against signals */
	MUKDEBUG(PM_PROC_FORMAT,PM_PROC_FIELDS(mp));

	/* Set the next free slot for USER processes */
	pm_next_child = dc_ptr->dc_nr_sysprocs; 

	/* change PRIVILEGES of PM */
//	MUKDEBUG("change PRIVILEGES of PM\n");
//	rcode = sys_privctl(pm_ep, SERVER_PRIV);
//	if(rcode < 0 ) ERROR_TSK_EXIT(rcode);

//	rcode = sys_proctab(pm_kproc, tab_len);
//	if(rcode < 0) ERROR_TSK_EXIT(rcode);

	/* Fetch clock ticks */
	clockTicks = sysconf(_SC_CLK_TCK);
	if (clockTicks == -1)	ERROR_TSK_EXIT(errno);
	MUKDEBUG("clockTicks =%ld\n",clockTicks );

	if ( (rcode=sys_getuptime(&boottime)) < 0) 
		ERROR_TSK_EXIT(rcode);
	boottime /= clockTicks;
	MUKDEBUG("boottime =%ld\n",boottime );
	
	if(rcode < 0) ERROR_TSK_EXIT(rcode);
	
}

/*===========================================================================*
 *				setreply				     *
/* proc_nr: process to reply to 					     *
/* result: result of call (usually OK or error #) 			     *	
 *===========================================================================*/
void setreply(int proc_nr, int result)
{
/* Fill in a reply message to be sent later to a user process.  System calls
 * may occasionally fill in other fields, this is only for the main return
 * value, and for setting the "must send reply" flag.
 */
  mproc_t *rmp = &pm_proc_table[proc_nr];

MUKDEBUG("proc_nr=%d result=%d\n",proc_nr, result);

  if(proc_nr < 0 || proc_nr >= dc_ptr->dc_nr_procs)
	ERROR_TSK_EXIT(result);

  rmp->mp_reply.reply_res = result;
  rmp->mp_flags |= REPLYPENDING;	/* reply pending */
}


#ifdef TEMPORAL

  message mess;

  int s;
  static struct boot_image image[NR_BOOT_PROCS];
  register struct boot_image *ip;
  static char core_sigs[] = { SIGQUIT, SIGILL, SIGTRAP, SIGABRT,
			SIGEMT, SIGFPE, SIGUSR1, SIGSEGV, SIGUSR2 };
  static char ign_sigs[] = { SIGCHLD, SIGWINCH, SIGCONT };
  register struct pm_proc_table *rmp;


  /* Initialize process table, including timers. */
  for (rmp=&pm_proc_table[0]; rmp<&pm_proc_table[NR_PROCS]; rmp++) {
	tmr_inittimer(&rmp->mp_timer);
  }

  /* Build the set of signals which cause core dumps, and the set of signals
   * that are by default ignored.
   */
  sigemptyset(&core_sset);
  for (sig_ptr = core_sigs; sig_ptr < core_sigs+sizeof(core_sigs); sig_ptr++)
	sigaddset(&core_sset, *sig_ptr);
  sigemptyset(&ign_sset);
  for (sig_ptr = ign_sigs; sig_ptr < ign_sigs+sizeof(ign_sigs); sig_ptr++)
	sigaddset(&ign_sset, *sig_ptr);

  /* Obtain a copy of the boot monitor parameters and the kernel pm_info struct.  
   * Parse the list of free memory chunks. This list is what the boot monitor 
   * reported, but it must be corrected for the kernel and system processes.
   */
  if ((s=sys_getmonparams(monitor_params, sizeof(monitor_params))) < 0 )
      panic(__FILE__,"get monitor params failed",s);
  get_mem_chunks(mem_chunks);
  if ((s=sys_getkinfo(&kinfo)) < 0)
      panic(__FILE__,"get kernel pm_info failed",s);

  /* Get the memory map of the kernel to see how much memory it uses. */
  if ((s=get_mem_map(SYSTASK, mem_map)) != OK)
  	panic(__FILE__,"couldn't get memory map of SYSTASK",s);
  minix_clicks = (mem_map[S].mem_phys+mem_map[S].mem_len)-mem_map[T].mem_phys;
  patch_mem_chunks(mem_chunks, mem_map);

  /* Initialize PM's process table. Request a copy of the system image table 
   * that is defined at the kernel level to see which slots to fill in.
   */
  if ((s=sys_getimage(image))< 0) 
  	panic(__FILE__,"couldn't get image table: %d\n", s);
  procs_in_use = 0;				/* start populating table */
  printf("Building process table:");		/* show what's happening */
  for (ip = &image[0]; ip < &image[NR_BOOT_PROCS]; ip++) {		
  	if (ip->proc_nr >= 0) {			/* task have negative nrs */
  		procs_in_use += 1;		/* found user process */

		/* Set process details found in the image table. */
		rmp = &pm_proc_table[ip->proc_nr];	
  		strncpy(rmp->mp_name, ip->proc_name, PROC_NAME_LEN); 
		rmp->mp_parent = RS_PROC_NR;
		rmp->mp_nice = get_nice_value(ip->priority);
  		sigemptyset(&rmp->mp_sig2mess);
  		sigemptyset(&rmp->mp_ignore);	
  		sigemptyset(&rmp->mp_sigmask);
  		sigemptyset(&rmp->mp_catch);
		if (ip->proc_nr == INIT_PROC_NR) {	/* user process */
  			rmp->mp_procgrp = rmp->mp_pid = INIT_PID;
			rmp->mp_flags |= IN_USE; 
		}
		else {					/* system process */
  			rmp->mp_pid = get_free_pid();
			rmp->mp_flags |= IN_USE | DONT_SWAP | PRIV_PROC; 
  			for (sig_ptr = mess_sigs; 
				sig_ptr < mess_sigs+sizeof(mess_sigs); 
				sig_ptr++)
			sigaddset(&rmp->mp_sig2mess, *sig_ptr);
		}

		/* Get kernel endpoint identifier. */
		rmp->mp_endpoint = ip->endpoint;

  		/* Get memory map for this process from the kernel. */
		if ((s=get_mem_map(ip->proc_nr, rmp->mp_seg)) != OK)
  			panic(__FILE__,"couldn't get process entry",s);
		if (rmp->mp_seg[T].mem_len != 0) rmp->mp_flags |= SEPARATE;
		minix_clicks += rmp->mp_seg[S].mem_phys + 
			rmp->mp_seg[S].mem_len - rmp->mp_seg[T].mem_phys;
  		patch_mem_chunks(mem_chunks, rmp->mp_seg);

		/* Tell FS about this system process. */
		mess.PR_SLOT = ip->proc_nr;
		mess.PR_PID = rmp->mp_pid;
		mess.PR_ENDPT = rmp->mp_endpoint;
  		if (OK != (s=send(fs_ep, &mess)))
			panic(__FILE__,"can't sync up with FS", s);
  		printf(" %s", ip->proc_name);	/* display process name */
  	}
  }
  printf(".\n");				/* last process done */

  /* Override some details. INIT, PM, FS and RS are somewhat special. */
  pm_proc_table[pm_ep].mp_pid = pm_id;		/* PM has magic pid */
//  pm_proc_table[RS_PROC_NR].mp_parent = INIT_PROC_NR;	/* INIT is root */
  sigfillset(&pm_proc_table[pm_ep].mp_ignore); 	/* guard against signals */

  /* Tell FS that no more system processes follow and synchronize. */
  mess.PR_ENDPT = NONE;
  if (sendrec(fs_ep, &mess) != OK || mess.m_type != OK)
	panic(__FILE__,"can't sync up with FS", NO_NUM);

#if ENABLE_BOOTDEV
  /* Possibly we must correct the memory chunks for the boot device. */
  if (kinfo.bootdev_size > 0) {
      mem_map[T].mem_phys = kinfo.bootdev_base >> CLICK_SHIFT;
      mem_map[T].mem_len = 0;
      mem_map[D].mem_len = (kinfo.bootdev_size+CLICK_SIZE-1) >> CLICK_SHIFT;
      patch_mem_chunks(mem_chunks, mem_map);
  }
#endif /* ENABLE_BOOTDEV */

  /* Withhold some memory from x86 DC */
  pm_x86_dc(mem_chunks);

  /* Initialize tables to all physical memory and print memory information. */
  printf("Physical memory:");
  mem_init(mem_chunks, &free_clicks);
  total_clicks = minix_clicks + free_clicks;
  printf(" total %u KB,", click_to_round_k(total_clicks));
  printf(" system %u KB,", click_to_round_k(minix_clicks));
  printf(" free %u KB.\n", click_to_round_k(free_clicks));
}

/*===========================================================================*
 *				TEMPO				     *
 *===========================================================================*/
void templ()
{
/* Main routine of the process manager. */
  int result, s, proc_nr;
  struct pm_proc_table *rmp;
  sigset_t sigset;
  
  	/* This is PM's main loop-  get work and do it, forever and forever. */
  	while (TRUE) {
		pm_get_work();		/* wait for an PM system call */

		/* Check for system notifications first. Special cases. */
		if (pm_call_nr == SYN_ALARM) {
			pm_expire_timers(pm_m_in.NOTIFY_TIMESTAMP);
			result = SUSPEND;		/* don't reply */
		} else if (pm_call_nr == SYS_SIG) {	/* signals pending */
			sigset = pm_m_in.NOTIFY_ARG;
			if (sigismember(&sigset, SIGKSIG))  {
				(void) ksig_pending();
			} 
			result = SUSPEND;		/* don't reply */
		}/* Else, if the system call number is valid, perform the call. */
		else if ((unsigned) pm_call_nr >= NCALLS) {
			result = ENOSYS;
		} else {
			result = (*pm_call_vec[pm_call_nr])();
		}

		/* Send the results back to the user to indicate completion. */
		if (result != SUSPEND) setreply(pm_who_p, result);


		/* Send out all pending reply messages, including the answer to
	 	* the call just made above.  
	 	*/
		for (proc_nr=0, rmp=pm_proc_table; proc_nr < NR_PROCS; proc_nr++, rmp++) {
			/* In the meantime, the process may have been killed by a
			 * signal (e.g. if a lethal pending signal was unblocked)
		 	* without the PM realizing it. If the slot is no longer in
		 	* use or just a zombie, don't try to reply.
		 	*/
			if ((rmp->mp_flags & (REPLYPENDING | ONSWAP | IN_USE | ZOMBIE)) ==
			   (REPLYPENDING | IN_USE)) {
				if ((s=muk_send(rmp->mp_endpoint, &rmp->mp_reply)) != OK) {
					printf("PM can't reply to %d (%s)\n",
						rmp->mp_endpoint, rmp->mp_name);
					panic(__FILE__, "PM can't reply", NO_NUM);
				}
				rmp->mp_flags &= ~REPLYPENDING;
			}
		}	
	}
}
#endif /* TEMPORAL */
