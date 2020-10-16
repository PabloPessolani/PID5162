TUNTAP = $(shell [ -e /usr/include/linux/if_tun.h ] && echo -DTUNTAP)

OBJS = hash.o port.o dvs_uml_switch.o switch_cfg.o rmttap.o 
BIN = dvs_uml_switch
CFLAGS ?= -g -Wall -pthread
override CFLAGS += $(TUNTAP)

LIBS = 	/usr/src/dvs/dvk-lib/stub_dvkcall.o \
		/usr/src/dvs/lib/config/libconfig.so 		

BIN_DIR ?= /usr/bin

ifneq ($(TUNTAP),)
	OBJS += tuntap.o 
endif

all : $(BIN)

$(BIN) : $(OBJS)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJS) $(LIBS)

clean : 
	rm -f $(BIN) $(OBJS) *~

install : $(BIN)
	install -d $(DESTDIR)$(BIN_DIR)
	install -s $(BIN) $(DESTDIR)$(BIN_DIR)
