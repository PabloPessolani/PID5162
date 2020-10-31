
#include "switch.h"

static int connect_to_switch(rmttap_t *rt_ptr);

int rmttap_init(rmttap_t *rt_ptr)
{
	int rcode; 
	struct timeval tv;
	struct {
		char zero;
		int pid;
		int usecs;
	} name;
	daemon_data_t *dd_ptr;

	USRDEBUG("rt_name=%s\n", rt_ptr->rt_name);
	
	rt_ptr->rt_dd.fd         = -1;
	rt_ptr->rt_dd.control    = -1;
	rt_ptr->rt_dd.dev        = rt_ptr->rt_name;
	rt_ptr->rt_dd.ctl_addr   = NULL;
	rt_ptr->rt_dd.data_addr  = NULL;
	rt_ptr->rt_dd.local_addr = NULL;
	
	strcpy(rt_ptr->rt_sock_type,"unix");
	rt_ptr->rt_dd.sock_type = rt_ptr->rt_sock_type;

//	sprintf(rt_ptr->rt_ctrl_path,"/tmp/uml%0.2d%0.2d.ctl", sw_ptr->sw_dcid, rt_ptr->rt_index);
	strcpy(rt_ptr->rt_ctrl_path, sw_ptr->sw_ctrl_path);
	USRDEBUG("rt_ptr->rt_ctrl_path=%s\n", rt_ptr->rt_ctrl_path);
//	rcode = remove(rt_ptr->rt_ctrl_path);
//	if (rcode == -1 && errno != ENOENT)
//		ERROR_PRINT(-errno);
	rt_ptr->rt_ctrl_sun.sun_family = AF_UNIX;
	memcpy(rt_ptr->rt_ctrl_sun.sun_path, rt_ptr->rt_ctrl_path, strlen(rt_ptr->rt_ctrl_path)+1);
	rt_ptr->rt_dd.ctl_addr = &rt_ptr->rt_ctrl_sun;
	rt_ptr->rt_dd.ctl_sock = rt_ptr->rt_ctrl_path;
	USRDEBUG("rt_ptr->rt_dd.ctl_sock=%s\n", rt_ptr->rt_dd.ctl_sock);

	name.zero = 0;
	name.pid = getpid();
	gettimeofday(&tv, NULL);
	name.usecs = tv.tv_usec;
	rt_ptr->rt_data_sun.sun_family = AF_UNIX;
	memcpy(rt_ptr->rt_data_sun.sun_path, &name,  sizeof(name));
	rt_ptr->rt_dd.local_addr = &rt_ptr->rt_data_sun;

#ifdef ANULADO
	sprintf(rt_ptr->rt_data_path,"/tmp/uml%0.2d%0.2d.data",sw_ptr->sw_dcid, rt_ptr->rt_index);
	rt_ptr->rt_data_sun.sun_family = AF_UNIX;
	memcpy(rt_ptr->rt_data_sun.sun_path,rt_ptr->rt_data_path,strlen(rt_ptr->rt_data_path)+1);
	rt_ptr->rt_dd.local_addr = &rt_ptr->rt_data_sun;
	rcode = remove(rt_ptr->rt_data_path);
	if (rcode == -1 && errno != ENOENT)
        ERROR_PRINT(-errno);
	USRDEBUG("rt_data_sun.sun_path=%s\n", rt_ptr->rt_data_sun.sun_path);
#endif // ANULADO
	
	rt_ptr->rt_dd.fd = connect_to_switch(rt_ptr);
	dd_ptr = &rt_ptr->rt_dd;
    USRDEBUG(DD_FORMAT, DD_FIELDS(dd_ptr));
	if(rt_ptr->rt_dd.fd < 0) 
		ERROR_RETURN(rt_ptr->rt_dd.fd);
	return(rt_ptr->rt_dd.fd);
} 

