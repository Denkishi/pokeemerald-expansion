// Harness di self-play VGC: una partita IA contro IA tra due veri allenatori VGC (src/data/trainers.party,
// linea AI con "Vgc Doubles"), con log strutturato e controllo dei misplay a ogni turno.
//
// Come funziona
//   - Il test "VGC self-play" ha un runner suo (non quello delle lotte di test): prepara le due squadre dai
//     dati degli allenatori, fa scegliere 4 su 6 e lead alla team preview dell'IA di ENTRAMBI i lati
//     (TeamPreview_AiSelectEnemyTeam, ognuno contro la squadra dell'altro), attiva IA contro IA
//     (gTestAiVsAiBattle) e avvia la lotta vera (CB2_InitBattle) con il seme scelto.
//   - Ogni lato pensa dal proprio punto di vista (src/battle_ai_perspective.c): conosce i propri set e
//     dell'altro solo ciò che ha visto. Anche il controllo dei misplay ragiona su ciò che il lato che
//     agisce poteva sapere (i suoi dati dell'IA), non sui set nascosti.
//   - I parametri (allenatori, seme, modalità, posizione preparata) stanno in gVgcSelfPlayArgs
//     (vgc_selfplay_args.c) e l'orchestratore li riscrive nel .gba: tools/vgc_training.
//   - Uscita: righe "VSP ..." (più le diagnostiche VGCPLAN / VGCTP dell'IA) lette da
//     tools/vgc_training/selfplay.py, che scrive i report in docs/ia_vgc/report/.
#include <stdarg.h>
#include "global.h"
#include "battle.h"
#include "battle_ai_main.h"
#include "battle_ai_perspective.h"
#include "battle_ai_scout.h"
#include "battle_ai_util.h"
#include "battle_ai_vgc.h"
#include "battle_ai_vgc_book.h"
#include "battle_ai_vgc_graph.h"
#include "battle_controllers.h"
#include "battle_gimmick.h"
#include "battle_main.h"
#include "battle_setup.h"
#include "battle_util.h"
#include "data.h"
#include "item.h"
#include "main.h"
#include "malloc.h"
#include "move.h"
#include "pokemon.h"
#include "random.h"
#include "team_preview.h"
#include "window.h"
#include "test/test.h"
#include "test/battle.h"
#include "test/vgc_selfplay.h"
#include "constants/abilities.h"
#include "constants/characters.h"
#include "constants/battle_ai.h"
#include "constants/battle_move_effects.h"
#include "constants/hold_effects.h"
#include "constants/moves.h"
#include "constants/trainers.h"

#define VSP_MAX_TRAINERS 32
#define VSP_DEFAULT_MAX_TURNS 50

enum
{
    VSP_CHOICE_NONE,
    VSP_CHOICE_MOVE,
    VSP_CHOICE_SWITCH,
};

struct VspChoice
{
    u8 kind;
    u8 moveIndex;
    u8 target;
    u8 partyIdx;        // CAMBIO: chi entra; MOSSA: chi era in campo quando ha scelto
    u8 gimmick;
    u16 move;
    u16 species;
};

// Ciò che il lato sapeva al momento della scelta, per i controlli che si chiudono dopo (KO subito).
struct VspDecisionInfo
{
    bool8 valid;
    bool8 isWinCon;
    bool8 koThreatKnown;    // un avversario poteva mandarlo KO con mosse note (secondo questo lato)
    bool8 couldProtect;
    bool8 couldSwitch;
    bool8 choseSafe;        // Protect o cambio
    u8 partyIdx;
};

struct VspPrev
{
    u16 turn;
    u8 partyIdx;
    u8 kind;
    u16 move;
    u8 fromIdx;             // CAMBIO: chi è uscito
};

struct VspState
{
    struct VgcSelfPlayArgs args;
    u16 trainers[VSP_MAX_TRAINERS];
    u8 trainerCount;
    u16 playerId, enemyId;
    bool8 battleStarted;
    bool8 ended;
    bool8 scenarioApplied;
    bool8 pending;          // scelte di questo turno registrate, controlli non ancora fatti
    bool8 evaluated;        // controlli del turno già fatti (il turno si sta svolgendo)
    u16 turn;               // turno corrente (gBattleTurnCounter all'inizio del turno)
    u32 progress;           // contatore di eventi, per il controllo dei blocchi
    u32 lastProgress;
    struct VspChoice choice[MAX_BATTLERS_COUNT];
    struct VspDecisionInfo info[MAX_BATTLERS_COUNT];
    struct VspPrev prev[MAX_BATTLERS_COUNT];
    struct VspPrev prevSwitch[MAX_BATTLERS_COUNT];
    struct VspPrev prevSwitch2[MAX_BATTLERS_COUNT];
    u16 misplays;
    u16 doubts;
    bool8 previewExplored[NUM_BATTLE_SIDES];
};

static EWRAM_DATA struct VspState sVsp = {0};

static void VgcSelfPlay_SetUp(void *data);
static void VgcSelfPlay_Run(void *data);
static void VgcSelfPlay_TearDown(void *data);
static bool32 VgcSelfPlay_CheckProgress(void *data);

static const struct TestRunner sVgcSelfPlayRunner =
{
    .setUp = VgcSelfPlay_SetUp,
    .run = VgcSelfPlay_Run,
    .tearDown = VgcSelfPlay_TearDown,
    .checkProgress = VgcSelfPlay_CheckProgress,
    // Niente randomUniform & co.: RNG vero, dal seme della partita.
};

__attribute__((section(".tests"), used)) static const struct Test sVgcSelfPlayTest =
{
    .name = "VGC self-play: partita IA contro IA",
    .filename = __FILE__,
    .runner = &sVgcSelfPlayRunner,
    .sourceLine = __LINE__,
    .data = NULL,
};

bool32 VgcSelfPlay_IsActive(void)
{
    return gTestRunnerState.test != NULL && gTestRunnerState.test->runner == &sVgcSelfPlayRunner;
}

// ---------------------------------------------------------------------
// Utilità di stampa
// ---------------------------------------------------------------------
static const u8 sTextNone[] = _("-");

static const u8 *MoveName(u32 move)
{
    if (move == MOVE_NONE || move >= MOVES_COUNT)
        return sTextNone;
    return GetMoveName(move);
}

static const u8 *ItemName(u32 item)
{
    if (item == ITEM_NONE)
        return sTextNone;
    return GetItemName(item);
}

static const u8 *AbilityName(u32 ability)
{
    if (ability == ABILITY_NONE || ability >= ABILITIES_COUNT)
        return sTextNone;
    return gAbilitiesInfo[ability].name;
}

static const char *SideName(u32 side)
{
    return side == B_SIDE_PLAYER ? "G" : "A"; // G = lato del giocatore, A = lato avversario
}

static void PrintPartyMon(u32 side, u32 slot, struct Pokemon *mon)
{
    Test_MgbaPrintf("VSP SET %s %d %S @ %S | %S | %S / %S / %S / %S | liv %d vel %d | id %d,%d,%d,%d",
        SideName(side), slot,
        GetSpeciesName(GetMonData(mon, MON_DATA_SPECIES)),
        ItemName(GetMonData(mon, MON_DATA_HELD_ITEM)),
        AbilityName(GetMonAbility(mon)),
        MoveName(GetMonData(mon, MON_DATA_MOVE1)), MoveName(GetMonData(mon, MON_DATA_MOVE2)),
        MoveName(GetMonData(mon, MON_DATA_MOVE3)), MoveName(GetMonData(mon, MON_DATA_MOVE4)),
        GetMonData(mon, MON_DATA_LEVEL), GetMonData(mon, MON_DATA_SPEED),
        GetMonData(mon, MON_DATA_MOVE1), GetMonData(mon, MON_DATA_MOVE2), GetMonData(mon, MON_DATA_MOVE3), GetMonData(mon, MON_DATA_MOVE4));
}

// Controllo della memoria (indagine sullo schermo blu): percorre i blocchi dell'heap e segnala il primo con
// l'intestazione rovinata, con il blocco prima (di solito è lui che ha scritto oltre la sua fine) e chi l'ha allocato.
static void VgcSelfPlay_CheckHeap(const char *when)
{
    static EWRAM_DATA bool8 sReported = FALSE;
    const struct MemBlock *head = HeapHead(), *b = head, *prev = NULL;
    u32 n = 0;

    if (sReported)
        return;
    do
    {
        if ((const u8 *)b < gHeap || (const u8 *)b >= gHeap + HEAP_SIZE || b->magic != MALLOC_SYSTEM_ID
         || (b->allocated && (const u8 *)b->data + b->size > gHeap + HEAP_SIZE))
        {
            const char *loc = (prev != NULL && prev->allocated) ? MemBlockLocation(prev) : NULL;
            sReported = TRUE;
            Test_MgbaPrintf("VSP HEAP corrotto (%s, turno %d): blocco %d a %p", when, sVsp.turn + 1, n, b);
            if (prev != NULL)
                Test_MgbaPrintf("VSP HEAP blocco prima: %p dimensione %d allocato=%d da %s", prev, prev->size, prev->allocated,
                                loc != NULL ? loc : "?");
            return;
        }
        prev = b;
        b = b->next;
        n++;
    } while (b != head && n < 2048);
}

