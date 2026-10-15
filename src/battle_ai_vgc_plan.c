// Pianificatore del turno per i doppi VGC (AI_FLAG_VGC_DOUBLES): ricerca ad albero, a pezzi.
//
// Le regole scritte (battle_ai_vgc.c) dicono cosa è "sensato" e danno i punteggi; ma nessuna lista
// di regole copre ogni situazione. Qui l'IA SIMULA: per ognuna delle sue azioni candidate (mossa +
// bersaglio tra le meglio valutate, più il cambio) e per ognuna delle azioni più probabili del
// giocatore, applica il turno (ordine di velocità, Fake Out, Protect, Segui Me, Aiuto, danni, sonno,
// controllo velocità, meteo, cambi, Baccacedro) e poi, dalla posizione che ne esce, guarda il TURNO
// DOPO (le nostre risposte migliori contro le loro) e, in forma ridotta, anche il terzo. Il valore di
// ogni ramo è la posizione finale: PS e KO, chi dorme, chi è potenziato, chi si muove per primo, chi
// rischia il KO. Si sceglie l'azione con la migliore media pesata (70%) + caso peggiore (30%).
//
// Il lavoro è spezzato in passi (AI_VgcPlanStep) eseguiti un po' per fotogramma mentre il giocatore
// sceglie: il controller avversario (battle_controller_opponent.c) chiama Begin, poi Step ogni
// fotogramma, e consegna la scelta solo quando il pianificatore ha finito.
//
// Nessun imbroglio. Del giocatore si usano: mosse viste; mosse che la specie PUÒ imparare e che in
// VGC porta di solito (STAB più forte, Fake Out, Distorsione, Tailwind, Spora, Segui Me...) con
// peso ridotto e danno stimato; strumenti noti o probabili; Protect e cambio con probabilità.
#include "global.h"
#include "battle.h"
#include "battle_ai_main.h"
#include "battle_ai_util.h"
#include "battle_ai_vgc.h"
#include "battle_ai_scout.h"
#include "battle_main.h"
#include "battle_util.h"
#include "fpmath.h"
#include "move.h"
#include "pokemon.h"
#include "random.h"
#include "config/custom.h"
#include "constants/abilities.h"
#include "constants/battle_ai.h"
#include "constants/battle_move_effects.h"
#include "constants/hold_effects.h"
#include "constants/moves.h"

#ifndef CUSTOM_VGC_PLAN_DEPTH
#define CUSTOM_VGC_PLAN_DEPTH 3
#endif
#ifndef CUSTOM_VGC_PLAN_SIMS_PER_FRAME
#define CUSTOM_VGC_PLAN_SIMS_PER_FRAME 24
#endif

#define PLAN_MAX_OURS        7   // azioni candidate nostre al primo turno (6 dalle regole + cambio)
#define PLAN_FOE_PER         2   // azioni per avversario (primo turno)
#define PLAN_P2_OURS_PER     2   // azioni per nostro Pokémon al turno dopo
#define PLAN_P2_FOE_PER      2   // azioni per avversario al turno dopo
#define PLAN_OVERRIDE_MARGIN 12  // quanto deve essere migliore (punti di posizione) per cambiare scelta
#define PLAN_CAND_MAX        12

enum PlanKind
{
    PLAN_ACT_MOVE,
    PLAN_ACT_PROTECT,
    PLAN_ACT_SWITCH,
    PLAN_ACT_NONE,
};

enum PlanWeather
{
    PW_NONE,
    PW_SUN,
    PW_RAIN,
    PW_SAND,
    PW_SNOW,
};

struct PlanAction
{
    u8 kind;
    u8 moveIndex;       // 0xFF = mossa stimata (non nella tabella dei danni)
    s8 partyIdx;        // per PLAN_ACT_SWITCH: chi entra
    enum Move move;
    enum BattlerId user;
    enum BattlerId target;
    s32 weight;
};

struct PlanMon
{
    s32 hp;
    s32 maxHp;
    u16 species;
    u32 speed;          // con strumento assunto (in campo) o base (panchina)
    u16 atk, spa, def, spd;
    enum Type t1, t2;
    enum Ability ability;
    u16 holdEffect;     // noto o assunto
    s8 partyIdx;        // -1 = il Pokémon in campo all'inizio (vale la tabella dei danni)
    u8 alive;
    u8 asleep;
    u8 protecting;
    u8 wideGuard;
    u8 flinched;
    u8 redirect;
    u8 helping;
    u8 berryUsed;
    s8 speedStage;
    s8 boost;
};

struct PlanState
{
    struct PlanMon m[MAX_BATTLERS_COUNT];
    u8 trickRoom;
    u8 tailwind[2];
    u8 weather;         // PW_*
    u8 firstTurn;       // turno simulato = turno attuale (Fake Out vale)
};

// Lavoro in corso (uno alla volta).
struct PlanJob
{
    u8 active;
    u8 finished;
    enum BattlerId me;
    u8 origMove;
    enum BattlerId origTarget;
    struct PlanAction ours[PLAN_MAX_OURS], partner, foeA[PLAN_FOE_PER], foeB[PLAN_FOE_PER];
    u8 nOurs, nA, nB;
    struct PlanState base;
    // livello 1
    u8 i, a, b, phase;
    s32 sum, wsum, worst;
    s32 ev[PLAN_MAX_OURS];
    struct PlanState s1;
    // livello 2 (turno dopo)
    struct PlanState la;
    struct PlanAction mine[PLAN_P2_OURS_PER], pa[PLAN_P2_OURS_PER], fa[PLAN_P2_FOE_PER], fb[PLAN_P2_FOE_PER];
    u8 nM, nP, nA2, nB2;
    u8 i2, j2, a2, b2;
    s32 laBest, laSum, laWsum, laWorst;
    // risultato
    u8 resultKind;
    u8 resultMove;
    s8 resultParty;
    enum BattlerId resultTarget;
};

static EWRAM_DATA struct PlanJob sJob = {0};

// ---------------------------------------------------------------------
// Stato
// ---------------------------------------------------------------------
static u8 WeatherFromAbility(enum Ability ab)
{
    switch (ab)
    {
    case ABILITY_DROUGHT: case ABILITY_ORICHALCUM_PULSE: case ABILITY_DESOLATE_LAND: return PW_SUN;
    case ABILITY_DRIZZLE: case ABILITY_PRIMORDIAL_SEA: return PW_RAIN;
    case ABILITY_SAND_STREAM: case ABILITY_SAND_SPIT: return PW_SAND;
    case ABILITY_SNOW_WARNING: return PW_SNOW;
    default: return PW_NONE;
    }
}

static u8 CurrentWeather(void)
{
    if (gBattleWeather & B_WEATHER_SUN) return PW_SUN;
    if (gBattleWeather & B_WEATHER_RAIN) return PW_RAIN;
    if (gBattleWeather & B_WEATHER_SANDSTORM) return PW_SAND;
    if (gBattleWeather & (B_WEATHER_HAIL | B_WEATHER_SNOW)) return PW_SNOW;
    return PW_NONE;
}

