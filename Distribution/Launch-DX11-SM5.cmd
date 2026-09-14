@echo off
setlocal
if not exist "%~dp0FestaFiesta.exe" (
    echo FestaFiesta.exe was not found. Extract the entire package first.
    pause
    exit /b 1
)
start "" /D "%~dp0" "%~dp0FestaFiesta.exe" -dx11 -sm5
