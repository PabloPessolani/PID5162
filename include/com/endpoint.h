
#ifndef _COM_ENDPOINT_H
#define _COM_ENDPOINT_H 1

/* The point of the padding in 'generation size' is to 
 * allow for certain bogus endpoint numbers such as NONE, ANY, etc.
 *
 * The _MAX_MAGIC_PROC is defined by <minix/com.h>. That include
 * file defines some magic process numbers such as ANY and NONE,
 * and must never be a valid endpoint number. Therefore we make sure
 * the generation size is big enough to start the next generation
 * above the highest magic number.
 */
#define _ENDPOINT_GENERATION_SIZE (NR_TASKS+_MAX_MAGIC_PROC+1)
#define _ENDPOINT_MAX_GENERATION  (INT_MAX/_ENDPOINT_GENERATION_SIZE-1)

/* Generation + Process slot number <-> endpoint. */
#define _ENDPOINT(g, p) ((g) * _ENDPOINT_GENERATION_SIZE + (p))
#define _ENDPOINT_G(e) (((e)+NR_TASKS) / _ENDPOINT_GENERATION_SIZE)
#define _ENDPOINT_P(e) ((((e)+NR_TASKS) % _ENDPOINT_GENERATION_SIZE) - NR_TASKS)

#endif //_COM_ENDPOINT_H
