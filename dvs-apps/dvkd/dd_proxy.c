/****************************************************************/
/* 				DD PROXY 						*/
/* DVK DEAMON PROXY BETWEEN MONITOR AND AGENT  		*/
/****************************************************************/

#define _GNU_SOURCE     
#define _MULTI_THREADED
#define _TABLE
#include "dd_common.h"
#include "dd_proxy.h"
#include "glo_proxy.h"

void *rcv_cmd_output(void *arg);
int do_cmd_input(ddp_t *ddp_ptr);
int ddp_reg_msg(char *sender_ptr, ddp_t *ddp_ptr, int16 msg_type);
void remove_pipes(void);

/*===========================================================================*
 *				   dd_proxy 				    					 *
 *===========================================================================*/
int main (int argc, char *argv[] )
{
	int rcode;
	int child_pid;

    ddp_t *ddp_ptr;

    if ( argc != 1) {
        fprintf( stderr,"Usage: %s \n", argv[0] );
        exit(1);
    }
	
	ddp_ptr = &ddp;
	USRDEBUG("DVKD PROXY: Initializing...\n");

	local_nodeid = DD_INVALID;
    rcode = dvk_open();     //load dvk
    if (rcode < 0)  ERROR_EXIT(rcode);	

    local_nodeid = dvk_getdvsinfo(&dvs);    //This dvk_call gets DVS status and parameter information from the local node.
    USRDEBUG("local_nodeid=%d\n",local_nodeid);
    if( local_nodeid < 0 )
        ERROR_EXIT(EDVSDVSINIT);    //DVS not initialized
	ddp_ptr->ddp_nodeid = local_nodeid;
    dvs_ptr = &dvs;     //DVS parameters pointer
    USRDEBUG(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));

    rcode = gethostname(ddp_ptr->ddp_name, MAXNODENAME-1);
	if(rcode != 0) ERROR_EXIT(-errno);
    USRDEBUG("gethostname=%s\n", ddp_ptr->ddp_name);
	
	// Creat the Monitor OUTCMD FIFO ( Which will receive the output of commands)
	snprintf(&ddp_ptr->ddp_OUTCMDfifo, MAX_PIPE_NAME, MONITOR_OUTCMD_FIFO,
            ddp_ptr->ddp_name);
    if (mkfifo(ddp_ptr->ddp_OUTCMDfifo, S_IRUSR | S_IWUSR | S_IWGRP) == -1
                && errno != EEXIST){
		fprintf( stderr,"main: MONITOR mkfifo %s errno=%d\n",
				 ddp_ptr->ddp_OUTCMDfifo, -errno);
		ERROR_EXIT(-errno);					
	}
	
	// Creat the Monitor INCMD FIFO  Which will send commands to proxy
	snprintf(&ddp_ptr->ddp_INCMDfifo, MAX_PIPE_NAME, MONITOR_INCMD_FIFO,
            ddp_ptr->ddp_name);
    if (mkfifo(ddp_ptr->ddp_INCMDfifo, S_IRUSR | S_IWUSR | S_IWGRP) == -1
                && errno != EEXIST){
		fprintf( stderr,"main: MONITOR mkfifo %s errno=%d\n",
				 ddp_ptr->ddp_INCMDfifo, -errno);

		ERROR_EXIT(-errno);					
	}		
		
	atexit(remove_pipes);
		
	// Open OUTPUT COMMAND pipe for WRITING 
    USRDEBUG("Open MONITOR OUTPUT COMMAND pipe=%s for WRITING\n", ddp_ptr->ddp_OUTCMDfifo);
    ddp_ptr->ddp_OUTCMDfd = open(ddp_ptr->ddp_OUTCMDfifo, O_RDWR , 0644);
    if (ddp_ptr->ddp_OUTCMDfd == -1){
		fprintf( stderr,"main: MONITOR OUTCMD open %s errno=%d\n",
				 ddp_ptr->ddp_OUTCMDfifo, -errno);
		ERROR_EXIT(-errno);					
	}

	// Open INPUT COMMAND pipe for READING   
    USRDEBUG("Open MONITOR INPUT COMMAND pipe=%s for READING\n", ddp_ptr->ddp_INCMDfifo);
    ddp_ptr->ddp_INCMDfd = open(ddp_ptr->ddp_INCMDfifo, O_RDWR , 0644);
    if (ddp_ptr->ddp_INCMDfd == -1){
		fprintf( stderr,"main: MONITOR INCMD open %s errno=%d\n",
				 ddp_ptr->ddp_INCMDfifo, -errno);
		ERROR_EXIT(-errno);					
	}
	
    pthread_mutex_init(&ddp_ptr->ddp_mutex, NULL);
	pthread_cond_init(&ddp_ptr->ddp_cond,NULL);
	
	rcode = pthread_create( &ddp_ptr->ddp_thread, NULL, rcv_cmd_output, ddp_ptr);
	if( rcode) ERROR_EXIT(rcode);
	
	MTX_LOCK(ddp_ptr->ddp_mutex);
	COND_WAIT(ddp_ptr->ddp_cond, ddp_ptr->ddp_mutex);
	MTX_UNLOCK(ddp_ptr->ddp_mutex);
		
	while(TRUE){
		rcode = do_cmd_input(ddp_ptr);
		if(rcode != EDVSDSTDIED) {
			ERROR_EXIT(rcode);
		}
	}
    rcode = wait();	

	USRDEBUG("Exiting...\n");
}	

