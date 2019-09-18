/* This file handles the 4 system calls that get and set uids and gids.
 * It also handles getpid(), setsid(), and getpgrp().  The code for each
 * one is so tiny that it hardly seemed worthwhile to make each a separate
 * function.
 */
#include "fs.h"

#undef DVK_GLOBAL_HERE
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

/*===========================================================================*
 *				fs_dump										     *
 *===========================================================================*/
int fs_dump(void)
{

	MUKDEBUG("dump type=%X\n",fs_m_ptr->m1_i1);
	
	switch(fs_m_ptr->m1_i1){
		case DMP_FS_PROC:
			dmp_fs_proc();
			break;			
		case DMP_FS_SUPER:
			dmp_fs_super();
			break;				
		case WDMP_FS_PROC:
			wdmp_fs_proc();
			break;			
		case WDMP_FS_SUPER:
			wdmp_fs_super();
			break;
		default:
			ERROR_RETURN(EDVSBADCALL);
	}
	return(OK);
}

/*===========================================================================*
 *				dmp_fs_proc				     						*
 *===========================================================================*/
int dmp_fs_proc(void)
{
	int i;
	proc_usr_t	*proc_ptr;
	fproc_t *fp_ptr;
	
	fprintf(dump_fd, "\n");
	fprintf(dump_fd,"===========================================================================\n");
	fprintf(dump_fd,"====================== dmp_fs_proc =======================================\n");
	fprintf(dump_fd,"===========================================================================\n");
	fprintf(dump_fd, "\n");
	fprintf(dump_fd, "-endp- -pid- -tty- -task -ruid -euid -rgid -egid name\n");	

	assert(fs_proc_table != NULL);
	assert(kproc_map != NULL);

	for(i = 0 ; i < dc_ptr->dc_nr_procs; i++) {
		fp_ptr = &fs_proc_table[i];
		proc_ptr = (proc_usr_t *) PROC_MAPPED(i);
		if (TEST_BIT(proc_ptr->p_rts_flags, BIT_SLOT_FREE)) {
			continue;
		}
		fprintf(dump_fd, "%6d %5d %5d %5d %5d %5d %5d %5d %-15.15s\n",
				fp_ptr->fp_endpoint,
				fp_ptr->fp_pid,
				fp_ptr->fp_tty,
				fp_ptr->fp_task,
				fp_ptr->fp_realuid,
				fp_ptr->fp_effuid,
				fp_ptr->fp_realgid,
				fp_ptr->fp_effgid,
				proc_ptr->p_name);
	}
	fprintf(dump_fd, "\n");

}

/*===========================================================================*
 *				dmp_fs_super				     						*
 *===========================================================================*/
int dmp_fs_super(void)
{
	int d;
	struct super_block *sb_ptr;
	
	fprintf(dump_fd, "\n");
	fprintf(dump_fd,"===========================================================================\n");
	fprintf(dump_fd,"====================== dmp_fs_super =======================================\n");
	fprintf(dump_fd,"===========================================================================\n");
	fprintf(dump_fd, "\n");
	fprintf(dump_fd, "dev magic ninode nzones iblock zblock 1data maxsiz bzize inoblk dzones izones ver\n");	
	
	assert(super_block != NULL);

	for(d = 0 ; d < NR_SUPERS; d++) {
		sb_ptr = &super_block[d];
		if (sb_ptr->s_dev == NO_DEV) {
			continue;
		}
		fprintf(dump_fd, "%3X %5X %6d %6d %6d %6d %5d %6d %6d %6d %6d %6d %X\n",
				sb_ptr->s_dev,
				sb_ptr->s_magic,
				sb_ptr->s_ninodes,
				sb_ptr->s_nzones,
				sb_ptr->s_imap_blocks,
				sb_ptr->s_zmap_blocks,
				sb_ptr->s_firstdatazone,
				sb_ptr->s_max_size,
				sb_ptr->s_block_size,
				sb_ptr->s_inodes_per_block,
				sb_ptr->s_ndzones,
				sb_ptr->s_nindirs,
				sb_ptr->s_version);
	}
	fprintf(dump_fd, "\n");

}

/*===========================================================================*
 *				wdmp_fs_proc				     						*
 *===========================================================================*/
