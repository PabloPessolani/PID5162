/* This file takes care of those system calls that deal with time.
 *
 * The entry points into this file are
 *   pm_time:		perform the TIME system call
 *   pm_stime:		perform the STIME system call
 *   pm_times:		perform the TIMES system call
 */

#include "pm.h"

#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

/*===========================================================================*
 *				pm_time					     *
 *===========================================================================*/
int pm_time(void)
{
/* Perform the time(tp) system call. This returns the time in seconds since 
 * 1.1.1970.  MINIX is an astrophysically naive system that assumes the earth 
 * rotates at a constant rate and that such things as leap seconds do not 
 * exist.
 */
	molclock_t uptime;
	int s;

	if ( (s=sys_getuptime(&uptime)) != OK) 
		ERROR_EXIT(s);

	MUKDEBUG("uptime/clockTicks=%ld\n", uptime/clockTicks);

	mp->mp_reply.reply_time = (time_t) (boottime + (uptime/clockTicks));
	mp->mp_reply.reply_utime = (uptime%clockTicks)*(1000000*clockTicks);
	return(OK);
}

/*===========================================================================*
 *				pm_stime				     *
 *===========================================================================*/
int pm_stime(void)
{
/* Perform the stime(tp) system call. Retrieve the system's uptime (ticks 
 * since boot) and store the time in seconds at system boot in the global
 * variable 'boottime'.
 */
  molclock_t uptime;
  int s;

//  if (mp->mp_effuid != SUPER_USER) { 
//      return(EPERM);
//  }

  if ( (s=sys_getuptime(&uptime)) != OK) 
  	ERROR_EXIT(s);

  boottime = (long) pm_m_in.stime - (uptime/clockTicks);

MUKDEBUG("boottime =%ld\n", boottime);

  /* Also inform FS about the new system time. */
//  tell_fs(STIME, boottime, 0, 0);

  return(OK);
}

/*===========================================================================*
 *				pm_times				     *
 *===========================================================================*/
int pm_times(void)
{
/* Perform the times(buffer) system call. */
  struct mproc *rmp = mp;
  molclock_t t[5];
  int s;

  if (OK != (s=sys_times(pm_who_e, t)))
  	ERROR_EXIT(s);
  rmp->mp_reply.reply_t1 = t[0];		/* user time */
  rmp->mp_reply.reply_t2 = t[1];		/* system time */
  rmp->mp_reply.reply_t3 = rmp->mp_child_utime;	/* child user time */
  rmp->mp_reply.reply_t4 = rmp->mp_child_stime;	/* child system time */
  rmp->mp_reply.reply_t5 = t[4];		/* uptime since boot */

  return(OK);
}

