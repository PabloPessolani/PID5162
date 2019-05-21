/****************************************************************/
/*			UML MACROS   			*/
/****************************************************************/

/*----------------------------------------------------------------*/
/*				MACROS 				*/
/*----------------------------------------------------------------*/
#define ERROR_PRINT(rcode) \
 do { \
     	printk("ERROR: %d:%s:%u: rcode=%d\n",task_pid_nr(current), __FUNCTION__ ,__LINE__,rcode); \
 }while(0);
 
#define ERROR_RETURN(rcode) \
 do { \
     	printk("ERROR: %d:%s:%u: rcode=%d\n",task_pid_nr(current), __FUNCTION__ ,__LINE__,rcode); \
	return(rcode); \
 }while(0);


