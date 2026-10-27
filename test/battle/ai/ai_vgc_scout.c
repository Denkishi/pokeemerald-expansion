#include "global.h"
#include "test/battle.h"
#include "battle_ai_util.h"
#include "battle_ai_scout.h"

// Base di conoscenza VGC (src/data/vgc_meta.h, statistiche d'uso Showdown) e belief sui Pokémon del
// giocatore (src/battle_ai_scout.c). Vedi docs/ia_vgc/piano_implementazione.md (M1).
// I test sulle statistiche controllano proprietà stabili (non i numeri esatti, che cambiano a ogni
// rigenerazione della tabella).

#define VGC_AI_FLAGS (AI_FLAG_BASIC_TRAINER | AI_FLAG_SMART_SWITCHING | AI_FLAG_SMART_MON_CHOICES \
                    | AI_FLAG_KNOW_OPPONENT_PARTY | AI_FLAG_ASSUME_STAB | AI_FLAG_ASSUME_STATUS_MOVES \
                    | AI_FLAG_HP_AWARE | AI_FLAG_WEIGH_ABILITY_PREDICTION | AI_FLAG_VGC_DOUBLES)

TEST("VGC scout: the meta table is sorted and covers the VGC trainers' species")
{
    static const u16 trainerSpecies[] =
    {
        SPECIES_INCINEROAR, SPECIES_AMOONGUSS, SPECIES_INDEEDEE_F, SPECIES_ROTOM_MOW, SPECIES_ROTOM_WASH,
        SPECIES_GOODRA_HISUI, SPECIES_KECLEON, SPECIES_SMEARGLE, SPECIES_WHIMSICOTT, SPECIES_PELIPPER,
        SPECIES_TOEDSCRUEL, SPECIES_KINGAMBIT, SPECIES_GHOLDENGO, SPECIES_SLAKING, SPECIES_ARMALDO,
    };
    u32 i;

    EXPECT(Scout_MetaTableIsSorted());
    for (i = 0; i < ARRAY_COUNT(trainerSpecies); i++)
        EXPECT(Scout_HasMetaData(trainerSpecies[i]));
    EXPECT(!Scout_HasMetaData(SPECIES_NONE));
}

TEST("VGC scout: spreads are sorted by frequency and are legal")
{
    struct ScoutSpread s[3];
    u32 n, i, k, sum = 0;

    n = Scout_GetMetaSpreads(SPECIES_INCINEROAR, s, ARRAY_COUNT(s));
    EXPECT_GT(n, 1);
    for (i = 0; i < n; i++)
    {
        u32 evs = 0;
        for (k = 0; k < NUM_STATS; k++)
        {
            EXPECT_LE(s[i].ev[k], 252);
            evs += s[i].ev[k];
        }
        // Champions: 66 punti statistica (1 punto ~ 8 EV) possono superare i 510 EV classici.
        EXPECT_LE(evs, 66 * 8);
        EXPECT_LT(s[i].nature, NUM_NATURES);
        if (i > 0)
            EXPECT_LE(s[i].p, s[i - 1].p);
        sum += s[i].p;
    }
    EXPECT_LE(sum, 100);
}

TEST("VGC scout: estimated stats use the most common spread")
{
    struct ScoutSpread s;
    u16 hp, stats[NUM_STATS], fromSpread[NUM_STATS];
    u32 i;

    EXPECT_EQ(Scout_GetMetaSpreads(SPECIES_GARCHOMP, &s, 1), 1);
    Scout_EstimateStats(SPECIES_GARCHOMP, 50, &hp, stats);
    Scout_StatsFromSpread(SPECIES_GARCHOMP, 50, &s, fromSpread);
    for (i = 0; i < NUM_STATS; i++)
        EXPECT_EQ(stats[i], fromSpread[i]);
    EXPECT_EQ(hp, stats[STAT_HP]);
}

TEST("VGC scout: speed range of a species at level 50")
{
    u16 lo, hi;

    // Garchomp (base 102): 0 IV/0 EV/natura - = 96, 31 IV/252 EV/natura + (Jolly) = 169.
    Scout_SpeciesSpeedRange(SPECIES_GARCHOMP, 50, &lo, &hi);
    EXPECT_EQ(lo, 96);
    EXPECT_EQ(hi, 169);
}

TEST("VGC scout: likely moves come from usage and are learnable")
{
    struct ScoutMoveChance m[10];
    u32 n, i;

    // Incineroar porta quasi sempre Fake Out e Parting Shot.
    EXPECT_GE(Scout_MoveChance(SPECIES_INCINEROAR, MOVE_FAKE_OUT), 80);
    EXPECT_GE(Scout_MoveChance(SPECIES_INCINEROAR, MOVE_PARTING_SHOT), 80);
    // Mossa che non può imparare.
    EXPECT_EQ(Scout_MoveChance(SPECIES_INCINEROAR, MOVE_SPORE), 0);
    // Amoonguss: Spore e Rage Powder.
    EXPECT_GE(Scout_MoveChance(SPECIES_AMOONGUSS, MOVE_SPORE), 60);
    EXPECT_GE(Scout_MoveChance(SPECIES_AMOONGUSS, MOVE_RAGE_POWDER), 60);

    n = Scout_GetLikelyMoves(SPECIES_INCINEROAR, m, ARRAY_COUNT(m));
    EXPECT_GT(n, 3);
    for (i = 1; i < n; i++)
        EXPECT_LE(m[i].p, m[i - 1].p);
}

