/* This file manages the super block table and the related data structures,
 * namely, the bit maps that keep track of which zones and which inodes are
 * allocated and which are free.  When a new inode or zone is needed, the
 * appropriate bit map is searched for a free entry.
 *
 * The entry points into this file are
 *   alloc_bit:       somebody wants to allocate a zone or inode; find one
 *   free_bit:        indicate that a zone or inode is available for allocation
 *   get_super:       search the 'superblock' table for a device
 *   mounted:         tells if file inode is on mounted (or ROOT) file system
 *   read_super:      read a superblock
 */

// #define SVRDBG    1

#include "fs.h"

/*===========================================================================*
 *        get_block_size             *
 *===========================================================================*/
int get_block_size(mnx_dev_t dev)
{
/* Search the superblock table for this device. */

  struct super_block *sp;

  if (dev == NO_DEV)
    panic(__FILE__,"request for block size of NO_DEV", NO_NUM);

  for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++) {
  if (sp->s_dev == dev) {
    return(sp->s_block_size);
  }
  }

  /* no mounted filesystem? use this block size then. */
  return _MIN_BLOCK_SIZE;
}

/*===========================================================================*
 *        alloc_bit            *
 *===========================================================================*/
mnx_bit_t alloc_bit(struct super_block *sp, int map, mnx_bit_t origin)
// struct super_block *sp;   /* the filesystem to allocate from */
// int map;       IMAP (inode map) or ZMAP (zone map) 
// bit_t origin;     /* number of bit to start searching at */
{
/* Allocate a bit from a bit map and return its bit number. */

  mnx_block_t start_block;    /* first bit block */
  mnx_bit_t map_bits;   /* how many bits are there in the bit map? */
  unsigned bit_blocks;    /* how many blocks are there in the bit map? */
  unsigned block, word, bcount;
  struct buf *bp;
  mnx_bitchunk_t *wptr, *wlim, k;
  mnx_bit_t i, b;

  if (sp->s_rd_only)
  panic(__FILE__,"can't allocate bit on read-only filesys.", NO_NUM);

  if (map == IMAP) {
  start_block = START_BLOCK;
  map_bits = sp->s_ninodes + 1;
  bit_blocks = sp->s_imap_blocks;
  } else {
  start_block = START_BLOCK + sp->s_imap_blocks;
  map_bits = sp->s_zones - (sp->s_firstdatazone - 1);
  bit_blocks = sp->s_zmap_blocks;
  }

  /* Figure out where to start the bit search (depends on 'origin'). */
  if (origin >= map_bits) origin = 0; /* for robustness */

  /* Locate the starting place. */
  block = origin / FS_BITS_PER_BLOCK(sp->s_block_size);
  word = (origin % FS_BITS_PER_BLOCK(sp->s_block_size)) / FS_BITCHUNK_BITS;

  /* Iterate over all blocks plus one, because we start in the middle. */
  bcount = bit_blocks + 1;
  do {
  bp = get_block(sp->s_dev, start_block + block, NORMAL);
  wlim = &bp->b_bitmap[FS_BITMAP_CHUNKS(sp->s_block_size)];

  /* Iterate over the words in block. */
  for (wptr = &bp->b_bitmap[word]; wptr < wlim; wptr++) {

    /* Does this word contain a free bit? */
    if (*wptr == (bitchunk_t) ~0) continue;

    /* Find and allocate the free bit. */
    k = conv2(sp->s_native, (int) *wptr);
    for (i = 0; (k & (1 << i)) != 0; ++i) {}

    /* Bit number from the start of the bit map. */
    b = ((mnx_bit_t) block * FS_BITS_PER_BLOCK(sp->s_block_size))
        + (wptr - &bp->b_bitmap[0]) * FS_BITCHUNK_BITS
        + i;

    /* Don't allocate bits beyond the end of the map. */
    if (b >= map_bits) break;

    /* Allocate and return bit number. */
    k |= 1 << i;
    *wptr = conv2(sp->s_native, (int) k);
    bp->b_dirt = DIRTY;
    put_block(bp, MAP_BLOCK);
    return(b);
  }
  put_block(bp, MAP_BLOCK);
  if (++block >= bit_blocks) block = 0; /* last block, wrap around */
  word = 0;
  } while (--bcount > 0);
  return(NO_BIT);   /* no bit could be allocated */
}

/*===========================================================================*
 *        free_bit             *
 *===========================================================================*/
