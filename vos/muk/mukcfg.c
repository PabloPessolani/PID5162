/***********************************************
	MUK CONFIGURATION FILE
*  Sample File format
# this is a comment 
muk SERVER1 {
	dcid			0;
	pm_ep		0;
	fs_ep		1;
	is_ep			8;
	rd_ep		3;
	nweb_ep		22;
	ftp_ep		14;
	ds_ep		-1;
	tty_ep		-1;
	eth_ep		-1;
	tap_ep		-1;
	inet_ep		-1;
	fs_cfg "/xxx/fs.cfg"
	rd_cfg "/xxx/rd.cfg"
	nweb_cfg "/xxx/nweb.cfg"
	ftp_cfg "/xxx/ftp.cfg"
};
remote rs {
	endpoint	XX;
	nodeid	XX;
}
}
**************************************************/

#define DVKDBG		1

#include "muk.h"
#undef EXTERN
#define EXTERN	extern
#include "./glo.h"

#define EXIT_CODE		1
#define NEXT_CODE		2

#define TKN_DCID		0
#define TKN_PM_EP		1
#define TKN_FS_EP		2
#define TKN_IS_EP		3
#define TKN_RD_EP		4
#define TKN_NWEB_EP		5
#define TKN_FTP_EP		6
#define TKN_DS_EP		7
#define TKN_TTY_EP		8
#define TKN_ETH_EP		9
#define TKN_TAP_EP		10
#define TKN_INET_EP		11
#define TKN_FS_CFG		12
#define TKN_RD_CFG		13
#define TKN_WEB_CFG		14
#define TKN_FTP_CFG		15
#define NR_IDENT 		16


char *muk_cfg_ident[] = {
	"dcid",
	"pm_ep",
	"fs_ep",
	"is_ep",
	"rd_ep",
	"nweb_ep",
	"ftp_ep",
	"ds_ep",
	"tty_ep",
	"eth_ep",
	"tap_ep",
	"inet_ep",
	"fs_cfg",
	"rd_cfg",
	"web_cfg",
	"ftp_cfg",
};

