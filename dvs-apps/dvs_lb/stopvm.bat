cd C:\Program Files (x86)\VMware\VMware Workstation\
rem vmrun -T ws  start "E:\NODE2\Debian 9.4.vmx" 
echo %1 %2 %3
vmrun -T ws -u %2 -p %1 stop %3 
vmrun list 
sleep 60