int wdmp_fs_proc(void)
{
	int i;
	proc_usr_t	*proc_ptr;
	fproc_t *fp_ptr;
	char *page_ptr;
	
	page_ptr = fs_m_ptr->m1_p1;
	MUKDEBUG("strlen(page_ptr)=%d\n", strlen(page_ptr));
	
	(void)strcat(page_ptr,"<table>\n");
	(void)strcat(page_ptr,"<thead>\n");
	(void)strcat(page_ptr,"<tr>\n");
	(void)strcat(page_ptr,"<th>-endp-</th>\n");
	(void)strcat(page_ptr,"<th>-pid-</th>\n");
	(void)strcat(page_ptr,"<th>-tty-</th>\n");
	(void)strcat(page_ptr,"<th>-task</th>\n");
	(void)strcat(page_ptr,"<th>-ruid</th>\n");
	(void)strcat(page_ptr,"<th>-euid</th>\n");
	(void)strcat(page_ptr,"<th>-rgid</th>\n");
	(void)strcat(page_ptr,"<th>-egid</th>\n");
	(void)strcat(page_ptr,"<th>name</th>\n");
	(void)strcat(page_ptr,"</tr>\n");
	(void)strcat(page_ptr,"</thead>\n");
	(void)strcat(page_ptr,"<tbody>\n");
		
// assert(fs_proc_table != NULL);
//	assert(kproc_map != NULL);

	MUKDEBUG("\n");
	
	for(i = 0 ; i < dc_ptr->dc_nr_procs; i++) {
		fp_ptr = &fs_proc_table[i];
		proc_ptr = (proc_usr_t *) PROC_MAPPED(i);
		if (TEST_BIT(proc_ptr->p_rts_flags, BIT_SLOT_FREE)) {
			continue;
		}
		(void)strcat(page_ptr,"<tr>\n");
		sprintf(is_buffer, "<td>%6d</td> <td>%5d</td> <td>%5d</td> <td>%5d</td> "
				"<td>%5d</td> <td>%5d</td> <td>%5d</td> <td>%5d</td> <td>%-15.15s</td>\n",
				fp_ptr->fp_endpoint,
				fp_ptr->fp_pid,
				fp_ptr->fp_tty,
				fp_ptr->fp_task,
				fp_ptr->fp_realuid,
				fp_ptr->fp_effuid,
				fp_ptr->fp_realgid,
				fp_ptr->fp_effgid,
				proc_ptr->p_name);
		(void)strcat(page_ptr,is_buffer);
		(void)strcat(page_ptr,"</tr>\n");				
	}
  (void)strcat(page_ptr,"</tbody></table>\n");
}

/*===========================================================================*
 *				wdmp_fs_super				     						*
 *===========================================================================*/
int wdmp_fs_super(void)
{
	char *page_ptr;
	int d;
	struct super_block *sb_ptr;
	
	MUKDEBUG("\n");
	page_ptr = fs_m_ptr->m1_p1;

	(void)strcat(page_ptr,"<table>\n");
	(void)strcat(page_ptr,"<thead>\n");
	(void)strcat(page_ptr,"<tr>\n");
	(void)strcat(page_ptr,"<th>dev</th>\n");
	(void)strcat(page_ptr,"<th>magic</th>\n");
	(void)strcat(page_ptr,"<th>ninode</th>\n");
	(void)strcat(page_ptr,"<th>nzones</th>\n");
	(void)strcat(page_ptr,"<th>iblock</th>\n");
	(void)strcat(page_ptr,"<th>zblock</th>\n");
	(void)strcat(page_ptr,"<th>1data</th>\n");
	(void)strcat(page_ptr,"<th>maxsiz</th>\n");
	(void)strcat(page_ptr,"<th>bzize</th>\n");
	(void)strcat(page_ptr,"<th>inoblk</th>\n");
	(void)strcat(page_ptr,"<th>dzones</th>\n");
	(void)strcat(page_ptr,"<th>izones</th>\n");
	(void)strcat(page_ptr,"<th>ver</th>\n");
	(void)strcat(page_ptr,"</tr>\n");
	(void)strcat(page_ptr,"</thead>\n");
	(void)strcat(page_ptr,"<tbody>\n");
	
	assert(super_block != NULL);

	for(d = 0 ; d < NR_SUPERS; d++) {
		sb_ptr = &super_block[d];
		if (sb_ptr->s_dev == NO_DEV) {
			continue;
		}
		(void)strcat(page_ptr,"<tr>\n");

		sprintf(is_buffer, "<td>%3X</td> <td>%5X</td> <td>%6d</td> <td>%6d</td> "
			"<td>%6d</td> <td>%6d</td> <td>%5d</td> <td>%6d</td> <td>%6d</td> "
			"<td>%6d</td> <td>%6d</td> <td>%6d</td> <td>%X</td>\n",
				sb_ptr->s_dev,
				sb_ptr->s_magic,
				sb_ptr->s_ninodes,
				sb_ptr->s_nzones,
				sb_ptr->s_imap_blocks,
				sb_ptr->s_zmap_blocks,
				sb_ptr->s_firstdatazone,
				sb_ptr->s_max_size,
				sb_ptr->s_block_size,
				sb_ptr->s_inodes_per_block,
				sb_ptr->s_ndzones,
				sb_ptr->s_nindirs,
				sb_ptr->s_version);
		(void)strcat(page_ptr,is_buffer);			
		(void)strcat(page_ptr,"</tr>\n");				
	}
  (void)strcat(page_ptr,"</tbody></table>\n");
}
