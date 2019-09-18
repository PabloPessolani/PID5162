/* This file contains the procedures for creating, opening, closing, and
 * seeking on files.
 *
 * The entry points into this file are
 *   fs_creat:  perform the CREAT system call
 *   fs_open: perform the OPEN system call
 *   fs_mknod:  perform the MKNOD system call
 *   fs_mkdir:  perform the MKDIR system call
 *   fs_close:  perform the CLOSE system call
 *   fs_lseek:  perform the LSEEK system call
 */
#define MUKDBG    1

#include "fs.h"

#define offset m2_l1

char mode_map[] = {R_BIT, W_BIT, R_BIT | W_BIT, 0};
int common_open(int oflags, mnx_mode_t omode);

/*===========================================================================*
 *        fs_creat             *
 *===========================================================================*/
int fs_creat()
{
  /* Perform the creat(name, mode) system call. */
  int r;

   MUKDEBUG("\n");

  if (fetch_name(fs_m_in.name, fs_m_in.name_length, M3) != OK) return (fs_err_code);
  r = common_open(O_WRONLY | O_CREAT | O_TRUNC, (mnx_mode_t) fs_m_in.mode);
  return (r);
}
/*===========================================================================*
 *        fs_open              *
 *===========================================================================*/
int fs_open()
{
  /* Perform the open(name, flags,...) system call. */

  int create_mode = 0;    /* is really mode_t but this gives problems */
  int r;

// MUKDEBUG("fs_who_e=%d name=%p fs_ep=%d user_path=%s len=%d\n",
//  fs_who_e, fs_m_in.name1, fs_ep, user_path, fs_m_in.name1_length);

// MUKDEBUG("fs_who_e=%d name=%p fs_ep=%d user_path=%s len=%d\n",
//  fs_who_e, fs_m_in.name, fs_ep, user_path, fs_m_in.name_length);

// MUKDEBUG("fs_m_in.mode=%d\n", fs_m_in.mode);
// MUKDEBUG("fs_m_in.c_name=%s\n", fs_m_in.c_name);
// MUKDEBUG("fs_m_in.name=%s\n", fs_m_in.name);
// MUKDEBUG("O_CREAT=%d O_CREAT(OCT)=%#o\n", O_CREAT, O_CREAT);
  /* If O_CREAT is set, open has three parameters, otherwise two. */
  if (fs_m_in.mode & O_CREAT) {
    create_mode = fs_m_in.c_mode;
    MUKDEBUG("M1\n");
    r = fetch_name(fs_m_in.c_name, fs_m_in.name1_length, M1);
  }
  else {
//MUKDEBUG("flag=%s\n", flag);
    MUKDEBUG("M3\n");
// MUKDEBUG("fs_m_in.name=%s\n", fs_m_in.name);
// MUKDEBUG("fs_m_in.name_length=%d\n", fs_m_in.name_length);
    r = fetch_name(fs_m_in.name, fs_m_in.name_length, M3);
  }

// MUKDEBUG("fs_who_e=%d name=%p fs_ep=%d user_path=%s len=%d\n",
//   fs_who_e, fs_m_in.c_name, fs_ep, user_path, fs_m_in.name1_length);

// MUKDEBUG("create_mode=%d\n",create_mode);
  if (r != OK) ERROR_RETURN(fs_err_code); /* name was bad */
  r = common_open(fs_m_in.mode, create_mode);
  return (r);
}

/*===========================================================================*
 *        common_open            *
 *===========================================================================*/
