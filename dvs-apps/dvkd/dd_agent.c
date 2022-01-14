#define _GNU_SOURCE     
#define _MULTI_THREADED
#define _TABLE
#include "dd_common.h"
#include "dd_agent.h"
#include "glo_agent.h"

char Spread_name[80];
sp_time dda_timeout;
dvs_usr_t dvs, *dvs_ptr;
int		local_nodeid;

int dda_reg_msg(char *sender_ptr, dda_t *dda_ptr, int16 msg_type);


/*===========================================================================*
 *				   dd_agent 				    					 *
 *===========================================================================*/
int main (int argc, char *argv[] )
{
	int i, rcode;
	int agent_pid;
    dda_t *dda_ptr;
	
    if ( argc != 1) {
        fprintf( stderr,"Usage: %s \n", argv[0] );
        exit(1);
    }

	dda_ptr = &dda;
	USRDEBUG("DVKD MONITOR: Initializing...\n");

	local_nodeid = DD_INVALID;
	clear_agent_vars(dda_ptr);

    rcode = dvk_open();     //load dvk
    if (rcode < 0)  ERROR_EXIT(rcode);	
	
	rcode = gethostname(dda_ptr->dda_name, MAXNODENAME-1);
	if(rcode != 0) ERROR_EXIT(-errno);
	
    local_nodeid = dvk_getdvsinfo(&dvs);    //This dvk_call gets DVS status and parameter information from the local node.
    USRDEBUG("local_nodeid=%d\n",local_nodeid);
    if( local_nodeid < 0 )
        ERROR_EXIT(EDVSDVSINIT);    //DVS not initialized
	dda_ptr->dda_nodeid = local_nodeid;
    dvs_ptr = &dvs;     //DVS parameters pointer
    USRDEBUG(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));
		
    init_spread( );
    connect_to_spread(dda_ptr);		

    while(TRUE){
        USRDEBUG(DDA_FORMAT, DDA_FIELDS(dda_ptr));
        
        rcode = dda_SP_receive(dda_ptr);		//loop receiving messages
        USRDEBUG("rcode=%d\n", rcode);
        if(rcode < 0 ) {
            ERROR_PRINT(rcode);
            sleep(DD_TIMEOUT_5SEC);		
            if( rcode == EDVSNOTCONN) {
                connect_to_spread(dda_ptr);
            }
        }
    }

    return(OK);				
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
    
    dda_timeout.sec  = DD_TIMEOUT_5SEC;
    dda_timeout.usec = DD_TIMEOUT_MSEC;
    
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
void connect_to_spread(dda_t *dda_ptr)
{
    int rcode;
    /*------------------------------------------------------------------------------------
	* dda_mbr_name:  it must be unique in the spread node.
	*--------------------------------------------------------------------------------------*/
    sprintf(dda_ptr->dda_sp_group, "%s", SPREAD_GROUP);
    USRDEBUG("spread_group=%s\n", dda_ptr->dda_sp_group);
    sprintf( Spread_name, "4803");
	
	sprintf( dda_ptr->dda_mbr_name, "DDA.%02d", local_nodeid);
    USRDEBUG("dda_mbr_name=%s\n", dda_ptr->dda_mbr_name);
    
    rcode = SP_connect_timeout( Spread_name,  dda_ptr->dda_mbr_name , 0, 1, 
                               &dda_ptr->dda_mbox, dda_ptr->dda_priv_group, dda_timeout);
							   
    if( rcode != ACCEPT_SESSION ) 	{
        SP_error (rcode);
        ERROR_PRINT(rcode);
        pthread_exit(NULL);
    }
    
    USRDEBUG("dda_mbr_name %s: connected to %s with private group %s\n",
             dda_ptr->dda_mbr_name, Spread_name, dda_ptr->dda_priv_group);
    
    rcode = SP_join( dda_ptr->dda_mbox, dda_ptr->dda_sp_group);
    if( rcode){
        SP_error (rcode);
        ERROR_PRINT(rcode);
        pthread_exit(NULL);
    }
}

/*===========================================================================*
 *				clear_agent_vars				     
 * Clear  several global and replicated  variables
 *===========================================================================*/
void clear_agent_vars(dda_t *dda_ptr)
{	
    USRDEBUG("\n");

    dda_ptr->dda_bm_nodes 	= 0;
    dda_ptr->dda_bm_init	= 0;	
    dda_ptr->dda_nr_nodes 	= 0;
    dda_ptr->dda_nr_init	= 0;
	dda_ptr->dda_monitor	= DD_INVALID;
}

