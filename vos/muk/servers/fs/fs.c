/* This file contains the main program of the File System.  It consists of
 * a loop that gets messages requesting work, carries out the work, and sends
 * replies.
 *
 * The entry points into this file are:
 *   main:	main program of the File System
 *   fs_reply:	send a fs_reply to a process after the requested work is done
 *
 */
  
#define DVK_GLOBAL_HERE
#include "fs.h"

#undef DVK_GLOBAL_HERE
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"


#define WAIT4BIND_MS 1000
#define nil ((void*)0) /*For configuration reader*/

void fs_init(char *cfg_file);
void fs_get_work(void);
struct inode *init_root(void);
void buf_pool(void);
int get_root_major(void); 

// void print_usage(char *argv[]) {
//   printf( "Usage: %s --dcid vmIdNumber --cfgFile cfgFilename [--buffSize=sizeBufferInBytes]\n", argv[0] );
// }

void print_usage(char* errmsg, ...) {
  if(errmsg) {
      printf("ERROR: %s\n", errmsg);  
    }
  printf( "Usage: fs cfgFilename \n");
}


/*===========================================================================*
 *				main					     *
 *===========================================================================*/
 int main_fs ( int argc, char *argv[] )
 {
/* This is the main program of the file system.  The main loop consists of
 * three major activities: getting new work, processing the work, and sending
 * the fs_reply.  This loop never terminates as long as the file system runs.
 */
	int rcode;
	
	if (argc != 2) {
		printf( "Usage: %s  <config_file> \n", argv[0]);
		rcode = EDVSINVAL;
		ERROR_PT_EXIT(rcode);
	}
  	
	fs_init(argv[1]);

	// SYNCHRONIZE WITH MUK
	MTX_LOCK(muk_mutex);
	COND_SIGNAL(muk_cond);
	COND_WAIT(fs_cond, muk_mutex);
	MTX_UNLOCK(muk_mutex);
	
	MUKDEBUG("FS(%d) main loop that gets work\n", dcu.dc_dcid);

	while (TRUE) {
		fs_get_work();			/* sets who and fs_call_nr */

		fs_proc = &fs_proc_table[fs_who_p];	/* pointer to proc table struct */
	// MUKDEBUG(FS_PROC_FORMAT, FS_PROC_FIELDS(fs_proc));
	// MUKDEBUG("Proceso fs_who_p %d\n",fs_who_p);
		super_user = (fs_proc->fp_effuid == SU_UID ? TRUE : FALSE);   /* su? */
	// MUKDEBUG("super_user (0=super_user) %d\n", super_user);
		/* Check for special control messages first. */
		if (fs_call_nr == PROC_EVENT) 
		{
			/* Assume FS got signal. Synchronize, but don't exit. */
	// MUKDEBUG("fs_call_nr %d\n",fs_call_nr);
	// MUKDEBUG("PROC_EVENT %d\n",PROC_EVENT);
			fs_sync();
		} 
		else if (fs_call_nr == SYN_ALARM) 
		{
				/* Alarm timer expired. Used only for select(). Check it. */
	//        	fs_expire_timers(fs_m_in.NOTIFY_TIMESTAMP);
		} 
		else if ((fs_call_nr & NOTIFY_MESSAGE)) {
				/* Device notifies us of an event. */
	//        	dev_status(&fs_m_in);
		}	else	{
			/* Call the internal function that does the work. */
			if (fs_call_nr < 0 || fs_call_nr >= NCALLS) { 
				rcode = EDVSNOSYS;
				fprintf(stderr,"ERROR: warning illegal %d system call by %d\n", fs_call_nr, fs_who_e);
	//		} else if (fs_proc->fp_pid == PID_FREE) {
	//			rcode = ENOSYS;
	//			printf("FS, bad process, who = %d, fs_call_nr = %d, endpt1 = %d\n",
	//				 fs_who_e, fs_call_nr, fs_m_in.endpt1);
			} else {
				rcode = (*fs_call_vec[fs_call_nr])();
			}
			MUKDEBUG("rcode (RESPUESTA syscall) %d\n", rcode);
			/* Copy the results back to the user and send fs_reply. */
			if (rcode != SUSPEND) { fs_reply(fs_who_e, rcode); }
#ifdef READAHEAD			
			if (rdahead_inode != NIL_INODE) {
				read_ahead(); /* do block read ahead */
			}
#endif // READAHEAD			
			
		}
	}
  return(OK);				/* shouldn't come here */
}

