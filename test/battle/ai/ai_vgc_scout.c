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

// --- Contesto della squadra (team preview): cosa si capisce già guardando i 6 ------------------------

static const u16 sCtxTrickRoomTeam[] = { SPECIES_HATTERENE, SPECIES_DUSCLOPS, SPECIES_URSALUNA, SPECIES_TORKOAL, SPECIES_AMOONGUSS, SPECIES_INCINEROAR };
static const u16 sCtxTailwindTeam[] = { SPECIES_WHIMSICOTT, SPECIES_KILOWATTREL, SPECIES_GARCHOMP, SPECIES_DRAGAPULT, SPECIES_URSALUNA, SPECIES_CHIEN_PAO };

TEST("VGC context: a Trick Room team gives its slow mons the minimum Speed")
{
    struct ScoutTeamContext tr, tw;
    u16 hp, stats[NUM_STATS], lo, hi;
    u32 i;

    Scout_ComputeTeamContext(sCtxTrickRoomTeam, ARRAY_COUNT(sCtxTrickRoomTeam), &tr);
    Scout_ComputeTeamContext(sCtxTailwindTeam, ARRAY_COUNT(sCtxTailwindTeam), &tw);
    EXPECT(tr.archetype & SCOUT_ARCH_TRICK_ROOM);
    EXPECT_EQ(tr.plan, SCOUT_PLAN_TRICK_ROOM);
    EXPECT(!(tw.archetype & SCOUT_ARCH_TRICK_ROOM));
    // I lenti della squadra TR: Velocità minima possibile (natura -Vel, 0 EV, 0 IV).
    for (i = 0; i < tr.n; i++)
    {
        u16 sp = tr.slot[i].species;
        if (gSpeciesInfo[sp].baseSpeed > 60)
            continue;
        EXPECT_EQ(tr.slot[i].speedMode, SCOUT_SPEED_MIN);
        Scout_EstimateStatsInTeam(&tr, sp, 50, &hp, stats);
        Scout_SpeciesSpeedRange(sp, 50, &lo, &hi);
        EXPECT_EQ(stats[STAT_SPEED], lo);
    }
    // Il setter della squadra TR ha Trick Room quasi certa.
    EXPECT_GE(Scout_MoveChanceInTeam(&tr, SPECIES_DUSCLOPS, MOVE_TRICK_ROOM), 90);
    // Lo stesso Ursaluna in una squadra Tailwind non è "da Trick Room".
    {
        u16 trStats[NUM_STATS], twStats[NUM_STATS];
        Scout_EstimateStatsInTeam(&tr, SPECIES_URSALUNA, 50, &hp, trStats);
        Scout_EstimateStatsInTeam(&tw, SPECIES_URSALUNA, 50, &hp, twStats);
        EXPECT_LE(trStats[STAT_SPEED], twStats[STAT_SPEED]);
    }
}

TEST("VGC context: a Tailwind team gives its attackers the maximum Speed")
{
    struct ScoutTeamContext tw;
    u16 hp, stats[NUM_STATS], lo, hi;

    Scout_ComputeTeamContext(sCtxTailwindTeam, ARRAY_COUNT(sCtxTailwindTeam), &tw);
    EXPECT(tw.archetype & (SCOUT_ARCH_TAILWIND | SCOUT_ARCH_FAST));
    EXPECT(tw.slot[2].flags & SCOUT_CTX_SPEED_ABUSER);
    Scout_EstimateStatsInTeam(&tw, SPECIES_GARCHOMP, 50, &hp, stats);
    Scout_SpeciesSpeedRange(SPECIES_GARCHOMP, 50, &lo, &hi);
    EXPECT_EQ(stats[STAT_SPEED], hi);
}

