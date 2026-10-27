// Libro dei piani VGC (M9) e agganci dell'esplorazione del self-play. Vedi include/battle_ai_vgc_book.h e
// docs/ia_vgc/training.md. I dati (src/data/vgc_plan_book.h) sono const in ROM; in RAM solo il piano scelto.
#include "global.h"
#include "battle.h"
#include "battle_ai_vgc_book.h"
#include "pokemon.h"
#include "random.h"
#include "constants/moves.h"
#include "constants/species.h"

#include "data/vgc_plan_book.h"

#define BOOK_MIN_GAMES          3   // un piano con meno partite non si gioca dal libro
#define BOOK_CLASS_MIN_GAMES    10  // partite contro la classe avversaria per usarne le righe (altrimenti "qualsiasi")
#define BOOK_NEAR_BEST_PCT      12  // piani giocabili: tasso stimato entro 12 punti dal migliore
#define BOOK_MIN_RATE_PCT       20  // e almeno il 20% (una squadra debole gioca comunque i suoi piani migliori)
#define BOOK_GROUP_BONUS        400 // team preview: il gruppo di 4 del piano scelto
#define BOOK_GROUP_LEADS_BONUS  150 // un gruppo che contiene almeno le lead del piano
#define BOOK_LEAD_BONUS         400 // la coppia di lead del piano scelto
#define BOOK_MOVE_BONUS         30  // turni 1/2: per ciascuna delle due mosse del piano (punti della valutazione)
#define EXPLORE_PREVIEW_NOISE   500 // team preview esplorativa: rumore sui punteggi di gruppi e lead
#define EXPLORE_PAIR_MARGIN     60  // turni 1/2 esplorativi: coppie entro questo margine dalla migliore
#define BOOK_MAX_EXCLUDED       3

static EWRAM_DATA u16 sBookPick[NUM_BATTLE_SIDES] = {0};
static EWRAM_DATA u16 sBookExcluded[BOOK_MAX_EXCLUDED][2] = {0};
static EWRAM_DATA u8 sBookExcludedCount = 0;
static EWRAM_DATA bool8 sBookReady = FALSE;

#if TESTING
struct VgcTrainingConfig gVgcTraining = {0};
#define BOOK_OFF(side)      ((gVgcTraining.bookOff >> (side)) & 1)
#define PREVIEW_SIDE()      (gVgcTraining.previewSide)
#define PREVIEW_EXPLORE()   (gVgcTraining.previewExplore)
#define EXPLORE_EPS(side)   (gVgcTraining.exploreEps[side])
#else
#define BOOK_OFF(side)      FALSE
#define PREVIEW_SIDE()      B_SIDE_OPPONENT
#define PREVIEW_EXPLORE()   FALSE
#define EXPLORE_EPS(side)   0
#endif

static u16 BaseSpecies(u16 species)
{
    return species == SPECIES_NONE ? SPECIES_NONE : GET_BASE_SPECIES_ID(species);
}

// Hash della composizione (ordine indifferente): specie ordinate, poi h = h * 31 + specie (16 bit).
u32 VgcBook_TeamHash(const u16 *species, u32 n)
{
    u16 s[PARTY_SIZE];
    u32 i, j, h = 0;

    n = min(n, (u32)PARTY_SIZE);
    for (i = 0; i < n; i++)
        s[i] = species[i];
    for (i = 1; i < n; i++)
    {
        u16 v = s[i];
        for (j = i; j > 0 && s[j - 1] > v; j--)
            s[j] = s[j - 1];
        s[j] = v;
    }
    for (i = 0; i < n; i++)
        h = (h * 31 + s[i]) & 0xFFFF;
    return h;
}

