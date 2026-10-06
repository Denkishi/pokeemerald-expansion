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

#endif // GUARD_TEAM_PREVIEW_H
