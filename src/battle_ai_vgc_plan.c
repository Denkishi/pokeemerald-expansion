// Pianificatore del turno per i doppi VGC (AI_FLAG_VGC_DOUBLES).
//
// Le regole scritte (battle_ai_vgc.c) dicono cosa è "sensato"; ma nessuna lista di regole copre ogni
// situazione. Qui l'IA SIMULA il turno: per ognuna delle sue azioni candidate (mossa + bersaglio,
// prese tra le meglio valutate dalle regole) e per ognuna delle azioni più probabili del giocatore
// (stimate da ciò che è lecito sapere: mosse viste, strumenti probabili, Protect probabile), applica
// ordine di velocità, Fake Out, Protect, Segui Me, Aiuto, danni, sonno, controllo velocità, e valuta
// la POSIZIONE che ne esce: PS e KO di entrambe le parti, chi si muove per primo al turno dopo, chi
// dorme, chi è potenziato, chi rischia il KO. Sceglie l'azione con la migliore media pesata (70%) e
// caso peggiore (30%): così la decisione nasce dallo stato della partita, non da una strada fissa.
//
// Nessun imbroglio: le mosse del giocatore sono quelle note (GetMovesArray), i danni quelli già
// stimati dall'IA con strumenti/abilità noti o assunti, il Protect ha la probabilità stimata.
#include "global.h"
#include "battle.h"
#include "battle_ai_main.h"
#include "battle_ai_util.h"
#include "battle_ai_vgc.h"
#include "battle_ai_scout.h"
#include "battle_main.h"
#include "battle_util.h"
#include "move.h"
#include "random.h"
#include "constants/abilities.h"
#include "constants/battle_ai.h"
#include "constants/battle_move_effects.h"
#include "constants/moves.h"

#define PLAN_MAX_OURS   8   // azioni candidate nostre
#define PLAN_FOE_PER    3   // azioni per avversario
#define PLAN_MAX_FOES   (PLAN_FOE_PER * PLAN_FOE_PER)
#define PLAN_OVERRIDE_MARGIN 12  // quanto deve essere migliore (in punti di posizione) per cambiare scelta

enum PlanKind
{
    PLAN_ACT_MOVE,
    PLAN_ACT_PROTECT,
    PLAN_ACT_NONE,      // non agisce (es. sconosciuto / passa)
};

struct PlanAction
{
    u8 kind;
    u8 moveIndex;
    enum Move move;
    enum BattlerId user;
    enum BattlerId target;
    s32 weight;
};

struct PlanState
{
    s32 hp[MAX_BATTLERS_COUNT];
    u8 alive[MAX_BATTLERS_COUNT];
    u8 protecting[MAX_BATTLERS_COUNT];
    u8 wideGuard[MAX_BATTLERS_COUNT];     // per lato (indice battler)
    u8 flinched[MAX_BATTLERS_COUNT];
    u8 asleep[MAX_BATTLERS_COUNT];
    u8 redirect[MAX_BATTLERS_COUNT];      // Segui Me attivo su questo battler (riceve gli attacchi singoli dei nemici)
    u8 helpingHand[MAX_BATTLERS_COUNT];
    s8 speedStage[MAX_BATTLERS_COUNT];
    s8 boost[MAX_BATTLERS_COUNT];         // stadi offensivi guadagnati
    u8 trickRoom;
    u8 tailwind[2];                       // per lato
};

static s32 ActionPriority(const struct PlanAction *a)
{
    if (a->kind == PLAN_ACT_PROTECT)
        return 4;
    if (a->kind == PLAN_ACT_NONE)
        return -8;
    return GetBattleMovePriority(a->user, gAiLogicData->abilities[a->user], a->move);
}

static u32 EffSpeed(const struct PlanState *st, enum BattlerId b)
{
    u32 s = gAiLogicData->speedStats[b];
    s32 stage = st->speedStage[b];

    if (stage < 0)
        s = s * 2 / (2 - stage);
    else if (stage > 0)
        s = s * (2 + stage) / 2;
    if (st->tailwind[GetBattlerSide(b)])
        s *= 2;
    return s;
}

// TRUE se a si muove prima di b (pessimista per l'IA nei pareggi).
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
        return IsOnPlayerSide(a->user); // pareggio: si assume che il giocatore sia più veloce
    return sa > sb;
}

