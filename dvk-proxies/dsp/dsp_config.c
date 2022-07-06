/***********************************************
DISTRIBUTED SERVICE PROXY CONFIGURATION FILE
# this is a comment 

# default period: 30 seconds
dsp DSP_NAME {
	nodeid 	0;
	lowwater	30;
	highwater	70;
	period	45;
	start		60; <<=== START_VM_PERIOD
	stop 	60; <<=== SHUTDOWN_VM_PERIOD
	ssh_user	Admin;
	ssh_pass	mendienta
	vm_start  "C:\Users\Usuario\start_vm.bat";
	vm_stop   "C:\Users\Usuario\stop_vm.bat";
	cltname	client0;
	svrname      node0;
	cltdev	eth1;
	svrdev   	eth0;

};

#dcid puede ser un numero 0-(dvs_ptr->d_nr_dcs-1)
#si no se menciona dcid, entonces es ANY
#ext_ep es el endpoint presentado a los clientes.
#min_ep y max_ep es el rango de endpoints en los que se pueden arrancar los services  

client client11 {
	nodeid 		11;
	lbRport		3011; // LB Receiver port  
	cltRport		3000; // Client Receiver port 
	compress	YES; 
	batch		YES;
};

server node1 {
	nodeid 		1;
	lbRport		3001; // LB Receiver port  
	svrRport		3000; // Client Receiver port 
	compress	YES; 
	batch		YES;
	node_img    "D:\PAP\Virtual Machines\Debian 9.4\Debian 9.4.vmx"
};
		
#this service is started on demand 
service m3ftp {
	dcid	0;
	ext_ep	10;
	min_ep	10;
	max_ep	20;	
	bind	replica;
	prog	"/usr/src/dvs/dvs-apps/lb_dvs/run_server.sh" 
};

# This service is already started by itself
service m3ftp2 {
	ext_ep	21;
	min_ep	21;
	max_ep	30;	
};

**************************************************/
#define _GNU_SOURCE     
#define _MULTI_THREADED

#include "dsp_proxy.h"
#include "../../include/generic/configfile.h"
extern char *ctl_socket;	

#define OK				0
#define EXIT_CODE		1
#define NEXT_CODE		2

#define MAX_FLAG_LEN 	30
#define MAXTOKENSIZE	20

#define	TKN_SVR_NODEID	0
#define	TKN_SVR_RPORT	1
#define	TKN_SVR_SPORT 	2
#define TKN_SVR_COMPRESS 3
#define TKN_SVR_BATCH	4
#define TKN_SVR_IMAGE	5
#define TKN_SVR_MAX 	6 // MUST be the last

#define TKN_CLT_NODEID	0
#define TKN_CLT_RPORT	1
#define TKN_CLT_SPORT 	2
#define TKN_CLT_COMPRESS 3
#define TKN_CLT_BATCH	4
#define TKN_CLT_MAX 	5	// MUST be the last

#define TKN_SVC_DCID	0
#define TKN_SVC_EXTEP	1
#define TKN_SVC_MINEP	2
#define TKN_SVC_MAXEP	3
#define TKN_SVC_BIND    4
#define TKN_SVC_PROG	5
#define TKN_SVC_MAX 	6 // MUST be the last

#define TKN_LB_NODEID	 0
#define TKN_LB_LOWWATER  1
#define TKN_LB_HIGHWATER 2
#define TKN_LB_PERIOD	 3
#define TKN_LB_START	 4
#define TKN_LB_STOP	 	 5
#define TKN_LB_VM_START	 6
#define TKN_LB_VM_STOP 	 7
#define TKN_LB_VM_STATUS 8
#define TKN_LB_CLTNAME   9
#define TKN_LB_SVRNAME   10
#define TKN_LB_CLTDEV    11
#define TKN_LB_SVRDEV    12
#define TKN_LB_SSH_HOST  13
#define TKN_LB_SSH_USER  14
#define TKN_LB_SSH_PASS  15
#define TKN_LB_MINSERVERS 16
#define TKN_LB_MAXSERVERS 17
#define TKN_LB_HELLOTIME  18
#define TKN_LB_MAX 		 19	// MUST be the last

#define nil ((void*)0)

char *loadb_name;
char *server_name;
char *client_name;
char *service_name;
int nodeid;

char *cfg_svr_ident[] = {
    "nodeid",
	"rport",
	"sport",
	"compress",
	"batch",
	"node_image",	
};

char *cfg_clt_ident[] = {
    "nodeid",
	"rport",
	"sport",
	"compress",
	"batch",
};

char *cfg_svc_ident[] = {
    "dcid",
    "ext_ep",
    "min_ep",
    "max_ep",
	"bind",
    "prog",

};

char *cfg_lb_ident[] = {
    "nodeid",
    "lowwater",
    "highwater",
    "period",
    "start",
    "stop",
    "vm_start",
    "vm_stop",
    "vm_status",	
    "cltname",
    "svrname",
    "cltdev",
    "svrdev",	
    "ssh_host",
    "ssh_user",
    "ssh_pass",	
    "min_servers",
    "max_servers",
	"hello_time"
};

char nonprog[] = "none";

static void print_list(int indent, config_t *cfg);

int valid_ipaddr(char *ipaddr)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipaddr, &(sa.sin_addr));
    return(result != 0);
}


static void print_words(int indent, config_t *cfg)
{
	PXYDEBUG("\n");

    while (cfg != nil) {
	if (config_isatom(cfg)) {
	    if (config_isstring(cfg)) fputc('"', stdout);
	    printf("%s", cfg->word);
	    if (config_isstring(cfg)) fputc('"', stdout);
	} else {
	    printf("{\n");
	    print_list(indent+4, cfg->list);
	    printf("%*s}", indent, "");
	}
	cfg= cfg->next;
	if (cfg != nil) fputc(' ', stdout);
    }
    printf(";\n");
}

static void print_list(int indent, config_t *cfg)
{
	PXYDEBUG("\n");

    while (cfg != nil) {
	if (!config_issub(cfg)) {
	    fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n");
	    break;
	}
	printf("%*s", indent, "");
	print_words(indent, cfg->list);
	cfg= cfg->next;
    }
}

