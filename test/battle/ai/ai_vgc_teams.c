#include "global.h"
#include "test/battle.h"
#include "battle_ai_util.h"

// Test di scenario con le squadre VERE dei 12 allenatori VGC (src/data/trainers.party): specie, mosse,
// abilità, strumenti, natura ed EV come nel file, a Lv50. L'IA comanda il lato "opponent" (l'allenatore
// sotto esame); il lato "player" è un'altra delle 12 squadre, così le posizioni sono realistiche.
// Una posizione = la giocata chiave della linea di gioco dell'allenatore (docs/vgc_trainers.md,
// src/data/vgc_gameplans.h). Dove ci sono più giocate buone si usa NOT_EXPECT_MOVE (escludere l'errore
// chiaro) o EXPECT_MOVES (una tra le giocate buone); EXPECT_MOVE solo quando la giocata è una sola.
// I test KNOWN_FAILING sono la lista dei bug del pianificatore: il commento dice cosa sbaglia l'IA e cosa
// mostra il log (righe VGCPLAN). Riepilogo: docs/ia_vgc/scenari.md.
//
// Nota: nei test l'allenatore non ha un TRAINER_* (niente piano di squadra da vgc_gameplans.h): le
// decisioni vengono dalle regole per ruolo, dal grafo della squadra e dal pianificatore.

#include "ai_vgc_sets.h" // flag degli allenatori, VgcSpread e set delle 12 squadre

// =========================================================================================================
// SIMONE — bilanciato (Arcanine Intimidate, Rotom-Mow scarf) / Trick Room (Toedscruel) per Machamp e Aggron
// =========================================================================================================

