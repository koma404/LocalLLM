@echo off
cd /d "%~dp0"
chcp 65001 >nul
call config.bat
rem Same as ask.bat but with thinking ON, limited to 400 tokens (at ~2 t/s that is ~3 min per answer).
llama-cli.exe --server-base http://%SERVER%:8080 --reasoning on --reasoning-budget 400 ^
  --color off --log-colors off --simple-io
pause
