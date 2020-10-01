/* Debugging dump procedures for SYSTASK */

#include "../../tasks/systask/systask.h"

#undef DVK_GLOBAL_HERE
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

void p_rts_flags_str(int flags, char *str );
void p_misc_flags_str(int flags, char *str );
void p_endpoint_str(int endpoint, char *str);
void bm2ascii(char *buf, unsigned long int bitmap);


#if USE_SYSDUMP 
/*===========================================================================*
 *				st_dump										     *
 *===========================================================================*/
int st_dump(message *m_ptr)
{
	MUKDEBUG(MSG1_FORMAT,MSG1_FIELDS(m_ptr));

	switch(m_ptr->m1_i1){
		case DMP_SYS_DC:
			dmp_sys_dc();
			break;
		case DMP_SYS_PROC:
			dmp_sys_proc();
			break;	
#ifdef ENABLE_DMP_SYS_STATS 				
		case DMP_SYS_STATS:
			dmp_sys_stats();
			break;		
#endif // ENABLE_DMP_SYS_STATS 		

#ifdef ENABLE_DMP_SYS_PRIV 		
		case DMP_SYS_PRIV:
			dmp_sys_priv();
			break;
#endif //ENABLE_DMP_SYS_PRIV 
		
		case WDMP_SYS_DC:
			wdmp_sys_dc(m_ptr);
			break;
		case WDMP_SYS_PROC:
			wdmp_sys_proc(m_ptr);
			break;		
#ifdef ENABLE_DMP_SYS_STATS 				
		case WDMP_SYS_STATS:
			wdmp_sys_stats(m_ptr);
			break;		
#endif // ENABLE_DMP_SYS_STATS 		

#ifdef ENABLE_WDMP_SYS_PRIV 		
		case WDMP_SYS_PRIV:
			wdmp_sys_priv(m_ptr);
			break;
#endif //ENABLE_DMP_SYS_PRIV			
		default:
			ERROR_RETURN(EDVSBADCALL);
	}
	return(OK);
}

/*===========================================================================*
 *				dmp_sys_dc												     *
 *===========================================================================*/
void dmp_sys_dc(void)
{
	char bmbuf[BITMAP_32BITS+1];

	bm2ascii(bmbuf, dc_ptr->dc_nodes);      

	fprintf(dump_fd, "\n");
	fprintf(dump_fd,"===========================================================================\n");
	fprintf(dump_fd,"======================  dmp_sys_dc  =======================================\n");
	fprintf(dump_fd,"===========================================================================\n");
	fprintf(dump_fd, "\n");

	fprintf(dump_fd, "dcid=%d\nflags=%X\nnr_procs=%d\nnr_tasks=%d\n"
	"nr_sysprocs=%d\nnr_nodes=%d\ndc_nodes=%lX\ndc_pid=%ld\n"
	"warn2proc=%d\nwarnmsg=%d\ndc_name=%s\n",
		dc_ptr->dc_dcid,
		dc_ptr->dc_flags,
		dc_ptr->dc_nr_procs,
		dc_ptr->dc_nr_tasks,
		dc_ptr->dc_nr_sysprocs,
		dc_ptr->dc_nr_nodes,
		dc_ptr->dc_nodes,
		dc_ptr->dc_pid,		
		dc_ptr->dc_warn2proc,
		dc_ptr->dc_warnmsg,
		dc_ptr->dc_name);
	fprintf(dump_fd, "nodes 33222222222211111111110000000000\n"); 
	fprintf(dump_fd, "      10987654321098765432109876543210\n");
	fprintf(dump_fd, "      %s\n", bmbuf);
	fprintf(dump_fd, "\n");
}

#ifdef ENABLE_DMP_SYS_STATS 		
/*===========================================================================*
 *				dmp_sys_stats  					    				     *
 *===========================================================================*/
