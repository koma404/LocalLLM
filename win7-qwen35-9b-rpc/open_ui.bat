@echo off
setlocal
pushd "%~dp0"
call cluster\settings.bat
rem Opens the chat web page of the cluster in the browser (Supermium).
if exist cluster\server.txt goto have_server
echo The main PC has never been started (cluster\server.txt missing).
pause
exit /b 1
:have_server
set /p SERVER=<cluster\server.txt
set URL=http://%SERVER%:8080
set TRIES=0
:wait
bin\qwen-helper.exe health %SERVER%:8080 2000 && goto connected
if %TRIES%==0 echo Waiting for the main PC %SERVER% (still loading, or not started)...
set /a TRIES+=1
set /a SECS=TRIES*5
echo   waited %SECS% s of max 300 s ...
if %TRIES% GEQ 60 goto fail
ping -n 6 127.0.0.1 >nul
goto wait
:fail
echo Cannot reach %URL% after 5 minutes. Is start_main.bat running?
echo If it is, the main PC firewall may be blocking port 8080 (see README).
pause
exit /b 1
:connected
echo Opening %URL%
echo If the page does not open, type this address into Supermium yourself.
if not defined BROWSER goto default_browser
if not exist "%BROWSER%" goto default_browser
start "" "%BROWSER%" %URL%
exit /b 0
:default_browser
start "" %URL%