TEST("VGC context: a rain team gives Swift Swim to its abuser")
{
    static const u16 rain[] = { SPECIES_PELIPPER, SPECIES_LUDICOLO, SPECIES_KINGDRA, SPECIES_INCINEROAR, SPECIES_AMOONGUSS, SPECIES_GARCHOMP };
    static const u16 dry[] = { SPECIES_LUDICOLO, SPECIES_KINGDRA, SPECIES_INCINEROAR, SPECIES_AMOONGUSS, SPECIES_GARCHOMP, SPECIES_GHOLDENGO };
    struct ScoutTeamContext ctx, noRain;

    Scout_ComputeTeamContext(rain, ARRAY_COUNT(rain), &ctx);
    Scout_ComputeTeamContext(dry, ARRAY_COUNT(dry), &noRain);
    EXPECT(ctx.archetype & SCOUT_ARCH_WEATHER);
    EXPECT_EQ(ctx.weather, SCOUT_WEATHER_RAIN);
    EXPECT(ctx.slot[0].flags & SCOUT_CTX_WEATHER_SETTER);
    EXPECT(ctx.slot[1].flags & SCOUT_CTX_WEATHER_ABUSER);
    EXPECT_GE(Scout_AbilityChanceInTeam(&ctx, SPECIES_LUDICOLO, ABILITY_SWIFT_SWIM), 90);
    EXPECT_GE(Scout_AbilityChanceInTeam(&ctx, SPECIES_KINGDRA, ABILITY_SWIFT_SWIM), 90);
    EXPECT_EQ(Scout_LikelyAbilityInTeam(&ctx, SPECIES_LUDICOLO, NULL), ABILITY_SWIFT_SWIM);
    EXPECT_GE(Scout_AbilityChanceInTeam(&ctx, SPECIES_PELIPPER, ABILITY_DRIZZLE), 90);
    // Senza Drizzle in squadra resta la probabilità della specie.
    EXPECT_EQ(Scout_AbilityChanceInTeam(&noRain, SPECIES_LUDICOLO, ABILITY_SWIFT_SWIM), Scout_AbilityChance(SPECIES_LUDICOLO, ABILITY_SWIFT_SWIM));
    // Mosse da pioggia più probabili (Hurricane / Thunder / Weather Ball se le impara).
    EXPECT_GE(Scout_MoveChanceInTeam(&ctx, SPECIES_PELIPPER, MOVE_HURRICANE), Scout_MoveChance(SPECIES_PELIPPER, MOVE_HURRICANE));
}

TEST("VGC context: only one likely Mega per team")
{
    static const u16 megas[] = { SPECIES_CHARIZARD, SPECIES_GENGAR, SPECIES_SALAMENCE, SPECIES_GARCHOMP, SPECIES_KANGASKHAN, SPECIES_METAGROSS };
    struct ScoutTeamContext ctx;
    u32 i, likely = 0, flagged = 0, possible = 0;

    Scout_ComputeTeamContext(megas, ARRAY_COUNT(megas), &ctx);
    for (i = 0; i < ctx.n; i++)
    {
        struct ScoutItemGuess g;
        Scout_GuessItem(ctx.slot[i].species, &g);
        if (g.megaSpecies != SPECIES_NONE && g.megaChance >= 50)
            possible++;
        if (ctx.slot[i].megaP >= 50)
            likely++;
        if (ctx.slot[i].flags & SCOUT_CTX_MEGA)
            flagged++;
    }
    EXPECT_GE(possible, 2);   // da sole, più specie sembrerebbero Mega...
    EXPECT_LE(likely, 1);     // ...nella squadra una sola
    EXPECT_EQ(flagged, 1);
}

TEST("VGC context: item clause - the same item is not guessed twice in a team")
{
    static const u16 team[] = { SPECIES_SMEARGLE, SPECIES_CHATOT, SPECIES_DUGTRIO, SPECIES_KECLEON, SPECIES_INCINEROAR, SPECIES_AMOONGUSS };
    struct ScoutTeamContext ctx;
    u32 i, k;

    Scout_ComputeTeamContext(team, ARRAY_COUNT(team), &ctx);
    for (i = 0; i < ctx.n; i++)
        for (k = i + 1; k < ctx.n; k++)
            if (ctx.slot[i].heldEffect != HOLD_EFFECT_NONE)
                EXPECT_NE(ctx.slot[i].heldEffect, ctx.slot[k].heldEffect);
}

