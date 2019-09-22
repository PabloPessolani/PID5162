/* This file contains device independent device driver interface.
 *
 * Changes:
 *   Jul 25, 2005   added SYS_SIG type for signals  (Jorrit N. Herder)
 *   Sep 15, 2004   added SYN_ALARM type for timeouts  (Jorrit N. Herder)
 *   Jul 23, 2004   removed kernel dependencies  (Jorrit N. Herder)
 *   Apr 02, 1992   constructed from AT wini and floppy driver  (Kees J. Bot)
 *
 *
 * The drivers support the following operations (using message format m2):
 *
 *    m_type      DEVICE    IO_ENDPT    COUNT    POSITION  ADRRESS
 * ----------------------------------------------------------------
 * |  DEV_OPEN  | device  | proc nr |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_CLOSE | device  | proc nr |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_READ  | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_WRITE | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * | DEV_GATHER | device  | proc nr | iov len |  offset | iov ptr |
 * |------------+---------+---------+---------+---------+---------|
 * | DEV_SCATTER| device  | proc nr | iov len |  offset | iov ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_IOCTL | device  | proc nr |func code|         | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |  CANCEL    | device  | proc nr | r/w     |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  HARD_STOP |         |         |         |         |         |
 * ----------------------------------------------------------------
 *
 *----------------------------------------------------------------------------
 *New operations: compresses or decompresses data
 *----------------------------------------------------------------------------
 *     m_type    | DEVICE  |IO_ENDPT | COUNT   |POSITION | ADRRESS |         | 
 *               |(m2_i1)  |(m2_i2)  |(m2_i3)  |(m2_l1)  | (m2_p1) |(m2_l2)  | 
 * --------------------------------------------------------------------------+
 * |  DEV_CREAD  | device  | proc nr |  bytes  |  offset | buf ptr |bytes    |
 * |------_------+---------+---------+---------+---------+---------+---------+
 * |  DEV_CWRITE | device  | proc nr |  bytes  |  offset | buf ptr |bytes    |
 * |-------_-----+---------+---------+---------+---------+---------+---------+
 * | DEV_CGATHER | device  | proc nr | iov len |  offset | iov ptr |iov len  | 
 * |-------------+---------+---------+---------+---------+---------+---------+
 * | DEV_CSCATTER| device  | proc nr | iov len |  offset | iov ptr |iov len  |
 * |-------------+---------+---------+---------+---------+---------+---------+
 *
 * The file contains one entry point:
 *
 *   driver_task:	called by the device dependent task entry
 */
/*For rd_dtab*/


// #define TASKDBG		1
#define OPER_NAME 0
#define OPER_OPEN 1
#define OPER_NOP 2
#define OPER_IOCTL 3
#define OPER_PREPARE 4
#define OPER_TRANSF 5
#define OPER_CLEAN 6
#define OPER_GEOM 7
#define OPER_SIG 8
#define OPER_ALARM 9
#define OPER_CANC 10
#define OPER_SEL 11

//#include "drivers.h"
#include "rdisk.h"
#include "data_usr.h"

#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

#define BUF_EXTRA	0

//FORWARD _PROTOTYPE( void init_buffer, (void) );
_PROTOTYPE( void init_buffer, (void) );
//FORWARD _PROTOTYPE( int do_rdwt, (struct driver *dr, message *mp) );
_PROTOTYPE( int do_rdwt, (struct driver *dr, message *mp) );
//FORWARD _PROTOTYPE( int do_vrdwt, (struct driver *dr, message *mp) );
_PROTOTYPE( int do_vrdwt, (struct driver *dr, message *mp) );

int local_nodeid;

int device_caller;
 
/*===========================================================================*
 *				driver_task				     *
 *===========================================================================*/
//PUBLIC void driver_task(dp)
void driver_task(dp)
struct driver *dp;	/* Device dependent entry points. */
{
/* Main program of any device driver task. */

