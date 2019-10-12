/* System Information Service. 
 * This service handles the various debugging dumps, such as the process
 * table, so that these no longer directly touch kernel memory. Instead, the 
 * system task is asked to copy some table in local memory. 
 */
#define MUKDBG		1
#define DVK_GLOBAL_HERE
#include "is.h"

#undef DVK_GLOBAL_HERE
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

#define WIS_PORT		8080

/* Declare some local functions. */
void is_handler(int signo, siginfo_t *info, void *extra);
int wis_id;

/*===========================================================================*
 *				main_is                                         *
 *===========================================================================*/
int main_is(int argc, char **argv)
{
/* This is the main_is routine of this service. The main_is loop consists of 
 * three major activities: getting new work, processing the work, and
 * sending the is_reply. The loop never terminates, unless a panic occurs.
 */
	int rcode;                 

	if( argc != 1) {
		ERROR_PRINT(EDVSENDPOINT);
		taskexit(&is_ep);
	}
	
	MUKDEBUG("IS: Starting with endpoint is_ep=%d...\n", is_ep);
	/* Initialize the server, then go to work. */
	init_is();

  	// SYNCHRONIZE WITH MUK
	MTX_LOCK(muk_mutex);
	COND_SIGNAL(muk_cond);
	COND_WAIT(is_cond, muk_mutex);
	MTX_UNLOCK(muk_mutex);

	
	MUKDEBUG("IS(%d) main loop that gets work\n", dcu.dc_dcid);
  
  /* Main loop - get work and do it, forever. */  
	m_ptr = &msg;
	
	MTX_LOCK(wis_mutex);

	while (TRUE) {              
		COND_WAIT(wis_cond, wis_mutex);

		m_ptr->m1_p1 = wis_html;
		MUKDEBUG("dump_type:%X m1_p1=%p\n", dump_type, m_ptr->m1_p1);

#if	ENABLE_SYSTASK

		// Request INFO DUMP to SYSTASK(local_nodeid) in text mode 
		if( TEST_BIT( dump_type, DMP_SYS_DC)){
			MUKDEBUG("Request DC DUMP to SYSTASK(%d) in TEXT mode\n", SYSTASK(local_nodeid));
			m_ptr->m_type = SYS_DUMP;
			m_ptr->m1_i1 = DMP_SYS_DC;
			rcode = muk_sendrec(SYSTASK(local_nodeid), m_ptr);
			if( rcode < 0) ERROR_PRINT(rcode);
		}
		
		if( TEST_BIT( dump_type, DMP_SYS_PROC)){
			MUKDEBUG("Request PROC DUMP to SYSTASK(%d) in TEXT mode\n", SYSTASK(local_nodeid));
			m_ptr->m_type = SYS_DUMP;
			m_ptr->m1_i1 = DMP_SYS_PROC;
			rcode = muk_sendrec(SYSTASK(local_nodeid), m_ptr);
			if( rcode < 0) ERROR_PRINT(rcode);
		}

#ifdef ENABLE_DMP_SYS_STATS	
		if( TEST_BIT( dump_type, DMP_SYS_STATS)){
			MUKDEBUG("Request STATS DUMP to SYSTASK(%d) in TEXT mode\n", SYSTASK(local_nodeid));
			m_ptr->m_type = SYS_DUMP;
			m_ptr->m1_i1 = DMP_SYS_STATS;
			rcode = muk_sendrec(SYSTASK(local_nodeid), m_ptr);
			if( rcode < 0) ERROR_PRINT(rcode);
		}
#endif // ENABLE_DMP_SYS_STATS

		if( TEST_BIT( dump_type, WDMP_SYS_DC)){
			MUKDEBUG("Request DC DUMP to SYSTASK(%d) in HTML mode\n", SYSTASK(local_nodeid));
			m_ptr->m_type = SYS_DUMP;
			m_ptr->m1_i1 = WDMP_SYS_DC;
			rcode = muk_sendrec(SYSTASK(local_nodeid), m_ptr);
			if( rcode < 0) ERROR_PRINT(rcode);
			COND_SIGNAL(www_cond);
			continue;
		}
		
		if( TEST_BIT( dump_type, WDMP_SYS_PROC)){
			MUKDEBUG("Request PROC DUMP to SYSTASK(%d) in HTML mode\n", SYSTASK(local_nodeid));
			m_ptr->m_type = SYS_DUMP;
			m_ptr->m1_i1 = WDMP_SYS_PROC;
			rcode = muk_sendrec(SYSTASK(local_nodeid), m_ptr);
			if( rcode < 0) ERROR_PRINT(rcode);
			COND_SIGNAL(www_cond);
			continue;
		}
#ifdef ENABLE_DMP_SYS_STATS	
		if( TEST_BIT( dump_type, WDMP_SYS_STATS)){
			MUKDEBUG("Request STATS DUMP to SYSTASK(%d) in HTML mode\n", SYSTASK(local_nodeid));
			m_ptr->m_type = SYS_DUMP;
			m_ptr->m1_i1 = WDMP_SYS_STATS;
			rcode = muk_sendrec(SYSTASK(local_nodeid), m_ptr);
			if( rcode < 0) ERROR_PRINT(rcode);	
			COND_SIGNAL(www_cond);
			continue;
		}
#endif // ENABLE_DMP_SYS_STATS
		
#endif // ENABLE_SYSTASK

#if	ENABLE_RDISK 
			// Request INFO DUMP to RDISK in TEXT mode 
			if( TEST_BIT( dump_type, DMP_RD_DEV)){
				MUKDEBUG("Request INFO DUMP to RDISK in TEXT mode\n");
				m_ptr->m_type = DEV_DUMP;
				m_ptr->m1_i1 = DMP_RD_DEV;
				rcode = muk_sendrec(rd_ep, m_ptr);
				if( rcode < 0) ERROR_PRINT(rcode);
			}

			// Request INFO DUMP to RDISK in HTML mode 
			if( TEST_BIT( dump_type, WDMP_RD_DEV)){
				MUKDEBUG("Request INFO DUMP to RDISK in HTML mode\n");
				m_ptr->m_type = DEV_DUMP;
				m_ptr->m1_i1 = WDMP_RD_DEV;
				rcode = muk_sendrec(rd_ep, m_ptr);
				if( rcode < 0) ERROR_PRINT(rcode);
				COND_SIGNAL(www_cond);
				continue;
			}
#endif // ENABLE_RDISK
		
#if	ENABLE_PM
		// Request INFO DUMP to PM in TEXT mode
		if( TEST_BIT( dump_type, DMP_PM_PROC)){
			MUKDEBUG("Request INFO DUMP to PM in TEXT mode\n");
			m_ptr->m_type = MOLDUMP;
			m_ptr->m1_i1 = DMP_PM_PROC;
			rcode = muk_sendrec(pm_ep, m_ptr);
			if( rcode < 0) ERROR_PRINT(rcode);
		}

		// Request INFO DUMP to PM in HTML mode
		if( TEST_BIT( dump_type, WDMP_PM_PROC)){
			MUKDEBUG("Request INFO DUMP to PM in HTML mode\n");
			m_ptr->m_type = MOLDUMP;
			m_ptr->m1_i1 = WDMP_PM_PROC;
			rcode = muk_sendrec(pm_ep, m_ptr);
			if( rcode < 0) ERROR_PRINT(rcode);
			COND_SIGNAL(www_cond);
			continue;
		}
		
#endif // ENABLE_PM

#if	ENABLE_FS
		// Request INFO DUMP to FS in TEXT mode
		if( TEST_BIT( dump_type, DMP_FS_PROC)){
			MUKDEBUG("Request INFO DUMP to FS in TEXT mode\n");
			m_ptr->m_type = MOLDUMP;
			m_ptr->m1_i1 = DMP_FS_PROC;
			rcode = muk_sendrec(fs_ep, m_ptr);
			if( rcode < 0) ERROR_PRINT(rcode);
		}
		
		if( TEST_BIT( dump_type, DMP_FS_SUPER)){
			MUKDEBUG("Request SUPER DUMP to FS in TEXT mode\n");
			m_ptr->m_type = MOLDUMP;
			m_ptr->m1_i1 = DMP_FS_SUPER;
			rcode = muk_sendrec(fs_ep, m_ptr);
			if( rcode < 0) ERROR_PRINT(rcode);
		}
		
		// Request INFO DUMP to FS in HTML mode
		if( TEST_BIT( dump_type, WDMP_FS_PROC)){
			MUKDEBUG("Request INFO DUMP to FS in HTML mode\n");
			m_ptr->m_type = MOLDUMP;
			m_ptr->m1_i1 = WDMP_FS_PROC;
			rcode = muk_sendrec(fs_ep, m_ptr);
			if( rcode < 0) ERROR_PRINT(rcode);
			COND_SIGNAL(www_cond);
			continue;
		}
		
		if( TEST_BIT( dump_type, WDMP_FS_SUPER)){
			MUKDEBUG("Request SUPER DUMP to FS in HTML mode\n");
			m_ptr->m_type = MOLDUMP;
			m_ptr->m1_i1 = WDMP_FS_SUPER;
			rcode = muk_sendrec(fs_ep, m_ptr);
			if( rcode < 0) ERROR_PRINT(rcode);
			COND_SIGNAL(www_cond);
			continue;
		}
		
#endif // ENABLE_FS
	}
	return(OK);				/* shouldn't come here */
}

