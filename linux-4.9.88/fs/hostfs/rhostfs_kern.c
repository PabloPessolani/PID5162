/*
 * Copyright (C) 2000 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 *
 * Ported the filesystem routines to 2.5.
 * 2003-02-10 Petr Baudis <pasky@ucw.cz>
 */
#ifdef CONFIG_RHOSTFS

#define RHOSTFS_GLOBAL_HERE

#include <linux/fs.h>
#include <linux/magic.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/statfs.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/dirent.h>
#include <init.h>
#include <kern.h>
#include "rhostfs.h"

static char *dentry_name(struct dentry *dentry);
static char *inode_name(struct inode *ino);


struct rhostfs_inode_info {
	int fd;
	fmode_t mode;
	struct inode vfs_inode;
	struct mutex open_mutex;
};

static inline struct rhostfs_inode_info *RHOSTFS_I(struct inode *inode)
{
	RHDEBUG("i_ino=%ld\n",inode->i_ino);	
	return list_entry(inode, struct rhostfs_inode_info, vfs_inode);
}

#define FILE_RHOSTFS_I(file) RHOSTFS_I(file_inode(file))

/* Changed in hostfs_args before the kernel starts running */
static char *rh_root_ino = "";
static int rh_append;

static const struct inode_operations rhostfs_iops;
static const struct inode_operations rhostfs_dir_iops;
static const struct inode_operations rhostfs_link_iops;

#ifdef ANULADO 

#ifndef MODULE
static int __init rhostfs_args(char *options, int *add)
{
	char *ptr;

	ptr = strchr(options, ',');
	if (ptr != NULL)
		*ptr++ = '\0';
	if (*options != '\0')
		rh_root_ino = options;

	options = ptr;
	while (options) {
		ptr = strchr(options, ',');
		if (ptr != NULL)
			*ptr++ = '\0';
		if (*options != '\0') {
			if (!strcmp(options, "rh_append"))
				rh_append = 1;
			else printf("rhostfs_args - unsupported option - %s\n",
				    options);
		}
		options = ptr;
	}
	return 0;
}

__uml_setup("rhostfs=", rhostfs_args,
"rhostfs=<root dir>,<flags>,...\n"
"    This is used to set rhostfs parameters.  The root directory argument\n"
"    is used to confine all rhostfs mounts to within the specified directory\n"
"    tree on the host.  If this isn't specified, then a user inside UML can\n"
"    mount anything on the host that's accessible to the user that's running\n"
"    it.\n"
"    The only flag currently supported is 'rh_append', which specifies that all\n"
"    files opened by rhostfs will be opened in rh_append mode.\n\n"
);
#endif
#endif // ANULADO 


static char *__dentry_name(struct dentry *dentry, char *name)
{
	char *p = dentry_path_raw(dentry, name, PATH_MAX);
	char *root;
	size_t len;
	
	RHDEBUG("\n");	

	root = dentry->d_sb->s_fs_info;
	len = strlen(root);
	if (IS_ERR(p)) {
		__putname(name);
		return NULL;
	}

	/*
	 * This function relies on the fact that dentry_path_raw() will place
	 * the path name at the end of the provided buffer.
	 */
	BUG_ON(p + strlen(p) + 1 != name + PATH_MAX);

	strlcpy(name, root, PATH_MAX);
	if (len > p - name) {
		__putname(name);
		return NULL;
	}

	if (p > name + len)
		strcpy(name + len, p);

	return name;
}

static char *dentry_name(struct dentry *dentry)
{
	RHDEBUG("\n");	
	char *name = __getname();
	if (!name) {
		ERROR_PRINT(EDVSNOENT);
		return NULL;
	}
	RHDEBUG("name=%s\n", name);	
	return __dentry_name(dentry, name);
}

static char *inode_name(struct inode *ino)
{
	struct dentry *dentry;
	char *name;

	RHDEBUG("\n");	

	dentry = d_find_alias(ino);
	if (!dentry)
		return NULL;

	name = dentry_name(dentry);

	dput(dentry);

	return name;
}

