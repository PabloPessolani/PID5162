#!/bin/bash
nsenter -p -t $DC0 ./linux con0=null,fd:2 con1=fd:0,fd:1 con=null ssl=null umid=node1 ubda=/dev/sdb6 mem=1024M dcid=0 uml_ep=-2 rd_ep=3 eth0=tuntap,HTAP,fe:fd:c0:a8:00:fd nosysemu
exit
