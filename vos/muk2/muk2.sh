#!/bin/bash
cp /usr/src/dvs/vos/images/minixweb.img /dev/shm/rdisk.img
cp /usr/src/dvs/vos/muk2/tasks/rdisk/rdisk.cfg /dev/shm
./muk2 muk.cfg
exit 



  
