#!/bin/bash
# Divide pokeemerald-test.elf in N ROM headless (gTestRunnerN/I), da eseguire in parallelo su Windows
# con test_veloci.ps1. Uso (WSL, dalla radice del progetto):
#   tools/vgc_training/shard_tests.sh N [file_di_test] [cartella] [prefisso]
# Con file_di_test si riscrive anche il filtro (gTestRunnerArgv): la ELF contiene tutti i test, quindi basta
# compilarla una volta e dividerla per ogni file.
set -e
N=${1:-16}
FILE=${2:-}
OUT=${3:-build/shards}
P=${4:-shard}
mkdir -p "$OUT"
rm -f "$OUT"/"$P"_*.gba
for ((i = 0; i < N; i++)); do
    (
        e="$OUT/${P}_$i.elf"
        cp pokeemerald-test.elf "$e"
        if [ -n "$FILE" ]; then
            tools/patchelf/patchelf "$e" gTestRunnerArgv "$FILE\\0"
        fi
        tools/patchelf/patchelf "$e" gTestRunnerHeadless '\x01' gTestRunnerSkipIsFail '\x00' \
            gTestRunnerN "$(printf '\\x%02x' "$N")" gTestRunnerI "$(printf '\\x%02x' "$i")"
        arm-none-eabi-objcopy -O binary "$e" "$OUT/${P}_$i.gba"
        rm -f "$e"
    ) &
done
wait
echo "SHARDS-OK $N"
