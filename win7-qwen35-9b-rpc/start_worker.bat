@echo off
setlocal
pushd "%~dp0"
set ROOT=%CD%
call cluster\settings.bat
title Qwen3.5-9B WORKER
if not defined WORKER_PCS goto role_ok
echo  %WORKER_PCS% | findstr /i /c:" %COMPUTERNAME% " >nul && goto role_ok
echo This PC is %COMPUTERNAME%, which is not in WORKER_PCS in cluster\settings.bat.
pause
exit /b 1
:role_ok
if not exist "%LOCAL_DIR%\rpc-cache" mkdir "%LOCAL_DIR%\rpc-cache"
set LLAMA_CACHE=%LOCAL_DIR%\rpc-cache
for /f %%i in ('bin\qwen-helper.exe myip') do set MYIP=%%i
del "%ROOT%\cluster\workers\%COMPUTERNAME%.txt" >nul 2>&1
echo %MYIP%:50052> "%ROOT%\cluster\workers\%COMPUTERNAME%.txt"
if not exist "%ROOT%\cluster\workers\%COMPUTERNAME%.txt" (
  echo Cannot write to %ROOT%\cluster\workers - check write permission on the share.
  pause & exit /b 1
)
echo Registered %COMPUTERNAME% = %MYIP%:50052. Keep this window open.
echo WARNING: no password. Only use inside the lab LAN.
:loop
bin\ggml-rpc-server.exe -H 0.0.0.0 -p 50052 -t 4 -c
echo RPC server stopped, restarting in 5 s...
ping -n 6 127.0.0.1 >nul
goto loop
