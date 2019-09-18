/* This file handles the 4 system calls that get and set uids and gids.
 * It also handles getpid(), setsid(), and getpgrp().  The code for each
 * one is so tiny that it hardly seemed worthwhile to make each a separate
 * function.
 */
#include "pm.h"

#undef DVK_GLOBAL_HERE
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

/*===========================================================================*
 *				pm_getset				     						*
 *===========================================================================*/
int pm_getset(void)
{
/* Handle MOLGETUID, MOLGETGID, MOLGETPID, MOLGETPGRP, MOLSETUID, MOLSETGID, MOLSETSID.  The four
 * MOLGETs and MOLSETSID return their primary results in 'r'.  MOLGETUID, MOLGETGID, and
 * MOLGETPID also return secondary results (the effective IDs, or the parent
 * process ID) in 'reply_res2', which is returned to the user.
 */

	mproc_t *rmp = mp;
	proc_usr_t *rkp = kp;
	int ret, proc;
	
MUKDEBUG("pm_call_nr=%d\n", pm_call_nr);
  switch(pm_call_nr) {
	case MOLGETUID:
		ret = rmp->mp_realuid;
		rmp->mp_reply.reply_res2 = rmp->mp_effuid;
		MUKDEBUG("ret=%d\n", ret);
		break;
	case MOLGETGID:
		ret = rmp->mp_realgid;
		rmp->mp_reply.reply_res2 = rmp->mp_effgid;
		MUKDEBUG("ret=%d\n", ret);
		break;
	case MOLGETPID:
		MUKDEBUG(PM_PROC_FORMAT,PM_PROC_FIELDS(rmp));
		rmp->mp_reply.reply_res2 = pm_proc_table[rmp->mp_parent].mp_pid;
		ret = pm_isokendpt(pm_m_in.m_source, &proc);
		if( (ret == OK) && (proc >= 0 ) ) {
			rmp->mp_reply.reply_res3 = pm_proc_table[proc].mp_pid;
		} else {
			ERROR_RETURN(ret);
		}
		ret = pm_proc_table[pm_who_p].mp_pid;
		MUKDEBUG("ret=%d\n", ret);
		break;

	case MOLSETEUID:
	case MOLSETUID:
		if (rmp->mp_realuid != (uid_t) pm_m_in.usr_id && 
				rmp->mp_effuid != SUPER_USER)
			ERROR_RETURN(EDVSPERM);
		if(pm_call_nr == MOLSETUID) rmp->mp_realuid = (uid_t) pm_m_in.usr_id;
		rmp->mp_effuid = (uid_t) pm_m_in.usr_id;
		tell_fs(MOLSETUID, pm_who_e, rmp->mp_realuid, rmp->mp_effuid);
		ret = OK;
		MUKDEBUG("ret=%d\n", ret);
		break;

	case MOLSETEGID:
	case MOLSETGID:
		if (rmp->mp_realgid != (gid_t) pm_m_in.grp_id && 
				rmp->mp_effuid != SUPER_USER)
			return(EDVSPERM);
		if(pm_call_nr == MOLSETGID) rmp->mp_realgid = (gid_t) pm_m_in.grp_id;
		rmp->mp_effgid = (gid_t) pm_m_in.grp_id;
		tell_fs(MOLSETGID, pm_who_e, rmp->mp_realgid, rmp->mp_effgid);
		ret = OK;
		MUKDEBUG("ret=%d\n", ret);
		break;

	case MOLSETSID:
		if (rmp->mp_procgrp == rmp->mp_pid) ERROR_RETURN(EDVSPERM);
		rmp->mp_procgrp = rmp->mp_pid;
		tell_fs(MOLSETSID, pm_who_e, 0, 0);
		/* fall through */
	case MOLGETPGRP:
		ret = rmp->mp_procgrp;
		MUKDEBUG("ret=%d\n", ret);
		break;
	case MOLSETPNAME:
		proc = _ENDPOINT_P(pm_m_in.M3_ENDPT);
		rkp =  (proc_usr_t *) PM_KPROC(proc);
		/* Tell SYSTASK copy the kernel entry to pm_kproc[parent_nr]  	*/
		if((ret =sys_procinfo(pm_who_p)) != OK) 
			ERROR_EXIT(ret);
		MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(rkp));

		if((ret =sys_setpname(pm_m_in.M3_ENDPT,pm_m_in.M3_NAME)) != OK) 
			ERROR_EXIT(ret);

		if((ret =sys_procinfo(pm_who_p)) != OK) 
			ERROR_EXIT(ret);
		MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(rkp));
		
		break;
	default:
		ERROR_RETURN(EDVSINVAL);
		break;	
  }
  return(ret);
}