  int r, proc_nr;
  message rd_mess;
  int ret; /*nuevas variables*/

  /* Get a DMA buffer. */
  init_buffer();

  	// set the name of RDISK 
	ret = sys_rsetpname(rd_ep,  "rdisk", local_nodeid);
	if(ret < 0) ERROR_TSK_EXIT(ret);
	
  	// SYNCHRONIZE WITH MUK
	MTX_LOCK(muk_mutex);
	COND_SIGNAL(muk_cond);
	COND_WAIT(rd_cond, muk_mutex);
	MTX_UNLOCK(muk_mutex);
	
	MUKDEBUG("This is RDISK's main loop-  get work and do it, forever and forever.\n"); 
 
  /* Here is the main loop of the disk task.  It waits for a message, carries
   * it out, and sends a reply.
   */
	while (TRUE) {

		/* Wait for a request to read or write a disk block. */
		//if (receive(ANY, &rd_mess) != OK) continue;

		/*recibo el mensaje*/

		
		ret = muk_receive( ANY , (long) &rd_mess);
	
		if( ret != OK){
			fprintf( stderr,"muk_receive1 ERROR %d\n",ret); 
			exit(1);
		}
		MUKDEBUG("\nRECEIVE: m_source=%d, m_type=%d, DEVICE=%d, IO_ENDPT=%d, POSITION=%lX, COUNT=%d, ADDRESS:%lX, compress(m2_l2):%ld\n",//pos%u y add %p
			rd_mess.m_source,
			rd_mess.m_type,
			rd_mess.DEVICE,
			rd_mess.IO_ENDPT,
			rd_mess.POSITION,
			rd_mess.COUNT,
			rd_mess.ADDRESS,
			rd_mess.m2_l2); /*if data compress m2_l2=bytes compress*/
		
				
	//MENSAJE TIPO m2
	//*    m_type      DEVICE    IO_ENDPT    COUNT    POSITION  ADRRESS
	//* | DEV_SCATTER| device  | proc nr | iov len |  offset | iov ptr |
	//	m.m_type		m.m2_i1	m.m2_i2		m.m2_i3	 m.m2_l1	m.m2_p1
		
	/*fin de recibir el mensaje*/	

		device_caller = rd_mess.m_source;
		MUKDEBUG("device_caller= %d, rd_mess.m_source= %d\n", device_caller, rd_mess.m_source);
		//proc_nr = rd_mess.IO_ENDPT;
		proc_nr = rd_mess.IO_ENDPT;
	
	/* Now carry out the work. */
		switch(rd_mess.m_type) {
	//case DEV_OPEN:		r = (*dp->dr_open)(dp, &rd_mess);	break;	
			case DEV_OPEN:		MUKDEBUG("m_type: %d - DEV_OPEN\n", rd_mess.m_type);	
						r = (*dp->dr_open)(dp, &rd_mess);
						break;	
	//case DEV_CLOSE:		r = (*dp->dr_close)(dp, &rd_mess);	break;
			case DEV_CLOSE:		MUKDEBUG("m_type: %d - DEV_CLOSE\n", rd_mess.m_type);	
						r = (*dp->dr_close)(dp, &rd_mess);
						break;	
	//case DEV_IOCTL:		r = (*dp->dr_ioctl)(dp, &rd_mess);	break;
			case DEV_IOCTL:		MUKDEBUG("m_type: %d - DEV_IOCTL\n", rd_mess.m_type);	break;	
	//			r = (*dp->dr_geometry)(dp, &rd_mess);break;
			case CANCEL:		MUKDEBUG("m_type: %d - DEV_CANCEL\n", rd_mess.m_type);	break;	
	//case DEV_SELECT:	r = (*dp->dr_select)(dp, &rd_mess);break;
			case DEV_SELECT:	MUKDEBUG("m_type: %d - DEV_SELECT\n", rd_mess.m_type);	break;	
	//case DEV_READ:	
	//case DEV_WRITE:	  	r = do_rdwt(dp, &rd_mess);	break;
			case DEV_READ:	
			case DEV_WRITE:		MUKDEBUG("m_type: %d - (DEV_READ= %d ? WRITE= %d)\n", rd_mess.m_type, DEV_READ, DEV_WRITE);	
						r = do_rdwt(dp, &rd_mess);	
						break;
			case DEV_CREAD:	
			case DEV_CWRITE:	MUKDEBUG("m_type: %d - (DEV_CREAD= %d ? CWRITE= %d)\n", rd_mess.m_type, DEV_CREAD, DEV_CWRITE);	
						r = do_rdwt(dp, &rd_mess);	
						break;
	//case DEV_GATHER: 
	//case DEV_SCATTER: 	r = do_vrdwt(dp, &rd_mess);	break;
			case DEV_GATHER: 
			case DEV_SCATTER:		MUKDEBUG("m_type: %d - (DEV_GATHER= %d ? DEV_SCATTER=%d)\n", rd_mess.m_type, DEV_GATHER, DEV_SCATTER);	
							r = do_vrdwt(dp, &rd_mess); 
							break;
			case DEV_CGATHER: 
			case DEV_CSCATTER:		MUKDEBUG("m_type: %d - (DEV_CGATHER= %d ? DEV_CSCATTER=%d)\n", rd_mess.m_type, DEV_CGATHER, DEV_CSCATTER);	
							//r = do_vrdwt(dp, &rd_mess); 
							break;				
							
	//case HARD_INT:		/* leftover interrupt or expired timer. */
			//	if(dp->dr_hw_int) {
				//	(*dp->dr_hw_int)(dp, &rd_mess);
				//}
				//continue;
			case HARD_INT:		MUKDEBUG("m_type: %d - HARD_INT\n", rd_mess.m_type);	break;	
#ifdef ANULADO 	
			case PROC_EVENT:
	//case SYS_SIG:		(*dp->dr_signal)(dp, &rd_mess);
	//			continue;	/* don't reply */
			case SYS_SIG:		MUKDEBUG("m_type: %d - PROC_EVENT Ó SYS_SIG\n", rd_mess.m_type);	break;	
	
	//case SYN_ALARM:		(*dp->dr_alarm)(dp, &rd_mess);	
	//			continue;	/* don't reply */
			case SYN_ALARM:		MUKDEBUG("m_type: %d - SYN_ALARM\n", rd_mess.m_type);	break;				
				
	//case DEV_PING:		notify(rd_mess.m_source);
	//			continue;
			case DEV_PING:		MUKDEBUG("m_type: %d - DEV_PING\n", rd_mess.m_type);	break;				
#endif // ANULADO 	

			case DEV_DUMP:		
				MUKDEBUG("m_type: %d - DEV_DUMP\n", rd_mess.m_type);	
				r = (*dp->dr_dump)(dp, &rd_mess);	
				break;
				
			default:		
		//if(dp->dr_other)
		//	r = (*dp->dr_other)(dp, &rd_mess);
		//else	
		//	r = EINVAL;
				break;
		}
	
	
	/* Clean up leftover state. */
		(*dp->dr_cleanup)();

	/* Finally, prepare and send the reply message. */
	
	//if (r != EDONTREPLY) {
		if (r != EDVSDONTREPLY) { /*para MOL, /kernel/minix/molerrno.h no enviar respuesta (SIGN 201)*/
			//rd_mess.m_type = TASK_REPLY;
			rd_mess.m_type = MOLTASK_REPLY; /*para MOL, /kernel/minix/callnr.h to FS: reply code from tty task */
		
			rd_mess.REP_ENDPT = proc_nr;
				
			/* Status is # of bytes transferred or error code. */
			rd_mess.REP_STATUS = r;	
			
			MUKDEBUG("SEND msg a DEVICE_CALLER: %d -> m_type=%d, (REP_ENDPT)=%d, (REP_STATUS)=%d\n",
				device_caller,
				rd_mess.m_type,
				rd_mess.REP_ENDPT,
				rd_mess.REP_STATUS);
		
		//send(device_caller, &rd_mess);
			ret = muk_send(device_caller, &rd_mess); /*envío respuesta al cliente q solicitó*/
			if( ret != 0 ) {
				fprintf( stderr,"SEND ret=%d\n",ret);
				exit(1);
			}
		}
	}
}

