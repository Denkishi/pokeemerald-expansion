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

// Ciò che il giocatore sa di un Pokémon in campo. Per i nostri è il dato vero; per gli
// avversari solo se è stato rivelato (altrimenti ABILITY_NONE / HOLD_EFFECT_NONE).
enum Ability BattleInfo_KnownAbility(enum BattlerId battler);
enum HoldEffect BattleInfo_KnownHoldEffect(enum BattlerId battler);

// Efficacia peggiore (x100: 0, 25, 50, 100, 200, 400) che `mon` subirebbe dalle mosse
// offensive già rivelate degli avversari in campo. -1 se non ne sono state rivelate.
s32 BattleInfo_WorstIncomingEffectiveness(struct Pokemon *mon);

// Nomi degli effetti di campo attivi (NULL se non c'è meteo / terreno).
const u8 *BattleInfo_GetWeatherName(void);
const u8 *BattleInfo_GetTerrainName(void);
const u8 *BattleInfo_GetRoomName(u32 fieldStatus);

// Registro: va chiamata con ogni messaggio mostrato nel riquadro di lotta.
void BattleInfo_LogMessage(const u8 *str);

// Schermata. Al termine torna alla lotta con ReshowBattleScreenAfterMenu.
void CB2_BattleInfo(void);

#endif // GUARD_BATTLE_INFO_H