int search_svc_ident(config_t *cfg)
{
    int i, j, rcode;
    service_t *svc_ptr;
	
	svc_ptr = &service_tab[lb.lb_nr_services];				
    for( i = 0; cfg!=nil; i++) {
        if (config_isatom(cfg)) {
            PXYDEBUG("search_svc_ident[%d] line=%d word=%s\n",i,cfg->line, cfg->word); 
            for( j = 0; j < TKN_SVC_MAX; j++) {
                if( !strcmp(cfg->word, cfg_svc_ident[j])) {
                    PXYDEBUG("line[%d] MATCH identifier %s\n", cfg->line, cfg->word); 
                    if( cfg->next == nil)
                        fprintf(stderr, "Void value found at line %d\n", cfg->line);
                    cfg = cfg->next;	
                    switch(j){		
                        case TKN_SVC_PROG:
							if (!config_isstring(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							PXYDEBUG("prog=%s\n", cfg->word);
							svc_ptr->svc_prog=(cfg->word);
							PXYDEBUG("svc_prog=%s\n", svc_ptr->svc_prog);
							SET_BIT(svc_ptr->svc_bm_params, TKN_SVC_PROG);
							break;
                        case TKN_SVC_DCID:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}		
							svc_ptr->svc_dcid =atoi(cfg->word);							
							if ((svc_ptr->svc_dcid < 0) || (svc_ptr->svc_dcid >= dvs_ptr->d_nr_dcs)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "DCID:%d, must be > 0 and < dvs_ptr->d_nr_dcs(%d)\n", 
									svc_ptr->svc_dcid,dvs_ptr->d_nr_dcs);
								return(EXIT_CODE);
							}
							PXYDEBUG("svr_dcid=%d\n", svc_ptr->svc_dcid);
							SET_BIT(svc_ptr->svc_bm_params, TKN_SVC_DCID);
							break;
                        case TKN_SVC_EXTEP:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							svc_ptr->svc_extep = atoi(cfg->word);
							PXYDEBUG("svc_extep=%d\n", svc_ptr->svc_extep);
							if ((svc_ptr->svc_extep < 0) || (svc_ptr->svc_extep >= MAX_SVC_NR)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "ext_ep:%d, must be > 0 and < MAX_SVC_NR(%d)\n", 
										svc_ptr->svc_extep,MAX_SVC_NR);
								return(EXIT_CODE);
							}
							SET_BIT(svc_ptr->svc_bm_params, TKN_SVC_EXTEP);
							break;
                        case TKN_SVC_MINEP:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							svc_ptr->svc_minep = atoi(cfg->word);
							PXYDEBUG("svc_minep=%d\n", svc_ptr->svc_minep);
							if ((svc_ptr->svc_minep < 0) || (svc_ptr->svc_minep >= MAX_SVC_NR)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "svc_minep:%d, must be > 0 and < MAX_SVC_NR(%d)\n", 
										svc_ptr->svc_minep,MAX_SVC_NR);
								return(EXIT_CODE);
							}
							SET_BIT(svc_ptr->svc_bm_params, TKN_SVC_MINEP);
							break;
                        case TKN_SVC_MAXEP:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							svc_ptr->svc_maxep = atoi(cfg->word);
							PXYDEBUG("svc_maxep=%d\n", svc_ptr->svc_maxep);
							if ((svc_ptr->svc_maxep < 0) || (svc_ptr->svc_maxep >= MAX_SVC_NR)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "svc_maxep:%d, must be > 0 and < MAX_SVC_NR(%d)\n", 
										svc_ptr->svc_maxep,MAX_SVC_NR);
								return(EXIT_CODE);
							}
							SET_BIT(svc_ptr->svc_bm_params, TKN_SVC_MAXEP);
							break;
						case TKN_SVC_BIND:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							if( strcmp(cfg->word, "replica") == 0) 
								svc_ptr->svc_bind = REPLICA_BIND;
							else if ( strcmp(cfg->word, "local") == 0)
								svc_ptr->svc_bind = LCL_BIND;
							else if ( strcmp(cfg->word, "backup") == 0)
								svc_ptr->svc_bind = BKUP_BIND;
							else if ( strcmp(cfg->word, "external") == 0)
								svc_ptr->svc_bind = PROG_BIND;
							else {
								fprintf(stderr, "Configuration Error\n");
								exit(1);
							}		
							PXYDEBUG("svc_bind=%X\n", svc_ptr->svc_bind);
							SET_BIT(svc_ptr->svc_bm_params, TKN_SVC_BIND);
							break;
                        default:
							fprintf(stderr, "Programming Error\n");
							exit(1);
                    }
                    return(OK);
                }	
            }
            if( j == TKN_SVC_MAX)
                fprintf(stderr, "Invalid identifier found at line %d\n", cfg->line);
        }
        cfg = cfg->next;
    }
    return(OK);
}

int search_svr_ident(config_t *cfg)
{
    int i, j, rcode, dcid;
    server_t *svr_ptr;
	
    PXYDEBUG("nodeid=%d\n", nodeid);
	if(nodeid != LB_INVALID)
		svr_ptr = &server_tab[nodeid];
    for( i = 0; cfg!=nil; i++) {
        if (config_isatom(cfg)) {
            PXYDEBUG("search_svr_ident[%d] line=%d word=%s\n",i,cfg->line, cfg->word); 
            for( j = 0; j < TKN_SVR_MAX; j++) {
                if( !strcmp(cfg->word, cfg_svr_ident[j])) {
                    PXYDEBUG("line[%d] MATCH identifier %s\n", cfg->line, cfg->word); 
                    if( cfg->next == nil)
                        fprintf(stderr, "Void value found at line %d\n", cfg->line);
                    cfg = cfg->next;	
					if( j != TKN_SVR_NODEID){
						if( svr_ptr->svr_nodeid == LB_INVALID){
							fprintf(stderr, "Server nodeid must be defined first: line %d\n", cfg->line);
							return(EXIT_CODE);
						}						
					}
                    switch(j){		
                        case TKN_SVR_NODEID:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							if( nodeid != (-1)){
								fprintf(stderr, "nodeid previously defined %d: line %d\n", nodeid, cfg->line);
								return(EXIT_CODE);
							}
							PXYDEBUG("nodeid=%d\n", atoi(cfg->word));
							nodeid = atoi(cfg->word);							
							if ((nodeid < 0) || (nodeid >= dvs_ptr->d_nr_nodes)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "nodeid:%d, must be > 0 and < %d\n", nodeid,dvs_ptr->d_nr_nodes);
								return(EXIT_CODE);
							}
							svr_ptr = &server_tab[nodeid];
							svr_ptr->svr_nodeid = nodeid;
							if( server_name == NULL) {
								fprintf(stderr, "server name not defined at line %d\n", cfg->line);
								return(EXIT_CODE);							
							}
							svr_ptr->svr_name = server_name;
							PXYDEBUG("svr_nodeid=%d\n", svr_ptr->svr_nodeid);
							SET_BIT(svr_ptr->svr_bm_params, TKN_SVR_NODEID);
							break;
                        case TKN_SVR_RPORT:						
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							svr_ptr->svr_rport = atoi(cfg->word);
							PXYDEBUG("svr_rport=%d\n", svr_ptr->svr_rport);

							if ((svr_ptr->svr_rport < LBP_BASE_PORT) || (svr_ptr->svr_rport >= (LBP_BASE_PORT+dvs_ptr->d_nr_nodes))) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "svr_rport:%d, must be >= LBP_BASE_PORT(%d) and < (LBP_BASE_PORT+dvs_ptr->d_nr_nodes)(%d)\n", 
										svr_ptr->svr_rport,LBP_BASE_PORT,(LBP_BASE_PORT+dvs_ptr->d_nr_nodes));
								return(EXIT_CODE);
							}
							SET_BIT(svr_ptr->svr_bm_params, TKN_SVR_RPORT);
							break;
                        case TKN_SVR_SPORT:		
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							svr_ptr->svr_sport = atoi(cfg->word);
							PXYDEBUG("svr_sport=%d\n", svr_ptr->svr_sport);

							if ((svr_ptr->svr_sport < LBP_BASE_PORT) || (svr_ptr->svr_sport >= (LBP_BASE_PORT+dvs_ptr->d_nr_nodes))) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "svr_sport:%d, must be >= LBP_BASE_PORT(%d) and < (LBP_BASE_PORT+dvs_ptr->d_nr_nodes)(%d)\n", 
										svr_ptr->svr_sport,LBP_BASE_PORT,(LBP_BASE_PORT+dvs_ptr->d_nr_nodes));
								return(EXIT_CODE);
							}
							SET_BIT(svr_ptr->svr_bm_params, TKN_SVR_SPORT);
							break;
                        case TKN_SVR_COMPRESS:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							PXYDEBUG("compress=%s\n", cfg->word);
							if ( strncasecmp(cfg->word,"YES", 3) == 0){
								svr_ptr->svr_compress = YES;
							} else if ( strncasecmp(cfg->word,"NO", 2) == 0) {
								PXYDEBUG("svr_ptr->svr_nodeid=%d\n", svr_ptr->svr_nodeid);
								svr_ptr->svr_compress = NO;								
							} else {
								PXYDEBUG("\n");
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "compress: must be YES or NO\n");
								return(EXIT_CODE);
							}
							PXYDEBUG("svr_compress=%d\n", svr_ptr->svr_compress);
							SET_BIT(svr_ptr->svr_bm_params, TKN_SVR_COMPRESS);							
							break;
                        case TKN_SVR_BATCH:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							PXYDEBUG("batch=%s\n", cfg->word);					
							if ( strncasecmp(cfg->word,"YES", 3) == 0){
								svr_ptr->svr_batch = YES;
							} else if ( strncasecmp(cfg->word,"NO", 2) == 0) {
								svr_ptr->svr_batch = NO;								
							} else {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "batch: must be YES or NO\n");
								return(EXIT_CODE);
							}
							PXYDEBUG("svr_batch=%d\n", svr_ptr->svr_batch);
							SET_BIT(svr_ptr->svr_bm_params, TKN_SVR_BATCH);							
							break;							
                        case TKN_SVR_IMAGE:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							svr_ptr->svr_image = cfg->word;		
							PXYDEBUG("svr_image=%s\n", svr_ptr->svr_image);
							SET_BIT(svr_ptr->svr_bm_params, TKN_SVR_IMAGE);							
							break;
						default:
							fprintf(stderr, "Programming Error\n");
							exit(1);
                    }
                    return(OK);
                }	
            }
            if( j == TKN_SVR_MAX)
                fprintf(stderr, "Invalid identifier found at line %d\n", cfg->line);
        }
        cfg = cfg->next;
    }
    return(OK);
}

