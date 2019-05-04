/****************************************************************/
/****************************************************************/
/* 				REPLICATE							*/
/* REPLICATE algorithm routines for intra-nodes RDISKs 			*/
/****************************************************************/
#define _MULTI_THREADED
#define _GNU_SOURCE     
#define  MOL_USERSPACE	1
#define TASKDBG		1

#include "rdisk.h"
#include "const.h"
#include "glo.h"

#define TRUE 1
extern struct driver *m_dtab;

SP_message sp_ptr1; /*message to m_transfer*/	

SP_message *sp_ptr;
message *msg_ptr;
unsigned *data_ptr, slave_ac, sync_pr, master_ac; /*slave active_flag; sync proccesing; master active_flag*/

/*===========================================================================*
 *				init_replicate				     
 * It connects REPLICATE thread to the SPREAD daemon and initilize several local 
 * and replicated  variables
 *===========================================================================*/
int init_replicate(void)
{
	int rcode;
#ifdef SPREAD_VERSION
    int     mver, miver, pver;
#endif
    sp_time test_timeout;

    test_timeout.sec = RDISK_TIMEOUT_SEC;
    test_timeout.usec = RDISK_TIMEOUT_MSEC;


#ifdef SPREAD_VERSION
    rcode = SP_version( &mver, &miver, &pver);
	if(!rcode)
        {
		SP_error (rcode);
	  	SYSERR(rcode);
	  	ERROR_EXIT(rcode);
	}
	TASKDEBUG("Spread library version is %d.%d.%d\n", mver, miver, pver);
#else
    TASKDEBUG("Spread library version is %1.2f\n", SP_version() );
#endif
	/*------------------------------------------------------------------------------------
	* User:  it must be unique in the spread node.
	*  RDISKlocal_nodeid.dcid
	*--------------------------------------------------------------------------------------*/
	sprintf( rdisk_group, "RDISK%02d", dc_ptr->dc_dcid);
	TASKDEBUG("rdisk_group=%s\n", rdisk_group);
	sprintf( Spread_name, "4803");
	sprintf( User, "RDISK%02d.%02d", dc_ptr->dc_dcid,local_nodeid);
	TASKDEBUG("User=%s\n", User);

	rcode = SP_connect_timeout( Spread_name, User , 0, 1, 
				&sysmbox, Private_group, test_timeout );
	if( rcode != ACCEPT_SESSION ) 	{
		SP_error (rcode);
		ERROR_EXIT(rcode);
	}
	TASKDEBUG("User %s: connected to %s with private group %s\n",
			User , Spread_name, Private_group);

	/*------------------------------------------------------------------------------------
	* Group name: RDISK<dcid>
	*--------------------------------------------------------------------------------------*/
	rcode = SP_join( sysmbox, rdisk_group);
	if( rcode){
		SP_error (rcode);
 		ERROR_EXIT(rcode);
	}

	synchronized = FALSE;
	TASKDEBUG("synchronized=%d\n", synchronized);
	FSM_state 	= STS_NEW;	
	primary_mbr = NO_PRIMARY;
	
	bm_nodes 	= 0;			/* initialize  conected  members'  bitmap */
	bm_sync   	= 0;			/* initialize  synchoronized  members'  bitmap */
	nr_nodes 	= 0;
	nr_sync  	= 0;
	
	return(OK);
}

/*===========================================================================*
 *				replicate_main									     *
 *===========================================================================*/

void *replicate_main(void *arg)
{
	static 	char source[MAX_GROUP_NAME];
	int rcode, mtype, i;

	TASKDEBUG("replicate_main dcid=%d local_nodeid=%d\n"
		,dcid, local_nodeid);
		
	for ( i = 0; i < NR_DEVS; i++)
		sumdevvec[i].nr_updated = 0;	

	while(TRUE){
		rcode = replica_loop(&mtype, source);
	}
}

/*===========================================================================*
 *				replica_loop				    
 * return : service_type
 *===========================================================================*/

