@echo off
cd C:\Program Files (x86)\VMware\VMware Workstation\
rem echo %1 %2 %3
vmrun -T ws start "%3" nogui
exit


