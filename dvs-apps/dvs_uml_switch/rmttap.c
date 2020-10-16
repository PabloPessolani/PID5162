
#include "switch.h"

void rmttap_init(rmttap_t *rt_ptr)
{
	int rcode; 
	
	USRDEBUG("\n");

	strcpy(rt_ptr->rt_sock_type,"unix");
	sprintf(rt_ptr->rt_ctrl_path,"/tmp/uml%0.2d%0.2d.ctrl",sw_ptr->sw_dcid, rt_ptr->rt_index);
	sprintf(rt_ptr->rt_data_path,"/tmp/uml%0.2d%0.2d.data",sw_ptr->sw_dcid, rt_ptr->rt_index);
	rcode = remove(rt_ptr->rt_ctrl_path);
	if (rcode == -1 && errno != ENOENT)
        ERROR_PRINT(-errno);
	rcode = remove(rt_ptr->rt_data_path);
	if (rcode == -1 && errno != ENOENT)
        ERROR_PRINT(-errno);
	
	printf("rmttap_init (dvs_uml_switch version %d) - %s\n\t\t%s\n\t\t%s\n",
	       SWITCH_VERSION, rt_ptr->rt_sock_type, 
		   rt_ptr->rt_ctrl_path, 
		   rt_ptr->rt_ctrl_path);
}
 
static int connect_to_switch(rmttap_t *rt_ptr)
{
	int n, rcode;
	struct sockaddr_un sun;
	
	USRDEBUG("rt_name=%s\n", rt_ptr->rt_name);

	rt_ptr->rt_ctrl_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (rt_ptr->rt_ctrl_fd < 0) {
		rcode = -errno;
		fprintf(stderr, "connect_to_switch(%s) : control socket failed, "
		       "errno = %d\n", rt_ptr->rt_name,  rcode);
		ERROR_RETURN(rcode);
	}
	
	rt_ptr->rt_ctrl_sun.sun_family = AF_UNIX;
	strncpy(rt_ptr->rt_ctrl_sun.sun_path, sw_ptr->sw_ctrl_path, strlen(sw_ptr->sw_ctrl_path));
	USRDEBUG("CTRL sun_path=%s \n", rt_ptr->rt_ctrl_sun.sun_path);

	if (connect(rt_ptr->rt_ctrl_fd, (struct sockaddr *) &rt_ptr->rt_ctrl_sun,
		   sizeof(struct sockaddr_un)) < 0) {
		rcode = -errno;
		fprintf(stderr,"connect_to_switch(%s) : control connect failed, "
		       "errno = %d\n", rt_ptr->rt_name, -rcode);
		goto out;
	}

	rt_ptr->rt_data_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (rt_ptr->rt_data_fd < 0) {
		rcode = -errno;
		fprintf(stderr, "connect_to_switch(%s) : data socket failed, "
		       "errno = %d\n", rt_ptr->rt_name, rcode);
		goto out;
	}
	
	rt_ptr->rt_data_sun.sun_family = AF_UNIX;
	strncpy(rt_ptr->rt_data_sun.sun_path, rt_ptr->rt_data_path, strlen(rt_ptr->rt_data_path));
	USRDEBUG("DATA sun_path=%s \n", rt_ptr->rt_data_sun.sun_path);

	if (bind(rt_ptr->rt_data_fd, (struct sockaddr *) &rt_ptr->rt_data_sun, sizeof(struct sockaddr_un)) < 0) {
		rcode = -errno;
		fprintf(stderr, "connect_to_switch(%s) : data bind failed, "
		       "errno = %d\n", rt_ptr->rt_name, rcode);
		goto out_close;
	}

	rt_ptr->rt_req.magic 	= SWITCH_MAGIC;
	rt_ptr->rt_req.version  = SWITCH_VERSION;
	rt_ptr->rt_req.type 	= REQ_NEW_CONTROL;
	rt_ptr->rt_req.sock 	= rt_ptr->rt_data_sun;
	
	USRDEBUG("write to switch %s \n", rt_ptr->rt_name);
	n = write(rt_ptr->rt_ctrl_fd, &rt_ptr->rt_req, sizeof(struct request_v3));
	if (n != sizeof(struct request_v3)) {
		fprintf(stderr, "connect_to_switch(%s) : control setup request "
		       "failed, rcode = %d\n", rt_ptr->rt_name, errno);
		rcode = -ENOTCONN;
		goto out_free;
	}

	USRDEBUG("read from switch %s \n", rt_ptr->rt_name);
	n = read(rt_ptr->rt_ctrl_fd, &sun, sizeof(sun));
	if (n != sizeof(sun)) {
		fprintf(stderr, "connect_to_switch(%s) : read of data socket failed, "
		       "rcode = %d\n", rt_ptr->rt_name, errno);
		rcode = -ENOTCONN;
		goto out_free;
	}

	rt_ptr->rt_data_sun = sun;
	return rt_ptr->rt_data_fd;

 out_free:
 out_close:
	close(rt_ptr->rt_data_fd);
 out:
	close(rt_ptr->rt_ctrl_fd);
	return rcode;
}

