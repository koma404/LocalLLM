@echo off
setlocal
pushd "%~dp0"
rem Multi-turn chat with the cluster. /clear = new conversation, /exit or Ctrl+C = quit.
if exist cluster\server.txt goto have_server
echo The main PC has never been started (cluster\server.txt missing).
pause
exit /b 1
:have_server
set /p SERVER=<cluster\server.txt
set TRIES=0
:wait
bin\qwen-helper.exe health %SERVER%:8080 2000 && goto connected
if %TRIES%==0 echo Waiting for the main PC %SERVER% (still loading, or not started)...
set /a TRIES+=1
if %TRIES% GEQ 60 goto fail
ping -n 6 127.0.0.1 >nul
goto wait
:fail
echo Cannot reach http://%SERVER%:8080 after 5 minutes. Is start_main.bat running?
echo If it is, the main PC firewall may be blocking port 8080 (see README).
pause
exit /b 1
:connected
chcp 65001 >nul
bin\llama-cli.exe --server-base http://%SERVER%:8080 --color off --log-colors off --simple-io %*
pause
