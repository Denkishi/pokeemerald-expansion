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
#include "battle_ai_vgc_graph.h"
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

#define PLAN_MAX_OURS        7   // azioni candidate per ciascuno dei nostri Pokémon (6 dalle regole + cambio)
#define PLAN_MAX_PAIRS       14  // coppie (nostra azione, azione del partner) simulate al primo turno
#define PLAN_FOE_PER         2   // azioni per avversario (primo turno)
#define PLAN_P2_OURS_PER     2   // azioni per nostro Pokémon al turno dopo
#define PLAN_P2_FOE_PER      2   // azioni per avversario al turno dopo
#define PLAN_OVERRIDE_MARGIN 1   // la simulazione decide: basta essere migliori (a pari valore resta la scelta delle regole)
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

enum PlanStage
{
    PS_ATK,
    PS_DEF,
    PS_SPA,
    PS_SPD,
    PS_SPE,
    PS_COUNT,
};

// Valore di una statistica allo stadio `s` (v = valore a stadio 0).
static u32 ApplyStage(u32 v, s32 s)
{
    s = max(-6, min(6, s));
    return (s >= 0) ? v * (2 + s) / 2 : v * 2 / (2 - s);
}

// Da un valore già allo stadio `from` allo stesso valore allo stadio `to`.
static u32 StageRatio(u32 v, s32 from, s32 to)
{
    from = max(-6, min(6, from));
    to = max(-6, min(6, to));
    if (from == to)
        return v;
    v = (from >= 0) ? v * 2 / (2 + from) : v * (2 - from) / 2;
    return ApplyStage(v, to);
}

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
    u8 burned;          // scottato (dimezza gli attacchi fisici)
    u8 burned0;         // già scottato all'inizio (compreso nella tabella dei danni)
    u8 paralyzed;       // paralizzato (dimezza la Velocità)
    u8 sleepTurns;      // turni di sonno rimasti (si sveglia quando arriva a 0 provando a muovere)
    u8 protecting;
    u8 protectedLast;   // ha usato Protect il turno prima: un altro di fila fallisce
    u8 wideGuard;
    u8 flinched;
    u8 redirect;
    u8 helping;
    u8 berryUsed;
    s8 stage[PS_COUNT];     // stadi attuali nella simulazione (-6..+6)
    s8 stage0[PS_COUNT];    // stadi all'inizio (già compresi in velocità e danni della tabella dell'IA)
    u8 canAttack:1;         // ha mosse d'attacco (i boost offensivi servono a qualcosa)
    u8 canPass:1;           // ha Baton Pass (i boost valgono per chi li riceve)
};

struct PlanState
{
    struct PlanMon m[MAX_BATTLERS_COUNT];
    u8 trickRoom;       // turni rimasti (0 = non attivo)
    u8 tailwind[2];     // turni rimasti per lato (0 = non attivo)
    u8 weather;         // PW_*
    u8 screens[2];      // per lato: bit 0 Reflect, bit 1 Light Screen (Aurora Veil = entrambi)
    u8 screens0[2];     // schermi all'inizio (compresi nella tabella dei danni)
    u8 firstTurn;       // turno simulato = turno attuale (Fake Out vale)
};

// Lavoro in corso (uno alla volta).
struct PlanJob
{
    u8 active;
    u8 finished;
    u16 turn;
    enum BattlerId me;
    u8 origMove;
    enum BattlerId origTarget;
    // Coppia i: ours[i] è la nostra azione, partners[i] quella del partner (decise insieme).
    struct PlanAction ours[PLAN_MAX_PAIRS], partners[PLAN_MAX_PAIRS], foeA[PLAN_FOE_PER], foeB[PLAN_FOE_PER];
    u8 nOurs, nA, nB;
    struct VgcTeamGraph graph;  // conoscenza della nostra squadra (combo, ricevente del Baton Pass)
    u8 origPMove;
    enum BattlerId origPTarget;
    struct PlanState base;
    // livello 1
    u8 i, a, b, phase;
    s32 sum, wsum, worst;
    s32 ev[PLAN_MAX_PAIRS];
    struct PlanState s1;
    s32 missPen;        // quanto si perde, in media, per le azioni del turno 1 che possono mancare
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

// Decisione già presa per il partner nello stesso turno (la ricerca sceglie la coppia): quando tocca a lui,
// il pianificatore la applica senza ricalcolare.
struct PlanPartnerDecision
{
    u16 turn;
    u8 valid;
    enum BattlerId battler;
    u8 kind;
    u8 move;
    s8 party;
    enum BattlerId target;
};

static EWRAM_DATA struct PlanPartnerDecision sPartnerDecision = {0};

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
    if (IsOnPlayerSide(b))
    {
        // Del giocatore si stimano le statistiche da specie e livello (le vere non si vedono).
        u16 hp, stats[NUM_STATS];
        Scout_EstimateStats(pm->species, gBattleMons[b].level, &hp, stats);
        pm->atk = stats[STAT_ATK];
        pm->spa = stats[STAT_SPATK];
        pm->def = stats[STAT_DEF];
        pm->spd = stats[STAT_SPDEF];
        pm->berryUsed = Scout_ItemKnownGone(b);
    }
    else
    {
        pm->atk = gBattleMons[b].attack;
        pm->spa = gBattleMons[b].spAttack;
        pm->def = gBattleMons[b].defense;
        pm->spd = gBattleMons[b].spDefense;
        pm->berryUsed = (gBattleMons[b].item == ITEM_NONE);
    }
    pm->t1 = gBattleMons[b].types[0];
    pm->t2 = gBattleMons[b].types[1];
    pm->ability = gAiLogicData->abilities[b];
    pm->holdEffect = gAiLogicData->holdEffects[b];
    pm->partyIdx = -1;
    pm->alive = IsBattlerAlive(b);
    pm->asleep = (gBattleMons[b].status1 & STATUS1_SLEEP) != 0;
    pm->sleepTurns = gBattleMons[b].status1 & STATUS1_SLEEP;
    pm->burned = pm->burned0 = (gBattleMons[b].status1 & STATUS1_BURN) != 0;
    pm->paralyzed = (gBattleMons[b].status1 & STATUS1_PARALYSIS) != 0;
    {
        enum Move last = gLastResultingMoves[b];
        pm->protectedLast = last != MOVE_NONE && last != MOVE_UNAVAILABLE
                         && gBattleMoveEffects[GetMoveEffect(last)].usesProtectCounter
                         && gBattleMons[b].volatiles.consecutiveMoveUses > 0;
    }
    // Stadi attuali (visibili a entrambi i giocatori).
    pm->stage[PS_ATK] = gBattleMons[b].statStages[STAT_ATK] - DEFAULT_STAT_STAGE;
    pm->stage[PS_DEF] = gBattleMons[b].statStages[STAT_DEF] - DEFAULT_STAT_STAGE;
    pm->stage[PS_SPA] = gBattleMons[b].statStages[STAT_SPATK] - DEFAULT_STAT_STAGE;
    pm->stage[PS_SPD] = gBattleMons[b].statStages[STAT_SPDEF] - DEFAULT_STAT_STAGE;
    pm->stage[PS_SPE] = gBattleMons[b].statStages[STAT_SPEED] - DEFAULT_STAT_STAGE;
    memcpy(pm->stage0, pm->stage, sizeof(pm->stage));
    pm->canAttack = TRUE;
    pm->canPass = FALSE;
    if (!IsOnPlayerSide(b))
    {
        u32 i;
        pm->canAttack = FALSE;
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            enum Move m = gBattleMons[b].moves[i];
            if (m != MOVE_NONE && m != MOVE_UNAVAILABLE && !IsBattleMoveStatus(m))
                pm->canAttack = TRUE;
            if (m != MOVE_NONE && m != MOVE_UNAVAILABLE && GetMoveEffect(m) == EFFECT_BATON_PASS)
                pm->canPass = TRUE;
        }
    }
}

