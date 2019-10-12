/* This file contains some utility routines for PM.
 *
 */

#include "pm.h"

#undef DVK_GLOBAL_HERE
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

/*===========================================================================*
 *				pm_get_uptime				     *
 *===========================================================================*/
molclock_t pm_get_uptime(void)
{
	int rcode;

	rcode = sysinfo(&pm_info); 
	if( rcode) ERROR_EXIT(errno);
	return(pm_info.uptime); 
}


/*===========================================================================*
 *				pm_no_sys					     *
 *===========================================================================*/
int pm_no_sys(void)
{
	
MUKDEBUG("A system call number not implemented by PM has been requested from %d\n", pm_who_e);
	mp = &pm_proc_table[pm_who_p];
	mp->mp_flags |= (REPLYPENDING | IN_USE);
			   
  return(ENOSYS);
}

/*===========================================================================*
 *				pm_isokendpt			 	     *
 *===========================================================================*/
int pm_isokendpt(int endpoint, int *proc)
{
  	muk_proc_t *rkp;	

MUKDEBUG("endpoint=%d \n", endpoint);
	*proc = _ENDPOINT_P(endpoint);
MUKDEBUG("*proc=%d\n", *proc);
	rkp =  (muk_proc_t *) get_task(*proc);
MUKDEBUG("rkp->p_proc->p_endpoint=%d\n", rkp->p_proc->p_endpoint);
	
	CHECK_P_NR(*proc);

	if( !(pm_proc_table[*proc].mp_flags & IN_USE)) {	/* Free PM process slot ?		*/
		if( (*proc+dc_ptr->dc_nr_tasks) < dc_ptr->dc_nr_sysprocs ) {	/* Is it a system process ?		*/
			return(OK);			/* if the process has sent a request 	*/
		}					/* to PM is because it has binded to 	*/
	}						/* the kernel, therefore has endpoint	*/

	if(endpoint != rkp->p_proc->p_endpoint) 
		ERROR_RETURN(EDVSENDPOINT);	
	return(OK);
}

/*===========================================================================*
 *				sys_proctab				     *
 *===========================================================================*/
int sys_proctab(muk_proc_t *kp, int tab_len)
{
	int rcode;

MUKDEBUG("Sending SYS_GETINFO request %d to SYSTASK(local_nodeid)\n", GET_PROCTAB);
	rcode = sys_getinfo(GET_PROCTAB, (char*) kp, tab_len, NULL, 0);
	if (rcode) ERROR_RETURN(rcode);
	return(OK);
}

/*===========================================================================*
 *				sys_slotstab				     *
 *===========================================================================*/
int sys_slotstab(slot_t *slots, int tab_len)
{
	int rcode;

MUKDEBUG("Sending SYS_GETINFO request %d to SYSTASK(local_nodeid)\n", GET_SLOTSTAB);
	rcode = sys_getinfo(GET_SLOTSTAB, (char*) slots, tab_len, NULL, 0);
	if (rcode) ERROR_RETURN(rcode);
	return(OK);
}

/*===========================================================================*
 *				sys_privtab				     *
 *===========================================================================*/
int sys_privtab(priv_usr_t *kp, int tab_len)
{
	int rcode;

MUKDEBUG("Sending SYS_GETINFO request %d to SYSTASK(local_nodeid)\n", GET_PRIVTAB);
	rcode = sys_getinfo(GET_PRIVTAB, (char*) kp, tab_len, NULL, 0);
	if (rcode) ERROR_RETURN(rcode);
	return(OK);
}

/*===========================================================================*
 *				sys_procinfo				     *
 *===========================================================================*/
int sys_procinfo(int p_nr)
{
	int rcode;
	muk_proc_t *pm_proc_ptr;
    proc_usr_t *proc_ptr;

	pm_proc_ptr =  (muk_proc_t *) get_task(p_nr);
	
#ifdef ALLOC_LOCAL_TABLE 			
	MUKDEBUG("Sending SYS_GETINFO request %d to SYSTASK(local_nodeid) for p_nr=%d\n", GET_PROC, p_nr);
	rcode = sys_getinfo(GET_PROC, (void *) pm_proc_ptr, sizeof(muk_proc_t), NULL, p_nr);
	if(rcode) ERROR_RETURN(rcode);
#endif // ALLOC_LOCAL_TABLE 			
	proc_ptr = pm_proc_ptr->p_proc;
	MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));

	return(OK);
}

/*===========================================================================*
 *				mproc_init			     *
 *===========================================================================*/
