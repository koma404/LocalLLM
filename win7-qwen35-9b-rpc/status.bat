@echo off
setlocal
pushd "%~dp0"
rem Live cluster status from ANY PC (read only): users, workers, memory, swapping, speed.
if exist cluster\server.txt goto have_server
echo The main PC has never been started (cluster\server.txt missing).
pause
exit /b 1
:have_server
set /p SERVER=<cluster\server.txt
bin\qwen-helper.exe dashboard "%CD%" %SERVER%:8080