int replica_loop(int *mtype, char *source)
{
	char		 sender[MAX_GROUP_NAME];
    char		 target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
   	int		 num_groups;

	int		 service_type;
	int16	 mess_type;
	int		 endian_mismatch;
	int		 i,j;
	int		 ret, mbr;
	int 	 rcode;
	// message  *sp_ptr;
	

	service_type = 0;
	num_groups = -1;
	
	TASKDEBUG("SPREAD Listening ...\n");  
	ret = SP_receive( sysmbox, &service_type, sender, 100, &num_groups, target_groups,
			&mess_type, &endian_mismatch, sizeof(mess_in), mess_in );
	
	
	if( ret < 0 ){
       	if ( (ret == GROUPS_TOO_SHORT) || (ret == BUFFER_TOO_SHORT) ) {
			service_type = DROP_RECV;
            TASKDEBUG("\n========Buffers or Groups too Short=======\n");
            ret = SP_receive( sysmbox, &service_type, sender, 
					MAX_MEMBERS, &num_groups, target_groups,
					&mess_type, &endian_mismatch, sizeof(mess_in), mess_in );
		}
	}

	if (ret < 0 ) {
		SP_error( ret );
		ERROR_EXIT(ret);
	}

	TASKDEBUG("sender=%s Private_group=%s dc_name=%s service_type=%d\n"
			,sender, Private_group, dc_ptr->dc_name, service_type);

	// sp_ptr = (message *) mess_in;
	sp_ptr = (SP_message *) mess_in;
	msg_ptr = (message *) mess_in;
		
	// pthread_mutex_lock(&rd_mutex);	/* protect global variables */
	MTX_LOCK(bk_mutex); //MARIE
		
	if( Is_regular_mess( service_type ) )	{
		mess_in[ret] = 0;
		if     ( Is_unreliable_mess( service_type ) ) {TASKDEBUG("received UNRELIABLE \n ");}
		else if( Is_reliable_mess(   service_type ) ) {TASKDEBUG("received RELIABLE \n");}
		else if( Is_causal_mess(       service_type ) ) {TASKDEBUG("received CAUSAL \n");}
		else if( Is_agreed_mess(       service_type ) ) {TASKDEBUG("received AGREED \n");}
		else if( Is_safe_mess(   service_type ) || Is_fifo_mess(       service_type ) ) {
			TASKDEBUG("message from %s, of type %d, (endian %d) to %d groups (%d bytes)\n",
				sender, mess_type, endian_mismatch, num_groups, ret);

			/*----------------------------------------------------------------------------------------------------
			*   DEV_WRITE		The PRIMARY has sent a WRITE request 
			*----------------------------------------------------------------------------------------------------*/
			if( mess_type == DEV_WRITE ) {
				TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(msg_ptr));
				ret = handle_dev_write(sp_ptr);
				*mtype = mess_type;
			/*----------------------------------------------------------------------------------------------------
			*   DEV_SCATTER		The PRIMARY has sent a DEV_SCATTER request 
			*----------------------------------------------------------------------------------------------------*/
			}else if( mess_type == DEV_SCATTER ) {
				TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(msg_ptr));
				// ret = rep_dev_scatter(sp_ptr);
				ret = handle_dev_write(sp_ptr); /*como rdisk hace un msg spread x cada vcopy, siempre hago un write*/
				*mtype = mess_type;;
			/*----------------------------------------------------------------------------------------------------
			*   A BACKUP member has sent a reply to the PRIMARY
			*----------------------------------------------------------------------------------------------------*/
			}else if ( mess_type == MOLTASK_REPLY ) {
				TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(msg_ptr));
				ret = handle_bkup_reply(sp_ptr);
				*mtype = mess_type;
				TASKDEBUG("mess_type=%d\n", mess_type); 
			/*----------------------------------------------------------------------------------------------------
			*   DEV_OPEN		The PRIMARY has sent DEV_OPEN request 
			*----------------------------------------------------------------------------------------------------*/
			}else if ( mess_type == DEV_OPEN ) {
				TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(msg_ptr));
				ret = handle_dev_open(sp_ptr);
				*mtype = mess_type;
						
			/*----------------------------------------------------------------------------------------------------
			*   DEV_CLOSE		The PRIMARY has sent DEV_CLOSE request 
			*----------------------------------------------------------------------------------------------------*/
			}else if ( mess_type == DEV_CLOSE ) {
				TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(msg_ptr));
				ret = handle_dev_close(sp_ptr);
				*mtype = mess_type;
			/*----------------------------------------------------------------------------------------------------
			*   DEV_IOCTL		The PRIMARY has sent DEV_IOCTL request 
			*----------------------------------------------------------------------------------------------------*/
			}else if ( mess_type == DEV_IOCTL ) {
				TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(msg_ptr));
				ret = handle_dev_ioctl(sp_ptr);
				*mtype = mess_type;
			/*----------------------------------------------------------------------------------------------------
			*   CANCEL		The PRIMARY has sent CANCEL request 
			*----------------------------------------------------------------------------------------------------*/
			}else if ( mess_type == CANCEL ) {
				TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(msg_ptr));
				ret = handle_cancel(sp_ptr);
				*mtype = mess_type;
			/*----------------------------------------------------------------------------------------------------
			*   SELECT		The PRIMARY has sent SELECT request 
			*----------------------------------------------------------------------------------------------------*/
			}else if ( mess_type == CANCEL ) {
				TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(msg_ptr));
				ret = handle_select(sp_ptr);
				*mtype = mess_type;
			/*----------------------------------------------------------------------------------------------------
			*   MC_STATUS_INFO		The PRIMARY has sent MC_STATUS_INFO message 
			*----------------------------------------------------------------------------------------------------*/
			}else if ( mess_type == MC_STATUS_INFO ) {
				TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(msg_ptr));
				ret = handle_status_info(sp_ptr);
				*mtype = mess_type;
			/*----------------------------------------------------------------------------------------------------
			*   MC_SYNCHRONIZED		The new member inform that it is SYNCHRONIZED
			*----------------------------------------------------------------------------------------------------*/
			}else if ( mess_type == MC_SYNCHRONIZED ) {
				TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(msg_ptr));
				ret = handle_synchronized(sp_ptr);
				*mtype = mess_type;
			}else if ( mess_type == MC_RADAR_INFO ) {
				TASKDEBUG("RADAR Message, ignored..\n");
				*mtype = mess_type;
			} else {
				TASKDEBUG("Unknown message type %X\n", mess_type);
				TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(msg_ptr));
				*mtype = mess_type;
				ret = OK;
			}
		}
	}else if( Is_membership_mess( service_type ) )	{
        ret = SP_get_memb_info( mess_in, service_type, &memb_info );
        if (ret < 0) {
			TASKDEBUG("BUG: membership message does not have valid body\n");
           	SP_error( ret );
         	ERROR_EXIT(ret);
        }

		if  ( Is_reg_memb_mess( service_type ) ) {
			TASKDEBUG("Received REGULAR membership for group %s with %d members, where I am member %d:\n",
				sender, num_groups, mess_type );

			if( Is_caused_join_mess( service_type ) ||
				Is_caused_leave_mess( service_type ) ||
				Is_caused_disconnect_mess( service_type ) ){
				sp_nr_mbrs = num_groups;
				memcpy((void*) sp_members, (void *) target_groups, sp_nr_mbrs*MAX_GROUP_NAME);
				nr_nodes = 0;
				for( i=0; i < sp_nr_mbrs; i++ ){
					TASKDEBUG("\t%s\n", &sp_members[i][0]);
//					TASKDEBUG("grp id is %d %d %d\n",memb_info.gid.id[0], memb_info.gid.id[1], memb_info.gid.id[2] );
					if ( strncmp(&sp_members[i][0], "#RDISK",6) == 0) { // RDISK member 		
						SET_BIT(bm_nodes, get_nodeid("RDISK",&sp_members[i][0]));
						nr_nodes++;
					} 
				}
				TASKDEBUG("bm_nodes=%X nr_nodes=%d\n", bm_nodes, nr_nodes);
			}
		}

		if( Is_caused_join_mess( service_type ) )	{
			/*----------------------------------------------------------------------------------------------------
			*   JOIN: The group has a new member
			*----------------------------------------------------------------------------------------------------*/
			TASKDEBUG("Due to the JOIN of %s service_type=%d\n", 
				memb_info.changed_member, service_type );
			if ( strncmp(memb_info.changed_member, "#RDISK",6) == 0) { // RDISK member 		
				mbr = get_nodeid("RDISK", (char *)  memb_info.changed_member);
				nr_nodes = num_groups - nr_radar;
				TASKDEBUG("JOIN - nr_nodes=%d\n", nr_nodes); 
				ret = sp_join(mbr);
				TASKDEBUG("JOIN end - nr_nodes=%d\n", nr_nodes); 
			}
			if( local_nodeid == primary_mbr)
				mcast_radar_info();
		}else if( Is_caused_leave_mess( service_type ) 
			||  Is_caused_disconnect_mess( service_type ) ){
			/*----------------------------------------------------------------------------------------------------
			*   LEAVE or DISCONNECT:  A member has left the group
			*----------------------------------------------------------------------------------------------------*/
			TASKDEBUG("Due to the LEAVE or DISCONNECT of %s\n", 
				memb_info.changed_member );
			if ( strncmp(memb_info.changed_member, "#RDISK",6) == 0) { // RDISK member 		
				mbr = get_nodeid("RDISK", (char *)  memb_info.changed_member);
				nr_nodes = num_groups - nr_radar;
				TASKDEBUG("LEAVE - nr_nodes=%d\n", nr_nodes); 
				ret = sp_disconnect(mbr);
				TASKDEBUG("LEAVE end - nr_nodes=%d\n", nr_nodes); 
				if( local_nodeid == primary_mbr)
					mcast_radar_info();
			}
		}else if( Is_caused_network_mess( service_type ) ){
			/*----------------------------------------------------------------------------------------------------
			*   NETWORK CHANGE:  A network partition or a dead deamon
			*----------------------------------------------------------------------------------------------------*/
			TASKDEBUG("Due to NETWORK change with %u VS sets\n", memb_info.num_vs_sets);
            		num_vs_sets = SP_get_vs_sets_info( mess_in, 
									&vssets[0], MAX_VSSETS, &my_vsset_index );
            if (num_vs_sets < 0) {
				TASKDEBUG("BUG: membership message has more then %d vs sets. Recompile with larger MAX_VSSETS\n", MAX_VSSETS);
				SP_error( num_vs_sets );
               	ERROR_EXIT( num_vs_sets );
			}
            if (num_vs_sets == 0) {
				TASKDEBUG("BUG: membership message has %d vs_sets\n", 
					num_vs_sets);
				SP_error( num_vs_sets );
               	ERROR_EXIT( EDVSGENERIC );
			}

			bm_nodes = 0;
			nr_nodes = 0;
            for( i = 0; i < num_vs_sets; i++ )  {
				TASKDEBUG("%s VS set %d has %u members:\n",
					(i  == my_vsset_index) ?("LOCAL") : ("OTHER"), 
						i, vssets[i].num_members );
               	ret = SP_get_vs_set_members(mess_in, &vssets[i], members, MAX_MEMBERS);
               	if (ret < 0) {
					TASKDEBUG("VS Set has more then %d members. Recompile with larger MAX_MEMBERS\n", MAX_MEMBERS);
					SP_error( ret );
                   	ERROR_EXIT( ret);
              	}

				/*---------------------------------------------
				* get the bitmap of current members
				--------------------------------------------- */
				for( j = 0; j < vssets[i].num_members; j++ ) {
					TASKDEBUG("\t%s\n", members[j] );
					if ( strncmp(members[j], "#RDISK",6) == 0) {			
						mbr = get_nodeid("RDISK", members[j]);
						if(!TEST_BIT(bm_nodes, mbr)) {
							SET_BIT(bm_nodes, mbr);
							nr_nodes++;
						}
					}
				}
				TASKDEBUG("old bm_sync=%X bm_nodes=%X primary_mbr=%d nr_radar=%d bm_radar=%X\n",
					bm_sync, bm_nodes, primary_mbr, nr_radar, bm_radar);
			}
			if( bm_sync > bm_nodes) {		/* a NETWORK PARTITION has occurred 	*/
				sp_net_partition();
			}else{
				if (bm_sync < bm_nodes) {	/* a NETWORK MERGE has occurred 		*/
					sp_net_merge();
				}else{
					TASKDEBUG("NETWORK CHANGE with no changed members (may be RADAR member) !! ");
				}
			}
			if( local_nodeid == primary_mbr)
				mcast_radar_info();
		}else if( Is_transition_mess(   service_type ) ) {
			TASKDEBUG("received TRANSITIONAL membership for group %s\n", sender );
			if( Is_caused_leave_mess( service_type ) ){
				TASKDEBUG("received membership message that left group %s\n", sender );
			}else {
				TASKDEBUG("received incorrecty membership message of type 0x%x\n", service_type );
			}
		} else if ( Is_reject_mess( service_type ) )      {
			TASKDEBUG("REJECTED message from %s, of servicetype 0x%x messtype %d, (endian %d) to %d groups \n(%d bytes): %s\n",
				sender, service_type, mess_type, endian_mismatch, num_groups, ret, mess_in );
		}else {
			TASKDEBUG("received message of unknown message type 0x%x with ret %d\n", service_type, ret);
		}
	}
	// pthread_mutex_unlock(&rd_mutex);
	MTX_UNLOCK(bk_mutex); 
	
	if(ret < 0) ERROR_RETURN(ret);
	return(ret);
}

