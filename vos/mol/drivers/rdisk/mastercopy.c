/****************************************************************/
/****************************************************************/
/* 				MASTER_COPY										*/
/* MASTER_COPY algorithm routines for intra-nodes RDISKs			*/
/****************************************************************/
#define _MULTI_THREADED
#define _GNU_SOURCE     
#define  MOL_USERSPACE	1


// #define TASKDBG		1


#include "rdisk.h"
#include <sys/syscall.h>
#include "const.h"

#define TRUE 1
#define WAIT4BIND_MS 1000

message *m_ptr, mess;
int maxbuf,dev_caller, proc_nr; /*dcid definida en glo.h*/
blksize_t tr_blksize;
char *buff;
int mtr_dev; /*device number*/
vir_bytes address_bk; /*buffer bk to dvk_vcopy*/

/*for master-slave struct*/

struct msdevvec_s {		/* vector for minor devices */
  blksize_t st_trblksize; /* of stat */
  unsigned *localbuff;	/* buffer to the device*/
  unsigned	buff_size;	/* buffer size for this device*/
  int img_fd; 			/*file descriptor - disk image*/
};

typedef struct msdevvec_s msdevvec_t;

msdevvec_t ms_devvec[NR_DEVS];

int slv_mbr; // PAP

/*===========================================================================*
 *				init_mastercopy				     
 * It connects REPLICATE thread to the SPREAD daemon and initilize several local 
 * and replicated  var2iables
 *===========================================================================*/
int init_mastercopy(void)
{
	int rcode;

	TASKDEBUG("Initializing MASTER_COPY\n"); 
	return(OK);
}

/*===========================================================================*
 *				init_m3ipcm					     *
 *===========================================================================*/
void init_m3ipcm()
{
	int rcode;

	TASKDEBUG("Binding RDISK MASTER=%d\n", RDISK_MASTER);
	mc_ep = dvk_tbind(dcid, RDISK_MASTER);
	TASKDEBUG("mc_ep=%d\n", mc_ep);
	if(mc_ep < 0) ERROR_EXIT(mc_ep);
	
	mc_lpid = (pid_t) syscall (SYS_gettid);
	TASKDEBUG("mc_ep=%d mc_lpid=%d\n", mc_ep, mc_lpid);
	
	if( endp_flag == 0) { // RDISK runs under MOL 
		TASKDEBUG("Bind MASTER to SYSTASK\n");
		rcode = sys_bindproc(mc_ep, mc_lpid, REPLICA_BIND);
		if(rcode < 0) ERROR_EXIT(rcode);				
		do {
			rcode = dvk_wait4bind_T(WAIT4BIND_MS);
			TASKDEBUG("MASTER dvk_wait4bind_T  rcode=%d\n", rcode);
			if (rcode == EDVSTIMEDOUT) {
				TASKDEBUG("CLIENT dvk_wait4bind_T TIMEOUT\n");
				continue ;
			} else if ( rcode < 0)
				ERROR_EXIT(EXIT_FAILURE);
		} while	(rcode < OK);
	}else{ // RDISK runs autonomous 
		TASKDEBUG("Binding Remote RDISK SLAVE=%d on node=%d\n", RDISK_SLAVE, sc_node);
		sc_ep = dvk_rmtbind(dcid,"slave",RDISK_SLAVE, sc_node);
		if(sc_ep < 0) ERROR_EXIT(sc_ep);
		TASKDEBUG("sc_ep=%d\n", sc_ep);
	}

   return(OK);
}

/*===========================================================================*
 *				mastercopy_main									     *
 *===========================================================================*/
void *mastercopy_main(void *arg)
{
	static 	char source[MAX_GROUP_NAME];
	int rcode, mtype;
	
	slv_mbr = (int *) arg; // PAP obtengo nuevo miembro 
	TASKDEBUG("replicate_main dcid=%d local_nodeid=%d slv_mbr=%d\n"
		,dcid, local_nodeid, slv_mbr ); // PAP	
	
    init_m3ipcm();	
	
	TASKDEBUG("MASTER CONNECT\n");

	while(TRUE){
		rcode = mastercopy_loop(&mtype, source);
	}
	return(rcode);
}

/*===========================================================================*
 *				mastercopy_loop				    
 * return : service_type
 *===========================================================================*/
