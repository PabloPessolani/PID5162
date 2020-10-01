/*
 * tap.c
 *
 * This file contains a TAP device driver 
  * The valid messages and their parameters are:
 *
 *   m_type       DL_PORT    DL_PROC   DL_COUNT   DL_MODE   DL_ADDR
 * |------------+----------+---------+----------+---------+---------|
 * | HARDINT    |          |         |          |         |         |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_WRITE   | port nr  | proc nr | count    | mode    | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_WRITEV  | port nr  | proc nr | count    | mode    | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_READ    | port nr  | proc nr | count    |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_READV   | port nr  | proc nr | count    |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_INIT    | port nr  | proc nr | mode     |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_GETSTAT | port nr  | proc nr |          |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_STOP    | port_nr  |         |          |         |         |
 * |------------|----------|---------|----------|---------|---------|
 *
 * The messages sent are:
 *
 *   m-type       DL_POR T   DL_PROC   DL_COUNT   DL_STAT   DL_CLCK
 * |------------|----------|---------|----------|---------|---------|
 * |DL_TASK_REPL| port nr  | proc nr | rd-count | err|stat| clock   |
 * |------------|----------|---------|----------|---------|---------|
 *
 *   m_type       m3_i1     m3_i2       m3_ca1
 * |------------+---------+-----------+---------------|
 * |DL_INIT_REPL| port nr | last port | ethernet addr |
 * |------------|---------|-----------|---------------|
 *
 * Created: Jul 27, 2002 by Kazuya Kodama <kazuya@nii.ac.jp>
 * Adapted for Minix 3: Sep 05, 2005 by Joren l'Ami <jwlami@cs.vu.nl>
 * Adapted for Mol 3: Feb 01, 2019 by Pablo Pessolani <ppessolani@hotmail.com>
 */

 
#include "tap.h"

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>

#define IFCONFIG_BIN "/sbin/ifconfig "

#if defined(linux)
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#define DEVTAP "/dev/net/tun"
#define IFCONFIG_ARGS "%s inet %d.%d.%d.%d netmask %d.%d.%d.%d"
#elif defined(openbsd)
#define DEVTAP "/dev/tun0"
#define IFCONFIG_ARGS "tun0 inet %d.%d.%d.%d link0"
#else /* others */
#define DEVTAP "/dev/tap0"
#define IFCONFIG_ARGS "tap0 inet %d.%d.%d.%d"
#endif

#define IFNAME0 't'
#define IFNAME1 'p'

#ifndef TAPIF_DEBUG
#define TAPIF_DEBUG LWIP_DBG_OFF
#endif

#define TAP_PORT_NR_MAX 2

int		tap_ep;
int 	tap_lpid; 
int		mayor_dev = (-1);
int		local_nodeid;
dvs_usr_t	dvs, *dvs_ptr;
dc_usr_t	dcu, *dc_ptr;
proc_usr_t	tap_proc, *tap_ptr;
int		endp_flag;

tap_card_t tap_table[TAP_PORT_NR_MAX];
int tap_tasknr = ANY;

#define DCID	0 // temporal 

/*===========================================================================*
 *                            tap_task                                     *
 *===========================================================================*/
void main( int argc, char **argv )
{
	message m;
	int rcode;

	if((tap_tasknr=muk_getep(getpid())) < EDVSERRCODE){
		TASKDEBUG("muk_getep:%d\n", tap_tasknr);
		ERROR_EXIT(tap_tasknr);
	}
	
	rcode = muk_open();
	if (rcode < 0)  ERROR_EXIT(rcode);
	
	rcode = tap_init();
	if(rcode) ERROR_EXIT(rcode);  
  
	/* Try to notify inet that we are present (again) */
	rcode = muk_wait4bindep_T(INET_PROC_NR, 1);
	TASKDEBUG("muk_wait4bindep_T:%d\n", rcode);
	if( rcode == INET_PROC_NR){
		muk_notify(INET_PROC_NR);
	}
	
	while (TRUE)    {
	
		if ((rcode = muk_receive(ANY, &m)) != OK){
			ERROR_EXIT(rcode);
        }
		
		switch (m.m_type){
			case DEV_PING:   
				if ((rcode = muk_notify(m.m_source))!= OK){
					ERROR_EXIT(rcode);
				}
				continue;
			case DL_WRITE:   do_vwrite(&m);   	break;
			case DL_WRITEV:  do_vwrite(&m);    	break;
			case DL_READ:    do_vread(&m);      break;
			case DL_READV:   do_vread(&m);		break;
			case DL_INIT:    do_init(&m);       break;
			case DL_GETSTAT: do_getstat(&m);    break;
			case DL_STOP:    do_stop(&m);       break;
			case DL_GETNAME: do_getname(&m); 	break;
//			case FKEY_PRESSED: lance_dump();                break;
//			case HARD_STOP:  lance_stop();               	break;
//		  	case PROC_EVENT:					break;
		  default:
			TASKDEBUG("illegal message:%d\n", m.m_type);
			ERROR_EXIT(m.m_type);
			break;
		}
	}
}

