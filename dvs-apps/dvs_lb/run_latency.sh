#!/bin/bash
# usage <dcid> <svr_ep> <clt_node> <clt_ep> <clt_name>
# obtener el PID del DC0 (se puede obviar)
# hacer el bind del client 
if [ $# -ne 4 ]
then 
 echo "./run_latency.sh <dcid> <svr_ep> <clt_node> <clt_ep>"
 exit 1 
fi
dcid=$1
svr_ep=$2
clt_node=$3
clt_ep=$4
clt_name="clt_"$clt_node"_"$dcid"_"$clt_ep 
echo run_latency.sh $dcid $svr_ep $clt_node $clt_ep $clt_name
cd /usr/src/dvs/dvk-tests/
./test_rmtbind $dcid $clt_ep $clt_node $clt_name
./latency_server  $dcid $svr_ep > /tmp/latency$svr_ep.out 2> /tmp/latency$svr_ep.err &
cd /usr/src/dvs/dvs-apps/dvs_run/
./wait4bind $dcid 0 $svr_ep
exit 


