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
#define DVK_GLOBAL_HERE
#define _MULTI_THREADED
#define _GNU_SOURCE     
#define  MOL_USERSPACE	1
#define _RDISK

#include "rdisk.h"
#include "data_usr.h"
#include "const.h"

#undef DVK_GLOBAL_HERE
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

struct device rd_geom[NR_DEVS];  /* base and size of each device */
int rd_device;			/* current device */

struct partition entry; /*no en código original, pero para completar los datos*/

/* Entry points to this driver. */
	
message rd_mess;
unsigned *localbuff;		/* pointer to the first byte of the local buffer (=disk image)*/	
struct mdevvec *devinfo_ptr;


struct driver rd_dtab = {
  rd_name,	/* current device's name */
  rd_do_open,	/* open or mount */
  rd_do_close,	/* nothing on a close */
  rd_nop,// rd_ioctl,	/* specify ram disk geometry */
  rd_prepare,	/* prepare for I/O on a given minor device */
  rd_transfer,	/* do the I/O */
  nop_cleanup,	/* no need to clean up */
  rd_geometry,	/* memory device "geometry" */
  rd_nop, //  nop_signal,	/* system signals */
  rd_nop, //  nop_alarm,
  rd_nop,	//  nop_cancel,
  rd_nop, //  nop_select,
  NULL,
  NULL,
  rd_do_dump
};

void usage(char* errmsg, ...) {
	if(errmsg) {
		fprintf("ERROR: %s\n", errmsg);
		fprintf(stderr, "Usage: rdisk -c <config file>\n");
		
	} else {
		fprintf(stderr, "por ahora nada nbd-client imprime la versión\n");
	}
fprintf(stderr, "Usage: rdisk -c <config file>\n");
fflush(stderr);
}

/*===========================================================================*
 *				   main_rdisk 				     *
 *===========================================================================*/
int main_rdisk (int argc, char *argv[] )
{
	/* Main program.*/
	int rcode, c, i, j, l_dev, f_dev, cfile_flag;  
	char *c_file;
	
	struct stat img_stat,img_stat1;
	
	struct option long_options[] = {
		/* These options are their values. */
//		{ "replicate", 	no_argument,		NULL, 'r' },
//		{ "full_update",no_argument, 		NULL, 'f' },
//		{ "diff_update",no_argument, 		NULL, 'd' },
//		{ "dyn_update", no_argument, 		NULL, 'D' }, /*dynamic update*/
//		{ "zcompress", 	no_argument, 		NULL, 'z' },
//		{ "endpoint", 	no_argument, 		NULL, 'e' },
		{ "config", 	required_argument, 	NULL, 'c' },
		{ 0, 0, 0, 0 }, 		
	};

	
	/* Not availables minor device  */
	for( i = 0; i < NR_DEVS; i++){
		devvec[i].available = 0;
	}

	/* flags getopt*/
	cfile_flag = 0;

	while((c = getopt_long_only(argc, argv, "c:", long_options, NULL)) >= 0) {
		switch(c) {
			case 'c': /*config file*/
				c_file = optarg;
				MUKDEBUG("Option c: %s\n", c_file);
				cfile_flag=1; 
				break;	
			default:
				usage("Unknown option %s encountered", optarg);
				ERROR_TSK_EXIT(c);
			}
		}	
	
	rd_dev_nr = 0;
	parse_config(c_file);
		
	if (rd_dev_nr == 0){
		fprintf( stderr,"\nERROR. No availables devices in %s\n", c_file );
		fflush(stderr);
		ERROR_TSK_EXIT(c);
	}
	MUKDEBUG("rd_dev_nr=%d\n",rd_dev_nr);
	
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
				MUKDEBUG("stat0 %s rcode=%d\n",devvec[i].img_ptr, rcode);
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
						MUKDEBUG("stat1 %s rcode=%d\n",devvec[i].img_ptr, rcode);
						if(rcode){
							fprintf( stderr,"\nERROR %d: Device %s minor_number %d is not valid\n", rcode, c_file, j );
							fflush(stderr);
							l_dev++;
						}
						MUKDEBUG("devvec[%d].img_ptr=%s,devvec[%d].img_ptr=%s\n", 
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
			MUKDEBUG("Minor device %d is not available\n", i);
		}else{
			MUKDEBUG("devvec[%d].img_ptr=%s\n", i, devvec[i].img_ptr);
			rcode = stat(devvec[i].img_ptr, &img_stat);
			
			if(rcode) ERROR_TSK_EXIT(errno);
			
			devvec[i].st_size = img_stat.st_size;
			MUKDEBUG("image size=%d[bytes] %d\n", img_stat.st_size, devvec[i].st_size);
			devvec[i].st_blksize = img_stat.st_blksize;
			MUKDEBUG("block size=%d[bytes] %d\n", img_stat.st_blksize, devvec[i].st_blksize);
		}
	}
