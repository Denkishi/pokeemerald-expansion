// Logger delle lotte VGC (CUSTOM_VGC_BATTLE_LOG, include/config/custom.h).
//
// Nelle lotte contro un allenatore con AI_FLAG_VGC_DOUBLES scrive righe "VGCLOG ..." nel log di mGBA: squadre e team
// preview, stato del campo a ogni turno, decisioni del pianificatore, scelte di tutti, cronaca (messaggi e PS) ed
// esito. Le legge tools/vgc_training/analizza_log.py; formato e istruzioni in docs/ia_vgc/logger.md.
//
// Uscita: i registri di debug di mGBA (0x4FFF780 / 0x4FFF700 / 0x4FFF600), gli stessi di MgbaPrintf, che nel ROM
// normale è spento (NDEBUG). La stretta di mano si fa una volta a inizio lotta: se il registro non risponde 0x1DEA
// (console vera, altri emulatori) il logger resta spento e non scrive nulla. Nel ROM dei test le righe passano da
// Test_MgbaPrintf e il logger è acceso solo nel self-play VGC (test/vgc_selfplay), così le battaglie di test non
// cambiano.
//
// Costo: stato in EWRAM di poche decine di byte, buffer della riga sullo stack; nulla gira se il logger è spento.
// Al pianificatore costa solo le righe a fine lavoro (una per coppia), fuori dalle fette di tempo.
#include <stdarg.h>
#include "global.h"
#include "battle.h"
#include "battle_ai_main.h"
#include "battle_ai_perspective.h"
#include "battle_ai_util.h"
#include "battle_ai_vgc.h"
#include "battle_gimmick.h"
#include "battle_main.h"
#include "battle_setup.h"
#include "battle_util.h"
#include "data.h"
#include "item.h"
#include "move.h"
#include "pokemon.h"
#include "string_util.h"
#include "vgc_battle_log.h"
#include "config/custom.h"
#include "constants/abilities.h"
#include "constants/battle.h"
#include "constants/battle_ai.h"
#include "constants/battle_move_effects.h"
#include "constants/characters.h"
#include "constants/moves.h"
#include "constants/trainers.h"
#if TESTING
#include "test/vgc_selfplay.h"
extern s32 Test_MgbaPrintf(const char *fmt, ...);
extern bool8 gTestMgbaQuiet;
#endif

#define VGCLOG_LINE_MAX 240 // mGBA accetta fino a 256 byte per riga

struct VgcLogState
{
    u8 on;              // logger acceso per questa lotta/serie
    u8 ended;           // FINE già scritta
    u8 nGroups;
    u16 trainer;        // allenatore della lotta (TRAINER_BATTLE_PARAM.opponentA)
    u8 groupMask[3];    // team preview: i 3 gruppi di 4 migliori (slot originali)
    s32 groupScore[3];
};

static EWRAM_DATA struct VgcLogState sVl = {0};

// ---------------------------------------------------------------------
// Uscita
// ---------------------------------------------------------------------
#define VGCLOG_REG_ENABLE (*(vu16 *)0x4FFF780)
#define VGCLOG_REG_FLAGS  (*(vu16 *)0x4FFF700)
#define VGCLOG_REG_STRING ((vu8 *)0x4FFF600)
#define VGCLOG_LEVEL_WARN 2

static bool32 Handshake(void)
{
#if TESTING
    return VgcSelfPlay_IsActive() && !gTestMgbaQuiet; // self-play non silenzioso
#else
    // Una volta per lotta (TryStart): su console vera il registro non esiste e la lettura non dà 0x1DEA.
    VGCLOG_REG_ENABLE = 0xC0DE;
    return VGCLOG_REG_ENABLE == 0x1DEA;
#endif
}

static void Emit(const char *line, u32 len)
{
#if TESTING
    (void)len;
    Test_MgbaPrintf("%s", line);
#else
    u32 i;
    for (i = 0; i < len; i++)
        VGCLOG_REG_STRING[i] = line[i];
    VGCLOG_REG_STRING[len] = 0;
    VGCLOG_REG_FLAGS = VGCLOG_LEVEL_WARN | 0x100;
#endif
}

