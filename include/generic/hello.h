
#ifndef _HELLO_H
#define _HELLO_H

////////////////////////////////////////////////////////////////////////////
//  	HELLO MESSAGES COULD TRANSPORT INFO TO THE PEER PROXY 
//  AN EXAMPLE IS LOAD INFO 
////////////////////////////////////////////////////////////////////////////
// IF c_cmd == HELLO 
// The message fiels are filled with the following info types


#define MT_CLT_WAIT_BIND	1	// Client proxy waits for BIND
#define MT_CLT_WAIT_UNBIND	2	// Client proxy waits for UNBIND
#define MT_SVR_WAIT_UNBIND	3   // Server proxy waits for UNBIND
#define MT_LOAD_THRESHOLDS	4	// From MONITOR to AGENTS
#define MT_LOAD_LEVEL		5 	// From AGENTS to MONITOR 
#define MT_RUN_COMMAND 	 	6	// From LB to AGENT (unicast)
#define MT_RMTBIND			7 
#define	MT_GET_DVSINFO		8
#define	MT_GET_DCINFO		9
#define	MT_GET_PROCINFO		10
#define MT_ACKNOWLEDGE 	 	0x1000	

#define LVL_IDLE 			0
#define LVL_BUSY 			1
#define LVL_SATURATED 		2

/***************************************************************************
if m_type==MT_LOAD_THRESHOLDS
			m_ptr->m_source			=> nodeid;
			m_ptr->m1_i1			=> low_water level 
			m_ptr->m1_i2			=> high_water level 
			m_ptr->m1_i3			=> period;
	DO NOT HAVE REPLY
	
if m_type==MT_LOAD_LEVEL
			m_ptr->m_source			=> nodeid;
			m_ptr->m9_i1			=> load_lvl;
			m_ptr->m9_l1			=> cpu_usage;
			m_ptr->m9_t1			=> Timestamp 
		DO NOT HAVE REPLY 
		
if m_type == MT_RUN_COMMAND
			c_len					= strlen(command)
			payload				= command 
		REPLY IS  the result of system(cmd) in m_type 
		
if m_type== MT_CLT_WAIT_BIND:
		 MT_CLT_WAIT_UNBIND:
		 MT_SVR_WAIT_UNBIND:
			m_ptr->m1_i1			= dcid 
			m_ptr->m1_i2			= endpoint 
		REPLY IS  the result of dvk_wait4bind() o dvk_wait4unbind()  in m_type 

********************************************************************************/
			
#endif // _HELLO_H
