# Test in parallelo: compila la ROM di test (WSL), la divide in N parti e le esegue insieme su Windows.
# Uso: .\tools\vgc_training\test_veloci.ps1 -Tests "test/battle/ai/ai_vgc_doubles.c test/battle/ai/ai_vgc_teams.c" [-N 16]
param(
    [string]$Tests = "test/battle/ai/ai_vgc_doubles.c test/battle/ai/ai_vgc_team_preview.c test/battle/ai/ai_vgc_scout.c test/battle/ai/ai_vgc_teams.c",
    [int]$N = 16,
    [switch]$NoBuild   # usa le parti già in build\shards
)
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$wslRoot = "/mnt/" + $root.Substring(0, 1).ToLower() + ($root.Substring(2) -replace '\\', '/')
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH

if (-not $NoBuild) {
    wsl bash -lc "cd '$wslRoot' && make -j32 pokeemerald-test.elf TESTS='$Tests' > build/test_build.log 2>&1 || { grep -E 'error' build/test_build.log | head -30; exit 1; }; tools/vgc_training/shard_tests.sh $N"
    if ($LASTEXITCODE -ne 0) { Write-Host "COMPILAZIONE FALLITA"; exit 1 }
}

$out = Join-Path $root "build\shards"
$procs = for ($i = 0; $i -lt $N; $i++) {
    Start-Process -FilePath (Join-Path $root "tools\mgba\mgba-rom-test.exe") `
        -ArgumentList "-l15", "-ClogLevel.gba.dma=16", "-Rr0", "`"$out\shard_$i.gba`"" `
        -RedirectStandardOutput "$out\shard_$i.out" -RedirectStandardError "$out\shard_$i.err" `
        -NoNewWindow -PassThru
}
$procs | Wait-Process

$all = Get-Content "$out\shard_*.out", "$out\shard_*.err" -ErrorAction SilentlyContinue
$all | Set-Content "$out\risultati.txt"
# Codici del test runner (test/test_runner.c): :N nome, :P pass, :K known failing, :F fail, :T timeout,
# :A assumption, :E errore/crash, :U/:V known failing che passa.
$codes = [ordered]@{ PASS = ':P'; KNOWN_FAILING = ':K'; KNOWN_FAILING_PASSA = ':U|:V'; FAIL = ':F'; TIMEOUT = ':T'; ASSUMPTION = ':A'; ERRORE = ':E' }
foreach ($k in $codes.Keys) {
    "{0,-20} {1}" -f $k, ($all | Select-String -Pattern ("GBA Debug: (" + $codes[$k] + ")") ).Count
}
$name = ''
foreach ($line in $all) {
    if ($line -match 'GBA Debug: :N(.*)') { $name = $Matches[1] }
    elseif ($line -match 'GBA Debug: :([FTEUV])') { Write-Host ("  " + $Matches[1] + "  " + $name) }
}
Write-Host "Dettagli: $out\risultati.txt"