static char *rhostfs_follow_link(char *link)
{
	int len, n;
	char *name, *resolved, *end;

	RHDEBUG("\n");	

	name = __getname();
	if (!name) {
		n = -ENOMEM;
		goto out_free;
	}

	n = rmt_readlink(link, name, PATH_MAX);
	if (n < 0)
		goto out_free;
	else if (n == PATH_MAX) {
		n = -E2BIG;
		goto out_free;
	}

	if (*name == '/')
		return name;

	end = strrchr(link, '/');
	if (end == NULL)
		return name;

	*(end + 1) = '\0';
	len = strlen(link) + strlen(name) + 1;

	resolved = kmalloc(len, GFP_KERNEL);
	if (resolved == NULL) {
		n = -ENOMEM;
		goto out_free;
	}

	sprintf(resolved, "%s%s", link, name);
	__putname(name);
	kfree(link);
	return resolved;

 out_free:
	__putname(name);
	return ERR_PTR(n);
}

static struct inode *rhostfs_iget(struct super_block *sb)
{
	RHDEBUG("\n");	

	struct inode *inode = new_inode(sb);
	if (!inode){
		ERROR_PRINT(EDVSNOMEM);
		return ERR_PTR(-ENOMEM);
	}
	return inode;
}

static int rhostfs_statfs(struct dentry *dentry, struct kstatfs *sf)
{
	/*
	 * rh_do_statfs uses struct statfs64 internally, but the linux kernel
	 * struct statfs still has 32-bit versions for most of these fields,
	 * so we convert them here
	 */
	int err;
	long long f_blocks;
	long long f_bfree;
	long long f_bavail;
	long long f_files;
	long long f_ffree;

	RHDEBUG("\n");	

	err = rh_do_statfs(dentry->d_sb->s_fs_info,
			&sf->f_bsize, &f_blocks, &f_bfree, &f_bavail, &f_files,
			&f_ffree, &sf->f_fsid, sizeof(sf->f_fsid),
			&sf->f_namelen);
	if (err)
		return err;
	sf->f_blocks = f_blocks;
	sf->f_bfree = f_bfree;
	sf->f_bavail = f_bavail;
	sf->f_files = f_files;
	sf->f_ffree = f_ffree;
	sf->f_type = HOSTFS_SUPER_MAGIC;
	return 0;
}

static struct inode *rhostfs_alloc_inode(struct super_block *sb)
{
	struct rhostfs_inode_info *hi;

	RHDEBUG("\n");	

	hi = kmalloc(sizeof(*hi), GFP_KERNEL_ACCOUNT);
	if (hi == NULL) {
		ERROR_PRINT(EDVSNOMEM);
		return NULL;
	}
	hi->fd = -1;
	hi->mode = 0;
	inode_init_once(&hi->vfs_inode);
	mutex_init(&hi->open_mutex);
	return &hi->vfs_inode;
}

static void rhostfs_evict_inode(struct inode *inode)
{
	RHDEBUG("\n");	

	truncate_inode_pages_final(&inode->i_data);
	clear_inode(inode);
	if (RHOSTFS_I(inode)->fd != -1) {
		rh_close_file(&RHOSTFS_I(inode)->fd);
		RHOSTFS_I(inode)->fd = -1;
	}
}

static void rhostfs_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);
	kfree(RHOSTFS_I(inode));
}

static void rhostfs_destroy_inode(struct inode *inode)
{
	RHDEBUG("\n");	

	call_rcu(&inode->i_rcu, rhostfs_i_callback);
}

static int rhostfs_show_options(struct seq_file *seq, struct dentry *root)
{
	RHDEBUG("\n");	

	const char *root_path = root->d_sb->s_fs_info;
	size_t offset = strlen(rh_root_ino) + 1;

	RHDEBUG("root_path=%s\n",root_path);	

	if (strlen(root_path) > offset)
		seq_show_option(seq, root_path + offset, NULL);

	if (rh_append)
		seq_puts(seq, ",rh_append");

	return 0;
}

static const struct super_operations rhostfs_sbops = {
	.alloc_inode	= rhostfs_alloc_inode,
	.destroy_inode	= rhostfs_destroy_inode,
	.evict_inode	= rhostfs_evict_inode,
	.statfs		= rhostfs_statfs,
	.show_options	= rhostfs_show_options,
};

static int rhostfs_readdir(struct file *file, struct dir_context *ctx)
{
	void *dir;
	char *name;
	unsigned long long next, ino;
	int error, len;
	unsigned int type;

	RHDEBUG("\n");	
	name = dentry_name(file->f_path.dentry);
	RHDEBUG("name=%s\n", name);	
	if (name == NULL) ERROR_RETURN(-ENOMEM);
	dir = rh_open_dir(name, &error);
	__putname(name);
	if (dir == NULL) ERROR_RETURN(-error);
	next = ctx->pos;
	rh_seek_dir(dir, next);
	while ((name = rh_read_dir(dir, &next, &ino, &len, &type)) != NULL) {
		if (!dir_emit(ctx, name, len, ino, type))
			break;
		ctx->pos = next;
	}
	rh_close_dir(dir);
	return 0;
}

