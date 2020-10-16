/***********************************************
UML DVS SWITCH CONFIGURATION FILE
 
# this is a comment 
switch SW_NAME {
	path 	"my-control-socket";
	hub		NO;
	daemon	NO;
	dcid	0;
	tap    "TAP9";				<<<< LOCAL TAP interface of the LOCAL  switch
};

rmttap LTAP0 {
	tap		"TAP0";			
	nodeid 	0;				<<<< Local TAP interface of a REMOTE switch  
};

rmttap LTAP1 {
	tap		"TAP1";
	nodeid 	0;
};

rmttap RTAP1 {
	tap		"TAP0";
	nodeid 	1;				<<<< Remote TAP interface of the LOCAL switch  
};

rmttap RTAP2 {
	tap		"TAP0";
	nodeid 	2;
};

**************************************************/
#define _GNU_SOURCE     
#define _MULTI_THREADED

#include "switch.h"
#include "../../include/generic/configfile.h"
extern char *ctl_socket;	

#define OK				0
#define EXIT_CODE		1
#define NEXT_CODE		2

#define MAX_FLAG_LEN 	30
#define MAXTOKENSIZE	20

#define	TKN_SW_PATH		0
#define TKN_SW_DCID		1
#define TKN_SW_HUB		2
#define TKN_SW_DAEMON   3
#define TKN_SW_TAP 		4
#define TKN_SW_MAX 		5 // MUST be the last

#define TKN_RT_TAP 		0
#define TKN_RT_NODEID	1
#define TKN_RT_MAX 		2	// MUST be the last

#define nil ((void*)0)

char *cfg_sw_ident[] = {
    "path",
    "dcid",
    "hub",
	"daemon",
    "tap",
};

char *cfg_rt_ident[] = {
	"tap",
    "nodeid",
};

