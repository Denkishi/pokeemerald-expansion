@echo off
REM Self-play VGC: rigenera src/data/vgc_plan_book.h dall'esplorazione (poi compila.bat)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0training.ps1" libro %*
pause