static void PrintField(void)
{
    const char *weather = "nessuno";
    if (gBattleWeather & B_WEATHER_SUN) weather = "sole";
    else if (gBattleWeather & B_WEATHER_RAIN) weather = "pioggia";
    else if (gBattleWeather & B_WEATHER_SANDSTORM) weather = "sabbia";
    else if (gBattleWeather & (B_WEATHER_HAIL | B_WEATHER_SNOW)) weather = "neve";
    Test_MgbaPrintf("VSP CAMPO meteo=%s terreno=%d trickroom=%d tailwindG=%d tailwindA=%d schermiG=%d schermiA=%d",
        weather,
        gFieldTimers.terrain,
        (gFieldStatuses & STATUS_FIELD_TRICK_ROOM) ? gFieldTimers.trickRoomTimer : 0,
        (gSideStatuses[B_SIDE_PLAYER] & SIDE_STATUS_TAILWIND) ? gSideTimers[B_SIDE_PLAYER].tailwindTimer : 0,
        (gSideStatuses[B_SIDE_OPPONENT] & SIDE_STATUS_TAILWIND) ? gSideTimers[B_SIDE_OPPONENT].tailwindTimer : 0,
        gSideStatuses[B_SIDE_PLAYER] & (SIDE_STATUS_REFLECT | SIDE_STATUS_LIGHTSCREEN | SIDE_STATUS_AURORA_VEIL),
        gSideStatuses[B_SIDE_OPPONENT] & (SIDE_STATUS_REFLECT | SIDE_STATUS_LIGHTSCREEN | SIDE_STATUS_AURORA_VEIL));
}

static void PrintBattler(enum BattlerId b)
{
    if (!IsBattlerAlive(b))
    {
        Test_MgbaPrintf("VSP MON b%d vuoto", b);
        return;
    }
    if (gBattleMons[b].level != 50)
        Test_MgbaPrintf("VSP ERRORE livello b%d %d (atteso 50)", b, gBattleMons[b].level);
    Test_MgbaPrintf("VSP MON b%d %S liv%d slot%d ps=%d/%d stato=%d stadi=%d,%d,%d,%d,%d strum=%S abil=%S primoturno=%d",
        b, GetSpeciesName(gBattleMons[b].species), gBattleMons[b].level, gBattlerPartyIndexes[b], gBattleMons[b].hp, gBattleMons[b].maxHP,
        gBattleMons[b].status1,
        gBattleMons[b].statStages[STAT_ATK] - DEFAULT_STAT_STAGE, gBattleMons[b].statStages[STAT_DEF] - DEFAULT_STAT_STAGE,
        gBattleMons[b].statStages[STAT_SPEED] - DEFAULT_STAT_STAGE, gBattleMons[b].statStages[STAT_SPATK] - DEFAULT_STAT_STAGE,
        gBattleMons[b].statStages[STAT_SPDEF] - DEFAULT_STAT_STAGE,
        ItemName(gBattleMons[b].item), AbilityName(gBattleMons[b].ability), IsBattlersFirstTurn(b));
}

// Stringa del gioco -> ASCII (per i dettagli di ReportMisplay); buffer a rotazione.
static const char *A(const u8 *gameString)
{
    extern char mini_pchar_decode(u8);
    static char sBuffers[8][32];
    static u32 sNext = 0;
    char *out = sBuffers[sNext++ % ARRAY_COUNT(sBuffers)];
    u32 i = 0;
    while (gameString != NULL && gameString[i] != EOS && i < sizeof(sBuffers[0]) - 1)
    {
        out[i] = mini_pchar_decode(gameString[i]);
        i++;
    }
    out[i] = 0;
    return out;
}

// Formattazione minima (%s, %d) dei dettagli: niente vsnprintf della libc (vuole malloc/sbrk).
static void FormatDetail(char *out, u32 size, const char *fmt, va_list va)
{
    u32 n = 0;
    while (*fmt && n + 1 < size)
    {
        if (fmt[0] == '%' && fmt[1] == 's')
        {
            const char *s = va_arg(va, const char *);
            while (s != NULL && *s && n + 1 < size)
                out[n++] = *s++;
            fmt += 2;
        }
        else if (fmt[0] == '%' && fmt[1] == 'd')
        {
            s32 d = va_arg(va, s32);
            char tmp[12];
            u32 k = 0, u = d < 0 ? -d : d;
            if (d < 0 && n + 1 < size)
                out[n++] = '-';
            do { tmp[k++] = '0' + u % 10; u /= 10; } while (u > 0);
            while (k > 0 && n + 1 < size)
                out[n++] = tmp[--k];
            fmt += 2;
        }
        else
        {
            out[n++] = *fmt++;
        }
    }
    out[n] = 0;
}

// Misplay certo / caso dubbio: categoria, battler, dettaglio. Il turno è quello corrente.
static void ReportMisplay(bool32 certain, const char *category, enum BattlerId b, const char *fmt, ...)
{
    char buffer[160];
    va_list va;
    va_start(va, fmt);
    FormatDetail(buffer, sizeof(buffer), fmt, va);
    va_end(va);
    if (certain)
        sVsp.misplays++;
    else
        sVsp.doubts++;
    Test_MgbaPrintf("VSP %s %s t%d b%d %S: %s", certain ? "MISPLAY" : "DUBBIO", category, sVsp.turn + 1, b,
                    GetSpeciesName(gBattleMons[b].species), buffer);
}

// ---------------------------------------------------------------------
// Allenatori VGC presenti nel ROM dei test (generati da src/data/trainers.party)
// ---------------------------------------------------------------------
static void BuildTrainerList(void)
{
    u32 i;
    sVsp.trainerCount = 0;
    for (i = 0; i < TRAINERS_COUNT && sVsp.trainerCount < VSP_MAX_TRAINERS; i++)
    {
        const struct Trainer *t = &gTrainers[DIFFICULTY_NORMAL][i];
        if ((t->aiFlags & AI_FLAG_VGC_DOUBLES) && t->partySize > 0 && t->party != NULL)
            sVsp.trainers[sVsp.trainerCount++] = i;
    }
}

// ---------------------------------------------------------------------
// Team preview
// ---------------------------------------------------------------------
static bool32 OrderHas(const u8 *order, u32 n, u32 slot)
{
    u32 i;
    for (i = 0; i < n; i++)
        if (order[i] == slot)
            return TRUE;
    return FALSE;
}

static bool32 MonHasMoveEffect(struct Pokemon *mon, enum BattleMoveEffects effect)
{
    u32 i;
    for (i = 0; i < MAX_MON_MOVES; i++)
        if (GetMoveEffect(GetMonData(mon, MON_DATA_MOVE1 + i)) == effect)
            return TRUE;
    return FALSE;
}

static bool32 MonIsType(struct Pokemon *mon, enum Type type)
{
    u32 species = GetMonData(mon, MON_DATA_SPECIES);
    return GetSpeciesType(species, 0) == type || GetSpeciesType(species, 1) == type;
}

// Il compagno non si fa male con una mossa ad area di tipo `type` (immunità di tipo/abilità/strumento).
static bool32 PartnerImmuneToType(struct Pokemon *mon, enum Type type)
{
    enum Ability ab = GetMonAbility(mon);
    enum HoldEffect he = GetItemHoldEffect(GetMonData(mon, MON_DATA_HELD_ITEM));
    switch (type)
    {
    case TYPE_GROUND:
        return MonIsType(mon, TYPE_FLYING) || ab == ABILITY_LEVITATE || ab == ABILITY_EARTH_EATER || he == HOLD_EFFECT_AIR_BALLOON;
    case TYPE_ELECTRIC:
        return MonIsType(mon, TYPE_GROUND) || ab == ABILITY_VOLT_ABSORB || ab == ABILITY_LIGHTNING_ROD || ab == ABILITY_MOTOR_DRIVE;
    case TYPE_WATER:
        return ab == ABILITY_WATER_ABSORB || ab == ABILITY_STORM_DRAIN || ab == ABILITY_DRY_SKIN;
    case TYPE_FIRE:
        return ab == ABILITY_FLASH_FIRE || ab == ABILITY_WELL_BAKED_BODY;
    default:
        return ab == ABILITY_TELEPATHY;
    }
}

