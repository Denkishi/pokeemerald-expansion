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

// Base di conoscenza VGC generata da statistiche d'uso Showdown (tools/vgc_meta/gen_vgc_meta.py).
#include "data/vgc_meta.h"

#define META_UNLISTED_P  3   // mossa imparabile ma assente dalle più usate della specie

// Voce della tabella meta per una specie (forma esatta). Una Mega usa la voce della forma base
// (le spread e le mosse sono le stesse; statistiche e abilità della Mega vengono dal motore).
static const struct VgcMetaEntry *MetaFind(u16 species)
{
    s32 lo = 0, hi = ARRAY_COUNT(sVgcMeta) - 1;

    while (lo <= hi)
    {
        s32 mid = (lo + hi) / 2;
        if (sVgcMeta[mid].species == species)
            return &sVgcMeta[mid];
        if (sVgcMeta[mid].species < species)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return NULL;
}

static const struct VgcMetaEntry *MetaEntryFor(u16 species)
{
    const struct VgcMetaEntry *e;
    u32 i, k;

    if (species == SPECIES_NONE || species >= NUM_SPECIES)
        return NULL;
    e = MetaFind(species);
    if (e != NULL || !gSpeciesInfo[species].isMegaEvolution)
        return e;
    for (i = 0; i < ARRAY_COUNT(sVgcMeta); i++)
    {
        for (k = 0; k < VGC_META_MEGAS; k++)
        {
            if (sVgcMeta[i].megas[k] == species)
                return &sVgcMeta[i];
        }
    }
    return NULL;
}

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
    const struct VgcMetaEntry *meta = info->isMegaEvolution ? NULL : MetaEntryFor(species);
    u32 i, k, total = 0, hit = 0;

    // Con le statistiche d'uso vale la frequenza vera (Incineroar: Intimidate 100%, Gyarados: Intimidate o Moxie...).
    if (meta != NULL && meta->abilities[0] != ABILITY_NONE)
    {
        for (i = 0; i < VGC_META_ABILITIES && meta->abilities[i] != ABILITY_NONE; i++)
        {
            for (k = 0; k < n; k++)
            {
                if (list[k] == meta->abilities[i])
                {
                    hit += meta->abilityP[i];
                    break;
                }
            }
        }
        return min(hit, 100u);
    }

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

// Cache dei profili e degli attacchi imparabili: dipendono solo dalla specie (dati in ROM), ma calcolarli
// scorre interi learnset, e il pianificatore li chiede in ogni simulazione. In lotta ci sono al massimo
// 12 specie: le ultime usate restano qui (sostituzione circolare).
#define SCOUT_CACHE_SIZE    12
#define SCOUT_CACHE_MOVES   16

struct ScoutCacheEntry
{
    struct ScoutProfile prof;
    enum Move attacks[SCOUT_CACHE_MOVES];
    u8 nAttacks;
    u8 hasProf:1;
    u8 hasAttacks:1;
};

static EWRAM_DATA struct ScoutCacheEntry sScoutCache[SCOUT_CACHE_SIZE] = {0};
static EWRAM_DATA u8 sScoutCacheNext = 0;

static struct ScoutCacheEntry *ScoutCacheGet(u16 species)
{
    u32 i;
    struct ScoutCacheEntry *e;

    for (i = 0; i < SCOUT_CACHE_SIZE; i++)
    {
        if ((sScoutCache[i].hasProf || sScoutCache[i].hasAttacks) && sScoutCache[i].prof.species == species)
            return &sScoutCache[i];
    }
    e = &sScoutCache[sScoutCacheNext];
    sScoutCacheNext = (sScoutCacheNext + 1) % SCOUT_CACHE_SIZE;
    memset(e, 0, sizeof(*e));
    e->prof.species = species;
    return e;
}

static void ComputeSpeciesProfile(u16 species, struct ScoutProfile *out);
static u32 ComputeLearnableAttacks(u16 species, enum Move *out, u32 max);

void Scout_GetSpeciesProfile(u16 species, struct ScoutProfile *out)
{
    struct ScoutCacheEntry *e;

    if (species == SPECIES_NONE || species >= NUM_SPECIES)
    {
        memset(out, 0, sizeof(*out));
        return;
    }
    e = ScoutCacheGet(species);
    if (!e->hasProf)
    {
        ComputeSpeciesProfile(species, &e->prof);
        e->hasProf = TRUE;
    }
    *out = e->prof;
}

u32 Scout_CollectLearnableAttacks(u16 species, enum Move *out, u32 max)
{
    struct ScoutCacheEntry *e;
    u32 n;

    if (species == SPECIES_NONE || species >= NUM_SPECIES)
        return 0;
    e = ScoutCacheGet(species);
    if (!e->hasAttacks)
    {
        e->nAttacks = ComputeLearnableAttacks(species, e->attacks, SCOUT_CACHE_MOVES);
        e->hasAttacks = TRUE;
    }
    n = min(e->nAttacks, max);
    memcpy(out, e->attacks, n * sizeof(enum Move));
    return n;
}

static void ComputeSpeciesProfile(u16 species, struct ScoutProfile *out)
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
    out->species = species;

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
    // 2b. Statistiche d'uso: la probabilità di un ruolo è quella delle sue mosse nei set reali.
    //     Il "può impararla" scende (le mosse più usate sono già in tabella), la tabella a mano resta un minimo.
    {
        const struct VgcMetaEntry *meta = MetaEntryFor(species);
        if (meta != NULL)
        {
            u32 r, k, metaRole[SCOUT_ROLE_COUNT] = {0};
            bool32 hand[SCOUT_ROLE_COUNT] = {0};

            for (i = 0; i < ARRAY_COUNT(sScoutMeta); i++)
            {
                if (GET_BASE_SPECIES_ID(sScoutMeta[i].species) == base)
                {
                    for (r = 0; r < SCOUT_ROLE_COUNT; r++)
                        hand[r] = (sScoutMeta[i].roles & SR(r)) != 0;
                    break;
                }
            }
            for (k = 0; k < VGC_META_MOVES && meta->moves[k] != MOVE_NONE; k++)
            {
                for (r = 0; r < SCOUT_ROLE_COUNT; r++)
                {
                    if (Scout_MoveHasRole(meta->moves[k], r))
                        metaRole[r] = min(100u, metaRole[r] + meta->moveP[k]);
                }
            }
            for (r = 0; r < SCOUT_ROLE_COUNT; r++)
            {
                u32 v = max(metaRole[r], (u32)min(out->role[r], META_UNLISTED_P * 3));
                if (hand[r])
                    v = max(v, (u32)META_P);
                out->role[r] = v;
            }
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

static void CollectAttack(enum Move move, enum Move *out, u32 *n, u32 max)
{
    u32 i;
    enum Type type;

    if (move == MOVE_NONE || move == MOVE_UNAVAILABLE || IsBattleMoveStatus(move) || GetMovePower(move) < 50)
        return;
    type = GetMoveType(move);
    for (i = 0; i < *n; i++)
    {
        if (GetMoveType(out[i]) == type)
        {
            if (GetMovePower(move) > GetMovePower(out[i]))
                out[i] = move;
            return;
        }
    }
    if (*n < max)
        out[(*n)++] = move;
}

static u32 ComputeLearnableAttacks(u16 species, enum Move *out, u32 max)
{
    u32 n = 0, i, pass;
    u16 list[2];

    if (species == SPECIES_NONE || species >= NUM_SPECIES)
        return 0;
    list[0] = species;
    list[1] = GET_BASE_SPECIES_ID(species);
    for (pass = 0; pass < 2; pass++)
    {
        const struct LevelUpMove *l;
        const u16 *t, *e;

        if (pass == 1 && list[1] == list[0])
            break;
        l = GetSpeciesLevelUpLearnset(list[pass]);
        if (l != NULL)
            for (i = 0; l[i].move != LEVEL_UP_MOVE_END; i++)
                CollectAttack(l[i].move, out, &n, max);
        t = GetSpeciesTeachableLearnset(list[pass]);
        if (t != NULL)
            for (i = 0; t[i] != MOVE_UNAVAILABLE; i++)
                CollectAttack(t[i], out, &n, max);
        e = GetSpeciesEggMoves(list[pass]);
        if (e != NULL)
            for (i = 0; e[i] != MOVE_UNAVAILABLE; i++)
                CollectAttack(e[i], out, &n, max);
    }
    return n;
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
    // Prima la Mega più usata (Charizard: Y molto più di X), se la sua pietra è nella lista del formato.
    {
        const struct VgcMetaEntry *meta = gSpeciesInfo[species].isMegaEvolution ? NULL : MetaEntryFor(species);
        u32 k;
        for (k = 0; meta != NULL && k < VGC_META_MEGAS && meta->megas[k] != SPECIES_NONE; k++)
        {
            for (i = 0; fc[i].method != FORM_CHANGE_TERMINATOR; i++)
            {
                if (fc[i].method == FORM_CHANGE_BATTLE_MEGA_EVOLUTION_ITEM && fc[i].targetSpecies == meta->megas[k]
                 && Scout_PoolHasItem(fc[i].param1))
                    return fc[i].targetSpecies;
            }
        }
    }
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

    // 0. Statistiche d'uso: Mega e strumenti con la loro frequenza (solo strumenti della lista del formato).
    {
        const struct VgcMetaEntry *meta = info->isMegaEvolution ? NULL : MetaEntryFor(species);
        if (meta != NULL)
        {
            u32 k, megaP = 0;
            for (k = 0; k < VGC_META_MEGAS && meta->megas[k] != SPECIES_NONE; k++)
            {
                if (meta->megas[k] == mega)
                    megaP = meta->megaP[k];
            }
            if (mega != SPECIES_NONE)
                out->megaChance = megaP;
            for (k = 0; k < VGC_META_ITEMS && meta->items[k] != ITEM_NONE; k++)
            {
                u32 p = meta->itemP[k];
                u16 eff = GetItemHoldEffect(meta->items[k]);
                if (eff == HOLD_EFFECT_NONE || !Scout_PoolHasItem(meta->items[k]))
                    continue;
                // Senza la sua Mega (pietra fuori lista) gli strumenti "normali" si spartiscono anche quella quota.
                if (mega == SPECIES_NONE && megaP == 0)
                {
                    u32 m, all = 0;
                    for (m = 0; m < VGC_META_MEGAS; m++)
                        all += meta->megaP[m];
                    if (all < 100)
                        p = min(100u, p * 100 / (100 - all));
                }
                out->effect = eff;
                out->chance = p;
                return;
            }
        }
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

    if (Scout_BattlerDeducedHoldEffect(battler) != HOLD_EFFECT_NONE)
        return Scout_BattlerDeducedHoldEffect(battler);
    Scout_GuessItem(gBattleMons[battler].species, &g);
    if (g.effect == HOLD_EFFECT_NONE || Scout_BattlerHoldEffectExcluded(battler, g.effect))
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

bool32 Scout_ItemKnownGone(enum BattlerId battler)
{
    // Consumo e Privazione sono annunciati a schermo: è informazione pubblica.
    struct PartyState *ps = GetBattlerPartyState(battler);
    return ps->usedHeldItem != ITEM_NONE || ps->isKnockedOff;
}

static u16 EstimatedStat(u32 base, u32 level, u32 ev, bool32 boosted)
{
    u32 stat = ((2 * base + 31 + ev / 4) * level) / 100 + 5;
    return boosted ? stat * 110 / 100 : stat;
}

// Spread generica dal profilo della specie (quando mancano le statistiche d'uso): veloce e offensivo =
// 4 PS / 252 attacco / 252 Velocità con natura +Velocità; altrimenti 252 PS / 252 attacco con natura +attacco.
static void EstimateStatsGeneric(u16 species, u32 level, u16 *hp, u16 stats[NUM_STATS])
{
    const struct SpeciesInfo *si = &gSpeciesInfo[species];
    bool32 physical = si->baseAttack >= si->baseSpAttack;
    u32 offBase = physical ? si->baseAttack : si->baseSpAttack;
    bool32 sweeper = si->baseSpeed >= 85 && offBase >= 100;
    u32 hpEv = sweeper ? 4 : 252;
    u32 speEv = sweeper ? 252 : 0;
    u32 defEv = sweeper ? 0 : 4;

    *hp = ((2 * si->baseHP + 31 + hpEv / 4) * level) / 100 + level + 10;
    stats[STAT_HP] = *hp;
    stats[STAT_ATK] = EstimatedStat(si->baseAttack, level, physical ? 252 : 0, !sweeper && physical);
    stats[STAT_SPATK] = EstimatedStat(si->baseSpAttack, level, physical ? 0 : 252, !sweeper && !physical);
    stats[STAT_DEF] = EstimatedStat(si->baseDefense, level, defEv, FALSE);
    stats[STAT_SPDEF] = EstimatedStat(si->baseSpDefense, level, 0, FALSE);
    stats[STAT_SPEED] = EstimatedStat(si->baseSpeed, level, speEv, sweeper);
}

void Scout_EstimateStats(u16 species, u32 level, u16 *hp, u16 stats[NUM_STATS])
{
    struct ScoutSpread spread;

    // Statistiche vere mai lette: la spread più usata dai giocatori (natura + EV) se la specie è nella
    // tabella delle statistiche d'uso, altrimenti la spread generica del profilo.
    if (Scout_GetMetaSpreads(species, &spread, 1) != 0)
    {
        Scout_StatsFromSpread(species, level, &spread, stats);
        *hp = stats[STAT_HP];
        return;
    }
    EstimateStatsGeneric(species, level, hp, stats);
}

enum Ability Scout_KnownAbility(u16 species)
{
    // Abilità certa solo se la specie ne ha una sola possibile; altrimenti nessuna (ignota).
    const enum Ability *ab = gSpeciesInfo[species].abilities;
    enum Ability only = ab[0];
    u32 i;

    for (i = 1; i < NUM_ABILITY_SLOTS; i++)
    {
        if (ab[i] != ABILITY_NONE && ab[i] != only)
            return ABILITY_NONE;
    }
    return only;
}

// ---------------------------------------------------------------------
// Base di conoscenza VGC (statistiche d'uso, src/data/vgc_meta.h): API per specie
// ---------------------------------------------------------------------
#include "battle_main.h"
#include "battle_util.h"
#include "constants/battle_ai.h"

// Ordine EV nella tabella (Showdown): PS, Att, Dif, AttSp, DifSp, Vel.
static const u8 sMetaStatOrder[NUM_STATS] = { STAT_HP, STAT_ATK, STAT_DEF, STAT_SPATK, STAT_SPDEF, STAT_SPEED };

bool32 Scout_HasMetaData(u16 species)
{
    return MetaEntryFor(species) != NULL;
}

bool32 Scout_MetaTableIsSorted(void)
{
    u32 i;
    for (i = 1; i < ARRAY_COUNT(sVgcMeta); i++)
    {
        if (sVgcMeta[i].species <= sVgcMeta[i - 1].species)
            return FALSE;
    }
    return TRUE;
}

u32 Scout_MetaUsage(u16 species)
{
    const struct VgcMetaEntry *meta = MetaEntryFor(species);
    return meta != NULL ? meta->usage : 0;
}

u32 Scout_GetMetaSpreads(u16 species, struct ScoutSpread *out, u32 max)
{
    const struct VgcMetaEntry *meta = MetaEntryFor(species);
    u32 i, k, n = 0;

    if (meta == NULL)
        return 0;
    for (i = 0; i < VGC_META_SPREADS && n < max; i++)
    {
        if (meta->spreads[i].p == 0)
            break;
        out[n].nature = meta->spreads[i].nature;
        out[n].p = meta->spreads[i].p;
        for (k = 0; k < NUM_STATS; k++)
            out[n].ev[sMetaStatOrder[k]] = min(meta->spreads[i].ev[k] * 4, 252);
        n++;
    }
    return n;
}

static u16 CalcStatFromParts(u16 species, enum Stat stat, u32 level, u32 iv, u32 ev, u32 nature)
{
    const struct SpeciesInfo *si = &gSpeciesInfo[species];
    u32 base, v;

    switch (stat)
    {
    case STAT_HP:
        if (si->baseHP == 1) // Shedinja
            return 1;
        return ((2 * si->baseHP + iv + ev / 4) * level) / 100 + level + 10;
    case STAT_ATK:   base = si->baseAttack;    break;
    case STAT_DEF:   base = si->baseDefense;   break;
    case STAT_SPEED: base = si->baseSpeed;     break;
    case STAT_SPATK: base = si->baseSpAttack;  break;
    default:         base = si->baseSpDefense; break;
    }
    v = ((2 * base + iv + ev / 4) * level) / 100 + 5;
    return ModifyStatByNature(nature, v, stat);
}

void Scout_StatsFromSpread(u16 species, u32 level, const struct ScoutSpread *spread, u16 stats[NUM_STATS])
{
    u32 i;

    for (i = 0; i < NUM_STATS; i++)
    {
        // Trick Room: chi ha natura -Velocità e 0 EV in Velocità di solito ha anche 0 IV.
        u32 iv = (i == STAT_SPEED && spread->ev[STAT_SPEED] == 0
               && gNaturesInfo[spread->nature].statDown == STAT_SPEED) ? 0 : 31;
        stats[i] = CalcStatFromParts(species, i, level, iv, spread->ev[i], spread->nature);
    }
}

// Velocità possibile della specie: da 0 IV / 0 EV / natura - a 31 IV / 252 EV / natura +.
void Scout_SpeciesSpeedRange(u16 species, u32 level, u16 *lo, u16 *hi)
{
    u32 v;

    v = ((2 * gSpeciesInfo[species].baseSpeed) * level) / 100 + 5;
    *lo = v * 90 / 100;
    v = ((2 * gSpeciesInfo[species].baseSpeed + 31 + 63) * level) / 100 + 5;
    *hi = v * 110 / 100;
}

static bool32 SpeciesCanLearnAny(u16 species, enum Move move)
{
    if (SpeciesCanLearn(species, move))
        return TRUE;
    // Le Mega imparano quello che impara la forma base.
    if (gSpeciesInfo[species].isMegaEvolution)
        return SpeciesCanLearn(GET_BASE_SPECIES_ID(species), move);
    return FALSE;
}

u32 Scout_GetLikelyMoves(u16 species, struct ScoutMoveChance *out, u32 max)
{
    const struct VgcMetaEntry *meta = MetaEntryFor(species);
    u32 i, n = 0;

    if (meta == NULL)
        return 0;
    for (i = 0; i < VGC_META_MOVES && n < max && meta->moves[i] != MOVE_NONE; i++)
    {
        // Le statistiche vengono da altri giochi: conta solo ciò che qui la specie può davvero avere.
        if (!SpeciesCanLearnAny(species, meta->moves[i]))
            continue;
        out[n].move = meta->moves[i];
        out[n].p = meta->moveP[i];
        n++;
    }
    return n;
}

u32 Scout_MoveChance(u16 species, enum Move move)
{
    const struct VgcMetaEntry *meta;
    struct ScoutProfile prof;
    u32 i, r, roleP = 0;

    if (move == MOVE_NONE || move == MOVE_UNAVAILABLE || species == SPECIES_NONE || species >= NUM_SPECIES)
        return 0;
    meta = MetaEntryFor(species);
    for (i = 0; meta != NULL && i < VGC_META_MOVES && meta->moves[i] != MOVE_NONE; i++)
    {
        if (meta->moves[i] == move)
            return meta->moveP[i];
    }
    if (!SpeciesCanLearnAny(species, move))
        return 0;
    // Non tra le più usate (o specie senza statistiche): vale il profilo dei ruoli.
    Scout_GetSpeciesProfile(species, &prof);
    for (r = 0; r < SCOUT_ROLE_COUNT; r++)
    {
        if (Scout_MoveHasRole(move, r))
            roleP = max(roleP, (u32)prof.role[r]);
    }
    if (meta != NULL)
        return max(roleP, (u32)META_UNLISTED_P); // il ruolo include già la tabella a mano (Fake Out di Incineroar...)
    return roleP != 0 ? roleP : 10;
}

u32 Scout_GetLikelyItems(u16 species, struct ScoutItemChance *out, u32 max)
{
    const struct VgcMetaEntry *meta = MetaEntryFor(species);
    u32 i, n = 0;

    if (meta == NULL)
        return 0;
    for (i = 0; i < VGC_META_ITEMS && n < max && meta->items[i] != ITEM_NONE; i++)
    {
        out[n].item = meta->items[i];
        out[n].p = meta->itemP[i];
        n++;
    }
    return n;
}

u32 Scout_GetMegaForms(u16 species, struct ScoutMegaChance *out, u32 max)
{
    const struct VgcMetaEntry *meta;
    u32 i, n = 0;

    if (species == SPECIES_NONE || species >= NUM_SPECIES || gSpeciesInfo[species].isMegaEvolution)
        return 0;
    meta = MetaEntryFor(species);
    for (i = 0; meta != NULL && i < VGC_META_MEGAS && n < max && meta->megas[i] != SPECIES_NONE; i++)
    {
        out[n].species = meta->megas[i];
        out[n].p = meta->megaP[i];
        out[n].ability = gSpeciesInfo[meta->megas[i]].abilities[0];
        n++;
    }
    return n;
}

// Abilità distinte della specie (con lo slot di gSpeciesInfo.abilities). Ritorna quante.
static u32 SpeciesAbilities(u16 species, enum Ability out[NUM_ABILITY_SLOTS], u8 slot[NUM_ABILITY_SLOTS])
{
    const enum Ability *ab = gSpeciesInfo[species].abilities;
    u32 i, k, n = 0;

    for (i = 0; i < NUM_ABILITY_SLOTS; i++)
    {
        bool32 dup = FALSE;
        if (ab[i] == ABILITY_NONE)
            continue;
        for (k = 0; k < n; k++)
        {
            if (out[k] == ab[i])
                dup = TRUE;
        }
        if (dup)
            continue;
        out[n] = ab[i];
        slot[n] = i;
        n++;
    }
    return n;
}

// Probabilità di un'abilità con alcuni slot esclusi (bit = slot di gSpeciesInfo.abilities).
static u32 AbilityChanceMasked(u16 species, enum Ability ability, u32 excludedSlots)
{
    const struct VgcMetaEntry *meta;
    enum Ability list[NUM_ABILITY_SLOTS];
    u8 slot[NUM_ABILITY_SLOTS];
    u32 i, k, n, total = 0, hit = 0;

    if (species == SPECIES_NONE || species >= NUM_SPECIES || ability == ABILITY_NONE)
        return 0;
    n = SpeciesAbilities(species, list, slot);
    meta = gSpeciesInfo[species].isMegaEvolution ? NULL : MetaEntryFor(species);
    for (i = 0; i < n; i++)
    {
        u32 w;
        if (excludedSlots & (1u << slot[i]))
            continue;
        if (meta != NULL && meta->abilities[0] != ABILITY_NONE)
        {
            w = 0;
            for (k = 0; k < VGC_META_ABILITIES && meta->abilities[k] != ABILITY_NONE; k++)
            {
                if (meta->abilities[k] == list[i])
                    w = meta->abilityP[k];
            }
            w = w * 4 + 1; // anche l'abilità mai vista nelle statistiche resta possibile (poco)
        }
        else
        {
            w = 1;
        }
        total += w;
        if (list[i] == ability)
            hit = w;
    }
    if (total == 0)
        return 0;
    return (hit * 100 + total / 2) / total;
}

u32 Scout_AbilityChance(u16 species, enum Ability ability)
{
    return AbilityChanceMasked(species, ability, 0);
}

enum Ability Scout_LikelyAbility(u16 species, u32 *chance)
{
    enum Ability list[NUM_ABILITY_SLOTS], best = ABILITY_NONE;
    u8 slot[NUM_ABILITY_SLOTS];
    u32 i, n = 0, bestP = 0;

    if (species != SPECIES_NONE && species < NUM_SPECIES)
        n = SpeciesAbilities(species, list, slot);
    for (i = 0; i < n; i++)
    {
        u32 p = AbilityChanceMasked(species, list[i], 0);
        if (p > bestP)
        {
            bestP = p;
            best = list[i];
        }
    }
    if (chance != NULL)
        *chance = bestP;
    return best;
}

// ---------------------------------------------------------------------
// Belief sui Pokémon del giocatore: ciò che l'IA deduce da quello che VEDE in lotta
// ---------------------------------------------------------------------
#define SCOUT_BF_ITEM_DEDUCED   (1 << 0) // heldEffect dedotto qui (si può ritirare)
#define SCOUT_BF_WROTE_HISTORY  (1 << 1) // la deduzione è stata scritta in gBattleHistory / gAiPartyData
#define SCOUT_BF_OFFENSE_BOOST  (1 << 2) // ha fatto più danni del massimo senza strumento (Life Orb, strumento di tipo...)

// Uno per Pokémon della squadra del giocatore (indice di gPlayerParty): 14 byte in EWRAM.
struct ScoutBelief
{
    u16 species;        // forma a cui si riferisce (con la Mega velocità e abilità ripartono)
    u16 speedLo;        // statistica Velocità (senza stadi, strumento, abilità) ancora possibile
    u16 speedHi;
    u16 lastMove;       // ultima mossa usata da quando è entrato (Choice: sempre la stessa)
    u8 lastSwitchIn;    // switchInCount quando ha usato lastMove
    u8 heldEffect;      // HOLD_EFFECT_* dedotto (0 = nessuno)
    u8 itemNot;         // SCOUT_NOT_*: strumenti esclusi dalle osservazioni
    u8 abilityExcluded; // bit = slot di gSpeciesInfo.abilities escluso (abilità d'ingresso non attivata)
    u8 flags;
};

static EWRAM_DATA struct ScoutBelief sScoutBelief[PARTY_SIZE] = {0};

static bool32 BeliefActive(void)
{
    return gAiThinkingStruct != NULL && gAiPartyData != NULL && gBattleHistory != NULL && IsDoubleBattle()
        && (gBattleTypeFlags & BATTLE_TYPE_TRAINER) && IsAiFlagPresent(AI_FLAG_VGC_DOUBLES);
}

static u32 NotBitFor(u32 holdEffect)
{
    switch (holdEffect)
    {
    case HOLD_EFFECT_CHOICE_SCARF: return SCOUT_NOT_SCARF;
    case HOLD_EFFECT_CHOICE_BAND:  return SCOUT_NOT_BAND;
    case HOLD_EFFECT_CHOICE_SPECS: return SCOUT_NOT_SPECS;
    case HOLD_EFFECT_ASSAULT_VEST: return SCOUT_NOT_ASSAULT_VEST;
    case HOLD_EFFECT_LIFE_ORB:     return SCOUT_NOT_LIFE_ORB;
    case HOLD_EFFECT_FOCUS_SASH:   return SCOUT_NOT_FOCUS_SASH;
    case HOLD_EFFECT_EVIOLITE:     return SCOUT_NOT_EVIOLITE;
    default:                       return 0;
    }
}

static bool32 IsChoiceEffect(u32 holdEffect)
{
    return holdEffect == HOLD_EFFECT_CHOICE_SCARF || holdEffect == HOLD_EFFECT_CHOICE_BAND || holdEffect == HOLD_EFFECT_CHOICE_SPECS;
}

static void BeliefInitSpecies(struct ScoutBelief *b, u16 species, u32 level)
{
    u8 keepNot = b->itemNot, keepEffect = b->heldEffect, keepFlags = b->flags;
    bool32 sameMon = (b->species != SPECIES_NONE);

    memset(b, 0, sizeof(*b));
    b->species = species;
    Scout_SpeciesSpeedRange(species, level, &b->speedLo, &b->speedHi);
    // Cambio di forma (Mega): ciò che si sa dello strumento resta.
    if (sameMon)
    {
        b->itemNot = keepNot;
        b->heldEffect = keepEffect;
        b->flags = keepFlags & ~SCOUT_BF_OFFENSE_BOOST;
    }
}

static struct ScoutBelief *BeliefFor(enum BattlerId battler)
{
    struct ScoutBelief *b;
    u32 slot;

    if (battler >= gBattlersCount || !IsOnPlayerSide(battler))
        return NULL;
    slot = gBattlerPartyIndexes[battler];
    if (slot >= PARTY_SIZE)
        return NULL;
    b = &sScoutBelief[slot];
    if (b->species != gBattleMons[battler].species)
        BeliefInitSpecies(b, gBattleMons[battler].species, gBattleMons[battler].level);
    return b;
}

static const struct ScoutBelief *BeliefPeek(enum BattlerId battler)
{
    u32 slot;

    if (battler >= gBattlersCount || !IsOnPlayerSide(battler))
        return NULL;
    slot = gBattlerPartyIndexes[battler];
    if (slot >= PARTY_SIZE || sScoutBelief[slot].species != gBattleMons[battler].species)
        return NULL;
    return &sScoutBelief[slot];
}

void Scout_BeliefReset(void)
{
    memset(sScoutBelief, 0, sizeof(sScoutBelief));
}

static struct AiPartyMon *KnownMon(enum BattlerId battler)
{
    return &gAiPartyData->mons[B_TRAINER_PLAYER][gBattlerPartyIndexes[battler]];
}

// Abilità che l'IA conosce: vista (popup), ricordata (Bo3) o l'unica possibile della specie.
static enum Ability RevealedAbility(enum BattlerId battler)
{
    if (gBattleHistory != NULL && gBattleHistory->abilities[battler] != ABILITY_NONE)
        return gBattleHistory->abilities[battler];
    if (gAiPartyData != NULL && KnownMon(battler)->ability != ABILITY_NONE)
        return KnownMon(battler)->ability;
    return Scout_KnownAbility(gBattleMons[battler].species);
}

static void BeliefSetHeldEffect(enum BattlerId battler, struct ScoutBelief *b, u32 holdEffect)
{
    b->heldEffect = holdEffect;
    b->flags |= SCOUT_BF_ITEM_DEDUCED;
    // Come farebbe un giocatore: da qui in poi l'IA ragiona con quello strumento. Stessa via delle
    // deduzioni già presenti (gBattleHistory / gAiPartyData), così lo vedono subito regole e calcolo danni.
    if (gBattleHistory->itemEffects[battler] == HOLD_EFFECT_NONE)
    {
        gBattleHistory->itemEffects[battler] = holdEffect;
        KnownMon(battler)->heldEffect = holdEffect;
        b->flags |= SCOUT_BF_WROTE_HISTORY;
    }
}

static void BeliefRetractHeldEffect(enum BattlerId battler, struct ScoutBelief *b)
{
    b->itemNot |= NotBitFor(b->heldEffect);
    if ((b->flags & SCOUT_BF_WROTE_HISTORY) && gBattleHistory->itemEffects[battler] == b->heldEffect)
    {
        gBattleHistory->itemEffects[battler] = HOLD_EFFECT_NONE;
        KnownMon(battler)->heldEffect = HOLD_EFFECT_NONE;
    }
    b->heldEffect = HOLD_EFFECT_NONE;
    b->flags &= ~(SCOUT_BF_ITEM_DEDUCED | SCOUT_BF_WROTE_HISTORY);
}

// Strumento visto davvero o dedotto qui; HOLD_EFFECT_NONE se ignoto.
static u32 BeliefItemEffect(enum BattlerId battler, const struct ScoutBelief *b)
{
    if (gBattleHistory->itemEffects[battler] != HOLD_EFFECT_NONE)
        return gBattleHistory->itemEffects[battler];
    return b->heldEffect;
}

// --- Velocità ------------------------------------------------------------------------------
// Velocità effettiva del giocatore se la sua statistica fosse `raw`: stadi, Tailwind, paralisi e meteo
// con l'abilità NOTA sono tutti pubblici. La statistica vera si rimette subito a posto.
static u32 SpeedWithRaw(enum BattlerId battler, u32 raw, enum Ability ability, u32 holdEffect)
{
    u16 saved = gBattleMons[battler].speed;
    u32 v;

    gBattleMons[battler].speed = raw;
    v = GetBattlerTotalSpeedStat(battler, ability, holdEffect);
    gBattleMons[battler].speed = saved;
    return v;
}

// Minima statistica con velocità effettiva >= target (0xFFFF se nessuna fino a 2000).
static u32 MinRawAtLeast(enum BattlerId battler, u32 target, enum Ability ability, u32 holdEffect)
{
    u32 lo = 1, hi = 2000;

    if (SpeedWithRaw(battler, hi, ability, holdEffect) < target)
        return 0xFFFF;
    while (lo < hi)
    {
        u32 mid = (lo + hi) / 2;
        if (SpeedWithRaw(battler, mid, ability, holdEffect) >= target)
            hi = mid;
        else
            lo = mid + 1;
    }
    return lo;
}

// Massima statistica con velocità effettiva <= target (0 se nessuna).
static u32 MaxRawAtMost(enum BattlerId battler, u32 target, enum Ability ability, u32 holdEffect)
{
    u32 lo = 0, hi = 2000;

    if (SpeedWithRaw(battler, 1, ability, holdEffect) > target)
        return 0;
    while (lo < hi)
    {
        u32 mid = (lo + hi + 1) / 2;
        if (SpeedWithRaw(battler, mid, ability, holdEffect) <= target)
            lo = mid;
        else
            hi = mid - 1;
    }
    return lo;
}

static bool32 SpeciesMayHaveAbility(u16 species, enum Ability ability)
{
    u32 i;
    for (i = 0; i < NUM_ABILITY_SLOTS; i++)
    {
        if (gSpeciesInfo[species].abilities[i] == ability)
            return TRUE;
    }
    return FALSE;
}

// La priorità della mossa del giocatore potrebbe dipendere da un'abilità che l'IA non conosce?
static bool32 PlayerPriorityUncertain(enum BattlerId battler, enum Move move)
{
    u16 species = gBattleMons[battler].species;
    enum Ability known = RevealedAbility(battler);

    if (known != ABILITY_NONE)
        return GetBattleMovePriority(battler, known, move) != GetMovePriority(move)
            || known == ABILITY_STALL || known == ABILITY_MYCELIUM_MIGHT || known == ABILITY_QUICK_DRAW;
    if (IsBattleMoveStatus(move) && (SpeciesMayHaveAbility(species, ABILITY_PRANKSTER) || SpeciesMayHaveAbility(species, ABILITY_MYCELIUM_MIGHT)))
        return TRUE;
    if (GetMoveType(move) == TYPE_FLYING && SpeciesMayHaveAbility(species, ABILITY_GALE_WINGS))
        return TRUE;
    return SpeciesMayHaveAbility(species, ABILITY_TRIAGE) || SpeciesMayHaveAbility(species, ABILITY_STALL)
        || SpeciesMayHaveAbility(species, ABILITY_QUICK_DRAW);
}

// Quick Claw, Custap Berry, Quick Draw, Quash, Lagging Tail, Mycelium Might: l'ordine non dice la velocità.
static bool32 OrderAltered(enum BattlerId battler)
{
    return gProtectStructs[battler].usedCustapBerry || gProtectStructs[battler].quickDraw
        || gProtectStructs[battler].quash || gProtectStructs[battler].laggingTail || gProtectStructs[battler].myceliumMight;
}

// Il giocatore `pl` è stato più veloce (faster) o più lento di un nostro Pokémon con velocità effettiva
// aiSpeed (a pari priorità): si restringe la sua statistica Velocità; se nessuna statistica possibile
// spiega l'ordine senza strumento, ha una Choice Scarf.
static void BeliefSpeedObservation(enum BattlerId pl, u32 aiSpeed, bool32 faster)
{
    struct ScoutBelief *b = BeliefFor(pl);
    enum Ability ability;
    u32 he, bound;
    u16 minPossible, maxPossible;

    if (b == NULL)
        return;
    ability = RevealedAbility(pl);
    he = BeliefItemEffect(pl, b);
    Scout_SpeciesSpeedRange(b->species, gBattleMons[pl].level, &minPossible, &maxPossible);
    if (faster)
    {
        bound = MinRawAtLeast(pl, aiSpeed, ability, he);
        if (bound > maxPossible)
        {
            // Troppo veloce per qualunque spread: Choice Scarf, se è nella lista del formato e non esclusa.
            if (he == HOLD_EFFECT_NONE && !(b->itemNot & SCOUT_NOT_SCARF) && PoolHasEffect(HOLD_EFFECT_CHOICE_SCARF)
             && !Scout_ItemKnownGone(pl))
            {
                bound = MinRawAtLeast(pl, aiSpeed, ability, HOLD_EFFECT_CHOICE_SCARF);
                if (bound <= maxPossible)
                {
                    BeliefSetHeldEffect(pl, b, HOLD_EFFECT_CHOICE_SCARF);
                    b->speedLo = max(b->speedLo, (u16)bound);
                    if (b->speedHi < b->speedLo)
                        b->speedHi = maxPossible;
                }
            }
            return; // altrimenti un'abilità che non conosciamo (Swift Swim, Unburden...): niente deduzioni
        }
        if (bound > b->speedLo)
            b->speedLo = min(bound, (u32)b->speedHi);
    }
    else
    {
        bound = MaxRawAtMost(pl, aiSpeed, ability, he);
        if (bound < minPossible || bound < b->speedLo)
        {
            // Più lento di quanto la Scarf dedotta permetta: la deduzione era sbagliata.
            if ((b->flags & SCOUT_BF_ITEM_DEDUCED) && b->heldEffect == HOLD_EFFECT_CHOICE_SCARF)
            {
                BeliefRetractHeldEffect(pl, b);
                b->speedLo = minPossible;
                bound = MaxRawAtMost(pl, aiSpeed, ability, HOLD_EFFECT_NONE);
                if (bound >= minPossible)
                    b->speedHi = min((u32)b->speedHi, bound);
            }
            return;
        }
        b->speedHi = min((u32)b->speedHi, bound);
        // Anche con la statistica più bassa ancora possibile, la Scarf l'avrebbe fatto passare davanti.
        if (he == HOLD_EFFECT_NONE && SpeedWithRaw(pl, b->speedLo, ability, HOLD_EFFECT_CHOICE_SCARF) > aiSpeed)
            b->itemNot |= SCOUT_NOT_SCARF;
    }
}

void Scout_BeliefNoteMove(enum BattlerId battler, enum Move move)
{
    u32 k;
    bool32 trickRoom;
    s32 prio;

    if (battler >= gBattlersCount || move == MOVE_NONE || move == MOVE_UNAVAILABLE || !BeliefActive())
        return;

    // Choice dedotta ma ha cambiato mossa senza uscire: non era Choice.
    if (IsOnPlayerSide(battler))
    {
        struct ScoutBelief *b = BeliefFor(battler);
        u8 sw = KnownMon(battler)->switchInCount;
        if (b != NULL)
        {
            if ((b->flags & SCOUT_BF_ITEM_DEDUCED) && IsChoiceEffect(b->heldEffect)
             && b->lastMove != MOVE_NONE && b->lastSwitchIn == sw && b->lastMove != move)
                BeliefRetractHeldEffect(battler, b);
            b->lastMove = move;
            b->lastSwitchIn = sw;
        }
    }

    // Ordine di turno: confronto con chi, del lato opposto, deve ancora muoversi in questo turno.
    if (OrderAltered(battler))
        return;
    trickRoom = (gFieldStatuses & STATUS_FIELD_TRICK_ROOM) != 0;
    if (IsOnPlayerSide(battler))
    {
        if (PlayerPriorityUncertain(battler, move))
            return;
        prio = GetMovePriority(move);
    }
    else
    {
        prio = GetBattleMovePriority(battler, GetBattlerAbility(battler), move);
    }
    for (k = gCurrentTurnActionNumber + 1; k < gBattlersCount; k++)
    {
        enum BattlerId other = gBattlerByTurnOrder[k];
        enum BattlerId pl, ai;
        enum Move otherMove;
        s32 otherPrio;
        bool32 plFirst;

        if (gActionsByTurnOrder[k] != B_ACTION_USE_MOVE || !IsBattlerAlive(other)
         || IsOnPlayerSide(other) == IsOnPlayerSide(battler) || OrderAltered(other))
            continue;
        otherMove = GetBattlerChosenMove(other);
        if (otherMove == MOVE_NONE || otherMove == MOVE_UNAVAILABLE)
            continue;
        if (IsOnPlayerSide(other))
        {
            if (PlayerPriorityUncertain(other, otherMove))
                continue;
            otherPrio = GetMovePriority(otherMove);
            pl = other;
            ai = battler;
            plFirst = FALSE;
        }
        else
        {
            otherPrio = GetBattleMovePriority(other, GetBattlerAbility(other), otherMove);
            pl = battler;
            ai = other;
            plFirst = TRUE;
        }
        if (otherPrio != prio)
            continue;
        // Sotto Trick Room si muove prima il più lento.
        BeliefSpeedObservation(pl, GetBattlerTotalSpeedStat(ai, GetBattlerAbility(ai), GetBattlerHoldEffect(ai)),
                               trickRoom ? !plFirst : plFirst);
    }
}

// --- Danni ------------------------------------------------------------------------------------
// Danno col tiro al 100%, senza modificare nulla dello stato della lotta (niente RNG, niente flag).
static s32 MaxRollDamage(struct DamageContext *ctx)
{
    s32 dmg;
    u16 savedPower = gBattleMovePower;

    ctx->randomFactor = FALSE;
    ctx->updateFlags = FALSE;
    ctx->runScript = FALSE;
    ctx->aiCalc = FALSE;
    dmg = CalculateMoveDamageVars(ctx);
    if (dmg > 0)
        dmg = ApplyModifiersAfterDmgRoll(ctx, dmg);
    gBattleMovePower = savedPower;
    return dmg;
}

// Statistiche del giocatore al massimo possibile (31 IV, 252 EV, natura +): solo per il calcolo, poi si ripristinano.
static void SetMaxStats(enum BattlerId battler, u16 saved[NUM_STATS])
{
    const struct SpeciesInfo *si = &gSpeciesInfo[gBattleMons[battler].species];
    u32 level = gBattleMons[battler].level;

    saved[STAT_ATK] = gBattleMons[battler].attack;
    saved[STAT_DEF] = gBattleMons[battler].defense;
    saved[STAT_SPEED] = gBattleMons[battler].speed;
    saved[STAT_SPATK] = gBattleMons[battler].spAttack;
    saved[STAT_SPDEF] = gBattleMons[battler].spDefense;
#define MAX_STAT(base) (((((2 * (base) + 31 + 63) * level) / 100 + 5) * 110) / 100)
    gBattleMons[battler].attack = MAX_STAT(si->baseAttack);
    gBattleMons[battler].defense = MAX_STAT(si->baseDefense);
    gBattleMons[battler].speed = MAX_STAT(si->baseSpeed);
    gBattleMons[battler].spAttack = MAX_STAT(si->baseSpAttack);
    gBattleMons[battler].spDefense = MAX_STAT(si->baseSpDefense);
#undef MAX_STAT
}

static void RestoreStats(enum BattlerId battler, const u16 saved[NUM_STATS])
{
    gBattleMons[battler].attack = saved[STAT_ATK];
    gBattleMons[battler].defense = saved[STAT_DEF];
    gBattleMons[battler].speed = saved[STAT_SPEED];
    gBattleMons[battler].spAttack = saved[STAT_SPATK];
    gBattleMons[battler].spDefense = saved[STAT_SPDEF];
}

// Abilità possibili del giocatore in campo: quella nota, altrimenti tutte quelle non escluse.
static u32 CandidateAbilities(enum BattlerId battler, const struct ScoutBelief *b, enum Ability out[NUM_ABILITY_SLOTS])
{
    enum Ability list[NUM_ABILITY_SLOTS];
    u8 slot[NUM_ABILITY_SLOTS];
    u32 i, n, m = 0;

    out[0] = RevealedAbility(battler);
    if (out[0] != ABILITY_NONE)
        return 1;
    n = SpeciesAbilities(gBattleMons[battler].species, list, slot);
    for (i = 0; i < n; i++)
    {
        if (!(b->abilityExcluded & (1u << slot[i])))
            out[m++] = list[i];
    }
    return m;
}

// Il compagno del giocatore potrebbe alterare il danno con un'abilità che non conosciamo?
static bool32 PartnerMayModifyDamage(enum BattlerId pl)
{
    enum BattlerId partner = GetPartnerBattler(pl);
    u16 sp;

    if (partner >= gBattlersCount || !IsBattlerAlive(partner) || RevealedAbility(partner) != ABILITY_NONE)
        return FALSE;
    sp = gBattleMons[partner].species;
    return SpeciesMayHaveAbility(sp, ABILITY_FRIEND_GUARD) || SpeciesMayHaveAbility(sp, ABILITY_POWER_SPOT)
        || SpeciesMayHaveAbility(sp, ABILITY_BATTERY) || SpeciesMayHaveAbility(sp, ABILITY_STEELY_SPIRIT)
        || SpeciesMayHaveAbility(sp, ABILITY_FLOWER_GIFT);
}

static bool32 DamageUsableForBelief(const struct DamageContext *ctx)
{
    switch (GetMoveEffect(ctx->move))
    {
    case EFFECT_LEVEL_DAMAGE:
    case EFFECT_PSYWAVE:
    case EFFECT_FIXED_HP_DAMAGE:
    case EFFECT_FIXED_PERCENT_DAMAGE:
    case EFFECT_FINAL_GAMBIT:
    case EFFECT_BEAT_UP:
    case EFFECT_REFLECT_DAMAGE:
    case EFFECT_ENDEAVOR:
    case EFFECT_OHKO:
    case EFFECT_BIDE:
    case EFFECT_FOUL_PLAY:
    case EFFECT_GYRO_BALL:
    case EFFECT_ELECTRO_BALL:
    case EFFECT_PSYSHOCK:
        return FALSE;
    default:
        break;
    }
    return GetMovePower(ctx->move) != 0 && !gSpecialStatuses[ctx->battlerAtk].attackerInParty;
}

void Scout_BeliefNoteDamage(const struct DamageContext *ctx, s32 damage)
{
    enum BattlerId atk = ctx->battlerAtk, def = ctx->battlerDef, pl;
    struct ScoutBelief *b;
    struct DamageContext c;
    enum Ability cand[NUM_ABILITY_SLOTS];
    u16 saved[NUM_STATS];
    u32 i, n;
    s32 ref = -1;
    bool32 special;

    if (atk >= gBattlersCount || def >= gBattlersCount || !BeliefActive())
        return;
    if (IsOnPlayerSide(atk) == IsOnPlayerSide(def) || damage <= 0 || damage + 1 >= gBattleMons[def].hp)
        return; // KO, Focus Sash, Endure (danno ridotto a PS - 1): il danno vero non si vede
    if (!DamageUsableForBelief(ctx))
        return;
    pl = IsOnPlayerSide(atk) ? atk : def;
    b = BeliefFor(pl);
    if (b == NULL || BeliefItemEffect(pl, b) != HOLD_EFFECT_NONE || Scout_ItemKnownGone(pl) || PartnerMayModifyDamage(pl))
        return;
    special = GetBattleMoveCategory(ctx->move) == DAMAGE_CATEGORY_SPECIAL;
    n = CandidateAbilities(pl, b, cand);
    if (n == 0)
        return;

    SetMaxStats(pl, saved);
    for (i = 0; i < n; i++)
    {
        s32 d;
        c = *ctx;
        c.abilities[pl] = cand[i];
        c.holdEffects[pl] = HOLD_EFFECT_NONE;
        d = MaxRollDamage(&c);
        if (pl == atk)
            ref = max(ref, d);                         // il massimo che può fare senza strumento
        else
            ref = (ref < 0) ? d : min(ref, d);         // il tiro massimo con la difesa più alta possibile
    }
    RestoreStats(pl, saved);
    if (ref <= 0)
        return;

    if (pl == atk)
    {
        // Choice Band/Specs: x1.5 (x1.275 col tiro più basso); Life Orb al massimo x1.3.
        if (damage * 100 > ref * 131 + 100)
        {
            u32 he = special ? HOLD_EFFECT_CHOICE_SPECS : HOLD_EFFECT_CHOICE_BAND;
            if (!(b->itemNot & NotBitFor(he)) && PoolHasEffect(he))
                BeliefSetHeldEffect(pl, b, he);
        }
        else if (damage * 100 > ref * 105 + 100)
        {
            b->flags |= SCOUT_BF_OFFENSE_BOOST;
        }
    }
    else
    {
        // Subito meno del minimo possibile senza strumento (tiro all'85%, margine 10%): Assault Vest sugli
        // speciali, Eviolite sui fisici se non è completamente evoluto.
        if (damage * 100 < ref * 85 * 90 / 100 - 100)
        {
            u32 he = special ? HOLD_EFFECT_ASSAULT_VEST
                   : (SpeciesIsNfe(b->species) ? HOLD_EFFECT_EVIOLITE : HOLD_EFFECT_NONE);
            if (he != HOLD_EFFECT_NONE && !(b->itemNot & NotBitFor(he)) && PoolHasEffect(he))
                BeliefSetHeldEffect(pl, b, he);
        }
    }
}

// --- Abilità d'ingresso -------------------------------------------------------------------------
// Abilità che si annunciano sempre entrando in campo (nelle condizioni attuali del campo).
static bool32 EntryAbilityWouldShow(enum Ability ability)
{
    u32 weather = GetWeather();
    u32 i;

    switch (ability)
    {
    case ABILITY_INTIMIDATE:
    case ABILITY_PRESSURE:
    case ABILITY_UNNERVE:
    case ABILITY_MOLD_BREAKER:
    case ABILITY_TERAVOLT:
    case ABILITY_TURBOBLAZE:
    case ABILITY_AIR_LOCK:
    case ABILITY_CLOUD_NINE:
    case ABILITY_NEUTRALIZING_GAS:
    case ABILITY_VESSEL_OF_RUIN:
    case ABILITY_SWORD_OF_RUIN:
    case ABILITY_TABLETS_OF_RUIN:
    case ABILITY_BEADS_OF_RUIN:
    case ABILITY_DOWNLOAD:
    case ABILITY_DESOLATE_LAND:
    case ABILITY_PRIMORDIAL_SEA:
    case ABILITY_DELTA_STREAM:
        return TRUE;
    case ABILITY_FRISK:
        for (i = 0; i < gBattlersCount; i++)
        {
            if (!IsOnPlayerSide(i) && IsBattlerAlive(i) && gBattleMons[i].item != ITEM_NONE)
                return TRUE;
        }
        return FALSE;
    case ABILITY_DRIZZLE:
        return !(weather & B_WEATHER_RAIN);
    case ABILITY_DROUGHT:
    case ABILITY_ORICHALCUM_PULSE:
        return !(weather & B_WEATHER_SUN);
    case ABILITY_SAND_STREAM:
        return !(weather & B_WEATHER_SANDSTORM);
    case ABILITY_SNOW_WARNING:
        return !(weather & B_WEATHER_ICY_ANY);
    case ABILITY_ELECTRIC_SURGE:
    case ABILITY_HADRON_ENGINE:
        return gFieldTimers.terrain != B_TERRAIN_ELECTRIC;
    case ABILITY_GRASSY_SURGE:
        return gFieldTimers.terrain != B_TERRAIN_GRASSY;
    case ABILITY_MISTY_SURGE:
        return gFieldTimers.terrain != B_TERRAIN_MISTY;
    case ABILITY_PSYCHIC_SURGE:
        return gFieldTimers.terrain != B_TERRAIN_PSYCHIC;
    default:
        return FALSE;
    }
}

void Scout_BeliefTurnStart(void)
{
    enum BattlerId battler;

    if (!BeliefActive() || IsNeutralizingGasOnField())
        return;
    for (battler = 0; battler < gBattlersCount; battler++)
    {
        struct ScoutBelief *b;
        enum Ability list[NUM_ABILITY_SLOTS];
        u8 slot[NUM_ABILITY_SLOTS];
        u32 i, k, n, mask, all = 0;

        if (!IsOnPlayerSide(battler) || !IsBattlerAlive(battler) || RevealedAbility(battler) != ABILITY_NONE)
            continue;
        b = BeliefFor(battler);
        if (b == NULL || gSpeciesInfo[b->species].isMegaEvolution)
            continue;
        n = SpeciesAbilities(b->species, list, slot);
        mask = b->abilityExcluded;
        for (i = 0; i < n; i++)
        {
            all |= 1u << slot[i];
            if (EntryAbilityWouldShow(list[i]))
                mask |= 1u << slot[i];
        }
        // Escluse tutte: qualcosa l'ha soppressa (Gastro Acid, Skill Swap...): meglio non dedurre nulla.
        if ((mask & all) == all)
            continue;
        // La stessa abilità in due slot va esclusa in entrambi.
        for (i = 0; i < NUM_ABILITY_SLOTS; i++)
        {
            for (k = 0; k < n; k++)
            {
                if (gSpeciesInfo[b->species].abilities[i] == list[k] && (mask & (1u << slot[k])))
                    mask |= 1u << i;
            }
        }
        b->abilityExcluded = mask;
    }
}

// --- Lettura del belief ----------------------------------------------------------------------
bool32 Scout_BattlerSpeedRange(enum BattlerId battler, u16 *lo, u16 *hi)
{
    const struct ScoutBelief *b = BeliefPeek(battler);

    if (b == NULL || b->speedHi == 0)
    {
        *lo = *hi = 0;
        if (battler < gBattlersCount)
            Scout_SpeciesSpeedRange(gBattleMons[battler].species, gBattleMons[battler].level, lo, hi);
        return FALSE;
    }
    *lo = b->speedLo;
    *hi = b->speedHi;
    return TRUE;
}

u32 Scout_BattlerMaxSpeed(enum BattlerId battler, enum Ability ability, u32 holdEffect)
{
    u16 lo, hi;
    // Massima possibile per la specie (31 IV, 252 EV, natura +): pubblica. Non l'intervallo del belief, che dopo
    // l'ordine di turno visto si è già allargato fino alla velocità osservata.
    Scout_SpeciesSpeedRange(gBattleMons[battler].species, gBattleMons[battler].level, &lo, &hi);
    return SpeedWithRaw(battler, hi, ability, holdEffect);
}

void Scout_EstimateBattlerStats(enum BattlerId battler, u16 *hp, u16 stats[NUM_STATS])
{
    u16 species = gBattleMons[battler].species;
    u32 level = gBattleMons[battler].level;
    u16 lo, hi;

    Scout_EstimateStats(species, level, hp, stats);
    if (!Scout_BattlerSpeedRange(battler, &lo, &hi))
        return;
    if (stats[STAT_SPEED] < lo || stats[STAT_SPEED] > hi)
    {
        // La spread più usata non torna con l'ordine di turno visto: prova le altre, poi limita la Velocità.
        struct ScoutSpread spreads[VGC_META_SPREADS];
        u32 i, n = Scout_GetMetaSpreads(species, spreads, VGC_META_SPREADS);
        for (i = 1; i < n; i++)
        {
            u16 alt[NUM_STATS];
            Scout_StatsFromSpread(species, level, &spreads[i], alt);
            if (alt[STAT_SPEED] >= lo && alt[STAT_SPEED] <= hi)
            {
                memcpy(stats, alt, sizeof(alt));
                *hp = alt[STAT_HP];
                return;
            }
        }
        stats[STAT_SPEED] = (stats[STAT_SPEED] < lo) ? lo : hi;
    }
}

u32 Scout_BattlerBelievedSpeed(enum BattlerId battler, enum Ability ability, enum HoldEffect holdEffect)
{
    u16 hp, stats[NUM_STATS];

    Scout_EstimateBattlerStats(battler, &hp, stats);
    return SpeedWithRaw(battler, stats[STAT_SPEED], ability, holdEffect);
}

enum HoldEffect Scout_BattlerDeducedHoldEffect(enum BattlerId battler)
{
    const struct ScoutBelief *b = BeliefPeek(battler);
    return b != NULL ? b->heldEffect : HOLD_EFFECT_NONE;
}

bool32 Scout_BattlerHoldEffectExcluded(enum BattlerId battler, enum HoldEffect holdEffect)
{
    const struct ScoutBelief *b = BeliefPeek(battler);
    u32 bit = NotBitFor(holdEffect);
    return b != NULL && bit != 0 && (b->itemNot & bit);
}

bool32 Scout_BattlerOffenseBoosted(enum BattlerId battler)
{
    const struct ScoutBelief *b = BeliefPeek(battler);
    return b != NULL && (b->flags & SCOUT_BF_OFFENSE_BOOST);
}

u32 Scout_BattlerAbilityChance(enum BattlerId battler, enum Ability ability)
{
    const struct ScoutBelief *b;
    enum Ability known;

    if (battler >= gBattlersCount)
        return 0;
    known = RevealedAbility(battler);
    if (known != ABILITY_NONE)
        return known == ability ? 100 : 0;
    b = BeliefPeek(battler);
    return AbilityChanceMasked(gBattleMons[battler].species, ability, b != NULL ? b->abilityExcluded : 0);
}

enum Ability Scout_BattlerLikelyAbility(enum BattlerId battler, u32 *chance)
{
    enum Ability list[NUM_ABILITY_SLOTS], best = ABILITY_NONE, known;
    u8 slot[NUM_ABILITY_SLOTS];
    u32 i, n, bestP = 0;

    if (battler >= gBattlersCount)
    {
        if (chance != NULL)
            *chance = 0;
        return ABILITY_NONE;
    }
    known = RevealedAbility(battler);
    if (known != ABILITY_NONE)
    {
        if (chance != NULL)
            *chance = 100;
        return known;
    }
    n = SpeciesAbilities(gBattleMons[battler].species, list, slot);
    for (i = 0; i < n; i++)
    {
        u32 p = Scout_BattlerAbilityChance(battler, list[i]);
        if (p > bestP)
        {
            bestP = p;
            best = list[i];
        }
    }
    if (chance != NULL)
        *chance = bestP;
    return best;
}

u32 Scout_BattlerMoveChance(enum BattlerId battler, enum Move move)
{
    enum Move *moves = GetMovesArray(battler); // per il giocatore: solo le mosse viste
    const struct ScoutBelief *b;
    u32 i, seen = 0;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] == MOVE_NONE || moves[i] == MOVE_UNAVAILABLE)
            continue;
        if (moves[i] == move)
            return 100;
        seen++;
    }
    if (seen >= MAX_MON_MOVES)
        return 0;
    // Choice dedotta che da quando è entrato ha già usato un'altra mossa: questa non la userà.
    b = BeliefPeek(battler);
    if (b != NULL && IsChoiceEffect(b->heldEffect) && b->lastMove != MOVE_NONE && b->lastMove != move
     && gAiPartyData != NULL && b->lastSwitchIn == KnownMon(battler)->switchInCount)
        return 0;
    return Scout_MoveChance(gBattleMons[battler].species, move);
}

// --- Bo3 ----------------------------------------------------------------------------------------
void Scout_BeliefExport(u32 partySlot, struct ScoutBeliefSnapshot *out)
{
    const struct ScoutBelief *b;

    memset(out, 0, sizeof(*out));
    if (partySlot >= PARTY_SIZE)
        return;
    b = &sScoutBelief[partySlot];
    out->species = b->species;
    out->speedLo = b->speedLo;
    out->speedHi = b->speedHi;
    out->itemNot = b->itemNot;
    out->abilityExcluded = b->abilityExcluded;
    out->heldEffect = (b->flags & SCOUT_BF_ITEM_DEDUCED) ? b->heldEffect : HOLD_EFFECT_NONE;
}

void Scout_BeliefImport(u32 partySlot, const struct ScoutBeliefSnapshot *in)
{
    struct ScoutBelief *b;

    if (partySlot >= PARTY_SIZE || in->species == SPECIES_NONE)
        return;
    b = &sScoutBelief[partySlot];
    memset(b, 0, sizeof(*b));
    b->species = in->species;
    b->speedLo = in->speedLo;
    b->speedHi = in->speedHi;
    b->itemNot = in->itemNot;
    b->abilityExcluded = in->abilityExcluded;
    b->heldEffect = in->heldEffect;
    if (in->heldEffect != HOLD_EFFECT_NONE)
        b->flags |= SCOUT_BF_ITEM_DEDUCED;
}
