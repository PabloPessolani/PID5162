#include "fs.h"

#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

extern int fs_img_size;		/* testing image file size */
extern char *fs_img_ptr; 
extern struct super_block *sb_ptr; /*Super block pointer*/
#define 	OK   0

/*===========================================================================*
 *				load_image				     *
 *===========================================================================*/
int load_image(char *img_name)
{
	int rcode, img_fd, bytes, blocks, total;
	struct stat img_stat;
	char *ptr;

// MUKDEBUG("image name=%s\n", img_name);


	/* get the image file size */
	rcode = stat(img_name,  &img_stat);
	if(rcode) ERROR_TSK_EXIT(errno);

// MUKDEBUG("image size=%d[bytes]\n", img_stat.mnx_st_size);
// MUKDEBUG("block size=%d[bytes]\n", img_stat.mnx_st_blksize);
	fs_img_size = img_stat.st_size;

	/* alloc dynamic memory for image file size */
// MUKDEBUG("Alloc dynamic memory for disk image file bytes=%d\n", fs_img_size);
	posix_memalign( (void**) &fs_img_ptr, getpagesize(), (fs_img_size+getpagesize()));
	if(fs_img_ptr == NULL) ERROR_TSK_EXIT(errno);

	/* Try to open the disk image */
	img_fd = open(img_name, O_RDONLY);
	if(img_fd < 0) ERROR_TSK_EXIT(errno);
// MUKDEBUG("FD de archivo imagen (Memoria - Disco RAM): %d \n", img_fd);

	/* dump the image file into the allocated memory */
	ptr = fs_img_ptr;
	blocks = 0;
	total = 0;
	while( (bytes = read(img_fd, ptr, img_stat.st_blksize)) > 0 ) {
		blocks++;
		total += bytes;
		ptr += bytes;
	}
// MUKDEBUG("blocks read=%d bytes read=%d\n", blocks, total);

	/* close the disk image */
// MUKDEBUG("Cerrando FD (Memoria - Disco RAM): %d \n", img_fd);
	rcode = close(img_fd);
	if(rcode) ERROR_TSK_EXIT(errno);

#define BLOCK_SIZE	1024

	sb_ptr = (struct super_block *) (fs_img_ptr + BLOCK_SIZE);

// MUKDEBUG(SUPER_BLOCK_FORMAT1, SUPER_BLOCK_FIELDS1(sb_ptr));

  return(OK);
}
