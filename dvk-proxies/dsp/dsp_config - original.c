/***********************************************
DSP CONFIGURATION FILE
# this is a comment 

proxy node1 {
	proxyID		1;
	proto			tcp 
	rname		ANY;
	rport			3001;
	sport			3000; 
	compress		YES; 
	batch			YES;
	autobind		YES;	
};

proxy node2 {
	proxyID		2;
	proto			tcp 
	rname		client0;
	rport			3002;
	sport			3000; 
	compress		NO; 
	batch			NO;
	autobind		NO;	
};
**************************************************/
#define _GNU_SOURCE     
#define _MULTI_THREADED

#include "../proxy.h"
#include "dsp_proxy.h"
#include "dsp_glo.h"
#include "../include/generic/configfile.h"
extern char *ctl_socket;	

#define OK				0
#define EXIT_CODE		1
#define NEXT_CODE		2

#define MAX_FLAG_LEN 	30
#define MAXTOKENSIZE	20

#define	TKN_NODE_NODEID	0
#define TKN_NODE_MAX 	1 // MUST be the last

#define TKN_PXY_PROXYID	0
#define TKN_PXY_PROTO	1
#define TKN_PXY_RPORT	2
#define TKN_PXY_SPORT	3
#define TKN_PXY_COMPRESS 4
#define TKN_PXY_BATCH	5
#define TKN_PXY_AUTOBIND 6 
#define TKN_PXY_RNAME 	 7
#define TKN_PXY_MAX		 8 // MUST be the last

#define nil ((void*)0)

char *cfg_proxy_ident[] = {
    "proxyid",
    "proto",
    "rport",
    "sport",
	"compress",
    "batch",
    "autobind",
    "rname",
};

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