int common_open(int oflags, mnx_mode_t omode)
{
  /* Common code from fs_creat and fs_open. */

  struct inode *rip, *ldirp;
  int r, b, exist = TRUE;
  mnx_dev_t dev;
  mnx_mode_t bits;
  mnx_off_t pos;
  struct filp *fil_ptr, *filp2;

  MUKDEBUG("oflags=%X omode=%X\n", oflags, omode);
  /* Remap the bottom two bits of oflags. */
  bits = (mnx_mode_t) mode_map[oflags & O_ACCMODE];
  /* See if file descriptor and filp slots are available. */
  if ( (r = get_fd(0, bits, &fs_m_in.fd, &fil_ptr)) != OK) ERROR_RETURN(r);

  MUKDEBUG("fs_m_in.fd=%d oflags=%X \n", fs_m_in.fd, oflags);

  /* If O_CREATE is set, try to make the file. */
  if (oflags & O_CREAT) {
    /* Create a new inode by calling new_node(). */
    MUKDEBUG("user_path=%s\n", user_path);
    omode = I_REGULAR | (omode & ALL_MODES & fs_proc->fp_umask);
    rip = new_node(&ldirp, user_path, omode, NO_ZONE, oflags & O_EXCL, NULL);
    r = fs_err_code;
    put_inode(ldirp);
    if (r == OK) exist = FALSE;      /* we just created the file */
    else if (r != EDVSEXIST) {ERROR_RETURN(r);} /* other error */
    else {exist = !(oflags & O_EXCL);} /* file exists, if the O_EXCL
              flag is set this is an error */
  } else {
    /* Scan path name. */
    MUKDEBUG("user_path=%s\n", user_path);
    if ( (rip = eat_path(user_path)) == NIL_INODE) {ERROR_RETURN(fs_err_code);}
    // MUKDEBUG("rip->i_zone[0] =%d \n", (mnx_dev_t) rip->i_zone[0]);
  }

// MUKDEBUG(INODE_FORMAT1, INODE_FIELDS1(rip));
  /* Claim the file descriptor and filp slot and fill them in. */
  fs_proc->fp_filp[fs_m_in.fd] = fil_ptr;
  FD_SET(fs_m_in.fd, &fs_proc->fp_filp_inuse);
  fil_ptr->filp_count = 1;
  fil_ptr->filp_ino = rip;
  fil_ptr->filp_flags = oflags;

  MUKDEBUG("FD: fs_m_in.fd=%d \n", fs_m_in.fd);
  MUKDEBUG(FS_FILP_FORMAT, FS_FILP_FIELDS(fil_ptr));

  MUKDEBUG("exist=%X \n", exist);

  /* Only do the normal open code if we didn't just create the file. */
  if (exist) {
    // /* Check protections. */
    if ((r = forbidden(rip, bits)) == OK) {
      MUKDEBUG("r forbidden = %d\n", r);
      /* Opening reg. files directories and special files differ. */
      switch (rip->i_mode & I_TYPE) {
      case I_REGULAR:
        /* Truncate regular file if O_TRUNC. */
        MUKDEBUG(" I_REGULAR\n");
        if (oflags & O_TRUNC) {
          if ((r = forbidden(rip, W_BIT)) != OK) {/*MUKDEBUG("r forbidden !=OK %d\n", r); */ break;}
          // MUKDEBUG("truncate_inode %d\n", 0);
          truncate_inode(rip, 0);
          // MUKDEBUG("wipe_inode %d\n", 0);
          wipe_inode(rip);
          /* Send the inode from the inode cache to the
           * block cache, so it gets written on the next
           * cache flush.
           */
		// MUKDEBUG("WRITING=%d \n", WRITING);
          rw_inode(rip, WRITING);
        }
        break;
      case I_DIRECTORY:
        // MUKDEBUG(" I_DIRECTORY dev %d\n", dev);
        /* Directories may be read but not written. */
        r = (bits & W_BIT ? EDVSISDIR : OK);
        MUKDEBUG("r=%d \n", r);
        break;
      case I_CHAR_SPECIAL:
      case I_BLOCK_SPECIAL:
        dev = (mnx_dev_t) rip->i_zone[0];
        MUKDEBUG(" I_CHAR_SPECIAL/I_BLOCK_SPECIAL dev %d\n", dev);
        /* Invoke the driver for special processing. */
        //dev = rip->i_dev;//TODO: Ver si este reemplazo es correcto!!!!!!
        r = dev_open(dev, fs_who_e, bits | (oflags & ~O_ACCMODE));
        MUKDEBUG("r=%d\n", r);
        break;
#ifdef ANULADO
      case I_NAMED_PIPE:
        oflags |= O_APPEND; /* force append mode */
        fil_ptr->filp_flags = oflags;
        r = pipe_open(rip, bits, oflags);
        if (r != ENXIO) {
          /* See if someone else is doing a rd or wt on
           * the FIFO.  If so, use its filp entry so the
           * file position will be automatically shared.
           */
          b = (bits & R_BIT ? R_BIT : W_BIT);
          fil_ptr->filp_count = 0; /* don't find self */
          if ((filp2 = find_filp(rip, b)) != NIL_FILP) {
            /* Co-reader or writer found. Use it.*/
            fs_proc->fp_filp[fs_m_in.fd] = filp2;
            filp2->filp_count++;
            filp2->filp_ino = rip;
            filp2->filp_flags = oflags;

            /* i_count was incremented incorrectly
             * by eatpath above, not knowing that
             * we were going to use an existing
             * filp entry.  Correct this error.
             */
            rip->i_count--;
          } else {
            /* Nobody else found.  Restore filp. */
            fil_ptr->filp_count = 1;
            if (b == R_BIT)
              pos = rip->i_zone[V2_NR_DZONES + 0];
            else
              pos = rip->i_zone[V2_NR_DZONES + 1];
            fil_ptr->filp_pos = pos;
          }
        }
        break;
#endif /* ANULADO */
      }
    }
  }

	/* If error, release inode. */
	if (r < OK) {
		if (r == SUSPEND) ERROR_RETURN(r);   /* Oops, just suspended */
		fs_proc->fp_filp[fs_m_in.fd] = NIL_FILP;
		FD_CLR(fs_m_in.fd, &fs_proc->fp_filp_inuse);
		fil_ptr->filp_count = 0;
		put_inode(rip);
		ERROR_RETURN(r);
	}
	MUKDEBUG("fs_m_in.fd=%d \n", fs_m_in.fd);
	return (fs_m_in.fd);
}

