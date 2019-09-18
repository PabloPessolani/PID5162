/* Function prototypes. */

u64_t cvul64(unsigned long x32);

unsigned long cv64ul(u64_t i);

unsigned long div64u(u64_t i, unsigned j);

int dev_transfer(message *mp);
int dev_ready(message *mp);
int rd_ready(message *mp);

_PROTOTYPE( char *rd_name, (void) 				);
_PROTOTYPE( struct device *rd_prepare, (int device) 		);
_PROTOTYPE( int rd_transfer, (int proc_nr, int opcode, off_t position,
					iovec_t *iov, unsigned nr_req) 	);
_PROTOTYPE( int rd_do_open, (struct driver *dp, message *rd_mptr));
_PROTOTYPE( int rd_do_close, (struct driver *dp, message *rd_mptr));
_PROTOTYPE( int rd_init, (void) );
_PROTOTYPE( void rd_geometry, (struct partition *entry));
_PROTOTYPE( int rd_nop, (struct driver *dp, message *rd_mptr));
_PROTOTYPE( void parse_config, (char *f_conf));
_PROTOTYPE( int rd_do_dump,(struct driver *dp, message *rd_mptr));









