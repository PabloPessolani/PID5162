#!/bin/bash
if [ $# -ne 1 ]
then 
	echo "usage: $0 <dcid>"
	exit
fi
dcid=$1
lcl=$NODEID
################# START RDISK  #################
cd /usr/src/dvs/vos/mol/drivers/rdisk
echo "================= building /tmp/rdisk$dcid.cfg ===================="
echo "device MY_FILE_IMG {" 			>   /tmp/rdisk$dcid.cfg
echo "	major			3;"				>>  /tmp/rdisk$dcid.cfg
echo "	minor			0;"				>>  /tmp/rdisk$dcid.cfg
echo "	type			FILE_IMAGE;"	>>  /tmp/rdisk$dcid.cfg
echo " 	image_file 		\"/usr/src/dvs/vos/mol/images/minixweb.img\";" 	>>  /tmp/rdisk$dcid.cfg
echo "	volatile		NO;"			>>  /tmp/rdisk$dcid.cfg
echo "	replicated		NO;"			>>  /tmp/rdisk$dcid.cfg
echo "  buffer			4096;" 			>>  /tmp/rdisk$dcid.cfg
echo "};"								>>  /tmp/rdisk$dcid.cfg
cat /tmp/rdisk$dcid.cfg 
read  -p "Enter para continuar... "
dmesg -c  >> /usr/src/dvs/vos/mol/dmesg$lcl.txt
read  -p "RDISK Enter para continuar... "
#./rdisk -r<replicate> -[f<full_update>|d<diff_updates>] -D<dyn_updates> -z<compress>  -c <config file>
echo "./rdisk -rdDec /tmp/rdisk$dcid.cfg > rdisk$lcl.txt 2> rdisk_err$lcl.txt"
./rdisk -ec /tmp/rdisk$dcid.cfg > rdisk$lcl.txt 2> rdisk_err$lcl.txt &
sleep 2
dmesg -c  >> /usr/src/dvs/dvk-tests/dmesg.txt
sleep 2
cat /proc/dvs/DC$dcid/procs
exit

