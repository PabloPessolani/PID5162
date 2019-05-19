/*
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

/* 2001-09-28...2002-04-17
 * Partition stuff by James_McMechan@hotmail.com
 * old style rdisk by setting RD_SHIFT to 0
 * 2002-09-27...2002-10-18 massive tinkering for 2.5
 * partitions have changed in 2.5
 * 2003-01-29 more tinkering for 2.5.59-1
 * This should now address the sysfs problems and has
 * the symlink for devfs to allow for booting with
 * the common /dev/rdisk/discX/... names rather than
 * only /dev/ubdN/discN this version also has lots of
 * clean ups preparing for rdisk-many.
 * James McMechan
 */

#ifdef CONFIG_UML_RDISK 

#define RD_SHIFT 4

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

#include "uml_rdisk.h"

extern proc_usr_t 	uml_proc;
extern int 			dcid; 
extern dvs_usr_t 	dvs;
extern dc_usr_t 	dcu;
extern int 			local_nodeid;
int					rdc_ep; // RDISK Client endpoint 

enum rd_req { RD_READ, RD_WRITE, RD_FLUSH };

static LIST_HEAD(restart);
static int thread_fd = -1;

#define MAX_SG 64

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

struct rd_thread_req {
	struct request *req;
	enum rd_req op;
	unsigned long long offset;
	unsigned long length;
	char *buffer;
	int sectorsize;
	int error;
};

#define RDISK_NAME "rdisk"

static DEFINE_MUTEX(rd_lock);
static DEFINE_MUTEX(rd_mutex); /* replaces BKL, might not be needed */

static int rd_open(struct block_device *bdev, fmode_t mode);
static void rd_release(struct gendisk *disk, fmode_t mode);
static int rd_ioctl(struct block_device *bdev, fmode_t mode,
		     unsigned int cmd, unsigned long arg);
static int rd_getgeo(struct block_device *bdev, struct hd_geometry *geo);

#define MAX_DEV (16)

static const struct block_device_operations rd_blops = {
        .owner		= THIS_MODULE,
        .open		= rd_open,
        .release	= rd_release,
        .ioctl		= rd_ioctl,
		.getgeo		= rd_getgeo,
};

/* Protected by rd_lock */
static int 	  fake_major = RD_MAJOR;
static struct gendisk *rd_gendisk[MAX_DEV];
static struct gendisk *fake_gendisk[MAX_DEV];

#ifdef  ANULADO 

/* Only changed by fake_ide_setup which is a setup */
static int fake_ide = 0;
static struct proc_dir_entry *proc_ide_root = NULL;
static struct proc_dir_entry *proc_ide = NULL;

static void make_proc_ide(void)
{
	proc_ide_root = proc_mkdir("ide", NULL);
	proc_ide = proc_mkdir("ide0", proc_ide_root);
}

static int fake_ide_media_proc_show(struct seq_file *m, void *v)
{
	seq_puts(m, "disk\n");
	return 0;
}