void remove_pipes(void)
{
	ddp_t *ddp_ptr;
	
	USRDEBUG("Removing Pipes\n");
	ddp_ptr = &ddp;
    unlink(ddp_ptr->ddp_OUTCMDfifo);
    unlink(ddp_ptr->ddp_INCMDfifo);
}


/*===========================================================================*
 *				   do_cmd_input 				    					 *
 *===========================================================================*/
int do_cmd_input(ddp_t *ddp_ptr)
{ 
	int inbytes, outbytes, bytes;
	char *ptr;
	int i;
	
	USRDEBUG("\n");

	while(TRUE){

		m_ptr = &m_in;
		// Reading CMDIN HEADER from MONITOR
		USRDEBUG("Reading CMDIN HEADER from pipe %s\n", ddp_ptr->ddp_INCMDfifo);
		inbytes = read( ddp_ptr->ddp_INCMDfd, m_ptr, sizeof(message));	
		if( inbytes != sizeof(message)){
			fprintf( stderr,"main: PROXY read CMDIN HEADER errno=%d inbytes=%d\n",-errno, inbytes);
			ERROR_EXIT(-errno);					
		}
	
		assert( m_ptr->m_source == SOURCE_MONITOR);
		
		if (  m_ptr->m_type 	== DVK_BIND){
			USRDEBUG("API HEADER (%d): " MSGC_FORMAT, sizeof(message), MSGC_FIELDS(m_ptr));
			// MULTICAST REMOTE API TO AGENT  	
			SP_multicast(ddp_ptr->ddp_mbox, FIFO_MESS, SPREAD_GROUP,
					DVK_BIND, inbytes, m_ptr);
		}else{
			USRDEBUG("INCMD HEADER: inbytes=%d " MSG1_FORMAT, inbytes, MSG1_FIELDS(m_ptr));

			// Reading CMDIN from MONITOR
			USRDEBUG("Reading CMDIN from pipe %s\n", ddp_ptr->ddp_INCMDfifo);
			inbytes = m_ptr->m1_i1;
			if( inbytes > 0){
				bytes = read( ddp_ptr->ddp_INCMDfd, cmd_input, inbytes);
				if( bytes != inbytes){
					fprintf( stderr,"main: PROXY read CMDIN HEADER errno=%d inbytes=%d\n",-errno, inbytes);
					ERROR_EXIT(-errno);					
				}
			}
			USRDEBUG("INCMD: bytes=%d cmd_input=%s", bytes, cmd_input);

			// MULTICAST TO AGENT CMDIN 	
			SP_multicast(ddp_ptr->ddp_mbox, FIFO_MESS, SPREAD_GROUP,
						MT_CMD2AGENT, inbytes, cmd_input);
		}			
		printf("do_cmd_input: Waiting to receive output...\n");			
		MTX_LOCK(ddp_ptr->ddp_mutex);
		COND_WAIT(ddp_ptr->ddp_cond, ddp_ptr->ddp_mutex);
		MTX_UNLOCK(ddp_ptr->ddp_mutex);
	}
}

/*===========================================================================*
 *				   rcv_cmd_output 				    					 *
 *===========================================================================*/
void *rcv_cmd_output(void *arg)
{
	int rcode;
	ddp_t *ddp_ptr;
	
	USRDEBUG("Initializing...\n");
	ddp_ptr = (ddp_t *) arg;
	
    init_spread( );
	clear_monitor_vars(ddp_ptr);
    connect_to_spread(ddp_ptr);
    	
    while(TRUE){
        USRDEBUG(DDP_FORMAT, DDP_FIELDS(ddp_ptr));
        rcode = ddp_SP_receive(ddp_ptr);		//loop receiving messages
        USRDEBUG("rcode=%d\n", rcode);
        if(rcode < 0 ) {
            ERROR_PRINT(rcode);
            sleep(DD_TIMEOUT_5SEC);		
            if( rcode == EDVSNOTCONN) {
                connect_to_spread(ddp_ptr);
            }
        }
    }
}