int search_lb_ident(config_t *cfg)
{
    int i, j, rcode;
    
    PXYDEBUG("lb.lb_nodeid=%d\n", lb.lb_nodeid);
    for( i = 0; cfg!=nil; i++) {
        if (config_isatom(cfg)) {
            PXYDEBUG("search_lb_ident[%d] line=%d word=%s\n",i,cfg->line, cfg->word); 
            for( j = 0; j < TKN_LB_MAX; j++) {
                if( !strcmp(cfg->word, cfg_lb_ident[j])) {
                    PXYDEBUG("line[%d] MATCH identifier %s\n", cfg->line, cfg->word); 
                    if( cfg->next == nil)
                        fprintf(stderr, "Void value found at line %d\n", cfg->line);
                    cfg = cfg->next;
					if( j != TKN_LB_NODEID) {
						if( lb.lb_nodeid == LB_INVALID){
							fprintf(stderr, "lb nodeid must be defined first: line %d\n", cfg->line);
							return(EXIT_CODE);
						}						
					}
                    switch(j){	
                        case TKN_LB_NODEID:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							
							if( lb.lb_nodeid != LB_INVALID){
								fprintf(stderr, "nodeid previously defined %d: line %d\n", lb.lb_nodeid, cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_nodeid = atoi(cfg->word);
							PXYDEBUG("lb.lb_nodeid=%d\n", lb.lb_nodeid);
							if ((lb.lb_nodeid < 0) || (lb.lb_nodeid >= dvs_ptr->d_nr_nodes)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "nodeid:%d, must be > 0 and < %d\n", lb.lb_nodeid,dvs_ptr->d_nr_nodes);
								return(EXIT_CODE);
							}	
							SET_BIT(lb.lb_bm_params, TKN_LB_NODEID);
							break;
                        case TKN_LB_LOWWATER:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_lowwater = atoi(cfg->word);
							PXYDEBUG("lb.lb_lowwater=%d\n", lb.lb_lowwater);
							if ((lb.lb_lowwater < 0) || (lb.lb_lowwater > 100)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "lowwater:%d, must be (0-100)\n", lb.lb_lowwater);
								return(EXIT_CODE);
							}
							SET_BIT(lb.lb_bm_params, TKN_LB_LOWWATER);							
							break;
                        case TKN_LB_HIGHWATER:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_highwater = atoi(cfg->word);
							PXYDEBUG("lb.lb_highwater=%d\n", lb.lb_highwater);
							if ((lb.lb_highwater < 0) || (lb.lb_highwater > 100)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "highwater:%d, must be (0-100)\n", lb.lb_highwater);
								return(EXIT_CODE);
							}		
							SET_BIT(lb.lb_bm_params, TKN_LB_HIGHWATER);							
							break;
                        case TKN_LB_PERIOD:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_period = atoi(cfg->word);
							PXYDEBUG("lb_period=%d\n", lb.lb_period);
							if ((lb.lb_period < 1) || (lb.lb_period > SECS_BY_HOUR)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "period:%d, must be (1-3600)\n", lb.lb_period);
								return(EXIT_CODE);
							}
							SET_BIT(lb.lb_bm_params, TKN_LB_PERIOD);							
							break;
                        case TKN_LB_START:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_start = atoi(cfg->word);
							PXYDEBUG("lb_start=%d\n", lb.lb_start);
							if ((lb.lb_start < 1) || (lb.lb_start > SECS_BY_HOUR)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "period:%d, must be (1-3600)\n", lb.lb_start);
								return(EXIT_CODE);
							}
							SET_BIT(lb.lb_bm_params, TKN_LB_START);							
							break;
                        case TKN_LB_STOP:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_stop = atoi(cfg->word);
							PXYDEBUG("lb_stop=%d\n", lb.lb_stop);
							if ((lb.lb_stop < 1) || (lb.lb_stop > SECS_BY_HOUR)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "period:%d, must be (1-3600)\n", lb.lb_stop);
								return(EXIT_CODE);
							}
							SET_BIT(lb.lb_bm_params, TKN_LB_STOP);							
							break;
                        case TKN_LB_VM_START:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_vm_start = cfg->word;
							PXYDEBUG("lb_vm_start=%s\n", lb.lb_vm_start);
							SET_BIT(lb.lb_bm_params, TKN_LB_VM_START);							
							break;
                        case TKN_LB_VM_STOP:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_vm_stop = cfg->word;
							PXYDEBUG("lb_vm_stop=%s\n", lb.lb_vm_stop);
							SET_BIT(lb.lb_bm_params, TKN_LB_VM_STOP);							
							break;							
                        case TKN_LB_VM_STATUS:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_vm_status = cfg->word;
							PXYDEBUG("lb_vm_status=%s\n", lb.lb_vm_status);
							SET_BIT(lb.lb_bm_params, TKN_LB_VM_STATUS);							
							break;							
                        case TKN_LB_CLTNAME:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_cltname = cfg->word;
							PXYDEBUG("lb_cltname=%s\n", lb.lb_cltname);
							if( !strncmp(lb.lb_cltname,"ANY",3)) {
								if( !valid_ipaddr(lb.lb_cltname)){
									fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
									fprintf(stderr, "cltname:%s, must be \"ANY\" or a valid local host name\n", 
											lb.lb_cltname);
									return(EXIT_CODE);
								} 
							}
							SET_BIT(lb.lb_bm_params, TKN_LB_CLTNAME);							
							break;
                        case TKN_LB_SVRNAME:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_svrname = cfg->word;
							PXYDEBUG("lb_svrname=%s\n", lb.lb_svrname);
							if( !strncmp(lb.lb_svrname,"ANY",3)) {
								if( !valid_ipaddr(lb.lb_svrname)){
									fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
									fprintf(stderr, "svrname:%s, must be \"ANY\" or a valid local host name\n", 
											lb.lb_svrname);
									return(EXIT_CODE);
								} 
							}
							SET_BIT(lb.lb_bm_params, TKN_LB_SVRNAME);							
							break;							
                        case TKN_LB_CLTDEV:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_cltdev = cfg->word;
							PXYDEBUG("lb_cltdev=%s\n", lb.lb_cltdev);
							SET_BIT(lb.lb_bm_params, TKN_LB_CLTDEV);							
							break;
                        case TKN_LB_SVRDEV:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_svrdev = cfg->word;
							PXYDEBUG("lb_svrdev=%s\n", lb.lb_svrdev);
							SET_BIT(lb.lb_bm_params, TKN_LB_SVRDEV);							
							break;
                        case TKN_LB_SSH_HOST:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_ssh_host = cfg->word;
							PXYDEBUG("lb_ssh_host=%s\n", lb.lb_ssh_host);
							SET_BIT(lb.lb_bm_params, TKN_LB_SSH_HOST);							
							break;
						case TKN_LB_SSH_USER:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_ssh_user = cfg->word;
							PXYDEBUG("lb_ssh_user=%s\n", lb.lb_ssh_user);
							SET_BIT(lb.lb_bm_params, TKN_LB_SSH_USER);							
							break;
                        case TKN_LB_SSH_PASS:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_ssh_pass = cfg->word;
							PXYDEBUG("lb_ssh_pass=%s\n", lb.lb_ssh_pass);
							SET_BIT(lb.lb_bm_params, TKN_LB_SSH_PASS);							
							break;
                        case TKN_LB_MINSERVERS:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_min_servers = atoi(cfg->word);
							PXYDEBUG("lb.lb_min_servers=%d\n", lb.lb_min_servers);
							if ((lb.lb_min_servers < 0) || (lb.lb_min_servers > dvs_ptr->d_nr_nodes)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "lb_min_servers:%d, must be (0-%d)\n", lb.lb_min_servers, dvs_ptr->d_nr_nodes);
								return(EXIT_CODE);
							}
							SET_BIT(lb.lb_bm_params, TKN_LB_MINSERVERS);							
							break;
                        case TKN_LB_MAXSERVERS:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_max_servers = atoi(cfg->word);
							PXYDEBUG("lb.lb_max_servers=%d\n", lb.lb_max_servers);
							if ((lb.lb_max_servers < 1) || (lb.lb_max_servers > dvs_ptr->d_nr_nodes)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "max_servers:%d, must be (1-%d)\n", lb.lb_max_servers, dvs_ptr->d_nr_nodes);
								return(EXIT_CODE);
							}		
							SET_BIT(lb.lb_bm_params, TKN_LB_MAXSERVERS);							
							break;
                        case TKN_LB_HELLOTIME:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_hellotime = atoi(cfg->word);
							PXYDEBUG("lb_hellotime=%d\n", lb.lb_hellotime);
							if ((lb.lb_hellotime < 1) || (lb.lb_hellotime > SECS_BY_HOUR)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "hello_time:%d, must be (1-3600)\n", lb.lb_hellotime);
								return(EXIT_CODE);
							}
							SET_BIT(lb.lb_bm_params, TKN_LB_HELLOTIME);							
							break;
							
						default:
							fprintf(stderr, "Programming Error\n");
							exit(1);
                    }
                    return(OK);
                }	
            }
            if( j == TKN_LB_MAX)
                fprintf(stderr, "Invalid identifier found at line %d\n", cfg->line);
        }
        cfg = cfg->next;
    }
    return(OK);
}

