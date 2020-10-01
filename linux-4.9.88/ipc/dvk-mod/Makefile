# Comment/uncomment the following line to disable/enable debugging
DEBUG = y


# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DDVK_MODULE -DDVK_DEBUG -zmuldefs # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2 -DDVK_MODULE
endif

DVSINC=/usr/src/dvs/include

EXTRA_CFLAGS += $(DEBFLAGS)
EXTRA_CFLAGS += -I$(DVSINC)

ifneq ($(KERNELRELEASE),)
# call from kernel build system
dvk-objs := main.o dvk_newcall.o dvk_decode.o reljmp_core.o \
			dvk_hyper.o dvk_utils.o dvk_acks.o dvk_procfs.o \
			dvk_sproxy.o  dvk_rproxy.o dvk_debugfs.o dvk_ipc.o \
			dvk_migrate.o
			
obj-m	:= dvk.o
else
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions 

install:
	echo moving dvk.ko
	mv  dvk.ko  /usr/src/dvs/dvk-mod 
	
depend .depend dep:
	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend
ifeq (.depend,$(wildcard .depend))
include .depend
endif