// Classe della squadra avversaria da ciò che si vede in team preview (solo specie): velocità base media
// (0 lenta < 70, 1 media, 2 veloce >= 95) + 3 se ha almeno due specie molto lente (<= 50, squadra da Trick Room).
u32 VgcBook_OppClass(const u16 *species, u32 n)
{
    u32 i, sum = 0, slow = 0, cls;
    if (n == 0)
        return 0;
    for (i = 0; i < n; i++)
    {
        u32 spe = gSpeciesInfo[species[i]].baseSpeed;
        sum += spe;
        if (spe <= 50)
            slow++;
    }
    sum /= n;
    cls = (sum < 70) ? 0 : (sum < 95 ? 1 : 2);
    return cls + (slow >= 2 ? 3 : 0);
}

u32 AI_VgcBookCount(void)
{
    return ARRAY_COUNT(sVgcPlanBook);
}

const struct VgcBookPlan *AI_VgcBookEntry(u32 idx)
{
    return idx < ARRAY_COUNT(sVgcPlanBook) ? &sVgcPlanBook[idx] : NULL;
}

// Inizio di ogni lotta (battle_setup.c, prima dell'eventuale team preview): nessun piano rimasto da prima.
void AI_VgcBookReset(void)
{
    sBookPick[0] = sBookPick[1] = VGC_BOOK_NO_PICK;
    sBookReady = TRUE;
}

u32 AI_VgcBookPick(u32 side)
{
    if (!sBookReady)
        return VGC_BOOK_NO_PICK;
    return sBookPick[side];
}

void AI_VgcBookSetPick(u32 side, u32 pick)
{
    if (!sBookReady)
        AI_VgcBookReset();
    sBookPick[side] = pick;
}

void AI_VgcBookExcludeLeads(u16 speciesA, u16 speciesB)
{
    if (sBookExcludedCount < BOOK_MAX_EXCLUDED)
    {
        sBookExcluded[sBookExcludedCount][0] = speciesA;
        sBookExcluded[sBookExcludedCount][1] = speciesB;
        sBookExcludedCount++;
    }
}

static bool32 SamePair(u16 a, u16 b, u16 x, u16 y)
{
    return (a == x && b == y) || (a == y && b == x);
}

static bool32 LeadsExcluded(const struct VgcBookPlan *p)
{
    u32 i;
    for (i = 0; i < sBookExcludedCount; i++)
        if (SamePair(p->lead[0], p->lead[1], sBookExcluded[i][0], sBookExcluded[i][1]))
            return TRUE;
    return FALSE;
}

// Tasso di vittoria stimato con prudenza (Laplace): (vittorie + 1) / (partite + 2), in %.
static u32 PlanRatePct(const struct VgcBookPlan *p)
{
    return (p->wins * 100 + 100) / (p->games + 2);
}

static s32 ExploreNoise(void)
{
    if (!PREVIEW_EXPLORE())
        return 0;
    return (s32)(Random() % (2 * EXPLORE_PREVIEW_NOISE + 1)) - EXPLORE_PREVIEW_NOISE;
}