// Controlli oggettivi sulla scelta della team preview di un lato (squadra già ridotta e in ordine).
static void CheckTeamPreview(u32 side, struct Pokemon *full, const u8 *order, u32 picks)
{
    struct VgcTeamGraph graph;
    u32 k, j;

    AI_VgcBuildTeamGraph(full, &graph);
    for (k = 0; k < picks; k++)
    {
        u32 slot = order[k];
        struct Pokemon *mon = &full[slot];
        enum Ability ab = GetMonAbility(mon);

        // Baton Passer senza il suo ricevente.
        if ((graph.roles[slot] & VGC_ROLE_BATON_PASSER) && graph.batonRecipient[slot] != VGC_GRAPH_NONE
         && !OrderHas(order, picks, graph.batonRecipient[slot]))
            Test_MgbaPrintf("VSP TPMISPLAY BATON_SENZA_RICEVENTE %s %S: ricevente %S lasciato fuori", SideName(side),
                            GetSpeciesName(GetMonData(mon, MON_DATA_SPECIES)),
                            GetSpeciesName(GetMonData(&full[graph.batonRecipient[slot]], MON_DATA_SPECIES)));
        // Abilità penalizzante senza chi la spegne.
        if (ab == ABILITY_TRUANT || ab == ABILITY_SLOW_START || ab == ABILITY_DEFEATIST)
        {
            bool32 fixed = FALSE;
            for (j = 0; j < picks; j++)
                if (order[j] != slot && graph.abilityFixPartner[order[j]] == slot)
                    fixed = TRUE;
            if (!fixed)
                Test_MgbaPrintf("VSP TPMISPLAY ABILITA_SENZA_FIX %s %S: %S e nessuno che la tolga", SideName(side),
                                GetSpeciesName(GetMonData(mon, MON_DATA_SPECIES)), AbilityName(ab));
        }
    }
    // Lead che si danneggia da sola.
    if (picks >= 2)
    {
        for (k = 0; k < 2; k++)
        {
            struct Pokemon *me = &full[order[k]], *other = &full[order[1 - k]];
            enum Ability ab = GetMonAbility(me);
            enum Type hurt = TYPE_NONE;
            u32 i, dmgMoves = 0, hurtMoves = 0;

            if (ab == ABILITY_DROUGHT || ab == ABILITY_DESOLATE_LAND || ab == ABILITY_ORICHALCUM_PULSE)
                hurt = TYPE_WATER;
            else if (ab == ABILITY_DRIZZLE || ab == ABILITY_PRIMORDIAL_SEA)
                hurt = TYPE_FIRE;
            if (hurt != TYPE_NONE)
            {
                for (i = 0; i < MAX_MON_MOVES; i++)
                {
                    enum Move m = GetMonData(other, MON_DATA_MOVE1 + i);
                    if (m == MOVE_NONE || IsBattleMoveStatus(m))
                        continue;
                    dmgMoves++;
                    if (GetMoveType(m) == hurt)
                        hurtMoves++;
                }
                if (dmgMoves > 0 && hurtMoves * 2 >= dmgMoves)
                    Test_MgbaPrintf("VSP TPMISPLAY LEAD_AUTOLESIONISTA %s %S + %S: il meteo di %S indebolisce gli attacchi del compagno",
                                    SideName(side), GetSpeciesName(GetMonData(me, MON_DATA_SPECIES)),
                                    GetSpeciesName(GetMonData(other, MON_DATA_SPECIES)), AbilityName(ab));
            }
            for (i = 0; i < MAX_MON_MOVES; i++)
            {
                enum Move m = GetMonData(me, MON_DATA_MOVE1 + i);
                if (m == MOVE_NONE || IsBattleMoveStatus(m) || GetMoveTarget(m) != TARGET_FOES_AND_ALLY)
                    continue;
                if (PartnerImmuneToType(other, GetMoveType(m)))
                    continue;
                Test_MgbaPrintf("VSP TPDUBBIO LEAD_AUTOLESIONISTA %s %S + %S: %S colpisce anche il compagno",
                                SideName(side), GetSpeciesName(GetMonData(me, MON_DATA_SPECIES)),
                                GetSpeciesName(GetMonData(other, MON_DATA_SPECIES)), MoveName(m));
                break;
            }
        }
    }
}

static void ApplyOrder(struct Pokemon *party, const u8 *order, u32 picks)
{
    struct Pokemon *tmp = Alloc(sizeof(struct Pokemon) * PARTY_SIZE);
    u32 k;
    memcpy(tmp, party, sizeof(struct Pokemon) * PARTY_SIZE);
    for (k = 0; k < PARTY_SIZE; k++)
    {
        if (k < picks && order[k] < PARTY_SIZE)
            party[k] = tmp[order[k]];
        else
            ZeroMonData(&party[k]);
    }
    Free(tmp);
}

// Scelta della team preview per i due lati. Il lato del giocatore si fa scegliere scambiando le squadre:
// TeamPreview_AiSelectEnemyTeam ragiona sempre per gEnemyParty contro gPlayerParty (mosse dell'altro non viste).
static void SelectTeams(u8 *orderP, u8 *orderE, u32 *picksP, u32 *picksE)
{
    struct Pokemon *backup = Alloc(sizeof(struct Pokemon) * PARTY_SIZE * 2);
    u16 megaP;
    u32 k;

    memcpy(backup, gParties[B_TRAINER_PLAYER], sizeof(struct Pokemon) * PARTY_SIZE);
    memcpy(backup + PARTY_SIZE, gParties[B_TRAINER_OPPONENT_A], sizeof(struct Pokemon) * PARTY_SIZE);

    // Lato del giocatore: la sua squadra al posto di quella avversaria.
    memcpy(gParties[B_TRAINER_OPPONENT_A], backup, sizeof(struct Pokemon) * PARTY_SIZE);
    memcpy(gParties[B_TRAINER_PLAYER], backup + PARTY_SIZE, sizeof(struct Pokemon) * PARTY_SIZE);
    TRAINER_BATTLE_PARAM.opponentA = sVsp.playerId;
    Test_MgbaPrintf("VSP TP G");
    for (k = 0; k < 4; k++) orderP[k] = 0xFF;
    gVgcTraining.previewSide = B_SIDE_PLAYER;
    gScoutContextPreviewOff = (gScoutContextOffMask & (1u << B_SIDE_PLAYER)) != 0;
    TeamPreview_AiSelectEnemyTeam(4, TRUE, orderP);
    sVsp.previewExplored[B_SIDE_PLAYER] = gVgcTraining.previewExplore;
    megaP = TeamPreview_GetMegaDesignateSpecies();

    // Lato avversario: squadre al loro posto.
    memcpy(gParties[B_TRAINER_PLAYER], backup, sizeof(struct Pokemon) * PARTY_SIZE);
    memcpy(gParties[B_TRAINER_OPPONENT_A], backup + PARTY_SIZE, sizeof(struct Pokemon) * PARTY_SIZE);
    TRAINER_BATTLE_PARAM.opponentA = sVsp.enemyId;
    Test_MgbaPrintf("VSP TP A");
    for (k = 0; k < 4; k++) orderE[k] = 0xFF;
    gVgcTraining.previewSide = B_SIDE_OPPONENT;
    gScoutContextPreviewOff = (gScoutContextOffMask & (1u << B_SIDE_OPPONENT)) != 0;
    TeamPreview_AiSelectEnemyTeam(4, TRUE, orderE);
    gScoutContextPreviewOff = FALSE;
    sVsp.previewExplored[B_SIDE_OPPONENT] = gVgcTraining.previewExplore;
    gVgcTraining.previewExplore = FALSE;
    AI_SetPlayerSideMegaDesignate(megaP);
    Free(backup);

    *picksP = *picksE = 0;
    for (k = 0; k < 4; k++)
    {
        if (orderP[k] < PARTY_SIZE) (*picksP)++;
        if (orderE[k] < PARTY_SIZE) (*picksE)++;
    }
    Test_MgbaPrintf("VSP MEGA G %S A %S", GetSpeciesName(megaP), GetSpeciesName(TeamPreview_GetMegaDesignateSpecies()));
}

// Chiave del libro dei piani (M9) per il lato `side`: hash della propria squadra, classe di quella avversaria (solo
// specie, visibili in team preview), specie per slot, piano scelto dal libro e se la team preview ha esplorato.
static void PrintBookKey(u32 side, struct Pokemon *ours, struct Pokemon *theirs)
{
    u16 so[PARTY_SIZE], st[PARTY_SIZE];
    u32 i, no = 0, nt = 0;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (GetMonData(&ours[i], MON_DATA_SPECIES) != SPECIES_NONE)
            so[no++] = GetMonData(&ours[i], MON_DATA_SPECIES);
        if (GetMonData(&theirs[i], MON_DATA_SPECIES) != SPECIES_NONE)
            st[nt++] = GetMonData(&theirs[i], MON_DATA_SPECIES);
    }
    Test_MgbaPrintf("VSP CHIAVE %s squadra=%d classe=%d piano=%d esplora=%d specie=%d,%d,%d,%d,%d,%d", SideName(side),
                    VgcBook_TeamHash(so, no), VgcBook_OppClass(st, nt), (s32)(s16)AI_VgcBookPick(side), sVsp.previewExplored[side],
                    no > 0 ? so[0] : 0, no > 1 ? so[1] : 0, no > 2 ? so[2] : 0, no > 3 ? so[3] : 0, no > 4 ? so[4] : 0, no > 5 ? so[5] : 0);
}

static void PrintOrder(u32 side, struct Pokemon *full, const u8 *order, u32 picks)
{
    Test_MgbaPrintf("VSP PORTA %s %d %d %d %d | lead %S + %S | dietro %S, %S", SideName(side),
        order[0], order[1], order[2], order[3],
        GetSpeciesName(picks > 0 ? GetMonData(&full[order[0]], MON_DATA_SPECIES) : SPECIES_NONE),
        GetSpeciesName(picks > 1 ? GetMonData(&full[order[1]], MON_DATA_SPECIES) : SPECIES_NONE),
        GetSpeciesName(picks > 2 ? GetMonData(&full[order[2]], MON_DATA_SPECIES) : SPECIES_NONE),
        GetSpeciesName(picks > 3 ? GetMonData(&full[order[3]], MON_DATA_SPECIES) : SPECIES_NONE));
}

