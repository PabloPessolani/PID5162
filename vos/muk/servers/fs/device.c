/* When a needed block is not in the cache, it must be fetched from the disk.
 * Special character files also require I/O.  The routines for these are here.
 *
 * The entry points in this file are:
 *   dev_open:   FS opens a device
 *   dev_close:  FS closes a device
 *   dev_io:   FS does a read or write on a device
 *   dev_status: FS processes callback request alert
 *   gen_opcl:   generic call to a task to perform an open/close
 *   gen_io:     generic call to a task to perform an I/O operation
 *   no_dev:     open/close processing for devices that don't exist
 *   no_dev_io:  i/o processing for devices that don't exist
 *   tty_opcl:   perform tty-specific processing for open/close
 *   ctty_opcl:  perform controlling-tty-specific processing for open/close
 *   ctty_io:    perform controlling-tty-specific processing for I/O
 *   fs_ioctl:   perform the IOCTL system call
 *   fs_setsid:  perform the SETSID system call (FS side)
 */

// #define MUKDBG    1

#include "fs.h"
#define ELEMENTS(a) (sizeof(a)/sizeof((a)[0]))

#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

int dmap_size;
int dummyproc;

/*===========================================================================*
 *				dev_open				     *
 *===========================================================================*/
int dev_open( mnx_dev_t dev, int proc, int flags)
// dev_t dev;			/* device to open */
// int proc;			 process to open for
// int flags;			/* mode bits and flags */
{
	int r, major, minor;
	dmap_t *dp;

	/* Determine the major device number call the device class specific
	 * open/close routine.  (This is the only routine that must check the
	 * device number for being in range.  All others can trust this check.)
	 */
	major = DEV2MAJOR(dev);
	minor = DEV2MINOR(dev);
	MUKDEBUG("dev=%d major=%d minor=%d proc=%d flags=%d\n", dev, major, minor , proc, flags);
	if (major >= NR_DEVICES) ERROR_RETURN(EDVSOVERRUN);
	dp = &MAJOR2TAB(major);

	if (dp->dmap_driver == NONE ) {
		fprintf(stderr, "FS: %s no driver for dev %x\n", __FUNCTION__, dev);
		ERROR_RETURN(EDVSNXIO);
	}
	
	/*Getting image filename for physical device drives*/
//	img_name = dp->filename;

	MUKDEBUG("CONFIGURED buffer_size =%d\n", dp->dmap_cfg.buffer_size);
	MUKDEBUG("_MAX_BLOCK_SIZE=%d\n", _MAX_BLOCK_SIZE);
	/*Setting the localbuffer and aligment*/
	dp->dmap_cfg.buffer_size = MIN(dp->dmap_cfg.buffer_size,_MAX_BLOCK_SIZE);
	MUKDEBUG("REAL buffer_size=%d\n", dp->dmap_cfg.buffer_size);

	r = (*dp->dmap_opcl)(DEV_OPEN, dev, proc, flags);
	if (r == SUSPEND) 
		ERROR_RETURN(EDVSGENERIC);

	MUKDEBUG("r=%d\n", r);
	return (r);
}

/*===========================================================================*
 *				dev_close				     *
 *===========================================================================*/
void dev_close(mnx_dev_t dev)
// dev_t dev;			/* device to close */
{
	dmap_t *dp;
	message dev_mess;
	int major, minor;
	
	/* Determine task dmap. */
	major = DEV2MAJOR(dev);
	minor = DEV2MINOR(dev);
	MUKDEBUG("dev=%d major=%d minor=%d\n", dev, major, minor);
	dp = &MAJOR2TAB(major);

	if (dp->dmap_driver == NONE ) {
		fprintf(stderr, "FS: %s no driver for dev %x\n", __FUNCTION__, dev);
		ERROR_RETURN(EDVSNXIO);
	}
	
	(void) (*dp->dmap_opcl)(DEV_CLOSE, dev, 0, 0);
}

/*===========================================================================*
 *        dev_io               *
 *===========================================================================*/
