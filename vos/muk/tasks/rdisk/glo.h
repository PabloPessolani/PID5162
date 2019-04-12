/* EXTERN should be extern except for the table file */
#ifdef DVK_GLOBAL_HERE
#define EXTERN
#else
#define EXTERN extern
#endif

/* The parameters of the call are kept here. */
EXTERN message rd_min;		/* the input message itself */
EXTERN message rd_mout;		/* the output message used for reply */
EXTERN message *rd_mptr;		/* pointer to message */

EXTERN int rd_who_p, rd_who_e;	/* caller's proc number, endpoint */
EXTERN int call_nr;		/* system call number */

EXTERN int img_size;		/* testing image file size */
EXTERN char *img_ptr;		/* pointer to the first byte of the ram disk image */

//EXTERN fproc_t *fproc;		/* FS process table			*/

EXTERN int rd_lpid;		
EXTERN int rd_ep;		
//EXTERN int fs_nr;	
EXTERN int img_p; /*pointer to the first byte of the ram disk image*/

EXTERN int	endp_flag;
EXTERN int minor_dev;
EXTERN int mayor_dev;
EXTERN int rd_dev_nr;
EXTERN long nr_optrans;

EXTERN proc_usr_t proc_rd, *rd_ptr;	

EXTERN int rd_rcode;		/* temporary storage for error number */
//EXTERN char user_path[PATH_MAX];/* storage for user path name */

/*
Ver que pasa con esto porque originalmente es un Dev_t con mayusculas
que es un int y aca lo estoy definiendo con minusculas que es un short
 */

EXTERN mnx_dev_t root_dev;		/* device number of the root device */ 

EXTERN char *img_name;		/* name of the ram disk image file*/
EXTERN int rdwt_err;		/* status of last disk i/o request */
EXTERN devvec_t devvec[NR_DEVS];
EXTERN sumdevvec_t sumdevvec[NR_DEVS];

extern _PROTOTYPE (int (*call_vec[]), (void) ); /* sys call table */


