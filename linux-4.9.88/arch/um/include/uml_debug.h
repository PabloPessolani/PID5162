
#define UMLDBG		1

#ifdef UMLDBG
#define UMLDEBUG(txt, args ...) \
    printf("DEBUG %s:%u: " txt, __FUNCTION__ ,__LINE__, ## args)
#else
#define UMLDEBUG(x, args ...)
#endif 