static s32 DamageOf(enum BattlerId atk, enum BattlerId def, u32 moveIndex, bool32 helping)
{
    s32 d = gAiLogicData->simulatedDmg[atk][def][moveIndex].median;
    if (helping)
        d = d * 3 / 2;
    return d;
}

// Applica una singola azione allo stato.
static void ApplyAction(struct PlanState *st, const struct PlanAction *a)
{
    enum BattlerId u = a->user;
    enum BattleMoveEffects effect;
    enum MoveTarget tt;

    if (!st->alive[u] || st->flinched[u] || st->asleep[u])
        return;
    if (a->kind == PLAN_ACT_NONE)
        return;
    if (a->kind == PLAN_ACT_PROTECT)
    {
        st->protecting[u] = TRUE;
        return;
    }

    effect = GetMoveEffect(a->move);
    tt = AI_GetBattlerMoveTargetType(u, a->move);

    switch (effect)
    {
    case EFFECT_PROTECT:
        if (GetMoveProtectMethod(a->move) == PROTECT_WIDE_GUARD)
            st->wideGuard[u] = st->wideGuard[GetPartnerBattler(u)] = TRUE;
        else if (GetMoveProtectMethod(a->move) != PROTECT_QUICK_GUARD && GetMoveProtectMethod(a->move) != PROTECT_CRAFTY_SHIELD)
            st->protecting[u] = TRUE;
        return;
    case EFFECT_FOLLOW_ME:
        st->redirect[u] = TRUE;
        return;
    case EFFECT_HELPING_HAND:
        st->helpingHand[GetPartnerBattler(u)] = TRUE;
        return;
    case EFFECT_TRICK_ROOM:
        st->trickRoom = !st->trickRoom;
        return;
    case EFFECT_TAILWIND:
        st->tailwind[GetBattlerSide(u)] = TRUE;
        return;
    case EFFECT_BELLY_DRUM:
        st->boost[u] += 6;
        st->hp[u] -= gBattleMons[u].maxHP / 2;
        if (st->hp[u] <= 0)
            st->hp[u] = 1;
        return;
    default:
        break;
    }

    if (IsBattleMoveStatus(a->move))
    {
        if (IsStatRaisingMove(a->move) && tt == TARGET_USER)
        {
            st->boost[u] += 2;
        }
        else if (GetMoveNonVolatileStatus(a->move) == MOVE_EFFECT_SLEEP && st->alive[a->target] && !st->protecting[a->target]
              && AI_CanPutToSleep(u, a->target, gAiLogicData->abilities[a->target], a->move, MOVE_NONE))
        {
            st->asleep[a->target] = TRUE;
        }
        else if (a->move == MOVE_ICY_WIND || a->move == MOVE_ELECTROWEB || a->move == MOVE_BULLDOZE)
        {
            ; // gestite sotto come mosse d'attacco ad area (hanno potenza)
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
            if (st->alive[fl]) targets[n++] = fl;
            if (st->alive[fr]) targets[n++] = fr;
        }
        else
        {
            enum BattlerId t = a->target;
            // Segui Me / Polverabbia avversario: l'attacco singolo va su chi reindirizza.
            if (!IsBattlerAlly(u, t))
            {
                enum BattlerId fl = GetBattlerLeftFoe(u), fr = GetBattlerRightFoe(u);
                if (st->alive[fl] && st->redirect[fl]) t = fl;
                else if (st->alive[fr] && st->redirect[fr]) t = fr;
            }
            if (!st->alive[t])
            {
                // Bersaglio caduto: la mossa va sull'altro avversario.
                enum BattlerId other = GetPartnerBattler(t);
                if (!IsBattlerAlly(u, t) && st->alive[other])
                    t = other;
                else
                    return;
            }
            targets[n++] = t;
        }

        for (i = 0; i < n; i++)
        {
            enum BattlerId t = targets[i];
            s32 dmg;

            if (st->protecting[t] || (spread && st->wideGuard[t]))
                continue;
            if (gAiLogicData->effectiveness[u][t][a->moveIndex] == UQ_4_12(0.0))
                continue;
            dmg = DamageOf(u, t, a->moveIndex, st->helpingHand[u]);
            if (st->boost[u] > 0)
                dmg = dmg * (2 + st->boost[u]) / 2;
            // Fake Out: tentennamento (salvo Forza Interiore / già agito).
            if (effect == EFFECT_FIRST_TURN_ONLY && IsBattlersFirstTurn(u) && gAiLogicData->abilities[t] != ABILITY_INNER_FOCUS)
                st->flinched[t] = TRUE;
            if (a->move == MOVE_ICY_WIND || a->move == MOVE_ELECTROWEB || a->move == MOVE_BULLDOZE)
                st->speedStage[t]--;
            st->hp[t] -= dmg;
            if (st->hp[t] <= 0)
            {
                st->hp[t] = 0;
                st->alive[t] = FALSE;
            }
        }
        if (effect == EFFECT_FINAL_GAMBIT)
        {
            st->hp[u] = 0;
            st->alive[u] = FALSE;
        }
    }
}

