
long usr_getep(int pid);
long usr_sendrec_T(int endpoint , message *mptr, long timeout);
long usr_getdvsinfo(dvs_usr_t *dvsu_ptr);
long usr_getdcinfo(int dcid, dc_usr_t *dcu_ptr);
long usr_getprocinfo(int dcid, int p_nr, proc_usr_t *p_usr);
long usr_bind_X(int cmd, int dcid, int pid, int endpoint, int nodeid);
#define usr_lclbind(dcid,pid,endpoint) 	usr_bind_X(LCL_BIND, dcid, pid, endpoint, LOCALNODE)

