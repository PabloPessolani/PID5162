#ifndef _MOL_CONFIG_H
#define _MOL_CONFIG_H

#define NR_BUFS         1200	/* # blocks in the buffer cache */
#define NR_BUF_HASH     2048	/* size of buf hash table; MUST BE POWER OF 2*/
	
/* Number of controller tasks (/dev/cN device classes). */
#define NR_CTRLRS          2

/* Enable or disable the second level file system cache on the RAM disk. */
#define ENABLE_CACHE2      0

/* Enable or disable swapping processes to disk. */
#define ENABLE_SWAP	   1

/* Include or exclude an image of /dev/boot in the boot image. 
 * Please update the makefile in /usr/src/tools/ as well.
 */
#define ENABLE_BOOTDEV	   0	/* load image of /dev/boot at boot time */

/* DMA_SECTORS may be increased to speed up DMA based drivers. */
#define DMA_SECTORS        1	/* DMA buffer size (must be >= 1) */

/* Include or exclude backwards compatibility code. */
#define ENABLE_BINCOMPAT   0	/* for binaries using obsolete calls */
#define ENABLE_SRCCOMPAT   0	/* for sources using obsolete calls */

/* Which processes should receive diagnostics from the kernel and system? 
 * Directly sending it to TTY only displays the output. Sending it to the
 * log driver will cause the diagnostics to be buffered and displayed.
 * Messages are sent by src/lib/sysutil/kputc.c to these processes, in
 * the order of this array, which must be terminated by NONE. This is used
 * by drivers and servers that printf().
 * The kernel does this for its own kprintf() in kernel/utility.c, also using
 * this array, but a slightly different mechanism.
 */
#define OUTPUT_PROCS_ARRAY	{ TTY_PROC_NR, LOG_PROC_NR, NONE }

/* NR_CONS, NR_RS_LINES, and NR_PTYS determine the number of terminals the
 * system can handle.
 */
#define NR_CONS        4	/* # system consoles (1 to 8) */
#define	NR_RS_LINES	   4	/* # rs232 terminals (0 to 4) */
#define	NR_PTYS		   32	/* # pseudo terminals (0 to 64) */
#define	NR_VTTYS	   8	/* # virtual terminals (0 to 7) */


#endif /* _MOL_CONFIG_H */
