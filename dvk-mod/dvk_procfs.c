/****************************************************************/
/****************************************************************/
/*			MINIX OVER LINUX PROC FS ROUTINES 			*/
/****************************************************************/

#include "dvk_mod.h"

#define MAX_LINE_WIDTH 130

/*--------------------------------------------------------------*/
/*			/proc/dvs/nodes 				*/
/*--------------------------------------------------------------*/
//int nodes_read( char *page, char **start, off_t off, int count, int *eof, void *data )
ssize_t nodes_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) 
{
	int i, len, rcode;
	cluster_node_t *n_ptr;
	char bmbuf[BITMAP_32BITS+1];
	char *page;
	static int last_node = 0;
		
	DVKDEBUG(INTERNAL,"last_node=%d count=%d ppos=%ld\n", last_node, count, *ppos);
	
	if( count <= 0) return(0);
	
	if( last_node == (-1)){
		last_node = 0;
		return(0);  // => EOF 
	}
	len = 0;
	
	page = kmalloc(PAGE_SIZE, GFP_KERNEL);

	if( last_node == 0)
		len = sprintf(page, "ID Flags Proxies -pxsent- -pxrcvd- 10987654321098765432109876543210 Name\n");
	
	for (i = last_node; i < dvs.d_nr_nodes; i++) {
		n_ptr = &node[i];
		RLOCK_NODE(n_ptr);
		if( n_ptr->n_usr.n_flags != NODE_FREE) {
			bm2ascii(bmbuf, n_ptr->n_usr.n_dcs);      
			len += sprintf(page+len, "%2d %5lX %7d %8ld %8ld %32s %-16.16s\n",
				n_ptr->n_usr.n_nodeid,
				n_ptr->n_usr.n_flags,
				n_ptr->n_usr.n_proxies,
				n_ptr->n_usr.n_pxsent,
				n_ptr->n_usr.n_pxrcvd,
				bmbuf,
				n_ptr->n_usr.n_name);
		}
		RUNLOCK_NODE(n_ptr);
		if(len > (count - MAX_LINE_WIDTH) )
			break;
		if(len > (PAGE_SIZE - MAX_LINE_WIDTH) )
			break;
	}
	
	if( i < dvs.d_nr_nodes) 
		last_node = i;
	else 
		last_node = (-1);

	rcode = copy_to_user(ubuf,page,len);
	kfree(page);
	if( rcode ) ERROR_RETURN(EDVSFAULT);
	return len;
}

/*--------------------------------------------------------------*/
/*			/proc/dvs/info 			*/
/*--------------------------------------------------------------*/
// int info_read( char *page, char **start, off_t off, int count, int *eof, void *data )
ssize_t info_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) 
{
	int i, len, rcode;
	char *page;
	static int last_line = 0;
	dvs_usr_t *d_ptr;

	
	DVKDEBUG(INTERNAL,"last_line=%d count=%d ppos=%ld\n", last_line, count, *ppos);

	if( count <= 0) return(0);
	if( last_line == (-1)){
		last_line = 0;
		return(0);  // => EOF 
	}
	
	len = 0;
	
	page = kmalloc(PAGE_SIZE, GFP_KERNEL);
  	len += sprintf(page+len, "nodeid=%d\n", atomic_read(&local_nodeid));
  	len += sprintf(page+len, "nr_dcs=%d\n", dvs.d_nr_dcs);
  	len += sprintf(page+len, "nr_nodes=%d\n", dvs.d_nr_nodes);
  	len += sprintf(page+len, "max_nr_procs=%d\n", dvs.d_nr_procs);
  	len += sprintf(page+len, "max_nr_tasks=%d\n", dvs.d_nr_tasks);
  	len += sprintf(page+len, "max_sys_procs=%d\n", dvs.d_nr_sysprocs);
  	len += sprintf(page+len, "max_copy_buf=%d\n", dvs.d_max_copybuf);
  	len += sprintf(page+len, "max_copy_len=%d\n", dvs.d_max_copylen);
  	len += sprintf(page+len, "dbglvl=%lX\n", dvs.d_dbglvl);
  	len += sprintf(page+len, "version=%d.%d\n", dvs.d_version, dvs.d_subver);
  	len += sprintf(page+len, "sizeof(proc)=%d\n", sizeof(struct proc));
  	len += sprintf(page+len, "sizeof(proc) aligned=%d\n", sizeof_proc_aligned);
  	len += sprintf(page+len, "sizeof(dc)=%d\n", sizeof(dc_desc_t));
  	len += sprintf(page+len, "sizeof(node)=%d\n", sizeof(cluster_node_t));

	d_ptr = &dvs;
	DVKDEBUG(DBGPARAMS,DVS_USR_FORMAT, DVS_USR_FIELDS(d_ptr));
	DVKDEBUG(DBGPARAMS,DVS_MAX_FORMAT, DVS_MAX_FIELDS(d_ptr));
	DVKDEBUG(DBGPARAMS,DVS_VER_FORMAT, DVS_VER_FIELDS(d_ptr));

	rcode = copy_to_user(ubuf,page,len);
	kfree(page);
	
	last_line = (-1);
	if( rcode ) ERROR_RETURN(EDVSFAULT);
	return len;
}