/*===========================================================================*
 *				rt_send_packet				     *
 *===========================================================================*/
int rt_send_packet(rmttap_t *r_ptr, void *packet, int len, void *unused)
{
	int rcode;
	message *m_ptr;

	USRDEBUG("r_ptr->rt_name=%s r_ptr->rt_rmttap_idx=%d\n",
		r_ptr->rt_name, r_ptr->rt_rmttap_idx);
		
	if( r_ptr->rt_rmttap_idx < 0) {
		ERROR_RETURN(EDVSBADF);
	}
	
	m_ptr = &sndr_msg;
	m_ptr->m_type 	= REQ_RT_WRITE;
	m_ptr->m3_p1	= packet;
	m_ptr->m3_i1	= r_ptr->rt_rmttap_idx;
	m_ptr->m3_i2	= len;
	
 	USRDEBUG("r_ptr->rt_nodeid=%d\n", r_ptr->rt_nodeid);
	USRDEBUG("WRITE REQUEST: " MSG3_FORMAT, MSG3_FIELDS(m_ptr));
	rcode = dvk_sendrec_T(r_ptr->rt_nodeid, m_ptr, TIMEOUT_MOLCALL);	
	if( rcode < 0) {
		ERROR_RETURN(rcode);
	}
	if( m_ptr->m3_i1 < 0) {
		ERROR_RETURN(m_ptr->m3_i1);
	}
	USRDEBUG("WRITE REPLY: " MSG3_FORMAT, MSG3_FIELDS(m_ptr));
	
	return(OK);
	
}

/*===========================================================================*
 *				rt_open_tap				     *
 *===========================================================================*/
int rt_open_tap(rmttap_t *r_ptr)
{
	int rcode;
	proc_usr_t *proc_ptr;
	message *m_ptr;
	
    USRDEBUG(RT_FORMAT, RT_FIELDS(r_ptr));

	proc_ptr = &r_ptr->rt_proc; 
	rcode = dvk_getprocinfo(sw_ptr->sw_dcid, r_ptr->rt_nodeid, proc_ptr);		
	//Get the status and parameter information about a process in a DC.
	USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
			
	m_ptr = &sndr_msg;
	m_ptr->m_type = REQ_RT_OPEN;
	strncpy(m_ptr->m3_ca1, r_ptr->rt_tap, M3_STRING-1);
	USRDEBUG("r_ptr->rt_nodeid=%d\n", r_ptr->rt_nodeid);
	USRDEBUG("OPEN REQUEST: " MSG3_FORMAT, MSG3_FIELDS(m_ptr));
	rcode = dvk_sendrec_T(r_ptr->rt_nodeid, m_ptr, TIMEOUT_MOLCALL);	
	if( rcode < 0) {
		ERROR_RETURN(rcode);
	}
	if( m_ptr->m3_i1 < 0) {
		ERROR_RETURN(m_ptr->m3_i1);
	}
	USRDEBUG("OPEN REPLY: " MSG3_FORMAT, MSG3_FIELDS(m_ptr));
	r_ptr->rt_rmttap_idx = m_ptr->m3_i1; 
	USRDEBUG("Remote TAP %s successfully opened with remote index %d\n", 
		r_ptr->rt_tap, r_ptr->rt_rmttap_idx);
	
	return(OK);
}

