/*
 * UDB_KERN.C with Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

/* 2020-07-28
  RDISK_KERN.C by Pablo Pessolani, based on udb_kern.c source code
 */

// #ifdef CONFIG_UML_RDISK 
// 
#define RD_SHIFT 0

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

#include "um_dvk.h"
#include "uml_rdisk.h"
#include "glo_dvk.h"

#include "/usr/src/dvs/vos/mol/include/com.h"
#include "/usr/src/dvs/vos/mol/include/partition.h"
#include "/usr/src/dvs/vos/mol/include/ioc_disk.h"

#ifdef  CONFIG_DVKIOCTL
#pragma message ("CONFIG_DVKIOCTL=YES")
#else // CONFIG_DVKIOCTL
#pragma message ("CONFIG_DVKIOCTL=NO")
#endif // CONFIG_DVKIOCTL

#ifdef  CONFIG_UML_DVK
#pragma message ("CONFIG_UML_DVK=YES")
#else // CONFIG_UML_DVK
#pragma message ("CONFIG_UML_DVK=NO")
#endif // CONFIG_UML_DVK

#ifdef  CONFIG_UML_USER
#pragma message ("CONFIG_UML_USER=YES")
#else // CONFIG_UML_USER
#pragma message ("CONFIG_UML_USER=NO")
#endif // CONFIG_UML_USER

#ifdef  CONFIG_DVKIPC
#pragma message ("CONFIG_DVKIPC=YES")
#else // CONFIG_DVKIPC
#pragma message ("CONFIG_DVKIPC=NO")
#endif // CONFIG_DVKIPC

#ifdef  CONFIG_UML_RDISK
#pragma message ("CONFIG_UML_RDISK=YES")
#else // CONFIG_UML_RDISK
#pragma message ("CONFIG_UML_RDISK=NO")
#endif // CONFIG_UML_RDISK

//////////////////////////////////////////////////////////////////////////////////
// MINIX (RDISK) REQUEST TYPES 
//#define DEV_RQ_BASE   0x400	/* base for device request types */
//#define DEV_RS_BASE   0x500	/* base for device response types */
//#define CANCEL       	(DEV_RQ_BASE +  0) /* force a task to cancel */
//#define DEV_READ		(DEV_RQ_BASE +  3) /* read from minor device */
//#define DEV_WRITE   	(DEV_RQ_BASE +  4) /* write to minor device */
//#define DEV_IOCTL    	(DEV_RQ_BASE +  5) /* I/O control code */
//#define DEV_OPEN     	(DEV_RQ_BASE +  6) /* open a minor device */
//#define DEV_CLOSE    	(DEV_RQ_BASE +  7) /* close a minor device */
//
// LINUX BLOCK DEVICE REQUEST TYPES 
// enum req_op {
//	REQ_OP_READ,
//	REQ_OP_WRITE,
//	REQ_OP_DISCARD,		/* request to discard sectors */
//	REQ_OP_SECURE_ERASE,	/* request to securely erase sectors */
//	REQ_OP_WRITE_SAME,	/* write same block many times */
//	REQ_OP_FLUSH,		/* request for cache flush */
//};
// AND TWO ADDED 
// #define REQ_OP_OPEN 	(REQ_OP_FLUSH+1)
// #define REQ_OP_CLOSE 	(REQ_OP_OPEN+1)
//////////////////////////////////////////////////////////////////////////////////

#define REQ_OP_OPEN 	(REQ_OP_FLUSH+1)
#define REQ_OP_CLOSE 	(REQ_OP_OPEN+1)

struct rd_thread_req {
	struct request *req;
	enum req_op op;
	unsigned long long offset;
	unsigned long length;
	char *buffer;
	int sectorsize;
	int error;
};

#define RD_REQ_FORMAT 		"op=%d offset=%lld length=%ld sectorsize=%d error=%d\n"
#define RD_REQ_FIELDS(p) 	p->op, p->offset, p->length, p->sectorsize, p->error

#define RDISK_NAME "rdisk"

static DEFINE_MUTEX(rd_lock);
static DEFINE_MUTEX(rd_mutex); /* replaces BKL, might not be needed */
//static DEFINE_MUTEX(rd_ksync);


#define MAX_SG 			1

struct rdisk {
	struct list_head restart;
	int count;
	__u64 size;
	struct openflags boot_openflags;
	struct openflags openflags;
	struct platform_device pdev;
	struct request_queue *queue;
	spinlock_t lock;
	struct scatterlist sg[MAX_SG];
	struct request *request;
	int start_sg, end_sg;
	sector_t rq_pos;
};

#define RD_OPEN_FLAGS ((struct openflags) { .r = 1, .w = 1, .s = 1, .c = 0, .cl = 1 })
static struct openflags global_openflags = RD_OPEN_FLAGS;

#define DEFAULT_RDISK { \
	.count =		0, \
	.size =			-1, \
	.boot_openflags =	RD_OPEN_FLAGS, \
	.openflags =		RD_OPEN_FLAGS, \
	.lock =			__SPIN_LOCK_UNLOCKED(rd_devs.lock), \
	.request =		NULL, \
	.start_sg =		0, \
	.end_sg =		0, \
	.rq_pos =		0, \
}

