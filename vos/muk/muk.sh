#!/bin/bash
if [ $# -ne 1 ]
then 
	echo "usage: $0 <dcid>"
	dcid=0
else 
	dcid=$1
fi
lcl=$NODEID
echo "lcl=$lcl" 
echo "dcid=$dcid" 
read  -p "Enter para continuar... "
dmesg -c > /dev/null
################## SYSTASK NODE0 ##################### 
read  -p "SYSTASK Enter para continuar... "
dmesg -c  > /usr/src/dvs/vos/mol/dmesg$lcl.txt
cd /usr/src/dvs/vos/mol/drivers/systask
echo PID DC$dcid=$DC0
./systask -d $dcid > st_out$lcl.txt 2> st_err$lcl.txt &
sleep 2
cat /proc/dvs/DC$dcid/procs
####################### START RDISK NODE0 #################
cd /usr/src/dvs/vos/mol/drivers/rdisk
echo "================= building /tmp/rdisk$dcid.cfg ===================="
echo "device MY_FILE_IMG {" 			>   /tmp/rdisk$dcid.cfg
echo "	major			3;"				>>  /tmp/rdisk$dcid.cfg
echo "	minor			0;"				>>  /tmp/rdisk$dcid.cfg
echo "	type			FILE_IMAGE;"	>>  /tmp/rdisk$dcid.cfg
echo " 	image_file 		\"/usr/src/dvs/vos/mol/images/minixweb.img\";" 	>>  /tmp/rdisk$dcid.cfg
echo "	volatile		NO;"			>>  /tmp/rdisk$dcid.cfg
echo "	replicated		YES;"			>>  /tmp/rdisk$dcid.cfg
echo "};"								>>  /tmp/rdisk$dcid.cfg
cat /tmp/rdisk$dcid.cfg 
read  -p "Enter para continuar... "
dmesg -c  >> /usr/src/dvs/vos/mol/dmesg$lcl.txt
read  -p "RDISK Enter para continuar... "
#./rdisk -r<replicate> -[f<full_update>|d<diff_updates>] -D<dyn_updates> -z<compress> -c <config file>
echo "./rdisk -rdDec /tmp/rdisk$dcid.cfg > rdisk$lcl.txt 2> rdisk_err$lcl.txt"
./rdisk -rdDec /tmp/rdisk$dcid.cfg > rdisk$lcl.txt 2> rdisk_err$lcl.txt &
sleep 2
dmesg -c  >> /usr/src/dvs/dvk-tests/dmesg.txt
sleep 2
cat /proc/dvs/DC$dcid/procs
################## RS NODE0 #####################
read  -p "RS Enter para continuar... "
dmesg -c  >> /usr/src/dvs/vos/mol/dmesg$lcl.txt
cd /usr/src/dvs/vos/mol/servers/rs
./rs $dcid > rs$lcl.txt 2> rs_err$lcl.txt &
sleep 2
cat /proc/dvs/DC$dcid/procs
################## PM NODE0 #####################
read  -p "PM Enter para continuar... "
dmesg -c  >> /usr/src/dvs/vos/mol/dmesg$lcl.txt
cd /usr/src/dvs/vos/mol/servers/pm/
./pm $dcid > pm_out$lcl.txt 2> pm_err$lcl.txt &
sleep 2
cat /proc/dvs/DC$dcid/procs
####################### START FS NODE0 #################
read  -p "FS Enter para continuar... "
echo "================= building /tmp/molfs$dcid.cfg ===================="
echo "device MY_FILE_IMG {" 			>   /tmp/molfs$dcid.cfg
echo "	major			1;"				>>  /tmp/molfs$dcid.cfg
echo "	minor			0;"				>>  /tmp/molfs$dcid.cfg
echo "	type			FILE_IMG;" 		>>  /tmp/molfs$dcid.cfg
echo " 	filename 		\"/usr/src/dvs/vos/mol/images/minixweb.img\";" 	>>  /tmp/molfs$dcid.cfg
echo "	volatile		NO;"			>>  /tmp/molfs$dcid.cfg
echo "	root_dev		YES;"			>>  /tmp/molfs$dcid.cfg
echo "	buffer_size		4096;"			>>  /tmp/molfs$dcid.cfg
echo "};"								>>  /tmp/molfs$dcid.cfg
cat /tmp/molfs$dcid.cfg 
read  -p "Enter para continuar... "
dmesg -c  >> /usr/src/dvs/vos/mol/dmesg$lcl.txt
cd /usr/src/dvs/vos/mol/commands/demonize
./demonize -l node$lcl $lcl $dcid 1 0 "/usr/src/dvs/vos/mol/servers/fs/fs /tmp/molfs$dcid.cfg" > fs$lcl.txt 2> fserr$lcl.txt &
sleep 2
cat /proc/dvs/DC$dcid/procs
####################### START M3FTPD #################
#read  -p "Starting M3FTPD. Enter para continuar... "
#echo "Archivo file10M.txt"
#base64 /dev/urandom | head -c $[1024*1024*10] > /usr/src/dvs/vos/mol/images/file10M.txt
#dmesg -c  >> /usr/src/dvs/vos/mol/dmesg$lcl.txt
#cd /usr/src/dvs/vos/mol/commands/demonize
#./demonize -l node$lcl $lcl $dcid 20 0 "/usr/src/dvs/vos/mol/servers/m3ftp/m3ftpd /usr/src/dvs/vos/mol/images/"  > ftpsrvout$lcl.txt 2> ftpsrverr$lcl.txt &
#sleep 2
#cat /proc/dvs/DC$dcid/procs
####################### START M3FTP on NODE0 #################
#read  -p "Starting M3FTP on NODE0. Enter para continuar... "
#dmesg -c  >> /usr/src/dvs/vos/mol/dmesg$lcl.txt
#cd /usr/src/dvs/vos/mol/commands/demonize
#./demonize  -l node$lcl $lcl $dcid 21 0 "/usr/src/dvs/vos/mol/servers/m3ftp/m3ftp -g 20 file10M.txt /tmp/file10M.txt"  > ftpcltout$lcl.txt 2> ftpclterr$lcl.txt &
#sleep 2
#cat /proc/dvs/DC$dcid/procs
################## M3NWEB  SERVER #####################
#read  -p "building /tmp/m3nweb$dcid.cfg  Enter para continuar... "
#echo "================= building /tmp/m3nweb$dcid.cfg ===================="
#echo "websrv SERVER1 {" 			>    /tmp/m3nweb$dcid.cfg
#echo "	port			8080;" 		>>   /tmp/m3nweb$dcid.cfg
#echo "	endpoint		22;" 		>>   /tmp/m3nweb$dcid.cfg
#echo "	rootdir		\"/\";" 		>>   /tmp/m3nweb$dcid.cfg
#echo "};"							>>   /tmp/m3nweb$dcid.cfg
#cat /tmp/m3nweb$dcid.cfg 
#read  -p "M3NWEB SERVER  Enter para continuar... "
#dmesg -c  >> /usr/src/dvs/vos/mol/dmesg$lcl.txt
#cd /usr/src/dvs/vos/mol/commands/demonize
#./demonize -l node$lcl $lcl $dcid 22 0 "/usr/src/dvs/vos/mol/servers/m3nweb/m3nweb /tmp/m3nweb$dcid.cfg" > m3nweb_out$lcl.txt 2> m3nweb_err$lcl.txt &
#sleep 2
#dmesg -c  >> /usr/src/dvs/vos/mol/dmesg$lcl.txt
#cat /proc/dvs/DC$dcid/procs
#echo "WARNING m3nweb may not appear in the list."
#echo "It has endpoint 22 and TCP port 8080"
#echo "the test url is http://192.168.1.100:8080/index.html" 
#netstat -nat | grep 8080
####################### START WEBSRV NODE0 #################
#read  -p "WEBSRV Enter para continuar... "
#dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
#cd /home/MoL_Module/mol-ipc/commands/demonize
#./demonize -l node$lcl $lcl  $dcid 20 0 "/home/MoL_Module/mol-ipc/commands/m3urlget/websrv /home/MoL_Module/mol-ipc/commands/m3urlget/websrv.cfg" > websrv$lcl.txt 2> websrv$lcl.txt &
#sleep 2
#cat /proc/dvs/DC$dcid/procs
####################### START WEBCLT NODE0 #################
#read  -p "WEBCLT Enter para continuar... "
#dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
#cd /home/MoL_Module/mol-ipc/commands/demonize
#./demonize -l node$lcl $lcl  $dcid 21 0 "/home/MoL_Module/mol-ipc/commands/m3urlget/webclt fake.jpg"  > webclt$lcl.txt 2> webclt$lcl.txt
#sleep 2
#cat /proc/dvs/DC$dcid/procs
####################### START TTY NODE0 #################
#read  -p "TTY Enter para continuar... "
#dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
#cd /home/MoL_Module/mol-ipc/commands/demonize
#./demonize -l node$lcl $lcl  $dcid 5 0 "/home/MoL_Module/mol-ipc/tasks/tty/tty /home/MoL_Module/mol-ipc/tasks/tty/tty.cfg" > tty$lcl.txt 2> tty_err$lcl.txt &
#sleep 2
#cat /proc/dvs/DC$dcid/procs
####################### TEST_TTY NODE0 #################
#read  -p "TEST_TTY Enter para continuar... "
#dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
#cd /home/MoL_Module/mol-ipc/commands/demonize
#./demonize -l node$lcl $lcl  $dcid 21 0 "/home/MoL_Module/mol-ipc/tasks/tty/test_tty"  > test_tty$lcl.txt 2> test_tty_err$lcl.txt 
#dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
#sleep 2
#cat /proc/dvs/DC$dcid/procs

####################### ETH #################
read  -p "ETHERNET Enter para continuar... "
dmesg -c  >> /usr/src/dvs/vos/mol/dmesg$lcl.txt
cd /usr/src/dvs/vos/mol/commands/demonize
#demonize -{l|r|b} <vmid> <endpoint> <pid> <filename> <args...> 
./demonize -l node$lcl $lcl $dcid 6 0 "/usr/src/dvs/vos/mol/drivers/eth/eth $dcid" > eth$lcl.txt 2> eth2$lcl.txt&
#
####################### CONFIG TAP9  #################
read  -p "Configuring tap9. Enter para continuar... "
mknod /dev/tap9 c 36 25
chmod 666 /dev/tap9
ls -l /dev/tap9
ip tuntap add dev tap9 mode tap
ip link set dev tap9 address 72:89:78:FF:88:EF
ip link set dev tap9 up 
ifconfig tap9 172.16.1.9 netmask 255.255.255.0 
ifconfig | grep tap9
brctl addif br$dcid tap9
exit 0

#
####################### INET  #################
read  -p "INET Enter para continuar... "
dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
cd /home/MoL_Module/mol-ipc/commands/demonize
#demonize -{l|r|b} <vmid> <endpoint> <pid> "<filename> <args...>"
ifconfig > /home/MoL_Module/mol-ipc/ifconfig.txt
tcpdump -i tap$dcid -N -n -e -vvv -XX > /home/MoL_Module/mol-ipc/tcpdump.txt &
./demonize -l node$lcl $lcl $dcid 9 0 "/home/MoL_Module/mol-ipc/servers/inet/inet 0" > inet$lcl.txt &
sleep 2
dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
cat /proc/dvs/DC$dcid/procs
####################### START FS NODE0 #################
read  -p "FS Enter para continuar... "
dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
cd /home/MoL_Module/mol-ipc/commands/demonize
./demonize -l node$lcl $lcl  $dcid 1 0 "/home/MoL_Module/mol-ipc/servers/fs/fs /home/MoL_Module/mol-ipc/servers/fs/molfs_DC$dcid.cfg" > fs$lcl.txt 2> fserr$lcl.txt &
sleep 2
cat /proc/dvs/DC$dcid/procs
####################### TEST_FS_INET  #################
read  -p "TEST_FS_INET Enter para continuar... "
dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
cd /home/MoL_Module/mol-ipc/commands/demonize
#demonize -{l|r|b} <vmid> <endpoint> <pid> <filename> <args...> 
read  -p "test_fs_inet_01: Enter para continuar... "
./demonize -l node$lcl $lcl  $dcid 20 0 "/home/MoL_Module/mol-ipc/molTestsLib/test_fs_inet_01" >> test_fs_inet_01.txt 2>> test_fs_inet_01.txt
sleep 5
dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
cat /proc/dvs/DC$dcid/procs
####################### TEST_INET  #################
read  -p "TEST_INET Enter para continuar... "
dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
cd /home/MoL_Module/mol-ipc/commands/demonize
#demonize -{l|r|b} <vmid> <endpoint> <pid> <filename> <args...> 
./demonize -l node$lcl $lcl $dcid 10 0 "/home/MoL_Module/mol-ipc/servers/inet/test_inet 0" > test_inet$lcl.txt &
sleep 2
cat /proc/dvs/DC$dcid/procs
ping -I tap9 -c 3 172.16.1.4
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
exit 
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#
####################### LOCAL INIT  #################
read  -p "LOCAL INIT Enter para continuar... "
dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
cd /home/MoL_Module/mol-ipc/commands/demonize
#demonize -{l|r|b} <vmid> <endpoint> <pid> <filename> <args...> 
./demonize -l node$lcl $lcl $dcid 11 1 "/home/MoL_Module/mol-ipc/servers/init/init 0" > init$lcl.txt &
sleep 2
cat /proc/dvs/DC$dcid/procs
####################### REMOTE INIT  #################
read  -p "REMOTE INIT Enter para continuar... "
dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
#demonize -R <vmid> <endpoint> <rmtnodeid> <filename> <args...>
cd /home/MoL_Module/mol-ipc/commands/demonize
./demonize -R node$lcl $rmt $dcid 12 $rmt "/home/MoL_Module/mol-ipc/servers/init/init 0" > init$rmt.txt 2> error$rmt.txt &
sleep 2
cat /proc/dvs/DC$dcid/procs
exit
fi
################## NODE 1 #####################
sleep 2
cat /proc/dvs/DC$dcid/procs
exit