/*==========================================================================*
*                              do_vwrite                                  							*
*===========================================================================*/
 void do_vwrite(message *mp){

		int port, count;
		tap_card_t *tc;

		TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(mp));	
		
		port = mp->DL_PORT;
		count = mp->DL_COUNT;
		tc = &tap_table[port];
		tc->client= mp->DL_PROC;

		/* convert the message to write_iovec */
		if (mp->m_type == DL_WRITEV) {
			get_userdata(mp->DL_PROC, (vir_bytes) mp->DL_ADDR,
					   (count > IOVEC_NR ? IOVEC_NR : count) *
					   sizeof(iovec_t), tc->write_iovec.iod_iovec);

			tc->write_iovec.iod_iovec_s    = count;
			tc->write_iovec.iod_proc_nr    = mp->DL_PROC;
			tc->write_iovec.iod_iovec_addr = (vir_bytes) mp->DL_ADDR;

			tc->tmp_iovec = tc->write_iovec;
			tc->write_s = calc_iovec_size(&tc->tmp_iovec);
		} else{  
			tc->write_iovec.iod_iovec[0].iov_addr = (vir_bytes) mp->DL_ADDR;
			tc->write_iovec.iod_iovec[0].iov_size = mp->DL_COUNT;

			tc->write_iovec.iod_iovec_s    = 1;
			tc->write_iovec.iod_proc_nr    = mp->DL_PROC;
			tc->write_iovec.iod_iovec_addr = 0;

			tc->write_s = mp->DL_COUNT;
		}

		/* copy write_iovec to the slot on DMA address */
		tc_user2tap(tc, &tc->write_iovec, 0, tc->write_s);
				  		
		tc->flags |= TAPF_PACK_SEND;

		reply(tc, OK, FALSE);
}

/*===========================================================================*
 *                              calc_iovec_size                              *
 *===========================================================================*/
int calc_iovec_size(iovec_dat_t *iovp)
{
	int size,i;

  	TASKDEBUG("\n");	

	size = i = 0;
        
	while (i < iovp->iod_iovec_s)  {
		if (i >= IOVEC_NR) {
			tc_next_iovec(iovp);
			i= 0;
			continue;
		}
		size += iovp->iod_iovec[i].iov_size;
		i++;
    }

	return size;
}

/*===========================================================================*
 *                          tc_next_iovec                                   *
 *===========================================================================*/
void tc_next_iovec(iovec_dat_t *iovp)
{
  	TASKDEBUG("\n");	

	iovp->iod_iovec_s -= IOVEC_NR;
	iovp->iod_iovec_addr += IOVEC_NR * sizeof(iovec_t);

	get_userdata(iovp->iod_proc_nr, iovp->iod_iovec_addr, 
               (iovp->iod_iovec_s > IOVEC_NR ? 
                IOVEC_NR : iovp->iod_iovec_s) * sizeof(iovec_t), 
               iovp->iod_iovec); 
}

/*===========================================================================*
 *                              tc_user2tap                                  *
 *===========================================================================*/