int open_lcltap(char *dev)
{
  struct ifreq ifr;
  int fd, err;

  USRDEBUG("dev=%s\n", dev);

  if((fd = open("/dev/net/tun", O_RDWR)) < 0){
    perror("Failed to open /dev/net/tun");
    return(-1);
  }
  
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI; // | IFF_MULTI_QUEUE;
  strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name) - 1);
  if(ioctl(fd, TUNSETIFF, (void *) &ifr) < 0){
  	USRDEBUG("TUNSETIFF failed %s\n", dev);
    perror("TUNSETIFF failed");
	close(fd);
    return(-1);
  }
  USRDEBUG("dev=%s fd=%d\n", dev, fd);

  return(fd);
}
 
static int connect_to_switch(rmttap_t *rt_ptr)
{
	struct sockaddr_un *ctl_addr 	= rt_ptr->rt_dd.ctl_addr;
	struct sockaddr_un *local_addr 	= rt_ptr->rt_dd.local_addr;
	struct sockaddr_un *sun 		= &rt_ptr->rt_data_sun;
	struct request_v3 req, *rq_ptr;
	int fd, n, rcode;
	
	USRDEBUG("rt_name=%s\n", rt_ptr->rt_name);

	rt_ptr->rt_dd.control = socket(AF_UNIX, SOCK_STREAM, 0);
	if (rt_ptr->rt_dd.control < 0) {
		rcode = -errno;
		fprintf(stderr, "connect_to_switch(%s) : control socket failed, "
		       "errno = %d\n", rt_ptr->rt_name,  rcode);
		ERROR_RETURN(rcode);
	}
	
	if (connect(rt_ptr->rt_dd.control, (struct sockaddr *) ctl_addr,
		   sizeof(*ctl_addr)) < 0) {
		rcode = -errno;
		fprintf(stderr,"connect_to_switch(%s) : control connect failed, "
		       "errno = %d\n", rt_ptr->rt_name, -rcode);
		goto out;
	}

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) {
		rcode = -errno;
		fprintf(stderr, "connect_to_switch(%s) : data socket failed, "
		       "errno = %d\n", rt_ptr->rt_name, rcode);
		goto out;
	}
	
	if (bind(fd, (struct sockaddr *) local_addr, sizeof(*local_addr)) < 0) {
		rcode = -errno;
		fprintf(stderr, "connect_to_switch(%s) : data bind failed, "
		       "errno = %d\n", rt_ptr->rt_name, rcode);
		goto out_close;
	}
	
	req.magic 	= SWITCH_MAGIC;
	req.version = SWITCH_VERSION;
	req.type 	= REQ_NEW_CONTROL;
	req.sock 	= *local_addr;
	rq_ptr = &req;
	USRDEBUG("write to switch %s " RQ3_FORMAT, rt_ptr->rt_name, RQ3_FIELDS(rq_ptr));			
	n = write(rt_ptr->rt_dd.control, &req, sizeof(req));
	if (n != sizeof(struct request_v3)) {
		fprintf(stderr, "connect_to_switch(%s) : control setup request "
		       "failed, rcode = %d\n", rt_ptr->rt_name, errno);
		rcode = -ENOTCONN;
		goto out_free;
	}

	USRDEBUG("read from switch %s\n", rt_ptr->rt_name);
	n = read(rt_ptr->rt_dd.control, sun, sizeof(*sun));
	if (n != sizeof(*sun)) {
		fprintf(stderr, "connect_to_switch(%s) : read of data socket failed, "
		       "rcode = %d\n", rt_ptr->rt_name, errno);
		rcode = -ENOTCONN;
		goto out_free;
	}
    USRDEBUG(RT_FORMAT, RT_FIELDS(rt_ptr));	
	rt_ptr->rt_dd.data_addr = sun;
	return fd;

 out_free:
 out_close:
	close(fd);
 out:
	close(rt_ptr->rt_dd.control);
	ERROR_RETURN(rcode);
}

/*===========================================================================*
 *				rt_send_write_packet				     *
 *===========================================================================*/