static void LoadFieldMon(struct PlanMon *pm, enum BattlerId b)
{
    pm->hp = gBattleMons[b].hp;
    pm->maxHp = gBattleMons[b].maxHP;
    pm->species = gBattleMons[b].species;
    pm->speed = gAiLogicData->speedStats[b];
    pm->atk = gBattleMons[b].attack;
    pm->spa = gBattleMons[b].spAttack;
    pm->def = gBattleMons[b].defense;
    pm->spd = gBattleMons[b].spDefense;
    pm->t1 = gBattleMons[b].types[0];
    pm->t2 = gBattleMons[b].types[1];
    pm->ability = gAiLogicData->abilities[b];
    pm->holdEffect = gAiLogicData->holdEffects[b];
    pm->partyIdx = -1;
    pm->alive = IsBattlerAlive(b);
    pm->asleep = (gBattleMons[b].status1 & STATUS1_SLEEP) != 0;
    pm->berryUsed = (gBattleMons[b].item == ITEM_NONE);
}

static void LoadPartyMon(struct PlanState *st, enum BattlerId b, u32 partyIdx)
{
    struct PlanMon *pm = &st->m[b];
    struct Pokemon *mon = &GetBattlerParty(b)[partyIdx];
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    u8 w;

    pm->hp = GetMonData(mon, MON_DATA_HP);
    pm->maxHp = GetMonData(mon, MON_DATA_MAX_HP);
    pm->species = species;
    pm->speed = GetMonData(mon, MON_DATA_SPEED);
    pm->atk = GetMonData(mon, MON_DATA_ATK);
    pm->spa = GetMonData(mon, MON_DATA_SPATK);
    pm->def = GetMonData(mon, MON_DATA_DEF);
    pm->spd = GetMonData(mon, MON_DATA_SPDEF);
    pm->t1 = gSpeciesInfo[species].types[0];
    pm->t2 = gSpeciesInfo[species].types[1];
    pm->ability = GetMonAbility(mon);
    if (IsOnPlayerSide(b))
    {
        struct ScoutItemGuess g;
        Scout_GuessItem(species, &g);
        pm->holdEffect = (g.chance >= 50) ? g.effect : HOLD_EFFECT_NONE;
        if (pm->holdEffect == HOLD_EFFECT_CHOICE_SCARF)
            pm->speed = pm->speed * 3 / 2;
    }
    else
    {
        pm->holdEffect = GetItemHoldEffect(GetMonData(mon, MON_DATA_HELD_ITEM));
        if (pm->holdEffect == HOLD_EFFECT_CHOICE_SCARF)
            pm->speed = pm->speed * 3 / 2;
    }
    pm->partyIdx = partyIdx;
    pm->alive = pm->hp > 0;
    pm->asleep = (GetMonData(mon, MON_DATA_STATUS) & STATUS1_SLEEP) != 0;
    pm->protecting = pm->wideGuard = pm->flinched = pm->redirect = pm->helping = 0;
    pm->berryUsed = FALSE;
    pm->speedStage = pm->boost = 0;
    // Meteo all'entrata (abilità).
    w = WeatherFromAbility(pm->ability);
    if (w != PW_NONE)
        st->weather = w;
}

static void InitState(struct PlanState *st)
{
    enum BattlerId b;

    memset(st, 0, sizeof(*st));
    for (b = 0; b < gBattlersCount; b++)
        LoadFieldMon(&st->m[b], b);
    st->trickRoom = (gFieldStatuses & STATUS_FIELD_TRICK_ROOM) != 0;
    st->tailwind[B_SIDE_PLAYER] = (gSideStatuses[B_SIDE_PLAYER] & SIDE_STATUS_TAILWIND) != 0;
    st->tailwind[B_SIDE_OPPONENT] = (gSideStatuses[B_SIDE_OPPONENT] & SIDE_STATUS_TAILWIND) != 0;
    st->weather = CurrentWeather();
    st->firstTurn = TRUE;
}

static void NextTurn(struct PlanState *st)
{
    enum BattlerId b;
    for (b = 0; b < gBattlersCount; b++)
        st->m[b].protecting = st->m[b].wideGuard = st->m[b].flinched = st->m[b].redirect = st->m[b].helping = 0;
    st->firstTurn = FALSE;
}

static u32 EffSpeed(const struct PlanState *st, enum BattlerId b)
{
    const struct PlanMon *pm = &st->m[b];
    u32 s = pm->speed;
    s32 stage = pm->speedStage;

    if (stage < 0)
        s = s * 2 / (2 - stage);
    else if (stage > 0)
        s = s * (2 + stage) / 2;
    if (st->tailwind[GetBattlerSide(b)])
        s *= 2;
    if ((st->weather == PW_RAIN && pm->ability == ABILITY_SWIFT_SWIM) || (st->weather == PW_SUN && pm->ability == ABILITY_CHLOROPHYLL)
     || (st->weather == PW_SAND && pm->ability == ABILITY_SAND_RUSH) || (st->weather == PW_SNOW && pm->ability == ABILITY_SLUSH_RUSH))
        s *= 2;
    return s;
}

static bool32 FasterNextTurn(const struct PlanState *st, enum BattlerId a, enum BattlerId b)
{
    bool32 f = EffSpeed(st, a) > EffSpeed(st, b);
    return st->trickRoom ? !f : f;
}

// ---------------------------------------------------------------------
// Danni
// ---------------------------------------------------------------------
static uq4_12_t TypeEff(enum Type moveType, const struct PlanMon *def)
{
    uq4_12_t e = GetTypeModifier(moveType, def->t1);
    if (def->t2 != def->t1)
        e = uq4_12_multiply(e, GetTypeModifier(moveType, def->t2));
    return e;
}

static u32 WeatherFactor(u8 weather, enum Type type)
{
    if (weather == PW_SUN)
        return type == TYPE_FIRE ? 150 : (type == TYPE_WATER ? 50 : 100);
    if (weather == PW_RAIN)
        return type == TYPE_WATER ? 150 : (type == TYPE_FIRE ? 50 : 100);
    return 100;
}

// Stima "da giocatore" per mosse o Pokémon fuori dalla tabella dei danni dell'IA.
static s32 EstimateDamage(const struct PlanState *st, const struct PlanMon *atk, const struct PlanMon *def, enum Move move)
{
    u32 power = GetMovePower(move);
    enum Type type = GetMoveType(move);
    bool32 physical = GetMoveCategory(move) == DAMAGE_CATEGORY_PHYSICAL;
    u32 a = physical ? atk->atk : atk->spa;
    u32 d = physical ? def->def : def->spd;
    u32 dmg;

    if (power == 0 || IsBattleMoveStatus(move))
        return 0;
    if (power == 1)
        power = 60;
    if (def->holdEffect == HOLD_EFFECT_ASSAULT_VEST && !physical)
        d = d * 3 / 2;
    if (def->holdEffect == HOLD_EFFECT_EVIOLITE)
        d = d * 3 / 2;
    if (d == 0)
        d = 1;
    dmg = ((2 * 50 / 5 + 2) * power * a / d) / 50 + 2;
    if (type == atk->t1 || type == atk->t2)
        dmg = dmg * 3 / 2;
    dmg = uq4_12_multiply_by_int_half_down(TypeEff(type, def), dmg);
    dmg = dmg * WeatherFactor(st->weather, type) / 100;
    if (atk->holdEffect == HOLD_EFFECT_LIFE_ORB)
        dmg = dmg * 13 / 10;
    if (IsSpreadMove(GetMoveTarget(move)))
        dmg = dmg * 3 / 4;
    return dmg * 92 / 100;
}

