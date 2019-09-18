/* Function prototypes for the system library.  The prototypes in this file 
 * are undefined to st_unused if the kernel call is not enabled in config.h. 
 * The implementation is contained in src/kernel/system/.  
 *
 * The system library allows to access system services by doing a kernel call.
 * System calls are transformed into request messages to the SYS task that is 
 * responsible for handling the call. By convention, sys_call() is transformed 
 * into a message with type SYS_CALL that is handled in a function st_call(). 
 * 
 * Changes:
 *   Jul 30, 2005   created SYS_INT86 to support BIOS driver  (Philip Homburg) 
 *   Jul 13, 2005   created SYS_PRIVCTL to manage services  (Jorrit N. Herder) 
 *   Jul 09, 2005   updated SYS_KILL to signal services  (Jorrit N. Herder) 
 *   Jun 21, 2005   created SYS_NICE for nice(2) kernel call  (Ben J. Gras)
 *   Jun 21, 2005   created SYS_MEMSET to speed up exec(2)  (Ben J. Gras)
 *   Apr 12, 2005   updated SYS_VCOPY for virtual_copy()  (Jorrit N. Herder)
 *   Jan 20, 2005   updated SYS_COPY for virtual_copy()  (Jorrit N. Herder)
 *   Oct 24, 2004   created SYS_GETKSIG to support PM  (Jorrit N. Herder) 
 *   Oct 10, 2004   created handler for unused calls  (Jorrit N. Herder) 
 *   Sep 09, 2004   updated SYS_EXIT to let services exit  (Jorrit N. Herder) 
 *   Aug 25, 2004   rewrote SYS_SETALARM to clean up code  (Jorrit N. Herder)
 *   Jul 13, 2004   created SYS_SEGCTL to support drivers  (Jorrit N. Herder) 
 *   May 24, 2004   created SYS_SDEVIO to support drivers  (Jorrit N. Herder) 
 *   May 24, 2004   created SYS_GETINFO to retrieve info  (Jorrit N. Herder) 
 *   Apr 18, 2004   created SYS_VDEVIO to support drivers  (Jorrit N. Herder) 
 *   Feb 24, 2004   created SYS_IRQCTL to support drivers  (Jorrit N. Herder) 
 *   Feb 02, 2004   created SYS_DEVIO to support drivers  (Jorrit N. Herder) 
 */ 

#ifndef SYSTEM_H
#define SYSTEM_H

/* Common includes for the system library. */
//#include "debug.h"
//#include "kernel.h"
//#include "proto.h"
//#include "proc_table.h"

#define printk printf


/* Default handler for unused kernel calls. */
_PROTOTYPE( int st_unused, (message *m_ptr) );

_PROTOTYPE( int st_exec, (message *m_ptr) );		
#if ! USE_EXEC
#define st_exec st_unused
#endif

_PROTOTYPE( int st_fork, (message *m_ptr) );
#if ! USE_FORK
#define st_fork st_unused
#endif

_PROTOTYPE( int st_rexec, (message *m_ptr) );
#if ! USE_REXEC
#define st_rexec st_unused
#endif

_PROTOTYPE( int st_newmap, (message *m_ptr) );
#if ! USE_NEWMAP
#define st_newmap st_unused
#endif

_PROTOTYPE( int st_exit, (message *m_ptr) );
#if ! USE_EXIT
#define st_exit st_unused
#endif

_PROTOTYPE( int st_trace, (message *m_ptr) );	
#if ! USE_TRACE
#define st_trace st_unused
#endif

_PROTOTYPE( int st_nice, (message *m_ptr) );
#if ! USE_NICE
#define st_nice st_unused
#endif

_PROTOTYPE( int st_copy, (message *m_ptr) );	
#define st_vircopy 	st_copy
#define st_physcopy 	st_copy
#if ! (USE_VIRCOPY || USE_PHYSCOPY)
#define st_copy st_unused
#endif

