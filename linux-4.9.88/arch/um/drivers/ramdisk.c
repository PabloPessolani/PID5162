
#include <linux/module.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/ata.h>
#include <linux/hdreg.h>
#include <linux/cdrom.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <asm/tlbflush.h>
#include <kern_util.h>
#include "mconsole_kern.h"
#include <init.h>
#include <irq_kern.h>
#include <os.h>
#include "ramdisk.h"


enum {
	RAM_SIMPLE  = 0,	/* The extra-simple request function */
	RAM_FULL    = 1,	/* The full-blown version */
	RAM_NOQUEUE = 2,	/* Use make_request */
};
static int request_mode = RAM_SIMPLE;

/*
 * The internal representation of our device.
 */
struct ramdisk_dev {
        int size;                       /* Device size in sectors */
        u8 *data;                       /* The data array */
        short users;                    /* How many users */
        short media_change;             /* Flag a media change? */
        spinlock_t lock;                /* For mutual exclusion */
        struct request_queue *queue;    /* The device request queue */
        struct gendisk *gd;             /* The gendisk structure */
        struct timer_list timer;        /* For simulated media changes */
};

static struct ramdisk_dev *ram_dev = NULL;

static int ram_open(struct block_device *bdev, fmode_t mode);
static void ram_release(struct gendisk *disk, fmode_t mode);
static int ram_ioctl(struct block_device *bdev, fmode_t mode,
		     unsigned int cmd, unsigned long arg);
static int ram_getgeo(struct block_device *bdev, struct hd_geometry *geo);
int ram_get_geometry(struct ramdisk_dev *dev, int minor, struct hd_geometry *geo);
int ram_media_changed(struct gendisk *gd);
int ram_revalidate(struct gendisk *gd);

/*
 * The device operations structure.
 */
static struct block_device_operations ram_ops = {
	.owner           = THIS_MODULE,
	.open 	         = ram_open,
	.release 	 	 = ram_release,
	.media_changed   = ram_media_changed,
	.revalidate_disk = ram_revalidate,
	.ioctl	         = ram_ioctl
};

static int bytes_to_sectors_checked(unsigned long bytes)
{
	if( bytes % KERNEL_SECTOR_SIZE )
	{
		printk("***************WhatTheFuck***********************\n");
	}
	return bytes / KERNEL_SECTOR_SIZE;
}

/*
 * Handle an I/O request.
 */
static void ram_transfer(struct ramdisk_dev *dev, unsigned long sector,
		unsigned long nsect, char *buffer, int write)
{
	unsigned long offset = sector*KERNEL_SECTOR_SIZE;
	unsigned long nbytes = nsect*KERNEL_SECTOR_SIZE;

	if ((offset + nbytes) > dev->size) {
		printk (KERN_NOTICE "Beyond-end write (%ld %ld)\n", offset, nbytes);
		return;
	}
	if (write)
		memcpy(dev->data + offset, buffer, nbytes);
	else
		memcpy(buffer, dev->data + offset, nbytes);
}

/*
 * The simple form of the request function.
 */
static void ram_request(struct request_queue *q)
{
	struct request *req;
	int ret;

	req = blk_fetch_request(q);
	while (req) {
		struct ramdisk_dev *dev = req->rq_disk->private_data;
		if (blk_rq_is_passthrough(req)) {
			printk (KERN_NOTICE "Skip non-fs request\n");
			ret = -EIO;
			goto done;
		}
		printk (KERN_NOTICE "Req dev %u dir %d sec %ld, nr %d\n",
			(unsigned)(dev - ram_dev), rq_data_dir(req),
			blk_rq_pos(req), blk_rq_cur_sectors(req));
		ram_transfer(dev, blk_rq_pos(req), blk_rq_cur_sectors(req),
				bio_data(req->bio), rq_data_dir(req));
		ret = 0;
	done:
		if(!__blk_end_request_cur(req, ret)){
			req = blk_fetch_request(q);
		}
	}
}

/*
 * Transfer a single BIO.
 */
static int ram_xfer_bio(struct ramdisk_dev *dev, struct bio *bio)
{
	struct bio_vec bvec;
	struct bvec_iter iter;
	sector_t sector = bio->bi_iter.bi_sector;

	/* Do each segment independently. */
	bio_for_each_segment(bvec, bio, iter) {
		char *buffer = __bio_kmap_atomic(bio, iter);
		ram_transfer(dev, sector,bytes_to_sectors_checked(bio_cur_bytes(bio)),
				buffer, bio_data_dir(bio) == WRITE);
		sector += (bytes_to_sectors_checked(bio_cur_bytes(bio)));
		__bio_kunmap_atomic(bio);
	}
	return 0; /* Always "succeed" */
}


/*
 * Transfer a full request.
 */
static int ram_xfer_request(struct ramdisk_dev *dev, struct request *req)
{
	struct bio *bio;
	int nsect = 0;
    
	__rq_for_each_bio(bio, req) {
		ram_xfer_bio(dev, bio);
		nsect += bio->bi_iter.bi_size/KERNEL_SECTOR_SIZE;
	}
	return nsect;
}



/*
 * Smarter request function that "handles clustering".
 */
static void ram_full_request(struct request_queue *q)
{
	struct request *req;
	struct ramdisk_dev *dev = q->queuedata;
	int ret;

	while ((req = blk_fetch_request(q)) != NULL) {
		if (blk_rq_is_passthrough(req)) {
			printk (KERN_NOTICE "Skip non-fs request\n");
			ret = -EIO;
			goto done;
		}
		ram_xfer_request(dev, req);
		ret = 0;
	done:
		__blk_end_request_all(req, ret);
	}
}



/*
 * The direct make request version.
 */
static blk_qc_t ram_make_request(struct request_queue *q, struct bio *bio)
{
	struct ramdisk_dev *dev = q->queuedata;
	int status;

	status = ram_xfer_bio(dev, bio);
	bio->bi_status = status;
	bio_endio(bio);
	return BLK_QC_T_NONE;
}

