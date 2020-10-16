/* Copyright 2020 Pablo Pessolani
 * Licensed under the GPL
 */

#ifndef __RMTTAP_H__
#define __RMTTAP_H__

extern int open_rmttap(rmttap_t *r_ptr);
extern void handle_rmttap(int fd, int hub);
extern void get_dvs_params(void);
extern void get_dc_params(int dcid);
extern void *send_thread(void *arg);
extern void *recv_thread(void *arg);
extern void init_rt_threads( void);
extern void init_rt_sockets( void);
extern void rmttap_cleanup(void);
extern void send_rmttap(int fd, void *packet, int len, void *unused);
#endif
