
long lcl_reply(int dest, int err, message *mptr)
{
	int rcode;
	mptr->m_type = err;
	rcode = dvk_send(dest, mptr, TIMEOUT_RMTCALL);
	if (rcode < 0) ERROR_RETURN(rcode);
	return(OK);
}

long lcl_gethostname(message *mptr)
{
	int rcode;
	int len; 
	char *hname[HOST_NAME_MAX+1]; 

	len = mptr->m1_i1;
	DVKDEBUG("len=%d\n", len);
	rcode = gethostname(hname, len);
	if( rcode < 0) ERROR_RETURN(-errno);
	DVKDEBUG("hname=%d\n", hname);
	rcode = dvk_vcopy(SELF, hname, m_ptr->m_source, m_ptr->m1_p1, strlen(hname)+1);
	if( rcode < 0) ERROR_RETURN(rcode);
	return(OK);
}

long lcl_open(message *mptr)
{
	int rcode;
	char *name[PATH_MAX+1]; 
	
	int len 	= mptr->m1_i1;
	int flags 	= mptr->m1_i2;
	int mode 	= mptr->m1_i3;
	DVKDEBUG("len=%d flags=%X mode=%X\n" , len, flags, mode);
	
	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p1, SELF, name, mptr->m1_i1);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("name=%d\n", name);

	rcode = rmt_open64(name, flags, mode);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);	
}

long lcl_access(message *mptr)
{
	int rcode;
	char *name[PATH_MAX+1]; 

	int len 	= mptr->m1_i1;
	int mode 	= mptr->m1_i2;
	DVKDEBUG("len=%d mode=%X\n" , len, mode);
	
	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p1, SELF, name, mptr->m1_i1);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("name=%d\n", name);

	rcode = access(name, mode);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode)
}


long lcl_pread64(message *mptr)
{
	int rcode;
	char *buf;
	long lpos[2];
	long long pos;
	
	int		fd 		= mptr->m2_i1;
	long 	count 	= mptr->m2_i2;
	lpos[0] 		= mptr->m2_l1;
	lpos[1] 		= mptr->m2_l2;
	ptr = (long long *) lpos;
	pos = *ptr;
	DVKDEBUG("fd=%d count=%ld pos=%lld\n" , fd, count, pos);

	buf = calloc( 1, count);
	if (buf == NULL) ERROR_RETURN(-errno);
	
	rcode = pread(fd, buf, count, pos);
	if( rcode < 0){
		rcode = -errno;
		goto free_buf;
	} 

	rcode = dvk_vcopy(SELF, buf, m_ptr->m_source, m_ptr->m2_p1, count);
	if( rcode < 0) goto free_buf;
	
free_buf:	
	free(buf);
	if( rcode < 0) ERROR_RETURN(rcode)
	return(rcode);
}

long lcl_pwrite64(message *mptr)
{
	int rcode;
	char *buf;
	long lpos[2];
	long long pos;
	
	int		fd 		= mptr->m2_i1;
	long 	count 	= mptr->m2_i2;
	lpos[0] 		= mptr->m2_li;
	lpos[1] 		= mptr->m2_l2;
	ptr = (long long *) lpos;
	pos = *ptr;
	DVKDEBUG("fd=%d count=%ld pos=%lld\n" , fd, count, pos);

	buf = calloc( 1, count);
	if (buf == NULL) ERROR_RETURN(-errno);

	rcode = dvk_vcopy( m_ptr->m_source, m_ptr->m2_p1, SELF, buf,  count);
	if( rcode < 0) goto free_buf;
	if(rcode != count) USRDEBUG("WARNING: (rcode=%d) != (count=%d)\n",rcode, count);
	if(rcode < count) count = rcode;
	
	rcode = pwrite(fd, buf, count, pos);
	if( rcode < 0){
		rcode = -errno;
		goto free_buf;
	} 
	
free_buf:	
	free(buf);
	if( rcode < 0) ERROR_RETURN(rcode)
	return(rcode);
}