void dmp_sys_stats(void)
{
	int i;
	muk_proc_t	*task_ptr;
    proc_usr_t *proc_ptr;

	fprintf(dump_fd, "\n");
	fprintf(dump_fd,"===========================================================================\n");
	fprintf(dump_fd,"====================== dmp_sys_stats ======================================\n");
	fprintf(dump_fd,"===========================================================================\n");
	fprintf(dump_fd, "\n");
	fprintf(dump_fd, "DC pnr -endp -lpid/vpid- nd --lsnt-- --rsnt-- -lcopy-- -rcopy-- name\n");	

//	assert(kproc_map != NULL);
	
	for(i = (-dc_ptr->dc_nr_tasks) ; i < dc_ptr->dc_nr_procs; i++) {
		task_ptr = (Task *) get_task(i);
		if( task_ptr == NULL) continue;
		if (TEST_BIT(task_ptr->p_proc->p_rts_flags, BIT_SLOT_FREE)) {
			continue;
		}
		proc_ptr = task_ptr->p_proc;
		fprintf(dump_fd, "%2d %3d %5d %5d/%5d %2d %8ld %8ld %8ld %8ld %-15.15s\n",
				proc_ptr->p_dcid,
				proc_ptr->p_nr,
				proc_ptr->p_endpoint,
				proc_ptr->p_lpid,
				proc_ptr->p_vpid,
				proc_ptr->p_nodeid,
				proc_ptr->p_lclsent,
				proc_ptr->p_rmtsent,
				proc_ptr->p_lclcopy,
				proc_ptr->p_rmtcopy,
				proc_ptr->p_name);	
	}				
	fprintf(dump_fd, "\n");

}
#endif // ENABLE_DMP_SYS_STATS 		

/*===========================================================================*
 *				dmp_sys_proc					    				     *
 *===========================================================================*/
void dmp_sys_proc(void)
{
	int i;
	static char rts_str[20];
	static char mis_str[20];
	static char getf_str[10];
	static char send_str[10];
    proc_usr_t *proc_ptr;
	muk_proc_t	*task_ptr;
	
	fprintf(dump_fd, "\n");
	fprintf(dump_fd,"===============================================================================\n");
	fprintf(dump_fd,"====================== dmp_sys_proc ===========================================\n");
	fprintf(dump_fd,"===============================================================================\n");
	fprintf(dump_fd, "\n");
	fprintf(dump_fd, "DC pnr -endp -id- ---state--- nd ------flag------ ----misc--- -getf -sndt name\n");	

//	assert(kproc_map != NULL);

	for(i = (-dc_ptr->dc_nr_tasks) ; i < dc_ptr->dc_nr_procs; i++) {
		task_ptr = (Task *) get_task(i);
		if( task_ptr == NULL) continue;
		if (TEST_BIT(task_ptr->p_proc->p_rts_flags, BIT_SLOT_FREE)) {
			continue;
		}
		proc_ptr = task_ptr->p_proc;	
	    p_rts_flags_str(proc_ptr->p_rts_flags, rts_str);		
	    p_misc_flags_str(proc_ptr->p_misc_flags, mis_str);	
	    p_endpoint_str(proc_ptr->p_getfrom,	getf_str);		
	    p_endpoint_str(proc_ptr->p_sendto,  send_str);
		fprintf(dump_fd, "%2d %3d %5d %4d %11s %2d %16s %11s %5s %5s %-15.15s\n",
				dc_ptr->dc_dcid,
				proc_ptr->p_nr,
				proc_ptr->p_endpoint,
				task_ptr->id,
				task_ptr->state,
				proc_ptr->p_nodeid,
				rts_str,
				mis_str,
				getf_str,
				send_str,
				task_ptr->name);
	}
	fprintf(dump_fd, "\n");
}

#ifdef ENABLE_DMP_SYS_PRIV 		
/*===========================================================================*
 *				dmp_sys_priv					    				     *
 *===========================================================================*/