AI_DOUBLE_BATTLE_TEST("VGC teams: Simone - Rotom-Mow does not click Leaf Storm into Pelipper + Ludicolo")
{
    // Contro la pioggia di Carletto: Leaf Storm è 1/4 su Ludicolo e neutra su Pelipper (e costa -2 AtkSp con la
    // Stolascelta bloccata). Thunderbolt / Volt Switch sono 4x su Pelipper: Leaf Storm è l'errore chiaro.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_PELIPPER) { CARLETTO_PELIPPER; }
        PLAYER(SPECIES_LUDICOLO) { CARLETTO_LUDICOLO; }
        PLAYER(SPECIES_KINGAMBIT) { CARLETTO_KINGAMBIT; }
        PLAYER(SPECIES_AMOONGUSS) { CARLETTO_AMOONGUSS; }
        OPPONENT(SPECIES_ROTOM_MOW) { SIMONE_ROTOM_MOW; }
        OPPONENT(SPECIES_ARCANINE) { SIMONE_ARCANINE; }
        OPPONENT(SPECIES_MACHAMP) { SIMONE_MACHAMP; }
        OPPONENT(SPECIES_AGGRON) { SIMONE_AGGRON; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_SCALD, target: opponentLeft); NOT_EXPECT_MOVE(opponentLeft, MOVE_LEAF_STORM); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Simone - Toedscruel sets Trick Room or redirects for Machamp against Enzo's fast lead")
{
    // Annihilape (Stolascelta) e Mega Aerodactyl sono molto più veloci di Machamp (base 55): la modalità di
    // riserva del piano è Trick Room, e Rage Powder protegge Machamp. Earth Power al turno 1 butta il setter.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_ANNIHILAPE) { ENZO_ANNIHILAPE; }
        PLAYER(SPECIES_AERODACTYL) { ENZO_AERODACTYL; }
        PLAYER(SPECIES_RILLABOOM) { ENZO_RILLABOOM; }
        PLAYER(SPECIES_GENGAR) { ENZO_GENGAR; }
        OPPONENT(SPECIES_TOEDSCRUEL) { SIMONE_TOEDSCRUEL; }
        OPPONENT(SPECIES_MACHAMP) { SIMONE_MACHAMP; }
        OPPONENT(SPECIES_ARCANINE) { SIMONE_ARCANINE; }
        OPPONENT(SPECIES_AGGRON) { SIMONE_AGGRON; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_ICE_PUNCH, target: opponentLeft); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVES(opponentLeft, MOVE_TRICK_ROOM, MOVE_RAGE_POWDER); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Simone - Arcanine finishes a weakened Roaring Moon with Extreme Speed")
{
    // Roaring Moon (Tony) a pochi PS è più veloce di Arcanine e lo minaccia: Extreme Speed (+2) lo manda KO
    // prima che si muova. Play Rough farebbe più danno ma arriva dopo il suo attacco.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_ROARING_MOON) { TONY_ROARING_MOON; HP(30); }
        PLAYER(SPECIES_CHARIZARD) { TONY_CHARIZARD; }
        PLAYER(SPECIES_SABLEYE) { TONY_SABLEYE; }
        PLAYER(SPECIES_GYARADOS) { TONY_GYARADOS; }
        OPPONENT(SPECIES_ARCANINE) { SIMONE_ARCANINE; }
        OPPONENT(SPECIES_ROTOM_MOW) { SIMONE_ROTOM_MOW; }
        OPPONENT(SPECIES_MACHAMP) { SIMONE_MACHAMP; }
        OPPONENT(SPECIES_AGGRON) { SIMONE_AGGRON; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVE(opponentLeft, MOVE_EXTREME_SPEED, target: playerLeft); }
    }
}

// =========================================================================================================
// ELIA — Trick Room (Mimikyu, Mega Banette) per Goodra-H e Lickitung; Vaporeon / Miltank supporto
// =========================================================================================================

AI_DOUBLE_BATTLE_TEST("VGC teams: Elia - Mimikyu sets Trick Room next to Goodra-H against Tony's fast lead")
{
    // Roaring Moon e Charizard sono tra i più veloci del formato; Goodra-H (base 45) e tutta la squadra di Elia
    // sono lenti. Disguise assorbe un colpo: è il setter ideale. Trick Room è la giocata del piano.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_ROARING_MOON) { TONY_ROARING_MOON; }
        PLAYER(SPECIES_CHARIZARD) { TONY_CHARIZARD; }
        PLAYER(SPECIES_SABLEYE) { TONY_SABLEYE; }
        PLAYER(SPECIES_GYARADOS) { TONY_GYARADOS; }
        OPPONENT(SPECIES_MIMIKYU) { ELIA_MIMIKYU; }
        OPPONENT(SPECIES_GOODRA_HISUI) { ELIA_GOODRA_H; }
        OPPONENT(SPECIES_LICKITUNG) { ELIA_LICKITUNG; }
        OPPONENT(SPECIES_MILTANK) { ELIA_MILTANK; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVE(opponentLeft, MOVE_TRICK_ROOM); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Elia - Banette Mega Evolves and sets Trick Room against Patatina")
{
    KNOWN_FAILING; // Protect invece di Trick Room (con Mega) al turno 1
    // Ceruledge e Armarouge (Patatina) sono veloci, la squadra di Elia è lenta. Mega Banette ha Prankster:
    // la Mega va fatta subito (statistiche e abilità servono dal turno 1) e la stanza va alzata.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_CERULEDGE) { PATATINA_CERULEDGE; }
        PLAYER(SPECIES_ARMAROUGE) { PATATINA_ARMAROUGE; }
        PLAYER(SPECIES_INDEEDEE_F) { PATATINA_INDEEDEE; }
        PLAYER(SPECIES_ALTARIA) { PATATINA_ALTARIA; }
        OPPONENT(SPECIES_BANETTE) { ELIA_BANETTE; }
        OPPONENT(SPECIES_VAPOREON) { ELIA_VAPOREON; }
        OPPONENT(SPECIES_GOODRA_HISUI) { ELIA_GOODRA_H; }
        OPPONENT(SPECIES_LICKITUNG) { ELIA_LICKITUNG; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVE(opponentLeft, MOVE_TRICK_ROOM, gimmick: GIMMICK_MEGA); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Elia - Miltank does not use Helping Hand while Mimikyu sets Trick Room")
{
    // Helping Hand non potenzia Trick Room: con il compagno che usa una mossa di stato è un turno buttato.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_ROARING_MOON) { TONY_ROARING_MOON; }
        PLAYER(SPECIES_CHARIZARD) { TONY_CHARIZARD; }
        PLAYER(SPECIES_SABLEYE) { TONY_SABLEYE; }
        PLAYER(SPECIES_GYARADOS) { TONY_GYARADOS; }
        OPPONENT(SPECIES_MIMIKYU) { ELIA_MIMIKYU; }
        OPPONENT(SPECIES_MILTANK) { ELIA_MILTANK; }
        OPPONENT(SPECIES_GOODRA_HISUI) { ELIA_GOODRA_H; }
        OPPONENT(SPECIES_LICKITUNG) { ELIA_LICKITUNG; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); NOT_EXPECT_MOVE(opponentRight, MOVE_HELPING_HAND); }
    }
}

// =========================================================================================================
// PATATINA (RAFEL) — Campo Psichico (Indeedee-F + Armarouge) / Tailwind (Mega Altaria) per Ceruledge
// =========================================================================================================

AI_DOUBLE_BATTLE_TEST("VGC teams: Patatina - Armarouge uses Expanding Force on Psychic Terrain")
{
    // Con il Campo Psichico di Indeedee, Expanding Force colpisce entrambi a 1,5x: superefficace su Machamp e
    // buon danno su Arcanine (Intimidate non tocca un attaccante speciale). È il motivo della lead.
    // Setup: il giocatore porta solo i due in campo, così un suo KO non chiede un cambio non previsto dal TURN.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_MACHAMP) { SIMONE_MACHAMP; }
        PLAYER(SPECIES_ARCANINE) { SIMONE_ARCANINE; }
        OPPONENT(SPECIES_INDEEDEE_F) { PATATINA_INDEEDEE; }
        OPPONENT(SPECIES_ARMAROUGE) { PATATINA_ARMAROUGE; }
        OPPONENT(SPECIES_CERULEDGE) { PATATINA_CERULEDGE; }
        OPPONENT(SPECIES_ALTARIA) { PATATINA_ALTARIA; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_KNOCK_OFF, target: opponentLeft); MOVE(playerRight, MOVE_SNARL); EXPECT_MOVE(opponentRight, MOVE_EXPANDING_FORCE); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Patatina - Altaria sets Tailwind for Ceruledge against Tony's fast lead")
{
    KNOWN_FAILING; // Dragon Pulse invece di Tailwind contro Roaring Moon + Charizard
    // Roaring Moon (171) e Charizard (167) superano Ceruledge (150): con Tailwind Ceruledge e Armarouge
    // tornano davanti per quattro turni. È la modalità di riserva del piano (Altaria + Ceruledge).
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_ROARING_MOON) { TONY_ROARING_MOON; }
        PLAYER(SPECIES_CHARIZARD) { TONY_CHARIZARD; }
        PLAYER(SPECIES_SABLEYE) { TONY_SABLEYE; }
        PLAYER(SPECIES_GYARADOS) { TONY_GYARADOS; }
        OPPONENT(SPECIES_ALTARIA) { PATATINA_ALTARIA; }
        OPPONENT(SPECIES_CERULEDGE) { PATATINA_CERULEDGE; }
        OPPONENT(SPECIES_ARMAROUGE) { PATATINA_ARMAROUGE; }
        OPPONENT(SPECIES_INDEEDEE_F) { PATATINA_INDEEDEE; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVE(opponentLeft, MOVE_TAILWIND); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Patatina - Ceruledge finishes a weakened Gengar with Shadow Sneak")
{
    // Gengar (Enzo) a pochi PS è più veloce di Ceruledge: Shadow Sneak (priorità, superefficace) lo manda KO
    // prima che usi Hypnosis o Shadow Ball. Bitter Blade arriverebbe dopo.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_GENGAR) { ENZO_GENGAR; HP(30); }
        PLAYER(SPECIES_RILLABOOM) { ENZO_RILLABOOM; }
        PLAYER(SPECIES_SMEARGLE) { ENZO_SMEARGLE; }
        PLAYER(SPECIES_ANNIHILAPE) { ENZO_ANNIHILAPE; }
        OPPONENT(SPECIES_CERULEDGE) { PATATINA_CERULEDGE; }
        OPPONENT(SPECIES_ARMAROUGE) { PATATINA_ARMAROUGE; }
        OPPONENT(SPECIES_INDEEDEE_F) { PATATINA_INDEEDEE; }
        OPPONENT(SPECIES_ALTARIA) { PATATINA_ALTARIA; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_GRASSY_GLIDE, target: opponentRight); EXPECT_MOVE(opponentLeft, MOVE_SHADOW_SNEAK, target: playerLeft); SEND_OUT(playerRight, 2); }
    }
}

// =========================================================================================================
// TONY — sole (Mega Charizard Y, Protosynthesis) / Tailwind (Roaring Moon); Sableye Prankster supporto
// =========================================================================================================

AI_DOUBLE_BATTLE_TEST("VGC teams: Tony - Sableye stops Porygon2's Trick Room with Fake Out or Taunt")
{
    // Gerozz in modalità Trick Room (Porygon2 + Great Tusk): sotto la stanza la squadra veloce di Tony perde.
    // Sableye ha Fake Out e Taunt con Prankster: una delle due sul setter è la giocata. Sunny Day è inutile
    // (il sole arriva con la Mega di Charizard), Icy Wind non ferma la stanza.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_PORYGON2) { GEROZZ_PORYGON2; }
        PLAYER(SPECIES_GREAT_TUSK) { GEROZZ_GREAT_TUSK; }
        PLAYER(SPECIES_TYRANITAR) { GEROZZ_TYRANITAR; }
        PLAYER(SPECIES_GHOLDENGO) { GEROZZ_GHOLDENGO; }
        OPPONENT(SPECIES_SABLEYE) { TONY_SABLEYE; }
        OPPONENT(SPECIES_CHARIZARD) { TONY_CHARIZARD; }
        OPPONENT(SPECIES_ROARING_MOON) { TONY_ROARING_MOON; }
        OPPONENT(SPECIES_SLITHER_WING) { TONY_SLITHER_WING; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_RECOVER); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVES(opponentLeft, MOVE_FAKE_OUT, MOVE_TAUNT); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Tony - Charizard Mega Evolves on turn 1 and Heat Waves two Grass types")
{
    // Venusaur e Lilligant (Gigi Pinto) sono entrambi deboli al Fuoco: Mega Charizard Y porta il sole e Heat
    // Wave li colpisce tutti e due. Rimandare la Mega non ha senso (Siccità e AtkSp servono subito).
    // Setup: il giocatore porta solo i due in campo, così un suo KO non chiede un cambio non previsto dal TURN.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_VENUSAUR) { GIGI_VENUSAUR; }
        PLAYER(SPECIES_LILLIGANT) { GIGI_LILLIGANT; }
        OPPONENT(SPECIES_CHARIZARD) { TONY_CHARIZARD; }
        OPPONENT(SPECIES_ROARING_MOON) { TONY_ROARING_MOON; }
        OPPONENT(SPECIES_SABLEYE) { TONY_SABLEYE; }
        OPPONENT(SPECIES_GYARADOS) { TONY_GYARADOS; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_ENERGY_BALL, target: opponentRight); EXPECT_MOVE(opponentLeft, MOVE_HEAT_WAVE, gimmick: GIMMICK_MEGA); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Tony - Slither Wing does not use Close Combat into Gengar + Rillaboom")
{
    // Gengar è immune al Lotta, Rillaboom lo prende neutro; Flare Blitz (al sole della Mega Y) è 2x su
    // Rillaboom e High Horsepower 2x su Gengar. Close Combat è sempre la scelta peggiore.
    // Squadra intera del giocatore: il contesto (Aerodactyl Tailwind) serve, e il KO di Rillaboom fa entrare il terzo
    // (SEND_OUT sotto). Il merge di ia-vgc-contesto aveva tenuto il SEND_OUT ma tolto il terzo Pokémon.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_GENGAR) { ENZO_GENGAR; }
        PLAYER(SPECIES_RILLABOOM) { ENZO_RILLABOOM; }
        PLAYER(SPECIES_SMEARGLE) { ENZO_SMEARGLE; }
        PLAYER(SPECIES_AERODACTYL) { ENZO_AERODACTYL; }
        OPPONENT(SPECIES_SLITHER_WING) { TONY_SLITHER_WING; }
        OPPONENT(SPECIES_CHARIZARD) { TONY_CHARIZARD; }
        OPPONENT(SPECIES_ROARING_MOON) { TONY_ROARING_MOON; }
        OPPONENT(SPECIES_SABLEYE) { TONY_SABLEYE; }
    } WHEN {
        // Contesto della squadra (Aerodactyl Tailwind + attaccanti veloci): Rillaboom stimato con la spread veloce (4 PS, come il vero)
        // va KO con Weather Ball al sole di Charizard: entra il terzo.
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_GRASSY_GLIDE, target: opponentRight); NOT_EXPECT_MOVE(opponentLeft, MOVE_CLOSE_COMBAT); SEND_OUT(playerRight, 2); }
    }
}

// =========================================================================================================
// MARIO LETT — Dondozo + Tatsugiri (Commander); Sinistcha Trick Room, Chatot Tailwind; Weezing-G + Slaking
// =========================================================================================================

AI_DOUBLE_BATTLE_TEST("VGC teams: Mario Lett - commanded Dondozo uses Earthquake into Arcanine + Aggron")
{
    // Tatsugiri entra in Dondozo (Commander: +2 a tutto, Tatsugiri non può essere colpito). Earthquake non tocca
    // il compagno nascosto ed è 2x su Arcanine e 4x su Mega Aggron: è il colpo che sfrutta la lead.
    // Setup: il giocatore porta solo i due in campo, così un suo KO non chiede un cambio non previsto dal TURN.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_ARCANINE) { SIMONE_ARCANINE; }
        PLAYER(SPECIES_AGGRON) { SIMONE_AGGRON; }
        OPPONENT(SPECIES_DONDOZO) { LETT_DONDOZO; }
        OPPONENT(SPECIES_TATSUGIRI) { LETT_TATSUGIRI; }
        OPPONENT(SPECIES_SINISTCHA) { LETT_SINISTCHA; }
        OPPONENT(SPECIES_CHATOT) { LETT_CHATOT; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_SNARL); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVE(opponentLeft, MOVE_EARTHQUAKE); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Mario Lett - Sinistcha sets Trick Room next to Dondozo against Enzo")
{
    // Mega Aerodactyl e Annihilape scarf contro Dondozo (base 35): Trick Room (modalità del piano con Sinistcha)
    // ribalta l'ordine per cinque turni.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_AERODACTYL) { ENZO_AERODACTYL; }
        PLAYER(SPECIES_ANNIHILAPE) { ENZO_ANNIHILAPE; }
        PLAYER(SPECIES_RILLABOOM) { ENZO_RILLABOOM; }
        PLAYER(SPECIES_GENGAR) { ENZO_GENGAR; }
        OPPONENT(SPECIES_SINISTCHA) { LETT_SINISTCHA; }
        OPPONENT(SPECIES_DONDOZO) { LETT_DONDOZO; }
        OPPONENT(SPECIES_CHATOT) { LETT_CHATOT; }
        OPPONENT(SPECIES_WEEZING_GALAR) { LETT_WEEZING_G; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_ICE_PUNCH, target: opponentRight); EXPECT_MOVE(opponentLeft, MOVE_TRICK_ROOM); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Mario Lett - Slaking keeps attacking under Neutralizing Gas")
{
    // Weezing-Galar spegne Truant: Slaking può attaccare ogni turno. Contro avversari che non lo minacciano
    // (qui non attaccano) Protect è un turno buttato, al primo come al secondo turno.
    // Setup: il giocatore porta solo i due in campo, così un suo KO non chiede un cambio non previsto dal TURN.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_POLITOED) { MARIO_POLITOED; }
        PLAYER(SPECIES_GOLDUCK) { MARIO_GOLDUCK; }
        OPPONENT(SPECIES_SLAKING) { LETT_SLAKING; }
        OPPONENT(SPECIES_WEEZING_GALAR) { LETT_WEEZING_G; }
        OPPONENT(SPECIES_DONDOZO) { LETT_DONDOZO; }
        OPPONENT(SPECIES_SINISTCHA) { LETT_SINISTCHA; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); NOT_EXPECT_MOVE(opponentLeft, MOVE_PROTECT); }
        TURN { MOVE(playerLeft, MOVE_SCALD, target: opponentRight); MOVE(playerRight, MOVE_ICE_BEAM, target: opponentRight); NOT_EXPECT_MOVE(opponentLeft, MOVE_PROTECT); }
    }
}

// =========================================================================================================
// TATORE — Trick Room (Dusclops, Scream Tail) per Armaldo / Lapras; Kecleon Fake Out; Mega Sceptile
// =========================================================================================================

AI_DOUBLE_BATTLE_TEST("VGC teams: Tatore - Kecleon does not Taunt a Magic Bounce Hatterene")
{
    // Hatterene (Gigi Pinto) ha Magic Bounce (l'abilità che porta in VGC): Taunt rimbalza su Kecleon.
    // Fake Out su Hatterene o su Torkoal è la giocata; Taunt è l'errore chiaro.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_HATTERENE) { GIGI_HATTERENE; }
        PLAYER(SPECIES_TORKOAL) { GIGI_TORKOAL; }
        PLAYER(SPECIES_VENUSAUR) { GIGI_VENUSAUR; }
        PLAYER(SPECIES_GARGANACL) { GIGI_GARGANACL; }
        OPPONENT(SPECIES_KECLEON) { TATORE_KECLEON; }
        OPPONENT(SPECIES_SCEPTILE) { TATORE_SCEPTILE; }
        OPPONENT(SPECIES_DUSCLOPS) { TATORE_DUSCLOPS; }
        OPPONENT(SPECIES_ARMALDO) { TATORE_ARMALDO; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); NOT_EXPECT_MOVE(opponentLeft, MOVE_TAUNT); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Tatore - Dusclops sets Trick Room next to Lapras against Patatina")
{
    // Ceruledge e Armarouge sono veloci, Dusclops / Lapras / Armaldo lenti: Trick Room è la lead del piano.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_CERULEDGE) { PATATINA_CERULEDGE; }
        PLAYER(SPECIES_ARMAROUGE) { PATATINA_ARMAROUGE; }
        PLAYER(SPECIES_INDEEDEE_F) { PATATINA_INDEEDEE; }
        PLAYER(SPECIES_ALTARIA) { PATATINA_ALTARIA; }
        OPPONENT(SPECIES_DUSCLOPS) { TATORE_DUSCLOPS; }
        OPPONENT(SPECIES_LAPRAS) { TATORE_LAPRAS; }
        OPPONENT(SPECIES_ARMALDO) { TATORE_ARMALDO; }
        OPPONENT(SPECIES_SCREAM_TAIL) { TATORE_SCREAM_TAIL; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVE(opponentLeft, MOVE_TRICK_ROOM); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Tatore - Scream Tail Encores Espathra after a Calm Mind")
{
    KNOWN_FAILING; // sceglie Encore ma con bersaglio registrato opponentLeft invece di Espathra: verificare il bersaglio
    // Savarotto prepara Espathra (Calm Mind + Speed Boost) per passare i boost a Chandelure. Dopo un Calm Mind,
    // Encore la blocca sulla mossa di setup per tre turni: niente Baton Pass. È la risposta da manuale.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_ESPATHRA) { SAVA_ESPATHRA; }
        PLAYER(SPECIES_INCINEROAR) { SAVA_INCINEROAR; }
        PLAYER(SPECIES_CHANDELURE) { SAVA_CHANDELURE; }
        PLAYER(SPECIES_GALLADE) { SAVA_GALLADE; }
        OPPONENT(SPECIES_SCREAM_TAIL) { TATORE_SCREAM_TAIL; }
        OPPONENT(SPECIES_ARMALDO) { TATORE_ARMALDO; }
        OPPONENT(SPECIES_DUSCLOPS) { TATORE_DUSCLOPS; }
        OPPONENT(SPECIES_LAPRAS) { TATORE_LAPRAS; }
    } WHEN {
        TURN { MOVE(playerRight, MOVE_FLARE_BLITZ, target: opponentRight); MOVE(playerLeft, MOVE_CALM_MIND); }
        TURN { MOVE(playerRight, MOVE_FLARE_BLITZ, target: opponentRight); MOVE(playerLeft, MOVE_CALM_MIND); EXPECT_MOVE(opponentLeft, MOVE_ENCORE, target: playerLeft); }
    }
}

// =========================================================================================================
// MARIO — bilanciato (Hariyama Fake Out + Iron Hands) / Trick Room (Oranguru); pioggia (Politoed + Golduck)
// =========================================================================================================

AI_DOUBLE_BATTLE_TEST("VGC teams: Mario - Hariyama uses Fake Out on Whimsicott")
{
    KNOWN_FAILING; // Heavy Slam invece di Fake Out su Whimsicott (Prankster Tailwind / Beat Up)
    // Whimsicott (Savarotto) con Prankster alza Tailwind o usa Beat Up su Gallade (Justified): entrambe le cose
    // rovinano Hariyama e Iron Hands, lenti. Fake Out (+3) arriva prima della Prankster (+1).
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_WHIMSICOTT) { SAVA_WHIMSICOTT; }
        PLAYER(SPECIES_GALLADE) { SAVA_GALLADE; }
        PLAYER(SPECIES_INCINEROAR) { SAVA_INCINEROAR; }
        PLAYER(SPECIES_CHANDELURE) { SAVA_CHANDELURE; }
        OPPONENT(SPECIES_HARIYAMA) { MARIO_HARIYAMA; }
        OPPONENT(SPECIES_IRON_HANDS) { MARIO_IRON_HANDS; }
        OPPONENT(SPECIES_ORANGURU) { MARIO_ORANGURU; }
        OPPONENT(SPECIES_POLITOED) { MARIO_POLITOED; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_MOONBLAST, target: opponentRight); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVE(opponentLeft, MOVE_FAKE_OUT, target: playerLeft); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Mario - Oranguru sets Trick Room next to Iron Hands against Enzo")
{
    // Oranguru ha Inner Focus (Fake Out di Rillaboom non lo blocca): Trick Room per Iron Hands (base 50) contro
    // Mega Aerodactyl e Rillaboom è la lead del piano in modalità stanza.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_RILLABOOM) { ENZO_RILLABOOM; }
        PLAYER(SPECIES_AERODACTYL) { ENZO_AERODACTYL; }
        PLAYER(SPECIES_ANNIHILAPE) { ENZO_ANNIHILAPE; }
        PLAYER(SPECIES_GENGAR) { ENZO_GENGAR; }
        OPPONENT(SPECIES_ORANGURU) { MARIO_ORANGURU; }
        OPPONENT(SPECIES_IRON_HANDS) { MARIO_IRON_HANDS; }
        OPPONENT(SPECIES_HARIYAMA) { MARIO_HARIYAMA; }
        OPPONENT(SPECIES_MEGANIUM) { MARIO_MEGANIUM; }
    } WHEN {
        // Contesto della squadra (squadra Tailwind): Rillaboom stimato con la spread veloce (4 PS, come il vero) va KO con
        // Close Combat di Iron Hands: entra il terzo.
        TURN { MOVE(playerLeft, MOVE_GRASSY_GLIDE, target: opponentRight); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVE(opponentLeft, MOVE_TRICK_ROOM); SEND_OUT(playerLeft, 2); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Mario - rain Golduck uses Hydro Pump on the Fire types")
{
    // Pioggia di Politoed: Golduck (Swift Swim) è il più veloce in campo e Hydro Pump (x1,5 pioggia) è
    // superefficace su Ceruledge e Armarouge. Ice Beam / Psyshock fanno molto meno.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_CERULEDGE) { PATATINA_CERULEDGE; }
        PLAYER(SPECIES_ARMAROUGE) { PATATINA_ARMAROUGE; }
        PLAYER(SPECIES_INDEEDEE_F) { PATATINA_INDEEDEE; }
        PLAYER(SPECIES_ROTOM_WASH) { PATATINA_ROTOM_WASH; }
        OPPONENT(SPECIES_POLITOED) { MARIO_POLITOED; }
        OPPONENT(SPECIES_GOLDUCK) { MARIO_GOLDUCK; }
        OPPONENT(SPECIES_HARIYAMA) { MARIO_HARIYAMA; }
        OPPONENT(SPECIES_IRON_HANDS) { MARIO_IRON_HANDS; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVE(opponentRight, MOVE_HYDRO_PUMP); }
    }
}

// =========================================================================================================
// ENZO — Tailwind (Mega Aerodactyl) / Gravità e sonno (Smeargle); Grimmsnarl schermi
// =========================================================================================================

AI_DOUBLE_BATTLE_TEST("VGC teams: Enzo - Aerodactyl sets Tailwind for Rillaboom against Tony")
{
    KNOWN_FAILING; // Rock Slide invece di Tailwind per Rillaboom contro Tony
    // Roaring Moon e Charizard superano Rillaboom (150): Tailwind (lead del piano Aerodactyl + Rillaboom) lo
    // porta davanti a tutti per quattro turni.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_ROARING_MOON) { TONY_ROARING_MOON; }
        PLAYER(SPECIES_CHARIZARD) { TONY_CHARIZARD; }
        PLAYER(SPECIES_SABLEYE) { TONY_SABLEYE; }
        PLAYER(SPECIES_GYARADOS) { TONY_GYARADOS; }
        OPPONENT(SPECIES_AERODACTYL) { ENZO_AERODACTYL; }
        OPPONENT(SPECIES_RILLABOOM) { ENZO_RILLABOOM; }
        OPPONENT(SPECIES_GENGAR) { ENZO_GENGAR; }
        OPPONENT(SPECIES_ANNIHILAPE) { ENZO_ANNIHILAPE; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVE(opponentLeft, MOVE_TAILWIND); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Enzo - Smeargle does not Spore two Grass types")
{
    // Venusaur e Lilligant (Gigi Pinto) sono d'Erba: immuni a Spore. Fake Out / Follow Me sono le giocate.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_VENUSAUR) { GIGI_VENUSAUR; }
        PLAYER(SPECIES_LILLIGANT) { GIGI_LILLIGANT; }
        PLAYER(SPECIES_TORKOAL) { GIGI_TORKOAL; }
        PLAYER(SPECIES_HATTERENE) { GIGI_HATTERENE; }
        OPPONENT(SPECIES_SMEARGLE) { ENZO_SMEARGLE; }
        OPPONENT(SPECIES_ANNIHILAPE) { ENZO_ANNIHILAPE; }
        OPPONENT(SPECIES_GENGAR) { ENZO_GENGAR; }
        OPPONENT(SPECIES_RILLABOOM) { ENZO_RILLABOOM; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_ENERGY_BALL, target: opponentRight); NOT_EXPECT_MOVE(opponentLeft, MOVE_SPORE); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Enzo - Grimmsnarl sets up a screen on turn 1")
{
    KNOWN_FAILING; // Spirit Break invece di Reflect / Light Screen con Prankster al turno 1
    // Roaring Moon (fisico) e Charizard (speciale): con Prankster e Light Clay uno schermo al turno 1 è la
    // ragione d'essere di Grimmsnarl. Taunt non ha bersagli utili (nessun setter di velocità o stanza in campo
    // tranne il Tailwind di Roaring Moon, che Grimmsnarl non può comunque fermare prima).
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_ROARING_MOON) { TONY_ROARING_MOON; }
        PLAYER(SPECIES_CHARIZARD) { TONY_CHARIZARD; }
        PLAYER(SPECIES_SABLEYE) { TONY_SABLEYE; }
        PLAYER(SPECIES_GYARADOS) { TONY_GYARADOS; }
        OPPONENT(SPECIES_GRIMMSNARL) { ENZO_GRIMMSNARL; }
        OPPONENT(SPECIES_SMEARGLE) { ENZO_SMEARGLE; }
        OPPONENT(SPECIES_AERODACTYL) { ENZO_AERODACTYL; }
        OPPONENT(SPECIES_GENGAR) { ENZO_GENGAR; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVES(opponentLeft, MOVE_REFLECT, MOVE_LIGHT_SCREEN); }
    }
}

// =========================================================================================================
// GEROZZ — sabbia (Mega Tyranitar, Dugtrio-A Sand Force) / Trick Room (Porygon2); Gholdengo, Great Tusk
// =========================================================================================================

AI_DOUBLE_BATTLE_TEST("VGC teams: Gerozz - Dugtrio-Alola uses Earthquake next to Levitate Rotom-Heat")
{
    // La Velocità di Arcanine non si conosce (stima 161 contro i 160 di Dugtrio): il ramo con la Velocità
    // alternativa (CUSTOM_VGC_PLAN_SPEED_ALT_PCT) rende Earthquake di nuovo la scelta migliore.
    // Rotom-Heat è immune (Levitate): Earthquake è 2x su Arcanine e 4x su Aggron senza costi per il compagno.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_ARCANINE) { SIMONE_ARCANINE; }
        PLAYER(SPECIES_AGGRON) { SIMONE_AGGRON; }
        PLAYER(SPECIES_MACHAMP) { SIMONE_MACHAMP; }
        PLAYER(SPECIES_ROTOM_MOW) { SIMONE_ROTOM_MOW; }
        OPPONENT(SPECIES_DUGTRIO_ALOLA) { GEROZZ_DUGTRIO_A; }
        OPPONENT(SPECIES_ROTOM_HEAT) { GEROZZ_ROTOM_HEAT; }
        OPPONENT(SPECIES_TYRANITAR) { GEROZZ_TYRANITAR; }
        OPPONENT(SPECIES_GHOLDENGO) { GEROZZ_GHOLDENGO; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_SNARL); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVE(opponentLeft, MOVE_EARTHQUAKE); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Gerozz - Porygon2 sets Trick Room next to Great Tusk against Enzo")
{
    // Great Tusk manda KO Annihilape con Headlong Rush: il giocatore manda dentro il terzo.
    // Mega Aerodactyl e Annihilape scarf superano Great Tusk (107): la modalità di riserva del piano
    // (Porygon2 + Great Tusk) è Trick Room.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_AERODACTYL) { ENZO_AERODACTYL; }
        PLAYER(SPECIES_ANNIHILAPE) { ENZO_ANNIHILAPE; }
        PLAYER(SPECIES_RILLABOOM) { ENZO_RILLABOOM; }
        PLAYER(SPECIES_GENGAR) { ENZO_GENGAR; }
        OPPONENT(SPECIES_PORYGON2) { GEROZZ_PORYGON2; }
        OPPONENT(SPECIES_GREAT_TUSK) { GEROZZ_GREAT_TUSK; }
        OPPONENT(SPECIES_TYRANITAR) { GEROZZ_TYRANITAR; }
        OPPONENT(SPECIES_GHOLDENGO) { GEROZZ_GHOLDENGO; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_ICE_PUNCH, target: opponentRight); EXPECT_MOVE(opponentLeft, MOVE_TRICK_ROOM); SEND_OUT(playerRight, 2); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Gerozz - Gholdengo uses Make It Rain into two Fairy types")
{
    // Hatterene e Grimmsnarl sono entrambi deboli all'Acciaio: Make It Rain (ad area, STAB, Life Orb) li colpisce
    // tutti e due superefficace. Good as Gold rende Gholdengo immune alle Prankster di Grimmsnarl.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_HATTERENE) { GIGI_HATTERENE; }
        PLAYER(SPECIES_GRIMMSNARL) { ENZO_GRIMMSNARL; }
        PLAYER(SPECIES_TORKOAL) { GIGI_TORKOAL; }
        PLAYER(SPECIES_VENUSAUR) { GIGI_VENUSAUR; }
        OPPONENT(SPECIES_GHOLDENGO) { GEROZZ_GHOLDENGO; }
        OPPONENT(SPECIES_ROTOM_HEAT) { GEROZZ_ROTOM_HEAT; }
        OPPONENT(SPECIES_TYRANITAR) { GEROZZ_TYRANITAR; }
        OPPONENT(SPECIES_GREAT_TUSK) { GEROZZ_GREAT_TUSK; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_LIGHT_SCREEN); EXPECT_MOVE(opponentLeft, MOVE_MAKE_IT_RAIN); }
    }
}

// =========================================================================================================
// CARLETTO — pioggia (Pelipper + Ludicolo) / Trick Room (Mega Malamar); Amoonguss + Snorlax Belly Drum
// =========================================================================================================

AI_DOUBLE_BATTLE_TEST("VGC teams: Carletto - Amoonguss Rage Powder covers Snorlax's Belly Drum")
{
    // Iron Hands e Meganium (Mario) non hanno Fake Out: con Rage Powder che attira i colpi di Iron Hands
    // (Amoonguss resiste al Lotta), Snorlax fa Belly Drum (Sitrus + Recycle con Gluttony recuperano i PS).
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_IRON_HANDS) { MARIO_IRON_HANDS; }
        PLAYER(SPECIES_MEGANIUM) { MARIO_MEGANIUM; }
        PLAYER(SPECIES_POLITOED) { MARIO_POLITOED; }
        PLAYER(SPECIES_GOLDUCK) { MARIO_GOLDUCK; }
        OPPONENT(SPECIES_AMOONGUSS) { CARLETTO_AMOONGUSS; }
        OPPONENT(SPECIES_SNORLAX) { CARLETTO_SNORLAX; }
        OPPONENT(SPECIES_KINGAMBIT) { CARLETTO_KINGAMBIT; }
        OPPONENT(SPECIES_PELIPPER) { CARLETTO_PELIPPER; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_DETECT); MOVE(playerRight, MOVE_SYNTHESIS); EXPECT_MOVE(opponentLeft, MOVE_RAGE_POWDER); EXPECT_MOVE(opponentRight, MOVE_BELLY_DRUM); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Carletto - Ludicolo uses Fake Out on turn 1 of the rain lead")
{
    // Lead del piano Pelipper + Ludicolo contro Tony: Fake Out al turno 1 (su Charizard prima di Heat Wave, o
    // su Sableye prima di Sunny Day) mentre Pelipper attacca. Non usarlo è sprecare la mossa.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_CHARIZARD) { TONY_CHARIZARD; }
        PLAYER(SPECIES_SABLEYE) { TONY_SABLEYE; }
        PLAYER(SPECIES_ROARING_MOON) { TONY_ROARING_MOON; }
        PLAYER(SPECIES_GYARADOS) { TONY_GYARADOS; }
        OPPONENT(SPECIES_PELIPPER) { CARLETTO_PELIPPER; }
        OPPONENT(SPECIES_LUDICOLO) { CARLETTO_LUDICOLO; }
        OPPONENT(SPECIES_KINGAMBIT) { CARLETTO_KINGAMBIT; }
        OPPONENT(SPECIES_AMOONGUSS) { CARLETTO_AMOONGUSS; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_SUNNY_DAY); EXPECT_MOVE(opponentRight, MOVE_FAKE_OUT); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Carletto - Malamar sets Trick Room next to Kingambit against Tony")
{
    // Kingambit (base 50) e Snorlax sono lenti contro Roaring Moon e Charizard: Trick Room di Malamar è la
    // modalità di riserva del piano (Malamar + Amoonguss / Kingambit).
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_ROARING_MOON) { TONY_ROARING_MOON; }
        PLAYER(SPECIES_CHARIZARD) { TONY_CHARIZARD; }
        PLAYER(SPECIES_SABLEYE) { TONY_SABLEYE; }
        PLAYER(SPECIES_GYARADOS) { TONY_GYARADOS; }
        OPPONENT(SPECIES_MALAMAR) { CARLETTO_MALAMAR; }
        OPPONENT(SPECIES_KINGAMBIT) { CARLETTO_KINGAMBIT; }
        OPPONENT(SPECIES_SNORLAX) { CARLETTO_SNORLAX; }
        OPPONENT(SPECIES_AMOONGUSS) { CARLETTO_AMOONGUSS; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVE(opponentLeft, MOVE_TRICK_ROOM); }
    }
}

// =========================================================================================================
// GIGI PINTO — sole (Torkoal + Venusaur / Lilligant Chlorophyll) / Trick Room (Hatterene); Mega Blastoise
// =========================================================================================================

AI_DOUBLE_BATTLE_TEST("VGC teams: Gigi Pinto - Torkoal uses Eruption at full HP in the sun")
{
    // Sole di Drought, PS pieni: Eruption (150, ad area, Charcoal) è 2x su Gholdengo e neutra su Great Tusk.
    // È la mossa che giustifica Torkoal in lead.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_GHOLDENGO) { GEROZZ_GHOLDENGO; }
        PLAYER(SPECIES_GREAT_TUSK) { GEROZZ_GREAT_TUSK; }
        PLAYER(SPECIES_PORYGON2) { GEROZZ_PORYGON2; }
        PLAYER(SPECIES_ROTOM_HEAT) { GEROZZ_ROTOM_HEAT; }
        OPPONENT(SPECIES_TORKOAL) { GIGI_TORKOAL; }
        OPPONENT(SPECIES_VENUSAUR) { GIGI_VENUSAUR; }
        OPPONENT(SPECIES_HATTERENE) { GIGI_HATTERENE; }
        OPPONENT(SPECIES_GARGANACL) { GIGI_GARGANACL; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVE(opponentLeft, MOVE_ERUPTION); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Gigi Pinto - Hatterene sets Trick Room next to Torkoal against Tony")
{
    KNOWN_FAILING; // Protect di Hatterene + Eruption (-165) batte Trick Room + Protect di Torkoal (-236): con le mosse note
                   // lette dallo slot giusto Hatterene va KO mentre mette Trick Room. Scelta discutibile, non un errore netto.
    // Roaring Moon e Charizard sono veloci; Torkoal (base 20) e Hatterene (29) sono i più lenti del formato:
    // Trick Room (lead di riserva del piano) li fa muovere per primi.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_ROARING_MOON) { TONY_ROARING_MOON; }
        PLAYER(SPECIES_CHARIZARD) { TONY_CHARIZARD; }
        PLAYER(SPECIES_SABLEYE) { TONY_SABLEYE; }
        PLAYER(SPECIES_GYARADOS) { TONY_GYARADOS; }
        OPPONENT(SPECIES_HATTERENE) { GIGI_HATTERENE; }
        OPPONENT(SPECIES_TORKOAL) { GIGI_TORKOAL; }
        OPPONENT(SPECIES_VENUSAUR) { GIGI_VENUSAUR; }
        OPPONENT(SPECIES_GARGANACL) { GIGI_GARGANACL; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVE(opponentLeft, MOVE_TRICK_ROOM); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Gigi Pinto - Lilligant helps Torkoal move first or puts Great Tusk to sleep")
{
    KNOWN_FAILING; // Energy Ball invece di After You su Torkoal / Sleep Powder su Great Tusk
    // Great Tusk è più veloce di Torkoal e lo manda KO con Headlong Rush: After You su Torkoal fa partire
    // Eruption a PS pieni prima di lui; Sleep Powder su Great Tusk lo ferma. Energy Ball / Encore no.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_GREAT_TUSK) { GEROZZ_GREAT_TUSK; }
        PLAYER(SPECIES_GHOLDENGO) { GEROZZ_GHOLDENGO; }
        PLAYER(SPECIES_PORYGON2) { GEROZZ_PORYGON2; }
        PLAYER(SPECIES_ROTOM_HEAT) { GEROZZ_ROTOM_HEAT; }
        OPPONENT(SPECIES_LILLIGANT) { GIGI_LILLIGANT; }
        OPPONENT(SPECIES_TORKOAL) { GIGI_TORKOAL; }
        OPPONENT(SPECIES_VENUSAUR) { GIGI_VENUSAUR; }
        OPPONENT(SPECIES_HATTERENE) { GIGI_HATTERENE; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVES(opponentLeft, MOVE_AFTER_YOU, MOVE_SLEEP_POWDER); }
    }
}

// =========================================================================================================
// SAVAROTTO — staffetta (Espathra -> Mega Chandelure), Fake Out (Incineroar / Sneasler), Beat Up su Gallade
// =========================================================================================================

AI_DOUBLE_BATTLE_TEST("VGC teams: Savarotto - Whimsicott uses Beat Up on Justified Gallade")
{
    // Kingambit e Snorlax (Carletto) prendono Drain Punch superefficace (Kingambit 4x): Beat Up su Gallade
    // (Justified) lo porta a +2 prima che attacchi. È la combo per cui i due stanno insieme.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_KINGAMBIT) { CARLETTO_KINGAMBIT; }
        PLAYER(SPECIES_SNORLAX) { CARLETTO_SNORLAX; }
        PLAYER(SPECIES_AMOONGUSS) { CARLETTO_AMOONGUSS; }
        PLAYER(SPECIES_PELIPPER) { CARLETTO_PELIPPER; }
        OPPONENT(SPECIES_WHIMSICOTT) { SAVA_WHIMSICOTT; }
        OPPONENT(SPECIES_GALLADE) { SAVA_GALLADE; }
        OPPONENT(SPECIES_INCINEROAR) { SAVA_INCINEROAR; }
        OPPONENT(SPECIES_CHANDELURE) { SAVA_CHANDELURE; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_RECYCLE); EXPECT_MOVE(opponentLeft, MOVE_BEAT_UP, target: opponentRight); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Savarotto - Incineroar uses Fake Out to cover Espathra on turn 1")
{
    // Lead Incineroar + Espathra contro il sole di Gigi Pinto: Fake Out compra il turno di setup a Espathra.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_TORKOAL) { GIGI_TORKOAL; }
        PLAYER(SPECIES_VENUSAUR) { GIGI_VENUSAUR; }
        PLAYER(SPECIES_LILLIGANT) { GIGI_LILLIGANT; }
        PLAYER(SPECIES_HATTERENE) { GIGI_HATTERENE; }
        OPPONENT(SPECIES_INCINEROAR) { SAVA_INCINEROAR; }
        OPPONENT(SPECIES_ESPATHRA) { SAVA_ESPATHRA; }
        OPPONENT(SPECIES_GALLADE) { SAVA_GALLADE; }
        OPPONENT(SPECIES_CHANDELURE) { SAVA_CHANDELURE; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVE(opponentLeft, MOVE_FAKE_OUT); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC teams: Savarotto - Chandelure Mega Evolves and Heat Waves two Grass types")
{
    // Venusaur e Lilligant sono deboli al Fuoco e il sole di Torkoal non c'è (Torkoal in panchina):
    // Heat Wave colpisce entrambi superefficace; la Mega va fatta subito.
    // Setup: il giocatore porta solo i due in campo, così un suo KO non chiede un cambio non previsto dal TURN.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_VENUSAUR) { GIGI_VENUSAUR; }
        PLAYER(SPECIES_LILLIGANT) { GIGI_LILLIGANT; }
        OPPONENT(SPECIES_CHANDELURE) { SAVA_CHANDELURE; }
        OPPONENT(SPECIES_GALLADE) { SAVA_GALLADE; }
        OPPONENT(SPECIES_INCINEROAR) { SAVA_INCINEROAR; }
        OPPONENT(SPECIES_WHIMSICOTT) { SAVA_WHIMSICOTT; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_ENERGY_BALL, target: opponentRight); EXPECT_MOVE(opponentLeft, MOVE_HEAT_WAVE, gimmick: GIMMICK_MEGA); }
    }
}
