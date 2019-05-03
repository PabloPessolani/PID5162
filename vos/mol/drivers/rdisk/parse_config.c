/***********************************************
	CONFIGURATION FILE
*  Sample File format *

device MY_FILE_IMG {
	type			FILE_IMAGE 
	image_file 		"/usr/src/test/minix3.img";
	replicated		YES
};

device MY_MEMORY_IMG {
	type			MEMORY_IMAGE 
	image_file 		"/usr/src/test/minix3.img";
	volatile		YES
	replicated		NO
};

device MY_MEMORY_IMG {
	type			NBD_IMAGE 
	image_file 		"/usr/src/test/minix3.img";
	replicated		NO
};
**************************************************/

#define TASKDEBG		1
#define  MOL_USERSPACE	1


#define OPER_NAME 0
#define OPER_OPEN 1
#define OPER_NOP 2
#define OPER_IOCTL 3
#define OPER_PREPARE 4
#define OPER_TRANSF 5
#define OPER_CLEAN 6
#define OPER_GEOM 7
#define OPER_SIG 8
#define OPER_ALARM 9
#define OPER_CANC 10
#define OPER_SEL 11


#include "rdisk.h"
//#include "../const.h"
#include "../../include/configfile.h"
#include "../debug.h"
 
#define MAXTOKENSIZE	20
#define OK				0
#define EXIT_CODE		(-1)
#define NEXT_CODE		1

#define TKN_IMAGE_TYPE	0
#define TKN_IMAGE_FILE	1
#define TKN_VOLATILE	2
#define TKN_BUFFER		3
#define TKN_REPLICATED	4
#define TKN_ACTIVE		5

#define nil ((void*)0)

char *cfg_ident[] = {
	"image_type",
	"image_file",
	"volatile",
	"buffer",
	"replicated",
	"active",
};
#define NR_IDENT 6

#define MAX_FLAG_LEN 30
struct flag_s {
	char f_name[MAX_FLAG_LEN];
	int f_value;
};
typedef struct flag_s flag_t;	

#define NR_ITYPE	3
flag_t img_type[] = {
	{"FILE_IMAGE",		FILE_IMAGE},
	{"MEMORY_IMAGE",	MEMORY_IMAGE},	
	{"NBD_IMAGE",		NBD_IMAGE},
};