/*--------------------------------------------------------------*/
/*			/proc/dvs/proxies/info			*/
/*--------------------------------------------------------------*/
//int proxies_info_read( char *page, char **start, off_t off, int count, int *eof, void *data )
ssize_t proxies_info_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) 
{
	int i, len, rcode;
	struct proc *proc_ptr;
	proxies_t *px_ptr;
	char *page;
	static int last_proc = 0;
	char bmbuf[BITMAP_32BITS+1];

	DVKDEBUG(INTERNAL,"last_proc=%d count=%d ppos=%ld\n", last_proc, count, *ppos);
	if( count <= 0) return(0);
	if( last_proc == (-1)){
		last_proc = 0;
		return(0);  // => EOF 
	}
	len = 0;
	page = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if( last_proc == 0)
		len = sprintf(page, "Proxies Flags Sender Receiver --Proxies_Name- 10987654321098765432109876543210 \n");
	
	for (i = 0; i < dvs.d_nr_nodes; i++) {
		px_ptr = &proxies[i];
		RLOCK_PROXY(px_ptr);
		if( px_ptr->px_usr.px_flags != PROXIES_FREE) {
			proc_ptr = &px_ptr->px_sproxy;
			bm2ascii(bmbuf, proc_ptr->p_usr.p_nodemap);     
			DVKDEBUG(INTERNAL,"flags=%lX pxnr=%d map=%lX\n",
				px_ptr->px_usr.px_flags, i, proc_ptr->p_usr.p_nodemap);
 			len += sprintf(page+len, "%7d %5lX %6d %8d %15s %s\n",
				px_ptr->px_usr.px_id,
				px_ptr->px_usr.px_flags,
				px_ptr->px_sproxy.p_usr.p_lpid,
				px_ptr->px_rproxy.p_usr.p_lpid,
				px_ptr->px_usr.px_name,
				bmbuf);
		}
		RUNLOCK_PROXY(px_ptr);
		if(len > (count - (MAX_LINE_WIDTH)) )
			break;
		if(len > (PAGE_SIZE - (MAX_LINE_WIDTH)))
			break;
	}
	
	if( i < dvs.d_nr_nodes) 
		last_proc = i;
	else 
		last_proc = (-1);

	rcode = copy_to_user(ubuf,page,len);
	kfree(page);
	if( rcode ) ERROR_RETURN(EDVSFAULT);
	return len;
}

