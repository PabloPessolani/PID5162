/****************************************************************/
/* 				RADAR							*/
/* RADAR algorithm detects replicated service Primary changes 		*/
/* WARNING!!! : This service can only be run in a node without an		*/
/* instance of the monitored service 						*/
/****************************************************************/

#define _GNU_SOURCE     
#define _MULTI_THREADED
#define _TABLE
#include "radar.h"
//@TODO: make a radar_cfg header
//right now we are getting a compiling warning (still works when linking)

#define TRUE 1
#define RADAR	1 

int radar_loop(radar_t	*r_ptr);
int get_radar_info(radar_t	*r_ptr,	SP_message  *sp_ptr);
int no_primary_dead(radar_t	*r_ptr);
int no_primary_part(radar_t	*r_ptr);
int get_nodeid(char *grp_name, char *mbr_string);
int get_random_node(radar_t	*r_ptr,unsigned int eligible_nodes);
int get_first_mbr(unsigned int nodes );
int get_nonprimary_node(radar_t* rad);
int get_dcid(char *grp_name, char *mbr_string);
void *radar_thread(void *arg);
void connect_to_spread(radar_t *r_ptr);
void init_control_vars(radar_t *r_ptr);
void init_spread(void);


/*===========================================================================*
 *				   main 				    					 *
 * This program detects node changes of endpoints (svr_ep) produced by a migration or	*
 * Primary crashes through SPREAD . It sets the remote endpoint svr_ep to the	*
* new endpoint location	.										* 
 *===========================================================================*/
int main (int argc, char *argv[] )
{
    int rcode /* codigos de error */;
    
    if ( argc != 2) {
        fprintf( stderr,"Usage: %s <config_file> \n", argv[0] );
        exit(1);
    }
    
    
    for (int i = 0;i < NR_MAX_CONTROL; i++){   //NR_MAX_CONTROL = 32
        posix_memalign( (void**) &rad_ptr[i], getpagesize(), sizeof(radar_t)); //DC memory reservation
        if( rad_ptr[i] == NULL) return (EDVSNOMEM);
    }
    
    radar_config(argv[1]);  //Reads Config File
    
    //nr_control (Global, glo.h) gets set in radar_config, error checking
    if( nr_control > NR_MAX_CONTROL){   
        fprintf( stderr,"The count of pair {dc,enpoint} to control (%d) >= NR_MAX_CONTROL(%d)\n", 
                nr_control,NR_MAX_CONTROL);
        exit(1);		
    }
    
    rcode = dvk_open();     //load dvk
    if (rcode < 0)  ERROR_EXIT(rcode);	
    
    init_spread( );
    
    //Global glo.h
    local_nodeid = dvk_getdvsinfo(&dvs);    //This dvk_call gets DVS status and parameter information from the local node.
    if(local_nodeid < 0 )
        ERROR_EXIT(EDVSDVSINIT);    //DVS not initialized
    dvs_ptr = &dvs;     //DVS parameters pointer
    USRDEBUG(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));
    USRDEBUG("PID=%d, local_nodeid=%d nr_control=%d\n", getpid(), local_nodeid, nr_control);
    
    /* get the DC info from kernel */
    for (int i = 0;  i < nr_control; i++){                      //for every node to control
        int dcid = rad_ptr[i]->rad_dcid;
        rcode = dvk_getdcinfo(dcid, &dcu[dcid]);            //gets DC status and parameter info in the local node.
        if(rcode <0) ERROR_EXIT(rcode);
        dc_ptr[dcid] = &dcu[dcid];                          //sets pointer to struct with dc parameters
        USRDEBUG(DC_USR1_FORMAT, DC_USR1_FIELDS(dc_ptr[dcid]));
        USRDEBUG(DC_USR2_FORMAT, DC_USR2_FIELDS(dc_ptr[dcid]));
        
        
        int svrep = rad_ptr[i]->rad_ep;
        //Checks if its a correct server endpoint
        if( svrep >  (dc_ptr[dcid]->dc_nr_sysprocs - dc_ptr[dcid]->dc_nr_tasks)		
           || 	(svrep < (-dc_ptr[dcid]->dc_nr_tasks))){
            fprintf( stderr,"Usage:  be lower than %d >= svr_ep=%d >= %d \n", svrep,
                    (dc_ptr[dcid]->dc_nr_sysprocs - dc_ptr[dcid]->dc_nr_tasks), 
                    (-dc_ptr[dcid]->dc_nr_tasks));
            exit(1);		
        }
        
        // checks if server is running in local_node 
        systask_flag = 0; //Global glo.h
        proc_usr_t svr_usr; /*struct with server data*/
        rcode = dvk_getprocinfo(dcid, svrep, &svr_usr);		//Get the status and parameter information about a process in a DC.
        if(rcode != 0) {
            USRDEBUG("dvk_getprocinfo rcode=%d\n", rcode);
            exit(1);
        }
        
        if( svr_usr.p_rts_flags != SLOT_FREE ) {		// is the process running?
            USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS((&svr_usr)));
            if( local_nodeid == svr_usr.p_nodeid) {
                fprintf( stderr,"Server %s on endpoint %d is RUNNING in this NODE!!!\n", argv[3], svrep);
                systask_flag = 1;
                // @HACK ?
                // exit temporal - removerlo cuando hay multiples servicios a controlar
                exit(1);
            } 
        }
        
        init_control_vars(rad_ptr[i]);		//values of radar_t struct are initialized
        
        srandom( getpid() );					// ??
        //@HACK, should be read from config, @NOTIMPLEMENTED
        rad_ptr[i]->rad_bm_valid = REPL_ANY_NODES;	// valid replication nodes = (-1)
        
        if( svr_usr.p_rts_flags != SLOT_FREE ) {
            // the server endpoint already bound in local_node, then its like it was dead
            //??
            rad_ptr[i]->rad_primary_mbr = svr_usr.p_nodeid;		//set radar actual primary nodeid as nodeid of server
            no_primary_dead(rad_ptr[i]);
        }
        
        USRDEBUG("Starting CONTROL thread[%d] \n", i)
            USRDEBUG(RAD1_FORMAT, RAD1_FIELDS(rad_ptr[i]));
        USRDEBUG(RAD2_FORMAT, RAD2_FIELDS(rad_ptr[i]));
        USRDEBUG(RAD3_FORMAT, RAD3_FIELDS(rad_ptr[i]));
        
        rcode = pthread_create( &rad_ptr[i]->rad_thread, NULL, radar_thread, (void *) rad_ptr[i]);
        
        if( rcode) ERROR_EXIT(rcode);
    }
    
    for (int i = 0; i < nr_control; i++)	{
        rcode = pthread_join(rad_ptr[i]->rad_thread, NULL);			// waits for all threads to end
    }
    return(OK);				
}

