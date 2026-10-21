@echo off
rem Same as ask.bat with thinking ON (max 256 tokens of thinking, ~2 min at 2.5 t/s).
call "%~dp0ask.bat" --reasoning on --reasoning-budget 256