/* the same inode of file image*/
			
  rcode = rd_init();
  if(rcode) ERROR_RETURN(rcode);  
  driver_task(&rd_dtab);	
  
  free(localbuff);
  return(OK);				
}

/*===========================================================================*
 *				 rd_name					     *
 *===========================================================================*/
//PRIVATE char *rd_name()
char *rd_name(void)
{
/* Return a name for the current device. */
  //static char name[] = "memory";
  static char name[] = "rd_driver";
  MUKDEBUG("n_name(): %s\n", name);
 		
  return name;  
}

/*===========================================================================*
 *				rd_prepare				     *
 *===========================================================================*/
//PRIVATE struct device *rd_prepare(device)
struct device *rd_prepare(int device)
{
/* Prepare for I/O on a device: check if the minor device number is ok. */
  
	if (device < 0 || device >= NR_DEVS || devvec[device].active != 1) {
		MUKDEBUG("Error en rd_prepare\n");
		return(NIL_DEV);
		}
	rd_device = device;
	MUKDEBUG("device = %d (rd_device = %d)\n", device, rd_device);
	MUKDEBUG("Prepare for I/O on a given minor device: (%X;%X), (%u;%u)\n", 
	rd_geom[device].dv_base._[0],rd_geom[device].dv_base._[1], rd_geom[device].dv_size._[0], rd_geom[device].dv_size._[1]);
  
  return(&rd_geom[device]);
 
}

/*===========================================================================*
 *				rd_transfer				     *
 *===========================================================================*/
