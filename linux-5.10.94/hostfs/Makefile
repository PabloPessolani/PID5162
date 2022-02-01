#
# Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
# Licensed under the GPL
#

hostfs-objs := hostfs_kern.o hostfs_user.o
rhostfs-objs := rhostfs_kern.o rhostfs_user.o  rhostfs_clib.o 

obj-y :=
obj-$(CONFIG_HOSTFS) += hostfs.o
obj-$(CONFIG_RHOSTFS) += rhostfs.o

include arch/um/scripts/Makefile.rules
