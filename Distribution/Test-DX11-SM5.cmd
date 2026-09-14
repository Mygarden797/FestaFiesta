@echo off
setlocal
cd /d "%~dp0"
if not exist "DebugLogs" mkdir "DebugLogs"
start "" "%~dp0FestaFiesta.exe" -dx11 -sm5 -log -abslog="%~dp0DebugLogs\Test-SM5-%RANDOM%-%RANDOM%.log" -windowed -ResX=1600 -ResY=900