void dmp_sys_priv(void)
{
	int i;
	muk_proc_t	*task_ptr;
    proc_usr_t *proc_ptr;

	fprintf(dump_fd, "\n");
	fprintf(dump_fd,"===========================================================================\n");
	fprintf(dump_fd,"====================== dmp_sys_priv =======================================\n");
	fprintf(dump_fd,"===========================================================================\n");
	fprintf(dump_fd, "\n");
	fprintf(dump_fd, "-nr- --id- -warn level ipcfr ipcto name \n");

//	assert(kproc_map != NULL);

	for(i = (-dc_ptr->dc_nr_tasks) ; i < dc_ptr->dc_nr_procs; i++) {
		task_ptr = get_task(i);
		if( task_ptr == NULL) continue;
		if (TEST_BIT(task_ptr->p_proc->p_rts_flags, BIT_SLOT_FREE)) {
			continue;
		}
		proc_ptr = task_ptr->p_proc;
		fprintf(dump_fd, "%3d %5d %5d %5d %5d %5d %5d %5d% -15.15s\n",
				proc_ptr->p_nr,
				proc_ptr->p_priv.priv_usr.priv_id,
				proc_ptr->p_priv.priv_usr.priv_warn,
				proc_ptr->p_priv.priv_usr.priv_level,
				
				proc_ptr->p_priv.priv_usr.priv_trap_mask,
				proc_ptr->p_priv.priv_usr.priv_ipc_from,
				proc_ptr->p_priv.priv_usr.priv_ipc_to,
				proc_ptr->p_priv.priv_usr.priv_call_mask
				proc_ptr->p_name);
	}
	fprintf(dump_fd, "\n");
}
#endif //ENABLE_DMP_SYS_PRIV 		

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//					WEB FORMAT 
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


/*===========================================================================*
 *				wdmp_sys_dc												     *
 *===========================================================================*/
void wdmp_sys_dc(message *m_ptr)
{
	char *page_ptr;
	char bmbuf[BITMAP_32BITS+1];

	MUKDEBUG("\n");
	page_ptr = m_ptr->m1_p1;
	bm2ascii(bmbuf, dc_ptr->dc_nodes);      

	(void)strcat(page_ptr,"<table>\n");
	(void)strcat(page_ptr,"<thead>\n");
	(void)strcat(page_ptr,"<tr>\n");
	(void)strcat(page_ptr,"<th colspan=\"2\">Dump of DC info.</th>\n");
	(void)strcat(page_ptr,"</tr></thead>\n");
	(void)strcat(page_ptr,"<tbody>\n");
	    
	(void)strcat(page_ptr,"<tr><th colspan=\"2\">Distributed Container Parameters:</th></tr>\n");

	sprintf(is_buffer, "<tr align=\"left\"><td>dc_dcid</td><td>%d</td></tr>\n", dc_ptr->dc_dcid); 
	(void)strcat(page_ptr,is_buffer);
	sprintf(is_buffer, "<tr align=\"left\"><td>dc_flags</td><td>%X</td></tr>\n", dc_ptr->dc_flags); 
	(void)strcat(page_ptr,is_buffer);
	sprintf(is_buffer, "<tr align=\"left\"><td>dc_nr_procs</td><td>%5u</td></tr>\n", dc_ptr->dc_nr_procs); 
	(void)strcat(page_ptr,is_buffer);
	sprintf(is_buffer, "<tr align=\"left\"><td>dc_nr_tasks</td><td>%5u</td></tr>\n", dc_ptr->dc_nr_tasks); 
	(void)strcat(page_ptr,is_buffer);
	sprintf(is_buffer, "<tr align=\"left\"><td>dc_nr_sysprocs</td><td>%5u</td></tr>\n", dc_ptr->dc_nr_sysprocs); 
	(void)strcat(page_ptr,is_buffer);
	sprintf(is_buffer, "<tr align=\"left\"><td>dc_nr_nodes</td><td>%5u</td></tr>\n", dc_ptr->dc_nr_nodes); 
	(void)strcat(page_ptr,is_buffer);
	sprintf(is_buffer, "<tr align=\"left\"><td>dc_nodes</td><td>%5u</td></tr>\n", dc_ptr->dc_nodes); 
	(void)strcat(page_ptr,is_buffer);
	sprintf(is_buffer, "<tr align=\"left\"><td>dc_pid</td><td>%5u</td></tr>\n", dc_ptr->dc_pid); 
	(void)strcat(page_ptr,is_buffer);
	sprintf(is_buffer, "<tr align=\"left\"><td>dc_warn2proc</td><td>%5u</td></tr>\n", dc_ptr->dc_warn2proc); 
	(void)strcat(page_ptr,is_buffer);
	sprintf(is_buffer, "<tr align=\"left\"><td>dc_warnmsg</td><td>%5u</td></tr>\n", dc_ptr->dc_warnmsg); 
	(void)strcat(page_ptr,is_buffer);
	sprintf(is_buffer, "<tr align=\"left\"><td>_</td><td>33222222222211111111110000000000</td></tr>\n"); 
	(void)strcat(page_ptr,is_buffer);
	sprintf(is_buffer, "<tr align=\"left\"><td>_</td><td>10987654321098765432109876543210</td></tr>\n"); 
	(void)strcat(page_ptr,is_buffer);
	sprintf(is_buffer, "<tr align=\"left\"><td>nodes</td><td>%32s</td></tr>\n", bmbuf); 
	(void)strcat(page_ptr,is_buffer);
	
    (void)strcat(page_ptr,"</tbody></table>\n");

	MUKDEBUG("%s\n",page_ptr);	
	MUKDEBUG("strlen(page_ptr)=%d\n", strlen(page_ptr));	
}

