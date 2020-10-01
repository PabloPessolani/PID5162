#include "syslib.h"
#include <./i386-linux-gnu/bits/posix1_lim.h>

/*===========================================================================*
 *				sys_bindproc				     *
 * A System process:
 *		- SERVER/TASK: is just registered to the kernel but not to SYSTASK.
  *		- REMOTE USER: it must be registered to the kernel and to SYSTASK     *
 * m_in.M3_LPID: Linux PID 		or node ID				     *
 * m_in.M3_SLOT < dc_ptr->dc_nr_sysprocs: for servers and tasks
 * m_in.M3_OPER  could be
  *	SELF_BIND		0
 * 	LCL_BIND		1
 *	 RMT_BIND		2
 * 	BKUP_BIND		3
 *===========================================================================*/
char* get_process_name_by_pid( int pid);
 
int sys_rbindproc(int sysproc_ep, int lpid, int oper, int st_nodeid)
{
	int rcode, i;
	message m, *m_ptr;
	char *name; 
	
	LIBDEBUG("SYS_BINDPROC request to SYSTEM(%d) sysproc_ep=%d lpid=%d oper=%X\n",
		st_nodeid, sysproc_ep, lpid, oper);
	m_ptr = &m;
	m_ptr->M3_ENDPT = sysproc_ep;
	m_ptr->M3_LPID  = lpid;
	m_ptr->M3_OPER  = (char *) oper;
	if( st_nodeid == local_nodeid && (oper != RMT_BIND) ){
		strncpy(m_ptr->m3_ca1,get_process_name_by_pid(lpid), (M3_STRING-1));
	}
	
	rcode = _taskcall(SYSTASK(st_nodeid), SYS_BINDPROC, &m);
	if( rcode < 0) return(rcode);
	LIBDEBUG("endpoint=%d\n",m_ptr->PR_ENDPT);
	return(m_ptr->PR_ENDPT);
}

extern Task *pproc[];

 char* get_process_name_by_pid( int pid)
{
	int i;
	
	LIBDEBUG("pid=%d\n",pid);
	for( i = 0; i < (NR_PROCS+NR_TASKS); i++){
		if( pproc[i] == NULL) continue;
		if( pproc[i]->id != pid) continue;
		LIBDEBUG("name=[%s]\n",pproc[i]->name);
		return(pproc[i]->name);
	}
    return NULL;
}
