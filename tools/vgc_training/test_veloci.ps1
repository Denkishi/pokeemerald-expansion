# Test in parallelo: compila UNA volta la ROM di test (WSL; la ELF contiene tutti i test), poi per ogni file la divide
# in N parti con il filtro su quel file (gTestRunnerArgv: il test runner accetta UN solo file, confronto esatto) e le
# esegue insieme su Windows. Alla fine somma i risultati.
# Uso: .\tools\vgc_training\test_veloci.ps1 [-Tests "test/battle/ai/ai_vgc_doubles.c test/battle/ai/ai_vgc_teams.c"] [-N 16] [-NoBuild] [-Out build\shards]
# -Out: cartella delle parti (una diversa per ogni sessione che lancia i test nello stesso momento).
param(
    [string]$Tests = "test/battle/ai/ai_vgc_doubles.c test/battle/ai/ai_vgc_team_preview.c test/battle/ai/ai_vgc_scout.c test/battle/ai/ai_vgc_teams.c test/battle/move_effect/parting_shot.c",
    [int]$N = 16,
    [switch]$NoBuild,   # usa la pokeemerald-test.elf già compilata
    [string]$Out = "build\shards"
)
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$wslRoot = "/mnt/" + $root.Substring(0, 1).ToLower() + ($root.Substring(2) -replace '\\', '/')
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
# PowerShell non distingue le maiuscole: $Out (parametro) e $out sarebbero la stessa variabile. Prima il percorso
# relativo per WSL, poi quello assoluto per Windows in $out.
$wslOut = $Out -replace '\\', '/'
$out = Join-Path $root $Out
$files = $Tests -split '\s+' | Where-Object { $_ -ne '' }
$allResults = @()
$missing = @()

if (-not $NoBuild) {
    wsl bash -lc "cd '$wslRoot' && make -j32 pokeemerald-test.elf TESTS='$($files[0])' > build/test_build.log 2>&1 || { grep -E 'error' build/test_build.log | head -30; exit 1; }"
    if ($LASTEXITCODE -ne 0) { Write-Host "COMPILAZIONE FALLITA"; exit 1 }
}

$fi = 0
foreach ($file in $files) {
    Write-Host "=== $file"
    # Nomi diversi per ogni file: Windows può leggere una vecchia copia di un file appena riscritto da WSL.
    $pre = "f$fi"
    $fi++
    wsl bash -lc "cd '$wslRoot' && bash tools/vgc_training/shard_tests.sh $N '$file' '$wslOut' '$pre' | tail -1"
    if ($LASTEXITCODE -ne 0) { Write-Host "DIVISIONE FALLITA ($file)"; exit 1 }
    Remove-Item "$out\shard_*.out", "$out\shard_*.err" -ErrorAction SilentlyContinue
    $procs = for ($i = 0; $i -lt $N; $i++) {
        Start-Process -FilePath (Join-Path $root "tools\mgba\mgba-rom-test.exe") `
            -ArgumentList "-l15", "-ClogLevel.gba.dma=16", "-Rr0", "`"$out\${pre}_$i.gba`"" `
            -RedirectStandardOutput "$out\shard_$i.out" -RedirectStandardError "$out\shard_$i.err" `
            -NoNewWindow -PassThru
    }
    $procs | Wait-Process
    $res = Get-Content "$out\shard_*.out", "$out\shard_*.err" -ErrorAction SilentlyContinue
    $res | Set-Content (Join-Path $out ("risultati_" + ($file -replace '[\\/]', '_') + ".txt"))
    $allResults += $res
    $cnt = ($res | Select-String -Pattern "GBA Debug: :[PKUVFTAE]").Count
    Write-Host ("  {0} test" -f $cnt)
    Remove-Item "$out\${pre}_*.gba" -ErrorAction SilentlyContinue
    if ($cnt -eq 0) { $missing += "  ?  nessun test eseguito in $file" }
}

$allResults | Set-Content "$out\risultati.txt"
# Codici del test runner (test/test_runner.c): :N nome, :P pass, :K known failing, :F fail, :T timeout,
# :A assumption, :E errore/crash, :U/:V known failing che passa.
$codes = [ordered]@{ PASS = ':P'; KNOWN_FAILING = ':K'; KNOWN_FAILING_PASSA = ':U|:V'; FAIL = ':F'; TIMEOUT = ':T'; ASSUMPTION = ':A'; ERRORE = ':E' }
Write-Host "=== Totale ($($files.Count) file)"
foreach ($k in $codes.Keys) {
    "{0,-20} {1}" -f $k, ($allResults | Select-String -Pattern ("GBA Debug: (" + $codes[$k] + ")") ).Count
}
$name = ''
foreach ($line in $allResults) {
    if ($line -match 'GBA Debug: :N(.*)') { $name = $Matches[1] }
    elseif ($line -match 'GBA Debug: :([FTEUV])') { Write-Host ("  " + $Matches[1] + "  " + $name) }
}
$missing | ForEach-Object { Write-Host $_ }
Write-Host "Dettagli: $out\risultati.txt (e risultati_<file>.txt)"