int mastercopy_loop(int *mtype, char *source){

int ret,r;	

while (TRUE) { 

	ret = dvk_receive( ANY , (long) &mess);

	if( ret != OK){
		TASKDEBUG("dvk_receive ERROR %d\n",ret); 
		exit(1); /*MARIE_VER:ver acá cual sería el manejo de error adecuado*/
		}
	
	// TASKDEBUG("\nRECEIVE: m_source=%d, m_type=%d, DEVICE=%d, IO_ENDPT=%d, POSITION/BLOCK_NR=%u, COUNT=%d, ADDRESS:%X, Device size/sigs(m2_l2):%d\n",
		// mess.m_source,
		// mess.m_type,
		// mess.DEVICE,
		// mess.IO_ENDPT,
		// mess.POSITION,
		// mess.COUNT,
		// mess.ADDRESS,
		// mess.m2_l2); 
		
	TASKDEBUG("\nRECEIVE: \n");
				
	//MESS: m2
	//*    m_type      DEVICE    IO_ENDPT    COUNT    POSITION  ADRRESS
	//* | DEV_SCATTER| device  | proc nr | iov len |  offset | iov ptr |
	//	m.m_type		m.m2_i1	m.m2_i2		m.m2_i3	 m.m2_l1	m.m2_p1
		
	dev_caller = mess.m_source;
	TASKDEBUG("device_caller: %d\n", dev_caller);
		
	proc_nr = mess.m_source;
	TASKDEBUG("proc_nr: %d\n", proc_nr);
	TASKDEBUG("r_type: %d\n", r_type);
	
	/* Now carry out the work. */
	switch(mess.m_type) {
	
		case DEV_TRANS:		
					TASKDEBUG("m_type: %d - DEV_TRANS\n", mess.m_type);	
					TASKDEBUG("\nRECEIVE: m_source=%d, m_type=%d, DEVICE=%d, IO_ENDPT=%d, POSITION/BLOCK_NR=%u, COUNT=%d, ADDRESS:%X, Device size/sigs(m2_l2):%d\n",
							mess.m_source,
							mess.m_type,
							mess.DEVICE,
							mess.IO_ENDPT,
							mess.POSITION,
							mess.COUNT,
							mess.ADDRESS,
							mess.m2_l2); 
					r = (dev_transfer)(&mess);
					TASKDEBUG("r(dev_transfer): %d\n", r);
					break;	
		case DEV_CFULL:	
					if (( dynamic_opt != DO_DYNUPDATES ) && ( r_type == DEV_CFULL )) {
						TASKDEBUG("m_type: %d - DEV_CFULL\n", mess.m_type);	
						TASKDEBUG("\nRECEIVE: m_source=%d, m_type=%d, DEVICE=%d, IO_ENDPT=%d, POSITION/BLOCK_NR=%u, COUNT=%d, ADDRESS:%X, Device size/sigs(m2_l2):%d\n",
							mess.m_source,
							mess.m_type,
							mess.DEVICE,
							mess.IO_ENDPT,
							mess.POSITION,
							mess.COUNT,
							mess.ADDRESS,
							mess.m2_l2); 
						r = (copy_full)(&mess);
						TASKDEBUG("r(copy_full): %d\n", r);
						break;					
					}
					else{
						fprintf( stderr,"ERROR Transfer Method - Prymary: %d(dyn=%d), Backup%d\n", r_type, dynamic_opt, mess.m_type);
						fflush(stderr);
						exit(1);	
					}
		case DEV_UFULL:
					if (( dynamic_opt == DO_DYNUPDATES ) && ( r_type == DEV_CFULL )) {
						TASKDEBUG("m_type: %d - DEV_UFULL\n", mess.m_type);	
						TASKDEBUG("\nRECEIVE: m_source=%d, m_type=%d, DEVICE=%d, IO_ENDPT=%d, POSITION/BLOCK_NR=%u, COUNT=%d, ADDRESS:%X, Device size/sigs(m2_l2):%d\n",
							mess.m_source,
							mess.m_type,
							mess.DEVICE,
							mess.IO_ENDPT,
							mess.POSITION,
							mess.COUNT,
							mess.ADDRESS,
							mess.m2_l2); 
						r = (mufull_copy)(&mess);
						TASKDEBUG("r(mufull_copy): %d\n", r);
						break;					
					}
					else{
						fprintf( stderr,"ERROR Transfer Method - Prymary: %d(dyn=%d), Backup%d\n", r_type, dynamic_opt, mess.m_type);
						fflush(stderr);
						exit(1);	
					}
		case DEV_CMD5:
					if (( dynamic_opt != DO_DYNUPDATES ) && ( r_type == DEV_CMD5 )) {
						TASKDEBUG("m_type: %d - DEV_CMD5 - DO_DYNUPDATES: %d\n", mess.m_type, dynamic_opt );	
						TASKDEBUG("\nRECEIVE: m_source=%d, m_type=%d, POSITION/BLOCK_NR=%u, sigs:%s\n",
								mess.m_source,
								mess.m_type,
								mess.mB_nr,
								mess.mB_md5); 
						r = (copy_md5)(&mess);
						TASKDEBUG("r(copy_md5): %d\n", r);
						break;					
					}
					else{
						fprintf( stderr,"ERROR Transfer Method - Prymary: %d(dyn=%d), Backup%d\n", r_type, dynamic_opt, mess.m_type);
						fflush(stderr);
						exit(1);	
					}
		case DEV_UMD5:
				if (( dynamic_opt == DO_DYNUPDATES ) && ( r_type == DEV_CMD5 )) {
						TASKDEBUG("m_type: %d - DEV_UMD5 - DO_DYNUPDATES: %d\n", mess.m_type, dynamic_opt );	
						TASKDEBUG("\nRECEIVE: m_source=%d, m_type=%d, POSITION/BLOCK_NR=%u, sigs:%s\n",
								mess.m_source,
								mess.m_type,
								mess.mB_nr,
								mess.mB_md5); 
						r = (copy_md5)(&mess);
						TASKDEBUG("r(copy_md5): %d\n", r);
						break;					
					}
					else{
						fprintf( stderr,"ERROR Transfer Method - Prymary: %d(dyn=%d), Backup%d\n", r_type, dynamic_opt, mess.m_type);
						fflush(stderr);
						exit(1);	
					}		
		case DEV_EOF:
					TASKDEBUG("m_type: %d - DEV_EOF\n", mess.m_type);	
					r = (dev_ready)(&mess);
					TASKDEBUG("r(dev_ready): %d\n", r);
					break;							
		case RD_DISK_ERR:			
		case RD_DISK_EOF:
					TASKDEBUG("m_type: %d - RD_DISK_EOF\n", mess.m_type);	
					r = (rd_ready)(&mess);
					TASKDEBUG("r(rd_ready): %d\n", r);
					break;							
		default:		
			TASKDEBUG("Invalid type: %d\n", mess.m_type);
			break;
		}
	
	if (r != EDVSDONTREPLY) { /*para MOL, /kernel/minix/molerrno.h no enviar respuesta (SIGN 201)*/
		 
		if ( r_type != DEV_CMD5 ){
			mess.REP_ENDPT = proc_nr;
			/* Status is # of bytes transferred or error code. */
			mess.REP_STATUS = r;	
			
			TASKDEBUG("SEND msg a DEVICE_CALLER: %d -> m_type=%d, (REP_ENDPT)=%d, (REP_STATUS)=%d\n",
				dev_caller,
				mess.m_type,
				mess.REP_ENDPT,
				mess.REP_STATUS);
		} else{
			TASKDEBUG("SEND msg a DEVICE_CALLER: %d -> m_type=%d, (POSITION/BLOCK_NR)=%d, (SIGS)=%s\n",
				dev_caller,
				mess.m_type,
				mess.mB_nr,
				mess.mB_md5);
		}
			
		ret = dvk_send(dev_caller, &mess); /*envío respuesta al cliente q solicitó*/
		if( ret != 0 ) {
			fprintf( stderr,"SEND ret=%d\n",ret);
			fflush(stderr);
			exit(1);
			}
		}
	}
}

