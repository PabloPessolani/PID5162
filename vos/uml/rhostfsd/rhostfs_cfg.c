/***********************************************
	RHS CONFIGURATION FILE
*  Sample File format
# this is a comment 
rhs SERVER1 {
	dcid			0;
	rhs_ep		0;
	rhs_dir		"/xxx/yyyy"
};
**************************************************/

#include "rhs.h"
#include "rhostfs_glo.h"

int rhs_mandatory = 0; 

#define EXIT_CODE		1
#define NEXT_CODE		2

#define TKN_DCID		0
#define TKN_RHS_EP		1
#define TKN_RHS_DIR		2
#define NR_IDENT		3

char *rhs_cfg_ident[] = {
	"dcid",
	"rhs_ep",
	"rhs_dir",
};

int rhs_search_ident(config_t *cfg)
{
	int ident_nr;

	RHSDEBUG("rhs_mandatory=%X cfg=%p\n", rhs_mandatory, cfg);

	while(cfg!=nil) {
		RHSDEBUG("cfg->line=%d\n",cfg->line); 
		if (config_isatom(cfg)) {
			RHSDEBUG("line=%d word=%s\n",cfg->line, cfg->word); 
			for( ident_nr = 0; ident_nr < NR_IDENT; ident_nr++) {	
//				RHSDEBUG("ident_nr=%d word=>%s< ident=>%s<\n",ident_nr, cfg->word, rhs_cfg_ident[ident_nr]); 
				if( !strcmp(cfg->word, rhs_cfg_ident[ident_nr])) {
					if( cfg->next == nil)
						fprintf(stderr, "Void value found at line %d\n", cfg->line);
					cfg = cfg->next;
					if( (ident_nr != TKN_DCID) && (TEST_BIT(rhs_mandatory, TKN_DCID))){
							fprintf(stderr, "dcid must be the first parameter set: %d\n", cfg->line);
							ERROR_RETURN(EXIT_CODE);
					}
					switch(ident_nr){
						case TKN_DCID:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								ERROR_RETURN(EXIT_CODE);
							}
							RHSDEBUG("\t TKN_DCID=%d\n", atoi(cfg->word));
							CLR_BIT(rhs_mandatory, TKN_DCID);					
							dcid =  atoi(cfg->word);
							if(dcid  < 0 || dcid >= dvs_ptr->d_nr_dcs)
								ERROR_RETURN(EXIT_CODE);	
							RHSDEBUG("dcid=%d\n", dcid);
							get_dc_params(dcid);							
							break;
						case TKN_RHS_EP:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								ERROR_RETURN(EXIT_CODE);
							}
							RHSDEBUG("\t TKN_RHS_EP=%d\n", atoi(cfg->word));
							CLR_BIT(rhs_mandatory, TKN_RHS_EP);					
							rhs_ep =  atoi(cfg->word);
							if( rhs_ep < 0 || rhs_ep >= (dc_ptr->dc_nr_sysprocs - dc_ptr->dc_nr_tasks))
								ERROR_RETURN(EXIT_CODE);						
							break;
						case TKN_RHS_DIR:
							if (!config_isstring(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								ERROR_RETURN(EXIT_CODE);						
							}
							rhs_dir = cfg->word;
							RHSDEBUG("rhs_dir=%s\n", rhs_dir);
							CLR_BIT(rhs_mandatory, TKN_RHS_DIR);					
							break;
						default:
							fprintf(stderr, "Programming Error\n");
							ERROR_PT_EXIT(ident_nr);
					}
					return(OK);
				}	
			}
			fprintf(stderr, "Invalid identifier found at line %d\n", cfg->line);
			ERROR_RETURN(EXIT_CODE);
		}		
		cfg = cfg->next;
	}
	RHSDEBUG("return OK\n");

	return(OK);	
}
		
int rhs_read_lines(config_t *cfg)
{
	int i;
	int rcode;
	RHSDEBUG("\n");
	for ( i = 0; cfg != nil; i++) {
		// printf("rhs_read_lines typedef 				=%X\n",cfg->flags); 
		rcode = rhs_search_ident(cfg->list);
		if( rcode) ERROR_RETURN(rcode);
		if( cfg == nil) return(OK);
		cfg = cfg->next;
	}
	return(OK);
}	

int rhs_search_token(config_t *cfg)
{
	int rcode;
	
	RHSDEBUG("\n");

    if (cfg != nil) {
		if (config_isatom(cfg)) {
			if( !strcmp(cfg->word, "rhs")) {		
				cfg = cfg->next;
				if (cfg != nil) {
					if (config_isatom(cfg)) {
						RHSDEBUG("rhs: %s\n", cfg->word);
						rhs_name = cfg->word;
						cfg = cfg->next;
						if (!config_issub(cfg)) {
							fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n",cfg->word, cfg->line);
							return(EXIT_CODE);
						}
						rcode = rhs_read_lines(cfg->list);
						return(rcode);
					}
				}
				if( rhs_mandatory != 0){
					fprintf(stderr, "Configuration Error rhs_mandatory=0x%X\n", rhs_mandatory);
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

/*Searchs for every server configuration and stores in rhs_web_child server table */
int rhs_search_config(config_t *cfg)
{
	int rcode;
	int i;
	
	RHSDEBUG("\n");

	SET_BIT(rhs_mandatory, TKN_DCID);
	SET_BIT(rhs_mandatory, TKN_RHS_EP);
	SET_BIT(rhs_mandatory, TKN_RHS_DIR);

    for( i=0; cfg != nil; i++) {
		if (!config_issub(cfg)) {
			fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n", cfg->word, cfg->line);
			return(EXIT_CODE);
		}
		rcode = rhs_search_token(cfg->list);
		if( rcode ) ERROR_RETURN(rcode);
		cfg= cfg->next;
	}
	return(OK);
}

int rhs_read_config(char *file_conf)
{
    config_t *cfg;
	int rcode;
	
	#define nil ((void*)0)
	cfg= nil;
	RHSDEBUG("file_conf=%s\n", file_conf);
	cfg = config_read(file_conf, CFG_ESCAPED, cfg);
	RHSDEBUG("before  rhs_search_config\n");	
	rcode = rhs_search_config(cfg);
	if(rcode) ERROR_RETURN(rcode);
	return(rcode );	
}

