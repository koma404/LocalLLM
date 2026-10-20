@echo off
cd /d "%~dp0"
chcp 65001 >nul
rem threads 4 / ctx 1024 / thinking off / single turn / prompt from prompt.txt (UTF-8)
llama-cli.exe -m Qwen3.5-2B-Q4_K_M.gguf -f prompt.txt -t 4 -c 1024 -n 256 ^
  --reasoning off -st --no-repack --color off --log-colors off --simple-io ^
  --temp 0.7 --top-p 0.8 --top-k 20
echo.
echo ==== Look for the line "[ Prompt: xx t/s | Generation: xx t/s ]" above ====
pause
