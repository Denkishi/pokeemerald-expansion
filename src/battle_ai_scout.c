// Scouting della squadra del giocatore (vedi include/battle_ai_scout.h).
// Nessuna lettura dei set veri: specie + abilità possibili + learnset + tabella di meta.
#include "global.h"
#include "battle.h"
#include "battle_ai_scout.h"
#include "battle_ai_util.h"
#include "data.h"
#include "move.h"
#include "pokemon.h"
#include "constants/abilities.h"
#include "constants/battle_move_effects.h"
#include "constants/moves.h"
#include "constants/species.h"

#define SR(r) (1u << (r))
#define META_P       85  // ruolo "da meta": lo porta quasi sempre
#define LEARN_P_LOW  15  // può impararla ma è raro che la porti
#define LEARN_P_MID  30
#define LEARN_P_HIGH 45

// Cosa porta quasi sempre ogni Pokémon comune in VGC (specie base: vale per Mega e forme).
static const struct { u16 species; u16 roles; } sScoutMeta[] =
{
    { SPECIES_INCINEROAR,   SR(SCOUT_FAKE_OUT) | SR(SCOUT_SOFT_SPEED) },
    { SPECIES_RILLABOOM,    SR(SCOUT_FAKE_OUT) },
    { SPECIES_AMOONGUSS,    SR(SCOUT_REDIRECT) | SR(SCOUT_SLEEP) },
    { SPECIES_INDEEDEE,     SR(SCOUT_REDIRECT) | SR(SCOUT_TRICK_ROOM) | SR(SCOUT_HELPING_HAND) },
    { SPECIES_HATTERENE,    SR(SCOUT_TRICK_ROOM) },
    { SPECIES_PORYGON2,     SR(SCOUT_TRICK_ROOM) },
    { SPECIES_DUSCLOPS,     SR(SCOUT_TRICK_ROOM) | SR(SCOUT_HELPING_HAND) },
    { SPECIES_ORANGURU,     SR(SCOUT_TRICK_ROOM) | SR(SCOUT_HELPING_HAND) },
    { SPECIES_FARIGIRAF,    SR(SCOUT_TRICK_ROOM) | SR(SCOUT_HELPING_HAND) },
    { SPECIES_CRESSELIA,    SR(SCOUT_TRICK_ROOM) | SR(SCOUT_HELPING_HAND) },
    { SPECIES_BRONZONG,     SR(SCOUT_TRICK_ROOM) },
    { SPECIES_MIMIKYU,      SR(SCOUT_TRICK_ROOM) },
    { SPECIES_SINISTCHA,    SR(SCOUT_TRICK_ROOM) | SR(SCOUT_REDIRECT) },
    { SPECIES_WHIMSICOTT,   SR(SCOUT_TAILWIND) | SR(SCOUT_TAUNT) },
    { SPECIES_TORNADUS,     SR(SCOUT_TAILWIND) | SR(SCOUT_TAUNT) },
    { SPECIES_TALONFLAME,   SR(SCOUT_TAILWIND) },
    { SPECIES_PELIPPER,     SR(SCOUT_TAILWIND) | SR(SCOUT_WIDE_GUARD) },
    { SPECIES_KILOWATTREL,  SR(SCOUT_TAILWIND) | SR(SCOUT_SOFT_SPEED) },
    { SPECIES_AERODACTYL,   SR(SCOUT_TAILWIND) | SR(SCOUT_WIDE_GUARD) },
    { SPECIES_ALTARIA,      SR(SCOUT_TAILWIND) },
    { SPECIES_CORVIKNIGHT,  SR(SCOUT_TAILWIND) },
    { SPECIES_SUICUNE,      SR(SCOUT_TAILWIND) },
    { SPECIES_ZAPDOS,       SR(SCOUT_TAILWIND) },
    { SPECIES_MURKROW,      SR(SCOUT_TAILWIND) | SR(SCOUT_TAUNT) },
    { SPECIES_CHATOT,       SR(SCOUT_TAILWIND) | SR(SCOUT_TAUNT) },
    { SPECIES_ROARING_MOON, SR(SCOUT_TAILWIND) | SR(SCOUT_SETUP) },
    { SPECIES_DRAGONITE,    SR(SCOUT_TAILWIND) | SR(SCOUT_SETUP) },
    { SPECIES_GRIMMSNARL,   SR(SCOUT_TAUNT) },
    { SPECIES_SABLEYE,      SR(SCOUT_FAKE_OUT) | SR(SCOUT_TAUNT) },
    { SPECIES_KECLEON,      SR(SCOUT_FAKE_OUT) | SR(SCOUT_TAUNT) | SR(SCOUT_HELPING_HAND) },
    { SPECIES_HARIYAMA,     SR(SCOUT_FAKE_OUT) },
    { SPECIES_LUDICOLO,     SR(SCOUT_FAKE_OUT) },
    { SPECIES_MIENSHAO,     SR(SCOUT_FAKE_OUT) | SR(SCOUT_WIDE_GUARD) },
    { SPECIES_WEAVILE,      SR(SCOUT_FAKE_OUT) },
    { SPECIES_MEOWSCARADA,  SR(SCOUT_FAKE_OUT) },
    { SPECIES_SCRAFTY,      SR(SCOUT_FAKE_OUT) },
    { SPECIES_IRON_HANDS,   SR(SCOUT_FAKE_OUT) },
    { SPECIES_SMEARGLE,     SR(SCOUT_SLEEP) | SR(SCOUT_REDIRECT) | SR(SCOUT_FAKE_OUT) },
    { SPECIES_TOGEKISS,     SR(SCOUT_REDIRECT) | SR(SCOUT_TAILWIND) },
    { SPECIES_CLEFAIRY,     SR(SCOUT_REDIRECT) | SR(SCOUT_HELPING_HAND) },
    { SPECIES_CLEFABLE,     SR(SCOUT_REDIRECT) },
    { SPECIES_MAUSHOLD,     SR(SCOUT_REDIRECT) },
    { SPECIES_BRUTE_BONNET, SR(SCOUT_REDIRECT) | SR(SCOUT_SLEEP) },
    { SPECIES_TOEDSCRUEL,   SR(SCOUT_REDIRECT) | SR(SCOUT_SLEEP) },
    { SPECIES_BRELOOM,      SR(SCOUT_SLEEP) },
    { SPECIES_VENUSAUR,     SR(SCOUT_SLEEP) },
    { SPECIES_LILLIGANT,    SR(SCOUT_SLEEP) | SR(SCOUT_HELPING_HAND) },
    { SPECIES_GENGAR,       SR(SCOUT_SLEEP) | SR(SCOUT_PERISH) },
    { SPECIES_POLITOED,     SR(SCOUT_PERISH) | SR(SCOUT_HELPING_HAND) | SR(SCOUT_SOFT_SPEED) },
    { SPECIES_ROTOM,        SR(SCOUT_SOFT_SPEED) },
    { SPECIES_IRON_BUNDLE,  SR(SCOUT_SOFT_SPEED) },
    { SPECIES_ARMAROUGE,    SR(SCOUT_WIDE_GUARD) },
    { SPECIES_GALLADE,      SR(SCOUT_WIDE_GUARD) | SR(SCOUT_SETUP) },
    { SPECIES_MILTANK,      SR(SCOUT_HELPING_HAND) },
    { SPECIES_URSHIFU,      SR(SCOUT_SETUP) },
    { SPECIES_PALAFIN,      SR(SCOUT_SETUP) },
    { SPECIES_GHOLDENGO,    SR(SCOUT_SETUP) },
};

