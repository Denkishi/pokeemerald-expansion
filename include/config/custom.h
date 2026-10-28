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

// --- 11. Informazioni e comodità stile simulatore ------------------------
#define CUSTOM_OPPONENT_HP_PERCENT          TRUE    // PS avversario in % al posto di "Lv50" sul riquadro
#define CUSTOM_BATTLE_HUD                   FALSE   // spento: etichette statistiche e meteo a schermo (restano in Info lotta)
#define CUSTOM_MOVE_CATEGORY_ICON           TRUE    // icona fisico/speciale/stato accanto al tipo nel menu mosse
#define CUSTOM_DAMAGE_PREVIEW               FALSE   // intervallo di danno stimato in % accanto al badge di efficacia
#define CUSTOM_SWITCH_EFFECTIVENESS         TRUE    // nel menu squadra in lotta: efficacia peggiore subita dalle mosse rivelate
#define CUSTOM_HEAL_AFTER_BATTLE            TRUE    // cura completa dopo ogni lotta vinta contro un allenatore
#define CUSTOM_BATTLE_AUTO_TEXT             TRUE    // i messaggi di lotta avanzano da soli (la sfida iniziale dell'allenatore aspetta A per vedere lo sprite)

// --- 12. Formato della lotta ---------------------------------------------
// Prima di ogni lotta con team preview si sceglie Bo1 o Bo3 (al meglio di tre: stesse squadre,
// si rifà la scelta dei 4 a ogni gara; l'AI ricorda ciò che ha visto nelle gare precedenti).
#define CUSTOM_BEST_OF_THREE                TRUE

// --- 13. Riquadro della mossa e animazioni ---------------------------------
#define CUSTOM_MOVE_INFO_BOX                TRUE    // riquadro in basso a destra: PP e tipo con icona categoria (senza moltiplicatore)
#define CUSTOM_BATTLE_BOUNCE                TRUE    // lotta: respiro dei Pokémon e dell'allenatore, schiacciamento all'arrivo, rinculo al colpo
#define CUSTOM_RUN_BOUNCE                   TRUE    // mappa: la corsa ha un piccolo rimbalzo verticale

