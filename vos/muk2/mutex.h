
#define MTX_LOCK(x) do{ \
		MUKDEBUG("MTX_LOCK %s:%d\n", #x, x);\
		x--;\
		}while(0)
			
#define MTX_UNLOCK(x) do{ \
		x++;\
		MUKDEBUG("MTX_UNLOCK %s:%d\n", #x, x);\
		}while(0)	
			
#define COND_WAIT(x,y) do{ \
		MUKDEBUG("COND_WAIT %s %s\n", #x,#y );\
		MTX_UNLOCK(y);\
		tasksleep(&x);\
		MTX_LOCK(y);\
		}while(0)	
 
#define COND_SIGNAL(x) do{ \
		taskwakeup(&x);\
		MUKDEBUG("COND_SIGNAL %s\n", #x);\
		}while(0)
			
/*---------------------------- PHTHREAD MUTEX ----------------------------------------*/

#define PH_MTX_LOCK(x) do{ \
		MUKDEBUG("PH_MTX_LOCK %s \n", #x);\
		pthread_mutex_lock(&x);\
		}while(0)
			
#define PH_MTX_UNLOCK(x) do{ \
		pthread_mutex_unlock(&x);\
		MUKDEBUG("PH_MTX_UNLOCK %s \n", #x);\
		}while(0)
			
#define PH_COND_WAIT(x,y) do{ \
		MUKDEBUG("PH_COND_WAIT %s %s\n", #x,#y );\
		pthread_cond_wait(&x, &y);\
		}while(0)	
 
#define PH_COND_SIGNAL(x) do{ \
		pthread_cond_signal(&x);\
		MUKDEBUG("PH_COND_SIGNAL %s\n", #x);\
		}while(0)
			