int dev_io(int op, mnx_dev_t dev, int proc_e, void *buf, mnx_off_t pos, int bytes, int flags)
// int op;        /* DEV_READ, DEV_WRITE, DEV_IOCTL, etc. */
// dev_t dev;     /* major-minor device number */
// int proc_e;      /* in whose address space is buf? */
// void *buf;      virtual address of the buffer
// off_t pos;     /* byte position */
// int bytes;     /* how many bytes to transfer */
// int flags;     /* special flags, like O_NONBLOCK */
{
	/* Read or write from a device.  The parameter 'dev' tells which one. */
	dmap_t *dp;
	message dev_mess, *m;
	int major, minor;
	
	/* Determine task dmap. */
	major = DEV2MAJOR(dev);
	minor = DEV2MINOR(dev);
	MUKDEBUG("op=%X dev=%d major=%d minor=%d proc_e=%d pos=%ld bytes=%d flags=%X\n", 
		op, dev, major, minor , proc_e, pos,  bytes, flags);
	dp = &MAJOR2TAB(major);

	if (dp->dmap_driver == NONE ) {
		fprintf(stderr, "FS: %s no driver for dev %x\n", __FUNCTION__, dev);
		ERROR_RETURN(EDVSNXIO);
	}

	switch (op) {
	case DEV_READ:
		MUKDEBUG(" DEV_READ -> m_type=%d, dev=%d, POS=%d, IO_ENDPT=%d, COUNT=%d\n",
			op, dev, pos, proc_e, bytes);
		break;
	case DEV_WRITE:
		MUKDEBUG(" DEV_WRITE -> m_type=%d, dev=%d, POS=%d, IO_ENDPT=%d, COUNT=%d\n", 
			op, dev, pos, proc_e, bytes);
		break;
	case DEV_OPEN:
		MUKDEBUG(" DEV_OPEN -> m_type=%d, dev=%d, POS=%d, IO_ENDPT=%d, COUNT=%d\n", 
			op, dev, pos, proc_e, bytes);
		break;
	case DEV_CLOSE:
		MUKDEBUG(" DEV_CLOSE -> m_type=%d, dev=%d, POS=%d, IO_ENDPT=%d, COUNT=%d\n", 
			op, dev, pos, proc_e, bytes);
		break;
	case DEV_SCATTER:
		MUKDEBUG(" DEV_SCATTER -> m_type=%d, dev=%d, POS=%d, IO_ENDPT=%d, COUNT=%d\n", 
			op, dev, pos, proc_e, bytes);
		break;
	case DEV_GATHER:
		MUKDEBUG(" DEV_GATHER -> m_type=%d, dev=%d, POS=%d, IO_ENDPT=%d, COUNT=%d\n", 
			op, dev, pos, proc_e, bytes);
		break;

	}

//MUKDEBUG(" m_type=%d, DEV=%d, POS=%d, IO_ENDPT=%d, COUNT=%d\n", op, (dev >> MNX_MINOR) & BYTE, pos, proc_e, bytes);

//MUKDEBUG("ANTES DE isokendpt %d\n", op);
//TODO: En suspenso por ahora hasta que tengamos PM
//if(isokendpt(dp->dmap_driver, &dummyproc) != OK) {
//  printf("FS: dev_io: old driver for dev %x (%d)\n",
//   dev, dp->dmap_driver);
//  return EDVSNXIO;
//}

	/* Set up the message passed to task. */
	dev_mess.m_type   = op;
	dev_mess.DEVICE   = minor;
	dev_mess.POSITION = pos;
	dev_mess.IO_ENDPT = proc_e;
	dev_mess.ADDRESS  = buf;
	dev_mess.COUNT    = bytes;
	dev_mess.TTY_FLAGS = flags;
	m= &dev_mess;
	MUKDEBUG("dmap_driver=%d " MSG2_FORMAT, dp->dmap_driver, MSG2_FIELDS(m));

	/* Call the task. */
	(*dp->dmap_io)(dp->dmap_driver, &dev_mess);

	if (dp->dmap_driver == NONE) {
		/* Driver has vanished. */
		return EDVSIO;
	}

// MUKDEBUG("DESPUES dp->dmap_driver == NONE. \n");

	/* Task has completed.  See if call completed. */
	if (dev_mess.REP_STATUS == SUSPEND) {
		if (flags & O_NONBLOCK) {
			/* Not supposed to block. */
			dev_mess.m_type = CANCEL;
			dev_mess.IO_ENDPT = proc_e;
			dev_mess.DEVICE = minor;
			(*dp->dmap_io)(dp->dmap_driver, &dev_mess);
			if (dev_mess.REP_STATUS == EDVSINTR) 
				dev_mess.REP_STATUS = EDVSAGAIN;
			dev_mess.REP_STATUS = EDVSNODEV;
		} else {
			/* Suspend user. */
			suspend(dp->dmap_driver);
			return (SUSPEND);
		}
	}
	MUKDEBUG("BEFORE return dev_mess.REP_STATUS=%d \n", dev_mess.REP_STATUS);

	return (dev_mess.REP_STATUS);
}

/*===========================================================================*
 *        r_gen_opcl             *
 * RDISK or other M3IPC device DISK OPEN/CLOSE
 *===========================================================================*/
