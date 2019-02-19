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
 
#define _TABLE
#include "pm.h"

void pm_init(int dcid);
void get_work(void);
		
/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main ( int argc, char *argv[] )
{
	int dcid, rcode, result, proc_nr, ds_lpid, changed_nr;
	mnxsigset_t sigset;
	mproc_t *rmp;	
	proc_usr_t *rkp;	
	struct timespec t; 
	long long tt, td;

	if ( argc != 2) {
 	        printf( "Usage: %s <dcid> \n", argv[0] );
 	        exit(1);
    }

	dcid = atoi(argv[1]);
	if( dcid < 0 || dcid >= NR_DCS) ERROR_EXIT(EDVSRANGE);

	pm_init(dcid);
	
SVRDEBUG("This is PM's main loop-  get work and do it, forever and forever.\n");
  	while (TRUE) {
	
		get_work();		/* wait for an PM system call */

		/* Check for system notifications first. Special cases. */
		if (call_nr == SYN_ALARM) {
SVRDEBUG("SYN_ALARM\n");
			t = m_in.NOTIFY_TIMESTAMP;
			td = 1000000000/clockTicks;
			tt =  t.tv_nsec / td;
			tt += (t.tv_sec * clockTicks);
			pm_expire_timers((molclock_t) tt);
			result = SUSPEND;		/* don't reply */
		} else if (call_nr == NOTIFY_FROM(LOCAL_SYSTASK) ) {	/* signals pending */
			sigset = m_in.NOTIFY_ARG;
//			if (sigismember(&sigset, SIGKSIG))  {
//				(void) ksig_pending();
//			} 
			result = SUSPEND;		/* don't reply */
		} else if (call_nr == NOTIFY_FROM(LOCAL_SLOTS)) {	/* SLOTS signal - a process descriptor needs update */
SVRDEBUG("NOTIFY_FROM LOCAL_SLOTS=%d NOTIFY_ARG=%d\n",
	NOTIFY_FROM(LOCAL_SLOTS),m_in.NOTIFY_ARG);
			sys_procinfo(m_in.NOTIFY_ARG);
			/* Else, if the system call number is valid, perform the call. */
		}else if ((unsigned) call_nr >= NCALLS) {
			result = ENOSYS;
		} else {
			result = (*call_vec[call_nr])();
		}
SVRDEBUG("call_nr=%d result=%d\n", call_nr, result);

		/* Send the results back to the user to indicate completion. */
		if (result != SUSPEND) setreply(who_p, result);

		/* Send out all pending reply messages, including the answer to
	 	* the call just made above.  
	 	*/
		/*!!!!!!!!!!!!!!!!!!!!!!!! ESTO ES UNA BURRADA !!!!!!!!!!!!!!*/
		/* Hay que hacer lista enlazada de procesos */
SVRDEBUG("Send out all pending reply messages\n");
		for (proc_nr=0, rmp=mproc, rkp=(&kproc[0] + dc_ptr->dc_nr_tasks); 
			proc_nr < dc_ptr->dc_nr_procs; 
			proc_nr++, rmp++, rkp++) {
			/* In the meantime, the process may have been killed by a
			 * signal (e.g. if a lethal pending signal was unblocked)
		 	* without the PM realizing it. If the slot is no longer in
		 	* use or just a zombie, don't try to reply.
		 	*/
			if ((rmp->mp_flags & (REPLYPENDING | IN_USE | ZOMBIE)) == (REPLYPENDING | IN_USE)) {
SVRDEBUG("Replying to %d\n",rkp->p_endpoint);		   
				if ((rcode = dvk_send(rkp->p_endpoint, &rmp->mp_reply)) != OK) {
					SVRDEBUG("PM can't reply to %d (%s)\n",rkp->p_endpoint, rkp->p_name);
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
 *				get_work				     *
 *===========================================================================*/
void get_work(void)
{
	int ret;
	
	/* Wait for the next message and extract useful information from it. */
	while( TRUE) {
		SVRDEBUG("Wait for the next message and extract useful information from it.\n");

		if ( (ret=dvk_receive(ANY, &m_in)) != OK) ERROR_EXIT(ret);
		who_e = m_in.m_source;	/* who sent the message */
		call_nr = m_in.m_type;	/* system call number */
		SVRDEBUG("Request received from who_e=%d, call_nr=%d\n", who_e, call_nr);

		if( (who_e == CLOCK) && (call_nr & NOTIFY_MESSAGE) ) return;

		if(( ret = pm_isokendpt(who_e, &who_p)) != OK) {
			SVRDEBUG("BAD ENDPOINT endpoint=%d, call_nr=%d\n", who_e, call_nr);
//			m_in.m_type = EDVSENDPOINT;
//			ret = dvk_send(who_e, &m_in);
			continue;
		} 

		/* Process slot of caller. Misuse PM's own process slot if the kernel is
		* calling. This can happen in case of synchronous alarms (CLOCK) or or 
		* event like pending kernel signals (SYSTEM).
		*/
		kp = &kproc[(who_p < 0 ? PM_PROC_NR : who_p)+dc_ptr->dc_nr_tasks];
		mp = &mproc[who_p < 0 ? PM_PROC_NR : who_p];
		SVRDEBUG(PM_PROC_FORMAT, PM_PROC_FIELDS(mp));

		if(who_p >= 0 && kp->p_endpoint != who_e) {
			SVRDEBUG("PM endpoint number out of sync with kernel endpoint=%d.\n",kp->p_endpoint);
			continue;
		}
		return;
	}
 
}


/*===========================================================================*
 *				pm_init					     *
 *===========================================================================*/
void pm_init(int dcid)
{
  	int i, rcode, tab_len;
	char *sig_ptr;
	static proc_usr_t rpm, *rpm_ptr;	
	static char mess_sigs[] = { SIGTERM, SIGHUP, SIGABRT, SIGQUIT };

	/*testing*/
	int ds_lpid, ds_ep;
	
	rcode = dvk_open();
	if (rcode < 0)  ERROR_EXIT(rcode);
	
	SVRDEBUG("PM: address of m_in=%p m_out=%p\n",&m_in, &m_out);

	local_nodeid = dvk_getdvsinfo(&dvs);
	if(local_nodeid < 0) ERROR_EXIT(local_nodeid);
	SVRDEBUG("local_nodeid=%d\n",local_nodeid);
	
	pm_lpid = getpid();
	m_ptr = &m_in;

	/* Bind PM to the kernel */
	SVRDEBUG("Binding process %d to DC%d with pm_nr=%d\n",pm_lpid,dcid,PM_PROC_NR);
#define SELFTASK (-1) 
	pm_ep = dvk_replbind(dcid, SELFTASK, PM_PROC_NR);
	
	if (pm_ep != PM_PROC_NR) {
		if(pm_ep == EDVSSLOTUSED){
			rpm_ptr = &rpm;
			rcode = dvk_getprocinfo(dcid, PM_PROC_NR, rpm_ptr);
			if(rcode) ERROR_EXIT(rcode);
			SVRDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(rpm_ptr));
			if( (TEST_BIT(rpm_ptr->p_rts_flags, BIT_REMOTE)) 
			 && (!TEST_BIT(rpm_ptr->p_misc_flags , MIS_BIT_RMTBACKUP))) {
				rcode = dvk_migr_start(dcid, PM_PROC_NR);
				if(rcode) ERROR_EXIT(rcode);
				rcode = dvk_migr_commit(pm_lpid, dcid, PM_PROC_NR, local_nodeid);
				if(rcode) ERROR_EXIT(rcode);
			}else{
				ERROR_EXIT(EDVSSLOTUSED);
			}
		} else {
			if(pm_ep < 0) ERROR_EXIT(pm_ep);
			ERROR_EXIT(EDVSBADPROC);
		}
	}

	/* Register into SYSTASK (as an autofork) */
	SVRDEBUG("Register PM into SYSTASK pm_lpid=%d\n",pm_lpid);
	pm_ep = sys_bindproc(PM_PROC_NR, pm_lpid, REPLICA_BIND);
	if(pm_ep < 0) ERROR_EXIT(pm_ep);
	
	rcode = sys_rsetpname(pm_ep,  program_invocation_short_name, local_nodeid);
	if(rcode < 0) ERROR_EXIT(rcode);
	
	SVRDEBUG("Get the DVS info from SYSTASK\n");
    rcode = sys_getkinfo(&dvs);
	if(rcode < 0 ) ERROR_EXIT(rcode);
	dvs_ptr = &dvs;

	SVRDEBUG(DVS_USR_FORMAT,DVS_USR_FIELDS(dvs_ptr));
	SVRDEBUG("Get the DC info from SYSTASK\n");
	sys_getmachine(&dcu);
	if(rcode < 0) ERROR_EXIT(rcode);
	dc_ptr = &dcu;
	SVRDEBUG(DC_USR1_FORMAT,DC_USR1_FIELDS(dc_ptr));
	SVRDEBUG(DC_USR2_FORMAT,DC_USR2_FIELDS(dc_ptr));
	
	/* alloc dynamic memory for the KERNEL process table */
	SVRDEBUG("Alloc dynamic memory for the Kernel process table nr_procs+nr_tasks=%d\n", (dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs));
//	kproc = malloc((dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs)*sizeof(proc_usr_t));
	posix_memalign( (void**) &kproc, getpagesize(), (dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs)*sizeof(proc_usr_t));
	if(kproc == NULL) ERROR_EXIT(rcode);

	/* alloc dynamic memory for the PM process table */
	SVRDEBUG("Alloc dynamic memory for the PM process table nr_procs=%d\n", dc_ptr->dc_nr_procs);
//	mproc = malloc((dc_ptr->dc_nr_procs)*sizeof(mproc_t));
	posix_memalign( (void**) &mproc, getpagesize(), (dc_ptr->dc_nr_procs)*sizeof(mproc_t));
	if(mproc == NULL) ERROR_EXIT(rcode)

	/* alloc dynamic memory for the KERNEL priviledge table */
	SVRDEBUG("Alloc dynamic memory for the Kernel priviledge table nr_procs+nr_tasks=%d\n", (dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs));
//	kpriv = malloc((dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs)*sizeof(priv_usr_t));
	posix_memalign( (void**) &kpriv, getpagesize(), (dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs)*sizeof(priv_usr_t));
	if(kpriv == NULL) ERROR_EXIT(rcode);

	/* alloc dynamic memory for the SYSTASK Slots allocation table  */
	SVRDEBUG("Alloc dynamic memory for the SYSTASK Slots allocation table nr_procs+nr_tasks=%d\n", (dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs));
//	slots = malloc((dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs)*sizeof(slots));
	posix_memalign( (void**) &slots, getpagesize(), (dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs)*sizeof(slots));
	if(slots == NULL) ERROR_EXIT(rcode);
	
 	/* Initialize PM's process table */
	for (i = 0; i < dc_ptr->dc_nr_procs; i++) 
		mproc_init(i);

 	/* Initialize PM entry in the PM's process table */
	SVRDEBUG("Initialize PM entry in the PM's process table\n");
 	procs_in_use = 1;			/* start populating table */
	/* Set process details found in the PM table. */
	mp = &mproc[PM_PROC_NR];	
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
	sigfillset(&mproc[PM_PROC_NR].mp_ignore); 	/* guard against signals */
	SVRDEBUG(PM_PROC_FORMAT,PM_PROC_FIELDS(mp));

	/* Set the next free slot for USER processes */
	next_child = dc_ptr->dc_nr_sysprocs; 

	/* change PRIVILEGES of PM */
	SVRDEBUG("change PRIVILEGES of PM\n");
	rcode = sys_privctl(pm_ep, SERVER_PRIV);
	if(rcode < 0 ) ERROR_EXIT(rcode);

	/* get kernel PROC TABLE */
	tab_len =  ((dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs)*sizeof(proc_usr_t));
	rcode = sys_proctab(kproc, tab_len);
	if(rcode < 0) ERROR_EXIT(rcode);

	/* Fetch clock ticks */
	clockTicks = sysconf(_SC_CLK_TCK);
	if (clockTicks == -1)	ERROR_EXIT(errno);
	SVRDEBUG("clockTicks =%ld\n",clockTicks );

	if ( (rcode=sys_getuptime(&boottime)) < 0) 
  		ERROR_EXIT(rcode);
	boottime /= clockTicks;
	SVRDEBUG("boottime =%ld\n",boottime );

	if(rcode < 0) ERROR_EXIT(rcode);
	
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
  struct mproc *rmp = &mproc[proc_nr];

SVRDEBUG("proc_nr=%d result=%d\n",proc_nr, result);

  if(proc_nr < 0 || proc_nr >= dc_ptr->dc_nr_procs)
	ERROR_EXIT(EDVSBADPROC);

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
  register struct mproc *rmp;


  /* Initialize process table, including timers. */
  for (rmp=&mproc[0]; rmp<&mproc[NR_PROCS]; rmp++) {
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

  /* Obtain a copy of the boot monitor parameters and the kernel info struct.  
   * Parse the list of free memory chunks. This list is what the boot monitor 
   * reported, but it must be corrected for the kernel and system processes.
   */
  if ((s=sys_getmonparams(monitor_params, sizeof(monitor_params))) < 0 )
      panic(__FILE__,"get monitor params failed",s);
  get_mem_chunks(mem_chunks);
  if ((s=sys_getkinfo(&kinfo)) < 0)
      panic(__FILE__,"get kernel info failed",s);

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
		rmp = &mproc[ip->proc_nr];	
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
  		if (OK != (s=send(FS_PROC_NR, &mess)))
			panic(__FILE__,"can't sync up with FS", s);
  		printf(" %s", ip->proc_name);	/* display process name */
  	}
  }
  printf(".\n");				/* last process done */

  /* Override some details. INIT, PM, FS and RS are somewhat special. */
  mproc[PM_PROC_NR].mp_pid = pm_lpid;		/* PM has magic pid */
  mproc[RS_PROC_NR].mp_parent = INIT_PROC_NR;	/* INIT is root */
  sigfillset(&mproc[PM_PROC_NR].mp_ignore); 	/* guard against signals */

  /* Tell FS that no more system processes follow and synchronize. */
  mess.PR_ENDPT = NONE;
  if (sendrec(FS_PROC_NR, &mess) != OK || mess.m_type != OK)
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
  do_x86_dc(mem_chunks);

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
  struct mproc *rmp;
  sigset_t sigset;
  
  	/* This is PM's main loop-  get work and do it, forever and forever. */
  	while (TRUE) {
		get_work();		/* wait for an PM system call */

		/* Check for system notifications first. Special cases. */
		if (call_nr == SYN_ALARM) {
			pm_expire_timers(m_in.NOTIFY_TIMESTAMP);
			result = SUSPEND;		/* don't reply */
		} else if (call_nr == SYS_SIG) {	/* signals pending */
			sigset = m_in.NOTIFY_ARG;
			if (sigismember(&sigset, SIGKSIG))  {
				(void) ksig_pending();
			} 
			result = SUSPEND;		/* don't reply */
		}/* Else, if the system call number is valid, perform the call. */
		else if ((unsigned) call_nr >= NCALLS) {
			result = ENOSYS;
		} else {
			result = (*call_vec[call_nr])();
		}

		/* Send the results back to the user to indicate completion. */
		if (result != SUSPEND) setreply(who_p, result);


		/* Send out all pending reply messages, including the answer to
	 	* the call just made above.  
	 	*/
		for (proc_nr=0, rmp=mproc; proc_nr < NR_PROCS; proc_nr++, rmp++) {
			/* In the meantime, the process may have been killed by a
			 * signal (e.g. if a lethal pending signal was unblocked)
		 	* without the PM realizing it. If the slot is no longer in
		 	* use or just a zombie, don't try to reply.
		 	*/
			if ((rmp->mp_flags & (REPLYPENDING | ONSWAP | IN_USE | ZOMBIE)) ==
			   (REPLYPENDING | IN_USE)) {
				if ((s=dvk_send(rmp->mp_endpoint, &rmp->mp_reply)) != OK) {
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
