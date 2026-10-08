@echo off
rem Moves Factorio's controls to the keys FactorioAccess expects. Run it from the game folder, with
rem the game closed. Pass -Revert to undo. See factorio-access-keys.ps1 for what changes.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0factorio-access-keys.ps1" %*
pause
