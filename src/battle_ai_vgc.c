// IA per le lotte in doppio stile VGC (AI_FLAG_VGC_DOUBLES).
//
// Si aggiunge alle valutazioni standard (Check Bad Move, Try To Faint, Check Viability,
// Double Battle...): non le sostituisce, le corregge nei punti in cui in VGC si ragiona
// diversamente. Tutto parte da ciò che l'IA sa degli avversari (con Omniscient sa tutto,
// altrimenti solo le mosse/abilità/strumenti già visti).
//
// Blocchi:
//   1. Apertura        Fake Out sul setter / sulla minaccia, Trick Room e Tailwind quando
//                      la SQUADRA (campo + panchina) ne beneficia davvero.
//   2. Setup sicuro    Danza di Spada, Ferrodifesa + Schiacciacorpo, Nasty Plot, Bellyrumble...
//                      solo quando nessuno ci manda KO questo turno (o il partner reindirizza).
//   3. Protect "safe"  Protect quando un avversario ci manda KO e il partner lo toglie di mezzo,
//                      quando rischiamo il doppio bersaglio, per far scorrere il Trick Room
//                      avversario; mai per sprecare i turni del NOSTRO Tailwind / Trick Room.
//   4. Fuoco concentrato  Se la mossa del partner + la nostra mandano KO lo stesso bersaglio
//                      è un ottimo turno; se il partner lo manda KO da solo e prima, non si spreca
//                      il colpo (a meno che il bersaglio conosca Protect).
//   5. Supporto        Segui Me / Polverabbia quando il partner fa setup o è la minaccia;
//                      Aiuto quando trasforma un 2HKO in OHKO.
//   6. Meteo           Giornodisole / Pioggia ecc. valgono di più se in panchina c'è chi le sfrutta.
#include "global.h"
#include "battle.h"
#include "battle_ai_main.h"
#include "battle_ai_util.h"
#include "battle_ai_vgc.h"
#include "team_preview.h"
#include "battle_ai_scout.h"
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

// Quanto il piano vuole questa mossa (controllo velocità / meteo) adesso.
static s32 PlanMoveBonus(const struct VgcGameplan *plan, enum BattlerId battlerAtk, enum Move move)
{
    enum BattleMoveEffects effect = GetMoveEffect(move);
    u8 want = VGC_PLAN_BALANCE;

    if (plan == NULL)
        return 0;
    switch (effect)
    {
    case EFFECT_TRICK_ROOM: want = VGC_PLAN_TRICK_ROOM; break;
    case EFFECT_TAILWIND:   want = VGC_PLAN_TAILWIND; break;
    case EFFECT_GRAVITY:    want = VGC_PLAN_GRAVITY; break;
    case EFFECT_WEATHER:
        switch (GetMoveWeatherType(move))
        {
        case BATTLE_WEATHER_SUN:       want = VGC_PLAN_SUN; break;
        case BATTLE_WEATHER_RAIN:      want = VGC_PLAN_RAIN; break;
        case BATTLE_WEATHER_SANDSTORM: want = VGC_PLAN_SAND; break;
        default: break;
        }
        break;
    default:
        break;
    }
    if (want == VGC_PLAN_BALANCE)
        return 0;
    if (want == plan->modeA)
        return 3;
    if (want == plan->modeB)
        return 2;
    return 0;
}

// La modalità del piano è attiva in campo adesso (per decidere i cambi verso la win condition).
bool32 AI_VgcPlanModeActive(enum BattlerId battler)
{
    const struct VgcGameplan *plan = AI_VgcGetPlan(TRAINER_BATTLE_PARAM.opponentA);
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

// Mossa ad area dannosa nota di un avversario (per Wide Guard).
static bool32 FoeHasKnownSpreadMove(enum BattlerId foe)
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
        if (IsSpreadMove(AI_GetBattlerMoveTargetType(foe, moves[i])))
            return TRUE;
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

static bool32 TeamHasWeatherAbuser(enum BattlerId battler, enum Move move)
{
    struct Pokemon *party = GetBattlerParty(battler);
    u32 i;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        enum Ability ability;
        if (GetMonData(&party[i], MON_DATA_SPECIES) == SPECIES_NONE || GetMonData(&party[i], MON_DATA_HP) == 0)
            continue;
        ability = GetMonAbility(&party[i]);
        switch (GetMoveWeatherType(move))
        {
        case BATTLE_WEATHER_SUN:
            if (ability == ABILITY_CHLOROPHYLL || ability == ABILITY_SOLAR_POWER || ability == ABILITY_PROTOSYNTHESIS
             || ability == ABILITY_LEAF_GUARD || ability == ABILITY_ORICHALCUM_PULSE)
                return TRUE;
            break;
        case BATTLE_WEATHER_RAIN:
            if (ability == ABILITY_SWIFT_SWIM || ability == ABILITY_RAIN_DISH || ability == ABILITY_DRY_SKIN
             || ability == ABILITY_HYDRATION)
                return TRUE;
            break;
        case BATTLE_WEATHER_SANDSTORM:
            if (ability == ABILITY_SAND_RUSH || ability == ABILITY_SAND_FORCE || ability == ABILITY_SAND_VEIL)
                return TRUE;
            break;
        case BATTLE_WEATHER_HAIL:
        case BATTLE_WEATHER_SNOW:
            if (ability == ABILITY_SLUSH_RUSH || ability == ABILITY_ICE_BODY || ability == ABILITY_SNOW_CLOAK)
                return TRUE;
            break;
        default:
            break;
        }
    }
    return FALSE;
}

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

// Il Trick Room attivo è dell'avversario (noi siamo il lato veloce).
static bool32 FoeTrickRoomActive(enum BattlerId battler)
{
    return (gFieldStatuses & STATUS_FIELD_TRICK_ROOM) && gFieldTimers.trickRoomTimer > 1
        && !AI_VgcTeamIsSlowerThanFoes(battler, 1);
}

// Mossa "di setup" nel senso VGC: alza statistiche, controllo velocità, meteo, schermi, redirect.
static bool32 IsVgcSetupMove(enum Move move)
{
    switch (GetMoveEffect(move))
    {
    case EFFECT_TRICK_ROOM:
    case EFFECT_TAILWIND:
    case EFFECT_WEATHER:
    case EFFECT_TERRAIN:
    case EFFECT_LIGHT_SCREEN:
    case EFFECT_REFLECT:
    case EFFECT_AURORA_VEIL:
    case EFFECT_BELLY_DRUM:
    case EFFECT_FOLLOW_ME:
        return TRUE;
    default:
        return IsStatRaisingMove(move) && IsBattleMoveStatus(move);
    }
}

// Solo mosse NOTE: per i Pokémon del giocatore GetMovesArray restituisce le mosse viste
// (niente sbirciate al set), per i propri quelle vere.
static bool32 BattlerHasVgcSetupMove(enum BattlerId battler)
{
    u32 i;
    enum Move *moves = GetMovesArray(battler);
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        enum Move m = moves[i];
        if (m != MOVE_NONE && m != MOVE_UNAVAILABLE && IsVgcSetupMove(m))
            return TRUE;
    }
    // Giocatore: mosse non ancora viste, ma la specie fa pensare a un setter (scouting).
    if (IsOnPlayerSide(battler)
     && (Scout_BattlerRoleChance(battler, SCOUT_TRICK_ROOM) >= 50 || Scout_BattlerRoleChance(battler, SCOUT_TAILWIND) >= 50
      || Scout_BattlerRoleChance(battler, SCOUT_REDIRECT) >= 60))
        return TRUE;
    return FALSE;
}

// Mossa già scelta dal partner (se ha già deciso in questo turno), MOVE_NONE altrimenti.
static enum Move PartnerChosenMove(enum BattlerId battler, u32 *moveIndex, enum BattlerId *target)
{
    enum BattlerId partner = GetPartnerBattler(battler);

