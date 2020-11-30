
#ifdef DVK_GLOBAL_HERE
#define EXTERN
char *dvk_dev = UML_DVK_DEV;
#else
#define EXTERN extern
extern char *dvk_dev;
#endif


#pragma message ("_RTAP_GLOBAL_HERE=" STRINGIFY(_RTAP_GLOBAL_HERE))
#ifdef _RTAP_GLOBAL_HERE
#undef EXTERN
#define EXTERN 
#else 
#undef EXTERN
#define EXTERN extern
#endif // _RTAP_GLOBAL_HERE
	
EXTERN rtap_init_t rt_init;
EXTERN rmttap_t 	rtap; 
EXTERN switch_t 	rt_switch;
EXTERN switch_t *sw_ptr;
EXTERN rmttap_t *rt_ptr;
EXTERN message sndr_msg;
EXTERN message rcvr_msg;
EXTERN dc_usr_t  *dcu_ptr;
EXTERN int tap_dvk_fd;

extern dvs_usr_t *dvs_ptr;
extern dvs_usr_t dvs;
extern dc_usr_t  dcu;
extern const struct net_user_info rtap_user_info;