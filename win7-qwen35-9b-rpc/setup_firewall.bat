@echo off
rem Run ONCE as administrator (right-click -> Run as administrator) on the main PC and on each worker.
net session >nul 2>&1 || (echo Please right-click this file and choose "Run as administrator". & pause & exit /b 1)
netsh advfirewall firewall delete rule name="qwen9b-server" >nul 2>&1
netsh advfirewall firewall delete rule name="qwen9b-rpc" >nul 2>&1
netsh advfirewall firewall add rule name="qwen9b-server" dir=in action=allow protocol=TCP localport=8080
netsh advfirewall firewall add rule name="qwen9b-rpc" dir=in action=allow protocol=TCP localport=50052
echo Done: TCP 8080 (main) and 50052 (worker) are allowed inbound.
pause