void mproc_init(int p_nr)
{
	muk_proc_t *pm_proc_ptr;
    proc_usr_t *proc_ptr;

	
	MUKDEBUG("p_nr=%d\n", p_nr);

	mp = &pm_proc_table[p_nr];	
	mp->mp_exitstatus	= OK;
	mp->mp_sigstatus	= OK;
	mp->mp_pid			= PROC_NO_PID;
	mp->mp_endpoint		= NONE;
	mp->mp_procgrp		= PROC_NO_PID;
	mp->mp_wpid			= PROC_NO_PID;
	mp->mp_parent		= -1;
	mp->mp_child_utime	= 0;
	mp->mp_child_stime	= 0;
	mp->mp_realuid		= -1;
	mp->mp_effuid		= -1;
	mp->mp_realgid		= -1;
	mp->mp_effgid		= -1;
	sigemptyset(&mp->mp_ignore);	
	sigemptyset(&mp->mp_catch);
	sigemptyset(&mp->mp_sig2mess);
  	sigemptyset(&mp->mp_sigmask);
  	sigemptyset(&mp->mp_sigmask2);
  	sigemptyset(&mp->mp_sigpending);
	mp->mp_flags		= 0;
	mp->mp_nice 		= PRIO_USERPROC;
	
	pm_proc_ptr =  (muk_proc_t *) get_task(p_nr);
	if ( pm_proc_ptr != NULL) {
		assert( !TEST_BIT(pm_proc_ptr->p_proc->p_rts_flags, BIT_SLOT_FREE));
		proc_ptr = pm_proc_ptr->p_proc;
		MUKDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
		mp->mp_endpoint	= pm_proc_ptr->p_proc->p_endpoint;
		mp->mp_flags	= IN_USE;
		mp->mp_pid		= 0;
		mp->mp_procgrp	= 0;
		mp->mp_parent	= 0;
	}
}


/*===========================================================================*
 *				tell_fs					     *
 *===========================================================================*/
int tell_fs(int what, int p1, int p2,int p3)
{
/* This routine is only used by PM to inform FS of certain events:
 *      tell_fs(CHDIR, slot, dir, 0)
 *      tell_fs(EXEC, proc, 0, 0)
 *      tell_fs(EXIT, proc, 0, 0)
 *      tell_fs(FORK, parent, child, pid)
 *      tell_fs(SETGID, proc, realgid, effgid)
 *      tell_fs(SETSID, proc, 0, 0)
 *      tell_fs(SETUID, proc, realuid, effuid)
 *      tell_fs(UNPAUSE, proc, signr, 0)
 *      tell_fs(STIME, time, 0, 0)
 * Ignore this call if the FS is already dead, e.g. on shutdown.
 */
  message m;
	int rcode;

//  if ((pm_proc_table[fs_ep].mp_flags & (IN_USE|ZOMBIE)) != IN_USE)
//      ERROR_RETURN(EDVSBUSY);
MUKDEBUG("what=%d p1=%d p2=%d p3=%d\n", what, p1, p2, p3);

  m.tell_fs_arg1 = p1;
  m.tell_fs_arg2 = p2;
  m.tell_fs_arg3 = p3;
  m.m_type = what;
  rcode =  muk_sendrec(fs_ep, &m);
  if(rcode) ERROR_RETURN(rcode);
  return(OK);
}

/*===========================================================================*
 *				get_free_pid				     *
 *===========================================================================*/
int get_free_pid(void)
{
  static pid_t next_pid = INIT_PID + 1;	/* next pid to be assigned */
  mproc_t *rmp;			/* check process table */
  int t;				/* zero if pid still free */

  /* Find a free pid for the child and put it in the table. */
  do {
	t = 0;			
	next_pid = (next_pid < NR_PIDS ? next_pid + 1 : INIT_PID + 1);
	for (rmp = &pm_proc_table[0]; rmp < &pm_proc_table[dc_ptr->dc_nr_procs]; rmp++)
		if (rmp->mp_pid == next_pid || rmp->mp_procgrp == next_pid) {
			t = 1;
			break;
		}
  } while (t);					/* 't' = 0 means pid free */
  return(next_pid);
}

#ifdef TEMPORAL 

/*===========================================================================*
 *				get_dvs_params				     *
 *===========================================================================*/
void get_dvs_params(void)
{
	local_nodeid = mnx_getdvsinfo(&dvs);
MUKDEBUG("local_nodeid=%d\n",local_nodeid);
	if( local_nodeid < DVS_NO_INIT) ERROR_EXIT(local_nodeid);
	dvs_ptr = &dvs;
MUKDEBUG(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));

}

/*===========================================================================*
 *				get_dc_params				     *
 *===========================================================================*/
