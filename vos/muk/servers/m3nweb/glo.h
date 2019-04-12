/* EXTERN should be extern except for the table file */
#ifdef DVK_GLOBAL_HERE
#define EXTERN
#else
#define EXTERN extern
#endif

EXTERN unsigned int nw_mandatory;
EXTERN int nw_listenfd;
EXTERN struct sockaddr_in nw_cli_addr; /* static = initialised to zeros */
EXTERN struct sockaddr_in nw_svr_addr; /* static = initialised to zeros */
EXTERN struct mnx_stat *nw_fstat_ptr;
EXTERN char *nw_in_buf;
EXTERN char *nw_out_buf;

EXTERN int web_port, web_hit;
EXTERN int web_ep;
EXTERN char *web_name;
EXTERN char *web_rootdir;		// root directory for this server

EXTERN pthread_t web_pth;

