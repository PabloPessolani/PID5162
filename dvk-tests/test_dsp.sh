#!/bin/bash
#######################################
# this script is launched by /etc/init.d/dvs 
#######################################
echo TEST_DSP_ARGS: $# , $0, $1, $2
cd /usr/src/dvs/dvk-tests/
#if [ $# -ne 0 ]
#then 
# echo "usage: $0 without arguments"
# exit 1 
#fi
########## CONSTANTS ###############
base_port=3000
dcid=0
#dbg=0
dbg=16777215
#################################
name=`hostname | awk '{print $1;}'`
lcl=`echo $name | sed 's/node//g'`
eth0="xx"
eth1="xx"
until [ $eth0 == "eth0" ]; do 
	echo eth0=$eth0
	sleep 1
	eth0=`ifconfig -s eth0 | awk '{print $1;}' | sed 's/Iface//g'`
done
echo name=$name dcid=$dcid base_port=$base_port
## build de DSP PROXY configuration file 
cd /dev/shm
echo "# dsp_proxy config file" 	>  /dev/shm/dsp_proxy.cfg
#######################################################
# CHANGE THE SEQUENCE OF NODEIDs IN THE FOLLOWING FOR SENTENCE
# i.e.: for i in 0 1 5 6 7 8; do
#######################################################
for i in 0 1; do
	if [ $lcl -ne $i ] 
	then
		proxy="node$i"
		rmt=$i
		rport=$(($base_port + $rmt))
		sport=$(($base_port + $lcl))	 
		echo "proxy $proxy {"	  			>> /dev/shm/dsp_proxy.cfg
		echo "	proxyid		$rmt;"  		>> /dev/shm/dsp_proxy.cfg
		echo "	proto		tcp;"  			>> /dev/shm/dsp_proxy.cfg
		echo "	compress	NO;"  			>> /dev/shm/dsp_proxy.cfg
		echo "	batch		NO;"  			>> /dev/shm/dsp_proxy.cfg
		echo "	autobind	NO;"  			>> /dev/shm/dsp_proxy.cfg
		echo "	rname		$name;"    		>> /dev/shm/dsp_proxy.cfg
		echo "	rport		$rport;"   		>> /dev/shm/dsp_proxy.cfg
		echo "	sport		$sport;" 		>> /dev/shm/dsp_proxy.cfg
		echo "};"          					>> /dev/shm/dsp_proxy.cfg
		echo "#"          					>> /dev/shm/dsp_proxy.cfg
	fi
done 	
cat  /dev/shm/dsp_proxy.cfg
#read  -p "#read to initilize TAP devices. Enter para continuar... "
./inittap.sh $lcl
echo > /var/log/kern.log
echo > /var/log/syslog
echo > /var/log/messages
dmesg -c > /dev/null
#read  -p "Spread Enter para continuar... "
mkdir /var/run/spread
/usr/local/sbin/spread -c /etc/spread.conf > /dev/shm/spread.txt &
#read  -p "DVK module Enter para continuar... "
cd /usr/src/dvs/dvk-mod
mknod /dev/dvk c 33 0
dmesg -c > /dev/shm/dmesg.txt
insmod dvk.ko dvk_major=33 dvk_minor=0 dvk_nr_devs=1 dbglvl=$dbg
dmesg -c >> /dev/shm/dmesg.txt
lsmod | grep dvk 
 #read  -p "mount Enter para continuar... "
#cd /usr/src/dvs/dvs-apps/dvsd
#./dvsd $lcl 
part=(5 + $dcid)
echo "partition $part"
#read  -p "mount Enter para continuar... "
#mount  /dev/sdb$part /usr/src/dvs/vos/rootfs/DC$dcid
cd /usr/src/dvs/dvk-tests
#read  -p "local_nodeid=$lcl Enter para continuar... "
./test_dvs_init -n $lcl -D $dbg -C TEST_CLUSTER
#read  -p "DC$dcid Enter para continuar... "
# ./test_dc_init -d $dcid
cd /dev/shm
echo "# dc_init config file"     	>  /dev/shm/DC$dcid.cfg
echo "dc DC$dcid {"       			>> /dev/shm/DC$dcid.cfg
echo "dcid $dcid;"     				>> /dev/shm/DC$dcid.cfg
echo "nr_procs 221;"    			>> /dev/shm/DC$dcid.cfg
echo "nr_tasks 34;"    				>> /dev/shm/DC$dcid.cfg
echo "nr_sysprocs 64;"  			>> /dev/shm/DC$dcid.cfg
echo "nr_nodes 32;"    				>> /dev/shm/DC$dcid.cfg
echo "tracker \"NO\";"    			>> /dev/shm/DC$dcid.cfg
#echo "warn2proc 0;"     			>> DC$dcid.cfg
#echo "warnmsg 1;"     				>> DC$dcid.cfg
#echo "ip_addr \"192.168.10.10$dcid\";"	>> DC$dcid.cfg
echo "memory 512;"    				>> /dev/shm/DC$dcid.cfg
echo "image \"/usr/src/dvs/vos/images/debian$dcid.img\";"  	>> /dev/shm/DC$dcid.cfg
 echo "mount \"/usr/src/dvs/vos/rootfs/DC$dcid\";"  		>> /dev/shm/DC$dcid.cfg
echo "};"          					>> DC$dcid.cfg
/usr/src/dvs/dvs-apps/dc_init/dc_init /dev/shm/DC$dcid.cfg > /dev/shm/dc_init$dcid.out 2> /dev/shm/dc_init$dcid.err
dmesg -c >> /dev/shm/dmesg.txt
# #read  -p "TCP PROXY Enter para continuar... "
#     PARA DESHABILITAR EL ALGORITMO DE NAGLE!! 
echo 1 > /proc/sys/net/ipv4/tcp_low_latency
echo 0 > /proc/sys/kernel/hung_task_timeout_secs
# tcp_keepalive_time: the interval between the last data packet sent (simple ACKs are not considered data) 
#           and the first keepalive probe; after the connection is marked to need keepalive, this counter is not used any further
# tcp_keepalive_intvl:  the interval between subsequential keepalive probes, regardless of what the connection has exchanged in the meantime
# tcp_keepalive_probes: the number of unacknowledged probes to send before considering the connection dead and notifying the application layer
#echo 60 > /proc/sys/net/ipv4/tcp_keepalive_time
#echo 30 > /proc/sys/net/ipv4/tcp_keepalive_intvl
# echo 3  > /proc/sys/net/ipv4/tcp_keepalive_probes
#read  -p " DSP Enter para continuar... "
cd /usr/src/dvs/dvk-proxies/dsp
/usr/src/dvs/dvk-proxies/dsp/dsp_proxy  /dev/shm/dsp_proxy.cfg > /dev/shm/node$lcl.txt 2> /dev/shm/error$lcl.txt &	
sleep 5
netstat  -nat
#tipc bearer list
#tipc nametable show
#tipc link list
#read  -p "Enter para continuar... "
cat /proc/dvs/nodes
cat /proc/dvs/proxies/info
cat /proc/dvs/proxies/procs
#read  -p "ADDNODE to DC$dcid Enter para continuar... "
cd /usr/src/dvs/dvk-tests
cat /proc/dvs/DC$dcid/info
./test_add_node $dcid $lb 
sleep 1
cat /proc/dvs/nodes
cat /proc/dvs/DC$dcid/info
cat /proc/dvs/DC$dcid/procs 
cd /dev/shm
echo "run # . /dev/shm/DC0.sh"
exit 



  