// Strategia mista: tra i piani del libro per questa composizione e questa classe di avversari, quelli buoni (tasso
// stimato vicino al migliore) si scelgono a caso con peso crescente con il tasso. Un avversario umano non può
// prevedere la lead dal libro; le gare perse della serie (Bo3) escludono le loro lead.
static void BookBegin(struct Pokemon *ours, const u32 *ourSlots, u32 nOurs, struct Pokemon *theirs, const u32 *theirSlots, u32 nTheirs)
{
    u16 sp[PARTY_SIZE];
    u32 i, hash, cls, side = PREVIEW_SIDE();
    u32 classGames = 0, bestRate = 0, total = 0;
    u8 useClass;
    u16 cand[16];
    u16 candW[16];
    u32 nCand = 0;

    if (!sBookReady)
        AI_VgcBookReset();
    sBookPick[side] = VGC_BOOK_NO_PICK;
#if TESTING
    {
        // Nei test normali niente libro né caso: risultati deterministici. Solo il self-play (IA contro IA) lo usa.
        extern bool8 gTestAiVsAiBattle;
        if (!gTestAiVsAiBattle)
            return;
    }
    gVgcTraining.previewExplore = EXPLORE_EPS(side) && (Random() % 100) < EXPLORE_EPS(side);
#endif
    if (BOOK_OFF(side) || PREVIEW_EXPLORE())
        return;

    for (i = 0; i < nOurs; i++)
        sp[i] = GetMonData(&ours[ourSlots[i]], MON_DATA_SPECIES);
    hash = VgcBook_TeamHash(sp, nOurs);
    for (i = 0; i < nTheirs; i++)
        sp[i] = GetMonData(&theirs[theirSlots[i]], MON_DATA_SPECIES);
    cls = VgcBook_OppClass(sp, nTheirs);

    for (i = 0; i < ARRAY_COUNT(sVgcPlanBook); i++)
        if (sVgcPlanBook[i].teamHash == hash && sVgcPlanBook[i].oppClass == cls)
            classGames += sVgcPlanBook[i].games;
    useClass = (classGames >= BOOK_CLASS_MIN_GAMES) ? cls : VGC_BOOK_ANY_CLASS;

    for (i = 0; i < ARRAY_COUNT(sVgcPlanBook); i++)
    {
        const struct VgcBookPlan *p = &sVgcPlanBook[i];
        if (p->teamHash != hash || p->oppClass != useClass || p->games < BOOK_MIN_GAMES || LeadsExcluded(p))
            continue;
        bestRate = max(bestRate, PlanRatePct(p));
    }
    if (bestRate < BOOK_MIN_RATE_PCT)
        return;
    for (i = 0; i < ARRAY_COUNT(sVgcPlanBook) && nCand < ARRAY_COUNT(cand); i++)
    {
        const struct VgcBookPlan *p = &sVgcPlanBook[i];
        u32 rate;
        if (p->teamHash != hash || p->oppClass != useClass || p->games < BOOK_MIN_GAMES || LeadsExcluded(p))
            continue;
        rate = PlanRatePct(p);
        if (rate + BOOK_NEAR_BEST_PCT < bestRate || rate < BOOK_MIN_RATE_PCT)
            continue;
        cand[nCand] = i;
        // Peso: (tasso - migliore + 13)^2, così il migliore è il più probabile ma non l'unico.
        candW[nCand] = (rate + BOOK_NEAR_BEST_PCT + 1 - bestRate) * (rate + BOOK_NEAR_BEST_PCT + 1 - bestRate);
        total += candW[nCand];
        nCand++;
    }
    if (nCand == 0 || total == 0)
        return;
    {
        u32 r = Random() % total;
        for (i = 0; i < nCand; i++)
        {
            if (r < candW[i])
                break;
            r -= candW[i];
        }
        sBookPick[side] = cand[min(i, nCand - 1)];
    }
}

void AI_VgcBookBegin(struct Pokemon *ours, const u32 *ourSlots, u32 nOurs, struct Pokemon *theirs, const u32 *theirSlots, u32 nTheirs)
{
    BookBegin(ours, ourSlots, nOurs, theirs, theirSlots, nTheirs);
    sBookExcludedCount = 0; // le esclusioni (Bo3) valgono per questa team preview
}

static const struct VgcBookPlan *PreviewPlan(void)
{
    u32 side = PREVIEW_SIDE();
    if (!sBookReady || sBookPick[side] == VGC_BOOK_NO_PICK || sBookPick[side] >= ARRAY_COUNT(sVgcPlanBook))
        return NULL;
    return &sVgcPlanBook[sBookPick[side]];
}

