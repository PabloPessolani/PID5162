TUNTAP = $(shell [ -e /usr/include/linux/if_tun.h ] && echo -DTUNTAP)

OBJS = hash.o port.o uml_switch.o 
BIN = uml_switch 

OBJS2 = sw_client.o 
BIN2 = sw_client

CFLAGS ?= -g -Wall
override CFLAGS += $(TUNTAP)

BIN_DIR ?= /usr/bin

ifneq ($(TUNTAP),)
	OBJS += tuntap.o
endif

all : $(BIN) $(BIN2)

$(BIN) : $(OBJS)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJS)

$(BIN2) : $(OBJS2)
	$(CC) $(CFLAGS) -o $(BIN2) $(OBJS2)

clean : 
	rm -f $(BIN) $(OBJS) $(BIN2) $(OBJS2) *~

install : $(BIN)
	install -d $(DESTDIR)$(BIN_DIR)
	install -s $(BIN) $(DESTDIR)$(BIN_DIR)
	install -s $(BIN2) $(DESTDIR)$(BIN_DIR)
