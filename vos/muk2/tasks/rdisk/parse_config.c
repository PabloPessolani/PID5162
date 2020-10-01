/***********************************************
	TEST CONFIGURATION FILE
*  Sample File format *

device MY_FILE_IMG {
	mayor			3
	minor			0;
	type			FILE_IMAGE 
	image_file 		"/usr/src/test/minix3.img";
	replicated		YES
};

device MY_MEMORY_IMG {
	mayor			3
	minor			1;
	type			MEMORY_IMAGE 
	image_file 		"/usr/src/test/minix3.img";
	volatile		YES
	replicated		NO
};

device MY_MEMORY_IMG {
	mayor			3
	minor			2;
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
#include "../debug.h"
 
#define MAXTOKENSIZE	20
#define OK				0
#define EXIT_CODE		1
#define NEXT_CODE		2

#define FILE_IMAGE		0
#define MEMORY_IMAGE	1
#define NBD_IMAGE		2

#define YES				1
#define NO				0

#define	TKN_MAJOR		0
#define TKN_MINOR		1
#define TKN_TYPE		2
#define TKN_IMAGE_FILE	3
#define TKN_VOLATILE	4
#define TKN_BUFFER		5
#define TKN_REPLICATED	6

#define nil ((void*)0)

char *rd_cfg_ident[] = {
	"major",
	"minor",
	"type",
	"image_file",
	"volatile",
	"buffer",
	"replicated",
};
#define NR_IDENT 7

#define MAX_FLAG_LEN 30
struct rd_flag_s {
	char f_name[MAX_FLAG_LEN];
	int f_value;
};
typedef struct rd_flag_s rd_flag_t;	

#define NR_DTYPE	3
rd_flag_t rd_dev_type[] = {
	{"FILE_IMAGE",FILE_IMAGE},
	{"MEMORY_IMAGE",MEMORY_IMAGE},	
	{"NBD_IMAGE",NBD_IMAGE},
};

#define NR_DBIT	2
rd_flag_t rd_dev_bit[] = {
	{"YES",YES},
	{"NO",NO},	
};


int rd_bit_flag;

int rd_search_type(config_t *cfg)
{
	int j;
	unsigned int flags;
	config_t *cfg_lcl;
	
	if( cfg == nil) {
		fprintf(stderr, "No type at line %d\n", cfg->line);
		return(EXIT_CODE);
	}
	if (config_isatom(cfg)) {
		MUKDEBUG("type=%s\n", cfg->word); 
		// cfg = cfg->next;
		
		if (! config_isatom(cfg)) {
			fprintf(stderr, "Bad argument type at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		cfg_lcl = cfg;
		flags = 0;
		/* 
		* Search for type 
		*/
		for( j = 0; j < NR_DTYPE; j++) {
			if( !strcmp(cfg->word, rd_dev_type[j].f_name)) {
				flags |= rd_dev_type[j].f_value;
				break;
			}
		}
		if( j == NR_DTYPE){
			fprintf(stderr, "No device type defined at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		// cfg = cfg->next;
		if (!config_isatom(cfg)) {
			fprintf(stderr, "Bad argument type at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		
	}
	return(OK);
}

int rd_search_bit(config_t *cfg)
{
	int j;
	unsigned int flags;
	config_t *cfg_lcl;
	
	if( cfg == nil) {
		fprintf(stderr, "No Bit OK at line %d\n", cfg->line);
		return(EXIT_CODE);
	}
	if (config_isatom(cfg)) {
		MUKDEBUG("Bit=%s\n", cfg->word); 
		// cfg = cfg->next;
		
		if (! config_isatom(cfg)) {
			fprintf(stderr, "Bad argument bit OK at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		cfg_lcl = cfg;
		flags = 0;
		/* 
		* Search for bit: YES / NO
		*/
		for( j = 0; j < NR_DBIT; j++) {
			if( !strcmp(cfg->word, rd_dev_bit[j].f_name)) {
				flags |= rd_dev_bit[j].f_value;
				break;
			}
		}
		if( j == NR_DBIT){
			fprintf(stderr, "No bit OK defined at line %d\n", cfg->line);
			return(EXIT_CODE);
		}
		// cfg = cfg->next;
		if (!config_isatom(cfg)) {
			fprintf(stderr, "Bad argument bit OK at line %d\n", cfg->line);
			return(EXIT_CODE);
		}

		if ( !strcmp(cfg->word, rd_dev_bit[0].f_name)){
			rd_bit_flag = YES;
			MUKDEBUG("rd_dev_bit[0].f_name=%s, rd_bit_flag=%d\n", rd_dev_bit[0].f_name, rd_bit_flag);
			}
		else{
			rd_bit_flag = NO;
			MUKDEBUG("rd_dev_bit[0].f_name=%s, rd_bit_flag=%d\n", rd_dev_bit[0].f_name, rd_bit_flag);
		}
			
	}
		
	return(OK);
}

int rd_search_ident(config_t *cfg)
{
	int i, j, rcode;
	
	for( i = 0; cfg!=nil; i++) {
		if (config_isatom(cfg)) {
			MUKDEBUG("rd_search_ident[%d] line=%d word=%s\n",i,cfg->line, cfg->word); 
			for( j = 0; j < NR_IDENT; j++) {
				if( !strcmp(cfg->word, rd_cfg_ident[j])) {
					MUKDEBUG("line[%d] MATCH identifier %s\n", cfg->line, cfg->word); 
					if( cfg->next == nil)
						fprintf(stderr, "Void value found at line %d\n", cfg->line);
					cfg = cfg->next;				
					switch(j){
						case TKN_MAJOR:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}					 
							MUKDEBUG("rd_ep=%d\n", rd_ep);
							mayor_dev=atoi(cfg->word);
							MUKDEBUG("mayor_dev=%d\n", mayor_dev);	
							if( mayor_dev != rd_ep) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "Mayor number (%d), must be equal to rd_ep (%d)\n", mayor_dev,rd_ep);
								mayor_dev = -1;
								return(EXIT_CODE);
							}
							break;
						case TKN_MINOR:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							MUKDEBUG("minor=%d\n", atoi(cfg->word));
							minor_dev=atoi(cfg->word);
							MUKDEBUG("minor_dev=%d\n", minor_dev);
							
							if ((minor_dev < 0) || (minor_dev >= NR_DEVS)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "Minor number %d, must be > 0 and < NR_DEVS(%d)\n", minor_dev,NR_DEVS);
								minor_dev = -1;
								}
							break;
						case TKN_TYPE:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							rcode = rd_search_type(cfg);
							if( rcode) return(rcode);
							break;
						case TKN_IMAGE_FILE:
							if (!config_isstring(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							MUKDEBUG("image_file=%s\n", cfg->word);
							
							if (minor_dev == -1){
								fprintf(stderr, "Check the minor device number for %s in the configure file\n", cfg->word);
							}
							else{
								devvec[minor_dev].img_ptr=(cfg->word);
								devvec[minor_dev].available = 1;
								MUKDEBUG("devvec[%d].img_ptr=%s\n", minor_dev,devvec[minor_dev].img_ptr);
								MUKDEBUG("devvec[%d].available=%d\n", minor_dev,devvec[minor_dev].available);
							}
							break;
						case TKN_VOLATILE:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							rcode = rd_search_bit(cfg);
							if( rcode) return(rcode);
							 break;
						 case TKN_REPLICATED:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							rcode = rd_search_bit(cfg);
							
							MUKDEBUG("rd_bit_flag=%d\n", rd_bit_flag);

							if( rcode) return(rcode);
							break;
						case TKN_BUFFER:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}					 
							MUKDEBUG("buffer=%u\n", atoi(cfg->word));
							
							devvec[minor_dev].buff_size = (( atoi(cfg->word) < 0 || atoi(cfg->word)) >= BUFF_SIZE)?BUFF_SIZE:atoi(cfg->word);
							MUKDEBUG("devvec[%d].buff_size=%d\n", minor_dev,devvec[minor_dev].buff_size);
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
		MUKDEBUG("próximo cfg\n");
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
				MUKDEBUG("TKN_DEVICE  ");
				if (cfg != nil) {
					if (config_isatom(cfg)) {
						MUKDEBUG("%s\n", cfg->word);
						name_cfg = cfg;
						cfg = cfg->next;
						if (!config_issub(cfg)) {
							fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n",cfg->word, cfg->line);
							return(EXIT_CODE);
						}
						rcode = rd_read_lines(cfg->list);
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

int rd_search_dev(config_t *cfg)
{
	int rcode;
	int i;
	
    for( i=0; cfg != nil; i++) {
		if (!config_issub(cfg)) {
			fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n", cfg->word, cfg->line);
			return(EXIT_CODE);
		}
		MUKDEBUG("rd_search_dev[%d] line=%d\n",i,cfg->line);
		rcode = search_device_tkn(cfg->list);
		if( rcode == EXIT_CODE)
			return(rcode);
		rd_dev_nr++;
		cfg= cfg->next;
	}
	return(OK);
}

 
/*===========================================================================*
 *				parse_config				     *
 *===========================================================================*/
void parse_config(char *f_conf)	/* config file name. */
{
/* Main program of test_config. */
config_t *cfg;
int rcode;

cfg = nil;
rcode  = OK;
minor_dev=0;

MUKDEBUG("BEFORE %s\n", f_conf);
cfg = config_read(f_conf, CFG_ESCAPED, cfg);
MUKDEBUG("AFTER %s\n", f_conf);

rcode = rd_search_dev(cfg);
MUKDEBUG("AFTER2 rcode=%d\n", rcode);

}

int rd_read_lines(config_t *cfg)
{
	int i;
	int rcode;
	for ( i = 0; cfg != nil; i++) {
		MUKDEBUG("rd_read_lines type=%X\n",cfg->flags); 
		rcode = rd_search_ident(cfg->list);
		if( rcode) ERROR_RETURN(rcode);
		if( cfg == nil)return(OK);
		cfg = cfg->next;
	}
	return(OK);
}


