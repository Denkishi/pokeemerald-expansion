#ifndef GUARD_BATTLE_AI_VGC_H
#define GUARD_BATTLE_AI_VGC_H

// IA per le lotte in doppio stile VGC (AI_FLAG_VGC_DOUBLES).
// Punteggio del motore per ogni mossa (= azione delle regole): solo il filtro e la spinta qui sotto (M8); la scelta
// la fa il pianificatore.
s32 AI_VgcDoubles(enum BattlerId battlerAtk, enum BattlerId battlerDef, enum Move move, s32 score);

// Filtro delle candidate (M8): perché un'azione (mossa `moveIndex` di `battler` su `target`) è illegale o certamente
// inutile. Lo usano allo stesso modo il pianificatore (candidate e azione delle regole) e il punteggio del motore.
enum VgcFilter
{
    VGC_FILTER_NONE,              // azione buona da valutare
    VGC_FILTER_ILLEGAL,           // senza PP, bloccata (Choice, Taunt, Disable, Encore, Assault Vest...)
    VGC_FILTER_FIRST_TURN_ONLY,   // Fake Out & co. fuori dal primo turno in campo
    VGC_FILTER_PROTECT_REPEAT,    // Protect & co. dopo un Protect riuscito al turno prima
    VGC_FILTER_PSYCHIC_TERRAIN,   // priorità (anche da Prankster / Gale Wings) su un avversario a terra nel Campo Psichico
    VGC_FILTER_IMMUNE,            // attacco su un'immunità nota (o su avversari tutti immuni, per le mosse ad area)
    VGC_FILTER_STATUS_BLOCKED,    // mossa di stato che fallisce sul bersaglio (polvere, Prankster sul Buio, tipo, stato...)
    VGC_FILTER_FIELD_ACTIVE,      // Tailwind/Gravity/meteo/terreno/schermi già attivi, Trick Room che chiude il nostro
    VGC_FILTER_NO_EFFECT,         // Recycle senza niente, Baton Pass senza panchina, Substitute/Belly Drum senza PS...
    VGC_FILTER_WRONG_TARGET,      // attacco o stato sul compagno senza combo, Instruct / After You su un avversario
};
enum VgcFilter AI_VgcFilterAction(enum BattlerId battler, u32 moveIndex, enum BattlerId target);
// Spinta leggera (M8), in punti della valutazione del pianificatore, per gli effetti che la simulazione non modella
// (Gravity, Quick Guard, Instruct, After You, meteo per la panchina, Contrary, Neutralizing Gas). partnerMove:
// la mossa del compagno nella stessa coppia (MOVE_NONE se non si sa, MOVE_UNAVAILABLE se non attacca).
s32 AI_VgcActionPrior(enum BattlerId battler, enum Move move, enum BattlerId target, enum Move partnerMove);
// Self-play (verifica-regole): il lato di `battler` usa le regole di prima di M8 (solo nel ROM dei test).
#if TESTING
bool32 AI_VgcRulesLegacy(enum BattlerId battler);
#else
#define AI_VgcRulesLegacy(battler) FALSE
#endif

// Vero se la squadra dell'AI (campo + panchina) ha almeno `minCount` Pokémon vivi più lenti
// di entrambi gli avversari in campo (= Trick Room le conviene).
bool32 AI_VgcTeamIsSlowerThanFoes(enum BattlerId battler, u32 minCount);

// Vero se la squadra dell'AI ha almeno `minCount` Pokémon vivi che con Tailwind supererebbero
// l'avversario più veloce ma senza no.
bool32 AI_VgcTeamBenefitsFromTailwind(enum BattlerId battler, u32 minCount);

// Vero se il controllo di velocità attivo (Tailwind nostro, oppure Trick Room quando siamo il lato lento)
// ha ancora più di un turno: in questi turni non si spreca tempo.
bool32 AI_VgcOwnSpeedControlActive(enum BattlerId battler);

// Piano di squadra per allenatore (src/data/vgc_gameplans.h).
struct VgcGameplan;
const struct VgcGameplan *AI_VgcGetPlan(u16 trainerId);
bool32 AI_VgcPlanIsWinCon(u16 trainerId, u16 species);
bool32 AI_VgcPlanIsTrickRoomTeam(u16 trainerId);
// Vantaggio numerico (Pokémon utilizzabili IA - giocatore).
s32 AI_VgcMaterialEdge(enum BattlerId battler);
// Probabilità stimata (0..100) che un Pokémon del giocatore usi Protect questo turno (senza imbrogli).
u32 AI_VgcProtectChance(enum BattlerId foe);
u32 AI_VgcSwitchChance(enum BattlerId foe);