static int rhostfs_open(struct inode *ino, struct file *file)
{
	char *name;
	fmode_t mode;
	int err;
	int r, w, fd;

	RHDEBUG("i_ino=%ld\n", ino->i_ino);	

	mode = file->f_mode & (FMODE_READ | FMODE_WRITE);
	if ((mode & RHOSTFS_I(ino)->mode) == mode)
		return 0;

	mode |= RHOSTFS_I(ino)->mode;

retry:
	r = w = 0;

	if (mode & FMODE_READ)
		r = 1;
	if (mode & FMODE_WRITE)
		r = w = 1;

	name = dentry_name(file->f_path.dentry);
	if (name == NULL)
		ERROR_RETURN(-ENOMEM);

	fd = rh_open_file(name, r, w, rh_append);
	__putname(name);
	if (fd < 0)
		return fd;

	mutex_lock(&RHOSTFS_I(ino)->open_mutex);
	/* somebody else had handled it first? */
	if ((mode & RHOSTFS_I(ino)->mode) == mode) {
		mutex_unlock(&RHOSTFS_I(ino)->open_mutex);
		rh_close_file(&fd);
		return 0;
	}
	if ((mode | RHOSTFS_I(ino)->mode) != mode) {
		mode |= RHOSTFS_I(ino)->mode;
		mutex_unlock(&RHOSTFS_I(ino)->open_mutex);
		rh_close_file(&fd);
		goto retry;
	}
	if (RHOSTFS_I(ino)->fd == -1) {
		RHOSTFS_I(ino)->fd = fd;
	} else {
		err = rh_replace_file(fd, RHOSTFS_I(ino)->fd);
		rh_close_file(&fd);
		if (err < 0) {
			mutex_unlock(&RHOSTFS_I(ino)->open_mutex);
			ERROR_RETURN(err);
		}
	}
	RHOSTFS_I(ino)->mode = mode;
	mutex_unlock(&RHOSTFS_I(ino)->open_mutex);

	return 0;
}

static int rhostfs_file_release(struct inode *inode, struct file *file)
{
	RHDEBUG("\n");	

	filemap_write_and_wait(inode->i_mapping);

	return 0;
}

static int rhostfs_fsync(struct file *file, loff_t start, loff_t end,
			int datasync)
{
	struct inode *inode = file->f_mapping->host;
	int ret;

	RHDEBUG("\n");	

	ret = filemap_write_and_wait_range(inode->i_mapping, start, end);
	if (ret)
		return ret;

	inode_lock(inode);
	ret = rh_fsync_file(RHOSTFS_I(inode)->fd, datasync);
	inode_unlock(inode);

	return ret;
}

static const struct file_operations rhostfs_file_fops = {
	.llseek		= generic_file_llseek,
	.splice_read	= generic_file_splice_read,
	.read_iter	= generic_file_read_iter,
	.write_iter	= generic_file_write_iter,
	.mmap		= generic_file_mmap,
	.open		= rhostfs_open,
	.release	= rhostfs_file_release,
	.fsync		= rhostfs_fsync,
};

static const struct file_operations rhostfs_dir_fops = {
	.llseek		= generic_file_llseek,
	.iterate_shared	= rhostfs_readdir,
	.read		= generic_read_dir,
	.open		= rhostfs_open,
	.fsync		= rhostfs_fsync,
};

static int rhostfs_writepage(struct page *page, struct writeback_control *wbc)
{
	RHDEBUG("\n");	

	struct address_space *mapping = page->mapping;
	struct inode *inode = mapping->host;
	char *buffer;
	loff_t base = page_offset(page);
	int count = PAGE_SIZE;
	int end_index = inode->i_size >> PAGE_SHIFT;
	int err;

	if (page->index >= end_index)
		count = inode->i_size & (PAGE_SIZE-1);

	buffer = kmap(page);

	err = rh_write_file(RHOSTFS_I(inode)->fd, &base, buffer, count);
	if (err != count) {
		ClearPageUptodate(page);
		goto out;
	}

	if (base > inode->i_size)
		inode->i_size = base;

	if (PageError(page))
		ClearPageError(page);
	err = 0;

 out:
	kunmap(page);

	unlock_page(page);
	return err;
}

