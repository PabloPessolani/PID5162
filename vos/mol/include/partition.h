/*	minix/partition.h				Author: Kees J. Bot
 *								7 Dec 1995
 * Place of a partition on disk and the disk geometry,
 * for use with the DIOCGETP and DIOCSETP ioctl's.
 */
#ifndef _MINIX__PARTITION_H
#define _MINIX__PARTITION_H

#ifndef _TYPES_H
#include <sys/types.h>
#endif

struct part_entry {
  unsigned char bootind;	/* boot indicator 0/ACTIVE_FLAG	 */
  unsigned char start_head;	/* head value for first sector	 */
  unsigned char start_sec;	/* sector value + cyl bits for first sector */
  unsigned char start_cyl;	/* track value for first sector	 */
  unsigned char sysind;		/* system indicator		 */
  unsigned char last_head;	/* head value for last sector	 */
  unsigned char last_sec;	/* sector value + cyl bits for last sector */
  unsigned char last_cyl;	/* track value for last sector	 */
  unsigned long lowsec;		/* logical first sector		 */
  unsigned long size;		/* size of partition in sectors	 */
};

typedef unsigned long  mnx_u32_t;	   /* 32 bit type */
typedef struct { mnx_u32_t _[2]; } mnx_u64_t;

struct mnx_part_s {
  u64_t base;		/* byte offset to the partition start */
  u64_t size;		/* number of bytes in the partition */
  unsigned cylinders;	/* disk geometry */
  unsigned heads;
  unsigned sectors;
};
typedef struct mnx_part_s mnx_part_t;

#define PART_FORMAT 	"base[0]=%lX base[1]=%lX size[0]=%lX size[1]=%lX cyl=%d heads=%d sec=%d\n"
#define PART_FIELDS(p) p->base._[0],p->base._[0],p->size._[0],p->size._[1], p->cylinders, p->heads, p->sectors

#define ACTIVE_FLAG	0x80	/* value for active in bootind field (hd0) */
#define NR_PARTITIONS	4	/* number of entries in partition table */
#define	PART_TABLE_OFF	0x1BE	/* offset of partition table in boot sector */

/* Partition types. */
#define NO_PART		0x00	/* unused entry */
#define MINIX_PART	0x81	/* Minix partition type */


#endif /* _MINIX__PARTITION_H */