static s32 DamageOf(const struct PlanState *st, const struct PlanAction *a, enum BattlerId t)
{
    const struct PlanMon *am = &st->m[a->user], *dm = &st->m[t];
    s32 dmg;

    if (a->moveIndex != 0xFF && am->partyIdx < 0 && dm->partyIdx < 0)
    {
        u8 now = CurrentWeather();
        if (gAiLogicData->effectiveness[a->user][t][a->moveIndex] == UQ_4_12(0.0))
            return 0;
        dmg = gAiLogicData->simulatedDmg[a->user][t][a->moveIndex].median;
        // La tabella include il meteo attuale: se nella simulazione è cambiato, si riscala.
        if (st->weather != now)
            dmg = dmg * WeatherFactor(st->weather, GetMoveType(a->move)) / WeatherFactor(now, GetMoveType(a->move));
    }
    else
    {
        dmg = EstimateDamage(st, am, dm, a->move);
    }
    if (am->helping)
        dmg = dmg * 3 / 2;
    if (am->boost > 0)
        dmg = dmg * (2 + am->boost) / 2;
    return dmg;
}

// ---------------------------------------------------------------------
// Applicazione di un'azione
// ---------------------------------------------------------------------
static s32 ActionPriority(const struct PlanAction *a)
{
    switch (a->kind)
    {
    case PLAN_ACT_SWITCH:  return 9;
    case PLAN_ACT_PROTECT: return 4;
    case PLAN_ACT_NONE:    return -8;
    default:               return GetBattleMovePriority(a->user, gAiLogicData->abilities[a->user], a->move);
    }
}

static bool32 MovesFirst(const struct PlanState *st, const struct PlanAction *a, const struct PlanAction *b)
{
    s32 pa = ActionPriority(a), pb = ActionPriority(b);
    u32 sa, sb;

    if (pa != pb)
        return pa > pb;
    sa = EffSpeed(st, a->user);
    sb = EffSpeed(st, b->user);
    if (st->trickRoom)
    {
        u32 t = sa; sa = sb; sb = t;
    }
    if (sa == sb)
        return IsOnPlayerSide(a->user); // pareggio: pessimisti, il giocatore è più veloce
    return sa > sb;
}

static void TakeDamage(struct PlanMon *tm, s32 dmg)
{
    // Focus Sash noto/assunto: regge il primo colpo a PS pieni.
    if (tm->hp == tm->maxHp && dmg >= tm->hp && tm->holdEffect == HOLD_EFFECT_FOCUS_SASH && !tm->berryUsed)
    {
        dmg = tm->hp - 1;
        tm->berryUsed = TRUE;
    }
    tm->hp -= dmg;
    if (tm->hp <= 0)
    {
        tm->hp = 0;
        tm->alive = FALSE;
        return;
    }
    // Baccacedro nota/assunta: +25% sotto metà PS, una volta.
    if (tm->holdEffect == HOLD_EFFECT_RESTORE_HP && !tm->berryUsed && tm->hp * 2 <= tm->maxHp)
    {
        tm->hp += tm->maxHp / 4;
        if (tm->hp > tm->maxHp)
            tm->hp = tm->maxHp;
        tm->berryUsed = TRUE;
    }
}

static void ApplyAction(struct PlanState *st, const struct PlanAction *a)
{
    enum BattlerId u = a->user;
    struct PlanMon *um = &st->m[u];
    enum BattleMoveEffects effect;
    enum MoveTarget tt;

    if (a->kind == PLAN_ACT_NONE)
        return;
    if (a->kind == PLAN_ACT_SWITCH)
    {
        if (a->partyIdx >= 0 && a->partyIdx < PARTY_SIZE)
            LoadPartyMon(st, u, a->partyIdx);
        return;
    }
    if (!um->alive || um->flinched || um->asleep)
        return;
    if (a->kind == PLAN_ACT_PROTECT)
    {
        um->protecting = TRUE;
        return;
    }

    effect = GetMoveEffect(a->move);
    tt = GetMoveTarget(a->move);
    switch (effect)
    {
    case EFFECT_PROTECT:
        if (GetMoveProtectMethod(a->move) == PROTECT_WIDE_GUARD)
            um->wideGuard = st->m[GetPartnerBattler(u)].wideGuard = TRUE;
        else if (GetMoveProtectMethod(a->move) != PROTECT_QUICK_GUARD && GetMoveProtectMethod(a->move) != PROTECT_CRAFTY_SHIELD)
            um->protecting = TRUE;
        return;
    case EFFECT_FOLLOW_ME:
        um->redirect = TRUE;
        return;
    case EFFECT_HELPING_HAND:
        st->m[GetPartnerBattler(u)].helping = TRUE;
        return;
    case EFFECT_TRICK_ROOM:
        st->trickRoom = !st->trickRoom;
        return;
    case EFFECT_TAILWIND:
        st->tailwind[GetBattlerSide(u)] = TRUE;
        return;
    case EFFECT_WEATHER:
        switch (GetMoveWeatherType(a->move))
        {
        case BATTLE_WEATHER_SUN:       st->weather = PW_SUN; break;
        case BATTLE_WEATHER_RAIN:      st->weather = PW_RAIN; break;
        case BATTLE_WEATHER_SANDSTORM: st->weather = PW_SAND; break;
        case BATTLE_WEATHER_HAIL:
        case BATTLE_WEATHER_SNOW:      st->weather = PW_SNOW; break;
        default: break;
        }
        return;
    case EFFECT_BELLY_DRUM:
        um->boost += 6;
        um->hp -= um->maxHp / 2;
        if (um->hp <= 0)
            um->hp = 1;
        return;
    default:
        break;
    }

    if (IsBattleMoveStatus(a->move))
    {
        struct PlanMon *tm = &st->m[a->target];
        if (IsStatRaisingMove(a->move) && tt == TARGET_USER)
        {
            um->boost += 2;
        }
        else if (GetMoveNonVolatileStatus(a->move) == MOVE_EFFECT_SLEEP && tm->alive && !tm->protecting && !tm->asleep)
        {
            bool32 ok = TRUE;
            if (IsPowderMove(a->move) && (tm->t1 == TYPE_GRASS || tm->t2 == TYPE_GRASS || tm->ability == ABILITY_OVERCOAT
                                        || tm->holdEffect == HOLD_EFFECT_SAFETY_GOGGLES))
                ok = FALSE;
            if (tm->ability == ABILITY_INSOMNIA || tm->ability == ABILITY_VITAL_SPIRIT || tm->ability == ABILITY_SWEET_VEIL
             || tm->ability == ABILITY_GOOD_AS_GOLD || tm->ability == ABILITY_MAGIC_BOUNCE || tm->ability == ABILITY_PURIFYING_SALT
             || tm->ability == ABILITY_COMATOSE)
                ok = FALSE;
            if (ok)
                tm->asleep = TRUE;
        }
        return;
    }

    // Mossa d'attacco.
    {
        enum BattlerId targets[2];
        u32 n = 0, i;
        bool32 spread = IsSpreadMove(tt);

        if (spread)
        {
            enum BattlerId fl = GetBattlerLeftFoe(u), fr = GetBattlerRightFoe(u);
            if (st->m[fl].alive) targets[n++] = fl;
            if (st->m[fr].alive) targets[n++] = fr;
        }
        else
        {
            enum BattlerId t = a->target;
            if (!IsBattlerAlly(u, t))
            {
                enum BattlerId fl = GetBattlerLeftFoe(u), fr = GetBattlerRightFoe(u);
                if (st->m[fl].alive && st->m[fl].redirect) t = fl;
                else if (st->m[fr].alive && st->m[fr].redirect) t = fr;
            }
            if (!st->m[t].alive)
            {
                enum BattlerId other = GetPartnerBattler(t);
                if (!IsBattlerAlly(u, t) && st->m[other].alive)
                    t = other;
                else
                    return;
            }
            targets[n++] = t;
        }

        for (i = 0; i < n; i++)
        {
            enum BattlerId t = targets[i];
            struct PlanMon *tm = &st->m[t];
            s32 dmg;

            if (tm->protecting || (spread && tm->wideGuard))
                continue;
            dmg = DamageOf(st, a, t);
            if (dmg <= 0)
                continue;
            if (effect == EFFECT_FIRST_TURN_ONLY && st->firstTurn && IsBattlersFirstTurn(u) && tm->ability != ABILITY_INNER_FOCUS)
                tm->flinched = TRUE;
            if (a->move == MOVE_ICY_WIND || a->move == MOVE_ELECTROWEB || a->move == MOVE_BULLDOZE)
                tm->speedStage--;
            TakeDamage(tm, dmg);
        }
        if (effect == EFFECT_FINAL_GAMBIT)
        {
            um->hp = 0;
            um->alive = FALSE;
        }
    }
}

