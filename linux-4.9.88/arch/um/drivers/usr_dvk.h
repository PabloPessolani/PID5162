
long dvk_getep(int pid);
long dvk_sendrec_T(int endpoint , message *mptr, long timeout);
long dvk_getdvsinfo(dvs_usr_t *dvsu_ptr);
long dvk_getdcinfo(int dcid, dc_usr_t *dcu_ptr);
long dvk_getprocinfo(int dcid, int p_nr, proc_usr_t *p_usr);
long dvk_bind_X(int cmd, int dcid, int pid, int endpoint, int nodeid);
#define dvk_lclbind(dcid,pid,endpoint) 	dvk_bind_X(LCL_BIND, dcid, pid, endpoint, LOCALNODE)


