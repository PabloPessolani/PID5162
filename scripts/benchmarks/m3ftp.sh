#!/bin/bash
cd /dev/shm 
rm file??M.out
rm time_ftp.txt 
dcid=0
for k in 10  
do
	for i in {1..2}
	do
		from="file"$k"M.txt"
		to="file"$k"M.out"
		echo "M3FTP GET $from to $to" 
		nsenter -p -t$DC0 /usr/src/dvs/vos/muk/servers/m3ftp/m3ftp -g $dcid $from $to > time_ftp.tmp
		echo "M3FTP GET $from to $to" >> time_ftp.txt
		grep "total_bytes" time_ftp.tmp >> time_ftp.txt
		grep "Throuhput"   time_ftp.tmp >> time_ftp.txt
		sleep 1
	done
done
# PUT #################
exit
#####################
for k in 10  
do
	for i in {1..2}
	do
		from="file"$k"M.txt"
		to="file"$k"M.out"
		echo "M3FTP PUT $to to $from" 
		nsenter -p -t$DC0 /usr/src/dvs/vos/muk/servers/m3ftp/m3ftp -p $dcid $from $to > time_ftp.tmp
		echo "M3FTP PUT $to to $from >> time_ftp.txt
		grep "total_bytes" time_ftp.tmp >> time_ftp.txt
		grep "Throuhput"   time_ftp.tmp >> time_ftp.txt
		sleep 1
	done
done

exit
