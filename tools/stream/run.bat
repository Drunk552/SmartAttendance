@echo off
setlocal
set "SCRIPT=%~dp0stream.ps1"
powershell -NoProfile -ExecutionPolicy Bypass -NoExit -File "%SCRIPT%"
