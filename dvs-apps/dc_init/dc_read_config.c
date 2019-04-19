/***********************************************
	DC CONFIGURATION FILE
*  Sample File format *

dc DCNAME {
	dcid			0;
	nr_procs		256;
	nr_tasks		34;
	nr_sysprocs		64;
	nr_nodes		32;
	warn2proc		0;
	warnmsg			1;
	tracker			YES;
	ip_addr		"192.168.10.100";
	memory		512;
	image		"/usr/src/dvs/vos/images/debian0.img";
	mount		"/usr/src/dvs/vos/rootfs/DC0";
};
**************************************************/

#define  USRDBG		1
#define  DVS_USERSPACE	1

#include "dc_init.h"
#include "../../include/generic/configfile.h"
#undef EXTERN
#define EXTERN	extern
#include "glo.h"
 
#define MAXTOKENSIZE	20
#define OK				0
#define EXIT_CODE		1
#define NEXT_CODE		2

#define YES				1
#define NO				0

#define	TKN_DCID		0
#define TKN_NR_PROCS	1
#define TKN_NR_TASKS	2
#define TKN_NR_SYSPROCS	3
#define TKN_NR_NODES	4
#define TKN_WARN2PROC	5
#define TKN_WARNMSG		6
#define TKN_IP_ADDR		7
#define TKN_MEMORY 		8
#define TKN_IMAGE 		9
#define TKN_MOUNT 		10
#define TKN_TRACKER 	11
#define NR_IDENT 		12

#define nil ((void*)0)

char *cfg_ident[] = {
	"dcid",
	"nr_procs",
	"nr_tasks",
	"nr_sysprocs",
	"nr_nodes",
	"warn2proc",
	"warnmsg",
	"ip_addr",
	"memory",
	"image",
	"mount",
	"tracker",
};

#define MAX_FLAG_LEN 30
struct flag_s {
	char f_name[MAX_FLAG_LEN];
	int f_value;
};
typedef struct flag_s flag_t;	

extern 	dc_usr_t *dc_ptr;
extern  container_t ctnr, *c_ptr;
extern  int nr_containers;

int search_ident(config_t *cfg)
{
	int i, j, rcode;
	
	for( i = 0; cfg!=nil; i++) {
		if (config_isatom(cfg)) {
			USRDEBUG("search_ident[%d] line=%d word=%s\n",i,cfg->line, cfg->word); 
			for( j = 0; j < NR_IDENT; j++) {
				if( !strcmp(cfg->word, cfg_ident[j])) {
					USRDEBUG("line[%d] MATCH identifier %s\n", cfg->line, cfg->word); 
					if( cfg->next == nil)
						fprintf(stderr, "Void value found at line %d\n", cfg->line);
					cfg = cfg->next;				
					switch(j){				
						case TKN_DCID:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}		
							dc_ptr->dc_dcid = atoi(cfg->word); 
							USRDEBUG("dc_dcid=%d\n", dc_ptr->dc_dcid);
							if( dc_ptr->dc_dcid < 0 || dc_ptr->dc_dcid >= dvs_ptr->d_nr_dcs) {
								fprintf (stderr, "Invalid dcid [0-%d]\n", dvs_ptr->d_nr_dcs-1 );
								exit(EXIT_FAILURE);
							}
							break;
						case TKN_NR_PROCS:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							dc_ptr->dc_nr_procs = atoi(cfg->word); 
							USRDEBUG("dc_nr_procs=%d\n", dc_ptr->dc_nr_procs);
							if( dc_ptr->dc_nr_procs <= 0 
							|| dc_ptr->dc_nr_procs  > dvs_ptr->d_nr_procs) {
								fprintf (stderr, "Invalid nr_procs [1-%d]\n", dvs_ptr->d_nr_procs);
								exit(EXIT_FAILURE);
							}
							break;
						case TKN_NR_TASKS:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							dc_ptr->dc_nr_tasks = atoi(cfg->word); 
							USRDEBUG("dc_nr_tasks=%d\n", dc_ptr->dc_nr_tasks);
							if( dc_ptr->dc_nr_tasks <= 0 
							|| dc_ptr->dc_nr_tasks  > dvs_ptr->d_nr_tasks ) {
								fprintf (stderr, "Invalid nr_tasks [1-%d]\n", dvs_ptr->d_nr_tasks);
								exit(EXIT_FAILURE);
							}
							break;
						case TKN_NR_SYSPROCS:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							dc_ptr->dc_nr_sysprocs = atoi(cfg->word); 
							USRDEBUG("dc_nr_sysprocs=%d\n", dc_ptr->dc_nr_sysprocs);
							if( dc_ptr->dc_nr_sysprocs <= 0 
							|| dc_ptr->dc_nr_sysprocs > dvs_ptr->d_nr_sysprocs ) {
								fprintf (stderr, "Invalid nr_sysprocs [1-%d]\n", dvs_ptr->d_nr_sysprocs );
								exit(EXIT_FAILURE);
							}
							break;
						case TKN_NR_NODES:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							dc_ptr->dc_nr_nodes = atoi(cfg->word); 
							USRDEBUG("dc_nr_nodes=%d\n", dc_ptr->dc_nr_nodes);
							if( dc_ptr->dc_nr_nodes <= 0 
							|| dc_ptr->dc_nr_nodes > dvs_ptr->d_nr_nodes ) {
								fprintf (stderr, "Invalid nr_nodes [1-%d]\n", dvs_ptr->d_nr_nodes);
								exit(EXIT_FAILURE);
							}
							break;							
						case TKN_WARN2PROC:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							dc_ptr->dc_warn2proc = atoi(cfg->word);
							USRDEBUG("dc_warn2proc=%d\n", dc_ptr->dc_warn2proc);
							if( dc_ptr->dc_warn2proc < (-dvs_ptr->d_nr_tasks) 
							|| dc_ptr->dc_warn2proc > (dvs_ptr->d_nr_sysprocs-dvs_ptr->d_nr_tasks) ) {
								fprintf (stderr, "Invalid dc_warn2proc [%d-%d]\n"
									,-dvs_ptr->d_nr_tasks
									,(dvs_ptr->d_nr_sysprocs-dvs_ptr->d_nr_tasks));
								exit(EXIT_FAILURE);
							}
							break;							
						case TKN_WARNMSG:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							dc_ptr->dc_warnmsg = atoi(cfg->word);
							USRDEBUG("dc_warnmsg=%d=0x%X\n", dc_ptr->dc_warnmsg, dc_ptr->dc_warnmsg);
							break;
						case TKN_IP_ADDR:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							if(c_ptr == NULL) ERROR_RETURN(-1);
							strncpy(ctnr.c_ip_addr, cfg->word, IPLEN);
							USRDEBUG("c_ip_addr=%s\n", ctnr.c_ip_addr);
							break;
						case TKN_MEMORY:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							ctnr.c_memory = atoi(cfg->word);
							USRDEBUG("c_memory=%d\n", ctnr.c_memory);
							if( ctnr.c_memory < 0)  {
								fprintf (stderr, "Invalid c_memory [>0]\n");
								exit(EXIT_FAILURE);
							}
							break;
						case TKN_IMAGE:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							strncpy(ctnr.c_image, cfg->word, IMAGELEN);
							USRDEBUG("c_image=%s\n", ctnr.c_image);
							break;
						case TKN_MOUNT:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							strncpy(ctnr.c_mount, cfg->word, IMAGELEN);
							USRDEBUG("c_mount=%s\n", ctnr.c_mount);
							break;
						case TKN_TRACKER:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							if( strncmp(cfg->word, "YES", 3) == 0) 
								tracker_flag = 1;								
							USRDEBUG("tracker_flag=%d\n", tracker_flag);
							break;
						default:
							fprintf(stderr, "Programming Error\n");
							exit(1);
					}
					return(OK);
				}	
			}
			if( j == NR_IDENT)
				fprintf(stderr, "Invaild identifier found at line %d\n", cfg->line);
		}
		USRDEBUG("próximo cfg\n");
		cfg = cfg->next;
	}
	return(OK);
}
		
