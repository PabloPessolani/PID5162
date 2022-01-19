#!/bin/bash
#######################################
# this script is launched by /etc/init.d/dvs 
#######################################
echo TEST_LB_ARGS: $# , $0, $1, $2
cd /usr/src/dvs/dvk-tests/
#if [ $# -ne 0 ]
#then 
# echo "usage: $0 without arguments"
# exit 1 
#fi
########## CONSTANTS ###############
base_port=3000
dcid=0
lb=0
#################################
node_name=`hostname | awk '{print $1;}'`
svr=`echo $node_name | sed 's/node//g'`
clt=`echo $node_name | sed 's/client//g'`
eth0="xx"
eth1="xx"
until [ $eth0 == "eth0" ]; do 
	echo eth0=$eth0
	sleep 1
	eth0=`ifconfig -s eth0 | awk '{print $1;}' | sed 's/Iface//g'`
done
echo node_name=$node_name svr=$svr clt=$clt dcid=$dcid lb=$lb base_port=$base_port
if [ $svr -eq $lb ]
then 
	until [ $eth1 == "eth1" ]; do 
		echo eth1=$eth1
		sleep 1
		eth1=`ifconfig -s eth1 | awk '{print $1;}' | sed 's/Iface//g'`
	done
	echo Running Load Balancer in Background 
	mkdir /var/run/spread
	/usr/local/sbin/spread -c /etc/spread.conf > /dev/shm/spread.txt &
	cd /usr/src/dvs/dvk-mod
	mknod /dev/dvk c 33 0
	dmesg -c > /dev/shm/dmesg.txt
	insmod dvk.ko dvk_major=33 dvk_minor=0 dvk_nr_devs=1 dbglvl=16777215
	dmesg -c >> /dev/shm/dmesg.txt
	lsmod | grep dvk 
	cd /usr/src/dvs/dvs-apps/dvs_lb/
	./lb_dvs lb_dvs.cfg > lb_dvs.out 2> lb_dvs.err &
	ps -ef | grep lb_dvs 
	exit
fi
## build de MULTIPROXY configuration file 
cd /dev/shm
if [ $clt == $node_name ]
then 
	echo $node_name is a server
	sport=$(($base_port + $svr))
	lcl=$svr
	proxy="node$lb"
else	
	echo $node_name is a client
	sport=$(($base_port + $clt)) 
	lcl=$clt
	proxy="client$lb"
fi
echo lcl=$lcl proxy=$proxy 
rport=$(($base_port + $lb))
echo "# multi_proxy config file" 	>  /dev/shm/multi_proxy.cfg
echo "proxy $proxy {"	  			>> /dev/shm/multi_proxy.cfg
echo "	proxyid 	$lb;"   		>> /dev/shm/multi_proxy.cfg
echo "	proto		tcp;"  			>> /dev/shm/multi_proxy.cfg
echo "	compress	NO;"  			>> /dev/shm/multi_proxy.cfg
echo "	batch		NO;"  			>> /dev/shm/multi_proxy.cfg
echo "	autobind	NO;"  			>> /dev/shm/multi_proxy.cfg
echo "	rname		$node_name;"    >> /dev/shm/multi_proxy.cfg
echo "	rport		$rport;"   		>> /dev/shm/multi_proxy.cfg
echo "	sport		$sport;" 		>> /dev/shm/multi_proxy.cfg
echo "};"          					>> /dev/shm/multi_proxy.cfg
cat  /dev/shm/multi_proxy.cfg
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
insmod dvk.ko dvk_major=33 dvk_minor=0 dvk_nr_devs=1 dbglvl=16777215
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
./test_dvs_init -n $lcl -D 16777215 -C TEST_CLUSTER
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
#read  -p " MULTIPROXY Enter para continuar... "
cd /usr/src/dvs/dvk-proxies
/usr/src/dvs/dvk-proxies/multi_proxy  /dev/shm/multi_proxy.cfg > /dev/shm/node$lcl.txt 2> /dev/shm/error$lcl.txt &	
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
if [ $clt == $node_name ]
then
	echo Running lb_agent on server $node_name
	cd /usr/src/dvs/dvs-apps/dvs_lb/
	./lb_agent > /tmp/agent$svr.out 2>/tmp/agent$svr.err &
fi 
exit 



  
