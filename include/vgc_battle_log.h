#ifndef GUARD_VGC_BATTLE_LOG_H
#define GUARD_VGC_BATTLE_LOG_H

// Logger delle lotte VGC (CUSTOM_VGC_BATTLE_LOG): righe "VGCLOG ..." nel log di mGBA durante le lotte contro gli
// allenatori con AI_FLAG_VGC_DOUBLES, per analizzare dopo le scelte dell'IA (tools/vgc_training/analizza_log.py).
// Formato delle righe e istruzioni: docs/ia_vgc/logger.md. Ogni funzione esce subito se il logger è spento
// (configurazione, lotta non VGC, nessun registro di debug di mGBA: console vera).

struct Pokemon;

// Team preview (src/team_preview.c)
void VgcLog_TeamPreviewBegin(struct Pokemon *enemy, struct Pokemon *player);
void VgcLog_TeamPreviewGroup(u32 mask, const u32 *enemySlots, u32 nEnemy, s32 score);
void VgcLog_TeamPreviewAi(struct Pokemon *enemy, struct Pokemon *player, const u8 *outOrder, u32 order,
                          u32 nCand, const s32 *candS, const s32 *candWorst, const u32 *candA, const u32 *candB,
                          const u32 *chosen, const u32 *enemySlots, u32 nPred, const u32 *predJ, const u32 *predK,
                          const s32 *predW, const u32 *playerSlots, u16 megaSpecies);
void VgcLog_TeamPreviewPlayer(u32 game, u32 bestOf, struct Pokemon *fullParty, const u8 *order, u32 picks);

// Lotta (src/battle_main.c, src/battle_controllers.c, src/battle_interface.c)
void VgcLog_TurnStart(void);
void VgcLog_ActionsChosen(void);
void VgcLog_Message(const u8 *string);
void VgcLog_HP(u32 battler, s32 oldHP, s32 newHP, s32 maxHP);
void VgcLog_AbilityPopUp(u32 battler, u32 ability);
void VgcLog_BattleEnd(void);

// Pianificatore (src/battle_ai_vgc_plan.c): coppie simulate e risposte previste degli avversari.
void VgcLog_PlanHead(u32 me, u32 nPairs, u32 bestIdx, u32 rulesIdx, u32 frames, u32 sims);
void VgcLog_PlanPair(u32 me, u32 idx, u32 kindA, u32 moveA, u32 userA, u32 targetA, s32 partyA,
                     u32 kindB, u32 moveB, u32 userB, u32 targetB, s32 partyB, s32 value, u32 mark);
void VgcLog_PlanFoe(u32 me, u32 slot, u32 idx, u32 kind, u32 move, u32 user, u32 target, s32 party, s32 weight);

#endif // GUARD_VGC_BATTLE_LOG_H