/*===========================================================================*
 *				get_dvs_params				     *
 *===========================================================================*/
void get_dvs_params(void)
{
	USRDEBUG("\n");
	local_nodeid = dvk_getdvsinfo(&dvs);
	USRDEBUG("local_nodeid=%d\n",local_nodeid);
	if( local_nodeid < DVS_NO_INIT) ERROR_EXIT(local_nodeid);
	dvs_ptr = &dvs;
	USRDEBUG(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));
}

/*===========================================================================*
 *				get_dc_params				     *
 *===========================================================================*/
void get_dc_params(int dcid)
{
	int rcode;

	USRDEBUG("dcid=%d\n", dcid);
	if ( dcid < 0 || dcid >= dvs.d_nr_dcs) {
 	        fprintf(stderr,"Invalid dcid [0-%d]\n", dvs.d_nr_dcs );
 	        ERROR_EXIT(EDVSBADDCID);
	}
	rcode = dvk_getdcinfo(dcid, &dcu);
	if( rcode ) ERROR_EXIT(rcode);
	dc_ptr = &dcu;
	USRDEBUG(DC_USR1_FORMAT, DC_USR1_FIELDS(dc_ptr));
	USRDEBUG(DC_USR2_FORMAT, DC_USR2_FIELDS(dc_ptr));
}

/*===========================================================================*
 *				try_rmttap_open		     
 *===========================================================================*/	
void try_rmttap_open(void)
{
	int i, rcode;
	rmttap_t *r_ptr;

	USRDEBUG("nr_rmttap=%d\n", nr_rmttap);
	
	for( i = 0; i < nr_rmttap; i++) {
		r_ptr = &rmttap_cfg[i];
		// Only those remote tap devices with open unix sockets 
        USRDEBUG(RT_FORMAT, RT_FIELDS(r_ptr));
		if( r_ptr->rt_poll_idx >= 0 ){ //if it has a virtual port allocated
			if( r_ptr->rt_nodeid != local_nodeid){ // it is a remote port 
				if( r_ptr->rt_rmttap_idx < 0) {  // if it is yet opened
					rcode = rt_open_tap(r_ptr);  
					if( rcode < 0) {
						ERROR_PRINT(rcode);
						continue;
					}
				}
			}
		}
	}
}

/*===========================================================================*
 *				send_thread		     
 *===========================================================================*/	
