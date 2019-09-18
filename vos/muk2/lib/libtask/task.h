/* Copyright (c) 2005 Russ Cox, MIT; see COPYRIGHT */

#ifndef _TASK_H_
#define _TASK_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <inttypes.h>
#include <time.h>
#include <stdio.h>

#include "/usr/src/dvs/include/com/com.h"
#include "/usr/src/dvs/include/com/ipc.h"
#include "/usr/src/dvs/include/com/proc_sts.h"
#include "/usr/src/dvs/include/com/proxy_sts.h"
#include "/usr/src/dvs/include/com/dvs_config.h"
#include "/usr/src/dvs/include/com/config.h"
#include "/usr/src/dvs/include/com/dvs_errno.h"
#include "/usr/src/dvs/include/com/endpoint.h"



#define TRUE	1
#define FALSE	0
#define OK		0

#define MAXPROCNAME	16

#include "../../include/libtask.h"
/*
 * basic procs and threads
 */


#ifdef __cplusplus
}
#endif
#endif

