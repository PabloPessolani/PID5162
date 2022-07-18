/***********************************************************/
/*  DSP_PWS									*/
/***********************************************************/

#include "dsp_proxy.h"

#if PWS_ENABLED

void update_stats(proxy_t *px_ptr, proxy_hdr_t *hd_ptr, proxy_payload_t *pl_ptr, int px_conn_type)
{
	px_stats_t *pxs_ptr;
	proxy_hdr_t *bat_cmd, *bat_ptr;
	int i;

	PXYDEBUG("PWS(%d): px_conn_type=%d\n", px_ptr->px_nodeid, px_conn_type);

	MTX_LOCK(px_ptr->px_mutex);
	pxs_ptr = &px_ptr->px_stat[hd_ptr->c_dcid];
	pxs_ptr->pst_nr_cmd++;
	pxs_ptr->pst_dcid 	=  hd_ptr->c_dcid;
	if( px_conn_type == CONNECT_RPROXY){
		pxs_ptr->pst_dnode	=  hd_ptr->c_dnode;
		pxs_ptr->pst_snode	=  hd_ptr->c_snode;		
	}else{
		pxs_ptr->pst_snode	=  lb_ptr->lb_nodeid;
		pxs_ptr->pst_dnode	=  hd_ptr->c_dnode;
	}
	
	switch(hd_ptr->c_cmd){
		case CMD_COPYIN_DATA:
		case CMD_COPYOUT_DATA:
			pxs_ptr->pst_nr_data += hd_ptr->c_len;
			break;
		case CMD_SEND_MSG:
		case CMD_SNDREC_MSG:
		case CMD_REPLY_MSG:
			pxs_ptr->pst_nr_msg++;
			// fall down 
		default:	
			pxs_ptr->pst_nr_cmd += hd_ptr->c_batch_nr;
			if( hd_ptr->c_batch_nr != 0) {
				bat_cmd = (proxy_hdr_t *) pl_ptr;
				for( i = 0; i < hd_ptr->c_batch_nr; i++){
					bat_ptr = &bat_cmd[i];
					if( (bat_ptr->c_cmd == CMD_SEND_MSG)
					||  (bat_ptr->c_cmd == CMD_SNDREC_MSG)
					||  (bat_ptr->c_cmd == CMD_REPLY_MSG)){
						pxs_ptr->pst_nr_msg++;
					}		  
				}
			}			
			break;
	}
	PXYDEBUG(PXS_FORMAT, PXS_FIELDS(pxs_ptr));
	MTX_UNLOCK(px_ptr->px_mutex);
}

/*===========================================================================*
 *				PWS web server					     *
 *===========================================================================*/