int search_clt_ident(config_t *cfg)
{
    int i, j, rcode;
    client_t *clt_ptr;

    PXYDEBUG("nodeid=%d\n", nodeid);
	if(nodeid != LB_INVALID)
		clt_ptr = &client_tab[nodeid];
    for( i = 0; cfg!=nil; i++) {
        if (config_isatom(cfg)) {
            PXYDEBUG("search_clt_ident[%d] line=%d word=%s\n",i,cfg->line, cfg->word); 
            for( j = 0; j < TKN_CLT_MAX; j++) {
                if( !strcmp(cfg->word, cfg_clt_ident[j])) {
                    PXYDEBUG("line[%d] MATCH identifier %s\n", cfg->line, cfg->word); 
                    if( cfg->next == nil)
                        fprintf(stderr, "Void value found at line %d\n", cfg->line);
                    cfg = cfg->next;
					if( j != TKN_CLT_NODEID){
						if( clt_ptr->clt_nodeid == LB_INVALID){
							fprintf(stderr, "Client nodeid must be defined first: line %d\n", cfg->line);
							return(EXIT_CODE);
						}						
					}
                    switch(j){	
                        case TKN_CLT_NODEID:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							if( nodeid != LB_INVALID){
								fprintf(stderr, "nodeid previously defined %d: line %d\n", nodeid, cfg->line);
								return(EXIT_CODE);
							}
							nodeid = atoi(cfg->word);
							PXYDEBUG("nodeid=%d\n", nodeid);
							if ((nodeid < 0) || (nodeid >= dvs_ptr->d_nr_nodes)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "nodeid:%d, must be > 0 and < %d\n", nodeid,dvs_ptr->d_nr_nodes);
								return(EXIT_CODE);
							}
							clt_ptr = &client_tab[nodeid];
							clt_ptr->clt_nodeid = nodeid;
							if( client_name == NULL) {
								fprintf(stderr, "client name not defined at line %d\n", cfg->line);
								return(EXIT_CODE);							
							}
							clt_ptr->clt_name = client_name;
							PXYDEBUG("clt_nodeid=%d\n", clt_ptr->clt_nodeid);
							SET_BIT(clt_ptr->clt_bm_params, TKN_CLT_NODEID);
							break;
                        case TKN_CLT_RPORT:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							clt_ptr->clt_rport = atoi(cfg->word);
							PXYDEBUG("clt_rport=%d\n", clt_ptr->clt_rport);

							if ((clt_ptr->clt_rport < LBP_BASE_PORT) || (clt_ptr->clt_rport >= (LBP_BASE_PORT+dvs_ptr->d_nr_nodes))) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "clt_rport:%d, must be >= LBP_BASE_PORT(%d) and < (LBP_BASE_PORT+dvs_ptr->d_nr_nodes)(%d)\n", 
										clt_ptr->clt_rport,LBP_BASE_PORT,(LBP_BASE_PORT+dvs_ptr->d_nr_nodes));
								return(EXIT_CODE);
							}
							break;
							SET_BIT(clt_ptr->clt_bm_params, TKN_CLT_RPORT);
                        case TKN_CLT_SPORT:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							clt_ptr->clt_sport = atoi(cfg->word);
							PXYDEBUG("clt_sport=%d\n", clt_ptr->clt_sport);

							if ((clt_ptr->clt_sport < LBP_BASE_PORT) || (clt_ptr->clt_sport >= (LBP_BASE_PORT+dvs_ptr->d_nr_nodes))) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "clt_sport:%d, must be >= LBP_BASE_PORT(%d) and < (LBP_BASE_PORT+dvs_ptr->d_nr_nodes)(%d)\n", 
										clt_ptr->clt_sport,LBP_BASE_PORT,(LBP_BASE_PORT+dvs_ptr->d_nr_nodes));
								return(EXIT_CODE);
							}
							SET_BIT(clt_ptr->clt_bm_params, TKN_CLT_SPORT);
							break;
                        case TKN_CLT_COMPRESS:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							PXYDEBUG("compress=%s\n", cfg->word);					
							if ( strncasecmp(cfg->word,"YES", 3) == 0){
								clt_ptr->clt_compress = YES;
							} else if ( strncasecmp(cfg->word,"NO", 2) == 0) {
								clt_ptr->clt_compress = NO;								
							} else {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "compress: must be YES or NO\n");
								return(EXIT_CODE);
							}
							PXYDEBUG("clt_compress=%d\n", clt_ptr->clt_compress);
							SET_BIT(clt_ptr->clt_bm_params, TKN_CLT_COMPRESS);							
							break;							
                        case TKN_CLT_BATCH:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							PXYDEBUG("batch=%s\n", cfg->word);					
							if ( strncasecmp(cfg->word,"YES", 3) == 0){
								clt_ptr->clt_batch = YES;
							} else if ( strncasecmp(cfg->word,"NO", 2) == 0) {
								clt_ptr->clt_batch = NO;								
							} else {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "batch: must be YES or NO\n");
								return(EXIT_CODE);
							}
							PXYDEBUG("clt_batch=%d\n", clt_ptr->clt_batch);
							SET_BIT(clt_ptr->clt_bm_params, TKN_CLT_BATCH);							
							break;							
						default:
							fprintf(stderr, "Programming Error\n");
							exit(1);
                    }
                    return(OK);
                }	
            }
            if( j == TKN_LB_MAX)
                fprintf(stderr, "Invalid identifier found at line %d\n", cfg->line);
        }
        cfg = cfg->next;
    }
    return(OK);
}