int rd_transfer(proc_nr, opcode, position, iov, nr_req)
int proc_nr;			/* process doing the request */
int opcode;			/* DEV_GATHER or DEV_SCATTER */
off_t position;			/* offset on device to read or write */
iovec_t *iov;			/* pointer to read or write request vector */
unsigned nr_req;		/* length of request vector */
{
	/* Read or write one the driver's minor devices. */
	phys_bytes mem_phys;
	unsigned count, tbytes, stbytes, bytes, bytes_c; //left, chunk; 
	vir_bytes user_vir, addr_s;
	struct device *dv;
	unsigned long dv_size;
	int rcode;
	off_t posit;
	
	tbytes = 0;
	bytes = 0;
	bytes_c = 0;
	
	MUKDEBUG("rd_device: %d\n", rd_device); 
	MUKDEBUG("proc_nr=%d opcode=%d nr_req=%d\n",proc_nr, opcode, nr_req);
	
	if (devvec[rd_device].active != 1) { /*minor device active must be -1-*/
		MUKDEBUG("Minor device = %d\n is not active", rd_device);
		ERROR_RETURN(EDVSNODEV);	
	}
	
	/* Get minor device number and check for /dev/null. */
	dv = &rd_geom[rd_device];
	dv_size = cv64ul(dv->dv_size); 
	
	posit = position;
	MUKDEBUG("posit: %X\n", posit);	
	MUKDEBUG("nr_req: %d\n", nr_req);	

	while (nr_req > 0) { /*2*/
	  
		/* How much to transfer and where to / from. */
		count = iov->iov_size;
		MUKDEBUG("count: %u\n", count);	
	
		user_vir = iov->iov_addr;
		addr_s = iov->iov_addr;
		MUKDEBUG("user_vir %X\n", user_vir);	
		
		if (position >= dv_size) {
			MUKDEBUG("EOF\n"); 
			return(OK);
			} 	/* check for EOF */
			
		if (position + count > dv_size) { 
			count = dv_size - position; 
			MUKDEBUG("count dv_size-position: %u\n", count); 
			}
					
		mem_phys = cv64ul(dv->dv_base) + position;
		MUKDEBUG("DRIVER - position I/O(mem_phys) %X\n", mem_phys);
			
		if (opcode == DEV_GATHER)  {/* copy data */ /*DEV_GATHER read from an array (com.h)*/
		
			MUKDEBUG("\n<DEV_GATHER>\n");
				
			stbytes = 0;
			do	{
				/* read to the virtual disk-file- into the buffer --> to the FS´s buffer*/
				bytes = (count > devvec[rd_device].buff_size)?devvec[rd_device].buff_size:count;
				MUKDEBUG("bytes: %d\n", bytes);		
				
				devinfo_ptr  = &devvec[rd_device];
				MUKDEBUG(DEV_USR_FORMAT,DEV_USR_FIELDS(devinfo_ptr));
  			
				/* read data from the virtual device file into the local buffer  */			
				bytes = pread(devvec[rd_device].img_p, devvec[rd_device].localbuff, bytes, position);
				MUKDEBUG("pread: bytes=%d\n", bytes);
				
				if(bytes < 0) ERROR_TSK_EXIT(errno);
				
				/* copy the data from the local buffer to the requester process address space in other DC */
				MUK_vcopy(rcode, rd_ep, devvec[rd_device].localbuff, proc_nr, user_vir, bytes); 	
//				memcpy( user_vir, devvec[rd_device].localbuff, bytes);
//				rcode = 0;
				
				MUKDEBUG("DRIVER: MUK_vcopy(DRIVER -> proc_nr) rcode=%d\n", rcode);  
				MUKDEBUG("bytes= %d\n", bytes);
				MUKDEBUG("DRIVER - Offset (read) %X\n", devvec[rd_device].localbuff);			
				MUKDEBUG("mem_phys: %X (in DRIVER)\n", devvec[rd_device].localbuff);			
				MUKDEBUG("user_vir: %X (in proc_nr %d)\n", user_vir, proc_nr);			
			
				if (rcode <0 ) {
					fprintf( stderr,"VCOPY rcode=%d\n",rcode);
					fflush(stderr);
					break;
				}

				stbytes += bytes; /*total bytes transfers*/								
				position += bytes;
				iov->iov_addr += bytes;

				user_vir = iov->iov_addr;
				MUKDEBUG("user_vir (do-buffer) %X\n", user_vir);	

				count -= bytes;
				MUKDEBUG("count=%d stbytes=%d position=%ld\n", count, stbytes, position);	

				} while(count > 0);
			/* END DEV_GATHER*/
			
		} else { /*DEV_SCATTER write from an array*/

			MUKDEBUG("\n<DEV_SCATTER>\n");
			
			stbytes = 0;
			MUKDEBUG("\dc_ptr->dc_nr_nodes=%d\n",dc_ptr->dc_nr_nodes);
	
			/*NOT REPLICATE*/
			MUKDEBUG("NOT REPLICATE\n");
			
			nr_optrans = 0;
			MUKDEBUG("nr_optrans: %d\n", nr_optrans); /*para enviar rta por cada nr_req*/
														
			do {
				/* from to buffer RDISK -> to local buffer and write into the file*/
						
				bytes = (count > devvec[rd_device].buff_size)?devvec[rd_device].buff_size:count;
				MUKDEBUG("bytes: %d\n", bytes);
				
				MUKDEBUG("WRITE - CLIENT TO PRIMARY\n");
				MUKDEBUG("proc_rn= %d\n", proc_nr);  
				MUKDEBUG("user_vir= %X\n", user_vir);     
				MUKDEBUG("rd_ep=%d\n", rd_ep);			
				MUKDEBUG("localbuff: %X\n", devvec[rd_device].localbuff);			
				
				/* copy the data from the requester process address space in other DC  to the local buffer */
				MUK_vcopy(rcode, proc_nr, user_vir, rd_ep, devvec[rd_device].localbuff, bytes); /*escribo bufferFS -> bufferlocal*/
//				memcpy( devvec[rd_device].localbuff, user_vir, bytes);
				rcode = 0;
				
				MUKDEBUG("DRIVER: MUK_vcopy(proc_nr -> DRIVER)= %d\n", rcode);  
				MUKDEBUG("bytes= %d\n", bytes);     
				MUKDEBUG("mem_phys: %X (in DRIVER)\n", devvec[rd_device].localbuff);			
				MUKDEBUG("user_vir: %X (in proc_nr %d)\n", user_vir, proc_nr);			
								
				if (rcode < 0 ){
					fprintf(stderr, "VCOPY rcode=%d\n", rcode);
					fflush(stderr);
					break;
				}else{
					stbytes = stbytes + bytes; /*si MUK_vcopy fue exitosa, devuelve cantidad de bytes transferidos*/
					}		
				
				
				if ( opcode == DEV_SCATTER) { /*uncompress*/
		
					/* write data from local buffer to the  virtual device file */
					MUKDEBUG("NOT COMPRESS DATA\n");
			
					MUKDEBUG("devvec[rd_device].img_p=%d, devvec[rd_device].localbuff=%X, bytes=%d, position=%u\n", 
						devvec[rd_device].img_p, devvec[rd_device].localbuff, bytes, position);			
					
					bytes = pwrite(devvec[rd_device].img_p, devvec[rd_device].localbuff, bytes, position);
					
					MUKDEBUG("pwrite: %d\n", bytes);
					
					if( bytes < 0) ERROR_RETURN(errno);	
						
				}
				nr_optrans++;
				MUKDEBUG("Operaciones de transferencias de bytes (cantidad vcopy)= %d\n", nr_optrans);
							
				position += bytes;
				iov->iov_addr += bytes;
					
				user_vir = iov->iov_addr;
				MUKDEBUG("user_vir (do-buffer) %X count %d bytes %d\n", user_vir, count, bytes);	
					
				count -= bytes;
				MUKDEBUG("count=%d stbytes=%d position=%ld\n", count, stbytes, position);	
				
			} while(count > 0);	
		}
		/* Book the number of bytes transferred. Registra el número de bytes transferidos? */
		MUKDEBUG("subtotal de bytes\n");	
		if ((iov->iov_size -= stbytes) == 0) { iov++; nr_req--; }  /*subtotal bytes, por cada iov_size según posición del vector*/
		MUKDEBUG("subtotal de bytes tbytes=%d nr_req=%d\n", tbytes, nr_req);	
		
		tbytes += stbytes; /*total de bytes leídos o escritos*/
	}
	
	return(tbytes);
}