void free_bit(struct super_block *sp, int map, mnx_bit_t bit_returned)
// struct super_block *sp;   /* the filesystem to operate on */
// int map;       IMAP (inode map) or ZMAP (zone map) 
// bit_t bit_returned;   /* number of bit to insert into the map */
{
/* Return a zone or inode by turning off its bitmap bit. */

  unsigned block, word, bit;
  struct buf *bp;
  mnx_bitchunk_t k, mask;
  mnx_block_t start_block;

  if (sp->s_rd_only)
  panic(__FILE__,"can't free bit on read-only filesys.", NO_NUM);

  if (map == IMAP) {
  start_block = START_BLOCK;
  } else {
  start_block = START_BLOCK + sp->s_imap_blocks;
  }
  block = bit_returned / FS_BITS_PER_BLOCK(sp->s_block_size);
  word = (bit_returned % FS_BITS_PER_BLOCK(sp->s_block_size))
     / FS_BITCHUNK_BITS;

  bit = bit_returned % FS_BITCHUNK_BITS;
  mask = 1 << bit;

  bp = get_block(sp->s_dev, start_block + block, NORMAL);

  k = conv2(sp->s_native, (int) bp->b_bitmap[word]);
  if (!(k & mask)) {
  panic(__FILE__,map == IMAP ? "tried to free unused inode" :
        "tried to free unused block", bit_returned);
  }

  k &= ~mask;
  bp->b_bitmap[word] = conv2(sp->s_native, (int) k);
  bp->b_dirt = DIRTY;

  put_block(bp, MAP_BLOCK);
}


/*===========================================================================*
 *        get_super            *
 *===========================================================================*/
struct super_block *get_super(mnx_dev_t dev)
//mnx_dev_t dev;      /* device number whose super_block is sought */
{
/* Search the superblock table for this device.  It is supposed to be there. */

  struct super_block *sp;
// SVRDEBUG("TEST \n"); 
  if (dev == NO_DEV)
    panic(__FILE__,"request for super_block of NO_DEV", NO_NUM);

  for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++)
  {
//SVRDEBUG("TEST for superBlock \n");
//SVRDEBUG("TEST dev %d \n", (int) dev);
//SVRDEBUG(SUPER_BLOCK_FORMAT1, SUPER_BLOCK_FIELDS1(sp));
    if (sp->s_dev == dev) return(sp);    
  }


  /* Search failed.  Something wrong. */
  panic(__FILE__,"can't find superblock for device (in decimal)", (int) dev);

  return(NIL_SUPER);    /* to keep the compiler and lint quiet */
}

/*===========================================================================*
 *        read_super             *
 *===========================================================================*/