// ---------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------
static void VgcSelfPlay_SetUp(void *data)
{
    (void)data;
    // Gli agganci del runner delle lotte leggono questo stato: tutto a zero = nessuna attesa.
    memset(gBattleTestRunnerState, 0, sizeof(*gBattleTestRunnerState));
    memset(&sVsp, 0, sizeof(sVsp));
}

static void VgcSelfPlay_Run(void *data)
{
    u32 i, picksP, picksE;
    u8 orderP[4], orderE[4];
    struct Pokemon *full;

    (void)data;
    memcpy(&sVsp.args, &gVgcSelfPlayArgs, sizeof(sVsp.args));
    BuildTrainerList();
    for (i = 0; i < sVsp.trainerCount; i++)
        Test_MgbaPrintf("VSP ALLENATORE %d %d %S", i, sVsp.trainers[i], GetTrainerNameFromId(sVsp.trainers[i]));
    if (sVsp.args.mode == VSP_MODE_LIST)
        return;
    if (sVsp.args.playerIdx >= sVsp.trainerCount || sVsp.args.enemyIdx >= sVsp.trainerCount)
    {
        Test_MgbaPrintf("VSP ERRORE indici allenatori fuori elenco (%d, %d su %d)", sVsp.args.playerIdx, sVsp.args.enemyIdx, sVsp.trainerCount);
        return;
    }
    if (sVsp.args.maxTurns == 0)
        sVsp.args.maxTurns = VSP_DEFAULT_MAX_TURNS;
    sVsp.playerId = sVsp.trainers[sVsp.args.playerIdx];
    sVsp.enemyId = sVsp.trainers[sVsp.args.enemyIdx];

    SeedRng(sVsp.args.seed);
    SeedRng2(sVsp.args.seed);
    gBattleTypeFlags = BATTLE_TYPE_IS_MASTER | BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE;
    TRAINER_BATTLE_PARAM.opponentA = sVsp.enemyId;
    TRAINER_BATTLE_PARAM.opponentB = TRAINER_NONE;
    gPartnerTrainerId = sVsp.playerId;
    gTestAiVsAiBattle = TRUE;
    // Allenamento (M9): libro dei piani per lato, esplorazione, log silenzioso.
    memset(&gVgcTraining, 0, sizeof(gVgcTraining));
    gVgcTraining.bookOff = sVsp.args.bookOff;
    gVgcTraining.exploreEps[B_SIDE_PLAYER] = sVsp.args.explorePct[0];
    gVgcTraining.exploreEps[B_SIDE_OPPONENT] = sVsp.args.explorePct[1];
    gTestMgbaQuiet = sVsp.args.quiet != 0;
    gScoutContextOffMask = sVsp.args.contextOff & 3; // verifica del contesto della squadra: lato spento
    AI_VgcBookReset();

    Test_MgbaPrintf("VSP INIZIO giocatore=%d:%S avversario=%d:%S seme=%d modo=%d maxturni=%d",
        sVsp.args.playerIdx, GetTrainerNameFromId(sVsp.playerId), sVsp.args.enemyIdx, GetTrainerNameFromId(sVsp.enemyId),
        sVsp.args.seed, sVsp.args.mode, sVsp.args.maxTurns);

    CreateNPCTrainerPartyFromTrainer(gParties[B_TRAINER_PLAYER], GetTrainerStructFromId(sVsp.playerId));
    CreateNPCTrainerPartyFromTrainer(gParties[B_TRAINER_OPPONENT_A], GetTrainerStructFromId(sVsp.enemyId));
    // Tutte le lotte sono a Lv50, come nel gioco: si scala PRIMA della team preview (come CB2_TeamPreview),
    // così la scelta usa le statistiche giuste; CB2_InitBattle lo rifà comunque.
    ScalePartiesToBattleLevel();
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_SPECIES) != SPECIES_NONE)
            PrintPartyMon(B_SIDE_PLAYER, i, &gParties[B_TRAINER_PLAYER][i]);
    }
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (GetMonData(&gParties[B_TRAINER_OPPONENT_A][i], MON_DATA_SPECIES) != SPECIES_NONE)
            PrintPartyMon(B_SIDE_OPPONENT, i, &gParties[B_TRAINER_OPPONENT_A][i]);
    }

    if (sVsp.args.mode == VSP_MODE_FIXED)
    {
        picksP = picksE = 0;
        for (i = 0; i < 4; i++)
        {
            orderP[i] = sVsp.args.playerOrder[i];
            orderE[i] = sVsp.args.enemyOrder[i];
            if (orderP[i] < PARTY_SIZE) picksP++;
            if (orderE[i] < PARTY_SIZE) picksE++;
        }
        AI_SetPlayerSideMegaDesignate(SPECIES_NONE);
    }
    else
    {
        SelectTeams(orderP, orderE, &picksP, &picksE);
    }

    PrintBookKey(B_SIDE_PLAYER, gParties[B_TRAINER_PLAYER], gParties[B_TRAINER_OPPONENT_A]);
    PrintBookKey(B_SIDE_OPPONENT, gParties[B_TRAINER_OPPONENT_A], gParties[B_TRAINER_PLAYER]);
    full = Alloc(sizeof(struct Pokemon) * PARTY_SIZE);
    memcpy(full, gParties[B_TRAINER_PLAYER], sizeof(struct Pokemon) * PARTY_SIZE);
    PrintOrder(B_SIDE_PLAYER, full, orderP, picksP);
    CheckTeamPreview(B_SIDE_PLAYER, full, orderP, picksP);
    memcpy(full, gParties[B_TRAINER_OPPONENT_A], sizeof(struct Pokemon) * PARTY_SIZE);
    PrintOrder(B_SIDE_OPPONENT, full, orderE, picksE);
    CheckTeamPreview(B_SIDE_OPPONENT, full, orderE, picksE);
    Free(full);

    ApplyOrder(gParties[B_TRAINER_PLAYER], orderP, picksP);
    ApplyOrder(gParties[B_TRAINER_OPPONENT_A], orderE, picksE);
    CalculatePlayerPartyCount();
    CalculateEnemyPartyCount();
    // Parametri della lotta allenatore azzerati: testi di sconfitta e script rimasti da prima puntavano a memoria a
    // caso (alla vittoria del lato G il testo di sconfitta dell'allenatore mandava il codice fuori strada).
    memset(&TRAINER_BATTLE_PARAM, 0, sizeof(TRAINER_BATTLE_PARAM));
    TRAINER_BATTLE_PARAM.opponentA = sVsp.enemyId;
    TRAINER_BATTLE_PARAM.opponentB = TRAINER_NONE;
    AI_VgcResetMemory();
    // Taratura (M9): pesi del lato weightSide dai parametri (reserved[0..6], % / 2; 0 = default), l'altro ai default.
    AI_VgcResetEvalWeights();
    AI_VgcSetEvalWeightPct(sVsp.args.weightSide ? B_SIDE_OPPONENT : B_SIDE_PLAYER, sVsp.args.reserved);
    Test_MgbaPrintf("VSP PESI lato=%s libro_spento=%d esplora=%d,%d", sVsp.args.weightSide ? "A" : "G", sVsp.args.bookOff,
                    sVsp.args.explorePct[0], sVsp.args.explorePct[1]);

    sVsp.battleStarted = TRUE;
    gMain.savedCallback = CB2_TestRunner;
    SetMainCallback2(CB2_InitBattle);
}

static void VgcSelfPlay_TearDown(void *data)
{
    (void)data;
    gTestAiVsAiBattle = FALSE;
    AI_SetPlayerSideMegaDesignate(SPECIES_NONE);
    if (!sVsp.battleStarted)
        return;
    // Come TearDownBattle del runner delle lotte: libera ciò che resta (anche dopo un'uscita forzata).
    for (enum BattleTrainer trainer = B_TRAINER_PLAYER; trainer < MAX_BATTLE_TRAINERS; trainer++)
    {
        ZeroPartyMons(gParties[trainer]);
        gPartiesCount[trainer] = 0;
    }
    for (u32 i = 0; i < MAX_BATTLERS_COUNT; i++)
    {
        gBattlerBattleController[i] = BATTLE_CONTROLLER_NONE;
        gBattlerPartyIndexes[i] = 0;
    }
    // Niente Free: ogni partita gira in un processo mGBA a sé e finisce qui. Liberare le risorse della lotta (già in
    // parte liberate dal motore o da un'uscita a metà) faceva scattare l'assert di malloc.c e poi codice a caso.
    gMain.inBattle = FALSE;
    sVsp.battleStarted = FALSE;
}