/***************************************************************************/
/* FUNCTIONS 			*/
/***************************************************************************/
/*===========================================================================*
 *				dev_transfer					     *
 *===========================================================================*/
int dev_transfer(mp)
message *mp;			/* pointer to read or write message */
{
/* Carry out a single read or write request. */
  int r, size, rcode;
  blksize_t blksize; 
  off_t position;
  unsigned bytes, tbytes;
    
  TASKDEBUG("Device size - Primary: %u Backup: %u\n", devvec[mp->DEVICE].st_size, mp->m2_l2);
  if (devvec[mp->DEVICE].st_size != mp->m2_l2) {
	  fprintf(stderr,"ERROR! Device size - Primary: %u Backup: %u\n", devvec[mp->DEVICE].st_size, mp->m2_l2);
	  fflush(stderr);
	  exit(EXIT_FAILURE);
	}
	
  mtr_dev = mp->DEVICE;
  TASKDEBUG("Device to update=%d\n", mtr_dev);

  /*---------- Open image device ---------------*/
  ms_devvec[mp->DEVICE].img_fd = open(devvec[mp->DEVICE].img_ptr, O_RDONLY);
  TASKDEBUG("Open imagen FD=%d\n", ms_devvec[mp->DEVICE].img_fd);
			
  if(ms_devvec[mp->DEVICE].img_fd < 0) {
	TASKDEBUG("img_fd=%d\n", ms_devvec[mp->DEVICE].img_fd);
	rcode = errno;
	TASKDEBUG("rcode=%d\n", rcode);
	exit(EXIT_FAILURE);
	}

     
  /*---------------- Allocate memory for buffer  ---------------*/
  TASKDEBUG("Buffer - Transfer block: %u\n", mp->COUNT);
  tr_blksize = mp->COUNT; /*genera el buffer a partir de la cantidad de bytes que le indicó el slave*/
  
  posix_memalign( (void **) &buff, getpagesize(), tr_blksize );
  if (buff == NULL) {
  	fprintf(stderr, "posix_memalign\n");
	fflush(stderr);
  	exit(EXIT_FAILURE);
   }
  ms_devvec[mp->DEVICE].localbuff = buff;
  TASKDEBUG("Buffer (block) ms_devvec[%d].localbuff=%p\n", mp->DEVICE, ms_devvec[mp->DEVICE].localbuff);
	
  position = mp->POSITION; /*esta es info que viene inicialmente del slave, no se modifica acá*/
  TASKDEBUG("Init copy of=%X\n", position); 
  
  address_bk = mp->ADDRESS; /*idem position*/
  TASKDEBUG("Buffer backup %X\n", mp->ADDRESS);
  
  mp->m_type=DEV_TRANSR; /*ok for transfer*/
  TASKDEBUG("mp->m_type=DEV_TRANSR %d\n", mp->m_type);
 	
return(OK);
}

