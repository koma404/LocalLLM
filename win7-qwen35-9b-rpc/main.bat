@echo off
cd /d "%~dp0"
call config.bat
set /a CTX=%USERS%*2048
echo Workers: %WORKERS%
echo Checking workers with ping (no reply may just be the firewall blocking ping)...
for %%W in (%WORKERS:,= %) do for /f "tokens=1 delims=:" %%H in ("%%W") do ping -n 1 -w 1000 %%H >nul && (echo   %%H OK) || (echo   %%H no ping reply)
echo.
echo Loading Qwen3.5-9B across the cluster (no web UI). First start sends ~5 GB over the LAN:
echo   1 Gbps: about 1-2 min    100 Mbps: about 8-10 min. Later starts use the worker cache.
echo When you see "server is listening", run ask.bat on any PC. Keep this window open.
echo WARNING: no password. Anyone on this LAN can use port 8080 while this window is open.
echo.
llama-server.exe -m Qwen3.5-9B-Q4_K_M.gguf --rpc %WORKERS% -ngl %NGL% -ts %TS% -fit off ^
  -lm none -t 4 -c %CTX% -np %USERS% --no-repack --cache-ram 0 --ctx-checkpoints 2 ^
  --no-webui --reasoning off --host 0.0.0.0 --port 8080 ^
  --temp 0.7 --top-p 0.8 --top-k 20
pause
