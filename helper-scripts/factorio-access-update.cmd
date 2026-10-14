@echo off
setlocal
rem Updates both parts of FactorioAccess from alex19EP/FactorioAccess's dev release. Close the game first.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0factorio-access-update.ps1" %*
set "update_result=%errorlevel%"
pause
exit /b %update_result%