static int rhostfs_readpage(struct file *file, struct page *page)
{
	RHDEBUG("\n");	

	char *buffer;
	loff_t start = page_offset(page);
	int bytes_read, ret = 0;

	buffer = kmap(page);
	bytes_read = rh_read_file(FILE_RHOSTFS_I(file)->fd, &start, buffer,
			PAGE_SIZE);
	if (bytes_read < 0) {
		ClearPageUptodate(page);
		SetPageError(page);
		ret = bytes_read;
		goto out;
	}

	memset(buffer + bytes_read, 0, PAGE_SIZE - bytes_read);

	ClearPageError(page);
	SetPageUptodate(page);

 out:
	flush_dcache_page(page);
	kunmap(page);
	unlock_page(page);
	return ret;
}

static int rhostfs_write_begin(struct file *file, struct address_space *mapping,
			      loff_t pos, unsigned len, unsigned flags,
			      struct page **pagep, void **fsdata)
{
	RHDEBUG("\n");	

	pgoff_t index = pos >> PAGE_SHIFT;

	*pagep = grab_cache_page_write_begin(mapping, index, flags);
	if (!*pagep)
		return -ENOMEM;
	return 0;
}

static int rhostfs_write_end(struct file *file, struct address_space *mapping,
			    loff_t pos, unsigned len, unsigned copied,
			    struct page *page, void *fsdata)
{
	RHDEBUG("\n");	

	struct inode *inode = mapping->host;
	void *buffer;
	unsigned from = pos & (PAGE_SIZE - 1);
	int err;

	buffer = kmap(page);
	err = rh_write_file(FILE_RHOSTFS_I(file)->fd, &pos, buffer + from, copied);
	kunmap(page);

	if (!PageUptodate(page) && err == PAGE_SIZE)
		SetPageUptodate(page);

	/*
	 * If err > 0, rh_write_file has added err to pos, so we are comparing
	 * i_size against the last byte written.
	 */
	if (err > 0 && (pos > inode->i_size))
		inode->i_size = pos;
	unlock_page(page);
	put_page(page);

	return err;
}

static const struct address_space_operations rhostfs_aops = {
	.writepage 	= rhostfs_writepage,
	.readpage	= rhostfs_readpage,
	.set_page_dirty = __set_page_dirty_nobuffers,
	.write_begin	= rhostfs_write_begin,
	.write_end	= rhostfs_write_end,
};

static int rh_read_name(struct inode *ino, char *name)
{
	dev_t rdev;
	struct hostfs_stat st;

	RHDEBUG("name=%s\n",name);	

	int err = rh_stat_file(name, &st, -1);
	if (err) ERROR_RETURN(err);
	
	/* Reencode maj and min with the kernel encoding.*/
	rdev = MKDEV(st.maj, st.min);

	RHDEBUG("rdev=%d st.maj=%d st.min=%d st.mode=%X\n",rdev,st.maj,st.min, st.mode);	

	switch (st.mode & S_IFMT) {
	case S_IFLNK:
		RHDEBUG("S_IFLNK\n");	
		ino->i_op = &rhostfs_link_iops;
		break;
	case S_IFDIR:
		RHDEBUG("S_IFDIR\n");	
		ino->i_op = &rhostfs_dir_iops;
		ino->i_fop = &rhostfs_dir_fops;
		break;
	case S_IFCHR:
	case S_IFBLK:
	case S_IFIFO:
	case S_IFSOCK:
		RHDEBUG("S_DEVICE\n");	
		init_special_inode(ino, st.mode & S_IFMT, rdev);
		ino->i_op = &rhostfs_iops;
		break;
	case S_IFREG:
		RHDEBUG("S_IFREG\n");	
		ino->i_op = &rhostfs_iops;
		ino->i_fop = &rhostfs_file_fops;
		ino->i_mapping->a_ops = &rhostfs_aops;
		break;
	default:
		ERROR_RETURN(-EIO);
	}

	ino->i_ino = st.ino;
	ino->i_mode = st.mode;
	set_nlink(ino, st.nlink);
	i_uid_write(ino, st.uid);
	i_gid_write(ino, st.gid);
	ino->i_atime = st.atime;
	ino->i_mtime = st.mtime;
	ino->i_ctime = st.ctime;
	ino->i_size = st.size;
	ino->i_blocks = st.blocks;
	return 0;
}

