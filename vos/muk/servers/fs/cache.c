/* The file system maintains a buffer cache to reduce the number of disk
 * accesses needed.  Whenever a read or write to the disk is done, a check is
 * first made to see if the block is in the cache.  This file manages the
 * cache.
 *
 * The entry points into this file are:
 *   get_block:	  request to fetch a block for reading or writing from cache
 *   put_block:	  return a block previously requested with get_block
 *   alloc_zone:  allocate a new zone (to increase the length of a file)
 *   free_zone:	  release a zone (when a file is removed)
 *   invalidate:  remove all the cache blocks on some device
 *
 * Private functions:
 *   rw_block:    read or write a block from the disk itself
 */
#define MUKDBG    1
#include "fs.h"
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

/*===========================================================================*
 *				alloc_zone				     *
 *===========================================================================*/
 mnx_zone_t alloc_zone(mnx_dev_t dev, mnx_zone_t z)
// dev_t dev;			/* device where zone wanted */
// zone_t z;			/* try to allocate new zone near this one */
 {
/* Allocate a new zone on the indicated device and return its number. */

  int major, minor;
  mnx_bit_t b, bit;
  struct super_block *sp;

  /* Note that the routine alloc_bit() returns 1 for the lowest possible
   * zone, which corresponds to sp->s_firstdatazone.  To convert a value
   * between the bit number, 'b', used by alloc_bit() and the zone number, 'z',
   * stored in the inode, use the formula:
   *     z = b + sp->s_firstdatazone - 1
   * Alloc_bit() never returns 0, since this is used for NO_BIT (failure).
   */
   sp = get_super(dev);

  /* If z is 0, skip initial part of the map known to be fully in use. */
   if (z == sp->s_firstdatazone) {
     bit = sp->s_zsearch;
   } else {
     bit = (mnx_bit_t) z - (sp->s_firstdatazone - 1);
   }
   b = alloc_bit(sp, ZMAP, bit);
   if (b == NO_BIT) {
     fs_err_code = ENOSPC;
     major = (int) (sp->s_dev >> MNX_MAJOR) & BYTE;
     minor = (int) (sp->s_dev >> MNX_MINOR) & BYTE;
     printf("No space on %sdevice %d/%d\n",
      sp->s_dev == root_dev ? "root " : "", major, minor);
     return(NO_ZONE);
   }
  if (z == sp->s_firstdatazone) sp->s_zsearch = b;	/* for next time */
   return(sp->s_firstdatazone - 1 + (mnx_zone_t) b);
 }

/*===========================================================================*
 *				free_zone				     *
 *===========================================================================*/
 void free_zone(mnx_dev_t dev, mnx_zone_t numb)
// dev_t dev;				 device where zone located 
// zone_t numb;				/* zone to be returned */
 {
/* Return a zone. */

  struct super_block *sp;
  mnx_bit_t bit;

  /* Locate the appropriate super_block and return bit. */
  sp = get_super(dev);
  if (numb < sp->s_firstdatazone || numb >= sp->s_zones) return;
  bit = (mnx_bit_t) (numb - (sp->s_firstdatazone - 1));
  free_bit(sp, ZMAP, bit);
  if (bit < sp->s_zsearch) sp->s_zsearch = bit;
}

/*===========================================================================*
 *				rw_block				     *
 *===========================================================================*/
 int rw_block(struct buf *bp, int rw_flag)