// ---------------------------------------------------------------------
// Valutazione della posizione
// ---------------------------------------------------------------------
static bool32 CanKoNextTurn(const struct PlanState *st, enum BattlerId atk, enum BattlerId def)
{
    const struct PlanMon *am = &st->m[atk], *dm = &st->m[def];
    u32 i;

    if (am->partyIdx < 0 && dm->partyIdx < 0)
    {
        enum Move *moves = GetMovesArray(atk);
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            struct PlanAction tmp = { PLAN_ACT_MOVE, (u8)i, -1, moves[i], atk, def, 0 };
            if (moves[i] == MOVE_NONE || moves[i] == MOVE_UNAVAILABLE || IsBattleMoveStatus(moves[i]))
                continue;
            if (DamageOf(st, &tmp, def) >= dm->hp)
                return TRUE;
        }
        return FALSE;
    }
    else
    {
        enum Move cand[16];
        u32 n = Scout_CollectLearnableAttacks(am->species, cand, ARRAY_COUNT(cand));
        for (i = 0; i < n; i++)
        {
            if (GetMoveType(cand[i]) != am->t1 && GetMoveType(cand[i]) != am->t2)
                continue;
            if (EstimateDamage(st, am, dm, cand[i]) >= dm->hp)
                return TRUE;
        }
        return FALSE;
    }
}

static s32 Evaluate(const struct PlanState *st)
{
    enum BattlerId b, o;
    s32 v = 0;
    s32 ourFirst = 0, theirFirst = 0;

    for (b = 0; b < gBattlersCount; b++)
    {
        s32 side = IsOnPlayerSide(b) ? -1 : 1;
        const struct PlanMon *pm = &st->m[b];

        if (pm->species == SPECIES_NONE)
            continue;
        if (!pm->alive)
        {
            v -= side * 160;
            continue;
        }
        v += side * (pm->hp * 100 / max(pm->maxHp, 1) + 40);
        if (pm->asleep)
            v -= side * 60;
        if (pm->boost > 0)
            v += side * 25 * min(pm->boost, 4);
    }
    for (b = 0; b < gBattlersCount; b++)
    {
        if (!st->m[b].alive || st->m[b].species == SPECIES_NONE)
            continue;
        for (o = 0; o < gBattlersCount; o++)
        {
            if (o == b || !st->m[o].alive || IsBattlerAlly(b, o) || st->m[o].species == SPECIES_NONE)
                continue;
            if (FasterNextTurn(st, b, o))
            {
                if (IsOnPlayerSide(b)) theirFirst++; else ourFirst++;
            }
        }
    }
    v += 12 * ourFirst - 12 * theirFirst;
    for (b = 0; b < gBattlersCount; b++)
    {
        if (!st->m[b].alive || st->m[b].species == SPECIES_NONE)
            continue;
        for (o = 0; o < gBattlersCount; o++)
        {
            if (!st->m[o].alive || IsBattlerAlly(b, o) || st->m[o].asleep || st->m[o].species == SPECIES_NONE)
                continue;
            if (CanKoNextTurn(st, o, b))
            {
                s32 pen = FasterNextTurn(st, o, b) ? 45 : 20;
                v += IsOnPlayerSide(b) ? pen / 2 : -pen;
                break;
            }
        }
    }
    return v;
}

// ---------------------------------------------------------------------
// Azioni candidate (generiche, dallo stato)
// ---------------------------------------------------------------------
static void AddCand(struct PlanAction *list, u32 *n, u32 max, u8 kind, u8 moveIndex, s8 partyIdx,
                    enum Move move, enum BattlerId user, enum BattlerId target, s32 weight)
{
    if (*n >= max)
        return;
    list[*n].kind = kind; list[*n].moveIndex = moveIndex; list[*n].partyIdx = partyIdx;
    list[*n].move = move; list[*n].user = user; list[*n].target = target; list[*n].weight = weight;
    (*n)++;
}

static s32 AttackWeight(const struct PlanState *st, const struct PlanAction *a, enum BattlerId t)
{
    s32 dmg = DamageOf(st, a, t);
    s32 w = dmg * 100 / max(st->m[t].maxHp, 1);
    if (dmg >= st->m[t].hp)
        w += 80;
    if (GetMoveEffect(a->move) == EFFECT_FIRST_TURN_ONLY && st->firstTurn && IsBattlersFirstTurn(a->user))
        w += 60;
    return w;
}

