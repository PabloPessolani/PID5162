/****************************************************************/
/* 				TRACKER											*/
/* This is a service for local process registered on a DC		*/
/* It solves:    nodeid = tracker(endpoint);					*/
/****************************************************************/

#define _GNU_SOURCE     
#define _MULTI_THREADED
#define _TABLE
#include "dc_init.h"

#undef EXTERN
#define EXTERN	extern
#include "glo.h"

#define TRUE 1

int	nr_tracker;
int bm_tracker;
trk_msg_t *sp_ptr;

extern dc_usr_t dcu;	

int get_nodeid(char *grp_name, char *mbr_string);
int get_dcid(char *grp_name, char *mbr_string);
void *tracker_thread(void *);
void connect_to_spread(void);
void init_control_vars(void);
void init_spread(void);

/*===========================================================================*
 *				   main 				    					 			*
 * This program receive requests of nodeid resolution for a provided endpoint*
 *===========================================================================*/
int tracker_main (void)
{
	int rcode, svrep, i;
	proc_usr_t svr_usr, *svr_ptr;
	message m, *m_ptr;
	int trk_nr, trk_ep; 
	proc_usr_t proc, *proc_ptr;
	
//	init_spread( );
		
	USRDEBUG("Starting TRACKER Server \n");
	rcode = pthread_create( &trk_thread, NULL, tracker_thread, (void *) NULL);
	if( rcode) ERROR_EXIT(rcode);
	
	trk_nr = SYSTASK(local_nodeid);
	USRDEBUG("dc_dcid=%d trk_nr=%d\n", dc_ptr->dc_dcid, trk_nr);
   	trk_ep = dvk_bind(dc_ptr->dc_dcid, trk_nr );
	USRDEBUG("trk_nr=%d tt_ep=%d\n", trk_nr, trk_ep);
	if( trk_ep != trk_nr ) ERROR_RETURN(trk_ep);
	
	m_ptr = &m;
	while(1){
		USRDEBUG("TRACKER Server dvk_receive_T\n");
		rcode = dvk_receive_T(ANY, m_ptr, TIMEOUT_MOLCALL);
		if (rcode == EDVSTIMEDOUT) continue;
		if( rcode < 0) {
			ERROR_PRINT(rcode);
			sleep(5);
			continue;
		}
		USRDEBUG(TRK_FORMAT, TRK_FIELDS(m_ptr));
		
		switch ( m_ptr->m_type){
			case TRACKER_REQUEST:	// from Client to Tracker
				// check that the requester is a local process 
				proc_ptr = &proc;
				rcode = dvk_getprocinfo(dc_ptr->dc_dcid, m_ptr->m_source, proc_ptr);
				if(rcode <  EDVSERRCODE) {
					ERROR_PRINT(rcode);
					break;
				}
				if( proc_ptr->p_nodeid != local_nodeid) {
					ERROR_PRINT(EDVSBADNODEID);
					break;
				}
				
				// Send a MULTICAST to the TRACKER  group
				// m_ptr->m_type = TRACKER_REQUEST
				// m_ptr->sch_ep = endpoint 
				m_ptr->rqtr_ep 		= m_ptr->m_source;// requester endpoint  			
				m_ptr->rqtr_nodeid 	= local_nodeid; 	// requester nodeid 
				// SAFE_MESS can be change by a more relaxed order 
				rcode = SP_multicast (trk_mbox, SAFE_MESS, (char *) trk_sp_group,  
					TRACKER_REQUEST, sizeof(message), (char *) m_ptr); 
				if(rcode <  0) {
					ERROR_PRINT(rcode);
					break;
				}
				break;
			case TRACKER_REPLY:		// from Tracker thread to Tracker main.
				rcode = dvk_send_T(m_ptr->rqtr_ep, m_ptr, TIMEOUT_NOWAIT);
				if( rcode < 0) ERROR_PRINT(rcode);
				break;
			default:
				ERROR_PRINT(m_ptr->m_type);
				break;
		}
	}
	
	// never here 
	rcode = pthread_join(trk_thread, NULL);
	return(OK);				
}

/*===========================================================================*
 *				tracker_thread				     
 *===========================================================================*/	
