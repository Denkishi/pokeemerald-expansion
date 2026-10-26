#include "global.h"
#include "test/battle.h"
#include "battle_ai_util.h"

// Suite dell'IA per i doppi VGC (AI_FLAG_VGC_DOUBLES): una posizione = una risposta attesa.
// Piano e lista delle posizioni: docs/ia_vgc/piano_implementazione.md (sezione 4).
// Ogni errore visto in partita va aggiunto qui come test. I test marcati KNOWN_FAILING descrivono
// un comportamento sbagliato già noto: quando la correzione arriva, si toglie il KNOWN_FAILING.

#define VGC_AI_FLAGS (AI_FLAG_BASIC_TRAINER | AI_FLAG_SMART_SWITCHING | AI_FLAG_SMART_MON_CHOICES \
                    | AI_FLAG_KNOW_OPPONENT_PARTY | AI_FLAG_ASSUME_STAB | AI_FLAG_ASSUME_STATUS_MOVES \
                    | AI_FLAG_HP_AWARE | AI_FLAG_WEIGH_ABILITY_PREDICTION | AI_FLAG_VGC_DOUBLES)

// Nel motore un attacco sul bersaglio già andato KO passa da solo sull'altro avversario: l'overkill costa
// quando la mossa scelta per il primo bersaglio è inutile sul secondo (qui Thunderbolt su Garchomp, immune).
AI_DOUBLE_BATTLE_TEST("VGC: no overkill - the second attacker picks the move that works on the other foe")
{
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { HP(1); Speed(10); }
        PLAYER(SPECIES_GARCHOMP) { Speed(10); }
        // Il più veloce ha solo Thunderbolt: l'unico bersaglio utile è il Wobbuffet a 1 PS.
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_THUNDERBOLT); Speed(50); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_THUNDERBOLT, MOVE_ICE_BEAM); Speed(40); }
    } WHEN {
        TURN {
            EXPECT_MOVE(opponentLeft, MOVE_THUNDERBOLT, target: playerLeft);
            EXPECT_MOVE(opponentRight, MOVE_ICE_BEAM, target: playerRight);
        }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: Fake Out is not used after the first turn on the field")
{
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { Speed(10); }
        PLAYER(SPECIES_WOBBUFFET) { Speed(10); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_FAKE_OUT, MOVE_SCRATCH); Speed(50); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_SCRATCH); Speed(40); }
    } WHEN {
        TURN { }
        TURN { NOT_EXPECT_MOVE(opponentLeft, MOVE_FAKE_OUT); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: Simple Beam on a Mycelium Might partner lets Spore go first")
{
    // Avversari più lenti di Toedscruel ma che attaccano forte: addormentarne uno PRIMA che colpisca vale
    // l'azione del compagno. Nessuna regola sulla specie: lo vede la simulazione.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_GARCHOMP) { Speed(60); }
        PLAYER(SPECIES_GARCHOMP) { Speed(60); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_SIMPLE_BEAM, MOVE_SCRATCH); Speed(150); }
        OPPONENT(SPECIES_TOEDSCRUEL) { Ability(ABILITY_MYCELIUM_MIGHT); Moves(MOVE_SPORE, MOVE_EARTH_POWER); Speed(100); }
    } WHEN {
        TURN { EXPECT_MOVE(opponentLeft, MOVE_SIMPLE_BEAM, target: opponentRight); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: Beat Up on a Justified partner before it attacks")
{
    // Beat Up colpisce Gallade una volta per Pokémon della squadra (qui 2): Justified lo porta a +2 e il suo
    // Close Combat nello stesso turno manda KO. Nessuna regola sulle specie: lo vede la simulazione.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { Speed(20); }
        PLAYER(SPECIES_WOBBUFFET) { Speed(20); }
        OPPONENT(SPECIES_WHIMSICOTT) { Moves(MOVE_BEAT_UP, MOVE_MOONBLAST); Speed(116); }
        OPPONENT(SPECIES_GALLADE) { Ability(ABILITY_JUSTIFIED); Moves(MOVE_CLOSE_COMBAT); Speed(80); }
    } WHEN {
        TURN { EXPECT_MOVE(opponentLeft, MOVE_BEAT_UP, target: opponentRight); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: Baton Pass sends in the designated sweeper, not the generic best switch-in")
{
    // Il grafo della squadra lega il Baton Passer (Speed Boost, niente attacchi) al miglior attaccante speciale
    // in panchina: è lui che deve entrare a raccogliere i boost.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { Speed(20); }
        PLAYER(SPECIES_WOBBUFFET) { Speed(20); }
        OPPONENT(SPECIES_ESPATHRA) { Ability(ABILITY_SPEED_BOOST); Moves(MOVE_BATON_PASS); Speed(105); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_SCRATCH); Speed(30); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_SCRATCH); Speed(30); }
        OPPONENT(SPECIES_CHANDELURE) { Moves(MOVE_SHADOW_BALL, MOVE_HEAT_WAVE, MOVE_ENERGY_BALL); Speed(80); }
    } WHEN {
        TURN { EXPECT_MOVE(opponentLeft, MOVE_BATON_PASS); EXPECT_SEND_OUT(opponentLeft, 3); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: Fake Out on turn 1 covers a Baton Passer partner")
{
    // Ruoli dal grafo (nessun nome): il compagno di un Baton Passer usa Fake Out al turno 1.
    // Cosa fa Espathra lo decide la simulazione: contro avversari innocui, o contro cui Chandelure non ha bisogno
    // di boost, far entrare subito l'attaccante può essere giusto (un turno d'attacco in più). Il valore del setup
    // su più turni è un punto aperto (piano, M3).
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_GARCHOMP) { Speed(102); }
        PLAYER(SPECIES_GARCHOMP) { Speed(102); }
        OPPONENT(SPECIES_INCINEROAR) { Moves(MOVE_FAKE_OUT, MOVE_FLARE_BLITZ); Speed(60); }
        OPPONENT(SPECIES_ESPATHRA) { Ability(ABILITY_SPEED_BOOST); Moves(MOVE_CALM_MIND, MOVE_BATON_PASS, MOVE_PROTECT); Speed(105); }
        OPPONENT(SPECIES_CHANDELURE) { Moves(MOVE_SHADOW_BALL, MOVE_HEAT_WAVE, MOVE_ENERGY_BALL); Speed(80); }
    } WHEN {
        TURN { EXPECT_MOVE(opponentLeft, MOVE_FAKE_OUT); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: after a KO the replacement that pairs best with the partner comes in")
{
    // Whimsicott è in campo: dopo il KO del compagno entra Gallade (Beat Up -> Justified), non il Pokémon
    // che il motore sceglierebbe per il solo matchup.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { Speed(200); Moves(MOVE_SCRATCH); }
        PLAYER(SPECIES_WOBBUFFET) { Speed(200); Moves(MOVE_SCRATCH); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(1); Moves(MOVE_SCRATCH); Speed(10); }
        OPPONENT(SPECIES_WHIMSICOTT) { Moves(MOVE_BEAT_UP, MOVE_MOONBLAST); Speed(116); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_SCRATCH); Speed(10); }
        OPPONENT(SPECIES_GALLADE) { Ability(ABILITY_JUSTIFIED); Moves(MOVE_CLOSE_COMBAT); Speed(80); }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_SCRATCH, target: opponentLeft); MOVE(playerRight, MOVE_SCRATCH, target: opponentRight);
               EXPECT_SEND_OUT(opponentLeft, 3); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: at equal damage the accurate move is preferred to close a KO")
{
    // Entrambe le mosse mandano KO il bersaglio a 1 PS: quella al 100% batte quella all'80%.
    // (Una mossa ad area al 90% che manda KO due bersagli, invece, vale di più: ~1,8 KO attesi.)
    // DA INDAGARE: nel log VGCPLAN tutte le coppie hanno lo stesso valore, anche "Stone Edge sul bersaglio a PS
    // pieni" contro "Scratch che manda KO": il danno fisico di Garchomp non sembra entrare nella simulazione.
    KNOWN_FAILING;
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { HP(5); Speed(10); }
        PLAYER(SPECIES_WOBBUFFET) { Speed(10); }
        OPPONENT(SPECIES_GARCHOMP) { Moves(MOVE_STONE_EDGE, MOVE_SCRATCH); Speed(50); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); Speed(40); }
    } WHEN {
        TURN { NOT_EXPECT_MOVE(opponentLeft, MOVE_STONE_EDGE); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: Trick Room setter sets the room for its slow partner")
{
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { Speed(100); }
        PLAYER(SPECIES_WOBBUFFET) { Speed(100); }
        OPPONENT(SPECIES_INDEEDEE_F) { Moves(MOVE_TRICK_ROOM, MOVE_FOLLOW_ME, MOVE_PSYCHIC); Speed(85); }
        OPPONENT(SPECIES_COPPERAJAH) { Moves(MOVE_IRON_HEAD, MOVE_HEAVY_SLAM); Speed(30); }
    } WHEN {
        TURN { EXPECT_MOVE(opponentLeft, MOVE_TRICK_ROOM); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: no Simple Beam on Mycelium Might when Spore would fail anyway (Good as Gold)")
{
    // Senza Mycelium Might, Spore rimbalza su Good as Gold: togliere l'abilità al compagno è controproducente.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_GHOLDENGO) { Ability(ABILITY_GOOD_AS_GOLD); Speed(60); }
        PLAYER(SPECIES_GHOLDENGO) { Ability(ABILITY_GOOD_AS_GOLD); Speed(60); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_SIMPLE_BEAM, MOVE_SHADOW_BALL); Speed(150); }
        OPPONENT(SPECIES_TOEDSCRUEL) { Ability(ABILITY_MYCELIUM_MIGHT); Moves(MOVE_SPORE, MOVE_EARTH_POWER); Speed(100); }
    } WHEN {
        TURN { NOT_EXPECT_MOVE(opponentLeft, MOVE_SIMPLE_BEAM); }
    }
}