int read_super(struct super_block *sp)
{
/* Read a superblock. */
	  mnx_dev_t dev;
	  int magic;
	  int version, native, r;
	  static char sbbuf[_MIN_BLOCK_SIZE];
	  int sbb;
	  // int i;

	SVRDEBUG("Read the Superblock \n"); 

	dev = sp->s_dev;    /* save device (will be overwritten by copy) */

	if (dev == NO_DEV)
		ERROR_EXIT(errno);
	SVRDEBUG(" dev_io -> op=%d, dev=%d, POS=%d, BYTES=%d\n",  DEV_READ, dev, sbb, _MIN_BLOCK_SIZE);   
	r = dev_io(DEV_READ, dev, FS_PROC_NR, sbbuf, SUPER_BLOCK_BYTES, _MIN_BLOCK_SIZE, 0);
	SVRDEBUG("output dev_io r=%d, \n", r);
	//Ver como hacer esta validacion
	if (r != _MIN_BLOCK_SIZE) {
		SVRDEBUG("_MIN_BLOCK_SIZE r=%d, \n", r);
		return EMOLINVAL;
	}

	// SVRDEBUG("memcpy de sb_ptr a sbbuf, de un tamaño de _MIN_BLOCK_SIZE \n");  
	// memcpy(&sbbuf, sb_ptr, _MIN_BLOCK_SIZE); 

	memcpy(sp, sbbuf, sizeof(*sp));
	SVRDEBUG("_MIN_BLOCK_SIZE r=%d, \n", r);
	sp->s_dev = NO_DEV;   /* restore later */
	magic = sp->s_magic;    /* determines file system type */

	/* Get file system version and type. */
	if (magic == SUPER_MAGIC || magic == conv2(BYTE_SWAP, SUPER_MAGIC)) {
		version = V1;
		native  = (magic == SUPER_MAGIC);
		// SVRDEBUG("NO 1 %d\n", i);
		// break;
	} else if (magic == SUPER_V2 || magic == conv2(BYTE_SWAP, SUPER_V2)) {
		version = V2;
		native  = (magic == SUPER_V2);
		// SVRDEBUG("NO 2 %d\n", i);
		// break;
	} else if (magic == SUPER_V3) {
		version = V3;
		  native = 1;
		// SVRDEBUG("NO 3 %d\n", i);
		  // break;
	} else {
		// SVRDEBUG("SUPER_MAGIC \n"); 
		// SVRDEBUG("native %d\n", native);
		// SVRDEBUG("NO ENCONTRADO %d\n", i);
		ERROR_RETURN(EMOLINVAL);
	}
	// }

    /* If the super block has the wrong byte order, swap the fields; the magic
   * number doesn't need conversion. */
  sp->s_ninodes =       conv4(native, sp->s_ninodes);
  sp->s_nzones =        conv2(native, (int) sp->s_nzones);
  sp->s_imap_blocks =   conv2(native, (int) sp->s_imap_blocks);
  sp->s_zmap_blocks =   conv2(native, (int) sp->s_zmap_blocks);
  sp->s_firstdatazone = conv2(native, (int) sp->s_firstdatazone);
  sp->s_log_zone_size = conv2(native, (int) sp->s_log_zone_size);
  sp->s_max_size =      conv4(native, sp->s_max_size);
  sp->s_zones =         conv4(native, sp->s_zones);

// SVRDEBUG("Superblock fill 2 \n");  
// SVRDEBUG(SUPER_BLOCK_FORMAT1, SUPER_BLOCK_FIELDS1(sp));

    /* In V1, the device size was kept in a short, s_nzones, which limited
   * devices to 32K zones.  For V2, it was decided to keep the size as a
   * long.  However, just changing s_nzones to a long would not work, since
   * then the position of s_magic in the super block would not be the same
   * in V1 and V2 file systems, and there would be no way to tell whether
   * a newly mounted file system was V1 or V2.  The solution was to introduce
   * a new variable, s_zones, and copy the size there.
   *
   * Calculate some other numbers that depend on the version here too, to
   * hide some of the differences.
   */
	if (version == V1) {
		sp->s_block_size = _STATIC_BLOCK_SIZE;
		sp->s_zones = sp->s_nzones; /* only V1 needs this copy */
		sp->s_inodes_per_block = V1_INODES_PER_BLOCK;
		sp->s_ndzones = V1_NR_DZONES;
		sp->s_nindirs = V1_INDIRECTS;
	} else {
		if (version == V2)
			sp->s_block_size = _STATIC_BLOCK_SIZE;
		if (sp->s_block_size < _MIN_BLOCK_SIZE)
			return EMOLINVAL;
		sp->s_inodes_per_block = V2_INODES_PER_BLOCK(sp->s_block_size);
		sp->s_ndzones = V2_NR_DZONES;
		sp->s_nindirs = V2_INDIRECTS(sp->s_block_size);
	}

	if (sp->s_block_size < _MIN_BLOCK_SIZE) {
		return EMOLINVAL;
	}

	// SVRDEBUG("Superblock fill 3 \n");  
	// SVRDEBUG(SUPER_BLOCK_FORMAT1, SUPER_BLOCK_FIELDS1(sp));

	if (sp->s_block_size > _MAX_BLOCK_SIZE) {
		printf("Filesystem block size is %d kB; maximum filesystem\n"
		  "block size is %d kB. This limit can be increased by recompiling.\n",
		  sp->s_block_size/1024, _MAX_BLOCK_SIZE/1024);
		return EMOLINVAL;
	}

    if ((sp->s_block_size % 512) != 0) {
		// SVRDEBUG("sp->s_block_size mod 512 != 0\n");
		return EMOLINVAL;
	}
	if (SUPER_SIZE > sp->s_block_size) {
		// SVRDEBUG("SUPER_SIZE > sp->s_block_size \n");    
		return EMOLINVAL;
	}
	if ((sp->s_block_size % V2_INODE_SIZE) != 0 ||
		(sp->s_block_size % V1_INODE_SIZE) != 0) {
		// SVRDEBUG("V2_INODE_SIZE %lu\n", V2_INODE_SIZE); 
		// SVRDEBUG("V1_INODE_SIZE %lu\n", V1_INODE_SIZE);     
		return EMOLINVAL;
	}

	sp->s_isearch = 0;    /* inode searches initially start at 0 */
	sp->s_zsearch = 0;    /* zone searches initially start at 0 */
	sp->s_version = version;
	sp->s_native  = native;

	// SVRDEBUG("Superblock fill 4 \n");  
	// SVRDEBUG(SUPER_BLOCK_FORMAT1, SUPER_BLOCK_FIELDS1(sp));

	/* Make a few basic checks to see if super block looks reasonable. */
	if (sp->s_imap_blocks < 1 || sp->s_zmap_blocks < 1
		|| sp->s_ninodes < 1 || sp->s_zones < 1
		|| (unsigned) sp->s_log_zone_size > 4) {
		printf("not enough imap or zone map blocks, \n");
		printf("or not enough inodes, or not enough zones, "
			"or zone size too large\n");
		ERROR_RETURN(EMOLINVAL);
	}
	sp->s_dev = dev;    /* restore device number */
	// SVRDEBUG("SALEOK \n");  
	return(OK);
}
