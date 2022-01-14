/****************************************************************/
/* 				DDM_monitor							*/
/* DVK DEAMON MONITOR 				 		*/
/****************************************************************/

#define _GNU_SOURCE     
#define _MULTI_THREADED
#define _TABLE
#include "dd_common.h"
#include "dd_monitor.h"
#include "glo_monitor.h"

void *rcv_cmd_output(void *arg);
int do_cmd_input(ddm_t *ddm_ptr);
int ddm_reg_msg(char *sender_ptr, ddm_t *ddm_ptr, int16 msg_type);
void remove_pipes(void);

/*===========================================================================*
 *				   dd_monitor 				    					 *
 *===========================================================================*/
int main (int argc, char *argv[] )
{
	int rcode;
	int child_pid;

    ddm_t *ddm_ptr;

    if ( argc != 1) {
        fprintf( stderr,"Usage: %s \n", argv[0] );
        exit(1);
    }
	
	ddm_ptr = &ddm;
	USRDEBUG("DVKD MONITOR: Initializing...\n");
	USRDEBUG("DVKD MONITOR: sizeof(message)=%d\n", sizeof(message));

	USRDEBUG("DVKD MONITOR: sizeof(mess_1)=%d\n", sizeof(mess_1));
	USRDEBUG("DVKD MONITOR: sizeof(mess_2)=%d\n", sizeof(mess_2));
	USRDEBUG("DVKD MONITOR: sizeof(mess_3)=%d\n", sizeof(mess_3));
	USRDEBUG("DVKD MONITOR: sizeof(mess_4)=%d\n", sizeof(mess_4));
	USRDEBUG("DVKD MONITOR: sizeof(mess_5)=%d\n", sizeof(mess_5));
	USRDEBUG("DVKD MONITOR: sizeof(mess_6)=%d\n", sizeof(mess_6));
	USRDEBUG("DVKD MONITOR: sizeof(mess_7)=%d\n", sizeof(mess_7));
	USRDEBUG("DVKD MONITOR: sizeof(mess_8)=%d\n", sizeof(mess_8));
	USRDEBUG("DVKD MONITOR: sizeof(mess_9)=%d\n", sizeof(mess_9));
	USRDEBUG("DVKD MONITOR: sizeof(mess_A)=%d\n", sizeof(mess_A));
	USRDEBUG("DVKD MONITOR: sizeof(mess_B)=%d\n", sizeof(mess_B));
	USRDEBUG("DVKD MONITOR: sizeof(mess_C)=%d\n", sizeof(mess_C));


    rcode = gethostname(ddm_ptr->ddm_name, MAXNODENAME-1);
	if(rcode != 0) ERROR_EXIT(-errno);
    USRDEBUG("gethostname=%s\n", ddm_ptr->ddm_name);

	snprintf(&ddm_ptr->ddm_OUTCMDfifo, MAX_PIPE_NAME, MONITOR_OUTCMD_FIFO,
            ddm_ptr->ddm_name);
	snprintf(&ddm_ptr->ddm_INCMDfifo, MAX_PIPE_NAME, MONITOR_INCMD_FIFO,
            ddm_ptr->ddm_name);
	
	// Open OUTPUT COMMAND pipe for READING 
    USRDEBUG("Open MONITOR pipe=%s for READING\n", ddm_ptr->ddm_OUTCMDfifo);
    ddm_ptr->ddm_OUTCMDfd = open(ddm_ptr->ddm_OUTCMDfifo, O_RDWR , 0644);
    if (ddm_ptr->ddm_OUTCMDfd == -1){
		fprintf( stderr,"main: MONITOR open %s errno=%d\n",
				 ddm_ptr->ddm_OUTCMDfifo, -errno);
		ERROR_EXIT(-errno);					
	}

	// Open INPUT COMMAND pipe for WRITING  
    USRDEBUG("Open MONITOR pipe=%s for WRITING\n", ddm_ptr->ddm_INCMDfifo);
    ddm_ptr->ddm_INCMDfd = open(ddm_ptr->ddm_INCMDfifo, O_RDWR , 0644);
    if (ddm_ptr->ddm_INCMDfd == -1){
		fprintf( stderr,"main: MONITOR open %s errno=%d\n",
				 ddm_ptr->ddm_INCMDfifo, -errno);
		ERROR_EXIT(-errno);					
	}
	
	ddm_ptr->ddm_seq = -1;
	while(TRUE){
		rcode = do_cmd_input(ddm_ptr);
		if(rcode != EDVSDSTDIED) {
			ERROR_EXIT(rcode);
		}
	}
	USRDEBUG("Exiting...\n");
}	