// Azioni plausibili di un battler nello stato `st`. isOurs: le nostre mosse sono tutte note.
// Per il giocatore: mosse viste + mosse probabili dalla specie (peso ridotto), Protect e cambio con probabilità.
static u32 BuildActions(const struct PlanState *st, enum BattlerId b, struct PlanAction *out, u32 maxOut, bool32 isOurs, bool32 allowSwitch)
{
    struct PlanAction cand[PLAN_CAND_MAX];
    u32 n = 0, i, k;
    const struct PlanMon *bm = &st->m[b];
    enum BattlerId foeL = GetBattlerLeftFoe(b), foeR = GetBattlerRightFoe(b);
    enum BattlerId targets[2] = { foeL, foeR };
    enum Move known[MAX_MON_MOVES];
    u32 nKnown = 0;
    bool32 hasProtect = FALSE;
    u8 seenRole[SCOUT_ROLE_COUNT] = {0};

    if (!bm->alive)
        return 0;

    if (bm->partyIdx < 0)
    {
        enum Move *moves = GetMovesArray(b);
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            if (moves[i] == MOVE_NONE || moves[i] == MOVE_UNAVAILABLE)
                continue;
            if (!isOurs || gBattleMons[b].pp[i] > 0)
                known[nKnown++] = moves[i];
        }
    }
    else if (isOurs)
    {
        struct Pokemon *mon = &GetBattlerParty(b)[bm->partyIdx];
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            enum Move m = GetMonData(mon, MON_DATA_MOVE1 + i);
            if (m != MOVE_NONE)
                known[nKnown++] = m;
        }
    }

    for (i = 0; i < nKnown; i++)
    {
        enum Move m = known[i];
        enum BattleMoveEffects eff = GetMoveEffect(m);
        u8 idx = (bm->partyIdx < 0) ? i : 0xFF;
        u32 r;

        for (r = 0; r < SCOUT_ROLE_COUNT; r++)
            if (Scout_MoveHasRole(m, r))
                seenRole[r] = TRUE;
        if (eff == EFFECT_PROTECT && GetMoveProtectMethod(m) == PROTECT_NORMAL)
        {
            hasProtect = TRUE;
            continue;
        }
        if (IsBattleMoveStatus(m))
        {
            s32 w = 0;
            if (eff == EFFECT_TRICK_ROOM && !st->trickRoom) w = 70;
            else if (eff == EFFECT_TAILWIND && !st->tailwind[GetBattlerSide(b)]) w = 70;
            else if (eff == EFFECT_FOLLOW_ME) w = 45;
            else if (eff == EFFECT_HELPING_HAND) w = 30;
            else if (eff == EFFECT_WEATHER) w = 40;
            else if (eff == EFFECT_PROTECT && GetMoveProtectMethod(m) == PROTECT_WIDE_GUARD) w = 25;
            else if (GetMoveNonVolatileStatus(m) == MOVE_EFFECT_SLEEP) w = 60;
            else if (IsStatRaisingMove(m) && GetMoveTarget(m) == TARGET_USER) w = 35;
            if (w == 0)
                continue;
            if (GetMoveNonVolatileStatus(m) == MOVE_EFFECT_SLEEP)
            {
                for (k = 0; k < 2; k++)
                    if (st->m[targets[k]].alive && !st->m[targets[k]].asleep)
                        AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_MOVE, idx, -1, m, b, targets[k], w);
            }
            else
            {
                AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_MOVE, idx, -1, m, b, b, w);
            }
            continue;
        }
        if (IsSpreadMove(GetMoveTarget(m)))
        {
            struct PlanAction tmp = { PLAN_ACT_MOVE, idx, -1, m, b, foeL, 0 };
            s32 w = 10;
            for (k = 0; k < 2; k++)
                if (st->m[targets[k]].alive)
                    w += AttackWeight(st, &tmp, targets[k]);
            AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_MOVE, idx, -1, m, b, foeL, w);
            continue;
        }
        for (k = 0; k < 2; k++)
        {
            struct PlanAction tmp = { PLAN_ACT_MOVE, idx, -1, m, b, targets[k], 0 };
            if (!st->m[targets[k]].alive)
                continue;
            AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_MOVE, idx, -1, m, b, targets[k], AttackWeight(st, &tmp, targets[k]));
        }
    }

    // Giocatore: mosse probabili ma non viste (specie + meta), con peso ridotto e danno stimato.
    if (!isOurs && nKnown < MAX_MON_MOVES)
    {
        struct ScoutProfile prof;
        u32 scale = (MAX_MON_MOVES - nKnown) * 100 / MAX_MON_MOVES;
        enum Move attacks[16];
        u32 na = Scout_CollectLearnableAttacks(bm->species, attacks, ARRAY_COUNT(attacks));
        enum Move bestStab = MOVE_NONE;
        u32 bestPow = 0;
        bool32 seenStabAttack = FALSE;

        Scout_GetSpeciesProfile(bm->species, &prof);
        for (i = 0; i < na; i++)
        {
            enum Type ty = GetMoveType(attacks[i]);
            if ((ty == bm->t1 || ty == bm->t2) && GetMovePower(attacks[i]) > bestPow && !IsSpreadMove(GetMoveTarget(attacks[i])))
            {
                bestPow = GetMovePower(attacks[i]);
                bestStab = attacks[i];
            }
        }
        for (i = 0; i < nKnown; i++)
            if (!IsBattleMoveStatus(known[i]) && (GetMoveType(known[i]) == bm->t1 || GetMoveType(known[i]) == bm->t2))
                seenStabAttack = TRUE;
        if (bestStab != MOVE_NONE && !seenStabAttack)
        {
            for (k = 0; k < 2; k++)
            {
                struct PlanAction tmp = { PLAN_ACT_MOVE, 0xFF, -1, bestStab, b, targets[k], 0 };
                if (!st->m[targets[k]].alive)
                    continue;
                AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_MOVE, 0xFF, -1, bestStab, b, targets[k],
                        AttackWeight(st, &tmp, targets[k]) * 70 / 100 * scale / 100);
            }
        }
        if (!seenRole[SCOUT_FAKE_OUT] && prof.role[SCOUT_FAKE_OUT] >= 50 && st->firstTurn && IsBattlersFirstTurn(b) && bm->partyIdx < 0)
            for (k = 0; k < 2; k++)
                if (st->m[targets[k]].alive)
                    AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_MOVE, 0xFF, -1, MOVE_FAKE_OUT, b, targets[k], prof.role[SCOUT_FAKE_OUT] * scale / 100);
        if (!seenRole[SCOUT_TRICK_ROOM] && prof.role[SCOUT_TRICK_ROOM] >= 50 && !st->trickRoom)
            AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_MOVE, 0xFF, -1, MOVE_TRICK_ROOM, b, b, prof.role[SCOUT_TRICK_ROOM] * 70 / 100 * scale / 100);
        if (!seenRole[SCOUT_TAILWIND] && prof.role[SCOUT_TAILWIND] >= 50 && !st->tailwind[GetBattlerSide(b)])
            AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_MOVE, 0xFF, -1, MOVE_TAILWIND, b, b, prof.role[SCOUT_TAILWIND] * 70 / 100 * scale / 100);
        if (!seenRole[SCOUT_REDIRECT] && prof.role[SCOUT_REDIRECT] >= 60)
            AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_MOVE, 0xFF, -1, MOVE_FOLLOW_ME, b, b, prof.role[SCOUT_REDIRECT] * 50 / 100 * scale / 100);
        if (!seenRole[SCOUT_SLEEP] && prof.role[SCOUT_SLEEP] >= 60)
            for (k = 0; k < 2; k++)
                if (st->m[targets[k]].alive && !st->m[targets[k]].asleep)
                    AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_MOVE, 0xFF, -1, MOVE_SPORE, b, targets[k], prof.role[SCOUT_SLEEP] * 60 / 100 * scale / 100);
    }

    // Protect: nostro se lo abbiamo; del giocatore con la probabilità stimata.
    if (isOurs)
    {
        if (hasProtect && !bm->protecting)
        {
            s32 w = 20;
            for (k = 0; k < 2; k++)
                if (st->m[targets[k]].alive && CanKoNextTurn(st, targets[k], b))
                    w += 50;
            AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_PROTECT, 0, -1, MOVE_PROTECT, b, b, w);
        }
        // Cambio nostro (turni successivi): verso chi non è in campo, se ha senso.
        if (allowSwitch && CountUsablePartyMons(b) > 0)
        {
            struct Pokemon *party = GetBattlerParty(b);
            enum BattlerId partner = GetPartnerBattler(b);
            for (i = 0; i < PARTY_SIZE; i++)
            {
                if (GetMonData(&party[i], MON_DATA_SPECIES) == SPECIES_NONE || GetMonData(&party[i], MON_DATA_HP) == 0)
                    continue;
                if ((s32)i == bm->partyIdx || gBattlerPartyIndexes[b] == i
                 || (st->m[partner].alive && (gBattlerPartyIndexes[partner] == i || st->m[partner].partyIdx == (s32)i)))
                    continue;
                AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_SWITCH, 0, (s8)i, MOVE_NONE, b, b, 15);
                break;
            }
        }
    }
    else if (bm->partyIdx < 0)
    {
        u32 p = AI_VgcProtectChance(b);
        if (p > 0)
            AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_PROTECT, 0, -1, MOVE_PROTECT, b, b, (s32)p);
        if (allowSwitch)
        {
            u32 sp = AI_VgcSwitchChance(b);
            if (sp >= 20)
            {
                struct Pokemon *party = GetBattlerParty(b);
                enum BattlerId partner = GetPartnerBattler(b);
                s32 bestIdx = -1;
                for (i = 0; i < PARTY_SIZE; i++)
                {
                    const struct AiPartyMon *known2 = &gAiPartyData->mons[B_TRAINER_PLAYER][i];
                    if (known2->species == SPECIES_NONE || known2->isFainted || GetMonData(&party[i], MON_DATA_HP) == 0)
                        continue;
                    if ((s32)i == bm->partyIdx || gBattlerPartyIndexes[b] == i
                     || (st->m[partner].alive && (gBattlerPartyIndexes[partner] == i || st->m[partner].partyIdx == (s32)i)))
                        continue;
                    bestIdx = i;
                    break;
                }
                if (bestIdx >= 0)
                    AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_SWITCH, 0, (s8)bestIdx, MOVE_NONE, b, b, (s32)sp);
            }
        }
    }

    if (n == 0)
        AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_NONE, 0, -1, MOVE_NONE, b, b, 50);

    {
        u32 outN = 0;
        while (outN < maxOut && n > 0)
        {
            u32 best = 0;
            for (i = 1; i < n; i++)
                if (cand[i].weight > cand[best].weight)
                    best = i;
            out[outN++] = cand[best];
            cand[best] = cand[--n];
        }
        return outN;
    }
}

