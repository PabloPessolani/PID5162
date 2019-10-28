/***********************************************
 RADAR CONFIGURATION FILE
 
service RDISK0 {
 replica		RPB
 dcid  		0;
 endpoint 		3;
 group		"RDISK";
};

service RDISK1 {
 replica		RSM;
 dcid  		1;
 endpoint 		3;
 group		"RDISK"
}

**************************************************/
#define _GNU_SOURCE     
#define _MULTI_THREADED
#include "radar.h"

#include "../../include/generic/configfile.h"

#define OK				0
#define EXIT_CODE		1
#define NEXT_CODE		2

#define YES				1
#define NO				0

#define MAX_FLAG_LEN 30
#define MAXTOKENSIZE	20

#define	TKN_REPLICA		0
#define TKN_DCID		1
#define TKN_ENDPOINT	2
#define TKN_GROUP 		3
#define TKN_NODES		4
#define TKN_MAX 		5	// MUST be the last

#define nil ((void*)0)
#define REPLICA_RPB		0
#define REPLICA_RSM 	1

char *cfg_ident[] = {
    "replica",
    "dcid",
    "endpoint",
    "group",
    "nodes"
};

struct flag_s {
    char f_name[MAX_FLAG_LEN];
    int f_value;
};
typedef struct flag_s flag_t;	

#define NR_REPLICA	2
flag_t replica[] = {
    {"RPB",REPLICA_RPB},
    {"RSM",REPLICA_RSM}
};

int search_replica(config_t *cfg)
{
    int j;
    config_t *cfg_lcl;
    
    USRDEBUG("\n");
    
    if( cfg == nil) {
        fprintf(stderr, "No replica at line %d\n", cfg->line);
        return(EXIT_CODE);
    }
    if (config_isatom(cfg)) {
        USRDEBUG("replica=%s\n", cfg->word); 
        // cfg = cfg->next;
        
        if (! config_isatom(cfg)) {
            fprintf(stderr, "Bad argument type at line %d\n", cfg->line);
            return(EXIT_CODE);
        }
        cfg_lcl = cfg;
        /* 
        * Search for type 
        */
        for( j = 0; j < NR_REPLICA; j++) {
            if( !strcmp(cfg->word, replica[j].f_name)) {
                USRDEBUG("replica value=%d\n", replica[j].f_value); 
                return(replica[j].f_value);
            }
        }
        if( j == NR_REPLICA){
            fprintf(stderr, "No replica type defined at line %d\n", cfg->line);
            return(EXIT_CODE);
        }
        // cfg = cfg->next;
        if (!config_isatom(cfg)) {
            fprintf(stderr, "Bad argument replica at line %d\n", cfg->line);
            return(EXIT_CODE);
        }	
    }
    return(EXIT_CODE);
}


int search_ident(config_t *cfg)
{
    int i, j, rcode, dcid;
    
    USRDEBUG("\n");
    for( i = 0; cfg!=nil; i++) {
        if (config_isatom(cfg)) {
            USRDEBUG("search_ident[%d] line=%d word=%s\n",i,cfg->line, cfg->word); 
            for( j = 0; j < TKN_MAX; j++) {
                if( !strcmp(cfg->word, cfg_ident[j])) {
                    USRDEBUG("line[%d] MATCH identifier %s\n", cfg->line, cfg->word); 
                    if( cfg->next == nil)
                        fprintf(stderr, "Void value found at line %d\n", cfg->line);
                    cfg = cfg->next;				
                    switch(j){			
                        case TKN_REPLICA:
                        if (!config_isatom(cfg)) {
                            fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
                            return(EXIT_CODE);
                        }					 
                        USRDEBUG("replica=%s\n", cfg->word);
                        rcode = search_replica(cfg);
                        if( rcode < 0) return(rcode);
                        rad_ptr[nr_control]->rad_replication = rcode;
                        break;
                        case TKN_DCID:
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
                        rad_ptr[nr_control]->rad_dcid = dcid;
                        break;
                        case TKN_ENDPOINT:
                        if (!config_isatom(cfg)) {
                            fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
                            return(EXIT_CODE);
                        }
                        USRDEBUG("endpoint=%d\n", atoi(cfg->word));
                        rad_ptr[nr_control]->rad_ep=atoi(cfg->word);
                        break;	
                        case TKN_GROUP:
                        if (!config_isatom(cfg)) {
                            fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
                            return(EXIT_CODE);
                        }
                        USRDEBUG("group=%s\n", cfg->word);
                        strncpy(rad_ptr[nr_control]->rad_group,cfg->word,MAXNODENAME);
                        break;
                        case TKN_NODES:
                        if (!config_isstring(cfg)) {
                            fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
                            return(EXIT_CODE);
                        }
                        USRDEBUG("nodes=%X\n", -1);
                        rad_ptr[nr_control]->rad_bm_valid = -1;
                        default:
                        fprintf(stderr, "Programming Error\n");
                        exit(1);
                    }
                    return(OK);
                }	
            }
            if( j == TKN_MAX)
                fprintf(stderr, "Invaild identifier found at line %d\n", cfg->line);
        }
        //		USRDEBUG("próximo cfg\n");
        cfg = cfg->next;
    }
    return(OK);
}

