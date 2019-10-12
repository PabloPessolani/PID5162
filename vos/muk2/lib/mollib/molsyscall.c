#include "mollib.h"

// int molsyscall(int who, int syscallnr, message *msgptr)
// {
//   int status;

//   msgptr->m_type = syscallnr;
//   status = muk_sendrec(who, msgptr);
//   if (status != 0) return(status);
//   return(msgptr->m_type);
// }

int molsyscall(int who, int syscallnr, message *msgptr)
{
	int status;

	msgptr->m_type = syscallnr;
	status = muk_sendrec(who, msgptr);

	if (status != 0) {
		msgptr->m_type = status;
	}

	if (msgptr->m_type < 0) {
		errno = -msgptr->m_type;
		return (-1);
	}
	return (msgptr->m_type);

}