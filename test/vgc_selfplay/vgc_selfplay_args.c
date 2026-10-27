#include "global.h"
#include "test/vgc_selfplay.h"

// Parametri della partita di self-play VGC. L'orchestratore (tools/vgc_training/selfplay.py) cerca
// `magic` nel .gba e riscrive i campi che seguono prima di lanciare l'emulatore. In una TU a parte
// così il compilatore non può incorporarne i valori nel codice.
// Valore predefinito: VSP_MODE_LIST (stampa solo l'elenco), così `make check` non gioca partite.
const struct VgcSelfPlayArgs gVgcSelfPlayArgs =
{
    .magic = VGC_SELFPLAY_MAGIC,
    .mode = VSP_MODE_LIST,
    .playerIdx = 0,
    .enemyIdx = 1,
    .maxTurns = 0,
    .seed = 1,
    .playerOrder = {0xFF, 0xFF, 0xFF, 0xFF},
    .enemyOrder = {0xFF, 0xFF, 0xFF, 0xFF},
};
