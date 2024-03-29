#include <mollib.h>


int mol_getprocnr(void)
{
	message m __attribute__((aligned(0x1000)));
  int ret;

  m.m1_i1 = -1;			/* don't pass pid to search for */
  m.m1_i2 = 0;			/* don't pass name to search for */
  if ((ret = molsyscall(pm_ep, MOLGETPROCNR, &m) < 0)) return(ret);
  return(m.m1_i1);		/* return own process number */
}