int rt_send_write_packet(int wtype, rmttap_t *rt_ptr, void *packet, int len)
{
	int rcode, svr_ep;
	message *m_ptr;
	struct packet *pkt_ptr;

	USRDEBUG("rt_name=%s wtype=%d len=%d rt_rmt_idx=%d\n", 
			rt_ptr->rt_name, wtype, len, rt_ptr->rt_rmt_idx);
		
	if( rt_ptr->rt_rmt_idx < 0) {
		ERROR_RETURN(EDVSBADF);
	}
	
	m_ptr 			= &sndr_msg;
	m_ptr->m_type 	= wtype;
	m_ptr->m3_p1	= packet;
	m_ptr->m3_i2	= len;
	
    USRDEBUG(RT_FORMAT, RT_FIELDS(rt_ptr));
	pkt_ptr= (struct packet *) &packet;
	if( wtype == REQ_SW_WRITE){
		svr_ep = rt_ptr->rt_rmt_ep - (dc_ptr->dc_nr_sysprocs - dc_ptr->dc_nr_tasks);	
		USRDEBUG("REQ_SW_WRITE: " PKTDST_FORMAT, PKTDST_FIELDS(pkt_ptr));
		USRDEBUG("REQ_SW_WRITE: " PKTSRC_FORMAT, PKTSRC_FIELDS(pkt_ptr));
	}else{ // REQ_RT_WRITE
		svr_ep = rt_ptr->rt_nodeid;
		USRDEBUG("REQ_RT_WRITE: " PKTDST_FORMAT, PKTDST_FIELDS(pkt_ptr));
		USRDEBUG("REQ_RT_WRITE: " PKTSRC_FORMAT, PKTSRC_FIELDS(pkt_ptr));
	}
	m_ptr->m3_i1 = rt_ptr->rt_rmt_idx;
	USRDEBUG("SEND WRITE PACKET REQUEST: svr_ep=%d " MSG3_FORMAT, svr_ep, MSG3_FIELDS(m_ptr));

	rcode = dvk_sendrec_T(svr_ep, m_ptr, TIMEOUT_MOLCALL);	
	if( rcode < 0) {
		ERROR_RETURN(rcode);
	}
	USRDEBUG("RECEIVED WRITE PACKET REPLY: " MSG3_FORMAT, MSG3_FIELDS(m_ptr));
	if( m_ptr->m_type < 0) {
		ERROR_RETURN(m_ptr->m_type);
	}
	return(m_ptr->m_type);
}

/*===========================================================================*
 *				rt_open_rmttap				     *
 *===========================================================================*/
