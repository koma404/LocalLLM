@echo off
cd /d "%~dp0"
rem Web UI: open http://127.0.0.1:8080 in the browser (Supermium). Close this window to stop.
rem threads 4 / ctx 4096 / 1 slot / only reachable from this PC
rem Thinking: switch it per message with the lightbulb button next to the input box
rem (Default/Off = no thinking, Low = max 512 tokens, Medium = 2048 ...)
start "" cmd /c "ping -n 20 127.0.0.1 >nul & start http://127.0.0.1:8080"
llama-server.exe -m Qwen3.5-2B-Q4_K_M.gguf -t 4 -c 4096 -np 1 ^
  --no-repack --host 127.0.0.1 --port 8080 ^
  --temp 0.7 --top-p 0.8 --top-k 20
pause