/***************************************************************************/
/* FUNCTIONS TO DEAL WITH SPREAD MESSAGES - MEMBERSHIP			*/
/***************************************************************************/

/*===========================================================================*
 *				sp_join														*
 * A NEW member has joint the DC group							*
 *===========================================================================*/
int sp_join( int new_mbr)
{
	int rcode; 
	TASKDEBUG("new_member=%d primary_mbr=%d nr_nodes=%d\n", 
		new_mbr, primary_mbr, nr_nodes);
	if( nr_nodes < 0 || nr_nodes >= dc_ptr->dc_nr_nodes){
		TASKDEBUG("nr_nodes=%d dc_ptr->dc_nr_nodes=%d\n", nr_nodes, dc_ptr->dc_nr_nodes);
		ERROR_EXIT(EDVSINVAL);
		}
		
	if( TEST_BIT(bm_radar, new_mbr) != 0){
		TASKDEBUG("new_mbr=%d is in the same node as RADAR (bm_radar=%X)\n", new_mbr, bm_radar);
		ERROR_EXIT(EDVSBADNODEID);
	}
		
	SET_BIT(bm_nodes, new_mbr);
	
	primary_mbr = get_primary_mbr();
	TASKDEBUG("new_member=%d primary_mbr=%d nr_nodes=%d\n", 
		new_mbr, primary_mbr, nr_nodes);

	if( new_mbr == local_nodeid){		/*  My own JOIN message	 */
		if( primary_mbr == NO_PRIMARY){ /* I am a LONELY member  */
			FSM_state 	= STS_SYNCHRONIZED;
			synchronized = TRUE;
			TASKDEBUG("synchronized=%d'n", synchronized);		
			if ( ! replica_updated(local_nodeid)) {
				TASKDEBUG("I am a BACKUP member: start the primary FIRST\n");
				rcode = EDVSPRIMARY;
				SP_error(rcode);
				ERROR_EXIT(rcode);
			}
			SET_BIT(bm_sync, local_nodeid);
			nr_sync = 1;
			TASKDEBUG("nr_sync=%d\n", nr_sync);
			primary_mbr =  local_nodeid;
			TASKDEBUG("PRIMARY_MBR=%d\n", primary_mbr);
			TASKDEBUG("Wake up rdisk: new_mbr=%d\n", new_mbr);
			COND_SIGNAL(rd_barrier);
			return(OK);
		}else{  /*SLAVE*/								
			if(update_opt == UPDATE_NO) { // WITHOUT UPDATE 
				FSM_state 	= STS_SYNCHRONIZED;
				synchronized = TRUE;
				TASKDEBUG("synchronized=%d'n", synchronized);
				SET_BIT(bm_sync, local_nodeid);
				nr_sync++;
				TASKDEBUG("primary_mbr=%d nr_sync=%d bm_sync=%X\n", primary_mbr, nr_sync, bm_sync);
				TASKDEBUG("Starting RDISK as a BACKUP\n");	
				rcode = dvk_unbind(dc_ptr->dc_dcid, rd_ep);
				if(rcode < 0 ) ERROR_PRINT(rcode);
				rcode = dvk_bkupbind(dc_ptr->dc_dcid, rd_lpid, rd_ep, primary_mbr);
				if(rcode != rd_ep ) ERROR_EXIT(rcode);
				COND_SIGNAL(rd_barrier);
			} else {						// WITH UPDATE 
							
				TASKDEBUG("Initializing SLAVE COPY THREAD\n");

				slave_ac = TRUE;
				sync_pr = TRUE;
				TASKDEBUG("slave_ac=%d sync_pr=%d\n", slave_ac, sync_pr);
				TASKDEBUG("dynamic_opt%d\n", dynamic_opt);
							
				rcode = init_slavecopy();	
				if(rcode){
					slave_ac = FALSE;
					sync_pr = FALSE;
					TASKDEBUG("slave_ac=%d sync_pr=%d\n", slave_ac, sync_pr);
					ERROR_EXIT(rcode);
				}

				TASKDEBUG("Starting SLAVE COPY\n");
				rcode = pthread_create( &slavecopy_thread, NULL, slavecopy_main, 0 );
				if(rcode){
					slave_ac = FALSE;
					sync_pr = FALSE;
					TASKDEBUG("slave_ac=%d sync_pr=%d\n", slave_ac, sync_pr);
					ERROR_EXIT(rcode);
				}
				
				TASKDEBUG("dynamic_opt:%d - DO_DYNUPDATES:%d\n", dynamic_opt, DO_DYNUPDATES);
				if ( dynamic_opt == DO_DYNUPDATES ){ 
					TASKDEBUG("dynamic_opt:%d - DO_DYNUPDATES:%d\n", dynamic_opt, DO_DYNUPDATES);
					COND_WAIT(bk_barrier, bk_mutex); //MARIE
				}

				/* Wait for the thread */
				rcode = pthread_join(slavecopy_thread, NULL);
				TASKDEBUG("rcode - pthread_join-slave\n", rcode);
				
				slave_ac = FALSE;
				TASKDEBUG("slave_ac=%d\n", slave_ac);
				
				if( rcode){
					sync_pr = FALSE;
					/*check open device*/
					TASKDEBUG("Sync backpup error - chek open device\n");
					
					int i;
					for( i = 0; i < NR_DEVS; i++){
						TASKDEBUG("devvec[%d].available=%d\n", i, devvec[i].available);
						if ( devvec[i].active_flag == 0 ){ /*device open in primary*/ 
							TASKDEBUG("devvec[%d].active_flag=%d\n", i, devvec[i].active_flag);
						}
					}	
					ERROR_EXIT(rcode);
				}

				/*MARIE_VER: en slave fijo STS_LEAVE si hubo un error*/
				// TASKDEBUG("RDISK has been signaled by the REPLICATE thread  FSM_state=%d\n",  FSM_state);
				// if( FSM_state == STS_LEAVE) {	/* An error occurs trying to join the spread group */
					// pthread_mutex_unlock(&rd_mutex);
					// ERROR_RETURN(EDVSCONNREFUSED);
					// }	
				TASKDEBUG("End SLAVE OK\n");
					
				if ( FSM_state != STS_WAIT4PRIMARY){
					TASKDEBUG("Sync ERROR\n");
					ERROR_RETURN(EDVSCONNREFUSED);
				}	
			}
		}
	}else{ /* Other node JOINs the group	*/
		if (primary_mbr == local_nodeid){ 	/* I am the first init member 	*/		
			if(update_opt != UPDATE_NO) {
				TASKDEBUG("Initializing MASTER COPY THREAD\n");
				master_ac = TRUE;
				sync_pr = TRUE;
				TASKDEBUG("master_ac=%d sync_pr=%d\n", master_ac, sync_pr);
				TASKDEBUG("dynamic_opt%d\n", dynamic_opt);
				
				sc_node = new_mbr;
				rcode = init_mastercopy();	
				if( rcode){
					master_ac = FALSE;
					sync_pr = FALSE;
					MTX_UNLOCK(rd_mutex);
					TASKDEBUG("master_ac=%d sync_pr=%d\n", master_ac, sync_pr);
					ERROR_EXIT(rcode);
				}
			
				TASKDEBUG("Starting MASTER COPY\n");
				TASKDEBUG("new_mbr %d, %d\n", &new_mbr, new_mbr);
				rcode = pthread_create( &mastercopy_thread, NULL, mastercopy_main, (void *) &new_mbr ); //PAP
				
				if( rcode){
					master_ac = FALSE;
					sync_pr = FALSE;
					TASKDEBUG("master_ac=%d sync_pr=%d\n", master_ac, sync_pr);
					ERROR_EXIT(rcode);
				}

				TASKDEBUG("dynamic_opt:%d - DO_DYNUPDATES:%d\n", dynamic_opt, DO_DYNUPDATES);
				if ( dynamic_opt == DO_DYNUPDATES ){ 
					TASKDEBUG("dynamic_opt:%d - DO_DYNUPDATES:%d\n", dynamic_opt, DO_DYNUPDATES);
					COND_WAIT(bk_barrier, bk_mutex); //MARIE
				}
				
				// rcode = pthread_join(mastercopy_thread, NULL);
				
				master_ac = FALSE;
				TASKDEBUG("master_ac=%d\n", master_ac);
				TASKDEBUG("rcode - pthread_join-master\n", rcode);
				if( rcode){
					master_ac = FALSE;
					sync_pr = FALSE;
					TASKDEBUG("master_ac=%d sync_pr=%d\n", master_ac, sync_pr);
					ERROR_EXIT(rcode);
				}
				TASKDEBUG("End MASTER OK\n");
			} else {				// WITHOUT UPDATE 
				rcode = mc_status_info();
				if( rcode < 0) ERROR_RETURN(rcode);
			}
		}
	}
	return(OK);
}

