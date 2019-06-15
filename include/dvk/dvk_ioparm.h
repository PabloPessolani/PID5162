#ifndef _DVK_DVK_IOPARM_H
#define _DVK_DVK_IOPARM_H

struct parm_rcvrqst_s {
	message			*parm_mptr;
	unsigned long	parm_tout;
};
typedef struct parm_rcvrqst_s parm_rcvrqst_t;

struct parm_getdcinfo_s {
	int				parm_dcid;
	dc_usr_t		*parm_dc;
};
typedef struct parm_getdcinfo_s parm_getdcinfo_t;

struct parm_getnodeinfo_s {
	int				parm_nodeid;
	node_usr_t		*parm_node;
};
typedef struct parm_getnodeinfo_s parm_getnodeinfo_t;

struct parm_relay_s {
	int				parm_ep;
	message			*parm_mptr;
};
typedef struct parm_relay_s parm_relay_t;

struct parm_wakeup_s {
	int				parm_dcid;
	int				parm_ep;
};
typedef struct parm_wakeup_s parm_wakeup_t;

struct parm_put2lcl_s {
	cmd_t			*parm_cmd;
	proxy_payload_t	*parm_pay;
};
typedef struct parm_put2lcl_s parm_put2lcl_t;

struct parm_dcnode_s {
	int				parm_dcid;
	int				parm_nodeid;
};
typedef struct parm_dcnode_s parm_dcnode_t;

struct parm_dvsinit_s {
	int				parm_nodeid;
	dvs_usr_t		*parm_dvs;
};
typedef struct parm_dvsinit_s parm_dvsinit_t;

struct parm_pxconn_s {
	int				parm_pxid;
	int				parm_sts;
};
typedef struct parm_pxconn_s parm_pxconn_t;

struct parm_wait4bind_s {
	int				parm_cmd;
	int				parm_ep;
	unsigned long	parm_tout;
};
typedef struct parm_wait4bind_s parm_wait4bind_t;

struct parm_unbind_s {
	int				parm_dcid;
	int				parm_ep;
	unsigned long	parm_tout;
};
typedef struct parm_unbind_s parm_unbind_t;

struct parm_ipc_s {
	int				parm_ep;
	message 		*parm_mptr;
	unsigned long	parm_tout;

};
typedef struct parm_ipc_s parm_ipc_t;

struct parm_notify_s {
	int				parm_nr;
	int				parm_ep;
	int				parm_val;
};
typedef struct parm_notify_s parm_notify_t;

struct parm_priv_s {
	int				parm_dcid;
	int				parm_ep;
	priv_usr_t		*parm_priv;
};
typedef struct parm_priv_s parm_priv_t;

struct parm_get2rmt_s {
	cmd_t			*parm_cmd;
	proxy_payload_t	*parm_pay;
	unsigned long	parm_tout;
};
typedef struct parm_get2rmt_s parm_get2rmt_t;

struct parm_nodeup_s {
	char			*parm_name;
	int				parm_nodeid;
	int				parm_pxid;
};
typedef struct parm_nodeup_s parm_nodeup_t;

struct parm_proxyinfo_s {
	int				parm_pxid;
	proc_usr_t		*parm_spx;
	proc_usr_t		*parm_rpx;
};
typedef struct parm_proxyinfo_s parm_proxyinfo_t;

struct parm_bind_s {
	int		parm_cmd;
	int		parm_dcid;
	int		parm_pid;
	int		parm_ep;
	int		parm_nodeid;	
};
typedef struct parm_bind_s parm_bind_t;
#define PARM_BIND_FORMAT 	"parm_cmd=%d parm_dcid=%d parm_pid=%d parm_ep=%d parm_nodeid=%d\n"
#define PARM_BIND_FIELDS(p) 	p->parm_cmd,p->parm_dcid, p->parm_pid, p->parm_ep, p->parm_nodeid

struct parm_pxbind_s {
	char 	*parm_name;
	int		parm_pxid;
	int		parm_spid;
	int		parm_rpid;
	int		parm_maxbuf;	
};
typedef struct parm_pxbind_s parm_pxbind_t;

struct parm_procinfo_s {
	int				parm_dcid;
	int				parm_nr;
	proc_usr_t		*parm_proc;
};
typedef struct parm_procinfo_s parm_procinfo_t;

#define parm_vcopy_t vcopy_t

#endif // _DVK_DVK_IOPARM_H