// Intimidate di `b` sugli avversari in campo, con le abilità che lo bloccano o lo ritorcono.
static void IntimidateFoes(struct PlanState *st, enum BattlerId b)
{
    enum BattlerId foes[2] = { GetBattlerLeftFoe(b), GetBattlerRightFoe(b) };
    u32 i;

    for (i = 0; i < 2; i++)
    {
        struct PlanMon *fm = &st->m[foes[i]];
        if (!fm->alive || fm->species == SPECIES_NONE)
            continue;
        switch (fm->ability)
        {
        case ABILITY_CLEAR_BODY: case ABILITY_WHITE_SMOKE: case ABILITY_FULL_METAL_BODY: case ABILITY_HYPER_CUTTER:
        case ABILITY_INNER_FOCUS: case ABILITY_OWN_TEMPO: case ABILITY_OBLIVIOUS: case ABILITY_SCRAPPY:
            break;
        case ABILITY_GUARD_DOG:
            fm->stage[PS_ATK] = min(6, fm->stage[PS_ATK] + 1);
            break;
        case ABILITY_DEFIANT:
            fm->stage[PS_ATK] = min(6, fm->stage[PS_ATK] + 1); // -1 e poi +2
            break;
        case ABILITY_COMPETITIVE:
            fm->stage[PS_ATK] = max(-6, fm->stage[PS_ATK] - 1);
            fm->stage[PS_SPA] = min(6, fm->stage[PS_SPA] + 2);
            break;
        default:
            if (fm->holdEffect != HOLD_EFFECT_CLEAR_AMULET)
                fm->stage[PS_ATK] = max(-6, fm->stage[PS_ATK] - 1);
            break;
        }
    }
}

static void LoadPartyMon(struct PlanState *st, enum BattlerId b, u32 partyIdx)
{
    struct PlanMon *pm = &st->m[b];
    struct Pokemon *mon = &GetBattlerParty(b)[partyIdx];
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    u8 w;

    pm->species = species;
    pm->t1 = gSpeciesInfo[species].types[0];
    pm->t2 = gSpeciesInfo[species].types[1];
    if (IsOnPlayerSide(b))
    {
        // Panchina del giocatore: specie visibile al team preview, il resto stimato. I PS si tengono in
        // percentuale (visibile se è già sceso in campo, pieni altrimenti) sulla stima dei PS massimi.
        struct ScoutItemGuess g;
        u16 hp, stats[NUM_STATS];
        u32 level = GetMonData(mon, MON_DATA_LEVEL);
        u32 realMax = GetMonData(mon, MON_DATA_MAX_HP);

        Scout_EstimateStats(species, level, &hp, stats);
        pm->maxHp = hp;
        pm->hp = (realMax == 0) ? 0 : hp * GetMonData(mon, MON_DATA_HP) / realMax;
        if (pm->hp == 0 && GetMonData(mon, MON_DATA_HP) != 0)
            pm->hp = 1;
        pm->speed = stats[STAT_SPEED];
        pm->atk = stats[STAT_ATK];
        pm->spa = stats[STAT_SPATK];
        pm->def = stats[STAT_DEF];
        pm->spd = stats[STAT_SPDEF];
        pm->ability = Scout_KnownAbility(species);
        if (pm->ability == ABILITY_NONE)
        {
            // Abilità incerta: se una delle possibili porta il meteo, si assume (chi lo porta lo usa).
            u32 i;
            for (i = 0; i < NUM_ABILITY_SLOTS; i++)
            {
                if (WeatherFromAbility(gSpeciesInfo[species].abilities[i]) != PW_NONE)
                    pm->ability = gSpeciesInfo[species].abilities[i];
            }
        }
        Scout_GuessItem(species, &g);
        pm->holdEffect = (g.chance >= 50) ? g.effect : HOLD_EFFECT_NONE;
        if (pm->holdEffect == HOLD_EFFECT_CHOICE_SCARF)
            pm->speed = pm->speed * 3 / 2;
    }
    else
    {
        pm->hp = GetMonData(mon, MON_DATA_HP);
        pm->maxHp = GetMonData(mon, MON_DATA_MAX_HP);
        pm->speed = GetMonData(mon, MON_DATA_SPEED);
        pm->atk = GetMonData(mon, MON_DATA_ATK);
        pm->spa = GetMonData(mon, MON_DATA_SPATK);
        pm->def = GetMonData(mon, MON_DATA_DEF);
        pm->spd = GetMonData(mon, MON_DATA_SPDEF);
        pm->ability = GetMonAbility(mon);
        pm->holdEffect = GetItemHoldEffect(GetMonData(mon, MON_DATA_HELD_ITEM));
        if (pm->holdEffect == HOLD_EFFECT_CHOICE_SCARF)
            pm->speed = pm->speed * 3 / 2;
    }
    pm->partyIdx = partyIdx;
    pm->alive = pm->hp > 0;
    pm->asleep = (GetMonData(mon, MON_DATA_STATUS) & STATUS1_SLEEP) != 0;
    pm->sleepTurns = GetMonData(mon, MON_DATA_STATUS) & STATUS1_SLEEP;
    pm->burned = pm->burned0 = (GetMonData(mon, MON_DATA_STATUS) & STATUS1_BURN) != 0;
    pm->paralyzed = (GetMonData(mon, MON_DATA_STATUS) & STATUS1_PARALYSIS) != 0;
    pm->protecting = pm->protectedLast = pm->wideGuard = pm->flinched = pm->redirect = pm->helping = 0;
    pm->berryUsed = FALSE;
    memset(pm->stage, 0, sizeof(pm->stage));
    memset(pm->stage0, 0, sizeof(pm->stage0));
    pm->canAttack = TRUE;
    pm->canPass = FALSE;
    if (!IsOnPlayerSide(b))
    {
        u32 i;
        pm->canAttack = FALSE;
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            enum Move m = GetMonData(mon, MON_DATA_MOVE1 + i);
            if (m != MOVE_NONE && !IsBattleMoveStatus(m))
                pm->canAttack = TRUE;
            if (m != MOVE_NONE && GetMoveEffect(m) == EFFECT_BATON_PASS)
                pm->canPass = TRUE;
        }
    }
    // Meteo all'entrata (abilità).
    w = WeatherFromAbility(pm->ability);
    if (w != PW_NONE)
        st->weather = w;
    // Intimidate all'entrata (per il giocatore: se la specie lo porta di solito).
    {
        bool32 intimidate = pm->ability == ABILITY_INTIMIDATE;
        if (!intimidate && IsOnPlayerSide(b) && pm->ability == ABILITY_NONE)
        {
            struct ScoutProfile prof;
            Scout_GetSpeciesProfile(species, &prof);
            intimidate = prof.intimidateP >= 50;
        }
        if (intimidate)
            IntimidateFoes(st, b);
    }
}

static void InitState(struct PlanState *st)
{
    enum BattlerId b;

    memset(st, 0, sizeof(*st));
    for (b = 0; b < gBattlersCount; b++)
        LoadFieldMon(&st->m[b], b);
    st->trickRoom = (gFieldStatuses & STATUS_FIELD_TRICK_ROOM) ? gFieldTimers.trickRoomTimer : 0;
    st->tailwind[B_SIDE_PLAYER] = (gSideStatuses[B_SIDE_PLAYER] & SIDE_STATUS_TAILWIND) ? gSideTimers[B_SIDE_PLAYER].tailwindTimer : 0;
    st->tailwind[B_SIDE_OPPONENT] = (gSideStatuses[B_SIDE_OPPONENT] & SIDE_STATUS_TAILWIND) ? gSideTimers[B_SIDE_OPPONENT].tailwindTimer : 0;
    st->weather = CurrentWeather();
    {
        u32 side;
        for (side = 0; side < NUM_BATTLE_SIDES; side++)
        {
            u32 ss = gSideStatuses[side];
            st->screens[side] = ((ss & (SIDE_STATUS_REFLECT | SIDE_STATUS_AURORA_VEIL)) ? 1 : 0)
                              | ((ss & (SIDE_STATUS_LIGHTSCREEN | SIDE_STATUS_AURORA_VEIL)) ? 2 : 0);
            st->screens0[side] = st->screens[side];
        }
    }
    st->firstTurn = TRUE;
}