static int read_name(struct inode *ino, char *name)
{
	dev_t rdev;
	struct hostfs_stat st;
	int err = stat_file(name, &st, -1);
	if (err)
		return err;

	/* Reencode maj and min with the kernel encoding.*/
	rdev = MKDEV(st.maj, st.min);

	switch (st.mode & S_IFMT) {
	case S_IFLNK:
		ino->i_op = &rhostfs_link_iops;
		break;
	case S_IFDIR:
		ino->i_op = &rhostfs_dir_iops;
		ino->i_fop = &rhostfs_dir_fops;
		break;
	case S_IFCHR:
	case S_IFBLK:
	case S_IFIFO:
	case S_IFSOCK:
		init_special_inode(ino, st.mode & S_IFMT, rdev);
		ino->i_op = &rhostfs_iops;
		break;
	case S_IFREG:
		ino->i_op = &rhostfs_iops;
		ino->i_fop = &rhostfs_file_fops;
		ino->i_mapping->a_ops = &rhostfs_aops;
		break;
	default:
		return -EIO;
	}

	ino->i_ino = st.ino;
	ino->i_mode = st.mode;
	set_nlink(ino, st.nlink);
	i_uid_write(ino, st.uid);
	i_gid_write(ino, st.gid);
	ino->i_atime = st.atime;
	ino->i_mtime = st.mtime;
	ino->i_ctime = st.ctime;
	ino->i_size = st.size;
	ino->i_blocks = st.blocks;
	return 0;
}

static int rhostfs_create(struct inode *dir, struct dentry *dentry, umode_t mode,
			 bool excl)
{
	struct inode *inode;
	char *name;
	int error, fd;

	RHDEBUG("\n");	

	inode = rhostfs_iget(dir->i_sb);
	if (IS_ERR(inode)) {
		error = PTR_ERR(inode);
		ERROR_PRINT(EDVSNOMEM);
		goto out;
	}

	error = -ENOMEM;
	name = dentry_name(dentry);
	if (name == NULL){
		ERROR_PRINT(EDVSNOENT);
		goto out_put;
	}
	
	fd = rh_file_create(name, mode & 0777);
	if (fd < 0)
		error = fd;
	else
		error = rh_read_name(inode, name);

	__putname(name);
	if (error) {
		ERROR_PRINT(error);
		goto out_put;
	}
	
	RHOSTFS_I(inode)->fd = fd;
	RHOSTFS_I(inode)->mode = FMODE_READ | FMODE_WRITE;
	d_instantiate(dentry, inode);
	return 0;

 out_put:
	iput(inode);
 out:
	ERROR_RETURN(error);
}

static struct dentry *rhostfs_lookup(struct inode *ino, struct dentry *dentry,
				    unsigned int flags)
{
	struct inode *inode;
	char *name;
	int err;

	RHDEBUG("\n");	

	inode = rhostfs_iget(ino->i_sb);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		ERROR_PRINT(err);
		goto out;
	}

	err = -ENOMEM;
	name = dentry_name(dentry);
	if (name == NULL){
		ERROR_PRINT(err);
		goto out_put;
	}
	
	err = rh_read_name(inode, name);

	__putname(name);
	if (err == -ENOENT) {
		iput(inode);
		inode = NULL;
		ERROR_PRINT(err);
	}
	else if (err) {
		ERROR_PRINT(err);
		goto out_put;
	}
	d_add(dentry, inode);
	return NULL;

 out_put:
	iput(inode);
 out:
	ERROR_PRINT(err);
	return ERR_PTR(err);
}

static int rhostfs_link(struct dentry *to, struct inode *ino,
		       struct dentry *from)
{
	char *from_name, *to_name;
	int err;

	RHDEBUG("\n");	

	if ((from_name = dentry_name(from)) == NULL)
		return -ENOMEM;
	to_name = dentry_name(to);
	if (to_name == NULL) {
		__putname(from_name);
		return -ENOMEM;
	}
	err = rh_link_file(to_name, from_name);
	__putname(from_name);
	__putname(to_name);
	return err;
}

static int rhostfs_unlink(struct inode *ino, struct dentry *dentry)
{
	char *file;
	int err;

	RHDEBUG("\n");	

	if (rh_append)
		return -EPERM;

	if ((file = dentry_name(dentry)) == NULL)
		return -ENOMEM;

	err = rh_unlink_file(file);
	__putname(file);
	return err;
}

static int rhostfs_symlink(struct inode *ino, struct dentry *dentry,
			  const char *to)
{
	char *file;
	int err;

	RHDEBUG("\n");	

	if ((file = dentry_name(dentry)) == NULL)
		return -ENOMEM;
	err = rh_make_symlink(file, to);
	__putname(file);
	return err;
}