/*===========================================================================*
 *				rd_do_open				     *
 *===========================================================================*/
int rd_do_open(struct driver *dp, message *rd_mptr) 
{
	int rcode;
	
	MUKDEBUG("rd_do_open - device number: %d - OK to open\n", rd_mptr->DEVICE);

	rcode = OK;
	MUKDEBUG("rcode %d\n", rcode);
	do {
		if ( devvec[rd_mptr->DEVICE].available == 0 ){
			MUKDEBUG("devvec[rd_mptr->DEVICE].available=%d\n", devvec[rd_mptr->DEVICE].available);
			rcode = errno;
			MUKDEBUG("rcode=%d\n", rcode);
			return(rcode);
			}
			
		devvec[rd_mptr->DEVICE].img_p = open(devvec[rd_mptr->DEVICE].img_ptr, O_RDWR);
		MUKDEBUG("Open imagen FD=%d\n", devvec[rd_mptr->DEVICE].img_p);
			
		if(devvec[rd_mptr->DEVICE].img_p < 0) {
			MUKDEBUG("devvec[rd_mptr->DEVICE].img_p=%d\n", devvec[rd_mptr->DEVICE].img_p);
			rcode = errno;
			MUKDEBUG("rcode=%d\n", rcode);
			return(rcode);
			}
			
		/* local buffer to the minor device */
		rcode = posix_memalign( (void**) &localbuff, getpagesize(), devvec[rd_mptr->DEVICE].buff_size);
		devvec[rd_mptr->DEVICE].localbuff = localbuff;
		if( rcode ) {
			fprintf(stderr,"posix_memalign rcode=%d, device=%d\n", rcode, rd_mptr->DEVICE);
			fflush(stderr);
			taskexit(&rcode);
		}
		
		MUKDEBUG("Aligned Buffer size=%d on address %X, device=%d\n", devvec[rd_mptr->DEVICE].buff_size, devvec[rd_mptr->DEVICE].localbuff, rd_mptr->DEVICE);
		MUKDEBUG("Local Buffer %X\n", devvec[rd_mptr->DEVICE].localbuff);
		MUKDEBUG("Buffer size %d\n", devvec[rd_mptr->DEVICE].buff_size);
			
		devvec[rd_mptr->DEVICE].active = 1;
		MUKDEBUG("Device %d is active %d\n", rd_mptr->DEVICE, devvec[rd_mptr->DEVICE].active);
		
		/* Check device number on open. */
		if (rd_prepare(rd_mptr->DEVICE) == NIL_DEV) {
			MUKDEBUG("'rd_prepare()' %d - NIL_DEV:%d\n", rd_prepare(rd_mptr->DEVICE), NIL_DEV);
			rcode = ENXIO;
			return(rcode);
		}
 	
	}while(0);
	  
  devinfo_ptr  = &devvec[rd_mptr->DEVICE];
  MUKDEBUG(DEV_USR_FORMAT,DEV_USR_FIELDS(devinfo_ptr));

  return(rcode);
}

