#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <task.h>

int quiet;
int goal;
int buffer;

#define 	SERVER_NR	1
#define 	CLIENT_NR	2

void server_task(void *arg)
{
	int rcode, p_nr;
	Task *current_ptr;
	message 	msg, *mptr;

	p_nr = (int) arg;
	MUKDEBUG("p_nr=%d\n", p_nr);
	current_ptr = current_task();
	rcode = task_bind( current_ptr, p_nr, "server_task");
    if(rcode < 0) {
		ERROR_PRINT(rcode);
		taskexitall(0);
	}
	
	MUKDEBUG("Starting SERVER\n");
	while(TRUE){
		MUKDEBUG("before receive\n");
		mptr = &msg;
		rcode = muk_receive(ANY, mptr);
		MUKDEBUG(MSG1_FORMAT,MSG1_FIELDS(mptr));
		
		MUKDEBUG("PROCESSING...\n");	
		sleep(1);

		MUKDEBUG("before send\n");
		mptr = &msg;
		mptr->m1_i1 = 0x0A;
		mptr->m1_i2 = 0x0B;
		mptr->m1_i3 = 0x0C;
		rcode = muk_send(mptr->m_source, mptr);
		MUKDEBUG(MSG1_FORMAT,MSG1_FIELDS(mptr));
	}
}

void client_task(void *arg)
{
	int rcode, p_nr;
	Task *current_ptr;
	message 	msg, *mptr;
	
	p_nr = (int) arg;
	MUKDEBUG("p_nr=%d\n", p_nr);
	current_ptr = current_task();
	
	rcode = task_bind( current_ptr, p_nr, "client_task");
    if(rcode < 0) {
		ERROR_PRINT(rcode);
		taskexitall(0);
	}

	MUKDEBUG("Starting CLIENT\n");
	while(TRUE){
		MUKDEBUG("before send\n");
		mptr = &msg;
		mptr->m1_i1 = 0x01;
		mptr->m1_i2 = 0x02;
		mptr->m1_i3 = 0x03;
		rcode = muk_send(SERVER_NR, mptr);
		MUKDEBUG(MSG1_FORMAT,MSG1_FIELDS(mptr));
		
		MUKDEBUG("before receive\n");
		mptr = &msg;
		rcode = muk_receive(ANY, mptr);
		MUKDEBUG(MSG1_FORMAT,MSG1_FIELDS(mptr));
		
		MUKDEBUG("PROCESSING...\n");	
		sleep(1);		
	}	

}

void taskmain(int argc, char **argv)
{
	MUKDEBUG("\n");
	taskcreate(server_task, SERVER_NR , 32768);
	taskcreate(client_task, CLIENT_NR , 32768);
	
}