static bool32 VgcSelfPlay_CheckProgress(void *data)
{
    bool32 progress = sVsp.progress != sVsp.lastProgress;
    (void)data;
    // Battito (ogni ~60 s emulati senza avanzamento): dove si trova la lotta, per diagnosticare i blocchi.
    if (sVsp.battleStarted)
        Test_MgbaPrintf("VSP BATTITO avanzamento=%d main=%p cb2=%p ctrl=%p,%p,%p,%p exec=%d com=%d,%d,%d,%d cmd=%d,%d,%d,%d bs=%p",
                        progress, gBattleMainFunc, gMain.callback2, gBattlerControllerFuncs[0], gBattlerControllerFuncs[1],
                        gBattlerControllerFuncs[2], gBattlerControllerFuncs[3], gBattleControllerExecFlags,
                        gBattleCommunication[0], gBattleCommunication[1], gBattleCommunication[2], gBattleCommunication[3],
                        gBattleResources->bufferA[0][0], gBattleResources->bufferA[1][0], gBattleResources->bufferA[2][0],
                        gBattleResources->bufferA[3][0], gBattlescriptCurrInstr);
    sVsp.lastProgress = sVsp.progress;
    return progress;
}

// ---------------------------------------------------------------------
// Inizio turno: intestazione, posizione preparata, limite di turni
// ---------------------------------------------------------------------
static void ApplyScenario(void)
{
    const struct VgcSelfPlayArgs *a = &sVsp.args;
    u32 side, stat;
    enum BattlerId b;
    static const u8 sStats[5] = { STAT_ATK, STAT_DEF, STAT_SPEED, STAT_SPATK, STAT_SPDEF };
    bool32 any = FALSE;

    if (a->startTurn)
    {
        gBattleTurnCounter = a->startTurn;
        any = TRUE;
    }
    for (side = 0; side < NUM_BATTLE_SIDES; side++)
    {
        if (a->tailwindTurns[side])
        {
            gSideStatuses[side] |= SIDE_STATUS_TAILWIND;
            gSideTimers[side].tailwindTimer = a->tailwindTurns[side];
            any = TRUE;
        }
    }
    if (a->trickRoomTurns)
    {
        gFieldStatuses |= STATUS_FIELD_TRICK_ROOM;
        gFieldTimers.trickRoomTimer = a->trickRoomTurns;
        any = TRUE;
    }
    for (b = 0; b < gBattlersCount; b++)
    {
        if (!IsBattlerAlive(b))
            continue;
        if (a->hpPercent[b] > 0 && a->hpPercent[b] < 100)
        {
            u32 hp = max(1, gBattleMons[b].maxHP * a->hpPercent[b] / 100);
            gBattleMons[b].hp = hp;
            SetMonData(GetBattlerMon(b), MON_DATA_HP, &hp);
            any = TRUE;
        }
        for (stat = 0; stat < 5; stat++)
        {
            if (a->statStages[b][stat] != 0)
            {
                gBattleMons[b].statStages[sStats[stat]] = DEFAULT_STAT_STAGE + max(-6, min(6, a->statStages[b][stat]));
                any = TRUE;
            }
        }
    }
    if (any)
        Test_MgbaPrintf("VSP SCENARIO applicato");
}

static void EvaluateTurn(void);

void VgcSelfPlay_OnTurnStart(void)
{
    enum BattlerId b;

    sVsp.progress++;
    if (sVsp.pending)
        EvaluateTurn();
    if (!sVsp.scenarioApplied)
    {
        sVsp.scenarioApplied = TRUE;
        ApplyScenario();
    }
    sVsp.turn = gBattleTurnCounter;
    sVsp.evaluated = FALSE;
    memset(sVsp.choice, 0, sizeof(sVsp.choice));
    memset(sVsp.info, 0, sizeof(sVsp.info));
    if (sVsp.turn >= sVsp.args.maxTurns)
    {
        u32 left[NUM_BATTLE_SIDES] = {0};
        for (b = 0; b < gBattlersCount; b++)
        {
            struct Pokemon *party = GetBattlerParty(b);
            u32 i;
            if (b >= 2)
                continue;
            for (i = 0; i < PARTY_SIZE; i++)
                if (GetMonData(&party[i], MON_DATA_SPECIES) != SPECIES_NONE && GetMonData(&party[i], MON_DATA_HP) > 0)
                    left[GetBattlerSide(b)]++;
        }
        sVsp.ended = TRUE;
        Test_MgbaPrintf("VSP FINE esito=LIMITE_TURNI turni=%d restantiG=%d restantiA=%d misplay=%d dubbi=%d",
                        sVsp.turn, left[B_SIDE_PLAYER], left[B_SIDE_OPPONENT], sVsp.misplays, sVsp.doubts);
        Test_ExitWithResult(TEST_RESULT_TODO, SourceLine(0), "VSP limite di turni");
    }
    Test_MgbaPrintf("VSP TURNO %d", sVsp.turn + 1);
    VgcSelfPlay_CheckHeap("inizio turno");
    PrintField();
    for (b = 0; b < gBattlersCount; b++)
        PrintBattler(b);
}

// ---------------------------------------------------------------------
// Scelte
// ---------------------------------------------------------------------
void VgcSelfPlay_OnChosenMove(enum BattlerId battler, enum Move move, u32 target, u32 gimmick)
{
    struct VspChoice *c = &sVsp.choice[battler];
    sVsp.progress++;
    c->kind = VSP_CHOICE_MOVE;
    c->move = move;
    c->moveIndex = gBattleStruct->chosenMovePositions[battler];
    c->target = target;
    c->gimmick = gimmick;
    c->partyIdx = gBattlerPartyIndexes[battler];
    c->species = gBattleMons[battler].species;
    sVsp.pending = TRUE;
}

void VgcSelfPlay_OnSwitch(enum BattlerId battler, u32 partyIndex)
{
    struct Pokemon *party = GetBattlerParty(battler);
    sVsp.progress++;
    if (partyIndex >= PARTY_SIZE)
    {
        Test_MgbaPrintf("VSP CAMBIO_NON_VALIDO b%d %d", battler, partyIndex);
        return;
    }
    if (gBattleMons[battler].hp == 0)
    {
        Test_MgbaPrintf("VSP RIMPIAZZO b%d -> %S (slot%d)", battler, GetSpeciesName(GetMonData(&party[partyIndex], MON_DATA_SPECIES)), partyIndex);
        return;
    }
    if (!sVsp.evaluated)
    {
        // Durante la scelta delle azioni: cambio volontario.
        sVsp.choice[battler].kind = VSP_CHOICE_SWITCH;
        sVsp.choice[battler].partyIdx = partyIndex;
        sVsp.choice[battler].species = gBattleMons[battler].species;
        sVsp.pending = TRUE;
        return;
    }
    // A turno in corso (U-turn, Baton Pass, Eject Button...): uscita, non una scelta del turno.
    Test_MgbaPrintf("VSP USCITA b%d %S -> %S (slot%d)", battler, GetSpeciesName(gBattleMons[battler].species),
                    GetSpeciesName(GetMonData(&party[partyIndex], MON_DATA_SPECIES)), partyIndex);
}

// ---------------------------------------------------------------------
// Controllo dei misplay (a scelte fatte, prima che il turno si svolga)
// ---------------------------------------------------------------------
static bool32 IsProtectMove(u32 move)
{
    return GetMoveEffect(move) == EFFECT_PROTECT
        && move != MOVE_WIDE_GUARD && move != MOVE_QUICK_GUARD && move != MOVE_CRAFTY_SHIELD && move != MOVE_MAT_BLOCK;
}

static bool32 KnowsProtect(enum BattlerId b)
{
    // Con la prospettiva di chi guarda: per un avversario solo le mosse viste.
    enum Move *moves = GetMovesArray(b);
    u32 i;
    for (i = 0; i < MAX_MON_MOVES; i++)
        if (moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE && IsProtectMove(moves[i]))
            return TRUE;
    return FALSE;
}

static bool32 ChoiceIsProtect(enum BattlerId b)
{
    return sVsp.choice[b].kind == VSP_CHOICE_MOVE && IsProtectMove(sVsp.choice[b].move);
}

static bool32 ChoiceIsSafe(enum BattlerId b)
{
    return ChoiceIsProtect(b) || sVsp.choice[b].kind == VSP_CHOICE_SWITCH;
}

// Danno minimo/massimo stimato da chi pensa (prospettiva impostata) di `atk` su `def` con la mossa idx.
static u32 DmgMin(enum BattlerId atk, enum BattlerId def, u32 idx)
{
    return gAiLogicData->simulatedDmg[atk][def][idx].minimum;
}

static u32 DmgMax(enum BattlerId atk, enum BattlerId def, u32 idx)
{
    return gAiLogicData->simulatedDmg[atk][def][idx].maximum;
}

static bool32 MoveUsable(enum BattlerId b, u32 idx)
{
    enum Move m = gBattleMons[b].moves[idx];
    return m != MOVE_NONE && m != MOVE_UNAVAILABLE && gBattleMons[b].pp[idx] > 0
        && !(gAiLogicData->moveLimitations[b] & (1u << idx));
}