/*===========================================================================*
 *				copy_full					     *
 *===========================================================================*/
int copy_full( mp)
message *mp;			/* pointer to read or write message */
{
  int r, size, rcode;
  off_t position;
  unsigned bytes;
  vir_bytes address_bk;
  
  position = mp->POSITION;
  TASKDEBUG("Position %X\n", mp->POSITION);
  
  address_bk = mp->ADDRESS;
  TASKDEBUG("Buffer backup %X\n", mp->ADDRESS);
  
  /*tr_blksize, ya quedó definido anteriormente es la cantidad de bytes que se van a transferir*/
  bytes = pread(ms_devvec[mp->DEVICE].img_fd, ms_devvec[mp->DEVICE].localbuff, tr_blksize, position);
  TASKDEBUG("pread: %s\n", ms_devvec[mp->DEVICE].localbuff);
				
  if(bytes < 0) ERROR_EXIT(errno);
	
  TASKDEBUG("master_ep: %d\n", mc_ep);
  TASKDEBUG("ms_devvec[mp->DEVICE].localbuff: %X\n", ms_devvec[mp->DEVICE].localbuff);
  TASKDEBUG("proc_nr: %d\n", proc_nr);
  TASKDEBUG("address_bk: %X\n", address_bk);
  TASKDEBUG("bytes: %u\n", bytes);
	
  rcode = dvk_vcopy(mc_ep, ms_devvec[mp->DEVICE].localbuff, proc_nr, address_bk, bytes); 
	
  if (rcode != 0 ) {
	fprintf( stderr,"VCOPY rcode=%d\n",rcode);
	fflush(stderr);
	exit(1);
	}
	
   mp->m_type=DEV_CFULLR; /*ok for transfer*/;	
   TASKDEBUG("mp->m_type=DEV_CFULLR %d\n", mp->m_type);
   
   TASKDEBUG("copy_img: dvk_vcopy(copy_img -> copy_img_bk) rcode=%d\n", rcode);  
   TASKDEBUG("bytes= %d\n", bytes);     
   TASKDEBUG("copy - Offset (read) %X, Data: %s\n", ms_devvec[mp->DEVICE].localbuff, ms_devvec[mp->DEVICE].localbuff);			
   
return(bytes);
}

/*===========================================================================*
 *				mufull_copy					     *
 *===========================================================================*/