// Fine turno simulato: scadono Trick Room e Tailwind (come in battle_end_turn.c), Protect diventa
// "usato il turno prima".
static void NextTurn(struct PlanState *st)
{
    enum BattlerId b;
    for (b = 0; b < gBattlersCount; b++)
    {
        // Speed Boost: +1 Velocità a fine turno.
        if (st->m[b].alive && st->m[b].ability == ABILITY_SPEED_BOOST)
            st->m[b].stage[PS_SPE] = min(6, st->m[b].stage[PS_SPE] + 1);
        st->m[b].protectedLast = st->m[b].protecting;
        st->m[b].protecting = st->m[b].wideGuard = st->m[b].flinched = st->m[b].redirect = st->m[b].helping = 0;
    }
    if (st->trickRoom > 0)
        st->trickRoom--;
    if (st->tailwind[B_SIDE_PLAYER] > 0)
        st->tailwind[B_SIDE_PLAYER]--;
    if (st->tailwind[B_SIDE_OPPONENT] > 0)
        st->tailwind[B_SIDE_OPPONENT]--;
    st->firstTurn = FALSE;
}

static u32 EffSpeed(const struct PlanState *st, enum BattlerId b)
{
    const struct PlanMon *pm = &st->m[b];
    u32 s = StageRatio(pm->speed, pm->stage0[PS_SPE], pm->stage[PS_SPE]);

    // La velocità di partenza della tabella dell'IA include già la paralisi presente: si conta solo quella nuova.
    if (pm->paralyzed && !(pm->partyIdx < 0 && (gBattleMons[b].status1 & STATUS1_PARALYSIS)))
        s /= 2;

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
    u32 a = physical ? ApplyStage(atk->atk, atk->stage[PS_ATK]) : ApplyStage(atk->spa, atk->stage[PS_SPA]);
    u32 d = physical ? ApplyStage(def->def, def->stage[PS_DEF]) : ApplyStage(def->spd, def->stage[PS_SPD]);
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
    if (physical && atk->burned0 && atk->ability != ABILITY_GUTS)
        dmg /= 2;
    {
        u32 i, side = 0;
        for (i = 0; i < gBattlersCount; i++)
            if (&st->m[i] == def)
                side = GetBattlerSide(i);
        if (st->screens0[side] & (physical ? 1 : 2))
            dmg = dmg * 2 / 3;
    }
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
        // La tabella include anche gli stadi attuali: si riscala per quelli cambiati nella simulazione.
        if (GetMoveCategory(a->move) == DAMAGE_CATEGORY_PHYSICAL)
        {
            dmg = StageRatio(dmg, am->stage0[PS_ATK], am->stage[PS_ATK]);
            dmg = StageRatio(dmg, -dm->stage0[PS_DEF], -dm->stage[PS_DEF]);
        }
        else
        {
            dmg = StageRatio(dmg, am->stage0[PS_SPA], am->stage[PS_SPA]);
            dmg = StageRatio(dmg, -dm->stage0[PS_SPD], -dm->stage[PS_SPD]);
        }
    }
    else
    {
        dmg = EstimateDamage(st, am, dm, a->move);
    }
    {
        // Scottatura e schermi comparsi durante la simulazione (quelli già presenti sono nella tabella o nella stima).
        bool32 physical = GetMoveCategory(a->move) == DAMAGE_CATEGORY_PHYSICAL;
        u32 side = GetBattlerSide(t), bit = physical ? 1 : 2;
        if (physical && am->burned && !am->burned0 && am->ability != ABILITY_GUTS)
            dmg /= 2;
        if ((st->screens[side] & bit) && !(st->screens0[side] & bit) && !IsBattlerAlly(a->user, t))
            dmg = dmg * 2 / 3;
    }
    if (am->helping)
        dmg = dmg * 3 / 2;
    // Niente precisione qui: il danno è quello di un colpo andato a segno. La probabilità di mancare la
    // conta il pianificatore a parte (ramo "mancato", vedi ActionAccuracy e MissPenalty).
    return dmg;
}

// Battler la cui azione manca il bersaglio nella simulazione in corso (ramo "mancato"), più 1; 0 = nessuno.
static EWRAM_DATA u8 sMissBattler = 0;

// Precisione (0..100) dell'azione `a` sul suo bersaglio: dalla tabella dell'IA quando c'è (comprende
// Compound Eyes, No Guard, meteo...), altrimenti quella della mossa. 100 = non può mancare.
static u32 ActionAccuracy(const struct PlanState *st, const struct PlanAction *a)
{
    u32 acc;
    enum MoveTarget tt;

    if (a->kind != PLAN_ACT_MOVE)
        return 100;
    tt = GetMoveTarget(a->move);
    if (tt == TARGET_USER || tt == TARGET_ALLY || tt == TARGET_USER_AND_ALLY || tt == TARGET_FIELD
     || tt == TARGET_OPPONENTS_FIELD || tt == TARGET_USER_OR_ALLY || tt == TARGET_ALL_BATTLERS || IsBattlerAlly(a->user, a->target))
        return 100;
    if (a->moveIndex < MAX_MON_MOVES && st->m[a->user].partyIdx < 0 && st->m[a->target].partyIdx < 0)
    {
        acc = gAiLogicData->moveAccuracy[a->user][a->target][a->moveIndex];
        if (acc != 0)
            return min(acc, 100);
    }
    acc = GetMoveAccuracy(a->move);
    return (acc == 0 || acc > 100) ? 100 : acc;
}

// ---------------------------------------------------------------------
// Applicazione di un'azione
// ---------------------------------------------------------------------
static bool32 CanBatonPassTo(struct Pokemon *party, u32 idx, u32 myIdx, u32 partnerIdx)
{
    return idx < PARTY_SIZE && idx != myIdx && idx != partnerIdx
        && GetMonData(&party[idx], MON_DATA_SPECIES) != SPECIES_NONE && !GetMonData(&party[idx], MON_DATA_IS_EGG)
        && GetMonData(&party[idx], MON_DATA_HP) > 0;
}

// Baton Pass nella simulazione: entra il ricevente designato dal grafo della squadra (o il primo libero)
// e si porta dietro gli stadi.
static void BatonPassInSim(struct PlanState *st, enum BattlerId u)
{
    struct Pokemon *party = GetBattlerParty(u);
    enum BattlerId p = GetPartnerBattler(u);
    u32 myIdx = (st->m[u].partyIdx >= 0) ? (u32)st->m[u].partyIdx : gBattlerPartyIndexes[u];
    u32 pIdx = (st->m[p].partyIdx >= 0) ? (u32)st->m[p].partyIdx : gBattlerPartyIndexes[p];
    u32 idx = (myIdx < PARTY_SIZE) ? sJob.graph.batonRecipient[myIdx] : VGC_GRAPH_NONE;
    s8 stages[PS_COUNT];

    if (!CanBatonPassTo(party, idx, myIdx, pIdx))
    {
        for (idx = 0; idx < PARTY_SIZE; idx++)
            if (CanBatonPassTo(party, idx, myIdx, pIdx))
                break;
        if (idx == PARTY_SIZE)
            return;
    }
    memcpy(stages, st->m[u].stage, sizeof(stages));
    LoadPartyMon(st, u, idx);
    memcpy(st->m[u].stage, stages, sizeof(stages));
}

// Priorità con l'abilità che il Pokémon ha ORA nella simulazione (può cambiare a metà turno, es. Simple Beam).
static s32 ActionPriority(const struct PlanState *st, const struct PlanAction *a)
{
    switch (a->kind)
    {
    case PLAN_ACT_SWITCH:  return 9;
    case PLAN_ACT_PROTECT: return 4;
    case PLAN_ACT_NONE:    return -8;
    default:               return GetBattleMovePriority(a->user, st->m[a->user].ability, a->move);
    }
}

