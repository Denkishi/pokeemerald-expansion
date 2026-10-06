#ifndef GUARD_CONFIG_CUSTOM_H
#define GUARD_CONFIG_CUSTOM_H

// =====================================================================
// Config custom (Vincenzo). Tutto qui: accendi / spegni.
// =====================================================================

// --- 1. Riavvio dopo trainer Enzo -------------------------------------
// Dopo una VITTORIA contro questo trainer il gioco fa soft reset.
// Metti la costante giusta del tuo trainer (vedi include/constants/opponents.h
// o src/data/trainers.party). 0 = feature spenta.
#define CUSTOM_RESET_AFTER_TRAINER          0   // era TRAINER_ENZO: dopo la vittoria il gioco si riavviava
#define CUSTOM_RESET_ALSO_ON_LOSS           FALSE   // TRUE = riavvia anche se perdi

// --- 2. Gimmick: solo Mega --------------------------------------------
#define CUSTOM_DISABLE_TERA                 TRUE
#define CUSTOM_DISABLE_DYNAMAX              TRUE    // Dynamax + Gigantamax
#define CUSTOM_DISABLE_Z_MOVES              TRUE    // "solo Mega": spegne anche Z-Mosse. FALSE per tenerle.

// --- 3. Indicatore efficacia in lotta ---------------------------------
// Badge x4 / x2 / x1 / 1/2 / 1/4 / X sopra ogni Pokémon bersaglio
// mentre scegli la mossa e mentre scegli il bersaglio.
#define CUSTOM_EFFECTIVENESS_BADGES         TRUE
#define CUSTOM_EFFECTIVENESS_SHOW_NEUTRAL   TRUE    // FALSE = niente badge "x1"

// --- 4. Tasto info: pagina campo --------------------------------------
// Premi B_MOVE_DESCRIPTION_BUTTON (L) una volta: info mossa.
// Premi ancora: info campo (meteo, terreno, Distorsione, schermi...).
// Premi ancora: chiudi.
#define CUSTOM_FIELD_INFO_PAGE              FALSE   // spenta: le stesse informazioni sono in Info lotta (R)

// --- 5/6. Team preview + scelta 4 -------------------------------------
#define CUSTOM_TEAM_PREVIEW                 TRUE
#define CUSTOM_TEAM_PREVIEW_PICKS           4       // quanti Pokémon si portano
#define CUSTOM_TEAM_PREVIEW_MIN_PARTY       2       // sotto questo numero di Pokémon usabili: niente preview
#define CUSTOM_FLAG_NO_TEAM_PREVIEW         0       // se != 0 e il flag è settato: niente preview (es. lotte storia)
#define CUSTOM_TEAM_PREVIEW_REVEAL_AI       FALSE   // TRUE = mostra i numeri scelti dall'AI (debug)

// --- 7. Sconfitta: dove si riparte -------------------------------------
// Se perdi una lotta torni sempre a questo punto di cura (dentro centrogamb,
// davanti all'infermiera). HEAL_LOCATION_NONE = comportamento normale
// (ultimo Centro Pokémon visitato). Lista in src/data/heal_locations.json.
#define CUSTOM_WHITEOUT_HEAL_LOCATION       HEAL_LOCATION_GAMB_ESTERNO

// --- 8. Corsa automatica -----------------------------------------------
// TRUE = il giocatore corre sempre (anche senza Scarpe da Corsa); tieni premuto B per camminare.
#define CUSTOM_AUTO_RUN                     TRUE

// --- 9. Velocità dei menu ----------------------------------------------
// Cambio pagina nel sommario: pixel per fotogramma (originale 32; deve dividere 256).
#define CUSTOM_SUMMARY_SCROLL_SPEED         64
// Cambio tasca nella borsa: passi dell'animazione per fotogramma (originale 1).
#define CUSTOM_BAG_POCKET_SWITCH_SPEED      2
// Tasto tenuto premuto nelle liste: fotogrammi prima di ripetere / tra una ripetizione e l'altra
// (originale 40 e 5).
#define CUSTOM_KEY_REPEAT_START_DELAY       20
#define CUSTOM_KEY_REPEAT_CONTINUE_DELAY    3

// --- 10. Info lotta ------------------------------------------------------
// Tasto R nel menu azioni o nel menu mosse: schermata con squadre, modifiche alle
// statistiche, effetti di campo e dettagli rivelati dell'avversario.
#define CUSTOM_BATTLE_INFO_SCREEN           TRUE

#endif // GUARD_CONFIG_CUSTOM_H