/*===========================================================================*
 *				rd_init					     *
 *===========================================================================*/
int rd_init(void )
{
	int i, rcode;
	
	MUKDEBUG("dcid=%d rd_ep=%d\n",dcid, rd_ep);

	rd_id = taskid();
	MUKDEBUG("rd_id=%d\n", rd_id);
	
	rcode = muk_tbind(dcid,rd_ep,"rdisk");
	MUKDEBUG("rd_ep=%d\n", rd_ep);
	if( rcode != rd_ep) {
		ERROR_PRINT(EDVSENDPOINT);
		taskexit(&rcode);
	}
	
	MUKDEBUG(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));
	MUKDEBUG("local_nodeid=%d\n", local_nodeid);
	
	MUKDEBUG(DC_USR1_FORMAT,DC_USR1_FIELDS(dc_ptr));
	MUKDEBUG(DC_USR2_FORMAT,DC_USR2_FIELDS(dc_ptr));

	MUKDEBUG("Get RDISK info\n");
	rd_task = get_task(rd_ep);
	MUKDEBUG(PROC_MUK_FORMAT,PROC_MUK_FIELDS(rd_task));
	
	/* Register into SYSTASK (as an autofork) */
	rd_lpid = taskid();
	MUKDEBUG("Register RDISK into SYSTASK rd_lpid=%d\n",rd_lpid);
	rd_ep = sys_bindproc(rd_ep, rd_lpid, LCL_BIND);
	if(rd_ep < 0) ERROR_TSK_EXIT(rd_ep);
			
	// set the name of RDISK 
	rcode = sys_rsetpname(rd_ep, "rdisk", local_nodeid);
	if(rcode < 0) ERROR_TSK_EXIT(rcode);
	
	for( i = 0; i < NR_DEVS; i++){
		if ( devvec[i].available != 0 ){
			MUKDEBUG("Byte offset to the partition start (Device = %d - img_ptr): %X\n", i, devvec[i].img_ptr);
			rd_geom[i].dv_base = cvul64(devvec[i].img_ptr);
			fprintf(stdout, "Byte offset to the partition start (rd_geom[DEV=%d].dv_base): %X\n", i, rd_geom[i].dv_base);
			fflush(stdout);
	
			MUKDEBUG("Number of bytes in the partition (Device = %d - img_size): %u\n", i, devvec[i].st_size);
			rd_geom[i].dv_size = cvul64(devvec[i].st_size);	
			fprintf(stdout, "Number of bytes in the partition (rd_geom[DEV=%d].dv_size): %u\n", i, rd_geom[i].dv_size);
			fflush(stdout);
			}
	}
	MUKDEBUG("END rd_init\n");
	return(OK);
}

/*===========================================================================*
 *				rd_geometry				     *
 *===========================================================================*/
void rd_geometry(struct partition *entry)
{
  /* Memory devices don't have a geometry, but the outside world insists. */
  entry->cylinders = div64u(rd_geom[rd_device].dv_size, SECTOR_SIZE) / (64 * 32);
  entry->heads = 64;
  entry->sectors = 32;
}

/*===========================================================================*
 *				rd_do_close										     *
 *===========================================================================*/
int rd_do_close(struct driver *dp, message *rd_mptr)
{
int rcode;

	// rcode = close(img_p);
	if (devvec[rd_mptr->DEVICE].active != 1) { 
		MUKDEBUG("Device %d, is not open\n", rd_mptr->DEVICE);
		rcode = -1; //MARIE: VER SI ESTO ES CORRECTO?
		}
	else{	
		MUKDEBUG("devvec[rd_mptr->DEVICE].img_p=%d\n",devvec[rd_mptr->DEVICE].img_p);
		rcode = close(devvec[rd_mptr->DEVICE].img_p);
		if(rcode < 0) ERROR_TSK_EXIT(errno); 
		
		MUKDEBUG("Close device number: %d\n", rd_mptr->DEVICE);
		devvec[rd_mptr->DEVICE].img_ptr = NULL;
		devvec[rd_mptr->DEVICE].img_p = NULL;
		devvec[rd_mptr->DEVICE].st_size = 0;
		devvec[rd_mptr->DEVICE].st_blksize = 0;
		devvec[rd_mptr->DEVICE].localbuff = NULL;
		devvec[rd_mptr->DEVICE].active = 0;
		devvec[rd_mptr->DEVICE].available = 0;
	
		MUKDEBUG("Buffer %X\n", devvec[rd_mptr->DEVICE].localbuff);
		free(devvec[rd_mptr->DEVICE].localbuff);
		MUKDEBUG("Free buffer\n");
		}
	// if(rcode < 0) ERROR_TSK_EXIT(errno); 
	
	
return(rcode);	
}

