#ifndef _COM_PRIV_USR_H
#define _COM_PRIV_USR_H

#ifndef _DVSCOM_TIMERS_H
#include "timers.h"
#endif // _DVSCOM_TIMERS_H

#ifndef KIPC_H
#include "kipc.h"
#endif // KIPC_H

#ifndef _DVKCOM_TYPES_H
#include "types.h"
#endif // _DVKCOM_TYPES_H

#define USER_PRIV	0x0000
#define SERVER_PRIV	0x0001
#define TASK_PRIV	0x0002
#define SYSTEM_PRIV	0x0003
#define KERNEL_PRIV	0x0004
#define PROXY_PRIV	0x0005

struct priv_usr {

  dvk_id_t 	priv_id;			/* index of this system structure */
  int		priv_warn;			/* process to warn when the process exit/fork */
  int		priv_level;			/* privilege level		*/

  short 	priv_trap_mask;		/* allowed system call traps */
  ipc_map_t priv_ipc_from;		/* allowed callers to receive from */
  ipc_map_t priv_ipc_to;		/* allowed destination processes */
  dvk_map_t	priv_dvk_allowed;		/* allowed dvk calls */

  dvktimer_t priv_alarm_timer;	/* synchronous alarm timer */ 

};
typedef struct priv_usr priv_usr_t;

#define PRIV_USR_FORMAT "priv_id=%d priv_warn=%d priv_level=%d\n"
#define PRIV_USR_FIELDS(p) p->priv_id, p->priv_warn, p->priv_level

#endif /* _COM_PRIV_USR_H */