    *moveIndex = 0;
    *target = partner;
    if (partner == battler || !IsBattlerAlive(partner))
        return MOVE_NONE;
    if (!(gAiLogicData->battlerMovesScored & (1u << partner)))
        return MOVE_NONE;
    if (gAiLogicData->shouldSwitch & (1u << partner))
        return MOVE_NONE;
    *moveIndex = gAiBattleData->chosenMoveIndex[partner];
    *target = gAiBattleData->chosenTarget[partner];
    if (*moveIndex >= MAX_MON_MOVES)
        return MOVE_NONE;
    if (gBattleMons[partner].moves[*moveIndex] == MOVE_UNAVAILABLE)
        return MOVE_NONE;
    return gBattleMons[partner].moves[*moveIndex];
}

// Danno mediano che il partner farebbe a `target` con la mossa scelta (0 se non lo colpisce).
static u32 PartnerDamageTo(enum BattlerId battler, enum BattlerId target)
{
    enum BattlerId partner = GetPartnerBattler(battler), chosenTarget;
    u32 idx;
    enum Move pMove = PartnerChosenMove(battler, &idx, &chosenTarget);

    if (pMove == MOVE_NONE || IsBattleMoveStatus(pMove))
        return 0;
    if (chosenTarget != target && !IsSpreadMove(AI_GetBattlerMoveTargetType(partner, pMove)))
        return 0;
    return gAiLogicData->simulatedDmg[partner][target][idx].median;
}

// Il partner (che ha già deciso) manda KO `target` da solo e prima che `target` si muova.
static bool32 PartnerKOsFirst(enum BattlerId battler, enum BattlerId target)
{
    enum BattlerId partner = GetPartnerBattler(battler), chosenTarget;
    u32 idx;
    enum Move pMove = PartnerChosenMove(battler, &idx, &chosenTarget);

    if (pMove == MOVE_NONE || IsBattleMoveStatus(pMove))
        return FALSE;
    if (PartnerDamageTo(battler, target) < gBattleMons[target].hp)
        return FALSE;
    return AI_IsFaster(partner, target, pMove, MOVE_NONE, CONSIDER_PRIORITY);
}

static bool32 PartnerIsRedirecting(enum BattlerId battler)
{
    enum BattlerId t;
    u32 idx;
    enum Move pMove = PartnerChosenMove(battler, &idx, &t);
    return pMove != MOVE_NONE && GetMoveEffect(pMove) == EFFECT_FOLLOW_ME;
}

// Il reindirizzamento del partner protegge davvero da questo avversario? Rage Powder è una mossa
// "polvere": non devia chi è di tipo Erba, ha Copripolvere o Visierantipolvere (se noti);
// Stalwart / Propeller Tail ignorano qualunque reindirizzamento.
static bool32 PartnerRedirectCoversFoe(enum BattlerId battler, enum BattlerId foe)
{
    enum BattlerId t;
    u32 idx;
    enum Move pMove = PartnerChosenMove(battler, &idx, &t);
    enum Ability foeAbility = gAiLogicData->abilities[foe];

    if (pMove == MOVE_NONE || GetMoveEffect(pMove) != EFFECT_FOLLOW_ME)
        return FALSE;
    if (foeAbility == ABILITY_STALWART || foeAbility == ABILITY_PROPELLER_TAIL)
        return FALSE;
    if (IsPowderMove(pMove) && !IsAffectedByPowderMove(foe, foeAbility, gAiLogicData->holdEffects[foe]))
        return FALSE;
    return TRUE;
}

// Turno sicuro: nessuno ci manda KO, oppure il partner reindirizza tutti quelli che potrebbero.
static bool32 IsSafeTurn(enum BattlerId battler, enum BattlerId foeL, bool32 threatL, enum BattlerId foeR, bool32 threatR)
{
    if (!threatL && !threatR)
        return TRUE;
    if (threatL && !PartnerRedirectCoversFoe(battler, foeL))
        return FALSE;
    if (threatR && !PartnerRedirectCoversFoe(battler, foeR))
        return FALSE;
    return TRUE;
}

