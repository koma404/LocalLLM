@echo off
cd /d "%~dp0"
chcp 65001 >nul
call config.bat
rem Single-turn speed test from ANY PC (needs main.bat running). Prompt from prompt.txt.
llama-cli.exe --server-base http://%SERVER%:8080 -f prompt.txt -n 200 -st ^
  --color off --log-colors off --simple-io
echo.
echo ==== Look for the line "[ Prompt: xx t/s | Generation: xx t/s ]" above ====
pause