int read_lines(config_t *cfg)
{
	int i;
	int rcode;
	for ( i = 0; cfg != nil; i++) {
		USRDEBUG("read_lines type=%X\n",cfg->flags); 
		rcode = search_ident(cfg->list);
		if( rcode) ERROR_RETURN(rcode);
		if( cfg == nil)return(OK);
		cfg = cfg->next;
	}
	return(OK);
}	

int search_dc_tkn(config_t *cfg)
{
	int rcode;
    config_t *name_cfg;
	
    if (cfg != nil) {
		if (config_isatom(cfg)) {
			if( !strcmp(cfg->word, "dc")) {
				cfg = cfg->next;
				USRDEBUG("token dc");
				if (cfg != nil) {
					if (config_isatom(cfg)) {
						USRDEBUG("%s\n", cfg->word);
						strncpy(dc_ptr->dc_name, cfg->word, (MAXDCNAME-1));
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
			fprintf(stderr, "Config error line:%d No machine token found\n", cfg->line);
			return(EXIT_CODE);
		}
		fprintf(stderr, "Config error line:%d No machine name found \n", cfg->line);
		return(EXIT_CODE);
	}
	return(EXIT_CODE);
}

int search_dc_config(config_t *cfg)
{
	int rcode;
	int i;
	
    for( i=0; cfg != nil; i++) {
		if (!config_issub(cfg)) {
			fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n", cfg->word, cfg->line);
			return(EXIT_CODE);
		}
		USRDEBUG("search_dc_config[%d] line=%d\n",i,cfg->line);
		rcode = search_dc_tkn(cfg->list);
		if( rcode == EXIT_CODE)
			return(rcode);
		nr_containers++;
		cfg= cfg->next;
	}
	return(OK);
}

 
/*===========================================================================*
 *				dc_read_config				     *
 *===========================================================================*/
void dc_read_config(char *f_conf)	/* config file name. */
{
/* Main program of dc_read_config. */
config_t *cfg;
int rcode;

cfg = nil;
rcode  = OK;
cfg = config_read(f_conf, CFG_ESCAPED, cfg);

rcode = search_dc_config(cfg);
}

