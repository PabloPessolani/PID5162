

#define RH_SYS_CALL	0x1000
#define RMT_opendir 	(RH_SYS_CALL +0) 
#define RMT_seekdir		(RH_SYS_CALL +1)
#define RMT_readdir		(RH_SYS_CALL +2)
#define RMT_closedir	(RH_SYS_CALL +3)
#define RMT_get_rootpath (RH_SYS_CALL +4)
#define RH_MAX_CALL		5

struct USR_dirent {
   ino_t          d_ino;       /* Inode number */
   off_t          d_off;       /* Not an offset; see below */
   unsigned short d_reclen;    /* Length of this record */
   unsigned char  d_type;      /* Type of file; not supported
								  by all filesystem types */
   char           d_name[256]; /* Null-terminated filename */
};

#define DIRE_FORMAT "d_ino=%ld d_off=%ld d_reclen=%d d_type=%X d_name=%s\n"
#define DIRE_FIELDS(p) p->d_ino, p->d_off, p->d_reclen, p->d_type, p->d_name

struct UML_dire_info {
	unsigned long 		ino_dire; 
	unsigned long 		pos_dire;
	int 				len_dire;
	unsigned int 		type_dire;
	char           		name_dire[256]; /* Null-terminated filename */
};

#define UDIRE_FORMAT "ino_dire=%ld pos_dire=%ld len_dire=%d type_dire=%X name_dire=%s\n"
#define UDIRE_FIELDS(p) p->ino_dire, p->pos_dire, p->len_dire, p->type_dire, p->name_dire

#define STAT_FORMAT "st_ino=%ld st_mode=%X st_nlink=%d st_uid=%d st_gid=%d\n"
#define STAT_FIELDS(p) p->st_ino, p->st_mode, p->st_nlink, p->st_uid, p->st_gid

#define STAT2_FORMAT "st_size=%ld st_blksize=%d st_blocks=%d\n"
#define STAT2_FIELDS(p) p->st_size, p->st_blksize, p->st_blocks

/* KERNEL stat struct 
struct stat {
	unsigned long  st_dev;
	unsigned long  st_ino;
	unsigned short st_mode;
	unsigned short st_nlink;
	unsigned short st_uid;
	unsigned short st_gid;
	unsigned long  st_rdev;
	unsigned long  st_size;
	unsigned long  st_blksize;
	unsigned long  st_blocks;
	unsigned long  st_atime;
	unsigned long  st_atime_nsec;
	unsigned long  st_mtime;
	unsigned long  st_mtime_nsec;
	unsigned long  st_ctime;
	unsigned long  st_ctime_nsec;
	unsigned long  __unused4;
	unsigned long  __unused5;
};
*/

struct USR_stat {
    unsigned long   ust_dev;     /* ID of device containing file */
    unsigned long   ust_ino;     /* inode number */
    unsigned long   ust_mode;    /* protection */
    unsigned int    ust_nlink;   /* number of hard links */
    unsigned int    ust_uid;     /* user ID of owner */
    unsigned int    ust_gid;     /* group ID of owner */
    unsigned int    ust_pad0; 
    unsigned long   ust_rdev;    /* device ID (if special file) */
    unsigned long   ust_size;    /* total size, in bytes */
	unsigned long 	ust_blksize; /* blocksize for file system I/O */
    unsigned long   ust_blocks;  /* number of 512B blocks allocated */
    long long    	ust_atime;   /* time of last access */
    long long    	ust_mtime;   /* time of last modification */
    long long    	ust_ctime;   /* time of last status change */
};

#define USTAT_FORMAT "ust_ino=%ld ust_mode=%X ust_nlink=%d ust_uid=%d ust_gid=%d\n"
#define USTAT_FIELDS(p) p->ust_ino, p->ust_mode, p->ust_nlink, p->ust_uid, p->ust_gid

#define USTAT2_FORMAT "ust_size=%ld ust_blksize=%d ust_blocks=%d\n"
#define USTAT2_FIELDS(p) p->ust_size, p->ust_blksize, p->ust_blocks