// Mycelium Might: le mosse di stato agiscono per ultime nella loro fascia di priorità.
static bool32 MovesLastInBracket(const struct PlanState *st, const struct PlanAction *a)
{
    return a->kind == PLAN_ACT_MOVE && st->m[a->user].ability == ABILITY_MYCELIUM_MIGHT && IsBattleMoveStatus(a->move);
}

static bool32 MovesFirst(const struct PlanState *st, const struct PlanAction *a, const struct PlanAction *b)
{
    s32 pa = ActionPriority(st, a), pb = ActionPriority(st, b);
    bool32 la = MovesLastInBracket(st, a), lb = MovesLastInBracket(st, b);
    u32 sa, sb;

    if (pa != pb)
        return pa > pb;
    if (la != lb)
        return lb;
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
    if (!um->alive || um->flinched)
        return;
    if (um->asleep)
    {
        // Il contatore scende quando prova a muovere; a 0 si sveglia e agisce nello stesso turno.
        if (um->sleepTurns > 0)
            um->sleepTurns--;
        if (um->sleepTurns > 0)
            return;
        um->asleep = FALSE;
    }
    if (a->kind == PLAN_ACT_PROTECT)
    {
        // Un secondo Protect di fila riesce solo 1 volta su 3: nel simulatore si considera fallito.
        um->protecting = !um->protectedLast;
        return;
    }

    effect = GetMoveEffect(a->move);
    tt = GetMoveTarget(a->move);
    // Ramo "mancato": l'azione verso gli avversari non ha effetto (le mosse su sé stessi o sul compagno non mancano).
    if (u + 1 == sMissBattler && ActionAccuracy(st, a) < 100)
        return;
    // Fake Out e simili falliscono del tutto dopo il primo turno in campo (niente danno).
    if (effect == EFFECT_FIRST_TURN_ONLY && !(st->firstTurn && IsBattlersFirstTurn(u)))
        return;
    switch (effect)
    {
    case EFFECT_PROTECT:
        if (GetMoveProtectMethod(a->move) == PROTECT_WIDE_GUARD)
            um->wideGuard = st->m[GetPartnerBattler(u)].wideGuard = TRUE;
        else if (GetMoveProtectMethod(a->move) != PROTECT_QUICK_GUARD && GetMoveProtectMethod(a->move) != PROTECT_CRAFTY_SHIELD)
            um->protecting = !um->protectedLast;
        return;
    case EFFECT_FOLLOW_ME:
        um->redirect = TRUE;
        return;
    case EFFECT_HELPING_HAND:
        st->m[GetPartnerBattler(u)].helping = TRUE;
        return;
    case EFFECT_BATON_PASS:
        if (!IsOnPlayerSide(u))
            BatonPassInSim(st, u);
        return;
    case EFFECT_REFLECT:
        st->screens[GetBattlerSide(u)] |= 1;
        return;
    case EFFECT_LIGHT_SCREEN:
        st->screens[GetBattlerSide(u)] |= 2;
        return;
    case EFFECT_AURORA_VEIL:
        if (st->weather == PW_SNOW)
            st->screens[GetBattlerSide(u)] |= 3;
        return;
    case EFFECT_TRICK_ROOM:
        // Dura 5 turni compreso questo (NextTurn toglie il primo a fine turno).
        st->trickRoom = st->trickRoom ? 0 : 5;
        return;
    case EFFECT_TAILWIND:
        if (!st->tailwind[GetBattlerSide(u)])
            st->tailwind[GetBattlerSide(u)] = 4;
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
        um->stage[PS_ATK] = 6;
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
        bool32 blocked = !tm->alive || (tm->protecting && !IsBattlerAlly(u, a->target));

        // Cambi d'abilità: valgono subito, anche per chi deve ancora muovere in questo turno
        // (es. Simple Beam sul compagno con Mycelium Might: la sua Spora non va più per ultima).
        switch (effect)
        {
        case EFFECT_OVERWRITE_ABILITY:
            if (!blocked)
                tm->ability = GetMoveOverwriteAbility(a->move);
            return;
        case EFFECT_GASTRO_ACID:
            if (!blocked)
                tm->ability = ABILITY_NONE;
            return;
        case EFFECT_ENTRAINMENT:
            if (!blocked)
                tm->ability = um->ability;
            return;
        case EFFECT_SKILL_SWAP:
            if (!blocked)
            {
                enum Ability tmp = tm->ability;
                tm->ability = um->ability;
                um->ability = tmp;
            }
            return;
        default:
            break;
        }
        if (IsStatRaisingMove(a->move) && tt == TARGET_USER)
        {
            // Stadi veri della mossa (Calm Mind +1 SpA/+1 SpD, Dragon Dance +1 Atk/+1 Spe, Shell Smash...).
            u32 e, ne = GetMoveAdditionalEffectCount(a->move);
            for (e = 0; e < ne; e++)
            {
                const struct AdditionalEffect *ae = GetMoveAdditionalEffectById(a->move, e);
                if (ae->moveEffect != STAT_CHANGE_EFFECT_PLUS)
                    continue;
                um->stage[PS_ATK] = min(6, um->stage[PS_ATK] + ae->attack);
                um->stage[PS_DEF] = min(6, um->stage[PS_DEF] + ae->defense);
                um->stage[PS_SPA] = min(6, um->stage[PS_SPA] + ae->spAtk);
                um->stage[PS_SPD] = min(6, um->stage[PS_SPD] + ae->spDef);
                um->stage[PS_SPE] = min(6, um->stage[PS_SPE] + ae->speed);
            }
        }
        else if ((GetMoveNonVolatileStatus(a->move) == MOVE_EFFECT_BURN || GetMoveNonVolatileStatus(a->move) == MOVE_EFFECT_PARALYSIS)
              && tm->alive && !tm->protecting && !tm->asleep && !tm->burned && !tm->paralyzed)
        {
            // Will-O-Wisp / Thunder Wave & co.: immunità di tipo (Fuoco alla scottatura, Elettro alla paralisi,
            // e il tipo della mossa: Thunder Wave non tocca i Terra).
            bool32 burn = GetMoveNonVolatileStatus(a->move) == MOVE_EFFECT_BURN;
            enum Type immune = burn ? TYPE_FIRE : TYPE_ELECTRIC;
            if (tm->t1 != immune && tm->t2 != immune && TypeEff(GetMoveType(a->move), tm) != UQ_4_12(0.0)
             && tm->ability != ABILITY_GOOD_AS_GOLD && tm->ability != ABILITY_MAGIC_BOUNCE && tm->ability != ABILITY_PURIFYING_SALT
             && (burn ? tm->ability != ABILITY_WATER_VEIL : tm->ability != ABILITY_LIMBER))
            {
                if (burn)
                    tm->burned = TRUE;
                else
                    tm->paralyzed = TRUE;
            }
        }
        else if (GetMoveNonVolatileStatus(a->move) == MOVE_EFFECT_SLEEP && tm->alive && !tm->protecting && !tm->asleep)
        {
            bool32 ok = TRUE;
            // Mycelium Might: le mosse di stato ignorano le abilità del bersaglio (non tipo e strumento).
            enum Ability defAbility = (um->ability == ABILITY_MYCELIUM_MIGHT) ? ABILITY_NONE : tm->ability;
            if (IsPowderMove(a->move) && (tm->t1 == TYPE_GRASS || tm->t2 == TYPE_GRASS || defAbility == ABILITY_OVERCOAT
                                        || tm->holdEffect == HOLD_EFFECT_SAFETY_GOGGLES))
                ok = FALSE;
            if (defAbility == ABILITY_INSOMNIA || defAbility == ABILITY_VITAL_SPIRIT || defAbility == ABILITY_SWEET_VEIL
             || defAbility == ABILITY_GOOD_AS_GOLD || defAbility == ABILITY_MAGIC_BOUNCE || defAbility == ABILITY_PURIFYING_SALT
             || defAbility == ABILITY_COMATOSE)
                ok = FALSE;
            if (ok)
            {
                tm->asleep = TRUE;
                tm->sleepTurns = 2; // 1-3 turni: si usa il valore medio
            }
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
            // Justified: ogni colpo di tipo Buio alza l'Attacco (Beat Up colpisce una volta per Pokémon della squadra).
            if (tm->ability == ABILITY_JUSTIFIED && tm->alive && GetMoveType(a->move) == TYPE_DARK)
                tm->stage[PS_ATK] = min(6, tm->stage[PS_ATK] + ((effect == EFFECT_BEAT_UP) ? min(CountUsablePartyMons(u) + 2, 4) : 1));
            if (dmg <= 0)
                continue;
            if (effect == EFFECT_FIRST_TURN_ONLY && st->firstTurn && IsBattlersFirstTurn(u) && tm->ability != ABILITY_INNER_FOCUS)
                tm->flinched = TRUE;
            if (a->move == MOVE_ICY_WIND || a->move == MOVE_ELECTROWEB || a->move == MOVE_BULLDOZE)
                tm->stage[PS_SPE] = max(-6, tm->stage[PS_SPE] - 1);
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
        u32 seen = 0;
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            struct PlanAction tmp = { PLAN_ACT_MOVE, (u8)i, -1, moves[i], atk, def, 0 };
            if (moves[i] == MOVE_NONE || moves[i] == MOVE_UNAVAILABLE)
                continue;
            seen++;
            if (IsBattleMoveStatus(moves[i]))
                continue;
            if (DamageOf(st, &tmp, def) >= dm->hp)
                return TRUE;
        }
        // Un avversario con mosse ancora non viste non è innocuo: minaccia con le sue STAB probabili
        // (come pensa un giocatore: "quel Garchomp ha sicuramente un attacco Terra").
        if (!IsOnPlayerSide(atk) || seen >= MAX_MON_MOVES)
            return FALSE;
    }
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
        // Stato: la scottatura pesa sugli attaccanti fisici, la paralisi su tutti (Velocità dimezzata).
        if (pm->burned && pm->atk > pm->spa && pm->ability != ABILITY_GUTS)
            v -= side * 35;
        if (pm->paralyzed)
            v -= side * 20;
        // Stadi: valgono se servono a qualcosa (chi attacca, o chi li passa con Baton Pass a qualcuno in panchina).
        if (pm->canAttack)
        {
            s32 off = (pm->atk >= pm->spa) ? pm->stage[PS_ATK] : pm->stage[PS_SPA];
            s32 dfn = (pm->stage[PS_DEF] + pm->stage[PS_SPD]) / 2;
            v += side * (20 * max(-4, min(4, off)) + 12 * max(-4, min(4, pm->stage[PS_SPE])) + 6 * max(-4, min(4, dfn)));
        }
        else if (pm->canPass && CountUsablePartyMons(b) > 0)
        {
            // Potenziale trasferibile: i boost di un Baton Passer finiscono su uno sweeper in panchina (scelto dal
            // grafo per sfruttarli) e lì valgono il doppio che su un attaccante qualunque.
            s32 off = max(pm->stage[PS_ATK], pm->stage[PS_SPA]);
            s32 dfn = (pm->stage[PS_DEF] + pm->stage[PS_SPD]) / 2;
            v += side * (40 * max(-4, min(4, off)) + 24 * max(-4, min(4, pm->stage[PS_SPE])) + 8 * max(-4, min(4, dfn)));
        }
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
    return w * (s32)ActionAccuracy(st, a) / 100;
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
            else if (GetMoveNonVolatileStatus(m) == MOVE_EFFECT_BURN || GetMoveNonVolatileStatus(m) == MOVE_EFFECT_PARALYSIS) w = 40;
            else if ((eff == EFFECT_REFLECT && !(st->screens[GetBattlerSide(b)] & 1))
                  || (eff == EFFECT_LIGHT_SCREEN && !(st->screens[GetBattlerSide(b)] & 2))
                  || (eff == EFFECT_AURORA_VEIL && st->weather == PW_SNOW && st->screens[GetBattlerSide(b)] != 3)) w = 40;
            else if (eff == EFFECT_BATON_PASS && isOurs && CountUsablePartyMons(b) > 0
                  && (bm->stage[PS_ATK] > 0 || bm->stage[PS_SPA] > 0 || bm->stage[PS_SPE] > 0)) w = 60;
            else if (IsStatRaisingMove(m) && GetMoveTarget(m) == TARGET_USER) w = 35;
            if (w == 0)
                continue;
            if (GetMoveNonVolatileStatus(m) == MOVE_EFFECT_SLEEP || GetMoveNonVolatileStatus(m) == MOVE_EFFECT_BURN
             || GetMoveNonVolatileStatus(m) == MOVE_EFFECT_PARALYSIS)
            {
                for (k = 0; k < 2; k++)
                    if (st->m[targets[k]].alive && !st->m[targets[k]].asleep && !st->m[targets[k]].burned && !st->m[targets[k]].paralyzed)
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
        if (hasProtect && !bm->protectedLast)
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

    // Un avversario senza mosse plausibili note non "fa Protect di sicuro": resta sempre l'alternativa in cui
    // non si protegge (qui come azione neutra), con la probabilità complementare. Senza, la simulazione dava
    // per certo il Protect e qualunque nostro attacco sembrava inutile.
    {
        bool32 hasMove = FALSE;
        s32 protectW = 0;
        for (i = 0; i < n; i++)
        {
            if (cand[i].kind == PLAN_ACT_MOVE)
                hasMove = TRUE;
            if (cand[i].kind == PLAN_ACT_PROTECT)
                protectW = cand[i].weight;
        }
        if (!hasMove)
            AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_NONE, 0, -1, MOVE_NONE, b, b, max(10, 100 - protectW));
    }

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
            // Sul compagno solo le mosse fatte per lui (Helping Hand & co.) o che gli cambiano l'abilità;
            // le combo che lo colpiscono (Beat Up -> Justified) le aggiunge il grafo della squadra.
            if (IsBattlerAlly(me, t) && GetMoveTarget(m) != TARGET_ALLY
             && !(IsBattleMoveStatus(m) && (GetMoveEffect(m) == EFFECT_OVERWRITE_ABILITY || GetMoveEffect(m) == EFFECT_SKILL_SWAP
                                          || GetMoveEffect(m) == EFFECT_GASTRO_ACID || GetMoveEffect(m) == EFFECT_ENTRAINMENT)))
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
    // Il cambio è candidato solo se non siamo appena entrati con un cambio (freno ai cambi a ripetizione).
    if (gAiLogicData->mostSuitableMonId[me] != PARTY_SIZE && CountUsablePartyMons(me) > 0 && !AI_VgcJustSwitchedIn(me))
    {
        u32 idx = gAiLogicData->mostSuitableMonId[me];
        enum BattlerId partner = GetPartnerBattler(me);
        if (idx < PARTY_SIZE && idx != gBattlerPartyIndexes[me] && !(IsBattlerAlive(partner) && gBattlerPartyIndexes[partner] == idx)
         && gBattleStruct->AI_monToSwitchIntoId[partner] != idx)
            AddCand(out, &n, PLAN_MAX_OURS, PLAN_ACT_SWITCH, 0, (s8)idx, MOVE_NONE, me, me, 0);
    }
    return n;
}

