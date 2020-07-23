
#ifndef PRIV_H
#define PRIV_H

#define SYS_PROC	0x10	/* system processes have own priv structure */

struct priv {

	priv_usr_t priv_usr;		/* Privileges user fields 		*/

	ipc_map_t 	priv_notify_pending; /* bit map with pending notifications */
	irq_id_t 	priv_int_pending;
	ksigset_t 	priv_sig_pending;
	update_t	priv_updt_pending;
};
typedef struct priv priv_t;

#endif /* PRIV_H */