TEST("VGC context: likely leads pair the Trick Room setter with a slow attacker or Fake Out")
{
    struct ScoutTeamContext ctx;
    u8 pairs[3][2], p[3];
    u32 n, i;
    bool32 setterInTop = FALSE;

    Scout_ComputeTeamContext(sCtxTrickRoomTeam, ARRAY_COUNT(sCtxTrickRoomTeam), &ctx);
    n = Scout_ContextLikelyLeads(&ctx, pairs, p, 3);
    EXPECT_EQ(n, 3);
    for (i = 0; i < 2; i++)
        if (ctx.slot[pairs[0][i]].flags & SCOUT_CTX_TR_SETTER)
            setterInTop = TRUE;
    EXPECT(setterInTop);
    EXPECT_GE(p[0], p[1]);
    // Due lenti senza setter: lead poco probabile.
    EXPECT_LT(Scout_ContextLeadScore(&ctx, SPECIES_URSALUNA, SPECIES_TORKOAL), Scout_ContextLeadScore(&ctx, SPECIES_DUSCLOPS, SPECIES_URSALUNA));
}

AI_DOUBLE_BATTLE_TEST("VGC context: in battle the belief uses the team context (Trick Room team, slow Speed from turn 1)")
{
    u16 hp = 0, stats[NUM_STATS] = {0}, lo = 0, hi = 0;

    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_DUSCLOPS) { Level(50); Moves(MOVE_TRICK_ROOM, MOVE_NIGHT_SHADE); }
        PLAYER(SPECIES_URSALUNA) { Level(50); Moves(MOVE_FACADE, MOVE_PROTECT); }
        PLAYER(SPECIES_TORKOAL) { Level(50); Moves(MOVE_ERUPTION); }
        PLAYER(SPECIES_HATTERENE) { Level(50); Moves(MOVE_TRICK_ROOM); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(50); Moves(MOVE_CELEBRATE); }
        OPPONENT(SPECIES_WOBBUFFET) { Level(50); Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_NIGHT_SHADE, target: opponentLeft); MOVE(playerRight, MOVE_PROTECT); }
    } THEN {
        Scout_EstimateBattlerStats(GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT), &hp, stats);
        Scout_SpeciesSpeedRange(SPECIES_URSALUNA, 50, &lo, &hi);
        EXPECT_EQ(stats[STAT_SPEED], lo);
        EXPECT_GE(Scout_BattlerMoveChance(GetBattlerAtPosition(B_POSITION_PLAYER_LEFT), MOVE_TRICK_ROOM), 90);
    }
}

// Contesto della squadra (team preview): lead ovvia Fake Out + setter di Trick Room di una squadra Trick Room.
// Al turno 1 l'IA ferma il Trick Room (Provocazione col Prankster prima del setter) invece di attaccare.
AI_DOUBLE_BATTLE_TEST("VGC context: turn 1 against Fake Out + Trick Room setter, the AI stops Trick Room")
{
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_INCINEROAR) { Level(50); Ability(ABILITY_INTIMIDATE); Moves(MOVE_FAKE_OUT, MOVE_FLARE_BLITZ); }
        PLAYER(SPECIES_PORYGON2) { Level(50); Ability(ABILITY_DOWNLOAD); Item(ITEM_EVIOLITE); Moves(MOVE_TRICK_ROOM, MOVE_TRI_ATTACK); }
        PLAYER(SPECIES_URSALUNA) { Level(50); Moves(MOVE_FACADE); }
        PLAYER(SPECIES_TORKOAL) { Level(50); Moves(MOVE_ERUPTION); }
        OPPONENT(SPECIES_WHIMSICOTT) { Level(50); Ability(ABILITY_PRANKSTER); Moves(MOVE_TAUNT, MOVE_MOONBLAST, MOVE_PROTECT); }
        OPPONENT(SPECIES_IRON_BUNDLE) { Level(50); Moves(MOVE_FREEZE_DRY, MOVE_HYDRO_PUMP, MOVE_PROTECT); }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_FAKE_OUT, target: opponentRight); MOVE(playerRight, MOVE_TRICK_ROOM);
               EXPECT_MOVE(opponentLeft, MOVE_TAUNT, target: playerRight); }
    }
}