static bool32 PartnerIsSettingUp(enum BattlerId battler)
{
    enum BattlerId t;
    u32 idx;
    enum Move pMove = PartnerChosenMove(battler, &idx, &t);
    return pMove != MOVE_NONE && IsVgcSetupMove(pMove);
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

static EWRAM_DATA struct VgcBehaviour sVgcMem[PARTY_SIZE] = {0};
static EWRAM_DATA u8 sVgcMemCount = 0;
static EWRAM_DATA u8 sVgcThreatened = 0;        // bitmask battler del giocatore sotto KO questo turno
static EWRAM_DATA u8 sVgcLosing = 0;            // bitmask battler del giocatore che perdono il matchup
static EWRAM_DATA u8 sVgcProtectedLastTurn = 0; // bitmask battler che hanno usato Protect al turno scorso
static EWRAM_DATA u8 sVgcProtectedThisTurn = 0;
static EWRAM_DATA u8 sVgcReadRoll = 0;          // 0..99, tirato una volta a turno: quanto fidarsi della lettura
static EWRAM_DATA enum Move sVgcPlayerLastMove[MAX_BATTLERS_COUNT] = {0}; // ultima mossa di ogni Pokémon del giocatore (blocco Choice)
static EWRAM_DATA u8 sVgcInferredScarf = 0;             // bitmask: Stolascelta dedotta dalla velocità

void AI_VgcResetMemory(void)
{
    memset(sVgcMem, 0, sizeof(sVgcMem));
    memset(sVgcPlayerLastMove, 0, sizeof(sVgcPlayerLastMove));
    sVgcInferredScarf = 0;
    sVgcMemCount = 0;
    sVgcThreatened = sVgcLosing = sVgcProtectedLastTurn = sVgcProtectedThisTurn = 0;
}

static struct VgcBehaviour *VgcFindOrAddBehaviour(enum BattlerId battler)
{
    u32 i, personality;

    if (gBattleMons[battler].species == SPECIES_NONE)
        return NULL;
    personality = GetMonData(GetBattlerMon(battler), MON_DATA_PERSONALITY);
    for (i = 0; i < sVgcMemCount; i++)
    {
        if (sVgcMem[i].personality == personality)
            return &sVgcMem[i];
    }
    if (sVgcMemCount >= PARTY_SIZE)
        return NULL;
    memset(&sVgcMem[sVgcMemCount], 0, sizeof(struct VgcBehaviour));
    sVgcMem[sVgcMemCount].personality = personality;
    return &sVgcMem[sVgcMemCount++];
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

    if (!IsDoubleBattle() || !(gBattleTypeFlags & BATTLE_TYPE_TRAINER))
        return;
    sVgcProtectedLastTurn = sVgcProtectedThisTurn;
    sVgcProtectedThisTurn = 0;
    sVgcThreatened = sVgcLosing = 0;
    sVgcReadRoll = Random() % 100;
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
// Deduzione da giocatore: si è mosso prima di un nostro Pokémon che, a statistiche, è più veloce,
// senza priorità, senza Distorsione: tiene una Stolascelta. Se poi cambia mossa senza uscire, non
// era Choice e la deduzione si ritira.
static void VgcInferChoiceScarf(enum BattlerId battler, enum Move move)
{
    u32 k;
    u32 plSpd;

    if (gFieldStatuses & STATUS_FIELD_TRICK_ROOM)
        return;
    if (GetBattleMovePriority(battler, gAiLogicData->abilities[battler], move) != 0)
        return;
    if (gBattleHistory->itemEffects[battler] != HOLD_EFFECT_NONE && !(sVgcInferredScarf & (1u << battler)))
        return; // strumento già noto davvero
    plSpd = GetBattlerTotalSpeedStat(battler, gAiLogicData->abilities[battler], HOLD_EFFECT_NONE);
    for (k = gCurrentTurnActionNumber + 1; k < gBattlersCount; k++)
    {
        enum BattlerId b = gBattlerByTurnOrder[k];
        enum Move aiMove;
        u32 aiSpd;

        if (IsOnPlayerSide(b) || !IsBattlerAlive(b) || gActionsByTurnOrder[k] != B_ACTION_USE_MOVE)
            continue;
        aiMove = gBattleMons[b].moves[gBattleStruct->chosenMovePositions[b]];
        if (aiMove == MOVE_NONE || aiMove == MOVE_UNAVAILABLE || GetBattleMovePriority(b, gAiLogicData->abilities[b], aiMove) != 0)
            continue;
        aiSpd = GetBattlerTotalSpeedStat(b, gAiLogicData->abilities[b], gAiLogicData->holdEffects[b]);
        if (aiSpd > plSpd && plSpd * 3 / 2 >= aiSpd)
        {
            sVgcInferredScarf |= 1u << battler;
            gBattleHistory->itemEffects[battler] = HOLD_EFFECT_CHOICE_SCARF;
            gAiPartyData->mons[B_TRAINER_PLAYER][gBattlerPartyIndexes[battler]].heldEffect = HOLD_EFFECT_CHOICE_SCARF;
            return;
        }
    }
}

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
        gAiPartyData->mons[B_TRAINER_PLAYER][gBattlerPartyIndexes[battler]].heldEffect = HOLD_EFFECT_NONE;
    }
    VgcInferChoiceScarf(battler, move);
    sVgcPlayerLastMove[battler] = move;
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

// La lettura si applica solo se "ci crediamo" questo turno: evita che il giocatore impari
// a sfruttare un'IA che reagisce sempre allo stesso modo.
static bool32 VgcTrustRead(u32 chance)
{
    return chance >= 50 && sVgcReadRoll < 70;
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
// Valutazione principale
// ---------------------------------------------------------------------
s32 AI_VgcDoubles(enum BattlerId battlerAtk, enum BattlerId battlerDef, enum Move move, s32 score)
{
    enum BattleMoveEffects effect = GetMoveEffect(move);
    enum BattlerId partner = GetPartnerBattler(battlerAtk);
    enum BattlerId foeL = GetBattlerLeftFoe(battlerAtk);
    enum BattlerId foeR = GetBattlerRightFoe(battlerAtk);
    u32 moveIndex = gAiThinkingStruct->movesetIndex;
    bool32 hasPartner = HasPartner(battlerAtk);
    bool32 targetingPartner = IsTargetingPartner(battlerAtk, battlerDef);
    bool32 isStatus = IsBattleMoveStatus(move);
    bool32 firstTurn = IsBattlersFirstTurn(battlerAtk);
    bool32 foeLAlive = IsBattlerAlive(foeL), foeRAlive = IsBattlerAlive(foeR);
    bool32 threatL = foeLAlive && CanTargetFaintAi(foeL, battlerAtk);
    bool32 threatR = foeRAlive && CanTargetFaintAi(foeR, battlerAtk);
    bool32 threatened = threatL || threatR;
    bool32 safeTurn = IsSafeTurn(battlerAtk, foeL, threatL, foeR, threatR);
    u32 hp = gBattleMons[battlerAtk].hp, maxHp = gBattleMons[battlerAtk].maxHP;
    u32 protectP = 0, switchP = 0;
    bool32 defProtectLikely = FALSE, defSwitchLikely = FALSE, anyFoeProtectLikely = FALSE;
    s32 edge;

    if (!IsDoubleBattle())
        return score;

    // Letture sul giocatore (solo info lecita + comportamento osservato).
    if (!targetingPartner && IsBattlerAlive(battlerDef))
    {
        protectP = VgcProtectChance(battlerDef);
        switchP = VgcSwitchChance(battlerDef);
        defProtectLikely = VgcTrustRead(protectP);
        defSwitchLikely = VgcTrustRead(switchP);
    }
    anyFoeProtectLikely = (foeLAlive && VgcTrustRead(VgcProtectChance(foeL))) || (foeRAlive && VgcTrustRead(VgcProtectChance(foeR)));
    edge = VgcMaterialEdge(battlerAtk);

    // Mosse a priorità bloccate da Campo Psichico su bersaglio a terra.
    if (!targetingPartner && IsBattlerAlive(battlerDef) && (gFieldTimers.terrain == B_TERRAIN_PSYCHIC) && AI_IsBattlerGrounded(battlerDef))
    {
        s32 prio = GetMovePriority(move);
        if (gAiLogicData->abilities[battlerAtk] == ABILITY_PRANKSTER && isStatus)
            prio++;
        if (gAiLogicData->abilities[battlerAtk] == ABILITY_GALE_WINGS && GetMoveType(move) == TYPE_FLYING && hp == maxHp)
            prio++;
        if (prio > 0)
            ADJUST_SCORE(-10);
    }

    // ---------------- 0. Piano di squadra ----------------
    {
        const struct VgcGameplan *plan = AI_VgcGetPlan(TRAINER_BATTLE_PARAM.opponentA);
        u16 trainerId = TRAINER_BATTLE_PARAM.opponentA;
        bool32 iAmWinCon = AI_VgcPlanIsWinCon(trainerId, gBattleMons[battlerAtk].species);
        bool32 partnerIsWinCon = hasPartner && AI_VgcPlanIsWinCon(trainerId, gBattleMons[partner].species);
        bool32 doomed = threatened && !PartnerIsRedirecting(battlerAtk)
                     && ((threatL && AI_IsSlower(battlerAtk, foeL, move, MOVE_NONE, CONSIDER_PRIORITY) && !CanAIFaintTarget(battlerAtk, foeL, 1))
                      || (threatR && AI_IsSlower(battlerAtk, foeR, move, MOVE_NONE, CONSIDER_PRIORITY) && !CanAIFaintTarget(battlerAtk, foeR, 1)));

        if (plan != NULL && !targetingPartner)
        {
            // Controllo velocità / meteo previsti dal piano: si impostano volentieri (se convengono:
            // i controlli di convenienza sono nei blocchi 1 e 6, qui solo la preferenza).
            ADJUST_SCORE(PlanMoveBonus(plan, battlerAtk, move));

            if (iAmWinCon)
            {
                // La win condition si conserva e si prepara.
                if (effect == EFFECT_PROTECT && threatened)
                    ADJUST_SCORE(1);
                if (isStatus && IsStatRaisingMove(move) && safeTurn)
                    ADJUST_SCORE(1);
            }
            if (partnerIsWinCon)
            {
                // Il partner è la win condition: lo si copre.
                if (effect == EFFECT_FOLLOW_ME)
                    ADJUST_SCORE(2);
                if (effect == EFFECT_HELPING_HAND)
                    ADJUST_SCORE(1);
                if (effect == EFFECT_FIRST_TURN_ONLY && firstTurn && IsBattlerAlive(battlerDef) && CanTargetFaintAi(battlerDef, partner))
                    ADJUST_SCORE(1);
            }
        }

        // ---------------- Risposte tattiche ----------------
        // Provocazione sul setter / supporto prima che si muova.
        if (effect == EFFECT_TAUNT && !targetingPartner && IsBattlerAlive(battlerDef)
         && BattlerHasVgcSetupMove(battlerDef) && AI_IsFaster(battlerAtk, battlerDef, move, MOVE_NONE, CONSIDER_PRIORITY)
         && !gBattleMons[battlerDef].volatiles.tauntTimer)
            ADJUST_SCORE(3);
        // Ampioguardia quando un avversario ha una mossa ad area nota (e non ci ha visto usarla di continuo).
        if (effect == EFFECT_PROTECT && !targetingPartner && GetMoveProtectMethod(move) == PROTECT_WIDE_GUARD
         && gBattleMons[battlerAtk].volatiles.consecutiveMoveUses == 0
         && (FoeHasKnownSpreadMove(foeL) || FoeHasKnownSpreadMove(foeR)))
            ADJUST_SCORE(2);
        // Anticipo contro Fake Out / priorità note, soprattutto al primo turno (Sceptile Mega ecc.).
        if (effect == EFFECT_PROTECT && !targetingPartner && GetMoveProtectMethod(move) == PROTECT_QUICK_GUARD
         && gBattleMons[battlerAtk].volatiles.consecutiveMoveUses == 0
         && (FoeHasKnownPriorityMove(foeL) || FoeHasKnownPriorityMove(foeR) || FoeThreatensFakeOut(battlerAtk)))
            ADJUST_SCORE(firstTurn ? 5 : 2);
        // Pokémon spacciato (più lento, va KO, non ha KO): Ultimascommessa / mossa che vale il turno.
        if (doomed && effect == EFFECT_FINAL_GAMBIT && !targetingPartner && IsBattlerAlive(battlerDef)
         && gBattleMons[battlerDef].hp <= hp && (!HasMoveWithEffect(battlerDef, EFFECT_PROTECT) || !defProtectLikely))
            ADJUST_SCORE(5);
        // Sonno (Spora, Ipnosi, Sbadiglio) su chi ha setup / controllo velocità: lo si toglie dalla lotta.
        if (isStatus && !targetingPartner && IsBattlerAlive(battlerDef) && GetMoveNonVolatileStatus(move) == MOVE_EFFECT_SLEEP
         && BattlerHasVgcSetupMove(battlerDef) && !defSwitchLikely)
            ADJUST_SCORE(1);
        // Ipnosi / Spora sotto Gravity: accuratezza al 100%, valore enorme (Enzo Gravity Hypnosis).
        if (isStatus && !targetingPartner && IsBattlerAlive(battlerDef) && GetMoveNonVolatileStatus(move) == MOVE_EFFECT_SLEEP
         && (gFieldStatuses & STATUS_FIELD_GRAVITY) && gBattleMons[battlerDef].status1 == STATUS1_NONE && !defSwitchLikely)
            ADJUST_SCORE(5);

        // ---------------- 7. Regole delle squadre (docs/vgc_trainers.md, sezione "Linee di gioco") ----------------
        // Ultimascommessa proattiva (Enzo, Annihilape Scarf): toglie subito il pezzo chiave e crea il 4v3.
        // Serve: essere più veloci, avere più PS del bersaglio, che il bersaglio non sia immune (Spettro),
        // non abbia Focus Sash / Vigore a PS pieni e non stia probabilmente proteggendosi.
        if (effect == EFFECT_FINAL_GAMBIT && !targetingPartner && IsBattlerAlive(battlerDef))
        {
            bool32 sashed = gBattleMons[battlerDef].hp == gBattleMons[battlerDef].maxHP
                         && (gAiLogicData->holdEffects[battlerDef] == HOLD_EFFECT_FOCUS_SASH || gAiLogicData->abilities[battlerDef] == ABILITY_STURDY);
            if (gAiLogicData->effectiveness[battlerAtk][battlerDef][moveIndex] == UQ_4_12(0.0))
            {
                ADJUST_SCORE(-20);
            }
            else if (hp >= gBattleMons[battlerDef].hp && !sashed && !defProtectLikely
                  && AI_IsFaster(battlerAtk, battlerDef, move, MOVE_NONE, CONSIDER_PRIORITY))
            {
                ADJUST_SCORE(7); // supera il -5 casuale del motore: è una giocata, non un azzardo
                if (BattlerHasVgcSetupMove(battlerDef) || CanTargetFaintAi(battlerDef, battlerAtk)
                 || (hasPartner && CanTargetFaintAi(battlerDef, partner)))
                    ADJUST_SCORE(2); // il bersaglio è il pezzo chiave
                if (firstTurn)
                    ADJUST_SCORE(1);
                if (CountUsablePartyMons(battlerAtk) == 0)
                    ADJUST_SCORE(-6); // ultimo Pokémon: sacrificarsi lascia il partner da solo
            }
            else if (!doomed)
            {
                ADJUST_SCORE(-4);
            }
        }

        // Toedscruel con Disinvoltura (dopo Raggio Semplice) è veloce: Distorsione gli toglierebbe il vantaggio.
        if (effect == EFFECT_TRICK_ROOM && !targetingPartner && gAiLogicData->abilities[battlerAtk] == ABILITY_SIMPLE)
            ADJUST_SCORE(-6);

        // Staffetta (Malamar con Inversione): passa i potenziamenti quando sta per andare KO.
        if (effect == EFFECT_BATON_PASS)
        {
            if (!AnyUsefulStatIsRaised(battlerAtk) || CountUsablePartyMons(battlerAtk) == 0)
                ADJUST_SCORE(-6);
            else if (threatened)
                ADJUST_SCORE(4);
        }

        // Weezing-Galar con Slaking accanto: il Gas Reagente tiene spento Pigrizia, Weezing va tenuto vivo.
        if (effect == EFFECT_PROTECT && !targetingPartner && threatened && hasPartner
         && gAiLogicData->abilities[battlerAtk] == ABILITY_NEUTRALIZING_GAS
         && AI_VgcSameSpecies(SPECIES_SLAKING, gBattleMons[partner].species))
            ADJUST_SCORE(3);

        // --- SAVAROTTO ---
        if (trainerId == TRAINER_SAVAROTTO)
        {
            u16 mySpecies = gBattleMons[battlerAtk].species;

            // 1. Fake Out al turno 1 da Incineroar / Sneasler per garantire il setup di Espathra
            if (hasPartner && AI_VgcSameSpecies(SPECIES_ESPATHRA, gBattleMons[partner].species) && firstTurn)
            {
                if (effect == EFFECT_FIRST_TURN_ONLY && !targetingPartner && IsFlinchGuaranteed(battlerAtk, battlerDef, move))
                    ADJUST_SCORE(8);
            }

            // 2. Whimsicott + Gallade: Beat Up su Gallade con Giustizia (Justified)
            if (hasPartner && AI_VgcSameSpecies(SPECIES_WHIMSICOTT, mySpecies) && AI_VgcSameSpecies(SPECIES_GALLADE, gBattleMons[partner].species))
            {
                if (move == MOVE_BEAT_UP && targetingPartner)
                    ADJUST_SCORE(12);
                else if (effect == EFFECT_TAILWIND && !(gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_TAILWIND) && !targetingPartner)
                    ADJUST_SCORE(5);
            }

            // 3. Espathra:
            // - Turno 1: setup di Calm Mind
            // - Turno 2: se non ha Substitute, Sub se non vede minacce immediate o Protect se minacciata
            // - Poi: Baton Pass per passare i boost a Chandelure
            if (AI_VgcSameSpecies(SPECIES_ESPATHRA, mySpecies))
            {
                bool32 hasSub = (gBattleMons[battlerAtk].status2 & STATUS2_SUBSTITUTE) != 0;
                s8 spatkStage = gBattleMons[battlerAtk].statStages[STAT_SPATK];
                s8 speedStage = gBattleMons[battlerAtk].statStages[STAT_SPEED];
                bool32 hasBoosts = (spatkStage > DEFAULT_STAT_STAGE || speedStage > DEFAULT_STAT_STAGE);

                if (firstTurn)
                {
                    if (move == MOVE_CALM_MIND)
                        ADJUST_SCORE(10);
                }
                else
                {
                    if (hasBoosts)
                    {
                        if (move == MOVE_BATON_PASS && CountUsablePartyMons(battlerAtk) > 0)
                        {
                            if (hasSub || spatkStage >= DEFAULT_STAT_STAGE + 2 || !safeTurn)
                                ADJUST_SCORE(10);
                            else
                                ADJUST_SCORE(5);
                        }
                        if (move == MOVE_SUBSTITUTE && !hasSub && hp > gBattleMons[battlerAtk].maxHP / 4)
                        {
                            if (!threatened)
                                ADJUST_SCORE(8);
                            else
                                ADJUST_SCORE(-4);
                        }
                        if (effect == EFFECT_PROTECT && threatened && !hasSub)
                        {
                            ADJUST_SCORE(6);
                        }
                    }
                    else
                    {
                        if (move == MOVE_CALM_MIND)
                            ADJUST_SCORE(8);
                    }
                }
            }
        }

        // --- PATATINA ---
        if (trainerId == TRAINER_PATATINA && hasPartner)
        {
            u16 mySpecies = gBattleMons[battlerAtk].species;
            u16 partnerSpecies = gBattleMons[partner].species;

            // 1. Armarouge + Indeedee-F:
            // Indeedee-F usa Segui Me ("sonoqui"), Armarouge usa Forzespansa (Expanding Force)
            if (AI_VgcSameSpecies(SPECIES_INDEEDEE_F, mySpecies) && AI_VgcSameSpecies(SPECIES_ARMAROUGE, partnerSpecies))
            {
                if (effect == EFFECT_FOLLOW_ME)
                    ADJUST_SCORE(8);
                else if (effect == EFFECT_TRICK_ROOM)
                    ADJUST_SCORE(-10); // mai fare Trick Room con Armarouge
            }
            if (AI_VgcSameSpecies(SPECIES_ARMAROUGE, mySpecies) && AI_VgcSameSpecies(SPECIES_INDEEDEE_F, partnerSpecies))
            {
                if (move == MOVE_EXPANDING_FORCE && gFieldTimers.terrain == B_TERRAIN_PSYCHIC && !targetingPartner)
                    ADJUST_SCORE(8);
            }

            // 2. Indeedee-F + Copperajah:
            // Indeedee-F usa Distorsione (Trick Room), Copperajah copre/prepara
            if (AI_VgcSameSpecies(SPECIES_INDEEDEE_F, mySpecies) && AI_VgcSameSpecies(SPECIES_COPPERAJAH, partnerSpecies))
            {
                if (effect == EFFECT_TRICK_ROOM && !(gFieldStatuses & STATUS_FIELD_TRICK_ROOM) && !targetingPartner)
                    ADJUST_SCORE(8);
                else if (effect == EFFECT_FOLLOW_ME && !(gFieldStatuses & STATUS_FIELD_TRICK_ROOM))
                    ADJUST_SCORE(-8); // non fare Follow Me quando devi piazzare Distorsione per Copperajah
            }
            if (AI_VgcSameSpecies(SPECIES_COPPERAJAH, mySpecies) && AI_VgcSameSpecies(SPECIES_INDEEDEE_F, partnerSpecies))
            {
                if (!(gFieldStatuses & STATUS_FIELD_TRICK_ROOM) && firstTurn)
                {
                    if (effect == EFFECT_PROTECT && !targetingPartner)
                        ADJUST_SCORE(4);
                    else if (move == MOVE_IRON_DEFENSE)
                        ADJUST_SCORE(3);
                }
            }

            // 3. Altaria + (Ceruledge / Armarouge / Rotom-Wash):
            // Altaria usa Ventoincoda (Tailwind)
            if (AI_VgcSameSpecies(SPECIES_ALTARIA, mySpecies))
            {
                if (effect == EFFECT_TAILWIND && !(gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_TAILWIND) && !targetingPartner)
                    ADJUST_SCORE(8);
            }
        }

        // Forzespansa sotto Campo Psichico (Armarouge): colpisce entrambi a potenza piena.
        if (move == MOVE_EXPANDING_FORCE && !targetingPartner && gFieldTimers.terrain == B_TERRAIN_PSYCHIC
         && AI_IsBattlerGrounded(battlerAtk) && foeLAlive && foeRAlive)
            ADJUST_SCORE(3);

        // Dopo di te sotto Distorsione: il partner lento si muove già per primo, il turno è sprecato.
        if (move == MOVE_AFTER_YOU && targetingPartner && (gFieldStatuses & STATUS_FIELD_TRICK_ROOM)
         && !(gBattleWeather & B_WEATHER_SUN))
            ADJUST_SCORE(-5);

        // Giornodisole (Sableye Burla) quando il sole manca e in squadra ci sono i Paradox con Protosintesi.
        if (effect == EFFECT_WEATHER && !targetingPartner && GetMoveWeatherType(move) == BATTLE_WEATHER_SUN
         && !(gBattleWeather & B_WEATHER_SUN) && TeamHasWeatherAbuser(battlerAtk, move))
            ADJUST_SCORE(2);
    }

    // ---------------- 1. Apertura ----------------
    if (effect == EFFECT_FIRST_TURN_ONLY && firstTurn && !targetingPartner && IsFlinchGuaranteed(battlerAtk, battlerDef, move))
    {
        // Fake Out su chi vuole fare setup / controllo velocità: gli si toglie il turno.
        if (BattlerHasVgcSetupMove(battlerDef))
            ADJUST_SCORE(4);
        // Fake Out sulla minaccia: il partner (o noi al turno dopo) ha un turno libero.
        if (CanTargetFaintAi(battlerDef, battlerAtk) || (hasPartner && CanTargetFaintAi(battlerDef, partner)))
            ADJUST_SCORE(3);
        // Il partner sta facendo setup: copriamolo.
        if (PartnerIsSettingUp(battlerAtk))
            ADJUST_SCORE(2);
        // Bersaglio con Focus Sash / Disguise: Fake Out lo rompe senza sprecare un colpo vero.
        if (gAiLogicData->holdEffects[battlerDef] == HOLD_EFFECT_FOCUS_SASH || gAiLogicData->abilities[battlerDef] == ABILITY_DISGUISE)
            ADJUST_SCORE(1);
        // Se probabilmente si protegge, il Fake Out va sull'altro.
        if (defProtectLikely)
            ADJUST_SCORE(-3);
    }

    // Turno "regalato": se un avversario quasi certamente si protegge o cambia, setup e controllo
    // velocità valgono di più (si usa il turno in cui loro non attaccano).
    if (isStatus && !targetingPartner && IsVgcSetupMove(move) && (anyFoeProtectLikely || defSwitchLikely))
        ADJUST_SCORE(2);

    if (effect == EFFECT_TRICK_ROOM && !targetingPartner)
    {
        bool32 trUp = (gFieldStatuses & STATUS_FIELD_TRICK_ROOM) != 0;
        bool32 weAreSlow = AI_VgcTeamIsSlowerThanFoes(battlerAtk, 2);

        if (!trUp)
        {
            if (gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_TAILWIND)
            {
                ADJUST_SCORE(-8); // Conflitto con Ventoincoda
            }
            else if ((gBattleWeather & B_WEATHER_RAIN) && hasPartner && gAiLogicData->abilities[partner] == ABILITY_SWIFT_SWIM)
            {
                ADJUST_SCORE(-10); // Swift Swim attivo sotto pioggia
            }
            else if (weAreSlow)
            {
                ADJUST_SCORE(5);
                // Ci servono subito: siamo più lenti e sotto minaccia, il partner non risolve da solo.
                if (threatened && !(foeLAlive && PartnerKOsFirst(battlerAtk, foeL)) && !(foeRAlive && PartnerKOsFirst(battlerAtk, foeR)))
                    ADJUST_SCORE(2);
                // Primo turno con Fake Out avversario in vista: Erbamentale o partner che li blocca.
                if (firstTurn && FoeThreatensFakeOut(battlerAtk) && gAiLogicData->holdEffects[battlerAtk] != HOLD_EFFECT_MENTAL_HERB
                 && !PartnerIsRedirecting(battlerAtk))
                    ADJUST_SCORE(-2);
            }
            else if (!AI_VgcTeamIsSlowerThanFoes(battlerAtk, 1))
            {
                ADJUST_SCORE(-8); // aiuterebbe solo l'avversario
            }
        }
        else if (gFieldTimers.trickRoomTimer > 1)
        {
            // Toglierlo conviene solo se è il loro.
            if (weAreSlow)
                ADJUST_SCORE(-10);
            else
                ADJUST_SCORE(4);
        }
    }

    if (effect == EFFECT_TAILWIND && !targetingPartner)
    {
        enum BattleSide side = GetBattlerSide(battlerAtk);

        if (gSideStatuses[side] & SIDE_STATUS_TAILWIND)
        {
            ADJUST_SCORE(-10);
        }
        else if ((gFieldStatuses & STATUS_FIELD_TRICK_ROOM) && gFieldTimers.trickRoomTimer > 1)
        {
            ADJUST_SCORE(-8);
        }
        else if (AI_VgcTeamBenefitsFromTailwind(battlerAtk, 2))
        {
            ADJUST_SCORE(6);
        }
        else if (AI_VgcTeamBenefitsFromTailwind(battlerAtk, 1))
        {
            ADJUST_SCORE(4);
        }
        else if (FoeMaxSpeed(battlerAtk) < gAiLogicData->speedStats[battlerAtk]
              && (!hasPartner || FoeMaxSpeed(battlerAtk) < gAiLogicData->speedStats[partner]))
        {
            ADJUST_SCORE(-4); // siamo già più veloci
        }
    }

    if (effect == EFFECT_GRAVITY && !targetingPartner)
    {
        bool32 gravUp = (gFieldStatuses & STATUS_FIELD_GRAVITY) != 0;
        if (!gravUp)
        {
            if (firstTurn || HasMove(battlerAtk, MOVE_HYPNOSIS) || (hasPartner && HasMove(partner, MOVE_HYPNOSIS)))
                ADJUST_SCORE(6);
            else
                ADJUST_SCORE(3);
        }
        else
        {
            ADJUST_SCORE(-10);
        }
    }

    // ---------------- 6. Meteo con chi lo sfrutta in panchina ----------------
    if (effect == EFFECT_WEATHER && !targetingPartner && TeamHasWeatherAbuser(battlerAtk, move))
        ADJUST_SCORE(3);

    // Schermi col primo turno (Prankster): valgono il turno.
    if ((effect == EFFECT_REFLECT || effect == EFFECT_LIGHT_SCREEN || effect == EFFECT_AURORA_VEIL)
     && !targetingPartner && firstTurn && gAiLogicData->abilities[battlerAtk] == ABILITY_PRANKSTER)
        ADJUST_SCORE(2);

    // ---------------- 2. Setup sicuro ----------------
    if (isStatus && !targetingPartner && AI_GetBattlerMoveTargetType(battlerAtk, move) == TARGET_USER
     && (IsStatRaisingMove(move) || effect == EFFECT_BELLY_DRUM))
    {
        bool32 canKoNow = (foeLAlive && CanAIFaintTarget(battlerAtk, foeL, 1)) || (foeRAlive && CanAIFaintTarget(battlerAtk, foeR, 1));
        bool32 slowerThanThreat = (threatL && AI_IsSlower(battlerAtk, foeL, move, MOVE_NONE, DONT_CONSIDER_PRIORITY))
                               || (threatR && AI_IsSlower(battlerAtk, foeR, move, MOVE_NONE, DONT_CONSIDER_PRIORITY));

        if (canKoNow)
        {
            ADJUST_SCORE(-4); // c'è un KO sul tavolo: si prende
        }
        else if (safeTurn)
        {
            ADJUST_SCORE(3);
            // Nessuno ci manda nemmeno 2HKO: turno regalato.
            if ((!foeLAlive || GetBestNoOfHitsToKO(foeL, battlerAtk, AI_DEFENDING) > 2)
             && (!foeRAlive || GetBestNoOfHitsToKO(foeR, battlerAtk, AI_DEFENDING) > 2))
                ADJUST_SCORE(2);
            if (effect == EFFECT_BELLY_DRUM && hp * 100 / maxHp > 60)
                ADJUST_SCORE(2);
            if (effect == EFFECT_BELLY_DRUM && hasPartner && threatened && hp * 100 / maxHp > 50)
                ADJUST_SCORE(5); // Snorlax Panciata con Amoonguss Polverabbia
        }
        else if (slowerThanThreat)
        {
            ADJUST_SCORE(effect == EFFECT_BELLY_DRUM ? -8 : -5); // fare setup e morire = regalare un turno
        }
        else
        {
            ADJUST_SCORE(-2);
        }

        // Ferrodifesa + Schiacciacorpo è un piano, non un ripiego.
        if (HasMove(battlerAtk, MOVE_BODY_PRESS) && effect == EFFECT_STAT_CHANGE && IsStatRaisingMove(move)
         && gBattleMons[battlerAtk].statStages[STAT_DEF] < DEFAULT_STAT_STAGE + 4)
            ADJUST_SCORE(2);

        // Già potenziato: ora si attacca.
        if (AnyUsefulStatIsRaised(battlerAtk))
            ADJUST_SCORE(-3);

        // Primo turno con Fake Out avversario: il setup salta.
        if (firstTurn && FoeThreatensFakeOut(battlerAtk) && !PartnerIsRedirecting(battlerAtk))
            ADJUST_SCORE(-3);
    }

    // Troppopugno con Inversione (Malamar) vs Danzaspada (Armaldo).
    if (move == MOVE_SUPERPOWER && !targetingPartner)
    {
        if (gAiLogicData->abilities[battlerAtk] == ABILITY_CONTRARY)
            ADJUST_SCORE(4); // Setup offensivo: +1 Atk e +1 Def
        else if (gBattleMons[battlerAtk].statStages[STAT_ATK] > DEFAULT_STAT_STAGE)
            ADJUST_SCORE(-3); // Non vanificare Danzaspada
    }

    // ---------------- 3. Protect "safe" ----------------
    if (effect == EFFECT_PROTECT && !targetingPartner)
    {
        enum ProtectMethod method = GetMoveProtectMethod(move);

        if (method != PROTECT_WIDE_GUARD && method != PROTECT_QUICK_GUARD && method != PROTECT_CRAFTY_SHIELD
         && method != PROTECT_MAT_BLOCK && gBattleMons[battlerAtk].volatiles.consecutiveMoveUses == 0)
        {
            u32 dmgL = foeLAlive ? GetBestDmgFromBattler(foeL, battlerAtk, AI_DEFENDING) : 0;
            u32 dmgR = foeRAlive ? GetBestDmgFromBattler(foeR, battlerAtk, AI_DEFENDING) : 0;
            bool32 doubleTargetKO = !threatened && (dmgL + dmgR >= hp) && foeLAlive && foeRAlive;

            if (threatened)
            {
                ADJUST_SCORE(2);
                // Il partner toglie di mezzo la minaccia mentre noi ci copriamo: turno perfetto.
                if ((threatL && PartnerKOsFirst(battlerAtk, foeL)) || (threatR && PartnerKOsFirst(battlerAtk, foeR)))
                    ADJUST_SCORE(4);
                // Siamo più lenti e non abbiamo il KO per primi: senza Protect si muore.
                if (((threatL && AI_IsSlower(battlerAtk, foeL, move, MOVE_NONE, CONSIDER_PRIORITY) && !CanAIFaintTarget(battlerAtk, foeL, 1))
                  || (threatR && AI_IsSlower(battlerAtk, foeR, move, MOVE_NONE, CONSIDER_PRIORITY) && !CanAIFaintTarget(battlerAtk, foeR, 1)))
                 && hp * 100 / maxHp > 35)
                    ADJUST_SCORE(2);
                // Primo turno con Fake Out avversario: Protect lo assorbe senza perdere nulla di importante.
                if (firstTurn && FoeThreatensFakeOut(battlerAtk))
                    ADJUST_SCORE(1);
            }
            else if (doubleTargetKO)
            {
                ADJUST_SCORE(3); // insieme ci mandano KO: ragionevole aspettarsi il doppio bersaglio
            }
            else if ((!foeLAlive || GetBestNoOfHitsToKO(foeL, battlerAtk, AI_DEFENDING) > 3)
                  && (!foeRAlive || GetBestNoOfHitsToKO(foeR, battlerAtk, AI_DEFENDING) > 3))
            {
                ADJUST_SCORE(-3); // nessuno ci fa male: Protect è un turno buttato
            }

            // Far scorrere il Trick Room avversario.
            if (FoeTrickRoomActive(battlerAtk))
                ADJUST_SCORE(3);
            // I turni del NOSTRO Tailwind / Trick Room sono preziosi.
            if (AI_VgcOwnSpeedControlActive(battlerAtk) && !threatened)
                ADJUST_SCORE(-4);
            // Il Focus Sash al primo turno copre già.
            if (firstTurn && hp == maxHp && gAiLogicData->holdEffects[battlerAtk] == HOLD_EFFECT_FOCUS_SASH)
                ADJUST_SCORE(-2);
            // Il partner sta cambiando: proteggersi mentre entra il sostituto è sensato.
            if (hasPartner && gBattleStruct->monToSwitchIntoId[partner] != PARTY_SIZE)
                ADJUST_SCORE(1);
        }
    }

    // ---------------- Risorse: turni di controllo velocità, endgame ----------------
    {
        enum BattleSide side = GetBattlerSide(battlerAtk);
        u32 scLeft = 0;

        if (gSideStatuses[side] & SIDE_STATUS_TAILWIND)
            scLeft = gSideTimers[side].tailwindTimer;
        else if ((gFieldStatuses & STATUS_FIELD_TRICK_ROOM) && AI_VgcTeamIsSlowerThanFoes(battlerAtk, 1))
            scLeft = gFieldTimers.trickRoomTimer;
        // Ultimi due turni del NOSTRO Tailwind / Distorsione: si spinge. Non è bianco o nero: se un
        // avversario ci manda KO, Protect resta valido (il blocco 3 lo valuta e può salvare la partita);
        // il malus cresce solo quanto meno siamo in pericolo.
        if (scLeft != 0 && scLeft <= 2 && !targetingPartner)
        {
            if (!isStatus && IsBattlerAlive(battlerDef))
            {
                ADJUST_SCORE(CanIndexMoveFaintTarget(battlerAtk, battlerDef, moveIndex, AI_ATTACKING) ? 2 : 1);
            }
            else if (effect == EFFECT_PROTECT)
            {
                if (threatened)
                    ADJUST_SCORE(0);  // sotto KO: decide il blocco 3 (partner che toglie la minaccia, ecc.)
                else if ((foeLAlive && GetBestNoOfHitsToKO(foeL, battlerAtk, AI_DEFENDING) <= 2)
                      || (foeRAlive && GetBestNoOfHitsToKO(foeR, battlerAtk, AI_DEFENDING) <= 2))
                    ADJUST_SCORE(-1); // ci fanno male ma non ci mandano KO: si preferisce attaccare
                else
                    ADJUST_SCORE(-3); // nessun pericolo: Protect spreca un turno prezioso
            }
            else if (isStatus && effect != EFFECT_TRICK_ROOM && effect != EFFECT_TAILWIND
                  && effect != EFFECT_HELPING_HAND && effect != EFFECT_FOLLOW_ME && !threatened)
            {
                ADJUST_SCORE(-1);
            }
        }
        // Endgame in vantaggio netto: si chiude con quello che c'è in campo, niente riposizionamenti.
        if (edge >= 2 && !targetingPartner)
        {
            if (!isStatus && IsBattlerAlive(battlerDef))
                ADJUST_SCORE(1);
            else if (isStatus && effect != EFFECT_PROTECT && !IsVgcSetupMove(move))
                ADJUST_SCORE(-1);
        }
    }

    // ---------------- 4. Fuoco concentrato / scelta del bersaglio ----------------
    if (!isStatus && !targetingPartner && IsBattlerAlive(battlerDef) && hasPartner)
    {
        u32 myDmg = gAiLogicData->simulatedDmg[battlerAtk][battlerDef][moveIndex].median;
        u32 myMin = gAiLogicData->simulatedDmg[battlerAtk][battlerDef][moveIndex].minimum;
        u32 myMax = gAiLogicData->simulatedDmg[battlerAtk][battlerDef][moveIndex].maximum;
        u32 partnerDmg = PartnerDamageTo(battlerAtk, battlerDef);
        u32 defHp = gBattleMons[battlerDef].hp;

        // KO sicuro (anche col tiro minimo) vale più di un KO "a tiro": un KO al 40% non è un KO.
        if (myMin >= defHp)
            ADJUST_SCORE(2);
        else if (myMax >= defHp && myDmg < defHp)
            ADJUST_SCORE(-1);

        if (partnerDmg > 0)
        {
            if (PartnerKOsFirst(battlerAtk, battlerDef))
            {
                // Il partner lo manda KO da solo e prima: non si spreca il colpo.
                if (HasMoveWithEffect(battlerDef, EFFECT_PROTECT))
                    ADJUST_SCORE(-1); // ...a meno che possa proteggersi: allora raddoppiare ha senso
                else if (!IsSpreadMove(AI_GetBattlerMoveTargetType(battlerAtk, move)))
                    ADJUST_SCORE(-3);
            }
            else if (myDmg < defHp && myDmg + partnerDmg >= defHp)
            {
                ADJUST_SCORE(4); // insieme lo mandiamo KO
            }
        }

        // La minaccia vale di più come bersaglio.
        if (CanTargetFaintAi(battlerDef, battlerAtk) || CanTargetFaintAi(battlerDef, partner))
            ADJUST_SCORE(1);
        // Può ancora megaevolvere (pietra probabile dalla lista del formato): cambierà statistiche e
        // abilità, meglio toglierlo prima.
        if (IsOnPlayerSide(battlerDef) && !HasTrainerUsedGimmick(battlerDef, GIMMICK_MEGA)
         && GET_BASE_SPECIES_ID(gBattleMons[battlerDef].species) == gBattleMons[battlerDef].species
         && Scout_MegaTargetSpecies(gBattleMons[battlerDef].species) != SPECIES_NONE)
            ADJUST_SCORE(1);
        if (BattlerHasVgcSetupMove(battlerDef) && !IsBattlerIncapacitated(battlerDef, gAiLogicData->abilities[battlerDef]))
            ADJUST_SCORE(1);

        // Mossa ad area che fa male a entrambi: ottimo in VGC.
        if (IsSpreadMove(AI_GetBattlerMoveTargetType(battlerAtk, move)) && foeLAlive && foeRAlive)
        {
            u32 dl = gAiLogicData->simulatedDmg[battlerAtk][foeL][moveIndex].median * 100 / max(gBattleMons[foeL].maxHP, 1);
            u32 dr = gAiLogicData->simulatedDmg[battlerAtk][foeR][moveIndex].median * 100 / max(gBattleMons[foeR].maxHP, 1);
            if (dl >= 35 && dr >= 35)
                ADJUST_SCORE(2);
        }

        // Sotto Trick Room avversario con il nostro Pokémon veloce: conviene attaccare subito ciò che si può.
        if (FoeTrickRoomActive(battlerAtk) && CanIndexMoveFaintTarget(battlerAtk, battlerDef, moveIndex, AI_ATTACKING))
            ADJUST_SCORE(1);

        // Lettura Protect: colpire chi probabilmente si protegge è un turno perso; le mosse ad area
        // perdono meno (colpiscono comunque l'altro).
        if (defProtectLikely)
            ADJUST_SCORE(IsSpreadMove(AI_GetBattlerMoveTargetType(battlerAtk, move)) ? -1 : -3);
        // Ha protetto il turno scorso ed è di nuovo sotto KO: difficilmente lo rifà, si insiste.
        else if ((sVgcProtectedLastTurn & (1u << battlerDef)) && (sVgcThreatened & (1u << battlerDef)))
            ADJUST_SCORE(1);
        // Lettura cambio: il colpo "da KO" finisce sul sostituto; meglio l'altro bersaglio o una mossa
        // che punisce l'entrata (Knock Off, mosse ad area, stato).
        if (defSwitchLikely)
        {
            if (CanIndexMoveFaintTarget(battlerAtk, battlerDef, moveIndex, AI_ATTACKING))
                ADJUST_SCORE(-2);
            if (move == MOVE_KNOCK_OFF || IsSpreadMove(AI_GetBattlerMoveTargetType(battlerAtk, move)))
                ADJUST_SCORE(1);
        }

        // Vantaggio numerico netto: si gioca pulito (KO sicuri, niente mosse imprecise);
        // in svantaggio si cercano i colpi che ribaltano.
        if (edge >= 2 && gAiLogicData->moveAccuracy[battlerAtk][battlerDef][moveIndex] < 85)
            ADJUST_SCORE(-1);
        if (edge <= -2 && CanIndexMoveFaintTarget(battlerAtk, battlerDef, moveIndex, AI_ATTACKING))
            ADJUST_SCORE(1);

        // Eruzione cala drasticamente con gli HP persi: sotto 65% meglio Ondacalda / Lanciafiamme (Gigi Pinto).
        if (move == MOVE_ERUPTION)
        {
            if (hp * 100 / maxHp < 65)
                ADJUST_SCORE(-4);
            else
                ADJUST_SCORE(2);
        }

        // Swift Swim Golduck sotto pioggia: pressione massima (Mario).
        if ((gBattleWeather & B_WEATHER_RAIN) && gAiLogicData->abilities[battlerAtk] == ABILITY_SWIFT_SWIM
         && GetMoveType(move) == TYPE_WATER)
            ADJUST_SCORE(3);

        // Fuoco amico su Terremoto e mosse ad area totali (TARGET_FOES_AND_ALLY - Gerozz).
        if (AI_GetBattlerMoveTargetType(battlerAtk, move) == TARGET_FOES_AND_ALLY)
        {
            bool32 partnerImmune = FALSE;
            if (gAiLogicData->abilities[partner] == ABILITY_TELEPATHY)
            {
                partnerImmune = TRUE;
            }
            else if (GetMoveType(move) == TYPE_GROUND && !AI_IsBattlerGrounded(partner))
            {
                partnerImmune = TRUE; // Levitazione (Rotom-Heat), Volante, Palloncino
            }
            else
            {
                u32 pIdx;
                enum BattlerId pTarget;
                enum Move pMove = PartnerChosenMove(battlerAtk, &pIdx, &pTarget);
                if (pMove != MOVE_NONE && GetMoveEffect(pMove) == EFFECT_PROTECT)
                    partnerImmune = TRUE;
            }
            if (!partnerImmune)
                ADJUST_SCORE(-5);
        }
    }

    // Stato / Spore / Encore su chi probabilmente cambia: lo prende il sostituto, non lui.
    if (isStatus && !targetingPartner && defSwitchLikely && AI_GetBattlerMoveTargetType(battlerAtk, move) == TARGET_SELECTED
     && (effect == EFFECT_ENCORE || effect == EFFECT_DISABLE || effect == EFFECT_TAUNT))
        ADJUST_SCORE(-3);

    // In vantaggio netto: Protect sotto minaccia vale un filo di più (si conserva il vantaggio).
    if (effect == EFFECT_PROTECT && !targetingPartner && edge >= 2 && threatened)
        ADJUST_SCORE(1);

    // ---------------- 5. Supporto ----------------
    if (effect == EFFECT_FOLLOW_ME && hasPartner)
    {
        if (PartnerIsSettingUp(battlerAtk))
            ADJUST_SCORE(4);
        else if ((foeLAlive && CanTargetFaintAi(foeL, partner)) || (foeRAlive && CanTargetFaintAi(foeR, partner)))
            ADJUST_SCORE(2); // il partner è la minaccia che vogliono togliere
        if (hp * 100 / maxHp < 30)
            ADJUST_SCORE(-3);
    }

    if (effect == EFFECT_HELPING_HAND && hasPartner)
    {
        enum BattlerId pTarget;
        u32 pIdx;
        enum Move pMove = PartnerChosenMove(battlerAtk, &pIdx, &pTarget);

        if (pMove != MOVE_NONE && !IsBattleMoveStatus(pMove) && IsBattlerAlive(pTarget) && !IsBattlerAlly(battlerAtk, pTarget))
        {
            u32 d = gAiLogicData->simulatedDmg[partner][pTarget][pIdx].median;
            if (d < gBattleMons[pTarget].hp && d * 3 / 2 >= gBattleMons[pTarget].hp)
                ADJUST_SCORE(3); // da 2HKO a OHKO
        }
    }

    // Instruct (Oranguru): fa ripetere una potente mossa offensiva al partner (Mario).
    if (effect == EFFECT_INSTRUCT && hasPartner)
    {
        if (targetingPartner)
        {
            u32 pIdx;
            enum BattlerId pTarget;
            enum Move pMove = PartnerChosenMove(battlerAtk, &pIdx, &pTarget);
            u32 lastMove = gAiLogicData->lastUsedMove[partner];
            enum Move mToRepeat = (pMove != MOVE_NONE) ? pMove : lastMove;
            if (mToRepeat != MOVE_NONE && !IsBattleMoveStatus(mToRepeat) && !IsMoveInstructBanned(mToRepeat))
                ADJUST_SCORE(5);
        }
        else
        {
            ADJUST_SCORE(-10);
        }
    }

    // After You (Lilligant): dà subito il turno al partner (es. Torkoal con Eruzione sotto Sole - Gigi Pinto).
    if (move == MOVE_AFTER_YOU && hasPartner)
    {
        if (targetingPartner)
        {
            if (gBattleMons[partner].species == SPECIES_TORKOAL && (gBattleWeather & B_WEATHER_SUN)
             && (gBattleMons[partner].hp * 100 / max(gBattleMons[partner].maxHP, 1) >= 70))
                ADJUST_SCORE(6);
            else if (AI_IsFaster(battlerAtk, partner, move, MOVE_NONE, CONSIDER_PRIORITY))
                ADJUST_SCORE(3);
        }
        else
        {
            ADJUST_SCORE(-10);
        }
    }

    // Simple Beam (Simone): sblocca Spore su Toedscruel (rimuove Mycelium Might), MAI su Machamp (toglierebbe Guts).
    if (move == MOVE_SIMPLE_BEAM)
    {
        if (targetingPartner && hasPartner)
        {
            if (gBattleMons[partner].species == SPECIES_TOEDSCRUEL && gAiLogicData->abilities[partner] == ABILITY_MYCELIUM_MIGHT)
                ADJUST_SCORE(6);
            else if (gBattleMons[partner].species == SPECIES_MACHAMP)
                ADJUST_SCORE(-20);
        }
        else if (targetingPartner)
        {
            ADJUST_SCORE(-10);
        }
    }

    // Icy Wind / Electroweb ecc. come controllo velocità "morbido" quando siamo appena più lenti.
    if (!isStatus && !targetingPartner && foeLAlive && foeRAlive && IsSpreadMove(AI_GetBattlerMoveTargetType(battlerAtk, move))
     && (move == MOVE_ICY_WIND || move == MOVE_ELECTROWEB || move == MOVE_BULLDOZE)
     && !(gFieldStatuses & STATUS_FIELD_TRICK_ROOM) && !(gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_TAILWIND)
     && AI_VgcTeamBenefitsFromTailwind(battlerAtk, 1))
        ADJUST_SCORE(2);

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
    u16 designate = TeamPreview_GetMegaDesignateSpecies();
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