static bool32 IsOrigAction(const struct PlanAction *a, u32 origMove, enum BattlerId origTarget)
{
    return a->kind != PLAN_ACT_SWITCH && a->kind != PLAN_ACT_NONE && a->moveIndex == origMove && a->target == origTarget;
}

// Coppie impossibili o che si annullano da sole.
static bool32 IsPairIllegal(const struct PlanAction *x, const struct PlanAction *y)
{
    enum BattleMoveEffects ex = (x->kind == PLAN_ACT_MOVE) ? GetMoveEffect(x->move) : EFFECT_PLACEHOLDER;
    enum BattleMoveEffects ey = (y->kind == PLAN_ACT_MOVE) ? GetMoveEffect(y->move) : EFFECT_PLACEHOLDER;

    if (x->kind == PLAN_ACT_SWITCH && y->kind == PLAN_ACT_SWITCH && x->partyIdx == y->partyIdx)
        return TRUE;
    // Helping Hand serve solo se il partner attacca.
    if (ex == EFFECT_HELPING_HAND && (y->kind != PLAN_ACT_MOVE || IsBattleMoveStatus(y->move)))
        return TRUE;
    if (ey == EFFECT_HELPING_HAND && (x->kind != PLAN_ACT_MOVE || IsBattleMoveStatus(x->move)))
        return TRUE;
    // Due Trick Room si annullano; due meteo/terreni nello stesso turno sono uno spreco.
    if (ex == EFFECT_TRICK_ROOM && ey == EFFECT_TRICK_ROOM)
        return TRUE;
    if (ex == EFFECT_WEATHER && ey == EFFECT_WEATHER)
        return TRUE;
    return FALSE;
}