long lcl_llseek(message *mptr)
{
	int rcode;
	long result; 

	int fd = mptr->m2_i1;
	int whence = mptr->m2_i2;
	long offset_high = mptr->m2_l1;
	long offset_low = mptr->m2_l2;
	DVKDEBUG("fd=%d high=%ld low=%ld whence=%d\n" , fd, offset_high, offset_low, whence);
	
	rcode = _llseek(fd, offset_high, offset_low, &result, whence);
	if( rcode < 0) ERROR_RETURN(-errno);
	
	mptr->m2_l3 = result;
	return(rcode);
}

long lcl_fsync(message *mptr)
{
	int rcode;

	int fd = mptr->m1_i1;
	DVKDEBUG("fd=%d\n" , fd);
	rcode = fsync(fd);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

long lcl_dup2(message *mptr)
{
	int rcode;

	int oldfd = mptr->m1_i1;
	int newfd = mptr->m1_i2;
	DVKDEBUG("oldfd=%d newfd=%d\n" , oldfd, newfd);

	rcode = dup2( oldfd, newfd);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);	
}

long lcl_fchmod(message *mptr)
{
	int rcode;

	int fd 	= mptr->m1_i1;
	int mode= mptr->m1_i2;	
	DVKDEBUG("fd=%d mode=%X\n" , fd, mode);

	rcode = fchmod(fd, mode);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);	
}

long lcl_chmod(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1]; 
	
	int len 	= mptr->m1_i1;
	int mode 	= mptr->m1_i2;
	DVKDEBUG("len=%d mode=%X\n" , len, mode);
	
	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p1, SELF, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("pathname=%d\n", pathname);

	rcode = chmod(pathname, mode);	
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

long lcl_fchown(message *mptr)
{
	int rcode;

	int fd 	= mptr->m1_i1;
	int user= mptr->m1_i2;
	int group= mptr->m1_i3;
	DVKDEBUG("fd=%d user=%d group=%d\n" , fd, user, group);

	rcode = fchown(fd, owner, group);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);	
}

long lcl_chown(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1]; 
	
	len = mptr->m1_i1;
	user = mptr->m1_i2;
	group = mptr->m1_i3;
	DVKDEBUG("len=%d user=%d group=%d\n" , len, user, group);

	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p1, SELF, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("pathname=%d\n", pathname);	
	
	rcode = chown(pathname, owner, group);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

long lcl_ftruncate(message *mptr)
{
	int rcode;

	int fd 		= mptr->m1_i1;
	int length 	= mptr->m1_l1;
	DVKDEBUG("fd=%d length=%uld\n", fd, length);

	rcode = ftruncate(fd, length);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode)	
}

long lcl_truncate(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1];
	
	int len 	= mptr->m1_i1;
	long length = mptr->m1_l1;
	DVKDEBUG("len=%d length=%uld\n", len, length);

	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p1, SELF, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("pathname=%d\n", pathname);	
	
	rcode =  truncate(pathname, length);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

long lcl_fdatasync(message *mptr)
{
	int rcode;

	int fd 	= mptr->m1_i1;
	DVKDEBUG("fd=%d\n", fd);

	rcode = fdatasync(fd);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode)	
}


long lcl_close(message *mptr)
{
	int rcode;

	int fd 	= mptr->m1_i1;
	DVKDEBUG("fd=%d\n", fd);

	rcode = close(fd);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode)	
}

long lcl_symlink(message *mptr)
{
	int rcode;
	char oldname[PATH_MAX+1];
	char newname[PATH_MAX+1];

	int olen = mptr->m1_i1;
	int nlen = mptr->m1_i2;
	
	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p1, SELF, oldname, olen);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("oldname=%d\n", oldname);

	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p2, SELF, newname, nlen);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("newname=%d\n", newname);

	rcode = symlink(oldname, newname);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);	
}

long lcl_unlink(message *mptr)
{
	int rcode;
	char *name[PATH_MAX+1]; 
	
	int len 	= mptr->m1_i1;
	DVKDEBUG("len=%d\n", len);
	
	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p1, SELF, name, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("name=%d\n", name);

	rcode = unlink(name);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

long lcl_link(message *mptr)
{
	int rcode;
	char oldname[PATH_MAX+1];
	char newname[PATH_MAX+1];

	int olen = mptr->m1_i1;
	int nlen = mptr->m1_i2;
	
	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p1, SELF, oldname, olen);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("oldname=%d\n", oldname);

	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p2, SELF, newname, nlen);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("newname=%d\n", newname);

	rcode = link(oldname, newname);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);	
}


