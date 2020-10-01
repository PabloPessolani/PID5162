#!/bin/bash

./rdisk.sh $1 0
. /usr/src/dvs/dvs-apps/dc_init/DC0.sh
nsenter -p -u -F -t$DC0 bash /usr/src/dvs/vos/mol/drivers/rdisk/rdisk.sh 0