/*===========================================================================*
 *				wdmp_sys_proc					    				     *
 *===========================================================================*/
void wdmp_sys_proc(message *m_ptr)
{
	int i;
	static char rts_str[20];
	static char mis_str[20];
	static char getf_str[10];
	static char send_str[10];
	static char migr_str[10];
	static char pxy_str[10];
	muk_proc_t	*task_ptr;
    proc_usr_t *proc_ptr;
	char *page_ptr;
	
	MUKDEBUG("\n");
	page_ptr = m_ptr->m1_p1;
	
	(void)strcat(page_ptr,"<table>\n");
	(void)strcat(page_ptr,"<thead>\n");
	(void)strcat(page_ptr,"<tr>\n");
	(void)strcat(page_ptr,"<th>DC</th>\n");
	(void)strcat(page_ptr,"<th>pnr</th>\n");
	(void)strcat(page_ptr,"<th>-endp</th>\n");
	(void)strcat(page_ptr,"<th>-id-</th>\n");
	(void)strcat(page_ptr,"<th>---state---</th>\n");
	(void)strcat(page_ptr,"<th>nd</th>\n");
	(void)strcat(page_ptr,"<th>------flags-----</th>\n");
	(void)strcat(page_ptr,"<th>----misc---</th>\n");
	(void)strcat(page_ptr,"<th>-getf</th>\n");
	(void)strcat(page_ptr,"<th>-sndt</th>\n");
	(void)strcat(page_ptr,"<th>name</th>\n");

	(void)strcat(page_ptr,"</tr>\n");
	(void)strcat(page_ptr,"</thead>\n");
	(void)strcat(page_ptr,"<tbody>\n");
	
	// assert(kproc_map != NULL);

	for(i = (-dc_ptr->dc_nr_tasks) ; i < dc_ptr->dc_nr_procs; i++) {
		task_ptr = (Task *) get_task(i);
		if( task_ptr == NULL) continue;
		if (TEST_BIT(task_ptr->p_proc->p_rts_flags, BIT_SLOT_FREE)) {
			continue;
		}
	    (void)strcat(page_ptr,"<tr>\n");
		proc_ptr = task_ptr->p_proc;	
		p_rts_flags_str(proc_ptr->p_rts_flags, rts_str);		
	    p_misc_flags_str(proc_ptr->p_misc_flags, mis_str);	
	    p_endpoint_str(proc_ptr->p_getfrom,	getf_str);		
	    p_endpoint_str(proc_ptr->p_sendto,  send_str);	
		sprintf(is_buffer, "<td>%2d</td> <td>%3d</td> <td>%5d</td> <td>%5ld</td>"
			"<td>%11s</td> <td>%2d</td> <td>%16s</td> <td>%11s</td> <td>%5s</td> <td>%5s</td> "
			"<td>%-15.15s</td> \n",
			dc_ptr->dc_dcid,
			proc_ptr->p_nr,
			proc_ptr->p_endpoint,
			task_ptr->id,	
			task_ptr->state,
			proc_ptr->p_nodeid,
			rts_str,
			mis_str,
			getf_str,
			send_str,
			task_ptr->name);
		
		(void)strcat(page_ptr,is_buffer);
		(void)strcat(page_ptr,"</tr>\n");
	}
	(void)strcat(page_ptr,"</tbody></table>\n");

	(void)strcat(page_ptr,"<table>\n");
	(void)strcat(page_ptr,"<thead>\n");
	(void)strcat(page_ptr,"<tr><th>Process Status Flags:</th></tr>\n");
	(void)strcat(page_ptr,"</thead>\n");

	(void)strcat(page_ptr,"<tbody>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>A: NO_MAP- keeps unmapped forked child from running </td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>S: SENDING- process blocked trying to send 	</td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>R: RECEIVING- process blocked trying to receive</td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>I: SIGNALED- set when new kernel signal arrives</td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>P: SIG_PENDING- unready while signal being processed</td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>T: P_STOP-  set when process is being traced </td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>V: NO_PRIV- keep forked system process from running</td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>Y: NO_PRIORITY- process has been stopped</td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>e: NO_ENDPOINT- process cannot send or receive messages</td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>c: ONCOPY- A copy request is pending	</td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>m: MIGRATING- the process is waiting that it ends its migration </td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>r: REMOTE- the process is running on a remote host</td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>o: RMTOPER- a process descriptor is just used for a  remote operation</td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>g: WAITMIGR- a destination process is migrating</td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>u: WAITUNBIND- a process is waiting another unbinding</td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>b: WAITBIND-  a process is waiting another binding</td></tr>\n");
	(void)strcat(page_ptr,"</tbody></table>\n");

	(void)strcat(page_ptr,"<table>\n");
	(void)strcat(page_ptr,"<thead>\n");
	(void)strcat(page_ptr,"<tr><th>Process Miscelaneous Flags:</th></tr>\n");
	(void)strcat(page_ptr,"</thead>\n");
	(void)strcat(page_ptr,"<tbody>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>X: MIS_PROXY- The process is a proxy </td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>C: MIS_CONNECTED- The proxy is connected 	</td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>N: MIS_BIT_NOTIFY- A notify is pending </td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>M: MIS_NEEDMIG- The proccess need to migrate </td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>B: MIS_RMTBACKUP- The proccess is a remote process' backup</td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>L: MIS_GRPLEADER-  The proccess is the thread group leader </td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>K: MIS_KTHREAD- The proccess is a KERNEL thread </td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>R: MIS_REPLICATED- The ep is LOCAL but it is replicated on other node</td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>k: MIS_KILLED- The process has been killed </td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>W: MIS_WOKENUP- The process has been signaled	</td></tr>\n");
	(void)strcat(page_ptr,"<tr align=\"left\"><td>U: MIS_UNIKERNEL- The process is running inside a local UNIKERNEL	</td></tr>\n");
  	(void)strcat(page_ptr,"</tbody></table>\n");

 
  	MUKDEBUG("%s\n",page_ptr);	
	MUKDEBUG("strlen(page_ptr)=%d\n", strlen(page_ptr));	

}

