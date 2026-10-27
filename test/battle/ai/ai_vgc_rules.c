#include "global.h"
#include "test/battle.h"
#include "battle_ai_util.h"
#include "battle_ai_vgc.h"
#include "ai_vgc_sets.h"

// M8: le regole di src/battle_ai_vgc.c ridotte a FILTRO delle candidate (azioni illegali o certamente inutili, tolte
// allo stesso modo dal pianificatore e dall'azione delle regole) e a una SPINTA leggera per gli effetti che la
// simulazione non modella. Un test per ogni filtro e per ogni spinta, con le squadre vere dei 12 allenatori VGC
// (src/data/trainers.party, Lv50; set in ai_vgc_sets.h). L'IA comanda il lato "opponent".
// Inventario delle regole e cosa è diventata ciascuna: docs/ia_vgc/checklist.md, sezione M8.

// ---------------------------------------------------------------------------------------------------------
// Filtro
// ---------------------------------------------------------------------------------------------------------

AI_DOUBLE_BATTLE_TEST("VGC rules: filter - no Fake Out after the first turn on the field (Carletto's Ludicolo)")
{
    // Turno 2: Fake Out fallisce sempre. Prima l'azione delle regole poteva esserlo comunque (a pari valore vinceva lei).
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_VENUSAUR) { GIGI_VENUSAUR; }
        PLAYER(SPECIES_TORKOAL) { GIGI_TORKOAL; }
        PLAYER(SPECIES_HATTERENE) { GIGI_HATTERENE; }
        PLAYER(SPECIES_GARGANACL) { GIGI_GARGANACL; }
        OPPONENT(SPECIES_LUDICOLO) { CARLETTO_LUDICOLO; }
        OPPONENT(SPECIES_PELIPPER) { CARLETTO_PELIPPER; }
        OPPONENT(SPECIES_KINGAMBIT) { CARLETTO_KINGAMBIT; }
        OPPONENT(SPECIES_AMOONGUSS) { CARLETTO_AMOONGUSS; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); }
        TURN { MOVE(playerLeft, MOVE_GIGA_DRAIN, target: opponentLeft); MOVE(playerRight, MOVE_HEAT_WAVE);
               NOT_EXPECT_MOVE(opponentLeft, MOVE_FAKE_OUT); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: filter - no Protect twice in a row (Gerozz's Dugtrio-Alola)")
{
    // Dugtrio a pochi PS (Focus Sash inutile) va KO da entrambi: al turno 1 si protegge, al turno 2 un secondo Protect
    // fallirebbe. Il turno 1 è la premessa del test; niente panchina (niente cambio al turno 2) e avversari robusti
    // (Vaporeon e Miltank di Elia: Tyranitar non li manda KO, il TURN resta quello scritto).
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_VAPOREON) { ELIA_VAPOREON; }
        PLAYER(SPECIES_MILTANK) { ELIA_MILTANK; }
        PLAYER(SPECIES_LICKITUNG) { ELIA_LICKITUNG; }
        PLAYER(SPECIES_GOODRA_HISUI) { ELIA_GOODRA_H; }
        OPPONENT(SPECIES_DUGTRIO_ALOLA) { GEROZZ_DUGTRIO_A; HP(20); }
        OPPONENT(SPECIES_TYRANITAR) { GEROZZ_TYRANITAR; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_MUDDY_WATER); MOVE(playerRight, MOVE_BODY_PRESS, target: opponentLeft);
               EXPECT_MOVE(opponentLeft, MOVE_PROTECT); }
        TURN { MOVE(playerLeft, MOVE_HELPING_HAND); MOVE(playerRight, MOVE_HELPING_HAND);
               NOT_EXPECT_MOVE(opponentLeft, MOVE_PROTECT); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: filter - no priority into Psychic Terrain (Tony's Sableye against Patatina)")
{
    // Indeedee porta il Campo Psichico: Fake Out e le mosse di stato con Prankster sugli avversari a terra falliscono.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_INDEEDEE_F) { PATATINA_INDEEDEE; }
        PLAYER(SPECIES_ARMAROUGE) { PATATINA_ARMAROUGE; }
        PLAYER(SPECIES_COPPERAJAH) { PATATINA_COPPERAJAH; }
        PLAYER(SPECIES_CERULEDGE) { PATATINA_CERULEDGE; }
        OPPONENT(SPECIES_SABLEYE) { TONY_SABLEYE; }
        OPPONENT(SPECIES_CHARIZARD) { TONY_CHARIZARD; }
        OPPONENT(SPECIES_ROARING_MOON) { TONY_ROARING_MOON; }
        OPPONENT(SPECIES_SLITHER_WING) { TONY_SLITHER_WING; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_FOLLOW_ME); MOVE(playerRight, MOVE_PROTECT); NOT_EXPECT_MOVES(opponentLeft, MOVE_FAKE_OUT, MOVE_TAUNT); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: filter - no Normal or Fighting move into two Ghosts (Mario's Hariyama against Elia)")
{
    // Mimikyu e Banette sono Spettro: Fake Out e Close Combat non li toccano. Resta Heavy Slam (o Protect).
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_MIMIKYU) { ELIA_MIMIKYU; }
        PLAYER(SPECIES_BANETTE) { ELIA_BANETTE; }
        PLAYER(SPECIES_GOODRA_HISUI) { ELIA_GOODRA_H; }
        PLAYER(SPECIES_LICKITUNG) { ELIA_LICKITUNG; }
        OPPONENT(SPECIES_HARIYAMA) { MARIO_HARIYAMA; }
        OPPONENT(SPECIES_POLITOED) { MARIO_POLITOED; }
        OPPONENT(SPECIES_IRON_HANDS) { MARIO_IRON_HANDS; }
        OPPONENT(SPECIES_GOLDUCK) { MARIO_GOLDUCK; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT);
               NOT_EXPECT_MOVES(opponentLeft, MOVE_FAKE_OUT, MOVE_CLOSE_COMBAT); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: filter - no Earthquake when both foes are immune (Gerozz's Dugtrio-Alola against Patatina)")
{
    // Altaria vola e Rotom-Wash ha Levitate: Earthquake colpirebbe solo il compagno.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_ALTARIA) { PATATINA_ALTARIA; }
        PLAYER(SPECIES_ROTOM_WASH) { PATATINA_ROTOM_WASH; }
        PLAYER(SPECIES_ARMAROUGE) { PATATINA_ARMAROUGE; }
        PLAYER(SPECIES_CERULEDGE) { PATATINA_CERULEDGE; }
        OPPONENT(SPECIES_DUGTRIO_ALOLA) { GEROZZ_DUGTRIO_A; }
        OPPONENT(SPECIES_TYRANITAR) { GEROZZ_TYRANITAR; }
        OPPONENT(SPECIES_GHOLDENGO) { GEROZZ_GHOLDENGO; }
        OPPONENT(SPECIES_GREAT_TUSK) { GEROZZ_GREAT_TUSK; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); NOT_EXPECT_MOVE(opponentLeft, MOVE_EARTHQUAKE); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: filter - no Sleep Powder into two Grass types (Gigi Pinto's Venusaur against Carletto)")
{
    // Amoonguss e Ludicolo sono Erba: le mosse polvere non li toccano.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_AMOONGUSS) { CARLETTO_AMOONGUSS; }
        PLAYER(SPECIES_LUDICOLO) { CARLETTO_LUDICOLO; }
        PLAYER(SPECIES_PELIPPER) { CARLETTO_PELIPPER; }
        PLAYER(SPECIES_SNORLAX) { CARLETTO_SNORLAX; }
        OPPONENT(SPECIES_VENUSAUR) { GIGI_VENUSAUR; }
        OPPONENT(SPECIES_TORKOAL) { GIGI_TORKOAL; }
        OPPONENT(SPECIES_HATTERENE) { GIGI_HATTERENE; }
        OPPONENT(SPECIES_LILLIGANT) { GIGI_LILLIGANT; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_RAGE_POWDER); MOVE(playerRight, MOVE_SCALD, target: opponentRight); NOT_EXPECT_MOVE(opponentLeft, MOVE_SLEEP_POWDER); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: filter - no Prankster Taunt into two Dark types (Enzo's Grimmsnarl against Carletto)")
{
    // Kingambit e Malamar sono Buio: le mosse di stato potenziate da Prankster falliscono su di loro.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_KINGAMBIT) { CARLETTO_KINGAMBIT; }
        PLAYER(SPECIES_MALAMAR) { CARLETTO_MALAMAR; }
        PLAYER(SPECIES_PELIPPER) { CARLETTO_PELIPPER; }
        PLAYER(SPECIES_SNORLAX) { CARLETTO_SNORLAX; }
        OPPONENT(SPECIES_GRIMMSNARL) { ENZO_GRIMMSNARL; }
        OPPONENT(SPECIES_ANNIHILAPE) { ENZO_ANNIHILAPE; }
        OPPONENT(SPECIES_GENGAR) { ENZO_GENGAR; }
        OPPONENT(SPECIES_RILLABOOM) { ENZO_RILLABOOM; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_TRICK_ROOM); NOT_EXPECT_MOVE(opponentLeft, MOVE_TAUNT); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: filter - no Sunny Day while the sun is already up (Tony's Sableye against Gigi Pinto)")
{
    // Drought di Torkoal ha già portato il sole: Sunny Day fallisce.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_TORKOAL) { GIGI_TORKOAL; }
        PLAYER(SPECIES_VENUSAUR) { GIGI_VENUSAUR; }
        PLAYER(SPECIES_HATTERENE) { GIGI_HATTERENE; }
        PLAYER(SPECIES_LILLIGANT) { GIGI_LILLIGANT; }
        OPPONENT(SPECIES_SABLEYE) { TONY_SABLEYE; }
        OPPONENT(SPECIES_ROARING_MOON) { TONY_ROARING_MOON; }
        OPPONENT(SPECIES_CHARIZARD) { TONY_CHARIZARD; }
        OPPONENT(SPECIES_SLITHER_WING) { TONY_SLITHER_WING; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); NOT_EXPECT_MOVE(opponentLeft, MOVE_SUNNY_DAY); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: filter - no second Trick Room that would end our own (Elia's Mimikyu against Tony)")
{
    // Turno 1: Mimikyu apre la stanza (premessa, come nel test della linea di Elia). Turno 2: un altro Trick Room la
    // chiuderebbe mentre la squadra lenta ne ha bisogno.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_ROARING_MOON) { TONY_ROARING_MOON; }
        PLAYER(SPECIES_CHARIZARD) { TONY_CHARIZARD; }
        PLAYER(SPECIES_SABLEYE) { TONY_SABLEYE; }
        PLAYER(SPECIES_GYARADOS) { TONY_GYARADOS; }
        OPPONENT(SPECIES_MIMIKYU) { ELIA_MIMIKYU; }
        OPPONENT(SPECIES_GOODRA_HISUI) { ELIA_GOODRA_H; }
        OPPONENT(SPECIES_LICKITUNG) { ELIA_LICKITUNG; }
        OPPONENT(SPECIES_BANETTE) { ELIA_BANETTE; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVE(opponentLeft, MOVE_TRICK_ROOM); }
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); NOT_EXPECT_MOVE(opponentLeft, MOVE_TRICK_ROOM); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: filter - no Recycle with the berry still held (Carletto's Snorlax)")
{
    // La Sitrus Berry è ancora in mano: Recycle non ha niente da riciclare.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_TORKOAL) { GIGI_TORKOAL; }
        PLAYER(SPECIES_VENUSAUR) { GIGI_VENUSAUR; }
        PLAYER(SPECIES_HATTERENE) { GIGI_HATTERENE; }
        PLAYER(SPECIES_LILLIGANT) { GIGI_LILLIGANT; }
        OPPONENT(SPECIES_SNORLAX) { CARLETTO_SNORLAX; }
        OPPONENT(SPECIES_AMOONGUSS) { CARLETTO_AMOONGUSS; }
        OPPONENT(SPECIES_KINGAMBIT) { CARLETTO_KINGAMBIT; }
        OPPONENT(SPECIES_PELIPPER) { CARLETTO_PELIPPER; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); NOT_EXPECT_MOVE(opponentLeft, MOVE_RECYCLE); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: filter - no Belly Drum at half HP or less (Carletto's Snorlax)")
{
    // A metà PS Belly Drum fallisce.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_TORKOAL) { GIGI_TORKOAL; }
        PLAYER(SPECIES_VENUSAUR) { GIGI_VENUSAUR; }
        PLAYER(SPECIES_HATTERENE) { GIGI_HATTERENE; }
        PLAYER(SPECIES_LILLIGANT) { GIGI_LILLIGANT; }
        OPPONENT(SPECIES_SNORLAX) { CARLETTO_SNORLAX; HP(100); }
        OPPONENT(SPECIES_AMOONGUSS) { CARLETTO_AMOONGUSS; }
        OPPONENT(SPECIES_KINGAMBIT) { CARLETTO_KINGAMBIT; }
        OPPONENT(SPECIES_PELIPPER) { CARLETTO_PELIPPER; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); NOT_EXPECT_MOVE(opponentLeft, MOVE_BELLY_DRUM); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: filter - no Baton Pass without anyone left on the bench (Savarotto's Espathra)")
{
    // Restano solo i due in campo: Baton Pass fallisce.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_MACHAMP) { SIMONE_MACHAMP; }
        PLAYER(SPECIES_ARCANINE) { SIMONE_ARCANINE; }
        OPPONENT(SPECIES_INCINEROAR) { SAVA_INCINEROAR; }
        OPPONENT(SPECIES_ESPATHRA) { SAVA_ESPATHRA; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_BULLET_PUNCH, target: opponentLeft); MOVE(playerRight, MOVE_SNARL); NOT_EXPECT_MOVE(opponentRight, MOVE_BATON_PASS); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: filter - no Simple Beam on a partner whose ability helps (Simone's Porygon-Z)")
{
    // Arcanine ha Intimidate: cambiargli l'abilità non toglie niente di penalizzante. Il filtro vale per il compagno;
    // Simple Beam sugli avversari resta lecito (e qui non serve).
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_PELIPPER) { CARLETTO_PELIPPER; }
        PLAYER(SPECIES_LUDICOLO) { CARLETTO_LUDICOLO; }
        PLAYER(SPECIES_KINGAMBIT) { CARLETTO_KINGAMBIT; }
        PLAYER(SPECIES_AMOONGUSS) { CARLETTO_AMOONGUSS; }
        OPPONENT(SPECIES_PORYGON_Z) { SIMONE_PORYGON_Z; }
        OPPONENT(SPECIES_ARCANINE) { SIMONE_ARCANINE; }
        OPPONENT(SPECIES_MACHAMP) { SIMONE_MACHAMP; }
        OPPONENT(SPECIES_AGGRON) { SIMONE_AGGRON; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_SCALD, target: opponentRight);
               SCORE_LT_VAL(opponentLeft, MOVE_SIMPLE_BEAM, AI_SCORE_DEFAULT, target: opponentRight);
               NOT_EXPECT_MOVE(opponentLeft, MOVE_SIMPLE_BEAM); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: filter - no Instruct on a foe (Mario's Oranguru)")
{
    // Instruct su un avversario gli fa ripetere il suo attacco: l'azione delle regole non può sceglierla.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_ANNIHILAPE) { ENZO_ANNIHILAPE; }
        PLAYER(SPECIES_SMEARGLE) { ENZO_SMEARGLE; }
        PLAYER(SPECIES_RILLABOOM) { ENZO_RILLABOOM; }
        PLAYER(SPECIES_GENGAR) { ENZO_GENGAR; }
        OPPONENT(SPECIES_ORANGURU) { MARIO_ORANGURU; }
        OPPONENT(SPECIES_IRON_HANDS) { MARIO_IRON_HANDS; }
        OPPONENT(SPECIES_HARIYAMA) { MARIO_HARIYAMA; }
        OPPONENT(SPECIES_POLITOED) { MARIO_POLITOED; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_ICE_PUNCH, target: opponentRight); MOVE(playerRight, MOVE_FOLLOW_ME);
               SCORE_LT_VAL(opponentLeft, MOVE_INSTRUCT, AI_SCORE_DEFAULT, target: playerLeft);
               SCORE_LT_VAL(opponentLeft, MOVE_INSTRUCT, AI_SCORE_DEFAULT, target: playerRight); }
    }
}

// ---------------------------------------------------------------------------------------------------------
// Spinta (effetti che la simulazione non modella)
// ---------------------------------------------------------------------------------------------------------

AI_DOUBLE_BATTLE_TEST("VGC rules: prior - Gravity is worth something next to Hypnosis (Enzo's Smeargle and Gengar)")
{
    // Gravity porta Hypnosis al 100%: la simulazione non lo vede, la spinta sì. Il valore della coppia con Gravity
    // deve superare quello senza spinta (si confronta con Spore, la mossa di sonno di Smeargle che non ne ha bisogno).
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_CHARIZARD) { TONY_CHARIZARD; }
        PLAYER(SPECIES_ROARING_MOON) { TONY_ROARING_MOON; }
        PLAYER(SPECIES_SABLEYE) { TONY_SABLEYE; }
        PLAYER(SPECIES_GYARADOS) { TONY_GYARADOS; }
        OPPONENT(SPECIES_SMEARGLE) { ENZO_SMEARGLE; }
        OPPONENT(SPECIES_GENGAR) { ENZO_GENGAR; }
        OPPONENT(SPECIES_ANNIHILAPE) { ENZO_ANNIHILAPE; }
        OPPONENT(SPECIES_RILLABOOM) { ENZO_RILLABOOM; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); }
    } THEN {
        EXPECT_EQ(AI_VgcPlanTestPrior(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT), MOVE_GRAVITY), 30);
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: prior - Quick Guard against a double Fake Out lead (Tatore's Sceptile against Savarotto)")
{
    // Incineroar e Sneasler hanno Fake Out al turno 1: Quick Guard lo blocca per entrambi. La simulazione non lo modella:
    // la spinta va a Quick Guard, non a Protect.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_INCINEROAR) { SAVA_INCINEROAR; }
        PLAYER(SPECIES_SNEASLER) { SAVA_SNEASLER; }
        PLAYER(SPECIES_ESPATHRA) { SAVA_ESPATHRA; }
        PLAYER(SPECIES_GALLADE) { SAVA_GALLADE; }
        OPPONENT(SPECIES_SCEPTILE) { TATORE_SCEPTILE; }
        OPPONENT(SPECIES_DUSCLOPS) { TATORE_DUSCLOPS; }
        OPPONENT(SPECIES_ARMALDO) { TATORE_ARMALDO; }
        OPPONENT(SPECIES_LAPRAS) { TATORE_LAPRAS; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_DARKEST_LARIAT, target: opponentRight); MOVE(playerRight, MOVE_PROTECT); }
    } THEN {
        EXPECT_EQ(AI_VgcPlanTestPrior(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT), MOVE_QUICK_GUARD), 30);
        EXPECT_EQ(AI_VgcPlanTestPrior(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT), MOVE_PROTECT), 0);
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: prior - Instruct repeats the partner's attack (Mario's Oranguru and Golduck)")
{
    // Golduck, più veloce di Oranguru fuori dal Trick Room, attacca: Instruct gli fa ripetere il colpo (il secondo
    // colpo la simulazione non lo vede). Con Iron Hands no: è più lento di Oranguru, Instruct arriverebbe prima.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_TORKOAL) { GIGI_TORKOAL; }
        PLAYER(SPECIES_HATTERENE) { GIGI_HATTERENE; }
        PLAYER(SPECIES_VENUSAUR) { GIGI_VENUSAUR; }
        PLAYER(SPECIES_GARGANACL) { GIGI_GARGANACL; }
        OPPONENT(SPECIES_ORANGURU) { MARIO_ORANGURU; }
        OPPONENT(SPECIES_GOLDUCK) { MARIO_GOLDUCK; }
        OPPONENT(SPECIES_HARIYAMA) { MARIO_HARIYAMA; }
        OPPONENT(SPECIES_IRON_HANDS) { MARIO_IRON_HANDS; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); }
    } THEN {
        EXPECT_EQ(AI_VgcPlanTestPrior(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT), MOVE_INSTRUCT), 40);
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: prior - After You lets Torkoal Erupt first (Gigi Pinto's Lilligant)")
{
    // Torkoal a PS pieni sotto il sole è più lento di Roaring Moon: After You lo fa muovere subito, prima di prendere
    // danni (Eruption cala con i PS). L'ordine dei turni dopo After You non è simulato: lo dice la spinta.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_ROARING_MOON) { TONY_ROARING_MOON; }
        PLAYER(SPECIES_GYARADOS) { TONY_GYARADOS; }
        PLAYER(SPECIES_SABLEYE) { TONY_SABLEYE; }
        PLAYER(SPECIES_SLITHER_WING) { TONY_SLITHER_WING; }
        OPPONENT(SPECIES_LILLIGANT) { GIGI_LILLIGANT; }
        OPPONENT(SPECIES_TORKOAL) { GIGI_TORKOAL; }
        OPPONENT(SPECIES_VENUSAUR) { GIGI_VENUSAUR; }
        OPPONENT(SPECIES_HATTERENE) { GIGI_HATTERENE; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); }
    } THEN {
        EXPECT_EQ(AI_VgcPlanTestPrior(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT), MOVE_AFTER_YOU), 40);
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: prior - Sunny Day for the Chlorophyll partner on the bench (Tony's Sableye)")
{
    // Nessun sole in campo; in panchina Brute Bonnet e Slither Wing hanno Protosynthesis... qui conta chi sfrutta il
    // meteo ed è fuori dalla simulazione (che vede solo chi è in campo).
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_PELIPPER) { CARLETTO_PELIPPER; Ability(ABILITY_KEEN_EYE); }
        PLAYER(SPECIES_KINGAMBIT) { CARLETTO_KINGAMBIT; }
        PLAYER(SPECIES_LUDICOLO) { CARLETTO_LUDICOLO; }
        PLAYER(SPECIES_AMOONGUSS) { CARLETTO_AMOONGUSS; }
        OPPONENT(SPECIES_SABLEYE) { TONY_SABLEYE; }
        OPPONENT(SPECIES_ROARING_MOON) { TONY_ROARING_MOON; }
        OPPONENT(SPECIES_VENUSAUR) { GIGI_VENUSAUR; }
        OPPONENT(SPECIES_CHARIZARD) { TONY_CHARIZARD; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); }
    } THEN {
        EXPECT_EQ(AI_VgcPlanTestPrior(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT), MOVE_SUNNY_DAY), 20);
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: prior - Contrary turns Superpower into a boost (Carletto's Malamar)")
{
    // Con Contrary Superpower alza Atk e Dif: la simulazione non vede il cambio di stadi, la spinta sì.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_TORKOAL) { GIGI_TORKOAL; }
        PLAYER(SPECIES_HATTERENE) { GIGI_HATTERENE; }
        PLAYER(SPECIES_VENUSAUR) { GIGI_VENUSAUR; }
        PLAYER(SPECIES_GARGANACL) { GIGI_GARGANACL; }
        OPPONENT(SPECIES_MALAMAR) { CARLETTO_MALAMAR; }
        OPPONENT(SPECIES_KINGAMBIT) { CARLETTO_KINGAMBIT; }
        OPPONENT(SPECIES_SNORLAX) { CARLETTO_SNORLAX; }
        OPPONENT(SPECIES_AMOONGUSS) { CARLETTO_AMOONGUSS; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); }
    } THEN {
        EXPECT_EQ(AI_VgcPlanTestPrior(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT), MOVE_SUPERPOWER), 30);
        EXPECT_EQ(AI_VgcPlanTestPrior(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT), MOVE_KNOCK_OFF), 0);
    }
}

AI_DOUBLE_BATTLE_TEST("VGC rules: prior - Neutralizing Gas protects itself next to Slaking (Mario Lett's Weezing-Galar)")
{
    // Se Weezing va KO, Truant torna attivo su Slaking: sotto minaccia di KO, Protect vale di più.
    GIVEN {
        AI_FLAGS(VGC_TRAINER_AI_FLAGS);
        PLAYER(SPECIES_GHOLDENGO) { GEROZZ_GHOLDENGO; }
        PLAYER(SPECIES_GREAT_TUSK) { GEROZZ_GREAT_TUSK; }
        PLAYER(SPECIES_TYRANITAR) { GEROZZ_TYRANITAR; }
        PLAYER(SPECIES_ROTOM_HEAT) { GEROZZ_ROTOM_HEAT; }
        OPPONENT(SPECIES_WEEZING_GALAR) { LETT_WEEZING_G; HP(40); }
        OPPONENT(SPECIES_SLAKING) { LETT_SLAKING; }
        OPPONENT(SPECIES_SINISTCHA) { LETT_SINISTCHA; }
        OPPONENT(SPECIES_DONDOZO) { LETT_DONDOZO; }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_MAKE_IT_RAIN); MOVE(playerRight, MOVE_HEADLONG_RUSH, target: opponentLeft);
               EXPECT_MOVE(opponentLeft, MOVE_PROTECT); }
    } THEN {
        EXPECT_EQ(AI_VgcPlanTestPrior(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT), MOVE_PROTECT), 30);
    }
}