//register struct buf *bp;	/* buffer pointer */
//int rw_flag;			/* READING or WRITING */
 {
/* Read or write a disk block. This is the only routine in which actual disk
 * I/O is invoked. If an error occurs, a message is printed here, but the error
 * is not reported to the caller.  If the error occurred while purging a block
 * from the cache, it is not clear what the caller could do about it anyway.
 */

 int r, op;
 mnx_off_t pos;
 mnx_dev_t dev;
 int block_size;
 //char *ptr;

   MUKDEBUG("\n");   

 block_size = get_block_size(bp->b_dev);
   
  MUKDEBUG("bp->b_blocknr= %d block_size=%d\n", bp->b_blocknr, block_size);   

 if ( (dev = bp->b_dev) != NO_DEV) {
 	pos = (mnx_off_t) bp->b_blocknr * block_size;
 	op = (rw_flag == READING ? DEV_READ : DEV_WRITE);
 	//ptr = pos + fs_img_ptr;

// MUKDEBUG("READING %d\n", READING);
// MUKDEBUG("WRITING %d\n", WRITING);
// MUKDEBUG("rw_flag %d\n", rw_flag);
// MUKDEBUG("op = %d (DEV_READ=%d o DEV_WRITE=%d) \n", op, DEV_READ, DEV_WRITE);	
MUKDEBUG(" dev_io -> op=%d, dev=%d, POS=%d, BYTES=%d\n",  op, dev, pos, block_size);   
	r = dev_io(op, dev, fs_ep, bp->b_data, pos, block_size, 0);
MUKDEBUG("dev_io r=%d, \n", r);
	//Para consistencia MOL en el archivo imagen
	// if (op == DEV_READ)
	// {
	// 	// MUKDEBUG("DEV_READ memcpy(&bp->b_data, ptr, block_size); \n");   
	// 	// MUKDEBUG("DEV_READ tamaño bloque= %d\n", block_size);   
	// 	memcpy(&bp->b_data, ptr, block_size);    
	// }
	// else
	// {
	// 	// MUKDEBUG("DEV_WRITE memcpy(&ptr, bp->b_data, block_size); \n");   
	// 	// MUKDEBUG("DEV_WRITE tamaño bloque= %d\n", block_size);   
	// 	memcpy(&ptr, bp->b_data, block_size);
	// }

// MUKDEBUG("block_size= %d\n", block_size); 
// MUKDEBUG("r= %d\n", r); 
  if (r != block_size) {
   if (r >= 0) r = END_OF_FILE;
   if (r != END_OF_FILE)
     printf("Unrecoverable disk error on device %d/%d, block %ld\n",
       (dev>>MNX_MAJOR)&BYTE, (dev>>MNX_MINOR)&BYTE, bp->b_blocknr);
		bp->b_dev = NO_DEV;	/* invalidate block */

		/* Report read errors to interested parties. */
   if (rw_flag == READING) rdwt_err = r;
 }
}

bp->b_dirt = CLEAN;
return OK;
}

/*===========================================================================*
 *				put_block				     *
 *===========================================================================*/
 void put_block(struct buf *bp, int block_type)
