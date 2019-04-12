/* This file provides a catch-all handler for unused kernel calls. A kernel 
 * call may be unused when it is not defined or when it is disabled in the
 * kernel's configuration.
 */
#include "systask.h"

/*===========================================================================*
 *			          st_unused				     *
 *===========================================================================*/
int st_unused(message *m)
{
  printf("SYSTASK: got unused request %d from %d\n", m->m_type, m->m_source);
  return(EDVSBADREQUEST);			/* illegal message type */
}