#ifdef ENABLE_DMP_SYS_STATS 		

/*===========================================================================*
 *				wdmp_sys_stats  					    				     *
 *===========================================================================*/
void wdmp_sys_stats(message *m_ptr)
{
	int i;
	char *page_ptr;
	muk_proc_t	*task_ptr;
    proc_usr_t *proc_ptr;

	MUKDEBUG("\n");
	page_ptr = m_ptr->m1_p1;
	
	(void)strcat(page_ptr,"<table>\n");
	(void)strcat(page_ptr,"<thead>\n");
	(void)strcat(page_ptr,"<tr>\n");
	(void)strcat(page_ptr,"<th>DC</th>\n");
	(void)strcat(page_ptr,"<th>pnr</th>\n");
	(void)strcat(page_ptr,"<th>-endp</th>\n");
	(void)strcat(page_ptr,"<th>-lpid</th>\n");
	(void)strcat(page_ptr,"<th>-vpid</th>\n");
	(void)strcat(page_ptr,"<th>nd</th>\n");
	(void)strcat(page_ptr,"<th>--lsnt--</th>\n");
	(void)strcat(page_ptr,"<th>--rsnt--</th>\n");
	(void)strcat(page_ptr,"<th>--lcopy-</th>\n");
	(void)strcat(page_ptr,"<th>--rcopy-</th>\n");
	(void)strcat(page_ptr,"<th>name</th>\n");
	(void)strcat(page_ptr,"</tr>\n");
	(void)strcat(page_ptr,"</thead>\n");
	(void)strcat(page_ptr,"<tbody>\n");
	
//	assert(kproc_map != NULL);
	
	for(i = (-dc_ptr->dc_nr_tasks) ; i < dc_ptr->dc_nr_procs; i++) {
		task_ptr = (Task *) get_task(i);
		if( task_ptr == NULL) continue;
		if (TEST_BIT(task_ptr->p_proc->p_rts_flags, BIT_SLOT_FREE)) {
			continue;
		}
		(void)strcat(page_ptr,"<tr>\n");
		proc_ptr = task_ptr->p_proc;
		sprintf(is_buffer, "<td>%2d</td> <td>%3d</td> <td>%5d</td> <td>%5ld</td> <td>%5ld</td> "
			"<td>%2d</td> <td>%8ld</td> <td>%8ld</td> <td>%8ld</td> <td>%8ld</td> <td>%-15.15s</td>\n",
				proc_ptr->p_dcid,
				proc_ptr->p_nr,
				proc_ptr->p_endpoint,
				proc_ptr->p_lpid,
				proc_ptr->p_vpid,
				proc_ptr->p_nodeid,
				proc_ptr->p_lclsent,
				proc_ptr->p_rmtsent,
				proc_ptr->p_lclcopy,
				proc_ptr->p_rmtcopy,
				proc_ptr->p_name);	
		(void)strcat(page_ptr,is_buffer);
		(void)strcat(page_ptr,"</tr>\n");
	}				
  (void)strcat(page_ptr,"</tbody></table>\n");
  
  	MUKDEBUG("%s\n",page_ptr);	
	MUKDEBUG("strlen(page_ptr)=%d\n", strlen(page_ptr));	
}
#endif // ENABLE_DMP_SYS_STATS 		