void tc_user2tap (tap_card_t *tc, iovec_dat_t *iovp, 
	     vir_bytes offset, vir_bytes tap_addr,  vir_bytes count)
{
	/*phys_bytes phys_hw, phys_user;*/
	int bytes, i, r;

	TASKDEBUG("%s count=%d \n", tc->port_name, count);	
	
	i= 0;
	while (count > 0)   {
		if (i >= IOVEC_NR) {
			tc_next_iovec(iovp);
			i= 0;
			continue;
        }
		if (offset >= iovp->iod_iovec[i].iov_size) {
			offset -= iovp->iod_iovec[i].iov_size;
			i++;
			continue;
        }
		bytes = iovp->iod_iovec[i].iov_size - offset;
		if (bytes > count)
			bytes = count;
      
		if ( (r= muk_vcopy(iovp->iod_proc_nr, iovp->iod_iovec[i].iov_addr + offset,
				SELF, tap_addr, count )) != OK )
			ERROR_EXIT(r);
      	
		count -= bytes;
		tap_addr += bytes;
		offset += bytes;
    }
}


/*===========================================================================*
 *                              reply                                        *
 *===========================================================================*/
void reply(tap_card_t *tc, int err, int may_block)
{
	message reply;
	int status,r;
	struct sysinfo si;

	TASKDEBUG("%s err=%d may_block=%d\n", tc->port_name,err, may_block);	

	status = 0;
	if (tc->flags & TAPF_PACK_SEND)
		status |= DL_PACK_SEND;
	if (tc->flags & TAPF_PACK_RECV)
		status |= DL_PACK_RECV;

	reply.m_type   = DL_TASK_REPLY;
	reply.DL_PORT  = tc->nr;
	reply.DL_PROC  = tc->client;
	reply.DL_STAT  = status | ((u32_t) err << 16);
	reply.DL_COUNT = tc->read_s;
	if ((r= sysinfo(&si)) != OK){
		TASKDEBUG("sysinfo() failed:%d", r);
		ERROR_EXIT(r);
	}
	reply.DL_CLCK = si.uptime; 

	r = muk_send(tc->client, &reply);
	if (r == EDVSLOCKED && may_block) {
		  return;
	}
	if (r < 0) 	
		ERROR_EXIT(r);
		
	tc->read_s = 0;
	tc->flags &= ~(TAPF_PACK_SEND | TAPF_PACK_RECV);
}


/*===========================================================================*
 *                              get_userdata                                 *
 *===========================================================================*/
void get_userdata(int user_proc, vir_bytes user_addr, vir_bytes count, void *loc_addr)
{
	int cps;
	TASKDEBUG("user_proc=%d count=%d \n", user_proc, count);	

	cps = muk_vcopy(user_proc, user_addr, SELF, (vir_bytes) loc_addr, count);
	if (cps != OK) {
		TASKDEBUG("Warning, scopy failed: %d\n", cps);
		ERROR_PRINT(cps);
	}
}

/*===========================================================================*
 *                              do_vread                                     *
 *===========================================================================*/
 void do_vread(message *mp)
{
	int port, count, size;
	tap_card_t *tc;

	TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(mp));	

	port = mp->DL_PORT;
	count = mp->DL_COUNT;
	tc= &tap_table[port];
	tc->client= mp->DL_PROC;

	if (mp->m_type == DL_READV) {
		  get_userdata(mp->DL_PROC, (vir_bytes) mp->DL_ADDR,
					   (count > IOVEC_NR ? IOVEC_NR : count) *
					   sizeof(iovec_t), tc->read_iovec.iod_iovec);
		  tc->read_iovec.iod_iovec_s    = count;
		  tc->read_iovec.iod_proc_nr    = mp->DL_PROC;
		  tc->read_iovec.iod_iovec_addr = (vir_bytes) mp->DL_ADDR;
		  tc->tmp_iovec = tc->read_iovec;
		  size= calc_iovec_size(&tc->tmp_iovec);
    } else {
		  tc->read_iovec.iod_iovec[0].iov_addr = (vir_bytes) mp->DL_ADDR;
		  tc->read_iovec.iod_iovec[0].iov_size = mp->DL_COUNT;
		  tc->read_iovec.iod_iovec_s           = 1;
		  tc->read_iovec.iod_proc_nr           = mp->DL_PROC;
		  tc->read_iovec.iod_iovec_addr        = 0;
		  size= count;
    }
	tc->flags |= TAPF_READING;

	tc_recv(tc);

	if ((tc->flags & (TAPF_READING|TAPF_STOPPED)) == (TAPF_READING|TAPF_STOPPED))
		tc_reset(tc);
	reply(tc, OK, FALSE);
}

/*===========================================================================*
 *                              tc_reset                                     *
 *===========================================================================*/
