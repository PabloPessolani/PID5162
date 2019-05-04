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
echo "================= building /tmp/rdisk$dcid.cfg ===================="
echo "device MY_FILE_IMG {" 			>   /tmp/rdisk$dcid.cfg
echo "	image_type		FILE_IMAGE;"	>>  /tmp/rdisk$dcid.cfg
echo " 	image_file 		\"/usr/src/dvs/vos/images/minixweb.img\";" 	>>  /tmp/rdisk$dcid.cfg
echo "	volatile		NO;"			>>  /tmp/rdisk$dcid.cfg
echo "	replicated		YES;"			>>  /tmp/rdisk$dcid.cfg
echo "  buffer			4096;" 			>>  /tmp/rdisk$dcid.cfg
echo "	active 			YES;"			>>  /tmp/rdisk$dcid.cfg
echo "};"								>>  /tmp/rdisk$dcid.cfg
cat /tmp/rdisk$dcid.cfg 
read  -p "Enter para continuar... "
dmesg -c  >> /usr/src/dvs/vos/mol/dmesg$lcl.txt
read  -p "RDISK Enter para continuar... "
#./rdisk rdisk  -d <dcid> [-e <endpoint>] [-R] [-D] [-Z] [-U {FULL | DIFF }] -c <config_file>
echo "./rdisk -d $dcid -e $rd_ep -R -c /tmp/rdisk$dcid.cfg"
./rdisk -d $dcid -e $rd_ep -R -c /tmp/rdisk$dcid.cfg > rdisk$lcl.txt 2> rdisk_err$lcl.txt &
sleep 2
dmesg -c  >> /usr/src/dvs/dvk-tests/dmesg.txt
sleep 2
cat /proc/dvs/DC$dcid/procs
exit

