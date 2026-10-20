@echo off
rem Single-turn speed test with prompt.txt; the last line shows [ Prompt: xx t/s | Generation: xx t/s ].
call "%~dp0ask.bat" -f "%~dp0prompt.txt" -n 200 -st