/*===========================================================================*
 *				fs_get_work				     *
 *===========================================================================*/
 void fs_get_work()
 {  
  /* Normally wait for new input.  However, if 'reviving' is
   * nonzero, a suspended process must be awakened.
   */
   int ret;

#ifdef ANULADO 
   register struct fproc_table *rp;

   if (reviving != 0) 
   {
	/* Revive a suspended process. */
   	for (rp = &fs_proc_table[0]; rp < &fs_proc_table[dc_ptr->dc_nr_procs]; rp++) 
   		if (rp->fp_pid != PID_FREE && rp->fp_revived == REVIVING) 
   		{
   			fs_who_p = (int)(rp - fs_proc_table);
   			fs_who_e = rp->fp_endpoint;
   			fs_call_nr = rp->fp_fd & BYTE;
   			fs_m_in.fd = (rp->fp_fd >>8) & BYTE;
   			fs_m_in.buffer = rp->fp_buffer;
   			fs_m_in.nbytes = rp->fp_nbytes;
			rp->fp_suspended = NOT_SUSPENDED; /*no longer hanging*/
   			rp->fp_revived = NOT_REVIVING;
   			reviving--;
   			return;
   		}
   		panic(__FILE__,"fs_get_work couldn't revive anyone", NO_NUM);
   	}
#endif /* ANULADO */

   	while( TRUE) 
   	{
// MUKDEBUG(" Normal case.  No one to revive. \n");
		MUKDEBUG(" LISTENING . \n");
   		if ( (ret=dvk_receive(ANY, &fs_m_in)) != OK) ERROR_PT_EXIT(ret);
// MUKDEBUG(" Peticion: %d\n", ret);
   		fs_m_ptr = &fs_m_in;
		MUKDEBUG( MSG1_FORMAT, MSG1_FIELDS(fs_m_ptr));
		MUKDEBUG( MSG3_FORMAT, MSG3_FIELDS(fs_m_ptr));

   		fs_who_e = fs_m_in.m_source;
   		fs_who_p = _ENDPOINT_P(fs_who_e);
   		if(fs_who_p < -(dc_ptr->dc_nr_tasks) || fs_who_p >= (dc_ptr->dc_nr_procs)){
			ret = EDVSBADPROC;
   			ERROR_PT_EXIT(ret);
		}
//	if(fs_who_p >= 0 && fs_proc_table[fs_who_p].fp_endpoint == NONE) {
//    	printf("FS: ignoring request from %d, endpointless slot %d (%d)\n",
//		fs_m_in.m_source, fs_who_p, fs_m_in.m_type);
//	continue;
//    }

//    if(fs_who_p >= 0 && fs_proc_table[fs_who_p].fp_endpoint != fs_who_e) {
//    	printf("FS: receive endpoint inconsistent (%d, %d, %d).\n",
//		fs_who_e, fs_proc_table[fs_who_p].fp_endpoint, fs_who_e);
//	panic(__FILE__, "FS: inconsistent endpoint ", NO_NUM);
//	continue;
//    }
   		fs_call_nr = fs_m_in.m_type;
// MUKDEBUG("fs_m_in.m_type = %d \n", fs_m_in.m_type);
		MUKDEBUG("fs_call_nr = %d \n", fs_call_nr);
   		return;
   	}
   }

extern mproc_t *mp;		/* PM process table			*/

/*===========================================================================*
 *				fs_init					     *
 *===========================================================================*/
 void fs_init(char *cfg_file)
 {
 	int rcode, i;
    config_t *cfg;
 	struct inode *rip;
 	fproc_t *rfp;

	MUKDEBUG("dcid=%d fs_ep=%d\n",dcid, fs_ep);
	
	fs_pid = syscall (SYS_gettid);
	MUKDEBUG("fs_pid=%d\n", fs_pid);
	
	fs_ep = dvk_tbind(dcid, fs_ep);
	MUKDEBUG("fs_ep=%d\n", fs_ep);
	if( fs_ep != fs_ep) {
		ERROR_PRINT(EDVSENDPOINT);
		pthread_exit(&fs_ep);
	}
	
	MUKDEBUG(DVS_USR_FORMAT,DVS_USR_FIELDS(dvs_ptr));
	MUKDEBUG(DC_USR1_FORMAT,DC_USR1_FIELDS(dc_ptr));
	MUKDEBUG(DC_USR2_FORMAT,DC_USR2_FIELDS(dc_ptr));

	MUKDEBUG("Get fs_ep info\n");
	fs_proc_ptr = (proc_usr_t *) PROC_MAPPED(fs_ep);
	MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(fs_proc_ptr));
	
	/* Register into SYSTASK (as an autofork) */
	MUKDEBUG("Register FS into SYSTASK fs_pid=%d\n",fs_pid);
	fs_ep = sys_bindproc(fs_ep, fs_pid, LCL_BIND);
	if(fs_ep < 0) ERROR_PT_EXIT(fs_ep);

	rcode = mol_bindproc(fs_ep, fs_ep, fs_pid, LCL_BIND);
	MUKDEBUG("rcode=%d\n", rcode);
	if( rcode < 0) {
		ERROR_PRINT(rcode);
		pthread_exit(&fs_ep);
	}

	// set the name of FS 
	rcode = sys_rsetpname(fs_ep, "fs", local_nodeid);
	if(rcode < 0) ERROR_PT_EXIT(rcode);
	
	/* alloc dynamic memory for the FS process table */
	MUKDEBUG("Alloc dynamic memory for the FS process table nr_procs=%d\n", dc_ptr->dc_nr_procs);
