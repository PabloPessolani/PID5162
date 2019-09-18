

#define MUKDBG		1

#ifdef MUKDBG
 #define MUKDEBUG(text, args ...) \
 do { \
     printf(" %s:%s:%u:" \
             text ,__FILE__,__FUNCTION__ ,__LINE__, ## args); \
     fflush(stdout);\
 }while(0);
#else 
#define MUKDEBUG(x, args ...) do {} while(0);
#endif 



