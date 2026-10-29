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
#include "battle_ai_vgc_book.h"
#include "battle_ai_scout.h"
#include "team_preview.h"
#include "battle_ai_vgc_graph.h"
#include "battle_gimmick.h"
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
#include "battle_ai_perspective.h"
#include "vgc_battle_log.h"

// IA contro IA (B_FLAG_AI_VS_AI_BATTLE): il "giocatore" di questo file è il lato avversario di chi sta
// pianificando (battle_ai_perspective.h). Fuori da IA contro IA è identico a IsOnPlayerSide.
#define IsOnPlayerSide(battler) AI_IsFoeOfPerspective(battler)

#ifndef CUSTOM_VGC_PLAN_DEPTH
#define CUSTOM_VGC_PLAN_DEPTH 3
#endif
#ifndef CUSTOM_VGC_PLAN_SIMS_PER_FRAME
#define CUSTOM_VGC_PLAN_SIMS_PER_FRAME 24
#endif

// Le funzioni più chiamate dalla simulazione (valutazione, turno, danni, velocità) stanno in IWRAM come codice ARM:
// lì il bus è a 32 bit senza attese, contro la ROM a 16 bit con attese. long_call: sono lontane dalla ROM.
#define PLAN_HOT ARM_FUNC __attribute__((section(".iwram.vgcplan"), noinline, long_call))

#define PLAN_MAX_OURS        7   // azioni per ciascuno dei nostri Pokémon che entrano nelle coppie (le migliori a 1 turno)
#define PLAN_ALL_OURS        15  // tutte le azioni legali di un nostro Pokémon (4 mosse x 3 bersagli + cambi)
#define PLAN_MAX_PAIRS       10  // coppie (nostra azione, azione del partner) simulate per intero (albero a 3 turni)
#define PLAN_EXTRA_PAIRS     4   // coppie aggiunte a fine albero: azioni di velocità/setup accanto al compagno della coppia migliore
#define PLAN_ALL_PAIRS       (PLAN_MAX_PAIRS + PLAN_EXTRA_PAIRS)
#define PLAN_FOE_PER         2   // azioni per avversario (primo turno)
#define PLAN_FOE_CAND        5   // azioni plausibili per avversario valutate dal modello del giocatore (livello 1)
#define PLAN_P2_OURS_PER     4   // azioni per chi pianifica al turno dopo (una corsia è per setup/Protect/velocità)
#define PLAN_P2_PARTNER_PER  4   // azioni del partner al turno dopo (anche qui con la corsia)
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
PLAN_HOT static u32 ApplyStage(u32 v, s32 s)
{
    s = max(-6, min(6, s));
    return (s >= 0) ? v * (2 + s) / 2 : v * 2 / (2 - s);
}

// Da un valore già allo stadio `from` allo stesso valore allo stadio `to`.
PLAN_HOT static u32 StageRatio(u32 v, s32 from, s32 to)
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
    u8 damaged;         // colpito in questo turno (Focus Punch fallisce)
    u8 taunted;         // turni di Taunt rimasti (mosse di stato bloccate)
    u8 drowsy;          // Yawn: a fine turno scende; arrivato a 0 si addormenta (2 = colpito in questo turno)
    u8 encored;         // turni di Encore rimasti (questo compreso): ripete lastMove
    u16 lastMove;       // ultima mossa usata (Encore la blocca)
    u8 megaPct[4];     // Mega a inizio turno: statistiche Mega / forma base in % (Atk, Def, SpA, SpD)
    u8 megaNow;         // megaevolve in questo turno simulato: la tabella dei danni (forma base) non vale più
    u8 subHp;           // PS del Sostituto (0 = nessuno)
    u8 unburden;        // Unburden attivo (strumento consumato): Velocità x2
    s8 stage[PS_COUNT];     // stadi attuali nella simulazione (-6..+6)
    s8 stage0[PS_COUNT];    // stadi all'inizio (già compresi in velocità e danni della tabella dell'IA)
    u8 canAttack:1;         // ha mosse d'attacco (i boost offensivi servono a qualcosa)
    u8 canPass:1;           // ha Baton Pass (i boost valgono per chi li riceve)
    u8 disguise:1;          // Disguise intatto (Mimikyu): il primo colpo toglie solo 1/8 dei PS
};

struct PlanState
{
    struct PlanMon m[MAX_BATTLERS_COUNT];
    u8 trickRoom;       // turni rimasti (0 = non attivo)
    u8 tailwind[2];     // turni rimasti per lato (0 = non attivo)
    u8 weather;         // PW_*
    u8 screens[2];      // per lato: bit 0 Reflect, bit 1 Light Screen (Aurora Veil = entrambi)
    u8 screens0[2];     // schermi all'inizio (compresi nella tabella dei danni)
    u8 screenClay[2];   // per lato: schermo nuovo messo con Light Clay (8 turni invece di 5)
    u8 firstTurn;       // turno simulato = turno attuale (Fake Out vale)
    u8 terrain;         // B_TERRAIN_* attuale nella simulazione
    u8 terrain0;        // terreno all'inizio (compreso nella tabella dei danni)
    u8 terrainTurns;    // turni rimasti (0 = senza scadenza nota)
    u8 replMask[2];     // per lato: indici di squadra già entrati come sostituti dopo un KO (o caduti) nella simulazione
    u8 benchUsed[2];    // per lato: Pokémon della panchina entrati dopo un KO (il materiale non li conta due volte)
};

#define PLAN_KO_CACHE 64
#define PLAN_LIKELY_MAX 4
#define PLAN_EST_CACHE 64

struct PlanEstCache
{
    u32 k1, k2, k3; // chiave (k1 = 0 = vuota)
    s32 val;
};

struct PlanKoCache
{
    u32 k1, k2, k3;     // chiave (k1 = 0 = vuota)
    u8 val;
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
    struct PlanAction ours[PLAN_ALL_PAIRS], partners[PLAN_ALL_PAIRS], foeA[PLAN_FOE_PER], foeB[PLAN_FOE_PER];
    u8 nOurs, nA, nB;
    struct VgcTeamGraph graph;  // conoscenza della nostra squadra (combo, ricevente del Baton Pass)
    s8 bench[NUM_BATTLE_SIDES]; // Pokémon utilizzabili in panchina per lato all'inizio (non cambia con i cambi simulati)
    u8 origPMove;
    enum BattlerId origPTarget;
    struct PlanState base;
    // Tutte le azioni legali nostre (cand[0]) e del partner (cand[1]), con il valore simulato a 1 turno
    // (fase 2): servono a scegliere le coppie senza che le regole filtrino prima cosa vede la simulazione.
    struct PlanAction cand[2][PLAN_ALL_OURS];
    s32 candVal[2][PLAN_ALL_OURS];
    u8 nCand[2], rankSide, rankIdx;
    // Modello del giocatore, livello 1 (fase 3): per ogni avversario le sue azioni plausibili, valutate con una
    // simulazione a 1 turno dal SUO punto di vista; le due migliori diventano foeA/foeB.
    struct PlanAction foeCand[2][PLAN_FOE_CAND];
    s32 foeVal[2][PLAN_FOE_CAND];
    u8 nFoeCand[2], pmSide, pmIdx;
    // Coppie a 1 turno (fase 4): le azioni rimaste dei due lati (TrimCands), simulate INSIEME contro le risposte più
    // probabili. Ordinano le coppie da simulare per intero: la somma dei valori singoli (ognuno contro un solo
    // riferimento del compagno) non vedeva le coppie che funzionano solo insieme (Eruption + Giga Drain su chi la
    // minaccia, Earthquake + Thunderbolt sullo stesso bersaglio).
    struct PlanAction trim[2][PLAN_MAX_OURS];
    s32 pairVal[PLAN_MAX_OURS][PLAN_MAX_OURS];
    u8 nTrim[2], pi, pj;
    // livello 1
    u8 i, a, b, phase;
    s32 sum, wsum, worst;
    s32 ev[PLAN_ALL_PAIRS];
    s32 koSum[PLAN_ALL_PAIRS], koWsum[PLAN_ALL_PAIRS]; // KO sicuri (ia-vgc-ko): credito pesato sui rami del turno 1
    s32 bestSoFar;      // miglior valore di coppia già finito (potatura)
    const struct VgcEvalWeights *w; // pesi della valutazione del lato di chi pensa
    u8 winconParty;     // bit per indice di squadra: le win condition del piano del nostro allenatore
    u8 repaired;        // seconda passata delle coppie già fatta
    s16 sleepW[MAX_BATTLERS_COUNT];   // valore del sonno di chi è in campo a inizio lavoro: cresce con la sua minaccia
    s16 threatPct[MAX_BATTLERS_COUNT]; // minaccia (ThreatPct) di chi è in campo a inizio lavoro: danno evitato per turno di sonno
    u32 speedAlt[MAX_BATTLERS_COUNT]; // Velocità alternativa (in campo) dei Pokémon del giocatore la cui stima è incerta, 0 = nessuna
    u8 alpha;           // peso (decimi) della media contro il caso peggiore: dinamico, più rischio quando si è sotto
    struct PlanState s1;
    s32 missPen;        // quanto si perde, in media, per le azioni del turno 1 che possono mancare
    // livello 2 (turno dopo)
    struct PlanState la;
    struct PlanAction mine[PLAN_P2_OURS_PER], pa[PLAN_P2_OURS_PER], fa[PLAN_P2_FOE_PER], fb[PLAN_P2_FOE_PER];
    struct PlanAction g3[4];    // azioni del terzo turno (ridotto), una per battler
    u8 nM, nP, nA2, nB2;
    u8 i2, j2, a2, b2;
    s32 laBest, laSum, laWsum, laWorst;
    u8 bestI2, bestJ2, refine;  // linea migliore del turno 2; refine = secondo passaggio con il terzo turno
    struct PlanKoCache koCache[PLAN_KO_CACHE];
    struct PlanEstCache estCache[PLAN_EST_CACHE];
    u8 protectChance[MAX_BATTLERS_COUNT], switchChance[MAX_BATTLERS_COUNT]; // del giocatore, letti una volta a inizio lavoro
    // Attacchi probabili ma non ancora visti dei Pokémon del giocatore in campo (belief M1: statistiche d'uso,
    // mosse viste, Choice dedotta), con la probabilità. Letti una volta a inizio lavoro.
    u16 likelyMove[MAX_BATTLERS_COUNT][PLAN_LIKELY_MAX];
    u8 likelyP[MAX_BATTLERS_COUNT][PLAN_LIKELY_MAX];
    u8 nLikely[MAX_BATTLERS_COUNT];
    // misura del costo (fotogrammi con lavoro, simulazioni fatte)
    u16 frames;
    u16 sims;
    // risultato
    u8 resultKind;
    u8 resultMove;
    s8 resultParty;
    enum BattlerId resultTarget;
};

static EWRAM_DATA struct PlanJob sJob = {0};

// Pesi della valutazione per lato (M9: la taratura automatica li cambia per il lato G del self-play; nel gioco
// restano i valori di include/config/custom.h). Si leggono con VW(campo) per il lato di chi pensa.
struct VgcEvalWeights
{
    s16 ko, alive, speedPair, threatFast, threatSlow, material, switchCost, wincon;
};
static EWRAM_DATA struct VgcEvalWeights sVgcWeights[NUM_BATTLE_SIDES] = {0};
static EWRAM_DATA u8 sVgcWeightsSet = 0; // bit per lato: pesi impostati

static void DefaultEvalWeights(struct VgcEvalWeights *w)
{
    w->ko = CUSTOM_VGC_EVAL_KO;
    w->alive = CUSTOM_VGC_EVAL_ALIVE;
    w->speedPair = CUSTOM_VGC_EVAL_SPEED_PAIR;
    w->threatFast = CUSTOM_VGC_EVAL_THREAT_FAST;
    w->threatSlow = CUSTOM_VGC_EVAL_THREAT_SLOW;
    w->material = CUSTOM_VGC_EVAL_MATERIAL;
    w->switchCost = CUSTOM_VGC_SWITCH_COST;
    w->wincon = CUSTOM_VGC_EVAL_WINCON_PCT;
}

// Pesi del lato `side` in % dei valori di default (ordine: KO, vivo, velocità, minacce, materiale, cambio, win
// condition; 0 = 100%). Usato dalla taratura (test/vgc_selfplay); senza chiamate valgono i default.
void AI_VgcSetEvalWeightPct(u32 side, const u8 pct[7])
{
    struct VgcEvalWeights *w = &sVgcWeights[side];
    u32 p[7], i;
    for (i = 0; i < 7; i++)
        p[i] = pct[i] ? pct[i] * 2 : 100;
    DefaultEvalWeights(w);
    w->ko = w->ko * p[0] / 100;
    w->alive = w->alive * p[1] / 100;
    w->speedPair = w->speedPair * p[2] / 100;
    w->threatFast = w->threatFast * p[3] / 100;
    w->threatSlow = w->threatSlow * p[3] / 100;
    w->material = w->material * p[4] / 100;
    w->switchCost = w->switchCost * p[5] / 100;
    w->wincon = 100 + (w->wincon - 100) * (s32)p[6] / 100;
    sVgcWeightsSet |= 1u << side;
}

void AI_VgcResetEvalWeights(void)
{
    sVgcWeightsSet = 0;
}

#define VW(field) (sJob.w->field)

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

// Per lato: in IA contro IA i due lati pianificano a turno nello stesso turno e non devono cancellarsi
// a vicenda la metà già decisa per il partner.
// Battler il cui colpo fa il danno minimo (ramo "colpo basso": un KO solo con la mediana non è sicuro), più 1.
static EWRAM_DATA u8 sLowRollBattler = 0;
// Battler il cui colpo fa il danno massimo (ramo "colpo alto": un KO che arriva solo con un tiro sopra la mediana), più 1.
static EWRAM_DATA u8 sHighRollBattler = 0;
// Ramo "velocità incerta": il battler del giocatore (più 1) gioca con la Velocità alternativa sJob.speedAlt.
static EWRAM_DATA u8 sSpeedAltBattler = 0;

static EWRAM_DATA struct PlanPartnerDecision sPartnerDecisionBySide[NUM_BATTLE_SIDES] = {0};

#if TESTING
// Registro della prima decisione del turno 0 (per i test: confronto dei valori delle azioni).
struct PlanTestLog
{
    u8 n;
    enum BattlerId user[PLAN_ALL_PAIRS], puser[PLAN_ALL_PAIRS];
    enum Move move[PLAN_ALL_PAIRS], pmove[PLAN_ALL_PAIRS];
    s32 ev[PLAN_ALL_PAIRS];
};
static EWRAM_DATA struct PlanTestLog sTestLog = {0};
// Spinta delle regole (M8) di ogni candidata della decisione del turno 0 (test delle spinte, ai_vgc_rules.c).
extern s32 Test_MgbaPrintf(const char *fmt, ...);
struct PlanTestPrior
{
    u8 n;
    enum BattlerId user[2 * PLAN_ALL_OURS];
    enum Move move[2 * PLAN_ALL_OURS];
    s32 prior[2 * PLAN_ALL_OURS];
};
static EWRAM_DATA struct PlanTestPrior sTestPrior = {0};
// Contatori di chiamate per la misura del costo (VGCCPU).
static EWRAM_DATA u32 sCntEval = 0, sCntBuild = 0, sCntKo = 0, sCntEst = 0, sCntDmg = 0;
static EWRAM_DATA u32 sProf[5] = {0}; // Evaluate, RunTurn, ApplyAction, CanKo (senza cache), EstimateDamage
#define PLAN_COUNT(c) ((c)++)
// Diagnostica: Evaluate stampa le sue parti (stato per stato) quando è acceso.
static EWRAM_DATA u8 sEvalDebug = 0;
#define EVAL_DEBUG(tag, val) do { if (sEvalDebug) { extern s32 Test_MgbaPrintf(const char *fmt, ...); Test_MgbaPrintf("  VGCEVAL %s=%d", tag, val); } } while (0)
#else
#define EVAL_DEBUG(tag, val)
#define PLAN_COUNT(c)
#endif

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

