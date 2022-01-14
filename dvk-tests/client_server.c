#include "tests.h"

#include "debug.h"

#define  NR_LOOPS		10
#define  WAIT_30SECS	30000

int run_server(int server_ep)
{
	int ret, i; 
	message *m_ptr;

	// solicita memoria dinámica para el mensaje, pero puede ser estatica tambien
	m_ptr = (char *) malloc(sizeof(message));
	memset(m_ptr,0x00,sizeof(message));
	
	USRDEBUG("SERVER before LOOP server_ep=%d\n", server_ep);
	for ( i = 0; i < (NR_LOOPS-1); ){
		
		// Waiting for a request message 
		USRDEBUG("SERVER Waiting for a request message loop=%d\n:", i);
		ret = dvk_receive_T(ANY, m_ptr, WAIT_30SECS);
		if( ret < 0) {
			if( ret == EDVSTIMEDOUT){
				ERROR_PRINT(ret);
				continue;
			}else{
				ERROR_EXIT(ret);
			}
		} 
		USRDEBUG("SERVER REQUEST msg:" MSG1_FORMAT, MSG1_FIELDS(m_ptr));
		
		// update i 
		i = m_ptr->m1_i1;
		
		// change message type 
		m_ptr->m_type= 0x0B;
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

int run_client(int server_ep, int client_ep)
{
	int i, ret; 
	message *m_ptr;

	// solicita memoria dinámica para el mensaje, pero puede ser estatica tambien
	m_ptr = (char *) malloc(sizeof(message));
	memset(m_ptr,0x00,sizeof(message));
	
	USRDEBUG("CLIENT before LOOP server_ep=%d client_ep=%d\n", server_ep, client_ep);
	for ( i = 0;  i < NR_LOOPS; i++){
		m_ptr->m_type= 0x0A;
		m_ptr->m1_i1 = i;
		m_ptr->m1_i2 = 0x02;
		m_ptr->m1_i3 = 0x03;
		USRDEBUG("CLIENT REQUEST msg:" MSG1_FORMAT, MSG1_FIELDS(m_ptr));
		ret = dvk_sendrec(server_ep, (long) m_ptr);
		if( ret != 0 ) ERROR_RETURN(ret);
		USRDEBUG("CLIENT REPLY msg:" MSG1_FORMAT, MSG1_FIELDS(m_ptr));
	}
	USRDEBUG("CLIENT exiting\n");
	return(0);
}  
   
void  main ( int argc, char *argv[] )
{
	int dcid, parent_pid, parent_ep, parent_nr, ret;
	int server_pid, server_ep, server_nr;
	int client_pid, client_ep, client_nr;

	proc_usr_t usr_info, *p_usr;
	
	p_usr = &usr_info;
	
    if ( argc != 4) {
		fprintf(stderr,  "Usage: %s <dcid> <server_nr> <client_nr>  \n", argv[0] );
		exit(EXIT_FAILURE);
	}

	dcid = atoi(argv[1]);
	if ( dcid < 0 || dcid >= NR_DCS) {
		fprintf(stderr,  "Invalid dcid [0-%d]\n", NR_DCS-1 );
		exit(EXIT_FAILURE);
	}

	server_ep = server_nr = atoi(argv[2]);
	client_ep = client_nr = atoi(argv[3]);
	
	// habilito el uso de las APIs del DVK para el padre. Los hijos heredan 
	ret = dvk_open();
	if (ret < 0)  ERROR_EXIT (ret);
	
	parent_pid = getpid();
	
	if( (server_pid = fork()) == 0 ){// SERVER  
		
		// Espera que el padre haga el bind 
		USRDEBUG("SERVER: Waiting for own binding\n");
		while( (ret = dvk_wait4bind_T(WAIT_30SECS)) != server_ep){
			if( ret == EDVSTIMEDOUT) {
				ERROR_PRINT(ret);
			}else{
				ERROR_EXIT(ret);
			}
		}		
		
		USRDEBUG("SERVER: Ready to call run_server() with server_ep=%d\n",server_ep);
		ret = run_server(server_ep);
		ERROR_EXIT(ret);
	}

		
	if( (client_pid = fork()) == 0 ){// CLIENT 
		
		// Espera que el padre haga el bind 
		USRDEBUG("CLIENT: Waiting for own binding\n");
		while( (ret = dvk_wait4bind_T(WAIT_30SECS)) != client_ep){
			if( ret == EDVSTIMEDOUT) {
				ERROR_PRINT(ret);
			}else{
				ERROR_EXIT (ret);
			}
		}		

		// Espera que el padre haga el bind del server porque sino va a enviarle peticiones a un endpoint que 
		// todavia no arranco 
		USRDEBUG("CLIENT: Waiting for SERVER binding\n");
		while( (ret = dvk_wait4bindep_T(server_ep, WAIT_30SECS)) != server_ep){
			if( ret == EDVSTIMEDOUT) {
				ERROR_PRINT(ret);
			}else{
				ERROR_EXIT (ret);
			}
		}
		
		USRDEBUG("CLIENT: Ready to call run_client() with server_ep=%d client_ep=%d\n", 
				server_ep, client_ep);
		ret = run_client(server_ep, client_ep);
		ERROR_EXIT(ret);
	}

	// PARENT binds  SERVER 
	server_ep = dvk_lclbind(dcid, server_pid, server_nr);
	if( server_ep < EDVSERRCODE) ERROR_EXIT(server_ep);
	USRDEBUG("PARENT binds SERVER: dcid=%d server_pid=%d server_ep=%d\n",
				dcid, server_pid, server_ep);
	
	ret = dvk_getprocinfo(dcid, server_nr, p_usr);
	if( ret < 0) ERROR_EXIT(ret);
	USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(p_usr));
	USRDEBUG(PROC_WAIT_FORMAT, PROC_WAIT_FIELDS(p_usr));
	USRDEBUG(PROC_COUNT_FORMAT, PROC_COUNT_FIELDS(p_usr));

	// PARENT binds  CLIENT 
	client_ep = dvk_lclbind(dcid, client_pid, client_nr);
	if( client_ep < EDVSERRCODE) ERROR_EXIT(server_ep);
	USRDEBUG("PARENT binds CLIENT: dcid=%d client_pid=%d client_ep=%d\n",
				dcid, client_pid, client_ep);
	
	ret = dvk_getprocinfo(dcid, client_nr, p_usr);
	if( ret < 0) ERROR_EXIT(ret);
	USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(p_usr));
	USRDEBUG(PROC_WAIT_FORMAT, PROC_WAIT_FIELDS(p_usr));
	USRDEBUG(PROC_COUNT_FORMAT, PROC_COUNT_FIELDS(p_usr));

	// Wait for one child process 
    waitpid(client_pid, &ret, 0);
	
	// Wait for the other child process 
    waitpid(server_pid, &ret, 0);
	
	USRDEBUG("PARENT exiting\n");
	exit(0);
}

