/***********************************************
DVS LOAD BALANCER CONFIGURATION FILE
# this is a comment 

# default period: 30 seconds
lb LB_NAME {
	nodeid 	0;
	lowwater	30;
	highwater	70;
	period	45;
	cltname	client0;
	svrname      node0;
	cltdev	eth1;
	svrdev   	eth0;

};

#dcid puede ser un numero 0-(NR_DCS-1)
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
	node_start  "C:\Program Files (x86)\VMware\VMware Player\vmplayer.exe start"
	node_stop   "C:\Program Files (x86)\VMware\VMware Player\vmplayer.exe stop "
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

#include "lb_dvs.h"
#include "../../include/generic/configfile.h"
extern char *ctl_socket;	

#define OK				0
#define EXIT_CODE		1
#define NEXT_CODE		2

#define MAX_FLAG_LEN 	30
#define MAXTOKENSIZE	20

#define	TKN_SVR_NODEID	0
#define	TKN_SVR_LBRPORT	1
#define	TKN_SVR_SVRRPORT 2
#define TKN_SVR_COMPRESS 3
#define TKN_SVR_BATCH	4
#define TKN_SVR_START	5
#define TKN_SVR_STOP	6
#define TKN_SVR_IMAGE	7
#define TKN_SVR_MAX 	8 // MUST be the last

#define TKN_CLT_NODEID	0
#define TKN_CLT_LBRPORT	1
#define TKN_CLT_CLTRPORT 2
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
#define TKN_LB_CLTNAME   4
#define TKN_LB_SVRNAME   5
#define TKN_LB_CLTDEV    6
#define TKN_LB_SVRDEV    7
#define TKN_LB_MAX 		 8	// MUST be the last

#define nil ((void*)0)

char *lb_name;
char *server_name;
char *client_name;
char *service_name;
int nodeid;

char *cfg_svr_ident[] = {
    "nodeid",
	"lbRport",
	"svrRport",
	"compress",
	"batch",	
	"node_start",
	"node_stop",
	"node_image",	
};

char *cfg_clt_ident[] = {
    "nodeid",
	"lbRport",
	"cltRport",
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
    "cltname",
    "svrname",
    "cltdev",
    "svrdev",	
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
	USRDEBUG("\n");

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
	USRDEBUG("\n");

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
            USRDEBUG("search_svc_ident[%d] line=%d word=%s\n",i,cfg->line, cfg->word); 
            for( j = 0; j < TKN_SVC_MAX; j++) {
                if( !strcmp(cfg->word, cfg_svc_ident[j])) {
                    USRDEBUG("line[%d] MATCH identifier %s\n", cfg->line, cfg->word); 
                    if( cfg->next == nil)
                        fprintf(stderr, "Void value found at line %d\n", cfg->line);
                    cfg = cfg->next;	
                    switch(j){		
                        case TKN_SVC_PROG:
							if (!config_isstring(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							USRDEBUG("prog=%s\n", cfg->word);
							svc_ptr->svc_prog=(cfg->word);
							USRDEBUG("svc_prog=%s\n", svc_ptr->svc_prog);
							SET_BIT(lb.lb_bm_svcparms, TKN_SVC_PROG);
							break;
                        case TKN_SVC_DCID:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}		
							svc_ptr->svc_dcid =atoi(cfg->word);							
							if ((svc_ptr->svc_dcid < 0) || (svc_ptr->svc_dcid >= NR_DCS)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "DCID:%d, must be > 0 and < NR_DCS(%d)\n", 
									svc_ptr->svc_dcid,NR_DCS);
								return(EXIT_CODE);
							}
							USRDEBUG("svr_dcid=%d\n", svc_ptr->svc_dcid);
							SET_BIT(lb.lb_bm_svcparms, TKN_SVC_DCID);
							break;
                        case TKN_SVC_EXTEP:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							svc_ptr->svc_extep = atoi(cfg->word);
							USRDEBUG("svc_extep=%d\n", svc_ptr->svc_extep);
							if ((svc_ptr->svc_extep < 0) || (svc_ptr->svc_extep >= MAX_SVC_NR)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "ext_ep:%d, must be > 0 and < MAX_SVC_NR(%d)\n", 
										svc_ptr->svc_extep,MAX_SVC_NR);
								return(EXIT_CODE);
							}
							SET_BIT(lb.lb_bm_svcparms, TKN_SVC_EXTEP);
							break;
                        case TKN_SVC_MINEP:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							svc_ptr->svc_minep = atoi(cfg->word);
							USRDEBUG("svc_minep=%d\n", svc_ptr->svc_minep);
							if ((svc_ptr->svc_minep < 0) || (svc_ptr->svc_minep >= MAX_SVC_NR)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "svc_minep:%d, must be > 0 and < MAX_SVC_NR(%d)\n", 
										svc_ptr->svc_minep,MAX_SVC_NR);
								return(EXIT_CODE);
							}
							SET_BIT(lb.lb_bm_svcparms, TKN_SVC_MINEP);
							break;
                        case TKN_SVC_MAXEP:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							svc_ptr->svc_maxep = atoi(cfg->word);
							USRDEBUG("svc_maxep=%d\n", svc_ptr->svc_maxep);
							if ((svc_ptr->svc_maxep < 0) || (svc_ptr->svc_maxep >= MAX_SVC_NR)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "svc_maxep:%d, must be > 0 and < MAX_SVC_NR(%d)\n", 
										svc_ptr->svc_maxep,MAX_SVC_NR);
								return(EXIT_CODE);
							}
							SET_BIT(lb.lb_bm_svcparms, TKN_SVC_MAXEP);
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
							USRDEBUG("svc_bind=%X\n", svc_ptr->svc_bind);
							SET_BIT(lb.lb_bm_svcparms, TKN_SVC_BIND);
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
	
    USRDEBUG("nodeid=%d\n", nodeid);
	if(nodeid != LB_INVALID)
		svr_ptr = &server_tab[nodeid];
    for( i = 0; cfg!=nil; i++) {
        if (config_isatom(cfg)) {
            USRDEBUG("search_svr_ident[%d] line=%d word=%s\n",i,cfg->line, cfg->word); 
            for( j = 0; j < TKN_SVR_MAX; j++) {
                if( !strcmp(cfg->word, cfg_svr_ident[j])) {
                    USRDEBUG("line[%d] MATCH identifier %s\n", cfg->line, cfg->word); 
                    if( cfg->next == nil)
                        fprintf(stderr, "Void value found at line %d\n", cfg->line);
                    cfg = cfg->next;	
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
							USRDEBUG("nodeid=%d\n", atoi(cfg->word));
							nodeid = atoi(cfg->word);							
							if ((nodeid < 0) || (nodeid >= NR_NODES)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "nodeid:%d, must be > 0 and < NR_NODES(%d)\n", nodeid,NR_NODES);
								return(EXIT_CODE);
							}
							svr_ptr = &server_tab[nodeid];
							svr_ptr->svr_nodeid = nodeid;
							if( server_name == NULL) {
								fprintf(stderr, "server name not defined at line %d\n", cfg->line);
								return(EXIT_CODE);							
							}
							svr_ptr->svr_name = server_name;
							USRDEBUG("svr_nodeid=%d\n", svr_ptr->svr_nodeid);
							SET_BIT(lb.lb_bm_svrparms, TKN_SVR_NODEID);
							break;
                        case TKN_SVR_LBRPORT:
							if( svr_ptr->svr_nodeid == LB_INVALID){
								fprintf(stderr, "Server nodeid must be defined first: line %d\n", cfg->line);
								return(EXIT_CODE);
							}
								
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							svr_ptr->svr_lbRport = atoi(cfg->word);
							USRDEBUG("svr_lbRport=%d\n", svr_ptr->svr_lbRport);

							if ((svr_ptr->svr_lbRport < LBP_BASE_PORT) || (svr_ptr->svr_lbRport >= (LBP_BASE_PORT+NR_NODES))) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "svr_lbRport:%d, must be >= LBP_BASE_PORT(%d) and < (LBP_BASE_PORT+NR_NODES)(%d)\n", 
										svr_ptr->svr_lbRport,LBP_BASE_PORT,(LBP_BASE_PORT+NR_NODES));
								return(EXIT_CODE);
							}
							SET_BIT(lb.lb_bm_svrparms, TKN_SVR_LBRPORT);
							break;
                        case TKN_SVR_SVRRPORT:
							if( svr_ptr->svr_nodeid == LB_INVALID){
								fprintf(stderr, "Server nodeid must be defined first: line %d\n", cfg->line);
								return(EXIT_CODE);
							}						
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							svr_ptr->svr_svrRport = atoi(cfg->word);
							USRDEBUG("svr_svrRport=%d\n", svr_ptr->svr_svrRport);

							if ((svr_ptr->svr_svrRport < LBP_BASE_PORT) || (svr_ptr->svr_svrRport >= (LBP_BASE_PORT+NR_NODES))) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "svr_svrRport:%d, must be >= LBP_BASE_PORT(%d) and < (LBP_BASE_PORT+NR_NODES)(%d)\n", 
										svr_ptr->svr_svrRport,LBP_BASE_PORT,(LBP_BASE_PORT+NR_NODES));
								return(EXIT_CODE);
							}
							SET_BIT(lb.lb_bm_svrparms, TKN_SVR_SVRRPORT);
							break;
                        case TKN_SVR_COMPRESS:
							if( svr_ptr->svr_nodeid == LB_INVALID){
								fprintf(stderr, "Server nodeid must be defined first: line %d\n", cfg->line);
								return(EXIT_CODE);
							}						
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							USRDEBUG("compress=%s\n", cfg->word);
							if ( strncasecmp(cfg->word,"YES", 3) == 0){
								svr_ptr->svr_compress = YES;
							} else if ( strncasecmp(cfg->word,"NO", 2) == 0) {
								USRDEBUG("svr_ptr->svr_nodeid=%d\n", svr_ptr->svr_nodeid);
								svr_ptr->svr_compress = NO;								
							} else {
								USRDEBUG("\n");
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "compress: must be YES or NO\n");
								return(EXIT_CODE);
							}
							USRDEBUG("svr_compress=%d\n", svr_ptr->svr_compress);
							SET_BIT(lb.lb_bm_svrparms, TKN_SVR_COMPRESS);							
							break;
                        case TKN_SVR_BATCH:
							if( svr_ptr->svr_nodeid == LB_INVALID){
								fprintf(stderr, "Server nodeid must be defined first: line %d\n", cfg->line);
								return(EXIT_CODE);
							}						
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							USRDEBUG("batch=%s\n", cfg->word);					
							if ( strncasecmp(cfg->word,"YES", 3) == 0){
								svr_ptr->svr_batch = YES;
							} else if ( strncasecmp(cfg->word,"NO", 2) == 0) {
								svr_ptr->svr_batch = NO;								
							} else {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "batch: must be YES or NO\n");
								return(EXIT_CODE);
							}
							USRDEBUG("svr_batch=%d\n", svr_ptr->svr_batch);
							SET_BIT(lb.lb_bm_svrparms, TKN_SVR_BATCH);							
							break;							
                        case TKN_SVR_START:
							if( svr_ptr->svr_nodeid == LB_INVALID){
								fprintf(stderr, "Server nodeid must be defined first: line %d\n", cfg->line);
								return(EXIT_CODE);
							}						
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							svr_ptr->svr_start = cfg->word;		
							USRDEBUG("svr_start=%s\n", svr_ptr->svr_start);
							SET_BIT(lb.lb_bm_svrparms, TKN_SVR_START);							
							break;							
                        case TKN_SVR_STOP:
							if( svr_ptr->svr_nodeid == LB_INVALID){
								fprintf(stderr, "Server nodeid must be defined first: line %d\n", cfg->line);
								return(EXIT_CODE);
							}						
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							svr_ptr->svr_stop = cfg->word;		
							USRDEBUG("svr_stop=%s\n", svr_ptr->svr_stop);
							SET_BIT(lb.lb_bm_svrparms, TKN_SVR_STOP);							
							break;
                        case TKN_SVR_IMAGE:
							if( svr_ptr->svr_nodeid == LB_INVALID){
								fprintf(stderr, "Server nodeid must be defined first: line %d\n", cfg->line);
								return(EXIT_CODE);
							}							
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							svr_ptr->svr_image = cfg->word;		
							USRDEBUG("svr_image=%s\n", svr_ptr->svr_image);
							SET_BIT(lb.lb_bm_svrparms, TKN_SVR_IMAGE);							
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
    
    USRDEBUG("lb.lb_nodeid=%d\n", lb.lb_nodeid);
    for( i = 0; cfg!=nil; i++) {
        if (config_isatom(cfg)) {
            USRDEBUG("search_lb_ident[%d] line=%d word=%s\n",i,cfg->line, cfg->word); 
            for( j = 0; j < TKN_LB_MAX; j++) {
                if( !strcmp(cfg->word, cfg_lb_ident[j])) {
                    USRDEBUG("line[%d] MATCH identifier %s\n", cfg->line, cfg->word); 
                    if( cfg->next == nil)
                        fprintf(stderr, "Void value found at line %d\n", cfg->line);
                    cfg = cfg->next;	
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
							lb.lb_name   = lb_name;
							USRDEBUG("lb.lb_nodeid=%d\n", lb.lb_nodeid);
							if ((lb.lb_nodeid < 0) || (lb.lb_nodeid >= NR_NODES)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "nodeid:%d, must be > 0 and < NR_NODES(%d)\n", lb.lb_nodeid,NR_NODES);
								return(EXIT_CODE);
							}	
							SET_BIT(lb.lb_bm_lbparms, TKN_LB_NODEID);
							break;
                        case TKN_LB_LOWWATER:
							if( lb.lb_nodeid == LB_INVALID){
								fprintf(stderr, "lb nodeid must be defined first: line %d\n", cfg->line);
								return(EXIT_CODE);
							}						
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_lowwater = atoi(cfg->word);
							USRDEBUG("lb.lb_lowwater=%d\n", lb.lb_lowwater);
							if ((lb.lb_lowwater < 0) || (lb.lb_lowwater > 100)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "lowwater:%d, must be (0-100)\n", lb.lb_lowwater);
								return(EXIT_CODE);
							}
							SET_BIT(lb.lb_bm_lbparms, TKN_LB_LOWWATER);							
							break;
                        case TKN_LB_HIGHWATER:
							if( lb.lb_nodeid == LB_INVALID){
								fprintf(stderr, "lb nodeid must be defined first: line %d\n", cfg->line);
								return(EXIT_CODE);
							}							
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_highwater = atoi(cfg->word);
							USRDEBUG("lb.lb_highwater=%d\n", lb.lb_highwater);
							if ((lb.lb_highwater < 0) || (lb.lb_highwater > 100)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "highwater:%d, must be (0-100)\n", lb.lb_highwater);
								return(EXIT_CODE);
							}		
							SET_BIT(lb.lb_bm_lbparms, TKN_LB_HIGHWATER);							
							break;
                        case TKN_LB_PERIOD:
							if( lb.lb_nodeid == LB_INVALID){
								fprintf(stderr, "lb nodeid must be defined first: line %d\n", cfg->line);
								return(EXIT_CODE);
							}							
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_period = atoi(cfg->word);
							USRDEBUG("lb_period=%d\n", lb.lb_period);
							if ((lb.lb_period < 1) || (lb.lb_period > 3600)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "period:%d, must be (1-3600)\n", lb.lb_period);
								return(EXIT_CODE);
							}
							SET_BIT(lb.lb_bm_lbparms, TKN_LB_PERIOD);							
							break;
                        case TKN_LB_CLTNAME:
							if( lb.lb_nodeid == LB_INVALID){
								fprintf(stderr, "lb nodeid must be defined first: line %d\n", cfg->line);
								return(EXIT_CODE);
							}							
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_cltname = cfg->word;
							USRDEBUG("lb_cltname=%s\n", lb.lb_cltname);
							if( !strncmp(lb.lb_cltname,"ANY",3)) {
								if( !valid_ipaddr(lb.lb_cltname)){
									fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
									fprintf(stderr, "cltname:%s, must be \"ANY\" or a valid local host name\n", 
											lb.lb_cltname);
									return(EXIT_CODE);
								} 
							}
							SET_BIT(lb.lb_bm_lbparms, TKN_LB_CLTNAME);							
							break;
                        case TKN_LB_SVRNAME:
							if( lb.lb_nodeid == LB_INVALID){
								fprintf(stderr, "lb nodeid must be defined first: line %d\n", cfg->line);
								return(EXIT_CODE);
							}							
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_svrname = cfg->word;
							USRDEBUG("lb_svrname=%s\n", lb.lb_svrname);
							if( !strncmp(lb.lb_svrname,"ANY",3)) {
								if( !valid_ipaddr(lb.lb_svrname)){
									fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
									fprintf(stderr, "svrname:%s, must be \"ANY\" or a valid local host name\n", 
											lb.lb_svrname);
									return(EXIT_CODE);
								} 
							}
							SET_BIT(lb.lb_bm_lbparms, TKN_LB_SVRNAME);							
							break;							
                        case TKN_LB_CLTDEV:
							if( lb.lb_nodeid == LB_INVALID){
								fprintf(stderr, "lb nodeid must be defined first: line %d\n", cfg->line);
								return(EXIT_CODE);
							}							
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_cltdev = cfg->word;
							USRDEBUG("lb_cltdev=%s\n", lb.lb_cltdev);
							SET_BIT(lb.lb_bm_lbparms, TKN_LB_CLTDEV);							
							break;							
                        case TKN_LB_SVRDEV:
							if( lb.lb_nodeid == LB_INVALID){
								fprintf(stderr, "lb nodeid must be defined first: line %d\n", cfg->line);
								return(EXIT_CODE);
							}							
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							lb.lb_svrdev = cfg->word;
							USRDEBUG("lb_svrdev=%s\n", lb.lb_svrdev);
							SET_BIT(lb.lb_bm_lbparms, TKN_LB_SVRDEV);							
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

    USRDEBUG("nodeid=%d\n", nodeid);
	if(nodeid != LB_INVALID)
		clt_ptr = &client_tab[nodeid];
    for( i = 0; cfg!=nil; i++) {
        if (config_isatom(cfg)) {
            USRDEBUG("search_clt_ident[%d] line=%d word=%s\n",i,cfg->line, cfg->word); 
            for( j = 0; j < TKN_CLT_MAX; j++) {
                if( !strcmp(cfg->word, cfg_clt_ident[j])) {
                    USRDEBUG("line[%d] MATCH identifier %s\n", cfg->line, cfg->word); 
                    if( cfg->next == nil)
                        fprintf(stderr, "Void value found at line %d\n", cfg->line);
                    cfg = cfg->next;	
                    switch(j){	
                        case TKN_CLT_NODEID:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							if( nodeid != (-1)){
								fprintf(stderr, "nodeid previously defined %d: line %d\n", nodeid, cfg->line);
								return(EXIT_CODE);
							}
							nodeid = atoi(cfg->word);
							USRDEBUG("nodeid=%d\n", nodeid);
							if ((nodeid < 0) || (nodeid >= NR_NODES)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "nodeid:%d, must be > 0 and < NR_NODES(%d)\n", nodeid,NR_NODES);
								return(EXIT_CODE);
							}
							clt_ptr = &client_tab[nodeid];
							clt_ptr->clt_nodeid = nodeid;
							if( client_name == NULL) {
								fprintf(stderr, "client name not defined at line %d\n", cfg->line);
								return(EXIT_CODE);							
							}
							clt_ptr->clt_name = client_name;
							USRDEBUG("clt_nodeid=%d\n", clt_ptr->clt_nodeid);
							SET_BIT(lb.lb_bm_cltparms, TKN_CLT_NODEID);
							break;
                        case TKN_CLT_LBRPORT:
							if( clt_ptr->clt_nodeid == LB_INVALID){
								fprintf(stderr, "Client nodeid must be defined first: line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							clt_ptr->clt_lbRport = atoi(cfg->word);
							USRDEBUG("clt_lbRport=%d\n", clt_ptr->clt_lbRport);

							if ((clt_ptr->clt_lbRport < LBP_BASE_PORT) || (clt_ptr->clt_lbRport >= (LBP_BASE_PORT+NR_NODES))) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "clt_lbRport:%d, must be >= LBP_BASE_PORT(%d) and < (LBP_BASE_PORT+NR_NODES)(%d)\n", 
										clt_ptr->clt_lbRport,LBP_BASE_PORT,(LBP_BASE_PORT+NR_NODES));
								return(EXIT_CODE);
							}
							break;
							SET_BIT(lb.lb_bm_cltparms, TKN_CLT_LBRPORT);
                        case TKN_CLT_CLTRPORT:
							if( clt_ptr->clt_nodeid == LB_INVALID){
								fprintf(stderr, "Client nodeid must be defined first: line %d\n", cfg->line);
								return(EXIT_CODE);
							}						
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							clt_ptr->clt_cltRport = atoi(cfg->word);
							USRDEBUG("clt_cltRport=%d\n", clt_ptr->clt_cltRport);

							if ((clt_ptr->clt_cltRport < LBP_BASE_PORT) || (clt_ptr->clt_cltRport >= (LBP_BASE_PORT+NR_NODES))) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "clt_cltRport:%d, must be >= LBP_BASE_PORT(%d) and < (LBP_BASE_PORT+NR_NODES)(%d)\n", 
										clt_ptr->clt_cltRport,LBP_BASE_PORT,(LBP_BASE_PORT+NR_NODES));
								return(EXIT_CODE);
							}
							SET_BIT(lb.lb_bm_cltparms, TKN_CLT_CLTRPORT);
							break;
                        case TKN_CLT_COMPRESS:
							if( clt_ptr->clt_nodeid == LB_INVALID){
								fprintf(stderr, "Client nodeid must be defined first: line %d\n", cfg->line);
								return(EXIT_CODE);
							}						
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							USRDEBUG("compress=%s\n", cfg->word);					
							if ( strncasecmp(cfg->word,"YES", 3) == 0){
								clt_ptr->clt_compress = YES;
							} else if ( strncasecmp(cfg->word,"NO", 2) == 0) {
								clt_ptr->clt_compress = NO;								
							} else {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "compress: must be YES or NO\n");
								return(EXIT_CODE);
							}
							USRDEBUG("clt_compress=%d\n", clt_ptr->clt_compress);
							SET_BIT(lb.lb_bm_cltparms, TKN_CLT_COMPRESS);							
							break;							
                        case TKN_CLT_BATCH:
							if( clt_ptr->clt_nodeid == LB_INVALID){
								fprintf(stderr, "Client nodeid must be defined first: line %d\n", cfg->line);
								return(EXIT_CODE);
							}						
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							USRDEBUG("batch=%s\n", cfg->word);					
							if ( strncasecmp(cfg->word,"YES", 3) == 0){
								clt_ptr->clt_batch = YES;
							} else if ( strncasecmp(cfg->word,"NO", 2) == 0) {
								clt_ptr->clt_batch = NO;								
							} else {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "batch: must be YES or NO\n");
								return(EXIT_CODE);
							}
							USRDEBUG("clt_batch=%d\n", clt_ptr->clt_batch);
							SET_BIT(lb.lb_bm_cltparms, TKN_CLT_BATCH);							
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
	
    USRDEBUG("\n");
    for ( i = 0; cfg != nil; i++) {
        USRDEBUG("read_svr_lines type=%X\n",cfg->flags); 
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
	
    USRDEBUG("\n");
    for ( i = 0; cfg != nil; i++) {
        USRDEBUG("read_svc_lines type=%X\n",cfg->flags); 
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
    USRDEBUG("\n");
    while ( cfg != nil) {
        USRDEBUG("read_lb_lines type=%X\n",cfg->flags); 
        rcode = search_lb_ident(cfg->list);
		USRDEBUG("\n");
        if( rcode) ERROR_RETURN(rcode);
        if( cfg == nil) return(OK);
        cfg = cfg->next;
    }
    return(OK);
}

int read_clt_lines( config_t *cfg)
{
    int rcode;
    USRDEBUG("\n");
    while ( cfg != nil) {
        USRDEBUG("read_clt_lines type=%X\n",cfg->flags); 
        rcode = search_clt_ident(cfg->list);
		USRDEBUG("\n");
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
	
	lb_name = NULL;
	server_name = NULL;
	client_name = NULL;
	nodeid = (-1);
	
    USRDEBUG("line=%d\n", cfg->line);
    if (cfg != nil) {
        if (config_isatom(cfg)) {
            USRDEBUG("word=%s\n", cfg->word);
            if( !strcmp(cfg->word, "server")) {
                cfg = cfg->next;
                USRDEBUG("server: \n");
                if (cfg != nil) {
					server_name = cfg->word; 
					USRDEBUG("%s\n", server_name);
					name_cfg = cfg;
					cfg = cfg->next;
					if (!config_issub(cfg)) {
						fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n",cfg->word, cfg->line);
						return(EXIT_CODE);
					}
					rcode = read_svr_lines(cfg->list);
					if(rcode) return(EXIT_CODE);
					svr_ptr = &server_tab[nodeid];						
			        USRDEBUG(SERVER_FORMAT, SERVER_FIELDS(svr_ptr));
			        USRDEBUG(SERVER1_FORMAT, SERVER1_FIELDS(svr_ptr));
					lb.lb_nr_svrpxy++;
					for( i = 0; i < TKN_SVR_MAX; i++){
						if( !TEST_BIT(lb.lb_bm_svrparms, i)){
							fprintf( stderr,"CONFIGURATION WARNING: server %s parameter %s not configured\n", 
								server_name, cfg_svr_ident[i]);
						}else{
							USRDEBUG("server %s: %s configured \n", server_name, cfg_svr_ident[i]);
						}
					}
					return(OK);
                }
			} else if( !strcmp(cfg->word, "service")) {
                cfg = cfg->next;
                USRDEBUG("service: \n");
                if (cfg != nil) {
					service_name = cfg->word; 
					USRDEBUG("%s\n", service_name);
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
			        USRDEBUG(SERVICE_FORMAT, SERVICE_FIELDS(svc_ptr));
					lb.lb_nr_services++;
					for( i = 0; i < TKN_SVC_MAX; i++){
						if( !TEST_BIT(lb.lb_bm_svcparms, i)){
							fprintf( stderr,"CONFIGURATION WARNING: service %s parameter %s not configured\n", 
								service_name, cfg_svc_ident[i]);
						}else{
							USRDEBUG("service %s: %s configured \n", service_name, cfg_svc_ident[i]);
						}				
					}						
					return(OK);
                }					
            } else if( !strcmp(cfg->word, "client")) {
                cfg = cfg->next;
                USRDEBUG("client: \n");
                if (cfg != nil) {
					client_name = cfg->word; 
					USRDEBUG("%s\n", client_name);
					name_cfg = cfg;
					cfg = cfg->next;
					if (!config_issub(cfg)) {
						fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n",cfg->word, cfg->line);
						return(EXIT_CODE);
					}
					rcode = read_clt_lines(cfg->list);
					if(rcode) return(EXIT_CODE);
					clt_ptr = &client_tab[nodeid];
			        USRDEBUG(CLIENT_FORMAT, CLIENT_FIELDS(clt_ptr));
					lb.lb_nr_cltpxy++;
					for( i = 0; i < TKN_CLT_MAX; i++){
						if( !TEST_BIT(lb.lb_bm_cltparms, i)){
							fprintf( stderr,"CONFIGURATION WARNING: client %s parameter %s not configured\n", 
								client_name, cfg_clt_ident[i]);
						}else{
							USRDEBUG("client %s: %s configured \n", client_name, cfg_clt_ident[i]);
						}					
					}					
					return(OK);
                }		
            } else if( !strcmp(cfg->word, "lb")) {
                cfg = cfg->next;
                USRDEBUG("lb: \n");
                if (cfg != nil) {
					lb_name = cfg->word; 
					USRDEBUG("%s\n", lb_name);
					cfg = cfg->next;
					if (!config_issub(cfg)) {
						fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n",cfg->word, cfg->line);
						return(EXIT_CODE);
					}
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
    
    USRDEBUG("\n");
    for(int i=0; cfg != nil; i++) {
        if (!config_issub(cfg)) {
            fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n", cfg->word, cfg->line);
            return(EXIT_CODE);
        }
        USRDEBUG("scan_config[%d] line=%d\n",i,cfg->line);
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

	lb.lb_nodeid 	= LOCALNODE;		
	lb.lb_nr_cltpxy	= 0;	
	lb.lb_nr_svrpxy	= 0;	
	lb.lb_nr_services 	= 0;
	lb.lb_lowwater 	= LB_INVALID;	
	lb.lb_highwater	= LB_INVALID;
	lb.lb_period   	= 30;	
	
	
	lb.lb_cltname   = NULL;		
	lb.lb_svrname   = NULL;		
	lb.lb_cltdev    = NULL;		
	lb.lb_svrdev    = NULL;		
	
	lb.lb_bm_lbparms	= 0;	
	lb.lb_bm_svcparms	= 0;
	lb.lb_bm_cltparms	= 0;
	lb.lb_bm_svrparms	= 0;
	
//////////////////////////////////////////////////////////////////////////////////
	for( i = 0; i < NR_NODES; i++){
		svr1_ptr = &server_tab[i];
		clt1_ptr = &client_tab[i];
		clt1_ptr->clt_compress = LB_INVALID;	
		clt1_ptr->clt_batch    = LB_INVALID;
		svr1_ptr->svr_lbRport  = (LBP_BASE_PORT+i);
		svr1_ptr->svr_svrRport = (LBP_BASE_PORT+i);
		clt1_ptr->clt_lbRport  = (LBP_BASE_PORT+i);
		clt1_ptr->clt_cltRport = (LBP_BASE_PORT+i);
	}
//////////////////////////////////////////////////////////////////////////////////
	
    USRDEBUG("BEFORE config_read\n");
    cfg = config_read(f_conf, CFG_ESCAPED, cfg);
    USRDEBUG("AFTER config_read\n");  
    rcode = scan_config(cfg);
	
//    USRDEBUG("lb.lb_nodeid=%d\n",lb.lb_nodeid);  
//    USRDEBUG("lb.lb_nr_svrpxy=%d\n",lb.lb_nr_svrpxy);  
//    USRDEBUG("lb.lb_nr_cltpxy=%d\n",lb.lb_nr_cltpxy);  
//    USRDEBUG("lb.lb_nr_services=%d\n",lb.lb_nr_services);  
//    USRDEBUG("lb.lb_lowwater=%d\n",lb.lb_lowwater);  
//    USRDEBUG("lb.lb_highwater=%d\n",lb.lb_highwater);  

	// check for mandatory parameters
    USRDEBUG("Check for mandatory parameters=%d\n", TKN_LB_MAX);  
	for( i = 0; i < TKN_LB_MAX; i++){
		if( !TEST_BIT(lb.lb_bm_lbparms, i)){
			fprintf( stderr,"CONFIGURATION WARNING: lb parameter %s not configured\n", 
				cfg_lb_ident[i]);
		}else{
			USRDEBUG("lb: %s configured \n", cfg_lb_ident[i]);
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

	if(lb.lb_period < 1 
	|| lb.lb_period > 3600
	) {
        fprintf( stderr,"CONFIGURATION ERROR: load measurement period must be (1-3600)\n");
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
	
	// check SERVER same NODEID
	for( i = 0; i < NR_NODES; i++){
		svr1_ptr = &server_tab[i];
		if( svr1_ptr->svr_nodeid == LB_INVALID) continue;
        USRDEBUG(SERVER_FORMAT, SERVER_FIELDS(svr1_ptr));
        USRDEBUG(SERVER1_FORMAT, SERVER1_FIELDS(svr1_ptr));
		if( i == NR_NODES-1) break;

		if( (TEST_BIT(lb.lb_bm_svrparms, TKN_SVR_START) == 0) 
		&&  (TEST_BIT(lb.lb_bm_svrparms, TKN_SVR_STOP) == 0)
		&&  (TEST_BIT(lb.lb_bm_svrparms, TKN_SVR_IMAGE) != 0) ){
			fprintf(stderr, "Server %s: node_start or node_stop must be defined with node_image\n", svr1_ptr->svr_name);
			exit(1);
		}
							
		if( svr1_ptr->svr_nodeid == lb.lb_nodeid){
			fprintf( stderr,"CONFIGURATION ERROR:Server %s have the same nodeid as load balancer\n",
				svr1_ptr->svr_name);
			exit(1);
		}
		for( j = i+1; j < NR_NODES; j++){
			svr2_ptr = &server_tab[j];
			if( svr2_ptr->svr_nodeid == LB_INVALID) continue;
			if( svr1_ptr->svr_nodeid == svr2_ptr->svr_nodeid ){
				fprintf( stderr,"CONFIGURATION ERROR:Servers %s and %s have the same nodeid\n",
					svr1_ptr->svr_name, svr2_ptr->svr_name);
				exit(1);
			}
		}
		for( j = 0; j < NR_NODES; j++){
			clt1_ptr = &client_tab[j];
			if( clt1_ptr->clt_nodeid == LB_INVALID) continue;
			if( svr1_ptr->svr_nodeid == clt1_ptr->clt_nodeid ){
				fprintf( stderr,"CONFIGURATION ERROR:Server %s and Client %s have the same nodeid\n",
					svr1_ptr->svr_name, clt1_ptr->clt_name);
				exit(1);
			}
		}		
	}

	// check CLIENT same NODEID
	for( i = 0; i < NR_NODES-1; i++){
		clt1_ptr = &client_tab[i];
		if( clt1_ptr->clt_nodeid == LB_INVALID) continue;
        USRDEBUG(CLIENT_FORMAT, CLIENT_FIELDS(clt1_ptr));
		if( i == NR_NODES-1) break;

		if( clt1_ptr->clt_nodeid == lb.lb_nodeid){
			fprintf( stderr,"CONFIGURATION ERROR:Client %s have the same nodeid as load balancer\n",
				clt1_ptr->clt_name);
			exit(1);
		}
		for( j = i+1; j < NR_NODES; j++){
			clt2_ptr = &client_tab[j];
			if( clt2_ptr->clt_nodeid == (-1)) continue;
			if( clt1_ptr->clt_nodeid == clt2_ptr->clt_nodeid ){
				fprintf( stderr,"CONFIGURATION ERROR:Clients %s and %s have the same nodeid\n",
					clt1_ptr->clt_name, clt2_ptr->clt_name);
				exit(1);
			}
		}
		for( j = 0; j < NR_NODES; j++){
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

		if( svc1_ptr->svc_bind == LB_INVALID){
				svc1_ptr->svc_bind  = PROG_BIND;
				svc1_ptr->svc_prog = nonprog;
		}

		if( svc1_ptr->svc_prog == NULL){
				svc1_ptr->svc_prog = nonprog;
				svc1_ptr->svc_bind  = PROG_BIND;				
		}
		
        USRDEBUG(SERVICE_FORMAT, SERVICE_FIELDS(svc1_ptr));
				
		if( i == lb.lb_nr_services-1) break;
		
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
	}

//    USRDEBUG("lb.lb_cltname=%s\n",lb.lb_cltname);
//    USRDEBUG("lb.lb_svrname=%s\n",lb.lb_svrname);
//    USRDEBUG("lb.lb_cltdev=%s\n",lb.lb_cltdev);
//    USRDEBUG("lb.lb_svrdev=%s\n",lb.lb_svrdev);
	
	lb_ptr = &lb;
    USRDEBUG(LB1_FORMAT, LB1_FIELDS(lb_ptr));
    USRDEBUG(LB2_FORMAT, LB2_FIELDS(lb_ptr));
    USRDEBUG(LB3_FORMAT, LB3_FIELDS(lb_ptr));
	
}

