/* Function prototypes. */

u64_t cvul64(unsigned long x32);

unsigned long cv64ul(u64_t i);

unsigned long div64u(u64_t i, unsigned j);

int init_replicate(void);
void *replicate_main(void *arg);
int replica_loop(int *mtype, char *source);

int init_mastercopy(void);
void *mastercopy_main(void *arg);
int mastercopy_loop(int *mtype, char *source);
int dev_transfer(message *mp);
int copy_full(message *mp);
int mufull_copy(message *mp);		

int copy_md5(message *mp);
int dev_ready(message *mp);
int rd_ready(message *mp);


int init_slavecopy(void);
void *slavecopy_main(void *arg);
int slavecopy_loop(int *mtype, char *source);
int scopy_full(int stype);
int sufull_copy(int stype);
int sdiff_copy(int stype );

int sp_join( int new_mbr);
int sp_disconnect(int  disc_mbr);
int sp_net_partition(void);
int sp_net_merge(void);

// int rep_dev_scatter( message *sp_ptr);
int rep_dev_write(SP_message *sp_ptr);
int rep_task_reply( message *sp_ptr);
int rep_dev_open( message *sp_ptr);
int rep_dev_close( message *sp_ptr);
int rep_dev_ioctl( message *sp_ptr);
int rep_cancel( message *sp_ptr);
int rep_select( message *sp_ptr);

int mc_status_info( void);
int mc_synchronized( void );

int get_nodeid(char *grp_name, char *mbr_string);
int replica_updated(int localnodeid);
int send_synchronized(void);
int send_radar_info(void);



