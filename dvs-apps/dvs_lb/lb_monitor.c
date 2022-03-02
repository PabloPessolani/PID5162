/****************************************************************/
/* 				LB_monitor							*/
/* LOAD BALANCER MONITOR 				 		*/
/****************************************************************/

#define _GNU_SOURCE     
#define _MULTI_THREADED
#include "lb_dvs.h"

#ifdef SPREAD_MONITOR 

int lbm_reg_msg(char *sender_ptr, lb_t *lb_ptr, int16 msg_type);

/*===========================================================================*
 *				bm32ascii		 			   				     *
 *===========================================================================*/
void bm32ascii(char *buf, unsigned long int bitmap)
{
	int i;
	unsigned long int mask;

	mask = 0x80000000; 
	for( i = 0; i < (sizeof(unsigned long) * 8) ; i++) {
//		*buf++ = (bitmap & mask)?'X':'-';
		*buf++ = (bitmap & mask)?'x': (((31-i)%10)+'0');
		mask =  (mask >> 1);		
	}
	*buf = '\0';
}

/*===========================================================================*
 *				   set_server_bm 				    					 *
 *===========================================================================*/
void set_server_bm(server_t *svr_ptr)
{
	int i, j; 
	service_t *svc_ptr;
	static char bmbuf[(sizeof(unsigned long)*8)+1];

	for( i = 0; i < lb.lb_nr_services; i++){
		svc_ptr = &service_tab[i];
		if( svc_ptr->svc_extep == LB_INVALID) 	continue;
		SET_BIT(svr_ptr->svr_bm_svc, svc_ptr->svc_extep);
		for(j = svc_ptr->svc_minep; j < svc_ptr->svc_maxep; j++)
			SET_BIT(svr_ptr->svr_bm_svc, j);
		bm32ascii(bmbuf, svr_ptr->svr_bm_svc);
		USRDEBUG("svr_nodeid=%d svr_bm_svc=%08X [%-32s]\n",
			svr_ptr->svr_nodeid, svr_ptr->svr_bm_svc, bmbuf);
	}
}		

/*===========================================================================*
 *				   lb_monitor 				    					 *
 *===========================================================================*/
