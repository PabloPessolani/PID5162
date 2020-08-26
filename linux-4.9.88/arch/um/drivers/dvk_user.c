
// WARNING the configuration setting CONFIG_UML_DVK is not seeing here!!!!!!!
// #ifdef CONFIG_UML_DVK 

#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <endian.h>
#include <byteswap.h>
#include <semaphore.h>

#include <os.h>
 
#define  CONFIG_UML_DVK
#define  DVS_USERSPACE

#include "um_dvk.h"
#include "uml_rdisk.h"
#include "glo_dvk.h"
#include "usr_dvk.h"

sem_t thread_sem;
sem_t kernel_sem;

int os_sem_init(sem_t *sem, unsigned int value)
{
#define BETWEEN_THREADS		0
#define BETWEEN_PROCESSES	1
	return(sem_init(sem, BETWEEN_PROCESSES, value));
}

int down_kernel(void)
{
	printk("down_kernel\n");
	return(sem_wait(&kernel_sem));
}

int down_thread(void)
{
	printk("down_thread\n");
	return(sem_wait(&thread_sem));
}

int up_kernel(void)
{
	printk("up_kernel\n");
	return(sem_post(&kernel_sem));
}

int up_thread(void)
{
	printk("up_thread\n");
	return(sem_post(&thread_sem));
}

int os_sem_post(sem_t *sem)
{
	return(sem_post(sem));	
}

int os_sem_wait(sem_t *sem)
{
	return (sem_wait(sem));
}

#ifdef ANULADO
int start_dvk_thread(unsigned long sp, int *fd_out)
{
	int pid, err;

	printk("start_dvk_thread - sp=%ld\n", sp);

	err = os_sem_init(&thread_sem, 0);
	if( err < 0) {
		err = -errno;
		printk("start_dvk_thread - os_sem_init thread_sem failed : errno = %d\n", errno);
		return err;
	}

	err = os_sem_init(&kernel_sem, 0);
	if( err < 0) {
		err = -errno;
		printk("start_dvk_thread - os_sem_init kernel_sem failed : errno = %d\n", errno);
		return err;
	}
	
	// create rd_thread 
	pid = clone(dvk_thread, (void *) sp, CLONE_FILES | CLONE_VM, NULL);
	printk("dvk_thread pid=%d\n", pid);
	if(pid < 0){
		err = -errno;
		printk("start_dvk_thread - clone failed : errno = %d\n", errno);
		return err;
	}
// ATENCION, ESTO ES PARA "SINCRONIZAR"	
    sleep(2);
	return(pid);
}
#endif // ANULADO
	
//#endif // CONFIG_UML_DVK 