//register struct buf *bp;	/* pointer to the buffer to be released */
//int block_type;			/* INODE_BLOCK, DIRECTORY_BLOCK, or whatever */
 {
/* Return a block to the list of available blocks.   Depending on 'block_type'
 * it may be put on the front or rear of the LRU chain.  Blocks that are
 * expected to be needed again shortly (e.g., partially full data blocks)
 * go on the rear; blocks that are unlikely to be needed again shortly
 * (e.g., full data blocks) go on the front.  Blocks whose loss can hurt
 * the integrity of the file system (e.g., inode blocks) are written to
 * disk immediately if they are dirty.
 */

   MUKDEBUG("\n");


 if (bp == NIL_BUF) return;	/* it is easier to check here than in caller */

  bp->b_count--;		/* there is one use fewer now */
  if (bp->b_count != 0) return;	/* block is still in use */

  bufs_in_use--;		/* one fewer block buffers in use */
 // MUKDEBUG("bufs_in_use (%d)\n", bufs_in_use);

  /* Put this block back on the LRU chain.  If the ONE_SHOT bit is set in
   * 'block_type', the block is not likely to be needed again shortly, so put
   * it on the front of the LRU chain where it will be the first one to be
   * taken when a free buffer is needed later.
   */
   if (bp->b_dev == DEV_RAM || (block_type & ONE_SHOT)) {
    // MUKDEBUG("bp->b_dev (%d)\n", bp->b_dev);
    // MUKDEBUG("DEV_RAM (%d)\n", DEV_RAM);
    // MUKDEBUG("block_type (%d)\n", block_type);
    // MUKDEBUG("ONE_SHOT (%d)\n", ONE_SHOT);

	/* Block probably won't be needed quickly. Put it on front of chain.
  	 * It will be the next block to be evicted from the cache.
  	 */
    bp->b_prev = NIL_BUF;
    bp->b_next = front;
    if (front == NIL_BUF)
		  rear = bp;	/* LRU chain was empty */
     else
      front->b_prev = bp;
    front = bp;
  } else {
	/* Block probably will be needed quickly.  Put it on rear of chain.
  	 * It will not be evicted from the cache for a long time.
  	 */
    bp->b_prev = rear;
    bp->b_next = NIL_BUF;
    if (rear == NIL_BUF)
      front = bp;
    else
      rear->b_next = bp;
    rear = bp;
  }

  /* Some blocks are so important (e.g., inodes, indirect blocks) that they
   * should be written to the disk immediately to avoid messing up the file
   * system in the event of a crash.
   */
  // MUKDEBUG("rw_block(bp, WRITING); con block_type %d\n", block_type);
  // MUKDEBUG("WRITE_IMMED %d\n", WRITE_IMMED);
  // MUKDEBUG("bp->b_dirt %d\n", bp->b_dirt);
  // MUKDEBUG("DIRTY %d\n", DIRTY);
  // MUKDEBUG("bp->b_dev %d\n", bp->b_dev);
  // MUKDEBUG("NO_DEV %d\n", NO_DEV);  
   if ((block_type & WRITE_IMMED) && bp->b_dirt==DIRTY && bp->b_dev != NO_DEV) {
   //if (bp->b_dirt==DIRTY && bp->b_dev != NO_DEV) {
// MUKDEBUG("Llamando a escribir rw_block(bp, WRITING);");  
    rw_block(bp, WRITING);
  } 
}

/*===========================================================================*
 *				rw_scattered				     *
 *===========================================================================*/
 void rw_scattered(mnx_dev_t dev, struct buf **bufq, int bufqsize, int rw_flag)
