/****************************************************************/
/*			RHOSTFS MACROS   			*/
/****************************************************************/

/*----------------------------------------------------------------*/
/*				MACROS 				*/
/*----------------------------------------------------------------*/
#define ERROR_PRINT(rcode) \
 do { \
     	printk("ERROR: %s:%u: rcode=%d\n", __FUNCTION__ ,__LINE__,rcode); \
 }while(0);
 
#define ERROR_RETURN(rcode) \
 do { \
     	printk("ERROR: %s:%u: rcode=%d\n",__FUNCTION__ ,__LINE__,rcode); \
	return(rcode); \
 }while(0);