static int rhostfs_mkdir(struct inode *ino, struct dentry *dentry, umode_t mode)
{
	char *file;
	int err;

	RHDEBUG("\n");	

	if ((file = dentry_name(dentry)) == NULL)
		return -ENOMEM;
	err = rh_do_mkdir(file, mode);
	__putname(file);
	return err;
}

static int rhostfs_rmdir(struct inode *ino, struct dentry *dentry)
{
	char *file;
	int err;

	RHDEBUG("\n");	

	if ((file = dentry_name(dentry)) == NULL)
		return -ENOMEM;
	err = rh_do_rmdir(file);
	__putname(file);
	return err;
}

static int rhostfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev)
{
	struct inode *inode;
	char *name;
	int err;

	RHDEBUG("\n");	

	inode = rhostfs_iget(dir->i_sb);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out;
	}

	err = -ENOMEM;
	name = dentry_name(dentry);
	if (name == NULL)
		goto out_put;

	init_special_inode(inode, mode, dev);
	err = rh_do_mknod(name, mode, MAJOR(dev), MINOR(dev));
	if (err)
		goto out_free;

	err = read_name(inode, name);
	__putname(name);
	if (err)
		goto out_put;

	d_instantiate(dentry, inode);
	return 0;

 out_free:
	__putname(name);
 out_put:
	iput(inode);
 out:
	return err;
}

static int rhostfs_rename2(struct inode *old_dir, struct dentry *old_dentry,
			  struct inode *new_dir, struct dentry *new_dentry,
			  unsigned int flags)
{
	char *old_name, *new_name;
	int err;

	RHDEBUG("\n");	

	if (flags & ~(RENAME_NOREPLACE | RENAME_EXCHANGE))
		return -EINVAL;

	old_name = dentry_name(old_dentry);
	if (old_name == NULL)
		return -ENOMEM;
	new_name = dentry_name(new_dentry);
	if (new_name == NULL) {
		__putname(old_name);
		return -ENOMEM;
	}
	if (!flags)
		err = rh_rename_file(old_name, new_name);
	else
		err = rh_rename2_file(old_name, new_name, flags);

	__putname(old_name);
	__putname(new_name);
	return err;
}

static int rhostfs_permission(struct inode *ino, int desired)
{
	char *name;
	int r = 0, w = 0, x = 0, err;

	RHDEBUG("\n");	

	if (desired & MAY_NOT_BLOCK)
		return -ECHILD;

	if (desired & MAY_READ) r = 1;
	if (desired & MAY_WRITE) w = 1;
	if (desired & MAY_EXEC) x = 1;
	name = inode_name(ino);
	if (name == NULL)
		return -ENOMEM;

	if (S_ISCHR(ino->i_mode) || S_ISBLK(ino->i_mode) ||
	    S_ISFIFO(ino->i_mode) || S_ISSOCK(ino->i_mode))
		err = 0;
	else
		err = rh_access_file(name, r, w, x);
	__putname(name);
	if (!err)
		err = generic_permission(ino, desired);
	return err;
}

static int rhostfs_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = d_inode(dentry);
	struct hostfs_iattr attrs;
	char *name;
	int err;

	RHDEBUG("\n");	

	int fd = RHOSTFS_I(inode)->fd;

	err = setattr_prepare(dentry, attr);
	if (err)
		return err;

	if (rh_append)
		attr->ia_valid &= ~ATTR_SIZE;

	attrs.ia_valid = 0;
	if (attr->ia_valid & ATTR_MODE) {
		attrs.ia_valid |= HOSTFS_ATTR_MODE;
		attrs.ia_mode = attr->ia_mode;
	}
	if (attr->ia_valid & ATTR_UID) {
		attrs.ia_valid |= HOSTFS_ATTR_UID;
		attrs.ia_uid = from_kuid(&init_user_ns, attr->ia_uid);
	}
	if (attr->ia_valid & ATTR_GID) {
		attrs.ia_valid |= HOSTFS_ATTR_GID;
		attrs.ia_gid = from_kgid(&init_user_ns, attr->ia_gid);
	}
	if (attr->ia_valid & ATTR_SIZE) {
		attrs.ia_valid |= HOSTFS_ATTR_SIZE;
		attrs.ia_size = attr->ia_size;
	}
	if (attr->ia_valid & ATTR_ATIME) {
		attrs.ia_valid |= HOSTFS_ATTR_ATIME;
		attrs.ia_atime = attr->ia_atime;
	}
	if (attr->ia_valid & ATTR_MTIME) {
		attrs.ia_valid |= HOSTFS_ATTR_MTIME;
		attrs.ia_mtime = attr->ia_mtime;
	}
	if (attr->ia_valid & ATTR_CTIME) {
		attrs.ia_valid |= HOSTFS_ATTR_CTIME;
		attrs.ia_ctime = attr->ia_ctime;
	}
	if (attr->ia_valid & ATTR_ATIME_SET) {
		attrs.ia_valid |= HOSTFS_ATTR_ATIME_SET;
	}
	if (attr->ia_valid & ATTR_MTIME_SET) {
		attrs.ia_valid |= HOSTFS_ATTR_MTIME_SET;
	}
	name = dentry_name(dentry);
	if (name == NULL)
		return -ENOMEM;
	err = rh_set_attr(name, &attrs, fd);
	__putname(name);
	if (err)
		return err;

	if ((attr->ia_valid & ATTR_SIZE) &&
	    attr->ia_size != i_size_read(inode))
		truncate_setsize(inode, attr->ia_size);

	setattr_copy(inode, attr);
	mark_inode_dirty(inode);
	return 0;
}

