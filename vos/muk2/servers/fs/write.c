/* This file is the counterpart of "read.c".  It contains the code for writing
 * insofar as this is not contained in read_write().
 *
 * The entry points into this file are
 *   fs_write:     call read_write to perform the WRITE system call
 *   clear_zone:   erase a zone in the middle of a file
 *   new_block:    acquire a new block
 */

// #define MUKDBG    1

#include "fs.h"


/*===========================================================================*
 *				fs_write				     *
 *===========================================================================*/
int fs_write()
{
/* Perform the write(fd, buffer, nbytes) system call. */

  return(read_write(WRITING));
}

/*===========================================================================*
 *				write_map				     *
 *===========================================================================*/
int write_map(struct inode *rip, mnx_off_t position, mnx_zone_t new_zone, int op)
// struct inode *rip;		/* pointer to inode to be changed */
// off_t position;			 file address to be mapped 
// zone_t new_zone;		/* zone # to be inserted */
// int op;				/* special actions */
{
/* Write a new zone into an inode.
 *
 * If op includes WMAP_FREE, free the data zone corresponding to that position
 * in the inode ('new_zone' is ignored then). Also free the indirect block
 * if that was the last entry in the indirect block.
 * Also free the double indirect block if that was the last entry in the
 * double indirect block.
 */
  int scale, ind_ex, new_ind, new_dbl, zones, nr_indirects, single, zindex, ex;
  mnx_zone_t z, z1, z2 = NO_ZONE, old_zone;
  mnx_block_t b;
  long excess, zone;
  struct buf *bp_dindir = NIL_BUF, *bp = NIL_BUF;

  rip->i_dirt = DIRTY;		/* inode will be changed */
  scale = rip->i_sp->s_log_zone_size;		/* for zone-block conversion */
  	/* relative zone # to insert */
  zone = (position/rip->i_sp->s_block_size) >> scale;
  zones = rip->i_ndzones;	/* # direct zones in the inode */
  nr_indirects = rip->i_nindirs;/* # indirect zones per indirect block */

  /* Is 'position' to be found in the inode itself? */
  if (zone < zones) {
	zindex = (int) zone;	/* we need an integer here */
	if(rip->i_zone[zindex] != NO_ZONE && (op & WMAP_FREE)) {
		free_zone(rip->i_dev, rip->i_zone[zindex]);
		rip->i_zone[zindex] = NO_ZONE;
	} else {
		rip->i_zone[zindex] = new_zone;
	}
	return(OK);
  }

  /* It is not in the inode, so it must be single or double indirect. */
  excess = zone - zones;	/* first Vx_NR_DZONES don't count */
  new_ind = FALSE;
  new_dbl = FALSE;

  if (excess < nr_indirects) {
	/* 'position' can be located via the single indirect block. */
	z1 = rip->i_zone[zones];	/* single indirect zone */
	single = TRUE;
  } else {
	/* 'position' can be located via the double indirect block. */
	if ( (z2 = z = rip->i_zone[zones+1]) == NO_ZONE &&
	    !(op & WMAP_FREE)) {
		/* Create the double indirect block. */
		if ( (z = alloc_zone(rip->i_dev, rip->i_zone[0])) == NO_ZONE)
			return(fs_err_code);
		rip->i_zone[zones+1] = z;
		new_dbl = TRUE;	/* set flag for later */
	}

	/* 'z' is zone number for double indirect block, either old
	 * or newly created.
	 * If there wasn't one and WMAP_FREE is set, 'z' is NO_ZONE.
	 */
	excess -= nr_indirects;	/* single indirect doesn't count */
	ind_ex = (int) (excess / nr_indirects);
	excess = excess % nr_indirects;
	if (ind_ex >= nr_indirects) return(EFBIG);

	if(z == NO_ZONE) {
		/* WMAP_FREE and no double indirect block - then no
		 * single indirect block either.
		 */
		z1 = NO_ZONE;
	} else {
		b = (mnx_block_t) z << scale;
		bp_dindir = get_block(rip->i_dev, b, (new_dbl?NO_READ:NORMAL));
		if (new_dbl) zero_block(bp_dindir);
		z1 = rd_indir(bp_dindir, ind_ex);
	}
	single = FALSE;
  }

  /* z1 is now single indirect zone, or NO_ZONE; 'excess' is index.
   * We have to create the indirect zone if it's NO_ZONE. Unless
   * we're freeing (WMAP_FREE).
   */
  if (z1 == NO_ZONE && !(op & WMAP_FREE)) {
	z1 = alloc_zone(rip->i_dev, rip->i_zone[0]);
	if (single)
		rip->i_zone[zones] = z1; /* update inode w. single indirect */
	else
		wr_indir(bp_dindir, ind_ex, z1);	/* update dbl indir */

	new_ind = TRUE;
	/* If double ind, it is dirty. */
	if (bp_dindir != NIL_BUF) bp_dindir->b_dirt = DIRTY;
	if (z1 == NO_ZONE) {
		/* Release dbl indirect blk. */
		put_block(bp_dindir, INDIRECT_BLOCK);
		return(fs_err_code);	/* couldn't create single ind */
	}
  }

  /* z1 is indirect block's zone number (unless it's NO_ZONE when we're
   * freeing).
   */
  if(z1 != NO_ZONE) {
  	ex = (int) excess;			/* we need an int here */
	b = (mnx_block_t) z1 << scale;
	bp = get_block(rip->i_dev, b, (new_ind ? NO_READ : NORMAL) );
	if (new_ind) zero_block(bp);
	if(op & WMAP_FREE) {
		if((old_zone = rd_indir(bp, ex)) != NO_ZONE) {
			free_zone(rip->i_dev, old_zone);
			wr_indir(bp, ex, NO_ZONE);
		}

		/* Last reference in the indirect block gone? Then
		 * Free the indirect block.
		 */
		if(empty_indir(bp, rip->i_sp)) {
			free_zone(rip->i_dev, z1);
			z1 = NO_ZONE;
			/* Update the reference to the indirect block to
			 * NO_ZONE - in the double indirect block if there
			 * is one, otherwise in the inode directly.
			 */
			if(single) {
				rip->i_zone[zones] = z1;
			} else {
				wr_indir(bp_dindir, ind_ex, z1);
				bp_dindir->b_dirt = DIRTY;
			}
		}
	} else {
		wr_indir(bp, ex, new_zone);
	}
	bp->b_dirt = DIRTY;
	put_block(bp, INDIRECT_BLOCK);
  }

  /* If the single indirect block isn't there (or was just freed),
   * see if we have to keep the double indirect block.
   */
  if(z1 == NO_ZONE && !single && empty_indir(bp_dindir, rip->i_sp) &&
     z2 != NO_ZONE) {
	free_zone(rip->i_dev, z2);
	rip->i_zone[zones+1] = NO_ZONE;
  }

  put_block(bp_dindir, INDIRECT_BLOCK);	/* release double indirect blk */

  return(OK);
}
/*===========================================================================*
 *				wr_indir				     *
 *===========================================================================*/