int rt_open_rmttap(rmttap_t *rt_ptr)
{
	int rcode, svr_ep;
	proc_usr_t *proc_ptr;
	message *m_ptr;
	
    USRDEBUG(RT_FORMAT, RT_FIELDS(rt_ptr));

	proc_ptr = &rt_ptr->rt_svr_proc; 
	svr_ep = rt_ptr->rt_nodeid;
	rcode = dvk_getprocinfo(sw_ptr->sw_dcid, svr_ep, proc_ptr);		
	//Get the status and parameter information about a process in a DC.
	USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
			
	m_ptr = &sndr_msg;
	m_ptr->m_type = REQ_RT_OPEN;
	m_ptr->m3_i1  = rt_ptr->rt_index;
	strncpy(m_ptr->m3_ca1, rt_ptr->rt_tap, M3_STRING-1);
	USRDEBUG("rt_ptr->rt_nodeid=%d\n", rt_ptr->rt_nodeid);
	USRDEBUG("SEND REQ_RT_OPEN REQUEST: " MSG3_FORMAT, MSG3_FIELDS(m_ptr));
	rcode = dvk_sendrec_T(rt_ptr->rt_nodeid, m_ptr, TIMEOUT_MOLCALL);
	if( rcode < 0) {
		ERROR_RETURN(rcode);
	}
	USRDEBUG("RCVD REQ_RT_OPEN REPLY: " MSG3_FORMAT, MSG3_FIELDS(m_ptr));
	if( m_ptr->m_type < 0) {
		ERROR_RETURN(m_ptr->m_type);
	}
	rt_ptr->rt_rmt_ep	 = m_ptr->m_source; 
	rt_ptr->rt_rmt_idx	 = m_ptr->m_type; 
	USRDEBUG("Remote TAP %s successfully opened on endpoint %d with remote index %d\n", 
		rt_ptr->rt_tap, rt_ptr->rt_rmt_ep, rt_ptr->rt_rmt_idx);
	
	return(m_ptr->m_type);
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
	rmttap_t *rt_ptr;

	USRDEBUG("nr_rmttap=%d\n", nr_rmttap);
	
	for( i = 0; i < nr_rmttap; i++) {
		rt_ptr = &rmttap_cfg[i];
		// Only those REMOTE TAP devices with open unix sockets 
		if( rt_ptr->rt_nodeid != local_nodeid){ // it is a remote port 
			USRDEBUG(RT_FORMAT, RT_FIELDS(rt_ptr));
			if( rt_ptr->rt_poll_idx >= 0 ){  
				if( rt_ptr->rt_rmttap_fd < 0) {  // if it is yet opened
					rcode = rt_open_rmttap(rt_ptr);  
					if( rcode < 0) {
						ERROR_PRINT(rcode);
						continue;
					}
					success_open++;
					USRDEBUG("success_open=%d\n", success_open);
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
	int i, j, write_type;
	int rcode, n, timeout; 
	proc_usr_t *proc_ptr;
static struct packet packet;
	struct packet *pkt_ptr;
    int len, socklen;
	rmttap_t *rt_ptr;
	
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
			if( success_open < (nr_rmttap-nr_lcltap))
				try_rmttap_open();
			continue;
		}
		if(n < 0){
			if(errno == EINTR) {
				USRDEBUG("EINTR\n");
				if( success_open < (nr_rmttap-nr_lcltap))
					try_rmttap_open();
				continue;
			}
			ERROR_PRINT(-errno);
			break;
		}
		// Check all monitored FDs 
		for(i = 0; i < rt_nfds; i++){
			if(rt_fds[i].revents == 0) continue;
			// Check against LOCAL and REMOTE TAP devices 
			for(j = 0; j < nr_rmttap; j++){
				rt_ptr = &rmttap_cfg[j];
				if( i == rt_ptr->rt_poll_idx){
					USRDEBUG("%s rt_poll_idx=%d\n",rt_ptr->rt_name ,rt_ptr->rt_poll_idx);
					if( rt_ptr->rt_nodeid != local_nodeid) { //  REMOTE TAPs 
						socklen = sizeof(struct sockaddr_un);
						len = recvfrom(rt_ptr->rt_data_fd, &packet, sizeof(packet), 0, 
							 (struct sockaddr *) &rt_ptr->rt_data_sun, &socklen);
						if(len < 0){
							if(errno != EAGAIN) 
								perror("handle_sock_data");
							ERROR_PRINT(-errno);
							continue;
						}
						write_type = REQ_RT_WRITE;	// Destination is REMOTE TAP   
					} else { // LOCAL  TAP 
						len = read(rt_ptr->rt_rmttap_fd, &packet, sizeof(packet));
						if(len < 0){
							if(errno != EAGAIN) {
								close(rt_ptr->rt_rmttap_fd);
								rt_fds[rt_ptr->rt_poll_idx].fd 		= (-1);
								rt_fds[rt_ptr->rt_poll_idx].events 	= POLLNVAL;		
								rt_ptr->rt_poll_idx 				= (-1);
								rt_ptr->rt_rmttap_fd				= (-1);
								rt_ptr->rt_rmt_ep 					= HARDWARE;
								USRDEBUG(RT_FORMAT, RT_FIELDS(rt_ptr));
								perror("Reading tap data");
							}
							continue;
						}
						// ignore packet if the remote client do not open the LOCAL TAP
						if( rt_ptr->rt_rmt_ep == HARDWARE) continue;
						write_type = REQ_SW_WRITE; // Destination is REMOTE SWITCH  
					}
					// Write PACKET to Remote NODE 
					pkt_ptr= (struct packet *) &packet;
					USRDEBUG(PKTDST_FORMAT, PKTDST_FIELDS(pkt_ptr));
					USRDEBUG(PKTSRC_FORMAT, PKTSRC_FIELDS(pkt_ptr));
		
					rcode = rt_send_write_packet(write_type, rt_ptr, &packet, len);
					if(rcode < 0){
						if( rcode != EDVSBADF){
							// clear the remote index 
							if( rt_ptr->rt_nodeid != local_nodeid) { //  REMOTE TAPs 
								success_open--;
								rt_ptr->rt_poll_idx = (-1);
							}
						}
						if( success_open < (nr_rmttap-nr_lcltap))
							try_rmttap_open();
						ERROR_PRINT(rcode);
					}
				}
			} 
		}			
	}

	dvk_unbind(sw_ptr->sw_dcid, sw_ptr->sw_send_ep);
	pthread_exit(OK);
}

void add_rt_fd(int fd)
{
	struct pollfd *p;
	USRDEBUG("LTAP fd=%d\n",fd);

	rt_ptr->rt_poll_idx		= rt_nfds;
	rt_fds[rt_nfds].fd 		= fd;
	rt_fds[rt_nfds].events 	= POLLIN;
	rt_nfds++;
	USRDEBUG("LTAP rt_nfds=%d\n",rt_nfds);
}

/*===========================================================================*
 *				recv_thread			     
 *===========================================================================*/	
void *recv_thread(void *arg)
{
	int rcode, i,tap_fd, len, rmt_idx,lcl_idx ; 
	proc_usr_t *proc_ptr;
	static struct packet packet, *pkt_ptr;
	message *m_ptr;
	rmttap_t *rt_ptr;
	sock_data_t data;

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
		USRDEBUG("RECEIVED REQUEST: " MSG3_FORMAT, MSG3_FIELDS(m_ptr));
		switch( m_ptr->m_type){
			case REQ_RT_OPEN:
				USRDEBUG("RECEIVED REQ_RT_OPEN REQUEST\n");
				//  m_ptr->m3_ca1 : local TAP name 
				for( i = 0; i < nr_rmttap; i++) {
					rt_ptr = &rmttap_cfg[i];
					if( rt_ptr->rt_nodeid == local_nodeid){
						if( !strncmp(rt_ptr->rt_tap, m_ptr->m3_ca1, M3_STRING-1)) 
							break;	
					}
				}
				if( i == nr_rmttap) { // TAP device not found 
					ERROR_PRINT(EDVSNOENT);
					rcode = EDVSNOENT;
				} else {
					if( rt_ptr->rt_rmt_ep == HARDWARE){
						rt_ptr->rt_rmttap_fd = open_lcltap(rt_ptr->rt_tap);
						if( rt_ptr->rt_rmttap_fd < 0) {
							rcode = (-errno);
							ERROR_PRINT(rcode);
							break;
						}
						add_rt_fd(rt_ptr->rt_rmttap_fd);
					} else { // TAP Device is already OPENED 
						ERROR_PRINT(EDVSBUSY);
						rcode = EDVSBUSY;		
						break;
					}
					// associate and remote endpoint to the LOCAL TAP 
					rt_ptr->rt_rmt_ep  = m_ptr->m_source;
					rt_ptr->rt_rmt_idx = m_ptr->m3_i1;
					USRDEBUG("REQ_RT_OPEN: " RT_FORMAT, RT_FIELDS(rt_ptr));
					rcode = rt_ptr->rt_index;
				}						
				break;
			case REQ_RT_WRITE: // the remote process want to write into a local TAP 
				USRDEBUG("RECEIVED REQ_RT_WRITE REQUEST\n");
				// get the remote packet 
				// m_ptr->m3_p1	= packet;
				// m_ptr->m3_i1	= rt_rmt_idx;
				// m_ptr->m3_i2	= len;
				lcl_idx = m_ptr->m3_i1;
				len 	= m_ptr->m3_i2;
				// search the port with the index  ATENCION!! Esto se puede cambiar con un vector  int rmtap_index[rt_nfds]
				if( lcl_idx < 0 || lcl_idx >= nr_rmttap){
					ERROR_PRINT(EDVSNODEV);
					rcode = EDVSNODEV;
					break;
				}			
				rt_ptr = &rmttap_cfg[lcl_idx];
				USRDEBUG(RT_FORMAT, RT_FIELDS(rt_ptr));
				if( rt_ptr->rt_nodeid != local_nodeid){
					ERROR_PRINT(EDVSBADNODEID);
					rcode = EDVSBADNODEID;	
					break;
				}
				if( rt_ptr->rt_rmt_ep != m_ptr->m_source) {
					ERROR_PRINT(EDVSBADOWNER);
					rcode = EDVSBADOWNER;	
					break;
				} 
				rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m3_p1, 
								sw_ptr->sw_recv_ep, &packet, len);
				if( rcode < 0){
					ERROR_PRINT(rcode);
					break;
				}
				tap_fd = rt_ptr->rt_rmttap_fd;
				pkt_ptr= &packet;
				USRDEBUG("REQ_RT_WRITE: " PKTDST_FORMAT, PKTDST_FIELDS(pkt_ptr));
				USRDEBUG("REQ_RT_WRITE: " PKTSRC_FORMAT, PKTSRC_FIELDS(pkt_ptr));
				send_tap(tap_fd, &packet, len, NULL);
				rcode = OK;
				break;
			case REQ_RT_CLOSE:
				USRDEBUG("RECEIVED REQ_RT_CLOSE REQUEST\n");
				if( lcl_idx < 0 || lcl_idx >= nr_rmttap){
					ERROR_PRINT(EDVSNODEV);
					rcode = EDVSNODEV;
					break;
				}			
				rt_ptr = &rmttap_cfg[lcl_idx];
				USRDEBUG(RT_FORMAT, RT_FIELDS(rt_ptr));
				if( rt_ptr->rt_nodeid != local_nodeid){
					ERROR_PRINT(EDVSBADNODEID);
					rcode = EDVSBADNODEID;	
					break;
				}
				if( rt_ptr->rt_rmt_ep != m_ptr->m_source) {
					ERROR_PRINT(EDVSBADOWNER);
					rcode = EDVSBADOWNER;	
					break;
				} 
				rt_ptr->rt_rmttap_fd = (-1);
				rt_ptr->rt_rmt_ep    = HARDWARE;
				rcode 				 = OK;						
				break;
			case REQ_SW_WRITE: // the remote TAP device request to write to switch's local  virtual port 	
				lcl_idx = m_ptr->m3_i1;
				len 	= m_ptr->m3_i2;
				USRDEBUG("RECEIVED REQ_SW_WRITE REQUEST lcl_idx=%d len=%d\n",lcl_idx, len);
				// search the port with this socket FD
				if( lcl_idx < 0 || lcl_idx >= nr_rmttap){
					ERROR_PRINT(EDVSNODEV);
					rcode = EDVSNODEV;
					break;
				}			
				rt_ptr = &rmttap_cfg[lcl_idx];
				USRDEBUG(RT_FORMAT, RT_FIELDS(rt_ptr));
				if( rt_ptr->rt_nodeid == local_nodeid){
					ERROR_PRINT(EDVSBADNODEID);
					rcode = EDVSBADNODEID;	
					break;
				}
				rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m3_p1, 
								sw_ptr->sw_recv_ep, &packet, len);
				if( rcode < 0){
					ERROR_PRINT(rcode);
					break;
				}
				
				pkt_ptr= &packet;
				USRDEBUG("REQ_SW_WRITE: " PKTDST_FORMAT, PKTDST_FIELDS(pkt_ptr));
				USRDEBUG("REQ_SW_WRITE: " PKTSRC_FORMAT, PKTSRC_FIELDS(pkt_ptr));
				data.fd = rt_ptr->rt_dd.fd;
				memcpy( &data.sock, rt_ptr->rt_dd.data_addr, sizeof(struct sockaddr_un)); 
				send_sock(data.fd, &packet, len, (void*) &data);	
				
//				send_sock(rt_ptr->rt_data_fd, &packet, len, (void*) &data);
//				send_sock(rt_ptr->rt_ctrl_fd, &packet, len, (void*) &data);
				rcode = OK;
				break;
			default:
				USRDEBUG("Invalid Request: %d\n", m_ptr->m_type);
				ERROR_PRINT(m_ptr->m_type);
				rcode = EDVSBADREQUEST;
				break;
		}
		m_ptr->m_type  =  rcode;
		USRDEBUG("SEND REPLY: " MSG3_FORMAT, MSG3_FIELDS(m_ptr));
		rcode = dvk_send_T( m_ptr->m_source, m_ptr, TIMEOUT_MOLCALL);
		if( rcode < 0){
			ERROR_PRINT(rcode);
			continue;
		}
	}

	dvk_unbind(sw_ptr->sw_dcid, sw_ptr->sw_recv_ep);
	pthread_exit(OK);
}