static const struct inode_operations rhostfs_iops = {
	.permission	= rhostfs_permission,
	.setattr	= rhostfs_setattr,
};

static const struct inode_operations rhostfs_dir_iops = {
	.create		= rhostfs_create,
	.lookup		= rhostfs_lookup,
	.link		= rhostfs_link,
	.unlink		= rhostfs_unlink,
	.symlink	= rhostfs_symlink,
	.mkdir		= rhostfs_mkdir,
	.rmdir		= rhostfs_rmdir,
	.mknod		= rhostfs_mknod,
	.rename		= rhostfs_rename2,
	.permission	= rhostfs_permission,
	.setattr	= rhostfs_setattr,
};

static const char *rhostfs_get_link(struct dentry *dentry,
				   struct inode *inode,
				   struct delayed_call *done)
{
	char *link;
	if (!dentry)
		return ERR_PTR(-ECHILD);
	link = kmalloc(PATH_MAX, GFP_KERNEL);
	if (link) {
		char *path = dentry_name(dentry);
		int err = -ENOMEM;
		if (path) {
			err = rmt_readlink(path, link, PATH_MAX);
			if (err == PATH_MAX)
				err = -E2BIG;
			__putname(path);
		}
		if (err < 0) {
			kfree(link);
			return ERR_PTR(err);
		}
	} else {
		return ERR_PTR(-ENOMEM);
	}

	set_delayed_call(done, kfree_link, link);
	return link;
}

static const struct inode_operations rhostfs_link_iops = {
	.readlink	= generic_readlink,
	.get_link	= rhostfs_get_link,
};

static int rhostfs_fill_sb_common(struct super_block *sb, void *d, int silent)
{
	struct inode *root_inode;
	char *host_root_path, *req_root = d;
	int err;

	RHDEBUG("req_root=%s\n", req_root);	

	sb->s_blocksize = 1024;
	sb->s_blocksize_bits = 10;
	sb->s_magic = HOSTFS_SUPER_MAGIC;
	sb->s_op = &rhostfs_sbops;
	sb->s_d_op = &simple_dentry_operations;
	sb->s_maxbytes = MAX_LFS_FILESIZE;

	/* NULL is printed as <NULL> by sprintf: avoid that. */
	if (req_root == NULL)
		req_root = "";

	err = -ENOMEM;
	sb->s_fs_info = host_root_path =
		kmalloc(strlen(rh_root_ino) + strlen(req_root) + 2, GFP_KERNEL);
	if (host_root_path == NULL)
		goto out;

	sprintf(host_root_path, "%s/%s", rh_root_ino, req_root);

	root_inode = new_inode(sb);
	if (!root_inode)
		goto out;

	err = read_name(root_inode, host_root_path);
	if (err)
		goto out_put;

	if (S_ISLNK(root_inode->i_mode)) {
		char *name = rhostfs_follow_link(host_root_path);
		if (IS_ERR(name)) {
			err = PTR_ERR(name);
			goto out_put;
		}
		err = read_name(root_inode, name);
		kfree(name);
		if (err)
			goto out_put;
	}

	err = -ENOMEM;
	sb->s_root = d_make_root(root_inode);
	if (sb->s_root == NULL)
		goto out;

	return 0;

out_put:
	iput(root_inode);
out:
	return err;
}