// Mosse che definiscono un ruolo.
bool32 Scout_MoveHasRole(enum Move move, enum ScoutRole role)
{
    enum BattleMoveEffects effect;

    if (move == MOVE_NONE || move == MOVE_UNAVAILABLE)
        return FALSE;
    effect = GetMoveEffect(move);
    switch (role)
    {
    case SCOUT_TRICK_ROOM:
        return effect == EFFECT_TRICK_ROOM;
    case SCOUT_TAILWIND:
        return effect == EFFECT_TAILWIND;
    case SCOUT_SOFT_SPEED:
        return move == MOVE_ICY_WIND || move == MOVE_ELECTROWEB || move == MOVE_THUNDER_WAVE
            || move == MOVE_SCARY_FACE || move == MOVE_BULLDOZE;
    case SCOUT_FAKE_OUT:
        return effect == EFFECT_FIRST_TURN_ONLY;
    case SCOUT_REDIRECT:
        return effect == EFFECT_FOLLOW_ME;
    case SCOUT_TAUNT:
        return effect == EFFECT_TAUNT;
    case SCOUT_SLEEP:
        return GetMoveNonVolatileStatus(move) == MOVE_EFFECT_SLEEP || effect == EFFECT_YAWN;
    case SCOUT_PERISH:
        return effect == EFFECT_PERISH_SONG;
    case SCOUT_WIDE_GUARD:
        return effect == EFFECT_PROTECT && GetMoveProtectMethod(move) == PROTECT_WIDE_GUARD;
    case SCOUT_SETUP:
        return effect == EFFECT_BELLY_DRUM || (IsBattleMoveStatus(move) && IsStatRaisingMove(move));
    case SCOUT_HELPING_HAND:
        return effect == EFFECT_HELPING_HAND;
    default:
        return FALSE;
    }
}