// Pianificatore del turno (src/battle_ai_vgc_plan.c), a pezzi: Begin dopo i punteggi delle regole,
// Step ogni fotogramma finché non ritorna TRUE, Apply per scrivere la scelta (mossa/bersaglio o cambio).
bool32 AI_VgcPlanBegin(enum BattlerId me);
bool32 AI_VgcPlanStep(void);
bool32 AI_VgcPlanApply(void);
bool32 AI_VgcPlanIsActive(void);
enum BattlerId AI_VgcPlanBattler(void);
// TRUE se il pianificatore è occupato (o ha un risultato da applicare) per un altro battler in questo turno:
// il partner aspetta, perché la ricerca sceglie la coppia e la sua metà arriva già decisa.
bool32 AI_VgcPlanIsBusy(enum BattlerId battler);
// A inizio turno: azzera lavori e decisioni rimasti dal turno (o dalla lotta) precedente.
void AI_VgcPlanResetTurn(void);
// Pesi della valutazione del pianificatore per lato, in % dei default (M9, taratura nel self-play).
void AI_VgcSetEvalWeightPct(u32 side, const u8 pct[7]);
void AI_VgcResetEvalWeights(void);
#if TESTING
// Test: miglior valore simulato, nella prima decisione del turno 0, di una coppia in cui `battler` usa `move`.
s32 AI_VgcPlanTestValue(enum BattlerId battler, enum Move move);
// Test: spinta delle regole (M8) data a una candidata di `battler` con `move` nella prima decisione del turno 0.
s32 AI_VgcPlanTestPrior(enum BattlerId battler, enum Move move);
#endif
// Chi entra quando `battler` usa Baton Pass: il ricevente designato dal grafo della squadra (PARTY_SIZE = nessuno).
u32 AI_VgcBatonRecipient(enum BattlerId battler);
// Freno ai cambi (src/battle_ai_switch.c): chi è appena entrato con un cambio volontario resta un turno.
bool32 AI_VgcJustSwitchedIn(enum BattlerId battler);
void AI_VgcNoteSwitchIn(enum BattlerId battler);
// Chi mandare in campo dopo un KO: il compagno che fa coppia migliore con chi è rimasto (grafo della squadra).
u32 AI_VgcReplacementAfterKo(enum BattlerId battler, u32 defaultIdx);
bool32 AI_VgcPlanIsSetter(u16 trainerId, u16 species);
bool32 AI_VgcPlanIsBack(u16 trainerId, u16 species);
s32 AI_VgcPlanLeadBonus(u16 trainerId, u16 a, u16 b, bool32 trViable, bool32 twViable);
s32 AI_VgcPlanBringBonus(u16 trainerId, u16 species, bool32 trViable, bool32 twViable);
bool32 AI_VgcPlanModeActive(enum BattlerId battler);
// Stessa specie ignorando Mega e forme (Indeedee-F == Indeedee, Rotom-Mow == Rotom...).
bool32 AI_VgcSameSpecies(u16 planSpecies, u16 species);
// TRUE = questo Pokémon non deve megaevolvere: la Mega è riservata alla win condition del piano.
bool32 AI_VgcShouldHoldMega(enum BattlerId battler);

// Lettura del giocatore senza imbrogli: memoria di comportamento (Protect sotto KO, cambi quando
// perde il matchup), per Pokémon, mantenuta tra le gare del Bo3.
void AI_VgcResetMemory(void);                                   // nuova serie / nuovo avversario
s32 AI_VgcFoeTargetBias(enum BattlerId atk, enum BattlerId t);         // storia dei bersagli del giocatore in questa lotta
void AI_VgcTurnStart(void);                                     // dopo SetAiLogicDataForTurn
void AI_VgcNotePlayerMove(enum BattlerId battler, enum Move move); // quando il giocatore esegue una mossa
void AI_VgcNotePlayerSwitch(enum BattlerId battler);            // quando il giocatore cambia

#endif // GUARD_BATTLE_AI_VGC_H
