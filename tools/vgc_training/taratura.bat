@echo off
REM Self-play VGC: taratura automatica dei pesi (SPSA). Pausa/riprendi/ferma/stato funzionano come per le campagne.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0training.ps1" tune %*
pause