static char DecodeChar(u8 c)
{
    if (c >= CHAR_a && c <= CHAR_z)
        return 'a' + (c - CHAR_a);
    if (c >= CHAR_A && c <= CHAR_Z)
        return 'A' + (c - CHAR_A);
    if (c >= CHAR_0 && c <= CHAR_9)
        return '0' + (c - CHAR_0);
    switch (c)
    {
    case CHAR_SPACE: case CHAR_NBSP: case CHAR_SPACER:
    case CHAR_NEWLINE: case CHAR_PROMPT_SCROLL: case CHAR_PROMPT_CLEAR:
        return ' ';
    case CHAR_EXCL_MARK: return '!';
    case CHAR_QUESTION_MARK: return '?';
    case CHAR_PERIOD: case CHAR_ELLIPSIS: return '.';
    case CHAR_HYPHEN: return '-';
    case CHAR_COMMA: return ',';
    case CHAR_COLON: return ':';
    case CHAR_SEMICOLON: return ';';
    case CHAR_SLASH: return '/';
    case CHAR_PERCENT: return '%';
    case CHAR_LEFT_PAREN: return '(';
    case CHAR_RIGHT_PAREN: return ')';
    case CHAR_AMPERSAND: return '&';
    case CHAR_PLUS: return '+';
    case CHAR_EQUALS: return '=';
    case CHAR_SGL_QUOTE_LEFT: case CHAR_SGL_QUOTE_RIGHT: return '\'';
    case CHAR_DBL_QUOTE_LEFT: case CHAR_DBL_QUOTE_RIGHT: return '"';
    case CHAR_A_GRAVE: case CHAR_A_ACUTE: return 'A';
    case CHAR_E_GRAVE: case CHAR_E_ACUTE: return 'E';
    case CHAR_I_GRAVE: case CHAR_I_ACUTE: return 'I';
    case CHAR_O_GRAVE: case CHAR_O_ACUTE: return 'O';
    case CHAR_U_GRAVE: case CHAR_U_ACUTE: return 'U';
    case CHAR_a_GRAVE: case CHAR_a_ACUTE: return 'a';
    case CHAR_e_GRAVE: case CHAR_e_ACUTE: return 'e';
    case CHAR_i_GRAVE: case CHAR_i_ACUTE: return 'i';
    case CHAR_o_GRAVE: case CHAR_o_ACUTE: return 'o';
    case CHAR_u_GRAVE: case CHAR_u_ACUTE: return 'u';
    case CHAR_PK: return 'P';
    case CHAR_MN: return 'M';
    }
    return '?';
}

struct LineBuf
{
    char s[VGCLOG_LINE_MAX + 1];
    u32 n;
};

static void PutC(struct LineBuf *b, char c)
{
    if (b->n < VGCLOG_LINE_MAX)
        b->s[b->n++] = c;
}

// Stringa del gioco in ASCII: codici di controllo saltati, a capo e spazi doppi compressi.
static void PutGameString(struct LineBuf *b, const u8 *str)
{
    if (str == NULL)
    {
        PutC(b, '-');
        return;
    }
    while (*str != EOS && b->n < VGCLOG_LINE_MAX)
    {
        u8 ch = *str++;
        char c;
        if (ch == EXT_CTRL_CODE_BEGIN)
        {
            u32 skip = GetExtCtrlCodeLength(*str);
            while (skip != 0 && *str != EOS)
            {
                str++;
                skip--;
            }
            continue;
        }
        if (ch == PLACEHOLDER_BEGIN)
        {
            if (*str != EOS)
                str++;
            continue;
        }
        c = DecodeChar(ch);
        if (c == ' ' && (b->n == 0 || b->s[b->n - 1] == ' '))
            continue;
        PutC(b, c);
    }
}

static void PutNum(struct LineBuf *b, s32 d)
{
    char tmp[12];
    u32 k = 0, u = (d < 0) ? -d : d;
    if (d < 0)
        PutC(b, '-');
    do { tmp[k++] = '0' + u % 10; u /= 10; } while (u > 0);
    while (k > 0)
        PutC(b, tmp[--k]);
}

// Formato minimo: %d (s32), %s (ASCII), %S (stringa del gioco), %c.
static void Out(const char *fmt, ...)
{
    struct LineBuf b;
    va_list va;

    b.n = 0;
    va_start(va, fmt);
    while (*fmt)
    {
        if (fmt[0] == '%' && fmt[1] != 0)
        {
            switch (fmt[1])
            {
            case 'd': PutNum(&b, va_arg(va, s32)); break;
            case 'c': PutC(&b, (char)va_arg(va, int)); break;
            case 'S': PutGameString(&b, va_arg(va, const u8 *)); break;
            case 's':
            {
                const char *s = va_arg(va, const char *);
                while (s != NULL && *s)
                    PutC(&b, *s++);
                break;
            }
            default: PutC(&b, fmt[1]); break;
            }
            fmt += 2;
        }
        else
        {
            PutC(&b, *fmt++);
        }
    }
    va_end(va);
    b.s[b.n] = 0;
    Emit(b.s, b.n);
}

