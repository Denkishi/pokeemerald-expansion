# Self-play VGC: comandi semplici per la campagna di partite IA contro IA.
#   .\training.ps1 build                  compila il ROM del self-play (WSL)
#   .\training.ps1 start [opzioni]        avvia o riprende (es. -- --parallel 24 --seeds 1)
#   .\training.ps1 pause                  pausa tra una partita e l'altra
#   .\training.ps1 resume                 riprende
#   .\training.ps1 stop                   ferma subito (le partite non finite restano in coda)
#   .\training.ps1 status                 avanzamento
#   .\training.ps1 report                 rigenera docs/ia_vgc/report
#   .\training.ps1 run-one 0 1 [seme]     una partita in primo piano
# Guida: docs/ia_vgc/training.md
param(
    [Parameter(Position = 0)][string]$Comando = 'status',
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$Resto
)
$ErrorActionPreference = 'Stop'
$script = Join-Path $PSScriptRoot 'selfplay.py'
$python = $null
foreach ($c in @('py', 'python', 'python3')) {
    $cmd = Get-Command $c -ErrorAction SilentlyContinue
    if ($cmd) { $python = $cmd.Source; break }
}
if (-not $python) { Write-Error 'Python 3 non trovato (serve py, python o python3 nel PATH).'; exit 1 }
$argsList = @()
if ((Split-Path $python -Leaf) -eq 'py.exe') { $argsList += '-3' }
$argsList += $script
$argsList += $Comando
if ($Resto) { $argsList += ($Resto | Where-Object { $_ -ne '--' }) }
& $python @argsList
exit $LASTEXITCODE