static int ram_open(struct block_device *bdev, fmode_t mode)
{
	struct ramdisk_dev *dev = bdev->bd_disk->private_data;

	del_timer_sync(&dev->timer);
	spin_lock(&dev->lock);
	if (! dev->users) 
		check_disk_change(bdev);
	dev->users++;
	spin_unlock(&dev->lock);
	return 0;
}

static void ram_release(struct gendisk *disk, fmode_t mode)
{
	struct ramdisk_dev *dev = disk->private_data;

	spin_lock(&dev->lock);
	dev->users--;

	if (!dev->users) {
		dev->timer.expires = jiffies + INVALIDATE_DELAY;
		add_timer(&dev->timer);
	}
	spin_unlock(&dev->lock);

}

/*
 * Look for a (simulated) media change.
 */
int ram_media_changed(struct gendisk *gd)
{
	struct ramdisk_dev *dev = gd->private_data;
	
	return dev->media_change;
}

/*
 * Revalidate.  WE DO NOT TAKE THE LOCK HERE, for fear of deadlocking
 * with open.  That needs to be reevaluated.
 */
int ram_revalidate(struct gendisk *gd)
{
	struct ramdisk_dev *dev = gd->private_data;
	
	if (dev->media_change) {
		dev->media_change = 0;
		memset (dev->data, 0, dev->size);
	}
	return 0;
}

/*
 * The "invalidate" function runs out of the device timer; it sets
 * a flag to simulate the removal of the media.
 */
void ram_invalidate(unsigned long ldev)
{
	struct ramdisk_dev *dev = (struct ramdisk_dev *) ldev;

	spin_lock(&dev->lock);
	if (dev->users || !dev->data) 
		printk (KERN_WARNING "sbull: timer sanity check failed\n");
	else
		dev->media_change = 1;
	spin_unlock(&dev->lock);
}

/*
 * The ioctl() implementation
 */

int ram_ioctl (struct block_device *bdev,
		 fmode_t mode,
                 unsigned int cmd, unsigned long arg)
{
	long size;
	struct hd_geometry geo;
	struct ramdisk_dev *dev = bdev->bd_disk->private_data;

	switch(cmd) {
	    case HDIO_GETGEO:
        	/*
		 * Get geometry: since we are a virtual device, we have to make
		 * up something plausible.  So we claim 16 sectors, four heads,
		 * and calculate the corresponding number of cylinders.  We set the
		 * start of data at sector four.
		 */
		size = dev->size*(ramsect_size/KERNEL_SECTOR_SIZE);
		geo.cylinders 	= RAM_MEM_CYLINDERS;
		geo.heads 		= RAM_MEM_HEADS;
		geo.sectors 	= RAM_MEM_SECTORS;
		geo.start 		= 0;
		if (copy_to_user((void __user *) arg, &geo, sizeof(geo)))
			return -EFAULT;
		return 0;
	}

	return -ENOTTY; /* unknown command */
}

/*
 * Set up our internal device.
 */
static void ram_setup(struct ramdisk_dev *dev, int which)
{
	memset (dev, 0, sizeof (struct ramdisk_dev));
	ramsect_size = RAM_MEM_CYLINDERS * RAM_MEM_HEADS * RAM_MEM_SECTORS;
	DVKDEBUG(INTERNAL,"which=%d ramsect_size=%d\n", which, ramsect_size);

	dev->size = ramsect_size * RAM_SECTOR_SIZE;
	
	if (dev->data == NULL) {
		printk (KERN_NOTICE "vmalloc failure.\n");
		return;
	}
	spin_lock_init(&dev->lock);
	
	/*
	 * The timer which "invalidates" the device.
	 */
	init_timer(&dev->timer);
	dev->timer.data = (unsigned long) dev;
	dev->timer.function = ram_invalidate;
	
	/*
	 * The I/O queue, depending on whether we are using our own
	 * make_request function or not.
	 */
	switch (request_mode) {
	    case RAM_NOQUEUE:
		dev->queue = blk_alloc_queue(GFP_KERNEL);
		if (dev->queue == NULL)
			goto out_vfree;
		blk_queue_make_request(dev->queue, ram_make_request);
		break;

	    case RAM_FULL:
		dev->queue = blk_init_queue(ram_full_request, &dev->lock);
		if (dev->queue == NULL)
			goto out_vfree;
		break;

	    default:
		printk(KERN_NOTICE "Bad request mode %d, using simple\n", request_mode);
        	/* fall into.. */
	
	    case RAM_SIMPLE:
		dev->queue = blk_init_queue(ram_request, &dev->lock);
		if (dev->queue == NULL)
			goto out_vfree;
		break;
	}
	blk_queue_logical_block_size(dev->queue, RAM_SECTOR_SIZE);
	dev->queue->queuedata = dev;
	/*
	 * And the gendisk structure.
	 */
	dev->gd = alloc_disk(RAM_MAX_DEV);
	if (! dev->gd) {
		printk (KERN_NOTICE "alloc_disk failure\n");
		goto out_vfree;
	}
	dev->gd->major = ram_major;
	dev->gd->first_minor = which*RAM_MAX_DEV;
	dev->gd->fops = &ram_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;
	snprintf (dev->gd->disk_name, 32, "mem%c", which + 'a');
	set_capacity(dev->gd, ramsect_size/KERNEL_SECTOR_SIZE);
	add_disk(dev->gd);
	return;

  out_vfree:
	if (dev->data)
		vfree(dev->data);
}