void is_handler(int signo, siginfo_t *info, void *extra) 
{
	MUKDEBUG("signo:%d\n", signo);

	// WAKEUP main IS 
	MTX_LOCK(wis_mutex);
	dump_type= 0x00FF; // all TEXT dumps 
	COND_SIGNAL(wis_cond);
	MTX_UNLOCK(wis_mutex);

}

void build_html( int bit_nr) 
{
	MUKDEBUG("bit_nr:%d\n", bit_nr);

	// WAKEUP main IS 
	MTX_LOCK(wis_mutex);
	dump_type = 0;
	SET_BIT(dump_type, bit_nr);  
	COND_SIGNAL(wis_cond);
	COND_WAIT(www_cond, wis_mutex);
	MTX_UNLOCK(wis_mutex);

}

/*===========================================================================*
 *				WIS web server					     *
 *===========================================================================*/
void wis_server(int fd)
{
	int j, file_fd, buflen, len;
	long i, ret;
	char * fstr;

	MUKDEBUG("fd:%d\n", fd);
	
	dump_fd = fd; 

	ret = fdread(fd, wis_buffer,WISBUFSIZE); 	/* fdread Web request in one go */
	if(ret == 0 || ret == -1) {	/* fdread failure stop now */
		MUKDEBUG("Failed to fdread browser request: %s",fd);
	}
	if(ret > 0 && ret < WISBUFSIZE)	/* return code is valid chars */
		wis_buffer[ret]=0;		/* terminate the wis_buffer */
	else wis_buffer[0]=0;

	MUKDEBUG("wis_buffer:%s\n", wis_buffer);

	for(i=0;i<ret;i++)	/* remove CF and LF characters */
		if(wis_buffer[i] == '\r' || wis_buffer[i] == '\n')
			wis_buffer[i]='*';

	if( strncmp(wis_buffer,"GET ",4) && strncmp(wis_buffer,"get ",4) )
		printf("Only simple GET operation supported: %s",fd);

	for(i=4;i<WISBUFSIZE;i++) { /* null terminate after the second space to ignore extra stuff */
		if(wis_buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
			wis_buffer[i] = 0;
			break;
		}
	}

	/* Clean the wis_html page wis_buffer */
//	for(i=0;i<WISBUFSIZE;i++) wis_html[i] = 0;
	memset(wis_html, 0, WISBUFSIZE);
		
	/* System call according to GET argument */
	if ( !strcmp(&wis_buffer[5],      "sys_dc_dmp") ) 		build_html(WDMP_SYS_DC);
	else if ( !strcmp(&wis_buffer[5], "sys_proc_dmp") ) 	build_html(WDMP_SYS_PROC);
#ifdef ENABLE_DMP_SYS_STATS
	else if ( !strcmp(&wis_buffer[5], "sys_stats_dmp") ) 	build_html(WDMP_SYS_STATS);
#endif // ENABLE_DMP_SYS_STATS
//	else if ( !strcmp(&wis_buffer[5], "sys_priv_dmp") ) 	build_html(wis_html);
	else if ( !strcmp(&wis_buffer[5], "rd_dev_dmp") ) 		build_html(WDMP_RD_DEV);
	else if ( !strcmp(&wis_buffer[5], "pm_proc_dmp") ) 		build_html(WDMP_PM_PROC);
	else if ( !strcmp(&wis_buffer[5], "fs_proc_dmp") ) 		build_html(WDMP_FS_PROC);
	else if ( !strcmp(&wis_buffer[5], "fs_super_dmp") ) 	build_html(WDMP_FS_SUPER);
	
	/* Start building the HTTP response */
	memset(wis_buffer, 0, WISBUFSIZE);
	(void)strcat(wis_buffer,"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
//	(void)fdwrite(fd,wis_buffer,strlen(wis_buffer));

	/* Start building the HTML page */
	(void)strcat(wis_buffer,"<html><head><title>WIS - Web Information Server</title>");

	/* Add some CSS style */
	(void)strcat(wis_buffer,"<style type=\"text/css\">\n");
	(void)strcat(wis_buffer,"body { font-family: Verdana,Arial,sans-serif; }\n");
	(void)strcat(wis_buffer,"body,p,blockquote,li,th,td { font-size: 10pt; }\n");
	(void)strcat(wis_buffer,"table { border-spacing: 0px; background: #E9E9F3; border: 0.5em solid #E9E9F3; }\n");
	(void)strcat(wis_buffer,"table th { text-align: left; font-weight: normal; padding: 0.1em 0.5em; border: 0px; border-bottom: 1px solid #9999AA; }\n");
	(void)strcat(wis_buffer,"table td { text-align: right; border: 0px; border-bottom: 1px solid #9999AA; border-left: 1px solid #9999AA; padding: 0.1em 0.5em; }\n");
	(void)strcat(wis_buffer,"table thead th { text-align: center; font-weight: bold; color: #6C6C9A; border-left: 1px solid #9999AA; }\n");
	(void)strcat(wis_buffer,"table th.Corner { text-align: left; border-left: 0px; }\n");
	(void)strcat(wis_buffer,"table tr.Odd { background: #F6F4E4; }\n");
	(void)strcat(wis_buffer,"</style></head>");
	
	/* Start body tag */
	(void)strcat(wis_buffer,"<body>");

	/* Add the main top table with links to the functions */
	(void)strcat(wis_buffer,"<table><thead><tr><th colspan=\"10\">MUK Web Information Server .</th></tr></thead><tbody><tr>");
	(void)strcat(wis_buffer,"<th><a href=\"/sys_dc_dmp\" >sys_dc_dmp (F1)</a></th>\n");
	(void)strcat(wis_buffer,"<th><a href=\"/sys_proc_dmp\" >sys_proc_dmp (F2)</a></th>\n");
	(void)strcat(wis_buffer,"<th><a href=\"/sys_stats_dmp\" >sys_stats_dmp (F3)</th>\n");
//	(void)strcat(wis_buffer,"<th><a href=\"/sys_priv_dmp\" >sys_priv_dmp (F4)</a></th>\n");
	(void)strcat(wis_buffer,"<th><a href=\"/rd_dev_dmp\" >rd_dev_dmp (F5)</a></th>\n");
	(void)strcat(wis_buffer,"<th><a href=\"/pm_proc_dmp\" >pm_proc_dmp (F6)</a></th>\n");
	(void)strcat(wis_buffer,"<th><a href=\"/fs_proc_dmp\" >fs_proc_dmp (F7)</a></th>\n");
	(void)strcat(wis_buffer,"<th><a href=\"/fs_super_dmp\" >fs_super_dmp (F9)</a></th>\n");

	(void)strcat(wis_buffer,"</tr></tbody></table><br />");
// (void)fdwrite(fd,wis_buffer,strlen(wis_buffer));

	/* Write the HTML generated in functions to network socket */
	if( strlen(wis_html) != 0){
		(void)strcat(wis_buffer,wis_html);
//		(void)fdwrite(fd,wis_html,strlen(wis_html));
		MUKDEBUG("%s\n",wis_buffer);	
		MUKDEBUG("strlen(wis_buffer)=%d\n", strlen(wis_buffer));
	}
	
	/* Finish HTML code */
	(void)strcat(wis_buffer,"</body></html>");
// (void)sprintf(wis_buffer,"</body></html>");
	(void)fdwrite(fd,wis_buffer,strlen(wis_buffer));

	close(fd);
		
}

/*===========================================================================*
 *				main_wis                                         *
 *===========================================================================*/
 static void main_wis(void *str_arg)
{
	int i, wis_port, wis_id, wis_listenfd, wis_sfd, wis_hit;
	size_t length;
	int rcode;
	int rport;
	char remote[16];
	static struct sockaddr_in wis_cli_addr; /* static = initialised to zeros */
	static struct sockaddr_in serv_addr; /* static = initialised to zeros */
	
	MUKDEBUG("WIS service starting.\n");
	wis_id = taskid();
	MUKDEBUG("wis_id=%d\n", wis_id);
	
	/* Setup the network socket */
	wis_port = WIS_PORT+dcid;
	if((wis_listenfd = netannounce(TCP, 0, wis_port)) < 0){
		rcode = -errno;
		ERROR_TSK_EXIT(rcode);
	}
	MUKDEBUG("netannounce on TCP port=%d\n", wis_port);
		
/*
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(wis_port);
		
	if(bind(wis_listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0){
		ERROR_TSK_EXIT(errno);
	}
		
	if( listen(wis_listenfd,64) <0){
		ERROR_TSK_EXIT(errno);
	}
*/
			
	MUKDEBUG("Starting WIS server main loop\n");
	for(wis_hit=1; ;wis_hit++) {
		length = sizeof(wis_cli_addr);
		MUKDEBUG("Conection accept\n");
//		wis_sfd = accept(wis_listenfd, (struct sockaddr *)&wis_cli_addr, &length);
		wis_sfd = netaccept(wis_listenfd, remote, &rport);
		if (wis_sfd < 0){
			rcode = -errno;
			ERROR_TSK_EXIT(rcode);
		}
		MUKDEBUG("hit %d - connection from %s:%d\n", wis_hit, remote, rport);
		taskcreate(wis_server, (void*)wis_sfd, MUK_STACK_SIZE);
	}
	
	return(OK);		/* shouldn't come here */
}


/*===========================================================================*
 *				 init_is                                 *
 *===========================================================================*/
void init_is(void)
{
     struct sigaction action;
	 int rcode;
    proc_usr_t *proc_ptr;

	MUKDEBUG("dcid=%d is_ep=%d\n",dcid, is_ep);

	MUKDEBUG(DVS_USR_FORMAT,DVS_USR_FIELDS(dvs_ptr));
	MUKDEBUG(DC_USR1_FORMAT,DC_USR1_FIELDS(dc_ptr));
	MUKDEBUG(DC_USR2_FORMAT,DC_USR2_FIELDS(dc_ptr));

	is_id = taskid();
	MUKDEBUG("is_id=%d\n", is_id);
	
	rcode = muk_tbind(dcid,is_ep, "is");
	MUKDEBUG("rcode=%d\n", rcode);
	if( rcode != is_ep) {
		ERROR_PRINT(EDVSENDPOINT);
		taskexit(&rcode);
	}
		
	MUKDEBUG("Get is_ep info\n");
	is_ptr = current_task();
	proc_ptr = is_ptr->p_proc;
	MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));	
	
	/* Register into SYSTASK(local_nodeid) (as an autofork) */
//	MUKDEBUG("Register IS into SYSTASK(local_nodeid) is_id=%d\n",is_id);
//	is_ep = sys_bindproc(is_ep, is_id, LCL_BIND);
//	if(is_ep < 0) ERROR_TSK_EXIT(is_ep);
	
	MUKDEBUG("Register IS into PM is_id=%d\n",is_id);
	rcode = mol_bindproc(is_ep, is_ep, is_id, LCL_BIND);
	if(rcode < 0) ERROR_TSK_EXIT(rcode);
	
	// set the name of IS 
	rcode = sys_rsetpname(is_ep, "is", local_nodeid);
	if(rcode < 0) ERROR_TSK_EXIT(rcode);
		
//	rcode = pthread_create( &wis_pth, NULL, main_wis, "wis\n" );
	sprintf(wis_cmd,"wis\n");
	wis_id = taskcreate(main_wis, wis_cmd, MUK_STACK_SIZE);
	MUKDEBUG("wis_id=%d\n",wis_id);
	
	action.sa_flags = SA_SIGINFO; 
    action.sa_sigaction = is_handler;
 
    if (sigaction(SIGUSR1, &action, NULL) == -1) {
		rcode = -errno;
		ERROR_TSK_EXIT(rcode);
    }
		
}