/*===========================================================================*
 *				sp_disconnect				*
 * A system task process has leave or disconnect from the DC  group	*
 *===========================================================================*/
int sp_disconnect(int  disc_mbr)
{
	int i, rcode;
	
	TASKDEBUG("disc_mbr=%d\n",disc_mbr);

	CLR_BIT(bm_nodes, disc_mbr);
	TASKDEBUG("CLR_BIT bm_nodes=%d, disc_mbr=%d\n",bm_nodes, disc_mbr);
	
	if(local_nodeid == disc_mbr) {
		FSM_state = STS_DISCONNECTED;
		CLR_BIT(bm_sync, disc_mbr);
		return(EDVSNOTCONN);
	}

	/* if local_nodeid is not synchronized and is the only surviving node , restart all*/
	if(synchronized == FALSE) {
		FSM_state = STS_NEW;
		bm_nodes = 0;
		bm_sync=0;
		nr_nodes=0;
		nr_sync=0;
		TASKDEBUG("nr_sync=%d\n", nr_sync);		
		return(sp_join(local_nodeid));
	}

	TASKDEBUG("bm_sync=%d, disc_mbr=%d, nr_sync=%d\n",bm_sync, disc_mbr, nr_sync);
	/* if the dead node was synchronized */
	if( TEST_BIT(bm_sync, disc_mbr)) {
		nr_sync--;	/* decrease the number of synchronized nodes */
		TASKDEBUG("nr_sync=%d\n", nr_sync);
		CLR_BIT(bm_sync, disc_mbr);
	}
	TASKDEBUG("primary_mbr=%d, disc_mbr=%d\n",primary_mbr, disc_mbr);
	TASKDEBUG("bm_sync=%d, disc_mbr=%d, nr_sync=%d\n",bm_sync, disc_mbr, nr_sync);

	
	/* if the dead node was the primary_mbr, search another primary_mbr */
	if( primary_mbr == disc_mbr) {
		TASKDEBUG("PRIMARY DEAD: primary_mbr=%d, disc_mbr=%d\n",primary_mbr, disc_mbr);
		if( bm_sync == 0) {
			TASKDEBUG("THERE IS NO SYNCHRONIZED REPLICAS\n");
			FSM_state = STS_NEW;
			return(sp_join(local_nodeid));
		} else { 
			primary_mbr = get_primary_mbr();
			TASKDEBUG("PRIMARY_MBR=%d\n", primary_mbr);
			if( primary_mbr == local_nodeid){ /* I am the new primary */
				TASKDEBUG("Wake up rdisk: local_nodeid=%d\n", local_nodeid);
				rcode = mc_status_info();
				if( rcode < 0) ERROR_RETURN(rcode);				
				COND_SIGNAL(rd_barrier);
			}
			rcode = dvk_migr_start(dc_ptr->dc_dcid, rd_ep);
			TASKDEBUG("dvk_migr_start rcode=%d\n",	rcode);
			rcode = dvk_migr_commit(rd_lpid, dc_ptr->dc_dcid, rd_ep, primary_mbr);
			TASKDEBUG("dvk_migr_commit rcode=%d\n",	rcode);			
			TASKDEBUG("primary_mbr=%d - local_nodeid=%d\n", primary_mbr, local_nodeid)
		}	
	}
	
	TASKDEBUG("disc_mbr=%d nr_nodes=%d\n",	disc_mbr, nr_nodes);
	TASKDEBUG("primary_mbr=%d nr_sync=%d\n", primary_mbr, nr_sync);
	TASKDEBUG("bm_nodes=%X bm_sync=%X\n", bm_nodes,bm_sync);
	
	return(OK);
}

