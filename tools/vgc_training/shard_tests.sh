#!/bin/bash
# Divide pokeemerald-test.elf in N ROM headless (gTestRunnerN/I), da eseguire in parallelo su Windows
# con test_veloci.ps1. Uso (WSL, dalla radice del progetto): tools/vgc_training/shard_tests.sh N
set -e
N=${1:-16}
OUT=build/shards
mkdir -p "$OUT"
rm -f "$OUT"/shard_*.gba
for ((i = 0; i < N; i++)); do
    (
        e="$OUT/shard_$i.elf"
        cp pokeemerald-test.elf "$e"
        tools/patchelf/patchelf "$e" gTestRunnerHeadless '\x01' gTestRunnerSkipIsFail '\x00' \
            gTestRunnerN "$(printf '\\x%02x' "$N")" gTestRunnerI "$(printf '\\x%02x' "$i")"
        arm-none-eabi-objcopy -O binary "$e" "$OUT/shard_$i.gba"
        rm -f "$e"
    ) &
done
wait
echo "SHARDS-OK $N"