void wr_indir(struct buf *bp, int index, mnx_zone_t zone)
// struct buf *bp;			/* pointer to indirect block */
// int index;			/* index into *bp */
// zone_t zone;			/* zone to write */
{
/* Given a pointer to an indirect block, write one entry. */

  struct super_block *sp;

  if(bp == NIL_BUF)
	panic(__FILE__, "wr_indir() on NIL_BUF", NO_NUM);

  sp = get_super(bp->b_dev);	/* need super block to find file sys type */

  /* write a zone into an indirect block */
  if (sp->s_version == V1)
	bp->b_v1_ind[index] = (mnx_zone1_t) conv2(sp->s_native, (int)  zone);
  else
	bp->b_v2_ind[index] = (mnx_zone_t)  conv4(sp->s_native, (long) zone);
}

/*===========================================================================*
 *				empty_indir				     *
 *===========================================================================*/
int empty_indir(struct buf *bp, struct super_block *sb)
// struct buf *bp;			/* pointer to indirect block */
// struct super_block *sb;		/* superblock of device block resides on */
{
/* Return nonzero if the indirect block pointed to by bp contains
 * only NO_ZONE entries.
 */
	int i;
	if(sb->s_version == V1) {
		for(i = 0; i < V1_INDIRECTS; i++)
			if(bp->b_v1_ind[i] != NO_ZONE)
				return 0;
	} else {
		for(i = 0; i < V2_INDIRECTS(sb->s_block_size); i++)
			if(bp->b_v2_ind[i] != NO_ZONE)
				return 0;
	}

	return 1;
}