// Mosse "rappresentative" di ogni ruolo e quanto è probabile che le porti chi le impara.
static const struct { u16 move; u8 role; u8 p; } sScoutLearnable[] =
{
    { MOVE_TRICK_ROOM,   SCOUT_TRICK_ROOM,   LEARN_P_MID },
    { MOVE_TAILWIND,     SCOUT_TAILWIND,     LEARN_P_MID },
    { MOVE_ICY_WIND,     SCOUT_SOFT_SPEED,   LEARN_P_LOW },
    { MOVE_ELECTROWEB,   SCOUT_SOFT_SPEED,   LEARN_P_LOW },
    { MOVE_THUNDER_WAVE, SCOUT_SOFT_SPEED,   LEARN_P_LOW },
    { MOVE_FAKE_OUT,     SCOUT_FAKE_OUT,     LEARN_P_HIGH },
    { MOVE_FOLLOW_ME,    SCOUT_REDIRECT,     LEARN_P_HIGH },
    { MOVE_RAGE_POWDER,  SCOUT_REDIRECT,     LEARN_P_HIGH },
    { MOVE_TAUNT,        SCOUT_TAUNT,        LEARN_P_LOW },
    { MOVE_SPORE,        SCOUT_SLEEP,        60 },
    { MOVE_SLEEP_POWDER, SCOUT_SLEEP,        LEARN_P_MID },
    { MOVE_HYPNOSIS,     SCOUT_SLEEP,        LEARN_P_LOW },
    { MOVE_YAWN,         SCOUT_SLEEP,        LEARN_P_LOW },
    { MOVE_PERISH_SONG,  SCOUT_PERISH,       LEARN_P_MID },
    { MOVE_WIDE_GUARD,   SCOUT_WIDE_GUARD,   LEARN_P_MID },
    { MOVE_HELPING_HAND, SCOUT_HELPING_HAND, LEARN_P_LOW },
    { MOVE_SWORDS_DANCE, SCOUT_SETUP,        LEARN_P_LOW },
    { MOVE_DRAGON_DANCE, SCOUT_SETUP,        LEARN_P_MID },
    { MOVE_NASTY_PLOT,   SCOUT_SETUP,        LEARN_P_LOW },
    { MOVE_BELLY_DRUM,   SCOUT_SETUP,        LEARN_P_MID },
    { MOVE_QUIVER_DANCE, SCOUT_SETUP,        LEARN_P_MID },
    { MOVE_SHELL_SMASH,  SCOUT_SETUP,        LEARN_P_MID },
    { MOVE_IRON_DEFENSE, SCOUT_SETUP,        LEARN_P_LOW },
};

static bool32 LevelUpHas(u16 species, enum Move move)
{
    const struct LevelUpMove *l = GetSpeciesLevelUpLearnset(species);
    u32 i;

    if (l == NULL)
        return FALSE;
    for (i = 0; l[i].move != LEVEL_UP_MOVE_END; i++)
    {
        if (l[i].move == move)
            return TRUE;
    }
    return FALSE;
}