static u8 TerrainFromAbility(enum Ability ab)
{
    switch (ab)
    {
    case ABILITY_GRASSY_SURGE: return B_TERRAIN_GRASSY;
    case ABILITY_MISTY_SURGE: return B_TERRAIN_MISTY;
    case ABILITY_ELECTRIC_SURGE: case ABILITY_HADRON_ENGINE: return B_TERRAIN_ELECTRIC;
    case ABILITY_PSYCHIC_SURGE: return B_TERRAIN_PSYCHIC;
    default: return B_TERRAIN_NONE;
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

// Velocità di partenza per la simulazione: statistica con gli stadi attuali e lo strumento, SENZA Tailwind,
// meteo e paralisi (li applica EffSpeed, che li segue mentre cambiano; prima la velocità della tabella dell'IA li
// conteneva già e Tailwind o Swift Swim contavano due volte). Del giocatore la statistica è STIMATA da specie e
// livello (quella vera non si vede) e lo strumento è quello noto, dedotto dall'ordine dei turni (Stolascelta:
// si è mosso prima di un nostro Pokémon più veloce, vedi battle_ai_vgc.c) o assunto per la specie.
static u32 PlanFieldSpeed(enum BattlerId b)
{
    u32 s, stage = gBattleMons[b].statStages[STAT_SPEED];
    enum HoldEffect he = gAiLogicData->holdEffects[b];

    if (IsOnPlayerSide(b))
    {
        u16 hp, stats[NUM_STATS];
        Scout_EstimateBattlerStats(b, &hp, stats);
        s = stats[STAT_SPEED];
    }
    else
    {
        s = gBattleMons[b].speed;
    }
    s = s * gStatStageRatios[stage][0] / gStatStageRatios[stage][1];
    if (he == HOLD_EFFECT_CHOICE_SCARF)
        s = s * 3 / 2;
    else if (he == HOLD_EFFECT_IRON_BALL || he == HOLD_EFFECT_MACHO_BRACE || he == HOLD_EFFECT_POWER_ITEM)
        s /= 2;
    if (gAiLogicData->abilities[b] == ABILITY_UNBURDEN && gBattleMons[b].volatiles.unburdenActive)
        s *= 2;
    return s;
}

static void LoadFieldMon(struct PlanMon *pm, enum BattlerId b)
{
    pm->hp = gBattleMons[b].hp;
    pm->maxHp = gBattleMons[b].maxHP;
    pm->species = gBattleMons[b].species;
    pm->speed = PlanFieldSpeed(b);
    if (IsOnPlayerSide(b))
    {
        // Del giocatore si stimano le statistiche da specie e livello (le vere non si vedono).
        u16 hp, stats[NUM_STATS];
        Scout_EstimateBattlerStats(b, &hp, stats);
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
    pm->subHp = gBattleMons[b].volatiles.substitute ? gBattleMons[b].volatiles.substituteHP : 0;
    pm->unburden = gBattleMons[b].volatiles.unburdenActive;
    pm->ability = gAiLogicData->abilities[b];
    pm->holdEffect = gAiLogicData->holdEffects[b];
    pm->partyIdx = -1;
    pm->alive = IsBattlerAlive(b);
    pm->asleep = (gBattleMons[b].status1 & STATUS1_SLEEP) != 0;
    pm->sleepTurns = gBattleMons[b].status1 & STATUS1_SLEEP;
    pm->burned = pm->burned0 = (gBattleMons[b].status1 & STATUS1_BURN) != 0;
    pm->paralyzed = (gBattleMons[b].status1 & STATUS1_PARALYSIS) != 0;
    // Yawn ed Encore già in corso (visibili a entrambi: messaggi in lotta) e ultima mossa usata.
    pm->drowsy = gBattleMons[b].volatiles.yawn;
    pm->encored = (gBattleMons[b].volatiles.encoredMove != MOVE_NONE) ? gBattleMons[b].volatiles.encoreTimer : 0;
    pm->lastMove = (gLastMoves[b] == MOVE_UNAVAILABLE) ? MOVE_NONE : gLastMoves[b];
    if (pm->encored)
        pm->lastMove = gBattleMons[b].volatiles.encoredMove;
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
    pm->disguise = pm->ability == ABILITY_DISGUISE && IsMimikyuDisguised(b); // forma visibile a entrambi
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

        Scout_EstimateStatsInTeam(Scout_BattlerContext(b), species, level, &hp, stats); // contesto della squadra
        pm->maxHp = hp;
        pm->hp = (realMax == 0) ? 0 : hp * GetMonData(mon, MON_DATA_HP) / realMax;
        if (pm->hp == 0 && GetMonData(mon, MON_DATA_HP) != 0)
            pm->hp = 1;
        pm->speed = stats[STAT_SPEED];
        pm->atk = stats[STAT_ATK];
        pm->spa = stats[STAT_SPATK];
        pm->def = stats[STAT_DEF];
        pm->spd = stats[STAT_SPDEF];
        {
            u32 chance;
            pm->ability = Scout_LikelyAbilityInTeam(Scout_BattlerContext(b), species, &chance);
            if (chance < 60)
                pm->ability = Scout_KnownAbility(species);
        }
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
        Scout_GuessItemInTeam(Scout_BattlerContext(b), species, &g);
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
    pm->megaPct[0] = pm->megaPct[1] = pm->megaPct[2] = pm->megaPct[3] = 100;
    pm->megaNow = FALSE;
    pm->subHp = 0;
    pm->unburden = FALSE;
    pm->alive = pm->hp > 0;
    pm->asleep = (GetMonData(mon, MON_DATA_STATUS) & STATUS1_SLEEP) != 0;
    pm->sleepTurns = GetMonData(mon, MON_DATA_STATUS) & STATUS1_SLEEP;
    pm->burned = pm->burned0 = (GetMonData(mon, MON_DATA_STATUS) & STATUS1_BURN) != 0;
    pm->paralyzed = (GetMonData(mon, MON_DATA_STATUS) & STATUS1_PARALYSIS) != 0;
    pm->protecting = pm->protectedLast = pm->wideGuard = pm->flinched = pm->redirect = pm->helping = 0;
    pm->taunted = pm->drowsy = pm->encored = 0;
    pm->lastMove = MOVE_NONE;
    pm->berryUsed = FALSE;
    memset(pm->stage, 0, sizeof(pm->stage));
    memset(pm->stage0, 0, sizeof(pm->stage0));
    pm->canAttack = TRUE;
    pm->canPass = FALSE;
    // Chi entra dalla panchina ha il Disguise intatto (la forma Busted torna normale col cambio).
    pm->disguise = pm->ability == ABILITY_DISGUISE
                && (species == SPECIES_MIMIKYU_DISGUISED || species == SPECIES_MIMIKYU_TOTEM_DISGUISED);
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
    // Terreno all'entrata (abilità).
    if (TerrainFromAbility(pm->ability) != B_TERRAIN_NONE)
    {
        st->terrain = TerrainFromAbility(pm->ability);
        st->terrainTurns = 5;
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

// Mega Evoluzione a inizio turno (prima delle mosse, Velocità della Mega già in questo turno): statistiche scalate
// sulle statistiche base della Mega, tipi e abilità della Mega. La tabella dei danni dell'IA è calcolata sulla forma
// base: DamageOf la riscala con megaPct.
static void ApplyMegaForm(struct PlanMon *pm, u16 megaSpecies, enum Ability megaAbility)
{
    const struct SpeciesInfo *bi = &gSpeciesInfo[pm->species], *mi = &gSpeciesInfo[megaSpecies];

    pm->megaPct[0] = mi->baseAttack * 100 / max(bi->baseAttack, 1);
    pm->megaPct[1] = mi->baseDefense * 100 / max(bi->baseDefense, 1);
    pm->megaPct[2] = mi->baseSpAttack * 100 / max(bi->baseSpAttack, 1);
    pm->megaPct[3] = mi->baseSpDefense * 100 / max(bi->baseSpDefense, 1);
    pm->atk = pm->atk * pm->megaPct[0] / 100;
    pm->def = pm->def * pm->megaPct[1] / 100;
    pm->spa = pm->spa * pm->megaPct[2] / 100;
    pm->spd = pm->spd * pm->megaPct[3] / 100;
    pm->speed = pm->speed * mi->baseSpeed / max(bi->baseSpeed, 1);
    pm->t1 = mi->types[0];
    pm->t2 = mi->types[1];
    pm->ability = megaAbility;
    pm->species = megaSpecies;
    pm->megaNow = TRUE;
}

// Chi megaevolve in questo turno: noi se possiamo (l'IA del motore la usa appena può), il giocatore se la sua
// specie di solito la usa (belief: statistiche d'uso) e non l'ha ancora usata.
static void LoadMega(struct PlanState *st, enum BattlerId b)
{
    struct PlanMon *pm = &st->m[b];
    u16 mega = SPECIES_NONE;
    enum Ability ab = ABILITY_NONE;

    pm->megaPct[0] = pm->megaPct[1] = pm->megaPct[2] = pm->megaPct[3] = 100;
    if (!pm->alive || gSpeciesInfo[pm->species].isMegaEvolution || HasTrainerUsedGimmick(b, GIMMICK_MEGA))
        return;
    if (!IsOnPlayerSide(b))
    {
        if (!CanActivateGimmick(b, GIMMICK_MEGA))
            return;
        mega = GetBattleFormChangeTargetSpecies(b, FORM_CHANGE_BATTLE_MEGA_EVOLUTION_ITEM, pm->ability);
        if (mega == SPECIES_NONE || mega == pm->species)
            mega = GetBattleFormChangeTargetSpecies(b, FORM_CHANGE_BATTLE_MEGA_EVOLUTION_MOVE, pm->ability);
        if (mega == SPECIES_NONE || mega == pm->species)
            return;
        ab = gSpeciesInfo[mega].abilities[0];
    }
    else
    {
        struct ScoutMegaChance mc[2];
        // Soglia alta: una Mega creduta e non avvenuta cambia molto la posizione (una sola per squadra).
        if (Scout_GetMegaForms(pm->species, mc, 2) == 0 || Scout_MegaChanceInTeam(Scout_BattlerContext(b), pm->species) < 80
         || pm->holdEffect != HOLD_EFFECT_NONE)
            return;
        mega = mc[0].species;
        ab = mc[0].ability ? mc[0].ability : gSpeciesInfo[mega].abilities[0];
    }
    ApplyMegaForm(pm, mega, ab);
    if (WeatherFromAbility(ab) != PW_NONE)
        st->weather = WeatherFromAbility(ab);
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
    st->terrain = st->terrain0 = gFieldTimers.terrain;
    st->terrainTurns = gFieldTimers.terrainTimer;
    // Le Mega dopo il meteo di partenza: il meteo dell'abilità della Mega (Charizard Y) vale già in questo turno.
    for (b = 0; b < gBattlersCount; b++)
        LoadMega(st, b);
}

// Fine turno simulato: scadono Trick Room e Tailwind (come in battle_end_turn.c), Protect diventa
// "usato il turno prima".
static bool32 CanBatonPassTo(struct Pokemon *party, u32 idx, u32 myIdx, u32 partnerIdx);
static void LoadPartyMon(struct PlanState *st, enum BattlerId b, u32 partyIdx);

// Sostituzioni dopo un KO, a fine turno come nel gioco: nel posto vuoto entra un Pokémon della panchina (per noi
// quello suggerito dal motore se libero; per il giocatore uno già visto in lotta se c'è, con le statistiche stimate).
// Senza, il turno dopo si giocava in inferiorità numerica e una linea che perde un Pokémon sembrava persa del tutto.
static void ReplaceFaintedInSim(struct PlanState *st)
{
    enum BattlerId b;
    for (b = 0; b < gBattlersCount; b++)
    {
        struct PlanMon *pm = &st->m[b];
        u32 side = GetBattlerSide(b), myIdx, pIdx, idx, best = PARTY_SIZE;
        enum BattlerId p = GetPartnerBattler(b);
        struct Pokemon *party;

        if (pm->alive || pm->species == SPECIES_NONE || st->benchUsed[side] >= (u32)max(sJob.bench[side], 0))
            continue;
        party = GetBattlerParty(b);
        myIdx = (pm->partyIdx >= 0) ? (u32)pm->partyIdx : gBattlerPartyIndexes[b];
        pIdx = (st->m[p].partyIdx >= 0) ? (u32)st->m[p].partyIdx : gBattlerPartyIndexes[p];
        if (myIdx < PARTY_SIZE)
            st->replMask[side] |= 1u << myIdx;
        for (idx = 0; idx < PARTY_SIZE; idx++)
        {
            if ((st->replMask[side] & (1u << idx)) || !CanBatonPassTo(party, idx, myIdx, pIdx)
             || (b != p && IsBattlerAlive(p) && gBattlerPartyIndexes[p] == idx))
                continue;
            if (best == PARTY_SIZE)
                best = idx;
            if (!IsOnPlayerSide(b) && idx == gAiLogicData->mostSuitableMonId[b])
            {
                best = idx;
                break;
            }
            if (IsOnPlayerSide(b) && GetMonData(&party[idx], MON_DATA_HP) < GetMonData(&party[idx], MON_DATA_MAX_HP))
            {
                best = idx; // già sceso in campo: lo si conosce
                break;
            }
        }
        if (best == PARTY_SIZE)
            continue;
        LoadPartyMon(st, b, best);
        st->replMask[side] |= 1u << best;
        st->benchUsed[side]++;
    }
}

static void NextTurn(struct PlanState *st)
{
    enum BattlerId b;
    for (b = 0; b < gBattlersCount; b++)
    {
        st->m[b].protectedLast = st->m[b].protecting;
        if (st->m[b].taunted > 0)
            st->m[b].taunted--;
        if (st->m[b].encored > 0)
            st->m[b].encored--;
        st->m[b].protecting = st->m[b].wideGuard = st->m[b].flinched = st->m[b].redirect = st->m[b].helping = st->m[b].damaged = 0;
    }
    if (CUSTOM_VGC_PLAN_REPLACE_KO)
        ReplaceFaintedInSim(st);
    if (st->trickRoom > 0)
        st->trickRoom--;
    if (st->tailwind[B_SIDE_PLAYER] > 0)
        st->tailwind[B_SIDE_PLAYER]--;
    if (st->tailwind[B_SIDE_OPPONENT] > 0)
        st->tailwind[B_SIDE_OPPONENT]--;
    if (st->terrainTurns > 0 && --st->terrainTurns == 0)
        st->terrain = B_TERRAIN_NONE;
    st->firstTurn = FALSE;
}

PLAN_HOT static u32 EffSpeed(const struct PlanState *st, enum BattlerId b)
{
    const struct PlanMon *pm = &st->m[b];
    u32 raw = (b + 1 == sSpeedAltBattler && pm->partyIdx < 0) ? sJob.speedAlt[b] : pm->speed;
    u32 s = StageRatio(raw, pm->stage0[PS_SPE], pm->stage[PS_SPE]);

    if (pm->paralyzed && pm->ability != ABILITY_QUICK_FEET)
        s /= 2;
    if (pm->unburden && pm->ability == ABILITY_UNBURDEN && !(pm->partyIdx < 0 && gBattleMons[b].volatiles.unburdenActive))
        s *= 2;

    if (st->tailwind[GetBattlerSide(b)])
        s *= 2;
    if ((st->weather == PW_RAIN && pm->ability == ABILITY_SWIFT_SWIM) || (st->weather == PW_SUN && pm->ability == ABILITY_CHLOROPHYLL)
     || (st->weather == PW_SAND && pm->ability == ABILITY_SAND_RUSH) || (st->weather == PW_SNOW && pm->ability == ABILITY_SLUSH_RUSH))
        s *= 2;
    return s;
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
static s32 EstimateDamageImpl(const struct PlanState *st, const struct PlanMon *atk, const struct PlanMon *def, enum Move move);
// Profilo (solo test): righe di schermo passate dentro la funzione, sommate (VGCPROF).
// Cache della stima per il lavoro in corso: la chiave contiene tutto ciò che la stima legge (chi, quale mossa,
// stadi, meteo, scottatura, strumento, abilità). Le stesse coppie attaccante/bersaglio tornano in ogni foglia.
static s32 EstimateDamage(const struct PlanState *st, const struct PlanMon *atk, const struct PlanMon *def, enum Move move)
{
    u32 ai = atk - st->m, di = def - st->m;
    u32 k1 = 0x80000000u | (u32)move | (ai << 16) | (di << 18) | (((u32)(atk->partyIdx + 1) & 7) << 20) | (((u32)(def->partyIdx + 1) & 7) << 23)
           | ((st->weather & 7u) << 26) | ((u32)atk->burned0 << 29);
    u32 k2 = (((u32)(atk->stage[PS_ATK] + 6) & 15)) | (((u32)(atk->stage[PS_SPA] + 6) & 15) << 4) | (((u32)(def->stage[PS_DEF] + 6) & 15) << 8)
           | (((u32)(def->stage[PS_SPD] + 6) & 15) << 12) | (((u32)def->holdEffect & 0xFF) << 16) | (((u32)atk->holdEffect & 0xFF) << 24);
    u32 k3 = ((u32)atk->ability & 0xFFFF) | ((u32)def->ability << 16);
    struct PlanEstCache *e = &sJob.estCache[(k1 * 2654435761u ^ k2 * 40503u ^ k3 * 97u) % PLAN_EST_CACHE];
    s32 r;
#if TESTING
    u32 t0 = REG_VCOUNT;
#endif
    if (e->k1 == k1 && e->k2 == k2 && e->k3 == k3)
        return e->val;
    r = EstimateDamageImpl(st, atk, def, move);
    e->k1 = k1;
    e->k2 = k2;
    e->k3 = k3;
    e->val = r;
#if TESTING
    sProf[4] += (REG_VCOUNT + 228 - t0) % 228;
#endif
    return r;
}

static s32 EstimateDamageImpl(const struct PlanState *st, const struct PlanMon *atk, const struct PlanMon *def, enum Move move)
{
    PLAN_COUNT(sCntEst);
    u32 power = GetMovePower(move);
    enum Type type = GetMoveType(move);
    bool32 physical = GetMoveCategory(move) == DAMAGE_CATEGORY_PHYSICAL;
    u32 a = physical ? ApplyStage(atk->atk, atk->stage[PS_ATK]) : ApplyStage(atk->spa, atk->stage[PS_SPA]);
    u32 d = physical ? ApplyStage(def->def, def->stage[PS_DEF]) : ApplyStage(def->spd, def->stage[PS_SPD]);
    u32 dmg;

    if (power == 0 || IsBattleMoveStatus(move))
        return 0;
    // Potenza variabile delle mosse più comuni in VGC (peso, meteo, PS): con 60 fissi Heavy Slam su Gengar o
    // Eruption a PS pieni erano sottostimate di metà.
    switch (GetMoveEffect(move))
    {
    case EFFECT_HEAT_CRASH:
    {
        static const u8 sHeatCrash[] = {40, 40, 60, 80, 100, 120};
        u32 wa = GetSpeciesWeight(atk->species), wd = max(GetSpeciesWeight(def->species), 1u);
        if (atk->ability == ABILITY_HEAVY_METAL) wa *= 2;
        if (atk->ability == ABILITY_LIGHT_METAL) wa /= 2;
        if (def->ability == ABILITY_HEAVY_METAL) wd *= 2;
        if (def->ability == ABILITY_LIGHT_METAL) wd = max(wd / 2, 1u);
        power = sHeatCrash[min(wa / wd, 5u)];
        break;
    }
    case EFFECT_LOW_KICK:
    {
        u32 wd = GetSpeciesWeight(def->species);
        power = wd < 100 ? 20 : wd < 250 ? 40 : wd < 500 ? 60 : wd < 1000 ? 80 : wd < 2000 ? 100 : 120;
        break;
    }
    case EFFECT_WEATHER_BALL:
        if (st->weather != PW_NONE)
        {
            power *= 2;
            type = st->weather == PW_SUN ? TYPE_FIRE : st->weather == PW_RAIN ? TYPE_WATER : st->weather == PW_SAND ? TYPE_ROCK : TYPE_ICE;
        }
        break;
    case EFFECT_POWER_BASED_ON_USER_HP:
        power = power * (u32)max(atk->hp, 0) / (u32)max(atk->maxHp, 1);
        if (power < 2)
            power = 2; // 1 vuol dire "potenza variabile" qui sotto
        break;
    default:
        break;
    }
    if (power == 1)
        power = 60;
    if (def->holdEffect == HOLD_EFFECT_ASSAULT_VEST && !physical)
        d = d * 3 / 2;
    if (def->holdEffect == HOLD_EFFECT_EVIOLITE)
        d = d * 3 / 2;
    if (d == 0)
        d = 1;
    // Abilità e strumenti che cambiano attacco/potenza (i più comuni in VGC).
    if (atk->ability == ABILITY_TECHNICIAN && power <= 60)
        power = power * 3 / 2;
    if (physical && (atk->ability == ABILITY_HUGE_POWER || atk->ability == ABILITY_PURE_POWER))
        a *= 2;
    if (physical && atk->ability == ABILITY_HUSTLE)
        a = a * 3 / 2;
    if (!physical && atk->ability == ABILITY_SOLAR_POWER && st->weather == PW_SUN)
        a = a * 3 / 2;
    if ((physical && atk->holdEffect == HOLD_EFFECT_CHOICE_BAND) || (!physical && atk->holdEffect == HOLD_EFFECT_CHOICE_SPECS))
        a = a * 3 / 2;
    if ((atk->ability == ABILITY_TRANSISTOR && type == TYPE_ELECTRIC) || (atk->ability == ABILITY_DRAGONS_MAW && type == TYPE_DRAGON)
     || (atk->ability == ABILITY_STEELWORKER && type == TYPE_STEEL))
        a = a * 3 / 2;
    if (atk->ability == ABILITY_WATER_BUBBLE && type == TYPE_WATER)
        a *= 2;
    if (!physical && def->ability == ABILITY_ICE_SCALES)
        d *= 2;
    if (physical && def->ability == ABILITY_FUR_COAT)
        d *= 2;
    dmg = ((2 * 50 / 5 + 2) * power * a / d) / 50 + 2;
    if (type == atk->t1 || type == atk->t2)
        dmg = (atk->ability == ABILITY_ADAPTABILITY) ? dmg * 2 : dmg * 3 / 2;
    {
        uq4_12_t eff = TypeEff(type, def);
        dmg = uq4_12_multiply_by_int_half_down(eff, dmg);
        if (eff > UQ_4_12(1.0) && (def->ability == ABILITY_FILTER || def->ability == ABILITY_SOLID_ROCK || def->ability == ABILITY_PRISM_ARMOR))
            dmg = dmg * 3 / 4;
    }
    if ((def->ability == ABILITY_THICK_FAT && (type == TYPE_FIRE || type == TYPE_ICE))
     || ((def->ability == ABILITY_HEATPROOF || def->ability == ABILITY_WATER_BUBBLE) && type == TYPE_FIRE)
     || (def->ability == ABILITY_PURIFYING_SALT && type == TYPE_GHOST))
        dmg /= 2;
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
    // Colpi multipli: Dual Wingbeat & co. (numero fisso), 2-5 colpi (in media 3, 5 con Skill Link).
    if (GetMoveStrikeCount(move) > 1)
        dmg *= GetMoveStrikeCount(move);
    else if (IsMultiHitMove(move))
        dmg *= (atk->ability == ABILITY_SKILL_LINK) ? 5 : 3;
    return dmg * 92 / 100;
}

// Mosse d'attacco che come "mossa probabile" del giocatore ingannano: Focus Punch fallisce se chi la usa viene
// colpito prima, le mosse a due turni colpiscono il turno dopo. Il simulatore le giocherebbe come colpi normali.
static bool32 IsUnreliableGuess(enum Move move)
{
    enum BattleMoveEffects eff = GetMoveEffect(move);
    return eff == EFFECT_FOCUS_PUNCH || eff == EFFECT_TWO_TURNS_ATTACK || eff == EFFECT_SOLAR_BEAM || eff == EFFECT_SKY_DROP
        || eff == EFFECT_FIRST_TURN_ONLY || IsBattleMoveStatus(move) || GetMovePower(move) == 0;
}

// Immunità date dall'abilità del bersaglio (quella che ha ORA nella simulazione: può cambiare con Skill Swap
// & co.). Mold Breaker e simili le ignorano. Telepathy: immune ai colpi del compagno.
static bool32 AbilityBlocksMove(const struct PlanMon *am, const struct PlanMon *dm, enum Move move, bool32 fromAlly)
{
    enum Type type = GetMoveType(move);

    if (am->ability == ABILITY_MOLD_BREAKER || am->ability == ABILITY_TERAVOLT || am->ability == ABILITY_TURBOBLAZE)
        return FALSE;
    switch (dm->ability)
    {
    case ABILITY_TELEPATHY:
        return fromAlly;
    case ABILITY_LEVITATE: case ABILITY_EARTH_EATER:
        return type == TYPE_GROUND;
    case ABILITY_FLASH_FIRE: case ABILITY_WELL_BAKED_BODY:
        return type == TYPE_FIRE;
    case ABILITY_WATER_ABSORB: case ABILITY_STORM_DRAIN: case ABILITY_DRY_SKIN:
        return type == TYPE_WATER;
    case ABILITY_VOLT_ABSORB: case ABILITY_LIGHTNING_ROD: case ABILITY_MOTOR_DRIVE:
        return type == TYPE_ELECTRIC;
    case ABILITY_SAP_SIPPER:
        return type == TYPE_GRASS;
    default:
        return FALSE;
    }
}

// A terra (i terreni valgono solo per chi tocca il suolo).
static bool32 IsGroundedInSim(const struct PlanMon *pm)
{
    return pm->t1 != TYPE_FLYING && pm->t2 != TYPE_FLYING && pm->ability != ABILITY_LEVITATE && pm->holdEffect != HOLD_EFFECT_AIR_BALLOON;
}

// Effetto del terreno sul danno, in %: Elettro/Erba/Psico x1,3 per chi attacca da terra; Earthquake & co. a metà
// sull'Erboso, Drago a metà sul Nebbioso (bersaglio a terra).
static u32 TerrainFactor(u8 terrain, const struct PlanMon *am, const struct PlanMon *dm, enum Move move)
{
    enum Type type = GetMoveType(move);
    u32 f = 100;

    if (terrain == B_TERRAIN_NONE)
        return 100;
    if (IsGroundedInSim(am) && ((terrain == B_TERRAIN_ELECTRIC && type == TYPE_ELECTRIC) || (terrain == B_TERRAIN_GRASSY && type == TYPE_GRASS)
                              || (terrain == B_TERRAIN_PSYCHIC && type == TYPE_PSYCHIC)))
        f = 130;
    if (IsGroundedInSim(dm))
    {
        if (terrain == B_TERRAIN_GRASSY && (move == MOVE_EARTHQUAKE || move == MOVE_BULLDOZE || move == MOVE_MAGNITUDE))
            f = f / 2;
        if (terrain == B_TERRAIN_MISTY && type == TYPE_DRAGON)
            f = f / 2;
    }
    return f;
}

// Danno minimo rispetto alla mediana, in %: dalla tabella dell'IA quando c'è, altrimenti il tiro minimo (85%).
static u32 LowRollPct(const struct PlanState *st, const struct PlanAction *a, enum BattlerId t)
{
    if (a->moveIndex < MAX_MON_MOVES && st->m[a->user].partyIdx < 0 && st->m[t].partyIdx < 0)
    {
        const struct SimulatedDamage *sd = &gAiLogicData->simulatedDmg[a->user][t][a->moveIndex];
        if (sd->median > 0)
            return sd->minimum * 100 / sd->median;
    }
    return 87;
}

// Danno massimo rispetto alla mediana, in %: dalla tabella dell'IA quando c'è, altrimenti il tiro massimo (la stima è
// la mediana: 100 / 92,5).
static u32 HighRollPct(const struct PlanState *st, const struct PlanAction *a, enum BattlerId t)
{
    if (a->moveIndex < MAX_MON_MOVES && st->m[a->user].partyIdx < 0 && st->m[t].partyIdx < 0)
    {
        const struct SimulatedDamage *sd = &gAiLogicData->simulatedDmg[a->user][t][a->moveIndex];
        if (sd->median > 0)
            return sd->maximum * 100 / sd->median;
    }
    return 108;
}

// ---- KO sicuri (ia-vgc-ko): tiro basso esatto ----
// Il tiro basso dalla tabella dell'IA in proporzione esatta (minimo / mediana). Con la percentuale troncata un KO sicuro
// (minimo = PS) restava a 1 PS nel ramo "colpo basso" e pagava una penalità che non esiste.
static s32 LowRollDmg(const struct PlanState *st, const struct PlanAction *a, enum BattlerId t, s32 dmg)
{
    if (CUSTOM_VGC_PLAN_LOW_ROLL_EXACT && a->moveIndex < MAX_MON_MOVES && st->m[a->user].partyIdx < 0 && st->m[t].partyIdx < 0)
    {
        const struct SimulatedDamage *sd = &gAiLogicData->simulatedDmg[a->user][t][a->moveIndex];
        if (sd->median > 0)
            return (s32)(((s64)dmg * sd->minimum + sd->median - 1) / sd->median);
    }
    return dmg * (s32)LowRollPct(st, a, t) / 100;
}
// ---- fine KO sicuri ----

static s32 DamageOf(const struct PlanState *st, const struct PlanAction *a, enum BattlerId t)
{
    const struct PlanMon *am = &st->m[a->user], *dm = &st->m[t];
    s32 dmg;

    PLAN_COUNT(sCntDmg);
    if (AbilityBlocksMove(am, dm, a->move, IsBattlerAlly(a->user, t)))
        return 0;
    if (a->moveIndex != 0xFF && am->partyIdx < 0 && dm->partyIdx < 0 && !am->megaNow && !dm->megaNow)
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
        dmg = dmg * TerrainFactor(st->terrain, am, dm, a->move) / 100;
    }
    // Terreno cambiato nella simulazione: la tabella ha quello di partenza.
    if (st->terrain != st->terrain0 && a->moveIndex != 0xFF && am->partyIdx < 0 && dm->partyIdx < 0 && !am->megaNow && !dm->megaNow)
        dmg = dmg * TerrainFactor(st->terrain, am, dm, a->move) / max(TerrainFactor(st->terrain0, am, dm, a->move), 1);
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
    if (a->user + 1 == sLowRollBattler)
        dmg = LowRollDmg(st, a, t, dmg);
    else if (a->user + 1 == sHighRollBattler)
        dmg = dmg * HighRollPct(st, a, t) / 100;
    // Niente precisione qui: il danno è quello di un colpo andato a segno. La probabilità di mancare la
    // conta il pianificatore a parte (ramo "mancato", vedi ActionAccuracy e MissPenalty).
    return dmg;
}

// Battler la cui azione manca il bersaglio nella simulazione in corso (ramo "mancato"), più 1; 0 = nessuno.
static EWRAM_DATA u8 sMissBattler = 0;
// After You riuscito nell'azione appena applicata: il compagno (più 1) che deve agire subito dopo; 0 = nessuno.
static EWRAM_DATA u8 sAfterYou = 0;


// Precisione (0..100) dell'azione `a` sul suo bersaglio: dalla tabella dell'IA quando c'è (comprende
// Compound Eyes, No Guard, meteo...), altrimenti quella della mossa. 100 = non può mancare.
// Bersaglio della mossa nella simulazione: come GetBattlerMoveTargetType, ma col terreno simulato (Expanding Force
// sul Campo Psichico colpisce entrambi gli avversari se chi la usa tocca terra).
static enum MoveTarget SimMoveTarget(const struct PlanState *st, enum BattlerId u, u32 move)
{
    if (GetMoveEffect(move) == EFFECT_TERRAIN_BOOST && GetMoveTerrainBoost_HitsBothFoes(move)
     && st->terrain == GetMoveTerrainBoost_Terrain(move) && IsGroundedInSim(&st->m[u]))
        return TARGET_BOTH;
    return GetMoveTarget(move);
}

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
    {
        u8 sub = st->m[u].subHp;
        memcpy(stages, st->m[u].stage, sizeof(stages));
        LoadPartyMon(st, u, idx);
        memcpy(st->m[u].stage, stages, sizeof(stages));
        st->m[u].subHp = sub;
    }
}

// U-turn, Volt Switch, Parting Shot: chi esce lascia il posto al Pokémon più adatto della panchina (scelto dal
// motore), senza stadi. Solo per noi: la panchina del giocatore non si conosce abbastanza.
static void SwitchOutInSim(struct PlanState *st, enum BattlerId u)
{
    struct Pokemon *party = GetBattlerParty(u);
    enum BattlerId p = GetPartnerBattler(u);
    u32 myIdx = (st->m[u].partyIdx >= 0) ? (u32)st->m[u].partyIdx : gBattlerPartyIndexes[u];
    u32 pIdx = (st->m[p].partyIdx >= 0) ? (u32)st->m[p].partyIdx : gBattlerPartyIndexes[p];
    u32 idx = gAiLogicData->mostSuitableMonId[u];

    if (IsOnPlayerSide(u))
        return;
    if (!CanBatonPassTo(party, idx, myIdx, pIdx))
    {
        for (idx = 0; idx < PARTY_SIZE; idx++)
            if (CanBatonPassTo(party, idx, myIdx, pIdx))
                break;
        if (idx == PARTY_SIZE)
            return;
    }
    LoadPartyMon(st, u, idx);
}

// Priorità con l'abilità che il Pokémon ha ORA nella simulazione (può cambiare a metà turno, es. Simple Beam).
// Calcolata qui e non con GetBattleMovePriority del motore: quella REGISTRA l'abilità (Prankster) e scrive
// gProtectStructs, effetti collaterali che una simulazione non deve avere; ed è anche più lenta.
static s32 ActionPriority(const struct PlanState *st, const struct PlanAction *a)
{
    const struct PlanMon *um = &st->m[a->user];
    s32 pr;

    switch (a->kind)
    {
    case PLAN_ACT_SWITCH:  return 9;
    case PLAN_ACT_PROTECT: return 4;
    case PLAN_ACT_NONE:    return -8;
    default:               break;
    }
    pr = GetMovePriority(a->move);
    if (um->ability == ABILITY_GALE_WINGS && um->hp == um->maxHp && GetMoveType(a->move) == TYPE_FLYING)
        pr++;
    else if (um->ability == ABILITY_PRANKSTER && IsBattleMoveStatus(a->move))
        pr++;
    else if (um->ability == ABILITY_TRIAGE && IsHealingMove(a->move))
        pr += 3;
    else if (GetMoveEffect(a->move) == EFFECT_GRASSY_GLIDE && st->terrain == B_TERRAIN_GRASSY && IsGroundedInSim(um))
        pr++;
    return pr;
}

// Mycelium Might: le mosse di stato agiscono per ultime nella loro fascia di priorità.
static bool32 MovesLastInBracket(const struct PlanState *st, const struct PlanAction *a)
{
    return a->kind == PLAN_ACT_MOVE && st->m[a->user].ability == ABILITY_MYCELIUM_MIGHT && IsBattleMoveStatus(a->move);
}

static void TakeDamage(struct PlanMon *tm, s32 dmg)
{
    // Focus Sash noto/assunto: regge il primo colpo a PS pieni.
    if (tm->hp == tm->maxHp && dmg >= tm->hp && tm->holdEffect == HOLD_EFFECT_FOCUS_SASH && !tm->berryUsed)
    {
        dmg = tm->hp - 1;
        tm->berryUsed = TRUE;
        tm->unburden = TRUE;
    }
    tm->hp -= dmg;
    if (dmg > 0)
        tm->damaged = TRUE;
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
        tm->unburden = TRUE;
    }
}

// Il sonno (mossa `move` di `um`, MOVE_NONE = Yawn che scatta a fine turno) non prende `tm`: Erba e Overcoat contro le
// polveri, terreni Elettrico e Nebbioso, abilità. Mycelium Might: le mosse di stato ignorano le abilità del bersaglio.
static bool32 SimSleepBlocked(const struct PlanState *st, const struct PlanMon *um, const struct PlanMon *tm, enum Move move)
{
    enum Ability defAbility = (um->ability == ABILITY_MYCELIUM_MIGHT && um != tm) ? ABILITY_NONE : tm->ability;
    if (move != MOVE_NONE && IsPowderMove(move) && (tm->t1 == TYPE_GRASS || tm->t2 == TYPE_GRASS || defAbility == ABILITY_OVERCOAT
                                                 || tm->holdEffect == HOLD_EFFECT_SAFETY_GOGGLES))
        return TRUE;
    if ((st->terrain == B_TERRAIN_ELECTRIC || st->terrain == B_TERRAIN_MISTY) && IsGroundedInSim(tm))
        return TRUE;
    return defAbility == ABILITY_INSOMNIA || defAbility == ABILITY_VITAL_SPIRIT || defAbility == ABILITY_SWEET_VEIL
        || defAbility == ABILITY_GOOD_AS_GOLD || defAbility == ABILITY_MAGIC_BOUNCE || defAbility == ABILITY_PURIFYING_SALT
        || defAbility == ABILITY_COMATOSE;
}

// Azione di chi è sotto Encore: l'ultima mossa usata, sul bersaglio scelto se è un avversario (altrimenti il primo
// avversario vivo), con lo slot vero se la mossa è nota (tabella dei danni).
static const struct PlanAction *EncoredAction(const struct PlanState *st, const struct PlanAction *a, struct PlanAction *out)
{
    enum BattlerId u = a->user;
    enum Move m = st->m[u].lastMove;
    enum MoveTarget tt = GetMoveTarget(m);
    u32 i;

    *out = *a;
    out->move = m;
    out->moveIndex = 0xFF;
    out->kind = (GetMoveEffect(m) == EFFECT_PROTECT && GetMoveProtectMethod(m) == PROTECT_NORMAL) ? PLAN_ACT_PROTECT : PLAN_ACT_MOVE;
    if (st->m[u].partyIdx < 0)
    {
        enum Move *moves = GetMovesArray(u);
        for (i = 0; i < MAX_MON_MOVES; i++)
            if (moves[i] == m)
                out->moveIndex = i;
    }
    if (tt == TARGET_USER || tt == TARGET_USER_AND_ALLY || tt == TARGET_FIELD || tt == TARGET_OPPONENTS_FIELD
     || tt == TARGET_ALL_BATTLERS || out->kind == PLAN_ACT_PROTECT)
        out->target = u;
    else if (tt == TARGET_ALLY || tt == TARGET_USER_OR_ALLY)
        out->target = GetPartnerBattler(u);
    else if (a->target >= gBattlersCount || IsBattlerAlly(u, a->target))
        out->target = st->m[GetBattlerLeftFoe(u)].alive ? GetBattlerLeftFoe(u) : GetBattlerRightFoe(u);
    return out;
}

static void ApplyActionImpl(struct PlanState *st, const struct PlanAction *a);
// Profilo (solo test): righe di schermo passate dentro la funzione, sommate (VGCPROF).
static void ApplyAction(struct PlanState *st, const struct PlanAction *a)
{
#if TESTING
    u32 t0 = REG_VCOUNT;
    ApplyActionImpl(st, a);
    sProf[2] += (REG_VCOUNT + 228 - t0) % 228;
    return;
#else
    ApplyActionImpl(st, a);
#endif
}

static void ApplyActionImpl(struct PlanState *st, const struct PlanAction *a)
{
    enum BattlerId u = a->user;
    struct PlanMon *um = &st->m[u];
    enum BattleMoveEffects effect;
    enum MoveTarget tt;
    struct PlanAction enc;

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
    // Encore: chi ne è colpito ripete l'ultima mossa, qualunque azione avesse scelto (il cambio resta libero).
    if (um->encored && um->lastMove != MOVE_NONE && a->move != um->lastMove)
        a = EncoredAction(st, a, &enc);
    if (a->kind == PLAN_ACT_PROTECT)
    {
        // Un secondo Protect di fila riesce solo 1 volta su 3: nel simulatore si considera fallito.
        um->protecting = !um->protectedLast;
        um->lastMove = a->move;
        return;
    }

    effect = GetMoveEffect(a->move);
    tt = SimMoveTarget(st, u, a->move);
    // Ramo "mancato": l'azione verso gli avversari non ha effetto (le mosse su sé stessi o sul compagno non mancano).
    if (u + 1 == sMissBattler && ActionAccuracy(st, a) < 100)
    {
        um->lastMove = a->move;
        return;
    }
    // Taunt: niente mosse di stato (Trick Room, Tailwind, setup, Spore...) finché dura.
    if (um->taunted && IsBattleMoveStatus(a->move))
        return;
    um->lastMove = a->move;
    // Campo Psichico: una mossa con priorità (anche da Prankster) su un avversario a terra fallisce.
    if (st->terrain == B_TERRAIN_PSYCHIC && !IsBattlerAlly(u, a->target) && IsGroundedInSim(&st->m[a->target])
     && !IsSpreadMove(tt) && ActionPriority(st, a) > 0)
        return;
    // Fake Out e simili falliscono del tutto dopo il primo turno in campo (niente danno).
    if (effect == EFFECT_FIRST_TURN_ONLY && !(st->firstTurn && IsBattlersFirstTurn(u)))
        return;
    // Focus Punch fallisce se chi la usa è già stato colpito nel turno; le mosse a due turni caricano (colpiscono
    // il turno dopo, fuori dall'orizzonte): Solar Beam è immediata solo al sole, le altre con la Power Herb.
    if (effect == EFFECT_FOCUS_PUNCH && um->damaged)
        return;
    if ((effect == EFFECT_SOLAR_BEAM && st->weather != PW_SUN && um->holdEffect != HOLD_EFFECT_POWER_HERB)
     || ((effect == EFFECT_TWO_TURNS_ATTACK || effect == EFFECT_SKY_DROP) && um->holdEffect != HOLD_EFFECT_POWER_HERB))
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
    case EFFECT_TAUNT:
        {
            struct PlanMon *tm = &st->m[a->target];
            // Oblivious, Aroma Veil e Magic Bounce la bloccano (Mold Breaker no: semplificato).
            if (tm->alive && !tm->protecting && tm->ability != ABILITY_OBLIVIOUS && tm->ability != ABILITY_AROMA_VEIL
             && tm->ability != ABILITY_MAGIC_BOUNCE && tm->holdEffect != HOLD_EFFECT_MENTAL_HERB)
                tm->taunted = 3; // questo turno (se non ha ancora agito) e i 2 dopo
        }
        return;
    case EFFECT_ENCORE:
        {
            // Blocca il bersaglio sulla sua ultima mossa per 3 turni (questo compreso, se non ha ancora agito).
            struct PlanMon *tm = &st->m[a->target];
            if (tm->alive && !tm->protecting && !tm->encored && !IsBattlerAlly(u, a->target)
             && tm->lastMove != MOVE_NONE && !IsMoveEncoreBanned(tm->lastMove)
             && tm->ability != ABILITY_AROMA_VEIL && tm->ability != ABILITY_MAGIC_BOUNCE && tm->holdEffect != HOLD_EFFECT_MENTAL_HERB)
                tm->encored = 3;
        }
        return;
    case EFFECT_AFTER_YOU:
        // Il compagno agisce subito dopo (RunTurn sposta la sua azione), se non ha già agito.
        if (a->target < gBattlersCount && a->target != u && IsBattlerAlly(u, a->target) && st->m[a->target].alive)
            sAfterYou = a->target + 1;
        return;
    case EFFECT_YAWN:
        {
            // Sonnolenza: si addormenta a fine del turno DOPO (non subito, come faceva quando passava per il sonno).
            struct PlanMon *tm = &st->m[a->target];
            if (tm->alive && !tm->protecting && tm->subHp == 0 && !tm->asleep && !tm->burned && !tm->paralyzed && !tm->drowsy
             && !SimSleepBlocked(st, um, tm, a->move))
                tm->drowsy = 2;
        }
        return;
    case EFFECT_BATON_PASS:
        if (!IsOnPlayerSide(u))
            BatonPassInSim(st, u);
        return;
    case EFFECT_REFLECT:
        st->screens[GetBattlerSide(u)] |= 1;
        if (um->holdEffect == HOLD_EFFECT_LIGHT_CLAY)
            st->screenClay[GetBattlerSide(u)] = TRUE;
        return;
    case EFFECT_LIGHT_SCREEN:
        st->screens[GetBattlerSide(u)] |= 2;
        if (um->holdEffect == HOLD_EFFECT_LIGHT_CLAY)
            st->screenClay[GetBattlerSide(u)] = TRUE;
        return;
    case EFFECT_AURORA_VEIL:
        if (st->weather == PW_SNOW)
        {
            st->screens[GetBattlerSide(u)] |= 3;
            if (um->holdEffect == HOLD_EFFECT_LIGHT_CLAY)
                st->screenClay[GetBattlerSide(u)] = TRUE;
        }
        return;
    case EFFECT_TRICK_ROOM:
        // Dura 5 turni compreso questo (NextTurn toglie il primo a fine turno).
        st->trickRoom = st->trickRoom ? 0 : 5;
        return;
    case EFFECT_TAILWIND:
        if (!st->tailwind[GetBattlerSide(u)])
            st->tailwind[GetBattlerSide(u)] = 4;
        return;
    case EFFECT_SUBSTITUTE:
        if (um->subHp == 0 && um->hp > um->maxHp / 4)
        {
            um->hp -= um->maxHp / 4;
            um->subHp = max(1, um->maxHp / 4);
        }
        return;
    case EFFECT_PARTING_SHOT:
        {
            struct PlanMon *tm = &st->m[a->target];
            if (tm->alive && !tm->protecting && tm->subHp == 0 && tm->ability != ABILITY_CLEAR_BODY && tm->ability != ABILITY_WHITE_SMOKE
             && tm->ability != ABILITY_FULL_METAL_BODY && tm->holdEffect != HOLD_EFFECT_CLEAR_AMULET)
            {
                tm->stage[PS_ATK] = max(-6, tm->stage[PS_ATK] - 1);
                tm->stage[PS_SPA] = max(-6, tm->stage[PS_SPA] - 1);
                SwitchOutInSim(st, u);
            }
        }
        return;
    case EFFECT_TERRAIN:
        st->terrain = GetMoveTerrainType(a->move);
        st->terrainTurns = 5;
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
        bool32 blocked = !tm->alive || (tm->protecting && !IsBattlerAlly(u, a->target))
                      || (tm->subHp > 0 && !IsBattlerAlly(u, a->target) && !IsSoundMove(a->move) && um->ability != ABILITY_INFILTRATOR);

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
              && !blocked && !tm->asleep && !tm->burned && !tm->paralyzed)
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
        else if (GetMoveNonVolatileStatus(a->move) == MOVE_EFFECT_SLEEP && !blocked && !tm->asleep)
        {
            if (!SimSleepBlocked(st, um, tm, a->move))
            {
                tm->asleep = TRUE;
                tm->sleepTurns = 2; // 1-3 turni: si usa il valore medio
            }
        }
        return;
    }

    // Mossa d'attacco.
    {
        enum BattlerId targets[3];
        u32 n = 0, i;
        bool32 spread = IsSpreadMove(tt);

        if (spread)
        {
            enum BattlerId fl = GetBattlerLeftFoe(u), fr = GetBattlerRightFoe(u), p = GetPartnerBattler(u);
            if (st->m[fl].alive) targets[n++] = fl;
            if (st->m[fr].alive) targets[n++] = fr;
            // Earthquake, Surf, Discharge... colpiscono anche il compagno (Protect, Wide Guard e immunità
            // valgono come per gli avversari: tipo dalla tabella o dalla stima, abilità in DamageOf).
            if (tt == TARGET_FOES_AND_ALLY && p != u && st->m[p].alive)
                targets[n++] = p;
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
            // Sostituto: assorbe il colpo (e il tentennamento) finché regge; i colpi sonori e Infiltrator lo superano.
            if (tm->subHp > 0 && !IsBattlerAlly(u, t) && !IsSoundMove(a->move) && um->ability != ABILITY_INFILTRATOR)
            {
                tm->subHp = (dmg >= tm->subHp) ? 0 : tm->subHp - dmg;
                continue;
            }
            if (effect == EFFECT_FIRST_TURN_ONLY && st->firstTurn && IsBattlersFirstTurn(u) && tm->ability != ABILITY_INNER_FOCUS)
                tm->flinched = TRUE;
            if (a->move == MOVE_ICY_WIND || a->move == MOVE_ELECTROWEB || a->move == MOVE_BULLDOZE)
                tm->stage[PS_SPE] = max(-6, tm->stage[PS_SPE] - 1);
            // Disguise: il primo colpo rompe il travestimento e costa solo 1/8 dei PS (Gen 8+), qualunque fosse il
            // danno. Senza, Mimikyu andava KO al turno 1 nella simulazione e Trick Room sembrava una perdita.
            if (tm->disguise && tm->ability == ABILITY_DISGUISE && um->ability != ABILITY_MOLD_BREAKER
             && um->ability != ABILITY_TERAVOLT && um->ability != ABILITY_TURBOBLAZE)
            {
                tm->disguise = FALSE;
                dmg = (GetConfig(B_DISGUISE_HP_LOSS) >= GEN_8) ? max(tm->maxHp / 8, 1) : 0;
                if (dmg == 0)
                    continue;
            }
            TakeDamage(tm, dmg);
        }
        if (effect == EFFECT_FINAL_GAMBIT)
        {
            um->hp = 0;
            um->alive = FALSE;
        }
        if (effect == EFFECT_HIT_ESCAPE && um->alive)
            SwitchOutInSim(st, u);
    }
}

// ---------------------------------------------------------------------
// Valutazione della posizione
// ---------------------------------------------------------------------
// Probabilità (0..100) che `atk` mandi KO `def` al turno dopo con una sua mossa: la precisione della mossa
// migliore che basta (0 = nessuna).
static u32 CanKoNextTurnUncachedImpl(const struct PlanState *st, enum BattlerId atk, enum BattlerId def);
// Profilo (solo test): righe di schermo passate dentro la funzione, sommate (VGCPROF).
static u32 CanKoNextTurnUncached(const struct PlanState *st, enum BattlerId atk, enum BattlerId def)
{
#if TESTING
    u32 t0 = REG_VCOUNT;
    u32 r = CanKoNextTurnUncachedImpl(st, atk, def);
    sProf[3] += (REG_VCOUNT + 228 - t0) % 228;
    return r;
#else
    return CanKoNextTurnUncachedImpl(st, atk, def);
#endif
}

static u32 CanKoNextTurnUncachedImpl(const struct PlanState *st, enum BattlerId atk, enum BattlerId def)
{
    PLAN_COUNT(sCntKo);
    const struct PlanMon *am = &st->m[atk], *dm = &st->m[def];
    u32 i, best = 0;

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
                best = max(best, ActionAccuracy(st, &tmp));
        }
        // Un avversario con mosse ancora non viste non è innocuo: minaccia con le sue STAB probabili
        // (come pensa un giocatore: "quel Garchomp ha sicuramente un attacco Terra").
        if (best >= 100 || !IsOnPlayerSide(atk) || seen >= MAX_MON_MOVES)
            return best;
    }
    if (am->partyIdx < 0 && IsOnPlayerSide(atk) && sJob.nLikely[atk] > 0)
    {
        // Belief: le mosse probabili non viste, pesate per quanto sono probabili.
        for (i = 0; i < sJob.nLikely[atk]; i++)
        {
            enum Move m = sJob.likelyMove[atk][i];
            u32 acc = GetMoveAccuracy(m);
            if (AbilityBlocksMove(am, dm, m, FALSE))
                continue;
            if (EstimateDamage(st, am, dm, m) >= dm->hp)
                best = max(best, ((acc == 0 || acc > 100) ? 100 : acc) * sJob.likelyP[atk][i] / 100);
        }
        return best;
    }
    {
        enum Move cand[16];
        u32 n = Scout_CollectLearnableAttacks(am->species, cand, ARRAY_COUNT(cand));
        for (i = 0; i < n; i++)
        {
            u32 acc = GetMoveAccuracy(cand[i]);
            if (GetMoveType(cand[i]) != am->t1 && GetMoveType(cand[i]) != am->t2)
                continue;
            if (IsUnreliableGuess(cand[i]))
                continue;
            if (AbilityBlocksMove(am, dm, cand[i], FALSE))
                continue;
            if (EstimateDamage(st, am, dm, cand[i]) >= dm->hp)
                best = max(best, (acc == 0 || acc > 100) ? 100 : acc);
        }
        return best;
    }
}

