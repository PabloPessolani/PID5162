#include <mollib.h>


int mol_rmdir(const char *name)
{
	message m __attribute__((aligned(0x1000)));

	LIBDEBUG("INICIO %s\n", __func__);
	mol_loadname(name, &m);
	LIBDEBUG("FIN %s\n", __func__);

	return (molsyscall(fs_ep, MOLRMDIR, &m));
}