/*===========================================================================*
 *        new_node             *
 *===========================================================================*/
struct inode *new_node(struct inode **ldirp, char *path, mnx_mode_t bits, mnx_zone_t z0, int opaque, char *parsed)
{
  /* New_node() is called by common_open(), fs_mknod(), and fs_mkdir().
   * In all cases it allocates a new inode, makes a directory entry for it on
   * the path 'path', and initializes it.  It returns a pointer to the inode if
   * it can do this; otherwise it returns NIL_INODE.  It always sets 'fs_err_code'
   * to an appropriate value (OK or an error code).
   *
   * The parsed path rest is returned in 'parsed' if parsed is nonzero. It
   * has to hold at least NAME_MAX bytes.
   */

  register struct inode *rip;
  register int r;
  char string[NAME_MAX];

  MUKDEBUG("path=%s\n", path);


  *ldirp = parse_path(path, string, opaque ? LAST_DIR : LAST_DIR_EATSYM);
  if (*ldirp == NIL_INODE) return (NIL_INODE);

  /* The final directory is accessible. Get final component of the path. */
  rip = advance(ldirp, string);

  if (S_ISDIR(bits) &&
      (*ldirp)->i_nlinks >= ((*ldirp)->i_sp->s_version == V1 ?
                             CHAR_MAX : SHRT_MAX)) {
    /* New entry is a directory, alas we can't give it a ".." */
    put_inode(rip);
    fs_err_code = EMLINK;
    return (NIL_INODE);
  }

  if ( rip == NIL_INODE && fs_err_code == EDVSNOENT) {
    /* Last path component does not exist.  Make new directory entry. */
    if ( (rip = alloc_inode((*ldirp)->i_dev, bits)) == NIL_INODE) {
      /* Can't creat new inode: out of inodes. */
      return (NIL_INODE);
    }

    /* Force inode to the disk before making directory entry to make
     * the system more robust in the face of a crash: an inode with
     * no directory entry is much better than the opposite.
     */
    rip->i_nlinks++;
    rip->i_zone[0] = z0;    /* major/minor device numbers */
    rw_inode(rip, WRITING);   /* force inode to disk now */

    /* New inode acquired.  Try to make directory entry. */
    if ((r = search_dir(*ldirp, string, &rip->i_num, ENTER)) != OK) {
      rip->i_nlinks--;  /* pity, have to free disk inode */
      rip->i_dirt = DIRTY;  /* dirty inodes are written out */
      put_inode(rip); /* this call frees the inode */
      fs_err_code = r;
      return (NIL_INODE);
    }

  } else {
    /* Either last component exists, or there is some problem. */
    if (rip != NIL_INODE)
      r = EDVSEXIST;
    else
      r = fs_err_code;
  }