// ---------------------------------------------------------------------
// Accensione
// ---------------------------------------------------------------------
static bool32 BattleIsVgc(void)
{
    u16 id = TRAINER_BATTLE_PARAM.opponentA;
    if (!(gBattleTypeFlags & BATTLE_TYPE_TRAINER) || id == TRAINER_NONE || id >= TRAINERS_COUNT)
        return FALSE;
    return (GetTrainerAIFlagsFromId(id) & AI_FLAG_VGC_DOUBLES) != 0;
}

// Accende il logger per la lotta in corso (se è una lotta VGC e mGBA risponde). TRUE = acceso.
static bool32 TryStart(void)
{
    if (!CUSTOM_VGC_BATTLE_LOG || !BattleIsVgc() || !Handshake())
    {
        sVl.on = FALSE;
        return FALSE;
    }
    if (!sVl.on || sVl.trainer != TRAINER_BATTLE_PARAM.opponentA)
        sVl.nGroups = 0;
    sVl.on = TRUE;
    sVl.ended = FALSE;
    sVl.trainer = TRAINER_BATTLE_PARAM.opponentA;
    return TRUE;
}

static bool32 IsOn(void)
{
    return CUSTOM_VGC_BATTLE_LOG && sVl.on && !sVl.ended && (gBattleTypeFlags & BATTLE_TYPE_TRAINER)
        && TRAINER_BATTLE_PARAM.opponentA == sVl.trainer;
}

// ---------------------------------------------------------------------
// Nomi
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
    if (item == ITEM_NONE || item >= ITEMS_COUNT)
        return sTextNone;
    return GetItemName(item);
}

static const u8 *AbilityName(u32 ability)
{
    if (ability == ABILITY_NONE || ability >= ABILITIES_COUNT)
        return sTextNone;
    return gAbilitiesInfo[ability].name;
}

static const u8 *SpeciesName(u32 species)
{
    if (species == SPECIES_NONE || species >= NUM_SPECIES)
        return sTextNone;
    return GetSpeciesName(species);
}

static u32 MonSpecies(struct Pokemon *mon)
{
    return GetMonData(mon, MON_DATA_SPECIES);
}

static const char *SideName(u32 battler)
{
    return GetBattlerSide(battler) == B_SIDE_PLAYER ? "G" : "A"; // G = giocatore, A = IA
}

static const char *StatusName(u32 status1)
{
    if (status1 & STATUS1_SLEEP) return "SLP";
    if (status1 & STATUS1_TOXIC_POISON) return "TOX";
    if (status1 & STATUS1_POISON) return "PSN";
    if (status1 & STATUS1_BURN) return "BRN";
    if (status1 & STATUS1_FREEZE) return "FRZ";
    if (status1 & STATUS1_PARALYSIS) return "PAR";
    if (status1 & STATUS1_FROSTBITE) return "FRB";
    return "-";
}

static bool32 IsProtectLike(u32 move)
{
    return GetMoveEffect(move) == EFFECT_PROTECT && move != MOVE_WIDE_GUARD && move != MOVE_QUICK_GUARD
        && move != MOVE_CRAFTY_SHIELD && move != MOVE_MAT_BLOCK;
}

// Proprietà della mossa per i controlli dell'analisi: P Protect, S setup, F Fake Out, W Tailwind, R Trick Room,
// A ad area sui due avversari, X ad area anche sul compagno, H Helping Hand, M Follow Me, Q priorità > 0.
static void MoveFlags(u32 battler, u32 move, char *out)
{
    u32 n = 0;
    enum BattleMoveEffects eff;
    enum MoveTarget tt;
    if (move == MOVE_NONE || move >= MOVES_COUNT)
    {
        out[0] = '-';
        out[1] = 0;
        return;
    }
    eff = GetMoveEffect(move);
    tt = GetBattlerMoveTargetType(battler, move);
    if (IsProtectLike(move)) out[n++] = 'P';
    if (IsBattleMoveStatus(move) && IsStatRaisingMove(move) && !IsProtectLike(move)) out[n++] = 'S';
    if (eff == EFFECT_FIRST_TURN_ONLY) out[n++] = 'F';
    if (eff == EFFECT_TAILWIND) out[n++] = 'W';
    if (eff == EFFECT_TRICK_ROOM) out[n++] = 'R';
    if (tt == TARGET_BOTH) out[n++] = 'A';
    if (tt == TARGET_FOES_AND_ALLY) out[n++] = 'X';
    if (eff == EFFECT_HELPING_HAND) out[n++] = 'H';
    if (eff == EFFECT_FOLLOW_ME) out[n++] = 'M';
    if (GetBattleMovePriority(battler, gBattleMons[battler].ability, move) > 0) out[n++] = 'Q';
    if (n == 0) out[n++] = '-';
    out[n] = 0;
}