void pws_server(proxy_t *px_ptr, int fd, int hit, int px_conn_type)
{
	int j, file_fd, buflen, len;
	long i, ret;
	char * fstr;

	PXYDEBUG("fd:%d hit:%d\n", fd, hit);

	ret =read(fd, pws_buffer,PWS_BUFSIZE); 	/* read Web request in one go */
	if(ret == 0 || ret == -1) {	/* read failure stop now */
		PXYDEBUG("Failed to read browser request: %s",fd);
	}
	if(ret > 0 && ret < PWS_BUFSIZE)	/* return code is valid chars */
		pws_buffer[ret]=0;		/* terminate the pws_buffer */
	else pws_buffer[0]=0;

	PXYDEBUG("pws_buffer:%s\n", pws_buffer);

	for(i=0;i<ret;i++)	/* remove CF and LF characters */
		if(pws_buffer[i] == '\r' || pws_buffer[i] == '\n')
			pws_buffer[i]='*';

	if( strncmp(pws_buffer,"GET ",4) && strncmp(pws_buffer,"get ",4) )
		printf("Only simple GET operation supported: %s",fd);

	for(i=4;i<PWS_BUFSIZE;i++) { /* null terminate after the second space to ignore extra stuff */
		if(pws_buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
			pws_buffer[i] = 0;
			break;
		}
	}

	/* Start building the HTTP response */
	memset(pws_buffer, 0, PWS_BUFSIZE);
	(void)strcat(pws_buffer,"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
//	(void)write(fd,pws_buffer,strlen(pws_buffer));

	/* Start building the HTML page */
	if(px_conn_type == CONNECT_SPROXY)
		(void)strcat(pws_buffer,"<html><head><title>PWS - SENDER   PROXY  Server</title>");
	else 
		(void)strcat(pws_buffer,"<html><head><title>PWS - RECEIVER PROXY  Server</title>");
		
	/* Add some CSS style */
	(void)strcat(pws_buffer,"<style type=\"text/css\">\n");
	(void)strcat(pws_buffer,"body { font-family: Verdana,Arial,sans-serif; }\n");
	(void)strcat(pws_buffer,"body,p,blockquote,li,th,td { font-size: 10pt; }\n");
	(void)strcat(pws_buffer,"table { border-spacing: 0px; background: #E9E9F3; border: 0.5em solid #E9E9F3; }\n");
	(void)strcat(pws_buffer,"table th { text-align: left; font-weight: normal; padding: 0.1em 0.5em; border: 0px; border-bottom: 1px solid #9999AA; }\n");
	(void)strcat(pws_buffer,"table td { text-align: right; border: 0px; border-bottom: 1px solid #9999AA; border-left: 1px solid #9999AA; padding: 0.1em 0.5em; }\n");
	(void)strcat(pws_buffer,"table thead th { text-align: center; font-weight: bold; color: #6C6C9A; border-left: 1px solid #9999AA; }\n");
	(void)strcat(pws_buffer,"table th.Corner { text-align: left; border-left: 0px; }\n");
	(void)strcat(pws_buffer,"table tr.Odd { background: #F6F4E4; }\n");
	(void)strcat(pws_buffer,"</style></head>");
	
	/* Start body tag */
	(void)strcat(pws_buffer,"<body>");

	/* Add the main top table with links to the functions */
	if(px_conn_type == CONNECT_SPROXY)
		(void)strcat(pws_buffer,"<table><thead><tr><th colspan=\"10\">SENDER Proxy Web Server .</th></tr></thead><tbody><tr>");
	else
		(void)strcat(pws_buffer,"<table><thead><tr><th colspan=\"10\">RECEIVER Proxy Web Server .</th></tr></thead><tbody><tr>");
		
	(void)strcat(pws_buffer,"</tr></tbody></table><br/>");
	
// (void)write(fd,pws_buffer,strlen(pws_buffer));
	/* Write the HTML generated in functions to network socket */
	wdmp_px_stats(px_ptr);
	if( strlen(pws_html) != 0){
		(void)strcat(pws_buffer,pws_html);
//		(void)write(fd,pws_html,strlen(pws_html));
//		PXYDEBUG("%s\n",pws_buffer);	
		PXYDEBUG("strlen(pws_buffer)=%d\n", strlen(pws_buffer));
	}
	
	/* Finish HTML code */
	(void)strcat(pws_buffer,"</body></html>");
// (void)sprintf(pws_buffer,"</body></html>");
	(void)write(fd,pws_buffer,strlen(pws_buffer));
		
}

/*===========================================================================*
 *				main_pws                                         *
 *===========================================================================*/
void main_pws(void *arg)
{
	int i, pws_pid, pws_listenfd, pws_sfd, pws_hit, px_conn_type;
	size_t length;
	int rcode;
	proxy_t *px_ptr;

	static struct sockaddr_in pws_cli_addr; /* static = initialised to zeros */
	static struct sockaddr_in serv_addr; /* static = initialised to zeros */
	
	PXYDEBUG("PWS service starting.\n");
	pws_pid = syscall (SYS_gettid);
	PXYDEBUG("pws_pid=%d\n", pws_pid);
	
			/* Setup the network socket */
	if((pws_listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
			ERROR_PT_EXIT(errno);
		
	/* PWS service Web server px_ptr->px_pws_port */
	px_ptr = (proxy_t *) arg;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(px_ptr->px_pws_port);
	PXYDEBUG("px_ptr->px_pws_port=%d\n", px_ptr->px_pws_port);
	
	if(bind(pws_listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0){
		ERROR_PT_EXIT(errno);
	}
		
	if( listen(pws_listenfd,64) <0){
		ERROR_PT_EXIT(errno);
	}
	
	MTX_LOCK(px_ptr->px_mutex);
	if( px_ptr->px_pws_port < BASE_PWS_RPORT){
		COND_SIGNAL(px_ptr->px_sdesc.td_cond);
		px_conn_type = CONNECT_SPROXY;
		COND_WAIT(px_ptr->px_sdesc.td_pws_cond, px_ptr->px_mutex);
	}else{
		COND_SIGNAL(px_ptr->px_rdesc.td_cond);
		px_conn_type = CONNECT_RPROXY;
		COND_WAIT(px_ptr->px_rdesc.td_pws_cond, px_ptr->px_mutex);
	}
	MTX_UNLOCK(px_ptr->px_mutex);
	
	if(px_conn_type == CONNECT_SPROXY){
		PXYDEBUG("Starting SENDER PWS server main loop\n");
	}else{
		PXYDEBUG("Starting RECEIVER PWS server main loop\n");
	}
	
	for(pws_hit=1; ;pws_hit++) {
		length = sizeof(pws_cli_addr);
		PXYDEBUG("Conection accept\n");
		pws_sfd = accept(pws_listenfd, (struct sockaddr *)&pws_cli_addr, &length);
		if (pws_sfd < 0){
			rcode = -errno;
			ERROR_PT_EXIT(rcode);
		}
		pws_server(px_ptr, pws_sfd, pws_hit, px_conn_type);
		close(pws_sfd);
	}
	
	return(OK);		/* shouldn't come here */
}

/*===========================================================================*
 *				wdmp_px_stats  					    				     *
 *===========================================================================*/
void wdmp_px_stats(proxy_t *px_ptr)
{
	int n, d;
	char *page_ptr;
	px_stats_t	*pxs_ptr;

	PXYDEBUG("\n");
	page_ptr = pws_html;
	*page_ptr = 0;

	(void)strcat(page_ptr,"<table>\n");
	(void)strcat(page_ptr,"<thead>\n");
	(void)strcat(page_ptr,"<tr>\n");
	(void)strcat(page_ptr,"<th>---snode--</th>\n");
	(void)strcat(page_ptr,"<th>---dnode--</th>\n");
	(void)strcat(page_ptr,"<th>---dcid---</th>\n");
	(void)strcat(page_ptr,"<th>--nr_msgs-</th>\n");
	(void)strcat(page_ptr,"<th>--nr_data-</th>\n");
	(void)strcat(page_ptr,"<th>--nr_cmd--</th>\n");
	(void)strcat(page_ptr,"</tr>\n");
	(void)strcat(page_ptr,"</thead>\n");
	(void)strcat(page_ptr,"<tbody>\n");

	MTX_LOCK(px_ptr->px_mutex);
	for(n = 0; n < (dvs.d_nr_nodes-1); n++) {
		for(d = 0; d < dvs.d_nr_dcs; d++) {
			pxs_ptr = &px_ptr->px_stat[d];
			if ((pxs_ptr->pst_nr_cmd == 0) || // have no traffic
			    (pxs_ptr->pst_snode == pxs_ptr->pst_dnode)) {  
				continue;
			}
			(void)strcat(page_ptr,"<tr>\n");
			sprintf(tmp_buffer, "<td>%10d</td> <td>%10d</td> <td>%10d</td>"
								" <td>%10ld</td> <td>%10ld</td> <td>%10ld</td>\n",
					pxs_ptr->pst_snode,
					pxs_ptr->pst_dnode,
					pxs_ptr->pst_dcid,
					pxs_ptr->pst_nr_msg,
					pxs_ptr->pst_nr_data,
					pxs_ptr->pst_nr_cmd);	
			(void)strcat(page_ptr,tmp_buffer);
			(void)strcat(page_ptr,"</tr>\n");
			}
	}
	MTX_UNLOCK(px_ptr->px_mutex);

	(void)strcat(page_ptr,"</tbody></table>\n");
   	PXYDEBUG("%s\n",page_ptr);	
	PXYDEBUG("strlen(page_ptr)=%d\n", strlen(page_ptr));	
}

#endif //  PWS_ENABLED

