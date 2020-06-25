#!/bin/bash
if [ $# -ne 1 ]
then 
	echo "usage: $0 <dcid>"
	exit
fi
dcid=$1
lcl=$NODEID
rd_ep=3
################# START RDISK  #################
cd /usr/src/dvs/vos/mol/drivers/rdisk
echo "================= building /dev/shm/rdisk$dcid.cfg ===================="
echo "device MY_FILE_IMG {" 			>   /dev/shm/rdisk$dcid.cfg
echo "	image_type		FILE_IMAGE;"	>>  /dev/shm/rdisk$dcid.cfg
echo " 	image_file 		\"/dev/shm/rdisk.img\";" 	>>  /dev/shm/rdisk$dcid.cfg
echo "	volatile		NO;"			>>  /dev/shm/rdisk$dcid.cfg
echo "	replicated		NO;"			>>  /dev/shm/rdisk$dcid.cfg
echo "  buffer			4096;" 			>>  /dev/shm/rdisk$dcid.cfg
echo "	active 			YES;"			>>  /dev/shm/rdisk$dcid.cfg
echo "};"								>>  /dev/shm/rdisk$dcid.cfg
cat /dev/shm/rdisk$dcid.cfg 
read  -p "Enter para continuar... "
dmesg -c  >> /dev/shm/dmesg$lcl.txt
cp /usr/src/dvs/vos/images/floppyext2.img /dev/shm/rdisk.img
ls -l /dev/shm/rdisk.img
fsck -vf  /dev/shm/rdisk.img
read  -p "RDISK Enter para continuar... "
#./rdisk rdisk  -d <dcid> [-e <endpoint>] [-R] [-D] [-Z] [-U {FULL | DIFF }] -c <config_file>
echo "./rdisk -d $dcid -e $rd_ep -R -c /dev/shm/rdisk$dcid.cfg"
./rdisk -d $dcid -e $rd_ep -c /dev/shm/rdisk$dcid.cfg > /dev/shm/rdisk$lcl.txt 2> /dev/shm/rdisk_err$lcl.txt &
sleep 2
dmesg -c  >> /dev/shm/dmesg.txt
sleep 2
cat /proc/dvs/DC$dcid/procs
exit