int search_sw_ident(config_t *cfg)
{
    int i, j, rcode, dcid;
    
    USRDEBUG("\n");
    for( i = 0; cfg!=nil; i++) {
        if (config_isatom(cfg)) {
            USRDEBUG("search_sw_ident[%d] line=%d word=%s\n",i,cfg->line, cfg->word); 
            for( j = 0; j < TKN_SW_MAX; j++) {
                if( !strcmp(cfg->word, cfg_sw_ident[j])) {
                    USRDEBUG("line[%d] MATCH identifier %s\n", cfg->line, cfg->word); 
                    if( cfg->next == nil)
                        fprintf(stderr, "Void value found at line %d\n", cfg->line);
                    cfg = cfg->next;	
                    switch(j){		
                        case TKN_SW_PATH:
							if (!config_isstring(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							USRDEBUG("path=%s\n", cfg->word);						
							sw_ptr->sw_ctrl_path=(cfg->word);
							USRDEBUG("sw_ptr->sw_ctrl_path=%s\n", sw_ptr->sw_ctrl_path);
							break;					
                        case TKN_SW_DCID:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							USRDEBUG("dcid=%d\n", atoi(cfg->word));
							dcid=atoi(cfg->word);							
							if ((dcid < 0) || (dcid >= NR_DCS)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "DCID:%d, must be > 0 and < NR_DCS(%d)\n", dcid,NR_DCS);
								dcid = -1;
							}
							sw_ptr->sw_dcid = dcid;
							USRDEBUG("sw_ptr->sw_dcid=%d\n", sw_ptr->sw_dcid);
							break;
						case TKN_SW_HUB:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							if( strncmp(cfg->word, "YES", 3) == 0) 
								sw_ptr->sw_hub = YES;
							else if ( strncmp(cfg->word, "NO", 2) == 0)
								sw_ptr->sw_hub = NO;
							else {
								fprintf(stderr, "Configuration Error\n");
								exit(1);
							}		
							USRDEBUG("sw_ptr->sw_hub=%d\n", sw_ptr->sw_hub);
							break;
						case TKN_SW_DAEMON:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							if( strncmp(cfg->word, "YES", 3) == 0) 
								sw_ptr->sw_daemon = YES;
							else if ( strncmp(cfg->word, "NO", 2) == 0)
								sw_ptr->sw_daemon = NO;
							else {
								fprintf(stderr, "Configuration Error\n");
								exit(1);
							}		
							USRDEBUG("sw_ptr->sw_daemon=%d\n", sw_ptr->sw_daemon);
							break;
						case TKN_SW_TAP:
							if (!config_isstring(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							USRDEBUG("tap=%s\n", cfg->word);						
							sw_ptr->sw_tap=(cfg->word);
							USRDEBUG("sw_ptr->sw_tap=%s\n", sw_ptr->sw_tap);
							break;							
                        default:
							fprintf(stderr, "Programming Error\n");
							exit(1);
                    }
                    return(OK);
                }	
            }
            if( j == TKN_SW_MAX)
                fprintf(stderr, "Invaild identifier found at line %d\n", cfg->line);
        }
        //		USRDEBUG("próximo cfg\n");
        cfg = cfg->next;
    }
    return(OK);
}

int search_rt_ident(config_t *cfg)
{
    int i, j, rcode;
    
    USRDEBUG("\n");
    for( i = 0; cfg!=nil; i++) {
        if (config_isatom(cfg)) {
            USRDEBUG("search_rt_ident[%d] line=%d word=%s\n",i,cfg->line, cfg->word); 
            for( j = 0; j < TKN_RT_MAX; j++) {
                if( !strcmp(cfg->word, cfg_rt_ident[j])) {
                    USRDEBUG("line[%d] MATCH identifier %s\n", cfg->line, cfg->word); 
                    if( cfg->next == nil)
                        fprintf(stderr, "Void value found at line %d\n", cfg->line);
                    cfg = cfg->next;	
                    switch(j){	
                        case TKN_RT_NODEID:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							rt_ptr->rt_nodeid = atoi(cfg->word);
							USRDEBUG("nodeid=%d\n", atoi(cfg->word));
							break;
						case TKN_RT_TAP:
							if (!config_isstring(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							USRDEBUG("tap=%s\n", cfg->word);						
							rt_ptr->rt_tap=(cfg->word);
							USRDEBUG("rt_ptr->rt_tap=%s\n", rt_ptr->rt_tap);
							break;
						default:
							fprintf(stderr, "Programming Error\n");
							exit(1);
                    }
                    return(OK);
                }	
            }
            if( j == TKN_RT_MAX)
                fprintf(stderr, "Invaild identifier found at line %d\n", cfg->line);
        }
        //		USRDEBUG("próximo cfg\n");
        cfg = cfg->next;
    }
    return(OK);
}


int read_sw_lines( config_t *cfg)
{
    int i;
    int rcode;
    USRDEBUG("\n");
    for ( i = 0; cfg != nil; i++) {
        USRDEBUG("read_sw_lines type=%X\n",cfg->flags); 
        rcode = search_sw_ident(cfg->list);
        if( rcode) ERROR_RETURN(rcode);
        if( cfg == nil)return(OK);
        cfg = cfg->next;
    }
    return(OK);
}	

int read_rt_lines( config_t *cfg)
{
    int rcode;
    USRDEBUG("\n");
    while ( cfg != nil) {
        USRDEBUG("read_rt_lines type=%X\n",cfg->flags); 
        rcode = search_rt_ident(cfg->list);
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
    
    USRDEBUG("line=%d\n", cfg->line);
    if (cfg != nil) {
        if (config_isatom(cfg)) {
            USRDEBUG("word=%s\n", cfg->word);
            if( !strcmp(cfg->word, "switch")) {
                cfg = cfg->next;
                USRDEBUG("switch: \n");
                if (cfg != nil) {
					sw_ptr->sw_name = cfg->word; 
					USRDEBUG("%s\n", sw_ptr->sw_name);
					name_cfg = cfg;
					cfg = cfg->next;
					if (!config_issub(cfg)) {
						fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n",cfg->word, cfg->line);
						return(EXIT_CODE);
					}
					rcode = read_sw_lines(cfg->list);
					if(rcode) return(EXIT_CODE);
			        USRDEBUG(SW_FORMAT, SW_FIELDS(sw_ptr));
					nr_switch++;
					if( nr_switch > 1) {
						fprintf(stderr, "CONFIGURATION ERROR: Only one switch line is allowed\n");
						return(EXIT_CODE);
					}
					return(OK);
                }
            } else if( !strcmp(cfg->word, "rmttap")) {
				rt_ptr = &rmttap_cfg[nr_rmttap];
                cfg = cfg->next;
                rt_ptr->rt_index = nr_rmttap;
                USRDEBUG("rmttap: \n");
                if (cfg != nil) {
					rt_ptr->rt_name = cfg->word; 
					USRDEBUG("%s\n", rt_ptr->rt_name);
					for( i = 0; i < nr_rmttap; i++){
						if( !strcmp(rt_ptr->rt_name, rmttap_cfg[i].rt_name)){
							fprintf(stderr, "line %u rmttap name %s already exists\n", cfg->line, rt_ptr->rt_name);
							return(EXIT_CODE);								
						}
					}
					name_cfg = cfg;
					cfg = cfg->next;
					if (!config_issub(cfg)) {
						fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n",cfg->word, cfg->line);
						return(EXIT_CODE);
					}
					rcode = read_rt_lines(cfg->list);
					if(rcode) ERROR_RETURN(EXIT_CODE);
					assert(rt_ptr != NULL );
			        USRDEBUG(RT_FORMAT, RT_FIELDS(rt_ptr));
					nr_rmttap++;
					if( nr_rmttap > NR_RMTTAP) {
						fprintf(stderr, "CONFIGURATION ERROR: Only until %d rmttap lines are allowed\n", NR_RMTTAP);
						return(EXIT_CODE);
					}
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
	if ( nr_switch == 0) {
		fprintf(stderr, "CONFIGURATION ERROR: At least one switch must be configured\n");
		return(EXIT_CODE);	
	}
	
    return(OK);
}


/*===========================================================================*
 *				switch_config				     *
 *===========================================================================*/
void switch_config(char *f_conf)	/* config file name. */
{
    /* Main program of switch_config. */
    config_t *cfg;
    int rcode, i, j;
    
    cfg = nil;
    rcode  = OK;
		
    USRDEBUG("BEFORE config_read\n");
    cfg = config_read(f_conf, CFG_ESCAPED, cfg);
    USRDEBUG("AFTER config_read\n");
    
    rcode = scan_config(cfg);
	if ( nr_switch == 0){
		fprintf(stderr, "Config error line:%d No switch token found\n", cfg->line);
        return(EXIT_CODE);		
	}
	
	for( i = 0; i < nr_rmttap; i++){
		rt_ptr = &rmttap_cfg[i];
		if ( rt_ptr->rt_nodeid == PROC_NO_PID){
			fprintf(stderr, "CONFIGURATION ERROR: rmttap %s void nodeid\n",
				rt_ptr->rt_name);
			return(EXIT_CODE);								
		}
		if( i > 0 ) {
			for( j = 0; j < i; j++){
				if( !strcmp(rt_ptr->rt_tap, rmttap_cfg[j].rt_tap)
				&& rt_ptr->rt_nodeid == rmttap_cfg[j].rt_nodeid ) {
					fprintf(stderr, "CONFIGURATION ERROR: rmttap %s with tap name %s already defined on node %d\n",
						rt_ptr->rt_name, rt_ptr->rt_tap, rt_ptr->rt_nodeid);
					return(EXIT_CODE);								
				}
			}
		}
		if( rt_ptr->rt_nodeid == local_nodeid){
			if( !strcmp(rt_ptr->rt_tap,sw_ptr->sw_tap)){
				fprintf(stderr, "CONFIGURATION ERROR: rmttap %s with tap name %s is already defined as switch tap\n",
					rt_ptr->rt_name, rt_ptr->rt_tap);
				return(EXIT_CODE);				
			}
		}
        USRDEBUG(RT_FORMAT, RT_FIELDS(rt_ptr));
	}

///////////////////////////////////////////////////////////////////////
// ATENCION: El rmt_ep es innecesario dado que tendría el valor del nodeid 
// POR OTRO LADO: tambien habria una entrada para los TAP locales dentro
// de los rmttap indicando el nodeid == localnodeid y por lo tanto 
// el lcl_ep en la configuracion del switch no seria necesaria 
///////////////////////////////////////////////////////////////////////

    USRDEBUG("nr_switch=%d nr_rmttap=%d\n", nr_switch, nr_rmttap);
}

#ifdef ANULADO 

int search_rmttap_tkn2(config_t *cfg)
{
    int rcode;
    config_t *name_cfg;
    
    USRDEBUG("line=%d\n", cfg->line);
    if (cfg != nil) {
        if (config_isatom(cfg)) {
            USRDEBUG("word=%s\n", cfg->word);
            if( !strcmp(cfg->word, "rmttap")) {
                cfg = cfg->next;
                rt_ptr->rt_index = nr_rmttap;
                USRDEBUG("rmttap: ");
                if (cfg != nil) {
					rt_ptr->rt_name = cfg->word; 
					name_cfg = cfg;
					cfg = cfg->next;
					if (!config_issub(cfg)) {
						fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n",cfg->word, cfg->line);
						return(EXIT_CODE);
					}
					rcode = read_rt_lines(cfg->list);
					if(rcode) ERROR_RETURN(EXIT_CODE);
					ERROR_RETURN(OK);
                }
            }
            fprintf(stderr, "Config error line:%d No rmttap token found\n", cfg->line);
            return(EXIT_CODE);
        }
        fprintf(stderr, "Config error line:%d No rmttap name found \n", cfg->line);
        return(EXIT_CODE);
    }
    return(EXIT_CODE);
}


int search_rmttap_cfg2(config_t *cfg)
{
    int rcode;	
    
    USRDEBUG("\n");
    for(int i=0; cfg != nil; i++) {
        if (!config_issub(cfg)) {
            fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n", cfg->word, cfg->line);
            return(EXIT_CODE);
        }
        USRDEBUG("search_rmttap_cfg[%d] line=%d\n",i,cfg->line);
		rt_ptr = &rmttap_cfg[nr_rmttap];
        rcode = search_rmttap_tkn(cfg->list);
        //@ERROR: Out of bounds read
        if( rcode == EXIT_CODE)
            return(rcode);
        nr_rmttap++;
		if( nr_rmttap >= NR_RMTTAP) {
            fprintf(stderr, "Number of Remote TAP configurations is bigger than %d\n", NR_RMTTAP-1);
            return(EXIT_CODE);			
		}
        cfg = cfg->next;
    }
    return(OK);
}
#endif // ANULADO 
