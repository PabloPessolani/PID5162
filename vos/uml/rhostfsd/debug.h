

#define RHSDBG		1

#ifdef RHSDBG
 #define RHSDEBUG(text, args ...) \
 do { \
     printf(" %s:%s:%u:" \
             text ,__FILE__,__FUNCTION__ ,__LINE__, ## args); \
     fflush(stdout);\
 }while(0);
#else 
#define RHSDEBUG(x, args ...) do {} while(0);
#endif 