int r_gen_opcl(int op, mnx_dev_t dev, int proc_e, int flags)
//int op;       /* operation, DEV_OPEN or DEV_CLOSE */
//dev_t dev;      /* device to open or close */
//int proc_e;     /* process to open/close for */
//int flags;      /* mode bits and flags */
{
	/* Called from the dmap struct in table.c on opens & closes of special files.*/
	dmap_t *dp;
	message dev_mess;
	int major, minor;
	
	/* Determine task dmap. */
	major = DEV2MAJOR(dev);
	minor = DEV2MINOR(dev);
	MUKDEBUG("op=%X dev=%d major=%d minor=%d proc_e=%d flags=%X\n", 
		op, dev, major, minor , proc_e, flags);
	dp = &MAJOR2TAB(major);
	if (dp->dmap_driver == NONE ) {
		fprintf(stderr, "FS: %s no driver for dev %x\n", __FUNCTION__, dev);
		ERROR_RETURN(EDVSNXIO);
	}

	dev_mess.m_type   = op;
	dev_mess.DEVICE   = minor;
	dev_mess.IO_ENDPT = proc_e;
	dev_mess.COUNT    = flags;

	// MUKDEBUG("Call the task %d\n", dp->dmap_driver);
	/* Call the task. */
	(*dp->dmap_io)(dp->dmap_driver, &dev_mess);

	return(dev_mess.REP_STATUS);
}

/*===========================================================================*
 *        f_gen_opcl             *
 * Local image FILE DISK OPEN/CLOSE
 *===========================================================================*/
int f_gen_opcl(int op, mnx_dev_t dev, int proc_e, int flags)
//int op;       /* operation, DEV_OPEN or DEV_CLOSE */
//dev_t dev;      /* device to open or close */
//int proc_e;     /* process to open/close for */
//int flags;      /* mode bits and flags */
{
	char *img_name;		/* name of the ram disk image file*/
	int	 img_fd;
	int r, fileDesc;
	dmap_t *dp;
	int major, minor;
	
	/* Determine task dmap. */
	major = DEV2MAJOR(dev);
	minor = DEV2MINOR(dev);
	MUKDEBUG("op=%X dev=%d major=%d minor=%d proc_e=%d flags=%X\n", 
		op, dev, major, minor , proc_e, flags);
	dp = &MAJOR2TAB(major);
	if (dp->dmap_driver == NONE ) {
		fprintf(stderr, "FS: %s no driver for dev %x\n", __FUNCTION__, dev);
		ERROR_RETURN(EDVSNXIO);
	}
//NOTA Diego Padula: Esto mediante un mapeo en "dmap" abre y cierra archivos,
//que es algo parecido a lo que voy a hacer abajo.
//ABRIR FD de DISCO
//CERRAR FD de DISCO

	img_name = dp->dmap_cfg.filename;
	img_fd   = dp->dmap_cfg.image_fd;
	MUKDEBUG("img_name=%s img_fd=%d\n",  img_name, img_fd);

	switch (op) {
		case DEV_OPEN:
			MUKDEBUG(" DEV_OPEN -> op=%d, dev=%d, proc_e=%d, flags=%d\n", op, dev, proc_e, flags);
			MUKDEBUG("OPENING file disk \n");
			MUKDEBUG("OPENING fileName %s\n", img_name );
			img_fd = open(img_name, O_RDWR);
			if (img_fd < 0){
				ERROR_PT_EXIT(errno);
			} 
			dp->dmap_cfg.image_fd = img_fd;
			MUKDEBUG("OPENED file disk succesfully, FD %d\n", img_fd);
			break;
		case DEV_CLOSE:
			MUKDEBUG(" DEV_CLOSE -> op=%d, dev=%d, proc_e=%d, flags=%d\n", op, dev, proc_e, flags);
			MUKDEBUG("CLOSING file image disk \n");
			MUKDEBUG("CLOSING fileName %s fd=%d\n", img_name, img_fd);
			r = close(img_fd);
			if (r != 0) {
				ERROR_PT_EXIT(errno);
			}
			MUKDEBUG("CLOSED file image disk succesfully, FD %d\n", img_fd);
			break;
		default:
			r = EDVSINVAL;
			ERROR_PT_EXIT(r);
	}

	return (OK);
}


/*===========================================================================*
 *        v_gen_opcl             *
 * virtual RAM DISK OPEN/CLOSE
 *===========================================================================*/
int v_gen_opcl(int op, mnx_dev_t dev, int proc, int flags)
{
	MUKDEBUG("op=%d dev=%X proc=%d flags=%X\n", op, dev, proc, flags);
	return (OK);
}

/*===========================================================================*
 *        n_gen_opcl             *
  * NDB DISK OPEN/CLOSE
 *===========================================================================*/
