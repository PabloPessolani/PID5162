/* This file contains the device dependent part of the drivers for the
 * following special files:
 *     /dev/ram		- RAM disk 
 *     /dev/mem		- absolute memory
 *     /dev/kmem	- kernel virtual memory
 *     /dev/null	- null device (data sink)
 *     /dev/boot	- boot device loaded from boot image 
 *     /dev/zero	- null byte stream generator
 *
 *  Changes:
 *	Apr 29, 2005	added null byte generator  (Jorrit N. Herder)
 *	Apr 09, 2005	added support for boot device  (Jorrit N. Herder)
 *	Jul 26, 2004	moved RAM driver to user-space  (Jorrit N. Herder)
 *	Apr 20, 1992	device dependent/independent split  (Kees J. Bot)
 */
#define _GLOBAL_VARS_HERE
#define _MULTI_THREADED
#define _GNU_SOURCE     
#define  MOL_USERSPACE	1
#define _RDISK

#include "rdisk.h"
#include "data_usr.h"
#include "const.h"

int m_device;			/* current device */

// struct partition entry; /*no en código original, pero para completar los datos*/

/* Entry points to this driver. */
	
message mess;
SP_message sp_msg; /*message Spread*/	

unsigned *localbuff;		/* pointer to the first byte of the local buffer (=disk image)*/	

devvec_t *devinfo_ptr;

struct driver m_dtab = {
  m_name,	/* current device's name */
  m_do_open,	/* open or mount */
  m_do_close,	/* nothing on a close */
  do_nop,// m_ioctl,	/* specify ram disk geometry */
  m_prepare,	/* prepare for I/O on a given minor device */
  m_transfer,	/* do the I/O */
  nop_cleanup,	/* no need to clean up */
  m_geometry,	/* memory device "geometry" */
  do_nop, //  nop_signal,	/* system signals */
  do_nop, //  nop_alarm,
  do_nop,	//  nop_cancel,
  do_nop, //  nop_select,
  NULL,
  NULL
};

static int replica_type; 

void usage(void) {
	fprintf(stderr, "Usage: rdisk  -d <dcid> [-e <endpoint>] [-R] [-D] [-Z] [-U {FULL | DIFF }] -c <config_file>\n");
	fprintf(stderr, "\t e: Endpoint number if RDISK is not started by MOL SYSTASK\n");
	fprintf(stderr, "\t R: Replicate (Default: do not replicate)\n");
	fprintf(stderr, "\t D: Dynamic Update (Default: no Dynamic update)\n");
	fprintf(stderr, "\t U: FULL update or DIFFerential update (Default: DIFFerential update)\n");
	fprintf(stderr, "\t Z: Use compression for data transfers during synchronization (Default: do not compress)\n");
	fflush(stderr);
}

void check_same_image( void ) 
{
	/* the same inode of file image*/
	struct stat i_stat, j_stat;
	int rcode;
	int i; /* first device*/
	int j; /* next device*/
		
	for( i = 0 ; i < minor_devs; i++){
		if( devvec[i].available == AVAILABLE_NO) continue;
		rcode = stat(devvec[i].img_ptr, &i_stat);
		TASKDEBUG("stat0 %s rcode=%d\n",devvec[i].img_ptr, rcode);
		if(rcode){
			fprintf( stderr,"\nERROR %d: Device %s minor_number %d is not valid\n", rcode , devvec[minor_devs].dev_name, i );
			fflush(stderr);
			devvec[i].available = AVAILABLE_NO;
			continue;
		}
		
		// get file size and block size 
		if( devvec[i].img_type != NBD_IMAGE ) {
			devvec[i].part.size = i_stat.st_size;
			devvec[i].st_size = i_stat.st_size;			
			TASKDEBUG("dev=%d image size=%ld[bytes] %lld\n", i, i_stat.st_size, devvec[i].part.size);
			devvec[i].st_blksize = i_stat.st_blksize;
			TASKDEBUG("dev=%d block size=%d[bytes] %d\n", i, i_stat.st_blksize, devvec[i].st_blksize);
		}
		
		if( i == (minor_devs-1) ) continue; // last device to check 
		
		for( j = i+1; j < minor_devs; j++){
			if( devvec[j].available == AVAILABLE_NO) continue;
			rcode = stat(devvec[j].img_ptr, &j_stat);
			TASKDEBUG("stat1 %s rcode=%d\n",devvec[j].img_ptr, rcode);
			if(rcode){
				fprintf( stderr,"\nERROR %d: Device %s minor_number %d is not valid\n", rcode, devvec[minor_devs].dev_name, j );
				fflush(stderr);
				devvec[j].available = AVAILABLE_NO;
			}
			TASKDEBUG("devvec[%d].img_ptr=%s,devvec[%d].img_ptr=%s\n", 
				i, devvec[i].img_ptr,j,devvec[j].img_ptr);
	
			if ( i_stat.st_ino == j_stat.st_ino ){
				fprintf( stderr,"\nERROR. Minor numbers %d - %d are the same file\n", i, j );
				fflush(stderr);
				devvec[j].available = AVAILABLE_NO;
				fprintf( stderr,"\nDevice with minor numbers: %d is not available now\n", j );
				fflush(stderr);
			}
		}
	}
}
	
/*===========================================================================*
 *				   main 				     *
 *===========================================================================*/
