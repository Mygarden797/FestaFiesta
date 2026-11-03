@echo off
setlocal EnableExtensions
rem ===================================================================
rem  LAUNCH:ER Day 7 - Unreal package build (Windows / Win64)
rem  Usage   : double-click, or  package_unreal.bat [output folder]
rem            e.g. package_unreal.bat D:\Builds\v0.1.1
rem  Output  : <output folder>\Windows\...   log: <output folder>.log
rem  Same command the editor runs for Platforms > Windows > Package Project.
rem  Day 8 adds a version number, folder name and zip on top of this.
rem  Comments are in English on purpose: Korean text in .bat files
rem  can break depending on the file encoding / console code page.
rem ===================================================================

rem ---- 1) Change these three lines for your PC ----------------------
if not defined UE_ROOT  set "UE_ROOT="C:\Unreal\UE_5.7""
if not defined PROJECT  set "PROJECT="C:\Users\gksro\OneDrive\Desktop\Git\FestaFiesta!\FestaFiesta\FestaFiesta.uproject""
if not defined OUT_DIR  set "OUT_DIR=C:\FestaFiestaBuild"
rem ---- Shipping: testers and submission / Development: your own tests
if not defined CONFIG   set "CONFIG=Shipping"
rem -------------------------------------------------------------------

set "RC=1"
set "PAUSE_AT_END=1"
if "%~1"=="" goto :args_done
set "OUT_DIR=%~1"
set "PAUSE_AT_END="
:args_done
if "%OUT_DIR:~-1%"=="\" set "OUT_DIR=%OUT_DIR:~0,-1%"
set "LOG=%OUT_DIR%.log"

set "UAT=%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat"
if not exist "%UAT%" goto :err_engine
if not exist "%PROJECT%" goto :err_project
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
if not exist "%OUT_DIR%" goto :err_out

echo [1/2] Packaging [%CONFIG%, Win64]
echo       Output : %OUT_DIR%
echo       Log    : %LOG%
echo       The first package takes long (shader compile). Keep this window open.

rem  Same flags the editor uses for Platforms ^> Windows ^> Package Project
rem  with a Launcher (installed) engine. Source-built engine: remove -installed.
rem  Tip: compare with the BuildCookRun line in your editor packaging log.
call "%UAT%" BuildCookRun ^
  -project="%PROJECT%" -platform=Win64 -clientconfig=%CONFIG% ^
  -nop4 -utf8output -unattended -installed ^
  -nocompileeditor -skipbuildeditor ^
  -build -cook -stage -package -pak -iostore -compressed -prereqs ^
  -archive -archivedirectory="%OUT_DIR%" > "%LOG%" 2>&1
if errorlevel 1 goto :err_build

echo [2/2] Done: %OUT_DIR%
echo       Run the .exe in the Windows folder there, then zip that whole folder.
set "RC=0"
goto :finish

:err_engine
echo [ERROR] RunUAT.bat not found. Check UE_ROOT: %UE_ROOT%
goto :finish

:err_project
echo [ERROR] .uproject not found. Check PROJECT: %PROJECT%
goto :finish

:err_out
echo [ERROR] Could not create the output folder: %OUT_DIR%
goto :finish

:err_build
echo [FAILED] Open the log and search for the FIRST line with "Error":
echo          %LOG%
echo          Try the editor menu once too. If that also fails,
echo          the problem is in the project, not in this script.
goto :finish

:finish
if defined PAUSE_AT_END pause
exit /b %RC%
