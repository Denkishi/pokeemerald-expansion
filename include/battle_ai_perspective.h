#ifndef GUARD_BATTLE_AI_PERSPECTIVE_H
#define GUARD_BATTLE_AI_PERSPECTIVE_H

// Prospettiva dell'IA nelle lotte IA contro IA (B_FLAG_AI_VS_AI_BATTLE / IsAiVsAiBattle()).
//
// Nella lotta normale l'IA è sempre il lato avversario: conosce i propri Pokémon e del giocatore
// solo ciò che ha visto. Con IA contro IA entrambi i lati sono comandati dall'IA e, senza questo
// modulo, ognuno vedrebbe tutto dell'altro (BattlerHasAi è vero per tutti = informazione completa).
// Qui si tiene "chi sta pensando": ogni lato vede tutto di sé e dell'altro solo ciò che ha osservato,
// con i propri dati dell'IA (gAiLogicData separati per lato: abilità/strumenti presunti, danni stimati).
//
// Fuori da IA contro IA tutte le funzioni si comportano come prima (prospettiva = lato avversario).

// Da chiamare subito dopo / subito prima dell'allocazione di gAiLogicData (battle_util2.c).
void AI_PerspectiveAlloc(void);
void AI_PerspectiveFree(void);

// TRUE solo in IA contro IA con i dati per lato pronti.
bool32 AI_PerspectiveActive(void);
// Imposta chi pensa: il lato di `battler` (e i suoi dati dell'IA). Nessun effetto fuori da IA contro IA.
void AI_SetPerspective(enum BattlerId battler);
void AI_SetPerspectiveSide(u32 side);
u32 AI_GetPerspectiveSide(void);

// `battler` è un avversario di chi pensa (= il "giocatore" per il codice scritto per l'IA avversaria).
bool32 AI_IsFoeOfPerspective(enum BattlerId battler);
// Chi pensa conosce davvero `battler` (mosse, abilità, strumento): è del suo lato.
bool32 AI_PerspectiveKnowsBattler(enum BattlerId battler);

// Allenatore che comanda `battler` (piano di squadra, win condition): l'avversario o, in IA contro IA,
// l'allenatore del lato del giocatore (gPartnerTrainerId).
u16 AI_TrainerIdOfBattler(enum BattlerId battler);

// Mega designata in team preview per il lato di `battler`. Il lato avversario usa quella della team preview
// (TeamPreview_GetMegaDesignateSpecies); il lato del giocatore in IA contro IA quella impostata da chi
// prepara la lotta (harness di self-play), SPECIES_NONE = libera.
u16 AI_GetMegaDesignateForBattler(enum BattlerId battler);
void AI_SetPlayerSideMegaDesignate(u16 species);

// Calcolo dei dati dell'IA di inizio turno: in IA contro IA una volta per lato, ognuno dal proprio punto di vista.
void AI_SetLogicDataForTurnPerSide(void (*compute)(struct AiLogicData *aiData));

#endif // GUARD_BATTLE_AI_PERSPECTIVE_H