static const char *CategoryName(u32 move)
{
    switch (GetMoveCategory(move))
    {
    case DAMAGE_CATEGORY_PHYSICAL: return "fis";
    case DAMAGE_CATEGORY_SPECIAL: return "spe";
    default: return "sta";
    }
}

// ---------------------------------------------------------------------
// Team preview
// ---------------------------------------------------------------------
static void PrintSet(const char *side, u32 slot, struct Pokemon *mon)
{
    Out("VGCLOG SET %s %d %S @ %S | %S | %S / %S / %S / %S | liv %d vel %d ps %d",
        side, slot, SpeciesName(MonSpecies(mon)), ItemName(GetMonData(mon, MON_DATA_HELD_ITEM)),
        AbilityName(GetMonAbility(mon)),
        MoveName(GetMonData(mon, MON_DATA_MOVE1)), MoveName(GetMonData(mon, MON_DATA_MOVE2)),
        MoveName(GetMonData(mon, MON_DATA_MOVE3)), MoveName(GetMonData(mon, MON_DATA_MOVE4)),
        GetMonData(mon, MON_DATA_LEVEL), GetMonData(mon, MON_DATA_SPEED), GetMonData(mon, MON_DATA_MAX_HP));
}

void VgcLog_TeamPreviewBegin(struct Pokemon *enemy, struct Pokemon *player)
{
    u32 i;
    if (!TryStart())
        return;
    sVl.nGroups = 0;
    Out("VGCLOG INIZIO allenatore=%d %S", TRAINER_BATTLE_PARAM.opponentA, GetTrainerNameFromId(TRAINER_BATTLE_PARAM.opponentA));
    for (i = 0; i < PARTY_SIZE; i++)
        if (MonSpecies(&player[i]) != SPECIES_NONE)
            PrintSet("G", i, &player[i]);
    for (i = 0; i < PARTY_SIZE; i++)
        if (MonSpecies(&enemy[i]) != SPECIES_NONE)
            PrintSet("A", i, &enemy[i]);
}

void VgcLog_TeamPreviewGroup(u32 mask, const u32 *enemySlots, u32 nEnemy, s32 score)
{
    u32 i, slotMask = 0, c;
    if (!IsOn())
        return;
    for (i = 0; i < nEnemy; i++)
        if (mask & (1u << i))
            slotMask |= 1u << enemySlots[i];
    // I 3 gruppi migliori, in ordine.
    if (sVl.nGroups < 3)
        c = sVl.nGroups++;
    else if (score > sVl.groupScore[2])
        c = 2;
    else
        return;
    while (c > 0 && sVl.groupScore[c - 1] < score)
    {
        sVl.groupScore[c] = sVl.groupScore[c - 1];
        sVl.groupMask[c] = sVl.groupMask[c - 1];
        c--;
    }
    sVl.groupScore[c] = score;
    sVl.groupMask[c] = slotMask;
}

static void PrintMaskSpecies(struct LineBuf *b, struct Pokemon *party, u32 mask)
{
    u32 i;
    bool32 first = TRUE;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (!(mask & (1u << i)))
            continue;
        if (!first)
            PutC(b, ',');
        PutGameString(b, SpeciesName(MonSpecies(&party[i])));
        first = FALSE;
    }
}

