#!/bin/bash
cd /dev/shm 
rm file??M.out
rm time_wget.txt 
for k in 10  
do
	for i in {1..2}
	do
		from="http://192.168.1.100:8081/file"$k"M.txt"
		to="file"$k"M.out"
		echo "WGET $from to $to" 
		rm $to
		wget -O $to $from  -o time_wget.tmp
		echo "WGET $from to $to" >> time_wget.txt
		grep "guardado" time_wget.tmp >> time_wget.txt
		rm time_wget.tmp
		sleep 1
	done
done
exit