// dev_t dev;			/* major-minor device number */
// struct buf **bufq;		 pointer to array of buffers 
// int bufqsize;			/* number of buffers */
// int rw_flag;			/* READING or WRITING */
 {
/* Read or write scattered data from a device. */

  register struct buf *bp;
  int gap;
  register int i;
  register iovec_t *iop;
  static iovec_t iovec[NR_IOREQS];  /* static so it isn't on stack */
  int j, r;
  int block_size;

  block_size = get_block_size(dev);

  /* (Shell) sort buffers on b_blocknr. */
  gap = 1;
  do
  gap = 3 * gap + 1;
  while (gap <= bufqsize);
  while (gap != 1) {
   gap /= 3;
   for (j = gap; j < bufqsize; j++) {
    for (i = j - gap;
     i >= 0 && bufq[i]->b_blocknr > bufq[i + gap]->b_blocknr;
     i -= gap) {
     bp = bufq[i];
   bufq[i] = bufq[i + gap];
   bufq[i + gap] = bp;
 }
}
}
// MUKDEBUG("NR_IOREQS %d\n", NR_IOREQS);
// MUKDEBUG("INICIO bufqsize %d\n", bufqsize);

  /* Set up I/O vector and do I/O.  The result of dev_io is OK if everything
   * went fine, otherwise the error code for the first failed transfer.
   */  
   while (bufqsize > 0) {
     for (j = 0, iop = iovec; j < NR_IOREQS && j < bufqsize; j++, iop++) {
      bp = bufq[j];
       // MUKDEBUG("j= %d\n", j);
       // MUKDEBUG("bp->b_blocknr= %d\n", bp->b_blocknr);
       // MUKDEBUG("bufq[%d]->b_blocknr= %d\n", j, bufq[j]->b_blocknr);
       // MUKDEBUG("bufq[%d]->b_blocknr + j =%d\n", j, bufq[j]->b_blocknr + j);
       // MUKDEBUG("bufq[%d]->b_data =%s\n", j, bufq[j]->b_data);
      if (bp->b_blocknr != bufq[0]->b_blocknr + j) break;
      //iop->iov_addr = (vir_bytes) bp->b_data;
      iop->iov_addr = bp->b_data;
      iop->iov_size = block_size;
    }

// MUKDEBUG("llamada a dev_io bufqsize %d\n", bufqsize);
MUKDEBUG(" dev_io -> op=%d, dev=%d, POS=%d, BYTES=%d\n",  rw_flag == WRITING ? DEV_SCATTER : DEV_GATHER, dev, (mnx_off_t) bufq[0]->b_blocknr * block_size, j);   
    r = dev_io(rw_flag == WRITING ? DEV_SCATTER : DEV_GATHER,
      dev, fs_ep, iovec,
      (mnx_off_t) bufq[0]->b_blocknr * block_size, j, 0);

	/* Harvest the results.  Dev_io reports the first error it may have
	 * encountered, but we only care if it's the first block that failed.
	 */
  MUKDEBUG("RESULT of dev_io r=%d, \n", r);
  for (i = 0, iop = iovec; i < j; i++, iop++) {
    bp = bufq[i];
    // MUKDEBUG("iop->iov_size =%d\n", iop->iov_size);
    if (iop->iov_size != 0) {
			/* Transfer failed. An error? Do we care? */
     if (r != OK && i == 0) {
      printf(
        "fs: I/O error on device %d/%d, block %lu\n",
        (dev>>MNX_MAJOR)&BYTE, (dev>>MNX_MINOR)&BYTE,
        bp->b_blocknr);
				bp->b_dev = NO_DEV;	/* invalidate block */
    }
    break;
  }
  if (rw_flag == READING) {
			bp->b_dev = dev;	/* validate block */
   put_block(bp, PARTIAL_DATA_BLOCK);
 } else {
   bp->b_dirt = CLEAN;
 }
}
bufq += i;
bufqsize -= i;
// MUKDEBUG("Iteracion bufqsize %d\n", bufqsize);
// MUKDEBUG("bufq %d\n", bufq);
// MUKDEBUG("i = %d\n", i);
if (rw_flag == READING) {
		/* Don't bother reading more than the device is willing to
		 * give at this time.  Don't forget to release those extras.
		 */
     while (bufqsize > 0) {
       put_block(*bufq++, PARTIAL_DATA_BLOCK);
       bufqsize--;
     }
   }
   if (rw_flag == WRITING && i == 0) {
		/* We're not making progress, this means we might keep
		 * looping. Buffers remain dirty if un-written. Buffers are
		 * lost if invalidate()d or LRU-removed while dirty. This
		 * is better than keeping unwritable blocks around forever..
		 */
     break;
   }
 }
}
/*===========================================================================*
 *				rm_lru					     *
 *===========================================================================*/
 void rm_lru(struct buf *bp)
//struct buf *bp;
 {
/* Remove a block from its LRU chain. */
  struct buf *next_ptr, *prev_ptr;

  bufs_in_use++;
// MUKDEBUG("bufs_in_use (%d)\n", bufs_in_use);
  // MUKDEBUG("ANTES  successor\n");
  next_ptr = bp->b_next;	/* successor on LRU chain */
  // MUKDEBUG("DESPUES  successor\n");
  // MUKDEBUG("ANTES  predecessor\n");
  prev_ptr = bp->b_prev;	/* predecessor on LRU chain */
  // MUKDEBUG("DESPUES  predecessor\n");
  if (prev_ptr != NIL_BUF)
  {
// MUKDEBUG("prev_ptr != NIL_BUF\n");
   prev_ptr->b_next = next_ptr;
  }
 else
  {
  // MUKDEBUG("prev_ptr = NIL_BUF\n");
   front = next_ptr;  /* this block was at front of chain */
}

  if (next_ptr != NIL_BUF)
  {
   // MUKDEBUG("next_ptr != NIL_BUF\n");
   next_ptr->b_prev = prev_ptr;
  }
 else
	{
    // MUKDEBUG("next_ptr = NIL_BUF\n");
  rear = prev_ptr;  /* this block was at rear of chain */
}
 
}