static int __init ram_init(void)
{
	char *error;
	int i, err;

	DVKDEBUG(INTERNAL,"\n");

	platform_driver_register(&ram_driver);
	
	ram_dev = kmalloc(RAM_MAX_DEV*sizeof (struct ramdisk_dev), GFP_KERNEL);
	if (ram_dev == NULL)
		goto out_unregister;
	
	
	
	mutex_lock(&ram_lock);
	for (i = 0; i < RAM_MAX_DEV; i++){
		ram_setup(ram_dev[i]  + i, i);
		
		ram_disk[i] = vmalloc( RAM_MEM_CYLINDERS * RAM_MEM_HEADS * RAM_MEM_SECTORS * RAM_SECTOR_SIZE,
					 GFP_ATOMIC);
		if(ram_disk[i] == NULL)
			ERROR_RETURN(EDVSNOMEM);
		err = ram_add(i, &error);
		if(err)
			printk(KERN_ERR "Failed to initialize ramdisk device %d :"
			       "%s\n", i, error);
	}
	mutex_unlock(&ram_lock);

out_unregister:
	unregister_blkdev(RAM_MAJOR, "ramdisk");
	
	return 0;
}

late_initcall(ram_init);

#ifdef ANULADO 

========================================== HASTA AQUI ======================================
char *ram_disk;


 
enum ram_req { RAM_READ, RAM_WRITE, RAM_FLUSH };

struct ram_thread_req {
	struct request *req;
	enum ram_req op;
	unsigned long long offset;
	unsigned long length;
	char *buffer;
	int sectorsize;
	int error;
};

#define RAM_REQ_FORMAT 		"op=%d offset=%lld length=%ld sectorsize=%d error=%d\n"
#define RAM_REQ_FIELDS(p) 	p->op, p->offset, p->length, p->sectorsize, p->error

#define RDISK_NAME "ramdisk"
#define RAM_MAX_SG 	1

struct ramdisk {
	struct list_head ram_restart;
	int count;
	__u64 size;
	struct openflags boot_openflags;
	struct openflags openflags;
	struct platform_device pdev;
	struct request_queue *queue;
	spinlock_t lock;
	struct scatterlist sg[RAM_MAX_SG];
	struct request *request;
	int start_sg, end_sg;
	sector_t rq_pos;
};

#define RAM_OPEN_FLAGS ((struct openflags) { .r = 1, .w = 1, .s = 1, .c = 0, .cl = 1 })
static struct openflags global_openflags = RAM_OPEN_FLAGS;

#define DEFAULT_RDISK { \
	.count =		0, \
	.size =			-1, \
	.boot_openflags =	RAM_OPEN_FLAGS, \
	.openflags =		RAM_OPEN_FLAGS, \
	.lock =			__SPIN_LOCK_UNLOCKED(ram_devs.lock), \
	.request =		NULL, \
	.start_sg =		0, \
	.end_sg =		0, \
	.rq_pos =		0, \
}

static struct ramdisk ram_devs[RAM_MAX_DEV] = { [0 ... RAM_MAX_DEV - 1] = DEFAULT_RDISK };

static int 	  fake_ram_major = RAM_MAJOR;
static struct gendisk *fake_ram_gendisk[RAM_MAX_DEV];
static int 		fake_ram_ide = 0;

#define GEO_FORMAT  	"cyl=%d heads=%d sec=%d\n"
#define GEO_FIELDS(p)    p->cylinders, p->heads, p->sectors
#define RAMDISK_NAME 	"ramdisk"

static DEFINE_MUTEX(ram_lock);
static DEFINE_MUTEX(ram_mutex); /* replaces BKL, might not be needed */

/* Protected by ram_lock */
static int fake_major = RAM_MAJOR;
static struct gendisk *ram_gendisk[RAM_MAX_DEV];
static struct gendisk *fake_gendisk[RAM_MAX_DEV];

/* Protected by ram_lock */
static struct gendisk *ram_gendisk[RAM_MAX_DEV];
#define GEO_FORMAT  	"cyl=%d heads=%d sec=%d\n"
#define GEO_FIELDS(p)    p->cylinders, p->heads, p->sectors

int ram_dev_size(struct ramdisk *ram_dev, __u64 *size)
{
	struct hd_geometry geo,  *g_ptr;
	int rcode, major, minor;

	DVKDEBUG(INTERNAL,"\n");

	major = RAM_MAJOR;
	minor = RAM_MINOR; // <<<<< ver de donde se puede obtener este 

	rcode = ram_get_geometry(ramdisk_dev, minor, &geo);
	if( rcode < 0) ERROR_RETURN(rcode);
	
	g_ptr = &geo;
	DVKDEBUG(INTERNAL, GEO_FORMAT, GEO_FIELDS(g_ptr));
	*size = g_ptr->heads * g_ptr->sectors * g_ptr->cylinders * RAM_SECTOR_SIZE;
	DVKDEBUG(INTERNAL, "RDISK size=%lld \n",*size);

	return(OK);	
}

static int parse_unit(char **ptr)
{
	char *str = *ptr, *end;
	int n = -1;

	if(isdigit(*str)) {
		n = simple_strtoul(str, &end, 0);
		if(end == str)
			return -1;
		*ptr = end;
	}
	else if (('a' <= *str) && (*str <= 'z')) {
		n = *str - 'a';
		str++;
		*ptr = str;
	}
	return n;
}

/* If *index_out == -1 at exit, the passed option was a general one;
 * otherwise, the str pointer is used (and owned) inside ram_devs array, so it
 * should not be freed on exit.
 */
static int ram_setup_common(char *str, int *index_out, char **error_out)
{
	DVKDEBUG(INTERNAL,"\n");

     if( index_out != NULL)
		*index_out = 0; 
	return 0;
}

static int ram_setup(char *str)
{
	char *error;
	int err;

	DVKDEBUG(INTERNAL,"\n");

	err = ram_setup_common(str, NULL, &error);
	if(err)
		printk(KERN_ERR "Failed to initialize device with \"%s\" : "
		       "%s\n", str, error);
	return 1;
}

