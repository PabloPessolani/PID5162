#define _GNU_SOURCE     
#define _MULTI_THREADED
#include "lb_dvs.h"
#include "lb_spread.h"

char Spread_name[80];
sp_time lba_timeout;
dvs_usr_t dvs, *dvs_ptr;
int		local_nodeid;

jiff  cuse[2],  cice[2], csys[2],  cide[2], ciow[2],  cxxx[2],   cyyy[2],   czzz[2];
int		idx = 0;

typedef struct {
		char 			lba_name[MAX_GROUP_NAME]; // MONITOR name 
		int				lba_monitor;			// MONITOR nodeid
		int				lba_lowwater; 		// low water load (0-100)
		int				lba_highwater;		// low water load (0-100)
		int				lba_period;			// load measurement period in seconds (1-3600)

		int				lba_load_lvl;
		int				lba_cpu_usage;
		unsigned int	lba_bm_eps;
		
	    unsigned int	lba_bm_nodes;	// bitmap of Connected nodes
		unsigned int	lba_bm_init;		// bitmap  initialized/active nodes 
		int				lba_nr_nodes;
		int				lba_nr_init;
		
		pthread_t 		lba_thread;
		pthread_mutex_t lba_mutex;    	

		mailbox			lba_mbox;
		int				lba_len;
		char 			lba_sp_group[MAXNODENAME];
		char    		lba_mbr_name[MAX_MEMBER_NAME];
		char    		lba_priv_group[MAX_GROUP_NAME];
		membership_info lba_memb_info;
		vs_set_info     lba_vssets[MAX_VSSETS];
		unsigned int    lba_my_vsset_index;
		int             lba_num_vs_sets;
		char            lba_members[MAX_MEMBERS][MAX_GROUP_NAME];
		char		   	lba_sp_members[MAX_MEMBERS][MAX_GROUP_NAME];
		int		   		lba_sp_nr_mbrs;
		char		 	lba_mess_in[MAX_MESSLEN];	
} lba_t;
#define LBA1_FORMAT 	"lba_mbr_name=%s lba_monitor=%d lba_lowwater=%d lba_highwater=%d lba_period=%d\n"
#define LBA1_FIELDS(p)  p->lba_mbr_name, p->lba_monitor, p->lba_lowwater, p->lba_highwater, p->lba_period
#define LBA2_FORMAT 	"lba_mbr_name=%s lba_monitor=%d lba_nr_nodes=%d lba_nr_init=%d lba_bm_nodes=%0X lba_bm_init=%0X\n"
#define LBA2_FIELDS(p)  p->lba_mbr_name, p->lba_monitor, p->lba_nr_nodes, p->lba_nr_init, p->lba_bm_nodes, p->lba_bm_init
#define LBA3_FORMAT 	"lba_mbr_name=%s lba_monitor=%d lba_load_lvl=%d lba_cpu_usage=%d lba_bm_eps=%0X\n"
#define LBA3_FIELDS(p)  p->lba_mbr_name, p->lba_monitor, p->lba_load_lvl, p->lba_cpu_usage, p->lba_bm_eps

int lba_reg_msg(char *sender_ptr, lba_t *lba_ptr, int16 msg_type);
void *get_metrics(void *arg);

lba_t lba;

/*===========================================================================*
 *				   main 				    					 *
 * Without ARGUMENTS!
 *===========================================================================*/
