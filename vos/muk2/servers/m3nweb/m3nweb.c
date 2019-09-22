/* 
		HTTP WEB SERVER using MOL-FS
		but LINUX SOCKETS
 */

#define MUKDBG		1
#define DVK_GLOBAL_HERE
#include "m3nweb.h"

#undef DVK_GLOBAL_HERE
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

struct {
  char *ext;
  char *filetype;
} extensions [] = {
  {"gif", "image/gif" },
  {"jpg", "image/jpg" },
  {"jpeg","image/jpeg"},
  {"png", "image/png" },
  {"ico", "image/ico" },
  {"zip", "image/zip" },
  {"gz",  "image/gz"  },
  {"tar", "image/tar" },
  {"htm", "text/html" },
  {"html","text/html" },
  {"mht", "text/mht" },
  {"txt","text/txt" },
  {0,0} };
  
const static char http_forbidden[] = "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n"; 
const static char http_not_found[] = "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n";
  
void nweb_init(char *cfg_file);
int nweb_server(int socket_fd);
int nw_search_config(config_t *cfg);
int nw_read_config(char *file_conf);

void nw_usage(char* errmsg, ...) {
	if(errmsg) {
		printf("ERROR: %s\n", errmsg);
	} 
	fprintf(stderr, "Usage: m3nweb <config_file>\n");
}

/*===========================================================================*
 *				nweb_init					     *
 *===========================================================================*/
 void nweb_init(char *cfg_file)
 {
 	int rcode;
    config_t *cfg;
	int  lcl_ep;
	
	MUKDEBUG(DVS_USR_FORMAT,DVS_USR_FIELDS(dvs_ptr));
	MUKDEBUG(DC_USR1_FORMAT,DC_USR1_FIELDS(dc_ptr));
	MUKDEBUG(DC_USR2_FORMAT,DC_USR2_FIELDS(dc_ptr));
	
	/*---------------- Allocate memory for struct mnx_stat  ---------------*/
	posix_memalign( (void **) &nw_fstat_ptr, getpagesize(), sizeof(struct mnx_stat) );
	if (nw_fstat_ptr== NULL) {
   		MUKDEBUG("posix_memalign nw_fstat_ptr\n");
		rcode = -errno;
		ERROR_TSK_EXIT(rcode);
	}

  	/*---------------- Allocate memory for output buffer  ---------------*/
	posix_memalign( (void **) &nw_out_buf, getpagesize(), WEBMAXBUF );
	if (nw_out_buf== NULL) {
   		MUKDEBUG("posix_memalign nw_out_buf\n");
		free(nw_fstat_ptr);
		rcode = -errno;
		ERROR_TSK_EXIT(rcode);
	}

  	/*---------------- Allocate memory for output buffer  ---------------*/
	posix_memalign( (void **) &nw_in_buf, getpagesize(), WEBMAXBUF );
	if (nw_in_buf== NULL) {
   		MUKDEBUG("posix_memalign nw_in_buf\n");
		free(nw_fstat_ptr);
		free(nw_out_buf);
		rcode = -errno;
		ERROR_TSK_EXIT(rcode);
	}
	
	cfg= nil;
	MUKDEBUG("cfg_file=%s\n", cfg_file);
	cfg = config_read(cfg_file, CFG_ESCAPED, cfg);
	
	MUKDEBUG("before nw_search_config\n");	
	rcode = nw_search_config(cfg);
	if(rcode) ERROR_TSK_EXIT(rcode);
	
	web_pid = syscall (SYS_gettid);
	MUKDEBUG("web_pid=%d\n", web_pid);
	
	lcl_ep = muk_tbind(dcid, web_ep);
	MUKDEBUG("web_ep=%d\n", web_ep);
	if( lcl_ep != web_ep) {
		ERROR_PRINT(EDVSENDPOINT);
		taskexit(&lcl_ep);
	}
	
	MUKDEBUG("Get web_ep info\n");
	web_ptr = (proc_usr_t *) get_task(web_ep);
	MUKDEBUG(PROC_MUK_FORMAT,PROC_MUK_FIELDS(web_ptr));
	
	/* Register into SYSTASK (as an autofork) */
	MUKDEBUG("Register NWEB into SYSTASK web_pid=%d\n",web_pid);
	lcl_ep = sys_bindproc(web_ep, fs_pid, LCL_BIND);
	if(lcl_ep < 0) ERROR_TSK_EXIT(lcl_ep);
	
	rcode = mol_bindproc(web_ep, web_ep, web_pid, LCL_BIND);
	MUKDEBUG("rcode=%d\n", rcode);
	if( rcode < 0) {
		ERROR_PRINT(rcode);
		taskexit(&web_ep);
	}
	
	// set the name of NWEB 
	rcode = sys_rsetpname(web_ep, "nweb", local_nodeid);
	if(rcode < 0) ERROR_TSK_EXIT(rcode);
		
	if((nw_listenfd = socket(AF_INET, SOCK_STREAM,0)) <0){
		rcode = -errno;
		ERROR_TSK_EXIT(rcode);
	}
	
	nw_svr_addr.sin_family = AF_INET;
	nw_svr_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	nw_svr_addr.sin_port = htons(web_port);
	
	rcode = bind(nw_listenfd, (struct sockaddr *)&nw_svr_addr,sizeof(nw_svr_addr));
	if (rcode <0){
		rcode = -errno;
		ERROR_TSK_EXIT(rcode);
	}
	rcode = listen(nw_listenfd,64);
	if ( rcode <0){
		rcode = -errno;
		ERROR_TSK_EXIT(rcode);
	}
	

}

