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

#define MT_BASE				1000		// From MONITOR to AGENTS
#define MT_WAIT_BIND		(MT_BASE + WAIT_BIND)
#define MT_WAIT_UNBIND		(MT_BASE + WAIT_UNBIND)
#define MT_LOAD_THRESHOLDS	(MT_WAIT_UNBIND + 1)	// From MONITOR to AGENTS
#define MT_LOAD_LEVEL		(MT_LOAD_THRESHOLDS + 1)// From AGENTS to MONITOR 
#define MT_RUN_COMMAND 	 	(MT_LOAD_LEVEL + 1)		// From LB to AGENT (unicast)

// FLAGS of svr_status 
#define CLT_WAIT_START		0 // At list a Client Proxy is waiting to the server node starts
#define CLT_WAIT_STOP		1 // At list a Client Proxy is waiting to the server node stops
#define SVR_WAIT_STOP		2 // The Server Proxy is waiting to the server node stops
 		 		