// Valore della posizione per l'IA.
static s32 Evaluate(const struct PlanState *st, enum BattlerId me)
{
    enum BattlerId b;
    s32 v = 0;
    u32 ourFirst = 0, theirFirst = 0;

    for (b = 0; b < gBattlersCount; b++)
    {
        s32 pct, side = IsOnPlayerSide(b) ? -1 : 1;

        if (gBattleMons[b].species == SPECIES_NONE)
            continue;
        if (!st->alive[b])
        {
            v -= side * 160;
            continue;
        }
        pct = st->hp[b] * 100 / max(gBattleMons[b].maxHP, 1);
        v += side * (pct + 40);
        if (st->asleep[b])
            v -= side * 60;
        if (st->boost[b] > 0)
            v += side * 25 * min(st->boost[b], 4);
    }

    // Chi si muove per primo al turno dopo (controllo velocità): per ogni nostro vivo, quanti nemici vivi supera.
    for (b = 0; b < gBattlersCount; b++)
    {
        enum BattlerId o;
        if (!st->alive[b] || gBattleMons[b].species == SPECIES_NONE)
            continue;
        for (o = 0; o < gBattlersCount; o++)
        {
            bool32 faster;
            if (o == b || !st->alive[o] || IsBattlerAlly(b, o) || gBattleMons[o].species == SPECIES_NONE)
                continue;
            faster = EffSpeed(st, b) > EffSpeed(st, o);
            if (st->trickRoom)
                faster = !faster;
            if (faster)
            {
                if (IsOnPlayerSide(b)) theirFirst++; else ourFirst++;
            }
        }
    }
    v += 12 * (s32)ourFirst - 12 * (s32)theirFirst;

    // Minaccia al turno dopo: un nostro Pokémon che un avversario (più veloce) può mandare KO.
    for (b = 0; b < gBattlersCount; b++)
    {
        enum BattlerId o;
        if (!st->alive[b] || IsOnPlayerSide(b) || gBattleMons[b].species == SPECIES_NONE)
            continue;
        for (o = 0; o < gBattlersCount; o++)
        {
            enum Move *moves;
            u32 i;
            bool32 faster;
            if (!st->alive[o] || !IsOnPlayerSide(o) || st->asleep[o])
                continue;
            moves = GetMovesArray(o);
            faster = EffSpeed(st, o) > EffSpeed(st, b);
            if (st->trickRoom)
                faster = !faster;
            for (i = 0; i < MAX_MON_MOVES; i++)
            {
                if (moves[i] == MOVE_NONE || moves[i] == MOVE_UNAVAILABLE || IsBattleMoveStatus(moves[i]))
                    continue;
                if (gAiLogicData->simulatedDmg[o][b][i].median >= st->hp[b])
                {
                    v -= faster ? 45 : 20;
                    break;
                }
            }
        }
    }
    (void)me;
    return v;
}