  if (parsed) { /* Give the caller the parsed string if requested. */
    strncpy(parsed, string, NAME_MAX - 1);
    parsed[NAME_MAX - 1] = '\0';
  }

  /* The caller has to return the directory inode (*ldirp).  */
  fs_err_code = r;
  return (rip);
}

/*===========================================================================*
 *        fs_mknod             *
 *===========================================================================*/
int fs_mknod()
{
  /* Perform the mknod(name, mode, addr) system call. */

  register mnx_mode_t bits, mode_bits;
  struct inode *ip, *ldirp;

   MUKDEBUG("\n");

  /* Only the super_user may make nodes other than fifos. */
  mode_bits = (mnx_mode_t) fs_m_in.mk_mode;    /* mode of the inode */
  if (!super_user && ((mode_bits & I_TYPE) != I_NAMED_PIPE)) return (EDVSPERM);
  if (fetch_name(fs_m_in.name1, fs_m_in.name1_length, M1) != OK) return (fs_err_code);
  bits = (mode_bits & I_TYPE) | (mode_bits & ALL_MODES & fs_proc->fp_umask);
  ip = new_node(&ldirp, user_path, bits, (mnx_zone_t) fs_m_in.mk_z0, TRUE, NULL);

  MUKDEBUG(INODE_FORMAT1, INODE_FIELDS1(ip));
  MUKDEBUG(INODE_FORMAT1, INODE_FIELDS1(ldirp));

  put_inode(ip);
  put_inode(ldirp);
  return (fs_err_code);
}

/*===========================================================================*
 *        fs_mkdir             *
 *===========================================================================*/
int fs_mkdir()
{
  /* Perform the mkdir(name, mode) system call. */

  int r1, r2;     /* status codes */
  mnx_ino_t dot, dotdot;    /* inode numbers for . and .. */
  mnx_mode_t bits;      /* mode bits for the new inode */
  char string[NAME_MAX];  /* last component of the new dir's path name */
  struct inode *rip, *ldirp;

   MUKDEBUG("\n");

  if (fetch_name(fs_m_in.name1, fs_m_in.name1_length, M1) != OK) return (fs_err_code);

  /* Next make the inode. If that fails, return error code. */
  bits = I_DIRECTORY | (fs_m_in.mode & RWX_MODES & fs_proc->fp_umask);
  rip = new_node(&ldirp, user_path, bits, (mnx_zone_t) 0, TRUE, string);
  if (rip == NIL_INODE || fs_err_code == EEXIST) {
    put_inode(rip);   /* can't make dir: it already exists */
    put_inode(ldirp);
    return (fs_err_code);
  }

  /* Get the inode numbers for . and .. to enter in the directory. */
  dotdot = ldirp->i_num;  /* parent's inode number */
  dot = rip->i_num;   /* inode number of the new dir itself */

  /* Now make dir entries for . and .. unless the disk is completely full. */
  /* Use dot1 and dot2, so the mode of the directory isn't important. */
  rip->i_mode = bits; /* set mode */
  r1 = search_dir(rip, dot1, &dot, ENTER);  /* enter . in the new dir */
  r2 = search_dir(rip, dot2, &dotdot, ENTER); /* enter .. in the new dir */

  /* If both . and .. were successfully entered, increment the link counts. */
  if (r1 == OK && r2 == OK) {
    /* Normal case.  It was possible to enter . and .. in the new dir. */
    rip->i_nlinks++;  /* this accounts for . */
    ldirp->i_nlinks++;  /* this accounts for .. */
    ldirp->i_dirt = DIRTY;  /* mark parent's inode as dirty */
  } else {
    /* It was not possible to enter . or .. probably disk was full -
     * links counts haven't been touched.
     */
    if (search_dir(ldirp, string, (mnx_ino_t *) 0, DELETE) != OK)
      panic(__FILE__, "Dir disappeared ", rip->i_num);
    rip->i_nlinks--;  /* undo the increment done in new_node() */
  }
  rip->i_dirt = DIRTY;    /* either way, i_nlinks has changed */

  put_inode(ldirp);   /* return the inode of the parent dir */
  put_inode(rip);   /* return the inode of the newly made dir */
  return (fs_err_code);  /* new_node() always sets 'fs_err_code' */
}


/*===========================================================================*
 *        fs_close             *
 *===========================================================================*/