// La mossa scelta da `b` manda KO di sicuro `def` (danno minimo stimato, precisione piena).
static bool32 ChoiceKOs(enum BattlerId b, enum BattlerId def)
{
    struct VspChoice *c = &sVsp.choice[b];
    enum MoveTarget tt;
    if (c->kind != VSP_CHOICE_MOVE || !IsBattlerAlive(def) || IsBattleMoveStatus(c->move) || c->moveIndex >= MAX_MON_MOVES)
        return FALSE;
    tt = GetBattlerMoveTargetType(b, c->move);
    if (c->target != def && tt != TARGET_BOTH && tt != TARGET_FOES_AND_ALLY)
        return FALSE;
    return DmgMin(b, def, c->moveIndex) >= gBattleMons[def].hp && gAiLogicData->moveAccuracy[b][def][c->moveIndex] >= 100;
}

// Il compagno trae vantaggio dall'essere colpito (combo): abilità che assorbe o si potenzia, Weakness Policy...
static bool32 PartnerHitIsCombo(enum BattlerId b, enum BattlerId p, u32 move, u32 idx)
{
    enum Ability ab = gBattleMons[p].ability;
    enum Type type = GetMoveType(move);
    enum HoldEffect he = GetBattlerHoldEffect(p);

    if (GetMoveEffect(move) == EFFECT_BEAT_UP)
        return TRUE;
    if (ab == ABILITY_TELEPATHY || ab == ABILITY_JUSTIFIED || ab == ABILITY_STAMINA || ab == ABILITY_WEAK_ARMOR
     || ab == ABILITY_ANGER_POINT || ab == ABILITY_BERSERK || ab == ABILITY_RATTLED)
        return TRUE;
    if (PartnerImmuneToType(GetBattlerMon(p), type))
        return TRUE;
    if ((ab == ABILITY_SAP_SIPPER && type == TYPE_GRASS) || (ab == ABILITY_STEAM_ENGINE && (type == TYPE_FIRE || type == TYPE_WATER))
     || (ab == ABILITY_WATER_COMPACTION && type == TYPE_WATER) || (ab == ABILITY_THERMAL_EXCHANGE && type == TYPE_FIRE))
        return TRUE;
    if (he == HOLD_EFFECT_WEAKNESS_POLICY && gAiLogicData->effectiveness[b][p][idx] >= UQ_4_12(2.0))
        return TRUE;
    if (gAiLogicData->effectiveness[b][p][idx] == UQ_4_12(0.0))
        return TRUE;
    return FALSE;
}

// Mosse di stato che falliscono per un'immunita' nota (tipo, abilità/strumento visti, Prankster sul Buio).
static bool32 StatusMoveBlocked(enum BattlerId b, enum BattlerId t, u32 move)
{
    enum BattleMoveEffects eff = GetMoveEffect(move);
    enum Ability tAb = gAiLogicData->abilities[t];
    enum HoldEffect tHe = gAiLogicData->holdEffects[t];

    if (IsPowderMove(move) && !IsAffectedByPowderMove(t, tAb, tHe))
        return TRUE;
    if (gAiLogicData->abilities[b] == ABILITY_PRANKSTER && IS_BATTLER_OF_TYPE(t, TYPE_DARK) && GetMoveTarget(move) == TARGET_SELECTED)
        return TRUE;
    if (move == MOVE_THUNDER_WAVE && (IS_BATTLER_OF_TYPE(t, TYPE_GROUND) || IS_BATTLER_OF_TYPE(t, TYPE_ELECTRIC)))
        return TRUE;
    if (eff == EFFECT_NON_VOLATILE_STATUS && GetMoveNonVolatileStatus(move) == MOVE_EFFECT_BURN && IS_BATTLER_OF_TYPE(t, TYPE_FIRE))
        return TRUE;
    if (eff == EFFECT_NON_VOLATILE_STATUS && (GetMoveNonVolatileStatus(move) == MOVE_EFFECT_TOXIC || GetMoveNonVolatileStatus(move) == MOVE_EFFECT_POISON)
     && (IS_BATTLER_OF_TYPE(t, TYPE_POISON) || IS_BATTLER_OF_TYPE(t, TYPE_STEEL)) && gAiLogicData->abilities[b] != ABILITY_CORROSION)
        return TRUE;
    return FALSE;
}

static enum BattlerId OtherFoe(enum BattlerId b, enum BattlerId foe)
{
    enum BattlerId l = GetBattlerLeftFoe(b), r = GetBattlerRightFoe(b);
    return foe == l ? r : l;
}