__setup("ramdisk", ram_setup);
__uml_help(ram_setup,
"ramdisk<n><flags>=<filename>[(:|,)<filename2>]\n"
"    This is used to associate a device with a file in the underlying\n"
"    filesystem. When specifying two filenames, the first one is the\n"
"    COW name and the second is the backing file name. As separator you can\n"
"    use either a ':' or a ',': the first one allows writing things like;\n"
"	ubd0=~/Uml/root_cow:~/Uml/root_backing_file\n"
"    while with a ',' the shell would not expand the 2nd '~'.\n"
"    When using only one filename, UML will detect whether to treat it like\n"
"    a COW file or a backing file. To override this detection, add the 'd'\n"
"    flag:\n"
"	ubd0d=BackingFile\n"
"    Usually, there is a filesystem in the file, but \n"
"    that's not required. Swap devices containing swap files can be\n"
"    specified like this. Also, a file which doesn't contain a\n"
"    filesystem can have its contents read in the virtual \n"
"    machine by running 'dd' on the device. <n> must be in the range\n"
"    0 to 7. Appending an 'r' to the number will cause that device\n"
"    to be mounted read-only. For example ubd1r=./ext_fs. Appending\n"
"    an 's' will cause data to be written to disk on the host immediately.\n"
"    'c' will cause the device to be treated as being shared between multiple\n"
"    UMLs and file locking will be turned off - this is appropriate for a\n"
"    cluster filesystem and inappropriate at almost all other times.\n\n"
);

static void do_ram_request(struct request_queue * q);

static LIST_HEAD(ram_restart);
static int ram_thread_fd = -1;


/* XXX - move this inside ram_intr. */
/* Called without dev->lock held, and only in interrupt context. */
static void ram_handler(void)
{
	struct ram_thread_req *req;
	struct ramdisk *ramdisk;
	struct list_head *list, *next_ele;
	unsigned long flags;
	int n;

	while(1){
		n = os_read_file(ram_thread_fd, &req,
				 sizeof(struct ram_thread_req *));
		if(n != sizeof(req)){
			if(n == -EAGAIN)
				break;
			printk(KERN_ERR "spurious interrupt in ram_handler, "
			       "err = %d\n", -n);
			return;
		}

		blk_end_request(req->req, 0, req->length);
		kfree(req);
	}
	reactivate_fd(ram_thread_fd, RDISK_IRQ);

	list_for_each_safe(list, next_ele, &ram_restart){
		ramdisk = container_of(list, struct ramdisk, ram_restart);
		list_del_init(&ramdisk->ram_restart);
		spin_lock_irqsave(&ramdisk->lock, flags);
		do_ram_request(ramdisk->queue);
		spin_unlock_irqrestore(&ramdisk->lock, flags);
	}
}

static irqreturn_t ram_intr(int irq, void *dev)
{
	ram_handler();
	return IRQ_HANDLED;
}


/* Only changed by ram_init, which is an initcall. */
static int ram_pid = -1;

static void kill_ram_thread(void)
{
	if(ram_pid != -1)
		os_kill_process(ram_pid, 1);
}

__uml_exitcall(kill_ram_thread);

static void ram_close_dev(struct ramdisk *ram_dev)
{
	message dev_mess, *m_ptr;
	int rcode, major, minor;

	major = RAM_MAJOR;
	minor = RAM_MINOR; // <<<<< ver de donde se puede obtener este 

	DVKDEBUG(INTERNAL, "major=%d minor=%d\n", major, minor);
	if (minor >= RAM_MAX_DEV) {
		ERROR_PRINT(EDVSOVERRUN);
		return;
	}
	
	/* Set up the message passed to task. */
	m_ptr= &dev_mess;
	m_ptr->m_type   = DEV_CLOSE;
	m_ptr->DEVICE   = minor;
	m_ptr->POSITION = 0;
	m_ptr->IO_ENDPT = rdc_ep;
//	m_ptr->IO_ENDPT = uml_ep;
	m_ptr->ADDRESS  = NULL;
	m_ptr->COUNT    = 0;
	DVKDEBUG(INTERNAL,MSG2_FORMAT, MSG2_FIELDS(m_ptr));
	rcode = dvk_sendrec_T(ram_ep, m_ptr, TIMEOUT_MOLCALL);
	if(rcode < 0) {
		ERROR_PRINT(rcode);
		return;
	}
	DVKDEBUG(INTERNAL,MSG2_FORMAT, MSG2_FIELDS(m_ptr));
	return;	
}

