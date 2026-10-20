@echo off
setlocal DisableDelayedExpansion

:: ---------------------------------------------------------------------------
::  pack_latest.bat
::  Double-click to rebuild context_bundle.txt from the latest pack.
::  It only calls the main packer with -Repack, so it must sit in the same
::  folder. If you rename the main packer, change the name on the next line.
:: ---------------------------------------------------------------------------
set "MAIN=%~dp0pack_code_2.1.bat"

if exist "%MAIN%" goto Run
echo [Error] Cannot find the main packer:
echo         "%MAIN%"
echo         Keep both files in the same folder, or edit the MAIN line in this file.
pause
exit /b 1

:Run
call "%MAIN%" -Repack -NoPause
set "RESULT=%ERRORLEVEL%"

:: Something went wrong: keep the window open so the message can be read.
if not "%RESULT%"=="0" pause & exit /b %RESULT%

:: Success: show the report for a moment, then close.
timeout /t 3 >nul
exit /b 0