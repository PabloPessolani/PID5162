#!/bin/bash
# cp /usr/src/dvs/vos/images/minix3-4096.img /dev/shm/rdisk.img
#cp /usr/src/dvs/vos/muk2/rdisk.cfg /dev/shm
#cp /usr/src/dvs/vos/muk2/molfs.cfg /dev/shm
#cp /usr/src/dvs/vos/muk2/m3nweb.cfg /dev/shm
nsenter -p -t $DC0 ./muk2 /dev/shm/muk.cfg
exit 



  