// --- Azioni probabili del giocatore -------------------------------------------------------
static u32 BuildFoeActions(enum BattlerId foe, struct PlanAction *out)
{
    enum Move *moves = GetMovesArray(foe);
    struct PlanAction cand[12];
    u32 n = 0, i, k, protectP;
    enum BattlerId me1 = GetBattlerLeftFoe(foe), me2 = GetBattlerRightFoe(foe);
    enum BattlerId targets[2] = { me1, me2 };
    bool32 anyKnown = FALSE;

    for (i = 0; i < MAX_MON_MOVES && n < 10; i++)
    {
        enum Move m = moves[i];
        enum BattleMoveEffects eff;
        if (m == MOVE_NONE || m == MOVE_UNAVAILABLE)
            continue;
        anyKnown = TRUE;
        eff = GetMoveEffect(m);
        if (eff == EFFECT_PROTECT)
            continue; // gestito sotto con la probabilità
        if (IsBattleMoveStatus(m))
        {
            s32 w = 0;
            if (eff == EFFECT_TRICK_ROOM && !(gFieldStatuses & STATUS_FIELD_TRICK_ROOM)) w = 70;
            else if (eff == EFFECT_TAILWIND && !(gSideStatuses[GetBattlerSide(foe)] & SIDE_STATUS_TAILWIND)) w = 70;
            else if (eff == EFFECT_FOLLOW_ME) w = 45;
            else if (eff == EFFECT_HELPING_HAND) w = 30;
            else if (GetMoveNonVolatileStatus(m) == MOVE_EFFECT_SLEEP) w = 60;
            else if (IsStatRaisingMove(m)) w = 35;
            if (w == 0)
                continue;
            cand[n].kind = PLAN_ACT_MOVE; cand[n].moveIndex = i; cand[n].move = m; cand[n].user = foe;
            cand[n].target = (GetMoveNonVolatileStatus(m) == MOVE_EFFECT_SLEEP) ? me1 : foe; cand[n].weight = w; n++;
            if (GetMoveNonVolatileStatus(m) == MOVE_EFFECT_SLEEP && IsBattlerAlive(me2) && n < 10)
            {
                cand[n] = cand[n - 1]; cand[n].target = me2; n++;
            }
            continue;
        }
        if (IsSpreadMove(AI_GetBattlerMoveTargetType(foe, m)))
        {
            s32 w = 0;
            for (k = 0; k < 2; k++)
                if (IsBattlerAlive(targets[k]))
                    w += gAiLogicData->simulatedDmg[foe][targets[k]][i].median * 100 / max(gBattleMons[targets[k]].maxHP, 1);
            cand[n].kind = PLAN_ACT_MOVE; cand[n].moveIndex = i; cand[n].move = m; cand[n].user = foe;
            cand[n].target = me1; cand[n].weight = w + 10; n++;
            continue;
        }
        for (k = 0; k < 2 && n < 10; k++)
        {
            enum BattlerId t = targets[k];
            s32 w;
            if (!IsBattlerAlive(t) || gAiLogicData->effectiveness[foe][t][i] == UQ_4_12(0.0))
                continue;
            w = gAiLogicData->simulatedDmg[foe][t][i].median * 100 / max(gBattleMons[t].maxHP, 1);
            if (gAiLogicData->simulatedDmg[foe][t][i].median >= gBattleMons[t].hp)
                w += 80; // il giocatore cerca il KO
            if (eff == EFFECT_FIRST_TURN_ONLY && IsBattlersFirstTurn(foe))
                w += 60;
            cand[n].kind = PLAN_ACT_MOVE; cand[n].moveIndex = i; cand[n].move = m; cand[n].user = foe;
            cand[n].target = t; cand[n].weight = w; n++;
        }
    }
    // Protect con la probabilità stimata (visto o no).
    protectP = AI_VgcProtectChance(foe);
    if (protectP > 0 && n < 12)
    {
        cand[n].kind = PLAN_ACT_PROTECT; cand[n].moveIndex = 0; cand[n].move = MOVE_PROTECT; cand[n].user = foe;
        cand[n].target = foe; cand[n].weight = (s32)protectP; n++;
    }
    if (n == 0 || !anyKnown)
    {
        // Niente di noto: un attacco "generico" (STAB stimato è già nelle mosse assunte, se attive) o nulla.
        cand[n].kind = PLAN_ACT_NONE; cand[n].moveIndex = 0; cand[n].move = MOVE_NONE; cand[n].user = foe;
        cand[n].target = foe; cand[n].weight = 50; n++;
    }
    // Le migliori PLAN_FOE_PER per peso.
    {
        u32 outN = 0;
        while (outN < PLAN_FOE_PER && n > 0)
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

// --- Azioni candidate nostre --------------------------------------------------------------
static u32 BuildOurActions(enum BattlerId me, struct PlanAction *out, u32 chosenMove, enum BattlerId chosenTarget)
{
    s32 best = INT32_MIN;
    u32 n = 0, i;
    enum BattlerId t;

    for (t = 0; t < gBattlersCount; t++)
        for (i = 0; i < MAX_MON_MOVES; i++)
            if (gBattleMons[me].moves[i] != MOVE_NONE && t != me && gAiBattleData->finalScore[me][t][i] > best)
                best = gAiBattleData->finalScore[me][t][i];

    for (t = 0; t < gBattlersCount; t++)
    {
        if (t == me || !IsBattlerAlive(t))
            continue;
        for (i = 0; i < MAX_MON_MOVES && n < PLAN_MAX_OURS; i++)
        {
            enum Move m = gBattleMons[me].moves[i];
            s32 sc = gAiBattleData->finalScore[me][t][i];
            if (m == MOVE_NONE || gBattleMons[me].pp[i] == 0)
                continue;
            if (sc <= 0 || sc < best - 14)
                continue;
            if (!CanTargetBattler(me, t, m))
                continue;
            // Niente doppioni per le mosse che non dipendono dal bersaglio.
            if (AI_GetBattlerMoveTargetType(me, m) != TARGET_SELECTED && AI_GetBattlerMoveTargetType(me, m) != TARGET_SMART)
            {
                u32 k; bool32 dup = FALSE;
                for (k = 0; k < n; k++)
                    if (out[k].moveIndex == i)
                        dup = TRUE;
                if (dup)
                    continue;
            }
            out[n].kind = PLAN_ACT_MOVE; out[n].moveIndex = i; out[n].move = m; out[n].user = me; out[n].target = t;
            out[n].weight = sc; n++;
        }
    }
    // La scelta delle regole è sempre tra le candidate.
    {
        u32 k; bool32 present = FALSE;
        for (k = 0; k < n; k++)
            if (out[k].moveIndex == chosenMove && out[k].target == chosenTarget)
                present = TRUE;
        if (!present && n < PLAN_MAX_OURS && gBattleMons[me].moves[chosenMove] != MOVE_NONE)
        {
            out[n].kind = PLAN_ACT_MOVE; out[n].moveIndex = chosenMove; out[n].move = gBattleMons[me].moves[chosenMove];
            out[n].user = me; out[n].target = chosenTarget; out[n].weight = best; n++;
        }
    }
    return n;
}

// Azione del partner: quella già decisa, altrimenti la sua mossa più dannosa.
static void PartnerAction(enum BattlerId me, struct PlanAction *out)
{
    enum BattlerId p = GetPartnerBattler(me);
    u32 i;
    enum BattlerId t;
    s32 best = -1;

    out->kind = PLAN_ACT_NONE; out->user = p; out->target = p; out->move = MOVE_NONE; out->moveIndex = 0; out->weight = 0;
    if (p == me || !IsBattlerAlive(p))
        return;
    if ((gAiLogicData->battlerMovesScored & (1u << p)) && !(gAiLogicData->shouldSwitch & (1u << p)))
    {
        u32 idx = gAiBattleData->chosenMoveIndex[p];
        if (idx < MAX_MON_MOVES && gBattleMons[p].moves[idx] != MOVE_NONE && gBattleMons[p].moves[idx] != MOVE_UNAVAILABLE)
        {
            out->kind = PLAN_ACT_MOVE; out->moveIndex = idx; out->move = gBattleMons[p].moves[idx]; out->target = gAiBattleData->chosenTarget[p];
        }
        return;
    }
    for (t = 0; t < gBattlersCount; t++)
    {
        if (IsBattlerAlly(p, t) || !IsBattlerAlive(t))
            continue;
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            enum Move m = gBattleMons[p].moves[i];
            s32 d;
            if (m == MOVE_NONE || IsBattleMoveStatus(m) || gBattleMons[p].pp[i] == 0)
                continue;
            d = gAiLogicData->simulatedDmg[p][t][i].median;
            if (d > best)
            {
                best = d;
                out->kind = PLAN_ACT_MOVE; out->moveIndex = i; out->move = m; out->target = t;
            }
        }
    }
}

static void InitState(struct PlanState *st)
{
    enum BattlerId b;

    memset(st, 0, sizeof(*st));
    for (b = 0; b < gBattlersCount; b++)
    {
        st->hp[b] = gBattleMons[b].hp;
        st->alive[b] = IsBattlerAlive(b);
        st->asleep[b] = (gBattleMons[b].status1 & STATUS1_SLEEP) != 0;
    }
    st->trickRoom = (gFieldStatuses & STATUS_FIELD_TRICK_ROOM) != 0;
    st->tailwind[B_SIDE_PLAYER] = (gSideStatuses[B_SIDE_PLAYER] & SIDE_STATUS_TAILWIND) != 0;
    st->tailwind[B_SIDE_OPPONENT] = (gSideStatuses[B_SIDE_OPPONENT] & SIDE_STATUS_TAILWIND) != 0;
}

// Simula un turno con le 4 azioni e restituisce il valore della posizione.
static s32 SimulateTurn(enum BattlerId me, const struct PlanAction *mine, const struct PlanAction *partner,
                        const struct PlanAction *foeA, const struct PlanAction *foeB)
{
    struct PlanState st;
    const struct PlanAction *acts[4];
    u32 n = 0, i, j;

    InitState(&st);
    acts[n++] = mine;
    if (partner->kind != PLAN_ACT_NONE) acts[n++] = partner;
    if (foeA != NULL && foeA->kind != PLAN_ACT_NONE) acts[n++] = foeA;
    if (foeB != NULL && foeB->kind != PLAN_ACT_NONE) acts[n++] = foeB;

    // Ordine: priorità, poi velocità (con Distorsione) — semplice selection sort.
    for (i = 0; i < n; i++)
    {
        u32 first = i;
        for (j = i + 1; j < n; j++)
            if (MovesFirst(&st, acts[j], acts[first]))
                first = j;
        if (first != i)
        {
            const struct PlanAction *t = acts[i]; acts[i] = acts[first]; acts[first] = t;
        }
        ApplyAction(&st, acts[i]);
    }
    return Evaluate(&st, me);
}

bool32 AI_VgcPlanTurn(enum BattlerId me, u8 *moveIndex, enum BattlerId *target)
{
    struct PlanAction ours[PLAN_MAX_OURS], partner, foeA[PLAN_FOE_PER], foeB[PLAN_FOE_PER];
    struct PlanAction none = { PLAN_ACT_NONE, 0, MOVE_NONE, 0, 0, 100 };
    u32 nOurs, nA = 0, nB = 0, i, a, b;
    enum BattlerId fL = GetBattlerLeftFoe(me), fR = GetBattlerRightFoe(me);
    s32 bestEv = INT32_MIN, chosenEv = INT32_MIN;
    u32 bestIdx = 0;

    if (!IsDoubleBattle())
        return FALSE;
    nOurs = BuildOurActions(me, ours, *moveIndex, *target);
    if (nOurs < 2)
        return FALSE;
    PartnerAction(me, &partner);
    if (IsBattlerAlive(fL)) nA = BuildFoeActions(fL, foeA);
    if (IsBattlerAlive(fR)) nB = BuildFoeActions(fR, foeB);
    if (nA == 0) { foeA[0] = none; foeA[0].user = fL; nA = 1; }
    if (nB == 0) { foeB[0] = none; foeB[0].user = fR; nB = 1; }

    for (i = 0; i < nOurs; i++)
    {
        s32 sum = 0, wsum = 0, worst = INT32_MAX, ev;
        for (a = 0; a < nA; a++)
        {
            for (b = 0; b < nB; b++)
            {
                s32 w = max(foeA[a].weight, 1) * max(foeB[b].weight, 1);
                s32 v = SimulateTurn(me, &ours[i], &partner, &foeA[a], &foeB[b]);
                sum += v * (w / 16 + 1);
                wsum += (w / 16 + 1);
                if (v < worst)
                    worst = v;
            }
        }
        ev = (7 * (sum / max(wsum, 1)) + 3 * worst) / 10;
        if (ours[i].moveIndex == *moveIndex && ours[i].target == *target)
            chosenEv = ev;
        if (ev > bestEv)
        {
            bestEv = ev;
            bestIdx = i;
        }
    }
    // Si cambia scelta solo se la posizione che ne esce è nettamente migliore.
    if (chosenEv == INT32_MIN || bestEv >= chosenEv + PLAN_OVERRIDE_MARGIN)
    {
        if (ours[bestIdx].moveIndex == *moveIndex && ours[bestIdx].target == *target)
            return FALSE;
        *moveIndex = ours[bestIdx].moveIndex;
        *target = ours[bestIdx].target;
        return TRUE;
    }
    return FALSE;
}
