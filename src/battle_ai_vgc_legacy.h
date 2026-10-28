// Regole VGC di prima di M8 (punteggi a mano), tenute SOLO nel ROM dei test per il confronto a specchio del
// self-play (tools/vgc_training: verifica-regole). Si include in fondo a battle_ai_vgc.c (usa le sue funzioni
// statiche); nel gioco non esiste. Non modificare: è il termine di paragone.
// IA contro IA: il lato con gVgcTraining.rulesLegacy usa queste regole e il pianificatore senza filtro né spinta.

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


// La lettura si applica solo se "ci crediamo" questo turno: evita che il giocatore impari
// a sfruttare un'IA che reagisce sempre allo stesso modo.
static bool32 VgcTrustRead(u32 chance)
{
    return chance >= 50 && sVgcReadRoll[AI_GetPerspectiveSide()] < 70;
}


// ---------------------------------------------------------------------
// Valutazione principale
// ---------------------------------------------------------------------
static s32 AI_VgcDoublesLegacy(enum BattlerId battlerAtk, enum BattlerId battlerDef, enum Move move, s32 score)
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
        const struct VgcGameplan *plan = AI_VgcGetPlan(AI_TrainerIdOfBattler(battlerAtk));
        u16 trainerId = AI_TrainerIdOfBattler(battlerAtk);
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

        // Gas Reagente accanto a un compagno con abilità penalizzante (Pigrizia, Partenzalenta...): chi lo tiene
        // spento va tenuto vivo.
        if (effect == EFFECT_PROTECT && !targetingPartner && threatened && hasPartner
         && gAiLogicData->abilities[battlerAtk] == ABILITY_NEUTRALIZING_GAS
         && (VgcRoles(partner) & VGC_ROLE_HINDER_ABILITY))
            ADJUST_SCORE(3);

        // ---------------- Ruoli dal grafo della squadra (valgono per qualunque squadra) ----------------
        // Erano le regole scritte a mano per Savarotto e Patatina: ora le dà il ruolo, non il nome.
        {
            u32 myRoles = VgcRoles(battlerAtk);
            u32 partnerRoles = hasPartner ? VgcRoles(partner) : 0;
            bool32 partnerSlow = (partnerRoles & VGC_ROLE_SLOW) != 0;
            bool32 partnerSweeper = (partnerRoles & (VGC_ROLE_ATK_SWEEPER | VGC_ROLE_SPA_SWEEPER)) != 0;
            bool32 trActive = (gFieldStatuses & STATUS_FIELD_TRICK_ROOM) != 0;
            bool32 twActive = (gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_TAILWIND) != 0;

            // Fake Out al turno 1 per comprare il turno a un compagno che fa setup (es. Espathra).
            if (firstTurn && (partnerRoles & VGC_ROLE_NEEDS_COVER)
             && effect == EFFECT_FIRST_TURN_ONLY && !targetingPartner && IsFlinchGuaranteed(battlerAtk, battlerDef, move))
                ADJUST_SCORE(8);

            // Colpire il compagno per attivargli l'abilità (Beat Up -> Justified...): combo trovata dal grafo.
            if (targetingPartner && !isStatus && hasPartner
             && sVgcTeamGraph.hitPartner[gBattlerPartyIndexes[battlerAtk]] == gBattlerPartyIndexes[partner])
                ADJUST_SCORE(12);

            // Tailwind per un compagno attaccante non lento.
            if (effect == EFFECT_TAILWIND && !twActive && !targetingPartner && (myRoles & VGC_ROLE_TAILWIND)
             && partnerSweeper && !partnerSlow)
                ADJUST_SCORE(5);

            // Baton Passer (accumula boost senza attaccare e li passa a chi in panchina li sfrutta).
            if (myRoles & VGC_ROLE_BATON_PASSER)
            {
                bool32 hasSub = gBattleMons[battlerAtk].volatiles.substitute;
                s8 spatkStage = gBattleMons[battlerAtk].statStages[STAT_SPATK];
                bool32 hasBoosts = AnyUsefulStatIsRaised(battlerAtk)
                                || gBattleMons[battlerAtk].statStages[STAT_SPEED] > DEFAULT_STAT_STAGE;
                bool32 setupMove = isStatus && IsStatRaisingMove(move) && GetMoveTarget(move) == TARGET_USER;

                if (!hasBoosts)
                {
                    if (setupMove)
                        ADJUST_SCORE(firstTurn ? 10 : 8);
                }
                else
                {
                    if (effect == EFFECT_BATON_PASS && CountUsablePartyMons(battlerAtk) > 0)
                        ADJUST_SCORE((hasSub || spatkStage >= DEFAULT_STAT_STAGE + 2 || !safeTurn) ? 10 : 5);
                    if (effect == EFFECT_SUBSTITUTE && !hasSub && hp > gBattleMons[battlerAtk].maxHP / 4)
                        ADJUST_SCORE(threatened ? -4 : 8);
                    if (effect == EFFECT_PROTECT && threatened && !hasSub)
                        ADJUST_SCORE(6);
                }
            }

            // Redirect per un compagno attaccante veloce (es. Indeedee + Armarouge); Trick Room lo rallenterebbe.
            if ((myRoles & VGC_ROLE_REDIRECT) && partnerSweeper && !partnerSlow)
            {
                if (effect == EFFECT_FOLLOW_ME)
                    ADJUST_SCORE(8);
                else if (effect == EFFECT_TRICK_ROOM)
                    ADJUST_SCORE(-10);
            }

            // Trick Room per un compagno lento (es. Indeedee + Copperajah): prima la stanza, poi il redirect.
            if ((myRoles & VGC_ROLE_TRICK_ROOM) && partnerSlow && !trActive)
            {
                if (effect == EFFECT_TRICK_ROOM && !targetingPartner)
                    ADJUST_SCORE(8);
                else if (effect == EFFECT_FOLLOW_ME)
                    ADJUST_SCORE(-8);
            }
            // Il Pokémon lento, mentre il compagno piazza Trick Room, si protegge o si potenzia.
            if ((myRoles & VGC_ROLE_SLOW) && (partnerRoles & VGC_ROLE_TRICK_ROOM) && !trActive && firstTurn)
            {
                if (effect == EFFECT_PROTECT && !targetingPartner)
                    ADJUST_SCORE(4);
                else if (isStatus && IsStatRaisingMove(move) && GetMoveTarget(move) == TARGET_USER)
                    ADJUST_SCORE(3);
            }
        }

        // Forzespansa sotto Campo Psichico: colpisce entrambi a potenza piena.
        if (move == MOVE_EXPANDING_FORCE && !targetingPartner && gFieldTimers.terrain == B_TERRAIN_PSYCHIC
         && AI_IsBattlerGrounded(battlerAtk) && foeLAlive && foeRAlive)
            ADJUST_SCORE(8);

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

    // Recycle senza uno strumento consumato da recuperare (o con lo strumento ancora in mano) non fa niente.
    if (effect == EFFECT_RECYCLE
     && (GetBattlerPartyState(battlerAtk)->usedHeldItem == ITEM_NONE || gBattleMons[battlerAtk].item != ITEM_NONE))
        ADJUST_SCORE(-10);

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
            ADJUST_SCORE(threatened ? -4 : -2); // più veloce, ma il colpo arriva lo stesso: il setup non lo regge
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
        // Il partner deve ancora scegliere ma ha Belly Drum: il redirect è la ragione della coppia.
        else if (HasMoveWithEffect(partner, EFFECT_BELLY_DRUM))
            ADJUST_SCORE(3);
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

    // After You: dà subito il turno al partner lento con una mossa che vale di più a PS pieni
    // (Eruption / Water Spout / Dragon Energy: es. Torkoal sotto Sole).
    if (move == MOVE_AFTER_YOU && hasPartner)
    {
        if (targetingPartner)
        {
            if ((VgcRoles(partner) & VGC_ROLE_SLOW) && HasMoveWithEffect(partner, EFFECT_POWER_BASED_ON_USER_HP)
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

    // Cambiare l'abilità del compagno (Simple Beam, Worry Seed, Skill Swap, Gastro Acid, Entrainment) ha senso
    // solo se la sua abilità lo penalizza (Mycelium Might, Truant, Slow Start...): la combo la trova il grafo
    // della squadra e se conviene lo decide la simulazione. Togliergli un'abilità utile (Guts...) è un errore.
    if (targetingPartner && isStatus)
    {
        enum BattleMoveEffects eff = GetMoveEffect(move);
        if (eff == EFFECT_OVERWRITE_ABILITY || eff == EFFECT_SKILL_SWAP || eff == EFFECT_GASTRO_ACID || eff == EFFECT_ENTRAINMENT)
        {
            enum Ability ab = hasPartner ? gAiLogicData->abilities[partner] : ABILITY_NONE;
            if (ab != ABILITY_MYCELIUM_MIGHT && ab != ABILITY_TRUANT && ab != ABILITY_SLOW_START
             && ab != ABILITY_DEFEATIST && ab != ABILITY_KLUTZ && ab != ABILITY_STALL)
                ADJUST_SCORE(-20);
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

