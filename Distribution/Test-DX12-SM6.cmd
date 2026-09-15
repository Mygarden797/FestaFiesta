@echo off
setlocal
cd /d "%~dp0"
if not exist "DebugLogs" mkdir "DebugLogs"
start "" "%~dp0FestaFiesta.exe" -dx12 -sm6 -log -abslog="%~dp0DebugLogs\Test-SM6-%RANDOM%-%RANDOM%.log" -windowed -ResX=1600 -ResY=900