/*===========================================================================*
 *				get_sync_nodes				     
 * get the count of sync members from bitmap					     
 *===========================================================================*/
int get_sync_nodes(void)
{
	int init_nr, i;

	init_nr = 0;
	for( i=0; i < dc_ptr->dc_nr_nodes; i++ ) {
		if( TEST_BIT(bm_sync, i)){
			init_nr++;
		}
	}
	
	TASKDEBUG("init_nr=%d\n", init_nr);
	return(init_nr);
}

/*----------------------------------------------------------------------------------------------------
*				sp_net_partition
*----------------------------------------------------------------------------------------------------*/
int sp_net_partition(void)
{
	TASKDEBUG("\n");
	return(OK);
}

/*----------------------------------------------------------------------------------------------------
*				sp_net_merge
*----------------------------------------------------------------------------------------------------*/
int sp_net_merge(void)
{
	TASKDEBUG("\n");
	return(OK);
}

/***************************************************************************/
/* FUNCTIONS TO DEAL WITH DEVICE REPLICATION  MESSAGES 			*/
/***************************************************************************/
/*===========================================================================*
*				handle_dev_write								     			 *
 ============================================================================*/
/*for SCATTER | WRITE */
 int handle_dev_write(SP_message *sp_ptr){

	int rcode;
	unsigned int sp_nrblock;
	message msg;
	unsigned bytes;
	
	msg_ptr = (message *) sp_ptr; /*minix message*/
		
	TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(msg_ptr));
	
	TASKDEBUG("synchronized(%d) - TRUE(%d)\n", synchronized, TRUE); 
	
	if (( synchronized != TRUE) || ( sync_pr == 1 ) || ( master_ac == 1) || (slave_ac == 1) ){
		TASKDEBUG("synchronized(%d) - TRUE(%d) - sync_pr(%d) - master_ac(%d) - slave_ac(%d)\n", synchronized, TRUE, sync_pr, master_ac, slave_ac); 
	
		if (( slave_ac == TRUE) || ( sync_pr == TRUE)) {
			
			if ((( r_type == DEV_CFULL ) || ( r_type == DEV_CMD5)) && ( dynamic_opt == DO_DYNUPDATES)) { /*sync: only updates*/
				TASKDEBUG("DEV_UFULL=%d DEV_UMD5=%d\n", DEV_UFULL, DEV_UMD5);
				if( primary_mbr == local_nodeid){
					TASKDEBUG("Number op_transf: %d\n", msg_ptr->m2_l2);					
									
					if (msg_ptr->m2_l2 == 0) {
							
						TASKDEBUG("multicast DEV_WRITE REPLY to %d\n", primary_mbr);
					
						msg.m_source= local_nodeid;			
						msg.m_type 	= MOLTASK_REPLY;
						rcode = SP_multicast (sysmbox, SAFE_MESS, (char *) rdisk_group,  
								MOLTASK_REPLY, sizeof(message), (char *) &msg); 
			
						if(rcode <0) ERROR_RETURN(rcode);
						CLR_BIT(bm_acks, primary_mbr);
						if(rcode < 0){
							ERROR_RETURN(rcode);
						}
				
					COND_SIGNAL(update_barrier);	
					return(OK);
						
					}
				}else{
					TASKDEBUG("BACKUP sync\n");					
					
					TASKDEBUG("Transfer block: %u\n", r_trblksize);
				
					/*sp_nrblock: spread number block = position received / transfer block size (calculated in slavecopy.c)*/
					sp_nrblock = (msg_ptr->POSITION / r_trblksize); 
				
					TASKDEBUG("sp_nrblock: %d\n", sp_nrblock);
				
					if ( sp_nrblock < s_nrblock ){ /*updates: device in sync*/
						MTX_LOCK(write_mutex);
										
						TASKDEBUG("sp_ptr->buf.flag_buff =%d\n", sp_ptr->buf.flag_buff);

						if ( sp_ptr->buf.flag_buff == COMP ){
							TASKDEBUG("DECOMPRESS DATA\n");

							lz4_data_cd(sp_ptr->buf.buffer_data, msg_ptr->COUNT, sp_ptr->buf.flag_buff);
						
							sp_ptr->buf.flag_buff = msg_lz4cd.buf.flag_buff;
							TASKDEBUG("sp_msg.buf.flag_buff =%d\n", sp_ptr->buf.flag_buff);
							sp_ptr->buf.buffer_size = msg_lz4cd.buf.buffer_size;
							TASKDEBUG("sp_msg.buf.buffer_size =%d\n", sp_ptr->buf.buffer_size);

							memcpy(sp_ptr->buf.buffer_data, msg_lz4cd.buf.buffer_data, sp_ptr->buf.buffer_size);
							TASKDEBUG("sp_msg.buf.buffer_data =%s\n", sp_ptr->buf.buffer_data);
									
							TASKDEBUG("msg_ptr->COUNT(%d) == sp_ptr->buf.buffer_size (%d)\n",msg_ptr->COUNT,sp_ptr->buf.buffer_size);
							if ( msg_ptr->COUNT == sp_ptr->buf.buffer_size) {
								TASKDEBUG("BYTES CLIENT = BYTES DECOMPRESS\n");
						
								if ( (pwrite(devvec[msg_ptr->DEVICE].img_fd, sp_ptr->buf.buffer_data, sp_ptr->buf.buffer_size, msg_ptr->POSITION)) < 0 ){ 
									rcode = errno;
									MTX_UNLOCK(write_mutex);
									return(rcode);
									}		
									
								sumdevvec[msg_ptr->DEVICE].nr_updated++;	
								TASKDEBUG("dev: %d, nr_update: %u\n", msg_ptr->DEVICE, sumdevvec[msg_ptr->DEVICE].nr_updated);
							} 
							else{
								TASKDEBUG("ERROR. Bytes decompress not equal Bytes original\n");
								MTX_UNLOCK(write_mutex);
								ERROR_EXIT( EDVSPACKSIZE );
							}	
												
						}
						else{		
							TASKDEBUG("DATA BUFFER UNCOMPRESS\n");
		
							if ( (pwrite(devvec[msg_ptr->DEVICE].img_fd, sp_ptr->buf.buffer_data, msg_ptr->COUNT, msg_ptr->POSITION)) < 0 ){
								rcode = errno;
								TASKDEBUG("devvec[msg_ptr->DEVICE].img_fd= %d, sp_ptr->buf.buffer_data= %s, msg_ptr->COUNT= %u, msg_ptr->POSITION=%X\n",
										devvec[msg_ptr->DEVICE].img_fd, 
										sp_ptr->buf.buffer_data, 
										msg_ptr->COUNT, 
										msg_ptr->POSITION);
								TASKDEBUG("rcode: %d\n", rcode);
								MTX_UNLOCK(write_mutex);
								return(rcode);
								}			
											
							}
							sumdevvec[msg_ptr->DEVICE].nr_updated++;	
							TASKDEBUG("dev: %d, nr_update: %u\n", msg_ptr->DEVICE, sumdevvec[msg_ptr->DEVICE].nr_updated);
					
						}
					MTX_UNLOCK(write_mutex);	
					}	
					
			}else{
				TASKDEBUG("SERÍA POR LA COPIA FULL, VER???\n"); 
				return(OK);
				
			}
	
		}
	}
	/* End sync*/
	if (( synchronized == TRUE) && ( sync_pr == 0 ) && ( (master_ac == 0) || (slave_ac == 0) )){
		TASKDEBUG("synchronized(%d) - TRUE(%d) - sync_pr(%d) - master_ac(%d) - slave_ac(%d)\n", synchronized, TRUE, sync_pr, master_ac, slave_ac); 
		if ( msg_ptr->m_source != primary_mbr){
			TASKDEBUG("FAKE PRIMARY member=%d\n", msg_ptr->m_source);
			return(OK);
		}
		
		TASKDEBUG("bm_acks=%d, bm_sync=%d \n", bm_acks, bm_sync); 	
		bm_acks = bm_sync;
		CLR_BIT(bm_acks,primary_mbr);
		TASKDEBUG("CLR_BIT -bm_acks=%d\n", bm_acks); 	
		TASKDEBUG("primary_mbr=%d, local_nodeid= %d\n", primary_mbr, local_nodeid); 	
		
		if( primary_mbr == local_nodeid){
			TASKDEBUG("primary_mbr=%d, local_nodeid= %d\n", primary_mbr, local_nodeid); 	
			return(OK);
			}
		
		
		/* Carry out a single read or write request. */
		iovec_t iovec1;
		int r, opcode;
  
		TASKDEBUG("sp_ptr->COUNT=%u\n", msg_ptr->COUNT);
		/* Disk address?  Address and length of the user buffer? */
		if (msg_ptr->COUNT < 0) return(EINVAL);
		/*COUNT: cantidad de  bytes reales del mensaje, sin comprimir*/ 	
  
		
		TASKDEBUG("sp_ptr->IO_ENDPT=%d - sp_ptr->ADDRESS:%p - sp_ptr->COUNT=%u\n", 
			msg_ptr->IO_ENDPT, (vir_bytes) msg_ptr->ADDRESS, msg_ptr->COUNT);
  
		/* Prepare for I/O. */
		// if (m_prepare(msg_ptr->DEVICE) == NIL_DEV) return(ENXIO);
		// TASKDEBUG("sp_ptr->DEVICE=%d\n", msg_ptr->DEVICE);

		/* Create a one element scatter/gather vector for the buffer. */
		opcode = DEV_SCATTER; 
		iovec1.iov_addr = (vir_bytes) msg_ptr->ADDRESS; /*acá podría completar con los datos del puntero a los datos??*/
		iovec1.iov_size = msg_ptr->COUNT;	  

		TASKDEBUG("iovec1.iov_addr= %X\n", iovec1.iov_addr);
		TASKDEBUG("iovec1.iov_size= %d\n", iovec1.iov_size);
		
		TASKDEBUG("File descriptor image= %d\n", devvec[msg_ptr->DEVICE].img_fd);
		TASKDEBUG("(receive) msg_ptr->POSITION %X\n", msg_ptr->POSITION);	
		TASKDEBUG("sp_ptr->buf.flag_buff =%d\n", sp_ptr->buf.flag_buff);
		TASKDEBUG("buffer: %s\n", sp_ptr->buf.buffer_data);			
		
		/*DECOMPRESS DATA BUFFER*/
	
		if ( sp_ptr->buf.flag_buff == COMP ){
			TASKDEBUG("DECOMPRESS DATA\n");

			lz4_data_cd(sp_ptr->buf.buffer_data, msg_ptr->COUNT, sp_ptr->buf.flag_buff);
		
			sp_ptr->buf.flag_buff = msg_lz4cd.buf.flag_buff;
			TASKDEBUG("sp_msg.buf.flag_buff =%d\n", sp_ptr->buf.flag_buff);
			sp_ptr->buf.buffer_size = msg_lz4cd.buf.buffer_size;
			TASKDEBUG("sp_msg.buf.buffer_size =%d\n", sp_ptr->buf.buffer_size);

			memcpy(sp_ptr->buf.buffer_data, msg_lz4cd.buf.buffer_data, sp_ptr->buf.buffer_size);
			TASKDEBUG("sp_msg.buf.buffer_data =%s\n", sp_ptr->buf.buffer_data);
					
			TASKDEBUG("msg_ptr->COUNT(%d) == sp_ptr->buf.buffer_size (%d)\n",msg_ptr->COUNT,sp_ptr->buf.buffer_size);
			if ( msg_ptr->COUNT == sp_ptr->buf.buffer_size) {
				TASKDEBUG("BYTES CLIENT = BYTES DECOMPRESS\n");
			
				if ( (bytes=(pwrite(devvec[msg_ptr->DEVICE].img_fd, sp_ptr->buf.buffer_data, sp_ptr->buf.buffer_size, msg_ptr->POSITION))) < 0 ){ 
					rcode = errno;
					return(rcode);
					}		
					
			} 
			else{
				TASKDEBUG("ERROR. Bytes decompress not equal Bytes original\n");
				ERROR_EXIT( EDVSPACKSIZE );
			}	
		}
		else{		
			TASKDEBUG("DATA BUFFER UNCOMPRESS\n");
			TASKDEBUG("msg_ptr->DEVICE].img_fd=%d, sp_ptr->buf.buffer_data= %s, msg_ptr->COUNT= %d, msg_ptr->POSITION= %X\n", 
					devvec[msg_ptr->DEVICE].img_fd, 
					sp_ptr->buf.buffer_data, 
					msg_ptr->COUNT, 
					msg_ptr->POSITION);
										
			if ( devvec[msg_ptr->DEVICE].active_flag == 1 ){ /*device sync active_flag*/
				TASKDEBUG("devvec[msg_ptr->DEVICE].active_flag=%d\n", devvec[msg_ptr->DEVICE].active_flag);
				if ( (bytes=(pwrite(devvec[msg_ptr->DEVICE].img_fd, sp_ptr->buf.buffer_data, msg_ptr->COUNT, msg_ptr->POSITION))) < 0 ){
					TASKDEBUG("bytes write=% u\n", bytes);
					rcode = errno;
					return(rcode);
					}			
				}	
	
			}
		TASKDEBUG("buffer: %s\n", sp_ptr->buf.buffer_data);			
		TASKDEBUG("bytes write=%u\n", bytes);		
		nr_optrans= msg_ptr->m2_l2; /*number transfer operation x nr_req*/
		TASKDEBUG("nr_optrans=%d\n", nr_optrans);	
			
		/* Transfer bytes from/to the device. */
		r = m_transfer(msg_ptr->IO_ENDPT, opcode, msg_ptr->POSITION, &iovec1, 1);
		TASKDEBUG("m_transfer = (r) %d\n", r);

		/* Return the number of bytes transferred or an error code. */
		return(r == OK ? (msg_ptr->COUNT - iovec1.iov_size) : r);
		
		/*return(rcode);*/
			
	}else {
		 TASKDEBUG("synchronized(%d) - TRUE(%d) - sync_pr(%d) - master_ac(%d) - slave_ac(%d)\n", synchronized, TRUE, sync_pr, master_ac, slave_ac); 
		 return(OK);
	} 
}