void *lb_monitor(void *arg)
{
	int rcode;
    lb_t *lb_ptr;
	
	lb_ptr = &lb;
	
	USRDEBUG("LB MONITOR: Initializing...\n");
	
	init_control_vars(lb_ptr);
    connect_to_spread(lb_ptr);		//establish connection to spread network and joins a group (fixed at lb_ptr->lb_sp_group)
    
    while(TRUE){
        USRDEBUG(LB1_FORMAT, LB1_FIELDS(lb_ptr));
        USRDEBUG(LB2_FORMAT, LB2_FIELDS(lb_ptr));
        
        rcode = lbm_SP_receive(lb_ptr);		//loop receiving messages
        USRDEBUG("rcode=%d\n", rcode);
        if(rcode < 0 ) {
            ERROR_PRINT(rcode);
            sleep(LB_TIMEOUT_5SEC);		
            if( rcode == EDVSNOTCONN) {
                connect_to_spread(lb_ptr);
            }
        }
    }
    
    pthread_exit(NULL);
	USRDEBUG("LB MONITOR: Exiting...\n");
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
    
    lb_timeout.sec  = LB_TIMEOUT_5SEC;
    lb_timeout.usec = LB_TIMEOUT_MSEC;
    
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
void connect_to_spread(lb_t *lb_ptr)
{
    int rcode;
    /*------------------------------------------------------------------------------------
	* lb_mbr_name:  it must be unique in the spread node.
	*--------------------------------------------------------------------------------------*/
    sprintf(lb_ptr->lb_sp_group, "%s", SPREAD_GROUP);
    USRDEBUG("spread_group=%s\n", lb_ptr->lb_sp_group);
    sprintf( Spread_name, "4803");
	
	sprintf( lb_ptr->lb_mbr_name, "LBM.%02d", lb_ptr->lb_nodeid);
    USRDEBUG("lb_mbr_name=%s\n", lb_ptr->lb_mbr_name);
    
    rcode = SP_connect_timeout( Spread_name,  lb_ptr->lb_mbr_name , 0, 1, 
                               &lb_ptr->lb_mbox, lb_ptr->lb_priv_group, lb_timeout);
							   
    if( rcode != ACCEPT_SESSION ) 	{
        SP_error (rcode);
        ERROR_PRINT(rcode);
        pthread_exit(NULL);
    }
    
    USRDEBUG("lb_mbr_name %s: connected to %s with private group %s\n",
             lb_ptr->lb_mbr_name, Spread_name, lb_ptr->lb_priv_group);
    
    rcode = SP_join( lb_ptr->lb_mbox, lb_ptr->lb_sp_group);
    if( rcode){
        SP_error (rcode);
        ERROR_PRINT(rcode);
        pthread_exit(NULL);
    }
}

/*===========================================================================*
 *				init_control_vars				     
 * nitilize several global and replicated  variables
 *===========================================================================*/
void init_control_vars(lb_t *lb_ptr)
{	
    lb_ptr->lb_bm_active 	= 0;
    lb_ptr->lb_bm_init		= 0;	
    lb_ptr->lb_nr_active 	= 0;
    lb_ptr->lb_nr_init		= 0;
}

/*===========================================================================*
 *				lbm_SP_receive				    
 *===========================================================================*/
int lbm_SP_receive(lb_t	*lb_ptr)
{
    char        sender[MAX_GROUP_NAME];
    char        target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
    int         num_groups = -1;
    int         service_type = 0;
    int16       mess_type;
    int         endian_mismatch;
    int         ret;
    SP_message *sp_ptr;
    
    USRDEBUG("%s\n", lb_ptr->lb_name);
    
    assert(lb_ptr->lb_mess_in != NULL);
    
    
    memset(lb_ptr->lb_mess_in,0,MAX_MESSLEN);
    //Returns message size in non-error
    ret = SP_receive( lb_ptr->lb_mbox, &service_type, sender, 100, &num_groups, target_groups,
                     &mess_type, &endian_mismatch, MAX_MESSLEN, lb_ptr->lb_mess_in );
    USRDEBUG("ret=%d\n", ret);
    
    if( ret < 0 ){
        if ( (ret == GROUPS_TOO_SHORT) || (ret == BUFFER_TOO_SHORT) ) {
            service_type = DROP_RECV;
            USRDEBUG("\n========Buffers or Groups too Short=======\n");
            ret = SP_receive( lb_ptr->lb_mbox, &service_type, sender, 
                             MAX_MEMBERS, &num_groups, target_groups,
                             &mess_type, &endian_mismatch, MAX_MESSLEN, lb_ptr->lb_mess_in);
        }
    }
    
    if (ret < 0 ) {
        SP_error( ret );
        ERROR_PRINT(ret);
        pthread_exit(NULL);
    }
    
    USRDEBUG("%s: sender=%s Private_group=%s service_type=%d\n", 
             lb_ptr->lb_name,
             sender, 
             lb_ptr->lb_priv_group, 
             service_type);
    
    sp_ptr = (SP_message *) lb_ptr->lb_mess_in;
    
    if( Is_regular_mess( service_type ) )	{
        if( Is_fifo_mess(service_type) ) {
            USRDEBUG("%s: FIFO message from %s, of type %d, (endian %d) to %d groups (%d bytes)\n",
                     lb_ptr->lb_name, sender, mess_type, endian_mismatch, num_groups, ret);
			lb_ptr->lb_len = ret;
			ret = lbm_reg_msg(sender, lb_ptr, mess_type);
        } else {
			USRDEBUG("received non SAFE/FIFO message\n");
            ret = OK;
		}
    } else if( Is_membership_mess( service_type ) )	{
        ret = SP_get_memb_info( lb_ptr->lb_mess_in, service_type, &lb_ptr->lb_memb_info );
        if (ret < 0) {
            USRDEBUG("BUG: membership message does not have valid body\n");
            SP_error( ret );
            ERROR_PRINT(ret);
            pthread_exit(NULL);
        }
        
        if  ( Is_reg_memb_mess( service_type ) ) {
            USRDEBUG("%s: Received REGULAR membership for group %s with %d members, where I am member %d:\n",
                     lb_ptr->lb_name, sender, num_groups, mess_type );
        }
        
        if( Is_caused_join_mess( service_type ) )	{
            /*----------------------------------------------------------------------------------------------------
			*   JOIN: The group has a new LBM  member
			*----------------------------------------------------------------------------------------------------*/
            lbm_udt_members(lb_ptr,target_groups,num_groups);
            
            USRDEBUG("%s: Due to the JOIN of %s service_type=%d\n", 
                     lb_ptr->lb_name, lb_ptr->lb_memb_info.changed_member, service_type );
            
            ret = lbm_join(lb_ptr,num_groups);
        }else if( Is_caused_leave_mess( service_type ) 
                 ||  Is_caused_disconnect_mess( service_type ) ){
            /*----------------------------------------------------------------------------------------------------
			*   LEAVE or DISCONNECT:  A member has left the group
			*----------------------------------------------------------------------------------------------------*/
            lbm_udt_members(lb_ptr,target_groups,num_groups);
            
            USRDEBUG("%s: Due to the LEAVE or DISCONNECT of %s\n", 
                     lb_ptr->lb_name, lb_ptr->lb_memb_info.changed_member );
            
            ret = lbm_leave(lb_ptr,num_groups);
        }else if( Is_caused_network_mess( service_type ) ){
      /*----------------------------------------------------------------------------------------------------
            *   NETWORK CHANGE:  A network partition or a dead deamon
            *----------------------------------------------------------------------------------------------------*/
            USRDEBUG("%s: Due to NETWORK change with %u VS sets\n", 
                     lb_ptr->lb_name,  lb_ptr->lb_num_vs_sets);
            
            ret = lbm_network(lb_ptr);
            
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
                     sender, service_type, mess_type, endian_mismatch, num_groups, ret, lb_ptr->lb_mess_in );
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
* format of mbr_string "#LBx.<nodeid>#nodename" 
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
    
	assert( nid >= 0 && nid < dvs_ptr->d_nr_nodes);
    return(nid);
}

/*===========================================================================*
*				lbm_reg_msg				     *
* Handle REGULAR messages 
*===========================================================================*/
int lbm_reg_msg(char *sender_ptr, lb_t *lb_ptr, int16 msg_type)
{
	int agent_id;
	int rcode;
	message *m_ptr;
	server_t *svr_ptr;
	client_t *clt_ptr;
	
	MTX_LOCK(lb_ptr->lb_mtx);
	m_ptr = lb_ptr->lb_mess_in;
	agent_id = get_nodeid(sender_ptr);
    USRDEBUG("sender_ptr=%s agent_id=%d msg_type=%d\n", 
				sender_ptr,  agent_id, msg_type);

	// self sent message 
	if ( msg_type == MT_LOAD_THRESHOLDS){
		lb_ptr->lb_nr_init = lb_ptr->lb_nr_active;
		lb_ptr->lb_bm_init = lb_ptr->lb_bm_active;
		goto unlock_ok;		
	}
	
	// ignore other self sent messages 
	if( agent_id == lb_ptr->lb_nodeid) {
		goto unlock_ok;		
	}
	
	// check that the sender is an initialized agent 
	if (TEST_BIT(lb_ptr->lb_bm_init, agent_id) == 0 ){
		fprintf( stderr,"lbm_reg_msg: agent_id %d is not initialized !\n",
				agent_id);
		// check if it is a valid server 
		SET_BIT(lb_ptr->lb_bm_init, agent_id);
		lb_ptr->lb_nr_init++;
        USRDEBUG(LB2_FORMAT, LB2_FIELDS(lb_ptr));
#ifdef NOT_USED		
		svr_ptr = &server_tab[agent_id];	
		MTX_LOCK(svr_ptr->svr_mutex);
		set_server_bm(svr_ptr);
		MTX_UNLOCK(svr_ptr->svr_mutex);
#endif // NOT_USED		
	}

    USRDEBUG("msg_type=%0X\n",msg_type);
	switch (msg_type){
		case MT_LOAD_LEVEL:
			// check that sender's name be an AGENT name 
			if( strncmp(sender_ptr, LBA_SHARP, strlen(LBA_SHARP)) != 0){
				fprintf( stderr,"lba_reg_msg: MT_LOAD_LEVEL message sent by %s\n",
					 sender_ptr);
				goto unlock_ok;		
			}
			// check message len 
			if( lb_ptr->lb_len < sizeof(message)){
				fprintf( stderr,"lb_reg_msg: bad message size=%d (must be %d)\n",
					 lb_ptr->lb_len, sizeof(message));
				goto unlock_ok;		
			}
			m_ptr = lb_ptr->lb_mess_in;
			USRDEBUG(MSG9_FORMAT, MSG9_FIELDS(m_ptr) );	
			// check message source 
			if( m_ptr->m_source != agent_id){
				fprintf( stderr,"lb_reg_msg: m_source(%d) != sender agent_id(%d)\n",
					 m_ptr->m_source, agent_id);
				goto unlock_ok;		
			}
			// check correct message type 
			if( m_ptr->m_type != msg_type){
				fprintf( stderr,"lb_reg_msg: m_type(%d) != msg_type(%d)\n",
					 m_ptr->m_type, msg_type);
				goto unlock_ok;		
			}
			assert(m_ptr->m_source == agent_id);
			lbm_lvlchg_msg(m_ptr);
			svr_ptr = &server_tab[agent_id];
			if(TEST_BIT(lb_ptr->lb_bm_init, agent_id) == 0){	
				SET_BIT(lb_ptr->lb_bm_init, agent_id);
				lb_ptr->lb_nr_init++;
				USRDEBUG(LB2_FORMAT, LB2_FIELDS(lb_ptr));
#ifdef NOT_USED					
				MTX_LOCK(svr_ptr->svr_mutex);
				set_server_bm(svr_ptr);
				MTX_UNLOCK(svr_ptr->svr_mutex);	
#endif // NOT_USED		
				
			}
			goto unlock_ok;
			break;
		case MT_CLT_WAIT_BIND | MT_ACKNOWLEDGE:
		case MT_CLT_WAIT_UNBIND | MT_ACKNOWLEDGE:
		case MT_SVR_WAIT_UNBIND | MT_ACKNOWLEDGE:
			// check that sender's name be an AGENT name 
			if( strncmp(sender_ptr, LBA_SHARP, strlen(LBA_SHARP)) != 0){
				fprintf( stderr,"lba_reg_msg: MT_ACKNOWLEDGE message sent by %s\n",
					 sender_ptr);
				goto unlock_ok;		
			}
			// check message len 
			if( lb_ptr->lb_len < sizeof(message)){
				fprintf( stderr,"lb_reg_msg: bad MT_ACKNOWLEDGE message size=%d (must be %d)\n",
					 lb_ptr->lb_len, sizeof(message));
				goto unlock_ok;		
			}
			m_ptr = lb_ptr->lb_mess_in;
			USRDEBUG(MSG1_FORMAT, MSG1_FIELDS(m_ptr) );	
			// check message source 
			if( m_ptr->m_source != agent_id){
				fprintf( stderr,"lb_reg_msg: MT_ACKNOWLEDGE m_source(%d) != sender agent_id(%d)\n",
					 m_ptr->m_source, agent_id);
				goto unlock_ok;		
			}
			// check correct message type 
			if( m_ptr->m_type != msg_type){
				fprintf( stderr,"lb_reg_msg: MT_ACKNOWLEDGE m_type(%d) != msg_type(%d)\n",
					 m_ptr->m_type, msg_type);
				goto unlock_ok;		
			}
			assert(m_ptr->m_source == agent_id);
			if( (msg_type == (MT_CLT_WAIT_BIND | MT_ACKNOWLEDGE))
			|| ( msg_type == (MT_CLT_WAIT_UNBIND | MT_ACKNOWLEDGE))){
				clt_ptr = (client_t*) m_ptr->m1_p1;
				MTX_LOCK(clt_ptr->clt_agent_mtx);
				COND_SIGNAL(clt_ptr->clt_agent_cond);
				MTX_UNLOCK(clt_ptr->clt_agent_mtx);	
			}else{ // MT_SVR_WAIT_UNBIND | MT_ACKNOWLEDGE
				svr_ptr = (server_t*) m_ptr->m1_p1;
				MTX_LOCK(svr_ptr->svr_agent_mtx);
				COND_SIGNAL(svr_ptr->svr_agent_cond);
				MTX_UNLOCK(svr_ptr->svr_agent_mtx);					
			}
			goto unlock_ok;
			break;			
		default:
			fprintf( stderr,"lbm_reg_msg: Invalid msg_type %d\n", msg_type);
			assert(FALSE);
			break;
	}
unlock_ok:	
	MTX_UNLOCK(lb_ptr->lb_mtx);
	return(OK);
}

/*===========================================================================*
*				lbm_lvlchg_msg				     *
* Handle LEVEL CHANGE messages from SERVER agents to LB Monitor  
*===========================================================================*/
int lbm_lvlchg_msg(message *m_ptr)
{
	int rcode;
	server_t *svr_ptr;

	USRDEBUG(MSG9_FORMAT, MSG9_FIELDS(m_ptr) );	
	
	switch(m_ptr->m1_i1) {
		case LVL_IDLE:
		case LVL_BUSY:
		case LVL_SATURATED:
			// check received LOAD VALUES  
			assert(m_ptr->m9_i1 >= LVL_IDLE && m_ptr->m9_i1<= LVL_SATURATED);
			assert(m_ptr->m9_l1 >= 0 && m_ptr->m9_l1 <= 100);
			svr_ptr = &server_tab[m_ptr->m_source];
			MTX_LOCK(svr_ptr->svr_mutex);
			svr_ptr->svr_level = m_ptr->m9_i1;
			svr_ptr->svr_load  = m_ptr->m9_l1;
			MTX_UNLOCK(svr_ptr->svr_mutex);	
			USRDEBUG(SERVER_FORMAT, SERVER_FIELDS(svr_ptr));	
			break;
		default:
			assert(FALSE); 
			break;
	}
	return(OK);
}

/*===========================================================================*
*				lbm_join				     *
* Handle JOIN 
*===========================================================================*/
int lbm_join(lb_t *lb_ptr,int num_groups)
{
	int agent_id, i, j;
    server_t *svr_ptr;

	 
    USRDEBUG("member=%s\n",lb_ptr->lb_memb_info.changed_member);
    agent_id  = get_nodeid((char *)lb_ptr->lb_memb_info.changed_member);
	
	if( agent_id == lb_ptr->lb_nodeid) {
		mcast_thresholds();
		return(OK);
	}
	
    if ( strncmp(lb_ptr->lb_memb_info.changed_member, LBA_SHARP, strlen(LBA_SHARP)) == 0) {
		// check if server has been configured 
		svr_ptr = &server_tab[agent_id];
		if( svr_ptr->svr_nodeid == LB_INVALID){
			fprintf( stderr,"WARNING:Agent of node %d is no a configured Server\n", agent_id);
			return(OK);
		}
		// The node is just started 
		MTX_LOCK(svr_ptr->svr_mutex);
		CLR_BIT(svr_ptr->svr_bm_sts, SVR_STARTING);
		// initialize timestamp of idle time 
		clock_gettime(clk_id, &svr_ptr->svr_idle_ts);
#ifdef NOT_USED		
		set_server_bm(svr_ptr);
#endif // NOT_USED		
				
		MTX_UNLOCK(svr_ptr->svr_mutex);
		mcast_thresholds();
    }
    USRDEBUG(LB2_FORMAT, LB2_FIELDS(lb_ptr));

	return(OK);
}

/*===========================================================================*
*				lbm_leave				     *
* Handle LEAVE OR DISCONNECT
*===========================================================================*/
int  lbm_leave(lb_t *lb_ptr,int num_groups)
{
	int agent_id;
	server_t *svr_ptr;
	
    USRDEBUG("member=%s\n",lb_ptr->lb_memb_info.changed_member);
    agent_id  = get_nodeid((char *)lb_ptr->lb_memb_info.changed_member);

    if ( strncmp(lb_ptr->lb_memb_info.changed_member, LBA_SHARP, strlen(LBA_SHARP)) == 0) {
		// ignore self messages
		if( agent_id == lb_ptr->lb_nodeid) return(OK);
		// uninitialized agent 
		MTX_LOCK(lb_ptr->lb_mtx);
		if( TEST_BIT(lb_ptr->lb_bm_init, agent_id) == 0){
			MTX_UNLOCK(lb_ptr->lb_mtx);
			return(OK);
		}
		clear_session(agent_id);
		clear_server(agent_id);
	    USRDEBUG("AGENT DEAD! STOP THE VM member=%s\n",lb_ptr->lb_memb_info.changed_member);
		MTX_UNLOCK(lb_ptr->lb_mtx);
    }
    USRDEBUG(LB2_FORMAT, LB2_FIELDS(lb_ptr));

	return(OK);
}

/*===========================================================================*
*				clear_session				     *
* Clear all sessions with the agent_id as a server node 
*===========================================================================*/
void  clear_session(int agent_id)
{
	sess_entry_t *sess_ptr;
	server_t *svr_ptr;
	
    USRDEBUG("agent_id=%d\n",agent_id);

	// CLEAR all sessions with this agent_id as server node 
	for (int i = 0;i < dvs_ptr->d_nr_dcs; i++){ 
		for( int j = 0; j < nr_sess_entries; j++){
			sess_ptr = (sess_entry_t *)sess_table[i].st_tab_ptr;
			if ( sess_ptr->se_clt_nodeid == LB_INVALID) continue;
			if ( sess_ptr->se_svr_nodeid == agent_id) {
				svr_ptr = &server_tab[sess_ptr->se_svr_nodeid];
				MTX_LOCK(sess_table[i].st_mutex);
				sess_ptr->se_clt_nodeid	= LB_INVALID;
				sess_ptr->se_clt_ep		= LB_INVALID;
				sess_ptr->se_clt_PID	= LB_INVALID;
				sess_ptr->se_lbclt_ep	= LB_INVALID;
				sess_ptr->se_lbsvr_ep	= LB_INVALID; 
				sess_ptr->se_svr_nodeid	= LB_INVALID;
				sess_ptr->se_svr_ep		= LB_INVALID;
				sess_ptr->se_svr_PID	= LB_INVALID;
				sess_ptr->se_service	= NULL;	
				sess_table[i].st_nr_sess--;
				MTX_UNLOCK(sess_table[i].st_mutex);
				MTX_LOCK(svr_ptr->svr_mutex);				
				CLR_BIT(svr_ptr->svr_bm_svc, j);
				MTX_UNLOCK(svr_ptr->svr_mutex);				
			}
			sess_ptr++;
		}
	}
}
	
/*===========================================================================*
*				clear_server 				     *
*===========================================================================*/
void  clear_server(int nodeid)
{
	server_t *svr_ptr;
	
    USRDEBUG("nodeid=%d\n",nodeid);

	svr_ptr = &server_tab[nodeid];
	assert(svr_ptr->svr_nodeid != LB_INVALID);

	MTX_LOCK(svr_ptr->svr_mutex);
	svr_ptr->svr_idle_count = 0;
	svr_ptr->svr_px_sts		= 0;
	svr_ptr->svr_bm_sts		= 0;
	svr_ptr->svr_bm_svc 	= 0;
	MTX_UNLOCK(svr_ptr->svr_mutex);
	USRDEBUG(SERVER_FORMAT, SERVER_FIELDS(svr_ptr));
	USRDEBUG(SERVER1_FORMAT, SERVER1_FIELDS(svr_ptr));
	
	return;
}
	
/*===========================================================================*
*				lbm_network				     *
* Handle NETWORK event
*===========================================================================*/
int lbm_network(lb_t* lb_ptr)
{
	int i, j;
    int ret = 0;
	int agent_id;
	int	bm_active;
	int	nr_active;
	int	bm_init;
	int	nr_init;
	server_t *svr_ptr;
	
    USRDEBUG("\n");
	
    lb_ptr->lb_num_vs_sets = 
        SP_get_vs_sets_info(lb_ptr->lb_mess_in, 
                            &lb_ptr->lb_vssets[0], 
                            MAX_VSSETS, 
                            &lb_ptr->lb_my_vsset_index);
    
    if (lb_ptr->lb_num_vs_sets < 0) {
        USRDEBUG("BUG: membership message has more then %d vs sets."
                 "Recompile with larger MAX_VSSETS\n",
                 MAX_VSSETS);
        SP_error( lb_ptr->lb_num_vs_sets );
        ERROR_EXIT( lb_ptr->lb_num_vs_sets );
    }
    if (lb_ptr->lb_num_vs_sets == 0) {
        USRDEBUG("BUG: membership message has %d vs_sets\n", 
                 lb_ptr->lb_num_vs_sets);
        SP_error( lb_ptr->lb_num_vs_sets );
        ERROR_EXIT( EDVSGENERIC );
    }
        
	nr_active = 1; // the monitor 
	nr_init  = 1; // the monitor 
	bm_active = 0;
	bm_init  = 0;
	SET_BIT(bm_active, lb_ptr->lb_nodeid); // the monitor 
	SET_BIT(bm_init,  lb_ptr->lb_nodeid); // the monitor
		
    for(i = 0; i < lb_ptr->lb_num_vs_sets; i++ )  {
        USRDEBUG("%s VS set %d has %u members:\n",
                 (i  == lb_ptr->lb_my_vsset_index) ?("LOCAL") : ("OTHER"), 
                 i, lb_ptr->lb_vssets[i].num_members );
        ret = SP_get_vs_set_members(lb_ptr->lb_mess_in, &lb_ptr->lb_vssets[i], lb_ptr->lb_members, MAX_MEMBERS);
        if (ret < 0) {
            USRDEBUG("VS Set has more then %d members. Recompile with larger MAX_MEMBERS\n", MAX_MEMBERS);
            SP_error( ret );
            ERROR_EXIT( ret);
        }
        
        /*---------------------------------------------
		* get the bitmap of current members
		--------------------------------------------- */
         for(j = 0; j < lb_ptr->lb_vssets[i].num_members; j++ ) {
            USRDEBUG("\t%s\n", lb_ptr->lb_members[j] );
			// only get the AGENTS
            if ( strncmp(lb_ptr->lb_members[j], LBA_SHARP, strlen(LBA_SHARP)) == 0) {	
                agent_id = get_nodeid(lb_ptr->lb_members[j]);
                SET_BIT(bm_active, agent_id);
                nr_active++;
				MTX_LOCK(lb_ptr->lb_mtx);				
                if(TEST_BIT(lb_ptr->lb_bm_init, agent_id) == 0) {
					SET_BIT(bm_init, agent_id);
					
					nr_init++;
				}
				MTX_UNLOCK(lb_ptr->lb_mtx);				
			}
		}
    }
	
    USRDEBUG("OLD " LB2_FORMAT, LB2_FIELDS(lb_ptr));	

	MTX_LOCK(lb_ptr->lb_mtx);				
	if( lb_ptr->lb_nr_init > nr_active){
	    USRDEBUG("NETWORK PARTITION \n");		
	}else{
	    USRDEBUG("NETWORK MERGE \n");		
	}
		
	for( i = 0; i < sizeof(lb_ptr->lb_bm_init)*8; i++){
		// the agent was on the old bitmap 
        if(TEST_BIT(lb_ptr->lb_bm_init, i) != 0) {
			// but it is not in the current bitmap 
			if(TEST_BIT(bm_init, i) == 0) {
				clear_session(i);
				clear_server(i);
				svr_ptr = &server_tab[i];
				MTX_LOCK(svr_ptr->svr_mutex);
				SET_BIT(svr_ptr->svr_bm_sts, SVR_STOPPING);
				MTX_LOCK(svr_ptr->svr_mutex);
			}
		}
	}
	
	lb_ptr->lb_bm_init   = bm_init;
	lb_ptr->lb_bm_active = bm_active;
	lb_ptr->lb_nr_init   = nr_init;
	lb_ptr->lb_nr_active = nr_active;

	mcast_thresholds();
	
    USRDEBUG("NEW " LB2_FORMAT, LB2_FIELDS(lb_ptr));
	MTX_UNLOCK(lb_ptr->lb_mtx);				
	
    return(OK);
}


/*===========================================================================*
*				ucast_wait4bind							     *
*===========================================================================*/
int  ucast_wait4bind( int cmd, char *ptr, 
					int agent_id, char *agent_name, 
					int dcid, int ep, unsigned long timeout)
{
	int rcode;
	message m, *m_ptr;
	lb_t *lb_ptr;
	char priv_name[MAX_MEMBER_NAME];
	
	lb_ptr 	= &lb; 
	m_ptr 	= &m;
	m_ptr->m_source = lb_ptr->lb_nodeid;
	m_ptr->m_type = cmd;
	m_ptr->m1_i1  = dcid;
	m_ptr->m1_i2  = ep;
	m_ptr->m1_p1  = ptr;
	USRDEBUG(MSG1_FORMAT, MSG1_FIELDS(m_ptr));	
	
	sprintf(priv_name,"#LBA.%02d#%s",agent_id, agent_name);
	USRDEBUG("priv_name=%s\n", priv_name);	
	
	rcode = SP_multicast(lb_ptr->lb_mbox, FIFO_MESS, priv_name,
					cmd , sizeof(message), m_ptr);
	return(rcode);
}

/*===========================================================================*
*				ucast_cmd									     *
* Unicast a command to a node 
*===========================================================================*/
int  ucast_cmd(int agent_id, char *agent_name, char *cmd)
{
	int rcode;
	lb_t *lb_ptr;
	char priv_name[MAX_MEMBER_NAME];
	
	lb_ptr = &lb; 
	sprintf(priv_name,"#LBA.%02d#%s",agent_id, agent_name);
	USRDEBUG("priv_name=%s\n", priv_name);		  
	rcode = SP_multicast(lb_ptr->lb_mbox, FIFO_MESS, priv_name,
					MT_RUN_COMMAND , strlen(cmd)+1, cmd);
	return(rcode);
}

/*===========================================================================*
*				mcast_thresholds				     *
* Multilcast the thresholds to new agent
*===========================================================================*/
void mcast_thresholds(void)
{
	message m;
	message *m_ptr;
	lb_t *lb_ptr;
	
	lb_ptr = &lb; 
	m_ptr = &m;
	
	m_ptr->m_source = lb_ptr->lb_nodeid;
	m_ptr->m_type   = MT_LOAD_THRESHOLDS;
	m_ptr->m1_i1	= lb_ptr->lb_lowwater;
	m_ptr->m1_i2	= lb_ptr->lb_highwater;
	m_ptr->m1_i3	= lb_ptr->lb_period;

    USRDEBUG(MSG1_FORMAT, MSG1_FIELDS(m_ptr));	
	SP_multicast(lb_ptr->lb_mbox, FIFO_MESS, SPREAD_GROUP,
					MT_LOAD_THRESHOLDS , sizeof(message), (char *) m_ptr);

}

/*===========================================================================*
*				lbm_udt_members				     *
*===========================================================================*/
void lbm_udt_members(lb_t* lb_ptr,char target_groups[MAX_MEMBERS][MAX_GROUP_NAME],
                    int num_groups)
{	
	int nodeid, i;
	unsigned int	bm_active;
	server_t *svr_ptr;
	client_t *clt_ptr;
	
	// save old node's bitmap
	MTX_LOCK(lb_ptr->lb_mtx);				
	bm_active = lb_ptr->lb_bm_active;
	
	lb_ptr->lb_bm_active = 0;
	lb_ptr->lb_nr_active = 0;
    lb_ptr->lb_sp_nr_mbrs = num_groups;
    memcpy((void*) lb_ptr->lb_sp_members, (void *) target_groups, lb_ptr->lb_sp_nr_mbrs*MAX_GROUP_NAME);
    for(int i=0; i < lb_ptr->lb_sp_nr_mbrs; i++ ){
		nodeid = get_nodeid(&lb_ptr->lb_sp_members[i][0]);
        USRDEBUG("\t%s nodeid=%d\n", &lb_ptr->lb_sp_members[i][0], nodeid);
		if( TEST_BIT(bm_active, nodeid) == 0){
			// NEW NODE 
			USRDEBUG("NEW %s nodeid=%d\n", &lb_ptr->lb_sp_members[i][0], nodeid);
			for( i = 0;  i < dvs_ptr->d_nr_nodes; i++ ){
				svr_ptr = &server_tab[i];
				if(svr_ptr->svr_nodeid == LB_INVALID) continue;
				if(svr_ptr->svr_nodeid == nodeid){
					MTX_LOCK(svr_ptr->svr_tail_mtx);
					if( TEST_BIT(svr_ptr->svr_bm_sts, CLT_WAIT_START)){
						assert(svr_ptr->svr_clt_head.tqh_first != NULL);
						clt_ptr = svr_ptr->svr_clt_head.tqh_first;
						MTX_LOCK(clt_ptr->clt_agent_mtx);
						COND_SIGNAL(clt_ptr->clt_agent_cond);
						MTX_UNLOCK(clt_ptr->clt_agent_mtx);	
					}
					MTX_UNLOCK(svr_ptr->svr_tail_mtx);
				}
			}
		}
		SET_BIT(lb_ptr->lb_bm_active, nodeid);
		lb_ptr->lb_nr_active++;
        USRDEBUG(LB2_FORMAT, LB2_FIELDS(lb_ptr));
    }
	MTX_UNLOCK(lb_ptr->lb_mtx);				
}
#endif // SPREAD_MONITOR 

#ifdef ANULADO 
/*===========================================================================*
 *				   check_server_idle 							 *
 *!!!!!!!!!!!!!!!!!!!!!!!! svr_ptr must be protected by MUTEX 
 *!!!!!!!!!!!!!!!!!!!!!!!! lb_ptr must be protected by MUTEX 
 *===========================================================================*/
void check_server_idle(server_t *svr_ptr)
{
	lb_t *lb_ptr;
	time_t  diff_secs;
	struct timespec ts;
	
	lb_ptr = &lb;
	USRDEBUG("Server %s\n",svr_ptr->svr_name);

	// there is a minimal amount of servers 
	if( (lb_ptr->lb_nr_init-1) <= lb_ptr->lb_min_servers) return;
	
	if(svr_ptr->svr_level == LVL_IDLE){
		clock_gettime(clk_id, &ts);
		diff_secs = ts.tv_sec - svr_ptr->svr_idle_ts.tv_sec;
		USRDEBUG("Server %s: Server IDLE diff_secs=%ld\n",svr_ptr->svr_name, diff_secs);
		if( (int) diff_secs > lb.lb_stop){
			USRDEBUG("Server %s: STOP THE VM!!!\n",svr_ptr->svr_name);
		}
	} else {
		svr_ptr->svr_idle_ts = ts;
	}
}
#endif // ANULADO 
			