static bool32 EggHas(u16 species, enum Move move)
{
    const u16 *e = GetSpeciesEggMoves(species);
    u32 i;

    if (e == NULL)
        return FALSE;
    for (i = 0; e[i] != MOVE_UNAVAILABLE; i++)
    {
        if (e[i] == move)
            return TRUE;
    }
    return FALSE;
}

static bool32 SpeciesCanLearn(u16 species, enum Move move)
{
    u16 base = GET_BASE_SPECIES_ID(species);

    if (LevelUpHas(species, move) || CanLearnTeachableMove(species, move) || EggHas(species, move))
        return TRUE;
    return base != species && (LevelUpHas(base, move) || CanLearnTeachableMove(base, move) || EggHas(base, move));
}

// Probabilità che la specie abbia una di queste abilità: tutte le sue abilità = 100, altrimenti
// in proporzione, con un minimo di 40 (chi può averla di solito la usa).
static u32 AbilityChance(u16 species, const enum Ability *list, u32 n)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[species];
    u32 i, k, total = 0, hit = 0;

    for (i = 0; i < NUM_ABILITY_SLOTS; i++)
    {
        enum Ability a = info->abilities[i];
        bool32 dup = FALSE;

        if (a == ABILITY_NONE)
            continue;
        for (k = 0; k < i; k++)
        {
            if (info->abilities[k] == a)
                dup = TRUE;
        }
        if (dup)
            continue;
        total++;
        for (k = 0; k < n; k++)
        {
            if (list[k] == a)
            {
                hit++;
                break;
            }
        }
    }
    if (hit == 0 || total == 0)
        return 0;
    return max(100 * hit / total, 40u);
}

static const enum Ability sSunSet[]   = { ABILITY_DROUGHT, ABILITY_ORICHALCUM_PULSE, ABILITY_DESOLATE_LAND };
static const enum Ability sRainSet[]  = { ABILITY_DRIZZLE, ABILITY_PRIMORDIAL_SEA };
static const enum Ability sSandSet[]  = { ABILITY_SAND_STREAM, ABILITY_SAND_SPIT };
static const enum Ability sSnowSet[]  = { ABILITY_SNOW_WARNING };
static const enum Ability sSunUse[]   = { ABILITY_CHLOROPHYLL, ABILITY_SOLAR_POWER, ABILITY_PROTOSYNTHESIS, ABILITY_FLOWER_GIFT, ABILITY_LEAF_GUARD };
static const enum Ability sRainUse[]  = { ABILITY_SWIFT_SWIM, ABILITY_RAIN_DISH, ABILITY_HYDRATION, ABILITY_DRY_SKIN };
static const enum Ability sSandUse[]  = { ABILITY_SAND_RUSH, ABILITY_SAND_FORCE, ABILITY_SAND_VEIL };
static const enum Ability sSnowUse[]  = { ABILITY_SLUSH_RUSH, ABILITY_ICE_BODY, ABILITY_SNOW_CLOAK };
static const enum Ability sGrassy[]   = { ABILITY_GRASSY_SURGE };
static const enum Ability sElectric[] = { ABILITY_ELECTRIC_SURGE, ABILITY_HADRON_ENGINE };
static const enum Ability sPsychic[]  = { ABILITY_PSYCHIC_SURGE };
static const enum Ability sMisty[]    = { ABILITY_MISTY_SURGE };
static const enum Ability sIntim[]    = { ABILITY_INTIMIDATE };
static const enum Ability sNoSleep[]  = { ABILITY_INSOMNIA, ABILITY_VITAL_SPIRIT, ABILITY_SWEET_VEIL, ABILITY_GOOD_AS_GOLD,
                                          ABILITY_MAGIC_BOUNCE, ABILITY_PURIFYING_SALT, ABILITY_COMATOSE };