int fs_close()
{
  /* Perform the close(fd) system call. */

  struct filp *rfilp;
  struct inode *rip;
  struct mnx_file_lock *flp;
  int rw, mode_word, lock_count;
  mnx_dev_t dev;

  MUKDEBUG("\n");

  /* First locate the inode that belongs to the file descriptor. */
  if ( (rfilp = get_filp(fs_m_in.fd)) == NIL_FILP) return (fs_err_code);
  rip = rfilp->filp_ino;  /* 'rip' points to the inode */

  if (rfilp->filp_count - 1 == 0 && rfilp->filp_mode != FILP_CLOSED) {
    /* Check to see if the file is special. */
    mode_word = rip->i_mode & I_TYPE;

// MUKDEBUG("ANTES DE mode_word %d\n", mode_word);
// MUKDEBUG("I_CHAR_SPECIAL %d\n", I_CHAR_SPECIAL);
// MUKDEBUG("I_BLOCK_SPECIAL %d\n", I_BLOCK_SPECIAL);
    if (mode_word == I_CHAR_SPECIAL || mode_word == I_BLOCK_SPECIAL) {
      dev = (mnx_dev_t) rip->i_zone[0];
      if (mode_word == I_BLOCK_SPECIAL)  {
        /* Invalidate cache entries unless special is mounted
         * or ROOT
         */
// MUKDEBUG("Llamada a fs_sync ------//////////////////////////////// ");
        //if (!mounted(rip)) {
        (void) fs_sync(); /* purge cache */
        //invalidate(dev);
        //}
      }

      /* Do any special processing on device close. */
      dev_close(dev);
    }
  }

#ifdef ANULADO
  /*ATENCION ver esto por error de compilacion en select.c y pippe.c dependiendo de int select_callback(struct filp *fs_proc, int ops)*/
  /* If the inode being closed is a pipe, release everyone hanging on it. */
  if (rip->i_pipe == I_PIPE) {
    rw = (rfilp->filp_mode & R_BIT ? MOLWRITE : MOLREAD);
    release(rip, rw, dc_ptr->dc_nr_procs);
  }

#endif /* ANULADO */

  /* If a write has been done, the inode is already marked as DIRTY. */
  if (--rfilp->filp_count == 0) {
    if (rip->i_pipe == I_PIPE && rip->i_count > 1) {
      /* Save the file position in the i-node in case needed later.
       * The read and write positions are saved separately.  The
       * last 3 zones in the i-node are not used for (named) pipes.
       */
      if (rfilp->filp_mode == R_BIT)
        rip->i_zone[V2_NR_DZONES + 0] = (mnx_zone_t) rfilp->filp_pos;
      else
        rip->i_zone[V2_NR_DZONES + 1] = (mnx_zone_t) rfilp->filp_pos;
    }
    put_inode(rip);
  }

  fs_proc->fp_cloexec &= ~(1L << fs_m_in.fd); /* turn off close-on-exec bit */
  fs_proc->fp_filp[fs_m_in.fd] = NIL_FILP;
  FD_CLR(fs_m_in.fd, &fs_proc->fp_filp_inuse);

  /* Check to see if the file is locked.  If so, release all locks. */
  if (fs_nr_locks == 0) return (OK);
  lock_count = fs_nr_locks;  /* save count of locks */
  for (flp = &file_lock[0]; flp < &file_lock[NR_LOCKS]; flp++) {
    if (flp->lock_type == 0) continue;  /* slot not in use */
    if (flp->lock_inode == rip && flp->lock_pid == fs_proc->fp_pid) {
      flp->lock_type = 0;
      fs_nr_locks--;
    }
  }
  //if (fs_nr_locks < lock_count) lock_revive(); /* lock released */
  return (OK);
}

/*===========================================================================*
 *        fs_lseek             *
 *===========================================================================*/
