// IA per le lotte in doppio stile VGC (AI_FLAG_VGC_DOUBLES): regole, letture e piano di squadra.
//
// La scelta del turno la fa il pianificatore (battle_ai_vgc_plan.c), che simula le coppie di azioni. Qui:
//   - filtro delle candidate (AI_VgcFilterAction) e spinta leggera (AI_VgcActionPrior): sono tutto ciò che resta
//     delle regole a punteggi (M8), applicati allo stesso modo al pianificatore e al punteggio del motore;
//   - lettura dell'avversario senza imbrogli (probabilità di Protect e di cambio, memoria del Bo3);
//   - piano di squadra (vgc_gameplans.h) per la team preview, velocità della squadra, Mega.
// Tutto parte da ciò che l'IA sa degli avversari (mosse/abilità/strumenti visti o presunti), mai dai dati nascosti.
#include "global.h"
#include "battle.h"
#include "battle_ai_main.h"
#include "battle_ai_util.h"
#include "battle_ai_vgc.h"
#include "team_preview.h"
#include "battle_ai_scout.h"
#include "battle_ai_vgc_graph.h"
#include "battle_ai_vgc_book.h"
#include "config/custom.h"
#include "battle_gimmick.h"
#include "battle_main.h"
#include "battle_setup.h"
#include "battle_util.h"
#include "item.h"
#include "move.h"
#include "pokemon.h"
#include "random.h"
#include "constants/abilities.h"
#include "constants/battle_ai.h"
#include "constants/battle_move_effects.h"
#include "constants/hold_effects.h"
#include "constants/moves.h"
#include "constants/opponents.h"
#include "constants/species.h"
#include "data/vgc_gameplans.h"
#include "battle_ai_perspective.h"

// IA contro IA (B_FLAG_AI_VS_AI_BATTLE): il "giocatore" di questo file è il lato avversario di chi sta
// pensando (battle_ai_perspective.h). Fuori da IA contro IA è identico a IsOnPlayerSide.
#define IsOnPlayerSide(battler) AI_IsFoeOfPerspective(battler)

// ---------------------------------------------------------------------
// Piano di squadra (src/data/vgc_gameplans.h)
// ---------------------------------------------------------------------
const struct VgcGameplan *AI_VgcGetPlan(u16 trainerId)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sVgcGameplans); i++)
    {
        if (sVgcGameplans[i].trainerId == trainerId)
            return &sVgcGameplans[i];
    }
    return NULL;
}

// Stessa specie "di squadra": confronto sulla specie base da entrambi i lati, così Mega, forme
// regionali e forme alternative scritte nel piano (Indeedee-F, Goodra-Hisui, Rotom-Mow,
// Dugtrio-Alola, Weezing-Galar...) corrispondono al Pokémon in campo in qualunque forma.
bool32 AI_VgcSameSpecies(u16 planSpecies, u16 species)
{
    if (planSpecies == SPECIES_NONE || species == SPECIES_NONE)
        return FALSE;
    return planSpecies == species || GET_BASE_SPECIES_ID(planSpecies) == GET_BASE_SPECIES_ID(species);
}

static bool32 PlanListHas(const u16 *list, u32 n, u16 species)
{
    u32 i;
    for (i = 0; i < n; i++)
    {
        if (AI_VgcSameSpecies(list[i], species))
            return TRUE;
    }
    return FALSE;
}

bool32 AI_VgcPlanIsTrickRoomTeam(u16 trainerId)
{
    const struct VgcGameplan *plan = AI_VgcGetPlan(trainerId);
    return plan != NULL && plan->modeA == VGC_PLAN_TRICK_ROOM;
}

bool32 AI_VgcPlanIsWinCon(u16 trainerId, u16 species)
{
    const struct VgcGameplan *plan = AI_VgcGetPlan(trainerId);
    return plan != NULL && PlanListHas(plan->winCons, 2, species);
}

bool32 AI_VgcPlanIsSetter(u16 trainerId, u16 species)
{
    const struct VgcGameplan *plan = AI_VgcGetPlan(trainerId);
    return plan != NULL && PlanListHas(plan->setters, 2, species);
}

bool32 AI_VgcPlanIsBack(u16 trainerId, u16 species)
{
    const struct VgcGameplan *plan = AI_VgcGetPlan(trainerId);
    return plan != NULL && PlanListHas(plan->back, 2, species);
}

// Una modalità "conviene" contro la squadra attuale del giocatore?
static bool32 PlanModeViable(u8 mode, bool32 trViable, bool32 twViable)
{
    switch (mode)
    {
    case VGC_PLAN_TRICK_ROOM: return trViable;
    case VGC_PLAN_TAILWIND:   return twViable;
    default:                  return TRUE;
    }
}

// Bonus (team preview) per la coppia di lead a+b secondo il piano. trViable / twViable: se Trick
// Room / Tailwind convengono contro la squadra del giocatore (calcolato dal chiamante).
s32 AI_VgcPlanLeadBonus(u16 trainerId, u16 a, u16 b, bool32 trViable, bool32 twViable)
{
    const struct VgcGameplan *plan = AI_VgcGetPlan(trainerId);
    u32 i;
    s32 bonus = 0;

    if (plan == NULL)
        return 0;
    for (i = 0; i < VGC_PLAN_LEADS; i++)
    {
        const struct VgcLeadPair *p = &plan->leads[i];
        if (p->a == SPECIES_NONE)
            continue;
        if ((AI_VgcSameSpecies(p->a, a) && AI_VgcSameSpecies(p->b, b)) || (AI_VgcSameSpecies(p->a, b) && AI_VgcSameSpecies(p->b, a)))
        {
            bonus = 120 - 30 * (s32)i;
            if (!PlanModeViable(p->mode, trViable, twViable))
                bonus /= 2;
            break;
        }
    }
    // Chi di norma sta dietro non va in lead.
    if (PlanListHas(plan->back, 2, a))
        bonus -= 40;
    if (PlanListHas(plan->back, 2, b))
        bonus -= 40;
    return bonus;
}

// Bonus (team preview) a portare questa specie tra i 4.
s32 AI_VgcPlanBringBonus(u16 trainerId, u16 species, bool32 trViable, bool32 twViable)
{
    const struct VgcGameplan *plan = AI_VgcGetPlan(trainerId);
    s32 bonus = 0;

    if (plan == NULL)
        return 0;
    if (PlanListHas(plan->winCons, 2, species))
        bonus += 50;
    if (PlanListHas(plan->setters, 2, species))
        bonus += PlanModeViable(plan->modeA, trViable, twViable) ? 35 : 15;
    return bonus;
}

// La modalità del piano è attiva in campo adesso (per decidere i cambi verso la win condition).
bool32 AI_VgcPlanModeActive(enum BattlerId battler)
{
    const struct VgcGameplan *plan = AI_VgcGetPlan(AI_TrainerIdOfBattler(battler));
    u8 modes[2];
    u32 i;

    if (plan == NULL)
        return FALSE;
    modes[0] = plan->modeA;
    modes[1] = plan->modeB;
    for (i = 0; i < 2; i++)
    {
        switch (modes[i])
        {
        case VGC_PLAN_TRICK_ROOM:
            if ((gFieldStatuses & STATUS_FIELD_TRICK_ROOM) && gFieldTimers.trickRoomTimer > 1)
                return TRUE;
            break;
        case VGC_PLAN_TAILWIND:
            if ((gSideStatuses[GetBattlerSide(battler)] & SIDE_STATUS_TAILWIND) && gSideTimers[GetBattlerSide(battler)].tailwindTimer > 1)
                return TRUE;
            break;
        case VGC_PLAN_GRAVITY:
            if ((gFieldStatuses & STATUS_FIELD_GRAVITY) && gFieldTimers.gravityTimer > 1)
                return TRUE;
            break;
        case VGC_PLAN_SUN:
            if (IsWeatherActive(B_WEATHER_SUN) != WEATHER_INACTIVE)
                return TRUE;
            break;
        case VGC_PLAN_RAIN:
            if (IsWeatherActive(B_WEATHER_RAIN) != WEATHER_INACTIVE)
                return TRUE;
            break;
        case VGC_PLAN_SAND:
            if (IsWeatherActive(B_WEATHER_SANDSTORM) != WEATHER_INACTIVE)
                return TRUE;
            break;
        default:
            break;
        }
    }
    return FALSE;
}