void Scout_GetSpeciesProfile(u16 species, struct ScoutProfile *out)
{
    const struct SpeciesInfo *info;
    u32 i, p;
    u16 base;

    memset(out, 0, sizeof(*out));
    if (species == SPECIES_NONE || species >= NUM_SPECIES)
        return;
    info = &gSpeciesInfo[species];
    base = GET_BASE_SPECIES_ID(species);
    out->baseSpeed = info->baseSpeed;
    out->physical = info->baseAttack > info->baseSpAttack;

    // 1. Learnset: "può impararla".
    for (i = 0; i < ARRAY_COUNT(sScoutLearnable); i++)
    {
        if (out->role[sScoutLearnable[i].role] >= sScoutLearnable[i].p)
            continue;
        if (SpeciesCanLearn(species, sScoutLearnable[i].move))
            out->role[sScoutLearnable[i].role] = sScoutLearnable[i].p;
    }
    // 2. Meta: lo porta quasi sempre.
    for (i = 0; i < ARRAY_COUNT(sScoutMeta); i++)
    {
        if (GET_BASE_SPECIES_ID(sScoutMeta[i].species) == base)
        {
            u32 r;
            for (r = 0; r < SCOUT_ROLE_COUNT; r++)
            {
                if (sScoutMeta[i].roles & SR(r))
                    out->role[r] = META_P;
            }
            break;
        }
    }
    // 3. Abilità.
    if ((p = AbilityChance(species, sSunSet, ARRAY_COUNT(sSunSet))) != 0)
    {
        out->weather = SCOUT_WEATHER_SUN;
        out->weatherP = p;
    }
    else if ((p = AbilityChance(species, sRainSet, ARRAY_COUNT(sRainSet))) != 0)
    {
        out->weather = SCOUT_WEATHER_RAIN;
        out->weatherP = p;
    }
    else if ((p = AbilityChance(species, sSandSet, ARRAY_COUNT(sSandSet))) != 0)
    {
        out->weather = SCOUT_WEATHER_SAND;
        out->weatherP = p;
    }
    else if ((p = AbilityChance(species, sSnowSet, ARRAY_COUNT(sSnowSet))) != 0)
    {
        out->weather = SCOUT_WEATHER_SNOW;
        out->weatherP = p;
    }

    if (AbilityChance(species, sSunUse, ARRAY_COUNT(sSunUse)) >= 40)
        out->weatherAbuse = SCOUT_WEATHER_SUN;
    else if (AbilityChance(species, sRainUse, ARRAY_COUNT(sRainUse)) >= 40)
        out->weatherAbuse = SCOUT_WEATHER_RAIN;
    else if (AbilityChance(species, sSandUse, ARRAY_COUNT(sSandUse)) >= 40)
        out->weatherAbuse = SCOUT_WEATHER_SAND;
    else if (AbilityChance(species, sSnowUse, ARRAY_COUNT(sSnowUse)) >= 40)
        out->weatherAbuse = SCOUT_WEATHER_SNOW;

    if ((p = AbilityChance(species, sGrassy, ARRAY_COUNT(sGrassy))) != 0)
    {
        out->terrain = SCOUT_TERRAIN_GRASSY;
        out->terrainP = p;
    }
    else if ((p = AbilityChance(species, sElectric, ARRAY_COUNT(sElectric))) != 0)
    {
        out->terrain = SCOUT_TERRAIN_ELECTRIC;
        out->terrainP = p;
    }
    else if ((p = AbilityChance(species, sPsychic, ARRAY_COUNT(sPsychic))) != 0)
    {
        out->terrain = SCOUT_TERRAIN_PSYCHIC;
        out->terrainP = p;
    }
    else if ((p = AbilityChance(species, sMisty, ARRAY_COUNT(sMisty))) != 0)
    {
        out->terrain = SCOUT_TERRAIN_MISTY;
        out->terrainP = p;
    }

    out->intimidateP = AbilityChance(species, sIntim, ARRAY_COUNT(sIntim));
    out->sleepImmuneP = AbilityChance(species, sNoSleep, ARRAY_COUNT(sNoSleep));
    if (info->types[0] == TYPE_GRASS || info->types[1] == TYPE_GRASS)
        out->sleepImmuneP = max(out->sleepImmuneP, 50); // immune alle polveri (Spora), non a Ipnosi
}

