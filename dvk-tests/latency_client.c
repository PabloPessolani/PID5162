#include "tests.h"

#ifdef USRDBG
// #undef USRDBG
#endif USRDBG
#include "debug.h"

#define  NR_LOOPS		10
#define  WAIT_30SECS	30000
#define RMTNODE	0

char *buffer;

double dwalltime()
{
	double sec;
	struct timeval tv;
	
	gettimeofday(&tv,NULL);
	sec = tv.tv_sec + tv.tv_usec/1000000.0;
	return sec;
}


int run_client(int server_ep, int client_ep, int loops, int cpu_secs)
{
	int i, ret; 
	message *m_ptr;
	double t_total,t_partial_i,t_partial_f,t_partial_consumed,tput,total_bytes,loopbysec; 
	
	// solicita memoria dinámica para el mensaje, pero puede ser estatica tambien
	m_ptr = (char *) malloc(sizeof(message));
	memset(m_ptr,0x00,sizeof(message));
	
	t_total=0.0;
	USRDEBUG("CLIENT before LOOP server_ep=%d client_ep=%d\n", server_ep, client_ep);
	for ( i = 0;  i < loops; i++){	
		m_ptr->m_source= client_ep;
		m_ptr->m_type= 0x0A;
		m_ptr->m1_i1 = i;
		if( cpu_secs < 0)
			m_ptr->m1_i2 = rand()%abs(cpu_secs);      // Returns a pseudo-random integer between 0 and RAND_MAX.
		else
			m_ptr->m1_i2 = cpu_secs;
		m_ptr->m1_i3 = 0x03;
		m_ptr->m1_p1 = 0x04;
		USRDEBUG("CLIENT REQUEST msg:" MSG1_FORMAT, MSG1_FIELDS(m_ptr));
		
		USRDEBUG("CLIENT SEND MSG AT: %.2f \n",t_partial_i);
		t_partial_i= dwalltime();
		ret = dvk_sendrec(server_ep, (long) m_ptr);
		t_partial_f= dwalltime();
		USRDEBUG("CLIENT RECEIVE server MSG AT: %.2f \n",t_partial_f);
		USRDEBUG("CLIENT RECEIVE REPLY msg:" MSG1_FORMAT, MSG1_FIELDS(m_ptr));
		t_partial_consumed = t_partial_f - t_partial_i;
		printf("Time total elapsed in loop %d: %.10f \n",i,t_partial_consumed);
		t_total = t_total + t_partial_consumed;
		
		if( ret != 0 ) ERROR_RETURN(ret);
	}
	
	printf("*********************************************************************\n");	
	printf("Total average latency in %d loops: %.10f [s] \n",loops,t_total/loops);
	printf("Message throughput in %d loops: %.2f [msg/s]\n", loops,2* (double)loops/t_total);
	printf("*********************************************************************\n");
	
	USRDEBUG("CLIENT END\n");
	USRDEBUG("CLIENT exiting\n");
	return(0);
} 
int  main ( int argc, char *argv[] )
{
	int dcid, parent_pid, parent_ep, parent_nr, ret;
	int server_ep, server_nr, loops, cpu_secs;
	int client_pid, client_ep, client_nr;
	char rmt_name[16]="latency_server";
	
	proc_usr_t usr_info, *p_usr;
	
	p_usr = &usr_info;
	
	if ( argc != 5 && argc != 6 ) {
		fprintf(stderr,  "Usage: %s <dcid> <client_nr> <server_nr> <loops> [<cpu_secs>]	\n", argv[0] );
		fprintf(stderr,  "  if cpu_secs > 0, it is the fixed time CPU usage in server\n", argv[0] );
		fprintf(stderr,  "  if cpu_secs < 0, it is a random time from 0 < t < abs(cpu_secs) of CPU usage in server\n", argv[0] );
		exit(EXIT_FAILURE);
	}
	
	if( argc == 6){
		cpu_secs = atoi(argv[5]);
		if(cpu_secs < 0){
			srand(time(NULL));   // Initialization, should only be called once.
		}	
	} else {
		cpu_secs = 0;
	}
	
	dcid = atoi(argv[1]);
	if ( dcid < 0 || dcid >= NR_DCS) {
		fprintf(stderr,  "Invalid dcid [0-%d]\n", NR_DCS-1 );
		exit(EXIT_FAILURE);
	}
	client_ep = client_nr = atoi(argv[2]);
	server_ep = server_nr = atoi(argv[3]);
	loops = atoi(argv[4]);
	
	// habilito el uso de las APIs del DVK para el padre. 
	ret = dvk_open();
	if (ret < 0)  ERROR_EXIT (ret);

	client_ep = dvk_bind(dcid,client_nr);
	if( client_ep < EDVSERRCODE) ERROR_EXIT(server_ep);

	USRDEBUG("CLIENT: Ready to call run_client() with server_ep=%d client_ep=%d\n", 
				 server_ep, client_ep);
		ret = run_client(server_ep, client_ep, loops, cpu_secs);		

	USRDEBUG("CLIENT exiting\n");
	exit(0);
}