/*===========================================================================*
 *				init_buffer				     *
 *===========================================================================*/
void init_buffer()
{
/* Select a buffer that can safely be used for DMA transfers.  It may also
 * be used to read partition tables and such.  Its absolute address is
 * 'tmp_phys', the normal address is 'tmp_buf'.
 */

  MUKDEBUG("\n---Init_buffer---\n");
}

/*===========================================================================*
 *				do_rdwt					     *
 *===========================================================================*/
//PRIVATE int do_rdwt(dp, mp)
int do_rdwt(dp, mp)
struct driver *dp;		/* device dependent entry points */
message *mp;			/* pointer to read or write message */
{
/* Carry out a single read or write request. */
  iovec_t iovec1;
  int r, opcode;

  
  MUKDEBUG("mp->COUNT=%u\n", mp->COUNT);
  /* Disk address?  Address and length of the user buffer? */
  if (mp->COUNT < 0) return(EINVAL);
  /*COUNT: cantidad de  bytes*/ 	
  
  /* Check the user buffer. */
  //sys_umap(mp->IO_ENDPT, D, (vir_bytes) mp->ADDRESS, mp->COUNT, &phys_addr);
  //if (phys_addr == 0) return(EFAULT);
  
  /*MOL es virtual, no se realizan conversiones a direcciones*/
  MUKDEBUG("mp->IO_ENDPT=%ld - mp->ADDRESS:%lX - mp->COUNT=%lu\n", mp->IO_ENDPT, (vir_bytes) mp->ADDRESS, mp->COUNT);
  
  /* Prepare for I/O. */
  if ((*dp->dr_prepare)(mp->DEVICE) == NIL_DEV)return(ENXIO);
   
  /* Create a one element scatter/gather vector for the buffer. */
  MUKDEBUG("mp->m_type: %d\n", mp->m_type);
  //flag_buff = mp->m_type;
  //opcode = ( mp->m_type == DEV_READ || mp->m_type == DEV_CREAD) ? DEV_GATHER : DEV_SCATTER;
  switch ( mp->m_type ){
	case DEV_READ:
		opcode = DEV_GATHER;
		break;
	case DEV_CREAD:
		opcode = DEV_CGATHER;
		break;
	case DEV_WRITE:
		opcode = DEV_SCATTER;
		break;
	case DEV_CWRITE:
		opcode = DEV_CSCATTER;
		break;
	default:
	    return(EINVAL);
	}
  
  MUKDEBUG("opcode: %d - DEV_GATHER=%d - DEV_SCATTER=%d\n", opcode, DEV_GATHER, DEV_SCATTER);
  iovec1.iov_addr = (vir_bytes) mp->ADDRESS;
  iovec1.iov_size = mp->COUNT;

  /* Transfer bytes from/to the device. */
  r = (*dp->dr_transfer)(mp->IO_ENDPT, opcode, mp->POSITION, &iovec1, 1);
  mp->m2_l2 = buffer_size;	
  MUKDEBUG("mp->m2_l2 =%d, buffer_size=%d\n", mp->m2_l2, buffer_size);
  MUKDEBUG("dr_trasnfer = (r) %d\n", r);
  if (( r == OK) && ( mp->m_type == DEV_CREAD )){
	fprintf( stderr,"DEV_CREAD is not possible. Bytes:%d <= %d\n",mp->COUNT, devvec[mp->DEVICE].localbuff);
	}
  /* Return the number of bytes transferred or an error code. */
  return(r == OK ? (mp->COUNT - iovec1.iov_size) : r);
}