void *send_thread(void *arg)
{
	int i, j;
	int rcode, n, timeout; 
	proc_usr_t *proc_ptr;
static struct packet packet;
    int len, socklen;
	rmttap_t *r_ptr;
	
	// Local Endpoint has is calculated from the first Client Endpoint plus the index 
	sw_ptr->sw_send_ep = (dc_ptr->dc_nr_sysprocs - dc_ptr->dc_nr_tasks);
	sw_ptr->sw_send_ep += (local_nodeid);
	sw_ptr->sw_send_tid = syscall (SYS_gettid);
	USRDEBUG("SENDER THREAD: sw_send_ep=%d sw_send_tid=%d\n", 
		sw_ptr->sw_send_ep, sw_ptr->sw_send_tid);

	// Bind Remote TAP Client endpoint 
	rcode = dvk_tbind(sw_ptr->sw_dcid, sw_ptr->sw_send_ep);
	if( rcode != sw_ptr->sw_send_ep) {
		ERROR_EXIT(rcode);
	}
	
	proc_ptr = &sw_ptr->sw_send_proc;  
    rcode = dvk_getprocinfo(sw_ptr->sw_dcid, sw_ptr->sw_send_ep, proc_ptr);	
    USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));

	// OPEN remote TAPs through M3IPC 
	success_open = 0;
	try_rmttap_open();
	
	// init_socket or pipe 
	timeout = POLL_TIMEOUT;
	while(1){
		n = poll(rt_fds, rt_nfds, timeout);
		if(n == 0){
			USRDEBUG("Timeout\n");
			if( success_open < nr_rmttap)
				try_rmttap_open();
			continue;
		}
		if(n < 0){
			if(errno == EINTR) {
				if( success_open < nr_rmttap)
					try_rmttap_open();
				// USRDEBUG("EINTR\n");
				continue;
			}
			ERROR_PRINT(-errno);
			break;
		}
		for(i = 0; i < rt_nfds; i++){
			if(rt_fds[i].revents == 0) 
				continue;
			for(j = 0; j < nr_rmttap; j++){
				r_ptr = &rmttap_cfg[j];
				if( i == r_ptr->rt_poll_idx){
					USRDEBUG("%s rt_poll_idx=%d\n",r_ptr->rt_name ,r_ptr->rt_poll_idx);
					// for those remote TAP which were down during initialization
					// retry the open 
					if( r_ptr->rt_poll_idx == (-1)){
						rcode = rt_open_tap(r_ptr);  
						if( rcode < 0 ) {
							ERROR_PRINT(rcode);
							continue;
						}						
					}
					socklen = sizeof(struct sockaddr_un);
					len = recvfrom(r_ptr->rt_data_fd, &packet, sizeof(packet), 0, 
						 (struct sockaddr *) &r_ptr->rt_data_sun, &socklen);
					if(len < 0){
						if(errno != EAGAIN) 
							perror("handle_sock_data");
						ERROR_RETURN(-errno);
					}
					rcode = rt_send_packet(r_ptr, &packet, len, NULL);
					if(rcode < 0){
						// clear the remote index 
						success_open--;
						r_ptr->rt_poll_idx = (-1);
						if( rcode != EDVSTIMEDOUT)
							try_rmttap_open();
						ERROR_RETURN(rcode);
					}
				}
			} 
		}			
	}

	dvk_unbind(sw_ptr->sw_dcid, sw_ptr->sw_send_ep);
	pthread_exit(OK);
}

/*===========================================================================*
 *				recv_thread			     
 *===========================================================================*/	
void *recv_thread(void *arg)
{
	int rcode, i; 
	proc_usr_t *proc_ptr;
	static struct packet packet;
	message *m_ptr;
	rmttap_t *r_ptr;


	sw_ptr->sw_recv_ep = (local_nodeid);
	sw_ptr->sw_recv_tid = syscall (SYS_gettid);
	USRDEBUG("RECEIVER THREAD: sw_recv_ep=%d sw_recv_tid=%d\n", 
		sw_ptr->sw_recv_ep, sw_ptr->sw_recv_tid);

	// Bind Receiver thread 
	rcode = dvk_tbind(sw_ptr->sw_dcid, sw_ptr->sw_recv_ep);
	if( rcode != sw_ptr->sw_recv_ep) {
		ERROR_EXIT(rcode);
	}
	proc_ptr = &sw_ptr->sw_recv_proc;
    rcode = dvk_getprocinfo(sw_ptr->sw_dcid, sw_ptr->sw_recv_ep, proc_ptr);	
    USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
	
	// init_socket or pipe
	m_ptr = &rcvr_msg;
	while(1){
		rcode = dvk_receive_T(ANY, m_ptr, TIMEOUT_MOLCALL); 
		if( rcode < 0){
			ERROR_PRINT(rcode);
			continue;
		}
		USRDEBUG(MSG3_FORMAT, MSG3_FIELDS(m_ptr));
		switch( m_ptr->m_type){
			case REQ_RT_OPEN:
				//  m_ptr->m3_ca1 : local TAP name 
				for( i = 0; i < nr_rmttap; i++) {
					r_ptr = &rmttap_cfg[i];
					if( !strncmp(r_ptr->rt_tap, m_ptr->m3_ca1, M3_STRING-1)) 
						break;	
				}
				if( i == nr_rmttap) { // TAP device not found 
					ERROR_PRINT(EDVSNOENT);
					rcode = EDVSNOENT;
				} else {
					rcode = i;
				}						
				break;
			case REQ_RT_WRITE: // the remote process want to write into a local socket 	
				// get the remote packet 
				// m_ptr->m3_p1	= packet;
				// m_ptr->m3_i1	= r_ptr->rt_rmttap_idx;
				// m_ptr->m3_i2	= len;
				rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m3_p1, 
							sw_ptr->sw_recv_ep, &packet, m_ptr->m3_i1);
				if( rcode < 0){
					ERROR_PRINT(rcode);
					break;
				}
				// search the port with the index  ATENCION!! Esto se puede cambiar con un vector  int rmtap_index[rt_nfds]
				for( i = 0; i < nr_rmttap; i++) {
					r_ptr = &rmttap_cfg[i];
					if( r_ptr->rt_rmttap_idx == m_ptr->m3_i1) 
						break;	
				}
				if( i == nr_rmttap) { // index not found 
					ERROR_PRINT(EDVSBADF);
					rcode = EDVSBADF;
				} else {
					//  Send to socket or pipe 
					send_sock(r_ptr->rt_data_fd, &packet, m_ptr->m3_i1, &r_ptr->rt_data_sun);
					rcode = OK;
				}
				break;
			case REQ_RT_CLOSE:
				//  m_ptr->m3_i1 : local TAP index  
				for( i = 0; i < nr_rmttap; i++) {
					r_ptr = &rmttap_cfg[i];
					if( r_ptr->rt_rmttap_idx == m_ptr->m3_i1) {
						r_ptr->rt_rmttap_idx = (-1);
						break;
					}
				}
				if( i == nr_rmttap) {  
					ERROR_PRINT(EDVSBADF);
					rcode = EDVSBADF;
				} else {
					rcode = OK;
				}						
				break;
			default:
				USRDEBUG("Invalid Request: %d\n", m_ptr->m_type);
				ERROR_PRINT(m_ptr->m_type);
				rcode = EDVSBADREQUEST;
				break;
		}
		m_ptr->m_type |=  MASK_ACKNOWLEDGE;
		m_ptr->m3_i1   =  rcode;
		rcode = dvk_send_T( m_ptr->m_source, m_ptr, TIMEOUT_MOLCALL);
		if( rcode < 0){
			ERROR_PRINT(rcode);
			break;
		}
	}

	dvk_unbind(sw_ptr->sw_dcid, sw_ptr->sw_recv_ep);
	pthread_exit(OK);
}

