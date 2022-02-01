cd C:\Program Files (x86)\VMware\VMware Workstation\
rem vmrun -T ws  start "E:\NODE2\Debian 9.4.vmx" 
echo %1 %2 %3
vmrun -T ws -u %1 -p %2 start %3 nogui
:loop1
	vmrun list 
	sleep 60
goto loop1