// Stima rapida (senza simulare) di quanto la coppia lavora bene insieme sui bersagli: due colpi che
// insieme mandano KO valgono di più; un secondo colpo su chi va già KO col primo vale meno (viene
// rediretto sull'altro avversario, che può resisterlo o esserne immune).
static s32 PairSynergy(const struct PlanState *st, const struct PlanAction *x, const struct PlanAction *y)
{
    s32 dx, dy, hp;
    enum BattlerId t;

    if (x->kind != PLAN_ACT_MOVE || y->kind != PLAN_ACT_MOVE || IsBattleMoveStatus(x->move) || IsBattleMoveStatus(y->move))
        return 0;
    if (x->target != y->target || IsBattlerAlly(x->user, x->target))
        return 0;
    if (IsSpreadMove(GetMoveTarget(x->move)) || IsSpreadMove(GetMoveTarget(y->move)))
        return 0;
    t = x->target;
    hp = st->m[t].hp;
    dx = DamageOf(st, x, t);
    dy = DamageOf(st, y, t);
    if (dx >= hp || dy >= hp)
        return -15;
    if (dx + dy >= hp)
        return 15;
    return 0;
}

// Azione rivolta al proprio compagno (combo: Simple Beam, Beat Up...).
static bool32 IsComboAction(const struct PlanAction *a)
{
    return a->kind == PLAN_ACT_MOVE && a->target != a->user && IsBattlerAlly(a->user, a->target);
}

// Combo sul compagno trovate dal grafo della squadra (togliergli un'abilità penalizzante, attivargli
// Justified & co.): entrano sempre tra le candidate, anche se le regole le valutano poco. Se convengono
// lo decide la simulazione.
static void AddComboCands(enum BattlerId b, enum BattlerId ally, struct PlanAction *list, u32 *n, s32 weight,
                          const struct VgcTeamGraph *g)
{
    u32 myIdx = gBattlerPartyIndexes[b], allyIdx = gBattlerPartyIndexes[ally], i, k;
    bool32 fix = g->abilityFixPartner[myIdx] == allyIdx, hit = g->hitPartner[myIdx] == allyIdx;

    if (!fix && !hit)
        return;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        enum Move move = gBattleMons[b].moves[i];
        enum BattleMoveEffects eff;
        bool32 combo, present = FALSE;

        if (move == MOVE_NONE || move == MOVE_UNAVAILABLE || gBattleMons[b].pp[i] == 0)
            continue;
        eff = GetMoveEffect(move);
        combo = (fix && (eff == EFFECT_OVERWRITE_ABILITY || eff == EFFECT_SKILL_SWAP || eff == EFFECT_GASTRO_ACID || eff == EFFECT_ENTRAINMENT))
             || (hit && !IsBattleMoveStatus(move) && (eff == EFFECT_BEAT_UP || GetMovePower(move) <= 40));
        if (!combo || !CanTargetBattler(b, ally, move))
            continue;
        for (k = 0; k < *n; k++)
            if (list[k].kind == PLAN_ACT_MOVE && list[k].moveIndex == i && list[k].target == ally)
                present = TRUE;
        if (present)
            continue;
        if (*n >= PLAN_MAX_OURS)
            *n = PLAN_MAX_OURS - 1; // lascia posto alla combo al posto dell'ultima candidata
        AddCand(list, n, PLAN_MAX_OURS, PLAN_ACT_MOVE, i, -1, move, b, ally, weight);
    }
}

// Coppie candidate per il primo turno: azioni nostre x azioni del partner (dalle regole), senza le coppie
// impossibili, ordinate per stima rapida; la coppia scelta dalle regole è sempre tra quelle simulate.
static u32 BuildPairs(enum BattlerId me, const struct PlanState *st)
{
    struct PlanAction mine[PLAN_MAX_OURS], theirs[PLAN_MAX_OURS];
    s32 pre[PLAN_MAX_PAIRS];
    s32 raw[PLAN_MAX_OURS][PLAN_MAX_OURS];
    u8 tier[PLAN_MAX_OURS][PLAN_MAX_OURS];
    u8 bestJ[PLAN_MAX_OURS], bestI[PLAN_MAX_OURS];
    enum BattlerId p = GetPartnerBattler(me);
    u32 nMine, nTheirs, i, j, k, n = 0;

    nMine = BuildOurFirstActions(me, mine, sJob.origMove, sJob.origTarget);
    if (p == me || !IsBattlerAlive(p))
    {
        // Da solo: le coppie sono le nostre azioni con il partner che non fa niente.
        struct PlanAction none = { PLAN_ACT_NONE, 0, -1, MOVE_NONE, p, p, 0 };
        for (i = 0; i < nMine && i < PLAN_MAX_PAIRS; i++)
        {
            sJob.ours[i] = mine[i];
            sJob.partners[i] = none;
        }
        return min(nMine, PLAN_MAX_PAIRS);
    }
    nTheirs = BuildOurFirstActions(p, theirs, sJob.origPMove, sJob.origPTarget);
    AddComboCands(me, p, mine, &nMine, nMine > 0 ? mine[0].weight : 0, &sJob.graph);
    AddComboCands(p, me, theirs, &nTheirs, nTheirs > 0 ? theirs[0].weight : 0, &sJob.graph);
    if (nTheirs == 0)
    {
        struct PlanAction none = { PLAN_ACT_NONE, 0, -1, MOVE_NONE, p, p, 0 };
        theirs[0] = none;
        nTheirs = 1;
    }
    // Il cambio pesa 0 nelle regole: gli si dà un peso medio perché possa entrare tra le coppie.
    for (i = 0; i < nMine; i++)
        if (mine[i].kind == PLAN_ACT_SWITCH)
            mine[i].weight = mine[0].weight - 20;
    for (j = 0; j < nTheirs; j++)
        if (theirs[j].kind == PLAN_ACT_SWITCH)
            theirs[j].weight = theirs[0].weight - 20;

    // Stima rapida di tutte le coppie legali, poi scelta a livelli: (1) la coppia delle regole, termine di
    // paragone; (2) le coppie con una combo sul compagno; (3) per ogni azione, nostra o del partner, la sua
    // coppia migliore (così nessuna mossa resta fuori solo perché le regole la valutano poco); (4) il resto.
    for (i = 0; i < nMine; i++)
        bestJ[i] = 0xFF;
    for (j = 0; j < nTheirs; j++)
        bestI[j] = 0xFF;
    for (i = 0; i < nMine; i++)
    {
        for (j = 0; j < nTheirs; j++)
        {
            tier[i][j] = 0xFF; // 0xFF = illegale
            if (IsPairIllegal(&mine[i], &theirs[j]))
                continue;
            raw[i][j] = mine[i].weight + theirs[j].weight + PairSynergy(st, &mine[i], &theirs[j]);
            tier[i][j] = 0;
            if (IsOrigAction(&mine[i], sJob.origMove, sJob.origTarget) && IsOrigAction(&theirs[j], sJob.origPMove, sJob.origPTarget))
                tier[i][j] = 3;
            else if (IsComboAction(&mine[i]) || IsComboAction(&theirs[j]))
                tier[i][j] = 2;
            if (bestJ[i] == 0xFF || raw[i][j] > raw[i][bestJ[i]])
                bestJ[i] = j;
            if (bestI[j] == 0xFF || raw[i][j] > raw[bestI[j]][j])
                bestI[j] = i;
        }
    }
    for (i = 0; i < nMine; i++)
        if (bestJ[i] != 0xFF && tier[i][bestJ[i]] < 1)
            tier[i][bestJ[i]] = 1;
    for (j = 0; j < nTheirs; j++)
        if (bestI[j] != 0xFF && tier[bestI[j]][j] < 1)
            tier[bestI[j]][j] = 1;

    for (i = 0; i < nMine; i++)
    {
        for (j = 0; j < nTheirs; j++)
        {
            s32 score;

            if (tier[i][j] == 0xFF)
                continue;
            score = tier[i][j] * 100000 + raw[i][j];
            // Inserimento ordinato nelle prime PLAN_MAX_PAIRS.
            for (k = n; k > 0 && pre[k - 1] < score; k--)
            {
                if (k < PLAN_MAX_PAIRS)
                {
                    pre[k] = pre[k - 1];
                    sJob.ours[k] = sJob.ours[k - 1];
                    sJob.partners[k] = sJob.partners[k - 1];
                }
            }
            if (k < PLAN_MAX_PAIRS)
            {
                pre[k] = score;
                sJob.ours[k] = mine[i];
                sJob.partners[k] = theirs[j];
                if (n < PLAN_MAX_PAIRS)
                    n++;
            }
        }
    }
    return n;
}

