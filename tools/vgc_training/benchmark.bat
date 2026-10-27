@echo off
REM Self-play VGC: partite all'ora (es. benchmark.bat --parallel 28 --games 300 --quiet)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0training.ps1" bench %*
pause