int n_gen_opcl(int op, mnx_dev_t dev, int proc, int flags)
{
	MUKDEBUG("op=%d dev=%X proc=%d flags=%X\n", op, dev, proc, flags);
	return (OK);
}

/*===========================================================================*
 *        fs_setsid            *
 *===========================================================================*/
int fs_setsid()
{
	/* Perform the FS side of the SETSID call, i.e. get rid of the controlling
	 * terminal of a process, and make the process a session leader.
	 */
	register fproc_t *rfp;
	int slot;

	/* Only MM may do the SETSID call directly. */
	if (fs_who_e != pm_ep) return (EDVSNOSYS);

	/* Make the process a session leader with no controlling tty. */
	okendpt(fs_m_in.endpt1, &slot);
	rfp = &fs_proc_table[slot];
	rfp->fp_sesldr = TRUE;
	rfp->fp_tty = 0;
	return (OK);
}

/*===========================================================================*
 *        fs_ioctl             *
 *===========================================================================*/
int fs_ioctl()
{
	/* Perform the ioctl(ls_fd, request, argx) system call (uses m2 fmt). */

	struct filp *f;
	register struct inode *rip;
	mnx_dev_t dev;

	if ( (f = get_filp(fs_m_in.ls_fd)) == NIL_FILP) ERROR_RETURN (fs_err_code);
	rip = f->filp_ino;    /* get inode pointer */
	
	MUKDEBUG(INODE_FORMAT1, INODE_FIELDS1(rip));
	MUKDEBUG("I_CHAR_SPECIAL=%X I_BLOCK_SPECIAL=%X\n",
		I_CHAR_SPECIAL, I_BLOCK_SPECIAL);

	if ( (rip->i_mode & I_TYPE) != I_CHAR_SPECIAL
	        && (rip->i_mode & I_TYPE) != I_BLOCK_SPECIAL) ERROR_RETURN (EDVSNOTTY);
	dev = (mnx_dev_t) rip->i_zone[0];
	MUKDEBUG("dev=%X\n",dev);
		
#if ENABLE_BINCOMPAT
	if ((fs_m_in.TTY_REQUEST >> 8) == 't') {
		/* Obsolete sgtty ioctl, message contains more than is sane. */
		struct dmap *dp;
		message dev_mess;

		dp = &dmap[(dev >> MNX_MAJOR) & BYTE];

		dev_mess = m; /* Copy full message with all the weird bits. */
		dev_mess.m_type   = DEV_IOCTL;
		dev_mess.PROC_NR  = fs_who_e;
		dev_mess.TTY_LINE = (dev >> MNX_MINOR) & BYTE;

		/* Call the task. */

		if (dp->dmap_driver == NONE) {
			printf("FS: fs_ioctl: no driver for dev %x\n", dev);
			return EDVSNXIO;
		}

		if (isokendpt(dp->dmap_driver, &dummyproc) != OK) {
			printf("FS: fs_ioctl: old driver for dev %x (%d)\n",
			       dev, dp->dmap_driver);
			return EDVSNXIO;
		}

		(*dp->dmap_io)(dp->dmap_driver, &dev_mess);

		fs_m_out.TTY_SPEK = dev_mess.TTY_SPEK; /* erase and kill */
		fs_m_out.TTY_FLAGS = dev_mess.TTY_FLAGS; /* flags */
		return (dev_mess.REP_STATUS);
	}
#endif

	return (dev_io(DEV_IOCTL, dev, fs_who_e, fs_m_in.ADDRESS, 0L,
	               fs_m_in.REQUEST, f->filp_flags));
}

/*===========================================================================*
 *       r_gen_io               *
 * RDISK  or other M3IPC device Generic IO
 *===========================================================================*/