void rmttap_cleanup(void)
{
	int i, rcode, svr_ep, clt_ep; 
	rmttap_t *r_ptr;
	
	for( i = 0; i < nr_rmttap; i++) {
		r_ptr = &rmttap_cfg[i];
		// unbind remote client and server endpoints 
		if( r_ptr->rt_nodeid != local_nodeid) {
			svr_ep = r_ptr->rt_nodeid; 
			rcode = dvk_unbind(sw_ptr->sw_dcid, svr_ep);
			if(rcode) ERROR_PRINT(rcode);
			clt_ep = (dc_ptr->dc_nr_sysprocs - dc_ptr->dc_nr_tasks);
			clt_ep += r_ptr->rt_nodeid; 
			rcode = dvk_unbind(sw_ptr->sw_dcid, clt_ep);
			if(rcode) ERROR_PRINT(rcode);
		}
	}
}

/*===========================================================================*
 *				init_rt_threads				     
 *===========================================================================*/
void init_rt_threads( void)
{
	int rcode;
	pthread_t init_thread;

	rcode = pthread_create( &init_thread, NULL, init_rt_sockets, (void *) NULL);
	if(rcode < 0) ERROR_PRINT(rcode);

	
}

/*===========================================================================*
 *				init_rt_sockets				     
 *===========================================================================*/