_PROTOTYPE( int st_vcopy, (message *m_ptr) );		
#define st_virvcopy 	st_vcopy
#define st_physvcopy 	st_vcopy
#if ! (USE_VIRVCOPY || USE_PHYSVCOPY)
#define st_vcopy st_unused
#endif

_PROTOTYPE( int st_umap, (message *m_ptr) );
#if ! USE_UMAP
#define st_umap st_unused
#endif

_PROTOTYPE( int st_memset, (message *m_ptr) );
#if ! USE_MEMSET
#define st_memset st_unused
#endif

_PROTOTYPE( int st_dc_setbuf, (message *m_ptr) );
_PROTOTYPE( int st_dc_map, (message *m_ptr) );

_PROTOTYPE( int st_abort, (message *m_ptr) );
#if ! USE_ABORT
#define st_abort st_unused
#endif

_PROTOTYPE( int st_getinfo, (message *m_ptr) );
#if ! USE_GETINFO
#define st_getinfo st_unused
#endif

_PROTOTYPE( int st_privctl, (message *m_ptr) );	
#if ! USE_PRIVCTL
#define st_privctl st_unused
#endif

_PROTOTYPE( int st_segctl, (message *m_ptr) );
#if ! USE_SEGCTL
#define st_segctl st_unused
#endif

_PROTOTYPE( int st_irqctl, (message *m_ptr) );
#if ! USE_IRQCTL
#define st_irqctl st_unused
#endif

_PROTOTYPE( int st_devio, (message *m_ptr) );
#if ! USE_DEVIO
#define st_devio st_unused
#endif

_PROTOTYPE( int st_vdevio, (message *m_ptr) );
#if ! USE_VDEVIO
#define st_vdevio st_unused
#endif

_PROTOTYPE( int st_int86, (message *m_ptr) );

_PROTOTYPE( int st_sdevio, (message *m_ptr) );
#if ! USE_SDEVIO
#define st_sdevio st_unused
#endif

_PROTOTYPE( int st_kill, (message *m_ptr) );
#if ! USE_KILL
#define st_kill st_unused
#endif

_PROTOTYPE( int st_getksig, (message *m_ptr) );
#if ! USE_GETKSIG
#define st_getksig st_unused
#endif

_PROTOTYPE( int st_endksig, (message *m_ptr) );
#if ! USE_ENDKSIG
#define st_endksig st_unused
#endif

_PROTOTYPE( int st_sigsend, (message *m_ptr) );
#if ! USE_SIGSEND
#define st_sigsend st_unused
#endif

_PROTOTYPE( int st_sigreturn, (message *m_ptr) );
#if ! USE_SIGRETURN
#define st_sigreturn st_unused
#endif

_PROTOTYPE( int st_times, (message *m_ptr) );		
#if ! USE_TIMES
#define st_times st_unused
#endif

_PROTOTYPE( int st_setalarm, (message *m_ptr) );	
#if ! USE_SETALARM
#define st_setalarm st_unused
#endif

_PROTOTYPE( int st_killed, (message *m_ptr) );
#if ! USE_KILLED
#define st_killed st_unused
#endif

_PROTOTYPE( int st_bindproc, (message *m_ptr) );
#if ! USE_BINDPROC
#define st_bindproc st_unused
#endif

_PROTOTYPE( int st_migrproc, (message *m_ptr) );
#if ! USE_MIGRPROC
#define st_migrproc st_unused
#endif

_PROTOTYPE( int st_setpname, (message *m_ptr) );
#if ! USE_SETPNAME
#define st_setpname st_unused
#endif

_PROTOTYPE( int st_iopenable, (message *m_ptr) );	


_PROTOTYPE( int st_unbind, (message *m_ptr) );
#if ! USE_UNBIND
#define st_unbind st_unused
#endif

_PROTOTYPE( int st_dump, (message *m_ptr) );
#if ! USE_SYSDUMP
#define st_dump st_unused
#endif

#endif	/* SYSTEM_H */