// ---------------------------------------------------------------------
// Lavoro a pezzi
// ---------------------------------------------------------------------

// Precisione: la simulazione applica il colpo pieno (un KO resta un KO). Per ogni azione del turno che può
// mancare si rigioca il turno con quel colpo mancato e si toglie dal valore la perdita attesa:
// (1 - precisione) x (valore col colpo - valore senza). Un KO all'80% vale l'80% del KO; una mossa ad area
// al 90% che manda KO due bersagli vale ~1,8 KO; due mosse che fanno lo stesso KO preferiscono la più precisa.
// La differenza si misura sul turno 1 e si assume uguale sul resto della linea (costo: un turno simulato in
// più per azione imprecisa, non un albero intero). Ritorna il numero di simulazioni fatte.
static u32 MissPenalty(const struct PlanState *base, const struct PlanState *hit, const struct PlanAction *const *acts4, s32 *pen)
{
    u32 k, sims = 0;
    s32 eHit = 0;

    *pen = 0;
    for (k = 0; k < 4; k++)
    {
        u32 acc = ActionAccuracy(base, acts4[k]);
        const struct PlanAction *acts[4];
        struct PlanState sm;
        s32 eMiss;

        if (acc >= 100)
            continue;
        if (sims == 0)
            eHit = Evaluate(hit);
        memcpy(acts, acts4, sizeof(acts));
        sm = *base;
        sMissBattler = acts4[k]->user + 1;
        RunTurn(&sm, acts, 4);
        sMissBattler = 0;
        eMiss = Evaluate(&sm);
        // Per il giocatore, un suo colpo mancato è un guadagno per noi: il segno viene da Evaluate.
        *pen += (s32)(100 - acc) * (eHit - eMiss) / 100;
        sims++;
    }
    return sims;
}
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

    u32 chosenIdx = PLAN_MAX_PAIRS;
    enum BattlerId partner = GetPartnerBattler(sJob.me);

    for (i = 0; i < sJob.nOurs; i++)
    {
        if (IsOrigAction(&sJob.ours[i], sJob.origMove, sJob.origTarget)
         && (sJob.partners[i].kind == PLAN_ACT_NONE || IsOrigAction(&sJob.partners[i], sJob.origPMove, sJob.origPTarget)))
        {
            chosenEv = sJob.ev[i];
            chosenIdx = i;
        }
        if (sJob.ev[i] > bestEv)
        {
            bestEv = sJob.ev[i];
            bestIdx = i;
        }
    }
    // A pari valore resta la coppia delle regole.
    if (chosenIdx != PLAN_MAX_PAIRS && bestEv < chosenEv + PLAN_OVERRIDE_MARGIN)
        bestIdx = chosenIdx;
#if TESTING
    {
        // Diagnostica nei test: valore di ogni coppia simulata (mossa/bersaglio nostra e del partner).
        extern s32 Test_MgbaPrintf(const char *fmt, ...);
        for (i = 0; i < sJob.nOurs; i++)
            Test_MgbaPrintf("VGCPLAN b%d pair%d: k%d m%d t%d + k%d m%d t%d = %d%s", sJob.me, i,
                            sJob.ours[i].kind, sJob.ours[i].move, sJob.ours[i].target,
                            sJob.partners[i].kind, sJob.partners[i].move, sJob.partners[i].target,
                            sJob.ev[i], i == bestIdx ? " <==" : (i == chosenIdx ? " (regole)" : ""));
        for (i = 0; i < sJob.nOurs; i++)
        {
            const struct PlanAction *a = &sJob.ours[i];
            if (a->kind != PLAN_ACT_MOVE)
                continue;
            Test_MgbaPrintf("VGCDMG pair%d: m%d idx%d t%d dmg=%d hp=%d eff=%d median=%d", i, a->move, a->moveIndex, a->target,
                            DamageOf(&sJob.base, a, a->target), sJob.base.m[a->target].hp,
                            a->moveIndex < MAX_MON_MOVES ? gAiLogicData->effectiveness[a->user][a->target][a->moveIndex] : -1,
                            a->moveIndex < MAX_MON_MOVES ? gAiLogicData->simulatedDmg[a->user][a->target][a->moveIndex].median : -1);
        }
        for (i = 0; i < sJob.nA; i++)
            Test_MgbaPrintf("VGCFOE A%d: k%d m%d t%d w%d", i, sJob.foeA[i].kind, sJob.foeA[i].move, sJob.foeA[i].target, sJob.foeA[i].weight);
        for (i = 0; i < sJob.nB; i++)
            Test_MgbaPrintf("VGCFOE B%d: k%d m%d t%d w%d", i, sJob.foeB[i].kind, sJob.foeB[i].move, sJob.foeB[i].target, sJob.foeB[i].weight);
    }
#endif

    sJob.resultKind = sJob.ours[bestIdx].kind;
    sJob.resultMove = sJob.ours[bestIdx].moveIndex;
    sJob.resultTarget = sJob.ours[bestIdx].target;
    sJob.resultParty = sJob.ours[bestIdx].partyIdx;

    // La metà del partner della coppia vincente vale per lui quando tocca a lui scegliere.
    sPartnerDecision.valid = FALSE;
    if (partner != sJob.me && sJob.partners[bestIdx].kind != PLAN_ACT_NONE)
    {
        sPartnerDecision.valid = TRUE;
        sPartnerDecision.turn = gBattleTurnCounter;
        sPartnerDecision.battler = partner;
        sPartnerDecision.kind = sJob.partners[bestIdx].kind;
        sPartnerDecision.move = sJob.partners[bestIdx].moveIndex;
        sPartnerDecision.target = sJob.partners[bestIdx].target;
        sPartnerDecision.party = sJob.partners[bestIdx].partyIdx;
    }
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

