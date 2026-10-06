#ifndef GUARD_BATTLE_HUD_H
#define GUARD_BATTLE_HUD_H

// HUD sempre visibile in lotta (modifiche statistiche accanto ai riquadri PS,
// meteo/terreno/stanze in alto a destra). Vedi src/battle_hud.c.

// A inizio lotta.
void BattleHud_Reset(void);

// Crea gli sprite mancanti; chiamata a ogni apertura del menu azioni.
void BattleHud_Ensure(void);

#endif // GUARD_BATTLE_HUD_H