// Fake Out ancora disponibile al turno dopo: solo chi è entrato durante la simulazione (chi era in campo
// all'inizio l'ha già "speso" con il primo turno). Del giocatore: se la specie lo porta di solito.
// Cache di CanKoNextTurn per il lavoro in corso: la stessa domanda (stessi Pokémon, PS del bersaglio, stadi,
// abilità, meteo, schermi) torna in quasi ogni foglia dell'albero. La chiave contiene tutto ciò da cui dipende
// il danno nella simulazione; a collisione si ricalcola e si sovrascrive.
PLAN_HOT static u32 CanKoNextTurn(const struct PlanState *st, enum BattlerId atk, enum BattlerId def)
{
    const struct PlanMon *am = &st->m[atk], *dm = &st->m[def];
    struct PlanKoCache *e;
    u32 k1 = 0x80000000u | atk | (def << 2) | (((u32)(am->partyIdx + 1) & 7) << 4) | (((u32)(dm->partyIdx + 1) & 7) << 7)
           | ((st->weather & 7u) << 10) | ((st->screens[GetBattlerSide(def)] & 3u) << 13) | ((u32)am->burned << 15)
           | ((u32)am->helping << 16) | ((u32)am->burned0 << 17) | ((u32)st->screens[GetBattlerSide(atk)] << 18)
           | ((st->terrain & 7u) << 20);
    u32 k2 = ((u32)dm->hp & 0xFFFF) | (((u32)(am->stage[PS_ATK] + 6) & 15) << 16) | (((u32)(am->stage[PS_SPA] + 6) & 15) << 20)
           | (((u32)(dm->stage[PS_DEF] + 6) & 15) << 24) | (((u32)(dm->stage[PS_SPD] + 6) & 15) << 28);
    u32 k3 = ((u32)am->ability & 0xFFFF) | ((u32)dm->ability << 16);

    e = &sJob.koCache[(k1 * 2654435761u ^ k2 * 40503u ^ k3 * 97u) % PLAN_KO_CACHE];
    if (e->k1 == k1 && e->k2 == k2 && e->k3 == k3)
        return e->val;
    e->k1 = k1;
    e->k2 = k2;
    e->k3 = k3;
    e->val = CanKoNextTurnUncached(st, atk, def);
    return e->val;
}

static bool32 FakeOutReady(const struct PlanState *st, enum BattlerId b)
{
    const struct PlanMon *pm = &st->m[b];
    u32 i;

    if (pm->partyIdx < 0)
        return FALSE;
    if (IsOnPlayerSide(b))
    {
        struct ScoutProfile prof;
        Scout_GetPartyMonProfile(b, pm->species, &prof);
        return prof.role[SCOUT_FAKE_OUT] >= 50;
    }
    for (i = 0; i < MAX_MON_MOVES; i++)
        if (GetMoveEffect(GetMonData(&GetBattlerParty(b)[pm->partyIdx], MON_DATA_MOVE1 + i)) == EFFECT_FIRST_TURN_ONLY)
            return TRUE;
    return FALSE;
}

// Valore della posizione dal nostro punto di vista (positivo = meglio per noi). Pesi in include/config/custom.h
// (CUSTOM_VGC_EVAL_*). Tutto è simmetrico: ciò che vale per un nostro Pokémon vale con il segno opposto per uno
// del giocatore.
// Minaccia di un Pokémon sul lato opposto (0..150): il danno migliore che fa a uno dei bersagli vivi, in % dei PS
// che gli restano (fino a 100), più 25 per ogni bersaglio che può mandare KO. La forma Mega in campo entra nelle
// statistiche della stima. (Provata anche la somma sui bersagli + 25 alla Mega con scala /60: rompe due test di
// squadra, Carletto/Ludicolo e Tony/Charizard.) Mosse: le sue se nostro, quelle viste
// + le probabili (belief) o le STAB plausibili della specie se del giocatore: mai i dati nascosti. Stadi, forma Mega,
// meteo e strumenti noti entrano nella stima del danno.
static s32 ThreatPct(const struct PlanState *st, enum BattlerId atk)
{
    const struct PlanMon *am = &st->m[atk];
    enum BattlerId def;
    s32 best = 0, kos = 0;

    if (!am->alive || am->species == SPECIES_NONE)
        return 0;
    for (def = 0; def < gBattlersCount; def++)
    {
        const struct PlanMon *dm = &st->m[def];
        enum Move cand[16];
        u32 n = 0, i, seen = 0;
        s32 pct = 0;

        if (IsBattlerAlly(atk, def) || !dm->alive || dm->species == SPECIES_NONE || dm->hp == 0)
            continue;
        if (am->partyIdx < 0)
        {
            enum Move *moves = GetMovesArray(atk);
            for (i = 0; i < MAX_MON_MOVES; i++)
                if (moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE)
                {
                    seen++;
                    if (!IsBattleMoveStatus(moves[i]) && n < ARRAY_COUNT(cand))
                        cand[n++] = moves[i];
                }
        }
        if (IsOnPlayerSide(atk) && seen < MAX_MON_MOVES)
        {
            if (am->partyIdx < 0 && sJob.nLikely[atk] > 0)
            {
                for (i = 0; i < sJob.nLikely[atk] && n < ARRAY_COUNT(cand); i++)
                    cand[n++] = sJob.likelyMove[atk][i];
            }
            else
            {
                enum Move learn[16];
                u32 nl = Scout_CollectLearnableAttacks(am->species, learn, ARRAY_COUNT(learn));
                for (i = 0; i < nl && n < ARRAY_COUNT(cand); i++)
                    if ((GetMoveType(learn[i]) == am->t1 || GetMoveType(learn[i]) == am->t2) && !IsUnreliableGuess(learn[i]))
                        cand[n++] = learn[i];
            }
        }
        for (i = 0; i < n; i++)
        {
            u32 acc = GetMoveAccuracy(cand[i]);
            s32 d;
            if (AbilityBlocksMove(am, dm, cand[i], FALSE))
                continue;
            d = EstimateDamage(st, am, dm, cand[i]) * 100 / dm->hp;
            if (d > 100)
                d = 100;
            d = d * (s32)((acc == 0 || acc > 100) ? 100 : acc) / 100;
            pct = max(pct, d);
        }
        best = max(best, pct);
        if (pct >= 100)
            kos++;
    }
    return best + 25 * kos;
}

// Valore del sonno di un Pokémon nella posizione: quanto pesa togliere di mezzo LUI. Minaccia calcolata a inizio
// lavoro (sJob.sleepW) e boost offensivi presi nella simulazione; chi entra durante la simulazione vale la base.
static s32 SleepValue(const struct PlanState *st, enum BattlerId b)
{
    const struct PlanMon *pm = &st->m[b];
    s32 w = (pm->partyIdx < 0 && sJob.sleepW[b] > 0) ? sJob.sleepW[b] : CUSTOM_VGC_EVAL_SLEEP;
    s32 off = max(pm->stage[PS_ATK], pm->stage[PS_SPA]);
    if (off > 0)
        w = w * (100 + 25 * min(off, 4)) / 100;
    // Oltre l'orizzonte: ogni turno di sonno rimasto evita un turno della sua minaccia (danno in % sul nostro lato,
    // + KO possibili). Metà peso (si sveglia a caso, può cambiare), al massimo 2 turni.
    if (pm->partyIdx < 0 && sJob.sleepW[b] > 0 && pm->sleepTurns > 0)
        w = max(w, sJob.threatPct[b] * min(pm->sleepTurns, 2) / 2);
    return w;
}