int muk_search_ident(config_t *cfg)
{
	int ident_nr;

	MUKDEBUG("muk_mandatory=%X cfg=%p\n", muk_mandatory, cfg);

	while(cfg!=nil) {
		MUKDEBUG("cfg->line=%d\n",cfg->line); 
		if (config_isatom(cfg)) {
			MUKDEBUG("line=%d word=%s\n",cfg->line, cfg->word); 
			for( ident_nr = 0; ident_nr < NR_IDENT; ident_nr++) {	
//				MUKDEBUG("ident_nr=%d word=>%s< ident=>%s<\n",ident_nr, cfg->word, muk_cfg_ident[ident_nr]); 
				if( !strcmp(cfg->word, muk_cfg_ident[ident_nr])) {
					if( cfg->next == nil)
						fprintf(stderr, "Void value found at line %d\n", cfg->line);
					cfg = cfg->next;
					if( (ident_nr != TKN_DCID) && (TEST_BIT(muk_mandatory, TKN_DCID))){
							fprintf(stderr, "dcid must be the first parameter set: %d\n", cfg->line);
							ERROR_RETURN(EXIT_CODE);
					}
					switch(ident_nr){
						case TKN_DCID:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								ERROR_RETURN(EXIT_CODE);
							}
							MUKDEBUG("\t TKN_DCID=%d\n", atoi(cfg->word));
							CLR_BIT(muk_mandatory, TKN_DCID);					
							dcid =  atoi(cfg->word);
							if(dcid  < 0 || dcid >= dvs_ptr->d_nr_dcs)
								ERROR_RETURN(EXIT_CODE);	
							MUKDEBUG("dcid=%d\n", dcid);
							get_dc_params(dcid);							
							break;
						case TKN_PM_EP:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								ERROR_RETURN(EXIT_CODE);
							}
							MUKDEBUG("\t TKN_PM_EP=%d\n", atoi(cfg->word));
							CLR_BIT(muk_mandatory, TKN_PM_EP);					
							pm_ep =  atoi(cfg->word);
							if( pm_ep < 0 || pm_ep >= (dc_ptr->dc_nr_sysprocs - dc_ptr->dc_nr_tasks))
								ERROR_RETURN(EXIT_CODE);						
							break;
						case TKN_FS_EP:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								ERROR_RETURN(EXIT_CODE);
							}
							MUKDEBUG("\t TKN_FS_EP=%d\n", atoi(cfg->word));
							fs_ep =  atoi(cfg->word);
							if( fs_ep < 0 || fs_ep >= (dc_ptr->dc_nr_sysprocs - dc_ptr->dc_nr_tasks))
								ERROR_RETURN(EXIT_CODE);
							CLR_BIT(muk_mandatory, TKN_FS_EP);					
							SET_BIT(muk_mandatory, TKN_FS_CFG);
							break;
						case TKN_RD_EP:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								ERROR_RETURN(EXIT_CODE);
							}
							MUKDEBUG("\t TKN_RD_EP=%d\n", atoi(cfg->word));
							rd_ep =  atoi(cfg->word);
							if( rd_ep < 0 || rd_ep >= (dc_ptr->dc_nr_sysprocs - dc_ptr->dc_nr_tasks))
								ERROR_RETURN(EXIT_CODE);
							CLR_BIT(muk_mandatory, TKN_RD_EP);					
							SET_BIT(muk_mandatory,  TKN_RD_CFG);						
							break;
						case TKN_IS_EP:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								ERROR_RETURN(EXIT_CODE);
							}
							MUKDEBUG("\t TKN_IS_EP=%d\n", atoi(cfg->word));
							is_ep =  atoi(cfg->word);
							if( is_ep < 0 || is_ep >= (dc_ptr->dc_nr_sysprocs - dc_ptr->dc_nr_tasks))
								ERROR_RETURN(EXIT_CODE);						
							CLR_BIT(muk_mandatory, TKN_IS_EP);					
							break;
						case TKN_NWEB_EP:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								ERROR_RETURN(EXIT_CODE);
							}
							MUKDEBUG("\t TKN_NWEB_EP=%d\n", atoi(cfg->word));
							web_ep =  atoi(cfg->word);
							if( web_ep < 0 || web_ep >= (dc_ptr->dc_nr_procs - dc_ptr->dc_nr_tasks))
								ERROR_RETURN(EXIT_CODE);
							CLR_BIT(muk_mandatory, TKN_NWEB_EP);					
							SET_BIT(muk_mandatory, TKN_WEB_CFG);													
							break;
						case TKN_FTP_EP:
							if (!config_isatom(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								ERROR_RETURN(EXIT_CODE);
							}
							MUKDEBUG("\t TKN_FTP_EP=%d\n", atoi(cfg->word));
							ftp_ep =  atoi(cfg->word);
							if( ftp_ep < 0 || ftp_ep >= (dc_ptr->dc_nr_procs - dc_ptr->dc_nr_tasks))
								ERROR_RETURN(EXIT_CODE);	
							CLR_BIT(muk_mandatory, TKN_FTP_EP);					
							SET_BIT(muk_mandatory, TKN_FTP_CFG);												
							break;
						case TKN_FS_CFG:
							if (!config_isstring(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								ERROR_RETURN(EXIT_CODE);						
							}
							fs_cfg = cfg->word;
							MUKDEBUG("fs_cfg=%s\n", fs_cfg);
							CLR_BIT(muk_mandatory, TKN_FS_CFG);					
							break;
						case TKN_RD_CFG:
							if (!config_isstring(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								ERROR_RETURN(EXIT_CODE);						
							}
							rd_cfg = cfg->word;
							MUKDEBUG("rd_cfg=%s\n", rd_cfg);
							CLR_BIT(muk_mandatory, TKN_RD_CFG);					
							break;						
						case TKN_WEB_CFG:
							if (!config_isstring(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								ERROR_RETURN(EXIT_CODE);						
							}
							web_cfg = cfg->word;
							MUKDEBUG("web_cfg=%s\n", web_cfg);
							CLR_BIT(muk_mandatory, TKN_WEB_CFG);					
							break;	
						case TKN_FTP_CFG:
							if (!config_isstring(cfg)) {
								fprintf(stderr, "Invalid value found at line %d\n", cfg->line);
								ERROR_RETURN(EXIT_CODE);						
							}
							ftp_cfg = cfg->word;
							MUKDEBUG("ftp_cfg=%s\n", ftp_cfg);
							CLR_BIT(muk_mandatory, TKN_FTP_CFG);					
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
	MUKDEBUG("return OK\n");

	return(OK);	
}
		
int muk_read_lines(config_t *cfg)
{
	int i;
	int rcode;
	MUKDEBUG("\n");
	for ( i = 0; cfg != nil; i++) {
		// printf("muk_read_lines typedef 				=%X\n",cfg->flags); 
		rcode = muk_search_ident(cfg->list);
		if( rcode) ERROR_RETURN(rcode);
		if( cfg == nil) return(OK);
		cfg = cfg->next;
	}
	return(OK);
}	

int muk_search_token(config_t *cfg)
{
	int rcode;
	
	MUKDEBUG("\n");

    if (cfg != nil) {
		if (config_isatom(cfg)) {
			if( !strcmp(cfg->word, "muk")) {		
				cfg = cfg->next;
				if (cfg != nil) {
					if (config_isatom(cfg)) {
						MUKDEBUG("muk: %s\n", cfg->word);
						muk_name = cfg->word;
						cfg = cfg->next;
						if (!config_issub(cfg)) {
							fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n",cfg->word, cfg->line);
							return(EXIT_CODE);
						}
						rcode = muk_read_lines(cfg->list);
						return(rcode);
					}
				}
				if( muk_mandatory != 0){
					fprintf(stderr, "Configuration Error muk_mandatory=0x%X\n", muk_mandatory);
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

/*Searchs for every server configuration and stores in muk_web_child server table */
int muk_search_config(config_t *cfg)
{
	int rcode;
	int i;
	
	MUKDEBUG("\n");

	SET_BIT(muk_mandatory, TKN_DCID);
	
    for( i=0; cfg != nil; i++) {
		if (!config_issub(cfg)) {
			fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n", cfg->word, cfg->line);
			return(EXIT_CODE);
		}
		rcode = muk_search_token(cfg->list);
		if( rcode ) ERROR_RETURN(rcode);
		cfg= cfg->next;
	}
	return(OK);
}

int muk_read_config(char *file_conf)
{
    config_t *cfg;
	int rcode;
	
	#define nil ((void*)0)
	cfg= nil;
	MUKDEBUG("file_conf=%s\n", file_conf);
	cfg = config_read(file_conf, CFG_ESCAPED, cfg);
	MUKDEBUG("before  muk_search_config\n");	
	rcode = muk_search_config(cfg);
	if(rcode) ERROR_RETURN(rcode);
	return(rcode );	
}