void VgcLog_TeamPreviewAi(struct Pokemon *enemy, struct Pokemon *player, const u8 *outOrder, u32 order,
                          u32 nCand, const s32 *candS, const s32 *candWorst, const u32 *candA, const u32 *candB,
                          const u32 *chosen, const u32 *enemySlots, u32 nPred, const u32 *predJ, const u32 *predK,
                          const s32 *predW, const u32 *playerSlots, u16 megaSpecies)
{
    u32 i;
    if (!IsOn())
        return;
    for (i = 0; i < sVl.nGroups; i++)
    {
        struct LineBuf b;
        const char *head = "VGCLOG TP GRUPPO ";
        b.n = 0;
        while (*head)
            PutC(&b, *head++);
        PutNum(&b, i + 1);
        PutC(&b, ' ');
        PutNum(&b, sVl.groupScore[i]);
        PutC(&b, ' ');
        PrintMaskSpecies(&b, enemy, sVl.groupMask[i]);
        b.s[b.n] = 0;
        Emit(b.s, b.n);
    }
    for (i = 0; i < nCand && i < 3; i++)
        Out("VGCLOG TP LEAD %d %d peggiore=%d %S + %S", i + 1, candS[i], candWorst[i],
            SpeciesName(MonSpecies(&enemy[enemySlots[chosen[candA[i]]]])),
            SpeciesName(MonSpecies(&enemy[enemySlots[chosen[candB[i]]]])));
    for (i = 0; i < nPred && i < 4; i++)
        Out("VGCLOG TP PREVISTE %d peso=%d %S + %S", i + 1, predW[i],
            SpeciesName(MonSpecies(&player[playerSlots[predJ[i]]])), SpeciesName(MonSpecies(&player[playerSlots[predK[i]]])));
    Out("VGCLOG PORTA A %d %d %d %d | lead %S + %S | dietro %S, %S | mega %S",
        order > 0 ? outOrder[0] : 255, order > 1 ? outOrder[1] : 255, order > 2 ? outOrder[2] : 255, order > 3 ? outOrder[3] : 255,
        SpeciesName(order > 0 ? MonSpecies(&enemy[outOrder[0]]) : SPECIES_NONE),
        SpeciesName(order > 1 ? MonSpecies(&enemy[outOrder[1]]) : SPECIES_NONE),
        SpeciesName(order > 2 ? MonSpecies(&enemy[outOrder[2]]) : SPECIES_NONE),
        SpeciesName(order > 3 ? MonSpecies(&enemy[outOrder[3]]) : SPECIES_NONE),
        SpeciesName(megaSpecies));
}

void VgcLog_TeamPreviewPlayer(u32 game, u32 bestOf, struct Pokemon *fullParty, const u8 *order, u32 picks)
{
    if (!IsOn())
        return;
    Out("VGCLOG GARA %d di Bo%d allenatore=%d %S", game, bestOf, sVl.trainer, GetTrainerNameFromId(sVl.trainer));
    Out("VGCLOG PORTA G %d %d %d %d | lead %S + %S | dietro %S, %S",
        picks > 0 ? order[0] : 255, picks > 1 ? order[1] : 255, picks > 2 ? order[2] : 255, picks > 3 ? order[3] : 255,
        SpeciesName(picks > 0 ? MonSpecies(&fullParty[order[0]]) : SPECIES_NONE),
        SpeciesName(picks > 1 ? MonSpecies(&fullParty[order[1]]) : SPECIES_NONE),
        SpeciesName(picks > 2 ? MonSpecies(&fullParty[order[2]]) : SPECIES_NONE),
        SpeciesName(picks > 3 ? MonSpecies(&fullParty[order[3]]) : SPECIES_NONE));
}

// ---------------------------------------------------------------------
// Turno
// ---------------------------------------------------------------------
static void PrintField(void)
{
    const char *weather = "nessuno", *terrain = "nessuno";
    u32 slowA = 0;
    enum BattlerId b;

    if (gBattleWeather & B_WEATHER_SUN) weather = "sole";
    else if (gBattleWeather & B_WEATHER_RAIN) weather = "pioggia";
    else if (gBattleWeather & B_WEATHER_SANDSTORM) weather = "sabbia";
    else if (gBattleWeather & (B_WEATHER_HAIL | B_WEATHER_SNOW)) weather = "neve";
    switch (gFieldTimers.terrain)
    {
    case B_TERRAIN_GRASSY: terrain = "erboso"; break;
    case B_TERRAIN_MISTY: terrain = "nebbioso"; break;
    case B_TERRAIN_ELECTRIC: terrain = "elettrico"; break;
    case B_TERRAIN_PSYCHIC: terrain = "psichico"; break;
    default: break;
    }
    for (b = 0; b < gBattlersCount; b++)
    {
        if (GetBattlerSide(b) == B_SIDE_OPPONENT && IsBattlerAlive(b))
        {
            slowA = AI_VgcTeamIsSlowerThanFoes(b, 2);
            break;
        }
    }
    Out("VGCLOG CAMPO meteo=%s terreno=%s trickroom=%d tailwindG=%d tailwindA=%d schermiG=%d schermiA=%d lentaA=%d",
        weather, terrain,
        (gFieldStatuses & STATUS_FIELD_TRICK_ROOM) ? gFieldTimers.trickRoomTimer : 0,
        (gSideStatuses[B_SIDE_PLAYER] & SIDE_STATUS_TAILWIND) ? gSideTimers[B_SIDE_PLAYER].tailwindTimer : 0,
        (gSideStatuses[B_SIDE_OPPONENT] & SIDE_STATUS_TAILWIND) ? gSideTimers[B_SIDE_OPPONENT].tailwindTimer : 0,
        gSideStatuses[B_SIDE_PLAYER] & (SIDE_STATUS_REFLECT | SIDE_STATUS_LIGHTSCREEN | SIDE_STATUS_AURORA_VEIL),
        gSideStatuses[B_SIDE_OPPONENT] & (SIDE_STATUS_REFLECT | SIDE_STATUS_LIGHTSCREEN | SIDE_STATUS_AURORA_VEIL),
        slowA);
}