int search_img_type(config_t *cfg)
{
	int j;
	unsigned int flags;
	config_t *cfg_lcl;
	
	if( cfg == nil) {
		fprintf(stderr, "No image type at line %d\n", cfg->line);
		return(EXIT_CODE);
	}
	if (config_isatom(cfg)) {
		TASKDEBUG("image type=%s\n", cfg->word); 
		// cfg = cfg->next;
		
		if (! config_isatom(cfg)) {
			fprintf(stderr, "Bad argument image type at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		cfg_lcl = cfg;
		flags = 0;
		/* 
		* Search for image type 
		*/
		for( j = 0; j < NR_ITYPE; j++) {
			if( !strcmp(cfg->word, img_type[j].f_name)) {
				flags |= img_type[j].f_value;
				break;
			}
		}
		if( j == NR_ITYPE){
			fprintf(stderr, "No image type defined at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		// cfg = cfg->next;
		if (!config_isatom(cfg)) {
			fprintf(stderr, "Bad argument image type at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
	}
	return(flags);
}

int search_ident(config_t *cfg)
{
	int i, j, rcode;
	
	for( i = 0; cfg!=nil; i++) {
		if (config_isatom(cfg)) {
			TASKDEBUG("search_ident[%d] line=%d word=%s\n",i,cfg->line, cfg->word); 
			for( j = 0; j < NR_IDENT; j++) { 
				if( !strcmp(cfg->word, cfg_ident[j])) {
					TASKDEBUG("line[%d] MATCH identifier %s\n", cfg->line, cfg->word); 
					if( cfg->next == nil)
						fprintf(stderr, "Void value found at line %d\n", cfg->line);
					cfg = cfg->next;				
					switch(j){
						case TKN_IMAGE_TYPE:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							rcode = search_img_type(cfg);
							if ( rcode == EXIT_CODE)
								return(EXIT_CODE);
							devvec[minor_devs].img_type = rcode; 
							break;
						case TKN_IMAGE_FILE:
							if (!config_isstring(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							TASKDEBUG("image_file=%s\n", cfg->word);						
							devvec[minor_devs].img_ptr=(cfg->word);
							devvec[minor_devs].available = YES;
							TASKDEBUG("devvec[%d].img_ptr=%s\n", minor_devs,devvec[minor_devs].img_ptr);
							TASKDEBUG("devvec[%d].available=%d\n", minor_devs,devvec[minor_devs].available);
							break;
						case TKN_VOLATILE:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							if( strncmp(cfg->word, "YES", 3) == 0) 
								devvec[minor_devs].volatile_flag = VOLATILE_YES;
							else if ( strncmp(cfg->word, "NO", 2) == 0)
								devvec[minor_devs].volatile_flag = VOLATILE_NO;
							else {
								fprintf(stderr, "Configuration Error\n");
								exit(1);
							}		
							TASKDEBUG("devvec[%d].volatile_flag=%d\n", minor_devs,devvec[minor_devs].volatile_flag);
							 break;
						case TKN_REPLICATED:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							if( strncmp(cfg->word, "YES", 3) == 0) 
								devvec[minor_devs].replica_flag = REPLICATED_YES;
							else if ( strncmp(cfg->word, "NO", 2) == 0)
								devvec[minor_devs].replica_flag = REPLICATED_NO;
							else {
								fprintf(stderr, "Configuration Error\n");
								exit(1);
							}		
							TASKDEBUG("devvec[%d].replica_flag=%d\n", minor_devs,devvec[minor_devs].replica_flag);
							break;							 
						case TKN_ACTIVE:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							if( strncmp(cfg->word, "YES", 3) == 0) 
								devvec[minor_devs].active_flag = ACTIVE_YES;
							else if ( strncmp(cfg->word, "NO", 2) == 0)
								devvec[minor_devs].active_flag = ACTIVE_NO;
							else {
								fprintf(stderr, "Configuration Error\n");
								exit(1);
							}		
							TASKDEBUG("devvec[%d].active_flag=%d\n", minor_devs,devvec[minor_devs].active_flag);
							break;	
						case TKN_BUFFER:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}					 
							devvec[minor_devs].buff_size = (( atoi(cfg->word) < 0 || atoi(cfg->word)) >= BUFF_SIZE)?BUFF_SIZE:atoi(cfg->word);
							TASKDEBUG("devvec[%d].buff_size=%d\n", minor_devs,devvec[minor_devs].buff_size);
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
		TASKDEBUG("próximo cfg\n");
		cfg = cfg->next;
	}
	return(OK);
}
		
int read_lines(config_t *cfg)
{
	int i;
	int rcode;
	for ( i = 0; cfg != nil; i++) {
		TASKDEBUG("read_lines type=%X\n",cfg->flags); 
		rcode = search_ident(cfg->list);
		if( rcode) ERROR_RETURN(rcode);
		if( cfg == nil)return(OK);
		cfg = cfg->next;
	}
	return(OK);
}	

int search_device_tkn(config_t *cfg)
{
	int rcode;
    config_t *name_cfg;
	
    if (cfg != nil) {
		if (config_isatom(cfg)) {
			if( !strcmp(cfg->word, "device")) {
				cfg = cfg->next;
				TASKDEBUG("TKN_DEVICE  ");
				if (cfg != nil) {
					if (config_isatom(cfg)) {
						devvec[minor_devs].dev_name = cfg->word;
						TASKDEBUG("%s\n", devvec[minor_devs].dev_name);
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
	devvec_t *p_dev;
	
	minor_devs = 0; 
    for( i=0; cfg != nil; i++) {
		if (!config_issub(cfg)) {
			fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n", cfg->word, cfg->line);
			return(EXIT_CODE);
		}
		TASKDEBUG("search_dc_config[%d] line=%d\n",i,cfg->line);
		rcode = search_device_tkn(cfg->list);
		if( rcode == EXIT_CODE)
			return(rcode);
		p_dev = &devvec[minor_devs];
		TASKDEBUG("MINOR=%d\n",minor_devs);
		TASKDEBUG(DEV_USR1_FORMAT, DEV_USR1_FIELDS(p_dev));
		TASKDEBUG(DEV_USR2_FORMAT, DEV_USR2_FIELDS(p_dev));
		TASKDEBUG(DEV_USR3_FORMAT, DEV_USR3_FIELDS(p_dev));
		minor_devs++;
		cfg= cfg->next;
	}
	return(OK);
}

 
/*===========================================================================*
 *				parse_config				     *
 *===========================================================================*/
void parse_config(char *f_conf)	/* config file name. */
{
/* Main program of parse_config. */
config_t *cfg;
int rcode;

cfg = nil;
rcode  = OK;
minor_dev=0;

TASKDEBUG("BEFORE\n");
cfg = config_read(f_conf, CFG_ESCAPED, cfg);
TASKDEBUG("AFTER \n");

rcode = search_dc_config(cfg);
TASKDEBUG("AFTER2 \n");

}

