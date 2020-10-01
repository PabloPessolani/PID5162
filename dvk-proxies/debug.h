


#if PXYDBG
 #define PXYDEBUG(text, args ...) \
 do { \
     printf(" %s:%s:%u:" \
             text ,__FILE__ ,__FUNCTION__ ,__LINE__, ## args); \
     fflush(stdout);\
 }while(0);
#else 
#define PXYDEBUG(x, args ...)
#endif 



