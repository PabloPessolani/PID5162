/*
 * Copyright (C) 2001 Lennert Buytenhek (buytenh@gnu.org) and
 * James Leu (jleu@mindspring.net).
 * Copyright (C) 2001 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Copyright (C) 2001 by various other people who didn't put their name here.
 * Licensed under the GPL.
 */

#include <linux/init.h>
#include <linux/netdevice.h>
#include <net_kern.h>
#include "rtap.h"

struct rtap_init {
	char *sock_type;
	char *ctl_sock;
};

static void rtap_init(struct net_device *dev, void *data)
{
	struct uml_net_private *pri;
	struct rtap_data *dpri;
	struct rtap_init *init = data;

	pri = netdev_priv(dev);
	dpri = (struct rtap_data *) pri->user;
	dpri->sock_type = init->sock_type;
	dpri->ctl_sock = init->ctl_sock;
	dpri->fd = -1;
	dpri->control = -1;
	dpri->dev = dev;
	/* We will free this pointer. If it contains crap we're burned. */
	dpri->ctl_addr = NULL;
	dpri->data_addr = NULL;
	dpri->local_addr = NULL;

	printk("rtap backend (uml_switch version %d) - %s:%s",
	       SWITCH_VERSION, dpri->sock_type, dpri->ctl_sock);
	printk("\n");
}

static int rtap_read(int fd, struct sk_buff *skb, struct uml_net_private *lp)
{
	return net_recvfrom(fd, skb_mac_header(skb),
			    skb->dev->mtu + ETH_HEADER_OTHER);
}

static int rtap_write(int fd, struct sk_buff *skb, struct uml_net_private *lp)
{
	return rtap_user_write(fd, skb->data, skb->len,
				 (struct rtap_data *) &lp->user);
}

static const struct net_kern_info rtap_kern_info = {
	.init			= rtap_init,
	.protocol		= eth_protocol,
	.read			= rtap_read,
	.write			= rtap_write,
};

static int rtap_setup(char *str, char **mac_out, void *data)
{
	struct rtap_init *init = data;
	char *remain;

	*init = ((struct rtap_init)
		{ .sock_type 		= "unix",
		  .ctl_sock 		= "/tmp/rtap.ctl" });

	remain = split_if_spec(str, mac_out, &init->sock_type, &init->ctl_sock,
			       NULL);
	if (remain != NULL)
		printk(KERN_WARNING "rtap_setup : Ignoring data socket specification\n");

	return 1;
}

static struct transport rtap_transport = {
	.list 		= LIST_HEAD_INIT(rtap_transport.list),
	.name 		= "rtap",
	.setup  	= rtap_setup,
	.user 		= &rtap_user_info,
	.kern 		= &rtap_kern_info,
	.private_size 	= sizeof(struct rtap_data),
	.setup_size 	= sizeof(struct rtap_init),
};

static int register_rtap(void)
{
	register_transport(&rtap_transport);
	return 0;
}

late_initcall(register_rtap);
