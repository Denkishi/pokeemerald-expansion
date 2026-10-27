@echo off
REM Self-play VGC: taratura automatica dei pesi (SPSA + validazione su semi mai visti). Pausa/riprendi/ferma/stato come le campagne.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0training.ps1" tune %*
pause