//PUBLIC int main(void)
int main (int argc, char *argv[] )
{
	/* Main program.*/
	int rcode, c, i, j;  
	char *c_file;
	char cfile_flag;
	
	struct option long_options[] = {
		{ "dcid", 		required_argument, 		NULL, 'd' },
		{ "endpoint", 	required_argument, 		NULL, 'e' },
		{ "config", 	required_argument, 		NULL, 'c' },
		{ "replicate", 	no_argument, 			NULL, 'R' },
		{ "dynamic", 	no_argument, 			NULL, 'D' },
		{ "update", 	required_argument, 		NULL, 'U' },
		{ "compress", 	no_argument, 			NULL, 'Z' },
		{ 0, 0, 0, 0 }, 		
	};
	
	rcode = dvk_open();
	if (rcode < 0)  ERROR_EXIT(rcode);
	
	/* Not availables minor device  */
	for( i = 0; i < NR_DEVS; i++){
		devvec[i].available = AVAILABLE_NO;
		devvec[i].img_fd = (-1); 
		devvec[i].dev_owner = HARDWARE;
	}

	/* flags getopt*/
	cfile_flag 		= NO;
	rd_ep			= HARDWARE; // (-1) 
	dcid			= HARDWARE;
	endp_flag		= NO;

	replicate_opt   = REPLICATE_NO;
	compress_opt    = COMPRESS_NO;
	dynamic_opt     = DYNAMIC_NO;
	update_opt 		= UPDATE_NO;
	
	while((c = getopt_long_only(argc, argv, "e:c:d:RDU:Z:", long_options, NULL)) >= 0) {
		switch(c) {
			case 'c': /*config file*/
				c_file = optarg;
				TASKDEBUG("Option c: %s\n", c_file);
				cfile_flag= YES; 
				break;	
			case 'e': /*endpoint number- not Started by MoL */
				rd_ep 		= atoi(optarg);
				endp_flag 	= YES;
				TASKDEBUG("Option e: %s rd_ep=%d\n", optarg, rd_ep);
				break;
			case 'd': /*DC ID - not Started by MoL */
				dcid = atoi(optarg);
				TASKDEBUG("Option d: %s dcid=%d\n", optarg, dcid);
				break;
			case 'R': 
				replicate_opt =  REPLICATE_YES;
				TASKDEBUG("Option R: replicate_opt=REPLICATE_YES\n");
				break;
			case 'D': 
				dynamic_opt =  DYNAMIC_YES;
				TASKDEBUG("Option D: dynamic_opt=DYNAMIC_YES\n");
				break;
			case 'Z': 
				compress_opt =  COMPRESS_YES;
				TASKDEBUG("Option Z: compress_opt=COMPRESS_YES\n");
				break;
			case 'U': 
				if( !strncmp(optarg,"FULL",4)){
					update_opt =  UPDATE_FULL;
					TASKDEBUG("Option U: update_opt=UPDATE_FULL\n");
				} else  if( !strncmp(optarg,"DIFF",4)){
					update_opt =  UPDATE_DIFF;
					TASKDEBUG("Option U: update_opt=UPDATE_DIFF\n");
				} else {
					usage();
					exit(EXIT_FAILURE);
				}
				break;
			default:
				usage();
				exit(EXIT_FAILURE);
			}
		}	
 	
	if ( cfile_flag == NO)  {
 	    usage();
		exit(1);
    }

	if( dcid == HARDWARE){
 	    usage();
		exit(1);
    }
	
	parse_config(c_file);
		
	if (minor_devs == 0){
		fprintf( stderr,"\nERROR. No availables devices in %s\n", c_file );
		fflush(stderr);
		exit(1);
	}
	TASKDEBUG("minor_devs=%d\n",minor_devs);
	
	check_same_image();

	rcode = rd_init();
	if(rcode) ERROR_RETURN(rcode);  
	driver_task(&m_dtab);	
  
	return(OK);				
}

/*===========================================================================*
 *				 m_name					     *
 *===========================================================================*/
//PRIVATE char *m_name()
char *m_name()
{
/* Return a name for the current device. */
  //static char name[] = "memory";
  static char name[] = "rd_driver";
  TASKDEBUG("n_name(): %s\n", name);
 		
  return name;  
}

/*===========================================================================*
 *				m_prepare				     *
 *===========================================================================*/
//PRIVATE struct device *m_prepare(device)
struct device *m_prepare(device)
int device;
{
	devvec_t *dv_ptr;
/* Prepare for I/O on a device: check if the minor device number is ok. */

	TASKDEBUG("device = %d\n", device);
 	if (device < 0 || device >= NR_DEVS || devvec[device].active_flag < 1) {
		TASKDEBUG("Error en m_prepare\n");
		return(NIL_DEV);
		}

	get_geometry(device);
  
  return(&devvec[device]);
 
}

/*===========================================================================*
 *				m_transfer				     *
 *===========================================================================*/
