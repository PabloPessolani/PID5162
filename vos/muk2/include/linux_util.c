
#define _MULTI_THREADED
#define _GNU_SOURCE     
#define  MOL_USERSPACE	1

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <setjmp.h>
#include <pthread.h>
#include <sched.h>
#include <getopt.h>
#include <fcntl.h>
#include <malloc.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/sysinfo.h> 
#include <sys/stat.h>
#include <sys/syscall.h> 
#include <sys/mman.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define DVS_USERSPACE	1
#define _GNU_SOURCE
#include <sched.h>
#define cpumask_t cpu_set_t


/*===========================================================================*
 *				load_image				     *
 *===========================================================================*/
int load_image(char *img_name)
{
	int rcode, img_fd, bytes, blocks, total;
	struct stat img_stat;
	char *ptr;

// SVRDEBUG("image name=%s\n", img_name);


	/* get the image file size */
	rcode = stat(img_name,  &img_stat);
	if(rcode) ERROR_EXIT(errno);

// SVRDEBUG("image size=%d[bytes]\n", img_stat.st_size);
// SVRDEBUG("block size=%d[bytes]\n", img_stat.st_blksize);
	img_size = img_stat.st_size;

	/* alloc dynamic memory for image file size */
// SVRDEBUG("Alloc dynamic memory for disk image file bytes=%d\n", img_size);
	posix_memalign( (void**) &img_ptr, getpagesize(), (img_size+getpagesize()));
	if(img_ptr == NULL) ERROR_EXIT(errno);

	/* Try to open the disk image */
	img_fd = open(img_name, O_RDONLY);
	if(img_fd < 0) ERROR_EXIT(errno);
// SVRDEBUG("FD de archivo imagen (Memoria - Disco RAM): %d \n", img_fd);

	/* dump the image file into the allocated memory */
	ptr = img_ptr;
	blocks = 0;
	total = 0;
	while( (bytes = read(img_fd, ptr, img_stat.st_blksize)) > 0 ) {
		blocks++;
		total += bytes;
		ptr += bytes;
	}
// SVRDEBUG("blocks read=%d bytes read=%d\n", blocks, total);

	/* close the disk image */
// SVRDEBUG("Cerrando FD (Memoria - Disco RAM): %d \n", img_fd);
	rcode = close(img_fd);
	if(rcode) ERROR_EXIT(errno);

#define BLOCK_SIZE	1024

	sb_ptr = (struct super_block *) (img_ptr + BLOCK_SIZE);

// SVRDEBUG(SUPER_BLOCK_FORMAT1, SUPER_BLOCK_FIELDS1(sb_ptr));

  return(OK);
}
