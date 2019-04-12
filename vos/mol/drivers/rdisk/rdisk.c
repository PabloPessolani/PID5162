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
#define _TABLE
#define _MULTI_THREADED
#define _GNU_SOURCE     
#define  MOL_USERSPACE	1
#define _RDISK

// #define TASKDBG		1
#define DCID		0
#define	NODE0		0
#define	NODE1		1

#include "rdisk.h"
#include "data_usr.h"
#include "const.h"

struct device m_geom[NR_DEVS];  /* base and size of each device */

int m_device;			/* current device */

struct partition entry; /*no en código original, pero para completar los datos*/

/* Entry points to this driver. */
	
message mess;
SP_message sp_msg; /*message Spread*/	

unsigned *localbuff;		/* pointer to the first byte of the local buffer (=disk image)*/	

struct mdevvec *devinfo_ptr;

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

static int replicate_flag; 

void usage(char* errmsg, ...) {
	if(errmsg) {
		fprintf("ERROR: %s\n", errmsg);
		fprintf(stderr, "Usage: rdisk -r<replicate> -[f<full_update>|d<diff_updates>] -D<dyn_updates> -z<compress> -c <config file>\n");
		
	} else {
		fprintf(stderr, "por ahora nada nbd-client imprime la versión\n");
	}
fprintf(stderr, "Usage: rdisk -r<replicate> -[f<full_update>|d<diff_updates>] -D<dyn_updates> -z<compress> -c <config file>\n");
fflush(stderr);
}

/*===========================================================================*
 *				   main 				     *
 *===========================================================================*/