s32 AI_VgcBookGroupBonus(struct Pokemon *ours, const u32 *ourSlots, u32 nOurs, u32 mask)
{
    const struct VgcBookPlan *p = PreviewPlan();
    s32 bonus = ExploreNoise();
    u32 i, k, hits = 0, leadHits = 0, need = 0;

    if (p == NULL)
        return bonus;
    for (k = 0; k < 2; k++)
    {
        if (p->lead[k] != SPECIES_NONE)
            need++;
        if (p->back[k] != SPECIES_NONE)
            need++;
    }
    for (i = 0; i < nOurs; i++)
    {
        u16 s;
        if (!(mask & (1u << i)))
            continue;
        s = GetMonData(&ours[ourSlots[i]], MON_DATA_SPECIES);
        for (k = 0; k < 2; k++)
        {
            if (s == p->lead[k])
            {
                hits++;
                leadHits++;
            }
            else if (s == p->back[k])
            {
                hits++;
            }
        }
    }
    if (hits == need)
        bonus += BOOK_GROUP_BONUS;
    else if (leadHits == 2)
        bonus += BOOK_GROUP_LEADS_BONUS;
    return bonus;
}

s32 AI_VgcBookLeadBonus(u16 speciesA, u16 speciesB)
{
    const struct VgcBookPlan *p = PreviewPlan();
    s32 bonus = ExploreNoise();
    if (p != NULL && SamePair(speciesA, speciesB, p->lead[0], p->lead[1]))
        bonus += BOOK_LEAD_BONUS;
    return bonus;
}

// Mossa del piano per il Pokémon in campo `battler` al turno corrente (1 o 2), MOVE_NONE se nessuna.
static u32 PlanMoveFor(const struct VgcBookPlan *p, enum BattlerId battler)
{
    u16 s = BaseSpecies(gBattleMons[battler].species);
    u32 k;
    for (k = 0; k < 2; k++)
    {
        if (BaseSpecies(p->lead[k]) == s) // in lotta la forma può cambiare (Mega): si confronta la specie base
            return (gBattleTurnCounter == 0) ? p->t1Move[k] : p->t2Move[k];
    }
    return MOVE_NONE;
}

s32 AI_VgcBookPairBonus(enum BattlerId me, u32 myMove, enum BattlerId partner, u32 partnerMove)
{
    u32 side = GetBattlerSide(me), pick;
    const struct VgcBookPlan *p;
    s32 bonus = 0;
    u32 planned;

    if (gBattleTurnCounter > 1 || !sBookReady || BOOK_OFF(side))
        return 0;
    pick = sBookPick[side];
    if (pick == VGC_BOOK_NO_PICK || pick >= ARRAY_COUNT(sVgcPlanBook))
        return 0;
    p = &sVgcPlanBook[pick];
    planned = PlanMoveFor(p, me);
    if (planned != MOVE_NONE && planned == myMove)
        bonus += BOOK_MOVE_BONUS;
    if (partner != me)
    {
        planned = PlanMoveFor(p, partner);
        if (planned != MOVE_NONE && planned == partnerMove)
            bonus += BOOK_MOVE_BONUS;
    }
    return bonus;
}

u32 AI_VgcExplorePick(enum BattlerId me, u32 bestIdx, const s32 *ev, u32 n)
{
#if TESTING
    u32 side = GetBattlerSide(me), i, count = 0, r;
    if (gBattleTurnCounter > 1 || EXPLORE_EPS(side) == 0 || n < 2 || (Random() % 100) >= EXPLORE_EPS(side))
        return bestIdx;
    for (i = 0; i < n; i++)
        if (i != bestIdx && ev[i] >= ev[bestIdx] - EXPLORE_PAIR_MARGIN)
            count++;
    if (count == 0)
        return bestIdx;
    r = Random() % count;
    for (i = 0; i < n; i++)
    {
        if (i != bestIdx && ev[i] >= ev[bestIdx] - EXPLORE_PAIR_MARGIN)
        {
            if (r == 0)
                return i;
            r--;
        }
    }
#else
    (void)me; (void)ev; (void)n;
#endif
    return bestIdx;
}
