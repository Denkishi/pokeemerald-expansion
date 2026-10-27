@echo off
REM Self-play VGC: esplorazione per il libro dei piani (pausa/riprendi/ferma/stato come le campagne)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0training.ps1" esplora %*
pause