// Mossa con priorità nota di un avversario (per Quick Guard).
static bool32 FoeHasKnownPriorityMove(enum BattlerId foe)
{
    enum Move *moves;
    u32 i;

    if (!IsBattlerAlive(foe))
        return FALSE;
    moves = GetMovesArray(foe);
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] == MOVE_NONE || moves[i] == MOVE_UNAVAILABLE || IsBattleMoveStatus(moves[i]))
            continue;
        if (GetMovePriority(moves[i]) > 0)
            return TRUE;
    }
    return FALSE;
}

// ---------------------------------------------------------------------
// Utilità sulla squadra (campo + panchina)
// ---------------------------------------------------------------------
static u32 FoeMaxSpeed(enum BattlerId battler)
{
    enum BattlerId foeL = GetBattlerLeftFoe(battler), foeR = GetBattlerRightFoe(battler);
    u32 s = 0;

    if (IsBattlerAlive(foeL))
        s = max(s, (u32)gAiLogicData->speedStats[foeL]);
    if (IsBattlerAlive(foeR))
        s = max(s, (u32)gAiLogicData->speedStats[foeR]);
    return s;
}

static u32 FoeMinSpeed(enum BattlerId battler)
{
    enum BattlerId foeL = GetBattlerLeftFoe(battler), foeR = GetBattlerRightFoe(battler);
    u32 s = 0xFFFF;

    if (IsBattlerAlive(foeL))
        s = min(s, (u32)gAiLogicData->speedStats[foeL]);
    if (IsBattlerAlive(foeR))
        s = min(s, (u32)gAiLogicData->speedStats[foeR]);
    return (s == 0xFFFF) ? 0 : s;
}

// Velocità di un membro della squadra: in campo quella reale (con modifiche), in panchina quella base.
static u32 TeamMonSpeed(enum BattlerId battler, u32 partyIndex, struct Pokemon *mon)
{
    enum BattlerId partner = GetPartnerBattler(battler);

    if (gBattlerPartyIndexes[battler] == partyIndex)
        return gAiLogicData->speedStats[battler];
    if (partner != battler && IsBattlerAlive(partner) && gBattlerPartyIndexes[partner] == partyIndex)
        return gAiLogicData->speedStats[partner];
    return GetMonData(mon, MON_DATA_SPEED);
}

bool32 AI_VgcTeamIsSlowerThanFoes(enum BattlerId battler, u32 minCount)
{
    struct Pokemon *party = GetBattlerParty(battler);
    u32 foeMin = FoeMinSpeed(battler);
    u32 i, slow = 0;

    if (foeMin == 0)
        return FALSE;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (GetMonData(&party[i], MON_DATA_SPECIES) == SPECIES_NONE || GetMonData(&party[i], MON_DATA_HP) == 0)
            continue;
        if (TeamMonSpeed(battler, i, &party[i]) < foeMin)
            slow++;
    }
    return slow >= minCount;
}

bool32 AI_VgcTeamBenefitsFromTailwind(enum BattlerId battler, u32 minCount)
{
    struct Pokemon *party = GetBattlerParty(battler);
    u32 foeMax = FoeMaxSpeed(battler);
    u32 i, n = 0;

    if (foeMax == 0)
        return FALSE;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        u32 spd;
        if (GetMonData(&party[i], MON_DATA_SPECIES) == SPECIES_NONE || GetMonData(&party[i], MON_DATA_HP) == 0)
            continue;
        spd = TeamMonSpeed(battler, i, &party[i]);
        if (spd <= foeMax && spd * 2 > foeMax)
            n++;
    }
    return n >= minCount;
}

// Abilità che sfrutta il meteo portato da questa mossa (Chlorophyll e Protosynthesis al sole, Swift Swim alla pioggia...).
static bool32 MonAbusesWeather(enum Ability ability, enum Move move)
{
    switch (GetMoveWeatherType(move))
    {
    case BATTLE_WEATHER_SUN:
        return ability == ABILITY_CHLOROPHYLL || ability == ABILITY_SOLAR_POWER || ability == ABILITY_PROTOSYNTHESIS
            || ability == ABILITY_LEAF_GUARD || ability == ABILITY_ORICHALCUM_PULSE;
    case BATTLE_WEATHER_RAIN:
        return ability == ABILITY_SWIFT_SWIM || ability == ABILITY_RAIN_DISH || ability == ABILITY_DRY_SKIN
            || ability == ABILITY_HYDRATION;
    case BATTLE_WEATHER_SANDSTORM:
        return ability == ABILITY_SAND_RUSH || ability == ABILITY_SAND_FORCE || ability == ABILITY_SAND_VEIL;
    case BATTLE_WEATHER_HAIL:
    case BATTLE_WEATHER_SNOW:
        return ability == ABILITY_SLUSH_RUSH || ability == ABILITY_ICE_BODY || ability == ABILITY_SNOW_CLOAK;
    default:
        return FALSE;
    }
}

#if TESTING
// (Solo le regole di prima di M8, battle_ai_vgc_legacy.h.)
static bool32 TeamHasWeatherAbuser(enum BattlerId battler, enum Move move)
{
    struct Pokemon *party = GetBattlerParty(battler);
    u32 i;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (GetMonData(&party[i], MON_DATA_SPECIES) == SPECIES_NONE || GetMonData(&party[i], MON_DATA_HP) == 0)
            continue;
        if (MonAbusesWeather(GetMonAbility(&party[i]), move))
            return TRUE;
    }
    return FALSE;
}
#endif

bool32 AI_VgcOwnSpeedControlActive(enum BattlerId battler)
{
    enum BattleSide side = GetBattlerSide(battler);

    if ((gSideStatuses[side] & SIDE_STATUS_TAILWIND) && gSideTimers[side].tailwindTimer > 1)
        return TRUE;
    if ((gFieldStatuses & STATUS_FIELD_TRICK_ROOM) && gFieldTimers.trickRoomTimer > 1
     && AI_VgcTeamIsSlowerThanFoes(battler, 1))
        return TRUE;
    return FALSE;
}

// Un avversario vivo ha Fake Out (per quanto ne sappiamo) ed è al primo turno.
static bool32 FoeThreatensFakeOut(enum BattlerId battler)
{
    enum BattlerId foeL = GetBattlerLeftFoe(battler), foeR = GetBattlerRightFoe(battler);

    if (IsBattlerAlive(foeL) && IsBattlersFirstTurn(foeL)
     && (HasMove(foeL, MOVE_FAKE_OUT) || (IsOnPlayerSide(foeL) && Scout_BattlerRoleChance(foeL, SCOUT_FAKE_OUT) >= 50)))
        return TRUE;
    if (IsBattlerAlive(foeR) && IsBattlersFirstTurn(foeR)
     && (HasMove(foeR, MOVE_FAKE_OUT) || (IsOnPlayerSide(foeR) && Scout_BattlerRoleChance(foeR, SCOUT_FAKE_OUT) >= 50)))
        return TRUE;
    return FALSE;
}

// ---------------------------------------------------------------------
// Lettura dell'avversario ("predict") SENZA imbrogliare
// ---------------------------------------------------------------------
// L'IA non legge le scelte del giocatore: stima Protect e cambio da ciò che è lecito sapere
// (mosse viste, situazione in campo) e da come il giocatore si è comportato finora nella serie
// (quante volte ha protetto quando era sotto KO, quante volte ha cambiato quando perdeva
// il matchup). La memoria è per Pokémon (personalità) e resiste tra le gare del Bo3.
struct VgcBehaviour
{
    u32 personality;
    u8 threatenedTurns;         // turni in cui l'IA poteva mandarlo KO
    u8 protectsWhenThreatened;  // ...e lui ha usato Protect
    u8 losingTurns;             // turni in cui perdeva il matchup (noi KO, lui no) con panchina disponibile
    u8 switchesWhenLosing;      // ...e lui ha cambiato
};

