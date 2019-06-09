#include "tests.h"
  
extern int dvk_fd;
  
void  main ( int argc, char *argv[] )
{
	int dcid, pid, p_nr, ret, ep, rep; 
	int index=0;

	if ( argc != 3) {
		fprintf(stderr,  "Usage: %s <dcid> <p_nr> \n", argv[0] );
		exit(EXIT_FAILURE);
	}

	dcid = atoi(argv[1]);
	if ( dcid < 0 || dcid >= NR_DCS) {
		fprintf(stderr,  "Invalid dcid [0-%d]\n", NR_DCS-1 );
		exit(EXIT_FAILURE);
	}

	p_nr = atoi(argv[2]);
	pid = getpid();
    printf("UML user-mode pid=%d DVK_FILE_NAME=%s\n", pid, DVK_FILE_NAME);
	
	dvk_fd = open(DVK_FILE_NAME, 0);
	if (dvk_fd < 0)  ERROR_PRINT(dvk_fd);
    printf("UML user-mode dvk_fd=%d\n", dvk_fd);
	
    ep = dvk_bind(dcid, p_nr);
	if( ep < 0) ERROR_PRINT(ep);
	
	rep = dvk_getep(pid);
	if (rep != ep)  ERROR_PRINT(rep);
	
    printf("PARENT dvk_getep pid=%d ep=%d rep=%d dvk_fd=%d\n", pid, ep, rep, dvk_fd);

sleep(60);

#ifdef ANULADO 
    printf("PARENT dvk_bind Binding process %d to DC%d with p_nr=%d\n",pid,dcid,p_nr);
    ep = dvk_bind(dcid, p_nr);
	if( ep < EDVSERRCODE) ERROR_PRINT(ep);
	// test dvk_getep	
    printf("PARENT ep=%d dvk_getep=%d\n", ep, dvk_getep(pid));
	index++;
	
	if( (pid = fork()) == 0){ // CHILD dvk_lclbind
		printf("CHILD[%d] dvk_lclbind\n", index);
		sleep(5);
		// test dvk_getep	
		pid = getpid();
		printf("CHILD[%d] dvk_getep=%d\n",index, dvk_getep(pid));	
		sleep(60);
		printf("CHILD[%d] exit\n",index);
		exit(0);
	}
    printf("PARENT dvk_lclbind %d to DC%d with p_nr+index=%d\n",pid,dcid,p_nr+index);
	ep = dvk_lclbind(dcid,pid,p_nr+index);
	if( ep < EDVSERRCODE) ERROR_PRINT(ep);
    printf("PARENT ep=%d dvk_getep=%d\n", ep, dvk_getep(pid));
	index++;

	if( (pid = fork() == 0)){ // CHILD dvk_replbind
		pid = getpid();
	    printf("CHILD[%d] dvk_replbind Binding process %d to DC%d with p_nr=%d\n",index, 
			pid,dcid,p_nr+index);
		ep = dvk_replbind(dcid, pid, p_nr+index);
		if( ep < EDVSERRCODE) ERROR_PRINT(ep);
		// test dvk_getep	
		printf("CHILD[%d] ep=%d dvk_getep=%d\n", index, ep, dvk_getep(pid));	
		sleep(60);
		printf("CHILD[%d] exit\n",index);
		exit(0);
	}

	// HABRIA QUE TESTEAR BACKUP_BIND PERO PARA ESO TIENE QUE ESTAR EL NODO REMOTO ACTIVO
	printf("PARENT waiting child index=%d\n", index--);
	wait(&ret);

	printf("PARENT waiting child index=%d\n", index--);
	wait(&ret);
#endif //  ANULADO 
	
 }



