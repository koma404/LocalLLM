@echo off
rem Same as ask.bat with thinking ON (max 400 tokens of thinking, ~3 min at 2 t/s).
call "%~dp0ask.bat" --reasoning on --reasoning-budget 400