int r_gen_io(int task_nr, message *mess_ptr)
// int task_nr;     /* which task to call */
// message *mess_ptr;   /* pointer to message for task */
{
	/* All file system I/O ultimately comes down to I/O on major/minor device
	 * pairs.  These lead to calls on the following routines via the dmap table.
	 */

	 /*
	 
	 Aca EN ESTA FUNCION probar la compresion y descompresion identificando el TYPE y los otros paramnetros
	 dentro del mensaje mess_ptr
	  */
	 int r, proc_e;

	MUKDEBUG("task_nr=%d " MSG2_FORMAT, task_nr, MSG2_FIELDS(mess_ptr));

	 proc_e = mess_ptr->IO_ENDPT;

	#if DEAD_CODE
	 while ((r = sendrec(task_nr, mess_ptr)) == ELOCKED) {
		/* sendrec() failed to avoid deadlock. The task 'task_nr' is
		 * trying to send a MOLREVIVE message for an earlier request.
		 * Handle it and go try again.
		 */
	  if ((r = receive(task_nr, &local_m)) != OK) {
	    break;
	  }

		/* If we're trying to send a cancel message to a task which has just
		 * sent a completion fs_reply, ignore the fs_reply and abort the cancel
		 * request. The caller will do the revive for the process.
		 */
	  if (mess_ptr->m_type == CANCEL && local_m.REP_ENDPT == proc_e) {
	    return OK;
	  }

		/* Otherwise it should be a MOLREVIVE. */
	  if (local_m.m_type != MOLREVIVE) {
	    printf(
	      "fs: strange device fs_reply from %d, type = %d, proc = %d (1)\n",
	      local_m.m_source,
	      local_m.m_type, local_m.REP_ENDPT);
	    continue;
	  }

	  revive(local_m.REP_ENDPT, local_m.REP_STATUS);
	}
	#endif

	  /* The message received may be a fs_reply to this call, or a MOLREVIVE for some
	   * other process.
	   */

	MUKDEBUG(" m_type=%d, COUNT/FLAGS=%d\n", mess_ptr->m_type, mess_ptr->COUNT);

	   r = dvk_sendrec(task_nr, mess_ptr);
	   for(;;) {
	     if (r != OK) {
	      if (r == EDVSDEADSRCDST || r == EDVSDSTDIED || r == EDVSSRCDIED) {
	       printf("fs: dead driver %d\n", task_nr);
	       dmap_unmap_by_endpt(task_nr);
	       return r;
	     }
	     if (r == EDVSLOCKED) {
	       printf("fs: ELOCKED talking to %d\n", task_nr);
	       return r;
	     }
	     panic(__FILE__,"call_task: can't send/receive", r);
	   }

	// MUKDEBUG("dvk_sendrec r=%d, \n", r);

	  	/* Did the process we did the sendrec() for get a result? */
	   if (mess_ptr->REP_ENDPT == proc_e) {
	    break;
	  } else if (mess_ptr->m_type == MOLREVIVE) {
			/* Otherwise it should be a MOLREVIVE. */
	    revive(mess_ptr->REP_ENDPT, mess_ptr->REP_STATUS);
	  } else {
	    printf(
	      "fs: strange device fs_reply from %d, type = %d, proc = %d (2) ignored\n",
	      mess_ptr->m_source,
	      mess_ptr->m_type, mess_ptr->REP_ENDPT);
	  }
	  r = dvk_receive(task_nr, mess_ptr);
	}

	// MUKDEBUG("dvk_receive r=%d, \n", r);

	return OK;
}

/*===========================================================================*
 *        f_gen_io               *
 * Local FILE generic IO  
 *===========================================================================*/
