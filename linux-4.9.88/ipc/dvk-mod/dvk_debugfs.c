/****************************************************************/
/*	MINIX OVER LINUX DEBUGFS ROUTINES 				*/
/****************************************************************/

#include "dvk_mod.h"

void mmap_open(struct vm_area_struct *vma);
void mmap_close(struct vm_area_struct *vma);
static int mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf);

struct vm_operations_struct mmap_vm_ops = {
	.open =     mmap_open,
	.close =    mmap_close,
	.fault =    mmap_fault,
	//.nopage =   mmap_nopage,				//--changed
};

int proc_dbg_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct task_struct *task_ptr;
	struct proc *caller_ptr;
	int caller_pid;
	int ret, dcid;
	struct dentry    *parent_dir;    /* dentry object of parent */
	static	char dc_name[8];
	
	DVKDEBUG(INTERNAL,"\n");
	
	ret = check_caller(&task_ptr, &caller_ptr, &caller_pid);
	if(ret) return(ret);
	
	if( DVS_NOT_INIT() )
		ERROR_RETURN(EDVSDVSINIT);

 	RLOCK_PROC(caller_ptr);
	dcid = caller_ptr->p_usr.p_dcid;
 	RUNLOCK_PROC(caller_ptr);
	DVKDEBUG(INTERNAL,"caller's dcid=%d\n", dcid);
	if( dcid < 0 || dcid >= dvs_ptr->d_nr_dcs) 			
		ERROR_RETURN(EDVSBADDCID);
	
	sprintf(dc_name, "DC%d", dcid);
	parent_dir = filp->f_path.dentry->d_parent;
	DVKDEBUG(INTERNAL,"d_name=[%s] dc_name=[%s]\n",parent_dir->d_name.name, dc_name);
	if(strcmp(parent_dir->d_name.name, dc_name) != 0){
		ERROR_RETURN(EDVSBADDCID);
	}
		
	vma->vm_ops = &mmap_vm_ops;
	vma->vm_flags |= VM_IO;
	DVKDEBUG(INTERNAL,"vm_flags=%X\n", (unsigned int) vma->vm_flags);
	/* assign the file private data to the dc private data */
	vma->vm_private_data = filp->private_data;
	mmap_open(vma);
	return 0;
}

int proc_dbg_open(struct inode *inode, struct file *filp)
{
	struct task_struct *task_ptr;
	struct proc *caller_ptr;
	int caller_pid;
	int ret, dcid;
	dc_desc_t *dc_ptr;
	struct dentry    *parent_dir;    /* dentry object of parent */
	static	char dc_name[8];
	
	DVKDEBUG(INTERNAL,"\n");
	
	ret = check_caller(&task_ptr, &caller_ptr, &caller_pid);
	if(ret) return(ret);
	
	if( DVS_NOT_INIT() )
		ERROR_RETURN(EDVSDVSINIT);

 	RLOCK_PROC(caller_ptr);
	dcid = caller_ptr->p_usr.p_dcid;
	DVKDEBUG(INTERNAL,"caller's dcid=%d\n", dcid);
 	RUNLOCK_PROC(caller_ptr);
	sprintf(dc_name, "DC%d", dcid);
	parent_dir = filp->f_path.dentry->d_parent;
	DVKDEBUG(INTERNAL,"d_name=[%s] dc_name=[%s]\n",parent_dir->d_name.name, dc_name);
	if(strcmp(parent_dir->d_name.name, dc_name) != 0){
		ERROR_RETURN(EDVSBADDCID);
	}
	
	DVKDEBUG(INTERNAL,"caller's dcid=%d\n", dcid);
	if( dcid < 0 || dcid >= dvs_ptr->d_nr_dcs) 			
		ERROR_RETURN(EDVSBADDCID);
	dc_ptr 	= &dc[dcid];
	WLOCK_DC(dc_ptr);
	filp->private_data = dc_ptr->dc_proc;
	dc_ptr->dc_ref_dgb++;  /* increment open reference count */
	WUNLOCK_DC(dc_ptr);

#ifdef ANULADO
	end_addr = (char *)dc_ptr->dc_proc;
	end_addr += ((dc_ptr->dc_usr.dc_nr_tasks + dc_ptr->dc_usr.dc_nr_procs) * sizeof_proc_aligned);
	for( data = dc_ptr->dc_proc; data < end_addr; data += PAGE_SIZE){
		/* get the page */
		page = virt_to_page(data);
		/* increment the reference count of this page */
		get_page(page);
	}
#endif /* ANULADO */
	
	return 0;
}

int proc_dbg_close(struct inode *inode, struct file *filp)
{
	struct task_struct *task_ptr;
	struct proc *caller_ptr;
	int caller_pid;
	int ret, dcid;
	int proctab_size, order;
	dc_desc_t *dc_ptr;
	struct dentry    *parent_dir;    /* dentry object of parent */
	static	char dc_name[8];
	
	DVKDEBUG(INTERNAL,"\n");
	
	ret = check_caller(&task_ptr, &caller_ptr, &caller_pid);
	if(ret) return(ret);
	
	if( DVS_NOT_INIT() )
		ERROR_RETURN(EDVSDVSINIT);

 	RLOCK_PROC(caller_ptr);
	dcid = caller_ptr->p_usr.p_dcid;
 	RUNLOCK_PROC(caller_ptr);
	DVKDEBUG(INTERNAL,"caller's dcid=%d\n", dcid);
	if( dcid < 0 || dcid >= dvs_ptr->d_nr_dcs) 			
		ERROR_RETURN(EDVSBADDCID);
	dc_ptr 	= &dc[dcid];


	sprintf(dc_name, "DC%d", dcid);
	parent_dir = filp->f_path.dentry->d_parent;
	DVKDEBUG(INTERNAL,"d_name=[%s] dc_name=[%s]\n",parent_dir->d_name.name, dc_name);
	if(strcmp(parent_dir->d_name.name, dc_name) != 0){
		ERROR_RETURN(EDVSBADDCID);
	}
	
	WLOCK_DC(dc_ptr);
	dc_ptr->dc_ref_dgb--;  /* decrement open reference count */
	if( dc_ptr->dc_ref_dgb) {
		WUNLOCK_DC(dc_ptr);
		return (0);
	}
	proctab_size = sizeof_proc_aligned  * (dc_ptr->dc_usr.dc_nr_tasks + dc_ptr->dc_usr.dc_nr_procs);
	for( order = 0; (PAGE_SIZE << order) < proctab_size; order++);
	free_pages((long unsigned int)dc_ptr->dc_proc,  order);
	filp->private_data = NULL;
	WUNLOCK_DC(dc_ptr);

	return 0;
}

void mmap_open(struct vm_area_struct *vma)
{
	DVKDEBUG(INTERNAL,"\n");
}

void mmap_close(struct vm_area_struct *vma)
{
	DVKDEBUG(INTERNAL,"\n");
}

/* nopage is called the first time a memory area is accessed which is not in memory,
 * it does the actual mapping between kernel and user space memory
 */
//struct page *mmap_nopage(struct vm_area_struct *vma, unsigned long address, int *type)	--changed
static int mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page;
	char *data;

	DVKDEBUG(INTERNAL,"pgoff=%ld \n", vmf->pgoff);
	
	data = (vmf->pgoff * PAGE_SIZE) + vma->vm_private_data;
	/* get the page */
	page = virt_to_page(data);
	
	/* increment the reference count of this page */
	get_page(page);
	vmf->page = page;					//--changed
	
		
	return 0;
}


