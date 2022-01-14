#include "tests.h"
#ifdef USRDBG
// #undef USRDBG
#endif USRDBG
#include "debug.h"

#define  WAIT_30SECS	30000

int retry  = FALSE;

void sig_handler(int signum){
	retry = FALSE;
}

int run_server(int server_ep)
{
	int ret, i, cpu_secs; 
	message *m_ptr;

	
	// solicita memoria dinámica para el mensaje, pero puede ser estatica tambien
	m_ptr = (char *) malloc(sizeof(message));
	memset(m_ptr,0x00,sizeof(message));
	
	USRDEBUG("SERVER before LOOP server_ep=%d\n", server_ep);
	while (TRUE) {
		
		// Waiting for a request message 
		USRDEBUG("SERVER Waiting for a request message\n:");
		ret = dvk_receive_T(ANY, m_ptr, WAIT_30SECS);
//		ret = dvk_rcvrqst_T(m_ptr, WAIT_30SECS);
		if( ret < 0) {
			if( ret == EDVSTIMEDOUT){
				ERROR_PRINT(ret);
				continue;
			}else{
				ERROR_EXIT(ret);
			}
		} 
		USRDEBUG("SERVER REQUEST msg:" MSG1_FORMAT, MSG1_FIELDS(m_ptr));
		
		cpu_secs = m_ptr->m1_i2;
		if( cpu_secs > 0) {
			signal(SIGALRM,sig_handler); // Register signal handler
			alarm(cpu_secs);  // Scheduled alarm after cpu_secs seconds

			retry = TRUE;
			while(retry == TRUE){} ;
		}		
		// change message type 
		m_ptr->m_type= 0x0B;
//		ret = dvk_reply_T(m_ptr->m_source, m_ptr, WAIT_30SECS);
		
		ret = dvk_send_T(m_ptr->m_source, m_ptr, WAIT_30SECS);
		if( ret < 0) {
			if( ret == EDVSTIMEDOUT){
				ERROR_PRINT(ret);
				continue;
			}else{
				ERROR_RETURN(ret);
			}
		}
		USRDEBUG("SERVER REPLY msg:" MSG1_FORMAT, MSG1_FIELDS(m_ptr));
	}
	USRDEBUG("SERVER exiting\n");
	return(0);
}



int  main ( int argc, char *argv[] )
{
	int dcid, parent_pid, parent_ep, parent_nr, ret;
	int server_pid,server_ep, server_nr,client_ep,client_nr;
	
	proc_usr_t usr_info, *p_usr;
	
	p_usr = &usr_info;
	
	if ( argc != 3) {
		fprintf(stderr,  "Usage: %s <dcid> <server_nr>\n", argv[0] );
		exit(EXIT_FAILURE);
	}
	
	dcid = atoi(argv[1]);
	if ( dcid < 0 || dcid >= NR_DCS) {
		fprintf(stderr,  "Invalid dcid [0-%d]\n", NR_DCS-1 );
		exit(EXIT_FAILURE);
	}
	
	server_ep = server_nr = atoi(argv[2]);
	
	// habilito el uso de las APIs del DVK para el padre. Los hijos heredan 
	ret = dvk_open();
	if (ret < 0)  ERROR_EXIT (ret);
	server_pid = getpid();
	
	server_ep = dvk_bind(dcid,server_nr);
	USRDEBUG("SERVER bind with endpoint: %d",server_ep);
	if( server_ep < EDVSERRCODE) ERROR_EXIT(server_ep);
	USRDEBUG("binds SERVER: dcid=%d server_pid=%d server_ep=%d\n",
			 dcid, server_pid, server_ep);

	USRDEBUG("SERVER: Ready to call run_server() with server_ep=%d\n",server_ep);
		ret = run_server(server_ep);

	USRDEBUG("SERVER exiting\n");
	exit(0);
}