int f_gen_io(int task_nr, message *mess_ptr)
// int task_nr;     /* which task to call */
// message *mess_ptr;   /* pointer to message for task */
{
	/* All file system I/O ultimately comes down to I/O on major/minor device
	 * pairs.  These lead to calls on the following routines via the dmap table.
	 */
	char *img_name;		/* name of the ram disk image file*/
	int	 img_fd;
	
	int r, proc_e, rcode;
	int op, bytes, flags;
	void *buf;
	mnx_dev_t dev;
	mnx_off_t pos;

	unsigned int nr_req;    /* length of request vector */
	static iovec_t iovec[NR_IOREQS];
	phys_bytes iovec_size;
	vir_bytes user_vir;
	iovec_t *iov; /* pointer to read or write request vector */
	unsigned count, tbytes, stbytes; //left, chunk;

	int leido = 0;
	int escrito = 0;
	tbytes = 0;
	bytes = 0;
	int salida_fseek;

	MUKDEBUG("task_nr=%d " MSG2_FORMAT, task_nr, MSG2_FIELDS(mess_ptr));

	op = mess_ptr->m_type;
	dev = mess_ptr->DEVICE; // this is the minor number 
	pos = mess_ptr->POSITION;
	proc_e = mess_ptr->IO_ENDPT;
	bytes = (mess_ptr->COUNT > _MAX_BLOCK_SIZE) ? _MAX_BLOCK_SIZE : mess_ptr->COUNT;
	flags = mess_ptr->TTY_FLAGS;
	buf = mess_ptr->ADDRESS;

	img_name =  MAJOR2TAB(task_nr).dmap_cfg.filename;
	img_fd   =  MAJOR2TAB(task_nr).dmap_cfg.image_fd;
	MUKDEBUG("img_name=%s img_fd=%d\n",  img_name, img_fd);

	switch (op) {
	case DEV_READ:
		MUKDEBUG("DEV_READ %d\n", DEV_READ);
		if (img_fd < 0) ERROR_PT_EXIT(errno);
		MUKDEBUG("image FILE FD: %d \n", img_fd);
		MUKDEBUG("bytes to READ: %d \n", bytes);
		leido = pread(img_fd, buf, bytes, pos);
		MUKDEBUG("Bytes read: %d \n", leido);
		mess_ptr->REP_STATUS = leido;
//		memcpy(buf, localbuff, leido);
		break;
	case DEV_WRITE:
		MUKDEBUG("DEV_WRITE %d\n", DEV_WRITE);
		if (img_fd < 0) ERROR_PT_EXIT(errno);
		MUKDEBUG("image FILE FD: %d \n", img_fd);
		MUKDEBUG("bytes to WRITE: %d \n", bytes);
//		memcpy(localbuff, buf, bytes);
		escrito = pwrite(img_fd, buf, bytes, pos);
		MUKDEBUG("Bytes written: %d \n", escrito);
		mess_ptr->REP_STATUS = escrito;
		break;
	case DEV_GATHER:
		MUKDEBUG("DEV_GATHER %d\n", DEV_GATHER);
		nr_req = mess_ptr->COUNT;
		iovec_size = (phys_bytes) (nr_req * sizeof(iovec[0]));
		memcpy(iovec, buf, iovec_size);
		iov = iovec;

		while (nr_req > 0)
		{
			/* How much to transfer and where to / from. */
			count = iov->iov_size;
			// MUKDEBUG("count: %u\n", count);
			user_vir = iov->iov_addr;
			// MUKDEBUG("user_vir %X\n", user_vir);
			stbytes = 0;
			do {
				bytes = (count > _MAX_BLOCK_SIZE) ? _MAX_BLOCK_SIZE : count;
				// MUKDEBUG("bytes a LEER: %d \n", bytes);
				leido = pread(img_fd, user_vir, bytes, pos);
				// MUKDEBUG("Bytes leidos: %d \n", leido);
				mess_ptr->REP_STATUS = leido;
//				memcpy(user_vir, localbuff, leido);

				stbytes = stbytes + leido;

				pos += leido;
				iov->iov_addr += leido;
				user_vir = iov->iov_addr;
				// MUKDEBUG("user_vir (do-buffer) %X\n", user_vir);
				count -= leido;
			} while (count > 0);

			if ((iov->iov_size -= stbytes) == 0) { iov++; nr_req--; }  /*subtotal bytes, por cada iov_size según posición del vector*/

			tbytes += stbytes; /*total de bytes leídos o escritos*/
		}
		memcpy(buf, iovec, iovec_size);
		break;
	case DEV_SCATTER:
		MUKDEBUG("DEV_SCATTER %d\n", DEV_SCATTER);
		nr_req = mess_ptr->COUNT;
		iovec_size = (phys_bytes) (nr_req * sizeof(iovec[0]));
		memcpy(iovec, buf, iovec_size);
		iov = iovec;

		while (nr_req > 0)
		{
			/* How much to transfer and where to / from. */
			count = iov->iov_size;
			// MUKDEBUG("count: %u\n", count);
			user_vir = iov->iov_addr;
			// MUKDEBUG("user_vir %X\n", user_vir);
			stbytes = 0;
			do {
				bytes = (count > _MAX_BLOCK_SIZE) ? _MAX_BLOCK_SIZE : count;
				// MUKDEBUG("bytes a ESCRIBIR: %d \n", bytes);
//				memcpy(localbuff, user_vir, bytes);
				escrito = pwrite(img_fd, user_vir, bytes, pos);
				// MUKDEBUG("Bytes escritos: %d \n", escrito);
				mess_ptr->REP_STATUS = escrito;

				stbytes = stbytes + escrito;

				pos += escrito;
				iov->iov_addr += escrito;
				user_vir = iov->iov_addr;
				// MUKDEBUG("user_vir (do-buffer) %X\n", user_vir);
				count -= escrito;
			} while (count > 0);

			if ((iov->iov_size -= stbytes) == 0) { iov++; nr_req--; }  /*subtotal bytes, por cada iov_size según posición del vector*/

			tbytes += stbytes; /*total de bytes leídos o escritos*/
		}
		memcpy(buf, iovec, iovec_size);
		break;
	}

	return OK;
}



/*===========================================================================*
 *        v_gen_io               *
 * 
 *===========================================================================*/
int v_gen_io(int task_nr, message *mess_ptr)
// int task_nr;     /* which task to call */
// message *mess_ptr;   /* pointer to message for task */
{
	MUKDEBUG("task_nr=%d " MSG2_FORMAT, task_nr, MSG2_FIELDS(mess_ptr));
	return OK;
}

/*===========================================================================*
 *        n_gen_io               *
 * NDB  generic IO
 *===========================================================================*/
