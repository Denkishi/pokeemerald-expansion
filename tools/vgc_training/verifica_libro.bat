@echo off
REM Self-play VGC: libro dei piani acceso contro spento, su semi mai visti
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0training.ps1" verifica-libro %*
pause