/*===========================================================================*
 *				handle_bkup_reply								     	*
 ===========================================================================*/
int handle_bkup_reply( message *m_ptr)
{
	TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(m_ptr));
	
	TASKDEBUG("synchronized=%d\n", synchronized);
	
	if( synchronized != TRUE) return(OK);
	
	if( !TEST_BIT(bm_sync, m_ptr->m_source)){
		TASKDEBUG("FAKE backup member=%d (bm_sync=%X)\n", 
			m_ptr->m_source, bm_sync );
		return(OK);
	}

	CLR_BIT(bm_acks, m_ptr->m_source);
	TASKDEBUG("m_ptr->m_source=%d\n",m_ptr->m_source); 
	
  	if( primary_mbr == local_nodeid){
		if(bm_acks == 0) {	/* all ACKs received */
			TASKDEBUG("ALL ACKS received\n"); 
			// pthread_cond_signal(&update_barrier);	/* Wakeup RDISK 		*/
			COND_SIGNAL(update_barrier);
			}
	}
	TASKDEBUG("termina la respuesta\n"); 
	return(OK);
}

/*===========================================================================*
 *				handle_dev_open								     	*
 ===========================================================================*/
int handle_dev_open( message *m_ptr)
{
	int rcode;
	unsigned *localbuff;
	message msg;

	TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(m_ptr));

	TASKDEBUG("synchronized=%d, TRUE=%d\n", synchronized, TRUE);
	
	if( synchronized != TRUE) return(OK);
		
	if ( m_ptr->m_source != primary_mbr){
		TASKDEBUG("FAKE PRIMARY member=%d\n", m_ptr->m_source);
		return(OK);
	}

	bm_acks = bm_sync;
	TASKDEBUG("bm_acks=%d\n", bm_acks); 
	CLR_BIT(bm_acks,  primary_mbr);
	
	TASKDEBUG("bm_acks=%d\n", bm_acks); 
	
	if( primary_mbr == local_nodeid){
		TASKDEBUG("sync proccesing - multicast DEV_OPEN REPLY to %d rcode=%d\n", local_nodeid ,rcode);
		TASKDEBUG("master_ac=%d sync_pr=%d\n", master_ac, sync_pr);
		if (( master_ac == TRUE) || ( sync_pr == TRUE)) {
			TASKDEBUG("master_ac=%d sync_pr=%d\n", master_ac, sync_pr);
			msg.m_source= local_nodeid;	
				TASKDEBUG("msg.m_source= %d\n", msg.m_source);	
				msg.m_type 	= MOLTASK_REPLY;
				msg.m2_i1	= m_ptr->DEVICE;
				msg.m2_i2	= DEV_OPEN;
				msg.m2_i3	= rcode;
				rcode = SP_multicast (sysmbox, SAFE_MESS, (char *) rdisk_group,  
						MOLTASK_REPLY, sizeof(message), (char *) &msg); 
						
			if(rcode < 0)ERROR_RETURN(rcode);
			COND_SIGNAL(update_barrier);	
			return(OK);
		}else{
			return(OK);	
		}
	}
	rcode = m_do_open(m_dtab, m_ptr); // modificado pap antes era &m_dtab
	TASKDEBUG("rcode=%d\n");
	if(rcode < 0) ERROR_RETURN(rcode);
	return(rcode);
}