int n_gen_io(int task_nr, message *mess_ptr)
// int task_nr;     /* which task to call */
// message *mess_ptr;   /* pointer to message for task */
{
	int r, proc_e;

	MUKDEBUG("task_nr=%d " MSG2_FORMAT, task_nr, MSG2_FIELDS(mess_ptr));
	
	/* All file system I/O ultimately comes down to I/O on major/minor device
	 * pairs.  These lead to calls on the following routines via the dmap table.
	 */
	proc_e = mess_ptr->IO_ENDPT;

	/* The message received may be a fs_reply to this call, or a MOLREVIVE for some
	 * other process.
	 */
	r = dvk_sendrec(task_nr, mess_ptr);
	for(;;) {
		if (r != OK) {
			if (r == EDVSDEADSRCDST || r == EDVSDSTDIED || r == EDVSSRCDIED) {
				fprintf(stderr, "fs: dead driver %d\n", task_nr);
				dmap_unmap_by_endpt(task_nr);
				ERROR_RETURN(r);
			}
			if (r == EDVSLOCKED) {
				fprintf(stderr, "fs: ELOCKED talking to %d\n", task_nr);
				ERROR_RETURN(r);
			}
			ERROR_PT_EXIT(r);
		}

		/* Did the process we did the sendrec() for get a result? */
		if (mess_ptr->REP_ENDPT == proc_e) {
			break;
		} else if (mess_ptr->m_type == MOLREVIVE) {
			/* Otherwise it should be a MOLREVIVE. */
			revive(mess_ptr->REP_ENDPT, mess_ptr->REP_STATUS);
		} else {
			fprintf(stderr,
			"fs: strange device fs_reply from %d, type = %d, proc = %d (2) ignored\n",
				mess_ptr->m_source,
				mess_ptr->m_type, mess_ptr->REP_ENDPT);
		}
		r = dvk_receive(task_nr, mess_ptr);
	}

	return(OK);	
}


/*===========================================================================*
 *        no_dev               *
 *===========================================================================*/
int no_dev(int op, mnx_dev_t dev, int proc, int flags)
//int op;			/* operation, DEV_OPEN or DEV_CLOSE */
//mnx_dev_t dev;	/* device to open or close */
//int proc;			/* process to open/close for */
//int flags;		/* mode bits and flags */
{
	/* Called when opening a nonexistent device. */
	MUKDEBUG("op=%d dev=%X proc=%d flags=%X\n", op, dev, proc, flags);

	return (EDVSNODEV);
}

/*===========================================================================*
 *        no_dev_io            *
 *===========================================================================*/
int no_dev_io(int proc, message *m)
{
	/* Called when doing i/o on a nonexistent device. */
	MUKDEBUG("proc=%d " MSG2_FORMAT, proc, MSG2_FIELDS(m));
	printf("FS: I/O on unmapped device number\n");
	return (EDVSIO);
}

/*===========================================================================*
 *        dev_up               *
 *===========================================================================*/
void dev_up(int maj)
{
	/* A new device driver has been mapped in. This function
	 * checks if any filesystems are mounted on it, and if so,
	 * dev_open()s them so the filesystem can be reused.
	 */
	struct super_block *sb;
	struct filp *fs_proc;
	int r;
	
	MUKDEBUG("maj=%d " , maj );

	/* Open a device once for every filp that's opened on it,
	 * and once for every filesystem mounted from it.
	 */

	for (sb = super_block; sb < &super_block[NR_SUPERS]; sb++) {
		int minor;
		if (sb->s_dev == NO_DEV)
			continue;
		if (((sb->s_dev >> MNX_MAJOR) & BYTE) != maj)
			continue;
		minor = ((sb->s_dev >> MNX_MINOR) & BYTE);
		printf("FS: remounting dev %d/%d\n", maj, minor);
		if ((r = dev_open(sb->s_dev, fs_ep,
		                  sb->s_rd_only ? R_BIT : (R_BIT | W_BIT))) != OK) {
			printf("FS: mounted dev %d/%d re-open failed: %d.\n",
			       maj, minor, r);
		}
	}

	for (fs_proc = filp; fs_proc < &filp[NR_FILPS]; fs_proc++) {
		struct inode *in;
		int minor;

		if (fs_proc->filp_count < 1 || !(in = fs_proc->filp_ino)) continue;
		if (((in->i_zone[0] >> MNX_MAJOR) & BYTE) != maj) continue;
		if (!(in->i_mode & (I_BLOCK_SPECIAL | I_CHAR_SPECIAL))) continue;

		minor = ((in->i_zone[0] >> MNX_MINOR) & BYTE);

		printf("FS: reopening special %d/%d..\n", maj, minor);

		if ((r = dev_open(in->i_zone[0], fs_ep,
		                  in->i_mode & (R_BIT | W_BIT))) != OK) {
			int n;
			/* This function will set the fp_filp[]s of processes
			 * holding that fs_proc to NULL, but _not_ clear
			 * fp_filp_inuse, so that fd can't be recycled until
			 * it's close()d.
			 */
			n = inval_filp(fs_proc);
			if (n != fs_proc->filp_count)
				printf("FS: warning: invalidate/count "
				       "discrepancy (%d, %d)\n", n, fs_proc->filp_count);
			fs_proc->filp_count = 0;
			printf("FS: file on dev %d/%d re-open failed: %d; "
			       "invalidated %d fd's.\n", maj, minor, r, n);
		}
	}

	return;
}