u32 Scout_BattlerRoleChance(enum BattlerId battler, enum ScoutRole role)
{
    struct ScoutProfile prof;
    enum Move *moves = GetMovesArray(battler); // per il giocatore: solo le mosse viste
    u32 i, seen = 0;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] == MOVE_NONE || moves[i] == MOVE_UNAVAILABLE)
            continue;
        seen++;
        if (Scout_MoveHasRole(moves[i], role))
            return 100;
    }
    if (seen >= MAX_MON_MOVES)
        return 0;
    Scout_GetSpeciesProfile(gBattleMons[battler].species, &prof);
    // Ogni mossa vista che non è del ruolo lo rende un po' meno probabile.
    return prof.role[role] * (MAX_MON_MOVES - seen) / MAX_MON_MOVES;
}

// ---------------------------------------------------------------------
// Strumenti: lista del formato + specie -> strumento probabile
// ---------------------------------------------------------------------
#include "item.h"
#include "constants/hold_effects.h"
#include "constants/items.h"
#include "constants/form_change_types.h"
#include "data/vgc_item_pool.h"

bool32 Scout_PoolHasItem(u16 item)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sVgcItemPool); i++)
    {
        if (sVgcItemPool[i] == item)
            return TRUE;
    }
    return FALSE;
}

static bool32 PoolHasEffect(u16 effect)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sVgcItemPool); i++)
    {
        if (GetItemHoldEffect(sVgcItemPool[i]) == effect)
            return TRUE;
    }
    return FALSE;
}

u16 Scout_MegaTargetSpecies(u16 species)
{
    const struct FormChange *fc;
    u32 i;

    if (species == SPECIES_NONE || species >= NUM_SPECIES)
        return SPECIES_NONE;
    fc = GetSpeciesFormChanges(species);
    if (fc == NULL)
        return SPECIES_NONE;
    for (i = 0; fc[i].method != FORM_CHANGE_TERMINATOR; i++)
    {
        if (fc[i].method == FORM_CHANGE_BATTLE_MEGA_EVOLUTION_ITEM && Scout_PoolHasItem(fc[i].param1))
            return fc[i].targetSpecies;
    }
    return SPECIES_NONE;
}

static bool32 SpeciesIsNfe(u16 species)
{
    const struct Evolution *evo = GetSpeciesEvolutions(species);
    return evo != NULL && evo[0].method != EVOLUTIONS_END && evo[0].method != EVO_NONE;
}