static int fake_ide_media_proc_open(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations fake_ide_media_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= fake_ide_media_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void make_ide_entries(const char *dev_name)
{
	struct proc_dir_entry *dir, *ent;
	char name[64];

	if(proc_ide_root == NULL) make_proc_ide();

	dir = proc_mkdir(dev_name, proc_ide);
	if(!dir) return;

	ent = proc_create("media", S_IRUGO, dir, &fake_ide_media_proc_fops);
	if(!ent) return;
	snprintf(name, sizeof(name), "ide0/%s", dev_name);
	proc_symlink(dev_name, proc_ide_root, name);
}


static void rd_close_dev(struct rdisk *rd_dev)
{

}

static int rd_open_dev(struct rdisk *rd_dev)
{
	return 0;
}

static void rd_device_release(struct device *dev)
{
	struct rdisk *rd_dev = dev_get_drvdata(dev);

	blk_cleanup_queue(rd_dev->queue);
	*rd_dev = ((struct rdisk) DEFAULT_UBD);
}

static int rd_disk_register(int major, u64 size, int unit,
			     struct gendisk **disk_out)
{
	struct device *parent = NULL;
	struct gendisk *disk;

	disk = alloc_disk(1 << RD_SHIFT);
	if(disk == NULL)
		return -ENOMEM;

	disk->major = major;
	disk->first_minor = unit << RD_SHIFT;
	disk->fops = &rd_blops;
	set_capacity(disk, size / SECTOR_SIZE);
	if (major == RD_MAJOR)
		sprintf(disk->disk_name, "rdisk%c", 'a' + unit);
	else
		sprintf(disk->disk_name, "rd_fake%d", unit);

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

#define ROUND_BLOCK(n) ((n + ((1 << 9) - 1)) & (-1 << 9))

static int rd_id(char **str, int *start_out, int *end_out)
{
	int n;

	n = parse_unit(str);
	*start_out = 0;
	*end_out = MAX_DEV - 1;
	return n;
}

static int rd_remove(int n, char **error_out)
{
	return 0;
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

static int __init ubd0_init(void)
{
	struct rdisk *rd_dev = &rd_devs[0];

	mutex_lock(&rd_lock);
	if(rd_dev->file == NULL)
		rd_dev->file = "root_fs";
	mutex_unlock(&rd_lock);

	return 0;
}

__initcall(ubd0_init);

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

	if (register_blkdev(RD_MAJOR, "rdisk"))
		return -1;

	if (fake_major != RD_MAJOR) {
		char name[sizeof("rd_nnn\0")];

		snprintf(name, sizeof(name), "rd_%d", fake_major);
		if (register_blkdev(fake_major, "rdisk"))
			return -1;
	}
	platform_driver_register(&rd_driver);
	mutex_lock(&rd_lock);
	for (i = 0; i < MAX_DEV; i++){
		err = rd_add(i, &error);
		if(err)
			printk(KERN_ERR "Failed to initialize rdisk device %d :"
			       "%s\n", i, error);
	}
	mutex_unlock(&rd_lock);
	return 0;
}

late_initcall(rd_init);

static int __init rd_driver_init(void){
	unsigned long stack;
	int err;

	/* Set by CONFIG_BLK_DEV_RD_SYNC or rdisk=sync.*/
	if(global_openflags.s){
		printk(KERN_INFO "rdisk: Synchronous mode\n");
		/* Letting rdisk=sync be like using rdisk#s= instead of rdisk#= is
		 * enough. So use anyway the io thread. */
	}
	stack = alloc_stack(0, 0);
	io_pid = start_io_thread(stack + PAGE_SIZE - sizeof(void *), &thread_fd);
	if(io_pid < 0){
		printk(KERN_ERR
		       "rdisk : Failed to start I/O thread (errno = %d) - "
		       "falling back to synchronous I/O\n", -io_pid);
		io_pid = -1;
		return 0;
	}
	err = um_request_irq(RD_IRQ, thread_fd, IRQ_READ, rd_intr, 0, "rdisk", rd_devs);
	if(err != 0)
		printk(KERN_ERR "um_request_irq failed - errno = %d\n", -err);
	return 0;
}

device_initcall(rd_driver_init);

static int rd_open(struct block_device *bdev, fmode_t mode)
{
	struct gendisk *disk = bdev->bd_disk;
	struct rdisk *rd_dev = disk->private_data;
	int err = 0;

	mutex_lock(&rd_mutex);
	if(rd_dev->count == 0){
		err = rd_open_dev(rd_dev);
		if(err){
			printk(KERN_ERR "%s: Can't open \"%s\": errno = %d\n",
			       disk->disk_name, rd_dev->file, -err);
			goto out;
		}
	}
	rd_dev->count++;
	set_disk_ro(disk, !rd_dev->openflags.w);

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
	struct rdisk *rd_dev = disk->private_data;

	mutex_lock(&rd_mutex);
	if(--rd_dev->count == 0)
		rd_close_dev(rd_dev);
	mutex_unlock(&rd_mutex);
}

#endif // ANULADO 

/* Called with dev->lock held */
static void prepare_request(struct request *req, struct rd_thread_req *io_req,
			    unsigned long long offset, int page_offset,
			    int len, struct page *page)
{
	struct gendisk *disk = req->rq_disk;
	struct rdisk *rd_dev = disk->private_data;

	io_req->req = req;
	io_req->offset = offset;
	io_req->length = len;
	io_req->error = 0;
	io_req->op = (rq_data_dir(req) == READ) ? RD_READ : RD_WRITE;
	io_req->offset= 0;
	io_req->buffer = page_address(page) + page_offset;
	io_req->sectorsize = 1 << 9;

}

/* Called with dev->lock held */
static void prepare_flush_request(struct request *req,
				  struct rd_thread_req *io_req)
{
	struct gendisk *disk = req->rq_disk;
	struct rdisk *rd_dev = disk->private_data;

	io_req->req = req;
	io_req->op = RD_FLUSH;
}

static bool submit_request(struct rd_thread_req *io_req, struct rdisk *dev)
{
	int n = os_write_file(thread_fd, &io_req,
			     sizeof(io_req));
	if (n != sizeof(io_req)) {
		if (n != -EAGAIN)
			printk("write to io thread failed, "
			       "errno = %d\n", -n);
		else if (list_empty(&dev->restart))
			list_add(&dev->restart, &restart);

		kfree(io_req);
		return false;
	}
	return true;
}

/* Called with dev->lock held */
static void do_rd_request(struct request_queue *q)
{
	struct rd_thread_req *io_req;
	struct request *req;

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
			io_req = kmalloc(sizeof(struct rd_thread_req),
					 GFP_ATOMIC);
			if (io_req == NULL) {
				if (list_empty(&dev->restart))
					list_add(&dev->restart, &restart);
				return;
			}
			prepare_flush_request(req, io_req);
			if (submit_request(io_req, dev) == false)
				return;
		}

		while(dev->start_sg < dev->end_sg){
			struct scatterlist *sg = &dev->sg[dev->start_sg];

			io_req = kmalloc(sizeof(struct rd_thread_req),
					 GFP_ATOMIC);
			if(io_req == NULL){
				if(list_empty(&dev->restart))
					list_add(&dev->restart, &restart);
				return;
			}
			prepare_request(req, io_req,
					(unsigned long long)dev->rq_pos << 9,
					sg->offset, sg->length, sg_page(sg));

			if (submit_request(io_req, dev) == false)
				return;

			dev->rq_pos += sg->length >> 9;
			dev->start_sg++;
		}
		dev->end_sg = 0;
		dev->request = NULL;
	}
}