/*==========================================================================*
 *				do_vrdwt				    *
 *==========================================================================*/
//PRIVATE int do_vrdwt(dp, mp)
int do_vrdwt(dp, mp)
struct driver *dp;	/* device dependent entry points */
message *mp;		/* pointer to read or write message */
{
/* Carry out an device read or write to/from a vector of user addresses.
 * The "user addresses" are assumed to be safe, i.e. FS transferring to/from
 * its own buffers, so they are not checked.
 */
static iovec_t iovec[NR_IOREQS];
iovec_t *iov;
phys_bytes iovec_size;
int nr_req;
int r, ret;

  nr_req = mp->COUNT;	/* Length of I/O vector */
  MUKDEBUG("nr_req: %d, NR_IOREQS=%d\n", nr_req, NR_IOREQS);
  
    /* Copy the vector from the caller to kernel space. */
	/*copio el vector desde el ps que llama al espacio del kernerl:sería del server*/
    if (NR_IOREQS < nr_req) {
		MUKDEBUG("nr_req > NR_IOREQS\n");
		nr_req = NR_IOREQS;
	}
    iovec_size = (phys_bytes) (nr_req * sizeof(iovec[0]));

	//MUK_vcopy(src_ep,src_addr,dst_ep, dst_addr, bytes) 
	MUKDEBUG("proc_nr: %d - driver_ep: %d\n", mp->IO_ENDPT, rd_ptr->p_endpoint);
	MUK_vcopy(ret, mp->IO_ENDPT, mp->ADDRESS, SELF, iovec, iovec_size);
	//muk_copy proc_nr -> server	
	if( ret < 0 ){
		fprintf( stderr,"VCOPY (vector) ret=%d\n",ret);	
		exit(1);
	}	
    iov = iovec;
	
	MUKDEBUG("iovec[0].iov_addr %u\n", iovec[0].iov_addr); 
	MUKDEBUG("iovec[0].iov_size %u\n", iovec[0].iov_size); 
	
    //if (OK != sys_datacopy(mp->m_source, (vir_bytes) mp->ADDRESS, 
    //		SELF, (vir_bytes) iovec, iovec_size))
    //    panic((*dp->dr_name)(),"bad I/O vector by", mp->m_source);
    //iov = iovec;


  /* Prepare for I/O. */
  //if ((*dp->dr_prepare)(mp->DEVICE) == NIL_DEV) return(ENXIO);
    if ((*dp->dr_prepare)(mp->DEVICE) == NIL_DEV) return(ENXIO);
	MUKDEBUG("mp->DEVICE (RAM_DEV=0): %d\n", mp->DEVICE);	

  /* Transfer bytes from/to the device. */
   MUKDEBUG("nr_req: %d\n", nr_req);	
  r = (*dp->dr_transfer)(mp->IO_ENDPT, mp->m_type, mp->POSITION, iov, nr_req);
  //  r = (*dp->dr_transfer)(mp->IO_ENDPT, mp->m_type, mp->m2_l1, iov, nr_req);	
  MUKDEBUG("dr_transfer = (r) %d\n", r);
  /* Copy the I/O vector back to the caller. */

  //sys_datacopy(SELF, (vir_bytes) iovec, 
  //	mp->m_source, (vir_bytes) mp->ADDRESS, iovec_size);
		
	MUK_vcopy(ret, SELF, iovec, mp->IO_ENDPT, mp->ADDRESS, iovec_size);
	//muk_copy server -> proc_nr	
	if( ret < 0 ) {
		fprintf( stderr,"VCOPY (vector) ret=%d\n",ret);		
		exit(1);
	}	
  return(r);
}


/*===========================================================================*
 *				nop_cleanup				     *
 *===========================================================================*/
//PUBLIC void nop_cleanup()
void nop_cleanup()
{
/* Nothing to clean up. */
}

