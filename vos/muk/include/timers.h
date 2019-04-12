/* This library provides generic watchdog timer management functionality.
 * The functions operate on a timer queue provided by the caller. Note that
 * the timers must use absolute time to allow sorting. The library provides:
 *
 *    tmrs_settimer:     (re)set a new watchdog timer in the timers queue 
 *    tmrs_clrtimer:     remove a timer from both the timers queue 
 *    tmrs_exptimers:    check for expired timers and run watchdog functions
 *
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 *    Adapted from tmr_settimer and tmr_clrtimer in src/kernel/clock.c. 
 *    Last modified: September 30, 2004.
 */

#ifndef _COM_TIMERS_H
#define _COM_TIMERS_H

#define molclock_t	dvkclock_t
#define moltimer 	dvktimer
#define moltimer_t dvktimer_t
#define tmr_func_t dvk_tmr_func_t

molclock_t tmrs_clrtimer(moltimer_t **tmrs, moltimer_t *tp, molclock_t *next_time);
void tmrs_exptimers(moltimer_t **tmrs, molclock_t now, molclock_t *new_head);
molclock_t tmrs_settimer(moltimer_t **tmrs, moltimer_t *tp, molclock_t exp_time, tmr_func_t watchdog, molclock_t *new_head);

#endif /* _COM_TIMERS_H */