static int ram_open_dev(struct ramdisk *ram_dev)
{
	message dev_mess, *m_ptr;
	int rcode, major, minor;

	major = RAM_MAJOR;
	minor = RAM_MINOR; // <<<<< ver de donde se puede obtener este 

	DVKDEBUG(INTERNAL, "major=%d minor=%d\n", major, minor);
	if (minor >= RAM_MAX_DEV) ERROR_RETURN(EDVSOVERRUN);
	
	/* Set up the message passed to task. */
	m_ptr= &dev_mess;
	m_ptr->m_type   = DEV_OPEN;
	m_ptr->DEVICE   = minor;
	m_ptr->POSITION = 0;
//	m_ptr->IO_ENDPT = rdc_ep;
	m_ptr->IO_ENDPT = uml_ep;
	m_ptr->ADDRESS  = NULL;
	m_ptr->COUNT    = NULL;
	DVKDEBUG(INTERNAL,MSG2_FORMAT, MSG2_FIELDS(m_ptr));
	rcode = dvk_sendrec_T(ram_ep, m_ptr, TIMEOUT_MOLCALL);
	if(rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG(INTERNAL,MSG2_FORMAT, MSG2_FIELDS(m_ptr));
	return(OK);	
}

static void ram_device_release(struct device *dev)
{
	struct ramdisk *ram_dev = dev_get_drvdata(dev);

	DVKDEBUG(INTERNAL,"\n");

	blk_cleanup_queue(ram_dev->queue);
	*ram_dev = ((struct ramdisk) DEFAULT_RDISK);
}

static int ram_disk_register(int major, u64 size, int unit,
			     struct gendisk **disk_out)
{
	struct device *parent = NULL;
	struct gendisk *disk;

	DVKDEBUG(INTERNAL,"major=%d unit=%d\n", major, unit);
// The argument minors is the maximum number of minor numbers that this disk can have
	disk = alloc_disk(1 << RAM_SHIFT);
	if(disk == NULL)
		return -ENOMEM;

	disk->major = major;
	disk->first_minor = unit << RAM_SHIFT;
	disk->fops = &ram_blops;
	set_capacity(disk, size / RAM_SECTOR_SIZE);
	if (major == RAM_MAJOR)
		sprintf(disk->disk_name, "ramdisk%c", 'a' + unit);
	else
		sprintf(disk->disk_name, "ram_fake%d", unit);

	DVKDEBUG(INTERNAL,"disk_name=%s\n", disk->disk_name);

	/* sysfs register (not for ide fake devices) */
	if (major == RAM_MAJOR) {
		ram_devs[unit].pdev.id   = unit;
		ram_devs[unit].pdev.name = RDISK_NAME;
		ram_devs[unit].pdev.dev.release = ram_device_release;
		dev_set_drvdata(&ram_devs[unit].pdev.dev, &ram_devs[unit]);
		platform_device_register(&ram_devs[unit].pdev);
		parent = &ram_devs[unit].pdev.dev;
	}

	disk->private_data = &ram_devs[unit];
	disk->queue = ram_devs[unit].queue;
	device_add_disk(parent, disk);

	*disk_out = disk;
	return 0;
}

#define ROUND_BLOCK(n) ((n + ((1 << RAM_SECTOR_SHIFT) - 1)) & (-1 << RAM_SECTOR_SHIFT))
static int ram_add(int n, char **error_out)
{
	struct ramdisk *ram_dev = &ram_devs[n];
	int err = 0;
	
	DVKDEBUG(INTERNAL,"n=%d\n", n);

	err = ram_dev_size(ram_dev, &ram_dev->size);
	if(err < 0){
		*error_out = "Couldn't determine size of device's file";
		goto out;
	}

	ram_dev->size = ROUND_BLOCK(ram_dev->size);

	INIT_LIST_HEAD(&ram_dev->ram_restart);
	sg_init_table(ram_dev->sg, RAM_MAX_SG);

	err = -ENOMEM;
	ram_dev->queue = blk_init_queue(do_ram_request, &ram_dev->lock);
	if (ram_dev->queue == NULL) {
		*error_out = "Failed to initialize device queue";
		goto out;
	}
	ram_dev->queue->queuedata = ram_dev;
	blk_queue_write_cache(ram_dev->queue, true, false);

	blk_queue_max_segments(ram_dev->queue, RAM_MAX_SG);
	err = ram_disk_register(RAM_MAJOR, ram_dev->size, n, &ram_gendisk[n]);
	if(err){
		*error_out = "Failed to register device";
		goto out_cleanup;
	}

	if (fake_ram_major != RAM_MAJOR)
		ram_disk_register(fake_ram_major, ram_dev->size, n,
				  &fake_ram_gendisk[n]);

	/*
	 * Perhaps this should also be under the "if (fake_ram_major)" above
	 * using the fake_disk->disk_name
	 */
//	if (fake_ram_ide)
//		make_ram_ide_entries(ram_gendisk[n]->disk_name);

	err = 0;
out:
	return err;

out_cleanup:
	blk_cleanup_queue(ram_dev->queue);
	goto out;
}

static int ram_config(char *str, char **error_out)
{
	int n, ret;

	/* This string is possibly broken up and stored, so it's only
	 * freed if ram_setup_common fails, or if only general options
	 * were set.
	 */
 	DVKDEBUG(INTERNAL,"\n");

	str = kstrdup(str, GFP_KERNEL);
	if (str == NULL) {
		*error_out = "Failed to allocate memory";
		return -ENOMEM;
	}

	ret = ram_setup_common(str, &n, error_out);
	if (ret)
		goto err_free;

	if (n == -1) {
		ret = 0;
		goto err_free;
	}

	mutex_lock(&ram_lock);
	ret = ram_add(n, error_out);
	mutex_unlock(&ram_lock);

out:
	return ret;

err_free:
	kfree(str);
	goto out;
}

static int ram_parse_unit(char **ptr)
{
	char *str = *ptr, *end;
	int n = -1;

	if(isdigit(*str)) {
		n = simple_strtoul(str, &end, 0);
		if(end == str)
			return -1;
		*ptr = end;
	}
	else if (('a' <= *str) && (*str <= 'z')) {
		n = *str - 'a';
		str++;
		*ptr = str;
	}
	return n;
}

static int ram_get_config(char *name, char *str, int size, char **error_out)
{
	struct ramdisk *ram_dev;
	int n, len = 0;

	DVKDEBUG(INTERNAL,"name=%s size=%d\n", name, size);

	n = ram_parse_unit(&name);
	if((n >= RAM_MAX_DEV) || (n < 0)){
		*error_out = "ram_get_config : device number out of range";
		return -1;
	}

	ram_dev = &ram_devs[n];
	mutex_lock(&ram_lock);
	CONFIG_CHUNK(str, size, len, "", 1);

 out:
	mutex_unlock(&ram_lock);
	return len;
}

static int ram_id(char **str, int *start_out, int *end_out)
{
	int n;

	DVKDEBUG(INTERNAL,"\n");

	n = ram_parse_unit(str);
	*start_out = 0;
	*end_out = RAM_MAX_DEV - 1;
	return n;
}

static int ram_remove(int n, char **error_out)
{
	struct gendisk *disk = ram_gendisk[n];
	struct ramdisk *ram_dev;
	int err = -ENODEV;


	DVKDEBUG(INTERNAL,"n=%d\n", n);

	mutex_lock(&ram_lock);

	ram_dev = &ram_devs[n];

	/* you cannot remove a open disk */
	err = -EBUSY;
	if(ram_dev->count > 0)
		goto out;

	ram_gendisk[n] = NULL;
	if(disk != NULL){
		del_gendisk(disk);
		put_disk(disk);
	}

	if(fake_ram_gendisk[n] != NULL){
		del_gendisk(fake_ram_gendisk[n]);
		put_disk(fake_ram_gendisk[n]);
		fake_ram_gendisk[n] = NULL;
	}

	err = 0;
	platform_device_unregister(&ram_dev->pdev);
out:
	mutex_unlock(&ram_lock);
	return err;
	
}


/* All these are called by mconsole in process context and without
 * ramdisk-specific locks.  The structure itself is const except for .list.
 */
static struct mc_device ram_mc = {
	.list		= LIST_HEAD_INIT(ram_mc.list),
	.name		= "ramdisk",
	.config		= ram_config,
	.get_config	= ram_get_config,
	.id			= ram_id,
	.remove		= ram_remove,
};

static int __init ram_mc_init(void)
{
	mconsole_register_dev(&ram_mc);
	return 0;
}

__initcall(ram_mc_init);

static int __init rd0_init(void)
{
	struct ramdisk *ram_dev = &ram_devs[0];

	return 0;
}

__initcall(rd0_init);

/* Used in ram_init, which is an initcall */
static struct platform_driver ram_driver = {
	.driver = {
		.name  = RAMDISK_NAME,
	},
};




static int __init ram_driver_init(void){
	unsigned long stack;
	int err;

	/* Set by CONFIG_BLK_DEV_RAM_SYNC or ramdisk=sync.*/
	if(global_openflags.s){
		printk(KERN_INFO "ramdisk: Synchronous mode\n");
		/* Letting ramdisk=sync be like using ramdisk#s= instead of ramdisk#= is
		 * enough. So use anyway the io thread. */
	}
	stack = alloc_stack(0, 0);
	ram_pid = start_ram_thread(stack + PAGE_SIZE - sizeof(void *),
				 &ram_thread_fd);
	if(ram_pid < 0){
		printk(KERN_ERR
		       "ramdisk : Failed to start I/O thread (errno = %d) - "
		       "falling back to synchronous I/O\n", -ram_pid);
		ram_pid = -1;
		return 0;
	}
	err = um_request_irq(RDISK_IRQ, ram_thread_fd, IRQ_READ, ram_intr,
			     0, "ramdisk", ram_devs);
	if(err != 0)
		printk(KERN_ERR "um_request_irq failed - errno = %d\n", -err);
	return 0;
}

device_initcall(ram_driver_init);

static int ram_open(struct block_device *bdev, fmode_t mode)
{
	struct gendisk *disk = bdev->bd_disk;
	struct ramdisk *ram_dev = disk->private_data;
	int err = 0;

	DVKDEBUG(INTERNAL,"\n");

	mutex_lock(&ram_mutex);
	if(ram_dev->count == 0){
		err = ram_open_dev(ram_dev);
		if(err){
			printk(KERN_ERR "%s: Can't open: errno = %d\n",
			       disk->disk_name, -err);
			goto out;
		}
	}
	ram_dev->count++;
//	set_disk_ro(disk, !ram_dev->openflags.w);

	/* This should no more be needed. And it didn't work anyway to exclude
	 * read-write remounting of filesystems.*/
	/*if((mode & FMODE_WRITE) && !ram_dev->openflags.w){
	        if(--ram_dev->count == 0) ram_close_dev(ram_dev);
	        err = -EROFS;
	}*/
out:
	mutex_unlock(&ram_mutex);
	return err;
}

static void ram_release(struct gendisk *disk, fmode_t mode)
{
	struct ramdisk *ram_dev = disk->private_data;

	mutex_lock(&ram_mutex);
	if(--ram_dev->count == 0)
		ram_close_dev(ram_dev);
	mutex_unlock(&ram_mutex);
}

/* Called with dev->lock held */
static void ram_prepare_request(struct request *req, struct ram_thread_req *ram_req,
			    unsigned long long offset, int page_offset,
			    int len, struct page *page)
{
	struct gendisk *disk = req->rq_disk;
	struct ramdisk *ram_dev = disk->private_data;

	DVKDEBUG(INTERNAL,"len=%d\n", len);

	ram_req->req = req;
	ram_req->offset = offset;
	ram_req->length = len;
	ram_req->error = 0;
	ram_req->op = (rq_data_dir(req) == READ) ? RAM_READ : RAM_WRITE;
	ram_req->buffer = page_address(page) + page_offset;
	ram_req->sectorsize = 1 << RAM_SECTOR_SHIFT;

	DVKDEBUG(INTERNAL,RAM_REQ_FORMAT,RAM_REQ_FIELDS(ram_req));

}

/* Called with dev->lock held */
static void ram_prepare_flush_request(struct request *req,
				  struct ram_thread_req *ram_req)
{
	struct gendisk *disk = req->rq_disk;
	struct ramdisk *ram_dev = disk->private_data;

	DVKDEBUG(INTERNAL,"\n");
	ram_req->req = req;
	ram_req->op = RAM_FLUSH;
}

static bool ram_submit_request(struct ram_thread_req *ram_req, struct ramdisk *dev)
{
	DVKDEBUG(INTERNAL,"\n");

	int n = os_write_file(ram_thread_fd, &ram_req,
			     sizeof(ram_req));
	if (n != sizeof(ram_req)) {
		if (n != -EAGAIN)
			printk("write to io thread failed, "
			       "errno = %d\n", -n);
		else if (list_empty(&dev->ram_restart))
			list_add(&dev->ram_restart, &ram_restart);

		kfree(ram_req);
		return false;
	}
	return true;
}


/* Called with dev->lock held */
static void do_ram_request(struct request_queue *q)
{
	struct ram_thread_req *io_req;
	struct request *req;

	while(1){
		struct ramdisk *dev = q->queuedata;
		if(dev->request == NULL){
			struct request *req = blk_fetch_request(q);
			if(req == NULL)
				return;

			dev->request = req;
			dev->rq_pos = blk_rq_pos(req);
			dev->start_sg = 0;
			dev->end_sg = blk_rq_map_sg(q, req, dev->sg);
		}

		req = dev->request;

		if (req_op(req) == REQ_OP_FLUSH) {
			io_req = kmalloc(sizeof(struct ram_thread_req),
					 GFP_ATOMIC);
			if (io_req == NULL) {
				if (list_empty(&dev->ram_restart))
					list_add(&dev->ram_restart, &ram_restart);
				return;
			}
			ram_prepare_flush_request(req, io_req);
			if (ram_submit_request(io_req, dev) == false)
				return;
		}

		while(dev->start_sg < dev->end_sg){
			struct scatterlist *sg = &dev->sg[dev->start_sg];

			io_req = kmalloc(sizeof(struct ram_thread_req),
					 GFP_ATOMIC);
			if(io_req == NULL){
				if(list_empty(&dev->ram_restart))
					list_add(&dev->ram_restart, &ram_restart);
				return;
			}
			ram_prepare_request(req, io_req,
					(unsigned long long)dev->rq_pos << 9,
					sg->offset, sg->length, sg_page(sg));

			if (ram_submit_request(io_req, dev) == false)
				return;

			dev->rq_pos += sg->length >> 9;
			dev->start_sg++;
		}
		dev->end_sg = 0;
		dev->request = NULL;
	}
}

static int ram_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	struct ramdisk *ram_dev = bdev->bd_disk->private_data;
	int minor= bdev->bd_disk->first_minor;
	int rcode; 
	
	DVKDEBUG(INTERNAL,"\n");

	rcode = ram_get_geometry(ram_dev, minor, geo);
	if( rcode < 0) ERROR_RETURN(rcode);
	return(rcode);
}


