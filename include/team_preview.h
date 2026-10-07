#ifndef GUARD_TEAM_PREVIEW_H
#define GUARD_TEAM_PREVIEW_H

// TRUE se questa lotta deve avere la team preview (chiamare dopo la transizione).
bool32 TeamPreview_ShouldRun(void);

// Entra nella schermata. Alla fine va da sola in CB2_InitBattle.
void CB2_TeamPreview(void);

// Da chiamare a fine lotta allenatore: rimette la squadra del giocatore com'era
// (con i dati aggiornati dei 4 che hanno lottato). Sicura da chiamare sempre.
void TeamPreview_RestorePlayerParty(void);

// AI: sceglie i suoi Pokémon e li mette in ordine (lead prima) dentro gEnemyParty.
// Usata dalla preview; esposta per test/debug.
void TeamPreview_AiSelectEnemyTeam(u32 picks, bool32 isDouble, u8 *outOrder);

// Squadra avversaria vista in anteprima, per la schermata Info lotta.
bool32 TeamPreview_HasEnemyPreview(void);
bool32 TeamPreview_GetEnemyPreviewMon(u32 index, u16 *species, u32 *battleSlot);

// --- Al meglio di tre ---------------------------------------------------
// Da chiamare a fine lotta, dopo TeamPreview_RestorePlayerParty. TRUE = la serie continua
// ed è già stata avviata la gara successiva (il chiamante deve solo uscire).
bool32 TeamPreview_SeriesHandleBattleEnd(void);
// Salva ciò che l'AI ha scoperto sui nostri Pokémon; va chiamata prima di liberare i dati dell'AI.
void TeamPreview_SeriesCaptureAiKnowledge(void);
// Rimette nell'AI ciò che aveva scoperto nelle gare precedenti della serie.
void TeamPreview_SeriesInjectAiKnowledge(void);

// special per gli script: VAR_0x8004 = 1 (Bo1) o 3 (Bo3), prima di trainerbattle.
void TeamPreview_SetFormatFromScript(void);

#endif // GUARD_TEAM_PREVIEW_H