//PRIVATE int m_transfer(proc_nr, opcode, position, iov, nr_req)
int m_transfer(proc_nr, opcode, position, iov, nr_req)
int proc_nr;			/* process doing the request */
int opcode;			/* DEV_GATHER or DEV_SCATTER */
off_t position;			/* offset on device to read or write */
iovec_t *iov;			/* pointer to read or write request vector */
unsigned nr_req;		/* length of request vector */
{
	/* Read or write one the driver's minor devices. */
//	phys_bytes mem_phys;
	unsigned count, tbytes, stbytes, bytes, count_s, bytes_c; //left, chunk; 
	vir_bytes user_vir, addr_s;
	struct device *dv;
	unsigned long dv_size;
	int rcode;
	off_t posit;
	message msg;
	
	tbytes = 0;
	bytes = 0;
	bytes_c = 0;
	
	TASKDEBUG("m_device: %d\n", m_device); 
	
//	if ( m_ptr->m_source != devvec[m_ptr->DEVICE].dev_owner)
//		ERROR_RETURN(EDVSBADOWNER);
	
	if (devvec[m_device].active_flag < 1) { /*minor device active_flag must be -1-*/
		TASKDEBUG("Minor device = %d\n is not active_flag", m_device);
		ERROR_RETURN(EDVSNODEV);	
	}
	
	/* Get minor device number and check for /dev/null. */
//	dv = &m_geom[m_device];
	dv_size = devvec[m_device].st_size; 
	TASKDEBUG("dv_size: %d\n", dv_size);	
	
	posit = position;
	TASKDEBUG("posit: %X\n", posit);	
	TASKDEBUG("nr_req: %d\n", nr_req);	

	while (nr_req > 0) { /*2*/
	  
		/* How much to transfer and where to / from. */
		count = iov->iov_size;
		TASKDEBUG("count: %u\n", count);	
	
		user_vir = iov->iov_addr;
		addr_s = iov->iov_addr;
		TASKDEBUG("user_vir %X\n", user_vir);	
		
		if (position >= dv_size) {
			TASKDEBUG("EOF\n"); 
			return(OK);
		} 	/* check for EOF */
			
		if (position + count > dv_size) { 
			count = dv_size - position; 
			TASKDEBUG("count dv_size-position: %u\n", count); 
		}

//		mem_phys = devvec[m_device].part.base + position;
//		TASKDEBUG("DRIVER - position I/O(mem_phys) %X\n", mem_phys);
		
		if ((opcode == DEV_GATHER) ||(opcode == DEV_CGATHER))  {/* copy data */ /*DEV_GATHER read from an array (com.h)*/
		
			TASKDEBUG("\n<DEV_GATHER>\n");
				
			stbytes = 0;
			do	{
				/* read to the virtual disk-file- into the buffer --> to the FS´s buffer*/
				bytes = (count > devvec[m_device].buff_size)?devvec[m_device].buff_size:count;
				TASKDEBUG("bytes: %d\n", bytes);		
			
				/* read data from the virtual device file into the local buffer  */			
				bytes = pread(devvec[m_device].img_fd, devvec[m_device].localbuff, bytes, position);
				TASKDEBUG("pread: bytes=%d\n", bytes);
				
				if(bytes < 0) ERROR_RETURN(errno);
				
				if ( opcode == DEV_CGATHER ) {
			
					TASKDEBUG("Compress data for to the requester process\n");
					
					/*compress data buffer*/
										
					TASKDEBUG("lz4_data_cd (in_buffer=%X, inbuffer_size=%d, condition UNCOMP =%d\n",
						devvec[m_device].localbuff,bytes,UNCOMP);
					
					lz4_data_cd(devvec[m_device].localbuff, bytes, UNCOMP);
					
					buffer_size = msg_lz4cd.buf.buffer_size;
					TASKDEBUG("buffer_size =%d\n", buffer_size);
					
					memcpy(devvec[m_device].localbuff, msg_lz4cd.buf.buffer_data, buffer_size);
					TASKDEBUG("buffer_data =%s\n", devvec[m_device].localbuff);
					
					mess.m2_l2 = buffer_size;	
					TASKDEBUG("mess.m2_l2 =%d\n", mess.m2_l2);
					
					/* copy the data from the local buffer to the requester process address space in other DC - compress data */
					rcode = dvk_vcopy(rd_ep, devvec[m_device].localbuff, proc_nr, user_vir, buffer_size);
					/*END compress data buffer*/
								
				}else{
					
					/* copy the data from the local buffer to the requester process address space in other DC */
					rcode = dvk_vcopy(rd_ep, devvec[m_device].localbuff, proc_nr, user_vir, bytes); 
				}
				
				
				TASKDEBUG("DRIVER: dvk_vcopy(DRIVER -> proc_nr) rcode=%d\n", rcode);  
				TASKDEBUG("bytes= %d\n", bytes);
				TASKDEBUG("DRIVER - Offset (read) %X\n", devvec[m_device].localbuff);		
//				TASKDEBUG("mem_phys: %X (in DRIVER)\n", devvec[m_device].localbuff);			
				TASKDEBUG("user_vir: %X (in proc_nr %d)\n", user_vir, proc_nr);			
			
				if (rcode < 0 ) {
					fprintf( stderr,"dvk_vcopy rcode=%d\n",rcode);
					fflush(stderr);
					ERROR_RETURN(rcode);
				}

				stbytes += bytes; /*total bytes transfers*/								
				position += bytes;
				iov->iov_addr += bytes;

				user_vir = iov->iov_addr;
				TASKDEBUG("user_vir (updated) %X\n", user_vir);	

				count -= bytes;
				TASKDEBUG("count=%d stbytes=%d position=%ld\n", count, stbytes, position);	

				} while(count > 0);
			/* END DEV_GATHER*/
			
		} else { /*DEV_SCATTER write from an array*/

			TASKDEBUG("\n<DEV_SCATTER>\n");
			
			stbytes = 0;
			TASKDEBUG("\dc_ptr->dc_nr_nodes=%d, nr_nodes=%d\n",dc_ptr->dc_nr_nodes, nr_nodes);

			if (replica_type == REPLICATE_YES){
				
				TASKDEBUG("DO REPLICATE\n");
				if(primary_mbr == local_nodeid) {
					
					count_s = iov->iov_size; /*PRYMARY: bytes iniciales de c/posición del vector a copiar, ver no sé para q lo voy a usar*/
					TASKDEBUG("count_s: %u\n", count_s);	
					
					nr_optrans = 0;
					TASKDEBUG("nr_optrans: %d\n", nr_optrans); /*para enviar rta por cada nr_req*/
					
					//pthread_mutex_lock(&rd_mutex);			
					MTX_LOCK(rd_mutex);
					TASKDEBUG("\n<LOCK x nr_req=%d>\n", nr_req);
					
					do {
						/* from to buffer RDISK -> to local buffer and write into the file*/
							
						bytes = (count > devvec[m_device].buff_size)?devvec[m_device].buff_size:count;
						TASKDEBUG("bytes: %d\n", bytes);
					
						TASKDEBUG("WRITE - CLIENT TO PRIMARY\n");
						TASKDEBUG("proc_rn= %d\n", proc_nr);  
						TASKDEBUG("user_vir= %X\n", user_vir);     
						TASKDEBUG("rd_ep=%d\n", rd_ep);			
						TASKDEBUG("localbuff: %X\n", devvec[m_device].localbuff);			
					
						/* copy the data from the requester process address space in other DC  to the local buffer */
						rcode = dvk_vcopy(proc_nr, user_vir, rd_ep, devvec[m_device].localbuff, bytes); /*escribo bufferFS -> bufferlocal*/
						
						TASKDEBUG("DRIVER: dvk_vcopy(proc_nr -> DRIVER)= %d\n", rcode);  
						TASKDEBUG("bytes= %d\n", bytes);     
//						TASKDEBUG("mem_phys: %X (in DRIVER)\n", devvec[m_device].localbuff);			
						TASKDEBUG("user_vir: %X (in proc_nr %d)\n", user_vir, proc_nr);			
									
						if (rcode < 0 ){
							fprintf(stderr, "VCOPY rcode=%d\n", rcode);
							fflush(stderr);
							ERROR_RETURN(rcode);
							break;
						}else{
							stbytes = stbytes + bytes; /*si dvk_vcopy fue exitosa, devuelve cantidad de bytes transferidos*/
						}		
						
						/* write data from local buffer to the  virtual device file */
				
						TASKDEBUG("devvec[m_device].img_fd=%d, devvec[m_device].localbuff=%X, bytes=%d, position=%u\n", 
							devvec[m_device].img_fd, devvec[m_device].localbuff, bytes, position);			

								
						bytes = pwrite(devvec[m_device].img_fd, devvec[m_device].localbuff, bytes, position);
						TASKDEBUG("buffer: %s\n", devvec[m_device].localbuff);		
						
						if ( bytes == (-1) ){ 
							TASKDEBUG("pwrite: %d\n", bytes);
							ERROR_RETURN(errno);
						}	
						TASKDEBUG("pwrite: %d\n", bytes);
						
							
						if (opcode == DEV_SCATTER) { /*uncompress*/
							
							TASKDEBUG("NOT COMPRESS DATA BUFFER BEFORE BROADCAST\n");
						
							sp_msg.msg.m_type = DEV_SCATTER; /*VER ESTO*/
							
							memcpy(sp_msg.buf.buffer_data,devvec[m_device].localbuff,bytes); 
							sp_msg.buf.buffer_size = bytes;
							TASKDEBUG("sizeof(sp_msg.buff) %d, %u\n", sp_msg.buf.buffer_size, sizeof(sp_msg.buf.buffer_data));		
							TASKDEBUG("buffer: %s\n", sp_msg.buf.buffer_data);		
							
						}else { /*compress*/
						
							TASKDEBUG("COMPRESS DATA BUFFER BEFORE BROADCAST\n");
							
							sp_msg.msg.m_type = DEV_CSCATTER;
														
							TASKDEBUG("lz4_data_cd (in_buffer=%X, inbuffer_size=%d, condition UNCOMP =%d\n",
									devvec[m_device].localbuff,bytes,UNCOMP);
						
							lz4_data_cd(devvec[m_device].localbuff, bytes, UNCOMP);
						
							sp_msg.buf.flag_buff = msg_lz4cd.buf.flag_buff;
							TASKDEBUG("sp_msg.buf.flag_buff =%d\n", sp_msg.buf.flag_buff);
							
							sp_msg.buf.buffer_size = msg_lz4cd.buf.buffer_size;
							TASKDEBUG("sp_msg.buf.buffer_size =%d\n", sp_msg.buf.buffer_size);
							
							memcpy(sp_msg.buf.buffer_data, msg_lz4cd.buf.buffer_data, sp_msg.buf.buffer_size);

						} 
						
						sp_msg.msg.m_source = local_nodeid;			/* this is the primary */
						
						sp_msg.msg.DEVICE = m_device;
						TASKDEBUG("sp_msg.msg.DEVICE= %d\n", sp_msg.msg.DEVICE);
						
						sp_msg.msg.IO_ENDPT = proc_nr; /*process number = m_source, original message*/
						
						sp_msg.msg.POSITION = position;
						TASKDEBUG("(Armo) sp_msg.msg.POSITION %X\n", sp_msg.msg.POSITION);	
						
						sp_msg.msg.COUNT = bytes; /*por ahora sólo los bytes=vcopy; pero ver? - sólo los bytes q escribí*/
						TASKDEBUG("sp_msg.msg.COUNT %u %d\n", sp_msg.msg.COUNT, sp_msg.msg.COUNT);	
						
						sp_msg.msg.ADDRESS = addr_s; /*= iov->iov_addr; address del cliente */
						
						sp_msg.msg.m2_l2 = ( count > 0 )?nr_optrans:0; /*se usa este campo para saber el número de operaciones*/
						
						bytes_c = sp_msg.buf.buffer_size + sizeof(int) + sizeof(long);
						TASKDEBUG("bytes_c=%d\n", bytes_c);
						
						TASKDEBUG("sp_msg replica m_source=%d, m_type=%d, DEVIDE=%d, IO_ENDPT=%d, POSITION=%X, COUNT=%u, ADDRESS=%X, nr_optrans=%d, BYTES_COMPRESS=%d\n", 
								  sp_msg.msg.m_source, 
								  sp_msg.msg.m_type, 
								  sp_msg.msg.DEVICE, 
								  sp_msg.msg.IO_ENDPT, 
								  sp_msg.msg.POSITION, 
								  sp_msg.msg.COUNT, 
								  sp_msg.msg.ADDRESS, 
								  sp_msg.msg.m2_l2, 
								  sp_msg.buf.buffer_size); 
							
						TASKDEBUG("broadcast x cada vcopy\n");
						rcode = SP_multicast (sysmbox, SAFE_MESS, (char *) rdisk_group,  
								DEV_WRITE, (sizeof(message) + bytes_c), (char *) &sp_msg); 
						
						TASKDEBUG("SP_multicast mensaje enviado\n");
						
						if(rcode) {
							// pthread_mutex_unlock(&rd_mutex);	
							MTX_UNLOCK(rd_mutex);
							ERROR_RETURN(rcode);
						}
						
						nr_optrans++;
						TASKDEBUG("Operaciones de transferencias de bytes (cantidad vcopy)= %d\n", nr_optrans);
												
						position += bytes;
						iov->iov_addr += bytes;
						
						user_vir = iov->iov_addr;
						TASKDEBUG("user_vir (do-buffer) %X count %d bytes %d\n", user_vir, count, bytes);	
						
						count -= bytes;
						TASKDEBUG("count=%d stbytes=%d position=%ld\n", count, stbytes, position);							

							
					} while(count > 0);

					// pthread_cond_wait(&update_barrier,&rd_mutex); /*wait until  the process will be the PRIMARY  */	
					COND_WAIT(update_barrier, rd_mutex);

					// pthread_mutex_unlock(&rd_mutex);			
					MTX_UNLOCK(rd_mutex);
					/* FIN - DESDE EL BUFFER DEL RDISK -> AL BUFFER LOCAL Y ESCRIBIR EN EL ARCHIVO*/
				}else{
					TASKDEBUG("WRITE <bytes=%d> <position=%X > <nr_optrans=%d> <nr_req=%d>\n", 
							  count,
							  position,	
							  nr_optrans,
							  nr_req);			
				
					stbytes = stbytes + count; /*sólo acumulo el total de bytes, escribí todo de una vez*/
					TASKDEBUG("BACKUP REPLY: %d\n", nr_optrans);			
				
					if (nr_optrans == 0) {
							
						TASKDEBUG("BACKUP multicast DEV_WRITE REPLY to %d nr_req=%d\n",
							primary_mbr,nr_req);
					
						msg.m_source= local_nodeid;			
						msg.m_type 	= MOLTASK_REPLY;
						rcode = SP_multicast (sysmbox, SAFE_MESS, (char *) rdisk_group,  
								MOLTASK_REPLY, sizeof(message), (char *) &msg); 
			
						if(rcode) ERROR_RETURN(rcode);
						CLR_BIT(bm_acks, primary_mbr);
					}
				}
			}else{
				/*NOT REPLICATE*/
				TASKDEBUG("NOT REPLICATE\n");
				
				nr_optrans = 0;
				TASKDEBUG("nr_optrans: %d\n", nr_optrans); /*para enviar rta por cada nr_req*/
															
				do {
					/* from to buffer RDISK -> to local buffer and write into the file*/
							
					bytes = (count > devvec[m_device].buff_size)?devvec[m_device].buff_size:count;
					TASKDEBUG("bytes: %d\n", bytes);
					
					TASKDEBUG("WRITE - CLIENT TO PRIMARY\n");
					TASKDEBUG("proc_rn= %d\n", proc_nr);  
					TASKDEBUG("user_vir= %X\n", user_vir);     
					TASKDEBUG("rd_ep=%d\n", rd_ep);			
					TASKDEBUG("localbuff: %X\n", devvec[m_device].localbuff);			
					
					/* copy the data from the requester process address space in other DC  to the local buffer */
					rcode = dvk_vcopy(proc_nr, user_vir, rd_ep, devvec[m_device].localbuff, bytes); /*escribo bufferFS -> bufferlocal*/
						
					TASKDEBUG("DRIVER: dvk_vcopy(proc_nr -> DRIVER)= %d\n", rcode);  
					TASKDEBUG("bytes= %d\n", bytes);     
					TASKDEBUG("mem_phys: %X (in DRIVER)\n", devvec[m_device].localbuff);			
					TASKDEBUG("user_vir: %X (in proc_nr %d)\n", user_vir, proc_nr);			
									
					if (rcode < 0 ){
						fprintf(stderr, "VCOPY rcode=%d\n", rcode);
						fflush(stderr);
						break;
					}else{
						stbytes = stbytes + bytes; /*si dvk_vcopy fue exitosa, devuelve cantidad de bytes transferidos*/
						}		
					
					
					if ( opcode == DEV_SCATTER) { /*uncompress*/
			
						/* write data from local buffer to the  virtual device file */
						TASKDEBUG("NOT COMPRESS DATA\n");
				
						TASKDEBUG("devvec[m_device].img_fd=%d, devvec[m_device].localbuff=%X, bytes=%d, position=%u\n", 
							devvec[m_device].img_fd, devvec[m_device].localbuff, bytes, position);			
						
						bytes = pwrite(devvec[m_device].img_fd, devvec[m_device].localbuff, bytes, position);
						
						TASKDEBUG("pwrite: %d\n", bytes);
						
						if( bytes < 0) ERROR_RETURN(errno);	
							
					}else { /*compress*/
						/*FS solicita que los datos que escriba en el dispositivo estén comprimidos*/
						TASKDEBUG("WRITE COMPRESS DATA (DEV_CWRITE)\n");
																
						/*compress data*/
						TASKDEBUG("lz4_data_cd (in_buffer=%X, inbuffer_size=%d, condition UNCOMP =%d\n",
									devvec[m_device].localbuff,bytes,UNCOMP);
						
						lz4_data_cd(devvec[m_device].localbuff, bytes, UNCOMP);
						
						sp_msg.buf.flag_buff = msg_lz4cd.buf.flag_buff;
						TASKDEBUG("sp_msg.buf.flag_buff =%d\n", sp_msg.buf.flag_buff);
							
						sp_msg.buf.buffer_size = msg_lz4cd.buf.buffer_size;
						TASKDEBUG("sp_msg.buf.buffer_size =%d\n", sp_msg.buf.buffer_size);
						
						bytes_c = pwrite(devvec[m_device].img_fd, msg_lz4cd.buf.buffer_data, sp_msg.buf.buffer_size, position);
						
						TASKDEBUG("pwrite: %d\n", bytes_c);
						
						TASKDEBUG("bytes: %d\n", bytes); /*no se modifica, pero es el contabiliza para count > 0)*/
						
						if(bytes_c < 0) ERROR_RETURN(errno);	
							
					} 
						

					nr_optrans++;
					TASKDEBUG("Operaciones de transferencias de bytes (cantidad vcopy)= %d\n", nr_optrans);
								
					position += bytes;
					iov->iov_addr += bytes;
						
					user_vir = iov->iov_addr;
					TASKDEBUG("user_vir (do-buffer) %X count %d bytes %d\n", user_vir, count, bytes);	
						
					count -= bytes;
					TASKDEBUG("count=%d stbytes=%d position=%ld\n", count, stbytes, position);	
					
					} while(count > 0);
					
			}
			
		}
		/* Book the number of bytes transferred. Registra el número de bytes transferidos? */
		TASKDEBUG("subtotal de bytes\n");	
		if ((iov->iov_size -= stbytes) == 0) { iov++; nr_req--; }  /*subtotal bytes, por cada iov_size según posición del vector*/
		
		tbytes += stbytes; /*total de bytes leídos o escritos*/
	}
	
	return(tbytes);
}