int mufull_copy( mp)
message *mp;			/* pointer to read or write message */
{
  int r, size, rcode;
  off_t position;
  unsigned bytes;
  vir_bytes address_bk;
  
  position = mp->POSITION;
  TASKDEBUG("Block_nr %u\n", mp->POSITION);
  
  address_bk = mp->ADDRESS;
  TASKDEBUG("Buffer backup %X\n", mp->ADDRESS);
  
  /*tr_blksize, ya quedó definido anteriormente es la cantidad de bytes que se van a transferir*/
  bytes = pread(ms_devvec[mp->DEVICE].img_fd, ms_devvec[mp->DEVICE].localbuff, tr_blksize, ( position * tr_blksize) );
  TASKDEBUG("pread: %s\n", ms_devvec[mp->DEVICE].localbuff);
				
  if(bytes < 0) ERROR_EXIT(errno);
	
  TASKDEBUG("master_ep: %d\n", mc_ep);
  TASKDEBUG("ms_devvec[mp->DEVICE].localbuff: %X\n", ms_devvec[mp->DEVICE].localbuff);
  TASKDEBUG("proc_nr: %d\n", proc_nr);
  TASKDEBUG("address_bk: %X\n", address_bk);
  TASKDEBUG("bytes: %u\n", bytes);
  TASKDEBUG("Block number: %d\n", position);
  
  rcode = dvk_vcopy(mc_ep, ms_devvec[mp->DEVICE].localbuff, proc_nr, address_bk, bytes); 
	
  if (rcode != 0 ) {
	fprintf( stderr,"VCOPY rcode=%d\n",rcode);
	fflush(stderr);
	exit(1);
	} 	
	
   mp->m_type=DEV_UFULLR; /* replay|*/;	
   TASKDEBUG("mp->m_type=DEV_UFULLR %d\n", mp->m_type);
   
   TASKDEBUG("copy_img: dvk_vcopy(copy_img -> copy_img_bk) rcode=%d\n", rcode);  
   TASKDEBUG("bytes= %d\n", bytes);     
   TASKDEBUG("copy - Offset (read) %X, Data: %s\n", ms_devvec[mp->DEVICE].localbuff, ms_devvec[mp->DEVICE].localbuff);			
   
return(bytes);
}
/*===========================================================================*
 *				copy_md5					     *
 *===========================================================================*/
int copy_md5( mp)
message *mp;			/* pointer to read or write message */
{
  int r, size, rcode;
  off_t position;
  unsigned bytes;

  position = mp->mB_nr;
  TASKDEBUG("Block_nr %u\n", mp->mB_nr);
  
  // address_bk = mp->ADDRESS;
  TASKDEBUG("Buffer backup %X\n", address_bk);
  
  TASKDEBUG("mp->mB_md5: %s\n", mp->mB_md5);	
  
  /*tr_blksize, ya quedó definido anteriormente es la cantidad de bytes que se van a transferir*/
  bytes = pread(ms_devvec[mtr_dev].img_fd, ms_devvec[mtr_dev].localbuff, tr_blksize, ( position * tr_blksize) );
  TASKDEBUG("pread: %s\n", ms_devvec[mtr_dev].localbuff);
				
  if(bytes < 0) ERROR_EXIT(errno);
	
  TASKDEBUG("master_ep: %d\n", mc_ep);
  TASKDEBUG("ms_devvec[mp->DEVICE].localbuff: %X\n", ms_devvec[mtr_dev].localbuff);
  TASKDEBUG("proc_nr: %d\n", proc_nr);
  TASKDEBUG("address_bk: %X\n", address_bk);
  TASKDEBUG("bytes: %u\n", bytes);
  TASKDEBUG("Block number: %d\n", position);
  
  TASKDEBUG("md5_compute: fd=%d, buffer=%X, bytes=%u, position=%u\n",
							ms_devvec[mtr_dev].img_fd,
							ms_devvec[mtr_dev].localbuff,
							tr_blksize,
							(tr_blksize * position));
							
  md5_compute(ms_devvec[mtr_dev].img_fd, ms_devvec[mtr_dev].localbuff, tr_blksize, ( tr_blksize * position), sigm);

  TASKDEBUG("sigm: %s\n", sigm);	
     
  if ( memcmp(mp->mB_md5, sigm, MD5_SIZE) == 0 ) {
  	bytes = 0;
	rcode = 0;
	TASKDEBUG("Block %d matches\n", mp->mB_nr);	
    }
  else{
	TASKDEBUG("Block %d NOT matches\n", mp->mB_nr);
	rcode = dvk_vcopy(mc_ep, ms_devvec[mtr_dev].localbuff, proc_nr, address_bk, bytes); 
	
	if (rcode != 0 ) {
		fprintf( stderr,"VCOPY rcode=%d\n",rcode);
		fflush(stderr);
		exit(1);
		} 	
	}

   
  memcpy(mp->mB_md5, sigm, MD5_SIZE);
  TASKDEBUG("mp->mB_md5: %s\n", mp->mB_md5); 
	
   mp->m_type = ( mp->m_type == DEV_CMD5 )?DEV_CMD5R:DEV_UMD5R;
   TASKDEBUG("mp->m_type=%d (DEV_CMD5R:%d - DEV_UCMD5R:%d\n", mp->m_type,DEV_CMD5R,DEV_UMD5R);
   
   TASKDEBUG("copy_img: dvk_vcopy(copy_img -> copy_img_bk) rcode=%d\n", rcode);  
   TASKDEBUG("bytes= %d\n", bytes);     
   TASKDEBUG("copy - Offset (read) %X, Data: %s\n", ms_devvec[mtr_dev].localbuff, ms_devvec[mtr_dev].localbuff);			
   
return(bytes);
}

