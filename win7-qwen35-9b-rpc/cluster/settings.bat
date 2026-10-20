@echo off
rem ===== Cluster settings (shared by every PC) =====
rem Local folder on each PC for the model copy (main) and the weight cache (workers).
rem Must be on a partition that the restore card does NOT wipe.
set LOCAL_DIR=D:\qwen9b
rem
rem How many people can generate at the same time (each gets 2048 tokens of context).
set USERS=3
rem
rem Optional: lock the roles to fixed PCs by computer name (run "hostname" to see it).
rem Leave empty to allow any PC. WORKER_PCS is a space-separated list.
set MAIN_PC=
set WORKER_PCS=
rem
rem Optional: full path of the browser for open_ui.bat (empty = default browser), e.g.
rem set BROWSER=C:\Program Files\Supermium\chrome.exe
set BROWSER=