/*===========================================================================*
 *				nweb_server					     *
 *===========================================================================*/
/* this is a child nw_web_child server process, so we can exit on errors */
int nweb_server(int socket_fd)
{
	u16_t in_len;
	int rcode;
	int j, file_fd;
	long i, ret, len, rbytes;
	double t_start, t_stop, t_total;
	long total_bytes;
	char *fstr;
  	FILE *log_fp;

	/* Register into SYSTASK (as an autofork) */
//	MUKDEBUG("Register NW into SYSTASK web_pid=%d\n",web_pid);
//	nw_ep = sys_bindproc(NW_PROC_NR, web_pid, LOCAL_BIND);
//	if(nw_ep < 0) ERROR_TSK_EXIT(nw_ep);
	
	// set the name of FS 
//	rcode = sys_rsetpname(nw_ep, "nweb", local_nodeid);
//	if(rcode < 0) ERROR_TSK_EXIT(rcode);
	
	MUKDEBUG("request socket_fd=%d\n", socket_fd);
	total_bytes = 0;

	/* Read the data from the port, blocking if nothing yet there. 
	We assume the request (the part we care about) is in one netbuf */
	t_start = dwalltime();
	rcode = OK;
	while(rcode == OK) {
			
		ret =read(socket_fd, nw_in_buf, WEBMAXBUF);
		
		in_len=strlen(nw_in_buf);
		/* remove CF and LF characters */
		for(i=0;i<in_len;i++)  
			if(nw_in_buf[i] == '\r' || nw_in_buf[i] == '\n')
				nw_in_buf[i]='*';
		  
		MUKDEBUG("HTTP request:%s\n",nw_in_buf);
	  
		if( strncmp(nw_in_buf,"GET ",4) 
		 && strncmp(nw_in_buf,"get ",4) ) {
			ERROR_PRINT(EDVSNOSYS);
			write(socket_fd,http_forbidden, strlen(http_forbidden));
			ERROR_RETURN(EDVSNOSYS);
			break;
		}  
		
		 /* null terminate after the second space to ignore extra stuff */
		for(i=4;i<in_len;i++) {
			if(nw_in_buf[i] == ' ') { /* string is "GET URL " +lots of other stuff */
				nw_in_buf[i] = 0;
				break;
			}
		}
		
		/* check for illegal parent directory use .. */
		for(j=0;j<i-1;j++) {  
			if(nw_in_buf[j] == '.' && nw_in_buf[j+1] == '.') {
				ERROR_PRINT(EDVSACCES);
				write(socket_fd,http_forbidden, strlen(http_forbidden));
				ERROR_RETURN(EDVSACCES);
				break;
			}
		}
		
		/* convert no filename to index file */
		if( !strncmp(&nw_in_buf[0],"GET /\0",6) 
		 || !strncmp(&nw_in_buf[0],"get /\0",6) ) 
			(void)strcpy(nw_in_buf,"GET /index.html");
		
		
		/* work out the file type and check we support it */
		in_len=strlen(nw_in_buf);
		fstr = (char *)0;
		for(i=0;extensions[i].ext != 0;i++) {
			len = strlen(extensions[i].ext);
			if( !strncmp(&nw_in_buf[in_len-len], extensions[i].ext, len)) {
			fstr =extensions[i].filetype;
			break;
			}
		}
		if(fstr == 0) {
			ERROR_PRINT(EDVSACCES);
			write(socket_fd,http_forbidden, strlen(http_forbidden));
			ERROR_RETURN(EDVSACCES);
			break;
		}
		
		/* open the file for reading */
		MUKDEBUG("filename:%s\n",&nw_in_buf[5]);
		if(( file_fd = mol_open(&nw_in_buf[5],O_RDONLY)) == -1) {  
			ERROR_PRINT(-errno);
			write(socket_fd,http_not_found, strlen(http_not_found));
			ERROR_RETURN(rcode);
			break;
		}
		
		/* get file size */
		rcode = mol_fstat(file_fd, nw_fstat_ptr);
		if( rcode < 0)  {
			ERROR_PRINT(-errno);
			write(socket_fd,http_not_found, strlen(http_not_found));
			ERROR_RETURN(rcode);
		}
		
		/* Send the HTML header */
		(void)sprintf(nw_out_buf,"HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", 
			VERSION, nw_fstat_ptr->mnx_st_size, fstr); /* Header + a blank line */
		write(socket_fd,nw_out_buf, strlen(nw_out_buf));
		total_bytes += strlen(nw_out_buf);
		/* send file in WEBMAXBUF block - last block may be smaller */
		while (  (rbytes = mol_read(file_fd, nw_out_buf, WEBMAXBUF)) > 0 ) {
			MUKDEBUG("rbytes:%d\n",rbytes);
			write(socket_fd,nw_out_buf, rbytes);
			total_bytes += rbytes;
			MUKDEBUG("total_bytes:%d\n",total_bytes);
		}
		break;
	}
	
	MUKDEBUG("total_bytes=%ld\n", total_bytes);

	ret = mol_close(file_fd); 
	if(ret < 0)	ERROR_PRINT(-errno);

	t_stop = dwalltime();
	t_total = (t_stop-t_start);
	log_fp = fopen("m3nweb.log","a+");
	fprintf(log_fp, "t_start=%.2f t_stop=%.2f t_total=%.2f\n",t_start, t_stop, t_total);
	fprintf(log_fp, "total_bytes = %ld\n", total_bytes);
	fprintf(log_fp, "Throuhput = %f [bytes/s]\n", (double)(total_bytes)/t_total);
	fclose(log_fp);

	return(OK);
}