/*===========================================================================*
 *				get_block				     *
 *===========================================================================*/
 struct buf *get_block(mnx_dev_t dev, mnx_block_t block, int only_search)
// register dev_t dev;		/* on which device is the block? */
// register block_t block;		 /* which block is wanted? */
// int only_search;		/* if NO_READ, don't read, else act normal */
 {
/* Check to see if the requested block is in the block cache.  If so, return
 * a pointer to it.  If not, evict some other block and fetch it (unless
 * 'only_search' is 1).  All the blocks in the cache that are not in use
 * are linked together in a chain, with 'front' pointing to the least recently
 * used block and 'rear' to the most recently used block.  If 'only_search' is
 * 1, the block being requested will be overwritten in its entirety, so it is
 * only necessary to see if it is in the cache; if it is not, any free buffer
 * will do.  It is not necessary to actually read the block in from disk.
 * If 'only_search' is PREFETCH, the block need not be read from the disk,
 * and the device is not to be marked on the block, so callers can tell if
 * the block returned is valid.
 * In addition to the LRU chain, there is also a hash chain to link together
 * blocks whose block numbers end with the same bit strings, for fast lookup.
 */

 int b;
 struct buf *bp, *prev_ptr;
// MUKDEBUG("TEST \n");
// MUKDEBUG("NO_DEV %d\n", NO_DEV);
// MUKDEBUG("dev %d\n", dev);
// MUKDEBUG("dev= %d DEV_IMGRD=%d\n", dev, DEV_IMGRD);   
// MUKDEBUG("block= %d \n", block);   
  /* Search the hash chain for (dev, block). Do_read() can use 
   * get_block(NO_DEV ...) to get an unnamed block to fill with zeros when
   * someone wants to read from a hole in a file, in which case this search
   * is skipped
   */
  // MUKDEBUG("ANTES de if NO_DEV rm_lru()\n");
   if (dev != NO_DEV) {
     b = (int) block & HASH_MASK;
     bp = buf_hash[b];
     while (bp != NIL_BUF) {
      // MUKDEBUG("bp->b_blocknr (%d)\n", bp->b_blocknr);
      // MUKDEBUG("block (%d)\n", block);
      if (bp->b_blocknr == block && bp->b_dev == dev) {
			/* Block needed has been found. */
       if (bp->b_count == 0) rm_lru(bp);
			bp->b_count++;	/* record that block is in use */
      // MUKDEBUG("bp->b_count (%d)\n", bp->b_count);

       return(bp);
     } else {
			/* This block is not the one sought. */
			bp = bp->b_hash; /* move to next block on hash chain */
     }
   }
 }
// MUKDEBUG("DESPUES de if NO_DEV rm_lru(%d)\n", bp);

 // MUKDEBUG("bp->b_blocknr (%d)\n", bp->b_blocknr);
// MUKDEBUG("bp->b_dev (%d)\n", bp->b_dev);
// MUKDEBUG("front->b_blocknr (%d)\n", front->b_blocknr);
// MUKDEBUG("front->b_dev (%d)\n", front->b_dev);

  /* Desired block is not on available chain.  Take oldest block ('front'). */
 if ((bp = front) == NIL_BUF) panic(__FILE__,"all buffers in use", NR_BUFS);
 // MUKDEBUG("ANTES rm_lru (%d)\n", 1);
 rm_lru(bp);
 // MUKDEBUG("DESPES rm_lru (%d)\n", 1);
 
// MUKDEBUG("DESPUES de all buffers in use\n");
  /* Remove the block that was just taken from its hash chain. */
 b = (int) bp->b_blocknr & HASH_MASK;
 prev_ptr = buf_hash[b];
 if (prev_ptr == bp) {
   buf_hash[b] = bp->b_hash;
 } else {
	/* The block just taken is not on the front of its hash chain. */
   while (prev_ptr->b_hash != NIL_BUF)
    if (prev_ptr->b_hash == bp) {
			prev_ptr->b_hash = bp->b_hash;	/* found it */
     break;
   } else {
			prev_ptr = prev_ptr->b_hash;	/* keep looking */
   }
 }

  /* If the block taken is dirty, make it clean by writing it to the disk.
   * Avoid hysteresis by flushing all other dirty blocks for the same device.
   */
// MUKDEBUG("ANTES de flushall\n");  
// MUKDEBUG("bp->b_dirt %d\n", bp->b_dirt);
// MUKDEBUG("DIRTY %d\n", DIRTY);
// MUKDEBUG("bp->b_dev %d\n", bp->b_dev);
// MUKDEBUG("NO_DEV %d\n", NO_DEV);  
   if (bp->b_dev != NO_DEV) {
  	 //MUKDEBUG("llamar a flushall(bp->b_dev)\n");   
     if (bp->b_dirt == DIRTY) flushall(bp->b_dev);
#if ENABLE_CACHE2
     put_block2(bp);
#endif
   }
// MUKDEBUG("DESPUES de flushall\n");  
  /* Fill in block's parameters and add it to the hash chain where it goes. */
  bp->b_dev = dev;		/* fill in device number */
  bp->b_blocknr = block;	/* fill in block number */
  bp->b_count++;		/* record that block is being used */
   b = (int) bp->b_blocknr & HASH_MASK;
   bp->b_hash = buf_hash[b];
  buf_hash[b] = bp;		/* add to hash list */

  /* Go get the requested block unless searching or prefetching. */
   if (dev != NO_DEV) {
#if ENABLE_CACHE2
	if (get_block2(bp, only_search)) /* in 2nd level cache */;
     else
#endif
       if (only_search == PREFETCH) bp->b_dev = NO_DEV;
     else
       if (only_search == NORMAL) {
        // MUKDEBUG("rw_block(bp, READING); con bp->b_blocknr %d\n", bp->b_blocknr);
        rw_block(bp, READING);
      }
    }
  return(bp);			/* return the newly acquired block */
  }