static void PrintBattler(enum BattlerId b)
{
    if (!IsBattlerAlive(b))
    {
        Out("VGCLOG MON b%d %s vuoto panchina=%d", b, SideName(b), CountUsablePartyMons(b));
        return;
    }
    Out("VGCLOG MON b%d %s %S slot%d ps=%d/%d stato=%s stadi=%d,%d,%d,%d,%d vel=%d strum=%S abil=%S primoturno=%d wincon=%d panchina=%d encore=%S",
        b, SideName(b), SpeciesName(gBattleMons[b].species), gBattlerPartyIndexes[b], gBattleMons[b].hp, gBattleMons[b].maxHP,
        StatusName(gBattleMons[b].status1),
        gBattleMons[b].statStages[STAT_ATK] - DEFAULT_STAT_STAGE, gBattleMons[b].statStages[STAT_DEF] - DEFAULT_STAT_STAGE,
        gBattleMons[b].statStages[STAT_SPEED] - DEFAULT_STAT_STAGE, gBattleMons[b].statStages[STAT_SPATK] - DEFAULT_STAT_STAGE,
        gBattleMons[b].statStages[STAT_SPDEF] - DEFAULT_STAT_STAGE,
        GetBattlerTotalSpeedStat(b, gBattleMons[b].ability, GetBattlerHoldEffect(b)),
        ItemName(gBattleMons[b].item), AbilityName(gBattleMons[b].ability), IsBattlersFirstTurn(b),
        AI_VgcPlanIsWinCon(AI_TrainerIdOfBattler(b), gBattleMons[b].species), CountUsablePartyMons(b),
        MoveName(gBattleMons[b].volatiles.encoredMove));
}

// Danni stimati dall'IA (gAiLogicData di inizio turno): per ogni coppia attaccante -> difensore in campo, ogni mossa
// conosciuta con danno minimo-massimo, precisione ed efficacia (x100). Per i Pokémon del giocatore sono le mosse
// che l'IA conosce.
static void PrintDamage(void)
{
    enum BattlerId atk, def;
    u32 i;

    if (!IsDoubleBattle() && gBattlersCount < 2)
        return;
    for (atk = 0; atk < gBattlersCount; atk++)
    {
        enum Move *moves;
        if (!IsBattlerAlive(atk))
            continue;
        moves = GetMovesArray(atk);
        for (def = 0; def < gBattlersCount; def++)
        {
            struct LineBuf b;
            const char *head = "VGCLOG DMG b";
            bool32 any = FALSE;
            if (def == atk || !IsBattlerAlive(def))
                continue;
            b.n = 0;
            while (*head)
                PutC(&b, *head++);
            PutNum(&b, atk);
            PutC(&b, '>');
            PutC(&b, 'b');
            PutNum(&b, def);
            for (i = 0; i < MAX_MON_MOVES; i++)
            {
                u32 eff;
                if (moves[i] == MOVE_NONE || moves[i] >= MOVES_COUNT)
                    continue;
                any = TRUE;
                PutC(&b, ' ');
                PutC(&b, '|');
                PutC(&b, ' ');
                PutGameString(&b, GetMoveName(moves[i]));
                PutC(&b, ' ');
                PutNum(&b, gAiLogicData->simulatedDmg[atk][def][i].minimum);
                PutC(&b, '-');
                PutNum(&b, gAiLogicData->simulatedDmg[atk][def][i].maximum);
                PutC(&b, ' ');
                PutC(&b, 'a');
                PutNum(&b, gAiLogicData->moveAccuracy[atk][def][i]);
                PutC(&b, ' ');
                PutC(&b, 'e');
                eff = gAiLogicData->effectiveness[atk][def][i];
                PutNum(&b, eff * 100 / UQ_4_12(1.0));
            }
            if (!any)
                continue;
            b.s[b.n] = 0;
            Emit(b.s, b.n);
        }
    }
}

void VgcLog_TurnStart(void)
{
    enum BattlerId b;

    if (gBattleTurnCounter == 0)
    {
        // Inizio della lotta (anche senza team preview): si accende qui, poi la squadra in lotta.
        bool32 wasOn = IsOn();
        if (!TryStart())
            return;
        if (!wasOn)
            Out("VGCLOG INIZIO allenatore=%d %S", sVl.trainer, GetTrainerNameFromId(sVl.trainer));
        Out("VGCLOG LOTTA doppia=%d", IsDoubleBattle());
    }
    if (!IsOn())
        return;
    Out("VGCLOG TURNO %d", gBattleTurnCounter + 1);
    PrintField();
    for (b = 0; b < gBattlersCount; b++)
        PrintBattler(b);
    PrintDamage();
}

