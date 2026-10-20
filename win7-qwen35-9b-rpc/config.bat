@echo off
rem ===== Same file on EVERY PC (copy it after editing) =====
rem Main PC IP (the PC that runs main.bat). Every PC asks questions through it.
set SERVER=192.168.1.10
rem
rem Worker IPs (each runs worker.bat, which prints its IP). Port 50052.
rem --- Plan A (recommended): 4 PCs = 1 main + 3 workers ---
set WORKERS=192.168.1.11:50052,192.168.1.12:50052,192.168.1.13:50052
set NGL=99
set TS=12,12,7
rem
rem --- Plan B (minimum): 3 PCs = main also holds layers + 2 workers ---
rem set WORKERS=192.168.1.11:50052,192.168.1.12:50052
rem set NGL=23
rem set TS=14,9
rem
rem How many people can ask at the same time (each gets 2048 tokens of context)
set USERS=3