void be_a_daemon(void)
{
	int i;
	
	MUKDEBUG("\n");

//	if(getppid()==1) return; /* already a daemon */
//	i=fork();
//	if (i<0) exit(1); /* fork error */
//	if (i>0) exit(0); /* parent exits */
	
	MUKDEBUG("pid=%d\n", getpid());

	/* child (daemon) continues */
//	setsid(); /* obtain a new process group */
//	for (i=getdtablesize();i>=0;--i) close(i); /* close all descriptors */
//	i=open("/dev/null",O_RDWR); dup(i); dup(i); /* handle standart I/O */
//	umask(027); /* set newly created file permissions */
//	chdir(RUNNING_DIR); /* change running directory */
//	lfp=open(LOCK_FILE,O_RDWR|O_CREAT,0640);
//	if (lfp<0) exit(1); /* can not open */
//	if (lockf(lfp,F_TLOCK,0)<0) exit(0); /* can not lock */
	/* first instance continues */
//	sprintf(str,"%d\n",getpid());
//	write(lfp,str,strlen(str)); /* record pid to lockfile */
	signal(SIGCHLD,SIG_IGN); /* ignore child */
	signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
	signal(SIGHUP,SIG_IGN); /* catch hangup signal */
	signal(SIGTERM,SIG_IGN); /* catch kill signal */
}	

/*===========================================================================*
 *				   main_nweb 				     *
 *===========================================================================*/
int main_nweb (int argc, char *argv[] )
{
	int length, pid, rcode, sts;
	int web_sfd;
	pthread_t my_pth;
	int web_pid, lcl_ep;
	
	if ( argc != 2) {
		nw_usage( "%s <config_file>", argv[0]);
		ERROR_TSK_EXIT(rcode);
	}
	
//	be_a_daemon();
		
	nweb_init(argv[1]);

#ifdef ANULADO 	
	my_pth = pthread_self();
	MUKDEBUG("my_pth=%u\n", my_pth);
	
	web_pid = syscall (SYS_gettid);
	MUKDEBUG("web_pid=%d\n", web_pid);
	
	lcl_ep = muk_tbind(dcid, web_ep);
	MUKDEBUG("web_ep=%d\n", web_ep);
	if( lcl_ep != web_ep) {
		ERROR_PRINT(EDVSENDPOINT);
		taskexit(&lcl_ep);
	}

	MUKDEBUG("Get web_ep info\n");
	nw_ptr = (proc_usr_t *) get_task(web_ep);
	MUKDEBUG(PROC_MUK_FORMAT,PROC_MUK_FIELDS(nw_ptr));

#endif // ANULADO 	
	
	
	MUKDEBUG("SYNCHRONIZE WITH MUK \n");
	// SYNCHRONIZE WITH MUK
	MTX_LOCK(muk_mutex);
	COND_SIGNAL(muk_cond);
	COND_WAIT(nw_cond, muk_mutex);
	MTX_UNLOCK(muk_mutex);

	MUKDEBUG("Starting Web server main loop\n");
	for(web_hit=1; ;web_hit++) {
		length = sizeof(nw_cli_addr);
		MUKDEBUG("Conection accept\n");
		web_sfd = accept(nw_listenfd, (struct sockaddr *)&nw_cli_addr, &length);
		if (web_sfd < 0){
			rcode = -errno;
			ERROR_TSK_EXIT(rcode);
		}
		rcode = nweb_server(web_sfd);
		if(rcode < 0)
			ERROR_PRINT(rcode);
		close(web_sfd);
	}

	return(OK);				
}