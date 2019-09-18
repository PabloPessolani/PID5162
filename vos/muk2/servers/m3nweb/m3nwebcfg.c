/***********************************************
	TEST CONFIGURATION FILE
*  Sample File format
# this is a comment 
websrv SERVER1 {
	port			80;
	endpoint		20;
	rootdir		"/websrv/server1";
	ipaddr		"192.168.1.100";
};
**************************************************/

#define DVKDBG		1

#include "m3nweb.h"
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"


#define NR_IDENT 		3
#define TKN_PORT		0
#define TKN_ENDPOINT	1
#define TKN_ROOTDIR		2


char *nw_cfg_ident[] = {
	"port",
	"endpoint",
	"rootdir",
};

int nw_search_ident(config_t *cfg)
{
	int ident_nr;

	MUKDEBUG("nw_mandatory=%X cfg=%p\n", nw_mandatory, cfg);

	while(cfg!=nil) {
		MUKDEBUG("cfg->line=%d\n",cfg->line); 
		if (config_isatom(cfg)) {
			MUKDEBUG("line=%d word=%s\n",cfg->line, cfg->word); 
			for( ident_nr = 0; ident_nr < NR_IDENT; ident_nr++) {	
//				MUKDEBUG("ident_nr=%d word=>%s< ident=>%s<\n",ident_nr, cfg->word, nw_cfg_ident[ident_nr]); 
				if( !strcmp(cfg->word, nw_cfg_ident[ident_nr])) {
					if( cfg->next == nil)
						fprintf(stderr, "Void value found at line %d\n", cfg->line);
					cfg = cfg->next;				
					switch(ident_nr){
						case TKN_ROOTDIR:
							if (!config_isstring(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								ERROR_RETURN(EXIT_CODE);
							}
							MUKDEBUG("\tROOTDIR=%s\n", cfg->word);
							CLR_BIT(nw_mandatory, TKN_ROOTDIR);
							web_rootdir = cfg->word;		
							break;
						case TKN_ENDPOINT:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								ERROR_RETURN(EXIT_CODE);
							}
							MUKDEBUG("\tENDPOINT=%d\n", atoi(cfg->word));
							CLR_BIT(nw_mandatory, TKN_ENDPOINT);					
							if( web_ep != atoi(cfg->word)){
									fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
									fprintf(stderr, "endpoint (%d), must be equal to web_ep (%d)\n", atoi(cfg->word),web_ep);
									ERROR_RETURN(EXIT_CODE);						
							}
							break;
						case TKN_PORT:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								ERROR_RETURN(EXIT_CODE);
							}
							MUKDEBUG("\tPORT=%d htons(PORT)=%d\n", atoi(cfg->word),htons((atoi(cfg->word))));
							CLR_BIT(nw_mandatory, TKN_PORT);
							web_port = atoi(cfg->word);
							nw_svr_addr.sin_port = htons(web_port);
							if( web_port < 0 || web_port >60000)
								ERROR_RETURN(EXIT_CODE);
							break;																			
						default:
							fprintf(stderr, "Programming Error\n");
							ERROR_TSK_EXIT(ident_nr);
					}
					return(OK);
				}	
			}
			fprintf(stderr, "Invalid identifier found at line %d\n", cfg->line);
			ERROR_RETURN(EXIT_CODE);
		}		
		cfg = cfg->next;
	}
	MUKDEBUG("return OK\n");

	return(OK);	
}
		
int nw_read_lines(config_t *cfg)
{
	int i;
	int rcode;
	MUKDEBUG("\n");
	for ( i = 0; cfg != nil; i++) {
		// printf("nw_read_lines typedef 				=%X\n",cfg->flags); 
		rcode = nw_search_ident(cfg->list);
		if( rcode) ERROR_RETURN(rcode);
		if( cfg == nil) return(OK);
		cfg = cfg->next;
	}
	return(OK);
}	

int nw_search_token(config_t *cfg)
{
	int rcode;
	
	MUKDEBUG("\n");

    if (cfg != nil) {
		if (config_isatom(cfg)) {
			if( !strcmp(cfg->word, "websrv")) {		
				nw_mandatory =((1 << TKN_PORT) |
							(1 << TKN_ENDPOINT) |
							(1 << TKN_ROOTDIR));
				cfg = cfg->next;
				if (cfg != nil) {
					if (config_isatom(cfg)) {
						MUKDEBUG("websrv: %s\n", cfg->word);
						web_name = cfg->word;
						cfg = cfg->next;
						if (!config_issub(cfg)) {
							fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n",cfg->word, cfg->line);
							return(EXIT_CODE);
						}
						rcode = nw_read_lines(cfg->list);
						return(rcode);
					}
				}
				if( nw_mandatory != 0){
					fprintf(stderr, "Configuration Error nw_mandatory=0x%X\n", nw_mandatory);
					return(EXIT_CODE);
				}				
			}
			fprintf(stderr, "Config error line:%d No websrv token found\n", cfg->line);
			return(EXIT_CODE);
		}
		fprintf(stderr, "Config error line:%d No websrv name found \n", cfg->line);
		return(EXIT_CODE);
	}
	return(EXIT_CODE);
}

/*Searchs for every server configuration and stores in nw_web_child server table */
int nw_search_config(config_t *cfg)
{
	int rcode;
	int i;
	
	MUKDEBUG("\n");

    for( i=0; cfg != nil; i++) {
		if (!config_issub(cfg)) {
			fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n", cfg->word, cfg->line);
			return(EXIT_CODE);
		}
		rcode = nw_search_token(cfg->list);
		if( rcode ) ERROR_RETURN(rcode);
		cfg= cfg->next;
	}
	return(OK);
}

int nw_read_config(char *file_conf)
{
    config_t *cfg;
	int rcode;
	
	#define nil ((void*)0)
	cfg= nil;
	MUKDEBUG("file_conf=%s\n", file_conf);
	cfg = config_read(file_conf, CFG_ESCAPED, cfg);
	MUKDEBUG("before  nw_search_config\n");	
	rcode = nw_search_config(cfg);
	if(rcode) ERROR_RETURN(rcode);
	return(rcode );	
}