static EWRAM_DATA struct VgcBehaviour sVgcMem[PARTY_SIZE * NUM_BATTLE_SIDES] = {0}; // (IA contro IA: si osservano entrambi i lati)
static EWRAM_DATA u8 sVgcMemCount = 0;
// Storia dei bersagli del giocatore in questa lotta (solo attacchi visti, per lato di chi attacca): con entrambi i nostri
// in campo, quante volte ha colpito il più danneggiato dei due e, quando uno solo è la win condition, quante volte lei.
// (struct VgcTargetHistory è in battle_ai_vgc.h: nel Bo3 passa alla gara dopo con la memoria della serie.)
static EWRAM_DATA struct VgcTargetHistory sVgcTargetHist[NUM_BATTLE_SIDES] = {0};
static EWRAM_DATA u8 sVgcThreatened = 0;        // bitmask battler del giocatore sotto KO questo turno
static EWRAM_DATA u8 sVgcLosing = 0;            // bitmask battler del giocatore che perdono il matchup
static EWRAM_DATA u8 sVgcProtectedLastTurn = 0; // bitmask battler che hanno usato Protect al turno scorso
static EWRAM_DATA u8 sVgcProtectedThisTurn = 0;
static EWRAM_DATA u8 sVgcReadRoll[NUM_BATTLE_SIDES] = {0}; // 0..99, tirato una volta a turno (per lato che legge): quanto fidarsi della lettura
static EWRAM_DATA enum Move sVgcPlayerLastMove[MAX_BATTLERS_COUNT] = {0}; // ultima mossa di ogni Pokémon del giocatore (blocco Choice)
static EWRAM_DATA u8 sVgcInferredScarf = 0;             // bitmask: Stolascelta dedotta dalla velocità
static EWRAM_DATA struct VgcTeamGraph sVgcTeamGraphBySide[NUM_BATTLE_SIDES] = {0}; // conoscenza della squadra dell'IA (ricalcolata a ogni turno; per lato in IA contro IA)
#define sVgcTeamGraph sVgcTeamGraphBySide[AI_GetPerspectiveSide()]

// Ruoli (dal grafo della squadra) di un Pokémon dell'IA in campo; 0 per il giocatore.
static u32 VgcRoles(enum BattlerId battler)
{
    if (IsOnPlayerSide(battler) || gBattlerPartyIndexes[battler] >= PARTY_SIZE)
        return 0;
    return sVgcTeamGraph.roles[gBattlerPartyIndexes[battler]];
}

void AI_VgcResetMemory(void)
{
    memset(sVgcMem, 0, sizeof(sVgcMem));
    memset(sVgcPlayerLastMove, 0, sizeof(sVgcPlayerLastMove));
    sVgcInferredScarf = 0;
    sVgcMemCount = 0;
    sVgcThreatened = sVgcLosing = sVgcProtectedLastTurn = sVgcProtectedThisTurn = 0;
    memset(sVgcTargetHist, 0, sizeof(sVgcTargetHist));
}

static bool32 VgcIsWinCon(enum BattlerId b)
{
    return AI_VgcPlanIsWinCon(AI_TrainerIdOfBattler(b), gBattleMons[b].species);
}

// Registra il bersaglio di un attacco visto (mossa a bersaglio singolo su un avversario, con il suo compagno in campo).
static void VgcNoteTarget(enum BattlerId atk, enum Move move)
{
    struct VgcTargetHistory *h = &sVgcTargetHist[GetBattlerSide(atk)];
    enum BattlerId t = gBattleStruct->moveTarget[atk], p;
    enum MoveTarget tt;
    u32 pt, pp;
    bool32 wt, wp;

    if (move == MOVE_NONE || IsBattleMoveStatus(move) || t >= gBattlersCount || IsBattlerAlly(atk, t) || !IsBattlerAlive(t))
        return;
    tt = GetBattlerMoveTargetType(atk, move);
    if (tt != TARGET_SELECTED && tt != TARGET_SMART && tt != TARGET_OPPONENT)
        return;
    p = GetPartnerBattler(t);
    if (!IsBattlerAlive(p))
        return;
    pt = gBattleMons[t].hp * 100 / max(gBattleMons[t].maxHP, 1);
    pp = gBattleMons[p].hp * 100 / max(gBattleMons[p].maxHP, 1);
    if (h->attacks < 255 && pt != pp)
    {
        h->attacks++;
        if (pt < pp)
            h->onWeakest++;
    }
    wt = VgcIsWinCon(t);
    wp = VgcIsWinCon(p);
    if (wt != wp && h->winChoices < 255)
    {
        h->winChoices++;
        if (wt)
            h->onWincon++;
    }
}

// Spinta (in punti del modello del giocatore) per un attacco di `atk` su `t`, dalla storia dei bersagli del suo lato: se
// colpisce di solito il più danneggiato (o la nostra win condition) e `t` lo è, l'attacco è più probabile; altrimenti
// meno. Almeno 3 osservazioni; 0 senza storia.
s32 AI_VgcFoeTargetBias(enum BattlerId atk, enum BattlerId t)
{
    const struct VgcTargetHistory *h = &sVgcTargetHist[GetBattlerSide(atk)];
    enum BattlerId p;
    s32 bias = 0;

    if (t >= gBattlersCount || IsBattlerAlly(atk, t) || !IsBattlerAlive(t))
        return 0;
    p = GetPartnerBattler(t);
    if (!IsBattlerAlive(p))
        return 0;
    if (h->attacks >= 3)
    {
        u32 pt = gBattleMons[t].hp * 100 / max(gBattleMons[t].maxHP, 1);
        u32 pp = gBattleMons[p].hp * 100 / max(gBattleMons[p].maxHP, 1);
        s32 tend = 2 * 100 * h->onWeakest / h->attacks - 100; // -100..100
        if (pt != pp)
            bias += CUSTOM_VGC_PLAN_FOE_HISTORY_W * tend * (pt < pp ? 1 : -1) / 100;
    }
    if (h->winChoices >= 3)
    {
        bool32 wt = VgcIsWinCon(t), wp = VgcIsWinCon(p);
        s32 tend = 2 * 100 * h->onWincon / h->winChoices - 100;
        if (wt != wp)
            bias += CUSTOM_VGC_PLAN_FOE_HISTORY_W * tend * (wt ? 1 : -1) / 100;
    }
    return bias;
}

static struct VgcBehaviour *VgcFindOrAddBehaviour(enum BattlerId battler)
{
    u32 i, personality;

    if (gBattleMons[battler].species == SPECIES_NONE)
        return NULL;
    // Il lato nella chiave: nei mirror IA contro IA le due squadre hanno le stesse personalità.
    personality = GetMonData(GetBattlerMon(battler), MON_DATA_PERSONALITY) ^ ((u32)GetBattlerSide(battler) << 31);
    for (i = 0; i < sVgcMemCount; i++)
    {
        if (sVgcMem[i].personality == personality)
            return &sVgcMem[i];
    }
    if (sVgcMemCount >= ARRAY_COUNT(sVgcMem))
        return NULL;
    memset(&sVgcMem[sVgcMemCount], 0, sizeof(struct VgcBehaviour));
    sVgcMem[sVgcMemCount].personality = personality;
    return &sVgcMem[sVgcMemCount++];
}

// Bo3: la storia dei bersagli del giocatore e le sue abitudini di Protect passano alla gara dopo (team_preview.c,
// memoria della serie). Solo dati osservati in lotta: attacchi visti, turni sotto KO e Protect eseguiti.
void AI_VgcSeriesExportTargets(struct VgcTargetHistory *out)
{
    *out = sVgcTargetHist[B_SIDE_PLAYER];
}

void AI_VgcSeriesImportTargets(const struct VgcTargetHistory *in)
{
    sVgcTargetHist[B_SIDE_PLAYER] = *in;
}

// Turni sotto KO (4 bit alti) e Protect usati in quei turni (4 bit bassi) di un Pokémon del giocatore. Oltre 15 turni si
// riscala tenendo la proporzione: la storia vecchia pesa al massimo come 15 turni (decadimento tra le gare).
u8 AI_VgcSeriesExportProtect(u32 personality)
{
    u32 i, t, q;

    for (i = 0; i < sVgcMemCount; i++)
    {
        if (sVgcMem[i].personality != personality) // lato del giocatore: chiave = personalità (bit del lato a 0)
            continue;
        t = sVgcMem[i].threatenedTurns;
        q = min(sVgcMem[i].protectsWhenThreatened, t);
        if (t > 15)
        {
            q = (q * 15 + t / 2) / t;
            t = 15;
        }
        return (t << 4) | q;
    }
    return 0;
}

void AI_VgcSeriesImportProtect(u32 personality, u8 packed)
{
    u32 i;

    if (packed == 0)
        return;
    for (i = 0; i < sVgcMemCount; i++)
    {
        if (sVgcMem[i].personality == personality)
            break;
    }
    if (i == sVgcMemCount)
    {
        if (sVgcMemCount >= ARRAY_COUNT(sVgcMem))
            return;
        memset(&sVgcMem[i], 0, sizeof(struct VgcBehaviour));
        sVgcMem[i].personality = personality;
        sVgcMemCount++;
    }
    sVgcMem[i].threatenedTurns = packed >> 4;
    sVgcMem[i].protectsWhenThreatened = min(packed & 0xF, packed >> 4);
}

