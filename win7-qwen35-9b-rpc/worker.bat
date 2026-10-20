@echo off
cd /d "%~dp0"
rem Worker node: exposes this PC's CPU to the main PC over the LAN (port 50052).
rem WARNING: no password/auth. Only use inside a trusted lab LAN; close this window when done.
rem -c: cache received weights in .\rpc-cache so the next start does not resend them (~1.6 GB)
set LLAMA_CACHE=%~dp0rpc-cache
echo ===== This PC's IP address (put it in config.bat on the main PC) =====
ipconfig | findstr /i "IPv4"
echo =====================================================================
ggml-rpc-server.exe -H 0.0.0.0 -p 50052 -t 4 -c
pause
