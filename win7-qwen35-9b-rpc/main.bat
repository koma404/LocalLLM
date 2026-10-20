@echo off
cd /d "%~dp0"
call config.bat
echo Workers: %WORKERS%
echo Checking workers with ping...
for %%W in (%WORKERS:,= %) do for /f "tokens=1 delims=:" %%H in ("%%W") do ping -n 1 -w 1000 %%H >nul && (echo   %%H OK) || (echo   %%H no ping reply - may just be the firewall blocking ping)
echo.
echo Loading Qwen3.5-9B across the cluster. First start sends ~5 GB over the LAN:
echo   1 Gbps: about 1-2 min    100 Mbps: about 8-10 min. Later starts use the worker cache.
echo When you see "server is listening", open http://127.0.0.1:8080 in Supermium.
echo.
llama-server.exe -m Qwen3.5-9B-Q4_K_M.gguf --rpc %WORKERS% -ngl %NGL% -ts %TS% -fit off ^
  -lm none -t 4 -c 4096 -np 1 --no-repack --cache-ram 0 --ctx-checkpoints 2 ^
  --reasoning off --host 127.0.0.1 --port 8080 ^
  --temp 0.7 --top-p 0.8 --top-k 20
pause