static bool32 VgcAnyAiBattlerCanKo(enum BattlerId playerBattler)
{
    enum BattlerId b;
    for (b = 0; b < gBattlersCount; b++)
    {
        if (IsOnPlayerSide(b) || !IsBattlerAlive(b))
            continue;
        if (CanAIFaintTarget(b, playerBattler, 1))
            return TRUE;
    }
    return FALSE;
}

static bool32 VgcPlayerCanKoAnyAi(enum BattlerId playerBattler)
{
    enum BattlerId b;
    for (b = 0; b < gBattlersCount; b++)
    {
        if (IsOnPlayerSide(b) || !IsBattlerAlive(b))
            continue;
        if (CanTargetFaintAi(playerBattler, b))
            return TRUE;
    }
    return FALSE;
}

// A inizio turno, dopo che i danni stimati sono pronti: fotografa la situazione del giocatore.
void AI_VgcTurnStart(void)
{
    enum BattlerId b;

    AI_VgcPlanResetTurn();
    if (!IsDoubleBattle() || !(gBattleTypeFlags & BATTLE_TYPE_TRAINER))
        return;
    // IA contro IA: si chiama una volta per lato (prospettiva), e ogni lato aggiorna solo la lettura dei
    // propri avversari. Fuori da IA contro IA foeMask copre tutto il lato del giocatore, come prima.
    {
        u8 foeMask = 0;
        for (b = 0; b < gBattlersCount; b++)
        {
            if (IsOnPlayerSide(b))
                foeMask |= 1u << b;
        }
        AI_VgcBuildTeamGraph(GetBattlerParty(GetBattlerAtPosition(AI_GetPerspectiveSide())), &sVgcTeamGraph);
        sVgcProtectedLastTurn = (sVgcProtectedLastTurn & ~foeMask) | (sVgcProtectedThisTurn & foeMask);
        sVgcProtectedThisTurn &= ~foeMask;
        sVgcThreatened &= ~foeMask;
        sVgcLosing &= ~foeMask;
    }
    sVgcReadRoll[AI_GetPerspectiveSide()] = Random() % 100;
    for (b = 0; b < gBattlersCount; b++)
    {
        struct VgcBehaviour *mem;
        bool32 threatened, losing;

        if (!IsOnPlayerSide(b) || !IsBattlerAlive(b))
            continue;
        threatened = VgcAnyAiBattlerCanKo(b);
        losing = threatened && !VgcPlayerCanKoAnyAi(b) && CountUsablePartyMons(b) > 0;
        if (threatened)
            sVgcThreatened |= 1u << b;
        if (losing)
            sVgcLosing |= 1u << b;
        mem = VgcFindOrAddBehaviour(b);
        if (mem == NULL)
            continue;
        if (threatened && mem->threatenedTurns < 255)
            mem->threatenedTurns++;
        if (losing && mem->losingTurns < 255)
            mem->losingTurns++;
    }
}

// Il giocatore ha usato una mossa (chiamata quando l'azione viene eseguita, non quando la sceglie).
// La deduzione della Stolascelta dall'ordine dei turni è nel belief (battle_ai_scout.c, Scout_BeliefNoteMove), con la
// Velocità massima possibile della specie e non quella vera del giocatore: qui c'era un doppione che leggeva la
// statistica vera.
void AI_VgcNotePlayerMove(enum BattlerId battler, enum Move move)
{
    struct VgcBehaviour *mem;

    if (!IsOnPlayerSide(battler) || !IsDoubleBattle() || !(gBattleTypeFlags & BATTLE_TYPE_TRAINER))
        return;
    if (!IsAiFlagPresent(AI_FLAG_VGC_DOUBLES))
        return;
    // Choice: stessa mossa di fila conferma, mossa diversa smentisce.
    if ((sVgcInferredScarf & (1u << battler)) && sVgcPlayerLastMove[battler] != MOVE_NONE && sVgcPlayerLastMove[battler] != move)
    {
        sVgcInferredScarf &= ~(1u << battler);
        gBattleHistory->itemEffects[battler] = HOLD_EFFECT_NONE;
        gAiPartyData->mons[GetBattlerSide(battler)][gBattlerPartyIndexes[battler]].heldEffect = HOLD_EFFECT_NONE;
    }
    sVgcPlayerLastMove[battler] = move;
    VgcNoteTarget(battler, move);
    if (GetMoveEffect(move) != EFFECT_PROTECT)
        return;
    sVgcProtectedThisTurn |= 1u << battler;
    mem = VgcFindOrAddBehaviour(battler);
    if (mem != NULL && (sVgcThreatened & (1u << battler)) && mem->protectsWhenThreatened < 255)
        mem->protectsWhenThreatened++;
}

// Il giocatore ha cambiato Pokémon.
void AI_VgcNotePlayerSwitch(enum BattlerId battler)
{
    struct VgcBehaviour *mem;

    if (!IsOnPlayerSide(battler) || !IsDoubleBattle() || !(gBattleTypeFlags & BATTLE_TYPE_TRAINER))
        return;
    sVgcPlayerLastMove[battler] = MOVE_NONE;
    sVgcInferredScarf &= ~(1u << battler);
    mem = VgcFindOrAddBehaviour(battler);
    if (mem != NULL && (sVgcLosing & (1u << battler)) && mem->switchesWhenLosing < 255)
        mem->switchesWhenLosing++;
}

// Probabilità stimata (0..100) che questo avversario usi Protect in questo turno.
static u32 VgcProtectChance(enum BattlerId foe)
{
    struct VgcBehaviour *mem;
    s32 p, unknownScale = 100;

    if (!IsBattlerAlive(foe))
        return 0;
    // Protect visto = certo; non visto = in VGC quasi tutti lo portano, finché restano mosse ignote.
    if (!HasMoveWithEffect(foe, EFFECT_PROTECT))
    {
        enum Move *moves = GetMovesArray(foe);
        u32 i, seen = 0;
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            if (moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE)
                seen++;
        }
        if (seen >= MAX_MON_MOVES)
            return 0;
        unknownScale = (MAX_MON_MOVES - seen) * 60 / MAX_MON_MOVES; // 60% a mosse tutte ignote
    }
    p = 20;
    if (sVgcThreatened & (1u << foe))
        p += 30;
    if (sVgcProtectedLastTurn & (1u << foe))
        p -= 35; // due di fila falliscono spesso: i giocatori lo sanno
    if (IsBattlersFirstTurn(foe) && FoeThreatensFakeOut(foe) && HasMoveWithEffect(foe, EFFECT_TRICK_ROOM))
        p += 10; // setter che aspetta che passi il nostro Fake Out
    mem = VgcFindOrAddBehaviour(foe);
    if (mem != NULL && mem->threatenedTurns >= 2)
        p = (p + (s32)(100 * mem->protectsWhenThreatened / mem->threatenedTurns)) / 2;
    p = p * unknownScale / 100;
    if (p < 0) p = 0;
    if (p > 95) p = 95;
    return p;
}

u32 AI_VgcProtectChance(enum BattlerId foe)
{
    return VgcProtectChance(foe);
}

static u32 VgcSwitchChance(enum BattlerId foe);

u32 AI_VgcSwitchChance(enum BattlerId foe)
{
    return VgcSwitchChance(foe);
}

// Probabilità stimata (0..100) che questo avversario cambi Pokémon in questo turno.
static u32 VgcSwitchChance(enum BattlerId foe)
{
    struct VgcBehaviour *mem;
    s32 p;

    if (!IsBattlerAlive(foe) || CountUsablePartyMons(foe) == 0)
        return 0;
    p = 8;
    if (sVgcLosing & (1u << foe))
        p += 30;
    if (gBattleMons[foe].volatiles.escapePrevention || gBattleMons[foe].volatiles.wrapped)
        return 0;
    mem = VgcFindOrAddBehaviour(foe);
    if (mem != NULL && mem->losingTurns >= 2)
        p = (p + (s32)(100 * mem->switchesWhenLosing / mem->losingTurns)) / 2;
    if (p < 0) p = 0;
    if (p > 90) p = 90;
    return p;
}

static s32 VgcMaterialEdge(enum BattlerId battlerAtk);

s32 AI_VgcMaterialEdge(enum BattlerId battler)
{
    return VgcMaterialEdge(battler);
}