void init_rt_sockets( void)
{
	int i, rcode, rmt_fd;
	int svr_ep, clt_ep;
	rmttap_t *r_ptr;
	proc_usr_t *proc_ptr;
	node_usr_t *node_ptr;
	static char my_node[M3_STRING];
	
	USRDEBUG("\n");

	MTX_LOCK(sw_mutex);
	rt_nfds = 0;
	for( i = 0; i < nr_rmttap; i++) {
		r_ptr = &rmttap_cfg[i];

		r_ptr->rt_ctrl_fd		= -1;
		r_ptr->rt_data_fd		= -1;
		r_ptr->rt_poll_idx		= -1;
		r_ptr->rt_rmttap_idx 	= -1;
	
		// If the entry is a local TAP continue 
		if( r_ptr->rt_nodeid == local_nodeid) {
			continue;
		}
        USRDEBUG(RT_FORMAT, RT_FIELDS(r_ptr));

		rmttap_init(r_ptr);
		rmt_fd = connect_to_switch(r_ptr);
		if(rmt_fd < 0 ) {
			ERROR_PRINT(rmt_fd);
			continue;
		}
		
		//Get the status and parameter information about a process in a DC.
		// remote endpoint is equal to remote nodeid 
		proc_ptr = &r_ptr->rt_proc; 
        rcode = dvk_getprocinfo(sw_ptr->sw_dcid, r_ptr->rt_nodeid, proc_ptr);	
		
		if( proc_ptr->p_rts_flags != SLOT_FREE) {
			USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
			continue; //the endpoint is already bound
		} else { 
			// Bind Remote TAP Server endpoint
			sprintf(my_node,"TAPserver%02d",r_ptr->rt_nodeid);
			svr_ep = r_ptr->rt_nodeid; 
			rcode = dvk_rmtbind(sw_ptr->sw_dcid, my_node,
				svr_ep, r_ptr->rt_nodeid);
			if( rcode != svr_ep) {
				ERROR_PRINT(rcode);
				continue;
			}
			//Get the status and parameter information about a process in a DC.
			proc_ptr = &r_ptr->rt_proc; 
			rcode = dvk_getprocinfo(sw_ptr->sw_dcid, svr_ep, proc_ptr);		
			USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
			
			// Bind Remote TAP Client endpoint
			sprintf(my_node,"TAPclient%02d",r_ptr->rt_nodeid);
			clt_ep = (dc_ptr->dc_nr_sysprocs - dc_ptr->dc_nr_tasks);
			clt_ep += r_ptr->rt_nodeid; 
			rcode = dvk_rmtbind(sw_ptr->sw_dcid, my_node,
				clt_ep, r_ptr->rt_nodeid);
			if( rcode != clt_ep) {
				ERROR_PRINT(rcode);
				continue;
			}
			//Get the status and parameter information about a process in a DC.
			proc_ptr = &r_ptr->rt_proc; 
			rcode = dvk_getprocinfo(sw_ptr->sw_dcid, svr_ep, proc_ptr);		
			USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
			
		}

		r_ptr->rt_poll_idx		= rt_nfds;
		rt_fds[rt_nfds].fd 		= rmt_fd;
		rt_fds[rt_nfds].events 	= POLLIN;
		rt_nfds++;
	}	

	USRDEBUG("rt_nfds=%d\n", rt_nfds);

	// Create SENDER, RECEIVER threads 
	rcode = pthread_create( &sw_ptr->sw_send_thread, NULL, send_thread, (void *) NULL);
	if(rcode < 0) ERROR_PRINT(rcode);
	rcode = pthread_create( &sw_ptr->sw_recv_thread, NULL, recv_thread, (void *) NULL);
	if(rcode < 0) ERROR_PRINT(rcode);
	MTX_UNLOCK(sw_mutex);
	pthread_exit(OK);
}

//////////////////////////////////////////////////////////////////////////////////////7
#ifdef ANULADO
//////////////////////////////////////////////////////////////////////////////////////
static void rt_send_packet(int fd, void *packet, int len, void *unused)
{
  int n;

  n = write(fd, packet, len);
  if(n != len){
    if(errno != EAGAIN) perror("send_tap");
  }
}

int open_rmttap(char *dev)
{
  struct ifreq ifr;
  int fd, err;

  if((fd = open("/dev/net/tun", O_RDWR)) < 0){
    perror("Failed to open /dev/net/tun");
    return(-1);
  }
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name) - 1);
  if(ioctl(fd, TUNSETIFF, (void *) &ifr) < 0){
    perror("TUNSETIFF failed");
    close(fd);
    return(-1);
  }
  err = setup_port(fd, send_tap, NULL, 0);
  if(err) return(err);
  return(fd);
}
#endif // ANULADO