// 	fs_proc_table = malloc((dc_ptr->dc_nr_procs)*sizeof(fproc_t));
	posix_memalign( (void**) &fs_proc_table, getpagesize(), (dc_ptr->dc_nr_procs)*sizeof(fproc_t));
 	if(fs_proc_table == NULL) ERROR_PT_EXIT(errno);

	/* alloc dynamic memory for a temporal copy of  PM process table */
//	MUKDEBUG("Alloc dynamic memory for temporal copy of  PM table nr_procs=%d\n", dc_ptr->dc_nr_procs);
//	posix_memalign( (void**) &pm_proc_table, getpagesize(), (dc_ptr->dc_nr_procs)*sizeof(mproc_t));
//	if(pm_proc_table == NULL) ERROR_PT_EXIT(rcode);
	
	/*dump PM proc table */
//	pm_proc_table = mp;
//	rcode = mol_getproctab(pm_proc_table);
//	if(rcode) ERROR_PT_EXIT(rcode);
	
 	/* Initialize FS's process table */
 	for (i = 0; i < dc_ptr->dc_nr_procs; i++) {
		/* descriptors of processes running before 		*/
		/* FS are filled with values get from PM table 	*/
 		fproc_init(i); 
	}
	
	MUKDEBUG("Alloc dynamic memory for user_path\n");
	posix_memalign( (void**) &user_path, getpagesize(), MNX_PATH_MAX);
 	if(user_path == NULL) ERROR_PT_EXIT(errno);

	/* All process table entries have been set. Continue with FS initialization.
	   * Certain relations must hold for the file system to work at all. Some 
	   * extra block_size requirements are checked at super-block-read-in time.
	   */
	if (OPEN_MAX > 127) panic(__FILE__,"OPEN_MAX > 127", NO_NUM);
	if (NR_BUFS < 6) panic(__FILE__,"NR_BUFS < 6", NO_NUM);
	if (V1_INODE_SIZE != 32) panic(__FILE__,"V1 inode size != 32", NO_NUM);
	if (V2_INODE_SIZE != 64) panic(__FILE__,"V2 inode size != 64", NO_NUM);
	if (OPEN_MAX > 8 * sizeof(long))
		panic(__FILE__,"Too few bits in fp_cloexec", NO_NUM);

	/* The following initializations are needed to let dev_opcl succeed .*/
	fs_proc = (struct fproc_table *) NULL;
	fs_who_e = fs_who_p = NONE;

	/*Reading the MOLFS config file*/
	init_dmap();     /* initialize device table and map boot driver */
	fs_cfg_dev_nr = 1;		/* start at 1: I don't know why !!! */
	cfg = nil;
	cfg = config_read(cfg_file, CFG_ESCAPED, cfg);
	rcode = fs_search_dev_cfg(cfg);
	if (rcode || fs_cfg_dev_nr==0 ) {
		MUKDEBUG("Configuration error: fs_cfg_dev_nr=%d\n", fs_cfg_dev_nr);        
		ERROR_PT_EXIT(rcode);
	}else{
		MUKDEBUG("fs_cfg_dev_nr=%d\n", fs_cfg_dev_nr);        		
	}
 	/* Verifying only one root device*/
	if (count_root_devs() != 1) {
		MUKDEBUG("\n Configuration error. One root dev is required!\n");     
		rcode = EDVSNXIO;
		ERROR_PT_EXIT(rcode);
	}
	
	buf_pool();     /* initialize buffer pool */
	rip = init_root();      /* init root device and load super block */
	MUKDEBUG(INODE_FORMAT1,INODE_FIELDS1(rip));    
	//init_select();     //init select() structures 
	
	/* The root device can now be accessed; set process directories. */
	for (rfp=&fs_proc_table[0]; rfp < &fs_proc_table[dc_ptr->dc_nr_procs]; rfp++)  {
		FD_ZERO(&(rfp->fp_filp_inuse));
//		if (rfp->fp_pid != PID_FREE) {
//			rip = get_inode(root_dev, ROOT_INODE);
			dup_inode(rip);
			rfp->fp_rootdir = rip;
			rfp->fp_workdir = rip;
//		} else  {
//			rfp->fp_endpoint = NONE;
//		}
//		MUKDEBUG(FS_PROC_FORMAT, FS_PROC_FIELDS(rfp));        
	} 
	
}

