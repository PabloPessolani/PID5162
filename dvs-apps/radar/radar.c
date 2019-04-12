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
#define TRUE 1

#define RADAR	1 

int radar_loop(radar_t	*r_ptr);
int get_radar_info(radar_t	*r_ptr,	SP_message  *sp_ptr);
int no_primary_dead(radar_t	*r_ptr);
int no_primary_part(radar_t	*r_ptr);
int get_nodeid(char *grp_name, char *mbr_string);
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
//PUBLIC int main(void)
int main (int argc, char *argv[] )
{
	int rcode, dcid, svrep, i;
	proc_usr_t svr_usr, *svr_ptr;

	if ( argc != 2) {
 	    fprintf( stderr,"Usage: %s <config_file> \n", argv[0] );
 	    exit(1);
    }
	
	for ( nr_control = 0; nr_control < NR_MAX_CONTROL; nr_control++){
		posix_memalign( (void**) &rad_ptr[nr_control], getpagesize(), sizeof(radar_t));
		if( rad_ptr[nr_control] == NULL) return (EDVSNOMEM);
	}
	
	nr_control = 0;
	radar_config(argv[1]);
	if( nr_control > NR_MAX_CONTROL){
 	    fprintf( stderr,"The count of pair {dc,enpoint} to control (%d) >= NR_MAX_CONTROL(%d)\n", 
			nr_control,NR_MAX_CONTROL);
 	    exit(1);		
	}
	
	rcode = dvk_open();
	if (rcode < 0)  ERROR_EXIT(rcode);	
	
	init_spread( );
	
	local_nodeid = dvk_getdvsinfo(&dvs);
	if(local_nodeid < 0 )
		ERROR_EXIT(EDVSDVSINIT);
	dvs_ptr = &dvs;
	USRDEBUG(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));
	USRDEBUG("PID=%d, local_nodeid=%d nr_control=%d\n", getpid(), local_nodeid, nr_control);
	
	/* get the DC info from kernel */
	for ( i = 0;  i < nr_control; i++){
		dcid = rad_ptr[i]->rad_dcid;
		rcode = dvk_getdcinfo(dcid, &dcu[dcid]);
		if(rcode <0) ERROR_EXIT(rcode);
		dc_ptr[dcid] = &dcu[dcid];
		USRDEBUG(DC_USR1_FORMAT, DC_USR1_FIELDS(dc_ptr[dcid]));
		USRDEBUG(DC_USR2_FORMAT, DC_USR2_FIELDS(dc_ptr[dcid]));
	
	
		rad_ptr[i]->rad_ep;
		if( svrep >  (dc_ptr[dcid]->dc_nr_sysprocs - dc_ptr[dcid]->dc_nr_tasks)
			|| 	(svrep < (-dc_ptr[dcid]->dc_nr_tasks))){
			fprintf( stderr,"Usage:  be lower than %d >= svr_ep=%d >= %d \n", svrep,
				(dc_ptr[dcid]->dc_nr_sysprocs - dc_ptr[dcid]->dc_nr_tasks), 
				(-dc_ptr[dcid]->dc_nr_tasks));
			exit(1);		
		}
     
		// checks if server is running in local_node 
		systask_flag = 0;
		rcode = dvk_getprocinfo(dcid, svrep, &svr_usr);
		if(rcode != 0) {
			USRDEBUG("dvk_getprocinfo rcode=%d\n", rcode);
			exit(1);
		}
		
		svr_ptr = &svr_usr;
		if( svr_ptr->p_rts_flags != SLOT_FREE ) {
			USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(svr_ptr));
			if( local_nodeid == svr_ptr->p_nodeid) {
				fprintf( stderr,"Server %s on endpoint %d is RUNNING in this NODE!!!\n", argv[3], svrep);
				systask_flag = 1;
			// exit temporal - removerlo cuando hay multiples servicios a controlar
				exit(1);
			} 
		}
	
		init_control_vars(rad_ptr[i]);

		srandom( getpid());
		rad_ptr[i]->rad_bm_valid = REPL_ANY_NODES;	

		if( svr_ptr->p_rts_flags != SLOT_FREE ) {
			// the server endpoint already bound in local_node, then its like it was dead 
			rad_ptr[i]->rad_primary_mbr = svr_ptr->p_nodeid;
			no_primary_dead(&rad_ptr[i]);	
		}
	
		USRDEBUG("Starting CONTROL thread[%d] \n", i)
		USRDEBUG(RAD1_FORMAT, RAD1_FIELDS(rad_ptr[i]));
		USRDEBUG(RAD2_FORMAT, RAD2_FIELDS(rad_ptr[i]));
		USRDEBUG(RAD3_FORMAT, RAD3_FIELDS(rad_ptr[i]));
		
		rcode = pthread_create( &rad_ptr[i]->rad_thread, NULL, radar_thread, (void *) rad_ptr[i]);
		if( rcode) ERROR_EXIT(rcode);
	}

	
	for ( i = 0; i < nr_control; i++)	{
		rcode = pthread_join(rad_ptr[i]->rad_thread, NULL);
	}
	return(OK);				
}

