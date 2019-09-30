/* Constants used by INIT. */

#define NO_MEM ((phys_clicks) 0)  /* returned by alloc_mem() with mem is up */

#define NR_PIDS	       32760	/* process ids range from 0 to NR_PIDS-1.
				 * (magic constant: some old applications use
				 * a 'short' instead of pid_t.)
				 */

#define PM_PID	       0	/* PM's process id number */
#define INIT_PID	   1	/* INIT's process id number */