void get_dc_params(int vmid)
{
	int rcode;

	if ( vmid < 0 || vmid >= dvs.d_nr_dcs) {
 	        printf( "Invalid vmid [0-%d]\n", dvs.d_nr_dcs );
 	        ERROR_EXIT(EDVSBADDCID);
	}
MUKDEBUG("vmid=%d\n", vmid);
	rcode = mnx_getvminfo(vmid, &dcu);
	if( rcode ) ERROR_EXIT(rcode);
	dc_ptr = &dcu;
MUKDEBUG(DC_USR_FORMAT, DC_USR_FIELDS(dc_ptr));
}


/*===========================================================================*
 *				allowed					     *
 *===========================================================================*/
PUBLIC int allowed(name_buf, s_buf, mask)
char *name_buf;			/* pointer to file name to be EXECed */
struct stat *s_buf;		/* buffer for doing and returning stat struct*/
int mask;			/* R_BIT, W_BIT, or X_BIT */
{
/* Check to see if file can be accessed.  Return EACCES or ENOENT if the access
 * is prohibited.  If it is legal open the file and return a file descriptor.
 */
  int fd;
  int save_errno;

  /* Use the fact that mask for access() is the same as the permissions mask.
   * E.g., X_BIT in <minix/const.h> is the same as X_OK in <unistd.h> and
   * S_IXOTH in <sys/stat.h>.  tell_fs(DO_CHDIR, ...) has set PM's real ids
   * to the user's effective ids, so access() works right for setuid programs.
   */
  if (access(name_buf, mask) < 0) return(-errno);

  /* The file is accessible but might not be readable.  Make it readable. */
  tell_fs(SETUID, pm_ep, (int) SUPER_USER, (int) SUPER_USER);

  /* Open the file and fstat it.  Restore the ids early to handle errors. */
  fd = open(name_buf, O_RDONLY | O_NONBLOCK);
  save_errno = errno;		/* open might fail, e.g. from ENFILE */
  tell_fs(SETUID, pm_ep, (int) mp->mp_effuid, (int) mp->mp_effuid);
  if (fd < 0) return(-save_errno);
  if (fstat(fd, s_buf) < 0) panic(__FILE__,"allowed: fstat failed", NO_NUM);

  /* Only regular files can be executed. */
  if (mask == X_BIT && (s_buf->st_mode & I_TYPE) != I_REGULAR) {
	close(fd);
	return(EACCES);
  }
  return(fd);
}


/*===========================================================================*
 *				panic					     *
 *===========================================================================*/
PUBLIC void panic(who, mess, num)
char *who;			/* who caused the panic */
char *mess;			/* panic message string */
int num;			/* number to go with it */
{
/* An unrecoverable error has occurred.  Panics are caused when an internal
 * inconsistency is detected, e.g., a programming error or illegal value of a
 * defined constant. The process manager decides to taskexit.
 */
  message m;
  int s;

  /* Switch to primary console and print panic message. */
  check_sig(pm_proc_table[TTY_PROC_NR].mp_pid, SIGTERM);
  printf("PM panic (%s): %s", who, mess);
  if (num != NO_NUM) printf(": %d",num);
  printf("\n");
   
  /* Exit PM. */
  sys_exit(SELF);
}



/*===========================================================================*
 *				find_param				     *
 *===========================================================================*/
PUBLIC char *find_param(name)
const char *name;
{
  register const char *namep;
  register char *envp;

  for (envp = (char *) monitor_params; *envp != 0;) {
	for (namep = name; *namep != 0 && *namep == *envp; namep++, envp++)
		;
	if (*namep == '\0' && *envp == '=') 
		return(envp + 1);
	while (*envp++ != 0)
		;
  }
  return(NULL);
}

/*===========================================================================*
 *				get_mem_map				     *
 *===========================================================================*/
PUBLIC int get_mem_map(proc_nr, mem_map)
int proc_nr;					/* process to get map of */
struct mem_map *mem_map;			/* put memory map here */
{
  struct proc p;
  int s;

  if ((s=sys_getproc(&p, proc_nr)) != OK)
  	return(s);
  memcpy(mem_map, p.p_memmap, sizeof(p.p_memmap));
  return(OK);
}

/*===========================================================================*
 *				get_stack_ptr				     *
 *===========================================================================*/
PUBLIC int get_stack_ptr(proc_nr_e, sp)
int proc_nr_e;					/* process to get sp of */
vir_bytes *sp;					/* put stack pointer here */
{
  struct proc p;
  int s;

  if ((s=sys_getproc(&p, proc_nr_e)) != OK)
  	return(s);
  *sp = p.p_reg.sp;
  return(OK);
}

/*===========================================================================*
 *				proc_from_pid				     *
 *===========================================================================*/
PUBLIC int proc_from_pid(mp_pid)
pid_t mp_pid;
{
	int rmp;

	for (rmp = 0; rmp < NR_PROCS; rmp++)
		if (pm_proc_table[rmp].mp_pid == mp_pid)
			return rmp;

	return -1;
}



#endif /* TEMPORAL */
