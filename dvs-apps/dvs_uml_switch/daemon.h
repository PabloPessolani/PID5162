/*
 * Copyright (C) 2001 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#ifndef __DAEMON_H__
#define __DAEMON_H__

void daemon_init(rmttap_t *rt_ptr);
static int connect_to_switch(rmttap_t *rt_ptr);
static struct sockaddr_un *new_addr(void *name, int len);
int daemon_user_init(rmttap_t *rt_ptr);
#endif