#define RD_MAX_DEV (1)
static struct rdisk rd_devs[RD_MAX_DEV] = { [0 ... RD_MAX_DEV - 1] = DEFAULT_RDISK };

static int rd_open(struct block_device *bdev, fmode_t mode);
static void rd_release(struct gendisk *disk, fmode_t mode);
static int rd_ioctl(struct block_device *bdev, fmode_t mode,
		     unsigned int cmd, unsigned long arg);
static int rd_getgeo(struct block_device *bdev, struct hd_geometry *geo);
int rd_get_geometry(struct rdisk *rd_dev, int minor, struct hd_geometry *geo);
static bool rd_submit_request(struct rd_thread_req *rd_req, struct rdisk *dev);

static const struct block_device_operations rd_blops = {
        .owner		= THIS_MODULE,
        .open		= rd_open,
        .release	= rd_release,
        .ioctl		= rd_ioctl,
		.getgeo		= rd_getgeo,
};


/* Protected by rd_lock */
static int 	  fake_rd_major = RD_MAJOR;
static struct gendisk *rd_gendisk[RD_MAX_DEV];
static struct gendisk *fake_rd_gendisk[RD_MAX_DEV];


/* Only changed by fake_rd_ide_setup which is a setup */
static int 		fake_rd_ide = 0;
static struct proc_dir_entry *proc_rd_ide_root = NULL;
static struct proc_dir_entry *proc_rd_ide = NULL;

#define GEO_FORMAT  	"cyl=%d heads=%d sec=%d\n"
#define GEO_FIELDS(p)    p->cylinders, p->heads, p->sectors

int rd_dev_size(struct rdisk *rd_dev, __u64 *size)
{
	struct hd_geometry geo,  *g_ptr;
	int rcode, major, minor;

	DVKDEBUG(INTERNAL,"\n");

	major = RD_MAJOR;
	minor = RD_MINOR; // <<<<< ver de donde se puede obtener este 

	rcode = rd_get_geometry(rd_dev, minor, &geo);
	if( rcode < 0) ERROR_RETURN(rcode);
	
	g_ptr = &geo;
	DVKDEBUG(INTERNAL, GEO_FORMAT, GEO_FIELDS(g_ptr));
	*size = g_ptr->heads * g_ptr->sectors * g_ptr->cylinders * RD_SECTOR_SIZE;
	DVKDEBUG(INTERNAL, "RDISK size=%lld \n",*size);

	return(OK);	
}

static void make_proc_ide(void)
{
	DVKDEBUG(INTERNAL,"\n");

	proc_rd_ide_root = proc_mkdir("rd_ide", NULL);
	proc_rd_ide = proc_mkdir("rd_ide0", proc_rd_ide_root);
}

static int fake_rd_ide_media_show(struct seq_file *m, void *v)
{
	DVKDEBUG(INTERNAL,"\n");
	seq_puts(m, "disk\n");
	return 0;
}

static int fake_rd_ide_media_open(struct inode *inode, struct file *file)
{
	DVKDEBUG(INTERNAL,"\n");
	return 0;
}