// Vantaggio numerico: >0 l'IA ha più Pokémon utilizzabili del giocatore.
static s32 VgcMaterialEdge(enum BattlerId battlerAtk)
{
    enum BattlerId b;
    s32 ai = 0, player = 0;

    for (b = 0; b < gBattlersCount; b++)
    {
        if (!IsBattlerAlive(b))
            continue;
        if (IsOnPlayerSide(b))
            player++;
        else
            ai++;
    }
    ai += CountUsablePartyMons(battlerAtk);
    player += CountUsablePartyMons(GetBattlerLeftFoe(battlerAtk));
    return ai - player;
}

// ---------------------------------------------------------------------
// Regole (M8): filtro delle candidate + spinta leggera
// ---------------------------------------------------------------------
// Le regole non scelgono più la mossa: la sceglie il pianificatore (battle_ai_vgc_plan.c), che simula la coppia di
// azioni per 2-3 turni. Di quello che c'era qui restano due cose:
//  - il FILTRO (AI_VgcFilterAction): azioni illegali o CERTAMENTE inutili (falliscono, o non possono fare niente di
//    buono). Lo stesso filtro toglie le azioni dalle candidate del pianificatore e dall'azione delle regole (il
//    punteggio del motore), così l'azione di riferimento del pianificatore non può essere una mossa scartata;
//  - la SPINTA (AI_VgcActionPrior): un piccolo bonus documentato, in punti della valutazione del pianificatore
//    (un KO vale CUSTOM_VGC_EVAL_KO = 160, un cambio CUSTOM_VGC_SWITCH_COST = 100), solo per gli effetti che la
//    simulazione non modella. Si somma al valore delle coppie (FinishJob) e, diviso per VGC_PRIOR_SCORE_DIV, al
//    punteggio del motore.
// Le preferenze strategiche (Fake Out, Protect, setup sicuro, Trick Room/Tailwind, redirect, Helping Hand, fuoco
// concentrato, letture di Protect e cambio, endgame...) le decide la simulazione: le regole a punteggi magici
// sono state tolte (inventario: docs/ia_vgc/checklist.md, sezione M8). Ogni filtro e ogni spinta ha un test in
// test/battle/ai/ai_vgc_rules.c.

#define VGC_FILTER_SCORE_MALUS  20  // punteggio del motore tolto a un'azione filtrata: l'azione delle regole non la sceglie
#define VGC_PRIOR_SCORE_DIV     10  // spinta nel punteggio del motore: 10 punti della valutazione = 1 punto

// Spinte, in punti della valutazione del pianificatore.
#define VGC_PRIOR_GRAVITY       30  // Gravity: la simulazione non la modella (precisione, mosse Terra sui Volanti)
#define VGC_PRIOR_QUICK_GUARD   30  // Quick Guard: la simulazione non blocca le priorità (Fake Out compreso)
#define VGC_PRIOR_INSTRUCT      40  // Instruct sul compagno che attacca prima di noi: il secondo colpo non è simulato
#define VGC_PRIOR_WEATHER_BENCH 20  // meteo per chi lo sfrutta ed è in panchina (la simulazione vede solo chi è in campo)
#define VGC_PRIOR_CONTRARY      30  // mossa che cala le statistiche di chi la usa, con Contrary: le alza (non simulato)
#define VGC_PRIOR_LOSE_BOOST    20  // (malus) la stessa mossa senza Contrary butta via un potenziamento già preso
#define VGC_PRIOR_GAS_PROTECT   30  // Protect di chi ha Neutralizing Gas accanto a un compagno con abilità penalizzante
#define VGC_PRIOR_FAKE_OUT      25  // Fake Out al primo turno: il tentennamento annulla QUALUNQUE azione del bersaglio, anche
                                    // quelle che la simulazione non prevede (ne guarda due per avversario: setup, Tailwind,
                                    // Trick Room, cambio di bersaglio restano fuori); Wide Guard o un attacco no

// Chi ha usato Protect & co. con successo al turno prima: il secondo di fila fallisce (o quasi).
static bool32 ProtectedLastTurn(enum BattlerId battler)
{
    enum Move last = gLastResultingMoves[battler];

    return last != MOVE_NONE && last != MOVE_UNAVAILABLE
        && gBattleMoveEffects[GetMoveEffect(last)].usesProtectCounter
        && gBattleMons[battler].volatiles.consecutiveMoveUses > 0;
}

// Priorità della mossa per chi la usa (Prankster sulle mosse di stato, Gale Wings a PS pieni), senza effetti collaterali.
static s32 VgcMovePriority(enum BattlerId battler, enum Move move)
{
    s32 prio = GetMovePriority(move);
    enum Ability ability = gAiLogicData->abilities[battler];

    if (ability == ABILITY_PRANKSTER && IsBattleMoveStatus(move))
        prio++;
    else if (ability == ABILITY_GALE_WINGS && GetMoveType(move) == TYPE_FLYING && gBattleMons[battler].hp == gBattleMons[battler].maxHP)
        prio++;
    return prio;
}

static u32 WeatherFlagsOfMove(enum Move move)
{
    switch (GetMoveWeatherType(move))
    {
    case BATTLE_WEATHER_SUN:       return B_WEATHER_SUN;
    case BATTLE_WEATHER_RAIN:      return B_WEATHER_RAIN;
    case BATTLE_WEATHER_SANDSTORM: return B_WEATHER_SANDSTORM;
    case BATTLE_WEATHER_HAIL:      return B_WEATHER_HAIL;
    case BATTLE_WEATHER_SNOW:      return B_WEATHER_SNOW;
    default:                       return 0;
    }
}

static bool32 IgnoresTargetAbility(enum BattlerId battler)
{
    enum Ability ability = gAiLogicData->abilities[battler];
    return ability == ABILITY_MOLD_BREAKER || ability == ABILITY_TERAVOLT || ability == ABILITY_TURBOBLAZE;
}

// Mossa di stato che sul bersaglio fallisce per quello che si sa: tipo, stato già presente, terreno, Safeguard, abilità e
// strumento noti o presunti (gAiLogicData: mai quelli nascosti del giocatore), Prankster sul Buio.
static bool32 StatusMoveBlocked(enum BattlerId battler, enum BattlerId target, enum Move move)
{
    enum BattleMoveEffects effect = GetMoveEffect(move);
    enum Ability tAbility = IgnoresTargetAbility(battler) ? ABILITY_NONE : gAiLogicData->abilities[target];
    enum HoldEffect tHold = gAiLogicData->holdEffects[target];
    bool32 grounded;

    if (IsPowderMove(move) && !IsAffectedByPowderMove(target, tAbility, tHold))
        return TRUE;
    if (gAiLogicData->abilities[battler] == ABILITY_PRANKSTER && IS_BATTLER_OF_TYPE(target, TYPE_DARK))
        return TRUE;
    if (tAbility == ABILITY_GOOD_AS_GOLD || tAbility == ABILITY_MAGIC_BOUNCE)
        return TRUE;
    if (effect == EFFECT_TAUNT && (gBattleMons[target].volatiles.tauntTimer || tAbility == ABILITY_OBLIVIOUS || tAbility == ABILITY_AROMA_VEIL))
        return TRUE;
    if (effect != EFFECT_NON_VOLATILE_STATUS)
        return FALSE;
    if (gBattleMons[target].status1 & STATUS1_ANY)
        return TRUE;
    if ((gSideStatuses[GetBattlerSide(target)] & SIDE_STATUS_SAFEGUARD) && gAiLogicData->abilities[battler] != ABILITY_INFILTRATOR)
        return TRUE;
    grounded = AI_IsBattlerGrounded(target);
    if (grounded && gFieldTimers.terrain == B_TERRAIN_MISTY)
        return TRUE;
    if (tAbility == ABILITY_PURIFYING_SALT || tAbility == ABILITY_COMATOSE)
        return TRUE;
    switch (GetMoveNonVolatileStatus(move))
    {
    case MOVE_EFFECT_SLEEP:
        if (grounded && gFieldTimers.terrain == B_TERRAIN_ELECTRIC)
            return TRUE;
        if (tAbility == ABILITY_INSOMNIA || tAbility == ABILITY_VITAL_SPIRIT || tAbility == ABILITY_SWEET_VEIL)
            return TRUE;
        break;
    case MOVE_EFFECT_PARALYSIS:
        if (IS_BATTLER_OF_TYPE(target, TYPE_ELECTRIC) || tAbility == ABILITY_LIMBER)
            return TRUE;
        if (GetMoveType(move) == TYPE_ELECTRIC && IS_BATTLER_OF_TYPE(target, TYPE_GROUND))
            return TRUE; // Thunder Wave sul Terra
        break;
    case MOVE_EFFECT_BURN:
        if (IS_BATTLER_OF_TYPE(target, TYPE_FIRE)
         || tAbility == ABILITY_WATER_VEIL || tAbility == ABILITY_WATER_BUBBLE || tAbility == ABILITY_THERMAL_EXCHANGE)
            return TRUE;
        break;
    case MOVE_EFFECT_POISON:
    case MOVE_EFFECT_TOXIC:
        if ((IS_BATTLER_OF_TYPE(target, TYPE_POISON) || IS_BATTLER_OF_TYPE(target, TYPE_STEEL))
         && gAiLogicData->abilities[battler] != ABILITY_CORROSION)
            return TRUE;
        if (tAbility == ABILITY_IMMUNITY || tAbility == ABILITY_PASTEL_VEIL)
            return TRUE;
        break;
    default:
        break;
    }
    return FALSE;
}

