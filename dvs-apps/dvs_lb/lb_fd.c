///////////////////////////////////////////////////////////////////////
//				FAILURE DETECTORS
// https://github.com/hayderimran7/advanced-socket-programming/blob/master/ping.c
///////////////////////////////////////////////////////////////////////

#define _GNU_SOURCE     
#define _MULTI_THREADED
#include "lb_dvs.h"

#define PACKET_SIZE     4096
#define MAX_WAIT_TIME   5
#define MAX_NO_PACKETS  3

char sendpacket[PACKET_SIZE];
char recvpacket[PACKET_SIZE];

int sockfd, datalen = 56;
int nsend = 0, nreceived = 0;
struct sockaddr_in dest_addr;

struct timeval tvrecv;
void statistics(int signo);

unsigned short cal_chksum(unsigned short *addr, int len);
void send_packet(void);
void recv_packet(void);
void tv_sub(struct timeval *out, struct timeval *in);


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

    icmp = (struct icmp*)sendpacket;
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
        ERROR_RETURN(LB_INVALID);
    } 

	USRDEBUG("Server %s: len=%d icmp_type=%X icmp_code=%X icmp_id=%d lb_pid=%d\n", 
		svr_ptr->svr_name, len, icmp->icmp_type, icmp->icmp_code, icmp->icmp_id, lb_ptr->lb_pid);
    if ((icmp->icmp_type == ICMP_ECHOREPLY) && (icmp->icmp_id == lb_ptr->lb_pid))
    {
        tvsend = (struct timeval*)icmp->icmp_data;
        tv_sub(&tvrecv, tvsend); 
        rtt = tvrecv.tv_sec * 1000+tvrecv.tv_usec / 1000;
        USRDEBUG("%d byte from %s: icmp_seq=%u ttl=%d rtt=%.3f ms\n", len,
            inet_ntoa(svr_ptr->svr_from.sin_addr), icmp->icmp_seq, ip->ip_ttl, rtt);
		// store the Timestamp of the last ECHO REPLY 
		memcpy((void *)&svr_ptr->svr_icmp_ts, (void *) icmp->icmp_data, sizeof(struct timeval));
		return(icmp->icmp_seq);
    } else{
        ERROR_RETURN(EDVSNOENT);
	}
}