void *tracker_thread(void *x)
{
	int rcode, tt_nr, tt_ep;
	
	USRDEBUG("Starting\n");

//	connect_to_spread();

	nr_tracker = 0;
	bm_tracker = 0;
	
	tt_nr = SYSTEM - dc_ptr->dc_nr_nodes;
	#define SELFTASK (-1) 
	tt_ep = dvk_replbind(dc_ptr->dc_dcid, SELFTASK ,tt_nr);
	USRDEBUG("tt_nr=%d tt_ep=%d\n", tt_nr, tt_ep);
	if( tt_ep != tt_nr) ERROR_EXIT(tt_ep);
	
	while(TRUE){
		rcode = tracker_loop();
		USRDEBUG("rcode=%d\n", rcode);
		if(rcode < 0 ) {
			ERROR_PRINT(rcode);
			sleep(TRACKER_ERROR_SPEEP);
			if( rcode == EDVSNOTCONN) {
				connect_to_spread(); /// OJO PUEDE NO ANDAR 
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

    trk_timeout.sec 	= TRACKER_TIMEOUT_SEC;
    trk_timeout.usec 	= TRACKER_TIMEOUT_MSEC;

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
void connect_to_spread(void)
{
	int rcode;

	/*------------------------------------------------------------------------------------
	* trk_mbr_name:  it must be unique in the spread node.
	*  TRACKER<dcid>.<local_nodeid>
	*--------------------------------------------------------------------------------------*/
	sprintf(trk_sp_group, "TRACKER%02d",  dc_ptr->dc_dcid);
	USRDEBUG("spread_group=%s\n", trk_sp_group);
	sprintf( Spread_name, "4803");
	sprintf( trk_mbr_name, "TRACKER%02d.%02d", dc_ptr->dc_dcid,local_nodeid);
	USRDEBUG("trk_mbr_name=%s\n", trk_mbr_name);

	rcode = SP_connect_timeout( Spread_name, trk_mbr_name , 0, 1, 
					&trk_mbox, trk_priv_group, trk_timeout );
					
	if( rcode != ACCEPT_SESSION ) 	{
		SP_error (rcode);
		ERROR_RETURN(rcode);
	}
	
	USRDEBUG("trk_mbr_name %s: connected to %s with private group %s\n",
				trk_mbr_name, Spread_name, trk_priv_group);

	rcode = SP_join( trk_mbox, trk_sp_group);
	if( rcode){
		SP_error (rcode);
		ERROR_RETURN(rcode);
	}
}

/*===========================================================================*
 *				init_control_vars				     
 * nitilize several global and replicated  variables
 *===========================================================================*/
void init_control_vars(void)
{	

}

/*===========================================================================*
 *				tracker_loop				    
 *===========================================================================*/

int tracker_loop(void )
{
	char	sender[MAX_GROUP_NAME];
	char	target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
	int		num_groups;

	int		service_type;
	int16	mess_type;
	int		endian_mismatch;
	int		i,j;
	int		ret, mbr, dcid;

	service_type = 0;
	num_groups = -1;

	USRDEBUG("SP_receive: %s\n", trk_mbr_name);
	
	assert(trk_mbox != NULL);
	assert(trk_mess_in != NULL);
		
	ret = SP_receive( trk_mbox, &service_type, sender, 100, &num_groups, target_groups,
			&mess_type, &endian_mismatch, MAX_MESSLEN, trk_mess_in );
	USRDEBUG("ret=%d\n", ret);
		
	if( ret < 0 ){
       	if ( (ret == GROUPS_TOO_SHORT) || (ret == BUFFER_TOO_SHORT) ) {
			service_type = DROP_RECV;
            USRDEBUG("\n========Buffers or Groups too Short=======\n");
            ret = SP_receive( trk_mbox, &service_type, sender, 
					MAX_MEMBERS, &num_groups, target_groups,
					&mess_type, &endian_mismatch, MAX_MESSLEN, trk_mess_in);
		}
	}

	if (ret < 0 ) {
		SP_error( ret );
		ERROR_PRINT(ret);
		pthread_exit(NULL);
	}
	
	USRDEBUG("sender=%s Private_group=%s service_type=%d\n", 
		sender, trk_priv_group, service_type);

	sp_ptr = (trk_msg_t *) trk_mess_in;
	
	if( Is_regular_mess( service_type ) )	{
		trk_mess_in[ret] = 0;
		if     ( Is_unreliable_mess( service_type ) ) {USRDEBUG("received UNRELIABLE \n ");}
		else if( Is_reliable_mess(   service_type ) ) {USRDEBUG("received RELIABLE \n");}
		else if( Is_causal_mess(       service_type ) ) {USRDEBUG("received CAUSAL \n");}
		else if( Is_agreed_mess(       service_type ) ) {USRDEBUG("received AGREED \n");}
		else if( Is_safe_mess(   service_type ) || Is_fifo_mess(       service_type ) ) {
			USRDEBUG("%s: message from %s, of type %d, (endian %d) to %d groups (%d bytes)\n",
				trk_mbr_name, sender, mess_type, endian_mismatch, num_groups, ret);

			/*----------------------------------------------------------------------------------------------------
			*   TRACKER_REQUEST		 
			*----------------------------------------------------------------------------------------------------*/
			if ( mess_type == TRACKER_REQUEST ) {
				ret = rcv_tracker_request(sp_ptr);
			} else 	if ( mess_type == TRACKER_REPLY ) {
				ret = rcv_tracker_reply(sp_ptr);
			} else {
				USRDEBUG("%s: Ignored message type %X\n", trk_mbr_name, mess_type);
				ret = OK;
			}
		}
	}else if( Is_membership_mess( service_type ) )	{
        ret = SP_get_memb_info( trk_mess_in, service_type, &trk_memb_info );
        if (ret < 0) {
			USRDEBUG("BUG: membership message does not have valid body\n");
           	SP_error( ret );
			ERROR_PRINT(ret);
			pthread_exit(NULL);
        }
		if  ( Is_reg_memb_mess( service_type ) ) {
			USRDEBUG("%s: Received REGULAR membership for group %s with %d members, where I am member %d:\n",
				trk_mbr_name, sender, num_groups, mess_type );

			if( Is_caused_join_mess( service_type ) ||
				Is_caused_leave_mess( service_type ) ||
				Is_caused_disconnect_mess( service_type ) ){
				trk_sp_nr_mbrs = num_groups;
#ifdef TEMPORAL				/// ESTA DANDO ERROR ESTO !!!!!!!
				memcpy((void*) trk_sp_members, (void *) target_groups, trk_sp_nr_mbrs*MAX_GROUP_NAME);
				for( i=0; i < trk_sp_nr_mbrs; i++ ){
					USRDEBUG("\t%s\n", trk_sp_members[i][0]);	
				}
#endif TEMPORAL

			}
		}

		if( Is_caused_join_mess( service_type ) )	{
			/*----------------------------------------------------------------------------------------------------
			*   JOIN: The group has a new TRACKER  member
			*----------------------------------------------------------------------------------------------------*/
			USRDEBUG("%s: Due to the JOIN of %s service_type=%d\n", 
				trk_mbr_name, trk_memb_info.changed_member, service_type );
			if ( strncmp(trk_memb_info.changed_member, "#TRACKER", 8) == 0) {
				mbr = get_nodeid("TRACKER", (char *)  trk_memb_info.changed_member);
				dcid= get_dcid("TRACKER", (char *) trk_memb_info.changed_member);
				assert(dcid == dc_ptr->dc_dcid);
				USRDEBUG("%s: JOIN - nr_tracker=%d bm_tracker=%X\n", 
					trk_mbr_name, nr_tracker, bm_tracker); 
				nr_tracker = num_groups;
				SET_BIT(bm_tracker, mbr);
				USRDEBUG("%s: JOIN end - nr_tracker=%d bm_tracker=%X\n", 
					trk_mbr_name, nr_tracker, bm_tracker); 
			}
		}else if( Is_caused_leave_mess( service_type ) 
			||  Is_caused_disconnect_mess( service_type ) ){
			/*----------------------------------------------------------------------------------------------------
			*   LEAVE or DISCONNECT:  A member has left the group
			*----------------------------------------------------------------------------------------------------*/
			USRDEBUG("%s: Due to the LEAVE or DISCONNECT of %s\n", 
				trk_mbr_name, trk_memb_info.changed_member );
			if ( strncmp(trk_memb_info.changed_member, "#TRACKER",8) == 0) { 
				mbr = get_nodeid("TRACKER", (char *) trk_memb_info.changed_member);
				dcid= get_dcid("TRACKER", (char *) trk_memb_info.changed_member);
				assert(dcid == dc_ptr->dc_dcid);
				USRDEBUG("%s: LEAVE - nr_tracker=%d bm_tracker=%X\n", 
					trk_mbr_name, nr_tracker, bm_tracker); 
				nr_tracker = num_groups;
				CLR_BIT(bm_tracker, mbr);
				USRDEBUG("%s: LEAVE end - nr_tracker=%d bm_tracker=%X\n", 
					trk_mbr_name, nr_tracker, bm_tracker); 
			}
		}else if( Is_caused_network_mess( service_type ) ){
			/*----------------------------------------------------------------------------------------------------
			*   NETWORK CHANGE:  A network partition or a dead deamon
			*----------------------------------------------------------------------------------------------------*/
			USRDEBUG("%s: Due to NETWORK change with %u VS sets\n", 
				trk_mbr_name, trk_memb_info, trk_num_vs_sets);
            trk_num_vs_sets = SP_get_vs_sets_info( trk_mess_in, 
									&trk_vssets[0], MAX_VSSETS, &trk_my_vsset_index );
            if (trk_num_vs_sets < 0) {
				USRDEBUG("BUG: membership message has more then %d vs sets. Recompile with larger MAX_VSSETS\n",
					MAX_VSSETS);
				SP_error( trk_num_vs_sets );
               	ERROR_EXIT( trk_num_vs_sets );
			}
            if (trk_num_vs_sets == 0) {
				USRDEBUG("BUG: membership message has %d vs_sets\n", 
					trk_num_vs_sets);
				SP_error( trk_num_vs_sets );
               	ERROR_EXIT( EDVSGENERIC );
			}


            for( i = 0; i < trk_num_vs_sets; i++ )  {
				USRDEBUG("%s VS set %d has %u members:\n",
					(i  == trk_my_vsset_index) ?("LOCAL") : ("OTHER"), 
						i, trk_vssets[i].num_members );
               	ret = SP_get_vs_set_members(trk_mess_in, &trk_vssets[i], trk_members, MAX_MEMBERS);
               	if (ret < 0) {
					USRDEBUG("VS Set has more then %d members. Recompile with larger MAX_MEMBERS\n", MAX_MEMBERS);
					SP_error( ret );
                   	ERROR_EXIT( ret);
              	}

				/*---------------------------------------------
				* get the bitmap of current members
				--------------------------------------------- */
				for( j = 0; j < trk_vssets[i].num_members; j++ ) {
					USRDEBUG("\t%s\n", trk_members[j] );
					if ( strncmp(trk_members[j], "#TRACKER",8) == 0) {	
						mbr = get_nodeid("TRACKER", trk_members[j]);
						dcid = get_dcid("TRACKER", trk_members[j]);
						assert(dcid == dc_ptr->dc_dcid);
						if(!TEST_BIT(bm_tracker, mbr)) {
							SET_BIT(bm_tracker, mbr);
							nr_tracker++;
						}		
					}
				}
				USRDEBUG("%s:  nr_tracker=%d bm_tracker=%X\n",
					trk_mbr_name, nr_tracker, bm_tracker);
			}
			USRDEBUG("%s: nr_tracker=%d bm_tracker=%X\n",
					trk_mbr_name, nr_tracker, bm_tracker);
		}else if( Is_transition_mess(   service_type ) ) {
			USRDEBUG("received TRANSITIONAL membership for group %s\n", sender );
			if( Is_caused_leave_mess( service_type ) ){
				USRDEBUG("received membership message that left group %s\n", sender );
			}else {
				USRDEBUG("received incorrecty membership message of type 0x%x\n", service_type );
			}
		} else if ( Is_reject_mess( service_type ) )      {
			USRDEBUG("REJECTED message from %s, of servicetype 0x%x messtype %d, (endian %d) to %d groups \n(%d bytes): %s\n",
				sender, service_type, mess_type, endian_mismatch, num_groups, ret, trk_mess_in );
		}else {
			USRDEBUG("received message of unknown message type 0x%x with ret %d\n", service_type, ret);
		}
	}
	if(ret < 0) ERROR_RETURN(ret);
	return(ret);
}

/*===========================================================================*
*				rcv_tracker_request
* Receive a TRACKER_REQUEST message 
*===========================================================================*/
int rcv_tracker_request(void)
{
	int rcode;
	message *m_ptr, *tk_mptr;
	proc_usr_t proc, *p_ptr;
	trk_msg_t  tmsg, *t_ptr;
	
	m_ptr = (message *) sp_ptr;
	USRDEBUG(TRK_FORMAT, TRK_FIELDS(m_ptr));

	// ignore SELF Request 
	if( m_ptr->rqtr_nodeid == local_nodeid)
		return(OK);
		
	p_ptr = &proc;
	rcode = dvk_getprocinfo(dc_ptr->dc_dcid, m_ptr->sch_ep, p_ptr);
	if( rcode < 0) ERROR_RETURN(rcode);

	if( TEST_BIT( p_ptr->p_rts_flags, BIT_SLOT_FREE))
		return(OK);
	
	if( TEST_BIT( p_ptr->p_rts_flags, BIT_REMOTE))
		return(OK);
	
	t_ptr = &tmsg;
	tk_mptr = &t_ptr->tk_message;
	memcpy( tk_mptr, m_ptr, sizeof(message)); 
	
	tk_mptr->m_type 		=  TRACKER_REPLY;
	tk_mptr->rply_nodeid 	=  local_nodeid;
	tk_mptr->sch_rts_flags 	=  p_ptr->p_rts_flags;
	tk_mptr->sch_misc_flags	=  p_ptr->p_misc_flags;
	USRDEBUG(TRK_FORMAT, TRK_FIELDS(tk_mptr));
	memcpy(	t_ptr->tk_name, p_ptr->p_name, MAXPROCNAME);
		
	// SAFE_MESS can be change by a more relaxed order 
	rcode = SP_multicast (trk_mbox, SAFE_MESS, (char *) trk_sp_group,  
			TRACKER_REPLY, sizeof(trk_msg_t), (char *) t_ptr); 
	if(rcode <0) ERROR_RETURN(rcode);
	return(OK);
}

/*===========================================================================*
*				rcv_tracker_reply
* Receive a TRACKER_REPLY message 
*===========================================================================*/
int rcv_tracker_reply(void)
{
	int rcode;
	message *m_ptr, *tk_mptr;
	proc_usr_t proc, *p_ptr;
	proc_usr_t tproc, *tp_ptr;
	trk_msg_t  tmsg, *t_ptr;
	
	t_ptr = (trk_msg_t *) sp_ptr;
	tk_mptr = &t_ptr->tk_message;
	USRDEBUG(TRK_FORMAT, TRK_FIELDS(tk_mptr));

	// ignore SELF Reply - It cant be, because self request is ignored 
	if( tk_mptr->rply_nodeid == local_nodeid)
		return(OK);
		
	p_ptr = &proc;
	rcode = dvk_getprocinfo(dc_ptr->dc_dcid, tk_mptr->sch_ep, p_ptr);
	if( rcode < 0) ERROR_RETURN(rcode);
	
	// Ingore replies from nodes with the endpoint unbound
//	if( TEST_BIT( (long) tk_mptr->sch_rts_flags, BIT_SLOT_FREE))
//		return(OK);

//	if( TEST_BIT( (long)  m_ptr->sch_rts_flags, BIT_REMOTE))
//		return(OK);
	
	// The local node is not the requester 
	if( tk_mptr->rqtr_nodeid != local_nodeid) {
		if ( TEST_BIT( (long) p_ptr->p_rts_flags, BIT_SLOT_FREE)){
			rcode = dvk_rmtbind(dc_ptr->dc_dcid, t_ptr->tk_name, 
						tk_mptr->sch_ep, tk_mptr->rply_nodeid);
			if( rcode < EDVSERRCODE) ERROR_RETURN(rcode);
			// if the remote process is bound in other node, migrate it 
		} else if( p_ptr->p_nodeid != tk_mptr->rply_nodeid){
			rcode = dvk_migr_start(dc_ptr->dc_dcid, tk_mptr->sch_ep);
			if( rcode < 0) ERROR_RETURN(rcode);						
			rcode = dvk_migr_commit(PROC_NO_PID, dc_ptr->dc_dcid, 
					tk_mptr->sch_ep, tk_mptr->rply_nodeid);
			if( rcode < 0) ERROR_RETURN(rcode);						
		}
		return(OK);
	}
	
	// the local node is the REQUESTER 
	rcode = dvk_rmtbind(dc_ptr->dc_dcid, t_ptr->tk_name, 
						tk_mptr->sch_ep, tk_mptr->rply_nodeid);
	if( rcode < EDVSERRCODE) ERROR_RETURN(rcode);

	// Only the requester will reply 
	rcode = dvk_send(TRACKER_EP(local_nodeid), tk_mptr);	
	if(rcode <0) ERROR_RETURN(rcode);
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