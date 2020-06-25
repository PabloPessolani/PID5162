/*minor device struct*/

struct devvec_s {				/* vector for minor devices */
	// fields from configuration options 
	char        *dev_name;
	char 		*img_ptr;		/* pointer - file name to the ram disk image */
	unsigned	buff_size;		/* buffer size for this device*/
	unsigned	img_type;		/* device type  */
	int			volatile_flag; 
	int			replica_flag; 
	
	// internal fields 
	int 		img_fd; 		/*file descriptor - disk image*/
	int			dev_owner;			/* client endpoint which owns/opens the device */
	off_t 		st_size;    	/* of stat */
	blksize_t 	st_blksize; 	/* of stat */
	unsigned 	*localbuff;		/* buffer to the device*/
	int 		active_flag;		/* if device active_flag for open value=1, else 0 */
	int 		available;		/* if device is available to use value=1, else 0. For example, its in configure file*/
	mnx_part_t 	part;
};
typedef struct devvec_s devvec_t;

#define DEV_USR1_FORMAT "img_fd=%d dev_owner=%d st_size=%d st_blksize=%d localbuff=%X active=%d available=%d\n"
#define DEV_USR1_FIELDS(p) p->img_fd, p->dev_owner, p->st_size, p->st_blksize, p->localbuff, p->active_flag, p->available

#define DEV_USR2_FORMAT "img_ptr=%s buff_size=%d img_type=%d\n"
#define DEV_USR2_FIELDS(p) p->img_ptr, p->buff_size, p->img_type

#define DEV_USR3_FORMAT "volatile=%X replica=%d \n"
#define DEV_USR3_FIELDS(p) p->volatile_flag, p->replica_flag

//crear el otro vector y declararlo en rdisk.h
struct sumdevvec {					/* vector for minor devices - summary */
  unsigned long nr_transferred;		/* blocks transferred */
  unsigned long nr_matched;			/* blocks matched */
  unsigned long nr_updated;			/* blocks updated */
  unsigned long tbytes;				/* total bytes */
};
		
typedef struct sumdevvec sumdevvec_t;

#define SUM_USR_FORMAT "nr_transferred=%u nr_matched=%u nr_updated=%u tbytes=%u\n"
#define SUM_USR_FIELDS(sum) sum->nr_transferred,sum->nr_matched, sum->nr_updated, sum->tbytes

#define DFT_HEADS		2
#define DFT_SECTORS 	32 