/*===========================================================================*
 *				   do_cmd_input 				    					 *
 *===========================================================================*/
int do_cmd_input(ddm_t *ddm_ptr)
{ 
	int inbytes, outbytes;
	int bytes, rcode;
	int total;
	char *ptr;
	
	USRDEBUG("\n");

	while(TRUE){
		ddm_ptr->ddm_seq++;
		
		printf("(%s)Input command> ",ddm_ptr->ddm_name);
		// Get COMMAND from STDIN 
		memset(cmd_input, 0, MAX_CMDIN_LEN);
		if( fgets( cmd_input, MAX_CMDIN_LEN-1, stdin ) == NULL ){
			fprintf( stderr,"do_cmd_input: MONITOR fgets errno=%d\n",-errno);
			ERROR_EXIT(-errno);					
		}
		
		// remove NEWLINE 
	    ptr = strchr(cmd_input, '\n');
		if( ptr != NULL){
			*ptr = 0;
		}
		inbytes = strlen(cmd_input);
		USRDEBUG("cmd_input[%s] inbytes=%d\n", cmd_input, inbytes);
	
		// OWN EXIT COMMAND 
		if(strcmp(cmd_input,"exit") == 0) {
			ERROR_EXIT(0);
		}
		
		// API COMMAND 
		if(strcmp(cmd_input,"API") == 0) {
			printf("(%s)API> rmtbind <dcid> <ep> <nodeid> <name>\n",ddm_ptr->ddm_name);
			printf("(%s)API> ",ddm_ptr->ddm_name);
			// Get COMMAND from STDIN 
			memset(cmd_input, 0, MAX_CMDIN_LEN);
			if( fgets( cmd_input, MAX_CMDIN_LEN-1, stdin ) == NULL ){
				fprintf( stderr,"do_cmd_input: MONITOR fgets errno=%d\n",-errno);
				ERROR_EXIT(-errno);					
			}
			// remove NEWLINE 
			ptr = strchr(cmd_input, '\n');
			if( ptr != NULL){
				*ptr = 0;
			}
			m_ptr = &m_in;
			rcode = do_getAPI(cmd_input, m_ptr);
			if( rcode != 0) {
				fprintf( stderr,"do_cmd_input: API bad arguments\n",-errno);
				continue;
			}		
		}else{
			m_ptr = &m_in;
			m_ptr->m_source = SOURCE_MONITOR;
			m_ptr->m_type 	= MT_CMD2AGENT;
			m_ptr->m1_i1	= inbytes;
			m_ptr->m1_i2	= ddm_ptr->ddm_seq;
			m_ptr->m1_i3	= 0;
			m_ptr->m1_p1	= NULL;
			m_ptr->m1_p2	= NULL;
			m_ptr->m1_p2	= NULL;
			USRDEBUG("INCMD HEADER (%d): " MSG1_FORMAT, sizeof(message), MSG1_FIELDS(m_ptr));
		}
	
		// Write  INCMD HEADER PIPE to PROXY 
		bytes = write(ddm_ptr->ddm_INCMDfd, m_ptr, sizeof(message));
		if( bytes != sizeof(message)) {
			fprintf( stderr,"do_cmd_input: MONITOR PIPE message write errno=%d\n",-errno);
			ERROR_EXIT(-errno);					
		}

		if (m_ptr->m_type == MT_CMD2AGENT) {
			// Write to INCMD PIPE to PROXY 
			USRDEBUG("INCMD: cmd_input=%s\n", cmd_input);
			bytes = write(ddm_ptr->ddm_INCMDfd, cmd_input, inbytes);
			if( bytes != inbytes) {
				fprintf( stderr,"do_cmd_input: MONITOR PIPE message write errno=%d\n",-errno);
				ERROR_EXIT(-errno);					
			}
		}
		
		// get from OUTPUT  PIPE 
		m_ptr = &m_out;
		bytes = read(ddm_ptr->ddm_OUTCMDfd, m_ptr, sizeof(message));
		if ( bytes < 0) {
			fprintf( stderr,"do_cmd_input: MONITOR PIPE read errno=%d\n",-errno);
			ERROR_EXIT(-errno);					
		}	
		
		switch(m_ptr->m_type){
			case MT_OUT2MONITOR:
				assert( m_ptr->m_source == SOURCE_PROXY);
				USRDEBUG("OUTCMD HEADER: " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
				// get OUTCMD from PIPE PROXY pipe
				total = 0;
				ptr = cmd_output;
				outbytes = m_ptr->m1_i1;
				USRDEBUG("Reading from %d PIPE...\n", outbytes);
				if( outbytes > 0) {
					bytes = read(ddm_ptr->ddm_OUTCMDfd, ptr, outbytes);
					if ( bytes < 0) {
						fprintf( stderr,"do_cmd_input: MONITOR PIPE read errno=%d\n",-errno);
						ERROR_EXIT(-errno);					
					}
					ptr += bytes;
					*ptr = 0;
					printf("%s", cmd_output);		
				}
				break;
			case (DVK_BIND| MASK_ACKNOWLEDGE):
				assert( m_ptr->m_source == SOURCE_AGENT);
				USRDEBUG("API REPLY (%d): " MSGC_FORMAT, sizeof(message), MSGC_FIELDS(m_ptr));	
				printf("API RESULT %s =%d\n",m_ptr->mC_ca1, m_ptr->mC_i1);	
				break;
			default:
				break;
		}
	}
}

/*===========================================================================*
 *				   do_getAPI 				    					*
 * Example: rmtbind 0 111 0 remote_111									*
 *===========================================================================*/
int do_getAPI(char *token, message *m_ptr)
{
	int i;
	
	// Extract the first token
	token = strtok(cmd_input, " ");
	USRDEBUG( " %s\n", token ); //printing each token
	if(strcmp(token,"rmtbind") != 0) {
		ERROR_RETURN(EDVSBADCALL);
	};		
	// loop through the string to extract all other tokens
	for( i=0; token != NULL; i++) {
		if(i == 0){
			m_ptr->mC_i1 = RMT_BIND;
			token = strtok(NULL, " ");
			continue;
		}
		USRDEBUG(" %s\n", token ); //printing each token
		if( i == 1){
			m_ptr->mC_i2 = atoi(token); //  dcid 
		}else if (i == 2){
			m_ptr->mC_i3 = atoi(token); //  ep 
		}else if (i == 3){
			m_ptr->mC_i4 = atoi(token); //  nodeid 
		}else if (i == 4){
			strncpy(m_ptr->mC_ca1, token, MC_STRING-1); // name 
		}else {
				ERROR_RETURN(EDVS2BIG);
		}
		token = strtok(NULL, " ");
	}
	if( i != 5) ERROR_RETURN(EDVS2BIG);
	m_ptr->m_type 	= DVK_BIND;
	m_ptr->m_source = SOURCE_MONITOR;
	USRDEBUG("API HEADER (%d): " MSGC_FORMAT, sizeof(message), MSGC_FIELDS(m_ptr));
	return(OK);
}