int handle_dev_close( message *sp_ptr){
		TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(sp_ptr));
		return(OK);
}
int handle_dev_ioctl( message *sp_ptr){
		TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(sp_ptr));
		return(OK);
}
int handle_cancel( message *sp_ptr){
		TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(sp_ptr));
		return(OK);
}
int handle_select( message *sp_ptr){
		TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(sp_ptr));
		return(OK);
}
/***************************************************************************/
/* FUNCTIONS TO DEAL WITH  REPLICATION PROTOCOL  MESSAGES 		*/
/***************************************************************************/

/*===========================================================================*
 *				handle_status_info								     	*
 * The PRIMARY member sent a multicast to no sync members					*
 ===========================================================================*/
int handle_status_info(	message  *sp_ptr)
{
	int rcode;
	
	TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(sp_ptr));

	if( FSM_state != STS_WAIT4PRIMARY) return (OK);
	
	primary_mbr = sp_ptr->m_source;
	TASKDEBUG("Primary_mbr=%d\n", primary_mbr);
	
	if (nr_nodes != sp_ptr->m2_i1){
		TASKDEBUG("Received nr_nodes=%d don't match local nr_nodes=%d\n"
			, sp_ptr->m2_i1, nr_nodes);
		rcode = EDVSBADVALUE;
		SP_error(rcode);
		// pthread_mutex_unlock(&rd_mutex);
		ERROR_EXIT(rcode);
	}
	nr_sync 	= sp_ptr->m2_i2;
	nr_radar  	= sp_ptr->m2_i3;
	TASKDEBUG("nr_sync=%d\n", nr_sync);
	bm_nodes 	= sp_ptr->m2_l1;
	bm_sync		= sp_ptr->m2_l2;
	bm_radar	= (long) sp_ptr->m2_p1;
	FSM_state   = STS_WAIT4SYNC;
	rcode = mc_synchronized();

	return(OK);	
}