static const struct file_operations fake_rd_ide_media_fops = {
	.owner		= THIS_MODULE,
	.open		= fake_rd_ide_media_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void make_rd_ide_entries(const char *dev_name)
{
	struct proc_dir_entry *dir, *ent;
	char name[64];

	DVKDEBUG(INTERNAL,"dev_name=%s\n", dev_name);

	if(proc_rd_ide_root == NULL) make_proc_ide();

	dir = proc_mkdir(dev_name, proc_rd_ide);
	if(!dir) return;

	ent = proc_create("media", S_IRUGO, dir, &fake_rd_ide_media_fops);
	if(!ent) return;
	snprintf(name, sizeof(name), "ide0/%s", dev_name);
	proc_symlink(dev_name, proc_rd_ide_root, name);
}

static int fake_rd_ide_setup(char *str)
{
	DVKDEBUG(INTERNAL,"\n");

	fake_rd_ide = 1;
	return 1;
}

__setup("fake_rd_ide", fake_rd_ide_setup);

__uml_help(fake_rd_ide_setup,
"fake_rd_ide\n"
"    Create ide0 entries that map onto rdisk devices.\n\n"
);

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
 * otherwise, the str pointer is used (and owned) inside rd_devs array, so it
 * should not be freed on exit.
 */
static int rd_setup_common(char *str, int *index_out, char **error_out)
{
	DVKDEBUG(INTERNAL,"\n");

     if( index_out != NULL)
		*index_out = 0; 
	return 0;
}

static void rd_build_request(struct request_queue * q);
static LIST_HEAD(restart);
static int rd_thread_fd = -1;


/* XXX - move this inside rd_intr. */
/* Called without dev->lock held, and only in interrupt context. */
static void rd_handler(void)
{
	struct rd_thread_req *req;
	struct rdisk *rd_dev;
	struct list_head *list, *next_ele;
	unsigned long flags;
	int n;
static int h;

	DVKDEBUG(INTERNAL,"rd_handler=%d\n", h++);

	// read REPLY messages from rd_thread reply pipe until it will be empty
	while(1){
			
		n = os_read_file(rd_thread_fd, &req,
				 sizeof(struct rd_thread_req *));
		if(n != sizeof(req)){
			DVKDEBUG(INTERNAL,"n=%d sizeof(req)=%d\n", n, sizeof(req));
			if(n == -EAGAIN)
				break;
			printk(KERN_ERR "spurious interrupt in rd_handler, err = %d\n", -n);
			return;
		}
		DVKDEBUG(INTERNAL,"req->op=%d\n", req->op);
		if( req->op != REQ_OP_OPEN && req->op != REQ_OP_CLOSE ){
			blk_end_request(req->req, 0, req->length);
		}
		// free Memory 
		kfree(req);
	}
	// Reanable IRQ 
	reactivate_fd(rd_thread_fd, RDISK_IRQ);

	// for each request in queue, insert into the rd_thread() input (request) pipe  
	list_for_each_safe(list, next_ele, &restart){
		rd_dev = container_of(list, struct rdisk, restart);
		list_del_init(&rd_dev->restart);
		spin_lock_irqsave(&rd_dev->lock, flags);
		rd_build_request(rd_dev->queue);
		spin_unlock_irqrestore(&rd_dev->lock, flags);
	}
}

static irqreturn_t rd_intr(int irq, void *dev)
{
	DVKDEBUG(INTERNAL,"\n");

	rd_handler();
	return IRQ_HANDLED;
}

/* Only changed by rd_init, which is an initcall. */
static int rd_pid = -1;

static void kill_rd_thread(void)
{
	if(rd_pid != -1)
		os_kill_process(rd_pid, 1);
}

__uml_exitcall(kill_rd_thread);

static int rd_close_dev( int minor)
{
	message dev_mess, *m_ptr;
	int rcode, major;

	major = RD_MAJOR;

	DVKDEBUG(INTERNAL, "major=%d minor=%d\n", major, minor);
	if (minor >= RD_MAX_DEV) {
		ERROR_RETURN(EDVSOVERRUN);
	}
	/* Set up the message passed to task. */
	m_ptr= &dev_mess;
	m_ptr->m_type   = DEV_CLOSE;
	m_ptr->DEVICE   = minor;
	m_ptr->POSITION = 0;
	m_ptr->IO_ENDPT = rdc_ep;
	m_ptr->ADDRESS  = NULL;
	m_ptr->COUNT    = 0;
	DVKDEBUG(INTERNAL,MSG2_FORMAT, MSG2_FIELDS(m_ptr));
	rcode = dvk_sendrec_T(rd_ep, m_ptr, TIMEOUT_MOLCALL);
	if(rcode < 0) {
		ERROR_RETURN(rcode);
	}
	DVKDEBUG(INTERNAL,MSG2_FORMAT, MSG2_FIELDS(m_ptr));
	return(0);	
}

static int rd_open_dev(int minor)
{
	message dev_mess, *m_ptr;
	int rcode, major;

	major = RD_MAJOR;

	DVKDEBUG(INTERNAL, "major=%d minor=%d\n", major, minor);
	if (minor >= RD_MAX_DEV) ERROR_RETURN(EDVSOVERRUN);
	
	/* Set up the message passed to task. */
	m_ptr= &dev_mess;
	m_ptr->m_type   = DEV_OPEN;
	m_ptr->DEVICE   = minor;
	m_ptr->POSITION = 0;
	m_ptr->IO_ENDPT = rdc_ep;
	m_ptr->ADDRESS  = NULL;
	m_ptr->COUNT    = NULL;
	DVKDEBUG(INTERNAL,MSG2_FORMAT, MSG2_FIELDS(m_ptr));
	rcode = dvk_sendrec_T(rd_ep, m_ptr, TIMEOUT_MOLCALL);
	if(rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG(INTERNAL,MSG2_FORMAT, MSG2_FIELDS(m_ptr));

	return(OK);	
}

static void rd_device_release(struct device *dev)
{
	struct rdisk *rd_dev = dev_get_drvdata(dev);

	DVKDEBUG(INTERNAL,"\n");

	blk_cleanup_queue(rd_dev->queue);
	*rd_dev = ((struct rdisk) DEFAULT_RDISK);
}

static int rd_disk_register(int major, u64 size, int unit,
			     struct gendisk **disk_out)
{
	struct device *parent = NULL;
	struct gendisk *disk;

	DVKDEBUG(INTERNAL,"major=%d unit=%d size=%lld\n", major, unit, size);
// The argument minors is the maximum number of minor numbers that this disk can have
	disk = alloc_disk(1 << RD_SHIFT);
	if(disk == NULL)
		return -ENOMEM;

	disk->major = major;
	disk->first_minor = unit << RD_SHIFT;
	disk->fops = &rd_blops;
	set_capacity(disk, size / RD_SECTOR_SIZE);
	if (major == RD_MAJOR)
		sprintf(disk->disk_name, "rdisk%c", 'a' + unit);
	else
		sprintf(disk->disk_name, "rd_fake%d", unit);

	DVKDEBUG(INTERNAL,"disk_name=%s\n", disk->disk_name);

	/* sysfs register (not for ide fake devices) */
	if (major == RD_MAJOR) {
		rd_devs[unit].pdev.id   = unit;
		rd_devs[unit].pdev.name = RDISK_NAME;
		rd_devs[unit].pdev.dev.release = rd_device_release;
		dev_set_drvdata(&rd_devs[unit].pdev.dev, &rd_devs[unit]);
		platform_device_register(&rd_devs[unit].pdev);
		parent = &rd_devs[unit].pdev.dev;
	}

	disk->private_data = &rd_devs[unit];
	disk->queue = rd_devs[unit].queue;
	device_add_disk(parent, disk);

	*disk_out = disk;
	return 0;
}

#define ROUND_BLOCK(n) ((n + ((1 << RD_SECTOR_SHIFT) - 1)) & (-1 << RD_SECTOR_SHIFT))

static int rd_add(int n, char **error_out)
{
	struct rdisk *rd_dev = &rd_devs[n];
	int err = 0;
	
	DVKDEBUG(INTERNAL,"n=%d\n", n);

	err = rd_dev_size(rd_dev, &rd_dev->size);
	if(err < 0){
		*error_out = "Couldn't determine size of device's file";
		goto out;
	}

	rd_dev->size = ROUND_BLOCK(rd_dev->size);

	INIT_LIST_HEAD(&rd_dev->restart);
	sg_init_table(rd_dev->sg, MAX_SG);

	err = -ENOMEM;
	rd_dev->queue = blk_init_queue(rd_build_request, &rd_dev->lock);
	if (rd_dev->queue == NULL) {
		*error_out = "Failed to initialize device queue";
		goto out;
	}
	rd_dev->queue->queuedata = rd_dev;
	blk_queue_write_cache(rd_dev->queue, true, false);

	blk_queue_max_segments(rd_dev->queue, MAX_SG);
	err = rd_disk_register(RD_MAJOR, rd_dev->size, n, &rd_gendisk[n]);
	if(err){
		*error_out = "Failed to register device";
		goto out_cleanup;
	}

	if (fake_rd_major != RD_MAJOR)
		rd_disk_register(fake_rd_major, rd_dev->size, n,
				  &fake_rd_gendisk[n]);

	/*
	 * Perhaps this should also be under the "if (fake_rd_major)" above
	 * using the fake_disk->disk_name
	 */
	if (fake_rd_ide)
		make_rd_ide_entries(rd_gendisk[n]->disk_name);

	err = 0;
out:
	return err;

out_cleanup:
	blk_cleanup_queue(rd_dev->queue);
	goto out;
}

static int rd_config(char *str, char **error_out)
{
	int n, ret;

	/* This string is possibly broken up and stored, so it's only
	 * freed if rd_setup_common fails, or if only general options
	 * were set.
	 */
 	DVKDEBUG(INTERNAL,"\n");

	str = kstrdup(str, GFP_KERNEL);
	if (str == NULL) {
		*error_out = "Failed to allocate memory";
		return -ENOMEM;
	}

	ret = rd_setup_common(str, &n, error_out);
	if (ret)
		goto err_free;

	if (n == -1) {
		ret = 0;
		goto err_free;
	}

	mutex_lock(&rd_lock);
	ret = rd_add(n, error_out);
	mutex_unlock(&rd_lock);

out:
	return ret;

err_free:
	kfree(str);
	goto out;
}

static int rd_parse_unit(char **ptr)
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

static int rd_get_config(char *name, char *str, int size, char **error_out)
{
	struct rdisk *rd_dev;
	int n, len = 0;

	DVKDEBUG(INTERNAL,"name=%s size=%d\n", name, size);

	n = rd_parse_unit(&name);
	if((n >= RD_MAX_DEV) || (n < 0)){
		*error_out = "rd_get_config : device number out of range";
		return -1;
	}

	rd_dev = &rd_devs[n];
	mutex_lock(&rd_lock);
	CONFIG_CHUNK(str, size, len, "", 1);

 out:
	mutex_unlock(&rd_lock);
	return len;
}

static int rd_id(char **str, int *start_out, int *end_out)
{
	int n;

	DVKDEBUG(INTERNAL,"\n");

	n = rd_parse_unit(str);
	*start_out = 0;
	*end_out = RD_MAX_DEV - 1;
	return n;
}

static int rd_remove(int n, char **error_out)
{
	struct gendisk *disk = rd_gendisk[n];
	struct rdisk *rd_dev;
	int err = -ENODEV;

	DVKDEBUG(INTERNAL,"n=%d\n", n);

	mutex_lock(&rd_lock);

	rd_dev = &rd_devs[n];

	/* you cannot remove a open disk */
	err = -EBUSY;
	if(rd_dev->count > 0)
		goto out;

	rd_gendisk[n] = NULL;
	if(disk != NULL){
		del_gendisk(disk);
		put_disk(disk);
	}

	if(fake_rd_gendisk[n] != NULL){
		del_gendisk(fake_rd_gendisk[n]);
		put_disk(fake_rd_gendisk[n]);
		fake_rd_gendisk[n] = NULL;
	}

	err = 0;
	platform_device_unregister(&rd_dev->pdev);
out:
	mutex_unlock(&rd_lock);
	return err;
	
}


/* All these are called by mconsole in process context and without
 * rdisk-specific locks.  The structure itself is const except for .list.
 */
static struct mc_device rd_mc = {
	.list		= LIST_HEAD_INIT(rd_mc.list),
	.name		= "rdisk",
	.config		= rd_config,
	.get_config	= rd_get_config,
	.id			= rd_id,
	.remove		= rd_remove,
};

static int __init rd_mc_init(void)
{
	mconsole_register_dev(&rd_mc);
	return 0;
}

__initcall(rd_mc_init);

static int __init rd0_init(void)
{
	struct rdisk *rd_dev = &rd_devs[0];

	return 0;
}

__initcall(rd0_init);


/* Used in rd_init, which is an initcall */
static struct platform_driver rd_driver = {
	.driver = {
		.name  = RDISK_NAME,
	},
};


static int __init rd_init(void)
{
	char *error;
	int i, err;
	pid_t my_tid;

	my_tid = os_gettid();
	DVKDEBUG(INTERNAL,"dcid=%d my_tid=%d uml_ep=%d\n",dcid, my_tid, uml_ep);
	
	err = dvk_umbind(dcid, my_tid, uml_ep);
	if( err != uml_ep)
		ERROR_PRINT(err);
	
	if (register_blkdev(RD_MAJOR, "rdisk"))
		return -1;

	if (fake_rd_major != RD_MAJOR) {
		char name[sizeof("rd_nnn\0")];

		snprintf(name, sizeof(name), "rd_%d", fake_rd_major);
		if (register_blkdev(fake_rd_major, "rdisk"))
			return -1;
	}
	platform_driver_register(&rd_driver);
	mutex_lock(&rd_lock);
	for (i = 0; i < RD_MAX_DEV; i++){
		err = rd_add(i, &error);
		if(err)
			printk(KERN_ERR "Failed to initialize rdisk device %d :"
			       "%s\n", i, error);
	}
	mutex_unlock(&rd_lock);
	return 0;
}


#ifdef CONFIG_UML_RDISK
late_initcall(rd_init);
#endif // CONFIG_UML_RDISK


static int __init rd_driver_init(void)
{
	unsigned long stack;
	int err;

	DVKDEBUG(INTERNAL,"RDISK_IRQ=%d\n",RDISK_IRQ);

	/* Set by CONFIG_BLK_DEV_RD_SYNC or rdisk=sync.*/
	if(global_openflags.s){
		printk(KERN_INFO "rdisk: Synchronous mode\n");
		/* Letting rdisk=sync be like using rdisk#s= instead of rdisk#= is
		 * enough. So use anyway the io thread. */
	}
	stack = alloc_stack(0, 0);
	// Start the rd_thread (in rdisk_user.c) with the rd_thread_fd as argument.
	rd_pid = start_rd_thread(stack + PAGE_SIZE - sizeof(void *), &rd_thread_fd);
	printk("start_rd_thread rd_pid=%d\n", rd_pid);
	if(rd_pid < 0){
		printk(KERN_ERR
		       "rdisk : Failed to start I/O thread (errno = %d) - "
		       "falling back to synchronous I/O\n", -rd_pid);
		rd_pid = -1;
		return 0;
	}
	// when rd_thread_fd (pipe) receives messages (write), it trigger RDISK_IRQ
	err = um_request_irq(RDISK_IRQ, rd_thread_fd, IRQ_READ, rd_intr, 0, "rdisk", rd_devs);
	if(err != 0)
		printk(KERN_ERR "um_request_irq failed - errno = %d\n", -err);
	return 0;
}

#ifdef CONFIG_UML_RDISK 
// HERE START ALL  equivalent to main() of RDISK 
device_initcall(rd_driver_init);
#endif // CONFIG_UML_RDISK

/* Called with dev->lock held */
static void rd_prepare_open_request(struct request *req,
				  struct rd_thread_req *rd_req)
{
	DVKDEBUG(INTERNAL,"\n");
	rd_req->req = req;
	rd_req->op = REQ_OP_OPEN;
	rd_req->length = 0;
}

static void rd_prepare_close_request(struct request *req,
				  struct rd_thread_req *rd_req)
{
	DVKDEBUG(INTERNAL,"\n");
	rd_req->req = req;
	rd_req->op = REQ_OP_CLOSE;
	rd_req->length = 0;
}

static int rd_open(struct block_device *bdev, fmode_t mode)
{
	int my_tid, my_ep;
	struct gendisk *disk = bdev->bd_disk;
	struct rdisk *rd_dev = disk->private_data;
	struct rd_thread_req *rd_req;
	int err = 0;

	my_tid = os_gettid();
	my_ep = dvk_getep(my_tid);
	DVKDEBUG(INTERNAL,"mode=%d my_tid=%d my_ep=%d\n", mode, my_tid, my_ep);
	
	mutex_lock(&rd_mutex);
	if(rd_dev->count == 0){
		DVKDEBUG(INTERNAL,"REQ_OP_OPEN\n");
		rd_req = kmalloc(sizeof(struct rd_thread_req),
				 GFP_ATOMIC);
		if (rd_req == NULL) {
			if (list_empty(&rd_dev->restart))
				list_add(&rd_dev->restart, &restart);
			goto out;
		} 
		rd_prepare_open_request(REQ_OP_OPEN, rd_req);
		if (rd_submit_request(rd_req, rd_dev) != false) {
			rd_dev->count++;
//			mutex_lock(&rd_ksync);
			mutex_unlock(&rd_mutex);
			return(OK);
		}
		goto out;
	}
	rd_dev->count++;
//	set_disk_ro(disk, !rd_dev->openflags.w);

	/* This should no more be needed. And it didn't work anyway to exclude
	 * read-write remounting of filesystems.*/
	/*if((mode & FMODE_WRITE) && !rd_dev->openflags.w){
	        if(--rd_dev->count == 0) rd_close_dev(rd_dev);
	        err = -EROFS;
	}*/
out:
	mutex_unlock(&rd_mutex);
	return err;
}

static void rd_release(struct gendisk *disk, fmode_t mode)
{
	int my_tid, my_ep;
	struct rdisk *rd_dev = disk->private_data;
	struct rd_thread_req *rd_req;

	my_tid = os_gettid();
	my_ep = dvk_getep(my_tid);
	DVKDEBUG(INTERNAL,"mode=%d my_tid=%d my_ep=%d\n", mode, my_tid, my_ep);

	mutex_lock(&rd_mutex);
	if(--rd_dev->count == 0){
		DVKDEBUG(INTERNAL,"REQ_OP_CLOSE\n");
		rd_req = kmalloc(sizeof(struct rd_thread_req),
				 GFP_ATOMIC);
		if (rd_req == NULL) {
			if (list_empty(&rd_dev->restart))
				list_add(&rd_dev->restart, &restart);
			goto out;
		}
		rd_prepare_close_request(REQ_OP_CLOSE, rd_req);
		if (rd_submit_request(rd_req, rd_dev) != false){
//			mutex_lock(&rd_ksync);
			mutex_unlock(&rd_mutex);
			return(OK);			
		}
		goto out;
	}
out:
	mutex_unlock(&rd_mutex);
}

/* Called with dev->lock held */
static void rd_prepare_rw_rqst(struct request *req, struct rd_thread_req *rd_req,
			    unsigned long long offset, int page_offset,
			    int len, struct page *page)
{
	struct gendisk *disk = req->rq_disk;
	struct rdisk *rd_dev = disk->private_data;

	rd_req->req = req;
	rd_req->offset = offset;
	rd_req->length = len;
	rd_req->error = 0;
	rd_req->op = (rq_data_dir(req) == READ) ? REQ_OP_READ : REQ_OP_WRITE;
	rd_req->buffer = page_address(page) + page_offset;
	rd_req->sectorsize = 1 << RD_SECTOR_SHIFT;

	DVKDEBUG(INTERNAL,RD_REQ_FORMAT,RD_REQ_FIELDS(rd_req));

}


/* Called with dev->lock held */
static void rd_build_flush_request(struct request *req,
				  struct rd_thread_req *rd_req)
{
	DVKDEBUG(INTERNAL,"\n");
	rd_req->req = req;
	rd_req->op = REQ_OP_FLUSH;
}

static bool rd_submit_request(struct rd_thread_req *rd_req, struct rdisk *dev)
{
	DVKDEBUG(INTERNAL,"\n");

	int n = os_write_file(rd_thread_fd, &rd_req,
			     sizeof(rd_req));
	if (n != sizeof(rd_req)) {
		if (n != -EAGAIN)
			printk("write to io thread failed, "
			       "errno = %d\n", -n);
		else if (list_empty(&dev->restart))
			list_add(&dev->restart, &restart);

		kfree(rd_req);
		return false;
	}
	return true;
}

/* Called with dev->lock held */
static void rd_build_request(struct request_queue *q)
{
	struct rd_thread_req *rd_req;
	struct request *req;
static int rq_nr;

	DVKDEBUG(INTERNAL,"rq_nr=%d\n", rq_nr++);

	while(1){
		struct rdisk *dev = q->queuedata;
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
			DVKDEBUG(INTERNAL,"REQ_OP_FLUSH\n");
			rd_req = kmalloc(sizeof(struct rd_thread_req),
					 GFP_ATOMIC);
			if (rd_req == NULL) {
				if (list_empty(&dev->restart))
					list_add(&dev->restart, &restart);
				return;
			}
			rd_build_flush_request(req, rd_req);
			if (rd_submit_request(rd_req, dev) == false)
				return;
		}
		DVKDEBUG(INTERNAL,"req_op(req)=%d\n", req_op(req));
//	REQ_OP_READ,
//	REQ_OP_WRITE,
//	REQ_OP_DISCARD,		/* request to discard sectors */
//	REQ_OP_SECURE_ERASE,	/* request to securely erase sectors */
//	REQ_OP_WRITE_SAME,	/* write same block many times */
//	REQ_OP_FLUSH,		/* request for cache flush */
	
		// from the original READ/WRITE request from KERNEL 
		//  It creates one or more device requests from a scatterliist 
		//  until it satisfies the KERNEL request 
		while(dev->start_sg < dev->end_sg){
			struct scatterlist *sg = &dev->sg[dev->start_sg];

			rd_req = kmalloc(sizeof(struct rd_thread_req),
					 GFP_ATOMIC);
			if(rd_req == NULL){
				if(list_empty(&dev->restart))
					list_add(&dev->restart, &restart);
				return;
			}
			rd_prepare_rw_rqst(req, rd_req,
					(unsigned long long)dev->rq_pos << RD_SECTOR_SHIFT,
					sg->offset, sg->length, sg_page(sg));

			if (rd_submit_request(rd_req, dev) == false)
				return;

			dev->rq_pos += sg->length >> RD_SECTOR_SHIFT;
			dev->start_sg++;
		}
		dev->end_sg = 0;
		
		dev->request = NULL;
	}
}

static int rd_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	int my_tid, my_ep;
	struct rdisk *rd_dev = bdev->bd_disk->private_data;
	int minor= bdev->bd_disk->first_minor;
	int rcode; 
	
	my_tid = os_gettid();
	my_ep = dvk_getep(my_tid);
	DVKDEBUG(INTERNAL,"my_tid=%d my_ep=%d\n", my_tid, my_ep);	

	rcode = rd_get_geometry(rd_dev, minor, geo);
	if( rcode < 0) ERROR_RETURN(rcode);
	return(rcode);
}