static int rd_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	return 0;
}

static int rd_ioctl(struct block_device *bdev, fmode_t mode,
		     unsigned int cmd, unsigned long arg)
{
	return -EINVAL;
}


static void do_rdisk(struct rd_thread_req *req)
{
	char *buf;
	unsigned long len;
	int n, nsectors, start, end, bit;
	__u64 off;

	if (req->op == RD_FLUSH) {
		printk("do_rdisk - sync ignored\n");
		return;
	}

	nsectors = req->length/req->sectorsize;
	start = 0;
	do {
		off = req->offset +	start * req->sectorsize;
		len = (end - start) * req->sectorsize;
		buf = &req->buffer[start * req->sectorsize];

		if(req->op == RD_READ){
			n = 0;
			do {
				buf = &buf[n];
				len -= n;
				n = rdisk_read(buf, len, off);
				if (n < 0) {
					printk("do_rdisk - read failed, err = %d\n", n);
					req->error = 1;
					return;
				}
			} while((n < len) && (n != 0));
			if (n < len) memset(&buf[n], 0, len - n);
		} else {
			n = rdisk_write( buf, len, off);
			if(n != len){
				printk("do_rdisk - write failed err = %d\n", n);
				req->error = 1;
				return;
			}
		}

		start = end;
	} while(start < nsectors);

	req->error = 0;
}

/* Changed in start_io_thread, which is serialized by being called only
 * from rd_init, which is an initcall.
 */

static int init_rdisk(void)
{
	int rcode, i;
	dvs_usr_t *dvsu_ptr;
	dc_usr_t *dcu_ptr;
	proc_usr_t *proc_ptr;
	
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
	

	for( i = dcu_ptr->dc_nr_sysprocs - dcu_ptr->dc_nr_procs;
		i < dcu_ptr->dc_nr_procs; i++){
		rdc_ep = dvk_tbind(dcid, i);
		if( rdc_ep == i ) break; 
	}
	if( i == dcu_ptr->dc_nr_procs) ERROR_RETURN(EDVSSLOTUSED);
	DVKDEBUG(INTERNAL,"rdisk bind rdc_ep=%d\n",rdc_ep);
	
	proc_ptr = &uml_proc;
	rcode = dvk_getprocinfo(dcid, rdc_ep, proc_ptr);
	if(rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG(INTERNAL, PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));	
	
}

int rdc_ep; // RDISK Client Endpoint 
int rdk_fd = -1;
/* Only changed by the io thread. XXX: currently unused. */
static int rd_count = 0;

int rd_thread(void *arg)
{
	struct rd_thread_req *req;
	int n;
	int rcode;

	os_fix_helper_signals();

	rcode = init_rdisk();

	while(1){
		n = os_read_file(rdk_fd, &req,
				 sizeof(struct rd_thread_req *));
		if(n != sizeof(struct rd_thread_req *)){
			if(n < 0)
				printk("rd_thread - read failed, fd = %d, "
				       "err = %d\n", rdk_fd, -n);
			else {
				printk("rd_thread - short read, fd = %d, "
				       "length = %d\n", rdk_fd, n);
			}
			continue;
		}
		rd_count++;
		do_rdisk(req);
		n = os_write_file(rdk_fd, &req,
				  sizeof(struct rd_thread_req *));
		if(n != sizeof(struct rd_thread_req *))
			printk("rd_thread - write failed, fd = %d, err = %d\n",
			       rdk_fd, -n);
	}

	return 0;
}

#endif // CONFIG_UML_RDISK 
