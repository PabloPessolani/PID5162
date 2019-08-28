

#define RH_SYS_CALL	0x1000
#define RMT_opendir 	(RH_SYS_CALL +0) 
#define RMT_seekdir		(RH_SYS_CALL +1)
#define RMT_readdir		(RH_SYS_CALL +2)
#define RMT_closedir	(RH_SYS_CALL +3)
#define RMT_get_rootpath (RH_SYS_CALL +4)
#define RH_MAX_CALL		5

struct UML_dirent {
   ino_t          d_ino;       /* Inode number */
   off_t          d_off;       /* Not an offset; see below */
   unsigned short d_reclen;    /* Length of this record */
   unsigned char  d_type;      /* Type of file; not supported
								  by all filesystem types */
   char           d_name[256]; /* Null-terminated filename */
};