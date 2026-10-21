@echo off
setlocal
pushd "%~dp0"
set ROOT=%CD%
call cluster\settings.bat
title Qwen3.5-9B MAIN
if not defined MAIN_PC goto role_ok
if /i "%COMPUTERNAME%"=="%MAIN_PC%" goto role_ok
echo This PC is %COMPUTERNAME%, but MAIN_PC in cluster\settings.bat is %MAIN_PC%.
pause
exit /b 1
:role_ok
if not exist "%LOCAL_DIR%" mkdir "%LOCAL_DIR%"
set MODEL=%LOCAL_DIR%\Qwen3.5-9B-Q4_K_M.gguf
echo [1/3] Copying the model to %LOCAL_DIR% (only the first time, 5.7 GB)...
echo   Progress in %% below. If very slow: copy it to %LOCAL_DIR% with Explorer.
robocopy "%ROOT%\model" "%LOCAL_DIR%" Qwen3.5-9B-Q4_K_M.gguf /R:2 /W:5 /NJH /NJS /NDL
if not exist "%MODEL%" copy /y "%ROOT%\model\Qwen3.5-9B-Q4_K_M.gguf" "%MODEL%" >nul
if not exist "%MODEL%" (
  echo   Local copy failed, reading the model from the share instead ^(slower^).
  set MODEL=%ROOT%\model\Qwen3.5-9B-Q4_K_M.gguf
)
if not exist "%MODEL%" (
  echo   Model not found. Put Qwen3.5-9B-Q4_K_M.gguf into %ROOT%\model
  pause & exit /b 1
)
if not defined CTX_TOTAL set CTX_TOTAL=16384
rem Image input (optional): needs model\mmproj-F16.gguf; the vision part runs on the main PC only.
set MMPROJ_ARGS=
set VISION_INFO=off
if not "%VISION%"=="1" goto vision_done
if not exist "%ROOT%\model\mmproj-F16.gguf" (
  echo   VISION=1 but model\mmproj-F16.gguf is missing: starting without image input.
  goto vision_done
)
echo   Copying the vision file mmproj-F16.gguf ^(only the first time, 0.9 GB^)...
robocopy "%ROOT%\model" "%LOCAL_DIR%" mmproj-F16.gguf /R:2 /W:5 /NJH /NJS /NDL
set MMPROJ=%LOCAL_DIR%\mmproj-F16.gguf
if not exist "%MMPROJ%" set MMPROJ=%ROOT%\model\mmproj-F16.gguf
if not defined IMAGE_TOKENS set IMAGE_TOKENS=256
set MMPROJ_ARGS=--mmproj "%MMPROJ%" --no-mmproj-offload --image-max-tokens %IMAGE_TOKENS%
set VISION_INFO=on, max %IMAGE_TOKENS% tokens per image
:vision_done
if not exist "%ROOT%\cluster\paused" mkdir "%ROOT%\cluster\paused"
if not exist "%LOCAL_DIR%\cache" mkdir "%LOCAL_DIR%\cache"
if not defined CACHE_PROMPT set CACHE_PROMPT=1
rem web UI defaults for new users: system prompt from cluster\system-prompt.txt + the tools switch
if not defined TOOLS set TOOLS=1
bin\qwen-helper.exe uiconfig "%ROOT%\cluster\system-prompt.txt" "%LOCAL_DIR%\ui-config.json" %TOOLS%
set DASH_AUTO=
if "%AUTO_PAUSE%"=="1" set DASH_AUTO=auto %MIN_WORKERS%
rem live status window (users, workers, memory, speed); with AUTO_PAUSE=1 it also pauses workers that run out of memory
start "Qwen cluster status" bin\qwen-helper.exe dashboard "%ROOT%" 127.0.0.1:8080 %DASH_AUTO%

:loop
echo.
echo [2/3] Looking for workers registered in %ROOT%\cluster\workers ...
bin\qwen-helper.exe plan "%ROOT%\cluster\workers" > "%TEMP%\qwen_plan.bat"
call "%TEMP%\qwen_plan.bat"
if %N_WORKERS% LSS 2 (
  echo   Only %N_WORKERS% worker^(s^) online, need at least 2. Retrying in 20 s...
  ping -n 21 127.0.0.1 >nul
  goto loop
)
for /f %%i in ('bin\qwen-helper.exe myip') do set MYIP=%%i
echo %MYIP%> "%ROOT%\cluster\server.txt"
echo [3/3] Starting: %N_WORKERS% workers, -ngl %NGL% -ts %TS%, %USERS% users, context %CTX_TOTAL%, images %VISION_INFO%. Main IP %MYIP%
echo   First start sends ~5 GB to the workers (1 Gbps: 1-2 min, 100 Mbps: 8-10 min).
echo   When "listening on" appears, anyone can use open_ui.bat / ask.bat.
echo   Keep this window open.
if %N_WORKERS%==2 echo   NOTE: only 2 workers - the main PC also computes. Needs ~2.1 GB free here
if %N_WORKERS%==2 echo         and ~2 GB on each worker. Start a 3rd worker if it crashes.
rem a new id per launch; warm_cache.bat only works for the newest one
set LAUNCH=%RANDOM%%RANDOM%
echo %LAUNCH%> "%ROOT%\cluster\cache_launch.txt"
if "%CACHE_PROMPT%"=="1" start "Qwen cache" /min cmd /c ""%ROOT%\cluster\warm_cache.bat""
bin\llama-server.exe -m "%MODEL%" --rpc %WORKERS% -ngl %NGL% -ts %TS% -fit off ^
  %MMPROJ_ARGS% --ui-config-file "%LOCAL_DIR%\ui-config.json" -lm none -t 4 -c %CTX_TOTAL% --kv-unified -np %USERS% --slot-save-path "%LOCAL_DIR%\cache" --no-repack --cache-ram 0 --ctx-checkpoints 2 --metrics ^
  --reasoning off --reasoning-budget-message "\n\nOK, time is up. I will give the final answer now.\n" ^
  --host 0.0.0.0 --port 8080 ^
  --temp 0.7 --top-p 0.8 --top-k 20
echo.
echo Server stopped (a worker went offline or was paused for low memory). Restarting in 10 s, Ctrl+C to quit...
ping -n 11 127.0.0.1 >nul
goto loop
