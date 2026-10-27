@echo off
REM Self-play VGC: pause (vedi docs\ia_vgc\training.md)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0training.ps1" pause %*
pause
