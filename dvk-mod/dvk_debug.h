
//define DVKDBG		1

#define NODEBUG  	0x00000000
#define DBGLVL0  	0x00000001
#define GENERIC  	0x00000002
#define INTERNAL  	0x00000004
#define DBGLVL3  	0x00000008
#define DBGLVL4  	0x00000010
#define DBGLVL5  	0x00000020
#define DBGLVL6  	0x00000040
#define DBGLVL7  	0x00000080

#define DBGPROCLOCK  0x00000100
#define DBGDCLOCK  	 0x00000200
#define DBGNODELOCK  0x00000400
#define DBGTASKLOCK  0x00000800

#define DBGMESSAGE	0x00001000
#define DBGCMD		0x00002000
#define DBGVCOPY	0x00004000
#define DBGPARAMS	0x00008000

#define DBGPROC		0x00010000
#define DBGPRIV		0x00020000
#define DBGPROCSEM	0x00040000
#define DBGNODE		0x00080000
#define DBGPROXYLOCK	0x00100000
#define DBGREFCOUNT	0x00200000


#ifdef DVKDBG

#ifdef CONFIG_DVS 
 #define DVKDEBUG(dbglvl, text, args ...) \
 do { \
 if(dbglvl & dvs.d_dbglvl) \
     printk("DEBUG %d:%s:%u: " \
             text, current->pid, __FUNCTION__ ,__LINE__, ## args); \
 }while(0);
#else //CONFIG_DVS
 #define DVKDEBUG(dbglvl, text, args ...) \
    printk("DEBUG %d:%s:%u: " text, current->pid, __FUNCTION__ ,__LINE__, ## args)
#endif //CONFIG_DVS
	 
#else
#define DVKDEBUG(x, args ...)
#endif 

#if DVKPROFILING != 0
#define DVKPROFVAR(tmp) uint64_t tmp;
#else 
#define DVKPROFVAR(tmp) 
#endif 

#if DVKPROFILING != 0
#define DVKPROFINIT(tmp) tmp = get_cicles()
#else 
#define DVKPROFINIT(tmp) 
#endif 

#if DVKPROFILING != 0
#define DVKPROFLOG(p,n) do { p->p_profiling[n]= get_cicles();  p->p_profline[n] = __LINE__;} while(0)
#else 
#define DVKPROFLOG(p,n)
#endif 

#if DVKPROFILING != 0
#define DVKPROFPRT(p,tmp) do { \
	int index;\
	p->p_profiling[0]= tmp;\
	p->p_profline[0]= 0;\
	printk("PROF %d:%s:%u:0:%lld:0\n", \
		current->pid, __FUNCTION__ ,p->p_profline[0], p->p_profiling[0]);\
	for ( index = 1; index < MAX_PROF; index++) \
	     printk("PROF %d:%s:%u:%d:%lld:%lld\n",\
		current->pid, __FUNCTION__ ,p->p_profline[index], index, p->p_profiling[index],\
		(p->p_profiling[index]- p->p_profiling[index-1]) ); \
	} while(0);
#else 
#define DVKPROFPRT(p,tmp)
#endif 