/*===========================================================================*
 *				dda_SP_receive				    
 *===========================================================================*/
int dda_SP_receive(dda_t	*dda_ptr)
{
    char        sender[MAX_GROUP_NAME];
    char        target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
    int         num_groups = -1;
    int         service_type = 0;
    int16       mess_type;
    int         endian_mismatch;
    int         ret;
    
    USRDEBUG("%s\n", dda_ptr->dda_mbr_name);
    
    assert(dda_ptr->dda_mess_in != NULL);
       
    memset(dda_ptr->dda_mess_in,0,MAX_MESSLEN);
    //Returns message size in non-error
    ret = SP_receive( dda_ptr->dda_mbox, &service_type, sender, 100, &num_groups, target_groups,
                     &mess_type, &endian_mismatch, MAX_MESSLEN, dda_ptr->dda_mess_in );
    USRDEBUG("ret=%d\n", ret);
    
    if( ret < 0 ){
        if ( (ret == GROUPS_TOO_SHORT) || (ret == BUFFER_TOO_SHORT) ) {
            service_type = DROP_RECV;
            USRDEBUG("\n========Buffers or Groups too Short=======\n");
            ret = SP_receive( dda_ptr->dda_mbox, &service_type, sender, 
                             MAX_MEMBERS, &num_groups, target_groups,
                             &mess_type, &endian_mismatch, MAX_MESSLEN, dda_ptr->dda_mess_in);
        }
    }
    
    if (ret < 0 ) {
        SP_error( ret );
        ERROR_PRINT(ret);
        pthread_exit(NULL);
    }
    
    USRDEBUG("%s: sender=%s Private_group=%s service_type=%d\n", 
             dda_ptr->dda_mbr_name,
             sender, 
             dda_ptr->dda_priv_group, 
             service_type);
     
    if( Is_regular_mess( service_type ) )	{
        if( Is_fifo_mess(service_type) ) {
            USRDEBUG("%s: FIFO message from %s, of type %d, (endian %d) to %d groups (%d bytes)\n",
                     dda_ptr->dda_mbr_name, sender, mess_type, endian_mismatch, num_groups, ret);
			dda_ptr->dda_len = ret;
			ret = dda_reg_msg(sender, dda_ptr, mess_type);
        } else {
			USRDEBUG("received non SAFE/FIFO message\n");
            ret = OK;
		}
    } else if( Is_membership_mess( service_type ) )	{
        ret = SP_get_memb_info( dda_ptr->dda_mess_in, service_type, &dda_ptr->dda_memb_info );
        if (ret < 0) {
            USRDEBUG("BUG: membership message does not have valid body\n");
            SP_error( ret );
            ERROR_PRINT(ret);
            pthread_exit(NULL);
        }
        
        if  ( Is_reg_memb_mess( service_type ) ) {
            USRDEBUG("%s: Received REGULAR membership for group %s with %d members, where I am member %d:\n",
                     dda_ptr->dda_mbr_name, sender, num_groups, mess_type );
        }
        
        if( Is_caused_join_mess( service_type ) )	{
            /*----------------------------------------------------------------------------------------------------
			*   JOIN: The group has a new LBM  member
			*----------------------------------------------------------------------------------------------------*/
            dda_udt_members(dda_ptr,target_groups,num_groups);
            
            USRDEBUG("%s: Due to the JOIN of %s service_type=%d\n", 
                     dda_ptr->dda_mbr_name, dda_ptr->dda_memb_info.changed_member, service_type );
            
            ret = dda_join(dda_ptr,num_groups);
        }else if( Is_caused_leave_mess( service_type ) 
                 ||  Is_caused_disconnect_mess( service_type ) ){
            /*----------------------------------------------------------------------------------------------------
			*   LEAVE or DISCONNECT:  A member has left the group
			*----------------------------------------------------------------------------------------------------*/
            dda_udt_members(dda_ptr,target_groups,num_groups);
            
            USRDEBUG("%s: Due to the LEAVE or DISCONNECT of %s\n", 
                     dda_ptr->dda_mbr_name, dda_ptr->dda_memb_info.changed_member );
            
            ret = dda_leave(dda_ptr,num_groups);
        }else if( Is_caused_network_mess( service_type ) ){
      /*----------------------------------------------------------------------------------------------------
            *   NETWORK CHANGE:  A network partition or a dead deamon
            *----------------------------------------------------------------------------------------------------*/
            USRDEBUG("%s: Due to NETWORK change with %u VS sets\n", 
                     dda_ptr->dda_mbr_name,  dda_ptr->dda_num_vs_sets);
            
            ret = dda_network(dda_ptr);
            
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
                     sender, service_type, mess_type, endian_mismatch, num_groups, ret, dda_ptr->dda_mess_in );
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
*				dda_reg_msg				     *
* Handle REGULAR messages 
*===========================================================================*/
int dda_reg_msg(char *sender_ptr, dda_t *dda_ptr, int16 msg_type)
{
	int nodeid, len, max;
	int rcode;
	FILE *pfp;
	char *ptr;

	nodeid = get_nodeid(sender_ptr);
    USRDEBUG("sender_ptr=%s nodeid=%d\n", sender_ptr,  nodeid );

	// ignore other self sent messages 
	if( nodeid == local_nodeid) return(OK);
	
	switch(msg_type){
		case MT_HELLO_AGENTS:
			// check that sender's name be the Monitor name 
			if( strncmp(sender_ptr, DDM_SHARP, strlen(DDM_SHARP)) != 0){
				fprintf( stderr,"dda_reg_msg: MT_HELLO_AGENTS message sent by %s\n",
					 sender_ptr);
				ERROR_RETURN(OK);					
			}
			// check message len 
			if( dda_ptr->dda_len < sizeof(message)){
				fprintf( stderr,"dda_reg_msg: bad message size=%d (must be %d)\n",
					 dda_ptr->dda_len, sizeof(message));
				ERROR_RETURN(OK);		
			}
			m_ptr = dda_ptr->dda_mess_in;
			USRDEBUG(MSG1_FORMAT, MSG1_FIELDS(m_ptr) );	
			// check message source 
			if( m_ptr->m_source != nodeid){
				fprintf( stderr,"dda_reg_msg: m_source(%d) != sender nodeid(%d)\n",
					 m_ptr->m_source, nodeid);
				ERROR_RETURN(OK);		
			}
			// check if current monitor has changed 
			if(dda_ptr->dda_monitor != DD_INVALID){
				if( dda_ptr->dda_monitor != nodeid){
					fprintf( stderr,"dda_reg_msg: MT_HELLO_AGENTS WARNING monitor has changed old(%d), new(%d)\n",
						 dda_ptr->dda_monitor, nodeid);
				ERROR_RETURN(OK);		
				}
			}
			// check correct message type 
			if( m_ptr->m_type != msg_type){
				fprintf( stderr,"dda_reg_msg: m_type(%d) != msg_type(%d)\n",
					 m_ptr->m_type, msg_type);
				ERROR_RETURN(OK);		
			}
			dda_ptr->dda_bm_init = dda_ptr->dda_bm_nodes;
			dda_ptr->dda_nr_init = dda_ptr->dda_nr_nodes;
			dda_ptr->dda_monitor = nodeid;
			USRDEBUG("MT_HELLO_AGENTS received from %d\n",nodeid);			
			mcast_hello_monitor(dda_ptr);
			break;
		case MT_CMD2AGENT:
			// check that sender's name be the Monitor name 
			if( strncmp(sender_ptr, DDM_SHARP, strlen(DDM_SHARP)) != 0){
				fprintf( stderr,"dda_reg_msg: MT_CMD2AGENT message sent by %s\n",
					 sender_ptr);
				ERROR_RETURN(OK);					
			}
			// check if current monitor has changed 
			if(dda_ptr->dda_monitor != DD_INVALID){
				if( dda_ptr->dda_monitor != nodeid){
					fprintf( stderr,"dda_reg_msg: MT_CMD2AGENT WARNING monitor has changed old(%d), new(%d)\n",
						 dda_ptr->dda_monitor, nodeid);
					ERROR_RETURN(OK);		
				}
			}
			// check message len 
			if( dda_ptr->dda_len >= MAX_CMDIN_LEN){
				fprintf( stderr,"dda_reg_msg: MT_CMD2AGENT message len(%d) > MAX_CMDIN_LEN(%d)\n",
					dda_ptr->dda_len , MAX_CMDIN_LEN);			
			}
			USRDEBUG("MT_CMD2AGENT:[%s]\n",dda_ptr->dda_mess_in);			
			
			// Execute and read OUTPUT from command 
			pfp = popen(dda_ptr->dda_mess_in, "r");
			if (pfp == NULL)	{
				fprintf( stderr,"dda_reg_msg: MT_CMD2AGENT error popen errno=%d\n", errno);
				ERROR_RETURN(OK);
			}
			memset(cmd_output,0,MAX_MESSLEN);
			ptr = cmd_output;
			max = MAX_MESSLEN-1;
			while (fgets(ptr, max , pfp) != NULL) {
				printf("%s", ptr);
				len = strlen(ptr);
				ptr+= len;
				max -= len;
			}
			*ptr ='\0';
			len = strlen(cmd_output);
			pclose(pfp);
			
			// multicast OUTPUT 
			SP_multicast(dda_ptr->dda_mbox, FIFO_MESS, SPREAD_GROUP,
					MT_OUT2MONITOR , len, cmd_output);
			break;	
		case DVK_BIND:
			// check that sender's name be the Monitor name 
			if( strncmp(sender_ptr, DDM_SHARP, strlen(DDM_SHARP)) != 0){
				fprintf( stderr,"dda_reg_msg: DVK_BIND message sent by %s\n",
					 sender_ptr);
				ERROR_RETURN(OK);					
			}
			// check if current monitor has changed 
			if(dda_ptr->dda_monitor != DD_INVALID){
				if( dda_ptr->dda_monitor != nodeid){
					fprintf( stderr,"dda_reg_msg: DVK_BIND WARNING monitor has changed old(%d), new(%d)\n",
						 dda_ptr->dda_monitor, nodeid);
					ERROR_RETURN(OK);		
				}
			}
			// check message len 
			if( dda_ptr->dda_len >= MAX_CMDIN_LEN){
				fprintf( stderr,"dda_reg_msg: DVK_BIND message len(%d) > MAX_CMDIN_LEN(%d)\n",
					dda_ptr->dda_len , MAX_CMDIN_LEN);			
			}
			m_ptr = dda_ptr->dda_mess_in;
			USRDEBUG("API REQUEST (%d): " MSGC_FORMAT, sizeof(message), MSGC_FIELDS(m_ptr));	
			if (m_ptr->mC_i1 != RMT_BIND)	{
				fprintf( stderr,"dda_reg_msg: DVK_BIND error no RMT_BIND errno=%d\n", errno);
				ERROR_RETURN(OK);
			}
			
			// dvk_rmtbind(dcid,name,endpoint,nodeid)
			rcode = dvk_rmtbind(m_ptr->mC_i2,m_ptr->mC_ca1,m_ptr->mC_i3,m_ptr->mC_i4);

			m_ptr = &m_out;
			m_ptr->m_source = SOURCE_AGENT;
			m_ptr->m_type 	= (DVK_BIND| MASK_ACKNOWLEDGE);
			m_ptr->mC_i1	= rcode;
			m_ptr->mC_i2	= 0;
			m_ptr->mC_i3	= 0;
			m_ptr->mC_i4	= 0;
			strcpy(m_ptr->mC_ca1,"rmtbind");
			USRDEBUG("API REPLY (%d): " MSGC_FORMAT, sizeof(message), MSGC_FIELDS(m_ptr));	
			
			// multicast REPLY  
			SP_multicast(dda_ptr->dda_mbox, FIFO_MESS, SPREAD_GROUP,
					(DVK_BIND| MASK_ACKNOWLEDGE) , sizeof(message), m_ptr);
			break;	
		case (DVK_BIND| MASK_ACKNOWLEDGE):
			break;
		case MT_HELLO_MONITOR:
			// ignore other agents messages 
			break;
		default:
			fprintf( stderr,"dda_reg_msg: invalid regular message type=%d\n",
				 msg_type);
			break;
	}

	return(OK);
}

/*===========================================================================*
*				dda_join				     *
* Handle JOIN 
*===========================================================================*/
int dda_join(dda_t *dda_ptr,int num_groups)
{
	int nodeid;

    USRDEBUG("member=%s\n",dda_ptr->dda_memb_info.changed_member);
    nodeid  = get_nodeid((char *)dda_ptr->dda_memb_info.changed_member);

	if( nodeid == local_nodeid) return(OK);
	
    if ( strncmp(dda_ptr->dda_memb_info.changed_member, DDM_SHARP, strlen(DDM_SHARP)) == 0) {
		// check if current monitor has changed 
		if(dda_ptr->dda_monitor != DD_INVALID){
			if( dda_ptr->dda_monitor != nodeid)
				fprintf( stderr,"dda_join: WARNING monitor has changed old(%d), new(%d)\n",
					 dda_ptr->dda_monitor, nodeid);
		}
		strncpy(dda_ptr->dda_name, dda_ptr->dda_memb_info.changed_member, MAX_GROUP_NAME-1);	 
		dda_ptr->dda_monitor = nodeid;
    }
    USRDEBUG(DDA_FORMAT, DDA_FIELDS(dda_ptr));
	return(OK);
}

/*===========================================================================*
*				dda_leave				     *
* Handle LEAVE OR DISCONNECT
*===========================================================================*/
int  dda_leave(dda_t *dda_ptr,int num_groups)
{
	int nodeid;
	
    USRDEBUG("member=%s\n",dda_ptr->dda_memb_info.changed_member);
    nodeid  = get_nodeid((char *)dda_ptr->dda_memb_info.changed_member);
	if( nodeid == local_nodeid) return(OK);
	
    if ( strncmp(dda_ptr->dda_memb_info.changed_member, DDM_SHARP, strlen(DDM_SHARP)) == 0) {
		dda_ptr->dda_monitor = DD_INVALID;
		clear_agent_vars(dda_ptr);
    }
    USRDEBUG(DDA_FORMAT, DDA_FIELDS(dda_ptr));
	return(OK);
}

/*===========================================================================*
*				dda_network				     *
* Handle NETWORK event
*===========================================================================*/
int dda_network(dda_t* dda_ptr)
{
	int i, j;
    int ret = 0;
	int nodeid;
	int	bm_nodes = 0;
	int	nr_nodes = 0;
	int	bm_init = 0;
	int	nr_init = 0;
	
    USRDEBUG("\n");
	
    dda_ptr->dda_num_vs_sets = 
        SP_get_vs_sets_info(dda_ptr->dda_mess_in, 
                            &dda_ptr->dda_vssets[0], 
                            MAX_VSSETS, 
                            &dda_ptr->dda_my_vsset_index);
    
    if (dda_ptr->dda_num_vs_sets < 0) {
        USRDEBUG("BUG: membership message has more then %d vs sets."
                 "Recompile with larger MAX_VSSETS\n",
                 MAX_VSSETS);
        SP_error( dda_ptr->dda_num_vs_sets );
        ERROR_EXIT( dda_ptr->dda_num_vs_sets );
    }
    if (dda_ptr->dda_num_vs_sets == 0) {
        USRDEBUG("BUG: membership message has %d vs_sets\n", 
                 dda_ptr->dda_num_vs_sets);
        SP_error( dda_ptr->dda_num_vs_sets );
        ERROR_EXIT( EDVSGENERIC );
    }
     
	dda_ptr->dda_monitor = DD_INVALID;	
    for(i = 0; i < dda_ptr->dda_num_vs_sets; i++ )  {
        USRDEBUG("%s VS set %d has %u members:\n",
                 (i  == dda_ptr->dda_my_vsset_index) ?("LOCAL") : ("OTHER"), 
                 i, dda_ptr->dda_vssets[i].num_members );
        ret = SP_get_vs_set_members(dda_ptr->dda_mess_in, &dda_ptr->dda_vssets[i], dda_ptr->dda_members, MAX_MEMBERS);
        if (ret < 0) {
            USRDEBUG("VS Set has more then %d members. Recompile with larger MAX_MEMBERS\n", MAX_MEMBERS);
            SP_error( ret );
            ERROR_EXIT( ret);
        }
        
        /*---------------------------------------------
		* get the bitmap of current members
		--------------------------------------------- */
        for(j = 0; j < dda_ptr->dda_vssets[i].num_members; j++ ) {
            USRDEBUG("\t%s\n", dda_ptr->dda_members[j] );
            if ( strncmp(dda_ptr->dda_members[j], DDA_SHARP, strlen(DDA_SHARP)) == 0){	
                nodeid = get_nodeid(dda_ptr->dda_members[j]);
                SET_BIT(bm_nodes, nodeid);
                nr_nodes++;
			} else if ( strncmp(dda_ptr->dda_members[j], DDM_SHARP, strlen(DDM_SHARP)) == 0){	
                nodeid = get_nodeid(dda_ptr->dda_members[j]);
                SET_BIT(bm_nodes, nodeid);
                nr_nodes++;
				dda_ptr->dda_monitor = nodeid;
			}
		}
	}

    USRDEBUG("OLD " DDA_FORMAT, DDA_FIELDS(dda_ptr));

#define LCL2_FORMAT 	"dda_mbr_name=%s dda_monitor=%d nr_nodes=%d nr_init=%d bm_nodes=%0lX bm_init=%0lX\n"
#define LCL2_FIELDS(p)  p->dda_mbr_name, dda_ptr->dda_monitor, nr_nodes, nr_init, bm_nodes, bm_init
    USRDEBUG(LCL2_FORMAT, LCL2_FIELDS(dda_ptr));

	// IS the monitor  on another partition ??  
	if(dda_ptr->dda_monitor == DD_INVALID) { 
		// the monitor is not in this partition 
	    USRDEBUG("NETWORK PARTITION \n");		
		clear_agent_vars(dda_ptr);
	} else { // the monitor is in current partition
		// Was the monitor in the previous partition ??
		if( TEST_BIT( dda_ptr->dda_bm_nodes, dda_ptr->dda_monitor) != 0) {
			// YES , It was in the previous partition 
			if( dda_ptr->dda_nr_nodes > nr_nodes){
				USRDEBUG("NETWORK PARTITION \n");		
			}else{
				USRDEBUG("NETWORK MERGE \n");		
			}
			dda_ptr->dda_bm_nodes = bm_nodes;
			dda_ptr->dda_nr_nodes = nr_nodes;
			dda_ptr->dda_bm_init  = bm_init; 
			dda_ptr->dda_nr_init  = nr_init;
		}else{
			USRDEBUG("NETWORK MERGE \n");		
			USRDEBUG("Waiting MT_LOAD_THRESHOLDS message from Monitor %d\n");	
		}
	}

    USRDEBUG("NEW " DDA_FORMAT, DDA_FIELDS(dda_ptr));	
    return(OK);
}

/*===========================================================================*
*				mcast_hello_monitor				     *
* Multilcast HELLO to new MONITOR
*===========================================================================*/
void mcast_hello_monitor(dda_t *dda_ptr)
{
	m_ptr = &m_out;
	m_ptr->m_source = local_nodeid;
	m_ptr->m_type   = MT_HELLO_MONITOR;
	m_ptr->m1_i1	= dda_ptr->dda_nr_nodes;
	m_ptr->m1_i2	= dda_ptr->dda_nr_init;
	m_ptr->m1_i3	= 0;
	m_ptr->m1_p1	= NULL;
	m_ptr->m1_p2	= NULL;
	m_ptr->m1_p3	= NULL;

    USRDEBUG(MSG1_FORMAT, MSG1_FIELDS(m_ptr));	
				  
	SP_multicast(dda_ptr->dda_mbox, FIFO_MESS, SPREAD_GROUP,
					MT_HELLO_MONITOR , sizeof(message), (char *) m_ptr);
}

/*===========================================================================*
*				dda_udt_members				     *
*===========================================================================*/
void dda_udt_members(dda_t* dda_ptr,char target_groups[MAX_MEMBERS][MAX_GROUP_NAME],
                    int num_groups)
{
	int nodeid;
	
	dda_ptr->dda_bm_nodes = 0;
	dda_ptr->dda_nr_nodes = 0;
    dda_ptr->dda_sp_nr_mbrs = num_groups;
    memcpy((void*) dda_ptr->dda_sp_members, (void *) target_groups, dda_ptr->dda_sp_nr_mbrs*MAX_GROUP_NAME);
    for(int i=0; i < dda_ptr->dda_sp_nr_mbrs; i++ ){
		nodeid = get_nodeid(&dda_ptr->dda_sp_members[i][0]);
        USRDEBUG("\t%s nodeid=%d\n", &dda_ptr->dda_sp_members[i][0], nodeid);
		SET_BIT(dda_ptr->dda_bm_nodes, nodeid);
		dda_ptr->dda_nr_nodes++;
        USRDEBUG(DDA_FORMAT, DDA_FIELDS(dda_ptr));
    }
}