void VgcLog_ActionsChosen(void)
{
    enum BattlerId b;
    u32 i;
    if (!IsOn())
        return;
    for (b = 0; b < gBattlersCount; b++)
    {
        switch (gChosenActionByBattler[b])
        {
        case B_ACTION_USE_MOVE:
        {
            u32 move = gChosenMoveByBattler[b], target = gBattleStruct->moveTarget[b];
            char flags[12];
            const char *rel = "";
            if (!IsBattlerAlive(b))
                break;
            MoveFlags(b, move, flags);
            if (target < MAX_BATTLERS_COUNT)
                rel = (target == b) ? "se" : (IsBattlerAlly(b, target) ? "compagno" : "avversario");
            Out("VGCLOG AZIONE b%d %s %S MOSSA %S -> b%d %s cat=%s fx=%s idx=%d%s%s", b, SideName(b), SpeciesName(gBattleMons[b].species),
                MoveName(move), target, rel, CategoryName(move), flags, gBattleStruct->chosenMovePositions[b],
                (gBattleStruct->gimmick.toActivate & (1u << b)) ? " +MEGA" : "",
                (gBattleStruct->choicedMove[b] != MOVE_NONE && gBattleStruct->choicedMove[b] == move) ? " bloccata" : "");
            break;
        }
        case B_ACTION_SWITCH:
        {
            u32 to = gBattleStruct->monToSwitchIntoId[b];
            Out("VGCLOG AZIONE b%d %s %S CAMBIO -> %S (slot%d)", b, SideName(b), SpeciesName(gBattleMons[b].species),
                SpeciesName(to < PARTY_SIZE ? MonSpecies(&GetBattlerParty(b)[to]) : SPECIES_NONE), to);
            break;
        }
        case B_ACTION_USE_ITEM:
            Out("VGCLOG AZIONE b%d %s %S STRUMENTO", b, SideName(b), SpeciesName(gBattleMons[b].species));
            break;
        case B_ACTION_RUN:
            Out("VGCLOG AZIONE b%d %s FUGA", b, SideName(b));
            break;
        default:
            if (IsBattlerAlive(b))
                Out("VGCLOG AZIONE b%d %s %S NESSUNA", b, SideName(b), SpeciesName(gBattleMons[b].species));
            break;
        }
    }
    {
        struct LineBuf lb;
        const char *head = "VGCLOG ORDINE";
        lb.n = 0;
        while (*head)
            PutC(&lb, *head++);
        for (i = 0; i < gBattlersCount; i++)
        {
            PutC(&lb, ' ');
            PutC(&lb, 'b');
            PutNum(&lb, gBattlerByTurnOrder[i]);
        }
        lb.s[lb.n] = 0;
        Emit(lb.s, lb.n);
    }
}

void VgcLog_Message(const u8 *string)
{
    if (!IsOn())
        return;
    if (string != NULL && string[0] != EOS)
        Out("VGCLOG MSG %S", string);
    // Esito deciso: la partita finisce qui (nel self-play il test esce subito dopo questo messaggio).
    if (gBattleOutcome != 0)
        VgcLog_BattleEnd();
}

void VgcLog_HP(u32 battler, s32 oldHP, s32 newHP, s32 maxHP)
{
    if (!IsOn() || battler >= MAX_BATTLERS_COUNT || oldHP == newHP)
        return;
    Out("VGCLOG PS b%d %s %S %d -> %d / %d", battler, SideName(battler), SpeciesName(gBattleMons[battler].species), oldHP, newHP, maxHP);
    if (newHP == 0 && oldHP > 0)
        Out("VGCLOG KO b%d %s %S t%d", battler, SideName(battler), SpeciesName(gBattleMons[battler].species), gBattleTurnCounter + 1);
}

void VgcLog_AbilityPopUp(u32 battler, u32 ability)
{
    if (!IsOn() || battler >= MAX_BATTLERS_COUNT)
        return;
    Out("VGCLOG ABILITA b%d %s %S %S", battler, SideName(battler), SpeciesName(gBattleMons[battler].species), AbilityName(ability));
}

