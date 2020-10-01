/***********************************************
	TEST CONFIGURATION FILE
*  Sample File format *

# this is a comment 
device tap01{
	minor			0;
	mac_addr			12:34:56:78:90:12
};

device tap02{
	minor			1;
	mac_addr			12:34:56:78:90:12
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

#define YES				1
#define NO				0

#define TKN_MINOR		0
#define TKN_MACADDR 	1

#define nil ((void*)0)

char *tap_cfg_ident[] = {
	"minor",
	"mac_addr",
};
#define NR_IDENT 2

int tap_bit_flag;

int tap_search_type(config_t *cfg)
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
			if( !strcmp(cfg->word, tap_dev_type[j].f_name)) {
				flags |= tap_dev_type[j].f_value;
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

int tap_search_bit(config_t *cfg)
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
			if( !strcmp(cfg->word, tap_dev_bit[j].f_name)) {
				flags |= tap_dev_bit[j].f_value;
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

		if ( !strcmp(cfg->word, tap_dev_bit[0].f_name)){
			tap_bit_flag = YES;
			MUKDEBUG("tap_dev_bit[0].f_name=%s, tap_bit_flag=%d\n", tap_dev_bit[0].f_name, tap_bit_flag);
			}
		else{
			tap_bit_flag = NO;
			MUKDEBUG("tap_dev_bit[0].f_name=%s, tap_bit_flag=%d\n", tap_dev_bit[0].f_name, tap_bit_flag);
		}
			
	}
		
	return(OK);
}

int tap_search_ident(config_t *cfg)
{
	int i, j, rcode;
	
	for( i = 0; cfg!=nil; i++) {
		if (config_isatom(cfg)) {
			MUKDEBUG("tap_search_ident[%d] line=%d word=%s\n",i,cfg->line, cfg->word); 
			for( j = 0; j < NR_IDENT; j++) {
				if( !strcmp(cfg->word, tap_cfg_ident[j])) {
					MUKDEBUG("line[%d] MATCH identifier %s\n", cfg->line, cfg->word); 
					if( cfg->next == nil)
						fprintf(stderr, "Void value found at line %d\n", cfg->line);
					cfg = cfg->next;				
					switch(j){
						case TKN_MINOR:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							MUKDEBUG("minor=%d\n", atoi(cfg->word));
							minor_dev=atoi(cfg->word);
							MUKDEBUG("minor_dev=%d\n", minor_dev);
							
							if ((minor_dev < 0) || (minor_dev >= TAP_PORT_NR_MAX)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								fprintf(stderr, "Minor number %d, must be > 0 and < TAP_PORT_NR_MAX(%d)\n", minor_dev,TAP_PORT_NR_MAX);
								minor_dev = -1;
								}
							break;
						case TKN_MACADDR:
							if (!config_isstring(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								return(EXIT_CODE);
							}
							MUKDEBUG("mac_addr=%s\n", cfg->word);
#ifdef ANULADO
							if (minor_dev == -1){
								fprintf(stderr, "Check the minor device number for %s in the configure file\n", cfg->word);
							} else{
								devvec[minor_dev].img_ptr=(cfg->word);
								devvec[minor_dev].available = 1;
								MUKDEBUG("devvec[%d].img_ptr=%s\n", minor_dev,devvec[minor_dev].img_ptr);
								MUKDEBUG("devvec[%d].available=%d\n", minor_dev,devvec[minor_dev].available);
							}
#endif // ANULADO
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
						rcode = tap_read_lines(cfg->list);
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

int tap_search_dev(config_t *cfg)
{
	int rcode;
	int i;
	
    for( i=0; cfg != nil; i++) {
		if (!config_issub(cfg)) {
			fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n", cfg->word, cfg->line);
			return(EXIT_CODE);
		}
		MUKDEBUG("tap_search_dev[%d] line=%d\n",i,cfg->line);
		rcode = search_device_tkn(cfg->list);
		if( rcode == EXIT_CODE)
			return(rcode);
		tap_dev_nr++;
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

rcode = tap_search_dev(cfg);
MUKDEBUG("AFTER2 rcode=%d\n", rcode);

}

int tap_read_lines(config_t *cfg)
{
	int i;
	int rcode;
	for ( i = 0; cfg != nil; i++) {
		MUKDEBUG("tap_read_lines type=%X\n",cfg->flags); 
		rcode = tap_search_ident(cfg->list);
		if( rcode) ERROR_RETURN(rcode);
		if( cfg == nil)return(OK);
		cfg = cfg->next;
	}
	return(OK);
}


