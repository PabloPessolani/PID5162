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
 *				pm_dump										     *
 *===========================================================================*/
int pm_dump(void)
{
	message *m_ptr;

	m_ptr= &pm_m_in; 
	MUKDEBUG("dump type=%X\n",m_ptr->m1_i1);
	
	switch(m_ptr->m1_i1){
		case DMP_PM_PROC:
			dmp_pm_proc();
			break;	
		case WDMP_PM_PROC:
			wdmp_pm_proc(m_ptr);
			break;				
		default:
			ERROR_RETURN(EDVSBADCALL);
	}
	return(OK);
}

/*===========================================================================*
 *				dmp_pm_proc				     						*
 *===========================================================================*/
int dmp_pm_proc(void)
{
	int i;
	proc_usr_t	*proc_ptr;
	mproc_t *mp_ptr;
	
	fprintf(dump_fd, "\n");
	fprintf(dump_fd,"===========================================================================\n");
	fprintf(dump_fd,"====================== dmp_pm_proc =======================================\n");
	fprintf(dump_fd,"===========================================================================\n");
	fprintf(dump_fd, "\n");
	fprintf(dump_fd, "-endp- -pid- -ppid flags nice pgroup -ruid -euid -rgid -egid name\n");	

	assert(pm_proc_table != NULL);
	assert(kproc_map != NULL);

	for(i = 0 ; i < dc_ptr->dc_nr_procs; i++) {
		mp_ptr = &pm_proc_table[i];
		proc_ptr = (proc_usr_t *) PROC_MAPPED(i);
		if ( !(mp_ptr->mp_flags & IN_USE)){
			continue;
		}
		fprintf(dump_fd, "%6d %5d %5d %5lX %4d %6d %5d %5d %5d %5d %-15.15s\n",
				mp_ptr->mp_endpoint,
				mp_ptr->mp_pid,
				mp_ptr->mp_parent,
				mp_ptr->mp_flags,
				mp_ptr->mp_nice,
				mp_ptr->mp_procgrp,
				mp_ptr->mp_realuid,
				mp_ptr->mp_effuid,
				mp_ptr->mp_realgid,
				mp_ptr->mp_effgid,
				proc_ptr->p_name);
	}
	fprintf(dump_fd, "\n");

}

/*===========================================================================*
 *				wdmp_pm_proc				     						*
 *===========================================================================*/
int wdmp_pm_proc(message *m_ptr)
{
	int i;
	proc_usr_t	*proc_ptr;
	mproc_t *mp_ptr;
	char *page_ptr;

	MUKDEBUG("\n");
	page_ptr = m_ptr->m1_p1;
	
	(void)strcat(page_ptr,"<table>\n");
	(void)strcat(page_ptr,"<thead>\n");
	(void)strcat(page_ptr,"<tr>\n");
	(void)strcat(page_ptr,"<th>-endp-</th>\n");
	(void)strcat(page_ptr,"<th>-pid-</th>\n");
	(void)strcat(page_ptr,"<th>-ppid</th>\n");
	(void)strcat(page_ptr,"<th>flags</th>\n");
	(void)strcat(page_ptr,"<th>nice</th>\n");
	(void)strcat(page_ptr,"<th>pgroup</th>\n");
	(void)strcat(page_ptr,"<th>-ruid</th>\n");
	(void)strcat(page_ptr,"<th>-euid</th>\n");
	(void)strcat(page_ptr,"<th>-rgid</th>\n");
	(void)strcat(page_ptr,"<th>-egid</th>\n");
	(void)strcat(page_ptr,"<th>name</th>\n");
	(void)strcat(page_ptr,"</tr>\n");
	(void)strcat(page_ptr,"</thead>\n");
	(void)strcat(page_ptr,"<tbody>\n");
	
	assert(pm_proc_table != NULL);
	assert(kproc_map != NULL);

	for(i = 0 ; i < dc_ptr->dc_nr_procs; i++) {
		mp_ptr = &pm_proc_table[i];
		proc_ptr = (proc_usr_t *) PROC_MAPPED(i);
		if ( !(mp_ptr->mp_flags & IN_USE)){
			continue;
		}
		(void)strcat(page_ptr,"<tr>\n");
		sprintf(is_buffer, "<td>%6d</td> <td>%5d</td> <td>%5d</td> <td>%5lX</td> <td>%4d</td> "
				"<td>%6d</td> <td>%5d</td> <td>%5d</td> <td>%5d</td> <td>%5d</td> <td>%-15.15s</td>\n",
				mp_ptr->mp_endpoint,
				mp_ptr->mp_pid,
				mp_ptr->mp_parent,
				mp_ptr->mp_flags,
				mp_ptr->mp_nice,
				mp_ptr->mp_procgrp,
				mp_ptr->mp_realuid,
				mp_ptr->mp_effuid,
				mp_ptr->mp_realgid,
				mp_ptr->mp_effgid,
				proc_ptr->p_name);
		(void)strcat(page_ptr,is_buffer);
		(void)strcat(page_ptr,"</tr>\n");
	}
  (void)strcat(page_ptr,"</tbody></table>\n");
}