int init_node_FD(server_t *svr_ptr)
{
    struct hostent *host;
    int size = 50 * 1024;
    struct protoent *protocol;
    unsigned long inaddr = 0l;

	USRDEBUG("Server %s\n", svr_ptr->svr_name);
	if ((protocol = getprotobyname("icmp")) == NULL) {
        ERROR_EXIT(EDVSNOPROTOOPT);
    }

    if ((svr_ptr->svr_icmp_fd = socket(AF_INET, SOCK_RAW, protocol->p_proto)) < 0){
        ERROR_EXIT(EDVSNOTSOCK);
    }
	
    setuid(getuid());
    setsockopt(svr_ptr->svr_icmp_fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
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

    svr_ptr->svr_icmp_sent++;
    svr_ptr->svr_icmp_seq = svr_ptr->svr_icmp_sent;
    packetsize = pack(svr_ptr); 
	USRDEBUG("Server %s: packetsize=%d svr_dstaddr.sin_addr.s_addr=%X\n", 
		svr_ptr->svr_name, packetsize, svr_ptr->svr_dstaddr.sin_addr.s_addr);
    if (sendto(svr_ptr->svr_icmp_fd, sendpacket, packetsize, 0, (struct sockaddr*)
          &svr_ptr->svr_dstaddr, sizeof(svr_ptr->svr_dstaddr)) < 0) {
			ERROR_PRINT(-errno);
    }
}

void recv_packet_FD(server_t *svr_ptr)
{
    int n, fromlen, seq, packsize, oper;
    extern int errno;
	
	USRDEBUG("Server %s(%d): INPUT lb_bm_init=%0lX lb_bm_suspect=%0lX lb_bm_suspect2=%0lX \n", 
			svr_ptr->svr_name, svr_ptr->svr_nodeid, 
			lb_ptr->lb_bm_init, lb_ptr->lb_bm_suspect, lb_ptr->lb_bm_suspect2);

    fromlen = sizeof(svr_ptr->svr_from);
    packsize = 8+datalen;

retry_recv:
	if ((n = recvfrom(svr_ptr->svr_icmp_fd, recvpacket, sizeof(recvpacket), 0, (struct
		sockaddr*) &svr_ptr->svr_from, &fromlen)) >=  0) {
		USRDEBUG("Server %s: recvfrom n=%d\n", svr_ptr->svr_name ,n);
		gettimeofday(&tvrecv, NULL); 
		seq = unpack(svr_ptr, recvpacket, n);
		oper = (seq > 0)?OK:seq;
		USRDEBUG("Server %s: seq=%d oper=%d\n", svr_ptr->svr_name, seq, oper);
		switch(oper) {
			case OK:
				svr_ptr->svr_icmp_rcvd++;
				USRDEBUG("Server %s: seq=%d svr_icmp_seq=%d\n", 
					svr_ptr->svr_name,seq, svr_ptr->svr_icmp_seq );
				if( seq < svr_ptr->svr_icmp_seq){
					ERROR_PRINT(EDVSNOTCONN);
					goto retry_recv;
				}	
				if( seq == svr_ptr->svr_icmp_seq){
					SET_BIT(lb_ptr->lb_bm_init, svr_ptr->svr_nodeid); 
					//////////////////////////////////////////////////////////
					//   SI ERA SOSPECHOSO DEJA DE SERLO 
					//////////////////////////////////////////////////////////								
					CLR_BIT(lb_ptr->lb_bm_suspect, svr_ptr->svr_nodeid); 
					CLR_BIT(lb_ptr->lb_bm_suspect2, svr_ptr->svr_nodeid); 
					svr_ptr->svr_icmp_seq = LB_INVALID;
					goto end_recv;
				}else{
					//////////////////////////////////////////////////////////
					//   NO SE CORRESPONDE CON LA SECUENCIA
					//////////////////////////////////////////////////////////				
					svr_ptr->svr_icmp_seq = LB_INVALID;
					ERROR_PRINT(EDVSNOTCONN);
					goto suspected;
				}
			case EDVSNOENT:
				goto retry_recv;
			case LB_INVALID:
suspected:		
	/////////////////////////////////////////////////////////////
	//			AQUI COMIENZA A SER SOSPECHOSO 
	////////////////////////////////////////////////////////////
					if( TEST_BIT(lb_ptr->lb_bm_suspect, svr_ptr->svr_nodeid) != 0) {
						if ( TEST_BIT(lb_ptr->lb_bm_suspect2, svr_ptr->svr_nodeid) != 0) {
							// FAULTY NODE !!!!!
							CLR_BIT(lb_ptr->lb_bm_init, svr_ptr->svr_nodeid); 				
						}else{
							// Second chance 
							SET_BIT(lb_ptr->lb_bm_suspect2, svr_ptr->svr_nodeid);
						}
					} else {
							// First chance 
							SET_BIT(lb_ptr->lb_bm_suspect, svr_ptr->svr_nodeid);			
					}
				break;
		}
	}else{
		goto suspected;
	}
end_recv:		
	USRDEBUG("Server %s(%d): OUTPUT lb_bm_init=%0lX lb_bm_suspect=%0lX lb_bm_suspect2=%0lX \n", 
		svr_ptr->svr_name, svr_ptr->svr_nodeid, 
		lb_ptr->lb_bm_init, lb_ptr->lb_bm_suspect, lb_ptr->lb_bm_suspect2);
	return;
}


/*===========================================================================*
 *				   lb_fd_sender 				    					 *
 *===========================================================================*/
void *lb_fd_sender(void *arg)
{
	int rcode, i;
    lb_t *lb_ptr;
	server_t *svr_ptr;
	
	lb_ptr = &lb;
	
	USRDEBUG("LB FAILURE DETECTOR SENDER : Initializing... lb_bm_nodes=%0lX\n", lb_ptr->lb_bm_nodes);
	
	while(TRUE) {
		for( i = 0; i < dvs_ptr->d_nr_nodes ; i++){

			USRDEBUG("i=%d lb_bm_nodes=%0lX\n", i, lb_ptr->lb_bm_nodes);

			MTX_LOCK(lb_ptr->lb_mtx);
			if( (TEST_BIT(lb_ptr->lb_bm_nodes, i) == 0)	// Only compute configured nodes   
			||  ( i == lb_ptr->lb_nodeid)) {
				MTX_UNLOCK(lb_ptr->lb_mtx);
				sleep(1);
				continue; 				
			}
			
			
			MTX_LOCK(lb_ptr->lb_fd_mtx);
			lb_ptr->lb_node_test = i;
			svr_ptr = &server_tab[i];
			MTX_LOCK(svr_ptr->svr_mutex);
			// Send an ICMP ECHO REQUEST packet for FAILURE DETECTION
				send_packet_FD(svr_ptr);	
			MTX_UNLOCK(svr_ptr->svr_mutex);
			MTX_UNLOCK(lb_ptr->lb_mtx);
			
			COND_SIGNAL(lb_ptr->lb_fd_rcond);
			COND_WAIT(lb_ptr->lb_fd_scond,lb_ptr->lb_fd_mtx);
			MTX_UNLOCK(lb_ptr->lb_fd_mtx);
			
			sleep(1);
		}
	}
	
	USRDEBUG("LB FAILURE DETECTOR SENDER: Exiting...\n");
    pthread_exit(NULL);
}	

/*===========================================================================*
 *				   lb_fd_receiver			    					 *
 *===========================================================================*/
void *lb_fd_receiver(void *arg)
{
	int rcode, i;
    lb_t *lb_ptr;
	server_t *svr_ptr;
	
	lb_ptr = &lb;
	
	USRDEBUG("LB FAILURE DETECTOR RECEIVER: Initializing... lb_bm_nodes=%0lX\n", lb_ptr->lb_bm_nodes);
	
	while(TRUE) {

		MTX_LOCK(lb_ptr->lb_fd_mtx);
		COND_WAIT(lb_ptr->lb_fd_rcond, lb_ptr->lb_fd_mtx);
		i = lb_ptr->lb_node_test;
		svr_ptr = &server_tab[i];

		MTX_LOCK(lb_ptr->lb_mtx);
		MTX_LOCK(svr_ptr->svr_mutex);
		if(svr_ptr->svr_icmp_seq != LB_INVALID){
			// Receive an ICMP ECHO REPLY for FAILURE DETECTION
			USRDEBUG("BEFORE lb_bm_init=%X\n", lb_ptr->lb_bm_init);
			recv_packet_FD(svr_ptr);
			USRDEBUG("AFTER  lb_bm_init=%X\n", lb_ptr->lb_bm_init);
		}
		MTX_UNLOCK(svr_ptr->svr_mutex);
		MTX_UNLOCK(lb_ptr->lb_mtx);
		
		COND_SIGNAL(lb_ptr->lb_fd_scond);
		MTX_UNLOCK(lb_ptr->lb_fd_mtx);
	}
	
	USRDEBUG("LB FAILURE DETECTOR RECEIVER: Exiting...\n");
    pthread_exit(NULL);
}	