int read_lines( config_t *cfg)
{
    int i;
    int rcode;
    USRDEBUG("\n");
    for ( i = 0; cfg != nil; i++) {
        USRDEBUG("read_lines type=%X\n",cfg->flags); 
        rcode = search_ident(cfg->list);
        if( rcode) ERROR_RETURN(rcode);
        if( cfg == nil)return(OK);
        cfg = cfg->next;
    }
    return(OK);
}	

int search_service_tkn(config_t *cfg)
{
    int rcode;
    config_t *name_cfg;
    
    USRDEBUG("line=%d\n", cfg->line);
    if (cfg != nil) {
        if (config_isatom(cfg)) {
            USRDEBUG("word=%s\n", cfg->word);
            if( !strcmp(cfg->word, "service")) {
                cfg = cfg->next;
                rad_ptr[nr_control]->rad_index = nr_control;
                USRDEBUG("service: ");
                if (cfg != nil) {
                    if (config_isatom(cfg)) {
                        strncpy(rad_ptr[nr_control]->rad_svrname,cfg->word,MAXPROCNAME-1); 
                        rad_ptr[nr_control]->rad_len = strlen(rad_ptr[nr_control]->rad_svrname);
                        name_cfg = cfg;
                        cfg = cfg->next;
                        if (!config_issub(cfg)) {
                            fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n",cfg->word, cfg->line);
                            return(EXIT_CODE);
                        }
                        rcode = read_lines(cfg->list);
                        if(rcode) return(EXIT_CODE);
                        return(OK);
                    }
                }
            }
            fprintf(stderr, "Config error line:%d No service token found\n", cfg->line);
            return(EXIT_CODE);
        }
        fprintf(stderr, "Config error line:%d No service name found \n", cfg->line);
        return(EXIT_CODE);
    }
    return(EXIT_CODE);
}

int search_radar_cfg(config_t *cfg)
{
    int rcode;	
    
    USRDEBUG("\n");
    for(int i=0; cfg != nil; i++) {
        if (!config_issub(cfg)) {
            fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n", cfg->word, cfg->line);
            return(EXIT_CODE);
        }
        USRDEBUG("search_radar_cfg[%d] line=%d\n",i,cfg->line);
        rcode = search_service_tkn(cfg->list);
        //@ERROR: Out of bounds read
        USRDEBUG(RAD1_FORMAT, RAD1_FIELDS(rad_ptr[nr_control]));
        if( rcode == EXIT_CODE)
            return(rcode);
        nr_control++;
        cfg = cfg->next;
    }
    return(OK);
}


/*===========================================================================*
 *				radar_config				     *
 *===========================================================================*/
void radar_config(char *f_conf)	/* config file name. */
{
    /* Main program of radar_config. */
    config_t *cfg;
    int rcode;
    
    cfg = nil;
    rcode  = OK;
    
    //Global glo.h
    nr_control = 0;
    
    USRDEBUG("BEFORE\n");
    cfg = config_read(f_conf, CFG_ESCAPED, cfg);
    USRDEBUG("AFTER \n");
    
    rcode = search_radar_cfg(cfg);
    USRDEBUG("AFTER2 \n");
    
    
}