int main (int argc, char *argv[] )
{
    int i, rcode;
	lba_t *lba_ptr;
	
	lba_ptr = &lba;
		
    if ( argc != 1) {
        fprintf( stderr,"Usage: %s \n", argv[0] );
        exit(1);
    }

	local_nodeid = LB_INVALID;
	pthread_mutex_init(&lba_ptr->lba_mutex, NULL);
	
	clear_control_vars(lba_ptr);

    rcode = dvk_open();     //load dvk
    if (rcode < 0)  ERROR_EXIT(rcode);	
	
    local_nodeid = dvk_getdvsinfo(&dvs);    //This dvk_call gets DVS status and parameter information from the local node.
    USRDEBUG("local_nodeid=%d\n",local_nodeid);
    if( local_nodeid < 0 )
        ERROR_EXIT(EDVSDVSINIT);    //DVS not initialized
    dvs_ptr = &dvs;     //DVS parameters pointer
    USRDEBUG(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));
	
	// SERVER PROXY Sender Thread 
	rcode = pthread_create( &lba_ptr->lba_thread, NULL, get_metrics, lba_ptr);
	if( rcode) ERROR_EXIT(rcode)
	
    init_spread( );
    connect_to_spread(lba_ptr);		

    while(TRUE){
        USRDEBUG(LBA1_FORMAT, LBA1_FIELDS(lba_ptr));
        USRDEBUG(LBA2_FORMAT, LBA2_FIELDS(lba_ptr));
        
        rcode = lba_SP_receive(lba_ptr);		//loop receiving messages
        USRDEBUG("rcode=%d\n", rcode);
        if(rcode < 0 ) {
            ERROR_PRINT(rcode);
            sleep(LB_TIMEOUT_5SEC);		
            if( rcode == EDVSNOTCONN) {
                connect_to_spread(lba_ptr);
            }
        }
    }

	rcode = pthread_join(lba_ptr->lba_thread, NULL);	

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
    
    lba_timeout.sec  = LB_TIMEOUT_5SEC;
    lba_timeout.usec = LB_TIMEOUT_MSEC;
    
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
void connect_to_spread(lba_t *lba_ptr)
{
    int rcode;
    /*------------------------------------------------------------------------------------
	* lba_mbr_name:  it must be unique in the spread node.
	*--------------------------------------------------------------------------------------*/
    sprintf(lba_ptr->lba_sp_group, "%s", SPREAD_GROUP);
    USRDEBUG("spread_group=%s\n", lba_ptr->lba_sp_group);
    sprintf( Spread_name, "4803");
	
	sprintf( lba_ptr->lba_mbr_name, "LBA.%02d", local_nodeid);
    USRDEBUG("lba_mbr_name=%s\n", lba_ptr->lba_mbr_name);
    
    rcode = SP_connect_timeout( Spread_name,  lba_ptr->lba_mbr_name , 0, 1, 
                               &lba_ptr->lba_mbox, lba_ptr->lba_priv_group, lba_timeout);
							   
    if( rcode != ACCEPT_SESSION ) 	{
        SP_error (rcode);
        ERROR_PRINT(rcode);
        pthread_exit(NULL);
    }
    
    USRDEBUG("lba_mbr_name %s: connected to %s with private group %s\n",
             lba_ptr->lba_mbr_name, Spread_name, lba_ptr->lba_priv_group);
    
    rcode = SP_join( lba_ptr->lba_mbox, lba_ptr->lba_sp_group);
    if( rcode){
        SP_error (rcode);
        ERROR_PRINT(rcode);
        pthread_exit(NULL);
    }
}

/*===========================================================================*
 *				clear_control_vars				     
 * nitilize several global and replicated  variables
 *===========================================================================*/
void clear_control_vars(lba_t *lba_ptr)
{	
    USRDEBUG("%s\n", lba_ptr->lba_mbr_name);

    lba_ptr->lba_bm_nodes 	= 0;
    lba_ptr->lba_bm_init	= 0;	
    lba_ptr->lba_nr_nodes 	= 0;
    lba_ptr->lba_nr_init	= 0;
	
	lba_ptr->lba_load_lvl	= LB_INVALID;
	lba_ptr->lba_cpu_usage  = LB_INVALID;
	lba_ptr->lba_bm_eps		= 0;
	
	lba_ptr->lba_monitor	= LB_INVALID;
	lba_ptr->lba_lowwater	= LB_INVALID; 	
	lba_ptr->lba_highwater	= LB_INVALID;	
	lba_ptr->lba_period		= LB_INVALID;
}