// Strumenti "da meta" per specie (specie base). Contano solo se presenti nella lista del formato.
static const struct { u16 species; u16 effect; u8 chance; } sScoutItemMeta[] =
{
    { SPECIES_SMEARGLE,     HOLD_EFFECT_FOCUS_SASH,   85 },
    { SPECIES_CHATOT,       HOLD_EFFECT_FOCUS_SASH,   65 },
    { SPECIES_PELIPPER,     HOLD_EFFECT_FOCUS_SASH,   55 },
    { SPECIES_DUGTRIO,      HOLD_EFFECT_FOCUS_SASH,   65 },
    { SPECIES_KECLEON,      HOLD_EFFECT_FOCUS_SASH,   60 },
    { SPECIES_CERULEDGE,    HOLD_EFFECT_FOCUS_SASH,   55 },
    { SPECIES_AMOONGUSS,    HOLD_EFFECT_ROCKY_HELMET, 50 },
    { SPECIES_INCINEROAR,   HOLD_EFFECT_RESTORE_HP,   55 },
    { SPECIES_RILLABOOM,    HOLD_EFFECT_ASSAULT_VEST, 55 },
    { SPECIES_DONDOZO,      HOLD_EFFECT_ASSAULT_VEST, 50 },
    { SPECIES_GOODRA,       HOLD_EFFECT_ASSAULT_VEST, 60 },
    { SPECIES_ARCANINE,     HOLD_EFFECT_ASSAULT_VEST, 45 },
    { SPECIES_SLITHER_WING, HOLD_EFFECT_ASSAULT_VEST, 45 },
    { SPECIES_LUDICOLO,     HOLD_EFFECT_ASSAULT_VEST, 45 },
    { SPECIES_TATSUGIRI,    HOLD_EFFECT_CHOICE_SCARF, 60 },
    { SPECIES_ROTOM,        HOLD_EFFECT_CHOICE_SCARF, 40 },
    { SPECIES_ANNIHILAPE,   HOLD_EFFECT_CHOICE_SCARF, 45 },
    { SPECIES_KINGAMBIT,    HOLD_EFFECT_LIFE_ORB,     50 },
    { SPECIES_GHOLDENGO,    HOLD_EFFECT_LIFE_ORB,     50 },
    { SPECIES_ARMAROUGE,    HOLD_EFFECT_LIFE_ORB,     50 },
    { SPECIES_GOLDUCK,      HOLD_EFFECT_LIFE_ORB,     45 },
    { SPECIES_PORYGON_Z,    HOLD_EFFECT_LIFE_ORB,     50 },
    { SPECIES_SLAKING,      HOLD_EFFECT_LIFE_ORB,     50 },
    { SPECIES_PORYGON2,     HOLD_EFFECT_EVIOLITE,     85 },
    { SPECIES_DUSCLOPS,     HOLD_EFFECT_EVIOLITE,     85 },
    { SPECIES_LICKITUNG,    HOLD_EFFECT_EVIOLITE,     80 },
    { SPECIES_CLEFAIRY,     HOLD_EFFECT_EVIOLITE,     80 },
    { SPECIES_GRIMMSNARL,   HOLD_EFFECT_LIGHT_CLAY,   70 },
    { SPECIES_INDEEDEE,     HOLD_EFFECT_MENTAL_HERB,  45 },
    { SPECIES_MIMIKYU,      HOLD_EFFECT_MENTAL_HERB,  45 },
    { SPECIES_SABLEYE,      HOLD_EFFECT_MENTAL_HERB,  45 },
    { SPECIES_WEEZING,      HOLD_EFFECT_RESTORE_HP,   50 },
    { SPECIES_ORANGURU,     HOLD_EFFECT_RESTORE_HP,   50 },
    { SPECIES_TOEDSCRUEL,   HOLD_EFFECT_RESTORE_HP,   45 },
    { SPECIES_SNORLAX,      HOLD_EFFECT_RESTORE_HP,   55 },
    { SPECIES_VENUSAUR,     HOLD_EFFECT_RESTORE_HP,   45 },
    { SPECIES_MACHAMP,      HOLD_EFFECT_FLAME_ORB,    55 },
    { SPECIES_HARIYAMA,     HOLD_EFFECT_FLAME_ORB,    55 },
    { SPECIES_GARGANACL,    HOLD_EFFECT_LEFTOVERS,    55 },
    { SPECIES_COPPERAJAH,   HOLD_EFFECT_LEFTOVERS,    45 },
    { SPECIES_LAPRAS,       HOLD_EFFECT_LEFTOVERS,    45 },
    { SPECIES_MILTANK,      HOLD_EFFECT_LEFTOVERS,    45 },
    { SPECIES_ARMALDO,      HOLD_EFFECT_CLEAR_AMULET, 45 },
};

