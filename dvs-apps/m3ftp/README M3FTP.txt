
PARA GENERAR UN ARCHIVO CON DATOS RANDOM DE UN TAMAÑO DADO 

dd bs=1024 count={tamaño_en_KB} </dev/urandom > {file.name}
dd bs=1024 count=10240 </dev/urandom > file-10M.out

CLIENT
======

Usage: m3ftp {-p|-g} <dcid> <clt_ep> <srv_ep> <rmt_fname> <lcl_fname> 

GET
 ./m3ftp -g 0 11 10 file-10M.out copy-10M.out
PUT 
 ./m3ftp -p 0 11 10 copy2-10M.out copy-10M.out

SERVER
=======
Usage: m3ftpd <dcid> <srv_ep>

LOGS DE RESULTADOS
================== 
m3ftpd.log 

root@node0:/usr/src/dvs/dvs-apps/m3ftp# more m3ftpd.log 
FTP_GET: t_start=1627482103.41 t_stop=1627482109.39 t_total=5.98
FTP_GET: total_bytes=10485760
FTP_GET: Throughput = 1752422.709175 [bytes/s]
FTP_PUT: t_start=1627482122.33 t_stop=1627482127.02 t_total=4.70
FTP_PUT: total_bytes=10485760
FTP_PUT: Throughput = 2232737.706558 [bytes/s]

PARA COMPARAR ARCHIVOS
=======================
root@node0:/usr/src/dvs/dvs-apps/m3ftp# openssl dgst file-100M.out 
SHA256(file-100M.out)= 5e006c01569b0215fd00f49eed18efc9971bb896c51403228792d56ab700992d

root@node0:/usr/src/dvs/dvs-apps/m3ftp# openssl dgst copy-100M.out 
SHA256(copy-100M.out)= 5e006c01569b0215fd00f49eed18efc9971bb896c51403228792d56ab700992d



./m3ftp -g 0 51 10 file-1M.out copy-1M.out