void tc_reset(tap_card_t *tc)
{
	int i;

	TASKDEBUG("%s\n", tc->port_name);
	tc_send(tc);
	tc->flags &= ~TAPF_STOPPED;
}

/*===========================================================================*
 *                              tc_send                                      *
 *===========================================================================*/
void tc_send(tap_card_t *tc)
{
	TASKDEBUG("%s\n", tc->port_name);

	if (!(tc->flags & TAPF_SEND_AVAIL))
		return;
  
	tc->flags &= ~TAPF_SEND_AVAIL;
	switch(tc->sendmsg.m_type) {
		case DL_WRITE:  do_vwrite(&tc->sendmsg);       break;
		case DL_WRITEV: do_vwrite(&tc->sendmsg);        break;
		default:
			ERROR_EXIT(tc->sendmsg.m_type);
		break;
    }
}

/*===========================================================================*
 *                             tc_recv                                      *
 *===========================================================================*/
void tc_recv(tap_card_t *tc)
{
	vir_bytes length;
	int packet_processed;
	int status;

	TASKDEBUG("%s\n", tc->port_name);

	if ((tc->flags & TAPF_READING)==0)
		return;
	if (!(tc->flags & TAPF_ENABLED))
		return;

	/* we check all the received slots until find a properly received packet */
	packet_processed = FALSE;
	while (!packet_processed)  {
		if (length > 0) {
			tc_tap2user(tc, &tc->read_iovec, 0, length);             
              tc->read_s = length;
              tc->flags |= TAPF_PACK_RECV;
              tc->flags &= ~TAPF_READING;
              packet_processed = TRUE;
		}
          /* set up this slot again, and we move to the next slot */
	}
}

/*===========================================================================*
 *                              tc_tap2user                                  *
 *===========================================================================*/
void tc_tap2user(tap_card_t *tc, int tap_addr, iovec_dat_t *iovp, 
		vir_bytes offset, vir_bytes count)
{
	/*phys_bytes phys_hw, phys_user;*/
	int bytes, i, r;

	TASKDEBUG("%s count=%d\n", tc->port_name, count);

	i= 0;
	while (count > 0) {
		if (i >= IOVEC_NR) {
			tc_next_iovec(iovp);
			i= 0;
			continue;
        }
		if (offset >= iovp->iod_iovec[i].iov_size) {
			offset -= iovp->iod_iovec[i].iov_size;
			i++;
			continue;
        }
		bytes = iovp->iod_iovec[i].iov_size - offset;
		if (bytes > count)
			bytes = count;

		if ( (r= muk_vcopy( SELF, tap_addr, iovp->iod_proc_nr, iovp->iod_iovec[i].iov_addr + offset, bytes )) != OK )
			ERROR_EXIT(r);
      
		count -= bytes;
		tap_addr += bytes;
		offset += bytes;
    }
}


/*===========================================================================*
 *                              do_init                                      *
 *===========================================================================*/
void do_init(message *mp)
{
	int port;
	tap_card_t *tc;
	message reply_mess, *rp;

	rp = &reply_mess;
	
	TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(mp));	

	port = mp->DL_PORT;
	if (port < 0 || port >= TAP_PORT_NR_MAX){
		reply_mess.m_type= DL_INIT_REPLY;
		reply_mess.m3_i1= ENXIO;
		mess_reply(mp, &reply_mess);
		return;
    }
	
	tc = &tap_table[port];
	strcpy(tc->port_name, "tap_card#0");
	tc->port_name[9] += port;

    if (tc->mode == TAP_DISABLED) {
	  /* Probe failed, or the device is configured off. */
	  reply_mess.m_type= DL_INIT_REPLY;
	  reply_mess.m3_i1= ENXIO;
	  mess_reply(mp, &reply_mess);
	  return;
	}
    if (tc->mode == TAP_ENABLED) {
		tc_init(tc);
    }

	if (tc->mode == TAP_SINK)   {
		tc->ethaddr.ea_addr[0] = 
		tc->ethaddr.ea_addr[1] = 
		tc->ethaddr.ea_addr[2] = 
		tc->ethaddr.ea_addr[3] = 
		tc->ethaddr.ea_addr[4] = 
		tc->ethaddr.ea_addr[5] = 0;
		tc_confaddr(tc);
		reply_mess.m_type = DL_INIT_REPLY;
		reply_mess.m3_i1 = mp->DL_PORT;
		reply_mess.m3_i2 = TAP_PORT_NR_MAX;
		*(mnx_ethaddr_t *) reply_mess.m3_ca1 = tc->ethaddr;
		mess_reply(mp, &reply_mess);
		return;
    }
	assert(tc->mode == TAP_ENABLED);
	assert(tc->flags & TAPF_ENABLED);

	tc->flags &= ~(TAPF_PROMISC | TAPF_MULTI | TAPF_BROAD);

	if (mp->DL_MODE & DL_PROMISC_REQ)
		tc->flags |= TAPF_PROMISC | TAPF_MULTI | TAPF_BROAD;
	if (mp->DL_MODE & DL_MULTI_REQ)
		tc->flags |= TAPF_MULTI;
	if (mp->DL_MODE & DL_BROAD_REQ)
		tc->flags |= TAPF_BROAD;

	tc->client = mp->m_source;
	tc_reinit(tc);

	reply_mess.m_type = DL_INIT_REPLY;
	reply_mess.m3_i1 = mp->DL_PORT;
	reply_mess.m3_i2 = TAP_PORT_NR_MAX;
	*(mnx_ethaddr_t *) reply_mess.m3_ca1 = tc->ethaddr;

	mess_reply(mp, &reply_mess);

	TASKDEBUG(MSG2_FORMAT, MSG2_FIELDS(rp));	
}