/*===========================================================================*
 *				init_spread				     
 *===========================================================================*/
void init_spread(void)
{
    int rcode;
#ifdef SPREAD_VERSION
    int     mver, miver, pver;
#endif
    
    ddp_timeout.sec  = DD_TIMEOUT_5SEC;
    ddp_timeout.usec = DD_TIMEOUT_MSEC;
    
#ifdef SPREAD_VERSION
    rcode = SP_version( &mver, &miver, &pver);
    if(!rcode)     {
        SP_error (rcode);
        ERROR_EXIT(rcode);
    }
    USRDEBUG("Spread library version is %d.%d.%d\n", mver, miver, pver);
#else
    USRDEBUG("Spread library version is %1.2f\n", SP_version() );
#endif
}

/*===========================================================================*
 *				connect_to_spread				     
 *===========================================================================*/
void connect_to_spread(ddp_t *ddp_ptr)
{
    int rcode;
    /*------------------------------------------------------------------------------------
	* ddp_mbr_name:  it must be unique in the spread node.
	*--------------------------------------------------------------------------------------*/
    sprintf(ddp_ptr->ddp_sp_group, "%s", SPREAD_GROUP);
    USRDEBUG("spread_group=%s\n", ddp_ptr->ddp_sp_group);
    sprintf( Spread_name, "4803");
	
	sprintf( ddp_ptr->ddp_mbr_name, "DDM.%02d", ddp_ptr->ddp_nodeid);
    USRDEBUG("ddp_mbr_name=%s\n", ddp_ptr->ddp_mbr_name);
    
    rcode = SP_connect_timeout( Spread_name,  ddp_ptr->ddp_mbr_name , 0, 1, 
                               &ddp_ptr->ddp_mbox, ddp_ptr->ddp_priv_group, ddp_timeout);
							   
    if( rcode != ACCEPT_SESSION ) 	{
        SP_error (rcode);
        ERROR_PRINT(rcode);
        pthread_exit(NULL);
    }
    
    USRDEBUG("ddp_mbr_name %s: connected to %s with private group %s\n",
             ddp_ptr->ddp_mbr_name, Spread_name, ddp_ptr->ddp_priv_group);
    
    rcode = SP_join( ddp_ptr->ddp_mbox, ddp_ptr->ddp_sp_group);
    if( rcode){
        SP_error (rcode);
        ERROR_PRINT(rcode);
        pthread_exit(NULL);
    }
}

/*===========================================================================*
 *				clear_monitor_vars				     
 *Clear several global and replicated  variables
 *===========================================================================*/
void clear_monitor_vars(ddp_t *ddp_ptr)
{	
    USRDEBUG("\n");

    ddp_ptr->ddp_bm_nodes 	= 0;
    ddp_ptr->ddp_bm_init	= 0;	
    ddp_ptr->ddp_nr_nodes 	= 0;
    ddp_ptr->ddp_nr_init	= 0;
}

/*===========================================================================*
 *				ddp_SP_receive				    
 *===========================================================================*/
