#include <mollib.h>

int mol_stime(long *top)
{
	message m __attribute__((aligned(0x1000)));

  m.m2_l1 = *top;
  return(molsyscall(pm_ep, MOLSTIME, &m));
}