long lcl_readlink(message *mptr)
{
	int rcode;
	char *filename[PATH_MAX+1];

	int len = mptr->m1_i1;
	int bufsiz = mptr->m1_i2;
	DVKDEBUG("len=%D bufsiz=%d\n", len, bufsiz);

	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p1, SELF, filename, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("filename=%d\n", filename);
		
	buf = calloc( 1, bufsiz);
	if (buf == NULL) ERROR_RETURN(-errno);
	
	rcode = readlink(filename, buf, bufsiz);
	if( rcode < 0){
		rcode = -errno;
		goto free_buf;
	} 

	rcode = dvk_vcopy(SELF, buf, m_ptr->m_source, m_ptr->m1_p2, count);
	if( rcode < 0) goto free_buf;
	
free_buf:	
	free(buf);
	if( rcode < 0) ERROR_RETURN(rcode)
	return(rcode);
}

long lcl_rename(message *mptr)
{
	int rcode;
	char oldname[PATH_MAX+1];
	char newname[PATH_MAX+1];

	int olen = mptr->m1_i1;
	int nlen = mptr->m1_i2;
	
	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p1, SELF, oldname, olen);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("oldname=%d\n", oldname);

	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p1, SELF, newname, nlen);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("newname=%d\n", newname);

	rcode = rename(oldname, newname);
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);	
}

long lcl_rmdir(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1]; 
	
	int len 	= mptr->m1_i1;
	DVKDEBUG("len=%d\n", len);
	
	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p1, SELF, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("pathname=%d\n", pathname);

	rcode = rmdir(pathname);	
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

long lcl_mkdir(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1]; 
	
	int len 	= mptr->m1_i1;
	int mode 	= mptr->m1_i2;
	DVKDEBUG("len=%d mode=%X\n" , len, mode);
	
	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p1, SELF, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("pathname=%d\n", pathname);

	rcode = mkdir(pathname, mode);	
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

long lcl_mknod(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1]; 
	
	int len 	= mptr->m1_i1;
	int mode 	= mptr->m1_i2;
	int dev 	= mptr->m1_i3;
	DVKDEBUG("len=%d mode=%X dev=%d\n" , len, mode, dev);
	
	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p1, SELF, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("pathname=%d\n", pathname);

	rcode = mknod(pathname, mode, dev);	
	if( rcode < 0) ERROR_RETURN(-errno);
	return(rcode);
}

long lcl_futimes(message *mptr)
{
	int rcode;
	const struct timeval tv[2];
	
	int fd 	= mptr->m1_i1;
	DVKDEBUG("fd=%d\n", fd);
	
	rcode = futimes(fd, tv);
	if( rcode < 0) ERROR_RETURN(-errno);
	
	rcode = dvk_vcopy(SELF, tv, m_ptr->m_source, m_ptr->m1_p1, sizeof(struct timeval));
	if( rcode < 0) ERROR_RETURN(rcode);

	return(OK);
}

long lcl_utimes(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1]; 
	const struct timeval tv[2];
	
	int len 	= mptr->m1_i1;
	DVKDEBUG("len=%d\n", len);
	
	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p1, SELF, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("pathname=%d\n", pathname);
	
	rcode = lutimes(pathname, tv);
	if( rcode < 0) ERROR_RETURN(-errno);
	
	rcode = dvk_vcopy(SELF, tv, m_ptr->m_source, m_ptr->m1_p2, sizeof(struct timeval));
	if( rcode < 0) ERROR_RETURN(rcode);

	return(OK);
}

long lcl_fstat64(message *mptr)
{
	int rcode;
	struct stat statbuf;
	
	int fd 	= mptr->m1_i1;
	DVKDEBUG("fd=%d\n", fd);
	
	rcode = fstat(fd, tv);
	if( rcode < 0) ERROR_RETURN(-errno);
	
	rcode = dvk_vcopy(SELF, &statbuf, m_ptr->m_source, m_ptr->m1_p1, sizeof(struct stat));
	if( rcode < 0) ERROR_RETURN(rcode);

	return(OK);
}