/*===========================================================================*
 *                              mess_reply                                   *
 *===========================================================================*/
void mess_reply(message *req, message *reply_mess)
{
	int rcode;
	if ( (rcode = muk_send(req->m_source, reply_mess)) != OK)
		ERROR_EXIT(rcode);
}

/*===========================================================================*
 *                              tc_confaddr                                  *
 *===========================================================================*/
void tc_confaddr(tap_card_t *tc)
{
	int i;
	char eakey[16];
	static char eafmt[]= "x:x:x:x:x:x";
	long v;

  	TASKDEBUG("%s\n", tc->port_name);

}

/*===========================================================================*
 *                              tc_reinit                                    *
 *===========================================================================*/
void tc_reinit(tap_card_t *tc)
{
	TASKDEBUG("%s\n", tc->port_name);
	return;
}


void do_getstat(message *m){
	TASKDEBUG("\n");	
}

void do_stop(message *m){
	TASKDEBUG("\n");	
}

void do_getname(message *m){
	TASKDEBUG("\n");	
}

int low_level_probe(tap_card_t *tc){
	TASKDEBUG("\n");	
}

void *tapif_main( void *arg){
	TASKDEBUG("\n");		
}

/*===========================================================================*
 *				tap_init					     *
 *===========================================================================*/
int tap_init(void )
{
	int rcode, i;
	tap_card_t *tc;

 	tap_lpid = getpid();
	
	if( mayor_dev != (-1) && endp_flag == 1)
		tap_ep = mayor_dev;
	else
		tap_ep = ETH_PROC_NR;
	
	TASKDEBUG("tap_ep=%d\n", tap_ep);

	/* NODE info */
	local_nodeid = muk_getdvsinfo(&dvs);
	if(local_nodeid < 0 )
		ERROR_EXIT(EDVSDVSINIT);
	dvs_ptr = &dvs;
	TASKDEBUG(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));
	TASKDEBUG("local_nodeid=%d\n", local_nodeid);
	
	TASKDEBUG("Get the DC info\n");
	rcode = muk_getdcinfo(DCID, &dcu);
	if(rcode) ERROR_EXIT(rcode);
	dc_ptr = &dcu;
	TASKDEBUG(DC_USR1_FORMAT,DC_USR1_FIELDS(dc_ptr));
	TASKDEBUG(DC_USR2_FORMAT,DC_USR2_FIELDS(dc_ptr));

	TASKDEBUG("Get TAP info\n");
	rcode = muk_getprocinfo(DCID, tap_ep, &tap_proc);
	
	if(rcode ) ERROR_EXIT(rcode);
	tap_ptr = &tap_proc;
	TASKDEBUG("BEFORE " PROC_MUK_FORMAT,PROC_MUK_FIELDS(tap_ptr));
	
	if( TEST_BIT(tap_ptr->p_rts_flags, BIT_SLOT_FREE)) {
		TASKDEBUG("Starting single TAP\n");
		rcode = muk_bind(DCID, tap_ep);
		if(rcode != tap_ep ) ERROR_EXIT(rcode);
		if (endp_flag == 0) { // Started by MoL 
			rcode = sys_bindproc(tap_ep, tap_lpid, LCL_BIND);
			if(rcode < 0) ERROR_EXIT(rcode);						
		} 
	}

	rcode = muk_getprocinfo(DCID, tap_ep, &tap_proc);
	if(rcode) ERROR_EXIT(rcode);
	TASKDEBUG("AFTER  " PROC_MUK_FORMAT,PROC_MUK_FIELDS(tap_ptr));	
	
	for (i=0;i< TAP_PORT_NR_MAX;++i)	{
		tc = &tap_table[i];
		tc->nr = i;
		tc_init(tc);
	}
	
	TASKDEBUG("END tap_init\n");
	return(OK);
}