/*===========================================================================*
 *				invalidate				     *
 *===========================================================================*/
 void invalidate(mnx_dev_t device)
//dev_t device;			/* device whose blocks are to be purged */
 {
/* Remove all the blocks belonging to some device from the cache. */

  register struct buf *bp;

  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++)
   if (bp->b_dev == device) bp->b_dev = NO_DEV;

#if ENABLE_CACHE2
 invalidate2(device);
#endif
}

/*===========================================================================*
 *				flushall				     *
 *===========================================================================*/
 void flushall(mnx_dev_t dev)
//dev_t dev;			/* device to flush */
 {
/* Flush all dirty blocks for one device. */

  register struct buf *bp;
  static struct buf *dirty[NR_BUFS];	/* static so it isn't on stack */
  int ndirty;

  // MUKDEBUG("EN FLUSHALL -----------!!!!!!!!!!!!!! dev %d\n", dev);
  for (bp = &buf[0], ndirty = 0; bp < &buf[NR_BUFS]; bp++)
  {  
  	if (bp->b_dirt == DIRTY && bp->b_dev == dev) 
  	{
		// MUKDEBUG("FLUSHALL--------> bp->b_data %s\n", bp->b_data);  		
  		dirty[ndirty++] = bp;
  	}
  }
  rw_scattered(dev, dirty, ndirty, WRITING);
}