static void CheckBattlerChoice(enum BattlerId b)
{
    struct VspChoice *c = &sVsp.choice[b];
    struct VspDecisionInfo *info = &sVsp.info[b];
    struct VspPrev *prev = &sVsp.prev[b];
    enum BattlerId p = GetPartnerBattler(b), foes[2] = { GetBattlerLeftFoe(b), GetBattlerRightFoe(b) };
    bool32 pAlive = IsDoubleBattle() && IsBattlerAlive(p);
    u32 move = c->move, idx = c->moveIndex, i, k;
    enum BattleMoveEffects eff = GetMoveEffect(move);
    bool32 isMove = c->kind == VSP_CHOICE_MOVE && idx < MAX_MON_MOVES;
    bool32 damaging = isMove && !IsBattleMoveStatus(move);
    enum MoveTarget tt = isMove ? GetBattlerMoveTargetType(b, move) : TARGET_SELECTED;
    bool32 spread = tt == TARGET_BOTH || tt == TARGET_FOES_AND_ALLY;
    enum BattlerId t = c->target;
    bool32 threatMin = FALSE, threatMax = FALSE;
    enum BattlerId threatFoe = foes[0];

    // Tutto da qui in poi con gli occhi di chi ha scelto.
    AI_SetPerspective(b);

    // Minaccia di KO su di noi (mosse viste degli avversari, abilità/strumenti presunti).
    for (k = 0; k < 2; k++)
    {
        enum BattlerId f = foes[k];
        if (!IsBattlerAlive(f))
            continue;
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            if (gBattleMons[f].moves[i] == MOVE_NONE || IsBattleMoveStatus(gBattleMons[f].moves[i]))
                continue;
            // KO prevedibile: danno minimo che basta e mossa che non può mancare (con il 90% il KO è un rischio,
            // non una certezza: va tra i dubbi).
            if (DmgMin(f, b, i) >= gBattleMons[b].hp && gAiLogicData->moveAccuracy[f][b][i] >= 100)
            {
                threatMin = TRUE;
                threatFoe = f;
            }
            if (DmgMax(f, b, i) >= gBattleMons[b].hp)
                threatMax = TRUE;
        }
    }
    // Focus Sash a PS pieni: non è un KO.
    if (gBattleMons[b].hp == gBattleMons[b].maxHP && gAiLogicData->holdEffects[b] == HOLD_EFFECT_FOCUS_SASH)
        threatMin = threatMax = FALSE;

    info->valid = TRUE;
    info->partyIdx = gBattlerPartyIndexes[b];
    info->isWinCon = AI_VgcPlanIsWinCon(AI_TrainerIdOfBattler(b), gBattleMons[b].species);
    info->koThreatKnown = threatMin;
    info->choseSafe = ChoiceIsSafe(b);
    info->couldSwitch = CountUsablePartyMons(b) > 0 && AI_CanBattlerEscape(b);
    info->couldProtect = FALSE;
    for (i = 0; i < MAX_MON_MOVES; i++)
        if (MoveUsable(b, i) && IsProtectMove(gBattleMons[b].moves[i]))
            info->couldProtect = !(prev->turn + 1 == sVsp.turn && prev->kind == VSP_CHOICE_MOVE && IsProtectMove(prev->move) && prev->partyIdx == gBattlerPartyIndexes[b]);

    if (isMove)
    {
        // 1. Protect consecutivo.
        if (IsProtectMove(move) && prev->turn + 1 == sVsp.turn && prev->kind == VSP_CHOICE_MOVE
         && IsProtectMove(prev->move) && prev->partyIdx == gBattlerPartyIndexes[b])
            ReportMisplay(TRUE, "PROTECT_CONSECUTIVO", b, "%s dopo %s al turno prima", A(MoveName(move)), A(MoveName(prev->move)));

        // 2. Fake Out dopo il primo turno in campo.
        if (eff == EFFECT_FIRST_TURN_ONLY && !IsBattlersFirstTurn(b))
            ReportMisplay(TRUE, "FAKE_OUT_TARDIVO", b, "%s non al primo turno in campo (fallisce)", A(MoveName(move)));

        // 3. Mossa su un immune noto.
        if (damaging && !spread && t < MAX_BATTLERS_COUNT && !IsBattlerAlly(b, t) && IsBattlerAlive(t)
         && gAiLogicData->effectiveness[b][t][idx] == UQ_4_12(0.0))
        {
            enum BattlerId o = OtherFoe(b, t);
            bool32 redirectUseful = pAlive && ChoiceKOs(p, t) && IsBattlerAlive(o) && gAiLogicData->effectiveness[b][o][idx] != UQ_4_12(0.0);
            ReportMisplay(!redirectUseful, "IMMUNE", b, "%s su %s (immune%s)", A(MoveName(move)), A(GetSpeciesName(gBattleMons[t].species)),
                          redirectUseful ? ", ma il compagno lo manda KO prima e la mossa passa sull'altro" : "");
        }
        else if (!damaging && t < MAX_BATTLERS_COUNT && !IsBattlerAlly(b, t) && IsBattlerAlive(t) && StatusMoveBlocked(b, t, move))
        {
            ReportMisplay(TRUE, "IMMUNE", b, "%s su %s (immunita' nota)", A(MoveName(move)), A(GetSpeciesName(gBattleMons[t].species)));
        }

        // 4. Bersaglio che il compagno manda già KO prima, e la mossa non serve sull'altro.
        if (damaging && !spread && pAlive && t < MAX_BATTLERS_COUNT && !IsBattlerAlly(b, t) && ChoiceKOs(p, t)
         && sVsp.choice[p].kind == VSP_CHOICE_MOVE
         && AI_IsFaster(p, b, sVsp.choice[p].move, move, CONSIDER_PRIORITY))
        {
            enum BattlerId o = OtherFoe(b, t);
            // Sull'ultimo Pokémon avversario (nessun altro in campo né in panchina) raddoppiare è solo un'assicurazione:
            // se il compagno lo manda KO la partita finisce, quindi non è un errore.
            bool32 lastFoe = !IsBattlerAlive(o) && CountUsablePartyMons(t) == 0;
            if (!lastFoe && (!IsBattlerAlive(o) || gAiLogicData->effectiveness[b][o][idx] == UQ_4_12(0.0)))
                ReportMisplay(TRUE, "BERSAGLIO_GIA_KO", b, "%s su %s che il compagno manda KO prima; nessun redirect utile",
                              A(MoveName(move)), A(GetSpeciesName(gBattleMons[t].species)));
        }

        // 5. Attacco sul compagno senza combo.
        if (damaging && pAlive)
        {
            if (!spread && t == p && !PartnerHitIsCombo(b, p, move, idx))
                ReportMisplay(TRUE, "ATTACCO_COMPAGNO", b, "%s sul compagno %s senza combo", A(MoveName(move)), A(GetSpeciesName(gBattleMons[p].species)));
            else if (tt == TARGET_FOES_AND_ALLY && !ChoiceIsProtect(p) && !PartnerHitIsCombo(b, p, move, idx))
            {
                if (DmgMin(b, p, idx) >= gBattleMons[p].hp)
                    ReportMisplay(TRUE, "ATTACCO_COMPAGNO", b, "%s manda KO il compagno %s", A(MoveName(move)), A(GetSpeciesName(gBattleMons[p].species)));
                else if (gAiLogicData->simulatedDmg[b][p][idx].median * 2 >= gBattleMons[p].hp)
                    ReportMisplay(FALSE, "ATTACCO_COMPAGNO", b, "%s toglie meta' dei PS rimasti al compagno %s", A(MoveName(move)), A(GetSpeciesName(gBattleMons[p].species)));
            }
        }

        // 6. Setup con un KO in arrivo e nessuna copertura.
        if (IsBattleMoveStatus(move) && IsStatRaisingMove(move) && !IsProtectMove(move) && threatMin)
        {
            bool32 covered = FALSE;
            if (pAlive && sVsp.choice[p].kind == VSP_CHOICE_MOVE)
            {
                u32 pe = GetMoveEffect(sVsp.choice[p].move);
                if (pe == EFFECT_FOLLOW_ME)
                    covered = TRUE;
                if (pe == EFFECT_FIRST_TURN_ONLY && sVsp.choice[p].target == threatFoe && IsBattlersFirstTurn(p))
                    covered = TRUE;
            }
            if (!covered)
                ReportMisplay(TRUE, "SETUP_SOTTO_KO", b, "%s mentre %s lo manda KO (danno minimo stimato)", A(MoveName(move)),
                              A(GetSpeciesName(gBattleMons[threatFoe].species)));
        }
        else if (IsBattleMoveStatus(move) && IsStatRaisingMove(move) && !IsProtectMove(move) && threatMax)
        {
            ReportMisplay(FALSE, "SETUP_SOTTO_KO", b, "%s con un KO possibile (danno massimo)", A(MoveName(move)));
        }

        // 7. Tailwind / Trick Room sprecati.
        if (eff == EFFECT_TAILWIND && (gSideStatuses[GetBattlerSide(b)] & SIDE_STATUS_TAILWIND) && gSideTimers[GetBattlerSide(b)].tailwindTimer > 1)
            ReportMisplay(TRUE, "VELOCITA_SPRECATA", b, "Tailwind con il proprio Tailwind ancora attivo (%d turni)", gSideTimers[GetBattlerSide(b)].tailwindTimer);
        if (eff == EFFECT_TRICK_ROOM && (gFieldStatuses & STATUS_FIELD_TRICK_ROOM) && gFieldTimers.trickRoomTimer > 1)
        {
            // Chiudere un Trick Room quando si è più veloci è giusto: errore solo se la squadra è lenta (stessa soglia
            // dell'IA, battle_ai_vgc.c: almeno 2 Pokémon più lenti degli avversari).
            if (AI_VgcTeamIsSlowerThanFoes(b, 2))
                ReportMisplay(TRUE, "VELOCITA_SPRECATA", b, "Trick Room con Trick Room attivo (%d turni): lo chiude",
                              gFieldTimers.trickRoomTimer);
        }
    }

    // 8. KO garantito disponibile e non preso.
    if (c->kind != VSP_CHOICE_NONE)
    {
        for (k = 0; k < 2; k++)
        {
            enum BattlerId f = foes[k];
            u32 koIdx = MAX_MON_MOVES;
            bool32 covered;
            if (!IsBattlerAlive(f))
                continue;
            for (i = 0; i < MAX_MON_MOVES; i++)
            {
                enum Move m = gBattleMons[b].moves[i];
                if (!MoveUsable(b, i) || IsBattleMoveStatus(m))
                    continue;
                // Non sono KO "garantiti": mosse che sacrificano chi le usa, priorità sul Campo Psichico contro chi è a
                // terra, colpi a bersaglio singolo quando il compagno di `f` attira gli attacchi (Follow Me / Rage Powder).
                if (GetMoveEffect(m) == EFFECT_FINAL_GAMBIT || IsExplosionMove(m))
                    continue;
                if (gFieldTimers.terrain == B_TERRAIN_PSYCHIC && GetBattleMovePriority(b, gAiLogicData->abilities[b], m) > 0
                 && IsBattlerGrounded(f, gAiLogicData->abilities[f], gAiLogicData->holdEffects[f]))
                    continue;
                if (!IsSpreadMove(GetMoveTarget(m)) && IsBattlerAlive(GetPartnerBattler(f))
                 && sVsp.choice[GetPartnerBattler(f)].kind == VSP_CHOICE_MOVE
                 && GetMoveEffect(sVsp.choice[GetPartnerBattler(f)].move) == EFFECT_FOLLOW_ME)
                    continue;
                // Focus Sash / Sturdy (presunti) a PS pieni: non è un KO.
                if (gBattleMons[f].hp == gBattleMons[f].maxHP
                 && (gAiLogicData->holdEffects[f] == HOLD_EFFECT_FOCUS_SASH || gAiLogicData->abilities[f] == ABILITY_STURDY))
                    continue;
                if (DmgMin(b, f, i) >= gBattleMons[f].hp && gAiLogicData->moveAccuracy[b][f][i] >= 100)
                {
                    koIdx = i;
                    break;
                }
            }
            if (koIdx == MAX_MON_MOVES)
                continue;
            covered = ChoiceKOs(b, f) || (pAlive && ChoiceKOs(p, f)) || ChoiceKOs(b, OtherFoe(b, f));
            if (covered)
                continue;
            // I casi dubbi divisi per tipo: lettura di un Protect visto, mossa ad area (danno su entrambi), scelta di
            // supporto/Protect/cambio. Restano "certi" solo gli attacchi a bersaglio singolo senza lettura possibile.
            if (damaging && KnowsProtect(f))
            {
                ReportMisplay(FALSE, "KO_NON_PRESO_LETTURA", b, "%s avrebbe mandato KO %s, scelto %s (ha Protect: possibile lettura)",
                              A(MoveName(gBattleMons[b].moves[koIdx])), A(GetSpeciesName(gBattleMons[f].species)), A(MoveName(move)));
            }
            else if (damaging && IsSpreadMove(GetMoveTarget(move)))
            {
                ReportMisplay(FALSE, "KO_NON_PRESO_AREA", b, "%s avrebbe mandato KO %s, scelta la mossa ad area %s",
                              A(MoveName(gBattleMons[b].moves[koIdx])), A(GetSpeciesName(gBattleMons[f].species)), A(MoveName(move)));
            }
            else if (damaging)
            {
                ReportMisplay(TRUE, "KO_NON_PRESO", b, "%s avrebbe mandato KO %s, scelto %s", A(MoveName(gBattleMons[b].moves[koIdx])),
                              A(GetSpeciesName(gBattleMons[f].species)), A(MoveName(move)));
            }
            else
            {
                ReportMisplay(FALSE, "KO_NON_PRESO_SUPPORTO", b, "%s avrebbe mandato KO %s, scelto %s %s", A(MoveName(gBattleMons[b].moves[koIdx])),
                              A(GetSpeciesName(gBattleMons[f].species)), c->kind == VSP_CHOICE_SWITCH ? "cambio" : "mossa", A(MoveName(move)));
            }
            break;
        }
    }

    // 9. Cambi a ripetizione (stesso posto: A -> B e poi B -> A entro due turni).
    if (c->kind == VSP_CHOICE_SWITCH)
    {
        struct VspPrev *ps = &sVsp.prevSwitch[b];
        if (ps->kind == VSP_CHOICE_SWITCH && sVsp.turn - ps->turn <= 2 && ps->partyIdx == gBattlerPartyIndexes[b] && ps->fromIdx == c->partyIdx)
            ReportMisplay(sVsp.turn - ps->turn <= 1, "CAMBI_RIPETUTI", b, "rientra %s uscito %d turni fa", A(GetSpeciesName(GetMonData(&GetBattlerParty(b)[c->partyIdx], MON_DATA_SPECIES))),
                          sVsp.turn - ps->turn);
        sVsp.prevSwitch2[b] = *ps;
        ps->kind = VSP_CHOICE_SWITCH;
        ps->turn = sVsp.turn;
        ps->partyIdx = c->partyIdx;
        ps->fromIdx = gBattlerPartyIndexes[b];
    }

    prev->turn = sVsp.turn;
    prev->kind = c->kind;
    prev->move = move;
    prev->partyIdx = gBattlerPartyIndexes[b];
}