// Encore su una mossa che non attacca, ancora attivo il turno dopo (a fine turno simulato encored conta anche quello
// appena giocato).
static bool32 LockedIntoStatus(const struct PlanMon *pm)
{
    return pm->encored > 1 && pm->lastMove != MOVE_NONE && IsBattleMoveStatus(pm->lastMove);
}

// Correzioni dei misplay accese per chi pensa. Spente solo nella verifica a specchio (CUSTOM_VGC_MIRROR_FIXES, self-play)
// per il lato con il bit di verifica-regole.
static bool32 MisplayFixesOn(void)
{
#if TESTING
    if (CUSTOM_VGC_MIRROR_FIXES)
        return !((gVgcTraining.rulesLegacy >> GetBattlerSide(sJob.me)) & 1);
#endif
    return TRUE;
}

// Probabilità (0..100) che `b` vada KO al turno dopo per mano di un avversario sveglio (la più alta).
static u32 MaxKoThreat(const struct PlanState *st, enum BattlerId b)
{
    enum BattlerId o;
    u32 best = 0;
    for (o = 0; o < gBattlersCount; o++)
    {
        if (IsBattlerAlly(b, o) || !st->m[o].alive || st->m[o].asleep || st->m[o].species == SPECIES_NONE)
            continue;
        best = max(best, CanKoNextTurn(st, o, b));
    }
    return best;
}

// Modello del giocatore "contro di noi fermi" (CUSTOM_VGC_PLAN_FOE_VS_IDLE). Nella verifica a specchio
// (CUSTOM_VGC_MIRROR_FOE_IDLE, self-play) acceso per il lato senza il bit di verifica-regole, spento per l'altro.
static bool32 FoeVsIdleOn(void)
{
#if TESTING
    if (CUSTOM_VGC_MIRROR_FOE_IDLE)
        return !((gVgcTraining.rulesLegacy >> GetBattlerSide(sJob.me)) & 1);
#endif
    return CUSTOM_VGC_PLAN_FOE_VS_IDLE && MisplayFixesOn();
}

// Storia dei bersagli del giocatore (CUSTOM_VGC_PLAN_FOE_HISTORY); verifica a specchio come FoeVsIdleOn.
static bool32 FoeHistoryOn(void)
{
#if TESTING
    if (CUSTOM_VGC_MIRROR_FOE_HISTORY)
        return !((gVgcTraining.rulesLegacy >> GetBattlerSide(sJob.me)) & 1);
#endif
    return CUSTOM_VGC_PLAN_FOE_HISTORY && MisplayFixesOn();
}

// PS di chi è uscito e di chi è entrato con un cambio (o un pivot) nella simulazione. Il valore della posizione conta
// solo i PS in campo: uscire con un Pokémon al 30% per far prendere il KO a uno al 100% non costava niente (stesso KO in
// campo, stesso materiale), e il pianificatore sacrificava i Pokémon sani al posto di quelli già consumati. Chi esce
// porta in panchina i suoi PS di inizio turno; chi entra lascia in panchina quelli che aveva.
static s32 BenchSwapValue(const struct PlanState *st)
{
    enum BattlerId b;
    s32 v = 0;

    for (b = 0; b < gBattlersCount; b++)
    {
        const struct PlanMon *pm = &st->m[b], *pm0 = &sJob.base.m[b];
        s32 side = IsOnPlayerSide(b) ? -1 : 1;
        struct Pokemon *party;
        u32 inIdx, outIdx;
        s32 inPct, outPct, inW = 100, outW = 100, inMax;

        if (pm->partyIdx < 0 || pm->species == SPECIES_NONE || !pm0->alive || pm0->species == SPECIES_NONE)
            continue;
        inIdx = (u32)pm->partyIdx;
        outIdx = gBattlerPartyIndexes[b];
        if (inIdx == outIdx || inIdx >= PARTY_SIZE)
            continue;
        party = GetBattlerParty(b);
        inMax = GetMonData(&party[inIdx], MON_DATA_MAX_HP);
        inPct = (inMax > 0) ? (s32)GetMonData(&party[inIdx], MON_DATA_HP) * 100 / inMax : 0;
        outPct = pm0->hp * 100 / max(pm0->maxHp, 1);
        if (side > 0)
        {
            if (sJob.winconParty & (1u << inIdx))
                inW = VW(wincon);
            if (outIdx < PARTY_SIZE && (sJob.winconParty & (1u << outIdx)))
                outW = VW(wincon);
        }
        v += side * (outPct * outW - inPct * inW) / 100 * CUSTOM_VGC_EVAL_BENCH_HP_PCT / 100;
    }
    return v;
}

PLAN_HOT static s32 EvaluateImpl(const struct PlanState *st);
// Profilo (solo test): righe di schermo passate dentro la funzione, sommate (VGCPROF).
static s32 Evaluate(const struct PlanState *st)
{
#if TESTING
    u32 t0 = REG_VCOUNT;
    s32 r = EvaluateImpl(st);
    sProf[0] += (REG_VCOUNT + 228 - t0) % 228;
    return r;
#else
    return EvaluateImpl(st);
#endif
}

