
#ifndef _DVSCOM_TYPES_H
#define _DVSCOM_TYPES_H

#define 	DVK_CHAR_BITS	8
typedef unsigned long irq_id_t;	
typedef unsigned short bitchunk_t; /* collection of bits in a bitmap */
typedef short dvk_id_t;			/* dvk process index */
typedef unsigned long ksigset_t;
typedef long unsigned int update_t;

#define BITCHUNK_BITS   (sizeof(bitchunk_t) * DVK_CHAR_BITS)
#define BITMAP_CHUNKS(nr_bits) (((nr_bits)+BITCHUNK_BITS-1)/BITCHUNK_BITS)

typedef struct {			/* bitmap for sys indexes */
  bitchunk_t chunk[BITMAP_CHUNKS(NR_DVK_CALLS)];
} dvk_map_t;

typedef struct {			/* bitmap for sys process indexes */
  bitchunk_t chunk[BITMAP_CHUNKS(NR_SYS_PROCS)];
} ipc_map_t;

typedef struct {			/* bitmap for VM indexes */
  bitchunk_t chunk[BITMAP_CHUNKS(NR_DCS)];
} dc_map_t;

typedef struct {			/* bitmap for NODES indexes */
  bitchunk_t chunk[BITMAP_CHUNKS(NR_NODES)];
} node_map_t;

#endif // _DVSCOM_TYPES_H
