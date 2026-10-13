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
#include "battle_main.h"
#include "battle_util.h"
#include "move.h"
#include "pokemon.h"
#include "random.h"
#include "constants/abilities.h"
#include "constants/battle_ai.h"
#include "constants/battle_move_effects.h"
#include "constants/hold_effects.h"
#include "constants/moves.h"

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

static bool32 BattlerHasVgcSetupMove(enum BattlerId battler)
{
    u32 i;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        enum Move m = gBattleMons[battler].moves[i];
        if (m != MOVE_NONE && IsVgcSetupMove(m))
            return TRUE;
    }
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

    if (IsBattlerAlive(foeL) && IsBattlersFirstTurn(foeL) && HasMove(foeL, MOVE_FAKE_OUT))
        return TRUE;
    if (IsBattlerAlive(foeR) && IsBattlersFirstTurn(foeR) && HasMove(foeR, MOVE_FAKE_OUT))
        return TRUE;
    return FALSE;
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
    bool32 safeTurn = !threatened || PartnerIsRedirecting(battlerAtk);
    u32 hp = gBattleMons[battlerAtk].hp, maxHp = gBattleMons[battlerAtk].maxHP;

    if (!IsDoubleBattle())
        return score;

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
    }

    if (effect == EFFECT_TRICK_ROOM && !targetingPartner)
    {
        bool32 trUp = (gFieldStatuses & STATUS_FIELD_TRICK_ROOM) != 0;
        bool32 weAreSlow = AI_VgcTeamIsSlowerThanFoes(battlerAtk, 2);

        if (!trUp)
        {
            if (weAreSlow)
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

    // ---------------- 4. Fuoco concentrato / scelta del bersaglio ----------------
    if (!isStatus && !targetingPartner && IsBattlerAlive(battlerDef) && hasPartner)
    {
        u32 myDmg = gAiLogicData->simulatedDmg[battlerAtk][battlerDef][moveIndex].median;
        u32 partnerDmg = PartnerDamageTo(battlerAtk, battlerDef);
        u32 defHp = gBattleMons[battlerDef].hp;

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
    }

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

    // Icy Wind / Electroweb ecc. come controllo velocità "morbido" quando siamo appena più lenti.
    if (!isStatus && !targetingPartner && foeLAlive && foeRAlive && IsSpreadMove(AI_GetBattlerMoveTargetType(battlerAtk, move))
     && (move == MOVE_ICY_WIND || move == MOVE_ELECTROWEB || move == MOVE_BULLDOZE)
     && !(gFieldStatuses & STATUS_FIELD_TRICK_ROOM) && !(gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_TAILWIND)
     && AI_VgcTeamBenefitsFromTailwind(battlerAtk, 1))
        ADJUST_SCORE(2);

    return score;
}