PLAN_HOT static s32 EvaluateImpl(const struct PlanState *st)
{
    PLAN_COUNT(sCntEval);
    enum BattlerId b, o;
    s32 v = 0;
    s32 count[NUM_BATTLE_SIDES] = {0}, raceSum = 0;
    u32 spd[MAX_BATTLERS_COUNT], spdRaw[MAX_BATTLERS_COUNT];

    // Velocità effettive una volta sola (servono a ogni confronto sotto); spdRaw senza Tailwind.
    for (b = 0; b < gBattlersCount; b++)
    {
        spd[b] = st->m[b].alive ? EffSpeed(st, b) : 0;
        spdRaw[b] = st->tailwind[GetBattlerSide(b)] ? spd[b] / 2 : spd[b];
    }
#define FASTER(x, y) (st->trickRoom ? spd[x] <= spd[y] : spd[x] > spd[y])

    for (b = 0; b < gBattlersCount; b++)
    {
        s32 side = IsOnPlayerSide(b) ? -1 : 1;
        const struct PlanMon *pm = &st->m[b];

        if (pm->species == SPECIES_NONE)
            continue;
        // Peso per ruolo: la win condition del piano (per il nostro lato) vale di più, viva e in PS.
        {
            u32 pIdx = (pm->partyIdx >= 0) ? (u32)pm->partyIdx : gBattlerPartyIndexes[b];
            s32 roleW = (side > 0 && pIdx < PARTY_SIZE && (sJob.winconParty & (1u << pIdx))) ? VW(wincon) : 100;
            if (!pm->alive)
            {
                v -= side * VW(ko) * roleW / 100;
                continue;
            }
            count[GetBattlerSide(b)]++;
            v += side * ((pm->hp + pm->subHp / 2) * 100 / max(pm->maxHp, 1) + VW(alive)) * roleW / 100;
        }
        // Il sonno vale quanto la minaccia di chi dorme (Tyranitar Mega addormentato vale più di Dugtrio).
        if (pm->asleep)
            v -= side * SleepValue(st, b);
        // Yawn: si addormenta a fine del turno dopo, oltre l'orizzonte (può ancora cambiare: metà).
        else if (pm->drowsy)
            v -= side * SleepValue(st, b) / 2;
        // Encore su una mossa che non attacca (Calm Mind, Protect, Tailwind già messo...): nei turni rimasti il Pokémon
        // non fa niente di utile, come chi dorme (resta il cambio, che costa un turno). Fino a 2 turni oltre l'orizzonte.
        if (LockedIntoStatus(pm))
            v -= side * SleepValue(st, b) * min(pm->encored - 1, 2) / 2;
        // Stato: la scottatura pesa sugli attaccanti fisici, la paralisi su tutti (Velocità dimezzata).
        if (pm->burned && pm->atk > pm->spa && pm->ability != ABILITY_GUTS)
            v -= side * CUSTOM_VGC_EVAL_BURN;
        if (pm->paralyzed)
            v -= side * CUSTOM_VGC_EVAL_PARALYSIS;
        // Risorse: Focus Sash intatto (regge un colpo), Fake Out pronto per il turno dopo.
        if (pm->holdEffect == HOLD_EFFECT_FOCUS_SASH && !pm->berryUsed && pm->hp == pm->maxHp)
            v += side * CUSTOM_VGC_EVAL_SASH;
        if (FakeOutReady(st, b))
            v += side * CUSTOM_VGC_EVAL_FAKE_OUT;
        // (Mega non ancora usata: il simulatore non la modella, sarebbe uguale in ogni ramo.)
        // Stadi: valgono se servono a qualcosa (chi attacca, o chi li passa con Baton Pass a qualcuno in panchina).
        if (LockedIntoStatus(pm))
        {
            // Bloccato da Encore: i boost offensivi e di Velocità non servono finché dura (resta la difesa).
            s32 dfn = (pm->stage[PS_DEF] + pm->stage[PS_SPD]) / 2;
            v += side * 6 * max(-4, min(4, dfn));
        }
        else if (pm->canAttack)
        {
            s32 off = (pm->atk >= pm->spa) ? pm->stage[PS_ATK] : pm->stage[PS_SPA];
            s32 dfn = (pm->stage[PS_DEF] + pm->stage[PS_SPD]) / 2;
            s32 sv = 20 * max(-4, min(4, off)) + 12 * max(-4, min(4, pm->stage[PS_SPE])) + 6 * max(-4, min(4, dfn));
            // Boost di chi può andare KO al turno dopo: valgono solo se sopravvive (setup sotto un KO in arrivo).
            if (sv > 0 && MisplayFixesOn())
                sv = sv * (100 - (s32)MaxKoThreat(st, b)) / 100;
            v += side * sv;
        }
        else if (pm->canPass && sJob.bench[GetBattlerSide(b)] > 0)
        {
            // Potenziale trasferibile: i boost di un Baton Passer finiscono su uno sweeper in panchina (scelto dal
            // grafo per sfruttarli) e lì valgono il doppio che su un attaccante qualunque.
            s32 off = max(pm->stage[PS_ATK], pm->stage[PS_SPA]);
            s32 dfn = (pm->stage[PS_DEF] + pm->stage[PS_SPD]) / 2;
            v += side * (40 * max(-4, min(4, off)) + 24 * max(-4, min(4, pm->stage[PS_SPE])) + 8 * max(-4, min(4, dfn)));
        }
    }
#if TESTING
    if (sEvalDebug)
    {
        extern s32 Test_MgbaPrintf(const char *fmt, ...);
        for (b = 0; b < gBattlersCount; b++)
            Test_MgbaPrintf("  VGCMON b%d sp%d p%d hp%d/%d vivo%d st%d/%d/%d/%d/%d atk%d def%d spa%d spd%d vel%d str%d ab%d", b, st->m[b].species,
                            st->m[b].partyIdx, st->m[b].hp, st->m[b].maxHp, st->m[b].alive, st->m[b].stage[PS_ATK], st->m[b].stage[PS_DEF],
                            st->m[b].stage[PS_SPA], st->m[b].stage[PS_SPD], st->m[b].stage[PS_SPE], st->m[b].atk, st->m[b].def, st->m[b].spa,
                            st->m[b].spd, st->m[b].speed, st->m[b].holdEffect, st->m[b].ability);
    }
#endif
    if (MisplayFixesOn())
        v += BenchSwapValue(st);
    EVAL_DEBUG("pokemon", v);
    // Materiale: Pokémon utilizzabili per lato (in campo + panchina). Lo stesso KO vale di più quando cambia
    // l'equilibrio (2 contro 1 pesa più di 4 contro 3): la differenza si scala sul totale rimasto.
    {
        // Lato di chi pensa (in IA contro IA può essere anche quello del giocatore).
        u32 ourSide = GetBattlerSide(sJob.me), theirSide = ourSide ^ BIT_SIDE;
        s32 ours = count[ourSide] + sJob.bench[ourSide] - st->benchUsed[ourSide];
        s32 theirs = count[theirSide] + sJob.bench[theirSide] - st->benchUsed[theirSide];
        v += VW(material) * 2 * (ours - theirs) / max(ours + theirs, 1);
        if (ours == 0 && theirs > 0)
            v -= VW(ko) * 4;
        else if (theirs == 0 && ours > 0)
            v += VW(ko) * 4;
    }
    EVAL_DEBUG("+materiale", v);
    // Velocità al turno dopo, per ogni coppia (nostro, loro). Contro un Pokémon andato KO la coppia conta come
    // vinta da chi resta: mandare KO non fa mai perdere punti di velocità.
    for (b = 0; b < gBattlersCount; b++)
    {
        if (IsOnPlayerSide(b) || st->m[b].species == SPECIES_NONE)
            continue;
        for (o = 0; o < gBattlersCount; o++)
        {
            if (!IsOnPlayerSide(o) || st->m[o].species == SPECIES_NONE || (!st->m[b].alive && !st->m[o].alive))
                continue;
            if (!st->m[o].alive || !st->m[b].alive)
            {
                v += (st->m[b].alive ? 1 : -1) * VW(speedPair) * CUSTOM_VGC_EVAL_SPEED_TURNS;
                continue;
            }
            // Ordine nei prossimi CUSTOM_VGC_EVAL_SPEED_TURNS turni, con i turni rimasti di Trick Room e Tailwind:
            // un Trick Room appena messo vale per i turni in cui dura, non solo per il prossimo.
            // Risorsa rimasta: nei turni in cui il controllo della velocità ROVESCIA l'ordine naturale, l'ordine vale
            // quanto la corsa ai KO tra i due (chi va prima e manda KO evita il colpo dell'altro). Senza, Tailwind,
            // Trick Room & co. rendevano solo nei 2-3 turni simulati e perdevano 150-350 punti contro un attacco.
            {
                u32 t, sb = spdRaw[b], so = spdRaw[o];
                bool32 natural = sb > so;
                s32 race = -1;
                for (t = 1; t <= CUSTOM_VGC_EVAL_SPEED_TURNS; t++)
                {
                    u32 xb = (st->tailwind[GetBattlerSide(b)] > t) ? sb * 2 : sb;
                    u32 xo = (st->tailwind[GetBattlerSide(o)] > t) ? so * 2 : so;
                    bool32 ourFirst = (st->trickRoom > t) ? xb <= xo : xb > xo;
                    v += ourFirst ? VW(speedPair) : -VW(speedPair);
                    if (ourFirst != natural)
                    {
                        if (race < 0)
                            race = (s32)CanKoNextTurn(st, b, o) + (s32)CanKoNextTurn(st, o, b); // 0..200
                        // fino a 3 volte il peso della coppia per turno, quando entrambi si mandano KO
                        raceSum += (ourFirst ? 1 : -1) * VW(speedPair) * (50 + race) / 100 * 3 / 2;
                    }
                }
            }
        }
    }
    // Tetto alla corsa ai KO dei turni futuri: sommata su 4 coppie e fino a 4 turni, con un Trick Room appena messo
    // arrivava a ~1100 punti (più di due KO) e passava davanti a un KO sicuro. Vinta tutta, vale al massimo un KO.
    if (CUSTOM_VGC_EVAL_SPEED_RACE_CAP > 0 && MisplayFixesOn())
        raceSum = max(-CUSTOM_VGC_EVAL_SPEED_RACE_CAP, min(CUSTOM_VGC_EVAL_SPEED_RACE_CAP, raceSum));
    v += raceSum;
    // Schermi messi nella simulazione: valgono anche nei turni dopo l'orizzonte (5 turni in tutto, 8 con Light Clay;
    // i 2-3 simulati li vedono solo in parte). Per ogni schermo nuovo, per ogni avversario vivo che attacca con quella
    // categoria, un terzo del danno evitato nei turni rimasti, stimato come una coppia di velocità per turno.
    {
        u32 side;
        for (side = 0; side < NUM_BATTLE_SIDES; side++)
        {
            u32 fresh = st->screens[side] & ~st->screens0[side], foes = 0;
            if (fresh == 0)
                continue;
            for (o = 0; o < gBattlersCount; o++)
                if (GetBattlerSide(o) != side && st->m[o].alive && st->m[o].species != SPECIES_NONE)
                {
                    bool32 phys = st->m[o].atk >= st->m[o].spa;
                    if ((phys && (fresh & 1)) || (!phys && (fresh & 2)))
                        foes++;
                }
            // 3 turni oltre l'orizzonte (6 con Light Clay)
            {
                s32 val = (s32)foes * VW(speedPair) * (st->screenClay[side] ? 6 : 3);
                bool32 oursSide = (side == GetBattlerSide(sJob.me));
                v += oursSide ? val : -val;
            }
        }
    }
    EVAL_DEBUG("+velocita", v);
    // Minacce di KO al turno dopo, simmetriche e tutte contate (due Pokémon che minacciano lo stesso bersaglio
    // sono due minacce), pesate per la precisione della mossa che basta.
    for (b = 0; b < gBattlersCount; b++)
    {
        if (!st->m[b].alive || st->m[b].species == SPECIES_NONE)
            continue;
        for (o = 0; o < gBattlersCount; o++)
        {
            u32 chance;
            if (!st->m[o].alive || IsBattlerAlly(b, o) || st->m[o].asleep || LockedIntoStatus(&st->m[o]) || st->m[o].species == SPECIES_NONE)
                continue;
            chance = CanKoNextTurn(st, o, b);
            if (chance > 0)
            {
                s32 pen = (FASTER(o, b) ? VW(threatFast) : VW(threatSlow)) * (s32)chance / 100;
                // La minaccia sulla nostra win condition pesa come il suo KO (stesso peso per ruolo).
                if (!IsOnPlayerSide(b))
                {
                    u32 pIdx = (st->m[b].partyIdx >= 0) ? (u32)st->m[b].partyIdx : gBattlerPartyIndexes[b];
                    if (pIdx < PARTY_SIZE && (sJob.winconParty & (1u << pIdx)))
                        pen = pen * VW(wincon) / 100;
                }
                v += IsOnPlayerSide(b) ? pen : -pen;
            }
        }
    }
    EVAL_DEBUG("+minacce", v);
#undef FASTER
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

// Azioni che rendono solo nei turni dopo (setup, Protect, controllo della velocità, Baton Pass): con poche
// candidate per turno resterebbero sempre fuori, dietro agli attacchi.
static bool32 IsUtilityAction(const struct PlanAction *a)
{
    enum BattleMoveEffects eff;

    if (a->kind == PLAN_ACT_PROTECT)
        return TRUE;
    if (a->kind != PLAN_ACT_MOVE)
        return FALSE;
    eff = GetMoveEffect(a->move);
    return eff == EFFECT_TRICK_ROOM || eff == EFFECT_TAILWIND || eff == EFFECT_BATON_PASS || eff == EFFECT_FOLLOW_ME
        || eff == EFFECT_REFLECT || eff == EFFECT_LIGHT_SCREEN || eff == EFFECT_AURORA_VEIL
        || (IsStatRaisingMove(a->move) && GetMoveTarget(a->move) == TARGET_USER);
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

// Mossa di stato (sonno, scottatura, paralisi, Taunt) che sul bersaglio non fa niente, con i tipi e le abilità della
// simulazione (tipi in lotta: Rotom-Heat è Fuoco). Le stesse immunità di ApplyAction: qui servono a non sprecare i posti
// delle azioni plausibili (turno 2, modello del giocatore) su Spore contro un Erba o Will-O-Wisp contro un Fuoco.
static bool32 SimStatusImmune(const struct PlanState *st, const struct PlanMon *um, const struct PlanMon *tm, enum Move m)
{
    enum Ability tAb = (um->ability == ABILITY_MYCELIUM_MIGHT || um->ability == ABILITY_MOLD_BREAKER) ? ABILITY_NONE : tm->ability;
    u32 status = GetMoveNonVolatileStatus(m);

    if (tAb == ABILITY_GOOD_AS_GOLD || tAb == ABILITY_MAGIC_BOUNCE)
        return TRUE;
    if (um->ability == ABILITY_PRANKSTER && (tm->t1 == TYPE_DARK || tm->t2 == TYPE_DARK))
        return TRUE;
    if (GetMoveEffect(m) == EFFECT_TAUNT)
        return tm->taunted || tAb == ABILITY_OBLIVIOUS || tAb == ABILITY_AROMA_VEIL;
    if (status != MOVE_EFFECT_SLEEP && status != MOVE_EFFECT_BURN && status != MOVE_EFFECT_PARALYSIS)
        return FALSE;
    if (tAb == ABILITY_PURIFYING_SALT || tAb == ABILITY_COMATOSE)
        return TRUE;
    if (IsPowderMove(m) && (tm->t1 == TYPE_GRASS || tm->t2 == TYPE_GRASS || tAb == ABILITY_OVERCOAT
                          || tm->holdEffect == HOLD_EFFECT_SAFETY_GOGGLES))
        return TRUE;
    switch (status)
    {
    case MOVE_EFFECT_SLEEP:
        return tAb == ABILITY_INSOMNIA || tAb == ABILITY_VITAL_SPIRIT || tAb == ABILITY_SWEET_VEIL
            || ((st->terrain == B_TERRAIN_ELECTRIC || st->terrain == B_TERRAIN_MISTY) && IsGroundedInSim(tm));
    case MOVE_EFFECT_BURN:
        return tm->t1 == TYPE_FIRE || tm->t2 == TYPE_FIRE || tAb == ABILITY_WATER_VEIL || tAb == ABILITY_WATER_BUBBLE
            || tAb == ABILITY_THERMAL_EXCHANGE;
    default: // paralisi
        return tm->t1 == TYPE_ELECTRIC || tm->t2 == TYPE_ELECTRIC || tAb == ABILITY_LIMBER
            || TypeEff(GetMoveType(m), tm) == UQ_4_12(0.0);
    }
}

// Azioni plausibili di un battler nello stato `st`. isOurs: le nostre mosse sono tutte note.
// Per il giocatore: mosse viste + mosse probabili dalla specie (peso ridotto), Protect e cambio con probabilità.
static u32 BuildActions(const struct PlanState *st, enum BattlerId b, struct PlanAction *out, u32 maxOut, bool32 isOurs, bool32 allowSwitch)
{
    PLAN_COUNT(sCntBuild);
    struct PlanAction cand[PLAN_CAND_MAX];
    u32 n = 0, i, k;
    const struct PlanMon *bm = &st->m[b];
    enum BattlerId foeL = GetBattlerLeftFoe(b), foeR = GetBattlerRightFoe(b);
    enum BattlerId targets[2] = { foeL, foeR };
    enum Move known[MAX_MON_MOVES];
    u8 knownSlot[MAX_MON_MOVES]; // slot vero della mossa (danni ed efficacia dell'IA sono per slot)
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
            {
                knownSlot[nKnown] = i;
                known[nKnown++] = moves[i];
            }
        }
    }
    else if (isOurs)
    {
        struct Pokemon *mon = &GetBattlerParty(b)[bm->partyIdx];
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            enum Move m = GetMonData(mon, MON_DATA_MOVE1 + i);
            if (m != MOVE_NONE)
            {
                knownSlot[nKnown] = i;
                known[nKnown++] = m;
            }
        }
    }

    for (i = 0; i < nKnown; i++)
    {
        enum Move m = known[i];
        enum BattleMoveEffects eff = GetMoveEffect(m);
        // Slot vero, non la posizione nella lista: con una sola mossa rivelata fuori dal primo slot (o una nostra a
        // 0 PP saltata) danni ed efficacia venivano letti dallo slot sbagliato.
        u8 idx = (bm->partyIdx < 0) ? knownSlot[i] : 0xFF;
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
            else if (eff == EFFECT_TERRAIN && st->terrain != GetMoveTerrainType(m)) w = 40;
            else if (eff == EFFECT_PROTECT && GetMoveProtectMethod(m) == PROTECT_WIDE_GUARD) w = 25;
            else if (GetMoveNonVolatileStatus(m) == MOVE_EFFECT_SLEEP) w = 60;
            else if (eff == EFFECT_TAUNT) w = 35;
            else if (eff == EFFECT_SUBSTITUTE && bm->subHp == 0 && bm->hp > bm->maxHp / 4) w = 30;
            else if (GetMoveNonVolatileStatus(m) == MOVE_EFFECT_BURN || GetMoveNonVolatileStatus(m) == MOVE_EFFECT_PARALYSIS) w = 40;
            else if ((eff == EFFECT_REFLECT && !(st->screens[GetBattlerSide(b)] & 1))
                  || (eff == EFFECT_LIGHT_SCREEN && !(st->screens[GetBattlerSide(b)] & 2))
                  || (eff == EFFECT_AURORA_VEIL && st->weather == PW_SNOW && st->screens[GetBattlerSide(b)] != 3)) w = 40;
            else if (eff == EFFECT_BATON_PASS && isOurs && CountUsablePartyMons(b) > 0
                  && (bm->stage[PS_ATK] > 0 || bm->stage[PS_SPA] > 0 || bm->stage[PS_SPE] > 0)) w = 60;
            else if (IsStatRaisingMove(m) && GetMoveTarget(m) == TARGET_USER) w = 35;
            if (w == 0)
                continue;
            if (eff == EFFECT_TAUNT)
            {
                for (k = 0; k < 2; k++)
                    if (st->m[targets[k]].alive && !SimStatusImmune(st, bm, &st->m[targets[k]], m))
                        AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_MOVE, idx, -1, m, b, targets[k], w);
            }
            else if (GetMoveNonVolatileStatus(m) == MOVE_EFFECT_SLEEP || GetMoveNonVolatileStatus(m) == MOVE_EFFECT_BURN
             || GetMoveNonVolatileStatus(m) == MOVE_EFFECT_PARALYSIS)
            {
                for (k = 0; k < 2; k++)
                    if (st->m[targets[k]].alive && !st->m[targets[k]].asleep && !st->m[targets[k]].burned && !st->m[targets[k]].paralyzed
                     && !SimStatusImmune(st, bm, &st->m[targets[k]], m))
                        AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_MOVE, idx, -1, m, b, targets[k],
                                (GetMoveNonVolatileStatus(m) == MOVE_EFFECT_SLEEP) ? w * SleepValue(st, targets[k]) / CUSTOM_VGC_EVAL_SLEEP : w);
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
            enum BattlerId ally = GetPartnerBattler(b);
            s32 w = 10;
            for (k = 0; k < 2; k++)
                if (st->m[targets[k]].alive)
                    w += AttackWeight(st, &tmp, targets[k]);
            // Il danno al compagno (Earthquake & co.) toglie peso.
            if (GetMoveTarget(m) == TARGET_FOES_AND_ALLY && ally != b && st->m[ally].alive)
                w -= AttackWeight(st, &tmp, ally);
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

        Scout_GetPartyMonProfile(b, bm->species, &prof); // ruoli nella sua squadra (contesto della team preview)
        for (i = 0; i < na; i++)
        {
            enum Type ty = GetMoveType(attacks[i]);
            if ((ty == bm->t1 || ty == bm->t2) && GetMovePower(attacks[i]) > bestPow && !IsSpreadMove(GetMoveTarget(attacks[i]))
             && !IsUnreliableGuess(attacks[i]))
            {
                bestPow = GetMovePower(attacks[i]);
                bestStab = attacks[i];
            }
        }
        for (i = 0; i < nKnown; i++)
            if (!IsBattleMoveStatus(known[i]) && (GetMoveType(known[i]) == bm->t1 || GetMoveType(known[i]) == bm->t2))
                seenStabAttack = TRUE;
        if (bm->partyIdx < 0 && sJob.nLikely[b] > 0)
        {
            // Belief (M1): gli attacchi più usati della specie non ancora visti, con la loro probabilità.
            for (i = 0; i < sJob.nLikely[b] && i < 2; i++)
            {
                enum Move m = sJob.likelyMove[b][i];
                if (IsSpreadMove(GetMoveTarget(m)))
                {
                    struct PlanAction tmp = { PLAN_ACT_MOVE, 0xFF, -1, m, b, foeL, 0 };
                    s32 w = 0;
                    for (k = 0; k < 2; k++)
                        if (st->m[targets[k]].alive)
                            w += AttackWeight(st, &tmp, targets[k]);
                    AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_MOVE, 0xFF, -1, m, b, foeL, w * sJob.likelyP[b][i] / 100);
                    continue;
                }
                for (k = 0; k < 2; k++)
                {
                    struct PlanAction tmp = { PLAN_ACT_MOVE, 0xFF, -1, m, b, targets[k], 0 };
                    if (!st->m[targets[k]].alive)
                        continue;
                    AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_MOVE, 0xFF, -1, m, b, targets[k],
                            AttackWeight(st, &tmp, targets[k]) * sJob.likelyP[b][i] / 100);
                }
            }
        }
        else if (bestStab != MOVE_NONE && !seenStabAttack)
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
                if (st->m[targets[k]].alive && !st->m[targets[k]].asleep && !SimStatusImmune(st, bm, &st->m[targets[k]], MOVE_SPORE))
                    AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_MOVE, 0xFF, -1, MOVE_SPORE, b, targets[k], prof.role[SCOUT_SLEEP] * 60 / 100 * scale / 100);
    }

    // Protect: nostro se lo abbiamo; del giocatore con la probabilità stimata.
    if (isOurs)
    {
        if (hasProtect && !bm->protectedLast)
        {
            s32 w = 20 + (st->firstTurn ? (s32)TeamPreview_ThreatPrior(bm->species) / 4 : 0); // minaccia vista in team preview
            for (k = 0; k < 2; k++)
                if (st->m[targets[k]].alive)
                    w += 50 * (s32)CanKoNextTurn(st, targets[k], b) / 100;
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
        u32 p = sJob.protectChance[b];
        if (p > 0)
            AddCand(cand, &n, PLAN_CAND_MAX, PLAN_ACT_PROTECT, 0, -1, MOVE_PROTECT, b, b, (s32)p);
        if (allowSwitch)
        {
            u32 sp = sJob.switchChance[b];
            if (sp >= 20)
            {
                struct Pokemon *party = GetBattlerParty(b);
                enum BattlerId partner = GetPartnerBattler(b);
                s32 bestIdx = -1;
                for (i = 0; i < PARTY_SIZE; i++)
                {
                    const struct AiPartyMon *known2 = &gAiPartyData->mons[GetBattlerSide(b)][i]; // (lato di b: IA contro IA)
                    if (known2->species == SPECIES_NONE || known2->isFainted || GetMonData(&party[i], MON_DATA_HP) == 0)
                        continue;
                    if ((s32)i == bm->partyIdx || gBattlerPartyIndexes[b] == i
                     || (st->m[partner].alive && (gBattlerPartyIndexes[partner] == i || st->m[partner].partyIdx == (s32)i)))
                        continue;
                    // Panchina: il più probabile secondo la previsione della team preview (a parità, il primo).
                    if (bestIdx < 0 || TeamPreview_BringPrior(known2->species) > TeamPreview_BringPrior(gAiPartyData->mons[GetBattlerSide(b)][bestIdx].species))
                        bestIdx = i;
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
        bool32 hasUtility = FALSE;
        while (outN < maxOut && n > 0)
        {
            u32 best = 0;
            // Corsia protetta (nostre, almeno 3 posti): l'ultimo posto va alla migliore azione "utile" se
            // nessuna è già entrata (Calm Mind ora -> Baton Pass dopo, Protect, Tailwind, Trick Room).
            bool32 lane = isOurs && maxOut >= 2 && outN == maxOut - 1 && !hasUtility;
            for (i = 0; lane && i < n; i++)
                if (IsUtilityAction(&cand[i]) && (!IsUtilityAction(&cand[best]) || cand[i].weight > cand[best].weight))
                    best = i;
            if (!lane || !IsUtilityAction(&cand[best]))
            {
                best = 0;
                for (i = 1; i < n; i++)
                    if (cand[i].weight > cand[best].weight)
                        best = i;
            }
            if (IsUtilityAction(&cand[best]))
                hasUtility = TRUE;
            out[outN++] = cand[best];
            cand[best] = cand[--n];
        }
        return outN;
    }
}

// ---------------------------------------------------------------------
// Simulazione
// ---------------------------------------------------------------------
static void RunTurnImpl(struct PlanState *st, const struct PlanAction **acts, u32 n);
// Profilo (solo test): righe di schermo passate dentro la funzione, sommate (VGCPROF).
static void RunTurn(struct PlanState *st, const struct PlanAction **acts, u32 n)
{
#if TESTING
    u32 t0 = REG_VCOUNT;
    RunTurnImpl(st, acts, n);
    sProf[1] += (REG_VCOUNT + 228 - t0) % 228;
    return;
#else
    RunTurnImpl(st, acts, n);
#endif
}

static void RunTurnImpl(struct PlanState *st, const struct PlanAction **acts, u32 n)
{
    u32 i, j, forced = n;
    for (i = 0; i < n; i++)
    {
        // Priorità e velocità ricalcolate a ogni passo (possono cambiare a metà turno: Simple Beam, Icy Wind,
        // Trick Room, Tailwind), ma una volta per azione e non a ogni confronto.
        s32 pr[MAX_BATTLERS_COUNT];
        u32 sp[MAX_BATTLERS_COUNT];
        bool8 last[MAX_BATTLERS_COUNT];
        u32 first = i;

        // After You: l'azione del compagno è già stata messa qui, salta l'ordinamento.
        if (i == forced)
        {
            ApplyAction(st, acts[i]);
            continue;
        }
        for (j = i; j < n; j++)
        {
            pr[j] = ActionPriority(st, acts[j]);
            last[j] = MovesLastInBracket(st, acts[j]);
            sp[j] = EffSpeed(st, acts[j]->user);
        }
        for (j = i + 1; j < n; j++)
        {
            bool32 better;
            if (pr[j] != pr[first])
                better = pr[j] > pr[first];
            else if (last[j] != last[first])
                better = last[first];
            else if (sp[j] == sp[first])
                better = IsOnPlayerSide(acts[j]->user) && !IsOnPlayerSide(acts[first]->user); // pareggio: pessimisti
            else
                better = st->trickRoom ? sp[j] < sp[first] : sp[j] > sp[first];
            if (better)
                first = j;
        }
        if (first != i)
        {
            const struct PlanAction *t = acts[i]; acts[i] = acts[first]; acts[first] = t;
        }
        sAfterYou = 0;
        ApplyAction(st, acts[i]);
        if (sAfterYou)
        {
            // After You riuscito: il compagno, se deve ancora agire, agisce subito dopo (prima degli avversari più veloci).
            for (j = i + 1; j < n; j++)
            {
                if (acts[j]->user == sAfterYou - 1 && acts[j]->kind != PLAN_ACT_SWITCH && acts[j]->kind != PLAN_ACT_NONE)
                {
                    const struct PlanAction *t = acts[i + 1]; acts[i + 1] = acts[j]; acts[j] = t;
                    forced = i + 1;
                    break;
                }
            }
            sAfterYou = 0;
        }
    }
    // Fine turno. Speed Boost: +1 Velocità (qui e non in NextTurn: la valutazione di fine turno lo vede già).
    for (i = 0; i < gBattlersCount; i++)
    {
        // Yawn: chi è insonnolito da un turno si addormenta ora (se nel frattempo non ha preso un altro stato).
        if (st->m[i].alive && st->m[i].drowsy > 0 && --st->m[i].drowsy == 0 && !st->m[i].asleep && !st->m[i].burned
         && !st->m[i].paralyzed && !SimSleepBlocked(st, &st->m[i], &st->m[i], MOVE_NONE))
        {
            st->m[i].asleep = TRUE;
            st->m[i].sleepTurns = 2;
        }
        if (st->m[i].alive && st->m[i].ability == ABILITY_SPEED_BOOST)
            st->m[i].stage[PS_SPE] = min(6, st->m[i].stage[PS_SPE] + 1);
        // Campo Erboso: 1/16 dei PS a chi è a terra.
        if (st->m[i].alive && st->terrain == B_TERRAIN_GRASSY && IsGroundedInSim(&st->m[i]))
            st->m[i].hp = min(st->m[i].maxHp, st->m[i].hp + st->m[i].maxHp / 16);
    }
}

// Terzo turno in forma ridotta: la nostra risposta migliore (una per Pokémon) contro la loro più probabile.
// Le azioni sono scelte una volta per linea (in StartLookAhead, dalla posizione dopo il turno 1) e riusate in
// ogni foglia del turno 2: sceglierle foglia per foglia costava 4 BuildActions a simulazione, più di metà del
// tempo di pensiero. Chi è andato KO nel turno 2 non agisce; un bersaglio KO passa sull'altro avversario.
static s32 GreedyValue(const struct PlanState *s2)
{
    struct PlanState st = *s2;
    const struct PlanAction *acts[4] = { &sJob.g3[0], &sJob.g3[1], &sJob.g3[2], &sJob.g3[3] };

    NextTurn(&st);
    RunTurn(&st, acts, 4);
    return Evaluate(&st);
}

// Rollout alla foglia: dopo l'orizzonte si giocano fino a CUSTOM_VGC_PLAN_ROLLOUT_TURNS turni "avidi" senza rami:
// ogni Pokémon usa il suo attacco migliore nello stato del momento (KO o più danno, dalla stessa lista ordinata
// di BuildActions), con l'ordine di velocità dei Tailwind/Trick Room rimasti, schermi e stadi attivi. Così il valore
// di velocità, schermi, setup e Baton Pass si vede nei turni in cui rende, senza termini fatti apposta.
static void GreedyAttack(const struct PlanState *st, enum BattlerId b, struct PlanAction *out)
{
    struct PlanAction buf[6];
    u32 n, k;

    out->kind = PLAN_ACT_NONE;
    out->user = out->target = b;
    out->move = MOVE_NONE;
    out->partyIdx = -1;
    out->weight = 0;
    if (!st->m[b].alive || st->m[b].species == SPECIES_NONE)
        return;
    n = BuildActions(st, b, buf, ARRAY_COUNT(buf), !IsOnPlayerSide(b), FALSE);
    for (k = 0; k < n; k++)
    {
        if (buf[k].kind == PLAN_ACT_MOVE && !IsBattleMoveStatus(buf[k].move) && !IsUnreliableGuess(buf[k].move))
        {
            *out = buf[k];
            return;
        }
    }
}

static s32 RolloutValue(const struct PlanState *s2)
{
    struct PlanState st = *s2;
    struct PlanAction act[MAX_BATTLERS_COUNT];
    const struct PlanAction *acts[MAX_BATTLERS_COUNT];
    u32 t, b;

    for (t = 0; t < CUSTOM_VGC_PLAN_ROLLOUT_TURNS; t++)
    {
        bool32 alive[NUM_BATTLE_SIDES] = {FALSE, FALSE};
        for (b = 0; b < gBattlersCount; b++)
            if (st.m[b].alive)
                alive[GetBattlerSide(b)] = TRUE;
        if (!alive[0] || !alive[1])
            break;
        NextTurn(&st);
        for (b = 0; b < gBattlersCount; b++)
        {
            GreedyAttack(&st, b, &act[b]);
            acts[b] = &act[b];
        }
        RunTurn(&st, acts, gBattlersCount);
    }
    return Evaluate(&st);
}

// ---------------------------------------------------------------------
// Candidate nostre al primo turno e azione del partner
// ---------------------------------------------------------------------
// Tutte le azioni legali di `me` al primo turno: ogni mossa usabile (PP, Choice, Taunt, Disable... dal motore)
// su ogni bersaglio possibile, più il cambio, tolte quelle che il filtro delle regole (AI_VgcFilterAction, M8) dà per
// illegali o certamente inutili: lo stesso filtro vale per l'azione delle regole. Le altre le ordina la simulazione.
static u32 BuildOurFirstActions(enum BattlerId me, struct PlanAction *out, u32 chosenMove, enum BattlerId chosenTarget)
{
    u32 n = 0, i, limits = gAiLogicData->moveLimitations[me];
    enum BattlerId t, p = GetPartnerBattler(me);
    enum Move lastMove = gLastResultingMoves[me];
    bool32 legacy = AI_VgcRulesLegacy(me);
    bool32 protectedLast = lastMove != MOVE_NONE && lastMove != MOVE_UNAVAILABLE
                        && gBattleMoveEffects[GetMoveEffect(lastMove)].usesProtectCounter
                        && gBattleMons[me].volatiles.consecutiveMoveUses > 0;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        enum Move m = gBattleMons[me].moves[i];
        enum MoveTarget tt;
        u8 kind;
        s32 w;

        if (m == MOVE_NONE || m == MOVE_UNAVAILABLE || gBattleMons[me].pp[i] == 0)
            continue;
        if (legacy)
        {
            // Regole di prima di M8 (solo self-play, verifica-regole): i filtri che c'erano qui.
            if ((limits & (1u << i)) && !(i == chosenMove))
                continue;
            if (GetMoveEffect(m) == EFFECT_FIRST_TURN_ONLY && !IsBattlersFirstTurn(me))
                continue;
            if (gBattleMoveEffects[GetMoveEffect(m)].usesProtectCounter && protectedLast)
                continue;
            if (GetMoveEffect(m) == EFFECT_TAILWIND && (gSideStatuses[GetBattlerSide(me)] & SIDE_STATUS_TAILWIND))
                continue;
        }
        kind = (GetMoveEffect(m) == EFFECT_PROTECT && GetMoveProtectMethod(m) == PROTECT_NORMAL) ? PLAN_ACT_PROTECT : PLAN_ACT_MOVE;
        tt = GetMoveTarget(m);
        if (tt == TARGET_SELECTED || tt == TARGET_SMART || tt == TARGET_OPPONENT || tt == TARGET_USER_OR_ALLY)
        {
            for (t = 0; t < gBattlersCount; t++)
            {
                if (t == me || !IsBattlerAlive(t) || !CanTargetBattler(me, t, m))
                    continue;
                if (legacy ? (!IsBattlerAlly(me, t) && !IsBattleMoveStatus(m) && gAiLogicData->effectiveness[me][t][i] == UQ_4_12(0.0))
                           : AI_VgcFilterAction(me, i, t) != VGC_FILTER_NONE)
                    continue;
                // Sul compagno solo le mosse fatte per lui o che gli cambiano l'abilità; le combo che lo colpiscono
                // (Beat Up -> Justified) le aggiunge il grafo della squadra.
                if (IsBattlerAlly(me, t) && !(i == chosenMove && t == chosenTarget)
                 && !(IsBattleMoveStatus(m) && (GetMoveEffect(m) == EFFECT_OVERWRITE_ABILITY || GetMoveEffect(m) == EFFECT_SKILL_SWAP
                                              || GetMoveEffect(m) == EFFECT_GASTRO_ACID || GetMoveEffect(m) == EFFECT_ENTRAINMENT
                                              || GetMoveEffect(m) == EFFECT_HELPING_HAND
                                              || (!legacy && (GetMoveEffect(m) == EFFECT_AFTER_YOU || GetMoveEffect(m) == EFFECT_INSTRUCT)))))
                    continue;
                // Un attacco sul compagno passa solo se è una combo vera (gli attiva l'abilità costandogli poco):
                // nemmeno la mossa delle regole lo porta dentro (Fake Out di Incineroar sul proprio Chandelure).
                if (IsBattlerAlly(me, t) && !IsBattleMoveStatus(m) && !AI_VgcIsPartnerHitCombo(m, me, t))
                    continue;
                w = gAiBattleData->finalScore[me][t][i];
                AddCand(out, &n, PLAN_ALL_OURS - 2, kind, i, -1, m, me, t, w);
            }
        }
        else
        {
            // Una sola scelta: su sé, sul compagno, sul campo o ad area (bersaglio nominale = un avversario vivo).
            t = GetBattlerLeftFoe(me);
            if (tt == TARGET_USER || tt == TARGET_USER_AND_ALLY || tt == TARGET_FIELD || tt == TARGET_ALL_BATTLERS || tt == TARGET_OPPONENTS_FIELD)
                t = me;
            else if (tt == TARGET_ALLY)
                t = p;
            else if (!IsBattlerAlive(t))
                t = GetBattlerRightFoe(me);
            if (t != me && !IsBattlerAlive(t))
                continue;
            if (i == chosenMove)
                t = (tt == TARGET_USER || tt == TARGET_ALLY) ? t : (IsBattlerAlive(chosenTarget) ? chosenTarget : t);
            if (!legacy && AI_VgcFilterAction(me, i, t) != VGC_FILTER_NONE)
                continue;
            w = gAiBattleData->finalScore[me][(t == me) ? GetBattlerLeftFoe(me) : t][i];
            AddCand(out, &n, PLAN_ALL_OURS - 2, kind, i, -1, m, me, t, w);
        }
    }
    // Il cambio è candidato solo se non siamo appena entrati con un cambio (freno ai cambi a ripetizione), e mai al
    // primo turno della lotta: le lead sono scelte dal team preview per fare il loro lavoro al turno 1.
    if (CountUsablePartyMons(me) > 0 && !AI_VgcJustSwitchedIn(me)
     && gBattleTurnCounter > 0)
    {
        // Ogni Pokémon in panchina (in VGC al massimo 2): chi entra lo sceglie la simulazione, che vede i colpi
        // degli avversari sullo slot; prima c'era solo quello di GetMostSuitableMonToSwitchInto, scelto sul
        // confronto con un solo avversario (Gholdengo dentro contro due Headlong Rush).
        struct Pokemon *party = GetBattlerParty(me);
        u32 k, idx, first = gAiLogicData->mostSuitableMonId[me];
        for (k = 0; k <= PARTY_SIZE; k++)
        {
            // Prima quello suggerito dal motore (se c'è), poi gli altri.
            idx = (k == 0) ? first : k - 1;
            if (idx >= PARTY_SIZE || (k > 0 && idx == first))
                continue;
            if (idx == gBattlerPartyIndexes[me] || (IsBattlerAlive(p) && gBattlerPartyIndexes[p] == idx)
             || gBattleStruct->AI_monToSwitchIntoId[p] == idx)
                continue;
            if (GetMonData(&party[idx], MON_DATA_HP) == 0 || GetMonData(&party[idx], MON_DATA_IS_EGG)
             || GetMonData(&party[idx], MON_DATA_SPECIES_OR_EGG) == SPECIES_NONE)
                continue;
            AddCand(out, &n, PLAN_ALL_OURS, PLAN_ACT_SWITCH, 0, (s8)idx, MOVE_NONE, me, me, idx == first ? 1 : 0);
        }
    }
    return n;
}

// Modello del giocatore livello 1: le due azioni migliori per lui, unendo il valore a 1 turno dal suo punto di vista
// (quanto peggiora la nostra posizione) e la probabilità a priori (mosse viste o probabili, Protect, cambio).
// Il peso della coppia di risposte resta nel campo `weight` (usato da PairWeight).
static u32 PickFoeActions(u32 side, struct PlanAction *out)
{
    u32 n = sJob.nFoeCand[side], i, k, outN = 0;
    s32 score[PLAN_FOE_CAND], best = INT32_MIN;
    bool8 taken[PLAN_FOE_CAND] = {0};

    for (i = 0; i < n; i++)
    {
        // Per il giocatore conta il nostro valore cambiato di segno; la priorità a priori vale come in BuildActions.
        score[i] = -sJob.foeVal[side][i] + sJob.foeCand[side][i].weight * 2;
        if (sJob.foeCand[side][i].kind != PLAN_ACT_PROTECT && sJob.foeCand[side][i].kind != PLAN_ACT_SWITCH && score[i] > best)
            best = score[i];
    }
    // Protect e cambio sulla stessa scala delle mosse: valore a 1 turno dal suo punto di vista più la probabilità a
    // priori, meno il costo del tempo perso, lo stesso che il pianificatore fa pagare ai nostri (cambio intero,
    // Protect mezzo cambio): a 1 turno sembrano sempre ottimi perché salvano PS e il tempo perso non si vede.
    if (CUSTOM_VGC_PLAN_FOE_SINGLE_SCALE)
    {
        for (i = 0; i < n; i++)
        {
            if (sJob.foeCand[side][i].kind == PLAN_ACT_SWITCH)
                score[i] -= VW(switchCost) * 3 / 2;
            else if (sJob.foeCand[side][i].kind == PLAN_ACT_PROTECT)
                score[i] -= VW(switchCost);
        }
    }
    if (CUSTOM_VGC_PLAN_FOE_SINGLE_SCALE)
        for (i = 0; i < n; i++)
            best = max(best, score[i]); // la migliore su tutta la scala (pesi delle risposte fino a 100)
    else
    {
        for (i = 0; i < n; i++)
            if (sJob.foeCand[side][i].kind == PLAN_ACT_PROTECT || sJob.foeCand[side][i].kind == PLAN_ACT_SWITCH)
                score[i] = (best == INT32_MIN ? 0 : best) - 4 * (100 - min(sJob.foeCand[side][i].weight, 100));
    }
    if (best == INT32_MIN)
        for (i = 0; i < n; i++)
            best = max(best, score[i]);
    // Contesto della squadra (team preview): il giocatore gioca il piano della sua squadra (Trick Room, Tailwind).
    // Solo l'ordine: la scala delle altre risposte (e di Protect) resta quella di prima.
    for (i = 0; i < n; i++)
        score[i] += 2 * (s32)Scout_BattlerPlanMoveBonus(sJob.foeCand[side][i].user, sJob.foeCand[side][i].move);
    // Storia dei bersagli di questa lotta: chi colpisce sempre il più danneggiato o la nostra win condition.
    if (FoeHistoryOn())
        for (i = 0; i < n; i++)
            if (sJob.foeCand[side][i].kind == PLAN_ACT_MOVE && !IsBattleMoveStatus(sJob.foeCand[side][i].move))
                score[i] += AI_VgcFoeTargetBias(sJob.foeCand[side][i].user, sJob.foeCand[side][i].target);
    // Il giocatore colpisce chi lo minaccia di più: tra i nostri due in campo, l'attacco sul più pericoloso per lui
    // (ThreatPct a inizio lavoro: danno in % e KO possibili sul suo lato) è più probabile.
    if (CUSTOM_VGC_PLAN_FOE_THREAT_FOCUS > 0 && FoeHistoryOn())
        for (i = 0; i < n; i++)
        {
            const struct PlanAction *c = &sJob.foeCand[side][i];
            enum BattlerId t = c->target, p;
            if (c->kind != PLAN_ACT_MOVE || IsBattleMoveStatus(c->move) || t >= gBattlersCount || IsOnPlayerSide(t)
             || !sJob.base.m[t].alive)
                continue;
            p = GetPartnerBattler(t);
            if (!sJob.base.m[p].alive)
                continue;
            score[i] += CUSTOM_VGC_PLAN_FOE_THREAT_FOCUS * max(-100, min(100, sJob.threatPct[t] - sJob.threatPct[p])) / 100;
        }
    while (outN < PLAN_FOE_PER && outN < n)
    {
        u32 bi = 0xFF;
        for (k = 0; k < n; k++)
            if (!taken[k] && (bi == 0xFF || score[k] > score[bi]))
                bi = k;
        taken[bi] = TRUE;
        out[outN] = sJob.foeCand[side][bi];
        // Peso: la migliore conta di più; una alternativa molto peggiore per lui resta, ma poco probabile.
        out[outN].weight = max(8, 100 - max(best - score[bi], 0) / 4);
        outN++;
    }
    return outN;
}

// L'azione scelta dalle regole per `b`, come azione del simulatore (PLAN_ACT_NONE se non è una mossa nota).
static u32 BestCandIdx(u32 side)
{
    u32 i, best = 0;
    for (i = 1; i < sJob.nCand[side]; i++)
        if (sJob.candVal[side][i] > sJob.candVal[side][best])
            best = i;
    return best;
}

static struct PlanAction RulesAction(enum BattlerId b, u32 moveIdx, enum BattlerId target)
{
    struct PlanAction a = { PLAN_ACT_NONE, 0, -1, MOVE_NONE, b, b, 0 };
    enum Move m;

    if (!IsBattlerAlive(b) || moveIdx >= MAX_MON_MOVES)
        return a;
    m = gBattleMons[b].moves[moveIdx];
    if (m == MOVE_NONE || m == MOVE_UNAVAILABLE)
        return a;
    // Un'azione delle regole scartata dal filtro non fa da termine di paragone (il compagno "non fa niente").
    if (!AI_VgcRulesLegacy(b) && AI_VgcFilterAction(b, moveIdx, target) != VGC_FILTER_NONE)
        return a;
    a.kind = (GetMoveEffect(m) == EFFECT_PROTECT && GetMoveProtectMethod(m) == PROTECT_NORMAL) ? PLAN_ACT_PROTECT : PLAN_ACT_MOVE;
    a.moveIndex = moveIdx;
    a.move = m;
    a.target = target;
    return a;
}

static bool32 IsOrigAction(const struct PlanAction *a, u32 origMove, enum BattlerId origTarget)
{
    return a->kind != PLAN_ACT_SWITCH && a->kind != PLAN_ACT_NONE && a->moveIndex == origMove && a->target == origTarget;
}

// Spinta delle regole (AI_VgcActionPrior, M8) per un'azione nostra. exact: `partner` è l'azione vera del compagno nella
// coppia (Protect o cambio = non attacca); altrimenti è solo un riferimento, e se non è una mossa la spinta la stima.
static s32 ActionPrior(const struct PlanAction *a, const struct PlanAction *partner, bool32 exact)
{
    enum Move partnerMove = MOVE_NONE;

    if ((a->kind != PLAN_ACT_MOVE && a->kind != PLAN_ACT_PROTECT) || AI_VgcRulesLegacy(a->user))
        return 0;
    if (partner != NULL && partner->kind == PLAN_ACT_MOVE)
        partnerMove = partner->move;
    else if (exact && partner != NULL && partner->kind != PLAN_ACT_NONE)
        partnerMove = MOVE_UNAVAILABLE;
    return AI_VgcActionPrior(a->user, a->move, a->target, partnerMove);
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
             || (hit && AI_VgcIsPartnerHitCombo(move, b, ally));
        if (!combo || !CanTargetBattler(b, ally, move))
            continue;
        for (k = 0; k < *n; k++)
            if (list[k].kind == PLAN_ACT_MOVE && list[k].moveIndex == i && list[k].target == ally)
                present = TRUE;
        if (present)
            continue;
        if (*n >= PLAN_ALL_OURS)
            *n = PLAN_ALL_OURS - 1; // lascia posto alla combo al posto dell'ultima candidata
        AddCand(list, n, PLAN_ALL_OURS, PLAN_ACT_MOVE, i, -1, move, b, ally, weight);
    }
}

// ---- KO sicuri (ia-vgc-ko) ----
// Un KO garantito (danno minimo della tabella dell'IA >= PS, precisione piena) preso ora toglie al giocatore tutte le
// azioni future: l'albero a 3 turni lo vede solo in parte, e lo azzera nei rami in cui il modello del giocatore prevede
// una lettura (Protect o cambio del bersaglio, Fake Out su chi attacca). Misurato sui log delle campagne (VGCFOE contro
// le azioni vere): Protect previsto al 16-28% avviene al 7-22% (x0,65), il cambio previsto al 27% avviene al 13% (x0,45),
// Fake Out x0,8. Ogni coppia con un KO sicuro riceve: nei rami in cui il bersaglio va KO un premio (una parte del valore
// del KO, CUSTOM_VGC_PLAN_SURE_KO_PCT); nei rami bloccati da una lettura la parte della lettura che il modello
// sopravvaluta (lì il KO passa, con il suo premio).
static bool32 KoFixesOn(void)
{
#if TESTING
    if (CUSTOM_VGC_MIRROR_KO)
        return !((gVgcTraining.rulesLegacy >> GetBattlerSide(sJob.me)) & 1);
#endif
    return MisplayFixesOn();
}

// L'azione `a` manda KO di sicuro l'avversario `f` (stessi criteri del controllore: niente Final Gambit/Explosion, niente
// Focus Sash/Sturdy a PS pieni, niente priorità sul Campo Psichico contro chi è a terra).
static bool32 SureKoOn(const struct PlanAction *a, enum BattlerId f)
{
    enum MoveTarget tt;
    enum BattlerId u = a->user;

    if (a->kind != PLAN_ACT_MOVE || a->moveIndex >= MAX_MON_MOVES || IsBattleMoveStatus(a->move) || u >= gBattlersCount
     || f >= gBattlersCount || IsBattlerAlly(u, f) || !IsBattlerAlive(f) || !IsBattlerAlive(u))
        return FALSE;
    tt = GetBattlerMoveTargetType(u, a->move);
    if (a->target != f && tt != TARGET_BOTH && tt != TARGET_FOES_AND_ALLY)
        return FALSE;
    if (GetMoveEffect(a->move) == EFFECT_FINAL_GAMBIT || IsExplosionMove(a->move))
        return FALSE;
    if (gBattleMons[f].hp == gBattleMons[f].maxHP
     && (gAiLogicData->holdEffects[f] == HOLD_EFFECT_FOCUS_SASH || gAiLogicData->abilities[f] == ABILITY_STURDY))
        return FALSE;
    if (gFieldTimers.terrain == B_TERRAIN_PSYCHIC && GetBattleMovePriority(u, gAiLogicData->abilities[u], a->move) > 0
     && IsBattlerGrounded(f, gAiLogicData->abilities[f], gAiLogicData->holdEffects[f]))
        return FALSE;
    return gAiLogicData->simulatedDmg[u][f][a->moveIndex].minimum >= gBattleMons[f].hp
        && gAiLogicData->moveAccuracy[u][f][a->moveIndex] >= 100;
}

static bool32 SureKoAny(const struct PlanAction *a)
{
    enum BattlerId u = a->user;
    if (a->kind != PLAN_ACT_MOVE || u >= gBattlersCount)
        return FALSE;
    return SureKoOn(a, GetBattlerLeftFoe(u)) || SureKoOn(a, GetBattlerRightFoe(u));
}

// Fake Out (e simili) di `fa` su `b`.
static bool32 IsFakeOutOn(const struct PlanAction *fa, enum BattlerId b)
{
    return fa->kind == PLAN_ACT_MOVE && fa->target == b && GetMoveEffect(fa->move) == EFFECT_FIRST_TURN_ONLY;
}

// Credito KO di un ramo del turno 1 (coppia sJob.i, risposte fa/fb, stato dopo il turno s1), in centesimi del valore di un
// KO, pesato `w` come il ramo.
static void KoCreditBranch(const struct PlanState *s1, const struct PlanAction *x, const struct PlanAction *y,
                           const struct PlanAction *fa, const struct PlanAction *fb, s32 w)
{
    enum BattlerId foes[2] = { GetBattlerLeftFoe(sJob.me), GetBattlerRightFoe(sJob.me) };
    s32 credit = 0;
    u32 k;

    if (CUSTOM_VGC_PLAN_SURE_KO_PCT == 0 || !KoFixesOn())
        return;
    for (k = 0; k < 2; k++)
    {
        enum BattlerId f = foes[k];
        const struct PlanAction *ko = SureKoOn(x, f) ? x : SureKoOn(y, f) ? y : NULL;
        const struct PlanAction *ff;
        u32 c = 100;

        if (ko == NULL || !sJob.base.m[f].alive)
            continue;
        if (!s1->m[f].alive)
        {
            credit += CUSTOM_VGC_PLAN_SURE_KO_PCT;
            continue;
        }
        ff = (fa->user == f) ? fa : (fb->user == f) ? fb : NULL;
        if (ff != NULL && ff->kind == PLAN_ACT_PROTECT)
            c = CUSTOM_VGC_PLAN_KO_CALIB_PROTECT;
        else if (ff != NULL && ff->kind == PLAN_ACT_SWITCH)
            c = CUSTOM_VGC_PLAN_KO_CALIB_SWITCH;
        else if (IsFakeOutOn(fa, ko->user) || IsFakeOutOn(fb, ko->user))
            c = CUSTOM_VGC_PLAN_KO_CALIB_FAKE_OUT;
        // La lettura avviene nel c% dei casi che il modello le dà: nel resto il KO passa (con il premio).
        credit += (s32)(100 - c) * (100 + CUSTOM_VGC_PLAN_SURE_KO_PCT) / 100;
    }
    sJob.koSum[sJob.i] += credit * w;
    sJob.koWsum[sJob.i] += w;
}

// Premio della coppia i, in punti.
static s32 KoBonus(u32 i)
{
    if (CUSTOM_VGC_PLAN_SURE_KO_PCT == 0 || !KoFixesOn() || sJob.koWsum[i] <= 0)
        return 0;
    return (VW(ko) + VW(alive)) * (sJob.koSum[i] / sJob.koWsum[i]) / 100;
}
// ---- fine KO sicuri ----

// Le migliori PLAN_MAX_OURS azioni di un lato per valore a 1 turno, tenendo sempre: l'azione delle regole
// (termine di paragone), le combo sul compagno, la migliore azione "utile" (setup, Protect, velocità, Baton Pass).
static u32 TrimCands(u32 side, struct PlanAction *out, u8 origMove, enum BattlerId origTarget)
{
    struct PlanAction *c = sJob.cand[side];
    s32 *val = sJob.candVal[side];
    u32 n = sJob.nCand[side], i, k, outN = 0, bestUtil = 0xFF, bestProtect = 0xFF;
    bool8 taken[PLAN_ALL_OURS] = {0};

    // Due posti protetti distinti: il miglior Protect e la migliore azione "utile" che non è Protect (velocità,
    // setup, Baton Pass). Con uno solo, Protect (sempre plausibile) lasciava fuori Tailwind e Trick Room.
    for (i = 0; i < n; i++)
    {
        if (c[i].kind == PLAN_ACT_PROTECT)
        {
            if (bestProtect == 0xFF || val[i] > val[bestProtect])
                bestProtect = i;
        }
        else if (IsUtilityAction(&c[i]) && (bestUtil == 0xFF || val[i] > val[bestUtil]))
        {
            bestUtil = i;
        }
    }
    // Un posto protetto anche per il miglior KO sicuro (danno minimo stimato, mossa precisa) su ciascun avversario:
    // valutato a 1 turno contro la mossa delle regole del compagno poteva restare fuori, se il compagno colpiva
    // già lo stesso bersaglio.
    for (k = 0; k < 2; k++)
    {
        u32 bestKo = 0xFF;
        for (i = 0; i < n; i++)
        {
            enum BattlerId t = c[i].target;
            if (c[i].kind != PLAN_ACT_MOVE || c[i].moveIndex >= MAX_MON_MOVES || IsBattleMoveStatus(c[i].move)
             || t >= MAX_BATTLERS_COUNT || IsBattlerAlly(c[i].user, t)
             || (k == 0) != (t == GetBattlerLeftFoe(c[i].user)) || !IsBattlerAlive(t))
                continue;
            if (gAiLogicData->simulatedDmg[c[i].user][t][c[i].moveIndex].minimum >= gBattleMons[t].hp
             && gAiLogicData->moveAccuracy[c[i].user][t][c[i].moveIndex] >= 100
             && (bestKo == 0xFF || val[i] > val[bestKo]))
                bestKo = i;
        }
        if (bestKo != 0xFF)
            taken[bestKo] = 2; // segnato: entra con i protetti qui sotto
    }
    // Un posto protetto per il miglior Fake Out (& co.) del primo turno in campo: rende nel turno dopo (chi lo subisce non
    // agisce) e a 1 turno resta dietro agli attacchi; prima ce lo portava l'azione delle regole (M8: ora è un filtro).
    if (!AI_VgcRulesLegacy(sJob.me))
    {
        u32 bestFo = 0xFF;
        for (i = 0; i < n; i++)
            if (c[i].kind == PLAN_ACT_MOVE && GetMoveEffect(c[i].move) == EFFECT_FIRST_TURN_ONLY && (bestFo == 0xFF || val[i] > val[bestFo]))
                bestFo = i;
        if (bestFo != 0xFF)
            taken[bestFo] = 2;
    }
    for (i = 0; i < n && outN < PLAN_MAX_OURS; i++)
    {
        if (IsOrigAction(&c[i], origMove, origTarget) || IsComboAction(&c[i]) || i == bestUtil || i == bestProtect || taken[i] == 2)
        {
            taken[i] = TRUE;
            out[outN] = c[i];
            out[outN++].weight = val[i];
        }
    }
    while (outN < PLAN_MAX_OURS)
    {
        u32 best = 0xFF;
        for (k = 0; k < n; k++)
            if (!taken[k] && (best == 0xFF || val[k] > val[best]))
                best = k;
        if (best == 0xFF)
            break;
        taken[best] = TRUE;
        out[outN] = c[best];
        out[outN++].weight = val[best];
    }
    return outN;
}

// Costo a priori di una coppia, lo stesso di FinishJob: il cambio (tempo perso, di più al primo turno della lead) e il
// doppio Protect (un turno regalato).
static s32 PairCost(const struct PlanAction *x, const struct PlanAction *y)
{
    s32 cost = 0;
    if (x->kind == PLAN_ACT_SWITCH)
        cost += IsBattlersFirstTurn(x->user) ? VW(switchCost) : VW(switchCost) / 2;
    if (y->kind == PLAN_ACT_SWITCH)
        cost += IsBattlersFirstTurn(y->user) ? VW(switchCost) : VW(switchCost) / 2;
    if (x->kind == PLAN_ACT_PROTECT && y->kind == PLAN_ACT_PROTECT)
        cost += VW(switchCost) / 2;
    return cost;
}

// Fase 4: le candidate rimaste dei due lati (TrimCands); FALSE se si pianifica da soli (niente coppie da stimare).
static bool32 StartPairRanking(void)
{
    enum BattlerId p = GetPartnerBattler(sJob.me);

    sJob.nTrim[0] = TrimCands(0, sJob.trim[0], sJob.origMove, sJob.origTarget);
    sJob.nTrim[1] = 0;
    sJob.pi = sJob.pj = 0;
    if (p == sJob.me || !IsBattlerAlive(p) || sJob.nCand[1] == 0)
        return FALSE;
    sJob.nTrim[1] = TrimCands(1, sJob.trim[1], sJob.origPMove, sJob.origPTarget);
    return sJob.nTrim[0] > 0 && sJob.nTrim[1] > 0;
}

// Coppie candidate per il primo turno: azioni nostre x azioni del partner (le migliori a 1 turno, fase 4), senza le
// coppie impossibili, ordinate per valore a 1 turno della coppia simulata insieme; la coppia scelta dalle regole è
// sempre tra quelle simulate.
static u32 BuildPairs(enum BattlerId me)
{
    const struct PlanAction *mine = sJob.trim[0], *theirs = sJob.trim[1];
    s32 pre[PLAN_MAX_PAIRS];
    s32 raw[PLAN_MAX_OURS][PLAN_MAX_OURS];
    u8 tier[PLAN_MAX_OURS][PLAN_MAX_OURS];
    u8 bestJ[PLAN_MAX_OURS], bestI[PLAN_MAX_OURS];
    enum BattlerId p = GetPartnerBattler(me);
    u32 nMine = sJob.nTrim[0], nTheirs = sJob.nTrim[1], i, j, k, n = 0;

    if (p == me || !IsBattlerAlive(p) || nTheirs == 0)
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

    // Valore di tutte le coppie legali (fase 4), poi scelta a livelli: (1) la coppia delle regole, termine di
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
            raw[i][j] = sJob.pairVal[i][j];
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
    // Le azioni di velocità/setup (non Protect) hanno sempre la loro coppia migliore tra quelle simulate: rendono
    // nei turni dopo, e la stima rapida (1 turno) le mette in fondo.
    for (i = 0; i < nMine; i++)
        if (bestJ[i] != 0xFF && mine[i].kind != PLAN_ACT_PROTECT && IsUtilityAction(&mine[i]) && tier[i][bestJ[i]] < 2)
            tier[i][bestJ[i]] = 2;
    for (j = 0; j < nTheirs; j++)
        if (bestI[j] != 0xFF && theirs[j].kind != PLAN_ACT_PROTECT && IsUtilityAction(&theirs[j]) && tier[bestI[j]][j] < 2)
            tier[bestI[j]][j] = 2;
    // ...e anche accanto alla migliore azione del compagno secondo la simulazione a 1 turno (il peso delle
    // candidate): la stima rapida sceglieva il compagno sbagliato (Tailwind + Fake Out sul bersaglio peggiore,
    // 500 punti sotto Rock Slide + Fake Out sul bersaglio giusto) e la mossa di velocità perdeva per questo.
    {
        u32 topI = 0, topJ = 0;
        for (i = 1; i < nMine; i++)
            if (mine[i].weight > mine[topI].weight)
                topI = i;
        for (j = 1; j < nTheirs; j++)
            if (theirs[j].weight > theirs[topJ].weight)
                topJ = j;
        for (i = 0; i < nMine; i++)
            if (mine[i].kind != PLAN_ACT_PROTECT && IsUtilityAction(&mine[i]) && tier[i][topJ] != 0xFF && tier[i][topJ] < 2)
                tier[i][topJ] = 2;
        for (j = 0; j < nTheirs; j++)
            if (theirs[j].kind != PLAN_ACT_PROTECT && IsUtilityAction(&theirs[j]) && tier[topI][j] != 0xFF && tier[topI][j] < 2)
                tier[topI][j] = 2;
    }

    // KO sicuri (ia-vgc-ko): la coppia migliore di ogni azione con un KO garantito è sempre simulata per intero.
    if (CUSTOM_VGC_PLAN_KO_PAIRS && KoFixesOn())
    {
        for (i = 0; i < nMine; i++)
            if (bestJ[i] != 0xFF && tier[i][bestJ[i]] < 2 && SureKoAny(&mine[i]))
                tier[i][bestJ[i]] = 2;
        for (j = 0; j < nTheirs; j++)
            if (bestI[j] != 0xFF && tier[bestI[j]][j] < 2 && SureKoAny(&theirs[j]))
                tier[bestI[j]][j] = 2;
    }

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
// Probabilità (0..50) che un colpo che manda KO con il danno medio NON lo faccia (tiro basso): i tiri sono uniformi tra
// minimo e massimo, la mediana è a metà; se anche il minimo basta, 0.
static u32 LowRollFailChance(const struct PlanState *st, const struct PlanAction *a)
{
    enum BattlerId t = a->target;
    s32 med, lo, hp;

    if (a->kind != PLAN_ACT_MOVE || IsBattleMoveStatus(a->move) || IsBattlerAlly(a->user, t) || !st->m[t].alive)
        return 0;
    med = DamageOf(st, a, t);
    hp = st->m[t].hp;
    if (med < hp)
        return 0;
    lo = LowRollDmg(st, a, t, med);
    if (lo >= hp)
        return 0;
    return (u32)(50 * (hp - lo) / max(med - lo, 1));
}

// Probabilità (0..50) che un colpo che NON manda KO con il danno medio lo faccia con un tiro alto (tra la mediana e il
// massimo); 0 se nemmeno il massimo basta. Il simmetrico di LowRollFailChance: senza, un Pokémon che restava a pochi PS
// con la mediana era "salvo" (cambi su chi va KO all'entrata, win condition lasciate a un tiro alto dal KO).
static u32 HighRollKoChance(const struct PlanState *st, const struct PlanAction *a)
{
    enum BattlerId t = a->target;
    s32 med, hi, hp;

    if (a->kind != PLAN_ACT_MOVE || IsBattleMoveStatus(a->move) || t >= MAX_BATTLERS_COUNT || IsBattlerAlly(a->user, t)
     || !st->m[t].alive)
        return 0;
    med = DamageOf(st, a, t);
    hp = st->m[t].hp + st->m[t].subHp;
    if (med <= 0 || med >= hp || st->m[t].subHp > 0)
        return 0;
    hi = med * (s32)HighRollPct(st, a, t) / 100;
    if (hi < hp)
        return 0;
    return (u32)(50 * (hi - hp + 1) / max(hi - med, 1));
}

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
    // Danno: un KO che riesce solo con il tiro medio non è sicuro. Ramo "colpo basso" pesato per la probabilità
    // che il tiro non basti: così un KO sicuro vale più di un KO a testa o croce, anche per gli avversari.
    for (k = 0; k < 4; k++)
    {
        u32 fail = LowRollFailChance(base, acts4[k]);
        const struct PlanAction *acts[4];
        struct PlanState sm;
        s32 eLow;

        if (fail == 0)
            continue;
        if (sims == 0)
            eHit = Evaluate(hit);
        memcpy(acts, acts4, sizeof(acts));
        sm = *base;
        sLowRollBattler = acts4[k]->user + 1;
        RunTurn(&sm, acts, 4);
        sLowRollBattler = 0;
        eLow = Evaluate(&sm);
        *pen += (s32)fail * (eHit - eLow) / 100;
        sims++;
    }
    // Danno: un colpo che manda KO solo con un tiro alto. Ramo "colpo alto" pesato per la probabilità del tiro, per
    // entrambi i lati (un nostro KO possibile vale un po', un loro KO possibile costa un po').
    for (k = 0; k < 4 && CUSTOM_VGC_PLAN_HIGH_ROLL && MisplayFixesOn(); k++)
    {
        u32 chance = HighRollKoChance(base, acts4[k]);
        const struct PlanAction *acts[4];
        struct PlanState sm;
        s32 eHigh;

        if (chance == 0)
            continue;
        if (sims == 0)
            eHit = Evaluate(hit);
        memcpy(acts, acts4, sizeof(acts));
        sm = *base;
        sHighRollBattler = acts4[k]->user + 1;
        RunTurn(&sm, acts, 4);
        sHighRollBattler = 0;
        eHigh = Evaluate(&sm);
        *pen += (s32)chance * (eHit - eHigh) / 100;
        sims++;
    }
    // Velocità incerta: lo stesso turno con la Velocità alternativa dell'avversario, al 35% (un quarto opposto
    // dell'intervallo, pesato meno della stima più usata).
    for (k = 0; k < gBattlersCount; k++)
    {
        const struct PlanAction *acts[4];
        struct PlanState sm;
        s32 eAlt;

        if (sJob.speedAlt[k] == 0)
            continue;
        if (sims == 0)
            eHit = Evaluate(hit);
        memcpy(acts, acts4, sizeof(acts));
        sm = *base;
        sSpeedAltBattler = k + 1;
        RunTurn(&sm, acts, 4);
        sSpeedAltBattler = 0;
        eAlt = Evaluate(&sm);
        *pen += CUSTOM_VGC_PLAN_SPEED_ALT_PCT * (eHit - eAlt) / 100;
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
    // Potatura: se la coppia, già sulle risposte avversarie più probabili, sta sotto la migliore trovata di molto,
    // non vale la pena finire le altre (le coppie vanno in ordine di promessa: la migliore arriva presto). Il suo
    // valore resta quello parziale, comunque peggiore.
    bool32 prune = sJob.bestSoFar != INT32_MIN && sJob.wsum > 0
                && (sJob.alpha * (sJob.sum / sJob.wsum) + (10 - sJob.alpha) * sJob.worst) / 10 + KoBonus(sJob.i) < sJob.bestSoFar - CUSTOM_VGC_PLAN_PRUNE;
    sJob.b++;
    if (prune)
    {
        sJob.b = sJob.nB;
        sJob.a = sJob.nA - 1;
    }
    if (sJob.b >= sJob.nB)
    {
        sJob.b = 0;
        sJob.a++;
        if (sJob.a >= sJob.nA)
        {
            sJob.a = 0;
            sJob.ev[sJob.i] = (sJob.alpha * (sJob.sum / max(sJob.wsum, 1)) + (10 - sJob.alpha) * sJob.worst) / 10;
            if (sJob.ev[sJob.i] + KoBonus(sJob.i) > sJob.bestSoFar)
                sJob.bestSoFar = sJob.ev[sJob.i] + KoBonus(sJob.i);
            sJob.sum = sJob.wsum = 0;
            sJob.worst = INT32_MAX;
            sJob.i++;
            if (sJob.i >= sJob.nOurs)
                return TRUE;
        }
    }
    return FALSE;
}

// Seconda passata: le azioni di velocità/setup/schermi (nostre o del compagno) si simulano anche accanto
// all'azione dell'altro nella coppia migliore trovata. La scelta delle coppie a priori usa stime a 1 turno e
// metteva Tailwind accanto al Fake Out sul bersaglio sbagliato (500 punti sotto quello giusto).
static bool32 PairPresent(const struct PlanAction *x, const struct PlanAction *y)
{
    u32 k;
    for (k = 0; k < sJob.nOurs; k++)
        if (memcmp(&sJob.ours[k], x, sizeof(*x)) == 0 && memcmp(&sJob.partners[k], y, sizeof(*y)) == 0)
            return TRUE;
    return FALSE;
}

static bool32 AddRepairPairs(void)
{
    u32 k, best = 0, n0 = sJob.nOurs;
    if (!CUSTOM_VGC_PLAN_REPAIR_PAIRS || sJob.repaired || n0 == 0)
        return FALSE;
    sJob.repaired = TRUE;
    for (k = 1; k < n0; k++)
        if (sJob.ev[k] > sJob.ev[best])
            best = k;
    for (k = 0; k < n0 && sJob.nOurs < PLAN_ALL_PAIRS; k++)
    {
        const struct PlanAction *u = &sJob.ours[k], *pu = &sJob.partners[k];
        if (u->kind != PLAN_ACT_PROTECT && IsUtilityAction(u) && !IsPairIllegal(u, &sJob.partners[best])
         && !PairPresent(u, &sJob.partners[best]))
        {
            sJob.ours[sJob.nOurs] = *u;
            sJob.partners[sJob.nOurs] = sJob.partners[best];
            sJob.nOurs++;
        }
        if (sJob.nOurs < PLAN_ALL_PAIRS && pu->kind != PLAN_ACT_PROTECT && pu->kind != PLAN_ACT_NONE && IsUtilityAction(pu)
         && !IsPairIllegal(&sJob.ours[best], pu) && !PairPresent(&sJob.ours[best], pu))
        {
            sJob.ours[sJob.nOurs] = sJob.ours[best];
            sJob.partners[sJob.nOurs] = *pu;
            sJob.nOurs++;
        }
    }
    if (sJob.nOurs == n0)
        return FALSE;
    sJob.i = n0; // AdvanceLevel1 ha già azzerato a, b, somme
    sJob.phase = 0;
    return TRUE;
}

static void StartLookAhead(void)
{
    struct PlanAction none = { PLAN_ACT_NONE, 0, -1, MOVE_NONE, 0, 0, 100 };
    enum BattlerId me = sJob.me, partner = GetPartnerBattler(me);
    enum BattlerId fL = GetBattlerLeftFoe(me), fR = GetBattlerRightFoe(me);

    sJob.la = sJob.s1;
    NextTurn(&sJob.la);
    sJob.nM = BuildActions(&sJob.la, me, sJob.mine, PLAN_P2_OURS_PER, TRUE, TRUE);
    sJob.nP = (partner != me) ? BuildActions(&sJob.la, partner, sJob.pa, PLAN_P2_PARTNER_PER, TRUE, FALSE) : 0;
    sJob.nA2 = BuildActions(&sJob.la, fL, sJob.fa, PLAN_P2_FOE_PER, FALSE, TRUE);
    sJob.nB2 = BuildActions(&sJob.la, fR, sJob.fb, PLAN_P2_FOE_PER, FALSE, FALSE);
    if (sJob.nM == 0) { sJob.mine[0] = none; sJob.mine[0].user = me; sJob.nM = 1; }
    if (sJob.nP == 0) { sJob.pa[0] = none; sJob.pa[0].user = partner; sJob.nP = 1; }
    if (sJob.nA2 == 0) { sJob.fa[0] = none; sJob.fa[0].user = fL; sJob.nA2 = 1; }
    if (sJob.nB2 == 0) { sJob.fb[0] = none; sJob.fb[0].user = fR; sJob.nB2 = 1; }
    // Terzo turno: la migliore azione di ciascuno dalla posizione del turno 2 (la prima di ogni lista).
    sJob.g3[0] = sJob.mine[0];
    sJob.g3[1] = sJob.pa[0];
    sJob.g3[2] = sJob.fa[0];
    sJob.g3[3] = sJob.fb[0];
    sJob.i2 = sJob.j2 = sJob.a2 = sJob.b2 = 0;
    sJob.bestI2 = sJob.bestJ2 = 0;
    sJob.refine = FALSE;
    sJob.laBest = INT32_MIN;
    sJob.laSum = sJob.laWsum = 0;
    sJob.laWorst = INT32_MAX;
    sJob.phase = 1;
}

// Spareggio tra coppie di valore identico: 3 = attacco che manda KO di sicuro (danno minimo della tabella, precisione
// piena), 1 = attacco su un avversario, 0 = il resto.
static u32 TieBreakScore(const struct PlanAction *a)
{
    enum BattlerId t = a->target;
    if (a->kind != PLAN_ACT_MOVE || IsBattleMoveStatus(a->move) || t >= MAX_BATTLERS_COUNT || IsBattlerAlly(a->user, t)
     || !IsBattlerAlive(t))
        return 0;
    if (a->moveIndex < MAX_MON_MOVES && gAiLogicData->simulatedDmg[a->user][t][a->moveIndex].minimum >= gBattleMons[t].hp
     && gAiLogicData->moveAccuracy[a->user][t][a->moveIndex] >= 100)
        return 3;
    return 1;
}

// Caso peggiore del cambio (CUSTOM_VGC_PLAN_SWITCH_WORST). Nella verifica a specchio (CUSTOM_VGC_MIRROR_SWITCH_WORST,
// self-play) acceso per il lato senza il bit di verifica-regole, spento per l'altro.
static bool32 SwitchWorstOn(void)
{
#if TESTING
    if (CUSTOM_VGC_MIRROR_SWITCH_WORST)
        return !((gVgcTraining.rulesLegacy >> GetBattlerSide(sJob.me)) & 1);
#endif
    return CUSTOM_VGC_PLAN_SWITCH_WORST && MisplayFixesOn();
}

// Caso peggiore del colpo su chi entra: quello che la stima "da giocatore" (EstimateDamage, usata per chi viene dalla
// panchina) non vede, solo da informazioni pubbliche: spinta di Protosynthesis/Quark Drive già annunciata, Body Press
// (usa la Difesa), abilità che potenziano la mossa. In % del danno stimato.
static u32 WorstCaseAtkPct(const struct PlanState *st, enum BattlerId b, enum Move move)
{
    const struct PlanMon *am = &st->m[b];
    bool32 physical = GetMoveCategory(move) == DAMAGE_CATEGORY_PHYSICAL;
    u32 pct = 100;
    enum Stat boosted = gBattleMons[b].volatiles.paradoxBoostedStat;

    if ((am->ability == ABILITY_PROTOSYNTHESIS || am->ability == ABILITY_QUARK_DRIVE)
     && ((physical && boosted == STAT_ATK) || (!physical && boosted == STAT_SPATK)))
        pct = pct * 13 / 10;
    if (GetMoveEffect(move) == EFFECT_BODY_PRESS && am->atk > 0)
        pct = pct * ApplyStage(am->def, am->stage[PS_DEF]) / max(ApplyStage(am->atk, am->stage[PS_ATK]), 1u);
    switch (am->ability)
    {
    case ABILITY_TOUGH_CLAWS:   if (MoveMakesContact(move)) pct = pct * 13 / 10; break;
    case ABILITY_IRON_FIST:     if (IsPunchingMove(move)) pct = pct * 12 / 10; break;
    case ABILITY_STRONG_JAW:    if (IsBitingMove(move)) pct = pct * 15 / 10; break;
    case ABILITY_SHARPNESS:     if (IsSlicingMove(move)) pct = pct * 15 / 10; break;
    case ABILITY_MEGA_LAUNCHER: if (IsPulseMove(move)) pct = pct * 15 / 10; break;
    case ABILITY_PUNK_ROCK:     if (IsSoundMove(move)) pct = pct * 13 / 10; break;
    case ABILITY_RECKLESS:      if (GetMoveRecoil(move) > 0) pct = pct * 12 / 10; break;
    case ABILITY_SHEER_FORCE:   if (MoveIsAffectedBySheerForce(move)) pct = pct * 13 / 10; break;
    default: break;
    }
    return pct;
}

// Rischio di un cambio volontario: chi entra contro le risposte plausibili degli avversari (tutte le candidate del modello
// del giocatore, non solo le due previste), dirette sul suo slot, con il tiro alto. Un avversario che da solo lo manda
// KO: 30%; i due insieme: 15%. Il costo è il suo KO (pesato per la win condition). La simulazione vede solo le risposte
// previste contro chi era in campo: i cambi finivano in KO quando l'avversario colpiva lo slot con un'altra mossa.
// Con il caso peggiore (SwitchWorstOn) il danno è quello del caso peggiore plausibile; se un solo avversario basta il
// costo è il KO intero e *single dice se la coppia non può vincere (chi esce ha pochi PS, vedi FinishJob).
static s32 SwitchKoRisk(const struct PlanAction *sw, bool32 *single)
{
    struct PlanState tmp;
    enum BattlerId u = sw->user;
    s32 dmg[2] = {0}, hp, pct, roleW = 100;
    u32 side, i;
    bool32 worst = SwitchWorstOn();

    *single = FALSE;
    if (sw->kind != PLAN_ACT_SWITCH || sw->partyIdx < 0 || u >= MAX_BATTLERS_COUNT)
        return 0;
    tmp = sJob.base;
    LoadPartyMon(&tmp, u, (u32)sw->partyIdx);
    hp = tmp.m[u].hp;
    if (hp <= 0)
        return 0;
    for (side = 0; side < 2; side++)
    {
        for (i = 0; i < sJob.nFoeCand[side]; i++)
        {
            struct PlanAction a = sJob.foeCand[side][i];
            enum MoveTarget tt;
            s32 d;
            if (a.kind != PLAN_ACT_MOVE || IsBattleMoveStatus(a.move) || !tmp.m[a.user].alive)
                continue;
            tt = SimMoveTarget(&tmp, a.user, a.move);
            if (tt != TARGET_SELECTED && tt != TARGET_SMART && tt != TARGET_BOTH && tt != TARGET_FOES_AND_ALLY && tt != TARGET_OPPONENT)
                continue;
            a.target = u;
            d = DamageOf(&tmp, &a, u) * (worst ? CUSTOM_VGC_PLAN_SWITCH_WORST_PCT : 108) / 100;
            if (worst)
                d = d * (s32)WorstCaseAtkPct(&tmp, a.user, a.move) / 100;
            if (tt == TARGET_BOTH || tt == TARGET_FOES_AND_ALLY)
                d = d * 3 / 4;
            dmg[side] = max(dmg[side], d);
        }
    }
    if (dmg[0] >= hp || dmg[1] >= hp)
    {
        // Caso peggiore: costa il KO intero; e se chi esce vale meno (pochi PS), il cambio regala un Pokémon sano per
        // salvarne uno quasi finito: non vince mai.
        const struct PlanMon *out = &sJob.base.m[u];
        pct = worst ? CUSTOM_VGC_PLAN_SWITCH_WORST_KO_PCT : 30;
        *single = worst && out->hp * 100 <= out->maxHp * CUSTOM_VGC_PLAN_SWITCH_WORST_CAP_HP;
    }
    else if (dmg[0] + dmg[1] >= hp)
        pct = worst ? CUSTOM_VGC_PLAN_SWITCH_DOUBLE_PCT : 15;
    else
        return 0;
    if (!IsOnPlayerSide(u) && (sJob.winconParty & (1u << (u32)sw->partyIdx)))
        roleW = VW(wincon);
    return pct * (VW(ko) + VW(alive) + 100) * roleW / 10000;
}

static void FinishJob(void)
{
    s32 bestEv = INT32_MIN, chosenEv = INT32_MIN;
    u32 i, bestIdx = 0;

    u32 chosenIdx = PLAN_ALL_PAIRS;
    enum BattlerId partner = GetPartnerBattler(sJob.me);
    u32 switchKo = 0; // bit per coppia: un cambio su chi va KO nel caso peggiore (CUSTOM_VGC_PLAN_SWITCH_WORST)

    // Costo del cambio: il simulatore vede il Pokémon che entra al meglio (pieno di PS, con le sue mosse il turno
    // dopo) ma non il tempo perso, né che il giocatore può punire chi entra. Pesa di più al primo turno della
    // lead, portata per fare il suo lavoro.
    for (i = 0; i < sJob.nOurs; i++)
    {
        if (sJob.ours[i].kind == PLAN_ACT_SWITCH)
            sJob.ev[i] -= IsBattlersFirstTurn(sJob.ours[i].user) ? VW(switchCost) : VW(switchCost) / 2;
        if (sJob.partners[i].kind == PLAN_ACT_SWITCH)
            sJob.ev[i] -= IsBattlersFirstTurn(sJob.partners[i].user) ? VW(switchCost) : VW(switchCost) / 2;
        if (CUSTOM_VGC_PLAN_SWITCH_RISK && MisplayFixesOn())
        {
            bool32 s1, s2;
            sJob.ev[i] -= SwitchKoRisk(&sJob.ours[i], &s1) + SwitchKoRisk(&sJob.partners[i], &s2);
            if (s1 || s2)
                switchKo |= 1u << i;
        }
        // Doppio Protect: un turno regalato all'avversario (setup, velocità, cambi gratis) che i tre turni simulati
        // vedono poco; costa come mezzo cambio.
        if (sJob.ours[i].kind == PLAN_ACT_PROTECT && sJob.partners[i].kind == PLAN_ACT_PROTECT)
            sJob.ev[i] -= VW(switchCost) / 2;
    }
    // Spinta delle regole (M8): solo per gli effetti che la simulazione non modella (Gravity, Quick Guard, Instruct...).
    for (i = 0; i < sJob.nOurs; i++)
        sJob.ev[i] += ActionPrior(&sJob.ours[i], &sJob.partners[i], TRUE) + ActionPrior(&sJob.partners[i], &sJob.ours[i], TRUE);
    // Libro dei piani (M9): ai turni 1 e 2, bonus alle azioni congiunte del piano scelto in team preview.
    for (i = 0; i < sJob.nOurs; i++)
        sJob.ev[i] += AI_VgcBookPairBonus(sJob.me, sJob.ours[i].kind <= PLAN_ACT_PROTECT ? sJob.ours[i].move : MOVE_NONE,
                                          partner, sJob.partners[i].kind <= PLAN_ACT_PROTECT ? sJob.partners[i].move : MOVE_NONE);
    // KO sicuri (ia-vgc-ko): premio delle coppie con un KO garantito.
    for (i = 0; i < sJob.nOurs; i++)
        sJob.ev[i] += KoBonus(i);
    // Caso peggiore del cambio: una coppia con un cambio su chi una risposta plausibile di un solo avversario manda KO
    // non batte la migliore coppia senza un cambio così (Protect, attacco, un altro Pokémon dalla panchina). Il KO di chi
    // entra regala un Pokémon sano; lasciare in campo chi era minacciato costa al più lui, e il cambio dopo il KO è gratis.
    if (switchKo != 0)
    {
        s32 bestSafe = INT32_MIN;
        for (i = 0; i < sJob.nOurs; i++)
            if (!(switchKo & (1u << i)) && sJob.ev[i] > bestSafe)
                bestSafe = sJob.ev[i];
        if (bestSafe != INT32_MIN)
        {
            for (i = 0; i < sJob.nOurs; i++)
                if ((switchKo & (1u << i)) && sJob.ev[i] >= bestSafe - 1)
                    sJob.ev[i] = bestSafe - 2;
        }
#if TESTING
        {
            extern s32 Test_MgbaPrintf(const char *fmt, ...);
            Test_MgbaPrintf("VGCSWKO b%d coppie=%x migliore_sicura=%d", sJob.me, switchKo, bestSafe);
        }
#endif
    }
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
    if (chosenIdx != PLAN_ALL_PAIRS && bestEv < chosenEv + PLAN_OVERRIDE_MARGIN)
        bestIdx = chosenIdx;
    // ...ma tra coppie di valore identico (azione indifferente nella simulazione: chi la usa va KO prima o subisce Fake
    // Out) vince chi attacca, e prima chi manda KO di sicuro: se la previsione sbaglia, il colpo serve; un setup o un
    // Iron Defense di chi sta per cadere no.
    if (MisplayFixesOn())
    {
        u32 bestTie = TieBreakScore(&sJob.ours[bestIdx]) + TieBreakScore(&sJob.partners[bestIdx]);
        for (i = 0; i < sJob.nOurs; i++)
        {
            u32 t;
            if (sJob.ev[i] != sJob.ev[bestIdx])
                continue;
            t = TieBreakScore(&sJob.ours[i]) + TieBreakScore(&sJob.partners[i]);
            if (t > bestTie)
            {
                bestTie = t;
                bestIdx = i;
            }
        }
    }
    // Esplorazione del self-play (M9, solo test): a volte una coppia vicina alla migliore.
    bestIdx = AI_VgcExplorePick(sJob.me, bestIdx, sJob.ev, sJob.nOurs);
    // --- Logger VGC (src/vgc_battle_log.c): coppie simulate e risposte previste. Solo righe di log. ---
    VgcLog_PlanHead(sJob.me, sJob.nOurs, bestIdx, chosenIdx, sJob.frames, sJob.sims);
    for (i = 0; i < sJob.nOurs; i++)
        VgcLog_PlanPair(sJob.me, i, sJob.ours[i].kind, sJob.ours[i].move, sJob.ours[i].user, sJob.ours[i].target, sJob.ours[i].partyIdx,
                        sJob.partners[i].kind, sJob.partners[i].move, sJob.partners[i].user, sJob.partners[i].target,
                        sJob.partners[i].partyIdx, sJob.ev[i], (i == bestIdx) | ((i == chosenIdx) << 1));
    for (i = 0; i < sJob.nA + sJob.nB; i++)
    {
        const struct PlanAction *f = (i < sJob.nA) ? &sJob.foeA[i] : &sJob.foeB[i - sJob.nA];
        VgcLog_PlanFoe(sJob.me, i >= sJob.nA, i < sJob.nA ? i : i - sJob.nA, f->kind, f->move, f->user, f->target, f->partyIdx, f->weight);
    }
    // --- fine logger VGC ---
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
        for (i = 0; i < gBattlersCount; i++)
        {
            u32 k;
            for (k = 0; k < sJob.nLikely[i]; k++)
                Test_MgbaPrintf("VGCLIKELY b%d: m%d p%d", i, sJob.likelyMove[i][k], sJob.likelyP[i][k]);
        }
        Test_MgbaPrintf("VGCPROF righe: eval=%d turno=%d azioni=%d ko=%d stima=%d", sProf[0], sProf[1], sProf[2], sProf[3], sProf[4]);
        Test_MgbaPrintf("VGCCPU b%d: fotogrammi=%d simulazioni=%d coppie=%d eval=%d build=%d ko=%d stima=%d danni=%d", sJob.me, sJob.frames,
                        sJob.sims, sJob.nOurs, sCntEval, sCntBuild, sCntKo, sCntEst, sCntDmg);
        for (i = 0; i < sJob.nA; i++)
            Test_MgbaPrintf("VGCFOE A%d: k%d m%d t%d w%d", i, sJob.foeA[i].kind, sJob.foeA[i].move, sJob.foeA[i].target, sJob.foeA[i].weight);
        for (i = 0; i < sJob.nB; i++)
            Test_MgbaPrintf("VGCFOE B%d: k%d m%d t%d w%d", i, sJob.foeB[i].kind, sJob.foeB[i].move, sJob.foeB[i].target, sJob.foeB[i].weight);
    }
#endif

#if TESTING
    if (gBattleTurnCounter == 0 && sTestLog.n == 0)
    {
        for (i = 0; i < sJob.nOurs; i++)
        {
            sTestLog.user[i] = sJob.ours[i].user;
            sTestLog.move[i] = (sJob.ours[i].kind == PLAN_ACT_SWITCH) ? MOVE_NONE : sJob.ours[i].move;
            sTestLog.puser[i] = sJob.partners[i].user;
            sTestLog.pmove[i] = (sJob.partners[i].kind == PLAN_ACT_SWITCH || sJob.partners[i].kind == PLAN_ACT_NONE) ? MOVE_NONE : sJob.partners[i].move;
            sTestLog.ev[i] = sJob.ev[i];
        }
        sTestLog.n = sJob.nOurs;
    }
    // Spinte delle candidate della decisione del turno 0 (al turno 0 decide un solo pianificatore per lato: il compagno
    // riceve la sua metà). Restano fino alla lotta dopo: le legge il THEN dei test.
    if (gBattleTurnCounter == 0)
    {
        sTestPrior.n = 0;
        for (i = 0; i < 2; i++)
        {
            u32 c;
            enum BattlerId pb = (i == 0) ? partner : sJob.me;
            struct PlanAction other = (i == 0) ? RulesAction(pb, sJob.origPMove, sJob.origPTarget)
                                               : RulesAction(pb, sJob.origMove, sJob.origTarget);
            for (c = 0; c < sJob.nCand[i] && sTestPrior.n < ARRAY_COUNT(sTestPrior.move); c++)
            {
                sTestPrior.user[sTestPrior.n] = sJob.cand[i][c].user;
                sTestPrior.move[sTestPrior.n] = sJob.cand[i][c].move;
                sTestPrior.prior[sTestPrior.n] = ActionPrior(&sJob.cand[i][c], &other, FALSE);
                Test_MgbaPrintf("VGCPRIOR b%d m%d t%d = %d", sTestPrior.user[sTestPrior.n], sTestPrior.move[sTestPrior.n],
                                sJob.cand[i][c].target, sTestPrior.prior[sTestPrior.n]);
                sTestPrior.n++;
            }
        }
    }
#endif
    sJob.resultKind = sJob.ours[bestIdx].kind;
    sJob.resultMove = sJob.ours[bestIdx].moveIndex;
    sJob.resultTarget = sJob.ours[bestIdx].target;
    sJob.resultParty = sJob.ours[bestIdx].partyIdx;

    // La metà del partner della coppia vincente vale per lui quando tocca a lui scegliere.
#define sPartnerDecision sPartnerDecisionBySide[GetBattlerSide(sJob.me)]
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
#undef sPartnerDecision
    sJob.finished = TRUE;
}

#if TESTING
// Miglior valore simulato di una coppia in cui `battler` usa `move` (INT32_MIN se nessuna coppia simulata la usa).
s32 AI_VgcPlanTestValue(enum BattlerId battler, enum Move move)
{
    s32 best = INT32_MIN;
    u32 i;
    for (i = 0; i < sTestLog.n; i++)
        if ((sTestLog.user[i] == battler && sTestLog.move[i] == move) || (sTestLog.puser[i] == battler && sTestLog.pmove[i] == move))
            best = max(best, sTestLog.ev[i]);
    return best;
}
#endif

#if TESTING
// Spinta più alta data a una candidata di `battler` con `move` nella prima decisione del turno 0 (INT32_MIN = mai candidata).
s32 AI_VgcPlanTestPrior(enum BattlerId battler, enum Move move)
{
    s32 best = INT32_MIN;
    u32 i;
    for (i = 0; i < sTestPrior.n; i++)
        if (sTestPrior.user[i] == battler && sTestPrior.move[i] == move)
            best = max(best, sTestPrior.prior[i]);
    return best;
}
#endif

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
        // Già in arrivo nell'altro posto (U-turn, Baton Pass, rimpiazzo del compagno in corso): non due volte lo stesso.
        if (i == gBattleStruct->monToSwitchIntoId[p] || i == gBattleStruct->AI_monToSwitchIntoId[p]
         || (IsBattlerAlive(p) && i == gBattlerPartyIndexes[p]))
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
#if TESTING
    if (gBattleTurnCounter == 0)
        sTestLog.n = 0;
#endif
    sJob.active = FALSE;
    sJob.finished = FALSE;
    sPartnerDecisionBySide[B_SIDE_PLAYER].valid = FALSE;
    sPartnerDecisionBySide[B_SIDE_OPPONENT].valid = FALSE;
}

