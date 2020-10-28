
#include "switch.h"

void send_tap(int fd, void *packet, int len, void *unused)
{
  int n;

	USRDEBUG("fd=%d len=%d \n", fd, len);

  n = write(fd, packet, len);
  if(n != len){
    if(errno != EAGAIN) perror("send_tap");
  }
}

int open_tap(char *dev)
{
  struct ifreq ifr;
  int fd, err;

	USRDEBUG("dev=%s\n", dev);

  if((fd = open("/dev/net/tun", O_RDWR)) < 0){
    perror("Failed to open /dev/net/tun");
    return(-1);
  }
  
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI; // | IFF_MULTI_QUEUE;
  strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name) - 1);
  if(ioctl(fd, TUNSETIFF, (void *) &ifr) < 0){
  	USRDEBUG("TUNSETIFF failed %s\n", dev);
    perror("TUNSETIFF failed");
	close(fd);
    return(-1);
  }
  err = setup_port(fd, send_tap, NULL, 0);
  if(err) return(err);
  USRDEBUG("dev=%s fd=%d\n", dev, fd);
  return(fd);
}

