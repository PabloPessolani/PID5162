
#define RHDBG		1

#ifdef RHDBG
#define RHDEBUG(txt, args ...) \
    printk("DEBUG %s:%u: " txt, __FUNCTION__ ,__LINE__, ## args)
#else
#define RHDEBUG(x, args ...)
#endif 