// --- 14. IA VGC -------------------------------------------------------------
// Profondità del pianificatore del turno (src/battle_ai_vgc_plan.c): 1 = simula solo il turno
// attuale, 2 = anche il turno dopo (ramificazione: nostre risposte contro le loro). Il 2 costa
// qualche decimo di secondo di "pensiero" dell'IA a ogni turno; se la lotta diventa lenta, metti 1.
#define CUSTOM_VGC_PLAN_DEPTH               3
// Simulazioni per fotogramma mentre pensa in sottofondo (più alto = decide prima ma la lotta può
// scattare; più basso = fluido ma, se scegli in fretta, vedi "penso..." più a lungo).
#define CUSTOM_VGC_PLAN_SIMS_PER_FRAME      24
// Tempo massimo per fotogramma del pianificatore, in righe di schermo (un fotogramma = 228 righe).
// Il pianificatore si ferma appena lo supera e riprende al fotogramma dopo: il gioco non scatta.
// Più alto = l'IA decide prima; più basso = più margine per animazioni e menu.
#define CUSTOM_VGC_PLAN_SCANLINES_PER_FRAME 100
// Pesi della valutazione della posizione del pianificatore (Evaluate in src/battle_ai_vgc_plan.c), in punti
// (100 = PS pieni di un Pokémon). Ognuno vale con il segno opposto per il giocatore.
#define CUSTOM_VGC_EVAL_KO                  160 // Pokémon in campo andato KO
#define CUSTOM_VGC_EVAL_ALIVE               40  // Pokémon in campo vivo (in più della percentuale di PS)
#define CUSTOM_VGC_EVAL_SLEEP               60  // addormentato
#define CUSTOM_VGC_EVAL_BURN                35  // scottato (attaccante fisico)
#define CUSTOM_VGC_EVAL_PARALYSIS           20  // paralizzato
#define CUSTOM_VGC_EVAL_SPEED_PAIR          18  // per coppia (nostro, loro) e per turno: chi si muove prima
#define CUSTOM_VGC_EVAL_SPEED_TURNS         4   // turni futuri contati per l'ordine di velocità (Trick Room/Tailwind rimasti)
#define CUSTOM_VGC_EVAL_THREAT_FAST         45  // minaccia di KO al turno dopo, da chi si muove prima
#define CUSTOM_VGC_EVAL_THREAT_SLOW         20  // minaccia di KO al turno dopo, da chi si muove dopo
#define CUSTOM_VGC_EVAL_MATERIAL            120 // differenza di Pokémon utilizzabili (campo + panchina), sul totale
#define CUSTOM_VGC_EVAL_FAKE_OUT            25  // Fake Out pronto per il turno dopo (appena entrato)
#define CUSTOM_VGC_PLAN_PRUNE               250 // una coppia peggiore della migliore di tanto non si finisce di simulare
#define CUSTOM_VGC_PLAN_MAX_FRAMES          0    // tetto al costo (0 = nessuno): prima la decisione giusta, poi quella veloce
#define CUSTOM_VGC_PLAN_SAFETY_FRAMES       1800 // solo sicurezza contro cicli infiniti (~30 s): oltre si chiude con le coppie finite
#define CUSTOM_VGC_PLAN_REPLACE_KO          FALSE // sostituzioni dopo un KO nella simulazione: provate il 27/10, peggiorano (misplay 445 -> 515, costo x2)
#define CUSTOM_VGC_PLAN_REPAIR_PAIRS        FALSE // seconda passata delle coppie (velocità/setup accanto al compagno della migliore): provata il 27/10, nessun KNOWN_FAILING risolto e Savarotto Beat Up rotto
#define CUSTOM_VGC_PLAN_ROLLOUT_TURNS       0     // turni avidi giocati dopo l'orizzonte sulla linea migliore (0 = solo il terzo turno ridotto). Con 4 (27/10): nessun KNOWN_FAILING risolto, Savarotto Calm Mind e Carletto Fake Out rotti, misplay 356 -> 386
#define CUSTOM_VGC_PLAN_SPEED_ALT_PCT       35    // peso del ramo con la Velocità alternativa di un avversario dalla stima incerta
#define CUSTOM_VGC_PLAN_FOE_SINGLE_SCALE    FALSE // Protect/cambio del giocatore sulla stessa scala delle mosse (con il costo del tempo): provato il 27/10, prevede Protect quasi sempre (peso 100) e rompe 2 test
#define CUSTOM_VGC_SWITCH_COST              100 // costo di un cambio volontario al primo turno in campo (metà dopo)
#define CUSTOM_VGC_EVAL_WINCON_PCT          150 // peso (%) di PS e KO della win condition del piano dell'allenatore
#define CUSTOM_VGC_EVAL_SASH                20  // Focus Sash intatto a PS pieni
#define CUSTOM_VGC_EVAL_BENCH_HP_PCT        100 // peso (%) dei PS portati in panchina da chi esce con un cambio (e lasciati da chi entra)
#define CUSTOM_VGC_EVAL_SPEED_RACE_CAP      200 // tetto (+/-) alla parte "corsa ai KO" dell'ordine di velocità nei turni futuri (0 = nessuno): un Trick Room appena messo valeva ~1100 punti, più di due KO
#define CUSTOM_VGC_MIRROR_FIXES             FALSE // solo self-play: le correzioni dei misplay (PS in panchina, colpo alto, spareggio, tetto alla corsa) spente per il lato col bit di verifica-regole, regole M8 per tutti
#define CUSTOM_VGC_PLAN_SWITCH_RISK         TRUE // cambio volontario: costo del KO di chi entra se una risposta plausibile degli avversari sul suo slot lo manda KO
#define CUSTOM_VGC_MIRROR_FOE_IDLE          FALSE // solo self-play: CUSTOM_VGC_PLAN_FOE_VS_IDLE acceso per il lato senza il bit di verifica-regole, spento per l'altro (regole M8 per tutti)
#define CUSTOM_VGC_PLAN_FOE_HISTORY         FALSE // modello del giocatore: i bersagli che ha scelto finora (il più danneggiato, la nostra win condition) spingono le sue risposte previste. 28/10: campagna 286 -> 258, specchio 1726 partite 50,3% (IC95 [49,1; 51,5], p=0,60): spento
#define CUSTOM_VGC_PLAN_FOE_THREAT_FOCUS    0     // modello del giocatore: spinta agli attacchi sul nostro Pokémon più pericoloso per lui (0 = spento). Con 40 (28/10): rompe il test Mario - Hariyama Fake Out su Whimsicott
#define CUSTOM_VGC_PLAN_FOE_HISTORY_W       60    // spinta massima (punti del modello del giocatore) della storia dei bersagli
#define CUSTOM_VGC_MIRROR_FOE_HISTORY       FALSE // solo self-play: storia dei bersagli accesa per il lato senza il bit di verifica-regole, spenta per l'altro
#define CUSTOM_VGC_PLAN_FOE_VS_IDLE         FALSE // modello del giocatore: le sue mosse valutate anche contro di noi fermi (media). Provato il 28/10: misplay 303 -> 285 ma rompe il test Altaria Tailwind; a specchio 1727 partite 50,7% (IC95 [48,9; 52,5], p=0,47): spento
#define CUSTOM_VGC_PLAN_HIGH_ROLL           TRUE // ramo "colpo alto": un colpo che manda KO solo con un tiro alto (sopra la mediana) pesa per la sua probabilità
// Ritratto dell'allenatore + "penso..." quando hai già scelto e l'IA sta ancora calcolando.
#define CUSTOM_VGC_THINKING_OVERLAY         TRUE
#define CUSTOM_VGC_THINKING_SLIDE_FRAMES    24      // fotogrammi dell'entrata (da destra) e dell'uscita (verso destra) del ritratto
#define CUSTOM_VGC_THINKING_MIN_FRAMES      60      // il ritratto con "sta pensando..." resta almeno questi fotogrammi (60 = 1 s)
#define CUSTOM_VGC_THINKING_X               176     // x del centro del ritratto a fine entrata (tra i Pokemon avversari a 152 e 200)
#define CUSTOM_VGC_THINKING_Y               44      // y del centro del ritratto
#define CUSTOM_VGC_THINKING_START_X         272     // x di partenza e di uscita, fuori schermo a destra (240 + 32)
// L'IA dedicata ai doppi si attiva per allenatore con il flag "Vgc Doubles" nella riga AI di
// src/data/trainers.party (vedi docs/vgc_trainers.md). Niente da configurare qui: il flag è
// AI_FLAG_VGC_DOUBLES e il codice è in src/battle_ai_vgc.c (mosse) e src/battle_ai_switch.c (cambi).
// Nel Bo3 l'IA ricorda anche lead e gruppo portato in ogni gara e chi le ha fatto i KO
// (src/team_preview.c, "history" e "kos"): dopo una sconfitta cambia lead e gruppo.

// --- 15. Salto filmati intro ------------------------------------------------
// Salta i filmati d'apertura e porta direttamente alla schermata del titolo.
// TRUE (1) = Avvio istantaneo direttamente alla Title Screen (Press Start), senza filmati né attese
// 2        = Mostra il copyright (saltabile con A/B/Start), poi passa direttamente alla Title Screen
// FALSE    = Comportamento standard (riproduce l'intro RHH e i filmati Rayquaza)
#define CUSTOM_SKIP_INTRO_CINEMATICS        TRUE

#endif // GUARD_CONFIG_CUSTOM_H