/*===========================================================================*
 *				m_do_open				     *
 *===========================================================================*/
int m_do_open(struct driver *dp, message *m_ptr) 
{
	int rcode;
	message msg;
	
	TASKDEBUG("m_do_open - device number: %d - OK to open\n", m_ptr->DEVICE);

//	if ( m_ptr->m_source == devvec[m_ptr->DEVICE].dev_owner)
//		ERROR_RETURN(OK);
	
//	if ( devvec[m_ptr->DEVICE].dev_owner != HARDWARE) 
//		ERROR_RETURN(EDVSBUSY);
	
	rcode = OK;
	TASKDEBUG("m_ptr->DEVICE=%d %s\n", m_ptr->DEVICE, devvec[m_ptr->DEVICE].img_ptr);
	do {
		if ( devvec[m_ptr->DEVICE].available == AVAILABLE_NO ){
			TASKDEBUG("devvec[m_ptr->DEVICE].available=%d\n", devvec[m_ptr->DEVICE].available);
			rcode = errno;
			ERROR_RETURN(rcode);
		}
			
		devvec[m_ptr->DEVICE].img_fd = open(devvec[m_ptr->DEVICE].img_ptr, O_RDWR);
		TASKDEBUG("Open imagen FD=%d\n", devvec[m_ptr->DEVICE].img_fd);
		if(devvec[m_ptr->DEVICE].img_fd < 0) {
			TASKDEBUG("devvec[m_ptr->DEVICE].img_fd=%d\n", devvec[m_ptr->DEVICE].img_fd);
			rcode = errno;
			ERROR_RETURN(rcode);
		}
		devvec[m_ptr->DEVICE].dev_owner =  m_ptr->m_source; 
		
		/* local buffer to the minor device */
		rcode = posix_memalign( (void**) &localbuff, getpagesize(), devvec[m_ptr->DEVICE].buff_size);
		devvec[m_ptr->DEVICE].localbuff = localbuff;
		if( rcode) {
			fprintf(stderr,"posix_memalign rcode=%d, device=%d\n", rcode, m_ptr->DEVICE);
			fflush(stderr);
			exit(1);
		}
		
		TASKDEBUG("Aligned Buffer size=%d on address %X, device=%d\n", devvec[m_ptr->DEVICE].buff_size, devvec[m_ptr->DEVICE].localbuff, m_ptr->DEVICE);
		TASKDEBUG("Local Buffer %X\n", devvec[m_ptr->DEVICE].localbuff);
		TASKDEBUG("Buffer size %d\n", devvec[m_ptr->DEVICE].buff_size);
			
		devvec[m_ptr->DEVICE].active_flag += 1;
		TASKDEBUG("Device %d is active_flag %d\n", m_ptr->DEVICE, devvec[m_ptr->DEVICE].active_flag);
		
		/* Check device number on open. */
		if (m_prepare(m_ptr->DEVICE) == NIL_DEV) {
			TASKDEBUG("'m_prepare()' %d - NIL_DEV:%d\n", m_prepare(m_ptr->DEVICE), NIL_DEV);
			rcode = ENXIO;
			ERROR_RETURN(rcode);
		}
 	
	}while(0);
	

   	if( replica_type == REPLICATE_YES ) { /* PRIMARY;  MULTICAST to other nodes the device operation */
		if(primary_mbr == local_nodeid) {
			TASKDEBUG("PRIMARY multicast DEV_OPEN dev=%d\n", m_ptr->DEVICE);
			
			if(rcode< 0) ERROR_RETURN(rcode);
			msg.m_source= local_nodeid;			/* this is the primary */
			msg.m_type 	= DEV_OPEN;
			msg.m2_i1	= m_ptr->DEVICE;
			
			// pthread_mutex_lock(&rd_mutex);
			MTX_LOCK(rd_mutex); 
			
			rcode = SP_multicast (sysmbox, SAFE_MESS, (char *) rdisk_group,  
						DEV_OPEN, sizeof(message), (char *) &msg); 
						
			if(rcode) {
				// pthread_mutex_unlock(&rd_mutex);	
				MTX_UNLOCK(rd_mutex);
				ERROR_RETURN(rcode);
			}
			
			
			// pthread_cond_wait(&update_barrier,&rd_mutex); /*wait until  the process will be the PRIMARY  */	
			COND_WAIT(update_barrier,rd_mutex);
			// pthread_mutex_unlock(&rd_mutex);	
			MTX_UNLOCK(rd_mutex);
			
			rcode = OK;
			TASKDEBUG("END PRIMARY\n");
		} else { 	/*  BACKUP:   MULTICAST to PRIMARY the ACKNOWLEDGE  */
			TASKDEBUG("BACKUP multicast DEV_OPEN REPLY to %d rcode=%d\n", primary_mbr ,rcode);
			
			
			msg.m_source= local_nodeid;			
			msg.m_type 	= MOLTASK_REPLY;
			msg.m2_i1	= m_ptr->DEVICE;
			msg.m2_i2	= DEV_OPEN;
			msg.m2_i3	= rcode;
			rcode = SP_multicast (sysmbox, SAFE_MESS, (char *) rdisk_group,  
						MOLTASK_REPLY, sizeof(message), (char *) &msg); 
						
			if(rcode) ERROR_RETURN(rcode);
			TASKDEBUG("bm_acks=%d\n", bm_acks); 
			CLR_BIT(bm_acks, primary_mbr);
			TASKDEBUG("bm_acks=%d\n", bm_acks);
		}
		
	}
  
  devinfo_ptr  = &devvec[m_ptr->DEVICE];
  TASKDEBUG(DEV_USR1_FORMAT,DEV_USR1_FIELDS(devinfo_ptr));
	
  TASKDEBUG("END m_do_open\n");  return(rcode);
}