void rmttap_cleanup(void)
{
	int i, rcode, svr_ep, clt_ep; 
	rmttap_t *rt_ptr;
	
	USRDEBUG("\n");
		// bind remote Client and Server endpoint getting information from DC 
	for( i = 0; i < (sizeof(dc_ptr->dc_nodes) * DVK_CHAR_BITS); i++){
		if( i == local_nodeid) continue;
		if( TEST_BIT(dc_ptr->dc_nodes, i) ){
			svr_ep = i;
			USRDEBUG("dvk_unbind dcid=%d svr_ep=%d nodeid=%d\n", sw_ptr->sw_dcid, svr_ep, i);
			rcode = dvk_unbind(sw_ptr->sw_dcid, svr_ep);
			if(rcode) ERROR_PRINT(rcode);			
			clt_ep = (dc_ptr->dc_nr_sysprocs - dc_ptr->dc_nr_tasks);
			clt_ep += i; 
			USRDEBUG("dvk_unbind dcid=%d clt_ep=%d nodeid=%d\n\n", sw_ptr->sw_dcid, clt_ep, i);			
			rcode = dvk_unbind(sw_ptr->sw_dcid, clt_ep);
			if(rcode) ERROR_PRINT(rcode);
		}
	}

	for( i = 0; i < nr_rmttap; i++) {
		rt_ptr = &rmttap_cfg[i];
		if( rt_ptr->rt_rmttap_fd != (-1)){
			USRDEBUG("close %s fd=%d\n", rt_ptr->rt_name, rt_ptr->rt_rmttap_fd);			
			close(rt_ptr->rt_rmttap_fd);
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
	int i, rcode, rmt_fd, lcl_fd;
	int svr_ep, clt_ep;
	rmttap_t *rt_ptr;
	proc_usr_t *proc_ptr, proc_usr;
	node_usr_t *node_ptr;
	static char node_name[M3_STRING];
	
	USRDEBUG("\n");
	MTX_LOCK(sw_mutex);
	if( nr_rmttap == 0) {
		MTX_UNLOCK(sw_mutex);
		pthread_exit(OK);	
	}
	
	// bind remote Client and Server endpoint getting information from DC 
	for( i = 0; i < (sizeof(dc_ptr->dc_nodes) * DVK_CHAR_BITS); i++){
		if( i == local_nodeid) continue;
		if( TEST_BIT(dc_ptr->dc_nodes, i) ){
			//Get the status and parameter information about a process in a DC.
			proc_ptr = &proc_usr;	
			// remote endpoint is equal to remote nodeid 
			svr_ep = i;
			rcode = dvk_getprocinfo(sw_ptr->sw_dcid, svr_ep, proc_ptr);	
			if( proc_ptr->p_rts_flags == SLOT_FREE) {
				// Bind Remote TAP Server endpoint
				sprintf(node_name,"TAPserver%02d", i);
				rcode = dvk_rmtbind(sw_ptr->sw_dcid, node_name,
					svr_ep, i);
				if( rcode != svr_ep) {
					ERROR_PRINT(rcode);
					continue;
				}
				//Get the status and parameter information about a process in a DC.
				rcode = dvk_getprocinfo(sw_ptr->sw_dcid, svr_ep, proc_ptr);	
			}
			USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
				
			// Bind Remote TAP Client endpoint
			clt_ep = (dc_ptr->dc_nr_sysprocs - dc_ptr->dc_nr_tasks);
			clt_ep += i; 
			proc_ptr = &proc_usr; 
			rcode = dvk_getprocinfo(sw_ptr->sw_dcid, clt_ep, proc_ptr);	
			if( proc_ptr->p_rts_flags == SLOT_FREE) {	
				sprintf(node_name,"TAPclient%02d", i);
				rcode = dvk_rmtbind(sw_ptr->sw_dcid, node_name,
					clt_ep, i);
				if( rcode != clt_ep) {
					ERROR_PRINT(rcode);
					continue;
				}
				//Get the status and parameter information about a process in a DC.
				rcode = dvk_getprocinfo(sw_ptr->sw_dcid, clt_ep, proc_ptr);
			}
			USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
		}
	}
	
	rt_nfds = 0;
	for( i = 0; i < nr_rmttap; i++) {
		rt_ptr = &rmttap_cfg[i];

		rt_ptr->rt_ctrl_fd		= -1;
		rt_ptr->rt_data_fd		= -1;
		rt_ptr->rt_poll_idx		= -1;
		rt_ptr->rt_rmttap_fd 	= -1;
		rt_ptr->rt_rmt_idx	 	= -1;
		rt_ptr->rt_rmt_ep 		= HARDWARE;
	
		// If the entry is a LOCAL TAP  
		if( rt_ptr->rt_nodeid == local_nodeid) {
		    nr_lcltap++; 
			USRDEBUG(RT_FORMAT, RT_FIELDS(rt_ptr));
			continue;
		}
		
		// Entries for REMOTE  TAPs
        USRDEBUG(RT_FORMAT, RT_FIELDS(rt_ptr));
		rcode = rmttap_init(rt_ptr);
		if(rcode < 0 ) {
			ERROR_PRINT(rcode);
			continue;
		}
		rt_ptr->rt_data_fd = rt_ptr->rt_dd.fd;
        USRDEBUG(RT_FORMAT, RT_FIELDS(rt_ptr));
		
		// get remote server process descriptor 
		proc_ptr = &rt_ptr->rt_svr_proc;	
		svr_ep = rt_ptr->rt_nodeid;
		rcode = dvk_getprocinfo(sw_ptr->sw_dcid, svr_ep, proc_ptr);	

		// get remote client process descriptor 
		proc_ptr = &rt_ptr->rt_clt_proc;	
		clt_ep = (dc_ptr->dc_nr_sysprocs - dc_ptr->dc_nr_tasks);
		clt_ep += rt_ptr->rt_nodeid;
		rcode = dvk_getprocinfo(sw_ptr->sw_dcid, svr_ep, proc_ptr);
		
		rt_ptr->rt_poll_idx		= rt_nfds;
		USRDEBUG(RT_FORMAT, RT_FIELDS(rt_ptr));

		rt_fds[rt_nfds].fd 		= rmt_fd;
		rt_fds[rt_nfds].events 	= POLLIN;
		rt_nfds++;
	}	

	USRDEBUG("rt_nfds=%d nr_lcltap=%d\n", rt_nfds, nr_lcltap);
	// Create SENDER, RECEIVER threads 
	rcode = pthread_create( &sw_ptr->sw_send_thread, NULL, send_thread, (void *) NULL);
	if(rcode < 0) ERROR_PRINT(rcode);
	rcode = pthread_create( &sw_ptr->sw_recv_thread, NULL, recv_thread, (void *) NULL);
	if(rcode < 0) ERROR_PRINT(rcode);

	MTX_UNLOCK(sw_mutex);
	pthread_exit(OK);
}