// TRUE se il pianificatore lavora (o ha un risultato non ancora applicato) per un altro battler in questo turno.
bool32 AI_VgcPlanIsBusy(enum BattlerId battler)
{
    return sJob.active && sJob.me != battler && sJob.turn == gBattleTurnCounter;
}

// Attacchi probabili non ancora visti di un Pokémon del giocatore in campo, dal belief (statistiche d'uso
// ristrette da ciò che si è visto). Esclusi quelli che il simulatore non sa giocare (Focus Punch, a due turni).
static void BuildLikelyAttacks(enum BattlerId b)
{
    struct ScoutMoveChance mc[12];
    u32 n = Scout_BattlerLikelyMoves(b, mc, ARRAY_COUNT(mc)), i, k; // nella sua squadra (contesto)
    enum Move *seen = GetMovesArray(b);

    sJob.nLikely[b] = 0;
    for (i = 0; i < n; i++)
    {
        enum Move m = mc[i].move;
        u32 p;
        bool32 dup = FALSE;
        if (IsUnreliableGuess(m))
            continue;
        for (k = 0; k < MAX_MON_MOVES; k++)
            if (seen[k] == m)
                dup = TRUE;
        if (dup)
            continue;
        p = Scout_BattlerMoveChance(b, m);
        if (p < 15)
            continue;
        // Inserimento ordinato per probabilità.
        for (k = sJob.nLikely[b]; k > 0 && sJob.likelyP[b][k - 1] < p; k--)
        {
            if (k < PLAN_LIKELY_MAX)
            {
                sJob.likelyMove[b][k] = sJob.likelyMove[b][k - 1];
                sJob.likelyP[b][k] = sJob.likelyP[b][k - 1];
            }
        }
        if (k < PLAN_LIKELY_MAX)
        {
            sJob.likelyMove[b][k] = m;
            sJob.likelyP[b][k] = p;
            if (sJob.nLikely[b] < PLAN_LIKELY_MAX)
                sJob.nLikely[b]++;
        }
    }
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
#define sPartnerDecision sPartnerDecisionBySide[GetBattlerSide(me)]
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
#undef sPartnerDecision

    {
        enum BattlerId p = GetPartnerBattler(me);
        sJob.origPMove = gAiBattleData->chosenMoveIndex[p];
        sJob.origPTarget = gAiBattleData->chosenTarget[p];
    }
    AI_VgcBuildTeamGraph(GetBattlerParty(me), &sJob.graph);
#if TESTING
    sCntEval = sCntBuild = sCntKo = sCntEst = sCntDmg = 0;
    memset(sProf, 0, sizeof(sProf));
#endif
    sJob.bench[GetBattlerSide(me)] = CountUsablePartyMons(me);
    sJob.bench[GetBattlerSide(fL)] = CountUsablePartyMons(fL);
    sJob.protectChance[fL] = AI_VgcProtectChance(fL);
    sJob.protectChance[fR] = AI_VgcProtectChance(fR);
    sJob.switchChance[fL] = AI_VgcSwitchChance(fL);
    sJob.switchChance[fR] = AI_VgcSwitchChance(fR);
    {
        enum BattlerId foes[2] = { fL, fR };
        u32 f;
        for (f = 0; f < 2; f++)
        {
            enum BattlerId fb = foes[f];
            enum HoldEffect he = gAiLogicData->holdEffects[fb];
            if (!IsBattlerAlive(fb))
                continue;
            // Assault Vest o Choice (visti o dedotti): niente Protect.
            if (he == HOLD_EFFECT_ASSAULT_VEST || he == HOLD_EFFECT_CHOICE_BAND || he == HOLD_EFFECT_CHOICE_SPECS || he == HOLD_EFFECT_CHOICE_SCARF)
                sJob.protectChance[fb] = 0;
            BuildLikelyAttacks(fb);
        }
    }
    InitState(&sJob.base);
    // Valore del sonno di ognuno in campo: base CUSTOM_VGC_EVAL_SLEEP scalata dalla minaccia (25 + 0..150)%.
    {
        enum BattlerId q;
        for (q = 0; q < MAX_BATTLERS_COUNT; q++)
            sJob.sleepW[q] = 0;
        for (q = 0; q < gBattlersCount; q++)
            if (sJob.base.m[q].alive)
            {
                sJob.threatPct[q] = ThreatPct(&sJob.base, q);
                sJob.sleepW[q] = CUSTOM_VGC_EVAL_SLEEP * (25 + sJob.threatPct[q]) / 100;
            }
    }
    // Velocità incerta (onesta: solo l'intervallo pubblico della specie o quello del belief, mai la vera): se la
    // stima di un avversario e una Velocità plausibile dall'altra parte dell'intervallo danno ordini diversi con
    // uno dei nostri, il turno 1 si gioca anche con l'ordine rovesciato (ramo pesato in MissPenalty).
    {
        enum BattlerId f, q;
        for (f = 0; f < MAX_BATTLERS_COUNT; f++)
            sJob.speedAlt[f] = 0;
        for (f = 0; f < gBattlersCount; f++)
        {
            u16 lo, hi, hp, est[NUM_STATS];
            u32 alt;
            const struct PlanMon *fm = &sJob.base.m[f];
            if (!IsOnPlayerSide(f) || !fm->alive || fm->partyIdx >= 0)
                continue;
            Scout_BattlerSpeedRange(f, &lo, &hi);
            Scout_EstimateBattlerStats(f, &hp, est);
            if (hi <= lo || est[STAT_SPEED] == 0)
                continue;
            // L'alternativa sta nel quarto opposto dell'intervallo rispetto alla stima.
            alt = (est[STAT_SPEED] * 2 >= (u32)lo + hi) ? lo + (hi - lo) / 4 : hi - (hi - lo) / 4;
            alt = fm->speed * alt / est[STAT_SPEED];
            for (q = 0; q < gBattlersCount; q++)
            {
                const struct PlanMon *qm = &sJob.base.m[q];
                if (IsOnPlayerSide(q) || !qm->alive)
                    continue;
                if ((fm->speed > qm->speed) != (alt > qm->speed))
                {
                    sJob.speedAlt[f] = alt;
                    break;
                }
            }
        }
    }
    sJob.nA = BuildActions(&sJob.base, fL, sJob.foeA, PLAN_FOE_PER, FALSE, TRUE);
    sJob.nB = BuildActions(&sJob.base, fR, sJob.foeB, PLAN_FOE_PER, FALSE, TRUE);
    if (sJob.nA == 0) { sJob.foeA[0] = none; sJob.foeA[0].user = fL; sJob.nA = 1; }
    if (sJob.nB == 0) { sJob.foeB[0] = none; sJob.foeB[0].user = fR; sJob.nB = 1; }
    // Modello del giocatore livello 1: più azioni plausibili per avversario, scelte poi dalla fase 3.
    sJob.nFoeCand[0] = BuildActions(&sJob.base, fL, sJob.foeCand[0], PLAN_FOE_CAND, FALSE, TRUE);
    sJob.nFoeCand[1] = BuildActions(&sJob.base, fR, sJob.foeCand[1], PLAN_FOE_CAND, FALSE, TRUE);
    sJob.pmSide = sJob.pmIdx = 0;
    // Tutte le azioni legali dei due lati; il loro valore a 1 turno lo calcola la fase 2 (a pezzi, in Step).
    {
        enum BattlerId p = GetPartnerBattler(me);
        u32 n0 = BuildOurFirstActions(me, sJob.cand[0], sJob.origMove, sJob.origTarget), n1 = 0;
        if (p != me && IsBattlerAlive(p))
        {
            n1 = BuildOurFirstActions(p, sJob.cand[1], sJob.origPMove, sJob.origPTarget);
            AddComboCands(me, p, sJob.cand[0], &n0, 0, &sJob.graph);
            AddComboCands(p, me, sJob.cand[1], &n1, 0, &sJob.graph);
        }
        sJob.nCand[0] = n0;
        sJob.nCand[1] = n1;
        if (n0 == 0)
            return FALSE;
    }
    sJob.rankSide = sJob.rankIdx = 0;
    sJob.bestSoFar = INT32_MIN;
    sJob.repaired = FALSE;
    {
        u32 side = GetBattlerSide(me);
        if (!(sVgcWeightsSet & (1u << side)))
        {
            DefaultEvalWeights(&sVgcWeights[side]);
            sVgcWeightsSet |= 1u << side;
        }
        sJob.w = &sVgcWeights[side];
    }
    {
        // Win condition del piano dell'allenatore (vgc_gameplans.h): pesano di più nella valutazione.
        u16 trainerId = AI_TrainerIdOfBattler(me);
        struct Pokemon *party = GetBattlerParty(me);
        u32 i;
        sJob.winconParty = 0;
        for (i = 0; i < PARTY_SIZE; i++)
            if (AI_VgcPlanIsWinCon(trainerId, GetMonData(&party[i], MON_DATA_SPECIES)))
                sJob.winconParty |= 1u << i;
    }
    {
        // α dinamico: in svantaggio di Pokémon si gioca sulla media (serve un colpo di fortuna, il caso peggiore
        // è già perso); in vantaggio si guarda di più il caso peggiore (non buttare via la partita).
        s32 ours = CountUsablePartyMons(sJob.me), theirs = CountUsablePartyMons(GetBattlerLeftFoe(sJob.me));
        enum BattlerId q;
        for (q = 0; q < gBattlersCount; q++)
            if (IsBattlerAlive(q))
            {
                if (IsBattlerAlly(q, sJob.me)) ours++;
                else theirs++;
            }
        sJob.alpha = (ours < theirs) ? 8 : (ours > theirs) ? 6 : 7;
    }
    sJob.phase = (sJob.nFoeCand[0] + sJob.nFoeCand[1] > 0) ? 3 : 2;
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
    if (!sJob.finished)
        sJob.frames++;
    // Nessun tetto al costo per scelta (la decisione giusta conta più di quella veloce: mentre l'IA pensa si vede
    // l'allenatore con "Penso..."). CUSTOM_VGC_PLAN_MAX_FRAMES > 0 rimette un tetto; il limite di sicurezza vale
    // solo contro cicli infiniti. Oltre, l'albero si chiude con le coppie già simulate per intero.
    if (!sJob.finished && (sJob.phase == 0 || sJob.phase == 1) && sJob.i > 0
     && ((CUSTOM_VGC_PLAN_MAX_FRAMES > 0 && sJob.frames >= CUSTOM_VGC_PLAN_MAX_FRAMES) || sJob.frames >= CUSTOM_VGC_PLAN_SAFETY_FRAMES))
    {
        sJob.nOurs = sJob.i;
        FinishJob();
    }
    while (budget > 0 && !sJob.finished && ScanlinesSince(start) < CUSTOM_VGC_PLAN_SCANLINES_PER_FRAME)
    {
        if (sJob.phase == 3)
        {
            // Modello del giocatore livello 1: ogni sua azione plausibile contro le nostre azioni delle regole (le
            // più probabili ai suoi occhi) e l'azione più probabile del suo compagno; vale per lui il nostro valore
            // cambiato di segno. Solo informazione onesta: le sue mosse sono quelle del belief, non le vere.
            u32 side = sJob.pmSide, idx = sJob.pmIdx;
            if (idx < sJob.nFoeCand[side])
            {
                enum BattlerId p = GetPartnerBattler(sJob.me);
                struct PlanAction ours = RulesAction(sJob.me, sJob.origMove, sJob.origTarget);
                struct PlanAction partner = RulesAction(p, sJob.origPMove, sJob.origPTarget);
                const struct PlanAction *acts[4];
                struct PlanState st = sJob.base;

                if (p == sJob.me)
                    partner.kind = PLAN_ACT_NONE;
                // Il giocatore sceglie senza sapere cosa facciamo: non mette in conto la nostra mossa di stato su un suo
                // Pokémon (Taunt, Encore, sonno...). Con l'azione delle regole Taunt sul setter, il suo Trick Room "falliva"
                // nel modello, non veniva previsto e Taunt non serviva più a niente: una profezia che si annulla da sola.
                if (ours.kind == PLAN_ACT_MOVE && IsBattleMoveStatus(ours.move) && ours.target < gBattlersCount
                 && !IsBattlerAlly(ours.user, ours.target))
                    ours.kind = PLAN_ACT_NONE;
                if (partner.kind == PLAN_ACT_MOVE && IsBattleMoveStatus(partner.move) && partner.target < gBattlersCount
                 && !IsBattlerAlly(partner.user, partner.target))
                    partner.kind = PLAN_ACT_NONE;
                acts[0] = &ours;
                acts[1] = &partner;
                acts[2] = &sJob.foeCand[side][idx];
                acts[3] = (side == 0) ? &sJob.foeB[0] : &sJob.foeA[0];
                RunTurn(&st, acts, 4);
                sJob.foeVal[side][idx] = Evaluate(&st);
                budget--;
                // Il giocatore non sa cosa faremo: ogni sua azione vale anche contro di noi fermi (media dei due casi).
                // Contro la sola azione delle regole un attacco su chi le regole davano KO prima (o in Protect) non
                // valeva niente e la minaccia sulla win condition spariva dalle risposte previste.
                if (FoeVsIdleOn())
                {
                    struct PlanAction idleA = ours, idleB = partner;
                    idleA.kind = PLAN_ACT_NONE;
                    idleB.kind = PLAN_ACT_NONE;
                    acts[0] = &idleA;
                    acts[1] = &idleB;
                    st = sJob.base;
                    RunTurn(&st, acts, 4);
                    sJob.foeVal[side][idx] = (sJob.foeVal[side][idx] + Evaluate(&st)) / 2;
                    budget--;
                }
                sJob.pmIdx++;
                continue;
            }
            if (side == 0)
            {
                sJob.pmSide = 1;
                sJob.pmIdx = 0;
                continue;
            }
            if (sJob.nFoeCand[0] > 0)
                sJob.nA = PickFoeActions(0, sJob.foeA);
            if (sJob.nFoeCand[1] > 0)
                sJob.nB = PickFoeActions(1, sJob.foeB);
            sJob.phase = 2;
            continue;
        }
        if (sJob.phase == 2)
        {
            // Valore a 1 turno di ogni azione legale: con il compagno che fa la mossa delle regole e gli avversari
            // la loro azione più probabile. Serve solo a ordinare le candidate (la scelta la fa l'albero).
            // Riferimento del compagno: prima la mossa delle regole (lato 0), poi la migliore candidata dell'altro lato
            // già ordinata (lato 1, e lato 0 una seconda volta come rankSide 2). Le regole M8 non danno più bonus, quindi
            // da sole sono un riferimento povero (es. Charizard col compagno "fermo" non vede il valore di Heat Wave).
            u32 side = (sJob.rankSide == 2) ? 0 : sJob.rankSide, idx = sJob.rankIdx;
            if (idx < sJob.nCand[side])
            {
                enum BattlerId p = GetPartnerBattler(sJob.me);
                struct PlanAction other;
                if (sJob.rankSide == 0 || sJob.nCand[side ^ 1] == 0)
                    other = (side == 0) ? RulesAction(p, sJob.origPMove, sJob.origPTarget)
                                        : RulesAction(sJob.me, sJob.origMove, sJob.origTarget);
                else
                    other = sJob.cand[side ^ 1][BestCandIdx(side ^ 1)];
                const struct PlanAction *acts[4], *acts4[4];
                struct PlanState st = sJob.base;
                s32 pen;

                if (p == sJob.me)
                    other.kind = PLAN_ACT_NONE;
                acts4[0] = acts[0] = &sJob.cand[side][idx];
                acts4[1] = acts[1] = &other;
                acts4[2] = acts[2] = &sJob.foeA[0];
                acts4[3] = acts[3] = &sJob.foeB[0];
                RunTurn(&st, acts, 4);
                budget -= 1 + MissPenalty(&sJob.base, &st, acts4, &pen);
#if TESTING
                // Diagnostica: lo stato dopo il turno 1 per le candidate Fake Out (confronto dei bersagli).
                if (sJob.cand[side][idx].move == MOVE_FAKE_OUT)
                {
                    extern s32 Test_MgbaPrintf(const char *fmt, ...);
                    Test_MgbaPrintf(" VGCCANDSTATE m%d t%d:", sJob.cand[side][idx].move, sJob.cand[side][idx].target);
                    sEvalDebug = TRUE;
                }
#endif
                sJob.candVal[side][idx] = Evaluate(&st) - pen + ActionPrior(&sJob.cand[side][idx], &other, FALSE);
                if (sJob.cand[side][idx].kind == PLAN_ACT_SWITCH)
                    sJob.candVal[side][idx] -= IsBattlersFirstTurn(sJob.cand[side][idx].user) ? VW(switchCost) : VW(switchCost) / 2;
#if TESTING
                sEvalDebug = FALSE;
#endif
                sJob.rankIdx++;
                continue;
            }
            if (sJob.rankSide == 0 && sJob.nCand[1] > 0)
            {
                sJob.rankSide = 1;
                sJob.rankIdx = 0;
                continue;
            }
            if (sJob.rankSide == 1 && sJob.nCand[0] > 0)
            {
                sJob.rankSide = 2; // di nuovo il lato 0, contro la migliore del lato 1
                sJob.rankIdx = 0;
                continue;
            }
#if TESTING
            {
                extern s32 Test_MgbaPrintf(const char *fmt, ...);
                u32 sd, ci;
                for (sd = 0; sd < 2; sd++)
                    for (ci = 0; ci < sJob.nCand[sd]; ci++)
                        Test_MgbaPrintf("VGCCAND lato%d: k%d m%d t%d = %d", sd, sJob.cand[sd][ci].kind, sJob.cand[sd][ci].move,
                                        sJob.cand[sd][ci].target, sJob.candVal[sd][ci]);
            }
#endif
            if (StartPairRanking())
            {
                sJob.phase = 4;
                continue;
            }
            sJob.phase = 5;
            continue;
        }
        if (sJob.phase == 4)
        {
            // Valore a 1 turno di ogni coppia (nostra azione, azione del partner) simulata insieme, contro le risposte
            // più probabili, con gli stessi costi e spinte di FinishJob. Ordina le coppie da simulare per intero.
            u32 i = sJob.pi, j = sJob.pj;
            if (i < sJob.nTrim[0])
            {
                const struct PlanAction *x = &sJob.trim[0][i], *y = &sJob.trim[1][j];
                if (!IsPairIllegal(x, y))
                {
                    const struct PlanAction *acts[4], *acts4[4];
                    struct PlanState st = sJob.base;
                    s32 pen;

                    acts4[0] = acts[0] = x;
                    acts4[1] = acts[1] = y;
                    acts4[2] = acts[2] = &sJob.foeA[0];
                    acts4[3] = acts[3] = &sJob.foeB[0];
                    RunTurn(&st, acts, 4);
                    budget -= 1 + MissPenalty(&sJob.base, &st, acts4, &pen);
                    sJob.pairVal[i][j] = Evaluate(&st) - pen - PairCost(x, y)
                                       + ActionPrior(x, y, TRUE) + ActionPrior(y, x, TRUE);
#if TESTING
                    {
                        extern s32 Test_MgbaPrintf(const char *fmt, ...);
                        Test_MgbaPrintf("VGCPAIR1 k%d m%d t%d + k%d m%d t%d = %d", x->kind, x->move, x->target,
                                        y->kind, y->move, y->target, sJob.pairVal[i][j]);
                    }
#endif
                }
                sJob.pj++;
                if (sJob.pj >= sJob.nTrim[1])
                {
                    sJob.pj = 0;
                    sJob.pi++;
                }
                continue;
            }
            sJob.phase = 5;
            continue;
        }
        if (sJob.phase == 5)
        {
            sJob.nOurs = BuildPairs(sJob.me);
            sJob.phase = 0;
            if (sJob.nOurs == 0)
            {
                sJob.resultKind = PLAN_ACT_NONE; // niente di meglio: resta la scelta delle regole
                sJob.finished = TRUE;
            }
            else if (sJob.nOurs == 1)
            {
                sJob.ev[0] = 0; // una sola coppia possibile: niente da simulare
                FinishJob();
            }
            continue;
        }
        if (sJob.phase == 0)
        {
            const struct PlanAction *acts[4] = { &sJob.ours[sJob.i], &sJob.partners[sJob.i], &sJob.foeA[sJob.a], &sJob.foeB[sJob.b] };
            const struct PlanAction *acts4[4] = { &sJob.ours[sJob.i], &sJob.partners[sJob.i], &sJob.foeA[sJob.a], &sJob.foeB[sJob.b] };
            sJob.s1 = sJob.base;
            RunTurn(&sJob.s1, acts, 4);
            budget--;
            budget -= MissPenalty(&sJob.base, &sJob.s1, acts4, &sJob.missPen);
            KoCreditBranch(&sJob.s1, acts[0], acts[1], acts[2], acts[3], PairWeight(&sJob.foeA[sJob.a], &sJob.foeB[sJob.b]));
            if (CUSTOM_VGC_PLAN_DEPTH >= 2)
            {
                StartLookAhead();
            }
            else
            {
                Accumulate(Evaluate(&sJob.s1) - sJob.missPen, PairWeight(&sJob.foeA[sJob.a], &sJob.foeB[sJob.b]));
                if (AdvanceLevel1() && !AddRepairPairs())
                    FinishJob();
            }
        }
        else
        {
            // Turno 2: ogni nostra coppia di risposte (i2, j2) contro le loro (a2, b2). Il terzo turno ridotto
            // (GreedyValue) si gioca solo sulla linea migliore, in un secondo passaggio (refine): farlo in ogni
            // foglia raddoppiava il costo per raffinare linee che poi non vengono scelte.
            const struct PlanAction *acts[4] = { &sJob.mine[sJob.i2], &sJob.pa[sJob.j2], &sJob.fa[sJob.a2], &sJob.fb[sJob.b2] };
            struct PlanState st = sJob.la;
            s32 v, w = PairWeight(&sJob.fa[sJob.a2], &sJob.fb[sJob.b2]);
            bool32 lineDone = FALSE;

            RunTurn(&st, acts, 4);
#if TESTING
            if (sJob.refine && sJob.a2 == 0 && sJob.b2 == 0 && sJob.a == 0 && sJob.b == 0)
            {
                extern s32 Test_MgbaPrintf(const char *fmt, ...);
                Test_MgbaPrintf(" VGCLINE pair%d turno2: m%d t%d + m%d t%d contro m%d t%d + m%d t%d", sJob.i, acts[0]->move, acts[0]->target,
                                acts[1]->move, acts[1]->target, acts[2]->move, acts[2]->target, acts[3]->move, acts[3]->target);
                sEvalDebug = TRUE;
                Test_MgbaPrintf(" VGCLINE dopo turno1:");
                Evaluate(&sJob.s1);
                Test_MgbaPrintf(" VGCLINE dopo turno2:");
            }
#endif
            v = Evaluate(&st);
#if TESTING
            sEvalDebug = FALSE;
#endif
            budget--;
            if (sJob.refine)
            {
                if (CUSTOM_VGC_PLAN_ROLLOUT_TURNS > 0)
                    v = (2 * v + RolloutValue(&st)) / 3;
                else
                    v = (v + GreedyValue(&st)) / 2;
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
                    s32 ev2 = (sJob.alpha * (sJob.laSum / max(sJob.laWsum, 1)) + (10 - sJob.alpha) * sJob.laWorst) / 10;
                    sJob.a2 = 0;
                    sJob.laSum = sJob.laWsum = 0;
                    sJob.laWorst = INT32_MAX;
                    if (sJob.refine)
                    {
                        sJob.laBest = ev2;  // la linea migliore, ora con il terzo turno
                        lineDone = TRUE;
                    }
                    else
                    {
                        if (ev2 > sJob.laBest)
                        {
                            sJob.laBest = ev2;
                            sJob.bestI2 = sJob.i2;
                            sJob.bestJ2 = sJob.j2;
                        }
                        sJob.j2++;
                        if (sJob.j2 >= sJob.nP)
                        {
                            sJob.j2 = 0;
                            sJob.i2++;
                            if (sJob.i2 >= sJob.nM)
                            {
                                if (CUSTOM_VGC_PLAN_DEPTH >= 3)
                                {
                                    sJob.refine = TRUE;
                                    sJob.i2 = sJob.bestI2;
                                    sJob.j2 = sJob.bestJ2;
                                }
                                else
                                {
                                    lineDone = TRUE;
                                }
                            }
                        }
                    }
                }
            }
            if (lineDone)
            {
                s32 e1 = Evaluate(&sJob.s1);
                s32 v1 = (e1 + sJob.laBest) / 2 - sJob.missPen;
#if TESTING
                {
                    extern s32 Test_MgbaPrintf(const char *fmt, ...);
                    Test_MgbaPrintf("VGCLEAF pair%d foes(%d,%d): turno1=%d turno2=%d mancati=-%d", sJob.i, sJob.a, sJob.b,
                                    e1, sJob.laBest, sJob.missPen);
                }
#endif
                Accumulate(v1, PairWeight(&sJob.foeA[sJob.a], &sJob.foeB[sJob.b]));
                sJob.phase = 0;
                if (AdvanceLevel1() && !AddRepairPairs())
                    FinishJob();
            }
        }
    }
    sJob.sims += CUSTOM_VGC_PLAN_SIMS_PER_FRAME - min(budget, CUSTOM_VGC_PLAN_SIMS_PER_FRAME);
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
    // Il pianificatore ha scelto una mossa avendo simulato anche il cambio: un cambio deciso dalle regole non vale più
    // (prima restava attivo quando la mossa coincideva con quella delle regole, e si cambiava comunque; al primo
    // turno della lotta il cambio non è nemmeno tra le candidate: le lead restano).
    if (gAiLogicData->shouldSwitch & (1u << me))
    {
        gAiLogicData->shouldSwitch &= ~(1u << me);
        changed = TRUE;
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