/*===========================================================================*
 *				clear_zone				     *
 *===========================================================================*/
void clear_zone(struct inode *rip, mnx_off_t pos, int flag)
//register struct inode *rip;	/* inode to clear */
//off_t pos;			/* points to block to clear */
//int flag;			/* 0 if called by read_write, 1 by new_block */
{
/* Zero a zone, possibly starting in the middle.  The parameter 'pos' gives
 * a byte in the first block to be zeroed.  Clearzone() is called from 
 * read_write and new_block().
 */

  struct buf *bp;
  mnx_block_t b, blo, bhi;
  mnx_off_t next;
  int scale;
  mnx_zone_t zone_size;

  /* If the block size and zone size are the same, clear_zone() not needed. */
  scale = rip->i_sp->s_log_zone_size;
  if (scale == 0) return;

  zone_size = (mnx_zone_t) rip->i_sp->s_block_size << scale;
  if (flag == 1) pos = (pos/zone_size) * zone_size;
  next = pos + rip->i_sp->s_block_size - 1;

  /* If 'pos' is in the last block of a zone, do not clear the zone. */
  if (next/zone_size != pos/zone_size) return;
  if ( (blo = read_map(rip, next)) == NO_BLOCK) return;
  bhi = (  ((blo>>scale)+1) << scale)   - 1;

  /* Clear all the blocks between 'blo' and 'bhi'. */
  for (b = blo; b <= bhi; b++) {
	bp = get_block(rip->i_dev, b, NO_READ);
	zero_block(bp);
	put_block(bp, FULL_DATA_BLOCK);
  }
}

/*===========================================================================*
 *				new_block				     *
 *===========================================================================*/
struct buf *new_block(struct inode *rip, mnx_off_t position)
//register struct inode *rip;	/* pointer to inode */
//off_t position;			/* file pointer */
{
/* Acquire a new block and return a pointer to it.  Doing so may require
 * allocating a complete zone, and then returning the initial block.
 * On the other hand, the current zone may still have some unused blocks.
 */

  struct buf *bp;
  mnx_block_t b, base_block;
  mnx_zone_t z;
  mnx_zone_t zone_size;
  int scale, r;
  struct super_block *sp;

  /* Is another block available in the current zone? */
  if ( (b = read_map(rip, position)) == NO_BLOCK) {
	/* Choose first zone if possible. */
	/* Lose if the file is nonempty but the first zone number is NO_ZONE
	 * corresponding to a zone full of zeros.  It would be better to
	 * search near the last real zone.
	 */
	if (rip->i_zone[0] == NO_ZONE) {
		sp = rip->i_sp;
		z = sp->s_firstdatazone;
	} else {
		z = rip->i_zone[0];	/* hunt near first zone */
	}
	if ( (z = alloc_zone(rip->i_dev, z)) == NO_ZONE) return(NIL_BUF);
	if ( (r = write_map(rip, position, z, 0)) != OK) {
		free_zone(rip->i_dev, z);
		fs_err_code = r;
		return(NIL_BUF);
	}

	/* If we are not writing at EOF, clear the zone, just to be safe. */
	if ( position != rip->i_size) clear_zone(rip, position, 1);
	scale = rip->i_sp->s_log_zone_size;
	base_block = (mnx_block_t) z << scale;
	zone_size = (mnx_zone_t) rip->i_sp->s_block_size << scale;
	b = base_block + (mnx_block_t)((position % zone_size)/rip->i_sp->s_block_size);
  }

  bp = get_block(rip->i_dev, b, NO_READ);
  zero_block(bp);
  return(bp);
}



/*===========================================================================*
 *				zero_block				     *
 *===========================================================================*/
void zero_block(struct buf *bp)
// register struct buf *bp;	/* pointer to buffer to zero */
{
/* Zero a block. */
  memset(bp->b_data, 0, _MAX_BLOCK_SIZE);
  bp->b_dirt = DIRTY;
}
