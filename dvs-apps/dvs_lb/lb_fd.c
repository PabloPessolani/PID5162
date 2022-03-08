///////////////////////////////////////////////////////////////////////
//				FAILURE DETECTORS
// https://github.com/hayderimran7/advanced-socket-programming/blob/master/ping.c
///////////////////////////////////////////////////////////////////////

#define _GNU_SOURCE     
#define _MULTI_THREADED
#include "lb_dvs.h"

#define MAX_WAIT_TIME   5
#define MAX_NO_PACKETS  3


// EDVSRESTARTSYS	(_SIGN 512)
#define 	EFD_BADLEN 		(EDVSRESTARTSYS -2) // -514
#define 	EFD_BADTYPE 	(EDVSRESTARTSYS -3) // -515
#define 	EFD_BADPID 		(EDVSRESTARTSYS -4) // -516
#define 	EFD_BADSEQ 		(EDVSRESTARTSYS -5) // -517
#define 	EFD_LOWERSEQ 	(EDVSRESTARTSYS -6) // -518
#define		EFD_LARGESEQ 	(EDVSRESTARTSYS -7) // -519
#define		EFD_UNREACH		(EDVSRESTARTSYS -8) // -520

int sockfd, datalen = 56;
int nsend = 0, nreceived = 0;
struct sockaddr_in dest_addr;

struct timeval tvrecv;
void statistics(int signo);

unsigned short cal_chksum(unsigned short *addr, int len);
void send_packet(void);
void recv_packet(void);
void tv_sub(struct timeval *out, struct timeval *in);

#ifdef ANULADO 
void flush_socket_buffer(server_t *svr_ptr)
{
	int cero; 
	
	free(svr_ptr->svr_recvpkt);
	svr_ptr->svr_recvpkt = malloc(PING_PACKET_SIZE); 
	if( svr_ptr->svr_recvpkt == NULL) ERROR_EXIT(-errno);
	
	// Flush the receiver buffer: first set buffer size to 0, then set buffer size to 4096
//	cero = 0;
//    setsockopt(svr_ptr->svr_icmp_fd, SOL_SOCKET, SO_RCVBUF, &cero, sizeof(cero));
//    setsockopt(svr_ptr->svr_icmp_fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));	
}
#endif // ANULADO 

void tv_sub(struct timeval *out, struct timeval *in)
{
    if ((out->tv_usec -= in->tv_usec) < 0)
    {
        --out->tv_sec;
        out->tv_usec += 1000000;
    } out->tv_sec -= in->tv_sec;
}

unsigned short cal_chksum(unsigned short *addr, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = addr;
    unsigned short answer = 0;

    while (nleft > 1)
    {
        sum +=  *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        *(unsigned char*)(&answer) = *(unsigned char*)w;
        sum += answer;
    }
    sum = (sum >> 16) + (sum &0xffff);
    sum += (sum >> 16);
    answer = ~sum;
    return answer;
}

int pack(server_t *svr_ptr)
{
    int i, packsize;
    struct icmp *icmp;
    struct timeval *tval;

	USRDEBUG("Server:%s svr_icmp_seq=%d\n", svr_ptr->svr_name, svr_ptr->svr_icmp_sent);

    icmp = (struct icmp*)svr_ptr->svr_sendpkt;
    icmp->icmp_type = ICMP_ECHO;
    icmp->icmp_code = 0;
    icmp->icmp_cksum = 0;
    icmp->icmp_seq = svr_ptr->svr_icmp_sent;
    icmp->icmp_id = lb_ptr->lb_pid;
    packsize = 8+datalen;
    tval = (struct timeval*)icmp->icmp_data;
    gettimeofday(tval, NULL); 
    icmp->icmp_cksum = cal_chksum((unsigned short*)icmp, packsize); 
    return packsize;
}

