//nombre del grupo de spread
#define SPREAD_GROUP "LBGROUP"
#define SHARP_GROUP  "#LBGROUP"
//nombre con el cual se va a identificar al balancer
#define LBM_NAME  "LBM"
#define LBM_SHARP "#LBM"

//Cantidad de nodos que se almacenan en el struct (podria ser diferente en ambos?)
#define MAX_AGENT_NODES NR_NODES
#define LBA_NAME 	"LBA"
#define LBA_SHARP  	"#LBA"

#define MT_CLT_WAIT_BIND	1	// Client proxy waits for BIND
#define MT_CLT_WAIT_UNBIND	2	// Client proxy waits for UNBIND
#define MT_SVR_WAIT_UNBIND	3   // Server proxy waits for UNBIND
#define MT_LOAD_THRESHOLDS	4	// From MONITOR to AGENTS
#define MT_LOAD_LEVEL		5 	// From AGENTS to MONITOR 
#define MT_RUN_COMMAND 	 	6	// From LB to AGENT (unicast)
#define MT_ACKNOWLEDGE 	 	0x1000	

// FLAGS of svr_bm_sts 
#define CLT_WAIT_START		0 // At least a Client Proxy is waiting to the server node starts
#define CLT_WAIT_STOP		1 // At least a Client Proxy is waiting to the server node stops
#define SVR_WAIT_STOP		2 // The Server Proxy is waiting to the server node stops
#define SVR_STARTING 		3 // The server is starting
#define SVR_STOPPING  		4 // The server is stopping 
#define SVR_RUNNING			5 // The server is running
#define SVR_STOPPED  		6 // The server is stopped

// FLAGS of xxx_px_sts 
#define PX_CONNECTED 		0 // At least a Client Proxy is waiting to the server node starts
#define PX_SEND_CONNECTED 	1 // The remote proxy SENDER   is connected 
#define PX_RECV_CONNECTED 	2 // The remote proxy RECEIVER is connected 



 		 		