/*--------------------------------------------------------------*/
/*			/proc/dvs/proxies/procs			*/
/*--------------------------------------------------------------*/
// int proxies_procs_read( char *page, char **start, off_t off, int count, int *eof, void *data )
ssize_t proxies_procs_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) 
{
	int i, len, rcode;
	char *page;
	static int last_proc = 0;
	proxies_t *px_ptr;
	struct proc *sproc_ptr,*rproc_ptr;

	DVKDEBUG(INTERNAL,"last_proc=%d count=%d ppos=%ld\n", last_proc, count, *ppos);
	if( count <= 0) return(0);
	if( last_proc == (-1)){
		last_proc = 0;
		return(0);  // => EOF 
	}
	len = 0;
	page = kmalloc(PAGE_SIZE, GFP_KERNEL);

	if( last_proc == 0)
		len = sprintf(page, "ID Type -lpid- -flag- -misc- -pxsent- -pxrcvd- -getf- -sendt -wmig- name\n");	

	for (i = 0; i < dvs.d_nr_nodes; i++) {
		px_ptr = &proxies[i];
		RLOCK_PROXY(px_ptr);
		if( px_ptr->px_usr.px_flags == PROXIES_FREE) {
			RUNLOCK_PROXY(px_ptr);
			continue;
		}

		sproc_ptr = &proxies[i].px_sproxy;
		RLOCK_PROC(sproc_ptr);
		len += sprintf(page+len, "%2d %4s %6d %6lX %6lX %8ld %8ld %6d %6d %6d %-15.15s\n",
					i,
					"send",
					sproc_ptr->p_usr.p_lpid,
					sproc_ptr->p_usr.p_rts_flags,
					sproc_ptr->p_usr.p_misc_flags,
					sproc_ptr->p_usr.p_rmtsent,		
					sproc_ptr->p_usr.p_lclsent,
					sproc_ptr->p_usr.p_getfrom,
					sproc_ptr->p_usr.p_sendto,
					sproc_ptr->p_usr.p_waitmigr,
					sproc_ptr->p_usr.p_name);
		RUNLOCK_PROC(sproc_ptr);

		rproc_ptr = &proxies[i].px_rproxy;
		RLOCK_PROC(rproc_ptr);
		len += sprintf(page+len, "%2d %4s %6d %6lX %6lX %8ld %8ld %6d %6d %6d %-15.15s\n",
					i,
					"recv",
					rproc_ptr->p_usr.p_lpid,
					rproc_ptr->p_usr.p_rts_flags,
					rproc_ptr->p_usr.p_misc_flags,
					rproc_ptr->p_usr.p_rmtsent,		
					rproc_ptr->p_usr.p_lclsent,
					rproc_ptr->p_usr.p_getfrom,
					rproc_ptr->p_usr.p_sendto,
					rproc_ptr->p_usr.p_waitmigr,
					rproc_ptr->p_usr.p_name);
		RUNLOCK_PROC(rproc_ptr);
		
		RUNLOCK_PROXY(px_ptr);
		if(len > (count - (2*MAX_LINE_WIDTH)) )
			break;
		if(len > (PAGE_SIZE - (2*MAX_LINE_WIDTH)))
			break;
	}

	if( i < dvs.d_nr_nodes) 
		last_proc = i;
	else 
		last_proc = (-1);

	rcode = copy_to_user(ubuf,page,len);
	kfree(page);
	if( rcode ) ERROR_RETURN(EDVSFAULT);
	return len;
}

/*--------------------------------------------------------------*/
/*			/proc/dvs/DCx/info 						*/
/*--------------------------------------------------------------*/
// int dc_info_read( char *page, char **start, off_t off, int count, int *eof, void *data )
ssize_t dc_info_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) 
{
	int dcid, len, rcode;
	char *page;
	int *ptr;
	dc_desc_t *dc_ptr;
	char bmbuf[BITMAP_32BITS+1];
	static int last_line = 0;
	int *data_ptr;
	
	DVKDEBUG(INTERNAL,"last_line=%d count=%d ppos=%ld\n", last_line, count, *ppos);

	if( count <= 0) return(0);
	if( last_line == (-1)){
		last_line = 0;
		return(0);  // => EOF 
	}

	// get de DCID
	data_ptr = (int *) PDE_DATA(file_inode(file));
	if(!(data_ptr)){
		DVKDEBUG(GENERIC,"Null data");
		return 0;
	}
	dcid = *data_ptr;
	DVKDEBUG(DBGPARAMS,"dcid=%d\n", dcid);
	dc_ptr = &dc[dcid];
	
	len = 0;
	page = kmalloc(PAGE_SIZE, GFP_KERNEL);
	RLOCK_DC(dc_ptr);
	bm2ascii(bmbuf, dc_ptr->dc_usr.dc_nodes);      
	if(dc_ptr->dc_usr.dc_flags != DC_FREE) {
		len += sprintf(page, "dcid=%d\nflags=%X\nnr_procs=%d\nnr_tasks=%d\n"
		"nr_sysprocs=%d\nnr_nodes=%d\ndc_nodes=%lX\ndc_pid=%ld\n"
		"warn2proc=%d\nwarnmsg=%d\ndc_name=%s\n",
			dc_ptr->dc_usr.dc_dcid,
			dc_ptr->dc_usr.dc_flags,
			dc_ptr->dc_usr.dc_nr_procs,
			dc_ptr->dc_usr.dc_nr_tasks,
			dc_ptr->dc_usr.dc_nr_sysprocs,
			dc_ptr->dc_usr.dc_nr_nodes,
			dc_ptr->dc_usr.dc_nodes,
			dc_ptr->dc_usr.dc_pid,		
			dc_ptr->dc_usr.dc_warn2proc,
			dc_ptr->dc_usr.dc_warnmsg,
			dc_ptr->dc_usr.dc_name);
		len += sprintf(page+len,"nodes 33222222222211111111110000000000\n"); 
		len += sprintf(page+len,"      10987654321098765432109876543210\n");
		len += sprintf(page+len,"      %s\n", bmbuf);
		len += sprintf(page+len,"cpumask=%*pb \n",cpumask_pr_args(&dc_ptr->dc_usr.dc_cpumask));
//		len += scnprintf(buf, PAGE_SIZE - 1, "%*pbl", cpumask_pr_args(&pmu->cpus));
//		len = snprintf(buf, PAGE_SIZE, "%*pb\n",  nr_cpu_ids, cpumask_bits(cpumask));
	}
	RUNLOCK_DC(dc_ptr);	

	rcode = copy_to_user(ubuf,page,len);
	kfree(page);
	
	last_line = (-1);
	if( rcode ) ERROR_RETURN(EDVSFAULT);
	return len;
}

