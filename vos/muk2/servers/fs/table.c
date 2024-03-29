/* This file contains the table used to map system call numbers onto the
 * routines that perform them. */                                                                                                                                                                          

// #define MUKDBG    1


#include "fs.h"


int (*fs_call_vec[NCALLS])(void) = {
	fs_no_sys,        /* 0  = unused	      -FS-*/                  
	fs_no_sys,        /* 1  = exit	          -FS- LLAMA DESDE    PM */                                                                    
	fs_fork,       /* 2  = fork	          -FS- LLAMA DESDE    PM */                                                                                 
	fs_read,       /* 3  = read	          -FS-*/                                                                                
	fs_write,      /* 4  = write	      -FS-*/                                                                                
	fs_open,       /* 5  = open	          -FS-*/                                                                                
	fs_close,      /* 6  = close	      -FS-*/                                                                                
	fs_no_sys,        /* 7  = wait	                             -PM-*/                                                                                
	fs_creat,      /* 8  = creat	      -FS-*/                                                                                
	fs_link,       /* 9  = link	          -FS-*/                                                                                
	fs_unlink,     /* 10 = unlink	      -FS-*/                                                                                
	fs_no_sys,        /* 11 = waitpid	                         -PM-*/                                                                        
	fs_chdir,      /* 12 = chdir	      -FS-*/                                                                                
	fs_no_sys,        /* 13 = time	                             -PM-*/                                                                                
	fs_mknod,      /* 14 = mknod	      -FS-*/                                                                                
	fs_chmod,      /* 15 = chmod	      -FS-*/                                                                                
	fs_chown,      /* 16 = chown	      -FS-*/                                                                                
	fs_no_sys,        /* 17 = break	                         -PM-*/                                                                                
	fs_stat,       /* 18 = stat	          -FS-*/                                                                                
	fs_lseek,      /* 19 = lseek	      -FS-*/                                                                                
	fs_no_sys,        /* 20 = getpid	                         -PM-*/                                                                                
	fs_mount,      /* 21 = mount	      -FS-*/                                                                                
	fs_umount,     /* 22 = umount	      -FS-*/                                                                                
	fs_set,        /* 23 = setuid	      -FS-*/                                                                                
	fs_no_sys,        /* 24 = getuid	                         -PM-*/                                                                                
	fs_stime,      /* 25 = stime	      -FS-*/                                                                                
	fs_no_sys,        /* 26 = ptrace	                         -PM-*/                                                                                
	fs_no_sys,        /* 27 = alarm	                         -PM-*/                                                                                
	fs_fstat,      /* 28 = fstat	      -FS-*/                                                                                
	fs_no_sys,        /* 29 = pause	                         -PM-*/                                                                                
	fs_utime,      /* 30 = utime	      -FS-*/                                                                                
	fs_no_sys,        /* 31 = (stty)	                         -PM-*/                                                                                
	fs_no_sys,        /* 32 = (gtty)	                         -PM-*/                                                                                
	fs_access,     /* 33 = access	      -FS-*/                                                                                
	fs_no_sys,        /* 34 = (nice)	                         -PM-*/                                                                                
	fs_no_sys,        /* 35 = (ftime)	                         -PM-*/                                                                        
	fs_sync,       /* 36 = sync	          -FS-*/                                                                                
	fs_no_sys,        /* 37 = kill	                             -PM-*/                                                                                
	fs_rename,     /* 38 = rename	      -FS-*/                                                                                
	fs_mkdir,      /* 39 = mkdir	      -FS-*/                                                                                
	fs_unlink,     /* 40 = rmdir	      -FS-*/                                                                                
	fs_dup,        /* 41 = dup	          -FS-*/                                                                                
	fs_no_sys,        /* 42 = pipe	          -FS- FALTA*/                                                                                
	fs_no_sys,        /* 43 = times	                         -PM-*/                                                                                
	fs_no_sys,        /* 44 = (prof)	                         -PM-*/                                                                                
	fs_slink,      /* 45 = symlink	      -FS-*/                                                                        
	fs_set,        /* 46 = setgid	      -FS-*/                                                                                
	fs_no_sys,        /* 47 = getgid	                         -PM-*/                                                                                
	fs_no_sys,        /* 48 = (signal)                          -PM-*/                                                                              
	fs_rdlink,     /* 49 = readlink       -FS-*/                                                                              
	fs_lstat,      /* 50 = lstat	      -FS-*/                                                                                
	fs_no_sys,        /* 51 = (acct)	                         -PM-*/                                                                                
	fs_no_sys,        /* 52 = (phys)	                         -PM-*/                                                                                
	fs_no_sys,        /* 53 = (lock)	                         -PM-*/                                                                                
	fs_ioctl,      /* 54 = ioctl	      -FS-*/                                                                                
	fs_fcntl,      /* 55 = fcntl	      -FS-*/                                                                                
	fs_no_sys,        /* 56 = (mpx)	                         -PM-*/                                                                                
	fs_dump,        /* 57 = unused > fs_dump	      -FS-*/                                                                                
	fs_no_sys,        /* 58 = unused	      -FS-*/                                                                                
	fs_no_sys,        /* 59 = execve	                         -PM-*/                                                                                
	fs_umask,      /* 60 = umask	      -FS-*/                                                                                
	fs_chroot,     /* 61 = chroot	      -FS-*/                                                                                
	fs_setsid,     /* 62 = setsid	      -FS-*/                                                                                
	fs_no_sys,        /* 63 = getpgrp	                         -PM-*/                                                                        
	fs_no_sys,        /* 64 = KSIG: signals orig. in the kernel -PM-*/                
	fs_no_sys,        /* 65 = UNPAUSE	      -FS- LLAMA DESDE    PM */                                                                        
	fs_no_sys,        /* 66 = unused                            -PM-*/                                                                              
	fs_revive,     /* 67 = REVIVE	      -FS-*/                                                                                
	fs_no_sys,        /* 68 = TASK_REPLY	                     -PM-*/                                                                        
	fs_no_sys,        /* 69 = unused                            -PM-*/                                                                                
	fs_no_sys,        /* 70 = unused                            -PM-*/                                                                                
	fs_no_sys,        /* 71 = sigaction                         -PM-*/                                                                                        
	fs_no_sys,        /* 72 = sigsuspend                        -PM-*/                                                                        
	fs_no_sys,        /* 73 = sigpending                        -PM-*/                                                                        
	fs_no_sys,        /* 74 = sigprocmask                       -PM-*/                                                                      
	fs_no_sys,        /* 75 = sigreturn                         -PM-*/                                                                          
	fs_no_sys,        /* 76 = reboot         -FS- LLAMA DESDE    PM */                                                                   
	fs_svrctl,     /* 77 = svrctl         -FS-*/                                                                                
	fs_no_sys,        /* 78 = unused                            -PM-*/                                                                                
	fs_getsysinfo, /* 79 = getsysinfo     -FS-*/                                                                        
	fs_no_sys,        /* 80 = unused                            -PM-*/                                                                                
	fs_devctl,     /* 81 = devctl         -FS-*/                                                                                
	fs_fstatfs,    /* 82 = fstatfs        -FS-*/                                                                              
	fs_no_sys,        /* 83 = memalloc                          -PM-*/                                                                            
	fs_no_sys,        /* 84 = memfree                           -PM-*/                                                                              
	fs_no_sys,        /* 85 = select         -FS- FALTA (timers.c sys_setalarm ???)*/                                                                                
	fs_fchdir,     /* 86 = fchdir         -FS-*/                                                                                
	fs_fsync,      /* 87 = fsync          -FS-*/                                                                                  
	fs_no_sys,        /* 88 = getpriority                       -PM-*/                                                                      
	fs_no_sys,        /* 89 = setpriority                       -PM-*/                                                                      
	fs_no_sys,        /* 90 = gettimeofday                      -PM-*/                                                                    
	fs_no_sys,        /* 91 = seteuid                           -PM-*/                                                                              
	fs_no_sys,        /* 92 = setegid                           -PM-*/                                                                              
	fs_truncate,   /* 93 = truncate       -FS-*/                                                                            
	fs_ftruncate,  /* 94 = truncate       -FS-*/                                                                            
};