/*===========================================================================*
 *				rd_init					     *
 *===========================================================================*/
//PRIVATE void rd_init()
int rd_init(void )
{
	int rcode, i;

 	rd_lpid = getpid();
	
	/* NODE info */
	local_nodeid = dvk_getdvsinfo(&dvs);
	if(local_nodeid < 0 )
		ERROR_EXIT(EDVSDVSINIT);
	dvs_ptr = &dvs;
	TASKDEBUG(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));
	TASKDEBUG("local_nodeid=%d\n", local_nodeid);
	
	TASKDEBUG("Get the DC info\n");
	rcode = dvk_getdcinfo(dcid, &dcu);
	if(rcode < 0) ERROR_EXIT(rcode);
	dc_ptr = &dcu;
	TASKDEBUG(DC_USR1_FORMAT,DC_USR1_FIELDS(dc_ptr));
	TASKDEBUG(DC_USR2_FORMAT,DC_USR2_FIELDS(dc_ptr));

	TASKDEBUG("Get RDISK info\n");
	if( rd_ep == HARDWARE) {
		rd_ep = RDISK_PROC_NR;
		TASKDEBUG("rd_ep=%d\n", rd_ep);
	}
	
	rd_ptr = &proc_rd;
	rcode = dvk_getprocinfo(dcid, rd_ep, rd_ptr);
	if(rcode < 0 ) ERROR_EXIT(rcode);
	TASKDEBUG("BEFORE " PROC_USR_FORMAT,PROC_USR_FIELDS(rd_ptr));
	
	if( replicate_opt == REPLICATE_NO) { // WITHOUT REPLICATION 
		TASKDEBUG("Starting single RDISK\n");
		nr_nodes = 1;
		TASKDEBUG("nr_nodes=%d\n", nr_nodes);

		// RDISK not bound by SYSTASK 
		if( TEST_BIT(rd_ptr->p_rts_flags, BIT_SLOT_FREE)) {
			rcode = dvk_bind(dcid, rd_ep);
			if(rcode != rd_ep ) ERROR_EXIT(rcode);					
			if (endp_flag == NO) { // Started by MoL 
				rcode = sys_bindproc(rd_ep, rd_lpid, LCL_BIND);
				if(rcode < 0) ERROR_EXIT(rcode);						
			} 
		}
		rcode = dvk_getprocinfo(dcid, rd_ep, &proc_rd);
		if(rcode < 0) ERROR_EXIT(rcode);
		TASKDEBUG("AFTER  " PROC_USR_FORMAT,PROC_USR_FIELDS(rd_ptr));
	} else {	
		TASKDEBUG("Initializing RDISK REPLICATED\n");
		rcode = init_replicate();	
		if( rcode)ERROR_EXIT(rcode);
		
		TASKDEBUG("Starting REPLICATE thread\n");
		rcode = pthread_create( &replicate_thread, NULL, replicate_main, 0 );
		if( rcode )ERROR_EXIT(rcode);
		
		// Wait until this process will be the primary 
		MTX_LOCK(rd_mutex);
		while (  primary_mbr != local_nodeid) {
			TASKDEBUG("wait until this process will be the PRIMARY\n");
			COND_WAIT(rd_barrier,rd_mutex);
			
			TASKDEBUG("RDISK has been signaled by the REPLICATE thread  FSM_state=%d\n",  FSM_state);
			if( FSM_state == STS_LEAVE) {	/* An error occurs trying to join the spread group */
				MTX_UNLOCK(rd_mutex);
				ERROR_RETURN(EDVSCONNREFUSED);
			}	

			rcode = dvk_getprocinfo(dcid, rd_ep, rd_ptr);
			if(rcode < 0 ) ERROR_EXIT(rcode);
			TASKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(rd_ptr));
		
			TASKDEBUG("Replicated driver. nr_nodes=%d primary_mbr=%d\n",  nr_nodes, primary_mbr);
			TASKDEBUG("primary_mbr=%d - local_nodeid=%d\n", primary_mbr, local_nodeid);
			if ( primary_mbr != local_nodeid) {
				// if RDISK is not bound, bound it as a BACKUP
				if( TEST_BIT(rd_ptr->p_rts_flags, BIT_SLOT_FREE)) {
					rcode = dvk_bkupbind(dcid,rd_lpid,rd_ep,primary_mbr);
					if(rcode != rd_ep ) ERROR_EXIT(rcode);
				} else {
					if( rd_ptr->p_nodeid != primary_mbr) {	// else Migrate it 
						TASKDEBUG("RDISK endpoint %d\n", rd_ep);
						rcode = dvk_migr_start(dc_ptr->dc_dcid, rd_ep);
						TASKDEBUG("dvk_migr_start rcode=%d\n",	rcode);
						rcode = dvk_migr_commit(rd_lpid, dc_ptr->dc_dcid, rd_ep, primary_mbr);
						TASKDEBUG("dvk_migr_commit rcode=%d\n",	rcode);			
						TASKDEBUG("primary_mbr=%d - local_nodeid=%d\n", primary_mbr, local_nodeid);
					}
				}
			} 
		}

		rcode = dvk_getprocinfo(dcid, rd_ep, rd_ptr);
		if(rcode < 0 ) ERROR_EXIT(rcode);
		TASKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(rd_ptr));	
		if( TEST_BIT(rd_ptr->p_misc_flags, MIS_BIT_RMTBACKUP)) {
			rcode = dvk_unbind(dc_ptr->dc_dcid, rd_ep);
			if(rcode < 0 ) ERROR_PRINT(rcode);
		}
		
		rcode = dvk_getprocinfo(dcid, rd_ep, rd_ptr);
		if(rcode < 0 ) ERROR_EXIT(rcode);
		TASKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(rd_ptr));		
		if( TEST_BIT(rd_ptr->p_rts_flags, BIT_SLOT_FREE)) {
			rcode = dvk_bind(dcid, rd_ep);
			if(rcode != rd_ep ) ERROR_EXIT(rcode);					
			if (endp_flag == NO) { // Started by MoL 
				rcode = sys_bindproc(rd_ep, rd_lpid, LCL_BIND);
				if(rcode < 0) ERROR_EXIT(rcode);						
			} 
		}
		rcode = dvk_getprocinfo(dcid, rd_ep, &proc_rd);
		if(rcode < 0) ERROR_EXIT(rcode);
		TASKDEBUG("AFTER  " PROC_USR_FORMAT,PROC_USR_FIELDS(rd_ptr));
		MTX_UNLOCK(rd_mutex);	
	}
	
	for( i = 0; i < NR_DEVS; i++){
		if ( devvec[i].available == AVAILABLE_YES ){
			TASKDEBUG("Byte offset to the partition start (Device = %d - img_ptr): %X\n", i, devvec[i].img_ptr);	
			devvec[i].part.base = 0;
			fprintf(stdout, "Byte offset to the partition start (m_geom[DEV=%d].dv_base): %X\n",
				i, devvec[i].part.base);
			fflush(stdout);
			TASKDEBUG("Number of bytes in the partition (Device = %d - img_size): %lld\n", 
			i, devvec[i].part.size);
			fflush(stdout);
			}
	}
	
	TASKDEBUG("END rd_init\n");
	return(OK);
}