#ifdef ENABLE_WDMP_SYS_PRIV 		
/*===========================================================================*
 *				wdmp_sys_priv					    				     *
 *===========================================================================*/
void wdmp_sys_priv(void)
{
	
	fprintf(dump_fd, "\n");

}
#endif //ENABLE_WDMP_SYS_PRIV 		

/*===========================================================================*
 *				bm2ascii		 			   				     *
 *===========================================================================*/
void bm2ascii(char *buf, unsigned long int bitmap)
{
	int i;
	unsigned long int mask;

	mask = 0x80000000; 
	for( i = 0; i < BITMAP_32BITS ; i++) {
		*buf++ = (bitmap & mask)?'X':'_';
		mask =  (mask >> 1);		
	}
	*buf = '\0';
}

/*===========================================================================*
 *				p_rts_flags_str		 			   				     *
 *===========================================================================*/
p_rts_flags_str(int flags, char *str )
{
	str[0] = (flags & NO_MAP) 		? 'A' : '-';
	str[1] = (flags & SENDING)  	? 'S' : '-';
	str[2] = (flags & RECEIVING)    ? 'R' : '-';
	str[3] = (flags & SIGNALED)    	? 'I' : '-';
	str[4] = (flags & SIG_PENDING)  ? 'P' : '-';
	str[5] = (flags & P_STOP)    	? 'T' : '-';
	str[6] = (flags & NO_PRIV)    	? 'V' : '-';
	str[7] = (flags & NO_PRIORITY)  ? 'Y' : '-';

	str[8] = (flags & NO_ENDPOINT) 	? 'e' : '-';
	str[9] = (flags & ONCOPY)  		? 'c' : '-';
	str[10] = (flags & MIGRATING)   ? 'm' : '-';
	str[11] = (flags & REMOTE)    	? 'r' : '-';
	str[12] = (flags & RMTOPER)    	? 'o' : '-';
	str[13] = (flags & WAITMIGR)    ? 'g' : '-';
	str[14] = (flags & WAITUNBIND)  ? 'u' : '-';
	str[15] = (flags & WAITBIND)    ? 'b' : '-';
	
	str[16] = '\0';

	return str;
}