long lcl_statfs64(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1]; 
	struct statfs buf;
	
	int len 	= mptr->m1_i1;
	DVKDEBUG("len=%d\n", len);
	
	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p1, SELF, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("pathname=%d\n", pathname);
	
	rcode = statfs(pathname, &buf);
	if( rcode < 0) ERROR_RETURN(-errno);
	
	rcode = dvk_vcopy(SELF, &buf, m_ptr->m_source, m_ptr->m1_p2, sizeof(struct statfs));
	if( rcode < 0) ERROR_RETURN(rcode);

	return(OK);
}

long lcl_lstat64(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1]; 
	struct statfs buf;
	
	int len 	= mptr->m1_i1;
	DVKDEBUG("len=%d\n", len);
	
	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p1, SELF, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("pathname=%d\n", pathname);
	
	rcode = statfs(pathname, &buf);
	if( rcode < 0) ERROR_RETURN(-errno);
	
	rcode = dvk_vcopy(SELF, &buf, m_ptr->m_source, m_ptr->m1_p2, sizeof(struct statfs));
	if( rcode < 0) ERROR_RETURN(rcode);

	return(OK);
}

// typedef struct {int m7i1, m7i2, m7i3, m7i4; char *m7p1, *m7p2;} mess_7;
long lcl_renameat2(message *mptr)
{
	int rcode;
	char oldname[PATH_MAX+1];
	char newname[PATH_MAX+1];

	int olen = mptr->m7_i1;
	int nlen = mptr->m7_i2;
	int flags= mptr->m7_i4;
	int oldfd= (mptr->m7_i3) >> sizeof(short int);
	int newfd= (mptr->m7_i3) & ((1 <<  sizeof(short int))-1)
	DVKDEBUG(" olen=%d nlen=%d oldfd=%d newfd=%d flags=%X\n",
		olen, nlen, oldfd, newfd, flags);
		
	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m7_p1, SELF, oldname, olen);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("oldname=%d\n", oldname);

	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m7_p2, SELF, newname, nlen);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("newname=%d\n", newname);
	
	rcode = renameat2(oldfd, oldname, newfd, newname, flags);
	if( rcode < 0) ERROR_RETURN(-errno);				 
	return(rcode);
}
	
int lcl_opendir(message *mptr)
{
	int rcode;
	char pathname[PATH_MAX+1]; 
static	DIR dir, *dir_ptr; 
	
	int len 	= mptr->m1_i1;
	DVKDEBUG("len=%d\n", len);
	
	rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m1_p1, SELF, pathname, len);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG("pathname=%d\n", pathname);

	dir_ptr = opendir(pathname);
	if( dir_ptr == NULL) ERROR_RETURN(-errno);
	
	len = memcpy(&dir, dir_ptr, sizeof(DIR));
	DVKDEBUG("memcpy len=%d\n", len);
	
	rcode = dvk_vcopy(SELF, &dir, m_ptr->m_source, m_ptr->m1_p2, sizeof(DIR));
	if( rcode < 0) ERROR_RETURN(rcode);
	m_ptr->m1_p3 = dir_ptr; // must be returned for future operations on DIR
	
	return(rcode);
}


int lcl_seekdir(message *mptr)
{
	long loc = mptr->m1_l1;
	DIR *dirp= mptr->m1_p1;
	DVKDEBUG("loc=%ld\n", loc);

	seekdir(dirp, long loc);
	return(OK);
}

int lcl_readdir(DIR *dirp)
{
	message m;
	int rcode;
	static struct dirent dire, *dire_ptr;

	DVKDEBUG("\n");
	DIR *dirp = mptr->m1_p1;
	dire_ptr = readdir(&dire);
	if( dire_ptr == NULL) ERROR_RETURN(-errno);

	int len = memcpy(&dire, dir_ptr, sizeof(struct dirent));
	DVKDEBUG("memcpy len=%d\n", len);
	
	rcode = dvk_vcopy(SELF, &dire, m_ptr->m_source, m_ptr->m1_p2, sizeof(struct dirent));
	if( rcode < 0) ERROR_RETURN(rcode);
	m_ptr->m1_p3 = dire_ptr;
	return(rcode);
}

int lcl_closedir(message *mptr)
{
	DVKDEBUG("\n");
	DIR *dirp= mptr->m1_p1;
	closedir(dirp);
	if( rcode < 0) ERROR_RETURN(rcode);
	return(rcode);
}



		




























































































































				





