/*===========================================================================*
 *				get_geometry				     *
 *===========================================================================*/
int get_geometry(int device)
{
	mnx_part_t *part_ptr;
	devvec_t *dv_ptr;
	m_device = device;
	dv_ptr =&devvec[m_device];

	TASKDEBUG("device=%d\n", m_device);
	
	TASKDEBUG(DEV_USR1_FORMAT, DEV_USR1_FIELDS(dv_ptr));	
	dv_ptr->part.cylinders	= (dv_ptr->st_size/(SECTOR_SIZE * DFT_HEADS * DFT_SECTORS));
	dv_ptr->part.heads		= DFT_HEADS;
	dv_ptr->part.sectors	= DFT_SECTORS;
	dv_ptr->part.base		= 0;
	dv_ptr->part.size		= dv_ptr->st_size;

	part_ptr = &dv_ptr->part;
	TASKDEBUG(PART_FORMAT, PART_FIELDS(part_ptr));
    return(OK);
}

/*===========================================================================*
 *				m_geometry				     *
 *===========================================================================*/
int m_geometry(struct driver *dp, message *m_ptr)
{
	int rcode;
	devvec_t *dv_ptr;
	
//	if ( m_ptr->m_source != devvec[m_ptr->DEVICE].dev_owner)
//		ERROR_RETURN(EDVSBADOWNER);
	
	m_device = m_ptr->DEVICE;
	dv_ptr =&devvec[m_device];
	
	get_geometry(m_device);
	
	rcode = dvk_vcopy(SELF, &dv_ptr->part, 
					m_ptr->IO_ENDPT, m_ptr->ADDRESS,
					sizeof(mnx_part_t));
	if( rcode < 0)
		ERROR_RETURN(rcode);
	
    return(OK);
}