//PUBLIC int main(void)
int main (int argc, char *argv[] )
{
	/* Main program.*/
	int rcode, c, i, j, l_dev, f_dev, cfile_flag, fflag, Fflag, Mflag;  
	char *c_file;
	char prueba;
	
	struct stat img_stat,img_stat1;
	
	struct option long_options[] = {
		/* These options are their values. */
		{ "replicate", 	no_argument,		NULL, 'r' },
		{ "full_update",no_argument, 		NULL, 'f' },
		{ "diff_update",no_argument, 		NULL, 'd' },
		{ "dyn_update", no_argument, 		NULL, 'D' }, /*dynamic update*/
		{ "zcompress", 	no_argument, 		NULL, 'z' },
		{ "endpoint", 	no_argument, 		NULL, 'e' },
		{ "config", 	required_argument, 	NULL, 'c' },
		{ 0, 0, 0, 0 }, 		
	};
	
	rcode = dvk_open();
	if (rcode < 0)  ERROR_EXIT(rcode);
	
	/* Not availables minor device  */
	for( i = 0; i < NR_DEVS; i++){
		devvec[i].available = 0;
	}

	/* flags getopt*/
	replicate_flag = DONOT_REPLICATE;
	r_comp = DONOT_COMPRESS;
	dynup_flag = DONOT_DYNUPDATES; /*not dynamic update*/
	cfile_flag = 0;
	fflag = 0;
	Fflag = 0;
	Mflag = 0;
	r_type = 0;
	r_comp = 0;
	endp_flag = 0;

	while((c = getopt_long_only(argc, argv, "rfFdDzec:", long_options, NULL)) >= 0) {
		switch(c) {
			case 'r':
				TASKDEBUG("Active replicate\n");
				replicate_flag = DO_REPLICATE;
				break;
			case 'f':
				TASKDEBUG("COPY_FULL\n");
				r_type = DEV_CFULL; 
				TASKDEBUG("r_type: %d - DEV_CFULL\n", r_type);	
				fflag = 1;
				break;	
			case 'd':		
				TASKDEBUG("DIFF_UPDATE\n");
				r_type = DEV_CMD5; 
				TASKDEBUG("r_type: %d - DEV_CMD5\n", r_type);	
				Mflag = 1;
				break;	
			case 'D':		
				TASKDEBUG("Sync - Active Dynamic Updates\n");
				dynup_flag = DO_DYNUPDATES; 
				break;		
			case 'z':		
				r_comp = DO_COMPRESS; 
				TASKDEBUG("r_comp(DO_COMPRESS=1/DONOT_COMPRESS=0)=%d\n",r_comp);
				break;			
			case 'c': /*config file*/
				c_file = optarg;
				TASKDEBUG("Option c: %s\n", c_file);
				cfile_flag=1; 
				break;	
			case 'e': /*endpoint number- not Started by MoL */
				TASKDEBUG("Autonomous Bind\n");
				endp_flag =1;
				break;
			default:
				usage("Unknown option %s encountered", optarg);
				exit(EXIT_FAILURE);
			}
		}	
	
	TASKDEBUG("(sizeof(mess_3) =  %d\n", sizeof(mess_3));
	TASKDEBUG("sizeof(message) =  %d\n", sizeof(message));
 	
	if ( (argc < 2) || (cfile_flag != 1) ) { /*al menos el nombre el archivo de configuración*/
 	    usage( "No arguments", optarg );
		exit(1);
    }
	
	if ( ( fflag + Mflag ) > 1) { /*más de un flag de tipo de transferencia activo, no se puede: copy or differents*/
 	    usage( "Select: f|d", optarg );
		exit(1);
    }
	
	count_availables = 0;		
	test_config(c_file);
		
	if (count_availables == 0){
		fprintf( stderr,"\nERROR. No availables devices in %s\n", c_file );
		fflush(stderr);
		exit(1);
	}
	TASKDEBUG("count_availables=%d\n",count_availables);
	
	
	/* the same inode of file image*/
	f_dev=0; /*count first device*/
	l_dev=1; /*count last device*/
		
	while (f_dev < NR_DEVS){
	for( i = f_dev; i < NR_DEVS; i++){
			if( devvec[i].available == 0){
				f_dev++;
			}
			else{
				rcode = stat(devvec[i].img_ptr, &img_stat);
				TASKDEBUG("stat0 %s rcode=%d\n",devvec[i].img_ptr, rcode);
				if(rcode){
					fprintf( stderr,"\nERROR %d: Device %s minor_number %d is not valid\n", rcode , c_file, i );
					fflush(stderr);
					f_dev++;
				}
				for( j = l_dev; j < NR_DEVS; j++){
					if( devvec[j].available == 0){
						l_dev++;
					}	
					else{
						rcode = stat(devvec[j].img_ptr, &img_stat1);
						TASKDEBUG("stat1 %s rcode=%d\n",devvec[i].img_ptr, rcode);
						if(rcode){
							fprintf( stderr,"\nERROR %d: Device %s minor_number %d is not valid\n", rcode, c_file, j );
							fflush(stderr);
							l_dev++;
						}
						TASKDEBUG("devvec[%d].img_ptr=%s,devvec[%d].img_ptr=%s\n", 
							i, devvec[i].img_ptr,j,devvec[j].img_ptr);
				
						if ( img_stat.st_ino == img_stat1.st_ino ){
							fprintf( stderr,"\nERROR. Minor numbers %d - %d are the same file\n", i, j );
							fflush(stderr);
							devvec[j].available = 0;
							fprintf( stderr,"\nDevice with minor numbers: %d is not available now\n", j );
							fflush(stderr);
						}
					}
				}
				f_dev++;
				l_dev++;
			}
		}	
	}

/* get the image file size */
	for( i = 0; i < NR_DEVS; i++){
		if (devvec[i].available == 0){
			TASKDEBUG("Minor device %d is not available\n", i);
		}else{
			TASKDEBUG("devvec[%d].img_ptr=%s\n", i, devvec[i].img_ptr);
			rcode = stat(devvec[i].img_ptr, &img_stat);
			
			if(rcode) ERROR_EXIT(errno);
			
			devvec[i].st_size = img_stat.st_size;
			TASKDEBUG("image size=%d[bytes] %d\n", img_stat.st_size, devvec[i].st_size);
			devvec[i].st_blksize = img_stat.st_blksize;
			TASKDEBUG("block size=%d[bytes] %d\n", img_stat.st_blksize, devvec[i].st_blksize);
		}
	}
/* the same inode of file image*/
			
  rcode = rd_init();
  if(rcode) ERROR_RETURN(rcode);  
  driver_task(&m_dtab);	
  
  free(localbuff);
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
/* Prepare for I/O on a device: check if the minor device number is ok. */
  
	if (device < 0 || device >= NR_DEVS || devvec[device].active != 1) {
		TASKDEBUG("Error en m_prepare\n");
		return(NIL_DEV);
		}
	m_device = device;
	TASKDEBUG("device = %d (m_device = %d)\n", device, m_device);
	TASKDEBUG("Prepare for I/O on a given minor device: (%X;%X), (%u;%u)\n", 
	m_geom[device].dv_base._[0],m_geom[device].dv_base._[1], m_geom[device].dv_size._[0], m_geom[device].dv_size._[1]);
  
  return(&m_geom[device]);
 
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
	phys_bytes mem_phys;
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
	
	if (devvec[m_device].active != 1) { /*minor device active must be -1-*/
		TASKDEBUG("Minor device = %d\n is not active", m_device);
		ERROR_RETURN(EDVSNODEV);	
	}
	
	/* Get minor device number and check for /dev/null. */
	dv = &m_geom[m_device];
	dv_size = cv64ul(dv->dv_size); 
	
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
					
		mem_phys = cv64ul(dv->dv_base) + position;
		TASKDEBUG("DRIVER - position I/O(mem_phys) %X\n", mem_phys);
			
		if ((opcode == DEV_GATHER) ||(opcode == DEV_CGATHER))  {/* copy data */ /*DEV_GATHER read from an array (com.h)*/
		
			TASKDEBUG("\n<DEV_GATHER>\n");
				
			stbytes = 0;
			do	{
				/* read to the virtual disk-file- into the buffer --> to the FS´s buffer*/
				bytes = (count > devvec[m_device].buff_size)?devvec[m_device].buff_size:count;
				TASKDEBUG("bytes: %d\n", bytes);		
			
				/* read data from the virtual device file into the local buffer  */			
				bytes = pread(devvec[m_device].img_p, devvec[m_device].localbuff, bytes, position);
				TASKDEBUG("pread: bytes=%d\n", bytes);
				
				if(bytes < 0) ERROR_EXIT(errno);
				
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
				TASKDEBUG("mem_phys: %X (in DRIVER)\n", devvec[m_device].localbuff);			
				TASKDEBUG("user_vir: %X (in proc_nr %d)\n", user_vir, proc_nr);			
			
				if (rcode < 0 ) {
					fprintf( stderr,"dvk_vcopy rcode=%d\n",rcode);
					fflush(stderr);
					break;
				}

				stbytes += bytes; /*total bytes transfers*/								
				position += bytes;
				iov->iov_addr += bytes;

				user_vir = iov->iov_addr;
				TASKDEBUG("user_vir (do-buffer) %X\n", user_vir);	

				count -= bytes;
				TASKDEBUG("count=%d stbytes=%d position=%ld\n", count, stbytes, position);	

				} while(count > 0);
			/* END DEV_GATHER*/
			
		} else { /*DEV_SCATTER write from an array*/

			TASKDEBUG("\n<DEV_SCATTER>\n");
			
			stbytes = 0;
			TASKDEBUG("\dc_ptr->dc_nr_nodes=%d, active_nr_nodes=%d\n",dc_ptr->dc_nr_nodes, active_nr_nodes);

			if (replicate_flag == DO_REPLICATE){
				
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
						TASKDEBUG("mem_phys: %X (in DRIVER)\n", devvec[m_device].localbuff);			
						TASKDEBUG("user_vir: %X (in proc_nr %d)\n", user_vir, proc_nr);			
									
						if (rcode < 0 ){
							fprintf(stderr, "VCOPY rcode=%d\n", rcode);
							fflush(stderr);
							break;
						}else{
							stbytes = stbytes + bytes; /*si dvk_vcopy fue exitosa, devuelve cantidad de bytes transferidos*/
						}		
						
						/* write data from local buffer to the  virtual device file */
				
						TASKDEBUG("devvec[m_device].img_p=%d, devvec[m_device].localbuff=%X, bytes=%d, position=%u\n", 
							devvec[m_device].img_p, devvec[m_device].localbuff, bytes, position);			

								
						bytes = pwrite(devvec[m_device].img_p, devvec[m_device].localbuff, bytes, position);
						TASKDEBUG("buffer: %s\n", devvec[m_device].localbuff);		
						
						if ( bytes == (-1) ){ 
							TASKDEBUG("pwrite: %d\n", bytes);
							ERROR_EXIT(errno);
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
				
						TASKDEBUG("devvec[m_device].img_p=%d, devvec[m_device].localbuff=%X, bytes=%d, position=%u\n", 
							devvec[m_device].img_p, devvec[m_device].localbuff, bytes, position);			
						
						bytes = pwrite(devvec[m_device].img_p, devvec[m_device].localbuff, bytes, position);
						
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
						
						bytes_c = pwrite(devvec[m_device].img_p, msg_lz4cd.buf.buffer_data, sp_msg.buf.buffer_size, position);
						
						TASKDEBUG("pwrite: %d\n", bytes_c);
						
						TASKDEBUG("bytes: %d\n", bytes); /*no se modifica, pero es el contabiliza para count > 0)*/
						
						if(bytes_c < 0) ERROR_EXIT(errno);	
							
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

	rcode = OK;
	TASKDEBUG("rcode %d\n", rcode);
	do {
		if ( devvec[m_ptr->DEVICE].available == 0 ){
			TASKDEBUG("devvec[m_ptr->DEVICE].available=%d\n", devvec[m_ptr->DEVICE].available);
			rcode = errno;
			TASKDEBUG("rcode=%d\n", rcode);
			return(rcode);
			}
			
		devvec[m_ptr->DEVICE].img_p = open(devvec[m_ptr->DEVICE].img_ptr, O_RDWR);
		TASKDEBUG("Open imagen FD=%d\n", devvec[m_ptr->DEVICE].img_p);
			
		if(devvec[m_ptr->DEVICE].img_p < 0) {
			TASKDEBUG("devvec[m_ptr->DEVICE].img_p=%d\n", devvec[m_ptr->DEVICE].img_p);
			rcode = errno;
			TASKDEBUG("rcode=%d\n", rcode);
			return(rcode);
			}
			
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
			
		devvec[m_ptr->DEVICE].active = 1;
		TASKDEBUG("Device %d is active %d\n", m_ptr->DEVICE, devvec[m_ptr->DEVICE].active);
		
		/* Check device number on open. */
		if (m_prepare(m_ptr->DEVICE) == NIL_DEV) {
			TASKDEBUG("'m_prepare()' %d - NIL_DEV:%d\n", m_prepare(m_ptr->DEVICE), NIL_DEV);
			rcode = ENXIO;
			ERROR_RETURN(rcode);
		}
 	
	}while(0);
	

   	if( replicate_flag == DO_REPLICATE ) { /* PRIMARY;  MULTICAST to other nodes the device operation */
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
  
  TASKDEBUG(DEV_USR_FORMAT,DEV_USR_FIELDS(devinfo_ptr));
	
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
	
	if( mayor_dev != (-1) && endp_flag == 1)
		rd_ep = mayor_dev;
	else
		rd_ep = RDISK_PROC_NR;
	
	TASKDEBUG("rd_ep=%d\n", rd_ep);

	/* NODE info */
	local_nodeid = dvk_getdvsinfo(&dvs);
	if(local_nodeid < 0 )
		ERROR_EXIT(EDVSDVSINIT);
	dvs_ptr = &dvs;
	TASKDEBUG(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));
	TASKDEBUG("local_nodeid=%d\n", local_nodeid);
	
	TASKDEBUG("Get the DC info\n");
	rcode = dvk_getdcinfo(DCID, &dcu);
	if(rcode < 0) ERROR_EXIT(rcode);
	dc_ptr = &dcu;
	TASKDEBUG(DC_USR1_FORMAT,DC_USR1_FIELDS(dc_ptr));
	TASKDEBUG(DC_USR2_FORMAT,DC_USR2_FIELDS(dc_ptr));

	TASKDEBUG("Get RDISK info\n");
	rcode = dvk_getprocinfo(DCID, rd_ep, &proc_rd);
	if(rcode < 0 ) ERROR_EXIT(rcode);
	rd_ptr = &proc_rd;
	TASKDEBUG("BEFORE " PROC_USR_FORMAT,PROC_USR_FIELDS(rd_ptr));
	
	if( replicate_flag != DO_REPLICATE) { // WITHOUT REPLICATION 
		if( TEST_BIT(rd_ptr->p_rts_flags, BIT_SLOT_FREE)) {
			TASKDEBUG("Starting single RDISK\n");
			active_nr_nodes = 1;
			TASKDEBUG("active_nr_nodes=%d\n", active_nr_nodes);
			rcode = dvk_bind(DCID, rd_ep);
			if(rcode != rd_ep ) ERROR_EXIT(rcode);					
			if (endp_flag == 0) { // Started by MoL 
				rcode = sys_bindproc(rd_ep, rd_lpid, LCL_BIND);
				if(rcode < 0) ERROR_EXIT(rcode);						
			} 
		}
	} else {							// WITH REPLICATION 
		if( TEST_BIT(rd_ptr->p_rts_flags, BIT_SLOT_FREE)) { // PRIMARY as REPLICA 
			TASKDEBUG("Starting RDISK PRIMARY\n");
			active_nr_nodes = 1;
			TASKDEBUG("active_nr_nodes=%d\n", active_nr_nodes);
			rcode = dvk_replbind(DCID, rd_lpid, rd_ep);
			if(rcode != rd_ep ) ERROR_EXIT(rcode);					
			if (endp_flag == 0) { // Started by MoL 
				rcode = sys_bindproc(rd_ep, rd_lpid, REPLICA_BIND);
				if(rcode < 0) ERROR_EXIT(rcode);						
			} 
			
		}else{											// SECONDARY as BACKUP 
			TASKDEBUG("Starting RDISK BACKUP\n");	
			rcode = dvk_bkupbind(DCID, rd_lpid, rd_ep, rd_ptr->p_nodeid);
			if(rcode != rd_ep ) ERROR_EXIT(rcode);					
		}
	}
	rcode = dvk_getprocinfo(DCID, rd_ep, &proc_rd);
	if(rcode < 0) ERROR_EXIT(rcode);
	TASKDEBUG("AFTER  " PROC_USR_FORMAT,PROC_USR_FIELDS(rd_ptr));	
	
	for( i = 0; i < NR_DEVS; i++){
		if ( devvec[i].available != 0 ){
			TASKDEBUG("Byte offset to the partition start (Device = %d - img_ptr): %X\n", i, devvec[i].img_ptr);
			m_geom[i].dv_base = cvul64(devvec[i].img_ptr);
			fprintf(stdout, "Byte offset to the partition start (m_geom[DEV=%d].dv_base): %X\n", i, m_geom[i].dv_base);
			fflush(stdout);
	
			TASKDEBUG("Number of bytes in the partition (Device = %d - img_size): %u\n", i, devvec[i].st_size);
			m_geom[i].dv_size = cvul64(devvec[i].st_size);	
			fprintf(stdout, "Number of bytes in the partition (m_geom[DEV=%d].dv_size): %u\n", i, m_geom[i].dv_size);
			fflush(stdout);
			}
	}
	
	if (replicate_flag == DO_REPLICATE){	
		TASKDEBUG("Initializing REPLICATE\n");
		rcode = init_replicate();	
		if( rcode)ERROR_EXIT(rcode);
		
		TASKDEBUG("Starting REPLICATE thread\n");
		rcode = pthread_create( &replicate_thread, NULL, replicate_main, 0 );
		if( rcode )ERROR_EXIT(rcode);

		// pthread_mutex_lock(&rd_mutex);
		MTX_LOCK(rd_mutex);
		
		// pthread_cond_wait(&rd_barrier,&rd_mutex); /* unlock, wait, and lock again rd_mutex */	
		COND_WAIT(rd_barrier,rd_mutex);
		
		TASKDEBUG("RDISK has been signaled by the REPLICATE thread  FSM_state=%d\n",  FSM_state);
		if( FSM_state == STS_LEAVE) {	/* An error occurs trying to join the spread group */
			// pthread_mutex_unlock(&rd_mutex);
			MTX_UNLOCK(rd_mutex);
			ERROR_RETURN(EDVSCONNREFUSED);
		}	

		TASKDEBUG("Replicated driver. nr_nodes=%d primary_mbr=%d\n",  nr_nodes, primary_mbr);
		TASKDEBUG("primary_mbr=%d - local_nodeid=%d\n", primary_mbr, local_nodeid);
		if ( primary_mbr != local_nodeid) {
			TASKDEBUG("wait until  the process will be the PRIMARY\n");
			// pthread_cond_wait(&primary_barrier,&rd_mutex); /*wait until  the process will be the PRIMARY  */	
			COND_WAIT(primary_barrier,rd_mutex);
			
			TASKDEBUG("RDISK_PROC_NR(%d) endpoint %d\n", RDISK_PROC_NR, rd_ep);
			rcode = dvk_migr_start(dc_ptr->dc_dcid, RDISK_PROC_NR);
			TASKDEBUG("dvk_migr_start rcode=%d\n",	rcode);
			rcode = dvk_migr_commit(rd_lpid, dc_ptr->dc_dcid, RDISK_PROC_NR, local_nodeid);
			TASKDEBUG("dvk_migr_commit rcode=%d\n",	rcode);			
			TASKDEBUG("primary_mbr=%d - local_nodeid=%d\n", primary_mbr, local_nodeid);
		}
		// pthread_mutex_unlock(&rd_mutex);
		
		MTX_UNLOCK(rd_mutex);
			
	}
	TASKDEBUG("END rd_init\n");
	return(OK);
}

/*===========================================================================*
 *				m_geometry				     *
 *===========================================================================*/
//PRIVATE void m_geometry(entry)
void m_geometry(entry)
struct partition *entry;
{
  /* Memory devices don't have a geometry, but the outside world insists. */
  entry->cylinders = div64u(m_geom[m_device].dv_size, SECTOR_SIZE) / (64 * 32);
  entry->heads = 64;
  entry->sectors = 32;
}

/*===========================================================================*
 *				do_close										     *
 *===========================================================================*/
int m_do_close(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
int rcode;

	// rcode = close(img_p);
	if (devvec[m_ptr->DEVICE].active != 1) { 
		TASKDEBUG("Device %d, is not open\n", m_ptr->DEVICE);
		rcode = -1; //MARIE: VER SI ESTO ES CORRECTO?
		}
	else{	
		TASKDEBUG("devvec[m_ptr->DEVICE].img_p=%d\n",devvec[m_ptr->DEVICE].img_p);
		rcode = close(devvec[m_ptr->DEVICE].img_p);
		if(rcode) ERROR_EXIT(errno); 
		
		TASKDEBUG("Close device number: %d\n", m_ptr->DEVICE);
		devvec[m_ptr->DEVICE].img_ptr = NULL;
		devvec[m_ptr->DEVICE].img_p = NULL;
		devvec[m_ptr->DEVICE].st_size = 0;
		devvec[m_ptr->DEVICE].st_blksize = 0;
		devvec[m_ptr->DEVICE].localbuff = NULL;
		devvec[m_ptr->DEVICE].active = 0;
		devvec[m_ptr->DEVICE].available = 0;
	
		TASKDEBUG("Buffer %X\n", devvec[m_ptr->DEVICE].localbuff);
		free(devvec[m_ptr->DEVICE].localbuff);
		TASKDEBUG("Free buffer\n");
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