/*===========================================================================*
 *                              tc_init                                      *
 *===========================================================================*/
void tc_init(tap_card_t *tc)
{
	int i;

	TASKDEBUG("%s\n", tc->port_name);

	/* General initialization */
	tc->flags = TAPF_EMPTY;
	tapif_init(tc); 

//	tc_confaddr(tc);

	TASKDEBUG("%s: Ethernet address ", tc->port_name);
	for (i= 0; i < 6; i++)
		printf("%x%c", tc->ethaddr.ea_addr[i],
			i < 5 ? ':' : '\n');

	/* Finish the initialization */
	tc->flags |= TAPF_ENABLED;

	return;
}

/*-----------------------------------------------------------------------------------*/
/*
 * tapif_init():
 *
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 */
/*-----------------------------------------------------------------------------------*/
int tapif_init(tap_card_t *tc)
{
	char *name = NULL;
	int	rcode;

	TASKDEBUG("%s\n", tc->port_name);

    name = tc->name;
    if (name != NULL) {
      rcode = low_level_probe(tc);
      if (rcode != OK)
        ERROR_RETURN(rcode);
    }

	tc->name[0] = IFNAME0;
	tc->name[1] = IFNAME1;
	
//	tc->output  = tap_output;
//	tc->linkoutput = low_level_output;
	tc->mtu = 1500;
	/* hardware address length */
	tc->hwaddr_len = 6;

//	tc->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_IGMP;

	low_level_init(tc,name);

	return(OK);

}

/*-----------------------------------------------------------------------------------
 *					 low_level_init():
-----------------------------------------------------------------------------------*/
void low_level_init(tap_card_t *tc, const char *name) 
{
	char buf[sizeof(IFCONFIG_ARGS) + sizeof(IFCONFIG_BIN) + 50];
	struct ifreq ifr;
	int rcode;

	/* (We just fake an address...) */
	tc->ethaddr.ea_addr[0] = 0x1;
	tc->ethaddr.ea_addr[1] = 0x2;
	tc->ethaddr.ea_addr[2] = 0x3;
	tc->ethaddr.ea_addr[3] = 0x4;
	tc->ethaddr.ea_addr[4] = 0x5;
	tc->ethaddr.ea_addr[5] = 0x6;

	/* Do whatever else is needed to initialize interface. */

	tc->fd = open(DEVTAP, O_RDWR);
	TASKDEBUG("fd: %d\n", tc->fd);
	if(tc->fd == -1) {
		TASKDEBUG("try running \"modprobe tun\" or rebuilding your kernel with CONFIG_TUN; cannot open " DEVTAP);
		ERROR_EXIT(errno);
	}

	memset(&ifr, 0, sizeof(ifr));
	if (name != NULL)
		strncpy(ifr.ifr_name,name,strlen(name));
	ifr.ifr_flags = IFF_TAP|IFF_NO_PI;
	if (ioctl(tc->fd, TUNSETIFF, (void *) &ifr) < 0) {
		TASKDEBUG(DEVTAP" ioctl TUNSETIFF");
		ERROR_EXIT(errno);
	}
	
	if ( (rcode = low_level_probe(tc)) != OK)
		ERROR_EXIT(rcode);
 
	if (name == NULL) {
		sprintf(buf, IFCONFIG_BIN IFCONFIG_ARGS,
				ifr.ifr_name,
				ip4_addr1(&(tc->gw)),
				ip4_addr2(&(tc->gw)),
				ip4_addr3(&(tc->gw)),
				ip4_addr4(&(tc->gw)),
				ip4_addr1(&(tc->netmask)),
				ip4_addr2(&(tc->netmask)),
				ip4_addr3(&(tc->netmask)),
				ip4_addr4(&(tc->netmask)));

		TASKDEBUG("system(\"%s\");\n", buf);
		system(buf);
	}
	TASKDEBUG("Starting tapif_main thread: %s\n", name);
	rcode = pthread_create( &tc->thread, NULL, tapif_main, tc );
	if( rcode)ERROR_EXIT(rcode);
}

