@echo off
cd C:\Program Files (x86)\VMware\VMware Workstation\
vmrun list | find /I "%1" > C:\Users\Usuario\%1.out
exit
