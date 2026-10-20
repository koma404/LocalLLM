@echo off
cd /d "%~dp0"
rem pp128 = prompt processing speed, tg64 = generation speed (t/s), 3 repetitions
echo Running llama-bench, please wait (about 1-3 minutes)...
llama-bench.exe -m Qwen3.5-2B-Q4_K_M.gguf -t 4 -p 128 -n 64 -r 3 --repack 0 > bench_result.txt 2> bench_log.txt
type bench_result.txt
echo.
echo Result saved to bench_result.txt  (errors, if any: bench_log.txt)
pause