/*===========================================================================*
 *				radar_thread				     
 *===========================================================================*/	
void *radar_thread(void *arg)
{
    radar_t *r_ptr = (radar_t*) arg;
    
    connect_to_spread(r_ptr);		//establish connection to spread network and joins a group (fixed at r_ptr->rad_sp_group)
    
    while(TRUE){
        USRDEBUG(RAD1_FORMAT, RAD1_FIELDS(r_ptr));
        USRDEBUG(RAD2_FORMAT, RAD2_FIELDS(r_ptr));
        USRDEBUG(RAD3_FORMAT, RAD3_FIELDS(r_ptr));
        
        int rcode = radar_loop(r_ptr);		//loop receiving messages
        USRDEBUG("rcode=%d\n", rcode);
        if(rcode < 0 ) {
            ERROR_PRINT(rcode);
            sleep(RADAR_ERROR_SPEEP);		//wait RADAR_ERROR_SPEEP=5 sec.
            if( rcode == EDVSNOTCONN) {
                connect_to_spread(r_ptr);
            }
        }
    }
    
    pthread_exit(NULL);
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
    
    test_timeout.sec = RADAR_TIMEOUT_SEC;
    test_timeout.usec = RADAR_TIMEOUT_MSEC;
    
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
void connect_to_spread(radar_t *r_ptr)
{
    int rcode;
    
    /*------------------------------------------------------------------------------------
    * rad_mbr_name:  it must be unique in the spread node.
    *  RADARlocal_nodeid.dcid
    *--------------------------------------------------------------------------------------*/
    sprintf(r_ptr->rad_sp_group, "%s%02d", r_ptr->rad_group, r_ptr->rad_dcid);
    USRDEBUG("spread_group=%s\n", r_ptr->rad_sp_group);
    sprintf( Spread_name, "4803");
    sprintf( r_ptr->rad_mbr_name, "RADAR%02d.%02d", r_ptr->rad_dcid,local_nodeid);
    USRDEBUG("rad_mbr_name=%s\n", r_ptr->rad_mbr_name);
    
    rcode = SP_connect_timeout( Spread_name, r_ptr->rad_mbr_name , 0, 1, 
                               &r_ptr->rad_mbox, r_ptr->rad_priv_group, test_timeout );
    if( rcode != ACCEPT_SESSION ) 	{
        SP_error (rcode);
        ERROR_PRINT(rcode);
        pthread_exit(NULL);
    }
    
    USRDEBUG("rad_mbr_name %s: connected to %s with private group %s\n",
             r_ptr->rad_mbr_name, Spread_name, r_ptr->rad_priv_group);
    
    rcode = SP_join( r_ptr->rad_mbox, r_ptr->rad_sp_group);
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
void init_control_vars(radar_t *r_ptr)
{	
    r_ptr->rad_primary_mbr 	= NO_PRIMARY_BIND;
    r_ptr->rad_primary_old 	= NO_PRIMARY_BIND;
    r_ptr->rad_nr_nodes 	= 0;
    r_ptr->rad_nr_init		= 0;
    r_ptr->rad_nr_radar		= 0;
    r_ptr->rad_bm_nodes 	= 0;
    r_ptr->rad_bm_init		= 0;	
    r_ptr->rad_bm_radar		= 0;
    r_ptr->rad_sp_nr_mbrs 	= 0;
}

/*===========================================================================*
 *				radar_loop				    
 *===========================================================================*/


void update_members(radar_t* r_ptr,
                    char target_groups[MAX_MEMBERS][MAX_GROUP_NAME],
                    int num_groups);
void handle_join(radar_t* r_ptr, int num_groups);
void handle_leave_or_disconnect(radar_t* r_ptr, int num_groups);
int handle_network(radar_t* r_ptr);
int get_nonprimary_node(radar_t* r_ptr);

int radar_loop(radar_t	*r_ptr)
{
    char        sender[MAX_GROUP_NAME];
    char        target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
    int         num_groups = -1;
    int         service_type = 0;
    int16       mess_type;
    int         endian_mismatch;
    int         ret;
    SP_message *sp_ptr;
    
    USRDEBUG("SP_receive: %s of DCID=%d\n", r_ptr->rad_svrname, r_ptr->rad_dcid);
    
    assert(r_ptr->rad_mess_in != NULL);
    
    
    memset(r_ptr->rad_mess_in,0,MAX_MESSLEN);
    //Returns message size in non-error
    ret = SP_receive( r_ptr->rad_mbox, &service_type, sender, 100, &num_groups, target_groups,
                     &mess_type, &endian_mismatch, MAX_MESSLEN, r_ptr->rad_mess_in );
    USRDEBUG("ret=%d\n", ret);
    
    if( ret < 0 ){
        if ( (ret == GROUPS_TOO_SHORT) || (ret == BUFFER_TOO_SHORT) ) {
            service_type = DROP_RECV;
            USRDEBUG("\n========Buffers or Groups too Short=======\n");
            ret = SP_receive( r_ptr->rad_mbox, &service_type, sender, 
                             MAX_MEMBERS, &num_groups, target_groups,
                             &mess_type, &endian_mismatch, MAX_MESSLEN, r_ptr->rad_mess_in);
        }
    }
    
    if (ret < 0 ) {
        SP_error( ret );
        ERROR_PRINT(ret);
        pthread_exit(NULL);
    }
    
    USRDEBUG("%s: sender=%s Private_group=%s service_type=%d\n", 
             r_ptr->rad_svrname,
             sender, 
             r_ptr->rad_priv_group, 
             service_type);
    
    sp_ptr = (SP_message *) r_ptr->rad_mess_in;
    
    if( Is_regular_mess( service_type ) )	{
        //r_ptr->rad_mess_in[ret] = 0;
        if( Is_safe_mess(service_type) || Is_fifo_mess(service_type) ) {
            USRDEBUG("%s: message from %s, of type %d, (endian %d) to %d groups (%d bytes)\n",
                     r_ptr->rad_svrname, sender, mess_type, endian_mismatch, num_groups, ret);
            
            /*----------------------------------------------------------------------------------------------------
            *   MC_RADAR_INFO		The PRIMARY has sent MC_RADAR_INFO message 
            *----------------------------------------------------------------------------------------------------*/
            if ( mess_type == MC_RADAR_INFO ) {
                ret = get_radar_info(r_ptr, sp_ptr);
            } else {
                USRDEBUG("%s: Ignored message type %X\n", r_ptr->rad_svrname, mess_type);
                ret = OK;
            }
        }
        else USRDEBUG("received non SAFE/FIFO message\n");
    }
    else if( Is_membership_mess( service_type ) )	{
        ret = SP_get_memb_info( r_ptr->rad_mess_in, service_type, &r_ptr->rad_memb_info );
        if (ret < 0) {
            USRDEBUG("BUG: membership message does not have valid body\n");
            SP_error( ret );
            ERROR_PRINT(ret);
            pthread_exit(NULL);
        }
        
        if  ( Is_reg_memb_mess( service_type ) ) {
            USRDEBUG("%s: Received REGULAR membership for group %s with %d members, where I am member %d:\n",
                     r_ptr->rad_svrname, sender, num_groups, mess_type );
        }
        
        if( Is_caused_join_mess( service_type ) )	{
            /*----------------------------------------------------------------------------------------------------
            *   JOIN: The group has a new RADAR  member
            *----------------------------------------------------------------------------------------------------*/
            update_members(r_ptr,target_groups,num_groups);
            
            USRDEBUG("%s: Due to the JOIN of %s service_type=%d\n", 
                     r_ptr->rad_svrname, r_ptr->rad_memb_info.changed_member, service_type );
            
            handle_join(r_ptr,num_groups);
        }else if( Is_caused_leave_mess( service_type ) 
                 ||  Is_caused_disconnect_mess( service_type ) ){
            /*----------------------------------------------------------------------------------------------------
            *   LEAVE or DISCONNECT:  A member has left the group
            *----------------------------------------------------------------------------------------------------*/
            update_members(r_ptr,target_groups,num_groups);
            
            USRDEBUG("%s: Due to the LEAVE or DISCONNECT of %s\n", 
                     r_ptr->rad_svrname, r_ptr->rad_memb_info.changed_member );
            
            handle_leave_or_disconnect(r_ptr,num_groups);
        }else if( Is_caused_network_mess( service_type ) ){
            /*----------------------------------------------------------------------------------------------------
            *   NETWORK CHANGE:  A network partition or a dead deamon
            *----------------------------------------------------------------------------------------------------*/
            USRDEBUG("%s: Due to NETWORK change with %u VS sets\n", 
                     r_ptr->rad_svrname,  r_ptr->rad_num_vs_sets);
            
            ret = handle_network(r_ptr);
            
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
                     sender, service_type, mess_type, endian_mismatch, num_groups, ret, r_ptr->rad_mess_in );
        }else {
            USRDEBUG("received message of unknown message type 0x%x with ret %d\n", service_type, ret);
        }
    }
    if(ret < 0) ERROR_RETURN(ret);
    return(ret);
}

/*===========================================================================*
*				get_radar_info								     	*
* The PRIMARY member sent a multicast to no sync members					*
===========================================================================*/
int get_radar_info(radar_t	*r_ptr,	SP_message  *sp_ptr)
{
    int ret;
    node_usr_t *n_ptr, n_usr;
    proc_usr_t p_usr, *p_ptr;
    message *m_ptr;
    
    USRDEBUG("\n");
    
    p_ptr = &p_usr;
    n_ptr = &n_usr;
    m_ptr = &sp_ptr->msg;
    
    assert( sp_ptr->msg.m_type == MC_RADAR_INFO);
    
    USRDEBUG("MC_RADAR_INFO = (m_source = %d,m_type = %d, nr_nodes = %d, nr_sync = %d"
             ", rd_ep = %d, bm_nodes = %ld, bm_sync = %ld)",
             m_ptr->m_source,
             m_ptr->m_type,
             m_ptr->m2_i1,
             m_ptr->m2_i2,
             m_ptr->m2_i3,
             m_ptr->m2_l1,
             m_ptr->m2_l2);
    
    
    // checks if remote node is DVK configured for local node  
    ret = dvk_getnodeinfo(sp_ptr->msg.m_source, n_ptr);
    if( ret != OK) ERROR_RETURN(ret);
    USRDEBUG(NODE_USR_FORMAT, NODE_USR_FIELDS(n_ptr));
    if( n_ptr->n_flags == NODE_FREE)
        ERROR_RETURN(EDVSNODEFREE);
    
    // checks if remote node is  connected through proxies 
    if( TEST_BIT(n_ptr->n_flags,NODE_BIT_ATTACHED) == 0)
        ERROR_RETURN(EDVSNOTCONN);
    
    // checks if remote node belongs to the DCID 
    if( TEST_BIT(n_ptr->n_dcs,r_ptr->rad_dcid) == 0)
        ERROR_RETURN(EDVSNODCNODE);
    
    USRDEBUG(MSG2_FORMAT, MSG2_FIELDS(m_ptr));
    // checks if the controlled endpoint is correct
    if(  sp_ptr->msg.m2_i3 != r_ptr->rad_ep)
        ERROR_RETURN(EDVSENDPOINT);
    
    // checks if node is a valid node 
    if( TEST_BIT(r_ptr->rad_bm_valid, sp_ptr->msg.m_source) == 0)
        ERROR_RETURN(EDVSNONODE);
    
    int primary_new = sp_ptr->msg.m_source;
    
    USRDEBUG("%s: primary_mbr=%d primary_old=%d primary_new=%d\n",
             r_ptr->rad_svrname, r_ptr->rad_primary_mbr, r_ptr->rad_primary_old, primary_new);
    
    switch (r_ptr->rad_primary_mbr) {
        case NO_PRIMARY_BIND:	
        if( r_ptr->rad_replication == REPLICA_RSM){
            r_ptr->rad_primary_mbr  = get_random_node(r_ptr,sp_ptr->msg.m2_i2);
        } else {
            r_ptr->rad_primary_mbr  = primary_new;
        }
        ret = dvk_getprocinfo(r_ptr->rad_dcid, r_ptr->rad_ep, p_ptr);
        USRDEBUG("%s: " PROC_USR_FORMAT, r_ptr->rad_svrname, PROC_USR_FIELDS(p_ptr));
        if( ret == OK){
            if( primary_new != p_ptr->p_nodeid) {
                USRDEBUG("%s: old primary differs from new primary\n", r_ptr->rad_svrname);
                if(!TEST_BIT(p_ptr->p_rts_flags, BIT_SLOT_FREE)){
                    ret = dvk_unbind(r_ptr->rad_dcid,r_ptr->rad_ep);
                }
                ret = dvk_rmtbind(r_ptr->rad_dcid, r_ptr->rad_svrname, r_ptr->rad_ep, primary_new);	
            } else {
                USRDEBUG("%s: old primary it is the same as new primary\n", r_ptr->rad_svrname);				
            }
        } else{
            USRDEBUG("%s: binding new primary\n", r_ptr->rad_svrname);
            ret = dvk_rmtbind(r_ptr->rad_dcid, r_ptr->rad_svrname, r_ptr->rad_ep, primary_new);	
        }
        break;
        case NO_PRIMARY_DEAD:
        USRDEBUG("%s: The old primary has dead\n", r_ptr->rad_svrname);
        if( r_ptr->rad_replication == REPLICA_RSM){
            r_ptr->rad_primary_mbr  = get_random_node(r_ptr,sp_ptr->msg.m2_i2);
        } else {
            r_ptr->rad_primary_mbr  = primary_new;
        }
        if ( r_ptr->rad_primary_old != primary_new) {
            USRDEBUG("%s: old primary differs from new primary\n", r_ptr->rad_svrname);
            ret = dvk_migr_commit(PROC_NO_PID, r_ptr->rad_dcid, r_ptr->rad_ep, r_ptr->rad_primary_mbr );
        }else{ 
            USRDEBUG("%s: new primary is the same as old primary\n", r_ptr->rad_svrname);
            ret = dvk_migr_rollback(r_ptr->rad_dcid, r_ptr->rad_ep);
        }
        break;
        case NO_PRIMARY_NET:
        USRDEBUG("%s: The old primary was on other PARTITION\n", r_ptr->rad_svrname);
        if( r_ptr->rad_replication == REPLICA_RSM){
            r_ptr->rad_primary_mbr  = get_random_node(r_ptr,sp_ptr->msg.m2_i2);
        } else {
            r_ptr->rad_primary_mbr  = primary_new;
        }
        if ( r_ptr->rad_primary_old != primary_new) {
            USRDEBUG("%s: old primary differs from new primary\n", r_ptr->rad_svrname);
            ret = dvk_migr_commit(PROC_NO_PID, r_ptr->rad_dcid, r_ptr->rad_ep, r_ptr->rad_primary_mbr);
        }else{ 
            USRDEBUG("%s: new primary is the same as old primary\n", r_ptr->rad_svrname);
            USRDEBUG("dvk_migr_rollback(dcid = %d,ep = %d)\n",r_ptr->rad_dcid,
                     r_ptr->rad_ep);
            ret = dvk_migr_rollback(r_ptr->rad_dcid, r_ptr->rad_ep);
            USRDEBUG("ROLLBACK PASSED\n");
        }
        break;
        default:
        break;
    } 			
    
    r_ptr->rad_nr_nodes   	= sp_ptr->msg.m2_i1;
    r_ptr->rad_nr_init 	   = sp_ptr->msg.m2_i2;
    r_ptr->rad_bm_nodes 	  = sp_ptr->msg.m2_l1;
    r_ptr->rad_bm_init		= sp_ptr->msg.m2_l2;
    USRDEBUG("%s: " RAD1_FORMAT, r_ptr->rad_svrname, RAD1_FIELDS(r_ptr));
    USRDEBUG("%s: " RAD2_FORMAT, r_ptr->rad_svrname, RAD2_FIELDS(r_ptr));
    USRDEBUG("%s: " RAD3_FORMAT, r_ptr->rad_svrname, RAD3_FIELDS(r_ptr));
    
    return(OK);	
}

/*===========================================================================*
*				get_random_node							     	*
===========================================================================*/
int get_random_node(radar_t	*r_ptr,unsigned int eligible_nodes)
{
    //@SPEED slow algorithm, should make a 64 size static array 
    //and choose from there, or something like that
    int r, retries;
    
    retries  = MAX_RANDOM_RETRIES;
    
    USRDEBUG("eligible_nodes=%X \n", eligible_nodes);
    assert( eligible_nodes != 0);
    
    eligible_nodes &=  r_ptr->rad_bm_valid;
    if( eligible_nodes == 0) return(-1);
    
    do {
        r = (random()/(RAND_MAX/sizeof(eligible_nodes)));
        if ( TEST_BIT(eligible_nodes, r) != 0){
            USRDEBUG("random node=%d \n", r);
            return(r);
        }
    } while( --retries != 0);
    USRDEBUG("exhausted attempts");
    
    return(get_first_mbr(eligible_nodes));	
}


/*===========================================================================*
*				no_primary_dead								     	*
===========================================================================*/
int no_primary_dead(radar_t	*r_ptr)
{
    USRDEBUG("%s BEFORE: rad_primary_mbr=%d  rad_primary_old=%d \n", 
             r_ptr->rad_svrname, r_ptr->rad_primary_mbr, r_ptr->rad_primary_old );
#ifdef RADAR 
    //@TODO: check return value of migr start 
    dvk_migr_start(r_ptr->rad_dcid, r_ptr->rad_ep);	// stops all messages and data transfers addressed
    // to the mentioned endpoint on the DC with dcid.
#endif // RADAR 
    
    
    CLR_BIT(r_ptr->rad_bm_nodes,r_ptr->rad_primary_mbr);
    
    r_ptr->rad_primary_old = r_ptr->rad_primary_mbr;	//actual ep from primary is set as old
    r_ptr->rad_primary_mbr = NO_PRIMARY_DEAD;			//actual ep is set to NO_PRIMARY_DEAD (-2)
    
    USRDEBUG("%s AFTER: rad_primary_mbr=%d  rad_primary_old=%d \n",
             r_ptr->rad_svrname, r_ptr->rad_primary_mbr, r_ptr->rad_primary_old );
    return(OK);	
}

/*===========================================================================*
*				no_primary_net								     	*
===========================================================================*/
int no_primary_net(radar_t	*r_ptr)
{
    USRDEBUG("%s BEFORE: rad_primary_mbr=%d  rad_primary_old=%d \n", 
             r_ptr->rad_svrname, r_ptr->rad_primary_mbr, r_ptr->rad_primary_old );
#ifdef RADAR 
    //@TODO: check return value of migr start 
    dvk_migr_start(r_ptr->rad_dcid, r_ptr->rad_ep);	// stops all messages and data transfers addressed
    // to the mentioned endpoint on the DC with dcid.
#endif // RADAR 
    
    CLR_BIT(r_ptr->rad_bm_nodes,r_ptr->rad_primary_mbr);
    
    r_ptr->rad_primary_old = r_ptr->rad_primary_mbr;	//actual ep from primary is set as old
    r_ptr->rad_primary_mbr = NO_PRIMARY_NET;			//actual ep is set to NO_PRIMARY_NET (-3)
    
    USRDEBUG("%s AFTER: rad_primary_mbr=%d  rad_primary_old=%d \n",
             r_ptr->rad_svrname, r_ptr->rad_primary_mbr, r_ptr->rad_primary_old );
    return(OK);	
}

/***************************************************************************/
/* ANCILLIARY FUNCTIONS 										*/
/***************************************************************************/

/*===========================================================================*
*				get_dcid				     *
* It converts a node string provided by SPREAD into a DCID
* format of mbr_string ""#xxxxxDCID.NODEID#nodename" 
* format of grp_name "xxxxx" 
*===========================================================================*/
int get_dcid(char *grp_name, char *mbr_string)
{
    char *n_ptr, *dot_ptr;
    int dcid, len;
    
    len = strlen(grp_name);
    mbr_string++; // skip the # character 
    USRDEBUG("grp_name=%s mbr_string=%s len=%d\n", grp_name,mbr_string, len);
    assert(strncmp(grp_name, mbr_string, len) == 0);  
    
    dot_ptr = strchr(&mbr_string[len], '.'); /* locate the dot character after "#xxxxxNN.yyyy" */
    assert(dot_ptr != NULL);
    
    *dot_ptr = '\0';
    n_ptr = &mbr_string[len];
    dcid = atoi(n_ptr);
    *dot_ptr = '.';
    
    USRDEBUG("member=%s dcid=%d\n", mbr_string,  dcid );
    return(dcid);
}

/*===========================================================================*
*				get_nodeid				     *
* It converts a node string provided by SPREAD into a NODEID
* format of mbr_string "#xxxxxDCID.NODEID#nodename" 
* format of grp_name "xxxxx" 
*===========================================================================*/
int get_nodeid(char *grp_name, char *mbr_string)
{
    char *s_ptr, *n_ptr, *dot_ptr;
    int nid, len;
    
    len = strlen(grp_name);
    mbr_string++; // skip the # character 
    USRDEBUG("grp_name=%s mbr_string=%s len=%d\n", grp_name,mbr_string, len);
    assert(strncmp(grp_name, mbr_string, len) == 0);  
    
    dot_ptr = strchr(&mbr_string[len], '.'); 
    assert(dot_ptr != NULL);
    n_ptr = dot_ptr+1;					
    
    s_ptr = strchr(n_ptr, '#'); 
    assert(s_ptr != NULL);
    
    *s_ptr = '\0';
    nid = atoi(n_ptr);
    *s_ptr = '#';
    USRDEBUG("member=%s nid=%d\n", mbr_string,  nid );
    
    return(nid);
}

/*===========================================================================*
*				get_first_mbr				     
*===========================================================================*/
int get_first_mbr(unsigned int nodes )
{
    int i;
    
    USRDEBUG("nodes=%X\n", nodes);
    assert(nodes != 0);
    
    i = 0;
    do {
        if (TEST_BIT(nodes, i) != 0 ){
            USRDEBUG("first_mbr=%d\n", i );
            return(i);
            break;
        } 
        i++;
    } while(1);
    assert(0);
    return(0);
}



void update_members(radar_t* r_ptr,
                    char target_groups[MAX_MEMBERS][MAX_GROUP_NAME],
                    int num_groups){
    r_ptr->rad_sp_nr_mbrs = num_groups;
    memcpy((void*) r_ptr->rad_sp_members, (void *) target_groups, r_ptr->rad_sp_nr_mbrs*MAX_GROUP_NAME);
    for(int i=0; i < r_ptr->rad_sp_nr_mbrs; i++ ){
        USRDEBUG("\t%s\n", &r_ptr->rad_sp_members[i][0]);
    }
}

void handle_join(radar_t* r_ptr,int num_groups){
    if ( strncmp(r_ptr->rad_memb_info.changed_member, "#RADAR", 6) == 0) {
        int mbr  = get_nodeid("RADAR", (char *)  r_ptr->rad_memb_info.changed_member);
        //int dcid = get_dcid("RADAR", (char *) r_ptr->rad_memb_info.changed_member);
        USRDEBUG("%s: JOIN - nr_radar=%d bm_radar=%X\n", 
                 r_ptr->rad_svrname, r_ptr->rad_nr_radar, r_ptr->rad_bm_radar); 
        r_ptr->rad_nr_radar = num_groups - r_ptr->rad_nr_nodes;
        SET_BIT(r_ptr->rad_bm_radar, mbr);
        USRDEBUG("%s: JOIN end - nr_radar=%d bm_radar=%X\n", 
                 r_ptr->rad_svrname, r_ptr->rad_nr_radar, r_ptr->rad_bm_radar); 
    }
}

void handle_leave_or_disconnect(radar_t* r_ptr,int num_groups){
    if ( strncmp(r_ptr->rad_memb_info.changed_member, "#RADAR",6) == 0) { 
        int mbr  = get_nodeid("RADAR", (char *) r_ptr->rad_memb_info.changed_member);
        //int dcid = get_dcid("RADAR", (char *) r_ptr->rad_memb_info.changed_member);
        USRDEBUG("%s: LEAVE - nr_radar=%d bm_radar=%X\n", 
                 r_ptr->rad_svrname, r_ptr->rad_nr_radar, r_ptr->rad_bm_radar); 
        r_ptr->rad_nr_radar = num_groups - r_ptr->rad_nr_nodes;
        CLR_BIT(r_ptr->rad_bm_radar, mbr);
        USRDEBUG("%s: LEAVE end - nr_radar=%d bm_radar=%X\n", 
                 r_ptr->rad_svrname, r_ptr->rad_nr_radar, r_ptr->rad_bm_radar); 
    }else{
        int mbr  = get_nodeid(r_ptr->rad_svrname, (char *) r_ptr->rad_memb_info.changed_member);
        int dcid = get_dcid(r_ptr->rad_svrname, (char *) r_ptr->rad_memb_info.changed_member);
        USRDEBUG("%s: LEAVE - mbr=%d dcid=%d\n", 
                 r_ptr->rad_svrname, mbr, dcid); 
        if( dcid == r_ptr->rad_dcid)
            if( mbr == r_ptr->rad_primary_mbr)
            no_primary_dead(r_ptr);
    }
}


int handle_network(radar_t* r_ptr){
    int ret = 0;
    r_ptr->rad_num_vs_sets = 
        SP_get_vs_sets_info(r_ptr->rad_mess_in, 
                            &r_ptr->rad_vssets[0], 
                            MAX_VSSETS, 
                            &r_ptr->rad_my_vsset_index);
    
    if (r_ptr->rad_num_vs_sets < 0) {
        USRDEBUG("BUG: membership message has more then %d vs sets."
                 "Recompile with larger MAX_VSSETS\n",
                 MAX_VSSETS);
        SP_error( r_ptr->rad_num_vs_sets );
        ERROR_EXIT( r_ptr->rad_num_vs_sets );
    }
    if (r_ptr->rad_num_vs_sets == 0) {
        USRDEBUG("BUG: membership message has %d vs_sets\n", 
                 r_ptr->rad_num_vs_sets);
        SP_error( r_ptr->rad_num_vs_sets );
        ERROR_EXIT( EDVSGENERIC );
    }
    
    
    //Clears nodes/radars bitmap
    r_ptr->rad_bm_nodes = 0;
    r_ptr->rad_nr_nodes = 0;
    r_ptr->rad_bm_radar = 0;
    r_ptr->rad_nr_radar = 0;
    int dcid = 0;
    
    for(int i = 0; i < r_ptr->rad_num_vs_sets; i++ )  {
        USRDEBUG("%s VS set %d has %u members:\n",
                 (i  == r_ptr->rad_my_vsset_index) ?("LOCAL") : ("OTHER"), 
                 i, r_ptr->rad_vssets[i].num_members );
        ret = SP_get_vs_set_members(r_ptr->rad_mess_in, &r_ptr->rad_vssets[i], r_ptr->rad_members, MAX_MEMBERS);
        if (ret < 0) {
            USRDEBUG("VS Set has more then %d members. Recompile with larger MAX_MEMBERS\n", MAX_MEMBERS);
            SP_error( ret );
            ERROR_EXIT( ret);
        }
        
        /*---------------------------------------------
        * get the bitmap of current members
        --------------------------------------------- */
        for(int j = 0; j < r_ptr->rad_vssets[i].num_members; j++ ) {
            USRDEBUG("\t%s\n", r_ptr->rad_members[j] );
            if ( strncmp(r_ptr->rad_members[j], "#RADAR",6) == 0) {	
                int mbr = get_nodeid("RADAR", r_ptr->rad_members[j]);
                dcid = get_dcid("RADAR", r_ptr->rad_members[j]);
                //@SPEED: might not need to test bit before setting it. CHECK IMPLEMENTATION
                if(!TEST_BIT(r_ptr->rad_bm_radar, mbr)) {
                    SET_BIT(r_ptr->rad_bm_radar, mbr);
                    r_ptr->rad_nr_radar++;
                }		
            }else{
                int mbr = get_nodeid(r_ptr->rad_svrname, r_ptr->rad_members[j]);
                dcid = get_dcid(r_ptr->rad_svrname, r_ptr->rad_members[j]);
                if( dcid == r_ptr->rad_dcid) {
                    if(!TEST_BIT(r_ptr->rad_bm_nodes, mbr)) {
                        SET_BIT(r_ptr->rad_bm_nodes, mbr);
                        r_ptr->rad_nr_nodes++;
                    }
                }
            }
        }
        USRDEBUG("%s: old bm_init=%X bm_nodes=%X primary_mbr=%d nr_radar=%d bm_radar=%X\n",
                 r_ptr->rad_svrname, r_ptr->rad_bm_init, r_ptr->rad_bm_nodes, 
                 r_ptr->rad_primary_mbr, r_ptr->rad_nr_radar, r_ptr->rad_bm_radar);
    }
    
    if(!TEST_BIT(r_ptr->rad_bm_nodes,r_ptr->rad_primary_mbr) &&
        r_ptr->rad_primary_mbr != NO_PRIMARY_NET){

        int newprimary = get_nonprimary_node(r_ptr);
        if(newprimary < 0 ){
            no_primary_net(r_ptr);
        }
        else{
            //@TODO: should check if endpoint is running, right?
            USRDEBUG("Primary dead, found replacement node %d, starting migr\n",newprimary);
            ret = dvk_migr_start(r_ptr->rad_dcid, r_ptr->rad_ep);
            
            r_ptr->rad_primary_old = r_ptr->rad_primary_mbr;
            r_ptr->rad_primary_mbr = newprimary;
            
            dvk_migr_commit(PROC_NO_PID,
                            r_ptr->rad_dcid,
                            r_ptr->rad_ep, 
                            r_ptr->rad_primary_mbr);
            
            USRDEBUG("Migration commited.\n");
        }
    }
    
    
    USRDEBUG("%s: new bm_init=%X bm_nodes=%X primary_mbr=%d nr_radar=%d bm_radar=%X\n",
             r_ptr->rad_svrname, r_ptr->rad_bm_init, r_ptr->rad_bm_nodes, 
             r_ptr->rad_primary_mbr, r_ptr->rad_nr_radar, r_ptr->rad_bm_radar);
    
    return 0;
}


int get_nonprimary_node(radar_t* rad){
    if(rad == NULL) return -1;
    if(rad->rad_nr_nodes == 0) return -2;
    if(rad->rad_nr_nodes == 1 && 
       TEST_BIT(rad->rad_bm_nodes,rad->rad_primary_mbr)) return -3;
    
    unsigned int eligible_nodes = rad->rad_bm_nodes;
    if(rad->rad_primary_mbr>=0){
        CLR_BIT(eligible_nodes,rad->rad_primary_mbr);
    }
    
    int ret = get_random_node(rad,eligible_nodes);
    if(ret<0) return -4;
    return ret;
}