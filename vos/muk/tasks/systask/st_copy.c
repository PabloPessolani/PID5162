/* The kernel call implemented in this file:
 *   m_type:	SYS_VIRCOPY
 *
 * The parameters for this kernel call are:
 *    m5_l1:	CP_SRC_ADDR		source offset within userspace
 *    m5_i1:	CP_SRC_ENDPT	source process endpoint
 *    m5_l2:	CP_DST_ADDR		destination offset within userspace
 *    m5_i2:	CP_DST_ENDPT	destination process endpoint
 *    m5_l3:	CP_NR_BYTES		number of bytes to copy
 */

#include "systask.h"

#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"


#if (USE_VIRCOPY || USE_PHYSCOPY)

/*===========================================================================*
 *				st_copy					     *
 *===========================================================================*/
int st_copy(message *m_ptr)	/* pointer to request message */
{

	proc_usr_t *src_ptr, *dst_ptr;
	int src_nr, dst_nr, rcode ;
	struct vir_cp_req req, *req_ptr;


	MUKDEBUG(MSG5_FORMAT, MSG5_FIELDS(m_ptr));

/*!!!!!!!!!!!!!! VERIFICAR QUE EL M_SOURCE/REQUESTER TENGA PRIVILEGIOS !!!!!!!!!!!!!!!!!!!!*/

	req_ptr = &req;
	req_ptr->src.proc_nr_e 	= m_ptr->CP_SRC_ENDPT;
	req_ptr->dst.proc_nr_e 	= m_ptr->CP_DST_ENDPT;
	req_ptr->count  		= m_ptr->CP_NR_BYTES;	
	if( req_ptr->src.proc_nr_e == SELF) req_ptr->src.proc_nr_e = sys_who_e;
	if( req_ptr->dst.proc_nr_e == SELF) req_ptr->dst.proc_nr_e = sys_who_e;

	MUKDEBUG(VIRCP_FORMAT, VIRCP_FIELDS(req_ptr));

	/* Parameter checking	*/
	if( 	(req_ptr->src.proc_nr_e == req_ptr->dst.proc_nr_e) 	
		|| 	(req_ptr->src.proc_nr_e == ANY)	
		|| 	(req_ptr->dst.proc_nr_e == ANY) 
		||	(req_ptr->src.proc_nr_e == NONE)
		||	(req_ptr->dst.proc_nr_e == NONE) )		
			ERROR_RETURN(EDVSENDPOINT);

	src_nr = _ENDPOINT_P(req_ptr->src.proc_nr_e);
	dst_nr = _ENDPOINT_P(req_ptr->dst.proc_nr_e);

	CHECK_P_NR(src_nr);		/* check process number limits */
	CHECK_P_NR(dst_nr);		/* check process number limits */

	src_ptr = PROC2PTR(src_nr);
	MUKDEBUG("SRC " PROC_USR_FORMAT,PROC_USR_FIELDS(src_ptr));

	dst_ptr = PROC2PTR(dst_nr);
	MUKDEBUG("DST " PROC_USR_FORMAT,PROC_USR_FIELDS(dst_ptr));

	if( src_ptr->p_rts_flags == SLOT_FREE) ERROR_RETURN(EDVSSRCDIED);
	if( dst_ptr->p_rts_flags == SLOT_FREE) ERROR_RETURN(EDVSDSTDIED);

  	if ( req_ptr->count <= 0 ) 			ERROR_RETURN(EDVSINVAL);
  	if ( req_ptr->count > MAXCOPYLEN ) 	ERROR_RETURN(E2BIG);

  	/* Now try to make the actual virtual copy. */
	MUK_vcopy( rcode, req_ptr->src.proc_nr_e, m_ptr->CP_SRC_ADDR,
			req_ptr->dst.proc_nr_e, m_ptr->CP_DST_ADDR, req_ptr->count);
	return(rcode);
}

#endif /* (USE_VIRCOPY || USE_PHYSCOPY) */