#ifdef ANULADO

/*===========================================================================*
 *                              tc_confaddr                                  *
 *===========================================================================*/
static void tc_confaddr(ether_card_t *tc)
{
	int i;
	char eakey[16];
	static char eafmt[]= "x:x:x:x:x:x";
	long v;

	/* User defined ethernet address? */
	strcpy(eakey, tc_conf[tc->nr].tc_envvar);
	strcat(eakey, "_EA");

	for (i= 0; i < 6; i++) {
      v= tc->mac_address.ea_addr[i];
      if (env_parse(eakey, eafmt, i, &v, 0x00L, 0xFFL) != EP_SET)
		break;
      tc->mac_address.ea_addr[i]= v;
    }
  
	if (i != 0 && i != 6)  {
      /* It's all or nothing; force a panic. */
      (void) env_parse(eakey, "?", 0, &v, 0L, 0L);
    }
}

/*-----------------------------------------------------------------------------------*/
#ifdef linux
static err_t
low_level_probe(struct netif *netif,const char *name)
{
  int len;
  int s;
  struct ifreq ifr;

  len = strlen(name);
  if (len > (IFNAMSIZ-1)) {
    perror("tapif_init: name is too long");
    return ERR_IF;
  }
  s = socket(AF_INET,SOCK_DGRAM,0);
  if (s == -1) {
    perror("tapif_init: socket");
    return ERR_IF;
  }
  memset(&ifr,0,sizeof(ifr));
  strncpy(ifr.ifr_name,name,len);
  if (ioctl(s,SIOCGIFHWADDR,&ifr) == -1) {
    perror("tapif_init: ioctl SIOCGIFHWADDR");
    goto err;
  }
  u8_t* hwaddr = (u8_t*)&ifr.ifr_hwaddr.sa_data;
  netif->hwaddr[0] = hwaddr[0];
  netif->hwaddr[1] = hwaddr[1];
  netif->hwaddr[2] = hwaddr[2];
  netif->hwaddr[3] = hwaddr[3];
  netif->hwaddr[4] = hwaddr[4];
  netif->hwaddr[5] = hwaddr[5] ^ 1;
  netif->hwaddr_len = 6;
  if (ioctl(s,SIOCGIFMTU,&ifr) == -1) {
    perror("tapif_init: ioctl SIOCGIFMTU");
    goto err;
  }
  netif->mtu = ifr.ifr_mtu;
  close(s);
  return ERR_OK;
 err:
  close(s);
  return ERR_IF;
}
#else
static err_t
low_level_probe(struct netif *netif,const char *name)
{
  return ERR_IF;
}
#endif


/*-----------------------------------------------------------------------------------*/
/*
 * low_level_output():
 *
 * Should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 */
/*-----------------------------------------------------------------------------------*/