int read_svr_lines( config_t *cfg)
{
    int i;
    int rcode;
	
    PXYDEBUG("\n");
    for ( i = 0; cfg != nil; i++) {
        PXYDEBUG("read_svr_lines type=%X\n",cfg->flags); 
        rcode = search_svr_ident(cfg->list);
        if( rcode) ERROR_RETURN(rcode);
        if( cfg == nil)return(OK);
        cfg = cfg->next;
    }
    return(OK);
}	

int read_svc_lines( config_t *cfg)
{
    int i;
    int rcode;
	
    PXYDEBUG("\n");
    for ( i = 0; cfg != nil; i++) {
        PXYDEBUG("read_svc_lines type=%X\n",cfg->flags); 
        rcode = search_svc_ident(cfg->list);
        if( rcode) ERROR_RETURN(rcode);
        if( cfg == nil)return(OK);
        cfg = cfg->next;
    }
    return(OK);
}

int read_lb_lines( config_t *cfg)
{
    int rcode;
    PXYDEBUG("\n");
    while ( cfg != nil) {
        PXYDEBUG("read_lb_lines type=%X\n",cfg->flags); 
        rcode = search_lb_ident(cfg->list);
		PXYDEBUG("\n");
        if( rcode) ERROR_RETURN(rcode);
        if( cfg == nil) return(OK);
        cfg = cfg->next;
    }
    return(OK);
}

int read_clt_lines( config_t *cfg)
{
    int rcode;
    PXYDEBUG("\n");
    while ( cfg != nil) {
        PXYDEBUG("read_clt_lines type=%X\n",cfg->flags); 
        rcode = search_clt_ident(cfg->list);
		PXYDEBUG("\n");
        if( rcode) ERROR_RETURN(rcode);
        if( cfg == nil) return(OK);
        cfg = cfg->next;
    }
    return(OK);
}

