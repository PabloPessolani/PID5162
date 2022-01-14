

#if USRDBG
 #define USRDEBUG(text, args ...) \
 do { \
     printf(" %d:%s:%s:%u:" \
             text ,(pid_t) syscall (SYS_gettid), __FILE__ ,__FUNCTION__ ,__LINE__, ## args); \
     fflush(stdout);\
 }while(0);
#else 
#define USRDEBUG(x, args ...)
#endif 