int fs_lseek()
{
  /* Perform the lseek(ls_fd, offset, whence) system call. */

  register struct filp *rfilp;
  register mnx_off_t pos;

   MUKDEBUG("\n");

  /* Check to see if the file descriptor is valid. */
  if ( (rfilp = get_filp(fs_m_in.ls_fd)) == NIL_FILP) return (fs_err_code);

  /* No lseek on pipes. */
  if (rfilp->filp_ino->i_pipe == I_PIPE) return (EDVSSPIPE);

  /* The value of 'whence' determines the start position to use. */
  switch (fs_m_in.whence) {
  case SEEK_SET: pos = 0; break;
  case SEEK_CUR: pos = rfilp->filp_pos; break;
  case SEEK_END: pos = rfilp->filp_ino->i_size; break;
  default: return (EDVSINVAL);
  }

  /* Check for overflow. */
  if (((long)fs_m_in.offset > 0) && ((long)(pos + fs_m_in.offset) < (long)pos))
    return (EDVSINVAL);
  if (((long)fs_m_in.offset < 0) && ((long)(pos + fs_m_in.offset) > (long)pos))
    return (EDVSINVAL);
  pos = pos + fs_m_in.offset;

  if (pos != rfilp->filp_pos)
    rfilp->filp_ino->i_seek = ISEEK;  /* inhibit read ahead */
  rfilp->filp_pos = pos;
  fs_m_out.reply_l1 = pos;   /* insert the long into the output message */
  return (OK);
}

/*===========================================================================*
 *                             fs_slink              *
 *===========================================================================*/
int fs_slink()
{
  /* Perform the symlink(name1, name2) system call. */

  register int r;              /* error code */
  char string[NAME_MAX];       /* last component of the new dir's path name */
  struct inode *sip;           /* inode containing symbolic link */
  struct buf *bp;              /* disk buffer for link */
  struct inode *ldirp;         /* directory containing link */

     MUKDEBUG("\n");

  if (fetch_name(fs_m_in.name2, fs_m_in.name2_length, M1) != OK)
    return (fs_err_code);

  if (fs_m_in.name1_length <= 1 || fs_m_in.name1_length >= _MIN_BLOCK_SIZE)
    return (EDVSNAMETOOLONG);

  /* Create the inode for the symlink. */
  sip = new_node(&ldirp, user_path, (mnx_mode_t) (I_SYMBOLIC_LINK | RWX_MODES),
                 (mnx_zone_t) 0, TRUE, string);

  /* Allocate a disk block for the contents of the symlink.
   * Copy contents of symlink (the name pointed to) into first disk block.
   */
  // if ((r = fs_err_code) == OK) {
  //      r = (bp = new_block(sip, (mnx_off_t) 0)) == NIL_BUF
  //          ? fs_err_code
  //          : sys_vircopy(fs_who_e, D, (vir_bytes) fs_m_in.name1,
  //                      SELF, D, (vir_bytes) bp->b_data,
  //          (vir_bytes) fs_m_in.name1_length-1);
  //
  if ((r = fs_err_code) == OK) {
	if( (bp = new_block(sip, (mnx_off_t) 0)) == NIL_BUF){
			r = fs_err_code;
	}else{
			MUK_vcopy( r, fs_who_e,  fs_m_in.name1,
				SELF, bp->b_data, fs_m_in.name1_length - 1);
	}
//    r = (bp = new_block(sip, (mnx_off_t) 0)) == NIL_BUF
//        ? fs_err_code
//        : muk_vcopy(fs_who_e,  fs_m_in.name1,
//                    SELF,  bp->b_data,
//                    fs_m_in.name1_length - 1);
					
	 if (r >= 0) r=OK;
     if (r == OK) {
      bp->b_data[_MIN_BLOCK_SIZE - 1] = '\0';
      sip->i_size = strlen(bp->b_data);
      if (sip->i_size != fs_m_in.name1_length - 1) {
        /* This can happen if the user provides a buffer
         * with a \0 in it. This can cause a lot of trouble
         * when the symlink is used later. We could just use
         * the strlen() value, but we want to let the user
         * know he did something wrong. ENAMETOOLONG doesn't
         * exactly describe the error, but there is no
         * ENAMETOOWRONG.
         */
        r = EDVSNAMETOOLONG;
      }
    }

    put_block(bp, DIRECTORY_BLOCK);  /* put_block() accepts NIL_BUF. */

    if (r != OK) {
      sip->i_nlinks = 0;
      if (search_dir(ldirp, string, (mnx_ino_t *) 0, DELETE) != OK)
        panic(__FILE__, "Symbolic link vanished", NO_NUM);
    }
  }

  /* put_inode() accepts NIL_INODE as a noop, so the below are safe. */
  put_inode(sip);
  put_inode(ldirp);

  return (r);
}