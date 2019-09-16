#include "taskimpl.h"

/*
 * sleep and wakeup
 */
void
tasksleep(Rendez *r)
{
	MUKDEBUG("\n");
	addtask(&r->waiting, taskrunning);
	if(r->l)
		qunlock(r->l);
	taskstate("sleep");
	taskswitch();
	if(r->l)
		qlock(r->l);
}

static int
_taskwakeup(Rendez *r, int all)
{
	int i;
	Task *t;

	MUKDEBUG("\n");
	for(i=0;; i++){
		if(i==1 && !all)
			break;
		if((t = r->waiting.head) == nil)
			break;
		deltask(&r->waiting, t);
		taskready(t);
	}
	MUKDEBUG("i=%d\n", i);
	return i;
}

int
taskwakeup(Rendez *r)
{
	return _taskwakeup(r, 0);
}

int
taskwakeupall(Rendez *r)
{
	return _taskwakeup(r, 1);
}