int search_proxy_ident(config_t *cfg)
{
    int i, j, rcode;
    proxy_t *px_ptr;
	
	px_ptr = &proxy_tab[dsp_ptr->dsp_nr_proxies];				
    for( i = 0; cfg!=nil; i++) {
        if (config_isatom(cfg)) {
            PXYDEBUG("search_proxy_ident[%d] line=%d word=%s\n",i,cfg->line, cfg->word); 
            for( j = 0; j < TKN_PXY_MAX; j++) {
                if( !strcmp(cfg->word, cfg_proxy_ident[j])) {
                    PXYDEBUG("line[%d] MATCH identifier %s\n", cfg->line, cfg->word); 
                    if( cfg->next == nil)
                        fprintf(stderr, "Void value found at line %d\n", cfg->line);
                    cfg = cfg->next;	
                    switch(j){		
                        case TKN_PXY_PROXYID:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}	
							px_ptr->px_proxyid =atoi(cfg->word);		
							PXYDEBUG("px_proxyid=%d\n", px_ptr->px_proxyid);
							SET_BIT(dsp_ptr->dsp_param_bm, TKN_PXY_PROXYID);
							break;
                        case TKN_PXY_PROTO:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							PXYDEBUG("proto=%s\n", cfg->word);							
							if( strncasecmp(cfg->word, "tcp",3) == 0) 
								px_ptr->px_proto = PX_PROTO_TCP;
							else if ( strncasecmp(cfg->word, "udp", 3) == 0)
								px_ptr->px_proto = PX_PROTO_UDP;
							else if ( strncasecmp(cfg->word, "tipc", 4) == 0)
								px_ptr->px_proto = PX_PROTO_TIPC;
							else {
								fprintf(stderr, "Configuration Error\n");
								exit(1);
							}		
							PXYDEBUG("px_proto=%X\n", px_ptr->px_proto);
							SET_BIT(dsp_ptr->dsp_param_bm, TKN_PXY_PROTO);
							break;
                        case TKN_PXY_RPORT:
							if( TEST_BIT(dsp_ptr->dsp_param_bm, TKN_PXY_PROTO) == 0){
								fprintf(stderr, "rport: \"proto\" must be defined before \"rport\"\n");
								return(EXIT_CODE);								
							}
							if( px_ptr->px_proto != PX_PROTO_TCP && px_ptr->px_proto != PX_PROTO_UDP) {
								fprintf(stderr, "rport: \"proto\" must be set to tcp or udp\n");
								return(EXIT_CODE);
							}	
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							px_ptr->px_rport = atoi(cfg->word);
							PXYDEBUG("px_rport=%d\n", px_ptr->px_rport);

							if ((px_ptr->px_rport < BASE_PORT) || (px_ptr->px_rport >= (BASE_PORT+dvs.d_nr_nodes))) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "rport:%d, must be >= BASE_PORT(%d) and < (BASE_PORT+dvs.d_nr_nodes)(%d)\n", 
										px_ptr->px_rport,BASE_PORT,(BASE_PORT+dvs.d_nr_nodes));
								return(EXIT_CODE);
							}
							SET_BIT(dsp_ptr->dsp_param_bm, TKN_PXY_RPORT);
							break;
                        case TKN_PXY_SPORT:
							if( TEST_BIT(dsp_ptr->dsp_param_bm, TKN_PXY_PROTO) == 0){
								fprintf(stderr, "sport: \"proto\" must be defined before \"sport\"\n");
								return(EXIT_CODE);								
							}	
							if( px_ptr->px_proto != PX_PROTO_TCP && px_ptr->px_proto != PX_PROTO_UDP) {
								fprintf(stderr, "sport: \"proto\" must be set to tcp or udp\n");
								return(EXIT_CODE);
							}							
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							px_ptr->px_sport = atoi(cfg->word);
							PXYDEBUG("px_sport=%d\n", px_ptr->px_sport);
							if ((px_ptr->px_sport < BASE_PORT) || (px_ptr->px_sport >= (BASE_PORT+dvs.d_nr_nodes))) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "sport:%d, must be >= BASE_PORT(%d) and < (BASE_PORT+dvs.d_nr_nodes)(%d)\n", 
										px_ptr->px_sport,BASE_PORT,(BASE_PORT+dvs.d_nr_nodes));
								return(EXIT_CODE);
							}
							SET_BIT(dsp_ptr->dsp_param_bm, TKN_PXY_SPORT);
							break;
                        case TKN_PXY_COMPRESS:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							PXYDEBUG("compress=%s\n", cfg->word);					
							if ( strncasecmp(cfg->word,"YES", 3) == 0){
								px_ptr->px_compress = YES;
							} else if ( strncasecmp(cfg->word,"NO", 2) == 0) {
								px_ptr->px_compress = NO;								
							} else {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "compress: must be YES or NO\n");
								return(EXIT_CODE);
							}
							PXYDEBUG("px_compress=%d\n", px_ptr->px_compress);
							SET_BIT(dsp_ptr->dsp_param_bm, TKN_PXY_COMPRESS);							
							break;
						case TKN_PXY_BATCH:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							PXYDEBUG("batch=%s\n", cfg->word);					
							if ( strncasecmp(cfg->word,"YES", 3) == 0){
								px_ptr->px_batch = YES;
							} else if ( strncasecmp(cfg->word,"NO", 2) == 0) {
								px_ptr->px_batch = NO;								
							} else {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "batch: must be YES or NO\n");
								return(EXIT_CODE);
							}
							PXYDEBUG("px_batch=%d\n", px_ptr->px_batch);
							SET_BIT(dsp_ptr->dsp_param_bm, TKN_PXY_BATCH);														
							break;
						case TKN_PXY_AUTOBIND:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							if ( strncasecmp(cfg->word,"YES", 3) == 0){
								px_ptr->px_autobind = YES;
							} else if ( strncasecmp(cfg->word,"NO", 2) == 0) {
								px_ptr->px_autobind = NO;								
							} else {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "autobind: must be YES or NO\n");
								return(EXIT_CODE);
							}
							PXYDEBUG("px_autobind=%d\n", px_ptr->px_autobind);
							SET_BIT(dsp_ptr->dsp_param_bm, TKN_PXY_AUTOBIND);														
							break;
                        case TKN_PXY_RNAME:
							if( TEST_BIT(dsp_ptr->dsp_param_bm, TKN_PXY_PROTO) == 0){
								fprintf(stderr, "rname: \"proto\" must be defined before \"rname\"\n");
								return(EXIT_CODE);								
							}
							if( px_ptr->px_proto != PX_PROTO_TCP && px_ptr->px_proto != PX_PROTO_UDP) {
								fprintf(stderr, "rname: \"proto\" must be set to tcp or udp\n");
								return(EXIT_CODE);
							}	
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							px_ptr->px_rname = cfg->word;
							PXYDEBUG("px_rname=%s\n", px_ptr->px_rname);
							
							if( !strncmp(px_ptr->px_rname,"ANY",3)) {
								if( !valid_ipaddr(px_ptr->px_rname)){
									fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
									fprintf(stderr, "rname:%s, must be \"ANY\" or a valid local host name\n", 
											px_ptr->px_rname);
									return(EXIT_CODE);
								} 
							}
							SET_BIT(dsp_ptr->dsp_param_bm, TKN_PXY_RNAME);
							break;							
						default:
							fprintf(stderr, "Programming Error\n");
							exit(1);
                    }
                    return(OK);
                }	
            }
            if( j == TKN_PXY_MAX)
                fprintf(stderr, "Invalid identifier found at line %d\n", cfg->line);
        }
        cfg = cfg->next;
    }
    return(OK);
}