/*===========================================================================*
 *				d_gen_io					     *
 *===========================================================================*/
int d_gen_io(task_nr, mess_ptr)
int task_nr;			/* which task to call */
message *mess_ptr;		/* pointer to message for task */
{
/* All file system I/O ultimately comes down to I/O on major/minor device
 * pairs.  These lead to calls on the following routines via the dmap table.
 */

  int r, proc_e;

	proc_e = mess_ptr->IO_ENDPT;

  /* The message received may be a fs_reply to this call, or a REVIVE for some
   * other process.
   */
	MUKDEBUG("task_nr=%d proc_e=%d " MSG2_FORMAT, task_nr, proc_e, MSG2_FIELDS(mess_ptr));
	r = dvk_sendrec(task_nr, mess_ptr);
	MUKDEBUG(MSG2_FORMAT, MSG2_FIELDS(mess_ptr));

	for(;;) {
		if (r != OK) {
			if (r == EDVSDEADSRCDST || r == EDVSDSTDIED || r == EDVSSRCDIED) {
				fprintf(stderr, "fs: dead driver %d\n", task_nr);
				dmap_unmap_by_endpt(task_nr);
				ERROR_RETURN(r);
			}
			if (r == EDVSLOCKED) {
				fprintf(stderr, "fs: EDVSLOCKED talking to %d\n", task_nr);
				ERROR_RETURN(r);
			}
			ERROR_PT_EXIT(r);
		}

		/* Did the process we did the sendrec() for get a result? */
		if (mess_ptr->REP_ENDPT == proc_e) {
			MUKDEBUG("proc_e=%d  REP_ENDPT=%d\n" , proc_e, mess_ptr->REP_ENDPT );
			break;
		} else if (mess_ptr->m_type == MOLREVIVE) {
			/* Otherwise it should be a REVIVE. */
			revive(mess_ptr->REP_ENDPT, mess_ptr->REP_STATUS);
		} else {
			fprintf(stderr,
			"fs: strange device fs_reply from %d, type = %d, proc = %d (2) ignored\n",
				mess_ptr->m_source,
				mess_ptr->m_type, mess_ptr->REP_ENDPT);
		}
		r = dvk_receive(task_nr, mess_ptr);
		MUKDEBUG("task_nr=%d " MSG2_FORMAT, task_nr, MSG2_FIELDS(mess_ptr));
	}

  return(OK);
}

/*===========================================================================*
 *				d_gen_opcl				     *
 *===========================================================================*/
int d_gen_opcl(op, dev, proc_e, flags)
int op;				/* operation, DEV_OPEN or DEV_CLOSE */
mnx_dev_t dev;			/* device to open or close */
int proc_e;			/* process to open/close for */
int flags;			/* mode bits and flags */
{
/* Called from the dmap struct in table.c on opens & closes of special files.*/
	dmap_t *dp;
  message dev_mess;
  int major;

   	MUKDEBUG("op=%X dev=%X proc_e=%d flags=%X\n", op, dev, proc_e, flags);

  /* Determine task dmap. */
  	major = DEV2MAJOR(dev);
	dp = &MAJOR2TAB(major);

  dev_mess.m_type   = op;
  dev_mess.DEVICE   = DEV2MINOR(dev);
  dev_mess.IO_ENDPT = proc_e;
  dev_mess.COUNT    = flags;

  if (dp->dmap_driver == NONE) {
	fprintf(stderr, "FS: gen_opcl: no driver for dev %x\n", dev);
	ERROR_RETURN(EDVSNXIO);
  }
  if(isokendpt(dp->dmap_driver, &dummyproc) != OK) {
	fprintf(stderr,"FS: gen_opcl: old driver for dev %x (%d)\n",
		dev, dp->dmap_driver);
	ERROR_RETURN(EDVSNXIO);
  }

  /* Call the task. */
  (*dp->dmap_io)(dp->dmap_driver, &dev_mess);

  MUKDEBUG("REP_STATUS=%X\n", dev_mess.REP_STATUS);

  return(dev_mess.REP_STATUS);
}