TEST("VGC scout: ability probabilities, including Mega abilities")
{
    struct ScoutMegaChance mega[2];
    u32 n, i, chance;
    bool32 foundY = FALSE;

    EXPECT_GE(Scout_AbilityChance(SPECIES_INCINEROAR, ABILITY_INTIMIDATE), 90);
    EXPECT_EQ(Scout_LikelyAbility(SPECIES_INCINEROAR, &chance), ABILITY_INTIMIDATE);
    EXPECT_GE(chance, 90);
    // Prima della Mega Salamence ha Intimidate, dopo Aerilate (certa).
    EXPECT_GE(Scout_AbilityChance(SPECIES_SALAMENCE, ABILITY_INTIMIDATE), 80);
    EXPECT_EQ(Scout_AbilityChance(SPECIES_SALAMENCE_MEGA, ABILITY_AERILATE), 100);
    // Charizard: la Mega Y (Drought) tra le Mega usate.
    n = Scout_GetMegaForms(SPECIES_CHARIZARD, mega, ARRAY_COUNT(mega));
    for (i = 0; i < n; i++)
    {
        if (mega[i].species == SPECIES_CHARIZARD_MEGA_Y)
        {
            foundY = TRUE;
            EXPECT_EQ(mega[i].ability, ABILITY_DROUGHT);
            EXPECT_GT(mega[i].p, 0);
        }
    }
    EXPECT(foundY);
    // Una Mega usa i dati della forma base (spread, mosse).
    EXPECT(Scout_HasMetaData(SPECIES_CHARIZARD_MEGA_Y));
}

TEST("VGC scout: likely items from usage")
{
    struct ScoutItemChance it[4];
    u32 n = Scout_GetLikelyItems(SPECIES_INCINEROAR, it, ARRAY_COUNT(it));

    EXPECT_GT(n, 0);
    EXPECT_NE(it[0].item, ITEM_NONE);
    EXPECT_GT(it[0].p, 0);
}

TEST("VGC scout: Bo3 belief snapshot round trip")
{
    struct ScoutBeliefSnapshot in = {0}, out;

    in.species = SPECIES_GARCHOMP;
    in.speedLo = 120;
    in.speedHi = 150;
    in.itemNot = SCOUT_NOT_SCARF;
    in.abilityExcluded = 1;
    in.heldEffect = HOLD_EFFECT_ASSAULT_VEST;
    Scout_BeliefReset();
    Scout_BeliefImport(2, &in);
    Scout_BeliefExport(2, &out);
    EXPECT_EQ(out.species, in.species);
    EXPECT_EQ(out.speedLo, in.speedLo);
    EXPECT_EQ(out.speedHi, in.speedHi);
    EXPECT_EQ(out.itemNot, in.itemNot);
    EXPECT_EQ(out.abilityExcluded, in.abilityExcluded);
    EXPECT_EQ(out.heldEffect, in.heldEffect);
    Scout_BeliefReset();
}

// --- Belief in lotta ---------------------------------------------------------------------------

AI_DOUBLE_BATTLE_TEST("VGC belief: Choice Scarf deduced from turn order")
{
    // Incineroar a Lv50 arriva al massimo a 123 di Velocità: se supera un nostro Pokémon a 150 senza
    // priorità, ha una Choice Scarf.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_INCINEROAR) { Level(50); Ability(ABILITY_BLAZE); Speed(200); Moves(MOVE_SCRATCH); }
        PLAYER(SPECIES_WOBBUFFET) { Level(50); Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(50); Speed(150); Moves(MOVE_SCRATCH); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(50); Speed(140); Moves(MOVE_SCRATCH); }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_SCRATCH, target: opponentLeft); MOVE(playerRight, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(Scout_BattlerDeducedHoldEffect(GetBattlerAtPosition(B_POSITION_PLAYER_LEFT)), HOLD_EFFECT_CHOICE_SCARF);
        EXPECT_EQ(Scout_BattlerDeducedHoldEffect(GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT)), HOLD_EFFECT_NONE);
    }
}

AI_DOUBLE_BATTLE_TEST("VGC belief: turn order narrows the player's speed")
{
    u16 lo = 0, hi = 0;

    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_GARCHOMP) { Level(50); Ability(ABILITY_ROUGH_SKIN); Speed(130); Moves(MOVE_SCRATCH); }
        PLAYER(SPECIES_WOBBUFFET) { Level(50); Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(50); Speed(140); Moves(MOVE_SCRATCH); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(50); Speed(120); Moves(MOVE_SCRATCH); }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_SCRATCH, target: opponentLeft); MOVE(playerRight, MOVE_CELEBRATE); }
    } THEN {
        EXPECT(Scout_BattlerSpeedRange(GetBattlerAtPosition(B_POSITION_PLAYER_LEFT), &lo, &hi));
        EXPECT_GE(lo, 120);
        EXPECT_LE(hi, 140);
        // Più lento di 140 con qualunque Velocità ancora possibile x1.5: niente Choice Scarf.
        EXPECT(Scout_BattlerHoldEffectExcluded(GetBattlerAtPosition(B_POSITION_PLAYER_LEFT), HOLD_EFFECT_CHOICE_SCARF));
        EXPECT_EQ(Scout_BattlerDeducedHoldEffect(GetBattlerAtPosition(B_POSITION_PLAYER_LEFT)), HOLD_EFFECT_NONE);
    }
}