/*===========================================================================*
 *				dev_ready					     *
 *===========================================================================*/
int dev_ready( mp)
message *mp;			/* pointer to read or write message */
{
int rcode;

/*---------- Close image device ---------------*/
TASKDEBUG("Close imagen FD=%d\n", ms_devvec[mp->DEVICE].img_fd);
			
if ( rcode = close(ms_devvec[mp->DEVICE].img_fd) < 0) {
	TASKDEBUG("Error close=%d\n", rcode);
	exit(EXIT_FAILURE);
	}

free(ms_devvec[mp->DEVICE].localbuff);	
TASKDEBUG("check open device\n");
if ( devvec[mp->DEVICE].available == 1){
	TASKDEBUG("devvec[%d].available=%d\n", mp->DEVICE, devvec[mp->DEVICE].available);
	mp->m2_l2 = ( devvec[mp->DEVICE].active_flag == 1 )?1:0;
	TASKDEBUG("mp->m2_l2=%d, devvec[%d].active_flag=%d\n", mp->m2_l2, mp->DEVICE, devvec[mp->DEVICE].active_flag);
	}


mp->m_type=DEV_EOFR; /*ok for transfer*/;	
TASKDEBUG("mp->m_type=DEV_EOF %d\n", mp->m_type);

return(OK);
}
/*===========================================================================*
 *				rd_ready					     *
 *===========================================================================*/
int rd_ready( message *mp)			/* pointer to read or write message */
{
	int rcode, i;
	message  msg;	

	if (mp->m_type == RD_DISK_EOF || mp->m_type == RD_DISK_ERR){
		TASKDEBUG("nuevo nodo: m2_l2 %d\n", mp->m2_l2);
		SET_BIT(bm_nodes, slv_mbr); // PAP 
		nr_nodes++;
		TASKDEBUG("nr_nodes=%d\n", nr_nodes);
		nr_sync++;
		TASKDEBUG("nr_sync=%d\n", nr_sync);
		SET_BIT(bm_sync, mp->m2_l2);
		TASKDEBUG("New sync mbr=%d bm_sync=%X\n", 
			mp->m2_l2 , bm_sync);
		mc_status_info();
	}
		
	TASKDEBUG("check open device\n");
	for( i = 0; i < NR_DEVS; i++){
		TASKDEBUG("devvec[%d].available=%d\n", i, devvec[i].available);
		if ( devvec[i].active_flag == 1 ){ /*device open in primary*/ 
			TASKDEBUG("devvec[%d].active_flag=%d\n", i, devvec[i].active_flag);
		}
	}
	
	TASKDEBUG("unbinding RDISK MASTER=%d\n", RDISK_MASTER);
	TASKDEBUG("dcid=%d, mc_ep=%d\n", dcid, mc_ep);
	rcode = dvk_unbind(dcid,mc_ep);
	TASKDEBUG("rcode unbind=%d\n", rcode);
	if(rcode < 0) ERROR_EXIT(rcode);

	TASKDEBUG("unbinding RDISK SLAVE=%d\n", RDISK_SLAVE);
	TASKDEBUG("dcid=%d, sc_ep=%d\n", dcid, sc_ep);
	rcode = dvk_unbind(dcid,sc_ep);
	TASKDEBUG("rcode unbind=%d\n", rcode);
	if(rcode < 0) ERROR_EXIT(rcode);

	if ( dynamic_opt == DO_DYNUPDATES ){ 
		COND_SIGNAL(bk_barrier); //MARIE
	}

	TASKDEBUG("Exit mastercopy\n");
	pthread_exit(NULL);
}	