/*===========================================================================*
 *				p_misc_flags_str		 			   				     *
 *===========================================================================*/
void p_misc_flags_str(int flags, char *str )
{
	str[0] = (flags & MIS_PROXY) 		? 'X' : '-';
	str[1] = (flags & MIS_CONNECTED)  	? 'C' : '-';
	str[2] = (flags & MIS_NOTIFY)   	? 'N' : '-';
	str[3] = (flags & MIS_NEEDMIG)    	? 'M' : '-';
	str[4] = (flags & MIS_RMTBACKUP)  	? 'B' : '-';
	str[5] = (flags & MIS_GRPLEADER)    ? 'L' : '-';
	str[6] = (flags & MIS_KTHREAD)    	? 'K' : '-';
	str[7] = (flags & MIS_REPLICATED)  	? 'R' : '-';
	str[8] = (flags & MIS_KILLED) 		? 'k' : '-';
	str[9] = (flags & MIS_WOKENUP)  	? 'W' : '-';
	str[10] = (flags & MIS_UNIKERNEL)  	? 'U' : '-';
	str[11] = '\0';

	return str;
}

/*===========================================================================*
 *				p_endpoint_str		 			   				     *
 *===========================================================================*/
void p_endpoint_str(int endpoint, char *str)
{
	muk_proc_t	*task_ptr;
	int i;
	
	if( endpoint == NONE) {
		strcpy(str,"-NONE");
	}else if (endpoint == ANY) {
		strcpy(str,"-ANY-");		
	}else {
//		sprintf(str,"%5d",endpoint);
		for(i = (-dc_ptr->dc_nr_tasks) ; i < dc_ptr->dc_nr_procs; i++) {
			task_ptr = (Task *) get_task(i);
			if( task_ptr == NULL) continue;
			if (TEST_BIT(task_ptr->p_proc->p_rts_flags, BIT_SLOT_FREE)) {
				continue;
			}
			if( task_ptr->p_proc->p_endpoint == endpoint){
				strncpy(str,task_ptr->name,5);
				break;
			}
		}
	}
	return str;
}

#endif // USE_SYSDUMP 