int ddp_SP_receive(ddp_t	*ddp_ptr)
{
    char        sender[MAX_GROUP_NAME];
    char        target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
    int         num_groups = -1;
    int         service_type = 0;
    int16       mess_type;
    int         endian_mismatch;
    int         ret;
   
    USRDEBUG("%s\n", ddp_ptr->ddp_name);
    
    assert(ddp_ptr->ddp_mess_in != NULL);
    
    
    memset(ddp_ptr->ddp_mess_in,0,MAX_MESSLEN);
    //Returns message size in non-error
    ret = SP_receive( ddp_ptr->ddp_mbox, &service_type, sender, 100, &num_groups, target_groups,
                     &mess_type, &endian_mismatch, MAX_MESSLEN, ddp_ptr->ddp_mess_in );
    USRDEBUG("ret=%d\n", ret);
    
    if( ret < 0 ){
        if ( (ret == GROUPS_TOO_SHORT) || (ret == BUFFER_TOO_SHORT) ) {
            service_type = DROP_RECV;
            USRDEBUG("\n========Buffers or Groups too Short=======\n");
            ret = SP_receive( ddp_ptr->ddp_mbox, &service_type, sender, 
                             MAX_MEMBERS, &num_groups, target_groups,
                             &mess_type, &endian_mismatch, MAX_MESSLEN, ddp_ptr->ddp_mess_in);
        }
    }
    
    if (ret < 0 ) {
        SP_error( ret );
        ERROR_EXIT(ret);
        pthread_exit(NULL);
    }
    
    USRDEBUG("%s: sender=%s Private_group=%s service_type=%d\n", 
             ddp_ptr->ddp_name,
             sender, 
             ddp_ptr->ddp_priv_group, 
             service_type);
      
    if( Is_regular_mess( service_type ) )	{
        if( Is_fifo_mess(service_type) ) {
            USRDEBUG("%s: FIFO message from %s, of type %d, (endian %d) to %d groups (%d bytes)\n",
                     ddp_ptr->ddp_name, sender, mess_type, endian_mismatch, num_groups, ret);
			ddp_ptr->ddp_len = ret;	 
			ret = ddp_reg_msg(sender, ddp_ptr, mess_type);
        } else {
			USRDEBUG("received non SAFE/FIFO message\n");
            ret = OK;
		}
    } else if( Is_membership_mess( service_type ) )	{
        ret = SP_get_memb_info( ddp_ptr->ddp_mess_in, service_type, &ddp_ptr->ddp_memb_info );
        if (ret < 0) {
            USRDEBUG("BUG: membership message does not have valid body\n");
            SP_error( ret );
            ERROR_PRINT(ret);
            pthread_exit(NULL);
        }
        
        if  ( Is_reg_memb_mess( service_type ) ) {
            USRDEBUG("%s: Received REGULAR membership for group %s with %d members, where I am member %d:\n",
                     ddp_ptr->ddp_name, sender, num_groups, mess_type );
        }
        
        if( Is_caused_join_mess( service_type ) )	{
            /*----------------------------------------------------------------------------------------------------
			*   JOIN: The group has a new DDM  member
			*----------------------------------------------------------------------------------------------------*/
            ddp_udt_members(ddp_ptr,target_groups,num_groups);
            
            USRDEBUG("%s: Due to the JOIN of %s service_type=%d\n", 
                     ddp_ptr->ddp_name, ddp_ptr->ddp_memb_info.changed_member, service_type );
            
            ret = ddp_join(ddp_ptr,num_groups);
        }else if( Is_caused_leave_mess( service_type ) 
                 ||  Is_caused_disconnect_mess( service_type ) ){
            /*----------------------------------------------------------------------------------------------------
			*   LEAVE or DISCONNECT:  A member has left the group
			*----------------------------------------------------------------------------------------------------*/
            ddp_udt_members(ddp_ptr,target_groups,num_groups);
            
            USRDEBUG("%s: Due to the LEAVE or DISCONNECT of %s\n", 
                     ddp_ptr->ddp_name, ddp_ptr->ddp_memb_info.changed_member );
            
            ret = ddp_leave(ddp_ptr,num_groups);
        }else if( Is_caused_network_mess( service_type ) ){
      /*----------------------------------------------------------------------------------------------------
            *   NETWORK CHANGE:  A network partition or a dead deamon
            *----------------------------------------------------------------------------------------------------*/
            USRDEBUG("%s: Due to NETWORK change with %u VS sets\n", 
                     ddp_ptr->ddp_name,  ddp_ptr->ddp_num_vs_sets);
            
            ret = ddp_network(ddp_ptr);
            
            //@TODO: delete this? Not sure if it they might be useful in the future
        }else if( Is_transition_mess(   service_type ) ) {
            USRDEBUG("received TRANSITIONAL membership for group %s\n", sender );
            if( Is_caused_leave_mess( service_type ) ){
                USRDEBUG("received membership message that left group %s\n", sender );
            }else {
                USRDEBUG("received incorrecty membership message of type 0x%x\n", service_type );
            }
        } else if ( Is_reject_mess( service_type ) )      {
            USRDEBUG("REJECTED message from %s, of servicetype 0x%x messtype %d, (endian %d) to %d groups \n(%d bytes): %s\n",
                     sender, service_type, mess_type, endian_mismatch, num_groups, ret, ddp_ptr->ddp_mess_in );
        }else {
            USRDEBUG("received message of unknown message type 0x%x with ret %d\n", service_type, ret);
        }
    }
    if(ret < 0) ERROR_RETURN(ret);
    return(ret);
}