int rd_get_geometry(struct rdisk *rd_dev, int minor, struct hd_geometry *geo)
{
	message dev_mess, *m_ptr;
	int rcode;
	mnx_part_t mnx_part, *part_ptr;
	int my_tid, my_ep;
	
	my_tid = os_gettid();
	my_ep = dvk_getep(my_tid);
	DVKDEBUG(INTERNAL,"minor=%d my_tid=%d my_ep=%d\n", minor, my_tid, my_ep);
	
  	part_ptr = &mnx_part;
	/* Set up the message passed to task. */
	m_ptr= &dev_mess;
	m_ptr->m_type   = DEV_IOCTL;
	m_ptr->REQUEST	= DIOCGETP;
	m_ptr->DEVICE   = minor;
	m_ptr->IO_ENDPT = uml_ep;
	m_ptr->ADDRESS  = (char *) part_ptr; 
	DVKDEBUG(INTERNAL,MSG2_FORMAT, MSG2_FIELDS(m_ptr));
	rcode = dvk_sendrec_T(rd_ep, m_ptr, TIMEOUT_MOLCALL);
	if(rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG(INTERNAL,MSG2_FORMAT, MSG2_FIELDS(m_ptr));

	DVKDEBUG(INTERNAL,PART_FORMAT, PART_FIELDS(part_ptr));
	geo->heads 		= part_ptr->heads;
	geo->sectors 	= part_ptr->sectors;
	geo->cylinders 	= part_ptr->cylinders;
	geo->start 		= part_ptr->base;
	return 0;	
}

static int rd_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg)
{
	struct rdisk *rd_dev = bdev->bd_disk->private_data;
	struct hd_geometry geo;
	u16 rd_id[ATA_ID_WORDS];
	int rcode;
	int my_tid, my_ep;
	
	my_tid = os_gettid();
	my_ep = dvk_getep(my_tid);
	DVKDEBUG(INTERNAL,"mode=%d cmd=%d my_tid=%d my_ep=%d\n", mode, cmd,  my_tid, my_ep);
	
	switch (cmd) {
		case HDIO_GET_IDENTITY:
			rcode = rd_getgeo(bdev, &geo);
			memset(&rd_id, 0, ATA_ID_WORDS * 2);
			rd_id[ATA_ID_CYLS]		= geo.cylinders;
			rd_id[ATA_ID_HEADS]		= geo.heads;
			rd_id[ATA_ID_SECTORS]	= geo.sectors;
			if(copy_to_user((char __user *) arg, (char *) &rd_id,
					 sizeof(rd_id)))
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
	int rcode, my_tid, my_ep;
		
	my_tid = os_gettid();
	my_ep = dvk_getep(my_tid);
	DVKDEBUG(INTERNAL,"oper=%d minor=%d len=%ld rd_tid=%d my_ep=%d rdc_ep=%d\n",
		oper, minor, len, my_tid, my_ep, rdc_ep);
	/* Set up the message passed to task. */
	m_ptr= &dev_mess;
	if( oper == REQ_OP_READ)
		m_ptr->m_type   = DEV_READ;
	else
		m_ptr->m_type   = DEV_WRITE;
	m_ptr->DEVICE   = minor;
	m_ptr->POSITION = (int) off;
	m_ptr->IO_ENDPT = rdc_ep;
	m_ptr->ADDRESS  = buf;
	m_ptr->COUNT    = len;
	m_ptr->TTY_FLAGS = 0;
	DVKDEBUG(INTERNAL,MSG2_FORMAT, MSG2_FIELDS(m_ptr));
	rcode = dvk_sendrec_T(rd_ep, m_ptr, TIMEOUT_MOLCALL);
	if(rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG(INTERNAL,MSG2_FORMAT, MSG2_FIELDS(m_ptr));
	DVKDEBUG(INTERNAL,"READ/WRITE bytes=%d\n", m_ptr->REP_STATUS);
	return(m_ptr->REP_STATUS);
}

static void do_rdisk_rqst(struct rd_thread_req *req)
{
	char *buf;
	unsigned long len;
	int n, minor, err;
	__u64 off;

	if (req->op == REQ_OP_FLUSH) {
		printk("do_rdisk_rqst - sync ignored\n");
		return;
	}

//	DVKDEBUG(INTERNAL,RD_REQ_FORMAT,RD_REQ_FIELDS(req));
	minor = req->req->rq_disk->first_minor;
	len = req->length;

	if (req->op == REQ_OP_OPEN) {
		err = rd_open_dev(minor);
		if( err != 0){
			ERROR_PRINT(err);
		}
		req->error = err;
//		mutex_unlock(&rd_ksync);
		return;
	}

	if (req->op == REQ_OP_CLOSE) {
		err = rd_close_dev(minor);
		if( err != 0){
			ERROR_PRINT(err);
		}
		req->error = err;
//		mutex_unlock(&rd_ksync);
//		mutex_lock(&rd_tsync);
		return;
	}
		
	do {
		off = req->offset +	(req->length - len);
		buf = &req->buffer[(req->length - len)];
		DVKDEBUG(INTERNAL,"op=%d minor=%d len=%d off=%d\n", req->op, minor, len, off);
		if(req->op == REQ_OP_READ){
			n = rdisk_rw(REQ_OP_READ, minor, buf, len, off);
			if (n < 0) {
				printk("do_rdisk_rqst - read failed, err = %d\n", n);
				req->error = 1;
				return;
			}
			if (n == 0 ) {
				if( len < req->length)
					memset(&buf[n], 0, req->length - len);
				len = 0;
				break;
			}
		} else if (req->op == REQ_OP_WRITE){
			n = rdisk_rw(REQ_OP_WRITE, minor, buf, len, off);
			if(n < 0){
				printk("do_rdisk_rqst - write failed err = %d\n", n);
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


/* Changed in start_rd_thread, which is serialized by being called only
 * from rd_init, which is an initcall.
 */

static int init_rdisk(void)
{
	int rcode, i;
	char *error;
	dvs_usr_t *dvsu_ptr;
	dc_usr_t *dcu_ptr;
	proc_usr_t *proc_ptr;  
	int my_tid, my_ep;
	
	my_tid = os_gettid();
	my_ep = dvk_getep(my_tid);
	DVKDEBUG(INTERNAL,"my_tid=%d my_ep=%d\n", my_tid, my_ep);

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
//	rdc_ep = dvk_lclbind(dcid, my_tid, FS_PROC_NR);
	rdc_ep = dvk_umbind(dcid, my_tid, FS_PROC_NR);
#else // RDC_IS_FS_PROC_NR -  FREE USER ENDPOINT SEARCH
	for( i = dcu_ptr->dc_nr_sysprocs - dcu_ptr->dc_nr_tasks;
		i < dcu_ptr->dc_nr_procs - dcu_ptr->dc_nr_tasks; i++){
		rdc_ep = dvk_lcltbind(dcid, my_tid, i);
		if( rdc_ep == i ) break; 
	}
	if( i == dcu_ptr->dc_nr_procs) ERROR_RETURN(EDVSSLOTUSED);
#endif // RDC_IS_FS_PROC_NR

	DVKDEBUG(INTERNAL,"rdisk bind rdc_ep=%d\n",rdc_ep);
	proc_ptr = &rdc_proc;
	rcode = dvk_getprocinfo(dcid, rdc_ep, proc_ptr);
	if(rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG(INTERNAL, PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));	
	
	return(rcode);
}

int umlkrn_fd = -1;

// This thread waits for Kernel requests throght a Unix socket umlkrn_fd
// schedule the operation, and then replies to kernel through the same socket 
int rd_thread(void *arg)
{
	struct rd_thread_req *req;
	int n;
	int rcode;

	os_fix_helper_signals();

	// bind RDISK to DVK 
	rcode = init_rdisk();

//while(1){
	
//	mutex_lock(&rd_tsync);	
	
	while(1){
		DVKDEBUG(INTERNAL,"\n");
		// waiting command from kernel 

		n = os_read_file(umlkrn_fd, &req,
				 sizeof(struct rd_thread_req *));
		if(n != sizeof(struct rd_thread_req *)){
			if(n < 0)
				printk("rd_thread - read failed, fd = %d, "
				       "err = %d\n", umlkrn_fd, -n);
			else {
				printk("rd_thread - short read, fd = %d, "
				       "length = %d\n", umlkrn_fd, n);
			}
			continue;
		}

		DVKDEBUG(INTERNAL,RD_REQ_FORMAT,RD_REQ_FIELDS(req));
		do_rdisk_rqst(req);
		DVKDEBUG(INTERNAL,RD_REQ_FORMAT,RD_REQ_FIELDS(req));
		// Trigger an RDIKS_IRQ ->  rd_handler()
		n = os_write_file(umlkrn_fd, &req,
				  sizeof(struct rd_thread_req *));
		if(n != sizeof(struct rd_thread_req *))
			printk("rd_thread - write failed, fd = %d, err = %d\n",
			       umlkrn_fd, -n);
//		if (req->op == RD_CLOSE) {
//			break;
//		}
	}
//}
	return 0;
}

// #endif // CONFIG_UML_RDISK 
