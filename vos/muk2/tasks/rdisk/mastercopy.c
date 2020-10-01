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

message *rd_mptr, rd_mess;
int maxbuf,dev_caller, proc_nr; /*dcid definida en glo.h*/
blksize_t tr_blksize;
char *buff;
int mtr_dev; /*device number*/
vir_bytes address_bk; /*buffer bk to MUK_vcopy*/

/*for master-slave struct*/

struct msdevvec {		/* vector for minor devices */
  blksize_t st_trblksize; /* of stat */
  unsigned *localbuff;	/* buffer to the device*/
  unsigned	buff_size;	/* buffer size for this device*/
  int img_p; 			/*file descriptor - disk image*/
};

typedef struct msdevvec msdevvec_t;

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

	MUKDEBUG("Initializing MASTER_COPY\n"); 
	return(OK);
}

/*===========================================================================*
 *				init_m3ipcm					     *
 *===========================================================================*/
void init_m3ipcm()
{
	int rcode;

	MUKDEBUG("Binding RDISK MASTER=%d\n", RDISK_MASTER);
	mc_ep = muk_tbind(dcid, RDISK_MASTER);
	MUKDEBUG("mc_ep=%d\n", mc_ep);
	if(mc_ep < 0) ERROR_TSK_EXIT(mc_ep);
	
	mc_lpid = (pid_t) syscall (SYS_gettid);
	MUKDEBUG("mc_ep=%d mc_lpid=%d\n", mc_ep, mc_lpid);
	
	if( endp_flag == 0) { // RDISK runs under MOL 
		MUKDEBUG("Bind MASTER to SYSTASK\n");
		rcode = sys_bindproc(mc_ep, mc_lpid, REPLICA_BIND);
		if(rcode < 0) ERROR_TSK_EXIT(rcode);				
		do {
			rcode = muk_wait4bind_T(WAIT4BIND_MS);
			MUKDEBUG("MASTER muk_wait4bind_T  rcode=%d\n", rcode);
			if (rcode == EDVSTIMEDOUT) {
				MUKDEBUG("CLIENT muk_wait4bind_T TIMEOUT\n");
				continue ;
			} else if ( rcode < 0)
				ERROR_TSK_EXIT(EXIT_FAILURE);
		} while	(rcode < OK);
	}else{ // RDISK runs autonomous 
		MUKDEBUG("Binding Remote RDISK SLAVE=%d on node=%d\n", RDISK_SLAVE, sc_node);
		sc_ep = muk_rmtbind(dcid,"slave",RDISK_SLAVE, sc_node);
		if(sc_ep < 0) ERROR_TSK_EXIT(sc_ep);
		MUKDEBUG("sc_ep=%d\n", sc_ep);
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
	MUKDEBUG("replicate_main dcid=%d local_nodeid=%d slv_mbr=%d\n"
		,dcid, local_nodeid, slv_mbr ); // PAP	
	
    init_m3ipcm();	
	
	MUKDEBUG("MASTER CONNECT\n");

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

	ret = muk_receive( ANY , (long) &rd_mess);

	if( ret != OK){
		MUKDEBUG("muk_receive ERROR %d\n",ret); 
		exit(1); /*MARIE_VER:ver acá cual sería el manejo de error adecuado*/
		}
	
	// MUKDEBUG("\nRECEIVE: m_source=%d, m_type=%d, DEVICE=%d, IO_ENDPT=%d, POSITION/BLOCK_NR=%u, COUNT=%d, ADDRESS:%X, Device size/sigs(m2_l2):%d\n",
		// rd_mess.m_source,
		// rd_mess.m_type,
		// rd_mess.DEVICE,
		// rd_mess.IO_ENDPT,
		// rd_mess.POSITION,
		// rd_mess.COUNT,
		// rd_mess.ADDRESS,
		// rd_mess.m2_l2); 
		
	MUKDEBUG("\nRECEIVE: \n");
				
	//MESS: m2
	//*    m_type      DEVICE    IO_ENDPT    COUNT    POSITION  ADRRESS
	//* | DEV_SCATTER| device  | proc nr | iov len |  offset | iov ptr |
	//	m.m_type		m.m2_i1	m.m2_i2		m.m2_i3	 m.m2_l1	m.m2_p1
		
	dev_caller = rd_mess.m_source;
	MUKDEBUG("device_caller: %d\n", dev_caller);
		
	proc_nr = rd_mess.m_source;
	MUKDEBUG("proc_nr: %d\n", proc_nr);
	MUKDEBUG("r_type: %d\n", r_type);
	
	/* Now carry out the work. */
	switch(rd_mess.m_type) {
	
		case DEV_TRANS:		
					MUKDEBUG("m_type: %d - DEV_TRANS\n", rd_mess.m_type);	
					MUKDEBUG("\nRECEIVE: m_source=%d, m_type=%d, DEVICE=%d, IO_ENDPT=%d, POSITION/BLOCK_NR=%u, COUNT=%d, ADDRESS:%X, Device size/sigs(m2_l2):%d\n",
							rd_mess.m_source,
							rd_mess.m_type,
							rd_mess.DEVICE,
							rd_mess.IO_ENDPT,
							rd_mess.POSITION,
							rd_mess.COUNT,
							rd_mess.ADDRESS,
							rd_mess.m2_l2); 
					r = (dev_transfer)(&rd_mess);
					MUKDEBUG("r(dev_transfer): %d\n", r);
					break;	
		case DEV_CFULL:	
					if (( dynup_flag != DO_DYNUPDATES ) && ( r_type == DEV_CFULL )) {
						MUKDEBUG("m_type: %d - DEV_CFULL\n", rd_mess.m_type);	
						MUKDEBUG("\nRECEIVE: m_source=%d, m_type=%d, DEVICE=%d, IO_ENDPT=%d, POSITION/BLOCK_NR=%u, COUNT=%d, ADDRESS:%X, Device size/sigs(m2_l2):%d\n",
							rd_mess.m_source,
							rd_mess.m_type,
							rd_mess.DEVICE,
							rd_mess.IO_ENDPT,
							rd_mess.POSITION,
							rd_mess.COUNT,
							rd_mess.ADDRESS,
							rd_mess.m2_l2); 
						r = (copy_full)(&rd_mess);
						MUKDEBUG("r(copy_full): %d\n", r);
						break;					
					}
					else{
						fprintf( stderr,"ERROR Transfer Method - Prymary: %d(dyn=%d), Backup%d\n", r_type, dynup_flag, rd_mess.m_type);
						fflush(stderr);
						exit(1);	
					}
		case DEV_UFULL:
					if (( dynup_flag == DO_DYNUPDATES ) && ( r_type == DEV_CFULL )) {
						MUKDEBUG("m_type: %d - DEV_UFULL\n", rd_mess.m_type);	
						MUKDEBUG("\nRECEIVE: m_source=%d, m_type=%d, DEVICE=%d, IO_ENDPT=%d, POSITION/BLOCK_NR=%u, COUNT=%d, ADDRESS:%X, Device size/sigs(m2_l2):%d\n",
							rd_mess.m_source,
							rd_mess.m_type,
							rd_mess.DEVICE,
							rd_mess.IO_ENDPT,
							rd_mess.POSITION,
							rd_mess.COUNT,
							rd_mess.ADDRESS,
							rd_mess.m2_l2); 
						r = (mufull_copy)(&rd_mess);
						MUKDEBUG("r(mufull_copy): %d\n", r);
						break;					
					}
					else{
						fprintf( stderr,"ERROR Transfer Method - Prymary: %d(dyn=%d), Backup%d\n", r_type, dynup_flag, rd_mess.m_type);
						fflush(stderr);
						exit(1);	
					}
		case DEV_CMD5:
					if (( dynup_flag != DO_DYNUPDATES ) && ( r_type == DEV_CMD5 )) {
						MUKDEBUG("m_type: %d - DEV_CMD5 - DO_DYNUPDATES: %d\n", rd_mess.m_type, dynup_flag );	
						MUKDEBUG("\nRECEIVE: m_source=%d, m_type=%d, POSITION/BLOCK_NR=%u, sigs:%s\n",
								rd_mess.m_source,
								rd_mess.m_type,
								rd_mess.mB_nr,
								rd_mess.mB_md5); 
						r = (copy_md5)(&rd_mess);
						MUKDEBUG("r(copy_md5): %d\n", r);
						break;					
					}
					else{
						fprintf( stderr,"ERROR Transfer Method - Prymary: %d(dyn=%d), Backup%d\n", r_type, dynup_flag, rd_mess.m_type);
						fflush(stderr);
						exit(1);	
					}
		case DEV_UMD5:
				if (( dynup_flag == DO_DYNUPDATES ) && ( r_type == DEV_CMD5 )) {
						MUKDEBUG("m_type: %d - DEV_UMD5 - DO_DYNUPDATES: %d\n", rd_mess.m_type, dynup_flag );	
						MUKDEBUG("\nRECEIVE: m_source=%d, m_type=%d, POSITION/BLOCK_NR=%u, sigs:%s\n",
								rd_mess.m_source,
								rd_mess.m_type,
								rd_mess.mB_nr,
								rd_mess.mB_md5); 
						r = (copy_md5)(&rd_mess);
						MUKDEBUG("r(copy_md5): %d\n", r);
						break;					
					}
					else{
						fprintf( stderr,"ERROR Transfer Method - Prymary: %d(dyn=%d), Backup%d\n", r_type, dynup_flag, rd_mess.m_type);
						fflush(stderr);
						exit(1);	
					}		
		case DEV_EOF:
					MUKDEBUG("m_type: %d - DEV_EOF\n", rd_mess.m_type);	
					r = (dev_ready)(&rd_mess);
					MUKDEBUG("r(dev_ready): %d\n", r);
					break;							
		case RD_DISK_ERR:			
		case RD_DISK_EOF:
					MUKDEBUG("m_type: %d - RD_DISK_EOF\n", rd_mess.m_type);	
					r = (rd_ready)(&rd_mess);
					MUKDEBUG("r(rd_ready): %d\n", r);
					break;							
		default:		
			MUKDEBUG("Invalid type: %d\n", rd_mess.m_type);
			break;
		}
	
	if (r != EDVSDONTREPLY) { /*para MOL, /kernel/minix/molerrno.h no enviar respuesta (SIGN 201)*/
		 
		if ( r_type != DEV_CMD5 ){
			rd_mess.REP_ENDPT = proc_nr;
			/* Status is # of bytes transferred or error code. */
			rd_mess.REP_STATUS = r;	
			
			MUKDEBUG("SEND msg a DEVICE_CALLER: %d -> m_type=%d, (REP_ENDPT)=%d, (REP_STATUS)=%d\n",
				dev_caller,
				rd_mess.m_type,
				rd_mess.REP_ENDPT,
				rd_mess.REP_STATUS);
		} else{
			MUKDEBUG("SEND msg a DEVICE_CALLER: %d -> m_type=%d, (POSITION/BLOCK_NR)=%d, (SIGS)=%s\n",
				dev_caller,
				rd_mess.m_type,
				rd_mess.mB_nr,
				rd_mess.mB_md5);
		}
			
		ret = muk_send(dev_caller, &rd_mess); /*envío respuesta al cliente q solicitó*/
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
    
  MUKDEBUG("Device size - Primary: %u Backup: %u\n", devvec[mp->DEVICE].st_size, mp->m2_l2);
  if (devvec[mp->DEVICE].st_size != mp->m2_l2) {
	  fprintf(stderr,"ERROR! Device size - Primary: %u Backup: %u\n", devvec[mp->DEVICE].st_size, mp->m2_l2);
	  fflush(stderr);
	  exit(EXIT_FAILURE);
	}
	
  mtr_dev = mp->DEVICE;
  MUKDEBUG("Device to update=%d\n", mtr_dev);

  /*---------- Open image device ---------------*/
  ms_devvec[mp->DEVICE].img_p = open(devvec[mp->DEVICE].img_ptr, O_RDONLY);
  MUKDEBUG("Open imagen FD=%d\n", ms_devvec[mp->DEVICE].img_p);
			
  if(ms_devvec[mp->DEVICE].img_p < 0) {
	MUKDEBUG("img_p=%d\n", ms_devvec[mp->DEVICE].img_p);
	rcode = errno;
	MUKDEBUG("rcode=%d\n", rcode);
	exit(EXIT_FAILURE);
	}

     
  /*---------------- Allocate memory for buffer  ---------------*/
  MUKDEBUG("Buffer - Transfer block: %u\n", mp->COUNT);
  tr_blksize = mp->COUNT; /*genera el buffer a partir de la cantidad de bytes que le indicó el slave*/
  
  posix_memalign( (void **) &buff, getpagesize(), tr_blksize );
  if (buff == NULL) {
  	fprintf(stderr, "posix_memalign\n");
	fflush(stderr);
  	exit(EXIT_FAILURE);
   }
  ms_devvec[mp->DEVICE].localbuff = buff;
  MUKDEBUG("Buffer (block) ms_devvec[%d].localbuff=%p\n", mp->DEVICE, ms_devvec[mp->DEVICE].localbuff);
	
  position = mp->POSITION; /*esta es info que viene inicialmente del slave, no se modifica acá*/
  MUKDEBUG("Init copy of=%X\n", position); 
  
  address_bk = mp->ADDRESS; /*idem position*/
  MUKDEBUG("Buffer backup %X\n", mp->ADDRESS);
  
  mp->m_type=DEV_TRANSR; /*ok for transfer*/
  MUKDEBUG("mp->m_type=DEV_TRANSR %d\n", mp->m_type);
 	
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
  MUKDEBUG("Position %X\n", mp->POSITION);
  
  address_bk = mp->ADDRESS;
  MUKDEBUG("Buffer backup %X\n", mp->ADDRESS);
  
  /*tr_blksize, ya quedó definido anteriormente es la cantidad de bytes que se van a transferir*/
  bytes = pread(ms_devvec[mp->DEVICE].img_p, ms_devvec[mp->DEVICE].localbuff, tr_blksize, position);
  MUKDEBUG("pread: %s\n", ms_devvec[mp->DEVICE].localbuff);
				
  if(bytes < 0) ERROR_TSK_EXIT(errno);
	
  MUKDEBUG("master_ep: %d\n", mc_ep);
  MUKDEBUG("ms_devvec[mp->DEVICE].localbuff: %X\n", ms_devvec[mp->DEVICE].localbuff);
  MUKDEBUG("proc_nr: %d\n", proc_nr);
  MUKDEBUG("address_bk: %X\n", address_bk);
  MUKDEBUG("bytes: %u\n", bytes);
	
  MUK_vcopy(rcode, mc_ep, ms_devvec[mp->DEVICE].localbuff, proc_nr, address_bk, bytes); 
  if (rcode != 0 ) {
	ERROR_TSK_EXIT(errno);
	}
	
   mp->m_type=DEV_CFULLR; /*ok for transfer*/;	
   MUKDEBUG("mp->m_type=DEV_CFULLR %d\n", mp->m_type);
   
   MUKDEBUG("copy_img: MUK_vcopy(copy_img -> copy_img_bk) rcode=%d\n", rcode);  
   MUKDEBUG("bytes= %d\n", bytes);     
   MUKDEBUG("copy - Offset (read) %X, Data: %s\n", ms_devvec[mp->DEVICE].localbuff, ms_devvec[mp->DEVICE].localbuff);			
   
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
  MUKDEBUG("Block_nr %u\n", mp->POSITION);
  
  address_bk = mp->ADDRESS;
  MUKDEBUG("Buffer backup %X\n", mp->ADDRESS);
  
  /*tr_blksize, ya quedó definido anteriormente es la cantidad de bytes que se van a transferir*/
  bytes = pread(ms_devvec[mp->DEVICE].img_p, ms_devvec[mp->DEVICE].localbuff, tr_blksize, ( position * tr_blksize) );
  MUKDEBUG("pread: %s\n", ms_devvec[mp->DEVICE].localbuff);
				
  if(bytes < 0) ERROR_TSK_EXIT(errno);
	
  MUKDEBUG("master_ep: %d\n", mc_ep);
  MUKDEBUG("ms_devvec[mp->DEVICE].localbuff: %X\n", ms_devvec[mp->DEVICE].localbuff);
  MUKDEBUG("proc_nr: %d\n", proc_nr);
  MUKDEBUG("address_bk: %X\n", address_bk);
  MUKDEBUG("bytes: %u\n", bytes);
  MUKDEBUG("Block number: %d\n", position);
  
  rcode = MUK_vcopy(mc_ep, ms_devvec[mp->DEVICE].localbuff, proc_nr, address_bk, bytes); 
	
  if (rcode != 0 ) {
	fprintf( stderr,"VCOPY rcode=%d\n",rcode);
	fflush(stderr);
	exit(1);
	} 	
	
   mp->m_type=DEV_UFULLR; /* replay|*/;	
   MUKDEBUG("mp->m_type=DEV_UFULLR %d\n", mp->m_type);
   
   MUKDEBUG("copy_img: MUK_vcopy(copy_img -> copy_img_bk) rcode=%d\n", rcode);  
   MUKDEBUG("bytes= %d\n", bytes);     
   MUKDEBUG("copy - Offset (read) %X, Data: %s\n", ms_devvec[mp->DEVICE].localbuff, ms_devvec[mp->DEVICE].localbuff);			
   
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
  MUKDEBUG("Block_nr %u\n", mp->mB_nr);
  
  // address_bk = mp->ADDRESS;
  MUKDEBUG("Buffer backup %X\n", address_bk);
  
  MUKDEBUG("mp->mB_md5: %s\n", mp->mB_md5);	
  
  /*tr_blksize, ya quedó definido anteriormente es la cantidad de bytes que se van a transferir*/
  bytes = pread(ms_devvec[mtr_dev].img_p, ms_devvec[mtr_dev].localbuff, tr_blksize, ( position * tr_blksize) );
  MUKDEBUG("pread: %s\n", ms_devvec[mtr_dev].localbuff);
				
  if(bytes < 0) ERROR_TSK_EXIT(errno);
	
  MUKDEBUG("master_ep: %d\n", mc_ep);
  MUKDEBUG("ms_devvec[mp->DEVICE].localbuff: %X\n", ms_devvec[mtr_dev].localbuff);
  MUKDEBUG("proc_nr: %d\n", proc_nr);
  MUKDEBUG("address_bk: %X\n", address_bk);
  MUKDEBUG("bytes: %u\n", bytes);
  MUKDEBUG("Block number: %d\n", position);
  
  MUKDEBUG("md5_compute: fd=%d, buffer=%X, bytes=%u, position=%u\n",
							ms_devvec[mtr_dev].img_p,
							ms_devvec[mtr_dev].localbuff,
							tr_blksize,
							(tr_blksize * position));
							
  md5_compute(ms_devvec[mtr_dev].img_p, ms_devvec[mtr_dev].localbuff, tr_blksize, ( tr_blksize * position), sigm);

  MUKDEBUG("sigm: %s\n", sigm);	
     
  if ( memcmp(mp->mB_md5, sigm, MD5_SIZE) == 0 ) {
  	bytes = 0;
	rcode = 0;
	MUKDEBUG("Block %d matches\n", mp->mB_nr);	
    }
  else{
	MUKDEBUG("Block %d NOT matches\n", mp->mB_nr);
	rcode = MUK_vcopy(mc_ep, ms_devvec[mtr_dev].localbuff, proc_nr, address_bk, bytes); 
	
	if (rcode != 0 ) {
		fprintf( stderr,"VCOPY rcode=%d\n",rcode);
		fflush(stderr);
		exit(1);
		} 	
	}

   
  memcpy(mp->mB_md5, sigm, MD5_SIZE);
  MUKDEBUG("mp->mB_md5: %s\n", mp->mB_md5); 
	
   mp->m_type = ( mp->m_type == DEV_CMD5 )?DEV_CMD5R:DEV_UMD5R;
   MUKDEBUG("mp->m_type=%d (DEV_CMD5R:%d - DEV_UCMD5R:%d\n", mp->m_type,DEV_CMD5R,DEV_UMD5R);
   
   MUKDEBUG("copy_img: MUK_vcopy(copy_img -> copy_img_bk) rcode=%d\n", rcode);  
   MUKDEBUG("bytes= %d\n", bytes);     
   MUKDEBUG("copy - Offset (read) %X, Data: %s\n", ms_devvec[mtr_dev].localbuff, ms_devvec[mtr_dev].localbuff);			
   
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
MUKDEBUG("Close imagen FD=%d\n", ms_devvec[mp->DEVICE].img_p);
			
if ( rcode = close(ms_devvec[mp->DEVICE].img_p) < 0) {
	MUKDEBUG("Error close=%d\n", rcode);
	exit(EXIT_FAILURE);
	}

free(ms_devvec[mp->DEVICE].localbuff);	
MUKDEBUG("check open device\n");
if ( devvec[mp->DEVICE].available == 1){
	MUKDEBUG("devvec[%d].available=%d\n", mp->DEVICE, devvec[mp->DEVICE].available);
	mp->m2_l2 = ( devvec[mp->DEVICE].active == 1 )?1:0;
	MUKDEBUG("mp->m2_l2=%d, devvec[%d].active=%d\n", mp->m2_l2, mp->DEVICE, devvec[mp->DEVICE].active);
	}


mp->m_type=DEV_EOFR; /*ok for transfer*/;	
MUKDEBUG("mp->m_type=DEV_EOF %d\n", mp->m_type);

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
		MUKDEBUG("nuevo nodo: m2_l2 %d\n", mp->m2_l2);
		SET_BIT(bm_nodes, slv_mbr); // PAP 
		active_nr_nodes++;
		MUKDEBUG("active_nr_nodes=%d\n", active_nr_nodes);
		nr_sync++;
		MUKDEBUG("nr_sync=%d\n", nr_sync);
		SET_BIT(bm_sync, mp->m2_l2);
		MUKDEBUG("New sync mbr=%d bm_sync=%X\n", 
			mp->m2_l2 , bm_sync);
		send_status_info();
	}
		
	MUKDEBUG("check open device\n");
	for( i = 0; i < NR_DEVS; i++){
		MUKDEBUG("devvec[%d].available=%d\n", i, devvec[i].available);
		if ( devvec[i].active == 1 ){ /*device open in primary*/ 
			MUKDEBUG("devvec[%d].active=%d\n", i, devvec[i].active);
		}
	}
	
	MUKDEBUG("unbinding RDISK MASTER=%d\n", RDISK_MASTER);
	MUKDEBUG("dcid=%d, mc_ep=%d\n", dcid, mc_ep);
	rcode = muk_unbind(dcid,mc_ep);
	MUKDEBUG("rcode unbind=%d\n", rcode);
	if(rcode < 0) ERROR_TSK_EXIT(rcode);

	MUKDEBUG("unbinding RDISK SLAVE=%d\n", RDISK_SLAVE);
	MUKDEBUG("dcid=%d, sc_ep=%d\n", dcid, sc_ep);
	rcode = muk_unbind(dcid,sc_ep);
	MUKDEBUG("rcode unbind=%d\n", rcode);
	if(rcode < 0) ERROR_TSK_EXIT(rcode);

	if ( dynup_flag == DO_DYNUPDATES ){ 
		COND_SIGNAL(bk_barrier); //MARIE
	}

	MUKDEBUG("Exit mastercopy\n");
	taskexit(NULL);
}	