int ram_get_geometry(struct ramdisk *ram_dev, int minor, struct hd_geometry *geo)
{
#ifdef RAM_MEMORY
	geo->heads 		= RAM_MEM_HEADS;
	geo->sectors 	= RAM_MEM_SECTORS;
	geo->cylinders 	= RAM_MEM_CYLINDERS;
	geo->start 		= 0;
#else // RAM_MEMORY
	message dev_mess, *m_ptr;
	int rcode, tid, my_ep;
	mnx_part_t mnx_part, *part_ptr;

	tid = os_gettid();
	my_ep = dvk_getep(tid);
	DVKDEBUG(INTERNAL,"tid=%d my_ep=%d minor=%d\n", tid, my_ep, minor);

  	part_ptr = &mnx_part;
	/* Set up the message passed to task. */
	m_ptr= &dev_mess;
	m_ptr->m_type   = DEV_IOCTL;
	m_ptr->REQUEST	= DIOCGETP;
	m_ptr->DEVICE   = minor;
	m_ptr->IO_ENDPT = my_ep;
	m_ptr->ADDRESS  = (char *) part_ptr; 
	DVKDEBUG(INTERNAL,MSG2_FORMAT, MSG2_FIELDS(m_ptr));
	rcode = dvk_sendrec_T(ram_ep, m_ptr, TIMEOUT_MOLCALL);
	if(rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG(INTERNAL,MSG2_FORMAT, MSG2_FIELDS(m_ptr));

	DVKDEBUG(INTERNAL,PART_FORMAT, PART_FIELDS(part_ptr));
	geo->heads 		= part_ptr->heads;
	geo->sectors 	= part_ptr->sectors;
	geo->cylinders 	= part_ptr->cylinders;
	geo->start 		= part_ptr->base;
#endif // RAM_MEMORY

	return 0;	
}

static int ram_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg)
{
	struct ramdisk *ram_dev = bdev->bd_disk->private_data;
	struct hd_geometry geo;
	u16 ram_id[ATA_ID_WORDS];
	int rcode, tid, my_ep;

	tid = os_gettid();
	my_ep = dvk_getep(tid);
	DVKDEBUG(INTERNAL,"cmd=%d tid=%d my_ep=%d\n",cmd, tid, my_ep);
	
	switch (cmd) {
		case HDIO_GET_IDENTITY:
#ifdef RAM_MEMORY
			ram_id[ATA_ID_HEADS]		= RAM_MEM_HEADS;
			ram_id[ATA_ID_SECTORS]	= RAM_MEM_SECTORS;
			ram_id[ATA_ID_CYLS]		= RAM_MEM_CYLINDERS;
#else // RAM_MEMORY
			rcode = ram_getgeo(bdev, &geo);
			memset(&ram_id, 0, ATA_ID_WORDS * 2);
			ram_id[ATA_ID_CYLS]		= geo.cylinders;
			ram_id[ATA_ID_HEADS]		= geo.heads;
			ram_id[ATA_ID_SECTORS]	= geo.sectors;
#endif // RAM_MEMORY
			if(copy_to_user((char __user *) arg, (char *) &ram_id,
					 sizeof(ram_id)))
				return -EFAULT;
			return 0;
		default:
			return -EINVAL;	
	}
	return -EINVAL;
}

