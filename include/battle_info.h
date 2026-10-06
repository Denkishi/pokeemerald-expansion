#ifndef GUARD_BATTLE_INFO_H
#define GUARD_BATTLE_INFO_H

#include "global.h"
#include "battle.h"

// Schermata "Info lotta" (tasto R in lotta). Vedi src/battle_info.c.

// TRUE = alla prossima apertura del menu azioni va aperta la schermata.
extern bool8 gBattleInfoRequested;

// Da chiamare a inizio lotta: azzera le informazioni rivelate.
void BattleInfo_ResetBattle(void);

// Agganci per registrare le informazioni rivelate dagli avversari
// (mosse usate, abilità e strumenti attivati).
void BattleInfo_RecordMove(enum BattlerId battler, enum Move move);
void BattleInfo_RecordAbility(enum BattlerId battler, enum Ability ability);
void BattleInfo_RecordItem(enum BattlerId battler);

// Aggiorna le durate massime degli effetti di campo; chiamata a ogni turno.
void BattleInfo_SampleTimers(void);

// Schermata. Al termine torna alla lotta con ReshowBattleScreenAfterMenu.
void CB2_BattleInfo(void);

#endif // GUARD_BATTLE_INFO_H
