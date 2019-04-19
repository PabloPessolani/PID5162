

#define	TRACKER_REQUEST 	1
#define	TRACKER_REPLY		2

#define rqtr_ep 		m1_i1 // requester endpoint
#define rqtr_nodeid 	m1_i2 // requester nodeid 
#define rply_nodeid  	m1_i3 // nodeid of the TRACKER replier
#define sch_ep			m1_p1 // searched endpoint 
#define sch_rts_flags 	m1_p2 // searched endpoint p_rts_flags
#define sch_misc_flags 	m1_p3 // searched endpoint p_misc_flags

struct trk_msg_s {
	message		tk_message;
	char 		tk_name[MAXPROCNAME]; 
};
typedef struct trk_msg_s trk_msg_t;

#define TRK_FORMAT "source=%d type=%d rqtr_ep=%d rqtr_nodeid=%d rply_nodeid=%d sch_ep=%ld sch_rts_flags=%lX sch_misc_flags=%lX\n"
#define TRK_FIELDS(p) 	p->m_source,p->m_type, p->rqtr_ep, p->rqtr_nodeid, p->rply_nodeid, (long) p->sch_ep, (long)p->sch_rts_flags, (long)p->sch_misc_flags

#define TRACKER_TIMEOUT_SEC		5
#define TRACKER_TIMEOUT_MSEC	0
#define TRACKER_ERROR_SPEEP		5

#define TRACKER_EP	SYSTASK 