int unpack(server_t *svr_ptr, char *buf, int len)
{
    int i, iphdrlen;
    struct ip *ip;
    struct icmp *icmp;
    struct timeval *tvsend;
    double rtt;

	USRDEBUG("Server %s\n", svr_ptr->svr_name);

    ip = (struct ip*)buf;
    iphdrlen = ip->ip_hl << 2; 
    icmp = (struct icmp*)(buf + iphdrlen);
    len -= iphdrlen; 
    if (len < 8)
    {
        printf("ICMP packets\'s length is less than 8\n");
        ERROR_RETURN(EFD_BADLEN);
    } 

	USRDEBUG("Server %s: len=%d icmp_type=%X icmp_code=%X icmp_id=%d lb_pid=%d\n", 
		svr_ptr->svr_name, len, icmp->icmp_type, icmp->icmp_code, icmp->icmp_id, lb_ptr->lb_pid);
    if (icmp->icmp_type == ICMP_ECHOREPLY) {
		if (icmp->icmp_id == lb_ptr->lb_pid) {
			tvsend = (struct timeval*)icmp->icmp_data;
			tv_sub(&tvrecv, tvsend); 
			rtt = tvrecv.tv_sec * 1000+tvrecv.tv_usec / 1000;
			USRDEBUG("%d byte from %s: icmp_seq=%u ttl=%d rtt=%.3f ms\n", len,
				inet_ntoa(svr_ptr->svr_from.sin_addr), icmp->icmp_seq, ip->ip_ttl, rtt);
			// store the Timestamp of the last ECHO REPLY 
			memcpy((void *)&svr_ptr->svr_icmp_ts, (void *) icmp->icmp_data, sizeof(struct timeval));
			if(icmp->icmp_seq < 0) ERROR_PRINT(EFD_BADSEQ);
			return(icmp->icmp_seq);
		} else{
			ERROR_RETURN(EFD_BADPID);
		}
	}else if (icmp->icmp_type == ICMP_DEST_UNREACH) {
		ERROR_RETURN(EFD_UNREACH);				
	}
	ERROR_RETURN(EFD_BADTYPE);		
}


