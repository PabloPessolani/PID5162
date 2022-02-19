@echo off
cd C:\Program Files (x86)\VMware\VMware Workstation\
rem echo %1 %2 %3
vmrun -T ws -u %2 -p %1 stop %3 
exit