void VgcLog_BattleEnd(void)
{
    const char *outcome;
    u32 left[NUM_BATTLE_SIDES] = {0};
    u32 side, i;

    if (!IsOn())
        return;
    switch (gBattleOutcome & 0x7F)
    {
    case B_OUTCOME_WON:  outcome = "VITTORIA_G"; break;
    case B_OUTCOME_LOST: outcome = "VITTORIA_A"; break;
    case B_OUTCOME_DREW: outcome = "PAREGGIO"; break;
    case B_OUTCOME_RAN:  outcome = "FUGA"; break;
    default:             outcome = "ALTRO"; break;
    }
    for (side = 0; side < NUM_BATTLE_SIDES; side++)
    {
        struct Pokemon *party = gParties[side == B_SIDE_PLAYER ? B_TRAINER_PLAYER : B_TRAINER_OPPONENT_A];
        for (i = 0; i < PARTY_SIZE; i++)
            if (MonSpecies(&party[i]) != SPECIES_NONE && GetMonData(&party[i], MON_DATA_HP) > 0)
                left[side]++;
    }
    Out("VGCLOG FINE esito=%s turni=%d restantiG=%d restantiA=%d", outcome, gBattleTurnCounter + 1, left[B_SIDE_PLAYER], left[B_SIDE_OPPONENT]);
    sVl.ended = TRUE;
}

// ---------------------------------------------------------------------
// Pianificatore
// ---------------------------------------------------------------------
static void PutAction(struct LineBuf *b, u32 kind, u32 move, u32 user, u32 target, s32 party)
{
    switch (kind)
    {
    case 0: // PLAN_ACT_MOVE
    case 1: // PLAN_ACT_PROTECT
        PutGameString(b, MoveName(move));
        if (kind == 0)
        {
            PutC(b, '>');
            PutC(b, 'b');
            PutNum(b, target);
        }
        break;
    case 2: // PLAN_ACT_SWITCH
    {
        const char *s = "CAMBIO>";
        while (*s)
            PutC(b, *s++);
        if (user < MAX_BATTLERS_COUNT && party >= 0 && party < PARTY_SIZE)
            PutGameString(b, SpeciesName(MonSpecies(&GetBattlerParty(user)[party])));
        else
            PutC(b, '?');
        break;
    }
    default:
        PutC(b, '-');
        break;
    }
}

static void PutStr(struct LineBuf *b, const char *s)
{
    while (*s)
        PutC(b, *s++);
}

void VgcLog_PlanHead(u32 me, u32 nPairs, u32 bestIdx, u32 rulesIdx, u32 frames, u32 sims)
{
    if (!IsOn())
        return;
    Out("VGCLOG PIANO b%d %S T%d coppie=%d scelta=%d regole=%d fotogrammi=%d simulazioni=%d", me, SpeciesName(gBattleMons[me].species),
        gBattleTurnCounter + 1, nPairs, bestIdx, rulesIdx < nPairs ? (s32)rulesIdx : -1, frames, sims);
}

void VgcLog_PlanPair(u32 me, u32 idx, u32 kindA, u32 moveA, u32 userA, u32 targetA, s32 partyA,
                     u32 kindB, u32 moveB, u32 userB, u32 targetB, s32 partyB, s32 value, u32 mark)
{
    struct LineBuf b;
    if (!IsOn())
        return;
    b.n = 0;
    PutStr(&b, "VGCLOG COPPIA b");
    PutNum(&b, me);
    PutStr(&b, " #");
    PutNum(&b, idx);
    PutStr(&b, ": b");
    PutNum(&b, userA);
    PutC(&b, ' ');
    PutAction(&b, kindA, moveA, userA, targetA, partyA);
    PutStr(&b, " + b");
    PutNum(&b, userB);
    PutC(&b, ' ');
    PutAction(&b, kindB, moveB, userB, targetB, partyB);
    PutStr(&b, " = ");
    PutNum(&b, value);
    if (mark == 1)
        PutStr(&b, " <==");
    else if (mark == 2)
        PutStr(&b, " (regole)");
    else if (mark == 3)
        PutStr(&b, " <== (regole)");
    b.s[b.n] = 0;
    Emit(b.s, b.n);
}

void VgcLog_PlanFoe(u32 me, u32 slot, u32 idx, u32 kind, u32 move, u32 user, u32 target, s32 party, s32 weight)
{
    struct LineBuf b;
    if (!IsOn())
        return;
    b.n = 0;
    PutStr(&b, "VGCLOG PREVISTA b");
    PutNum(&b, me);
    PutC(&b, ' ');
    PutC(&b, slot ? 'B' : 'A');
    PutNum(&b, idx);
    PutStr(&b, ": b");
    PutNum(&b, user);
    PutC(&b, ' ');
    PutAction(&b, kind, move, user, target, party);
    PutStr(&b, " peso=");
    PutNum(&b, weight);
    b.s[b.n] = 0;
    Emit(b.s, b.n);
}
