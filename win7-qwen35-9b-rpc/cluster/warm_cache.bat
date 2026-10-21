@echo off
rem Pre-computes the system prompt on the main PC, so the first question does not wait for it.
rem Started by start_main.bat for every server launch; it closes by itself.
rem  - If cache\sys.bin matches the current prompt, model, context and users: restore it (fast).
rem  - Otherwise: send the prompt once, then save the server's slot 0 to cache\sys.bin.
rem Log: cache\warm.log in LOCAL_DIR.
setlocal
set CACHE=%LOCAL_DIR%\cache
set SERVER=127.0.0.1:8080
set LOG=%CACHE%\warm.log
set KEY=%TEMP%\qwen_cache_key.txt
set SYSF=%TEMP%\qwen_cache_system.txt
set BODY=%TEMP%\qwen_cache_body.json
if not exist "%CACHE%" mkdir "%CACHE%"
bin\qwen-helper.exe prompt "%ROOT%\cluster\system-prompt.txt" "%SYSF%"
rem The key holds everything that changes the cached tokens or the server's memory layout.
copy /y "%SYSF%" "%KEY%" >nul
echo model=%MODEL% ctx=%CTX_TOTAL% users=%USERS%>> "%KEY%"
for %%F in ("%ROOT%\bin\llama-server.exe") do echo exe=%%~zF>> "%KEY%"
echo [%TIME%] waiting for the server (launch %LAUNCH%)>> "%LOG%"
set /a TRIES=0
:wait
bin\qwen-helper.exe health %SERVER% 1000 >nul 2>&1
if not errorlevel 1 goto ready
set /a TRIES+=1
if %TRIES% GEQ 900 goto giveup
ping -n 3 127.0.0.1 >nul
goto wait
:giveup
echo [%TIME%] the server did not become ready, giving up>> "%LOG%"
exit /b 1
:ready
rem a newer server launch has started since this one: let that window do the work
set /p CUR=<"%ROOT%\cluster\cache_launch.txt"
if not "%CUR%"=="%LAUNCH%" exit /b 0
bin\qwen-helper.exe same "%KEY%" "%CACHE%\key.txt"
if errorlevel 1 goto build
if not exist "%CACHE%\sys.bin" goto build
echo {"filename":"sys.bin"}>"%BODY%"
bin\qwen-helper.exe post %SERVER% "/slots/0?action=restore" "%BODY%" >> "%LOG%" 2>&1
if errorlevel 1 goto build
echo [%TIME%] restored the system prompt from cache>> "%LOG%"
exit /b 0
:build
echo [%TIME%] computing the system prompt (first time, or the settings changed)>> "%LOG%"
del "%CACHE%\key.txt" >nul 2>&1
bin\llama-cli.exe --server-base http://%SERVER% -sysf "%SYSF%" -p "hi" -n 1 -st --color off --log-colors off --simple-io >> "%LOG%" 2>&1
echo {"filename":"sys.bin"}>"%BODY%"
bin\qwen-helper.exe post %SERVER% "/slots/0?action=save" "%BODY%" >> "%LOG%" 2>&1
if errorlevel 1 goto failed
copy /y "%KEY%" "%CACHE%\key.txt" >nul
echo [%TIME%] saved the system prompt to cache>> "%LOG%"
exit /b 0
:failed
echo [%TIME%] could not save the cache; the first questions will be slower>> "%LOG%"
exit /b 1