static void PrintChoice(enum BattlerId b)
{
    struct VspChoice *c = &sVsp.choice[b];
    // Forma compatta per il libro dei piani (M9): turno, battler, posto in squadra (0..3, nell'ordine della team
    // preview), mossa (0 = cambio/niente).
    if (sVsp.turn <= 1 && IsBattlerAlive(b))
        Test_MgbaPrintf("VSP SCELTA t%d b%d p%d m%d", sVsp.turn + 1, b, gBattlerPartyIndexes[b], c->kind == VSP_CHOICE_MOVE ? c->move : 0);
    if (c->kind == VSP_CHOICE_MOVE)
    {
        const char *tgt = "";
        if (c->target < MAX_BATTLERS_COUNT)
            tgt = (c->target == b) ? "se stesso" : (IsBattlerAlly(b, c->target) ? "compagno" : "avversario");
        Test_MgbaPrintf("VSP AZIONE b%d %S MOSSA %S -> b%d %s%s", b, GetSpeciesName(gBattleMons[b].species), MoveName(c->move),
                        c->target, tgt, c->gimmick == GIMMICK_MEGA ? " +MEGA" : "");
    }
    else if (c->kind == VSP_CHOICE_SWITCH)
    {
        Test_MgbaPrintf("VSP AZIONE b%d %S CAMBIO -> %S (slot%d)", b, GetSpeciesName(gBattleMons[b].species),
                        GetSpeciesName(GetMonData(&GetBattlerParty(b)[c->partyIdx], MON_DATA_SPECIES)), c->partyIdx);
    }
    else if (IsBattlerAlive(b))
    {
        Test_MgbaPrintf("VSP AZIONE b%d %S NESSUNA (mossa obbligata o in ricarica)", b, GetSpeciesName(gBattleMons[b].species));
    }
}

static void EvaluateTurn(void)
{
    enum BattlerId b;
    u32 side;

    sVsp.pending = FALSE;
    sVsp.evaluated = TRUE;
    for (b = 0; b < gBattlersCount; b++)
        PrintChoice(b);
    for (b = 0; b < gBattlersCount; b++)
    {
        if (IsBattlerAlive(b) && sVsp.choice[b].kind != VSP_CHOICE_NONE)
            CheckBattlerChoice(b);
    }
    // Turno di controllo velocità sprecato: tutto il lato in Protect/cambio mentre il proprio Tailwind/Trick Room scorre.
    for (side = 0; side < NUM_BATTLE_SIDES; side++)
    {
        enum BattlerId l = GetBattlerAtPosition(side), r = GetBattlerAtPosition(side | BIT_FLANK);
        bool32 lIdle = !IsBattlerAlive(l) || ChoiceIsSafe(l), rIdle = !IsBattlerAlive(r) || ChoiceIsSafe(r);
        if ((IsBattlerAlive(l) || IsBattlerAlive(r)) && lIdle && rIdle)
        {
            enum BattlerId any = IsBattlerAlive(l) ? l : r;
            AI_SetPerspective(any);
            if (AI_VgcOwnSpeedControlActive(any))
                ReportMisplay(FALSE, "VELOCITA_SPRECATA", any, "tutto il lato in Protect/cambio durante il proprio Tailwind/Trick Room");
        }
    }
}

// ---------------------------------------------------------------------
// Eventi del turno
// ---------------------------------------------------------------------
void VgcSelfPlay_OnMessage(const u8 *string)
{
    if (sVsp.pending)
        EvaluateTurn();
    // Solo i messaggi veri contano come avanzamento (un ciclo di stringhe vuote non deve nascondere un blocco).
    if (string != NULL && string[0] != EOS)
    {
        sVsp.progress++;
        Test_MgbaPrintf("VSP MSG %.*S", 150, string);
    }
    // Esito deciso ("You defeated ...", "You were overwhelmed ..."): la partita finisce qui. Le scene di fine lotta
    // (allenatore che rientra, testo di sconfitta, soldi) non servono e nel ROM dei test leggono dati assenti.
    if (gBattleOutcome != 0 && !sVsp.ended)
        VgcSelfPlay_OnBattleEnd();
}

void VgcSelfPlay_OnAbilityPopUp(enum BattlerId battler, enum Ability ability)
{
    if (sVsp.pending)
        EvaluateTurn();
    Test_MgbaPrintf("VSP ABILITA b%d %S", battler, AbilityName(ability));
}

void VgcSelfPlay_OnHP(enum BattlerId battler, u32 oldHP, u32 newHP)
{
    sVsp.progress++;
    if (sVsp.pending)
        EvaluateTurn();
    Test_MgbaPrintf("VSP PS b%d %S %d -> %d / %d", battler, GetSpeciesName(gBattleMons[battler].species), oldHP, newHP, gBattleMons[battler].maxHP);
    if (newHP == 0 && oldHP > 0)
    {
        struct VspChoice *c = &sVsp.choice[battler];
        struct VspDecisionInfo *info = &sVsp.info[battler];
        Test_MgbaPrintf("VSP KO b%d %S t%d", battler, GetSpeciesName(gBattleMons[battler].species), sVsp.turn + 1);
        // Cambio su chi va KO all'entrata (prima di agire).
        if (c->kind == VSP_CHOICE_SWITCH && gBattlerPartyIndexes[battler] == c->partyIdx)
            ReportMisplay(!gSpecialStatuses[battler].criticalHit, "CAMBIO_IN_KO", battler, "entrato con un cambio e KO nello stesso turno%s",
                          gSpecialStatuses[battler].criticalHit ? " (colpo critico)" : "");
        // Win condition persa quando poteva proteggersi o uscire.
        if (info->valid && info->partyIdx == gBattlerPartyIndexes[battler] && info->isWinCon && !info->choseSafe
         && (info->couldProtect || info->couldSwitch))
        {
            AI_SetPerspective(battler);
            ReportMisplay(info->koThreatKnown, "WIN_CONDITION_PERSA", battler, "KO senza Protect/cambio (%s%s)%s",
                          info->couldProtect ? "Protect disponibile" : "", info->couldSwitch ? (info->couldProtect ? ", cambio possibile" : "cambio possibile") : "",
                          info->koThreatKnown ? ": il KO era prevedibile" : ": KO non previsto dalle stime");
        }
    }
}

void VgcSelfPlay_OnBattleEnd(void)
{
    const char *outcome;
    u32 left[NUM_BATTLE_SIDES] = {0};
    u32 side, i;

    if (sVsp.ended)
        return;
    sVsp.ended = TRUE;
    if (sVsp.pending)
        EvaluateTurn();
    switch (gBattleOutcome & 0x7F)
    {
    case B_OUTCOME_WON:  outcome = "VITTORIA_G"; break;
    case B_OUTCOME_LOST: outcome = "VITTORIA_A"; break;
    case B_OUTCOME_DREW: outcome = "PAREGGIO"; break;
    default:             outcome = "ALTRO"; break;
    }
    for (side = 0; side < NUM_BATTLE_SIDES; side++)
    {
        struct Pokemon *party = gParties[side == B_SIDE_PLAYER ? B_TRAINER_PLAYER : B_TRAINER_OPPONENT_A];
        for (i = 0; i < PARTY_SIZE; i++)
            if (GetMonData(&party[i], MON_DATA_SPECIES) != SPECIES_NONE && GetMonData(&party[i], MON_DATA_HP) > 0)
                left[side]++;
    }
    VgcSelfPlay_CheckHeap("fine partita");
    Test_MgbaPrintf("VSP FINE esito=%s turni=%d restantiG=%d restantiA=%d misplay=%d dubbi=%d", outcome, gBattleTurnCounter,
                    left[B_SIDE_PLAYER], left[B_SIDE_OPPONENT], sVsp.misplays, sVsp.doubts);
    // Si esce subito, come per il limite di turni: la chiusura della lotta del motore e il TearDown del runner
    // liberavano due volte le stesse risorse (assert in malloc.c e poi codice a caso).
    Test_ExitWithResult(TEST_RESULT_PASS, SourceLine(0), "VSP fine partita");
}
