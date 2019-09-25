/* EXTERN should be extern except for the table file */
#ifdef DVK_GLOBAL_HERE
#define EXTERN
#else
#define EXTERN extern
#endif

EXTERN fproc_t *fs_proc_table;		/* FS process table			*/


/* The parameters of the call are kept here. */
EXTERN message fs_m_in;		/* the input message itself */
EXTERN message fs_m_out;		/* the output message used for fs_reply */
EXTERN message *fs_m_ptr;		/* pointer to message */

EXTERN int fs_who_p, fs_who_e;	/* caller's proc number, endpoint */
EXTERN int fs_call_nr;		/* system call number */

EXTERN int fs_img_size;		/* testing image file size */
EXTERN char *fs_img_ptr;		/* pointer to the first byte of the ram disk image */

//EXTERN FILE *img_fd_disk;		/* file descriptor for disk image */
//EXTERN int fileDescDiskImage;	/* file descriptor for disk image */

//EXTERN unsigned *localbuff;		/* pointer to the first byte of the local buffer (=disk image)*/	
// EXTERN unsigned localbuff[_MAX_BLOCK_SIZE];


EXTERN dmap_t fs_dmap_tab[NR_DEVICES]; /* dev_nr -> major/task */
EXTERN int	fs_dmap_rev[NR_DEVICES];	/* major/task -> dev_nr */

EXTERN int	fs_cfg_dev_nr;			/* # of devices in config.file */
EXTERN unsigned int mandatory;

//Ver la diferencia entre el codigo original donde fproc es un arreglo de estructuras y aca es solo una estructura
EXTERN fproc_t *fs_proc;	/* pointer to caller's fproc struct */
EXTERN int super_user;		/* 1 if caller is super_user, else 0 */

EXTERN int fs_err_code;		/* temporary storage for error number */
//EXTERN char user_path[MNX_PATH_MAX];/* storage for user path name */
EXTERN char *user_path;
 
/*
Ver que pasa con esto porque originalmente es un Dev_t con mayusculas
que es un int y aca lo estoy definiendo con minusculas que es un short
 */

EXTERN mnx_dev_t root_dev;		/* device number of the root device */ 

struct super_block *sb_ptr; /*Super block pointer*/

/* The following variables are used for returning results to the caller. */
EXTERN int rdwt_err;		/* status of last disk i/o request */

EXTERN struct inode *rdahead_inode;  /* pointer to inode to read ahead */
EXTERN mnx_off_t rdaheadpos;             /* position to read ahead */

EXTERN int susp_count;              /* number of procs suspended on pipe */

EXTERN int fs_nr_locks;		/* number of locks currently in place */

EXTERN struct mnx_file_lock file_lock[NR_LOCKS];

EXTERN int reviving;		/* number of pipe processes to be revived */

extern _PROTOTYPE (int (*fs_call_vec[]), (void) ); /* sys call table */
extern char dot1[2];   /* dot1 (&dot1[0]) and dot2 (&dot2[0]) have a special */
extern char dot2[3];   /* meaning to search_dir: no access permission check. */

EXTERN muk_proc_t *fs_proc_ptr;
EXTERN priv_usr_t *fs_priv_ptr;