static struct dentry *rhostfs_read_sb(struct file_system_type *type,
			  int flags, const char *dev_name,
			  void *data)
{
	int rcode;
	RHDEBUG("\n");

	rcode = rh_get_rootpath(rootpath);
//	data  = rootpath;
	
	rcode = mount_nodev(type, flags, data, rhostfs_fill_sb_common);
	if( rcode < 0) ERROR_RETURN(rcode);
	return(rcode);
}

static void rhostfs_kill_sb(struct super_block *s)
{
	RHDEBUG("\n");	

	kill_anon_super(s);
	kfree(s->s_fs_info);
}

static struct file_system_type rhostfs_type = {
	.owner 		= THIS_MODULE,
	.name 		= "rhostfs",
	.mount	 	= rhostfs_read_sb,
	.kill_sb	= rhostfs_kill_sb,
	.fs_flags 	= 0,
};
MODULE_ALIAS_FS("rhostfs");

static int init_rh_client(void)
{
	int rcode, i;
	dvs_usr_t *dvsu_ptr;
	dc_usr_t *dcu_ptr;
	proc_usr_t *proc_ptr;  
	
	RHDEBUG("\n",local_nodeid);	

	rcode = dvk_open();
	if( rcode < 0) ERROR_RETURN(rcode);
	
	dvsu_ptr = &dvs;

#ifdef CONFIG_DVKIPC
	int rhclient_nodeid;
	rhclient_nodeid = dvk_getdvsinfo(dvsu_ptr);
	if( rhclient_nodeid < 0) ERROR_RETURN(rhclient_nodeid);
	RHDEBUG("rhclient_nodeid=%d\n",rhclient_nodeid);	
	RHDEBUG( DVS_USR_FORMAT, DVS_USR_FIELDS(dvsu_ptr));	
#else  // CONFIG_DVKIPC
	local_nodeid = dvk_getdvsinfo(dvsu_ptr);
	if( local_nodeid < 0) ERROR_RETURN(local_nodeid);
	RHDEBUG("local_nodeid=%d\n",local_nodeid);	
	RHDEBUG( DVS_USR_FORMAT, DVS_USR_FIELDS(dvsu_ptr));	
#endif // CONFIG_DVKIPC
	
	dcu_ptr = &dcu;
	rcode = dvk_getdcinfo(dcid, dcu_ptr);
	if( rcode < 0) ERROR_RETURN(rcode);
	RHDEBUG( DC_USR1_FORMAT, DC_USR1_FIELDS(dcu_ptr));	
	RHDEBUG( DC_USR2_FORMAT, DC_USR2_FIELDS(dcu_ptr));	
	
	for( i = dcu_ptr->dc_nr_sysprocs - dcu_ptr->dc_nr_tasks;
		i < dcu_ptr->dc_nr_procs - dcu_ptr->dc_nr_tasks; i++){
		rhc_ep = dvk_tbind(dcid, i);
		if( rhc_ep == i ) break; 
	}
	if( i == dcu_ptr->dc_nr_procs) ERROR_RETURN(EDVSSLOTUSED);
	RHDEBUG("rhostfs bind rhc_ep=%d\n",rhc_ep);
	
	proc_ptr = &rhc_proc;
	rcode = dvk_getprocinfo(dcid, rhc_ep, proc_ptr);
	if(rcode < 0) ERROR_RETURN(rcode);
	RHDEBUG("rhostfs CLIENT: " PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));	
	
//	rhs_ep = RHOSTFS_PROC_NR; 
	proc_ptr = &rhs_proc;
	rcode = dvk_getprocinfo(dcid, rhs_ep, proc_ptr);
	if(rcode < 0) ERROR_RETURN(rcode);
	RHDEBUG( "rhostfs SERVER: " PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));	

	if( proc_ptr->p_rts_flags == SLOT_FREE) ERROR_RETURN(EDVSNOTBIND);
	return(OK);
}


static int __init init_rhostfs(void)
{
	int rcode;
	
	printk("Initilizing Remote DVS HOSTFS\n");
	rcode = init_rh_client();
	if(rcode < 0) ERROR_RETURN(rcode);
	RHDEBUG("Remote DVS HOSTFS server is bound on endpoint:%d\n", rhs_ep);
	rcode = register_filesystem(&rhostfs_type);
	RHDEBUG("register_filesystem rhostfs_type rcode:%d\n", rcode);
	return(rcode);
}

static void __exit exit_rhostfs(void)
{
	unregister_filesystem(&rhostfs_type);
}

module_init(init_rhostfs)
module_exit(exit_rhostfs)
MODULE_LICENSE("GPL");

#endif //  CONFIG_RHOSTFS
