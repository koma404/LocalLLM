@echo off
rem ===== Edit this file on the MAIN machine only =====
rem Worker IPs (run worker.bat on each, it prints its IP). Port 50052.
rem
rem --- Plan A (recommended): 4 PCs = 1 main + 3 workers ---
set WORKERS=192.168.1.11:50052,192.168.1.12:50052,192.168.1.13:50052
set NGL=99
set TS=12,12,7
rem
rem --- Plan B (minimum): 3 PCs = main also holds layers + 2 workers ---
rem set WORKERS=192.168.1.11:50052,192.168.1.12:50052
rem set NGL=23
rem set TS=14,9
