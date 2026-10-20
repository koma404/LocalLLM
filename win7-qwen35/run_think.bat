@echo off
cd /d "%~dp0"
chcp 65001 >nul
rem Same as run.bat but with thinking ON.
rem --reasoning-budget 400: stop thinking after ~400 tokens and answer (the 2B model tends to overthink)
llama-cli.exe -m Qwen3.5-2B-Q4_K_M.gguf -f prompt.txt -t 4 -c 2048 -n 1024 ^
  --reasoning on --reasoning-budget 400 -st --no-repack --color off --log-colors off --simple-io ^
  --temp 1.0 --top-p 0.95 --top-k 20
echo.
echo ==== Look for the line "[ Prompt: xx t/s | Generation: xx t/s ]" above ====
pause