// ---------------------------------------------------------------------
// Simulazione
// ---------------------------------------------------------------------
static void RunTurn(struct PlanState *st, const struct PlanAction **acts, u32 n)
{
    u32 i, j;
    for (i = 0; i < n; i++)
    {
        u32 first = i;
        for (j = i + 1; j < n; j++)
            if (MovesFirst(st, acts[j], acts[first]))
                first = j;
        if (first != i)
        {
            const struct PlanAction *t = acts[i]; acts[i] = acts[first]; acts[first] = t;
        }
        ApplyAction(st, acts[i]);
    }
}

// Terzo turno in forma ridotta: la nostra risposta migliore (una per Pokémon) contro la loro più probabile.
static s32 GreedyValue(const struct PlanState *s2, enum BattlerId me)
{
    struct PlanState st = *s2;
    struct PlanAction mine, pa, fa, fb;
    struct PlanAction none = { PLAN_ACT_NONE, 0, -1, MOVE_NONE, 0, 0, 100 };
    enum BattlerId partner = GetPartnerBattler(me);
    const struct PlanAction *acts[4];

    NextTurn(&st);
    if (BuildActions(&st, me, &mine, 1, TRUE, FALSE) == 0) { mine = none; mine.user = me; }
    if (partner == me || BuildActions(&st, partner, &pa, 1, TRUE, FALSE) == 0) { pa = none; pa.user = partner; }
    if (BuildActions(&st, GetBattlerLeftFoe(me), &fa, 1, FALSE, FALSE) == 0) { fa = none; fa.user = GetBattlerLeftFoe(me); }
    if (BuildActions(&st, GetBattlerRightFoe(me), &fb, 1, FALSE, FALSE) == 0) { fb = none; fb.user = GetBattlerRightFoe(me); }
    acts[0] = &mine; acts[1] = &pa; acts[2] = &fa; acts[3] = &fb;
    RunTurn(&st, acts, 4);
    return Evaluate(&st);
}