/*--------------------------------------------------------------*/
/*			/proc/dvs/DCx/procs			*/
/*--------------------------------------------------------------*/
//int dc_procs_read( char *page, char **start, off_t off, int count, int *eof, void *data )
ssize_t dc_procs_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) 
{
	int dcid;
	int i, len, rcode;
	char *page;
	static int last_proc = 0;
	int *data_ptr;
	struct proc *proc_ptr;
	dc_desc_t *dc_ptr;
		
	DVKDEBUG(INTERNAL,"last_proc=%d count=%d ppos=%ld\n", last_proc, count, *ppos);
	if( count <= 0) return(0);
	if( last_proc == (-1)){
		last_proc = 0;
		return(0);  // => EOF 
	}
	len = 0;
	
	// get de DCID
	data_ptr = (int *) PDE_DATA(file_inode(file));
	if(!(data_ptr)){
		DVKDEBUG(GENERIC,"Null data");
		return 0;
	}
	dcid = *data_ptr;
	DVKDEBUG(DBGPARAMS,"dcid=%d\n", dcid);
	dc_ptr = &dc[dcid];
	
	len = 0;
	page = kmalloc(PAGE_SIZE, GFP_KERNEL);
	len = 0;
	if( last_proc == 0)
		len = sprintf(page, "DC pnr -endp -lpid/vpid- nd flag misc -getf -sndt -wmig -prxy name\n");	

	RLOCK_DC(dc_ptr);
	if( dc_ptr->dc_usr.dc_flags != DC_FREE)  {
		for(i = last_proc; i < (dc_ptr->dc_usr.dc_nr_tasks + dc_ptr->dc_usr.dc_nr_procs); i++) {
			proc_ptr = DC_PROC(dc_ptr,i);
			RLOCK_PROC(proc_ptr);
			if (test_bit(BIT_SLOT_FREE, &proc_ptr->p_usr.p_rts_flags)) {
				RUNLOCK_PROC(proc_ptr);
				continue;
			}
			len += sprintf(page+len, "%2d %3d %5d %5ld/%-5ld %2d %4lX %4lX %5d %5d %5d %5d %-15.15s\n",
					proc_ptr->p_usr.p_dcid,
					proc_ptr->p_usr.p_nr,
					proc_ptr->p_usr.p_endpoint,
					proc_ptr->p_usr.p_lpid,
					proc_ptr->p_usr.p_vpid,
					proc_ptr->p_usr.p_nodeid,
					proc_ptr->p_usr.p_rts_flags,
					proc_ptr->p_usr.p_misc_flags,
					proc_ptr->p_usr.p_getfrom,
					proc_ptr->p_usr.p_sendto,
					proc_ptr->p_usr.p_waitmigr,
					proc_ptr->p_usr.p_proxy,
					proc_ptr->p_usr.p_name);

			RUNLOCK_PROC(proc_ptr);	
			if(len > (count - MAX_LINE_WIDTH) )
				break;
			if(len > (PAGE_SIZE - MAX_LINE_WIDTH) )
				break;					
		} 
	}
	
	if( i < (dc_ptr->dc_usr.dc_nr_tasks + dc_ptr->dc_usr.dc_nr_procs)) 
		last_proc = i;
	else 
		last_proc = (-1);
	RUNLOCK_DC(dc_ptr);

	rcode = copy_to_user(ubuf,page,len);
	kfree(page);
	if( rcode ) ERROR_RETURN(EDVSFAULT);
	return len;
}

