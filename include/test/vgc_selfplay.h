#ifndef GUARD_TEST_VGC_SELFPLAY_H
#define GUARD_TEST_VGC_SELFPLAY_H

// Harness di self-play VGC (test/vgc_selfplay): partite IA contro IA tra i veri allenatori VGC, con
// controllo dei misplay e log strutturato. Guida: docs/ia_vgc/training.md.

// Parametri della partita: il ROM li contiene in chiaro dopo `magic`; l'orchestratore
// (tools/vgc_training) li riscrive nel .gba prima di lanciarlo. Mantenere in sincronia con
// tools/vgc_training/selfplay.py (ARGS_LAYOUT).
#define VGC_SELFPLAY_MAGIC "VGCSELFPLAYARG1"

enum VgcSelfPlayMode
{
    VSP_MODE_LIST = 0,      // stampa l'elenco degli allenatori VGC ed esce
    VSP_MODE_PREVIEW = 1,   // partita: 4 su 6 e lead scelti dalla team preview dell'IA di ogni lato
    VSP_MODE_FIXED = 2,     // partita con squadre/lead fissate (playerOrder / enemyOrder)
};

struct VgcSelfPlayArgs
{
    char magic[16];
    u8 mode;
    u8 playerIdx;            // indice nell'elenco degli allenatori VGC (lato del giocatore)
    u8 enemyIdx;             // (lato avversario)
    u8 maxTurns;             // 0 = 50
    u32 seed;
    u8 playerOrder[4];       // VSP_MODE_FIXED: slot originali (0..5), lead prima; 0xFF = nessuno
    u8 enemyOrder[4];
    // Posizione preparata (si applica prima delle scelte del primo turno).
    u8 startTurn;            // contatore dei turni iniziale (0 = normale)
    u8 tailwindTurns[2];     // [lato] turni di Tailwind rimasti (0 = niente)
    u8 trickRoomTurns;       // turni di Trick Room rimasti (0 = niente)
    u8 hpPercent[4];         // per battler (0 = PS pieni)
    s8 statStages[4][5];     // per battler: Atk, Def, Spe, SpA, SpD (-6..+6)
    u8 verbose;              // 1 = stampa anche i dati delle mosse a ogni scelta
    u8 reserved[7];          // pesi della valutazione in % / 2 (0 = default), per il lato weightSide
    // Allenamento (M9)
    u8 weightSide;           // lato a cui vanno i pesi di reserved: 0 = G, 1 = A (l'altro resta ai default)
    u8 bookOff;              // bit per lato (1 = G, 2 = A): libro dei piani spento
    u8 explorePct[2];        // per lato: % di team preview esplorative e di scelte esplorative ai turni 1/2
    u8 quiet;                // 1 = niente diagnostica dell'IA né messaggi della lotta nel log (partite più veloci)
    u8 contextOff;           // bit per lato (1 = G, 2 = A): contesto della squadra avversaria spento (verifica)
    u8 contextAnswerPct;     // peso % delle risposte al piano avversario in team preview (0 = 100, taratura)
    u8 pad[1];
};

extern const struct VgcSelfPlayArgs gVgcSelfPlayArgs;

bool32 VgcSelfPlay_IsActive(void);
void VgcSelfPlay_OnTurnStart(void);
void VgcSelfPlay_OnChosenMove(enum BattlerId battler, enum Move move, u32 target, u32 gimmick);
void VgcSelfPlay_OnSwitch(enum BattlerId battler, u32 partyIndex);
void VgcSelfPlay_OnMessage(const u8 *string);
void VgcSelfPlay_OnHP(enum BattlerId battler, u32 oldHP, u32 newHP);
void VgcSelfPlay_OnAbilityPopUp(enum BattlerId battler, enum Ability ability);
void VgcSelfPlay_OnBattleEnd(void);

#endif // GUARD_TEST_VGC_SELFPLAY_H
