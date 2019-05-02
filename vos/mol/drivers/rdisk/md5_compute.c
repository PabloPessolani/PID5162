/*
 * Functions to compute MD5 message digest memory blocks
 * according to the definition of MD5 in RFC 1321 from April 1992.
 */
#define _MULTI_THREADED
#define _GNU_SOURCE     
#define  MOL_USERSPACE	1


// #define TASKDBG		1


#include "rdisk.h" 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "md5.h"
 
/*===========================================================================*
 *				md5_compute				     *
 *===========================================================================*/  
void md5_compute(img_fd, localbuff, buff_size, position, sigc)
int img_fd;
unsigned *localbuff;
unsigned	buff_size;
off_t position;
unsigned char	sigc[MD5_SIZE];
{
/* Main program of md5_compute. */
md5_t		md5;
int bytes;

md5_init(&md5);

/*read file of bakcup*/
TASKDEBUG("img_fd: %d, localbuff: %X, buff_size: %u, position: %u:\n", img_fd, localbuff, buff_size, position);				
if( (bytes = pread(img_fd, localbuff, buff_size, position)) < 0) ERROR_EXIT(errno);
TASKDEBUG("pread: %s\n", localbuff);
TASKDEBUG("bytes: %d\n", bytes);

				
if(bytes < 0) ERROR_EXIT(errno);

/* process our buffer buffer */
md5_process(&md5, localbuff, bytes);

/*get the result*/
md5_finish(&md5, sigc);
TASKDEBUG("sigc: %s\n", sigc); 
return;
}	