// ---------------------------------------------------------------------
// Candidate nostre al primo turno e azione del partner
// ---------------------------------------------------------------------
static u32 BuildOurFirstActions(enum BattlerId me, struct PlanAction *out, u32 chosenMove, enum BattlerId chosenTarget)
{
    s32 best = INT32_MIN;
    u32 n = 0, i, k;
    enum BattlerId t;

    for (t = 0; t < gBattlersCount; t++)
        for (i = 0; i < MAX_MON_MOVES; i++)
            if (gBattleMons[me].moves[i] != MOVE_NONE && t != me && gAiBattleData->finalScore[me][t][i] > best)
                best = gAiBattleData->finalScore[me][t][i];

    for (t = 0; t < gBattlersCount; t++)
    {
        if (t == me || !IsBattlerAlive(t))
            continue;
        for (i = 0; i < MAX_MON_MOVES && n < PLAN_MAX_OURS - 1; i++)
        {
            enum Move m = gBattleMons[me].moves[i];
            s32 sc = gAiBattleData->finalScore[me][t][i];
            bool32 dup = FALSE;
            if (m == MOVE_NONE || gBattleMons[me].pp[i] == 0 || sc <= 0 || sc < best - 14)
                continue;
            if (!CanTargetBattler(me, t, m))
                continue;
            if (GetMoveTarget(m) != TARGET_SELECTED && GetMoveTarget(m) != TARGET_SMART)
                for (k = 0; k < n; k++)
                    if (out[k].kind != PLAN_ACT_SWITCH && out[k].moveIndex == i)
                        dup = TRUE;
            if (dup)
                continue;
            AddCand(out, &n, PLAN_MAX_OURS - 1, (GetMoveEffect(m) == EFFECT_PROTECT && GetMoveProtectMethod(m) == PROTECT_NORMAL) ? PLAN_ACT_PROTECT : PLAN_ACT_MOVE,
                    i, -1, m, me, t, sc);
        }
    }
    {
        bool32 present = FALSE;
        for (k = 0; k < n; k++)
            if (out[k].moveIndex == chosenMove && out[k].target == chosenTarget)
                present = TRUE;
        if (!present && chosenMove < MAX_MON_MOVES && gBattleMons[me].moves[chosenMove] != MOVE_NONE)
        {
            if (n >= PLAN_MAX_OURS - 1)
                n = PLAN_MAX_OURS - 2;
            AddCand(out, &n, PLAN_MAX_OURS - 1, PLAN_ACT_MOVE, chosenMove, -1, gBattleMons[me].moves[chosenMove], me, chosenTarget, best);
        }
    }
    if (gAiLogicData->mostSuitableMonId[me] != PARTY_SIZE && CountUsablePartyMons(me) > 0)
    {
        u32 idx = gAiLogicData->mostSuitableMonId[me];
        enum BattlerId partner = GetPartnerBattler(me);
        if (idx < PARTY_SIZE && idx != gBattlerPartyIndexes[me] && !(IsBattlerAlive(partner) && gBattlerPartyIndexes[partner] == idx)
         && gBattleStruct->AI_monToSwitchIntoId[partner] != idx)
            AddCand(out, &n, PLAN_MAX_OURS, PLAN_ACT_SWITCH, 0, (s8)idx, MOVE_NONE, me, me, 0);
    }
    return n;
}

static void PartnerFirstAction(enum BattlerId me, struct PlanAction *out)
{
    enum BattlerId p = GetPartnerBattler(me);
    struct PlanState st;

    out->kind = PLAN_ACT_NONE; out->moveIndex = 0; out->partyIdx = -1; out->move = MOVE_NONE; out->user = p; out->target = p; out->weight = 0;
    if (p == me || !IsBattlerAlive(p))
        return;
    if (gAiLogicData->battlerMovesScored & (1u << p))
    {
        if (gAiLogicData->shouldSwitch & (1u << p))
        {
            u32 idx = gBattleStruct->AI_monToSwitchIntoId[p];
            if (idx >= PARTY_SIZE)
                idx = gAiLogicData->mostSuitableMonId[p];
            if (idx < PARTY_SIZE)
            {
                out->kind = PLAN_ACT_SWITCH; out->partyIdx = (s8)idx;
            }
            return;
        }
        {
            u32 idx = gAiBattleData->chosenMoveIndex[p];
            if (idx < MAX_MON_MOVES && gBattleMons[p].moves[idx] != MOVE_NONE && gBattleMons[p].moves[idx] != MOVE_UNAVAILABLE)
            {
                enum Move m = gBattleMons[p].moves[idx];
                out->kind = (GetMoveEffect(m) == EFFECT_PROTECT && GetMoveProtectMethod(m) == PROTECT_NORMAL) ? PLAN_ACT_PROTECT : PLAN_ACT_MOVE;
                out->moveIndex = idx; out->move = m; out->target = gAiBattleData->chosenTarget[p];
            }
        }
        return;
    }
    InitState(&st);
    BuildActions(&st, p, out, 1, TRUE, FALSE);
}

// ---------------------------------------------------------------------
// Lavoro a pezzi
// ---------------------------------------------------------------------
static void Accumulate(s32 v, s32 w)
{
    sJob.sum += v * w;
    sJob.wsum += w;
    if (v < sJob.worst)
        sJob.worst = v;
}

static s32 PairWeight(const struct PlanAction *x, const struct PlanAction *y)
{
    return (max(x->weight, 1) * max(y->weight, 1)) / 16 + 1;
}

// Avanza gli indici di livello 1; TRUE se tutto è finito.
static bool32 AdvanceLevel1(void)
{
    sJob.b++;
    if (sJob.b >= sJob.nB)
    {
        sJob.b = 0;
        sJob.a++;
        if (sJob.a >= sJob.nA)
        {
            sJob.a = 0;
            sJob.ev[sJob.i] = (7 * (sJob.sum / max(sJob.wsum, 1)) + 3 * sJob.worst) / 10;
            sJob.sum = sJob.wsum = 0;
            sJob.worst = INT32_MAX;
            sJob.i++;
            if (sJob.i >= sJob.nOurs)
                return TRUE;
        }
    }
    return FALSE;
}

static void StartLookAhead(void)
{
    struct PlanAction none = { PLAN_ACT_NONE, 0, -1, MOVE_NONE, 0, 0, 100 };
    enum BattlerId me = sJob.me, partner = GetPartnerBattler(me);
    enum BattlerId fL = GetBattlerLeftFoe(me), fR = GetBattlerRightFoe(me);

    sJob.la = sJob.s1;
    NextTurn(&sJob.la);
    sJob.nM = BuildActions(&sJob.la, me, sJob.mine, PLAN_P2_OURS_PER, TRUE, TRUE);
    sJob.nP = (partner != me) ? BuildActions(&sJob.la, partner, sJob.pa, PLAN_P2_OURS_PER, TRUE, FALSE) : 0;
    sJob.nA2 = BuildActions(&sJob.la, fL, sJob.fa, PLAN_P2_FOE_PER, FALSE, TRUE);
    sJob.nB2 = BuildActions(&sJob.la, fR, sJob.fb, PLAN_P2_FOE_PER, FALSE, FALSE);
    if (sJob.nM == 0) { sJob.mine[0] = none; sJob.mine[0].user = me; sJob.nM = 1; }
    if (sJob.nP == 0) { sJob.pa[0] = none; sJob.pa[0].user = partner; sJob.nP = 1; }
    if (sJob.nA2 == 0) { sJob.fa[0] = none; sJob.fa[0].user = fL; sJob.nA2 = 1; }
    if (sJob.nB2 == 0) { sJob.fb[0] = none; sJob.fb[0].user = fR; sJob.nB2 = 1; }
    sJob.i2 = sJob.j2 = sJob.a2 = sJob.b2 = 0;
    sJob.laBest = INT32_MIN;
    sJob.laSum = sJob.laWsum = 0;
    sJob.laWorst = INT32_MAX;
    sJob.phase = 1;
}