// Abilità che penalizzano chi le ha: cambiarle o toglierle al compagno è una combo, non un danno.
static bool32 IsHinderingAbility(enum Ability ability)
{
    return ability == ABILITY_MYCELIUM_MIGHT || ability == ABILITY_TRUANT || ability == ABILITY_SLOW_START
        || ability == ABILITY_DEFEATIST || ability == ABILITY_KLUTZ || ability == ABILITY_STALL;
}

// Mossa di stato che sul compagno fa solo danni (stato, Taunt, Encore, Disable, Torment).
static bool32 IsHostileStatusMove(enum Move move)
{
    enum BattleMoveEffects effect = GetMoveEffect(move);
    return effect == EFFECT_NON_VOLATILE_STATUS || effect == EFFECT_TAUNT || effect == EFFECT_ENCORE
        || effect == EFFECT_DISABLE || effect == EFFECT_TORMENT || effect == EFFECT_YAWN;
}

enum VgcFilter AI_VgcFilterAction(enum BattlerId battler, u32 moveIndex, enum BattlerId target)
{
    enum Move move;
    enum BattleMoveEffects effect;
    enum MoveTarget targetType;
    enum BattlerId partner = GetPartnerBattler(battler);
    enum BattleSide side = GetBattlerSide(battler);
    bool32 hasPartner = IsDoubleBattle() && partner != battler && IsBattlerAlive(partner);
    bool32 chosenTarget, onPartner, onFoe;

    if (moveIndex >= MAX_MON_MOVES)
        return VGC_FILTER_ILLEGAL;
    move = gBattleMons[battler].moves[moveIndex];
    if (move == MOVE_NONE || move == MOVE_UNAVAILABLE || gBattleMons[battler].pp[moveIndex] == 0
     || (gAiLogicData->moveLimitations[battler] & (1u << moveIndex)))
        return VGC_FILTER_ILLEGAL;
    effect = GetMoveEffect(move);
    targetType = AI_GetBattlerMoveTargetType(battler, move);
    chosenTarget = targetType == TARGET_SELECTED || targetType == TARGET_SMART || targetType == TARGET_OPPONENT
                || targetType == TARGET_USER_OR_ALLY || targetType == TARGET_ALLY;
    onPartner = chosenTarget && hasPartner && (target == partner || targetType == TARGET_ALLY);
    onFoe = chosenTarget && !onPartner && target < gBattlersCount && !IsBattlerAlly(battler, target) && IsBattlerAlive(target);

    // Fake Out & co. fuori dal primo turno in campo falliscono sempre.
    if (effect == EFFECT_FIRST_TURN_ONLY && !IsBattlersFirstTurn(battler))
        return VGC_FILTER_FIRST_TURN_ONLY;
    // Protect & co. subito dopo un Protect riuscito.
    if (gBattleMoveEffects[effect].usesProtectCounter && ProtectedLastTurn(battler))
        return VGC_FILTER_PROTECT_REPEAT;
    // Campo Psichico: una mossa con priorità su un avversario a terra fallisce.
    if (onFoe && gFieldTimers.terrain == B_TERRAIN_PSYCHIC && AI_IsBattlerGrounded(target) && VgcMovePriority(battler, move) > 0)
        return VGC_FILTER_PSYCHIC_TERRAIN;

    if (!IsBattleMoveStatus(move))
    {
        // Immunità nota (tipo, o abilità che l'IA conosce): sul bersaglio scelto, o su tutti gli avversari per le mosse ad area.
        if (onFoe && gAiLogicData->effectiveness[battler][target][moveIndex] == UQ_4_12(0.0))
            return VGC_FILTER_IMMUNE;
        if (IsSpreadMove(targetType))
        {
            enum BattlerId foes[2] = { GetBattlerLeftFoe(battler), GetBattlerRightFoe(battler) };
            u32 k, alive = 0, immune = 0;
            for (k = 0; k < 2; k++)
            {
                if (!IsBattlerAlive(foes[k]))
                    continue;
                alive++;
                if (gAiLogicData->effectiveness[battler][foes[k]][moveIndex] == UQ_4_12(0.0))
                    immune++;
            }
            if (alive > 0 && immune == alive)
                return VGC_FILTER_IMMUNE;
        }
        // Attacco sul compagno: solo se il grafo della squadra ci vede una combo (Beat Up -> Justified...) o se lo cura.
        // La mossa deve attivare davvero l'abilità/strumento del compagno: Fake Out accanto a un Flash Fire non è una combo.
        if (onPartner && effect != EFFECT_HIT_ENEMY_HEAL_ALLY
         && (sVgcTeamGraph.hitPartner[gBattlerPartyIndexes[battler]] != gBattlerPartyIndexes[partner]
          || !AI_VgcIsPartnerHitCombo(move, battler, partner)))
            return VGC_FILTER_WRONG_TARGET;
        return VGC_FILTER_NONE;
    }

    // Mosse di stato.
    if (onFoe && StatusMoveBlocked(battler, target, move))
        return VGC_FILTER_STATUS_BLOCKED;
    if (onPartner)
    {
        if (IsHostileStatusMove(move))
            return VGC_FILTER_WRONG_TARGET;
        // Cambiare l'abilità del compagno serve solo se lo penalizza (Mycelium Might, Truant, Slow Start...).
        if ((effect == EFFECT_OVERWRITE_ABILITY || effect == EFFECT_SKILL_SWAP || effect == EFFECT_GASTRO_ACID || effect == EFFECT_ENTRAINMENT)
         && !IsHinderingAbility(gAiLogicData->abilities[partner]) && !(VgcRoles(partner) & VGC_ROLE_HINDER_ABILITY))
            return VGC_FILTER_WRONG_TARGET;
    }
    // Instruct e After You su un avversario: gli si regala un'azione.
    if ((effect == EFFECT_INSTRUCT || effect == EFFECT_AFTER_YOU) && onFoe)
        return VGC_FILTER_WRONG_TARGET;
    // Mosse per il compagno senza compagno.
    if (!hasPartner && (effect == EFFECT_HELPING_HAND || effect == EFFECT_FOLLOW_ME || effect == EFFECT_INSTRUCT || effect == EFFECT_AFTER_YOU))
        return VGC_FILTER_NO_EFFECT;