// Chi mandare in campo dopo un KO: tra i Pokémon utilizzabili in panchina, quello che fa la coppia migliore
// (grafo della squadra) con chi è in campo o sta per entrare accanto; a pari sinergia vale la scelta del motore.
// Il ricevente designato di un Baton Passer ancora vivo in panchina resta indietro.
u32 AI_VgcReplacementAfterKo(enum BattlerId battler, u32 defaultIdx)
{
    struct VgcTeamGraph graph;
    struct Pokemon *party = GetBattlerParty(battler);
    enum BattlerId p = GetPartnerBattler(battler);
    u32 mate = PARTY_SIZE, i, best = defaultIdx;
    s32 bestScore = INT32_MIN;

    AI_VgcBuildTeamGraph(party, &graph);
    if (IsBattlerAlive(p))
        mate = gBattlerPartyIndexes[p];
    else if (gBattleStruct->AI_monToSwitchIntoId[p] < PARTY_SIZE)
        mate = gBattleStruct->AI_monToSwitchIntoId[p];

    for (i = 0; i < PARTY_SIZE; i++)
    {
        s32 score = 0;
        u32 j;
        bool32 reserved = FALSE;

        if (GetMonData(&party[i], MON_DATA_SPECIES) == SPECIES_NONE || GetMonData(&party[i], MON_DATA_IS_EGG)
         || GetMonData(&party[i], MON_DATA_HP) == 0 || i == gBattlerPartyIndexes[battler] || i == mate)
            continue;
        // Riservato al Baton Pass di un compagno ancora in gioco (in campo o in panchina).
        for (j = 0; j < PARTY_SIZE; j++)
            if (j != i && graph.batonRecipient[j] == i && GetMonData(&party[j], MON_DATA_HP) > 0)
                reserved = TRUE;
        if (mate < PARTY_SIZE)
            score += graph.leadPair[mate][i];
        if (i == defaultIdx)
            score += 40;
        if (reserved)
            score -= 80;
        if (score > bestScore)
        {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

// Chi deve entrare quando `battler` usa Baton Pass (PARTY_SIZE = nessuno di adatto).
u32 AI_VgcBatonRecipient(enum BattlerId battler)
{
    struct VgcTeamGraph graph;
    struct Pokemon *party = GetBattlerParty(battler);
    enum BattlerId p = GetPartnerBattler(battler);
    u32 myIdx = gBattlerPartyIndexes[battler];
    u32 pIdx = IsBattlerAlive(p) ? gBattlerPartyIndexes[p] : PARTY_SIZE;
    u32 idx;

    AI_VgcBuildTeamGraph(party, &graph);
    idx = graph.batonRecipient[myIdx];
    if (idx == gBattleStruct->AI_monToSwitchIntoId[p] || !CanBatonPassTo(party, idx, myIdx, pIdx))
        return PARTY_SIZE;
    return idx;
}

// Inizio turno (prima della scelta delle azioni): niente lavori o decisioni rimasti dal turno o dalla lotta prima.
void AI_VgcPlanResetTurn(void)
{
    sJob.active = FALSE;
    sJob.finished = FALSE;
    sPartnerDecision.valid = FALSE;
}

// TRUE se il pianificatore lavora (o ha un risultato non ancora applicato) per un altro battler in questo turno.
bool32 AI_VgcPlanIsBusy(enum BattlerId battler)
{
    return sJob.active && sJob.me != battler && sJob.turn == gBattleTurnCounter;
}

bool32 AI_VgcPlanBegin(enum BattlerId me)
{
    struct PlanAction none = { PLAN_ACT_NONE, 0, -1, MOVE_NONE, 0, 0, 100 };
    enum BattlerId fL = GetBattlerLeftFoe(me), fR = GetBattlerRightFoe(me);

    if (!IsDoubleBattle() || AI_VgcPlanIsActive() || AI_VgcPlanIsBusy(me))
        return FALSE;
    memset(&sJob, 0, sizeof(sJob));
    sJob.me = me;
    sJob.turn = gBattleTurnCounter;
    sJob.origMove = gAiBattleData->chosenMoveIndex[me];
    sJob.origTarget = gAiBattleData->chosenTarget[me];

    // Il partner ha già pianificato la coppia in questo turno: si applica la nostra metà.
    if (sPartnerDecision.valid && sPartnerDecision.battler == me && sPartnerDecision.turn == gBattleTurnCounter)
    {
        sPartnerDecision.valid = FALSE;
        sJob.resultKind = sPartnerDecision.kind;
        sJob.resultMove = sPartnerDecision.move;
        sJob.resultTarget = sPartnerDecision.target;
        sJob.resultParty = sPartnerDecision.party;
        sJob.active = TRUE;
        sJob.finished = TRUE;
        return TRUE;
    }

    {
        enum BattlerId p = GetPartnerBattler(me);
        sJob.origPMove = gAiBattleData->chosenMoveIndex[p];
        sJob.origPTarget = gAiBattleData->chosenTarget[p];
    }
    AI_VgcBuildTeamGraph(GetBattlerParty(me), &sJob.graph);
    InitState(&sJob.base);
    sJob.nOurs = BuildPairs(me, &sJob.base);
    if (sJob.nOurs < 2)
        return FALSE;
    sJob.nA = BuildActions(&sJob.base, fL, sJob.foeA, PLAN_FOE_PER, FALSE, TRUE);
    sJob.nB = BuildActions(&sJob.base, fR, sJob.foeB, PLAN_FOE_PER, FALSE, TRUE);
    if (sJob.nA == 0) { sJob.foeA[0] = none; sJob.foeA[0].user = fL; sJob.nA = 1; }
    if (sJob.nB == 0) { sJob.foeB[0] = none; sJob.foeB[0].user = fR; sJob.nB = 1; }
    sJob.worst = INT32_MAX;
    sJob.active = TRUE;
    sJob.finished = FALSE;
    return TRUE;
}

// Righe di schermo passate da `start` (il contatore VCOUNT va da 0 a 227 e ricomincia).
static u32 ScanlinesSince(u32 start)
{
    return (REG_VCOUNT + 228 - start) % 228;
}

// Esegue simulazioni finché il tempo del fotogramma lo permette: si ferma dopo
// CUSTOM_VGC_PLAN_SCANLINES_PER_FRAME righe di schermo (così il gioco e i menu del giocatore restano
// fluidi qualunque sia il costo della singola simulazione) o dopo CUSTOM_VGC_PLAN_SIMS_PER_FRAME
// simulazioni. TRUE = finito (o nulla in corso).
bool32 AI_VgcPlanStep(void)
{
    s32 budget = CUSTOM_VGC_PLAN_SIMS_PER_FRAME;
    u32 start = REG_VCOUNT;

    if (!sJob.active)
        return TRUE;
    while (budget > 0 && !sJob.finished && ScanlinesSince(start) < CUSTOM_VGC_PLAN_SCANLINES_PER_FRAME)
    {
        if (sJob.phase == 0)
        {
            const struct PlanAction *acts[4] = { &sJob.ours[sJob.i], &sJob.partners[sJob.i], &sJob.foeA[sJob.a], &sJob.foeB[sJob.b] };
            const struct PlanAction *acts4[4] = { &sJob.ours[sJob.i], &sJob.partners[sJob.i], &sJob.foeA[sJob.a], &sJob.foeB[sJob.b] };
            sJob.s1 = sJob.base;
            RunTurn(&sJob.s1, acts, 4);
            budget--;
            budget -= MissPenalty(&sJob.base, &sJob.s1, acts4, &sJob.missPen);
            if (CUSTOM_VGC_PLAN_DEPTH >= 2)
            {
                StartLookAhead();
            }
            else
            {
                Accumulate(Evaluate(&sJob.s1) - sJob.missPen, PairWeight(&sJob.foeA[sJob.a], &sJob.foeB[sJob.b]));
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
                            s32 v1 = (Evaluate(&sJob.s1) + sJob.laBest) / 2 - sJob.missPen;
#if TESTING
                            {
                                extern s32 Test_MgbaPrintf(const char *fmt, ...);
                                Test_MgbaPrintf("VGCLEAF pair%d foes(%d,%d): turno1=%d turno2=%d mancati=-%d", sJob.i, sJob.a, sJob.b,
                                                Evaluate(&sJob.s1), sJob.laBest, sJob.missPen);
                            }
#endif
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
            AI_VgcNoteSwitchIn(me);
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