int rdisk_rw(int oper, int minor, char *buf, unsigned long len, __u64  off)
{
	message dev_mess, *m_ptr;
	int rcode, tid, my_ep;

#ifdef RAM_MEMORY
	void *ptr; 
	DVKDEBUG(INTERNAL,"oper=%d len=%ld\n",oper, len);
	if( oper == DEV_READ)
		ptr = memcpy(buf, (ram_disk + off), len);
	else 
		ptr = memcpy( (ram_disk + off), buf, len);
	return(len);	
#else // RAM_MEMORY

	tid = os_gettid();
	my_ep = dvk_getep(tid);
	DVKDEBUG(INTERNAL,"oper=%d tid=%d my_ep=%d minor=%d\n",oper, tid, my_ep, minor);
		
	/* Set up the message passed to task. */
	m_ptr= &dev_mess;
	m_ptr->m_type   = oper;
	m_ptr->DEVICE   = minor;
	m_ptr->POSITION = (int) off;
	m_ptr->IO_ENDPT = my_ep;
//	m_ptr->IO_ENDPT = rdc_ep;
//	m_ptr->IO_ENDPT = uml_ep;
	m_ptr->ADDRESS  = buf;
	m_ptr->COUNT    = len;
	m_ptr->TTY_FLAGS = 0;
	DVKDEBUG(INTERNAL,MSG2_FORMAT, MSG2_FIELDS(m_ptr));
	rcode = dvk_sendrec_T(ram_ep, m_ptr, TIMEOUT_MOLCALL);
	if(rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG(INTERNAL,MSG2_FORMAT, MSG2_FIELDS(m_ptr));
		
	DVKDEBUG(INTERNAL,"READ/WRITE bytes=%d\n", m_ptr->REP_STATUS);
	return(m_ptr->REP_STATUS);
#endif // RAM_MEMORY

}

static void do_rdisk(struct ram_thread_req *req)
{
	char *buf;
	unsigned long len;
	int n, minor;
	__u64 off;

	if (req->op == RAM_FLUSH) {
		printk("do_rdisk - sync ignored\n");
		return;
	}
	
	DVKDEBUG(INTERNAL,RAM_REQ_FORMAT,RAM_REQ_FIELDS(req));

	minor = req->req->rq_disk->first_minor;
	len = req->length;
	do {
		off = req->offset +	(req->length - len);
		buf = &req->buffer[(req->length - len)];
		DVKDEBUG(INTERNAL,"op=%d minor=%d len=%d off=%d\n", req->op, minor, len, off);
		if(req->op == RAM_READ){
			n = rdisk_rw(DEV_READ, minor, buf, len, off);
			if (n < 0) {
				printk("do_rdisk - read failed, err = %d\n", n);
				req->error = 1;
				return;
			}
			if (n == 0 ) {
				if( len < req->length)
					memset(&buf[n], 0, req->length - len);
				len = 0;
				break;
			}
		} else if (req->op == RAM_WRITE){
			n = rdisk_rw(DEV_WRITE, minor, buf, len, off);
			if(n < 0){
				printk("do_rdisk - write failed err = %d\n", n);
				req->error = 1;
				return;
			}
		} else {
			req->error = 1;
			ERROR_PRINT(EDVSNOSYS);
			return;
		}
		len -= n;
	} while( (len > 0) );

	DVKDEBUG(INTERNAL,"op=%d minor=%d len=%d off=%d\n", req->op, minor, len, off);
	req->error = 0;
}


/* Changed in start_ram_thread, which is serialized by being called only
 * from ram_init, which is an initcall.
 */

static int init_rdisk(void)
{
	int rcode, i;
	dvs_usr_t *dvsu_ptr;
	dc_usr_t *dcu_ptr;
	proc_usr_t *proc_ptr;  
	
	DVKDEBUG(INTERNAL,"\n");	

	rcode = dvk_open();
	if( rcode < 0) ERROR_RETURN(rcode);
	
	dvsu_ptr = &dvs;

	local_nodeid = dvk_getdvsinfo(dvsu_ptr);
	if( local_nodeid < 0) ERROR_RETURN(local_nodeid);
	DVKDEBUG(INTERNAL,"local_nodeid=%d\n",local_nodeid);	
	DVKDEBUG(INTERNAL, DVS_USR_FORMAT, DVS_USR_FIELDS(dvsu_ptr));	

	dcu_ptr = &dcu;
	rcode = dvk_getdcinfo(dcid, dcu_ptr);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG(INTERNAL, DC_USR1_FORMAT, DC_USR1_FIELDS(dcu_ptr));	
	DVKDEBUG(INTERNAL, DC_USR2_FORMAT, DC_USR2_FIELDS(dcu_ptr));	
 
#define RDC_IS_FS_PROC_NR 
#ifdef RDC_IS_FS_PROC_NR 
	rdc_ep = dvk_tbind(dcid, FS_PROC_NR);
#else // RDC_IS_FS_PROC_NR 
	for( i = dcu_ptr->dc_nr_sysprocs - dcu_ptr->dc_nr_tasks;
		i < dcu_ptr->dc_nr_procs - dcu_ptr->dc_nr_tasks; i++){
		rdc_ep = dvk_tbind(dcid, i);
		if( rdc_ep == i ) break; 
	}
	if( i == dcu_ptr->dc_nr_procs) ERROR_RETURN(EDVSSLOTUSED);
#endif // RDC_IS_FS_PROC_NR

	DVKDEBUG(INTERNAL,"ramdisk bind rdc_ep=%d\n",rdc_ep);
	proc_ptr = &rdc_proc;
	rcode = dvk_getprocinfo(dcid, rdc_ep, proc_ptr);
	if(rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG(INTERNAL, PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));	

}

int rdk_fd = -1;
/* Only changed by the io thread. XXX: currently unused. */
static int ram_count = 0;

int ram_thread(void *arg)
{
	struct ram_thread_req *req;
	int n;
	int rcode;

	os_fix_helper_signals();

	rcode = init_rdisk();

	while(1){
		DVKDEBUG(INTERNAL,"\n");

		n = os_read_file(rdk_fd, &req,
				 sizeof(struct ram_thread_req *));
		if(n != sizeof(struct ram_thread_req *)){
			if(n < 0)
				printk("ram_thread - read failed, fd = %d, "
				       "err = %d\n", rdk_fd, -n);
			else {
				printk("ram_thread - short read, fd = %d, "
				       "length = %d\n", rdk_fd, n);
			}
			continue;
		}

		DVKDEBUG(INTERNAL,RAM_REQ_FORMAT,RAM_REQ_FIELDS(req));
		ram_count++;
		do_rdisk(req);
		DVKDEBUG(INTERNAL,RAM_REQ_FORMAT,RAM_REQ_FIELDS(req));
		n = os_write_file(rdk_fd, &req,
				  sizeof(struct ram_thread_req *));
		if(n != sizeof(struct ram_thread_req *))
			printk("ram_thread - write failed, fd = %d, err = %d\n",
			       rdk_fd, -n);
	}

	return 0;
}

#endif //  ANULADO 