/*=========================================================================*
 *				handle_synchronized							     	*
 * A new sync member inform about it to other sync	members					*
 ===========================================================================*/
int handle_synchronized(  message  *sp_ptr)
{
	int rcode;
	
	TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(sp_ptr));
	
	if ( sp_ptr->m_source == local_nodeid) {
		if( FSM_state != STS_WAIT4SYNC){
			TASKDEBUG("FSM_state=%d != STS_WAIT4SYNC\n", FSM_state);
		}
		FSM_state = STS_SYNCHRONIZED;
		TASKDEBUG("synchronized=%d\n", synchronized);
		synchronized = TRUE;
		TASKDEBUG("synchronized=%d\n", synchronized);
		sync_pr = FALSE; //MARIE
		TASKDEBUG("Sync proccesing - sync_pr=%d\n", sync_pr);
	}
	/* ESTO NO SE SI ESTA BIEN 	
	* puede que un miembro recien ingresado haya recibido un mensaje 
	*  MC_STATUS_INFO y haya cambiado los valores de bm_sync
	* pero luego otro en el mismo estado se sincronizo y entonces
	* bm_sync cambio y el miembro no sync no actualiza el bm_sync.
	*/
	if( synchronized == FALSE ) return (OK);
	
	nr_sync++;
	TASKDEBUG("nr_sync=%d\n", nr_sync);
	SET_BIT(bm_sync, sp_ptr->m_source);
	TASKDEBUG("New sync mbr=%d bm_sync=%X\n", 
		sp_ptr->m_source , bm_sync);
	
	if ( primary_mbr == local_nodeid) {
		rcode = mc_status_info();
		if( rcode < 0) ERROR_RETURN(rcode);
	}
	return(OK);	
}
				
/***************************************************************************/
/* AUXILIARY FUNCTIONS 										*/
/***************************************************************************/

/*===========================================================================*
 *				replica_updated				     					*
 * check if the node storage is UPDATED								*
 *===========================================================================*/
int replica_updated(int localnodeid)
{
	return(TRUE);
}

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
	TASKDEBUG("grp_name=%s mbr_string=%s len=%d\n", grp_name,mbr_string, len);
	assert(strncmp(grp_name, mbr_string, len) == 0);  
					
	dot_ptr = strchr(&mbr_string[len], '.'); /* locate the dot character after "#xxxxxNN.yyyy" */
	assert(dot_ptr != NULL);

	*dot_ptr = '\0';
	n_ptr = &mbr_string[len];
	dcid = atoi(n_ptr);
	*dot_ptr = '.';

	TASKDEBUG("member=%s dcid=%d\n", mbr_string,  dcid );
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
	TASKDEBUG("grp_name=%s mbr_string=%s len=%d\n", grp_name,mbr_string, len);
	assert(strncmp(grp_name, mbr_string, len) == 0);  
					
	dot_ptr = strchr(&mbr_string[len], '.'); 
	assert(dot_ptr != NULL);
	n_ptr = dot_ptr+1;					
	
	s_ptr = strchr(n_ptr, '#'); 
	assert(s_ptr != NULL);

	*s_ptr = '\0';
	nid = atoi( (int *) n_ptr);
	*s_ptr = '#';
	TASKDEBUG("member=%s nid=%d\n", mbr_string,  nid );

	return(nid);
}


/*===========================================================================*
*				mc_status_info
* Send GROUP status to members
*===========================================================================*/
int mc_status_info(void)
{
	int rcode;
	message  msg;

	if( primary_mbr != local_nodeid)
		ERROR_RETURN(EDVSBADNODEID);
	
	msg.m_source= local_nodeid;			/* this is the primary */
	msg.m_type 	= MC_STATUS_INFO;
	
	msg.m2_i1	= nr_nodes;
	msg.m2_i2	= nr_sync;
	
	msg.m2_l1	= bm_nodes;
	msg.m2_l2	= bm_sync;
	
	TASKDEBUG("local_nodeid=%d, nr_nodes=%d, nr_sync=%d\n", local_nodeid, nr_nodes, nr_sync);
	
	rcode = SP_multicast (sysmbox, SAFE_MESS, (char *) rdisk_group,  
			MC_STATUS_INFO, sizeof(message), (char *) &msg); 
	if(rcode <0) ERROR_RETURN(rcode);

	return(rcode);
}

/*===========================================================================*
*				mcast_radar_info
* Send GROUP status to members
*===========================================================================*/
int mcast_radar_info(void)
{
	int rcode;
	message  msg, *m_ptr;
	
	if( primary_mbr != local_nodeid)
		ERROR_RETURN(EDVSBADNODEID);
	
	m_ptr = &msg;
	
	msg.m_source= local_nodeid;			/* this is the primary */
	msg.m_type 	= MC_RADAR_INFO;
	
	msg.m2_i1	= nr_nodes;
	msg.m2_i2	= nr_sync;
	msg.m2_i3	= rd_ep;	
	
	msg.m2_l1	= bm_nodes;
	msg.m2_l2	= bm_sync;
	
	TASKDEBUG("local_nodeid=%d, nr_nodes=%d, nr_sync=%d\n", local_nodeid, nr_nodes, nr_sync);
	TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(m_ptr));
	
	rcode = SP_multicast (sysmbox, SAFE_MESS, (char *) rdisk_group,  
			MC_RADAR_INFO, sizeof(message), (char *) &msg); 
	if(rcode <0) ERROR_RETURN(rcode);

	return(rcode);
}

/*===========================================================================*
*				mc_synchronized
* A new synchronized member informs to other sync members
*===========================================================================*/
int mc_synchronized(void)
{
	int rcode;
	message  msg;

	msg.m_source= local_nodeid;			/* this is the new sync member  */
	msg.m_type 	= MC_SYNCHRONIZED;
	
	rcode = SP_multicast (sysmbox, SAFE_MESS, (char *) rdisk_group,  
			MC_SYNCHRONIZED, sizeof(message), (char *) &msg); 
	if(rcode <0) ERROR_RETURN(rcode);

	return(rcode);
}

/*===========================================================================*
 *				get_primary_mbr				     
 * get the Primary member from bitmap					     
 *===========================================================================*/
int get_primary_mbr(void)
{
	int i, first_mbr;
	
	first_mbr = NO_PRIMARY;  /* to test for errors */
	for( i=0; i < dc_ptr->dc_nr_nodes; i++ ) {
		if ( TEST_BIT(bm_sync, i) ){
			first_mbr = i;
			break;
		}
	}
	TASKDEBUG("primary_mbr=%d\n", first_mbr );
	return(first_mbr);
}