int search_main_token(config_t *cfg)
{
    int rcode, i;
    config_t *name_cfg;
	server_t *svr_ptr;
    client_t *clt_ptr;
	service_t *svc_ptr;
	
	loadb_name = NULL;
	server_name = NULL;
	client_name = NULL;
	nodeid = (-1);
	
    PXYDEBUG("line=%d\n", cfg->line);
    if (cfg != nil) {
        if (config_isatom(cfg)) {
            PXYDEBUG("word=%s\n", cfg->word);
            if( !strcmp(cfg->word, "server")) {
                cfg = cfg->next;
                PXYDEBUG("server: \n");
                if (cfg != nil) {
					server_name = cfg->word; 
					PXYDEBUG("%s\n", server_name);
					name_cfg = cfg;
					cfg = cfg->next;
					if (!config_issub(cfg)) {
						fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n",cfg->word, cfg->line);
						return(EXIT_CODE);
					}
					rcode = read_svr_lines(cfg->list);
					if(rcode) return(EXIT_CODE);
					svr_ptr = &server_tab[nodeid];	
			        PXYDEBUG(SERVER_FORMAT, SERVER_FIELDS(svr_ptr));
			        PXYDEBUG(SERVER1_FORMAT, SERVER1_FIELDS(svr_ptr));
					lb.lb_nr_svrpxy++;
					SET_BIT(lb.lb_bm_svrpxy, nodeid);
					for( i = 0; i < TKN_SVR_MAX; i++){
						if( !TEST_BIT(svr_ptr->svr_bm_params, i)){
							fprintf( stderr,"CONFIGURATION WARNING: server %s parameter %s not configured\n", 
								server_name, cfg_svr_ident[i]);
						}else{
							PXYDEBUG("server %s: %s configured \n", server_name, cfg_svr_ident[i]);
						}
					}
					return(OK);
                }
			} else if( !strcmp(cfg->word, "service")) {
                cfg = cfg->next;
                PXYDEBUG("service: \n");
                if (cfg != nil) {
					service_name = cfg->word; 
					PXYDEBUG("%s\n", service_name);
					name_cfg = cfg;
					cfg = cfg->next;
					if (!config_issub(cfg)) {
						fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n",cfg->word, cfg->line);
						return(EXIT_CODE);
					}
					rcode = read_svc_lines(cfg->list);
					if(rcode) return(EXIT_CODE);
					svc_ptr = &service_tab[lb.lb_nr_services];
					svc_ptr->svc_name = service_name; 
			        PXYDEBUG(SERVICE_FORMAT, SERVICE_FIELDS(svc_ptr));
					lb.lb_nr_services++;
					for( i = 0; i < TKN_SVC_MAX; i++){
						if( !TEST_BIT(svc_ptr->svc_bm_params, i)){
							fprintf( stderr,"CONFIGURATION WARNING: service %s parameter %s not configured\n", 
								service_name, cfg_svc_ident[i]);
						}else{
							PXYDEBUG("service %s: %s configured \n", service_name, cfg_svc_ident[i]);
						}				
					}						
					return(OK);
                }					
            } else if( !strcmp(cfg->word, "client")) {
                cfg = cfg->next;
                PXYDEBUG("client: \n");
                if (cfg != nil) {
					client_name = cfg->word; 
					PXYDEBUG("%s\n", client_name);
					name_cfg = cfg;
					cfg = cfg->next;
					if (!config_issub(cfg)) {
						fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n",cfg->word, cfg->line);
						return(EXIT_CODE);
					}
					rcode = read_clt_lines(cfg->list);
					if(rcode) return(EXIT_CODE);
					clt_ptr = &client_tab[nodeid];
					clt_ptr->clt_name = client_name;
			        PXYDEBUG(CLIENT_FORMAT, CLIENT_FIELDS(clt_ptr));
					lb.lb_nr_cltpxy++;
					for( i = 0; i < TKN_CLT_MAX; i++){
						if( !TEST_BIT(clt_ptr->clt_bm_params, i)){
							fprintf( stderr,"CONFIGURATION WARNING: client %s parameter %s not configured\n", 
								client_name, cfg_clt_ident[i]);
						}else{
							PXYDEBUG("client %s: %s configured \n", client_name, cfg_clt_ident[i]);
						}					
					}					
					return(OK);
                }		
            } else if( !strcmp(cfg->word, "lb")) {
                cfg = cfg->next;
                PXYDEBUG("lb: \n");
                if (cfg != nil) {
					loadb_name = cfg->word; 
					PXYDEBUG("%s\n", loadb_name);
					cfg = cfg->next;
					if (!config_issub(cfg)) {
						fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n",cfg->word, cfg->line);
						return(EXIT_CODE);
					}
					lb.lb_name = loadb_name;
					rcode = read_lb_lines(cfg->list);
					if(rcode) ERROR_RETURN(EXIT_CODE);
					return(OK);
                }
            }
            fprintf(stderr, "Config error line:%d No token found\n", cfg->line);
            return(EXIT_CODE);
        }
        fprintf(stderr, "Config error line:%d No name found \n", cfg->line);
        return(EXIT_CODE);
    }
    return(EXIT_CODE);
}


int scan_config(config_t *cfg)
{
    int rcode;	
    
    PXYDEBUG("\n");
    for(int i=0; cfg != nil; i++) {
        if (!config_issub(cfg)) {
            fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n", cfg->word, cfg->line);
            return(EXIT_CODE);
        }
        PXYDEBUG("scan_config[%d] line=%d\n",i,cfg->line);
        rcode = search_main_token(cfg->list);
        //@ERROR: Out of bounds read
        if( rcode == EXIT_CODE)
            return(rcode);
        cfg = cfg->next;
    }
    return(OK);
}


/*===========================================================================*
 *				lb_config				     *
 *===========================================================================*/