    switch (effect)
    {
    case EFFECT_TAILWIND:
        if (gSideStatuses[side] & SIDE_STATUS_TAILWIND)
            return VGC_FILTER_FIELD_ACTIVE;
        break;
    case EFFECT_TRICK_ROOM:
        // Chiuderebbe il NOSTRO Trick Room (squadra lenta: stessa soglia del controllore dei misplay).
        if ((gFieldStatuses & STATUS_FIELD_TRICK_ROOM) && gFieldTimers.trickRoomTimer > 1 && AI_VgcTeamIsSlowerThanFoes(battler, 2))
            return VGC_FILTER_FIELD_ACTIVE;
        break;
    case EFFECT_GRAVITY:
        if (gFieldStatuses & STATUS_FIELD_GRAVITY)
            return VGC_FILTER_FIELD_ACTIVE;
        break;
    case EFFECT_WEATHER:
        if ((gBattleWeather & WeatherFlagsOfMove(move)) || (gBattleWeather & B_WEATHER_PRIMAL_ANY))
            return VGC_FILTER_FIELD_ACTIVE;
        break;
    case EFFECT_TERRAIN:
        if (gFieldTimers.terrain != B_TERRAIN_NONE && gFieldTimers.terrain == GetMoveTerrainType(move))
            return VGC_FILTER_FIELD_ACTIVE;
        break;
    case EFFECT_REFLECT:
        if (gSideStatuses[side] & SIDE_STATUS_REFLECT)
            return VGC_FILTER_FIELD_ACTIVE;
        break;
    case EFFECT_LIGHT_SCREEN:
        if (gSideStatuses[side] & SIDE_STATUS_LIGHTSCREEN)
            return VGC_FILTER_FIELD_ACTIVE;
        break;
    case EFFECT_AURORA_VEIL:
        if (gSideStatuses[side] & SIDE_STATUS_AURORA_VEIL)
            return VGC_FILTER_FIELD_ACTIVE;
        if (!(gBattleWeather & B_WEATHER_ICY_ANY))
            return VGC_FILTER_NO_EFFECT;
        break;
    case EFFECT_RECYCLE:
        // Niente da riciclare (nessuno strumento consumato, o lo strumento è ancora in mano).
        if (GetBattlerPartyState(battler)->usedHeldItem == ITEM_NONE || gBattleMons[battler].item != ITEM_NONE)
            return VGC_FILTER_NO_EFFECT;
        break;
    case EFFECT_BATON_PASS:
        if (CountUsablePartyMons(battler) == 0)
            return VGC_FILTER_NO_EFFECT;
        break;
    case EFFECT_SUBSTITUTE:
        if (gBattleMons[battler].volatiles.substitute || gBattleMons[battler].hp <= gBattleMons[battler].maxHP / 4)
            return VGC_FILTER_NO_EFFECT;
        break;
    case EFFECT_BELLY_DRUM:
        if (gBattleMons[battler].hp <= gBattleMons[battler].maxHP / 2 || gBattleMons[battler].statStages[STAT_ATK] >= MAX_STAT_STAGE)
            return VGC_FILTER_NO_EFFECT;
        break;
    default:
        break;
    }
    return VGC_FILTER_NONE;
}

// Statistiche offensive che la mossa cala a chi la usa (Superpower, Close Combat, Draco Meteor...): bit 0 Atk, bit 1 AtkSp.
static u32 SelfLoweredOffense(enum Move move)
{
    u32 i, n = GetMoveAdditionalEffectCount(move), mask = 0;

    for (i = 0; i < n; i++)
    {
        const struct AdditionalEffect *ae = GetMoveAdditionalEffectById(move, i);
        if (!ae->self || ae->moveEffect != MOVE_EFFECT_STAT_MINUS)
            continue;
        if (ae->attack)
            mask |= 1;
        if (ae->spAtk)
            mask |= 2;
        if (ae->defense || ae->spDef || ae->speed)
            mask |= 4;
    }
    return mask;
}

// Un Pokémon dell'IA in panchina (non in campo) sfrutta il meteo di questa mossa.
static bool32 BenchHasWeatherAbuser(enum BattlerId battler, enum Move move)
{
    struct Pokemon *party = GetBattlerParty(battler);
    enum BattlerId partner = GetPartnerBattler(battler);
    u32 i;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (i == gBattlerPartyIndexes[battler] || (IsBattlerAlive(partner) && i == gBattlerPartyIndexes[partner]))
            continue;
        if (GetMonData(&party[i], MON_DATA_SPECIES) == SPECIES_NONE || GetMonData(&party[i], MON_DATA_HP) == 0)
            continue;
        if (MonAbusesWeather(GetMonAbility(&party[i]), move))
            return TRUE;
    }
    return FALSE;
}

// L'attacco che il compagno userebbe più volentieri con Instruct, quando non si sa ancora cosa farà: una mossa
// che rende di più a PS pieni (Eruption & co.), altrimenti la più potente.
static enum Move GuessPartnerAttack(enum BattlerId partner)
{
    enum Move best = MOVE_UNAVAILABLE;
    u32 i, bestPower = 0;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        enum Move m = gBattleMons[partner].moves[i];
        u32 power;
        if (m == MOVE_NONE || m == MOVE_UNAVAILABLE || IsBattleMoveStatus(m) || gBattleMons[partner].pp[i] == 0)
            continue;
        power = GetMovePower(m) + (GetMoveEffect(m) == EFFECT_POWER_BASED_ON_USER_HP ? 1000 : 0);
        if (power > bestPower)
        {
            bestPower = power;
            best = m;
        }
    }
    return best;
}

s32 AI_VgcActionPrior(enum BattlerId battler, enum Move move, enum BattlerId target, enum Move partnerMove)
{
    enum BattleMoveEffects effect;
    enum BattlerId partner = GetPartnerBattler(battler);
    bool32 hasPartner = IsDoubleBattle() && partner != battler && IsBattlerAlive(partner);
    s32 prior = 0;

    if (move == MOVE_NONE || move == MOVE_UNAVAILABLE)
        return 0;
    effect = GetMoveEffect(move);
    // partnerMove: MOVE_NONE = non si sa (si stima), MOVE_UNAVAILABLE = il compagno non attacca.
    if (partnerMove == MOVE_NONE && hasPartner && effect == EFFECT_INSTRUCT)
        partnerMove = GuessPartnerAttack(partner);
    if (partnerMove == MOVE_UNAVAILABLE)
        partnerMove = MOVE_NONE;

    // Sonno e paralisi valgono quanto la minaccia del bersaglio (prova in gioco: Spore su Dugtrio accanto a un Tyranitar
    // Mega). Solo dati leciti: KO stimati dall'IA, forma in campo, stadi visibili.
    if (target != battler && target != partner && IsBattlerAlive(target) && gBattleMons[target].status1 == STATUS1_NONE
     && (GetMoveNonVolatileStatus(move) == MOVE_EFFECT_SLEEP || GetMoveNonVolatileStatus(move) == MOVE_EFFECT_PARALYSIS))
    {
        s32 threat = 0;
        if (CanTargetFaintAi(target, battler))
            threat += 2;
        if (hasPartner && CanTargetFaintAi(target, partner))
            threat += 2;
        if (IsBattlerMegaEvolved(target))
            threat += 1;
        if (gBattleMons[target].statStages[STAT_ATK] > DEFAULT_STAT_STAGE
         || gBattleMons[target].statStages[STAT_SPATK] > DEFAULT_STAT_STAGE)
            threat += 1;
        prior += min(threat, 4) * VGC_PRIOR_SCORE_DIV;
    }
    switch (effect)
    {
    case EFFECT_GRAVITY:
        // Gravity fa centrare le mosse imprecise (Hypnosis, Sleep Powder...) e porta a terra Volanti e Levitate.
        if (!(gFieldStatuses & STATUS_FIELD_GRAVITY))
        {
            enum BattlerId users[2] = { battler, partner };
            u32 u, i;
            bool32 useful = FALSE;
            for (u = 0; u < 2 && !useful; u++)
            {
                if (u == 1 && !hasPartner)
                    break;
                for (i = 0; i < MAX_MON_MOVES; i++)
                {
                    enum Move m = gBattleMons[users[u]].moves[i];
                    u32 acc;
                    if (m == MOVE_NONE || m == MOVE_UNAVAILABLE || GetMoveEffect(m) == EFFECT_GRAVITY)
                        continue;
                    acc = GetMoveAccuracy(m);
                    if ((acc > 0 && acc <= 80) || (GetMoveType(m) == TYPE_GROUND && !IsBattleMoveStatus(m)
                     && ((IsBattlerAlive(GetBattlerLeftFoe(battler)) && !AI_IsBattlerGrounded(GetBattlerLeftFoe(battler)))
                      || (IsBattlerAlive(GetBattlerRightFoe(battler)) && !AI_IsBattlerGrounded(GetBattlerRightFoe(battler))))))
                    {
                        useful = TRUE;
                        break;
                    }
                }
            }
            if (useful)
                prior += VGC_PRIOR_GRAVITY;
        }
        break;
    case EFFECT_PROTECT:
        // Quick Guard contro Fake Out (avversari al primo turno) o priorità viste.
        if (GetMoveProtectMethod(move) == PROTECT_QUICK_GUARD
         && (FoeThreatensFakeOut(battler) || FoeHasKnownPriorityMove(GetBattlerLeftFoe(battler)) || FoeHasKnownPriorityMove(GetBattlerRightFoe(battler))))
            prior += VGC_PRIOR_QUICK_GUARD;
        // Chi tiene spenta l'abilità penalizzante del compagno (Neutralizing Gas accanto a Truant...) va tenuto vivo.
        if (GetMoveProtectMethod(move) == PROTECT_NORMAL && hasPartner && gAiLogicData->abilities[battler] == ABILITY_NEUTRALIZING_GAS
         && (VgcRoles(partner) & VGC_ROLE_HINDER_ABILITY)
         && ((IsBattlerAlive(GetBattlerLeftFoe(battler)) && CanTargetFaintAi(GetBattlerLeftFoe(battler), battler))
          || (IsBattlerAlive(GetBattlerRightFoe(battler)) && CanTargetFaintAi(GetBattlerRightFoe(battler), battler))))
            prior += VGC_PRIOR_GAS_PROTECT;
        break;
    case EFFECT_INSTRUCT:
        // Il compagno ripete il suo attacco: serve che attacchi, che si muova prima di noi e che l'attacco valga la pena
        // (ad area o potente). Mai le mosse da primo turno: Fake Out ripetuto a fine turno non fa tentennare nessuno.
        if (hasPartner && target == partner && partnerMove != MOVE_NONE && !IsBattleMoveStatus(partnerMove)
         && GetMoveEffect(partnerMove) != EFFECT_FIRST_TURN_ONLY
         && (IsSpreadMove(GetMoveTarget(partnerMove)) || GetMovePower(partnerMove) >= 80)
         && !IsMoveInstructBanned(partnerMove) && AI_IsFaster(partner, battler, partnerMove, move, CONSIDER_PRIORITY))
            prior += VGC_PRIOR_INSTRUCT;
        break;
    // After You: niente spinta, l'ordine del turno dopo After You è simulato (battle_ai_vgc_plan.c, RunTurn).
    case EFFECT_FIRST_TURN_ONLY:
        // Solo sull'avversario che può tentennare (Inner Focus e Shield Dust noti lo impediscono; i Spettro li toglie il filtro).
        if (IsBattlersFirstTurn(battler) && target != battler && target != partner && target < gBattlersCount && IsBattlerAlive(target)
         && gAiLogicData->abilities[target] != ABILITY_INNER_FOCUS && gAiLogicData->abilities[target] != ABILITY_SHIELD_DUST)
            prior += VGC_PRIOR_FAKE_OUT;
        break;
    case EFFECT_WEATHER:
        if (!(gBattleWeather & WeatherFlagsOfMove(move)) && BenchHasWeatherAbuser(battler, move))
            prior += VGC_PRIOR_WEATHER_BENCH;
        break;
    default:
        break;
    }

    // Mosse che calano le statistiche di chi le usa (Superpower...): con Contrary sono un potenziamento.
    if (!IsBattleMoveStatus(move))
    {
        u32 lowered = SelfLoweredOffense(move);
        if (lowered && gAiLogicData->abilities[battler] == ABILITY_CONTRARY)
            prior += VGC_PRIOR_CONTRARY;
        else if (((lowered & 1) && gBattleMons[battler].statStages[STAT_ATK] > DEFAULT_STAT_STAGE)
              || ((lowered & 2) && gBattleMons[battler].statStages[STAT_SPATK] > DEFAULT_STAT_STAGE))
            prior -= VGC_PRIOR_LOSE_BOOST;
    }
    return prior;
}