int read_proxy_lines( config_t *cfg)
{
    int i;
    int rcode;
	
    PXYDEBUG("\n");
    for ( i = 0; cfg != nil; i++) {
        PXYDEBUG("read_proxy_lines type=%X\n",cfg->flags); 
        rcode = search_proxy_ident(cfg->list);
        if( rcode) ERROR_RETURN(rcode);
        if( cfg == nil)return(OK);
        cfg = cfg->next;
    }
    return(OK);
}	

int search_main_token(config_t *cfg)
{
    int rcode, i;
    config_t *name_cfg;
	proxy_t *px_ptr;
	char *proxy_name;

    PXYDEBUG("line=%d dsp_ptr->dsp_nr_proxies=%d\n", cfg->line, dsp_ptr->dsp_nr_proxies);
	px_ptr = &proxy_tab[dsp_ptr->dsp_nr_proxies];
    if (cfg != nil) {
        if (config_isatom(cfg)) {
            PXYDEBUG("word=%s\n", cfg->word);
            if( !strcmp(cfg->word, "proxy")) {
                cfg = cfg->next;
                PXYDEBUG("proxy: \n");
                if (cfg != nil) {
					px_ptr->px_name = cfg->word; 
					PXYDEBUG("%s\n", px_ptr->px_name);
					name_cfg = cfg;
					cfg = cfg->next;
					if (!config_issub(cfg)) {
						fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n",cfg->word, cfg->line);
						return(EXIT_CODE);
					}
					dsp_ptr->dsp_param_bm = 0;
					rcode = read_proxy_lines(cfg->list);
					dsp_ptr->dsp_nr_proxies++;
					if (dsp_ptr->dsp_nr_proxies > (dvs.d_nr_nodes-1)) {
						fprintf(stderr, "Config error: Number of proxies %d > (dvs.d_nr_nodes-1)\n",dsp_ptr->dsp_nr_proxies);
						return(EXIT_CODE);
					}
					if(rcode) return(EXIT_CODE);
			        PXYDEBUG(PROXY_FORMAT, PROXY_FIELDS(px_ptr));
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
 *				dsp_config				     *
 *===========================================================================*/
void dsp_config(char *f_conf)	/* config file name. */
{
    /* Main program of lb_config. */
    config_t *cfg;
    int rcode, i, j;
	proxy_t *px_ptr;
	proxy_t *px2_ptr;

    cfg = nil;
    rcode  = OK;

    PXYDEBUG("BEFORE config_read\n");
	dsp_ptr->dsp_nr_proxies = 0;
	dsp_ptr->dsp_compress_opt = NO;
	dsp_ptr->dsp_lowwater	= PX_INVALID;
	dsp_ptr->dsp_highwater	= PX_INVALID;
	dsp_ptr->dsp_period		= MP_DFT_PERIOD;
				
	for( i = 0; i < (dvs.d_nr_nodes-1); i++){
		px_ptr = &proxy_tab[i];
		px_ptr->px_proxyid  = PX_INVALID;
		px_ptr->px_proto    = PX_INVALID; 
		px_ptr->px_rport    = PX_INVALID; 
		px_ptr->px_sport    = PX_INVALID; 
		px_ptr->px_compress = PX_INVALID; 
		px_ptr->px_batch    = PX_INVALID; 
		px_ptr->px_autobind = PX_INVALID; 
		px_ptr->px_rname    = NULL; 
		
	}
	
    cfg = config_read(f_conf, CFG_ESCAPED, cfg);
    PXYDEBUG("AFTER config_read\n");  
    rcode = scan_config(cfg);
	if (rcode != 0) ERROR_EXIT(rcode);
	
	for( i = 0; i < (dvs.d_nr_nodes-1); i++){
		px_ptr = &proxy_tab[i];
		if( px_ptr->px_proxyid == PX_INVALID) continue;
        PXYDEBUG(PROXY_FORMAT, PROXY_FIELDS(px_ptr));
		
		if( px_ptr->px_proxyid == dsp_ptr->dsp_nodeid){
			fprintf( stderr,"CONFIGURATION ERROR:ProxyID %s have the same nodeid as local node %d\n",
				px_ptr->px_name, dsp_ptr->dsp_nodeid);
			exit(1);
		}

		if( px_ptr->px_proto == PX_INVALID){
			fprintf( stderr,"CONFIGURATION ERROR: proxy %s: proto must be defined\n",
				px_ptr->px_name);
			exit(1);
		}
	
		if( px_ptr->px_proto == PX_PROTO_TCP 
		|| px_ptr->px_proto == PX_PROTO_UDP) {

			if( px_ptr->px_rport == PX_INVALID){
				fprintf( stderr,"CONFIGURATION ERROR: proxy %s: rport must be defined\n",
					px_ptr->px_name);
				exit(1);
			}

			if( px_ptr->px_sport == PX_INVALID){
				fprintf( stderr,"CONFIGURATION ERROR: proxy %s: sport must be defined\n",
					px_ptr->px_name);
				exit(1);
			}
			
			if( px_ptr->px_rname == NULL){
				fprintf( stderr,"CONFIGURATION ERROR: proxy %s: rname must be defined\n",
					px_ptr->px_name);
				exit(1);
			}	
		}
		
		if( px_ptr->px_compress == PX_INVALID){
				fprintf( stderr,"WARNING: proxy %s: message compress undefined, it is set to YES\n",
					px_ptr->px_name);
				px_ptr->px_compress = YES;		
				exit(1);
		}

		if( px_ptr->px_batch == PX_INVALID){
				fprintf( stderr,"WARNING: proxy %s: message batch undefined, it is set to YES\n",
					px_ptr->px_name);
				px_ptr->px_batch = YES;		
				exit(1);
		}

		if( px_ptr->px_autobind == PX_INVALID){
				fprintf( stderr,"WARNING: proxy %s: message compress undefined, it is set to NO\n",
					px_ptr->px_name);
				px_ptr->px_autobind = NO;		
				exit(1);
		}
		
		if( px_ptr->px_compress  == YES)
			dsp_ptr->dsp_compress_opt = YES;
	
		if( i == (dvs.d_nr_nodes-2)) break;
		
		for( j = i+1; j < (dvs.d_nr_nodes-1); j++){
			px2_ptr = &proxy_tab[j];
			if( px2_ptr->px_proxyid == PX_INVALID) continue;
			if( px_ptr->px_proxyid == px2_ptr->px_proxyid ){
				fprintf( stderr,"CONFIGURATION ERROR:proxy %s and proxy %s have the same nodeid\n",
					px_ptr->px_name, px2_ptr->px_name);
				exit(1);
			}
			if( strcasecmp(px_ptr->px_name, px2_ptr->px_name) == 0){
				fprintf( stderr,"CONFIGURATION ERROR:proxy %s name repeated\n", px_ptr->px_name);
				exit(1);
			}
		}		
	}
	
    PXYDEBUG(DSP0_FORMAT, DSP0_FIELDS(dsp_ptr));
    PXYDEBUG(DSP1_FORMAT, DSP1_FIELDS(dsp_ptr));
    PXYDEBUG(DSP1_FORMAT, DSP2_FIELDS(dsp_ptr));
    PXYDEBUG(DSP2_FORMAT, DSP3_FIELDS(dsp_ptr));

}