int init_node_FD(server_t *svr_ptr)
{
    struct hostent *host;
    struct protoent *protocol;
    unsigned long inaddr = 0l;
	struct timeval tout;
	int bufsize = PING_PACKET_SIZE;
	
	// DO NOT WAIT 
	tout.tv_sec = 0;
	tout.tv_usec = 1;
	
	USRDEBUG("Server %s\n", svr_ptr->svr_name);
	if ((protocol = getprotobyname("icmp")) == NULL) {
        ERROR_EXIT(EDVSNOPROTOOPT);
    }
	
#ifdef ANULADO 	
	svr_ptr->svr_sendpkt = malloc(PING_PACKET_SIZE);
	if( svr_ptr->svr_sendpkt == NULL){
		ERROR_EXIT(-errno);
	}
	svr_ptr->svr_recvpkt = malloc(PING_PACKET_SIZE); 
	if( svr_ptr->svr_recvpkt == NULL) {
		ERROR_EXIT(-errno);
	}
#endif // ANULADO 	
	
    if ((svr_ptr->svr_icmp_fd = socket(AF_INET, SOCK_RAW, protocol->p_proto)) < 0){
        ERROR_EXIT(EDVSNOTSOCK);
    }
	
    setuid(getuid());
    setsockopt(svr_ptr->svr_icmp_fd, SOL_SOCKET, SO_RCVTIMEO, &tout, sizeof(struct timeval ));
    setsockopt(svr_ptr->svr_icmp_fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
    bzero(&svr_ptr->svr_dstaddr, sizeof(struct sockaddr_in));
    svr_ptr->svr_dstaddr.sin_family = AF_INET;

    if (inaddr = inet_addr(svr_ptr->svr_name) == INADDR_NONE) {
        if( (host = gethostbyname(svr_ptr->svr_name)) == NULL) {
			ERROR_EXIT(EDVSADDRNOTAVAIL);
        }
        memcpy((char*) &svr_ptr->svr_dstaddr.sin_addr, host->h_addr, host->h_length);
    } else {
		svr_ptr->svr_dstaddr.sin_addr.s_addr = inet_addr(svr_ptr->svr_name);
	}	
	
	svr_ptr->svr_icmp_seq = LB_INVALID;
}

void send_packet_FD(server_t *svr_ptr)
{
    int packetsize;
	
	USRDEBUG("Server %s\n", svr_ptr->svr_name);

    svr_ptr->svr_icmp_sent	= svr_ptr->svr_icmp_rcvd + 1;
    svr_ptr->svr_icmp_seq 	= svr_ptr->svr_icmp_sent;
    packetsize = pack(svr_ptr); 

//	flush_socket_buffer(svr_ptr);

	USRDEBUG("Server %s: packetsize=%d svr_dstaddr.sin_addr.s_addr=%X svr_icmp_seq=%d\n", 
		svr_ptr->svr_name, packetsize, svr_ptr->svr_dstaddr.sin_addr.s_addr, svr_ptr->svr_icmp_seq);
    if (sendto(svr_ptr->svr_icmp_fd, svr_ptr->svr_sendpkt, packetsize, 0, (struct sockaddr*)
          &svr_ptr->svr_dstaddr, sizeof(svr_ptr->svr_dstaddr)) < 0) {
			ERROR_PRINT(-errno);
    }
}

int recv_packet_FD(server_t *svr_ptr)
{
    int n, fromlen, seq, packsize, rcode, retries;
    extern int errno;

    fromlen = sizeof(svr_ptr->svr_from);
    packsize = 8+datalen;
	rcode = 0;

	if ((n = recvfrom(svr_ptr->svr_icmp_fd, svr_ptr->svr_recvpkt, PING_PACKET_SIZE, 0, (struct
		sockaddr*) &svr_ptr->svr_from, &fromlen)) >=  0) {
		gettimeofday(&tvrecv, NULL); 
		USRDEBUG("Server %s: recvfrom n=%d tv_sec=%ld\n", svr_ptr->svr_name ,n, tvrecv.tv_sec);
		seq = unpack(svr_ptr, svr_ptr->svr_recvpkt, n);
		USRDEBUG("Server %s: seq=%d \n", svr_ptr->svr_name, seq);
		if( seq > 0) {
			USRDEBUG("Server %s: seq=%d svr_icmp_rcvd+1=%d\n", 
				svr_ptr->svr_name,seq, svr_ptr->svr_icmp_rcvd+1 );
			if( seq < svr_ptr->svr_icmp_rcvd+1){ // sequence do not macht
				ERROR_RETURN(EFD_LOWERSEQ);
			}else if( seq > svr_ptr->svr_icmp_rcvd+1){
//				flush_socket_buffer(svr_ptr);
				ERROR_RETURN(EFD_LARGESEQ);
			}	
			return(seq);
		}
		ERROR_RETURN(seq);
	} 
	ERROR_RETURN(-errno);
}

int lb_echo_request(int n)
{
	server_t *svr_ptr;
    struct timeval tval;

    gettimeofday(&tval, NULL); 
	svr_ptr = &server_tab[n];
	USRDEBUG("Server %s (%d): tv_sec=%ld\n", svr_ptr->svr_name, n, tval.tv_sec);
	
	MTX_LOCK(lb_ptr->lb_mtx);
	MTX_LOCK(svr_ptr->svr_mutex);
//	if(svr_ptr->svr_icmp_retry > 0){
		// Send an ICMP ECHO REQUEST packet for FAILURE DETECTION
		send_packet_FD(svr_ptr);
//	}
	MTX_UNLOCK(svr_ptr->svr_mutex);
	MTX_UNLOCK(lb_ptr->lb_mtx);

	return(OK);
}

int lb_echo_reply(server_t *svr_ptr)
{
	int seq, retries;
    struct timeval tval;

    gettimeofday(&tval, NULL); 
	USRDEBUG("Server %s(%d): INPUT lb_bm_active=%0lX lb_bm_suspect=%0lX svr_icmp_retry=%d tv_sec=%ld\n", 
			svr_ptr->svr_name, svr_ptr->svr_nodeid, 
			lb_ptr->lb_bm_active, lb_ptr->lb_bm_suspect, 
			svr_ptr->svr_icmp_retry,  tval.tv_sec);
			
	MTX_LOCK(lb_ptr->lb_mtx);
	MTX_LOCK(svr_ptr->svr_mutex);
//	retries = FD_MAXRETRIES;
//	while( retries > 0){
		seq = recv_packet_FD(svr_ptr);
//		if( seq != EDVSAGAIN) break;
//		retries--;
//	}
		
	if( seq >= 0) {
		//////////////////////////////////////////////////////////
		//   ESTA VIVO,  SI ERA SOSPECHOSO DEJA DE SERLO 
		//////////////////////////////////////////////////////////	
		USRDEBUG("Server %s(%d): CONNECTED \n", svr_ptr->svr_name, svr_ptr->svr_nodeid);
		SET_BIT(lb_ptr->lb_bm_active, svr_ptr->svr_nodeid); 				
		CLR_BIT(lb_ptr->lb_bm_suspect, svr_ptr->svr_nodeid);
		svr_ptr->svr_icmp_seq = LB_INVALID;
		svr_ptr->svr_icmp_retry = FD_MAXRETRIES;
		svr_ptr->svr_icmp_rcvd++;		
	}else{
		/////////////////////////////////////////////////////////////
		//			AQUI COMIENZA A SER SOSPECHOSO 
		////////////////////////////////////////////////////////////
		if( seq == EFD_LARGESEQ) 
			svr_ptr->svr_icmp_seq = LB_INVALID;
		if(  TEST_BIT(lb_ptr->lb_bm_active, svr_ptr->svr_nodeid) != 0) {			// ACTIVE ??
			if( TEST_BIT(lb_ptr->lb_bm_suspect, svr_ptr->svr_nodeid) != 0) {		// SUSPECTED??
				if ( svr_ptr->svr_icmp_retry <= 0) { 
					// FAULTY NODE !!!!!
					USRDEBUG("Server %s(%d): FAULTY \n", svr_ptr->svr_name, svr_ptr->svr_nodeid);
					CLR_BIT(lb_ptr->lb_bm_active, svr_ptr->svr_nodeid); 	
					CLR_BIT(lb_ptr->lb_bm_init, svr_ptr->svr_nodeid);
					CLR_BIT(lb_ptr->lb_bm_suspect, svr_ptr->svr_nodeid); 
					svr_ptr->svr_icmp_retry 	= FD_MAXRETRIES;
					CLR_BIT(lb_ptr->lb_bm_echo,  svr_ptr->svr_nodeid);
					//////////////////////////////////////////////
					// HABRIA Q NOTIFICAR AL PROXY SI ES QUE NO LO DETECTO
					//////////////////////////////////////////////
				}else{															// SUSPECTED SEVERAL TIMES
//					if( seq != EFD_UNREACH) 		
						svr_ptr->svr_icmp_retry--;
				}
			} else {															// SUSPECTED FIRST TIME 
				// First chance 
				USRDEBUG("Server %s(%d): SUSPECTED \n", svr_ptr->svr_name, svr_ptr->svr_nodeid);
				SET_BIT(lb_ptr->lb_bm_suspect, svr_ptr->svr_nodeid);
//				if( seq != EFD_UNREACH) 		
					svr_ptr->svr_icmp_retry--;
			}
		}
	}
	MTX_UNLOCK(svr_ptr->svr_mutex);
	MTX_UNLOCK(lb_ptr->lb_mtx);

    gettimeofday(&tval, NULL); 
	USRDEBUG("Server %s(%d): OUTPUT lb_bm_active=%0lX lb_bm_suspect=%0lX svr_icmp_retry=%d tv_sec=%ld\n", 
			svr_ptr->svr_name, svr_ptr->svr_nodeid, 
			lb_ptr->lb_bm_active, lb_ptr->lb_bm_suspect, 
			svr_ptr->svr_icmp_retry,  tval.tv_sec);
	if( seq < 0 ) ERROR_RETURN(seq);
	return(seq);
}

/*===========================================================================*
 *				   lb_fd_monitor				    					 *
 *===========================================================================*/
void *lb_fd_monitor(void *arg)
{
	int rcode, n, i;
    lb_t *lb_ptr;
	server_t *svr_ptr;
	
	lb_ptr = &lb;
	
	USRDEBUG("LB FAILURE DETECTOR MONITOR : Initializing... lb_bm_svrpxy=%0lX\n", lb_ptr->lb_bm_svrpxy);

	while(TRUE) {
		
		for(i =0 ;  i < lb_ptr->lb_period ; i++) {
			
			// CHECKS WITH ICMP ECHO REQUEST ALL CONFIGURED NODES 
			for( n = 0; n < dvs_ptr->d_nr_nodes ; n++){
				
				if( (TEST_BIT(lb_ptr->lb_bm_svrpxy, n) == 0)	// Only compute configured nodes   
				||  ( n == lb_ptr->lb_nodeid)) {
					continue; 				
				}
				
				if( lb_ptr->lb_period >= dvs_ptr->d_nr_nodes){
					if( i != n) continue;
					USRDEBUG("CHECK i=%d n=%d lb_bm_active=%0lX\n", i, n, lb_ptr->lb_bm_active);
				}else{ 
					USRDEBUG("n=%d lb_period=%d i=%d remainder=%d\n", 
						n, lb_ptr->lb_bm_active, i, ((n)%(lb_ptr->lb_period)));
						
					if( ((n)%(lb_ptr->lb_period)) != i) continue;						
					USRDEBUG("CHECK n=%d lb_period=%d i=%d remainder=%d\n", 
						n, lb_ptr->lb_bm_active, i, ((n)%(lb_ptr->lb_period)));
				}
				
				rcode = lb_echo_request(n);
				
				SET_BIT(lb_ptr->lb_bm_echo, n);
			}
		
			// CHECK SUSPECTED NODES 
			if( lb_ptr->lb_bm_suspect != 0){ 
				for(n = 0; n < dvs_ptr->d_nr_nodes;  n++){
					// must be suspected but active 
					if(TEST_BIT(lb_ptr->lb_bm_active, n) == 0){
						continue;
					}
					// otrer node than LB 
					if( n == lb_ptr->lb_nodeid) 				continue;
				//	if( n == lb_ptr->lb_node_test) 				continue;
					// must be suspected 
					if(TEST_BIT(lb_ptr->lb_bm_suspect, n) == 0)	continue;
					// must not be tested in this cycle 
					if(TEST_BIT(lb_ptr->lb_bm_echo, n) 	  != 0)	continue;
					
					USRDEBUG("CHECK SUSPECTED n=%d\n",n); 
				//	svr_ptr->svr_icmp_seq = LB_INVALID;
					rcode = lb_echo_request(n);
					SET_BIT(lb_ptr->lb_bm_echo, n);
				}
			}
			sleep(1);						
			if( lb_ptr->lb_bm_echo != 0) {// there is something to test 
				for(n = 0; n < dvs_ptr->d_nr_nodes;  n++){
					if(TEST_BIT(lb_ptr->lb_bm_echo, n) == 0)	continue;
					if( n == lb_ptr->lb_nodeid) 				continue;
					svr_ptr = &server_tab[n];
					rcode = lb_echo_reply(svr_ptr);
					if(rcode >= 0) 
						CLR_BIT(lb_ptr->lb_bm_echo, n);
				}
			}
		}
	}
	
	USRDEBUG("LB FAILURE DETECTOR MONITOR: Exiting...\n");
    pthread_exit(NULL);
}	



#ifdef ANULADO

/*===========================================================================*
 *				   lb_fd_receiver			    					 *
 *===========================================================================*/
void *lb_fd_receiver(void *arg)
{
	int rcode, n;
    lb_t *lb_ptr;
	server_t *svr_ptr;
	
	lb_ptr = &lb;
	
	USRDEBUG("LB FAILURE DETECTOR RECEIVER: Initializing... lb_bm_active=%0lX\n", lb_ptr->lb_bm_active);
	
	while(TRUE) {
		MTX_LOCK(lb_ptr->lb_fd_mtx);
		COND_WAIT(lb_ptr->lb_fd_rcond, lb_ptr->lb_fd_mtx);
		
		MTX_LOCK(lb_ptr->lb_mtx);
		for(n = 0; n < dvs_ptr->d_nr_nodes;  n++){
			if(TEST_BIT(lb_ptr->lb_bm_echo, n) == 0)	continue;
			if( n == lb_ptr->lb_nodeid) 				continue;
			svr_ptr = &server_tab[n];
			rcode = lb_echo_reply(svr_ptr);	
			CLR_BIT(lb_ptr->lb_bm_echo, n);
		}
		MTX_UNLOCK(lb_ptr->lb_mtx);
		
		COND_SIGNAL(lb_ptr->lb_fd_scond);
		MTX_UNLOCK(lb_ptr->lb_fd_mtx);
	}
	
	USRDEBUG("LB FAILURE DETECTOR RECEIVER: Exiting...\n");
    pthread_exit(NULL);
}	

#endif // ANULADO
