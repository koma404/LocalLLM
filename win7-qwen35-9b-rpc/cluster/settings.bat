@echo off
rem ===== Cluster settings (shared by every PC) =====
rem Local folder on each PC for the model copy (main) and the weight cache (workers).
rem Must be on a partition that the restore card does NOT wipe.
set LOCAL_DIR=D:\qwen9b
rem
rem How many people can generate at the same time (more are queued).
set USERS=3
rem
rem Total context (tokens) shared by all users: one user alone can use all of it.
rem Each 1000 tokens costs only ~32 MB in total (spread over the PCs), so it can be large.
rem 16384 adds ~0.15 GB per worker compared with the old 6144; 32768 adds ~0.4 GB.
set CTX_TOTAL=16384
rem
rem Auto-pause: the status window on the main PC stops using a worker that keeps running
rem out of memory / swapping (then restarts the server without it), but never goes below MIN_WORKERS.
set AUTO_PAUSE=1
set MIN_WORKERS=3
rem
rem Image input (multimodal): 1 = on, 0 = off. Needs model\mmproj-F16.gguf (0.9 GB).
rem The vision part runs on the main PC only (+~0.9 GB memory there); workers are not affected.
rem IMAGE_TOKENS = max tokens one image becomes: bigger = more detail but slower (256 is ~10-20 s).
set VISION=1
set IMAGE_TOKENS=256
rem
rem Optional: lock the roles to fixed PCs by computer name (run "hostname" to see it).
rem Leave empty to allow any PC. WORKER_PCS is a space-separated list.
set MAIN_PC=
set WORKER_PCS=
rem
rem Optional: full path of the browser for open_ui.bat (empty = default browser), e.g.
rem set BROWSER=C:\Program Files\Supermium\chrome.exe
set BROWSER=
