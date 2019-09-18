/*minor device struct*/

struct mdevvec {		/* vector for minor devices */
  char *img_ptr;		/* pointer - file name to the ram disk image */
  int img_p; 			/*file descriptor - disk image*/
  off_t st_size;    	/* of stat */
  blksize_t st_blksize; /* of stat */
  unsigned *localbuff;	/* buffer to the device*/
  unsigned	buff_size;	/* buffer size for this device*/
  int active;			/* if device active for open value=1, else 0 */
  int available;		/* if device is available to use value=1, else 0. For example, its in configure file*/
  // int replicated;		
  /* agregar flags para replicate, bitmap nodes, nr_nodes, compresión, encriptado*/
};

typedef struct mdevvec devvec_t;

#define DEV_USR_FORMAT "img_ptr=%s img_p=%d st_size=%d st_blksize=%d localbuff=%X buff_size=%d active=%d available=%d\n"
#define DEV_USR_FIELDS(p) p->img_ptr,p->img_p, p->st_size, p->st_blksize, p->localbuff,p->buff_size, p->active, p->available

//crear el otro vector y declararlo en rdisk.h
struct sumdevvec {					/* vector for minor devices - summary */
  unsigned long nr_transfered;		/* blocks trasnfered */
  unsigned long nr_matched;			/* blocks mached */
  unsigned long nr_updated;			/* blocks updated */
  unsigned long tbytes;				/* total bytes */
};
		
typedef struct sumdevvec sumdevvec_t;

#define SUM_USR_FORMAT "nr_transfered=%u nr_matched=%u nr_updated=%u tbytes=%u\n"
#define SUM_USR_FIELDS(sum) sum->nr_transfered,sum->nr_matched, sum->nr_updated, sum->tbytes

