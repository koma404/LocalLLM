@echo off
cd /d "%~dp0"
chcp 65001 >nul
call config.bat
rem Single-turn command-line test across the cluster (thinking off), prints t/s at the end.
llama-cli.exe -m Qwen3.5-9B-Q4_K_M.gguf --rpc %WORKERS% -ngl %NGL% -ts %TS% -fit off ^
  -lm none -f prompt.txt -t 4 -c 2048 -n 200 --reasoning off -st --no-repack ^
  --color off --log-colors off --simple-io --temp 0.7 --top-p 0.8 --top-k 20
echo.
echo ==== Look for the line "[ Prompt: xx t/s | Generation: xx t/s ]" above ====
pause