static err_t
low_level_output(struct netif *netif, struct pbuf *p)
{
  struct pbuf *q;
  char buf[1514];
  char *bufptr;
  struct tapif *tapif;

  tapif = (struct tapif *)netif->state;
#if 0
    if(((double)rand()/(double)RAND_MAX) < 0.2) {
    printf("drop output\n");
    return ERR_OK;
    }
#endif
  /* initiate transfer(); */

  bufptr = &buf[0];

  for(q = p; q != NULL; q = q->next) {
    /* Send the data from the pbuf to the interface, one pbuf at a
       time. The size of the data in each pbuf is kept in the ->len
       variable. */
    /* send data from(q->payload, q->len); */
    memcpy(bufptr, q->payload, q->len);
    bufptr += q->len;
  }

  /* signal that packet should be sent(); */
  if(write(tapif->fd, buf, p->tot_len) == -1) {
    ERROR_RETURN(errno);
  }
  return(OK);
}
/*-----------------------------------------------------------------------------------*/
/*
 * low_level_input():
 *
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 */
/*-----------------------------------------------------------------------------------*/
static struct pbuf *
low_level_input(struct tapif *tapif)
{
  struct pbuf *p, *q;
  u16_t len;
  char buf[1514];
  char *bufptr;

  /* Obtain the size of the packet and put it into the "len"
     variable. */
  len = read(tapif->fd, buf, sizeof(buf));
#if 0
    if(((double)rand()/(double)RAND_MAX) < 0.2) {
    printf("drop\n");
    return NULL;
    }
#endif

  /* We allocate a pbuf chain of pbufs from the pool. */
  p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);

  if(p != NULL) {
    /* We iterate over the pbuf chain until we have read the entire
       packet into the pbuf. */
    bufptr = &buf[0];
    for(q = p; q != NULL; q = q->next) {
      /* Read enough bytes to fill this pbuf in the chain. The
         available data in the pbuf is given by the q->len
         variable. */
      /* read data into(q->payload, q->len); */
      memcpy(q->payload, bufptr, q->len);
      bufptr += q->len;
    }
    /* acknowledge that packet has been read(); */
  } else {
    /* drop packet(); */
  }

  return p;
}
/*-----------------------------------------------------------------------------------*/
static void
tapif_main(void *arg)
{
  struct netif *netif;
  struct tapif *tapif;
  fd_set fdset;
  int ret;

  netif = (struct netif *)arg;
  tapif = (struct tapif *)netif->state;

  while(1) {
    FD_ZERO(&fdset);
    FD_SET(tapif->fd, &fdset);

    /* Wait for a packet to arrive. */
    ret = select(tapif->fd + 1, &fdset, NULL, NULL, NULL);

    if(ret == 1) {
      /* Handle incoming packet. */
      tapif_input(netif);
    } else if(ret == -1) {
      ERROR_RETURN(errno);
    }
  }
}
/*-----------------------------------------------------------------------------------*/
/*
 * tapif_input():
 *
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface.
 *
 */
/*-----------------------------------------------------------------------------------*/
static void
tapif_input(struct netif *netif)
{
  struct tapif *tapif;
  struct eth_hdr *ethhdr;
  struct pbuf *p;


  tapif = (struct tapif *)netif->state;

  p = low_level_input(tapif);

  if(p == NULL) {
    LWIP_DEBUGF(TAPIF_DEBUG, ("tapif_input: low_level_input returned NULL\n"));
    return;
  }
  ethhdr = (struct eth_hdr *)p->payload;

  switch(htons(ethhdr->type)) {
  /* IP or ARP packet? */
  case ETHTYPE_IP:
  case ETHTYPE_ARP:
#if PPPOE_SUPPORT
  /* PPPoE packet? */
  case ETHTYPE_PPPOEDISC:
  case ETHTYPE_PPPOE:
#endif /* PPPOE_SUPPORT */
    /* full packet send to tcpip_thread to process */
    if (netif->input(p, netif) != ERR_OK) {
      LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
       pbuf_free(p);
       p = NULL;
    }
    break;
  default:
    pbuf_free(p);
    break;
  }
}
/*-----------------------------------------------------------------------------------*/
/*
 * tapif_init():
 *
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 */
/*-----------------------------------------------------------------------------------*/
err_t
tapif_init(struct netif *netif)
{
	struct tapif *tapif;
	char *name = NULL;
	err_t err;

	if (netif->state == NULL) {
	tapif = (struct tapif *)malloc(sizeof(struct tapif));
	if (!tapif) {
		ERROR_RETURN(ERR_MEM);
	}
		netif->state = tapif;
	} else {
		tapif = (struct tapif *)netif->state;
		name = tapif->name;
		if (name != NULL) {
			err = low_level_probe(netif,name);
			if (err != ERR_OK)
				ERROR_RETURN(err);
		}
	}
	netif->name[0] = IFNAME0;
	netif->name[1] = IFNAME1;
	netif->output = etharp_output;
	netif->linkoutput = low_level_output;
	netif->mtu = 1500;
	/* hardware address length */
	netif->hwaddr_len = 6;

	tapif->ethaddr = (struct eth_addr *)&(netif->hwaddr[0]);

	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_IGMP;

	low_level_init(netif,name);

	return ERR_OK;
}

#endif // ANULADO