void Scout_GuessItem(u16 species, struct ScoutItemGuess *out)
{
    const struct SpeciesInfo *info;
    struct ScoutProfile prof;
    u16 base, mega;
    u32 i, atk, spa, spd, bulk;
    bool32 found = FALSE;

    memset(out, 0, sizeof(*out));
    out->effect = HOLD_EFFECT_NONE;
    if (species == SPECIES_NONE || species >= NUM_SPECIES)
        return;
    info = &gSpeciesInfo[species];
    base = GET_BASE_SPECIES_ID(species);

    // Mega: una pietra in lista e una forma Mega raggiungibile.
    mega = Scout_MegaTargetSpecies(species);
    if (mega != SPECIES_NONE)
    {
        out->megaSpecies = mega;
        out->megaChance = 70;
    }

    // 1. Meta per specie.
    for (i = 0; i < ARRAY_COUNT(sScoutItemMeta); i++)
    {
        if (GET_BASE_SPECIES_ID(sScoutItemMeta[i].species) == base && PoolHasEffect(sScoutItemMeta[i].effect))
        {
            out->effect = sScoutItemMeta[i].effect;
            out->chance = sScoutItemMeta[i].chance;
            found = TRUE;
            break;
        }
    }

    // 2. Regole generali dalla specie.
    if (!found)
    {
        atk = info->baseAttack;
        spa = info->baseSpAttack;
        spd = info->baseSpeed;
        bulk = info->baseHP + info->baseDefense + info->baseSpDefense;
        Scout_GetSpeciesProfile(species, &prof);

        if (SpeciesIsNfe(species) && PoolHasEffect(HOLD_EFFECT_EVIOLITE) && bulk >= 180)
        {
            out->effect = HOLD_EFFECT_EVIOLITE;
            out->chance = 65;
        }
        else if (PoolHasEffect(HOLD_EFFECT_BOOSTER_ENERGY)
              && (info->abilities[0] == ABILITY_PROTOSYNTHESIS || info->abilities[0] == ABILITY_QUARK_DRIVE))
        {
            out->effect = HOLD_EFFECT_BOOSTER_ENERGY;
            out->chance = 55;
        }
        else if (PoolHasEffect(HOLD_EFFECT_FOCUS_SASH) && spd >= 95 && bulk <= 230)
        {
            out->effect = HOLD_EFFECT_FOCUS_SASH;
            out->chance = 50;
        }
        else if (PoolHasEffect(HOLD_EFFECT_RESTORE_HP)
              && (prof.role[SCOUT_REDIRECT] >= 60 || prof.role[SCOUT_TRICK_ROOM] >= 60 || prof.role[SCOUT_FAKE_OUT] >= 60))
        {
            out->effect = HOLD_EFFECT_RESTORE_HP;
            out->chance = 45;
        }
        else if (PoolHasEffect(HOLD_EFFECT_CHOICE_SCARF) && spd >= 70 && spd <= 110 && max(atk, spa) >= 110 && prof.role[SCOUT_SETUP] < 60)
        {
            out->effect = HOLD_EFFECT_CHOICE_SCARF;
            out->chance = 35;
        }
        else if (PoolHasEffect(HOLD_EFFECT_ASSAULT_VEST) && info->baseHP >= 80 && max(atk, spa) >= 100 && spd <= 90)
        {
            out->effect = HOLD_EFFECT_ASSAULT_VEST;
            out->chance = 40;
        }
        else if (PoolHasEffect(HOLD_EFFECT_LIFE_ORB) && max(atk, spa) >= 100 && spd >= 85)
        {
            out->effect = HOLD_EFFECT_LIFE_ORB;
            out->chance = 35;
        }
        else if (PoolHasEffect(HOLD_EFFECT_LEFTOVERS) && bulk >= 250)
        {
            out->effect = HOLD_EFFECT_LEFTOVERS;
            out->chance = 30;
        }
    }

    // Chi può megaevolvere di solito tiene la pietra: lo strumento "normale" diventa improbabile.
    if (out->megaSpecies != SPECIES_NONE && out->chance < 70)
        out->chance = out->chance * 40 / 100;
}

enum HoldEffect Scout_AssumedHoldEffect(enum BattlerId battler)
{
    struct ScoutItemGuess g;
    u32 threshold;

    Scout_GuessItem(gBattleMons[battler].species, &g);
    if (g.effect == HOLD_EFFECT_NONE)
        return HOLD_EFFECT_NONE;
    // Assumere un Focus Sash che non c'è rende timidi: serve più certezza.
    threshold = (g.effect == HOLD_EFFECT_FOCUS_SASH) ? 60 : 50;
    if (g.chance < threshold)
        return HOLD_EFFECT_NONE;
    // Il Focus Sash conta solo a PS pieni.
    if (g.effect == HOLD_EFFECT_FOCUS_SASH && gBattleMons[battler].hp < gBattleMons[battler].maxHP)
        return HOLD_EFFECT_NONE;
    return g.effect;
}
