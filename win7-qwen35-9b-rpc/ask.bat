@echo off
cd /d "%~dp0"
chcp 65001 >nul
call config.bat
rem Multi-turn chat with the cluster from ANY PC. Type a question and press Enter.
rem /clear = new conversation, /exit or Ctrl+C = quit. Speed (t/s) is shown after each answer.
llama-cli.exe --server-base http://%SERVER%:8080 --color off --log-colors off --simple-io
pause