AI_DOUBLE_BATTLE_TEST("VGC belief: Intimidate not activated means the other ability")
{
    u32 chance = 0;

    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_INCINEROAR) { Level(50); Ability(ABILITY_BLAZE); Speed(50); Moves(MOVE_SCRATCH); }
        PLAYER(SPECIES_INCINEROAR) { Level(50); Ability(ABILITY_INTIMIDATE); Speed(50); Moves(MOVE_SCRATCH); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(50); Speed(100); Moves(MOVE_SCRATCH); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(50); Speed(100); Moves(MOVE_SCRATCH); }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_SCRATCH, target: opponentLeft); MOVE(playerRight, MOVE_SCRATCH, target: opponentLeft); }
    } THEN {
        EXPECT_EQ(Scout_BattlerAbilityChance(GetBattlerAtPosition(B_POSITION_PLAYER_LEFT), ABILITY_INTIMIDATE), 0);
        EXPECT_EQ(Scout_BattlerLikelyAbility(GetBattlerAtPosition(B_POSITION_PLAYER_LEFT), &chance), ABILITY_BLAZE);
        EXPECT_EQ(chance, 100);
        EXPECT_EQ(Scout_BattlerAbilityChance(GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT), ABILITY_INTIMIDATE), 100);
    }
}

AI_DOUBLE_BATTLE_TEST("VGC belief: Assault Vest deduced from special damage taken")
{
    // Wobbuffet a Lv50 ha al massimo 121 di Difesa Speciale: con Assault Vest il danno speciale scende
    // sotto il minimo possibile senza strumento.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { Level(50); SpDefense(121); Item(ITEM_ASSAULT_VEST); Speed(10); Moves(MOVE_SCRATCH); }
        PLAYER(SPECIES_WOBBUFFET) { Level(50); SpDefense(121); Item(ITEM_ASSAULT_VEST); Speed(10); Moves(MOVE_SCRATCH); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(50); SpAttack(300); Speed(100); Moves(MOVE_DAZZLING_GLEAM); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(50); Speed(90); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_SCRATCH, target: opponentLeft); MOVE(playerRight, MOVE_SCRATCH, target: opponentLeft); }
    } THEN {
        EXPECT_EQ(Scout_BattlerDeducedHoldEffect(GetBattlerAtPosition(B_POSITION_PLAYER_LEFT)), HOLD_EFFECT_ASSAULT_VEST);
        EXPECT_EQ(Scout_BattlerDeducedHoldEffect(GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT)), HOLD_EFFECT_ASSAULT_VEST);
    }
}

AI_DOUBLE_BATTLE_TEST("VGC belief: no Assault Vest deduced without it")
{
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { Level(50); SpDefense(121); Speed(10); Moves(MOVE_SCRATCH); }
        PLAYER(SPECIES_WOBBUFFET) { Level(50); SpDefense(60); Speed(10); Moves(MOVE_SCRATCH); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(50); SpAttack(300); Speed(100); Moves(MOVE_DAZZLING_GLEAM); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(50); Speed(90); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_SCRATCH, target: opponentLeft); MOVE(playerRight, MOVE_SCRATCH, target: opponentLeft); }
    } THEN {
        EXPECT_EQ(Scout_BattlerDeducedHoldEffect(GetBattlerAtPosition(B_POSITION_PLAYER_LEFT)), HOLD_EFFECT_NONE);
        EXPECT_EQ(Scout_BattlerDeducedHoldEffect(GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT)), HOLD_EFFECT_NONE);
    }
}

AI_DOUBLE_BATTLE_TEST("VGC belief: Choice Band deduced from damage dealt")
{
    // Wobbuffet a Lv50 ha al massimo 93 di Attacco: con Choice Band il danno supera di molto il massimo
    // possibile senza strumento (qui Attacco 110 perché anche il tiro più basso basti).
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { Level(50); Attack(110); Item(ITEM_CHOICE_BAND); Speed(10); Moves(MOVE_BODY_SLAM, MOVE_SCRATCH); }
        PLAYER(SPECIES_WOBBUFFET) { Level(50); Speed(10); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(50); Speed(100); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(50); Speed(90); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_BODY_SLAM, target: opponentLeft); MOVE(playerRight, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(Scout_BattlerDeducedHoldEffect(GetBattlerAtPosition(B_POSITION_PLAYER_LEFT)), HOLD_EFFECT_CHOICE_BAND);
    }
}