/*===========================================================================*
 *				radar_thread				     
 *===========================================================================*/	
void *radar_thread(void *arg)
{
	radar_t	*r_ptr;
	int rcode;
	
	r_ptr = (int *) arg;

		
	connect_to_spread(r_ptr);
	while(TRUE){
		USRDEBUG(RAD1_FORMAT, RAD1_FIELDS(r_ptr));
		USRDEBUG(RAD2_FORMAT, RAD2_FIELDS(r_ptr));
		USRDEBUG(RAD3_FORMAT, RAD3_FIELDS(r_ptr));
		rcode = radar_loop(r_ptr);
		USRDEBUG("rcode=%d\n", rcode);
		if(rcode < 0 ) {
			ERROR_PRINT(rcode);
			sleep(RADAR_ERROR_SPEEP);
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

int radar_loop(radar_t	*r_ptr)
{
	char	sender[MAX_GROUP_NAME];
	char	target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
	int		num_groups;

	int		service_type;
	int16	mess_type;
	int		endian_mismatch;
	int		i,j;
	int		ret, mbr, dcid;
	SP_message *sp_ptr;

	service_type = 0;
	num_groups = -1;

	USRDEBUG("SP_receive: %s of DCID=%d\n", r_ptr->rad_svrname, r_ptr->rad_dcid);
	
	assert(r_ptr->rad_mbox != NULL);
	assert(r_ptr->rad_mess_in != NULL);
		
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
	
	USRDEBUG(" sender=%s Private_group=%s service_type=%d\n", r_ptr->rad_svrname,
			sender, r_ptr->rad_priv_group, service_type);

	sp_ptr = (SP_message *) r_ptr->rad_mess_in;
	
	if( Is_regular_mess( service_type ) )	{
		r_ptr->rad_mess_in[ret] = 0;
		if     ( Is_unreliable_mess( service_type ) ) {USRDEBUG("received UNRELIABLE \n ");}
		else if( Is_reliable_mess(   service_type ) ) {USRDEBUG("received RELIABLE \n");}
		else if( Is_causal_mess(       service_type ) ) {USRDEBUG("received CAUSAL \n");}
		else if( Is_agreed_mess(       service_type ) ) {USRDEBUG("received AGREED \n");}
		else if( Is_safe_mess(   service_type ) || Is_fifo_mess(       service_type ) ) {
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
	}else if( Is_membership_mess( service_type ) )	{
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

			if( Is_caused_join_mess( service_type ) ||
				Is_caused_leave_mess( service_type ) ||
				Is_caused_disconnect_mess( service_type ) ){
				r_ptr->rad_sp_nr_mbrs = num_groups;
				memcpy((void*) r_ptr->rad_sp_members, (void *) target_groups, r_ptr->rad_sp_nr_mbrs*MAX_GROUP_NAME);
				for( i=0; i < r_ptr->rad_sp_nr_mbrs; i++ ){
					USRDEBUG("\t%s\n", &r_ptr->rad_sp_members[i][0]);
				}
			}
		}

		if( Is_caused_join_mess( service_type ) )	{
			/*----------------------------------------------------------------------------------------------------
			*   JOIN: The group has a new RADAR  member
			*----------------------------------------------------------------------------------------------------*/
			USRDEBUG("%s: Due to the JOIN of %s service_type=%d\n", 
				r_ptr->rad_svrname, r_ptr->rad_memb_info.changed_member, service_type );
			if ( strncmp(r_ptr->rad_memb_info.changed_member, "#RADAR", 6) == 0) {
				mbr = get_nodeid("RADAR", (char *)  r_ptr->rad_memb_info.changed_member);
				dcid= get_dcid("RADAR", (char *) r_ptr->rad_memb_info.changed_member);
				USRDEBUG("%s: JOIN - nr_radar=%d bm_radar=%X\n", 
					r_ptr->rad_svrname, r_ptr->rad_nr_radar, r_ptr->rad_bm_radar); 
				r_ptr->rad_nr_radar = num_groups - r_ptr->rad_nr_nodes;
				SET_BIT(r_ptr->rad_bm_radar, mbr);
				USRDEBUG("%s: JOIN end - nr_radar=%d bm_radar=%X\n", 
					r_ptr->rad_svrname, r_ptr->rad_nr_radar, r_ptr->rad_bm_radar); 
			}
		}else if( Is_caused_leave_mess( service_type ) 
			||  Is_caused_disconnect_mess( service_type ) ){
			/*----------------------------------------------------------------------------------------------------
			*   LEAVE or DISCONNECT:  A member has left the group
			*----------------------------------------------------------------------------------------------------*/
			USRDEBUG("%s: Due to the LEAVE or DISCONNECT of %s\n", 
				r_ptr->rad_svrname, r_ptr->rad_memb_info.changed_member );
			if ( strncmp(r_ptr->rad_memb_info.changed_member, "#RADAR",6) == 0) { 
				mbr = get_nodeid("RADAR", (char *) r_ptr->rad_memb_info.changed_member);
				dcid= get_dcid("RADAR", (char *) r_ptr->rad_memb_info.changed_member);
				USRDEBUG("%s: LEAVE - nr_radar=%d bm_radar=%X\n", 
					r_ptr->rad_svrname, r_ptr->rad_nr_radar, r_ptr->rad_bm_radar); 
				r_ptr->rad_nr_radar = num_groups - r_ptr->rad_nr_nodes;
				CLR_BIT(r_ptr->rad_bm_radar, mbr);
				USRDEBUG("%s: LEAVE end - nr_radar=%d bm_radar=%X\n", 
					r_ptr->rad_svrname, r_ptr->rad_nr_radar, r_ptr->rad_bm_radar); 
			}else{
				mbr = get_nodeid(r_ptr->rad_svrname, (char *) r_ptr->rad_memb_info.changed_member);
				dcid= get_dcid(r_ptr->rad_svrname, (char *) r_ptr->rad_memb_info.changed_member);
				USRDEBUG("%s: LEAVE - mbr=%d dcid=%d\n", 
					r_ptr->rad_svrname, mbr, dcid); 
				if( dcid == r_ptr->rad_dcid)
					if( mbr == r_ptr->rad_primary_mbr)
						no_primary_dead(r_ptr);
			}
		}else if( Is_caused_network_mess( service_type ) ){
			/*----------------------------------------------------------------------------------------------------
			*   NETWORK CHANGE:  A network partition or a dead deamon
			*----------------------------------------------------------------------------------------------------*/
			USRDEBUG("%s: Due to NETWORK change with %u VS sets\n", 
				r_ptr->rad_svrname, r_ptr->rad_memb_info, r_ptr->rad_num_vs_sets);
            r_ptr->rad_num_vs_sets = SP_get_vs_sets_info( r_ptr->rad_mess_in, 
									&r_ptr->rad_vssets[0], MAX_VSSETS, &r_ptr->rad_my_vsset_index );
            if (r_ptr->rad_num_vs_sets < 0) {
				USRDEBUG("BUG: membership message has more then %d vs sets. Recompile with larger MAX_VSSETS\n",
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

			r_ptr->rad_bm_nodes = 0;
			r_ptr->rad_nr_nodes = 0;
            for( i = 0; i < r_ptr->rad_num_vs_sets; i++ )  {
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
				for( j = 0; j < r_ptr->rad_vssets[i].num_members; j++ ) {
					USRDEBUG("\t%s\n", r_ptr->rad_members[j] );
					if ( strncmp(r_ptr->rad_members[j], "#RADAR",6) == 0) {	
						mbr = get_nodeid("RADAR", r_ptr->rad_members[j]);
						dcid = get_dcid("RADAR", r_ptr->rad_members[j]);
						if(!TEST_BIT(r_ptr->rad_bm_radar, mbr)) {
							SET_BIT(r_ptr->rad_bm_radar, mbr);
							r_ptr->rad_nr_radar++;
						}		
					}else{
						mbr = get_nodeid(r_ptr->rad_svrname, r_ptr->rad_members[j]);
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
			if( dcid == r_ptr->rad_dcid){
				no_primary_net(r_ptr);	
			}
			USRDEBUG("%s: new bm_init=%X bm_nodes=%X primary_mbr=%d nr_radar=%d bm_radar=%X\n",
					r_ptr->rad_svrname, r_ptr->rad_bm_init, r_ptr->rad_bm_nodes, 
					r_ptr->rad_primary_mbr, r_ptr->rad_nr_radar, r_ptr->rad_bm_radar);
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
	int primary_new;
	node_usr_t *n_ptr, n_usr;
	proc_usr_t p_usr, *p_ptr;
	message *m_ptr;

	USRDEBUG("\n");

	p_ptr = &p_usr;
	n_ptr = &n_usr;
	m_ptr = &sp_ptr->msg;
	assert( sp_ptr->msg.m_type == MC_RADAR_INFO);
	
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
		
	primary_new = sp_ptr->msg.m_source;
	USRDEBUG("%s: primary_mbr=%d primary_old=%d primary_new=%d\n",
		r_ptr->rad_svrname, r_ptr->rad_primary_mbr, r_ptr->rad_primary_old, primary_new);

	switch (r_ptr->rad_primary_mbr) {
		case NO_PRIMARY_BIND:	
			if( r_ptr->rad_replication == REPLICA_RSM){
				r_ptr->rad_primary_mbr  = get_random_node(sp_ptr->msg.m2_i2);
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
				r_ptr->rad_primary_mbr  = get_random_node(sp_ptr->msg.m2_i2);
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
				r_ptr->rad_primary_mbr  = get_random_node(sp_ptr->msg.m2_i2);
			} else {
				r_ptr->rad_primary_mbr  = primary_new;
			}
			if ( r_ptr->rad_primary_old != primary_new) {
				USRDEBUG("%s: old primary differs from new primary\n", r_ptr->rad_svrname);
				ret = dvk_migr_commit(PROC_NO_PID, r_ptr->rad_dcid, r_ptr->rad_ep, r_ptr->rad_primary_mbr);
			}else{ 
				USRDEBUG("%s: new primary is the same as old primary\n", r_ptr->rad_svrname);
				ret = dvk_migr_rollback(r_ptr->rad_dcid, r_ptr->rad_ep);
			}
			break;
		default:
			break;
	} 			
	
	r_ptr->rad_nr_nodes   	= sp_ptr->msg.m2_i1;
	r_ptr->rad_nr_init 		= sp_ptr->msg.m2_i2;
	r_ptr->rad_bm_nodes 	= sp_ptr->msg.m2_l1;
	r_ptr->rad_bm_init		= sp_ptr->msg.m2_l2;
	USRDEBUG("%s: " RAD1_FORMAT, r_ptr->rad_svrname, RAD1_FIELDS(r_ptr));
	USRDEBUG("%s: " RAD2_FORMAT, r_ptr->rad_svrname, RAD2_FIELDS(r_ptr));
	USRDEBUG("%s: " RAD3_FORMAT, r_ptr->rad_svrname, RAD3_FIELDS(r_ptr));
	
	return(OK);	
}

/*===========================================================================*
 *				get_random_node							     	*
 ===========================================================================*/
int get_random_node(radar_t	*r_ptr, unsigned int nodes)
{
	int ret, r, retries;
	
	retries  = MAX_RANDOM_RETRIES;
	
	USRDEBUG("nodes=%X \n", nodes);
	assert( nodes != 0);
	
	nodes &=  r_ptr->rad_bm_valid;
	if( nodes == 0) return(-1);
	
	do {
		r = (random()/(RAND_MAX/sizeof(nodes)));
		if ( TEST_BIT(nodes, r) != 0){
			USRDEBUG("random node=%d \n", r);
			return(r);
		}
	} while( --retries != 0);
	USRDEBUG("exhausted attempts");
	
	return(get_first_mbr(nodes));	
}


/*===========================================================================*
 *				no_primary_dead								     	*
 ===========================================================================*/
int no_primary_dead(radar_t	*r_ptr)
{
	int ret;
	
	USRDEBUG("%s BEFORE: rad_primary_mbr=%d  rad_primary_old=%d \n", 
		r_ptr->rad_svrname, r_ptr->rad_primary_mbr, r_ptr->rad_primary_old );
#ifdef RADAR 
	ret = dvk_migr_start(r_ptr->rad_dcid, r_ptr->rad_ep);
#endif // RADAR 
	
	r_ptr->rad_primary_old = r_ptr->rad_primary_mbr;
	r_ptr->rad_primary_mbr = NO_PRIMARY_DEAD;
	
	r_ptr->rad_bm_init = 0;
	r_ptr->rad_nr_init = 0;
	r_ptr->rad_bm_nodes = 0;
	r_ptr->rad_nr_nodes = 0;	
	
	USRDEBUG("%s ARTER: rad_primary_mbr=%d  rad_primary_old=%d \n", 
		r_ptr->rad_svrname, r_ptr->rad_primary_mbr, r_ptr->rad_primary_old );
	return(OK);	
}

/*===========================================================================*
 *				no_primary_net								     	*
 ===========================================================================*/
int no_primary_net(radar_t	*r_ptr)
{
	int ret;

	USRDEBUG("%s BEFORE: rad_primary_mbr=%d  rad_primary_old=%d \n", 
		r_ptr->rad_svrname, r_ptr->rad_primary_mbr, r_ptr->rad_primary_old );
#ifdef RADAR 
	ret = dvk_migr_start(r_ptr->rad_dcid, r_ptr->rad_ep);
#endif // RADAR 
	r_ptr->rad_primary_old = r_ptr->rad_primary_mbr;
	r_ptr->rad_primary_mbr = NO_PRIMARY_NET;
	r_ptr->rad_bm_init = 0;
	r_ptr->rad_nr_init = 0;
	r_ptr->rad_bm_nodes = 0;
	r_ptr->rad_nr_nodes = 0;
	
	USRDEBUG("%s ARTER: rad_primary_mbr=%d  rad_primary_old=%d \n", 
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
	nid = atoi( (int *) n_ptr);
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