/*===========================================================================*
 *				rd_nop									     *
 *===========================================================================*/
int rd_nop(struct driver *dp, message *rd_mptr)
{
	MUKDEBUG(MSG2_FORMAT, MSG2_FIELDS(rd_mptr));
return(OK);	
}


/*===========================================================================*
 *				rd_do_dump									     *
 *===========================================================================*/
int rd_do_dump(struct driver *dp, message *rd_mptr)
{
	MUKDEBUG(MSG1_FORMAT, MSG1_FIELDS(rd_mptr));
	
	switch(rd_mptr->m1_i1){
		case DMP_RD_DEV:
			dmp_rd_dev();
			break;
		case WDMP_RD_DEV:
			wdmp_rd_dev(rd_mptr);
			break;		
		default:
			ERROR_RETURN(EDVSBADCALL);
	}
	return(OK);
}
	
/*===========================================================================*
 *				dmp_rd_dev									     *
 *===========================================================================*/
void dmp_rd_dev( void )
{	
	devvec_t *dv_ptr;
	int d;
	
	MUKDEBUG("\n");

	fprintf(dump_fd, "\n");
	fprintf(dump_fd,"===========================================================================\n");
	fprintf(dump_fd,"======================  dmp_rd_dev  =======================================\n");
	fprintf(dump_fd,"===========================================================================\n");
	fprintf(dump_fd, "\n");
	fprintf(dump_fd, "ifd -size_KB blk_size buf_size act image_name\n");	

	for( d = 0; d < NR_DEVS; d++){
		dv_ptr = &devvec[d];
		if(dv_ptr->available == 0) continue;
		assert( dv_ptr->img_ptr != NULL);
		assert( dv_ptr->localbuff != NULL);
		
		fprintf(dump_fd, "%3d %8d %8d %8d %3d %s\n",
			dv_ptr->img_p,
			(dv_ptr->st_size/1024),
			dv_ptr->st_blksize,
			dv_ptr->buff_size,
			dv_ptr->active,
			dv_ptr->img_ptr);
	}
	fprintf(dump_fd, "\n");

	return(OK);	
}

/*===========================================================================*
 *				wdmp_rd_dev									     *
 *===========================================================================*/
void wdmp_rd_dev( message *rd_mptr )
{	
	devvec_t *dv_ptr;
	int d;
	char *page_ptr;

	MUKDEBUG("\n");

	page_ptr = rd_mptr->m1_p1;
	
	(void)strcat(page_ptr,"<table>\n");
	(void)strcat(page_ptr,"<thead>\n");
	(void)strcat(page_ptr,"<tr>\n");
	(void)strcat(page_ptr,"<th>ifd</th>\n");
	(void)strcat(page_ptr,"<th>-size_KB</th>\n");
	(void)strcat(page_ptr,"<th>blk_size</th>\n");
	(void)strcat(page_ptr,"<th>buf_size</th>\n");
	(void)strcat(page_ptr,"<th>act</th>\n");
	(void)strcat(page_ptr,"<th>image_name</th>\n");
	(void)strcat(page_ptr,"</tr>\n");
	(void)strcat(page_ptr,"</thead>\n");
	(void)strcat(page_ptr,"<tbody>\n");
	

	for( d = 0; d < NR_DEVS; d++){
		dv_ptr = &devvec[d];
		if(dv_ptr->available == 0) continue;
		assert( dv_ptr->img_ptr != NULL);
		assert( dv_ptr->localbuff != NULL);
	    (void)strcat(page_ptr,"<tr>\n");
		sprintf(is_buffer, "<td>%3d</td> <td>%8d</td> <td>%8d</td> <td>%8d</td> <td>%3d</td> <td>%s</td>\n",
			dv_ptr->img_p,
			(dv_ptr->st_size/1024),
			dv_ptr->st_blksize,
			dv_ptr->buff_size,
			dv_ptr->active,
			dv_ptr->img_ptr);
		(void)strcat(page_ptr,is_buffer);
		(void)strcat(page_ptr,"</tr>\n");	
	}
  (void)strcat(page_ptr,"</tbody></table>\n");
}