/*===========================================================================*
 *				do_close										     *
 *===========================================================================*/
int m_do_close(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
int rcode;

//	if ( m_ptr->m_source != devvec[m_ptr->DEVICE].dev_owner)
//		ERROR_RETURN(EDVSBADOWNER);
	
	// rcode = close(img_fd);
	if (devvec[m_ptr->DEVICE].active_flag < 1) { 
		TASKDEBUG("Device %d, is not open\n", m_ptr->DEVICE);
		rcode = -1; //MARIE: VER SI ESTO ES CORRECTO?
		}
	else{	
		TASKDEBUG("Close device number: %d\n", m_ptr->DEVICE);
		TASKDEBUG("devvec[m_ptr->DEVICE].img_fd=%d\n",devvec[m_ptr->DEVICE].img_fd);
		rcode = close(devvec[m_ptr->DEVICE].img_fd);
		if(rcode) ERROR_RETURN(errno); 
		devvec[m_ptr->DEVICE].img_fd = (-1);
		devvec[m_ptr->DEVICE].dev_owner = HARDWARE;
//		devvec[m_ptr->DEVICE].st_size = 0;
		TASKDEBUG("Buffer %X\n", devvec[m_ptr->DEVICE].localbuff);
		free(devvec[m_ptr->DEVICE].localbuff);
		TASKDEBUG("Free buffer\n");
		devvec[m_ptr->DEVICE].localbuff = NULL;
		devvec[m_ptr->DEVICE].active_flag -= 1;
		}
	// if(rcode < 0) ERROR_EXIT(errno); 
	
	
return(rcode);	
}

/*===========================================================================*
 *				do_nop									     *
 *===========================================================================*/
int do_nop(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
	TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(m_ptr));
return(OK);	
}