void lb_config(char *f_conf)	/* config file name. */
{
    /* Main program of lb_config. */
    config_t *cfg;
    int rcode, i, j;
    server_t *svr1_ptr;
    server_t *svr2_ptr;
    client_t *clt1_ptr;
    client_t *clt2_ptr;
	service_t *svc1_ptr;
	service_t *svc2_ptr;
	lb_t *lb_ptr;

    cfg = nil;
    rcode  = OK;

    PXYDEBUG("BEFORE init_lb\n");
	init_lb();
    PXYDEBUG("AFTER init_lb\n");
	
//////////////////////////////////////////////////////////////////////////////////
	for( i = 0; i < dvs_ptr->d_nr_nodes; i++){
		svr1_ptr = &server_tab[i];
		clt1_ptr = &client_tab[i];
		clt1_ptr->clt_compress = LB_INVALID;	
		clt1_ptr->clt_batch    = LB_INVALID;
		svr1_ptr->svr_rport  = (LBP_BASE_PORT+i);
		svr1_ptr->svr_sport = (LBP_BASE_PORT+i);
		clt1_ptr->clt_rport  = (LBP_BASE_PORT+i);
		clt1_ptr->clt_sport = (LBP_BASE_PORT+i);
	}
//////////////////////////////////////////////////////////////////////////////////
	
    PXYDEBUG("BEFORE config_read\n");
    cfg = config_read(f_conf, CFG_ESCAPED, cfg);
    PXYDEBUG("AFTER config_read\n");  
    rcode = scan_config(cfg);
	
    PXYDEBUG("lb.lb_nodeid=%d\n",lb.lb_nodeid);  
    PXYDEBUG("lb.lb_nr_svrpxy=%d\n",lb.lb_nr_svrpxy);  
    PXYDEBUG("lb.lb_nr_cltpxy=%d\n",lb.lb_nr_cltpxy);  
    PXYDEBUG("lb.lb_nr_services=%d\n",lb.lb_nr_services);  
    PXYDEBUG("lb.lb_lowwater=%d\n",lb.lb_lowwater);  
    PXYDEBUG("lb.lb_highwater=%d\n",lb.lb_highwater);  
    PXYDEBUG("lb.lb_min_servers=%d\n",lb.lb_min_servers);  
    PXYDEBUG("lb.lb_max_servers=%d\n",lb.lb_max_servers);  
	
	// check for mandatory parameters
    PXYDEBUG("Check for mandatory parameters=%d\n", TKN_LB_MAX);  
	for( i = 0; i < TKN_LB_MAX; i++){
		if( !TEST_BIT(lb.lb_bm_params, i)){
			fprintf( stderr,"CONFIGURATION WARNING: lb parameter %s not configured\n", 
				cfg_lb_ident[i]);
		}else{
			PXYDEBUG("lb: %s configured \n", cfg_lb_ident[i]);
		}	
	}

	if(lb.lb_nr_cltpxy == 0) {
        fprintf( stderr,"CONFIGURATION ERROR: at least one CLIENT must be defined in configuration file\n");
        exit(1);
    }
	
	if(lb.lb_nr_svrpxy == 0) {
        fprintf( stderr,"CONFIGURATION ERROR: at least one SERVER must be defined in configuration file\n");
        exit(1);
    }
	
	if(lb.lb_nr_svrpxy > dvs_ptr->d_nr_nodes) {
        fprintf( stderr,"CONFIGURATION ERROR: the number of definde servers must <= %d\n", dvs_ptr->d_nr_nodes );
        exit(1);
    }

	if(lb.lb_nr_services == 0) {
        fprintf( stderr,"CONFIGURATION ERROR: at least one SERVICE must be defined in configuration file\n");
        exit(1);
    }
	if(lb.lb_nodeid ==LOCALNODE) {
        fprintf( stderr,"CONFIGURATION ERROR: The lb label must be defined in configuration file\n");
        exit(1);
    }

	if(lb.lb_lowwater < 0 
	|| lb.lb_lowwater > 100
	|| lb.lb_lowwater > lb.lb_highwater
	) {
        fprintf( stderr,"CONFIGURATION ERROR: lowwater must be (0-100) and lower than highwater\n");
        exit(1);
    }

	if(lb.lb_lowwater < 0 
	|| lb.lb_lowwater > 100
	) {
        fprintf( stderr,"CONFIGURATION ERROR: highwater must be (0-100)\n");
        exit(1);
    }

	if(lb.lb_min_servers <= 0 
	|| lb.lb_min_servers > lb.lb_max_servers
	|| lb.lb_min_servers > lb.lb_nr_svrpxy	
	) {
        fprintf( stderr,"CONFIGURATION ERROR: min_servers must be (1-%d) and lower or equal than max_servers\n", lb.lb_nr_svrpxy);
        exit(1);
    }

	if(lb.lb_max_servers <= 0 
	|| lb.lb_max_servers > lb.lb_nr_svrpxy
	) {
        fprintf( stderr,"CONFIGURATION ERROR: max_servers=%d must be (1-%d) \n", lb.lb_max_servers, dvs.d_nr_nodes);
        exit(1);
    }

	if(lb.lb_lowwater < 1 
	|| lb.lb_lowwater > dvs.d_nr_nodes
	) {
        fprintf( stderr,"CONFIGURATION ERROR: max_servers must be (1-%d)\n", dvs.d_nr_nodes);
        exit(1);
    }

	if(lb.lb_period < 1 
	|| lb.lb_period > SECS_BY_HOUR
	) {
        fprintf( stderr,"CONFIGURATION ERROR: load measurement period must be (1-3600)\n");
        exit(1);
    }

	if(lb.lb_hellotime < 1 
	|| lb.lb_hellotime > SECS_BY_HOUR
	) {
        fprintf( stderr,"CONFIGURATION ERROR: HELLO time must be (1-3600)\n");
        exit(1);
    }

	if(lb.lb_hellotime > lb.lb_period) {
        fprintf( stderr,"CONFIGURATION ERROR: HELLO time must be > then period=%d)\n",lb.lb_period);
        exit(1);
    }
	
	if(lb.lb_start < 1 
	|| lb.lb_start > SECS_BY_HOUR
	) {
        fprintf( stderr,"CONFIGURATION ERROR: start server VM period must be (1-3600)\n");
        exit(1);
    }
	
	if(lb.lb_stop < 1 
	|| lb.lb_stop > SECS_BY_HOUR
	) {
        fprintf( stderr,"CONFIGURATION ERROR: stop server VM period must be (1-3600)\n");
        exit(1);
    }
	
	if(lb.lb_cltname == NULL 
	|| lb.lb_svrname == NULL
	) {
        fprintf( stderr,"CONFIGURATION ERROR: cltname and svrname must be defined\n");
        exit(1);
    }

	if(lb.lb_cltdev == NULL 
	|| lb.lb_svrdev == NULL
	) {
        fprintf( stderr,"CONFIGURATION ERROR: cltdev and svrdev must be defined\n");
        exit(1);
    }
		
	if( (TEST_BIT(lb.lb_bm_params, TKN_LB_VM_START) != 0) 
	&&  (TEST_BIT(lb.lb_bm_params, TKN_LB_VM_STOP) != 0)
	&&  (TEST_BIT(lb.lb_bm_params, TKN_LB_VM_STATUS) != 0) ) {
		if( !(lb.lb_ssh_pass != NULL && lb.lb_ssh_user != NULL && lb.lb_ssh_host != NULL )) {
			fprintf( stderr,"CONFIGURATION ERROR: if ssh will be use, ssh_host, ssh_pass and ssh_user must be defined\n");
			exit(1);
		}
	} else {
        fprintf( stderr,"CONFIGURATION ERROR: if VMs will be started/stopped automatically lb parameters vm_start/vm_stop/vm_status must be defined\n");
        exit(1);
    }

	// check SERVER same NODEID
	lb.lb_nr_svrpxy = 0;
	lb.lb_bm_svrpxy = 0;
	for( i = 0; i < dvs_ptr->d_nr_nodes; i++){
		svr1_ptr = &server_tab[i];
		if( svr1_ptr->svr_nodeid == LB_INVALID) continue;
		lb.lb_nr_svrpxy++;
		SET_BIT(lb.lb_bm_svrpxy,i);
        PXYDEBUG(SERVER_FORMAT, SERVER_FIELDS(svr1_ptr));
        PXYDEBUG(SERVER1_FORMAT, SERVER1_FIELDS(svr1_ptr));
		if( i == dvs_ptr->d_nr_nodes-1) break;
							
		if( svr1_ptr->svr_nodeid == lb.lb_nodeid){
			fprintf( stderr,"CONFIGURATION ERROR:Server %s have the same nodeid as load balancer\n",
				svr1_ptr->svr_name);
			exit(1);
		}
		for( j = i+1; j < dvs_ptr->d_nr_nodes; j++){
			svr2_ptr = &server_tab[j];
			if( svr2_ptr->svr_nodeid == LB_INVALID) continue;
			if( svr1_ptr->svr_nodeid == svr2_ptr->svr_nodeid ){
				fprintf( stderr,"CONFIGURATION ERROR:Servers %s and %s have the same nodeid\n",
					svr1_ptr->svr_name, svr2_ptr->svr_name);
				exit(1);
			}
		}
		for( j = 0; j < dvs_ptr->d_nr_nodes; j++){
			clt1_ptr = &client_tab[j];
			if( clt1_ptr->clt_nodeid == LB_INVALID) continue;
			if( svr1_ptr->svr_nodeid == clt1_ptr->clt_nodeid ){
				fprintf( stderr,"CONFIGURATION ERROR:Server %s and Client %s have the same nodeid\n",
					svr1_ptr->svr_name, clt1_ptr->clt_name);
				exit(1);
			}
		}
		// check for a dynamic server node 
		if( svr1_ptr->svr_image != NULL){
			if( (TEST_BIT(lb.lb_bm_params, TKN_LB_VM_START) == 0) 	
			||  (TEST_BIT(lb.lb_bm_params, TKN_LB_VM_STOP) == 0)
			||  (TEST_BIT(lb.lb_bm_params, TKN_LB_VM_STATUS) == 0) ) {
				fprintf( stderr,"CONFIGURATION ERROR: server %s can't be started dynamically.", svr1_ptr->svr_name);
				fprintf( stderr,"Configure lb parameters: vm_start/vm_stop/vm_status \n");
				exit(1);
			}
		}		
	}

	// check CLIENT same NODEID
	for( i = 0; i < dvs_ptr->d_nr_nodes-1; i++){
		clt1_ptr = &client_tab[i];
		if( clt1_ptr->clt_nodeid == LB_INVALID) continue;
        PXYDEBUG(CLIENT_FORMAT, CLIENT_FIELDS(clt1_ptr));
		if( i == dvs_ptr->d_nr_nodes-1) break;

		if( clt1_ptr->clt_nodeid == lb.lb_nodeid){
			fprintf( stderr,"CONFIGURATION ERROR:Client %s have the same nodeid as load balancer\n",
				clt1_ptr->clt_name);
			exit(1);
		}
		for( j = i+1; j < dvs_ptr->d_nr_nodes; j++){
			clt2_ptr = &client_tab[j];
			if( clt2_ptr->clt_nodeid == (-1)) continue;
			if( clt1_ptr->clt_nodeid == clt2_ptr->clt_nodeid ){
				fprintf( stderr,"CONFIGURATION ERROR:Clients %s and %s have the same nodeid\n",
					clt1_ptr->clt_name, clt2_ptr->clt_name);
				exit(1);
			}
		}
		for( j = 0; j < dvs_ptr->d_nr_nodes; j++){
			svr1_ptr = &server_tab[j];
			if( svr1_ptr->svr_nodeid == LB_INVALID) continue;
			if( clt1_ptr->clt_nodeid == svr1_ptr->svr_nodeid ){
				fprintf( stderr,"CONFIGURATION ERROR:Client %s and Server %s have the same nodeid\n",
					clt1_ptr->clt_name, svr1_ptr->svr_name);
				exit(1);
			}
		}		
	}

	// check SERVICE same NAME and external Endpoint
	for( i = 0; i < lb.lb_nr_services; i++){
		svc1_ptr = &service_tab[i];
		assert(svc1_ptr != NULL);
		if( svc1_ptr->svc_extep == LB_INVALID){
				fprintf( stderr,"CONFIGURATION ERROR:Service %s: ext_ep must be defined\n",
					svc1_ptr->svc_name);
				exit(1);
		}
#ifdef NO_MINMAX	
		if( svc1_ptr->svc_minep == LB_INVALID){
				fprintf( stderr,"CONFIGURATION ERROR:Service %s: min_ep must be defined\n",
					svc1_ptr->svc_name);
				exit(1);
		}

		if( svc1_ptr->svc_maxep == LB_INVALID){
				fprintf( stderr,"CONFIGURATION ERROR:Service %s: max_ep must be defined\n",
					svc1_ptr->svc_name);
				exit(1);
		}
#endif // NO_MINMAX	

		if( svc1_ptr->svc_bind == LB_INVALID){
				svc1_ptr->svc_bind  = PROG_BIND;
				svc1_ptr->svc_prog = nonprog;
		}

		if( svc1_ptr->svc_prog == NULL){
				svc1_ptr->svc_prog = nonprog;
				svc1_ptr->svc_bind  = PROG_BIND;				
		}
		
		if( i < lb.lb_nr_services) {
			for( j = i+1; j < lb.lb_nr_services; j++){
				svc2_ptr = &service_tab[j];
				assert(svc2_ptr != NULL);
				if( !strcasecmp(svc1_ptr->svc_name,svc2_ptr->svc_name)){
					fprintf( stderr,"CONFIGURATION ERROR:Service %s repeated\n",
						svc1_ptr->svc_name);
					exit(1);
				}
				if( svc1_ptr->svc_extep == svc2_ptr->svc_extep ){
					fprintf( stderr,"CONFIGURATION ERROR:Service %s same external endpoint as %s\n",
						svc1_ptr->svc_name, svc2_ptr->svc_name);
					exit(1);
				}
			}
			if( (svc1_ptr->svc_minep == HARDWARE &&  svc1_ptr->svc_maxep != HARDWARE)
			||  (svc1_ptr->svc_minep != HARDWARE &&  svc1_ptr->svc_maxep == HARDWARE)){
				fprintf( stderr,"CONFIGURATION ERROR:Service %s must have maxep and minep defined or none\n",
						svc1_ptr->svc_name);
					exit(1);			
			} 
		}
		if(svc1_ptr->svc_minep == HARDWARE) 
			svc1_ptr->svc_minep = svc1_ptr->svc_extep;
		if(svc1_ptr->svc_maxep == HARDWARE) 
			svc1_ptr->svc_maxep = svc1_ptr->svc_extep;
        PXYDEBUG(SERVICE_FORMAT, SERVICE_FIELDS(svc1_ptr));
	}

//    PXYDEBUG("lb.lb_cltname=%s\n",lb.lb_cltname);
//    PXYDEBUG("lb.lb_svrname=%s\n",lb.lb_svrname);
//    PXYDEBUG("lb.lb_cltdev=%s\n",lb.lb_cltdev);
//    PXYDEBUG("lb.lb_svrdev=%s\n",lb.lb_svrdev);

	lb_ptr = &lb;
	lb.lb_nr_proxies = lb.lb_nr_cltpxy + lb.lb_nr_svrpxy;
    PXYDEBUG(LB1_FORMAT, LB1_FIELDS(lb_ptr));
    PXYDEBUG(LB2_FORMAT, LB2_FIELDS(lb_ptr));
    PXYDEBUG(LB3_FORMAT, LB3_FIELDS(lb_ptr));
	if( lb.lb_ssh_pass != NULL && lb.lb_ssh_user != NULL && lb.lb_ssh_host != NULL ) {
		PXYDEBUG(LB4_FORMAT, LB4_FIELDS(lb_ptr));
	}
	if( lb.lb_vm_start != NULL && lb.lb_vm_stop != NULL) {
		PXYDEBUG(LB5_FORMAT, LB5_FIELDS(lb_ptr));
		PXYDEBUG(LB6_FORMAT, LB6_FIELDS(lb_ptr));
	}
	PXYDEBUG(LB7_FORMAT, LB7_FIELDS(lb_ptr));		
}