/*===========================================================================*
*				get_nodeid				     *
* It converts a node string provided by SPREAD into a NODEID
* format of mbr_string "#DDx.<nodeid>#nodename" 
*===========================================================================*/
int get_nodeid(char *mbr_string)
{
    char *s_ptr, *n_ptr, *dot_ptr;
    int nid, len;
    
    USRDEBUG("mbr_string=%s\n", mbr_string);

    dot_ptr = strchr(mbr_string, '.'); 
    assert(dot_ptr != NULL);
    n_ptr = dot_ptr+1;					
    
    s_ptr = strchr(n_ptr, '#'); 
    assert(s_ptr != NULL);
    
    *s_ptr = '\0';
    nid = atoi(n_ptr);
    *s_ptr = '#';
    USRDEBUG("mbr_string=%s nid=%d\n", mbr_string,  nid );
    
	assert( nid >= 0 && nid < NR_NODES);
    return(nid);
}

/*===========================================================================*
*				ddp_reg_msg				     *
* Handle REGULAR messages 
*===========================================================================*/
int ddp_reg_msg(char *sender_ptr, ddp_t *ddp_ptr, int16 msg_type)
{
	int nodeid, total, bytes;
	int rcode;
	char *ptr;

	nodeid = get_nodeid(sender_ptr);
    USRDEBUG("sender_ptr=%s nodeid=%d msg_type=%d\n", 
				sender_ptr,  nodeid, msg_type);

	// ignore other self sent messages 
	if( nodeid == ddp_ptr->ddp_nodeid) return(OK);

	// check that the sender is an initialized agent 
	if (TEST_BIT(ddp_ptr->ddp_bm_init, nodeid) == 0 ){
		fprintf( stderr,"ddp_reg_msg: nodeid %d is not initialized !\n",
				nodeid);
		// check if it is a valid server 
		SET_BIT(ddp_ptr->ddp_bm_init, nodeid);
		ddp_ptr->ddp_nr_init++;
        USRDEBUG(DDP_FORMAT, DDP_FIELDS(ddp_ptr));
	}

	switch (msg_type){
		case MT_HELLO_MONITOR:
			m_ptr = ddp_ptr->ddp_mess_in;
			assert(m_ptr->m_source == nodeid);
			assert(m_ptr->m_type   == msg_type);
			// check that sender's name be the Monitor name 
			if( strncmp(sender_ptr, DDA_SHARP, strlen(DDA_SHARP)) != 0){
				fprintf( stderr,"ddp_reg_msg: MT_HELLO_MONITOR message sent by %s\n",
					 sender_ptr);
				ERROR_PRINT(OK);					
			}
			// check message len 
			if( ddp_ptr->ddp_len < sizeof(message)){
				fprintf( stderr,"ddp_reg_msg: MT_HELLO_MONITOR bad message size=%d (must be %d)\n",
					 ddp_ptr->ddp_len, sizeof(message));
				ERROR_PRINT(OK);		
			}
			m_ptr = ddp_ptr->ddp_mess_in;
			USRDEBUG(MSG1_FORMAT, MSG1_FIELDS(m_ptr) );	
			// check message source 
			if( m_ptr->m_source != nodeid){
				fprintf( stderr,"ddp_reg_msg: MT_HELLO_MONITOR m_source(%d) != sender nodeid(%d)\n",
					 m_ptr->m_source, nodeid);
				ERROR_PRINT(OK);		
			}
			// check correct message type 
			if( m_ptr->m_type != msg_type){
				fprintf( stderr,"ddp_reg_msg: MT_HELLO_MONITOR m_type(%d) != msg_type(%d)\n",
					 m_ptr->m_type, msg_type);
				ERROR_PRINT(OK);		
			}
			if(TEST_BIT(ddp_ptr->ddp_bm_init, nodeid) == 0){	
				SET_BIT(ddp_ptr->ddp_bm_init, nodeid);
				ddp_ptr->ddp_nr_init++;
				USRDEBUG(DDP_FORMAT, DDP_FIELDS(ddp_ptr));
			}			
			USRDEBUG("MT_HELLO_MONITOR received from %d\n",nodeid);
			MTX_LOCK(ddp_ptr->ddp_mutex);
			COND_SIGNAL(ddp_ptr->ddp_cond);
			MTX_UNLOCK(ddp_ptr->ddp_mutex);			
			break;
		case MT_OUT2MONITOR:
			// check that sender's name be the Monitor name 
			if( strncmp(sender_ptr, DDA_SHARP, strlen(DDA_SHARP)) != 0){
				fprintf( stderr,"ddp_reg_msg: MT_OUT2MONITOR message sent by %s\n",
					 sender_ptr);
				ERROR_PRINT(OK);					
			}
			// check message len 
			if( ddp_ptr->ddp_len >= MAX_MESSLEN-1 ){
				fprintf( stderr,"ddp_reg_msg: MT_OUT2MONITOR bad message size=%d (must <= %d)\n",
					 ddp_ptr->ddp_len, MAX_MESSLEN);
				ERROR_PRINT(OK);		
			}
			if(TEST_BIT(ddp_ptr->ddp_bm_init, nodeid) == 0){
				fprintf( stderr,"ddp_reg_msg: MT_OUT2MONITOR node %d is not initilized\n",
					 nodeid);
				ERROR_PRINT(OK);		
			}		
			USRDEBUG("MT_OUT2MONITOR: ddp_len=%d\n",ddp_ptr->ddp_len);		
			printf("output:\n%s\n",ddp_ptr->ddp_mess_in);
			
			m_ptr = &m_out;
			m_ptr->m_source = SOURCE_PROXY;
			m_ptr->m_type   = MT_OUT2MONITOR;
			m_ptr->m1_i1	= ddp_ptr->ddp_len;
			m_ptr->m1_i2	= ddp_ptr->ddp_seq;
			m_ptr->m1_i3	= 0;
			m_ptr->m1_p1	= NULL;
			m_ptr->m1_p2	= NULL;
			m_ptr->m1_p2	= NULL;
			USRDEBUG("OUTCMD HEADER: " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
			USRDEBUG("Writing OUTCMD HEADER to pipe %s\n", ddp_ptr->ddp_OUTCMDfifo);
			bytes = write( ddp_ptr->ddp_OUTCMDfd, m_ptr, sizeof(message));
			if( bytes != sizeof(message)){
				fprintf( stderr,"main: PROXY write OUTCMD HEADER  to pipe errno=%d\n",-errno);
				ERROR_EXIT(-errno);					
			}

			if( ddp_ptr->ddp_len != 0) {
				USRDEBUG("Writing OUTCMD to pipe %s\n", ddp_ptr->ddp_OUTCMDfifo);
				bytes = write( ddp_ptr->ddp_OUTCMDfd, ddp_ptr->ddp_mess_in, ddp_ptr->ddp_len);
				if( bytes != ddp_ptr->ddp_len){
					fprintf( stderr,"main: PROXY write OUTCMD to pipe errno=%d\n",-errno);
					ERROR_EXIT(-errno);					
				}
				USRDEBUG("Command bytes=%d ddp_mess_in:%s\n", bytes, ddp_ptr->ddp_mess_in);
			}
			USRDEBUG("MT_OUT2MONITOR: ddp_len=%d\n", ddp_ptr->ddp_len);	
			ddp_ptr->ddp_seq++;
			
			// WAKEUP MAIN THREAD 
			MTX_LOCK(ddp_ptr->ddp_mutex);
			COND_SIGNAL(ddp_ptr->ddp_cond);
			MTX_UNLOCK(ddp_ptr->ddp_mutex);
		
			break;
		case (DVK_BIND| MASK_ACKNOWLEDGE): 
			// check that sender's name be the Monitor name 
			if( strncmp(sender_ptr, DDA_SHARP, strlen(DDA_SHARP)) != 0){
				fprintf( stderr,"ddp_reg_msg: DVK_BIND ACK sent by %s\n",
					 sender_ptr);
				ERROR_PRINT(OK);					
			}
			// check message len 
			if( ddp_ptr->ddp_len != sizeof(message) ){
				fprintf( stderr,"ddp_reg_msg: DVK_BIND ACK bad message size=%d (must <= %d)\n",
					 ddp_ptr->ddp_len, sizeof(message));
				ERROR_PRINT(OK);		
			}
			if(TEST_BIT(ddp_ptr->ddp_bm_init, nodeid) == 0){
				fprintf( stderr,"ddp_reg_msg: DVK_BIND ACK node %d is not initilized\n",
					 nodeid);
				ERROR_PRINT(OK);		
			}		
			USRDEBUG("DVK_BIND ACK: ddp_len=%d\n",ddp_ptr->ddp_len);		
			m_ptr = ddp_ptr->ddp_mess_in;
			USRDEBUG("API BIND ACK (%d): " MSGC_FORMAT, sizeof(message), MSGC_FIELDS(m_ptr));	
			USRDEBUG("Writing API BIND ACK to pipe %s\n", ddp_ptr->ddp_OUTCMDfifo);
			bytes = write( ddp_ptr->ddp_OUTCMDfd, m_ptr, sizeof(message));
			if( bytes != sizeof(message)){
				fprintf( stderr,"main: PROXY write API BIND ACK  to pipe errno=%d\n",-errno);
				ERROR_EXIT(-errno);					
			}
			// WAKEUP MAIN THREAD 
			MTX_LOCK(ddp_ptr->ddp_mutex);
			COND_SIGNAL(ddp_ptr->ddp_cond);
			MTX_UNLOCK(ddp_ptr->ddp_mutex);
			break;	
		default:
			fprintf( stderr,"ddp_reg_msg: Invalid msg_type %d\n", msg_type);
			assert(FALSE);
			break;
	}
	
	return(OK);
}

/*===========================================================================*
*				ddp_join				     *
* Handle JOIN 
*===========================================================================*/
int ddp_join(ddp_t *ddp_ptr,int num_groups)
{
	int nodeid;

    USRDEBUG("member=%s\n",ddp_ptr->ddp_memb_info.changed_member);
    nodeid  = get_nodeid((char *)ddp_ptr->ddp_memb_info.changed_member);
	
	// the monitor joint after agents 
	if( nodeid == ddp_ptr->ddp_nodeid) {
		if( ddp_ptr->ddp_nr_nodes > 1) {
			mcast_hello_agents(ddp_ptr);
		}
		USRDEBUG(DDP_FORMAT, DDP_FIELDS(ddp_ptr));
		ddp_ptr->ddp_nr_init++;
		SET_BIT(ddp_ptr->ddp_bm_init, nodeid);
		return(OK);
	}
	
    if ( strncmp(ddp_ptr->ddp_memb_info.changed_member, DDA_SHARP, strlen(DDA_SHARP)) == 0) {
		mcast_hello_agents(ddp_ptr);
    }
    USRDEBUG(DDP_FORMAT, DDP_FIELDS(ddp_ptr));

	return(OK);
}

/*===========================================================================*
*				ddp_leave				     *
* Handle LEAVE OR DISCONNECT
*===========================================================================*/
int  ddp_leave(ddp_t *ddp_ptr,int num_groups)
{
	int nodeid;
	
    USRDEBUG("member=%s\n",ddp_ptr->ddp_memb_info.changed_member);
    nodeid  = get_nodeid((char *)ddp_ptr->ddp_memb_info.changed_member);

    if ( strncmp(ddp_ptr->ddp_memb_info.changed_member, DDA_SHARP, strlen(DDA_SHARP)) == 0) {
		// ignore self messages
		if( nodeid == ddp_ptr->ddp_nodeid) return(OK);
		// uninitialized agent 
		if( TEST_BIT(ddp_ptr->ddp_bm_init, nodeid) == 0) return(OK);
        CLR_BIT(ddp_ptr->ddp_bm_init, nodeid);
		ddp_ptr->ddp_nr_init--;
    }
    USRDEBUG(DDP_FORMAT, DDP_FIELDS(ddp_ptr));
	
	return(OK);
}
	
/*===========================================================================*
*				ddp_network				     *
* Handle NETWORK event
*===========================================================================*/
int ddp_network(ddp_t* ddp_ptr)
{
    USRDEBUG("\n");
	
	int i, j;
    int ret = 0;
	int nodeid;
	int	bm_nodes;
	int	nr_nodes;
	int	bm_init;
	int	nr_init;
	
    USRDEBUG("\n");
	
    ddp_ptr->ddp_num_vs_sets = 
        SP_get_vs_sets_info(ddp_ptr->ddp_mess_in, 
                            &ddp_ptr->ddp_vssets[0], 
                            MAX_VSSETS, 
                            &ddp_ptr->ddp_my_vsset_index);
    
    if (ddp_ptr->ddp_num_vs_sets < 0) {
        USRDEBUG("BUG: membership message has more then %d vs sets."
                 "Recompile with larger MAX_VSSETS\n",
                 MAX_VSSETS);
        SP_error( ddp_ptr->ddp_num_vs_sets );
        ERROR_EXIT( ddp_ptr->ddp_num_vs_sets );
    }
    if (ddp_ptr->ddp_num_vs_sets == 0) {
        USRDEBUG("BUG: membership message has %d vs_sets\n", 
                 ddp_ptr->ddp_num_vs_sets);
        SP_error( ddp_ptr->ddp_num_vs_sets );
        ERROR_EXIT( EDVSGENERIC );
    }
        
	nr_nodes = 1; // the monitor 
	nr_init  = 1; // the monitor 
	bm_nodes = 0;
	bm_init  = 0;
	SET_BIT(bm_nodes, ddp_ptr->ddp_nodeid); // the monitor 
	SET_BIT(bm_init,  ddp_ptr->ddp_nodeid); // the monitor
		
    for(i = 0; i < ddp_ptr->ddp_num_vs_sets; i++ )  {
        USRDEBUG("%s VS set %d has %u members:\n",
                 (i  == ddp_ptr->ddp_my_vsset_index) ?("LOCAL") : ("OTHER"), 
                 i, ddp_ptr->ddp_vssets[i].num_members );
        ret = SP_get_vs_set_members(ddp_ptr->ddp_mess_in, &ddp_ptr->ddp_vssets[i], ddp_ptr->ddp_members, MAX_MEMBERS);
        if (ret < 0) {
            USRDEBUG("VS Set has more then %d members. Recompile with larger MAX_MEMBERS\n", MAX_MEMBERS);
            SP_error( ret );
            ERROR_EXIT( ret);
        }
        
        /*---------------------------------------------
		* get the bitmap of current members
		--------------------------------------------- */
         for(j = 0; j < ddp_ptr->ddp_vssets[i].num_members; j++ ) {
            USRDEBUG("\t%s\n", ddp_ptr->ddp_members[j] );
			// only get the AGENTS
            if ( strncmp(ddp_ptr->ddp_members[j], DDA_SHARP, strlen(DDA_SHARP)) == 0) {	
                nodeid = get_nodeid(ddp_ptr->ddp_members[j]);
                SET_BIT(bm_nodes, nodeid);
                nr_nodes++;
                if(TEST_BIT(ddp_ptr->ddp_bm_init, nodeid) == 0) {
					SET_BIT(bm_init, nodeid);
					nr_init++;
				}
			}
		}
    }
	
    USRDEBUG("OLD " DDP_FORMAT, DDP_FIELDS(ddp_ptr));	

	if( ddp_ptr->ddp_nr_nodes > nr_nodes){
	    USRDEBUG("NETWORK PARTITION \n");		
	}else{
	    USRDEBUG("NETWORK MERGE \n");		
	}
		
#ifdef ANULADO		
	for( i = 0; i < sizeof(ddp_ptr->ddp_bm_init)*8; i++){
		// the agent was on the old bitmap 
        if(TEST_BIT(ddp_ptr->ddp_bm_init, i) != 0) {
			// but it is not in the current bitmap 
			if(TEST_BIT(bm_init, i) == 0) {
				clear_session(i);
			}
		}
	}
#endif // ANULADO		
	
	ddp_ptr->ddp_bm_init   = bm_init;
	ddp_ptr->ddp_bm_nodes  = bm_nodes;
	ddp_ptr->ddp_nr_init   = nr_init;
	ddp_ptr->ddp_nr_nodes  = nr_nodes;
	
	mcast_hello_agents(ddp_ptr);

    USRDEBUG("NEW " DDP_FORMAT, DDP_FIELDS(ddp_ptr));

    return(OK);
}

/*===========================================================================*
*				mcast_hello_agents				     *
* Multilcast HELLO to new agent
*===========================================================================*/
void mcast_hello_agents(ddp_t *ddp_ptr)
{
	m_ptr = &m_out;
	m_ptr->m_source = local_nodeid;
	m_ptr->m_type   = MT_HELLO_AGENTS;
	m_ptr->m1_i1	= ddp_ptr->ddp_nr_nodes;
	m_ptr->m1_i2	= ddp_ptr->ddp_nr_init;
	m_ptr->m1_i3	= 0;
	m_ptr->m1_p1	= NULL;
	m_ptr->m1_p2	= NULL;
	m_ptr->m1_p3	= NULL;

    USRDEBUG(MSG1_FORMAT, MSG1_FIELDS(m_ptr));	
				  
	SP_multicast(ddp_ptr->ddp_mbox, FIFO_MESS, SPREAD_GROUP,
					MT_HELLO_AGENTS , sizeof(message), (char *) m_ptr);
}

/*===========================================================================*
*				ddp_udt_members				     *
*===========================================================================*/
void ddp_udt_members(ddp_t* ddp_ptr,char target_groups[MAX_MEMBERS][MAX_GROUP_NAME],
                    int num_groups)
{	
	int nodeid;
	
	ddp_ptr->ddp_bm_nodes = 0;
	ddp_ptr->ddp_nr_nodes = 0;
    ddp_ptr->ddp_sp_nr_mbrs = num_groups;
    memcpy((void*) ddp_ptr->ddp_sp_members, (void *) target_groups, ddp_ptr->ddp_sp_nr_mbrs*MAX_GROUP_NAME);
    for(int i=0; i < ddp_ptr->ddp_sp_nr_mbrs; i++ ){
		nodeid = get_nodeid(&ddp_ptr->ddp_sp_members[i][0]);
        USRDEBUG("\t%s nodeid=%d\n", &ddp_ptr->ddp_sp_members[i][0], nodeid);
		SET_BIT(ddp_ptr->ddp_bm_nodes, nodeid);
		ddp_ptr->ddp_nr_nodes++;
        USRDEBUG(DDP_FORMAT, DDP_FIELDS(ddp_ptr));
    }
}

