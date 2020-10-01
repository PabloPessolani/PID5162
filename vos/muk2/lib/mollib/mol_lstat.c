#include <mollib.h>

int mol_lstat(const char *name, struct muk_stat *buffer)
{
	message m __attribute__((aligned(0x1000)));
	int r;
	LIBDEBUG("name=%s\n", name);

	m.m1_i1 = strlen(name) + 1;
	m.m1_p1 = (char *) name;
	m.m1_p2 = (char *) buffer;

	if ((r = molsyscall(fs_ep, MOLLSTAT, &m)) >= 0 || errno != EDVSNOSYS)
		return r;
	return mol_stat(name, buffer);
}