/*--------------------------------------------------------------*/
/*			/proc/dvs/DCx/stats			*/
/*--------------------------------------------------------------*/
//int dc_stats_read( char *page, char **start, off_t off, int count, int *eof, void *data )
ssize_t dc_stats_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) 
{
	int dcid;
	int i, len, rcode;
	char *page;
	static int last_proc = 0;
	int *data_ptr;
	struct proc *proc_ptr;
	dc_desc_t *dc_ptr;
		
	DVKDEBUG(INTERNAL,"last_proc=%d count=%d ppos=%ld\n", last_proc, count, *ppos);
	if( count <= 0) return(0);
	if( last_proc == (-1)){
		last_proc = 0;
		return(0);  // => EOF 
	}
	len = 0;
	
	// get de DCID
	data_ptr = (int *) PDE_DATA(file_inode(file));
	if(!(data_ptr)){
		DVKDEBUG(GENERIC,"Null data");
		return 0;
	}
	dcid = *data_ptr;
	DVKDEBUG(DBGPARAMS,"dcid=%d\n", dcid);
	dc_ptr = &dc[dcid];
	
	len = 0;
	page = kmalloc(PAGE_SIZE, GFP_KERNEL);
	len = 0;
	if( last_proc == 0)
		len = sprintf(page, "DC pnr -endp -lpid/vpid- nd --lsnt-- --rsnt-- -lcopy-- -rcopy-- name\n");	

	RLOCK_DC(dc_ptr);
	if( dc_ptr->dc_usr.dc_flags != DC_FREE)  {
		for(i = last_proc; i < (dc_ptr->dc_usr.dc_nr_tasks + dc_ptr->dc_usr.dc_nr_procs); i++) {
			proc_ptr = DC_PROC(dc_ptr,i);
			RLOCK_PROC(proc_ptr);
			if (test_bit(BIT_SLOT_FREE, &proc_ptr->p_usr.p_rts_flags)) {
				RUNLOCK_PROC(proc_ptr);
				continue;
			}
			len += sprintf(page+len, "%2d %3d %5d %5d/%5d %2d %8ld %8ld %8ld %8ld %-15.15s\n",
					proc_ptr->p_usr.p_dcid,
					proc_ptr->p_usr.p_nr,
					proc_ptr->p_usr.p_endpoint,
					proc_ptr->p_usr.p_lpid,
					proc_ptr->p_usr.p_vpid,
					proc_ptr->p_usr.p_nodeid,
					proc_ptr->p_usr.p_lclsent,
					proc_ptr->p_usr.p_rmtsent,
					proc_ptr->p_usr.p_lclcopy,
					proc_ptr->p_usr.p_rmtcopy,
					proc_ptr->p_usr.p_name);
			RUNLOCK_PROC(proc_ptr);	
			if(len > (count - MAX_LINE_WIDTH) )
				break;
			if(len > (PAGE_SIZE - MAX_LINE_WIDTH) )
				break;					
		} 
	}
	
	if( i < (dc_ptr->dc_usr.dc_nr_tasks + dc_ptr->dc_usr.dc_nr_procs)) 
		last_proc = i;
	else 
		last_proc = (-1);
	RUNLOCK_DC(dc_ptr);

	rcode = copy_to_user(ubuf,page,len);
	kfree(page);
	if( rcode ) ERROR_RETURN(EDVSFAULT);

    return len;
}



//################################################################
//################################################################
//################################################################
//################################################################
//################################################################
//################################################################
//################################################################
#ifdef 	NULL_CODE

int node_info_read( char *page, char **start, off_t off, int count, int *eof, void *data )
{
	int len=0, nodeid;
	int *ptr;
	cluster_node_t *n_ptr;
	char bmbuf[BITMAP_32BITS+1];

	ptr  = (int *) data;
	nodeid = *ptr;
	DVKDEBUG(DBGPARAMS,"nodeid=%d\n", nodeid);
	
	len = sprintf(page, "Node Flags Proxies 10987654321098765432109876543210 Name\n");
	n_ptr = &node[nodeid];
	RLOCK_NODE(n_ptr);
	bm2ascii(bmbuf, n_ptr->n_usr.n_dcs);      
	len += sprintf(page+len, "%4d %5lX %7d %32s %-16.16s\n",
		n_ptr->n_usr.n_nodeid,
		n_ptr->n_usr.n_flags,
		n_ptr->n_usr.n_proxies,
		bmbuf,
		n_ptr->n_usr.n_name);
	RUNLOCK_NODE(n_ptr);

   return len;
}

int node_stats_read( char *page, char **start, off_t off, int count, int *eof, void *data )
{

	int len=0, nodeid;
	cluster_node_t *n_ptr;
	int *ptr;

	ptr  = (int *) data;
	nodeid = *ptr;
	DVKDEBUG(DBGPARAMS,"nodeid=%d\n", nodeid);
	n_ptr = &node[nodeid];
	RLOCK_NODE(n_ptr);
	len = sprintf(page, "Node Flags\n");
	len += sprintf(page+len, "%-8.8s %lX \n", 
		n_ptr->n_usr.n_name, 
		n_ptr->n_usr.n_flags);
	RUNLOCK_NODE(n_ptr);

   return len;
}
#endif // 	NULL_CODE