/*===========================================================================*
 *        init_root            *
 *===========================================================================*/
 struct inode *init_root(void)
 {
 	int bad;
 	struct super_block *sp;
	int root_major;
	dconf_t *root_ptr; 	
 	register struct inode *rip = NIL_INODE;
	mnx_dev_t root_dev;
 	int s;

	MUKDEBUG("--BEGIN FS--\n"); 
	/* Open the root device. */
 	// root_dev = DEV_IMGRD; 

 	/*Get de root device from conf read*/
 	root_major = get_root_major();
	root_ptr = &MAJOR2TAB(root_major).dmap_cfg;
	MUKDEBUG(DCONF_FORMAT, DCONF_FIELDS(root_ptr)); 	 	
    root_dev = MM2DEV(root_major, root_ptr->minor);
	// Esta apertura a continuacion pertenece al codigo original.
	// Hace en paralelo lo que hacemos hasta ahora con la imagen del disco.
	MUKDEBUG(" dev_open -> root_dev=%d, PROC_NR=%d, R_BIT|W_BIT=%d\n", root_dev, fs_ep, R_BIT|W_BIT);    
	if ((s=dev_open(root_dev, fs_ep, R_BIT|W_BIT)) < OK)
		MUKDEBUG("Cannot open root device %d\n", s);

	//#if ENABLE_CACHE2
	  /* The RAM disk is a second level block cache while not otherwise used. */
	//  init_cache2(ram_size);
	//#endif

	/* Initialize the super_block table. */
	for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++)
		sp->s_dev = NO_DEV;

	/* Read in super_block for the root file system. */
 	sp = &super_block[0];
 	sp->s_dev = root_dev;

	/* Check super_block for consistency. */
 	bad = (read_super(sp) != OK);

 	//rip = get_inode(root_dev, ROOT_INODE);
	if (!bad) {
		rip = get_inode(root_dev, ROOT_INODE);  /* inode for root dir */
//		MUKDEBUG(INODE_FORMAT1,INODE_FIELDS1(rip));    
		if ( (rip->i_mode & I_TYPE) != I_DIRECTORY) bad++;
	}
	if (bad) panic(__FILE__,"Invalid root file system", NO_NUM);

	sp->s_imount = rip;
	dup_inode(rip);
	sp->s_isup = rip;
	sp->s_rd_only = 0;
	
	MUKDEBUG(SUPER_BLOCK_FORMAT1, SUPER_BLOCK_FIELDS1(sp));

	return(rip);
}

/*===========================================================================*
 *        buf_pool             *
 *===========================================================================*/
 void buf_pool(void)
 {
/* Initialize the buffer pool. */

 	register struct buf *bp;

 	bufs_in_use = 0;
 	front = &buf[0];
 	rear = &buf[NR_BUFS - 1];

 	for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++) {
 		bp->b_blocknr = NO_BLOCK;
 		bp->b_dev = NO_DEV;
 		bp->b_next = bp + 1;
 		bp->b_prev = bp - 1;
 	}
 	buf[0].b_prev = NIL_BUF;
 	buf[NR_BUFS - 1].b_next = NIL_BUF;

 	for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++) bp->b_hash = bp->b_next;
 		buf_hash[0] = front;

 }

/*===========================================================================*
 *				fs_reply					     *
 * int whom;			 process to fs_reply to 			     *
 * int result;			result of the call (usually OK or error #)   *
 *===========================================================================*/
 void fs_reply(int whom, int result)
 {
 	int rcode;

MUKDEBUG("Send a fs_reply to a user process.\n");

 	fs_m_out.reply_type = result;
 	fs_m_ptr = &fs_m_out;
MUKDEBUG( MSG1_FORMAT, MSG1_FIELDS(fs_m_ptr));

 	rcode = dvk_send(whom, &fs_m_out);
	MUKDEBUG("rcode=%d\n", rcode);
 	if (rcode) ERROR_PRINT(rcode);

 }

