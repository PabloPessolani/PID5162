/* Constants and macros for bit map manipulation. */
#define MAP_CHUNK(map,bit) (map)[((bit)/BITCHUNK_BITS)]
#define CHUNK_OFFSET(bit) ((bit)%BITCHUNK_BITS))
//#define GET_BIT(map,bit) ( MAP_CHUNK(map,bit) & (1 << CHUNK_OFFSET(bit) )
//#define SET_BIT(map,bit) ( MAP_CHUNK(map,bit) |= (1 << CHUNK_OFFSET(bit) )
//#define UNSET_BIT(map,bit) ( MAP_CHUNK(map,bit) &= ~(1 << CHUNK_OFFSET(bit) )

#define get_node_bit(x,y) 	get_sys_bit(x,y)
#define set_node_bit(x,y) 	set_sys_bit(x,y)
#define unset_node_bit(x,y) unset_sys_bit(x,y)

#define get_sys_bit(map,bit) \
	( MAP_CHUNK(map.chunk,bit) & (1 << CHUNK_OFFSET(bit) )
#define set_sys_bit(map,bit) \
	( MAP_CHUNK(map.chunk,bit) |= (1 << CHUNK_OFFSET(bit) )
#define unset_sys_bit(map,bit) \
	( MAP_CHUNK(map.chunk,bit) &= ~(1 << CHUNK_OFFSET(bit) )
#define NR_SYS_CHUNKS	BITMAP_CHUNKS(NR_SYS_PROCS)

#define PRINT_SYS_MAP(map) do {\
	int i;\
	DVKDEBUG(INTERNAL,"PRINT_SYS_MAP: %d:%s:%u:%s:",task_pid_nr(current), __FUNCTION__ ,__LINE__,#map);\
	DVKDEBUG(INTERNAL,"PRINT_SYS_MAP: %d:%s:%u:%d:",task_pid_nr(current), __FUNCTION__ ,__LINE__,NR_SYS_CHUNKS);\
	for(i = 0; i < NR_SYS_CHUNKS; i++){\
		DVKDEBUG(INTERNAL,"\n%X:",(map.chunk)[i]);\
	}\
	DVKDEBUG(INTERNAL,"\n");\
}while(0);

#define CLR_SYS_MAP(map) do {\
	int i;\
	for(i = 0; i < NR_SYS_CHUNKS; i++){\
		(map.chunk)[i]=0;\
	}\
}while(0);

#define SET_SYS_MAP(map) do {\
	int i;\
	for(i = 0; i < NR_SYS_CHUNKS; i++){\
		(map.chunk)[i]= ~0;\
	}\
}while(0);

#define NR_DVK_CHUNKS	BITMAP_CHUNKS(NR_DVK_CALLS)

#define PRINT_DVK_MAP(map) do {\
	int i;\
	DVKDEBUG(INTERNAL,"PRINT_DVKALLOWED_MAP: %d:%s:%u:%s:",task_pid_nr(current), __FUNCTION__ ,__LINE__,#map);\
	DVKDEBUG(INTERNAL,"PRINT_DVKALLOWED_MAP: %d:%s:%u:%d:",task_pid_nr(current), __FUNCTION__ ,__LINE__,NR_DVK_CHUNKS);\
	for(i = 0; i < NR_DVK_CHUNKS; i++){\
		DVKDEBUG(INTERNAL,"\n%X:",(map.chunk)[i]);\
	}\
	DVKDEBUG(INTERNAL,"\n");\
}while(0);

#define CLR_DVK_MAP(map) do {\
	int i;\
	for(i = 0; i < NR_DVK_CHUNKS; i++){\
		(map.chunk)[i]=0;\
	}\
}while(0);


#define SET_DVK_MAP(map) do {\
	int i;\
	for(i = 0; i < NR_DVK_CHUNKS; i++){\
		(map.chunk)[i]=~0;\
	}\
}while(0);