// ---------------------------------------------------------------------
// Valutazione del motore (AI_FLAG_VGC_DOUBLES)
// ---------------------------------------------------------------------
#if TESTING
static s32 AI_VgcDoublesLegacy(enum BattlerId battlerAtk, enum BattlerId battlerDef, enum Move move, s32 score);

// Self-play (verifica-regole): il lato con il bit in gVgcTraining.rulesLegacy usa le regole di prima di M8.
bool32 AI_VgcRulesLegacy(enum BattlerId battler)
{
    // Verifica a specchio delle correzioni dei misplay: il bit spegne quelle, le regole restano M8 per entrambi.
    if (CUSTOM_VGC_MIRROR_FIXES || CUSTOM_VGC_MIRROR_FOE_IDLE || CUSTOM_VGC_MIRROR_FOE_HISTORY || CUSTOM_VGC_MIRROR_KO
     || CUSTOM_VGC_MIRROR_SWITCH_WORST || CUSTOM_VGC_MIRROR_ROUND5 || CUSTOM_VGC_MIRROR_TUNE || CUSTOM_VGC_MIRROR_WINCON_GUARD
     || CUSTOM_VGC_MIRROR_MIXED)
        return FALSE;
    return (gVgcTraining.rulesLegacy >> GetBattlerSide(battler)) & 1;
}
#endif

// Il punteggio del motore decide l'azione delle regole: il termine di paragone del pianificatore (a pari valore resta
// lei) e la scelta quando il pianificatore non gira. Qui solo il filtro e la spinta, gli stessi del pianificatore.
s32 AI_VgcDoubles(enum BattlerId battlerAtk, enum BattlerId battlerDef, enum Move move, s32 score)
{
    u32 moveIndex = gAiThinkingStruct->movesetIndex;

    if (!IsDoubleBattle())
        return score;
#if TESTING
    if (AI_VgcRulesLegacy(battlerAtk))
        return AI_VgcDoublesLegacy(battlerAtk, battlerDef, move, score);
#endif
    if (AI_VgcFilterAction(battlerAtk, moveIndex, battlerDef) != VGC_FILTER_NONE)
    {
        ADJUST_SCORE(-VGC_FILTER_SCORE_MALUS);
        return score;
    }
    ADJUST_SCORE(AI_VgcActionPrior(battlerAtk, move, battlerDef, MOVE_NONE) / VGC_PRIOR_SCORE_DIV);
    return score;
}

// ---------------------------------------------------------------------
// Megaevoluzione: chi la fa in questa gara
// ---------------------------------------------------------------------
// Una sola Mega per lotta. Non è fissata dal piano: la team preview sceglie ogni gara chi la fa
// (TeamPreview_GetMegaDesignateSpecies) guardando la squadra del giocatore, la lead e il bisogno di
// controllo velocità (es. Enzo: Mega Aerodactyl se serve il Tailwind, Mega Gengar se conviene il
// blocco col sonno). In lotta si resta flessibili: la Mega si "presta" a un altro Pokémon se quello
// designato è già caduto, oppure se il designato è in panchina e chi è in campo rischia il KO
// (meglio megaevolvere ora che perdere il Pokémon senza averla usata).
bool32 AI_VgcShouldHoldMega(enum BattlerId battler)
{
    u16 designate = AI_GetMegaDesignateForBattler(battler); // (IA contro IA: quella del lato di `battler`)
    enum BattlerId partner = GetPartnerBattler(battler);
    enum BattlerId foeL = GetBattlerLeftFoe(battler), foeR = GetBattlerRightFoe(battler);
    struct Pokemon *party;
    bool32 designateAlive = FALSE, designateOnField = FALSE;
    u32 i;

    if (!(gAiThinkingStruct->aiFlags[battler] & AI_FLAG_VGC_DOUBLES) || designate == SPECIES_NONE)
        return FALSE;
    if (AI_VgcSameSpecies(designate, gBattleMons[battler].species))
        return FALSE;
    party = GetBattlerParty(battler);
    for (i = 0; i < PARTY_SIZE; i++)
    {
        u16 species = GetMonData(&party[i], MON_DATA_SPECIES);
        if (species == SPECIES_NONE || GetMonData(&party[i], MON_DATA_HP) == 0 || i == gBattlerPartyIndexes[battler])
            continue;
        if (!AI_VgcSameSpecies(designate, species)
         || GetItemHoldEffect(GetMonData(&party[i], MON_DATA_HELD_ITEM)) != HOLD_EFFECT_MEGA_STONE)
            continue;
        designateAlive = TRUE;
        if (IsBattlerAlive(partner) && gBattlerPartyIndexes[partner] == i)
            designateOnField = TRUE;
    }
    if (!designateAlive)
        return FALSE; // il designato è caduto: la Mega è libera
    if (designateOnField)
        return TRUE;  // il designato è in campo accanto: la Mega è sua
    // Designato in panchina: si tiene la Mega, a meno che questo Pokémon stia per andare KO.
    if ((IsBattlerAlive(foeL) && CanTargetFaintAi(foeL, battler)) || (IsBattlerAlive(foeR) && CanTargetFaintAi(foeR, battler)))
        return FALSE;
    return TRUE;
}

#if TESTING
#include "battle_ai_vgc_legacy.h"
#endif