/*===========================================================================*
 *				lba_SP_receive				    
 *===========================================================================*/
int lba_SP_receive(lba_t	*lba_ptr)
{
    char        sender[MAX_GROUP_NAME];
    char        target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
    int         num_groups = -1;
    int         service_type = 0;
    int16       mess_type;
    int         endian_mismatch;
    int         ret;
    SP_message *sp_ptr;
    
    USRDEBUG("%s\n", lba_ptr->lba_mbr_name);
    
    assert(lba_ptr->lba_mess_in != NULL);
       
    memset(lba_ptr->lba_mess_in,0,MAX_MESSLEN);
    //Returns message size in non-error
    ret = SP_receive( lba_ptr->lba_mbox, &service_type, sender, 100, &num_groups, target_groups,
                     &mess_type, &endian_mismatch, MAX_MESSLEN, lba_ptr->lba_mess_in );
    USRDEBUG("ret=%d\n", ret);
    
    if( ret < 0 ){
        if ( (ret == GROUPS_TOO_SHORT) || (ret == BUFFER_TOO_SHORT) ) {
            service_type = DROP_RECV;
            USRDEBUG("\n========Buffers or Groups too Short=======\n");
            ret = SP_receive( lba_ptr->lba_mbox, &service_type, sender, 
                             MAX_MEMBERS, &num_groups, target_groups,
                             &mess_type, &endian_mismatch, MAX_MESSLEN, lba_ptr->lba_mess_in);
        }
    }
    
    if (ret < 0 ) {
        SP_error( ret );
        ERROR_PRINT(ret);
        pthread_exit(NULL);
    }
    
    USRDEBUG("%s: sender=%s Private_group=%s service_type=%d\n", 
             lba_ptr->lba_mbr_name,
             sender, 
             lba_ptr->lba_priv_group, 
             service_type);
    
    sp_ptr = (SP_message *) lba_ptr->lba_mess_in;
    
    if( Is_regular_mess( service_type ) )	{
        if( Is_fifo_mess(service_type) ) {
            USRDEBUG("%s: FIFO message from %s, of type %d, (endian %d) to %d groups (%d bytes)\n",
                     lba_ptr->lba_mbr_name, sender, mess_type, endian_mismatch, num_groups, ret);
			lba_ptr->lba_len = ret;
			ret = lba_reg_msg(sender, lba_ptr, mess_type);
        } else {
			USRDEBUG("received non SAFE/FIFO message\n");
            ret = OK;
		}
    } else if( Is_membership_mess( service_type ) )	{
        ret = SP_get_memb_info( lba_ptr->lba_mess_in, service_type, &lba_ptr->lba_memb_info );
        if (ret < 0) {
            USRDEBUG("BUG: membership message does not have valid body\n");
            SP_error( ret );
            ERROR_PRINT(ret);
            pthread_exit(NULL);
        }
        
        if  ( Is_reg_memb_mess( service_type ) ) {
            USRDEBUG("%s: Received REGULAR membership for group %s with %d members, where I am member %d:\n",
                     lba_ptr->lba_mbr_name, sender, num_groups, mess_type );
        }
        
        if( Is_caused_join_mess( service_type ) )	{
            /*----------------------------------------------------------------------------------------------------
			*   JOIN: The group has a new LBM  member
			*----------------------------------------------------------------------------------------------------*/
            lba_udt_members(lba_ptr,target_groups,num_groups);
            
            USRDEBUG("%s: Due to the JOIN of %s service_type=%d\n", 
                     lba_ptr->lba_mbr_name, lba_ptr->lba_memb_info.changed_member, service_type );
            
            ret = lba_join(lba_ptr,num_groups);
        }else if( Is_caused_leave_mess( service_type ) 
                 ||  Is_caused_disconnect_mess( service_type ) ){
            /*----------------------------------------------------------------------------------------------------
			*   LEAVE or DISCONNECT:  A member has left the group
			*----------------------------------------------------------------------------------------------------*/
            lba_udt_members(lba_ptr,target_groups,num_groups);
            
            USRDEBUG("%s: Due to the LEAVE or DISCONNECT of %s\n", 
                     lba_ptr->lba_mbr_name, lba_ptr->lba_memb_info.changed_member );
            
            ret = lba_leave(lba_ptr,num_groups);
        }else if( Is_caused_network_mess( service_type ) ){
      /*----------------------------------------------------------------------------------------------------
            *   NETWORK CHANGE:  A network partition or a dead deamon
            *----------------------------------------------------------------------------------------------------*/
            USRDEBUG("%s: Due to NETWORK change with %u VS sets\n", 
                     lba_ptr->lba_mbr_name,  lba_ptr->lba_num_vs_sets);
            
            ret = lba_network(lba_ptr);
            
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
                     sender, service_type, mess_type, endian_mismatch, num_groups, ret, lba_ptr->lba_mess_in );
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
    
	assert( nid >= 0 && nid < NR_NODES);
    return(nid);
}

/*===========================================================================*
*				lba_reg_msg				     *
* Handle REGULAR messages 
*===========================================================================*/
int lba_reg_msg(char *sender_ptr, lba_t *lba_ptr, int16 msg_type)
{
	int node_id;
	int rcode;
	message *m_ptr;
	server_t *svr_ptr;

	node_id = get_nodeid(sender_ptr);
    USRDEBUG("sender_ptr=%s node_id=%d\n", sender_ptr,  node_id );

	// ignore other self sent messages 
	if( node_id == local_nodeid) return(OK);
	
	switch(msg_type){
		case MT_LOAD_THRESHOLDS:
			// check that sender's name be the Monitor name 
			if( strncmp(sender_ptr, LBM_SHARP, strlen(LBM_SHARP)) != 0){
				fprintf( stderr,"lba_reg_msg: MT_LOAD_THRESHOLDS message sent by %s\n",
					 sender_ptr);
				return(OK);					
			}
			// check message len 
			if( lba_ptr->lba_len < sizeof(message)){
				fprintf( stderr,"lba_reg_msg: bad message size=%d (must be %d)\n",
					 lba_ptr->lba_len, sizeof(message));
				return(OK);		
			}
			m_ptr = lba_ptr->lba_mess_in;
			USRDEBUG(MSG1_FORMAT, MSG1_FIELDS(m_ptr) );	
			// check message source 
			if( m_ptr->m_source != node_id){
				fprintf( stderr,"lba_reg_msg: m_source(%d) != sender node_id(%d)\n",
					 m_ptr->m_source, node_id);
				return(OK);		
			}
			// check if current monitor has changed 
			if(lba_ptr->lba_monitor != LB_INVALID){
				if( lba_ptr->lba_monitor != node_id){
					fprintf( stderr,"lba_reg_msg: WARNING monitor has changed old(%d), new(%d)\n",
						 lba_ptr->lba_monitor, node_id);
					strncpy(lba_ptr->lba_name, sender_ptr, MAX_GROUP_NAME-1);	 
				}
			}
			// check correct message type 
			if( m_ptr->m_type != msg_type){
				fprintf( stderr,"lba_reg_msg: m_type(%d) != msg_type(%d)\n",
					 m_ptr->m_type, msg_type);
				return(OK);		
			}
			if(TEST_BIT(lba_ptr->lba_bm_init, node_id) == 0){	
				SET_BIT(lba_ptr->lba_bm_init, node_id);
				lba_ptr->lba_nr_init++;
				USRDEBUG(LBA2_FORMAT, LBA2_FIELDS(lba_ptr));
			}
			
			// check received paramenters 
			assert(m_ptr->m1_i1 >= 0 && m_ptr->m1_i1 <= 100);
			assert(m_ptr->m1_i2 >= 0 && m_ptr->m1_i2 <= 100);
			assert(m_ptr->m1_i3 >= 1 && m_ptr->m1_i3 <= 3600);
			// update parameters 
			MTX_LOCK(lba_ptr->lba_mutex);
			lba_ptr->lba_monitor 	= m_ptr->m_source;
			lba_ptr->lba_lowwater 	= m_ptr->m1_i1;
			lba_ptr->lba_highwater	= m_ptr->m1_i2;
			lba_ptr->lba_period		= m_ptr->m1_i3;
			MTX_UNLOCK(lba_ptr->lba_mutex);
			
			break;
		case MT_LOAD_LEVEL:
			// ignore other agents messages 
			break;
		default:
			fprintf( stderr,"lba_reg_msg: invalid regular message type=%d\n",
				 msg_type);
			break;
	}

	return(OK);
}

/*===========================================================================*
*				lba_join				     *
* Handle JOIN 
*===========================================================================*/
int lba_join(lba_t *lba_ptr,int num_groups)
{
	int node_id;
    server_t *svr_ptr;

    USRDEBUG("member=%s\n",lba_ptr->lba_memb_info.changed_member);
    node_id  = get_nodeid((char *)lba_ptr->lba_memb_info.changed_member);

	if( node_id == local_nodeid) return(OK);
	
    if ( strncmp(lba_ptr->lba_memb_info.changed_member, LBM_SHARP, strlen(LBM_SHARP)) == 0) {
		// check if current monitor has changed 
		if(lba_ptr->lba_monitor != LB_INVALID){
			if( lba_ptr->lba_monitor != node_id)
				fprintf( stderr,"lba_join: WARNING monitor has changed old(%d), new(%d)\n",
					 lba_ptr->lba_monitor, node_id);
		}
		strncpy(lba_ptr->lba_name, lba_ptr->lba_memb_info.changed_member, MAX_GROUP_NAME-1);	 
		lba_ptr->lba_monitor = node_id;
    }
    USRDEBUG(LBA2_FORMAT, LBA2_FIELDS(lba_ptr));
	return(OK);
}

/*===========================================================================*
*				lba_leave				     *
* Handle LEAVE OR DISCONNECT
*===========================================================================*/
int  lba_leave(lba_t *lba_ptr,int num_groups)
{
	int node_id;
	
    USRDEBUG("member=%s\n",lba_ptr->lba_memb_info.changed_member);
    node_id  = get_nodeid((char *)lba_ptr->lba_memb_info.changed_member);
	if( node_id == local_nodeid) return(OK);
	
    if ( strncmp(lba_ptr->lba_memb_info.changed_member, LBM_SHARP, strlen(LBM_SHARP)) == 0) {
		lba_ptr->lba_monitor = node_id;
		clear_control_vars(lba_ptr);
    }
    USRDEBUG(LBA2_FORMAT, LBA2_FIELDS(lba_ptr));
	return(OK);
}

/*===========================================================================*
*				lba_network				     *
* Handle NETWORK event
*===========================================================================*/
int lba_network(lba_t* lba_ptr)
{
	int i, j;
    int ret = 0;
	int node_id;
	int	bm_nodes = 0;
	int	nr_nodes = 0;
	int	bm_init = 0;
	int	nr_init = 0;
	
    USRDEBUG("\n");
	
    lba_ptr->lba_num_vs_sets = 
        SP_get_vs_sets_info(lba_ptr->lba_mess_in, 
                            &lba_ptr->lba_vssets[0], 
                            MAX_VSSETS, 
                            &lba_ptr->lba_my_vsset_index);
    
    if (lba_ptr->lba_num_vs_sets < 0) {
        USRDEBUG("BUG: membership message has more then %d vs sets."
                 "Recompile with larger MAX_VSSETS\n",
                 MAX_VSSETS);
        SP_error( lba_ptr->lba_num_vs_sets );
        ERROR_EXIT( lba_ptr->lba_num_vs_sets );
    }
    if (lba_ptr->lba_num_vs_sets == 0) {
        USRDEBUG("BUG: membership message has %d vs_sets\n", 
                 lba_ptr->lba_num_vs_sets);
        SP_error( lba_ptr->lba_num_vs_sets );
        ERROR_EXIT( EDVSGENERIC );
    }
     
	lba_ptr->lba_monitor = LB_INVALID;	
    for(i = 0; i < lba_ptr->lba_num_vs_sets; i++ )  {
        USRDEBUG("%s VS set %d has %u members:\n",
                 (i  == lba_ptr->lba_my_vsset_index) ?("LOCAL") : ("OTHER"), 
                 i, lba_ptr->lba_vssets[i].num_members );
        ret = SP_get_vs_set_members(lba_ptr->lba_mess_in, &lba_ptr->lba_vssets[i], lba_ptr->lba_members, MAX_MEMBERS);
        if (ret < 0) {
            USRDEBUG("VS Set has more then %d members. Recompile with larger MAX_MEMBERS\n", MAX_MEMBERS);
            SP_error( ret );
            ERROR_EXIT( ret);
        }
        
        /*---------------------------------------------
		* get the bitmap of current members
		--------------------------------------------- */
        for(j = 0; j < lba_ptr->lba_vssets[i].num_members; j++ ) {
            USRDEBUG("\t%s\n", lba_ptr->lba_members[j] );
            if ( strncmp(lba_ptr->lba_members[j], LBA_SHARP, strlen(LBA_SHARP)) == 0){	
                node_id = get_nodeid(lba_ptr->lba_members[j]);
                SET_BIT(bm_nodes, node_id);
                nr_nodes++;
			} else if ( strncmp(lba_ptr->lba_members[j], LBM_SHARP, strlen(LBM_SHARP)) == 0){	
                node_id = get_nodeid(lba_ptr->lba_members[j]);
                SET_BIT(bm_nodes, node_id);
                nr_nodes++;
				lba_ptr->lba_monitor = node_id;
			}
		}
	}

    USRDEBUG("OLD " LBA2_FORMAT, LBA2_FIELDS(lba_ptr));

#define LCL2_FORMAT 	"lba_mbr_name=%s lba_monitor=%d nr_nodes=%d nr_init=%d bm_nodes=%0lX bm_init=%0lX\n"
#define LCL2_FIELDS(p)  p->lba_mbr_name, lba_ptr->lba_monitor, nr_nodes, nr_init, bm_nodes, bm_init
    USRDEBUG(LCL2_FORMAT, LCL2_FIELDS(lba_ptr));

	// IS the monitor  on another partition ??  
	if(lba_ptr->lba_monitor == LB_INVALID) { 
		// the monitor is not in this partition 
	    USRDEBUG("NETWORK PARTITION \n");		
		clear_control_vars(lba_ptr);
	} else { // the monitor is in current partition
		// Was the monitor in the previous partition ??
		if( TEST_BIT( lba_ptr->lba_bm_nodes, lba_ptr->lba_monitor) != 0) {
			// YES , It was in the previous partition 
			if( lba_ptr->lba_nr_nodes > nr_nodes){
				USRDEBUG("NETWORK PARTITION \n");		
			}else{
				USRDEBUG("NETWORK MERGE \n");		
			}
			lba_ptr->lba_bm_nodes = bm_nodes;
			lba_ptr->lba_nr_nodes = nr_nodes;
			lba_ptr->lba_bm_init  = bm_init; 
			lba_ptr->lba_nr_init  = nr_init;
		}else{
			USRDEBUG("NETWORK MERGE \n");		
			USRDEBUG("Waiting MT_LOAD_THRESHOLDS message from Monitor %d\n");	
		}
	}

    USRDEBUG("NEW " LBA2_FORMAT, LBA2_FIELDS(lba_ptr));	
    return(OK);
}


/*===========================================================================*
*				mcast_load_level				     *
* Multilcast the load level  to Monitor 
*===========================================================================*/
void mcast_load_level(lba_t *lba_ptr)
{
	int rcode;
	message m;
	message *m_ptr;
	
	m_ptr = &m;
	m_ptr->m_source = local_nodeid;
	m_ptr->m_type   = MT_LOAD_LEVEL;
	m_ptr->m9_i1	= lba_ptr->lba_load_lvl;
	m_ptr->m9_l1	= lba_ptr->lba_cpu_usage;
	rcode = clock_gettime(CLOCK_REALTIME, &m_ptr->m9_t1);
	if(rcode < 0) ERROR_PRINT(-errno);
    USRDEBUG(MSG9_FORMAT, MSG9_FIELDS(m_ptr));	
				  
	SP_multicast(lba_ptr->lba_mbox, FIFO_MESS, SPREAD_GROUP,
					MT_LOAD_LEVEL , sizeof(message), (char *) m_ptr);
}

/*===========================================================================*
*				lba_udt_members				     *
*===========================================================================*/
void lba_udt_members(lba_t* lba_ptr,char target_groups[MAX_MEMBERS][MAX_GROUP_NAME],
                    int num_groups)
{
	int nodeid;
	
	lba_ptr->lba_bm_nodes = 0;
	lba_ptr->lba_nr_nodes = 0;
    lba_ptr->lba_sp_nr_mbrs = num_groups;
    memcpy((void*) lba_ptr->lba_sp_members, (void *) target_groups, lba_ptr->lba_sp_nr_mbrs*MAX_GROUP_NAME);
    for(int i=0; i < lba_ptr->lba_sp_nr_mbrs; i++ ){
		nodeid = get_nodeid(&lba_ptr->lba_sp_members[i][0]);
        USRDEBUG("\t%s nodeid=%d\n", &lba_ptr->lba_sp_members[i][0], nodeid);
		SET_BIT(lba_ptr->lba_bm_nodes, nodeid);
		lba_ptr->lba_nr_nodes++;
        USRDEBUG(LBA2_FORMAT, LBA2_FIELDS(lba_ptr));
    }
}

/*===========================================================================*
*				get_metrics				     *
*===========================================================================*/
void *get_metrics(void *arg)
{
	int period;
	int cpu_usage;
	int load_lvl;
	lba_t	*lba_ptr;
	
	lba_ptr = (lba_t *) arg; 
	USRDEBUG(LBA1_FORMAT, LBA1_FIELDS(lba_ptr));
		
	while(1) {
		// wait until be initiliazed 
		USRDEBUG("waiting for Monitor..\n");
		MTX_LOCK(lba_ptr->lba_mutex);
		if(lba_ptr->lba_lowwater == LB_INVALID){
			MTX_UNLOCK(lba_ptr->lba_mutex);
			sleep(LB_TIMEOUT_5SEC);
			continue;
		}
		period = lba_ptr->lba_period;
		MTX_UNLOCK(lba_ptr->lba_mutex);
		
		while (1) {
			USRDEBUG("\n");
			MTX_LOCK(lba_ptr->lba_mutex);
			if(lba_ptr->lba_lowwater == LB_INVALID){
				MTX_UNLOCK(lba_ptr->lba_mutex);
				break;
			}
			MTX_UNLOCK(lba_ptr->lba_mutex);

			cpu_usage = get_CPU_usage();
			if( cpu_usage == LB_INVALID) break;
			
			assert( cpu_usage >= 0 && cpu_usage <= 100);
			if( cpu_usage < lba_ptr->lba_lowwater){
				load_lvl = LVL_UNLOADED;
			} else	if( cpu_usage >= lba_ptr->lba_highwater){
				load_lvl = LVL_SATURATED;
			} else {
				load_lvl = LVL_LOADED;
			}
			
			// update agent values 
			MTX_LOCK(lba_ptr->lba_mutex);
			lba_ptr->lba_cpu_usage = cpu_usage;
			if( load_lvl != lba_ptr->lba_load_lvl){
				lba_ptr->lba_load_lvl = load_lvl;
				USRDEBUG(LBA3_FORMAT, LBA3_FIELDS(lba_ptr));
				mcast_load_level(lba_ptr);
			}
			MTX_UNLOCK(lba_ptr->lba_mutex);

			sleep(period);
		}
	}
    pthread_exit(NULL);
	USRDEBUG("LB AGENT: Exiting...\n");
}

int get_CPU_usage(void)
{
	int rcode;
	float idle_float;
	unsigned int  cpu_usage, cpu_idle;
	char *ptr;
	FILE *fp;
	static char cpu_string[128];
	jiff duse, dsys, didl, diow, dstl, Div, divo2;

	USRDEBUG("\n");

	/* Open the command for reading. */
to_popen:	
	fp = popen("head -n1 /proc/stat", "r");
	if (fp == NULL)	{
		fprintf( stderr,"get_CPU_usage: error popen errno=%d\n", errno);
		ERROR_RETURN(LB_INVALID);
	}

	/* Read the output a line at a time - output it. */
	ptr = fgets(cpu_string, sizeof(cpu_string), fp);
	if( ptr == NULL) {
		fprintf( stderr,"get_CPU_usage: error fgets errno=%d\n", errno);
		fclose(fp);
		ERROR_RETURN(LB_INVALID);
	}
//	USRDEBUG("cpu_string=%s\n", cpu_string);

	rcode = sscanf(cpu_string,  "cpu  %llu %llu %llu %llu %llu %llu %llu %llu", 
				&cuse[idx], &cice[idx], &csys[idx], &cide[idx], &ciow[idx], &cxxx[idx], &cyyy[idx], &czzz[idx]);
	USRDEBUG("cpu  %llu %llu %llu %llu %llu %llu %llu %llu\n", 
				cuse[idx], cice[idx], csys[idx], cide[idx], ciow[idx], cxxx[idx], cyyy[idx], czzz[idx]);
	if (idx == 0){
		idx = 1;
		pclose(fp);
		sleep(1);
		goto to_popen;
	}
			
	duse = (cuse[1] + cice[1]) - (cuse[0] + cice[0]);
	dsys = (csys[1] + cxxx[1] + cyyy[1]) - (csys[0] + cxxx[0] + cyyy[0]);
	didl = cide[1]-cide[0];
	diow = ciow[1]-ciow[0];
	dstl = czzz[1]-czzz[0];
	Div = cuse[1] + cice[1] + csys[1] + cide[1] + ciow[1] + cxxx[1] + cyyy[1] + czzz[1];
	Div -= cuse[0] + cice[0] + csys[0] + cide[0] + ciow[0] + cxxx[0] + cyyy[0] + czzz[0];
	if (!Div) Div = 1, didl = 1;
	divo2 = Div / 2UL;
	
	USRDEBUG("didl=%llu Div=%llu\n", didl, Div);

//	cpu_idle =(unsigned)( (100*didl	+ divo2) / Div );
	idle_float  = (float) didl/Div;
	idle_float *= 100;
	cpu_idle  = (unsigned) idle_float;
	cpu_usage = 100 - cpu_idle;
	
	USRDEBUG("cpu_usage=%d cpu_idle=%d\n", cpu_usage, cpu_idle);
	pclose(fp);
	
	cuse[0] = cuse[1]; 
	cice[0] = cice[1];
	csys[0] = csys[1];
	cide[0] = cide[1];
	ciow[0] = ciow[1];
	cxxx[0] = cxxx[1];
	cyyy[0] = cyyy[1];
	czzz[0] = czzz[1];
	
	return(cpu_usage);
}