static void FinishJob(void)
{
    s32 bestEv = INT32_MIN, chosenEv = INT32_MIN;
    u32 i, bestIdx = 0;

    for (i = 0; i < sJob.nOurs; i++)
    {
        if (sJob.ours[i].kind != PLAN_ACT_SWITCH && sJob.ours[i].moveIndex == sJob.origMove && sJob.ours[i].target == sJob.origTarget)
            chosenEv = sJob.ev[i];
        if (sJob.ev[i] > bestEv)
        {
            bestEv = sJob.ev[i];
            bestIdx = i;
        }
    }
    sJob.resultKind = PLAN_ACT_NONE;
    if (chosenEv != INT32_MIN && bestEv < chosenEv + PLAN_OVERRIDE_MARGIN)
    {
        sJob.finished = TRUE;
        return;
    }
    sJob.resultKind = sJob.ours[bestIdx].kind;
    sJob.resultMove = sJob.ours[bestIdx].moveIndex;
    sJob.resultTarget = sJob.ours[bestIdx].target;
    sJob.resultParty = sJob.ours[bestIdx].partyIdx;
    sJob.finished = TRUE;
}

bool32 AI_VgcPlanIsActive(void)
{
    return sJob.active && !sJob.finished;
}

enum BattlerId AI_VgcPlanBattler(void)
{
    return sJob.me;
}

bool32 AI_VgcPlanBegin(enum BattlerId me)
{
    struct PlanAction none = { PLAN_ACT_NONE, 0, -1, MOVE_NONE, 0, 0, 100 };
    enum BattlerId fL = GetBattlerLeftFoe(me), fR = GetBattlerRightFoe(me);

    if (!IsDoubleBattle() || AI_VgcPlanIsActive())
        return FALSE;
    memset(&sJob, 0, sizeof(sJob));
    sJob.me = me;
    sJob.origMove = gAiBattleData->chosenMoveIndex[me];
    sJob.origTarget = gAiBattleData->chosenTarget[me];
    sJob.nOurs = BuildOurFirstActions(me, sJob.ours, sJob.origMove, sJob.origTarget);
    if (sJob.nOurs < 2)
        return FALSE;
    InitState(&sJob.base);
    PartnerFirstAction(me, &sJob.partner);
    sJob.nA = BuildActions(&sJob.base, fL, sJob.foeA, PLAN_FOE_PER, FALSE, TRUE);
    sJob.nB = BuildActions(&sJob.base, fR, sJob.foeB, PLAN_FOE_PER, FALSE, TRUE);
    if (sJob.nA == 0) { sJob.foeA[0] = none; sJob.foeA[0].user = fL; sJob.nA = 1; }
    if (sJob.nB == 0) { sJob.foeB[0] = none; sJob.foeB[0].user = fR; sJob.nB = 1; }
    sJob.worst = INT32_MAX;
    sJob.active = TRUE;
    sJob.finished = FALSE;
    return TRUE;
}

// Esegue fino a CUSTOM_VGC_PLAN_SIMS_PER_FRAME simulazioni. TRUE = finito (o nulla in corso).
bool32 AI_VgcPlanStep(void)
{
    s32 budget = CUSTOM_VGC_PLAN_SIMS_PER_FRAME;

    if (!sJob.active)
        return TRUE;
    while (budget > 0 && !sJob.finished)
    {
        if (sJob.phase == 0)
        {
            const struct PlanAction *acts[4] = { &sJob.ours[sJob.i], &sJob.partner, &sJob.foeA[sJob.a], &sJob.foeB[sJob.b] };
            sJob.s1 = sJob.base;
            RunTurn(&sJob.s1, acts, 4);
            budget--;
            if (CUSTOM_VGC_PLAN_DEPTH >= 2)
            {
                StartLookAhead();
            }
            else
            {
                Accumulate(Evaluate(&sJob.s1), PairWeight(&sJob.foeA[sJob.a], &sJob.foeB[sJob.b]));
                if (AdvanceLevel1())
                    FinishJob();
            }
        }
        else
        {
            const struct PlanAction *acts[4] = { &sJob.mine[sJob.i2], &sJob.pa[sJob.j2], &sJob.fa[sJob.a2], &sJob.fb[sJob.b2] };
            struct PlanState st = sJob.la;
            s32 v, w = PairWeight(&sJob.fa[sJob.a2], &sJob.fb[sJob.b2]);

            RunTurn(&st, acts, 4);
            v = Evaluate(&st);
            budget--;
            if (CUSTOM_VGC_PLAN_DEPTH >= 3)
            {
                v = (v + GreedyValue(&st, sJob.me)) / 2;
                budget--;
            }
            sJob.laSum += v * w;
            sJob.laWsum += w;
            if (v < sJob.laWorst)
                sJob.laWorst = v;

            sJob.b2++;
            if (sJob.b2 >= sJob.nB2)
            {
                sJob.b2 = 0;
                sJob.a2++;
                if (sJob.a2 >= sJob.nA2)
                {
                    s32 ev2 = (7 * (sJob.laSum / max(sJob.laWsum, 1)) + 3 * sJob.laWorst) / 10;
                    sJob.a2 = 0;
                    if (ev2 > sJob.laBest)
                        sJob.laBest = ev2;
                    sJob.laSum = sJob.laWsum = 0;
                    sJob.laWorst = INT32_MAX;
                    sJob.j2++;
                    if (sJob.j2 >= sJob.nP)
                    {
                        sJob.j2 = 0;
                        sJob.i2++;
                        if (sJob.i2 >= sJob.nM)
                        {
                            s32 v1 = (Evaluate(&sJob.s1) + sJob.laBest) / 2;
                            Accumulate(v1, PairWeight(&sJob.foeA[sJob.a], &sJob.foeB[sJob.b]));
                            sJob.phase = 0;
                            if (AdvanceLevel1())
                                FinishJob();
                        }
                    }
                }
            }
        }
    }
    return sJob.finished;
}

// Applica il risultato alla decisione dell'IA. TRUE = la scelta è cambiata.
bool32 AI_VgcPlanApply(void)
{
    enum BattlerId me = sJob.me;
    bool32 changed = FALSE;

    if (!sJob.active)
        return FALSE;
    sJob.active = FALSE;
    if (!sJob.finished || sJob.resultKind == PLAN_ACT_NONE)
        return FALSE;
    if (sJob.resultKind == PLAN_ACT_SWITCH)
    {
        if (sJob.resultParty >= 0 && sJob.resultParty < PARTY_SIZE)
        {
            gBattleStruct->AI_monToSwitchIntoId[me] = sJob.resultParty;
            gAiLogicData->shouldSwitch |= 1u << me;
            changed = TRUE;
        }
        return changed;
    }
    if (sJob.resultMove != sJob.origMove || sJob.resultTarget != sJob.origTarget)
    {
        gAiBattleData->chosenMoveIndex[me] = sJob.resultMove;
        gAiBattleData->chosenTarget[me] = sJob.resultTarget;
        // La mossa è cambiata: un cambio deciso dalle regole "perché le mosse erano tutte brutte" non vale più.
        gAiLogicData->shouldSwitch &= ~(1u << me);
        changed = TRUE;
    }
    return changed;
}
