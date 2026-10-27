// Team preview stile VGC: scegli 4 Pokémon, ordine = lead.
// L'AI avversaria sceglie i suoi 4 + lead con un punteggio di matchup.
#include "global.h"
// gPlayerParty/gEnemyParty: deprecati nelle expansion nuove ma vanno anche nelle vecchie.
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "battle.h"
#include "battle_main.h"
#include "battle_setup.h"
#include "battle_util.h"
#include "bg.h"
#include "data.h"
#include "event_data.h"
#include "gpu_regs.h"
#include "item.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "move.h"
#include "palette.h"
#include "pokemon.h"
#include "pokemon_icon.h"
#include "pokemon_storage_system.h"
#include "pokemon_summary_screen.h"
#include "random.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "script_pokemon_util.h"
#include "battle_ai_main.h"
#include "battle_ai_vgc.h"
#include "battle_ai_vgc_book.h"
#include "battle_ai_vgc_graph.h"
#include "battle_ai_scout.h"
#include "task.h"
#include "text.h"
#include "text_window.h"
#include "window.h"
#include "team_preview.h"
#include "config/custom.h"
#include "constants/abilities.h"
#include "constants/battle_ai.h"
#include "constants/hold_effects.h"
#include "constants/moves.h"
#include "constants/rgb.h"
#include "constants/songs.h"

// ---------------------------------------------------------------------
// Stato
// ---------------------------------------------------------------------
enum
{
    MENU_NONE,
    MENU_ACTION,   // Info / Scegli / Annulla
    MENU_CONFIRM,  // Lotta! / Indietro
    MENU_FORMAT,   // Bo1 / Bo3
};

enum
{
    WIN_HEADER,
    WIN_PLAYER,
    WIN_ENEMY,
    WIN_MENU,
    WIN_COUNT,
};

#define ROW_H        24
#define LIST_TOP_Y   16
#define FRAME_TILE   0x2F0
#define FRAME_PAL    14
#define MENU_PAL     15

struct TeamPreviewState
{
    u8 cursor;
    u8 pickOrder[PARTY_SIZE];   // 0 = non scelto, 1..4 = ordine
    u8 numPicks;
    u8 required;
    u8 menu;
    u8 menuCursor;
    u8 iconIds[2][PARTY_SIZE];
    u8 enemyOrder[PARTY_SIZE];  // risultato AI: slot originali in ordine
    u8 enemyPicks;
    bool8 aiDone;
    bool8 active;
    bool8 warn;                 // START premuto senza aver scelto tutti
};

static EWRAM_DATA struct TeamPreviewState sTP = {0};
static EWRAM_DATA struct Pokemon sSavedParty[PARTY_SIZE] = {0};
static EWRAM_DATA u8 sSavedOrder[PARTY_SIZE] = {0};   // nuovo slot -> slot originale
static EWRAM_DATA u8 sSavedPicks = 0;
static EWRAM_DATA bool8 sPartyReduced = FALSE;
// Ultima scelta fatta (per SELECT): si riconoscono i Pokémon dalla personalità.
static EWRAM_DATA u32 sLastPickPersonality[PARTY_SIZE] = {0};
static EWRAM_DATA u8 sLastPickCount = 0;

// --- Al meglio di tre -------------------------------------------------
struct SeriesState
{
    u8 game;        // 0 = nessuna serie in corso; altrimenti numero della gara (1..3)
    u8 bestOf;      // 1 o 3
    u8 playerWins;
    u8 enemyWins;
    u16 trainerId;
    u32 battleTypeFlags;
    MainCallback savedCallback;
    // Storia delle gare della serie: cosa ha portato l'AI e com'è andata, per non ripetere
    // un piano che ha perso (e insistere su uno che ha vinto).
    struct
    {
        u8 valid;
        u8 aiWon;
        u8 enemyLead[2];    // slot originali (nel party completo) dei due lead dell'AI
        u8 enemyMask;       // bitmask degli slot originali portati
    } history[3];
};

// Ciò che l'AI ricorda di un nostro Pokémon dalle gare precedenti (riconosciuto dalla personalità).
struct SeriesMonMemory
{
    u32 personality;
    u16 moves[MAX_MON_MOVES];   // mosse viste
    u16 ability;                // ABILITY_NONE = non vista
    u16 item;                   // ITEM_NONE = non visto
    u16 heldEffect;             // HOLD_EFFECT_* visto o dedotto (es. Stolascelta dalla velocità)
    u8 timesBrought;
    u8 timesLead;
    u8 kos;                     // quanti Pokémon dell'AI ha mandato KO
    struct ScoutBeliefSnapshot belief; // Velocità ristretta, strumenti esclusi, abilità d'ingresso escluse
};

static EWRAM_DATA struct SeriesState sSeries = {0};
static EWRAM_DATA struct Pokemon sSeriesEnemyParty[PARTY_SIZE] = {0}; // squadra avversaria completa
static EWRAM_DATA struct SeriesMonMemory sSeriesMemory[PARTY_SIZE] = {0};
static EWRAM_DATA u8 sSeriesMemoryCount = 0;
// Formato già scelto nel dialogo prima della lotta (0 = va chiesto nella team preview).
static EWRAM_DATA u8 sPendingBestOf = 0;

// special: VAR_0x8004 = 1 (Bo1) oppure 3 (Bo3). Da chiamare nello script prima di trainerbattle.
void TeamPreview_SetFormatFromScript(void)
{
    sPendingBestOf = (gSpecialVar_0x8004 == 3) ? 3 : 1;
}

static bool32 SeriesIsBestOfThree(void)
{
    return sSeries.game != 0 && sSeries.bestOf == 3;
}

static struct SeriesMonMemory *SeriesFindMemory(u32 personality)
{
    u32 i;

    if (!SeriesIsBestOfThree())
        return NULL;
    for (i = 0; i < sSeriesMemoryCount; i++)
    {
        if (sSeriesMemory[i].personality == personality)
            return &sSeriesMemory[i];
    }
    return NULL;
}

static struct SeriesMonMemory *SeriesFindOrAddMemory(u32 personality)
{
    struct SeriesMonMemory *mem = SeriesFindMemory(personality);

    if (mem != NULL || !SeriesIsBestOfThree() || sSeriesMemoryCount >= PARTY_SIZE)
        return mem;
    mem = &sSeriesMemory[sSeriesMemoryCount++];
    memset(mem, 0, sizeof(*mem));
    mem->personality = personality;
    return mem;
}

static void SeriesRememberMove(struct SeriesMonMemory *mem, u16 move)
{
    u32 i;

    if (move == MOVE_NONE)
        return;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (mem->moves[i] == move)
            return;
    }
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (mem->moves[i] == MOVE_NONE)
        {
            mem->moves[i] = move;
            return;
        }
    }
}
static EWRAM_DATA u16 *sTilemaps[2] = {NULL};
// Squadra avversaria com'era in anteprima (serve alla schermata Info lotta).
static EWRAM_DATA bool8 sEnemyPreviewValid = FALSE;
static EWRAM_DATA u16 sEnemyPreviewSpecies[PARTY_SIZE] = {0};
static EWRAM_DATA u8 sEnemyPreviewBattleSlot[PARTY_SIZE] = {0}; // slot in lotta, 0xFF = non portato

static void CB2_TeamPreviewInit(void);
static void CB2_TeamPreviewMain(void);
static void VBlankCB_TeamPreview(void);
static void CB2_ReturnFromSummary(void);
static void Task_TeamPreviewInput(u8 taskId);
static void Task_TeamPreviewExitToSummary(u8 taskId);
static void Task_TeamPreviewExitToBattle(u8 taskId);
static void DrawAll(void);
static void DrawMenu(void);
static void FreeScreen(void);
static void ApplyPartySelections(void);

// ---------------------------------------------------------------------
// Testi (cambia qui per tradurre)
// ---------------------------------------------------------------------
static const u8 sText_Header[]   = _("SCEGLI {STR_VAR_1} POKéMON");
static const u8 sText_Hint[]     = _("A:Menu  SEL:Ultimi 4");
static const u8 sText_Ready[]    = _("START: Lotta!");
static const u8 sText_Missing[]  = _("Scegline ancora {STR_VAR_1}!");
static const u8 sText_Info[]     = _("Info");
static const u8 sText_Pick[]     = _("Scegli");
static const u8 sText_Unpick[]   = _("Togli");
static const u8 sText_Cancel[]   = _("Annulla");
static const u8 sText_Fight[]    = _("Lotta!");
static const u8 sText_Back[]     = _("Indietro");
static const u8 sText_Bo1[]      = _("Bo1");
static const u8 sText_Bo3[]      = _("Bo3");
static const u8 sText_FormatHint[] = _("Formato: 1 lotta o meglio di 3?");
static const u8 sText_SeriesHeader[] = _("G{STR_VAR_1} {STR_VAR_2}-{STR_VAR_3}  SCEGLI ");
static const u8 sText_Space[]    = _(" ");
static const u8 sText_KO[]       = _("KO");
static const u8 sText_Egg[]      = _("UOVO");
static const u8 sText_Cursor[]   = _("▶");
static const u8 sText_Iper[]     = _("{UP_ARROW}{UP_ARROW}");
static const u8 sText_Super[]    = _("{UP_ARROW}");
static const u8 sText_Resist[]   = _("{DOWN_ARROW}");
static const u8 sText_Resist4[]  = _("{DOWN_ARROW}{DOWN_ARROW}");
static const u8 sText_Immune[]   = _("X");
static const u8 sText_Slash[]    = _("/");

// ---------------------------------------------------------------------
// Grafica
// ---------------------------------------------------------------------
// Colori ispirati al menu squadra: schede blu (noi) e rosse (loro), selezione arancio.
enum
{
    COL_BG,          // sfondo (trasparente nelle finestre)
    COL_WHITE,
    COL_SHADOW,
    COL_P_FILL,      // scheda giocatore
    COL_P_BORDER,
    COL_P_LIGHT,
    COL_E_FILL,      // scheda avversario
    COL_E_BORDER,
    COL_E_LIGHT,
    COL_SEL_FILL,    // scheda sotto il cursore
    COL_ORANGE,
    COL_YELLOW,
    COL_HEADER,
    COL_GREY,
    COL_GREEN,
    COL_CYAN,
};

static const u16 sPreviewPal[16] =
{
    [COL_BG]       = RGB(5, 9, 13),
    [COL_WHITE]    = RGB(31, 31, 31),
    [COL_SHADOW]   = RGB(4, 5, 8),
    [COL_P_FILL]   = RGB(9, 17, 27),
    [COL_P_BORDER] = RGB(4, 9, 17),
    [COL_P_LIGHT]  = RGB(16, 24, 31),
    [COL_E_FILL]   = RGB(25, 10, 10),
    [COL_E_BORDER] = RGB(14, 4, 5),
    [COL_E_LIGHT]  = RGB(31, 17, 16),
    [COL_SEL_FILL] = RGB(13, 22, 31),
    [COL_ORANGE]   = RGB(31, 19, 4),
    [COL_YELLOW]   = RGB(31, 29, 10),
    [COL_HEADER]   = RGB(2, 4, 8),
    [COL_GREY]     = RGB(17, 18, 20),
    [COL_GREEN]    = RGB(12, 28, 12),
    [COL_CYAN]     = RGB(18, 28, 31),
};

// {sfondo, testo, ombra}: sfondo 0 = trasparente, si vede la scheda sotto.
static const u8 sColWhite[]   = {COL_BG, COL_WHITE, COL_SHADOW};
static const u8 sColGrey[]    = {COL_BG, COL_GREY, COL_SHADOW};
static const u8 sColYellow[]  = {COL_BG, COL_YELLOW, COL_SHADOW};
static const u8 sColOrange[]  = {COL_BG, COL_ORANGE, COL_SHADOW};
static const u8 sColGreen[]   = {COL_BG, COL_GREEN, COL_SHADOW};
static const u8 sColCyan[]    = {COL_BG, COL_CYAN, COL_SHADOW};
static const u8 sColNumber[]  = {COL_BG, COL_SHADOW, COL_BG};
static const u8 sColMenu[]    = {TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY};

#define ICON_X       17
#define TEXT_X       34
#define CARD_X       1
#define CARD_W       118
#define CARD_H       22

static const struct BgTemplate sBgTemplates[] =
{
    { .bg = 0, .charBaseIndex = 0, .mapBaseIndex = 30, .screenSize = 0, .paletteMode = 0, .priority = 2, .baseTile = 0 },
    { .bg = 1, .charBaseIndex = 0, .mapBaseIndex = 31, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0 },
};

static const struct WindowTemplate sWinTemplates[] =
{
    [WIN_HEADER] = { .bg = 0, .tilemapLeft = 0,  .tilemapTop = 0, .width = 30, .height = 2,  .paletteNum = 0, .baseBlock = 1 },
    [WIN_PLAYER] = { .bg = 0, .tilemapLeft = 0,  .tilemapTop = 2, .width = 15, .height = 18, .paletteNum = 0, .baseBlock = 61 },
    [WIN_ENEMY]  = { .bg = 0, .tilemapLeft = 15, .tilemapTop = 2, .width = 15, .height = 18, .paletteNum = 0, .baseBlock = 331 },
    [WIN_MENU]   = { .bg = 1, .tilemapLeft = 21, .tilemapTop = 13, .width = 8,  .height = 6,  .paletteNum = MENU_PAL, .baseBlock = 601 },
    DUMMY_WIN_TEMPLATE,
};

// ---------------------------------------------------------------------
// Utilità mon
// ---------------------------------------------------------------------
static bool32 IsPlayerMon(struct Pokemon *mon)
{
    return mon >= gPlayerParty && mon < gPlayerParty + PARTY_SIZE;
}

// Contesto della squadra del giocatore (le sue 6 specie, vedi Scout_SetTeamContextFromParty): calcolato all'inizio della
// scelta dell'IA, ritrovato per contenuto (anche dopo che la squadra è stata ridotta ai 4 portati).
static const struct ScoutTeamContext *PlayerContext(void)
{
    return gScoutContextPreviewOff ? NULL : Scout_PartyContext(gPlayerParty);
}

// Statistica di un Pokémon: vera per i nostri, stimata da specie e livello per quelli del giocatore
// (in team preview si vedono solo specie e livello), con la spread che ha senso nella sua squadra (contesto).
static u32 PreviewStat(struct Pokemon *mon, enum MonData id)
{
    u16 hp, stats[NUM_STATS];

    if (!IsPlayerMon(mon))
        return GetMonData(mon, id);
    Scout_EstimateStatsInTeam(PlayerContext(), GetMonData(mon, MON_DATA_SPECIES), GetMonData(mon, MON_DATA_LEVEL), &hp, stats);
    switch (id)
    {
    case MON_DATA_MAX_HP: return hp;
    case MON_DATA_ATK:    return stats[STAT_ATK];
    case MON_DATA_DEF:    return stats[STAT_DEF];
    case MON_DATA_SPATK:  return stats[STAT_SPATK];
    case MON_DATA_SPDEF:  return stats[STAT_SPDEF];
    case MON_DATA_SPEED:  return stats[STAT_SPEED];
    default:              return GetMonData(mon, id);
    }
}

// Abilità: vera per i nostri; per il giocatore solo se la specie ne ha una sola possibile.
static enum Ability PreviewAbility(struct Pokemon *mon)
{
    if (!IsPlayerMon(mon))
        return GetMonAbility(mon);
    return Scout_KnownAbility(GetMonData(mon, MON_DATA_SPECIES));
}

static bool32 IsMonUsable(struct Pokemon *mon)
{
    return GetMonData(mon, MON_DATA_SPECIES) != SPECIES_NONE
        && !GetMonData(mon, MON_DATA_IS_EGG)
        && GetMonData(mon, MON_DATA_HP) != 0;
}

static u32 CountUsable(struct Pokemon *party)
{
    u32 i, n = 0;
    for (i = 0; i < PARTY_SIZE; i++)
        if (IsMonUsable(&party[i]))
            n++;
    return n;
}

static enum Type MonType(struct Pokemon *mon, u32 slot)
{
    return GetSpeciesType(GetMonData(mon, MON_DATA_SPECIES), slot);
}

static bool32 MonHasType(struct Pokemon *mon, enum Type type)
{
    return MonType(mon, 0) == type || MonType(mon, 1) == type;
}

// Moltiplicatore x100 (0, 25, 50, 100, 200, 400) solo per tipi.
static u32 TypeVsTypesPct(enum Type atkType, enum Type def1, enum Type def2)
{
    u32 m = GetTypeModifier(atkType, def1);
    if (def2 != def1)
        m = (m * GetTypeModifier(atkType, def2)) >> 12;
    return (m * 100) >> 12;
}

// Come sopra ma conta anche abilità che annullano/dimezzano (per l'AI).
static u32 TypeVsMonPct(enum Type atkType, struct Pokemon *def)
{
    u32 pct = TypeVsTypesPct(atkType, MonType(def, 0), MonType(def, 1));
    switch (PreviewAbility(def))
    {
    case ABILITY_LEVITATE:
    case ABILITY_EARTH_EATER:
        if (atkType == TYPE_GROUND) return 0;
        break;
    case ABILITY_FLASH_FIRE:
    case ABILITY_WELL_BAKED_BODY:
        if (atkType == TYPE_FIRE) return 0;
        break;
    case ABILITY_WATER_ABSORB:
    case ABILITY_STORM_DRAIN:
    case ABILITY_DRY_SKIN:
        if (atkType == TYPE_WATER) return 0;
        break;
    case ABILITY_VOLT_ABSORB:
    case ABILITY_LIGHTNING_ROD:
    case ABILITY_MOTOR_DRIVE:
        if (atkType == TYPE_ELECTRIC) return 0;
        break;
    case ABILITY_SAP_SIPPER:
        if (atkType == TYPE_GRASS) return 0;
        break;
    case ABILITY_WONDER_GUARD:
        if (pct < 200) return 0;
        break;
    case ABILITY_THICK_FAT:
        if (atkType == TYPE_FIRE || atkType == TYPE_ICE) return pct / 2;
        break;
    default:
        break;
    }
    return pct;
}

// Efficacia per la UI: il migliore dei tipi STAB del nostro mon contro il nemico.
static u32 PreviewEffectiveness(struct Pokemon *atk, struct Pokemon *def)
{
    enum Type t1 = MonType(atk, 0), t2 = MonType(atk, 1);
    enum Type d1 = MonType(def, 0), d2 = MonType(def, 1);
    u32 a = TypeVsTypesPct(t1, d1, d2);
    u32 b = (t2 != t1) ? TypeVsTypesPct(t2, d1, d2) : 0;
    return max(a, b);
}

// ---------------------------------------------------------------------
// AI: stima danno e scelta squadra
// ---------------------------------------------------------------------
// Danno stimato in % degli HP max del difensore (cap 300).
static u32 CalcPct(struct Pokemon *atk, struct Pokemon *def, u32 power, enum Type type, bool32 physical)
{
    u32 level = GetMonData(atk, MON_DATA_LEVEL);
    u32 a = PreviewStat(atk, physical ? MON_DATA_ATK : MON_DATA_SPATK);
    u32 d = PreviewStat(def, physical ? MON_DATA_DEF : MON_DATA_SPDEF);
    u32 hp = PreviewStat(def, MON_DATA_MAX_HP);
    u32 dmg;

    if (d == 0) d = 1;
    if (hp == 0) hp = 1;
    dmg = ((2 * level / 5 + 2) * power * a / d) / 50 + 2;
    dmg = dmg * TypeVsMonPct(type, def) / 100;
    if (MonHasType(atk, type))
        dmg = dmg * 3 / 2;
    dmg = dmg * 92 / 100; // tiro medio
    dmg = dmg * 100 / hp;
    return min(dmg, 300);
}

static u32 BestMovePct(struct Pokemon *atk, struct Pokemon *def)
{
    u32 i, best = 0;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        enum Move move = GetMonData(atk, MON_DATA_MOVE1 + i);
        u32 power;
        enum DamageCategory cat;
        if (move == MOVE_NONE)
            continue;
        cat = GetMoveCategory(move);
        if (cat == DAMAGE_CATEGORY_STATUS)
            continue;
        power = GetMovePower(move);
        if (power <= 1)
            power = 60; // danno fisso / variabile
        best = max(best, CalcPct(atk, def, power, GetMoveType(move), cat == DAMAGE_CATEGORY_PHYSICAL));
    }
    return best;
}

// Senza onniscienza: l'AI vede solo specie/tipi (come una vera team preview).
static u32 BestStabPct(struct Pokemon *atk, struct Pokemon *def)
{
    bool32 physical = PreviewStat(atk, MON_DATA_ATK) >= PreviewStat(atk, MON_DATA_SPATK);
    u32 a = CalcPct(atk, def, 80, MonType(atk, 0), physical);
    u32 b = CalcPct(atk, def, 80, MonType(atk, 1), physical);
    return max(a, b);
}

// Come BestMovePct ma solo sulle mosse che l'AI ha visto usare.
static u32 BestRememberedMovePct(struct Pokemon *atk, struct Pokemon *def, const struct SeriesMonMemory *mem)
{
    u32 i, best = 0;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        enum Move move = mem->moves[i];
        u32 power;
        enum DamageCategory cat;

        if (move == MOVE_NONE)
            continue;
        cat = GetMoveCategory(move);
        if (cat == DAMAGE_CATEGORY_STATUS)
            continue;
        power = GetMovePower(move);
        if (power <= 1)
            power = 60;
        best = max(best, CalcPct(atk, def, power, GetMoveType(move), cat == DAMAGE_CATEGORY_PHYSICAL));
    }
    return best;
}

// Senza vedere le mosse: il danno peggiore tra quelle che la specie PUÒ imparare. Le STAB contano
// per intero (le porta quasi sempre), le coperture al 65% (può non averle).
static u32 BestEstimatedPct(struct Pokemon *atk, struct Pokemon *def)
{
    enum Move moves[24];
    u32 n = Scout_CollectLearnableAttacks(GetMonData(atk, MON_DATA_SPECIES), moves, ARRAY_COUNT(moves));
    u32 i, best = 0;

    for (i = 0; i < n; i++)
    {
        enum Type type = GetMoveType(moves[i]);
        u32 pct = CalcPct(atk, def, GetMovePower(moves[i]), type, GetMoveCategory(moves[i]) == DAMAGE_CATEGORY_PHYSICAL);
        if (!MonHasType(atk, type))
            pct = pct * 65 / 100;
        best = max(best, pct);
    }
    return best;
}

static u32 HitsToKo(u32 pct)
{
    if (pct == 0)
        return 9;
    return min((100 + pct - 1) / pct, 9);
}

static bool32 MonKnowsEffect(struct Pokemon *mon, enum BattleMoveEffects effect)
{
    u32 i;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        enum Move move = GetMonData(mon, MON_DATA_MOVE1 + i);
        if (move != MOVE_NONE && GetMoveEffect(move) == effect)
            return TRUE;
    }
    return FALSE;
}

static bool32 MonKnowsMove(struct Pokemon *mon, enum Move target)
{
    u32 i;
    for (i = 0; i < MAX_MON_MOVES; i++)
        if (GetMonData(mon, MON_DATA_MOVE1 + i) == target)
            return TRUE;
    return FALSE;
}

static bool32 HasFakeOut(struct Pokemon *mon)
{
    return MonKnowsMove(mon, MOVE_FAKE_OUT);
}

static bool32 HoldsMegaStone(struct Pokemon *mon)
{
    return GetItemHoldEffect(GetMonData(mon, MON_DATA_HELD_ITEM)) == HOLD_EFFECT_MEGA_STONE;
}

static u32 BaseSpeed(struct Pokemon *mon)
{
    return gSpeciesInfo[GetMonData(mon, MON_DATA_SPECIES)].baseSpeed;
}

// Valore come lead (ruolo), indipendente dal partner.
static s32 LeadRoleBonus(struct Pokemon *mon, bool32 isDouble, bool32 foeHasFakeOut)
{
    s32 s = 0;
    enum Ability ability = GetMonAbility(mon);

    if (ability == ABILITY_INTIMIDATE)
        s += isDouble ? 30 : 15;
    if (isDouble)
    {
        if (HasFakeOut(mon))                         s += 35;
        if (MonKnowsEffect(mon, EFFECT_TAILWIND))    s += 25;
        if (MonKnowsEffect(mon, EFFECT_TRICK_ROOM))  s += 25;
        if (MonKnowsEffect(mon, EFFECT_FOLLOW_ME))   s += 25;
        if (MonKnowsEffect(mon, EFFECT_PROTECT))     s += 10;
        if (MonKnowsMove(mon, MOVE_ICY_WIND) || MonKnowsMove(mon, MOVE_ELECTROWEB)
         || MonKnowsMove(mon, MOVE_THUNDER_WAVE))
            s += 15;
        if (ability == ABILITY_PRANKSTER)
            s += 15;
    }
    switch (ability)
    {
    case ABILITY_DRIZZLE: case ABILITY_DROUGHT: case ABILITY_SAND_STREAM: case ABILITY_SNOW_WARNING:
    case ABILITY_ELECTRIC_SURGE: case ABILITY_GRASSY_SURGE: case ABILITY_PSYCHIC_SURGE: case ABILITY_MISTY_SURGE:
    case ABILITY_ORICHALCUM_PULSE: case ABILITY_HADRON_ENGINE:
        s += 20;
        break;
    default:
        break;
    }
    // Dal file: non leadare la Mega contro chi ha Fake Out.
    if (HoldsMegaStone(mon) && foeHasFakeOut)
        s -= 30;
    return s;
}

static bool32 BenefitsFromWeather(struct Pokemon *mon, enum Ability setter)
{
    enum Ability a = GetMonAbility(mon);
    switch (setter)
    {
    case ABILITY_DRIZZLE:
        return a == ABILITY_SWIFT_SWIM || a == ABILITY_RAIN_DISH || MonHasType(mon, TYPE_WATER);
    case ABILITY_DROUGHT:
    case ABILITY_ORICHALCUM_PULSE:
        return a == ABILITY_CHLOROPHYLL || a == ABILITY_SOLAR_POWER || a == ABILITY_PROTOSYNTHESIS || MonHasType(mon, TYPE_FIRE);
    case ABILITY_SAND_STREAM:
        return a == ABILITY_SAND_RUSH || a == ABILITY_SAND_FORCE || MonHasType(mon, TYPE_ROCK);
    case ABILITY_SNOW_WARNING:
        return a == ABILITY_SLUSH_RUSH || MonKnowsMove(mon, MOVE_BLIZZARD) || MonKnowsMove(mon, MOVE_AURORA_VEIL);
    case ABILITY_ELECTRIC_SURGE:
    case ABILITY_HADRON_ENGINE:
        return a == ABILITY_SURGE_SURFER || a == ABILITY_QUARK_DRIVE || MonHasType(mon, TYPE_ELECTRIC);
    case ABILITY_PSYCHIC_SURGE:
        return MonHasType(mon, TYPE_PSYCHIC) || MonKnowsMove(mon, MOVE_EXPANDING_FORCE);
    case ABILITY_GRASSY_SURGE:
        return MonHasType(mon, TYPE_GRASS);
    default:
        return FALSE;
    }
}

static enum Ability MonWeatherAbility(struct Pokemon *mon)
{
    u16 item = GetMonData(mon, MON_DATA_HELD_ITEM);
    if (item == ITEM_CHARIZARDITE_Y)
        return ABILITY_DROUGHT;
    if (item == ITEM_TYRANITARITE)
        return ABILITY_SAND_STREAM;
    return GetMonAbility(mon);
}

static bool32 MonKnowsSetupMove(struct Pokemon *mon)
{
    u32 i;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        enum Move move = GetMonData(mon, MON_DATA_MOVE1 + i);
        if (move != MOVE_NONE && Scout_MoveHasRole(move, SCOUT_SETUP))
            return TRUE;
    }
    return FALSE;
}

static bool32 MonIsSupport(struct Pokemon *mon)
{
    enum Ability ab = GetMonAbility(mon);
    if (MonKnowsEffect(mon, EFFECT_TRICK_ROOM)
     || MonKnowsEffect(mon, EFFECT_TAILWIND)
     || MonKnowsEffect(mon, EFFECT_FOLLOW_ME)
     || MonKnowsEffect(mon, EFFECT_HELPING_HAND)
     || HasFakeOut(mon)
     || MonKnowsMove(mon, MOVE_SPORE)
     || MonKnowsMove(mon, MOVE_QUICK_GUARD)
     || MonKnowsMove(mon, MOVE_WIDE_GUARD)
     || ab == ABILITY_INTIMIDATE
     || ab == ABILITY_PRANKSTER)
        return TRUE;
    return FALSE;
}

// Supporto "puro": fa supporto e ha al massimo una mossa d'attacco (Fake Out esclusa).
static bool32 MonIsPureSupport(struct Pokemon *mon)
{
    u32 i, attacks = 0;

    if (!MonIsSupport(mon))
        return FALSE;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        enum Move move = GetMonData(mon, MON_DATA_MOVE1 + i);
        if (move != MOVE_NONE && !IsBattleMoveStatus(move) && GetMoveEffect(move) != EFFECT_FIRST_TURN_ONLY)
            attacks++;
    }
    return attacks <= 1;
}

static s32 PairSynergy(struct Pokemon *a, struct Pokemon *b)
{
    s32 s = 0;
    u32 t;
    bool32 aTr = MonKnowsEffect(a, EFFECT_TRICK_ROOM);
    bool32 bTr = MonKnowsEffect(b, EFFECT_TRICK_ROOM);
    bool32 aTw = MonKnowsEffect(a, EFFECT_TAILWIND);
    bool32 bTw = MonKnowsEffect(b, EFFECT_TAILWIND);
    bool32 aFo = HasFakeOut(a);
    bool32 bFo = HasFakeOut(b);
    bool32 aRedir = MonKnowsEffect(a, EFFECT_FOLLOW_ME);
    bool32 bRedir = MonKnowsEffect(b, EFFECT_FOLLOW_ME);
    bool32 aHh = MonKnowsEffect(a, EFFECT_HELPING_HAND);
    bool32 bHh = MonKnowsEffect(b, EFFECT_HELPING_HAND);
    bool32 aSetup = MonKnowsSetupMove(a);
    bool32 bSetup = MonKnowsSetupMove(b);

    // Trick Room + partner lento
    if (aTr && BaseSpeed(b) <= 60) s += 120;
    if (bTr && BaseSpeed(a) <= 60) s += 120;
    // Trick Room + partner veloce che odia la Distorsione
    if (aTr && BaseSpeed(b) >= 105 && !bTr) s -= 80;
    if (bTr && BaseSpeed(a) >= 105 && !aTr) s -= 80;

    // Trick Room + Tailwind insieme = conflitto
    if ((aTr && bTw) || (bTr && aTw))
        s -= 150;

    // Tailwind + attaccante
    if (aTw && BaseSpeed(b) >= 70) s += 60;
    if (bTw && BaseSpeed(a) >= 70) s += 60;

    // Meteo / terreno + chi ne beneficia
    if (BenefitsFromWeather(b, MonWeatherAbility(a))) s += 120;
    if (BenefitsFromWeather(a, MonWeatherAbility(b))) s += 120;

    // Fake Out / redirect + setup / speed control
    if ((aFo || aRedir) && (bTr || bTw || bSetup))
        s += 100;
    if ((bFo || bRedir) && (aTr || aTw || aSetup))
        s += 100;

    // Helping Hand + attaccante
    if (aHh && (GetMonData(b, MON_DATA_ATK) >= 90 || GetMonData(b, MON_DATA_SPATK) >= 90))
        s += 50;
    if (bHh && (GetMonData(a, MON_DATA_ATK) >= 90 || GetMonData(a, MON_DATA_SPATK) >= 90))
        s += 50;

    // Redirect + attaccante fragile o da setup
    if (aRedir && (bSetup || BaseSpeed(b) <= 60))
        s += 80;
    if (bRedir && (aSetup || BaseSpeed(a) <= 60))
        s += 80;

    // Due Fake Out insieme: non si possono usare entrambi
    if (aFo && bFo)
        s -= 60;

    // Terremoto: immunità partner o danno al compagno
    if (MonKnowsMove(a, MOVE_EARTHQUAKE))
    {
        if (TypeVsMonPct(TYPE_GROUND, b) == 0)
            s += 80;
        else if (!MonKnowsEffect(b, EFFECT_PROTECT))
            s -= 100;
    }
    if (MonKnowsMove(b, MOVE_EARTHQUAKE))
    {
        if (TypeVsMonPct(TYPE_GROUND, a) == 0)
            s += 80;
        else if (!MonKnowsEffect(a, EFFECT_PROTECT))
            s -= 100;
    }

    // Scarica: immunità partner
    if (MonKnowsMove(a, MOVE_DISCHARGE))
    {
        if (TypeVsMonPct(TYPE_ELECTRIC, b) == 0)
            s += 80;
        else if (!MonKnowsEffect(b, EFFECT_PROTECT))
            s -= 80;
    }
    if (MonKnowsMove(b, MOVE_DISCHARGE))
    {
        if (TypeVsMonPct(TYPE_ELECTRIC, a) == 0)
            s += 80;
        else if (!MonKnowsEffect(a, EFFECT_PROTECT))
            s -= 80;
    }

    // Debolezze in comune e coperture difensive
    for (t = TYPE_NORMAL; t < NUMBER_OF_MON_TYPES; t++)
    {
        if (t == TYPE_MYSTERY || t == TYPE_STELLAR)
            continue;
        u32 effA = TypeVsMonPct(t, a);
        u32 effB = TypeVsMonPct(t, b);
        if (effA >= 200 && effB >= 200)
            s -= 25;
        else if (effA >= 200 && effB <= 50)
            s += 15;
        else if (effB >= 200 && effA <= 50)
            s += 15;
    }
    return s;
}

static s16 sMatch[PARTY_SIZE][PARTY_SIZE]; // [enemy][player]

// Punteggio 1v1: + buono per il nemico (AI), - buono per il giocatore.
static s32 Matchup(struct Pokemon *e, struct Pokemon *p, bool32 omniscient)
{
    const struct SeriesMonMemory *mem = SeriesFindMemory(GetMonData(p, MON_DATA_PERSONALITY));
    struct ScoutItemGuess g;
    u16 pSpecies = GetMonData(p, MON_DATA_SPECIES);
    u32 off = BestMovePct(e, p);
    // Senza onniscienza le mosse del giocatore non si vedono: si stima dallo STAB (come in una
    // vera team preview) e da ciò che si è visto nelle gare precedenti della serie.
    u32 def = omniscient ? BestMovePct(p, e) : max(BestStabPct(p, e), BestEstimatedPct(p, e));
    u32 eSpd = GetMonData(e, MON_DATA_SPEED), pSpd = PreviewStat(p, MON_DATA_SPEED);
    u32 hitsE, hitsP;
    bool32 eFaster;
    s32 s;

    // Gare precedenti della serie: le mosse viste contano per intero.
    if (!omniscient && mem != NULL)
        def = max(def, BestRememberedMovePct(p, e, mem));

    // Strumento e Mega probabili (solo dalla specie e dalla lista degli strumenti del formato;
    // nel Bo3 lo strumento visto nelle gare prima è certo).
    Scout_GuessItemInTeam(PlayerContext(), pSpecies, &g); // clausola degli strumenti e una sola Mega per squadra
    if (mem != NULL && mem->heldEffect != HOLD_EFFECT_NONE)
    {
        g.effect = mem->heldEffect;
        g.chance = 100;
    }
    if (!omniscient)
    {
        if (g.megaSpecies != SPECIES_NONE && g.megaChance >= 50)
        {
            // Può megaevolvere: statistiche della forma Mega (in proporzione alle basi).
            const struct SpeciesInfo *b = &gSpeciesInfo[pSpecies], *m = &gSpeciesInfo[g.megaSpecies];
            u32 bOff = b->baseAttack + b->baseSpAttack, mOff = m->baseAttack + m->baseSpAttack;
            u32 bDef = b->baseDefense + b->baseSpDefense, mDef = m->baseDefense + m->baseSpDefense;
            if (bOff != 0)
                def = def * mOff / bOff;
            if (mDef != 0)
                off = off * bDef / mDef;
            if (b->baseSpeed != 0)
                pSpd = pSpd * m->baseSpeed / b->baseSpeed;
        }
        if (g.chance >= 50)
        {
            switch (g.effect)
            {
            case HOLD_EFFECT_CHOICE_SCARF: pSpd = pSpd * 3 / 2; break;
            case HOLD_EFFECT_EVIOLITE:     off = off * 2 / 3;   break;
            case HOLD_EFFECT_ASSAULT_VEST: off = off * 4 / 5;   break;
            case HOLD_EFFECT_LIFE_ORB:     def = def * 13 / 10; break;
            default: break;
            }
        }
    }
    hitsE = HitsToKo(off);
    hitsP = HitsToKo(def);
    if (!omniscient && g.chance >= 55 && g.effect == HOLD_EFFECT_FOCUS_SASH && hitsE < 2)
        hitsE = 2; // il Focus Sash regge il primo colpo
    eFaster = eSpd > pSpd;
    s = (s32)min(off, 150) - (s32)min(def, 150);

    if (hitsE < hitsP || (hitsE == hitsP && eFaster))
        s += 50;
    else
        s -= 50;
    return s;
}

static u32 PopCount(u32 x)
{
    u32 n = 0;
    while (x) { n += x & 1; x >>= 1; }
    return n;
}

// Cosa si può dedurre dalla sola specie (come in una vera team preview): una delle abilità possibili.
static bool32 SpeciesMayHaveAbility(u16 species, enum Ability ability)
{
    u32 i;
    for (i = 0; i < NUM_ABILITY_SLOTS; i++)
    {
        if (gSpeciesInfo[species].abilities[i] == ability)
            return TRUE;
    }
    return FALSE;
}

static bool32 MaskHasSpecies(struct Pokemon *enemy, const u32 *slots, u32 n, u32 mask, u16 species)
{
    u32 i;
    for (i = 0; i < n; i++)
    {
        if ((mask & (1u << i)) && AI_VgcSameSpecies(species, GetMonData(&enemy[slots[i]], MON_DATA_SPECIES)))
            return TRUE;
    }
    return FALSE;
}

// ---------------------------------------------------------------------
// Conoscenza della squadra dell'IA dal grafo (src/battle_ai_vgc_graph.c): regole per ruolo e per abilità,
// valide per qualunque squadra (al posto delle regole scritte per singolo allenatore).
// ---------------------------------------------------------------------
static EWRAM_DATA struct VgcTeamGraph sTPGraph = {0};

static bool32 MaskHasAbility(struct Pokemon *enemy, const u32 *slots, u32 n, u32 mask, enum Ability ability)
{
    u32 i;
    for (i = 0; i < n; i++)
        if ((mask & (1u << i)) && GetMonAbility(&enemy[slots[i]]) == ability)
            return TRUE;
    return FALSE;
}

static bool32 MaskHasRole(const u32 *slots, u32 n, u32 mask, u32 role)
{
    u32 i;
    for (i = 0; i < n; i++)
        if ((mask & (1u << i)) && (sTPGraph.roles[slots[i]] & role))
            return TRUE;
    return FALSE;
}

static bool32 MonReliesOnType(struct Pokemon *mon, enum Type type)
{
    u32 i, n = 0;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        enum Move m = GetMonData(mon, MON_DATA_MOVE1 + i);
        if (m != MOVE_NONE && !IsBattleMoveStatus(m) && GetMoveType(m) == type && GetMovePower(m) >= 70)
            n++;
    }
    return n >= 2;
}

static bool32 IsWeatherSpeedAbility(enum Ability ab)
{
    return ab == ABILITY_SWIFT_SWIM || ab == ABILITY_CHLOROPHYLL || ab == ABILITY_SAND_RUSH || ab == ABILITY_SLUSH_RUSH;
}

// Risposte del nostro gruppo (o della lead) ai piani di gioco dell'avversario letti dalla sua squadra in team preview
// (contesto, include/battle_ai_scout.h): ogni piano pesa per quanto è forte. Positivo = buono per l'IA.
//  - Trick Room: Provocazione, Fake Out, Imprison con Trick Room, il nostro Trick Room (lo annulla); i nostri lenti;
//  - Tailwind / squadra veloce: il nostro Trick Room o Tailwind, Icy Wind & co., priorità, Fake Out;
//  - meteo: il nostro meteo o Cloud Nine / Air Lock, chi resiste al tipo potenziato;
//  - redirect + setup: mosse ad area, Provocazione, Haze / Clear Smog.
static bool32 MonHasSpreadAttack(struct Pokemon *mon);
static s32 ContextAnswerScore(struct Pokemon *enemy, const u32 *slots, u32 n, u32 mask, const struct ScoutTeamContext *ctx)
{
    u32 i, k, taunt = 0, fakeOut = 0, imprison = 0, ownTr = 0, ownTw = 0, soft = 0, prio = 0, slowAtk = 0;
    u32 weather = 0, resist = 0, spread = 0, haze = 0;
    s32 s = 0;

    if (ctx == NULL || ctx->n == 0)
        return 0;
    for (i = 0; i < n; i++)
    {
        struct Pokemon *mon = &enemy[slots[i]];
        enum Ability ab;
        if (!(mask & (1u << i)))
            continue;
        ab = MonWeatherAbility(mon);
        taunt += MonKnowsEffect(mon, EFFECT_TAUNT);
        fakeOut += HasFakeOut(mon);
        ownTr += MonKnowsEffect(mon, EFFECT_TRICK_ROOM);
        ownTw += MonKnowsEffect(mon, EFFECT_TAILWIND);
        imprison += MonKnowsEffect(mon, EFFECT_IMPRISON) && MonKnowsEffect(mon, EFFECT_TRICK_ROOM);
        haze += MonKnowsEffect(mon, EFFECT_HAZE) || MonKnowsMove(mon, MOVE_CLEAR_SMOG);
        spread += MonHasSpreadAttack(mon);
        if (BaseSpeed(mon) <= 60 && max(gSpeciesInfo[GetMonData(mon, MON_DATA_SPECIES)].baseAttack,
                                         gSpeciesInfo[GetMonData(mon, MON_DATA_SPECIES)].baseSpAttack) >= 100)
            slowAtk++;
        for (k = 0; k < MAX_MON_MOVES; k++)
        {
            enum Move m = GetMonData(mon, MON_DATA_MOVE1 + k);
            if (m == MOVE_NONE)
                continue;
            if (Scout_MoveHasRole(m, SCOUT_SOFT_SPEED))
                soft++;
            else if (!IsBattleMoveStatus(m) && GetMovePriority(m) > 0 && GetMoveEffect(m) != EFFECT_FIRST_TURN_ONLY)
                prio++;
        }
        switch (ab)
        {
        case ABILITY_DROUGHT: case ABILITY_ORICHALCUM_PULSE:
            weather += ctx->weather != SCOUT_WEATHER_SUN;
            break;
        case ABILITY_DRIZZLE:
            weather += ctx->weather != SCOUT_WEATHER_RAIN;
            break;
        case ABILITY_SAND_STREAM:
            weather += ctx->weather != SCOUT_WEATHER_SAND;
            break;
        case ABILITY_SNOW_WARNING:
            weather += ctx->weather != SCOUT_WEATHER_SNOW;
            break;
        case ABILITY_CLOUD_NINE: case ABILITY_AIR_LOCK:
            weather += 2;
            break;
        default:
            break;
        }
        if ((ctx->weather == SCOUT_WEATHER_RAIN && TypeVsMonPct(TYPE_WATER, mon) < 100)
         || (ctx->weather == SCOUT_WEATHER_SUN && TypeVsMonPct(TYPE_FIRE, mon) < 100))
            resist++;
    }
    if (ctx->archetype & SCOUT_ARCH_TRICK_ROOM)
    {
        u32 ans = 2 * taunt + fakeOut + 3 * imprison + ownTr;
        s += (s32)ctx->trP * (s32)min(ans, 4u) * 60 / 100;
        s += (s32)ctx->trP * (s32)min(slowAtk, 2u) * 30 / 100;
    }
    if (ctx->archetype & (SCOUT_ARCH_TAILWIND | SCOUT_ARCH_FAST))
    {
        u32 ans = 2 * ownTr + 2 * ownTw + soft + prio + fakeOut;
        s += (s32)ctx->twP * (s32)min(ans, 4u) * 60 / 100;
    }
    if (ctx->archetype & SCOUT_ARCH_WEATHER)
    {
        u32 ans = 2 * weather + resist;
        s += (s32)ctx->weatherP * (s32)min(ans, 4u) * 50 / 100;
    }
    if (ctx->archetype & SCOUT_ARCH_REDIRECT)
    {
        u32 ans = spread + taunt + 2 * haze;
        s += (s32)ctx->redirectP * (s32)min(ans, 3u) * 40 / 100;
    }
    return s * (s32)(gScoutContextAnswerPct ? gScoutContextAnswerPct : 100) / 100;
}

// Coerenza del gruppo di 4: dipendenze e conflitti ricavati dalle proprietà.
static s32 GraphGroupScore(struct Pokemon *enemy, const u32 *slots, u32 n, u32 mask)
{
    s32 score = 0;
    u32 i, j, megas = 0;
    u32 teamMask = (1u << n) - 1;

    for (i = 0; i < n; i++)
    {
        u32 slot = slots[i], recipient;
        enum Ability ab;
        if (!(mask & (1u << i)))
            continue;
        ab = GetMonAbility(&enemy[slot]);
        // Baton Passer senza il suo ricevente: inutile.
        recipient = sTPGraph.requiredBack[slot];
        if (recipient != VGC_GRAPH_NONE)
        {
            bool32 found = FALSE;
            for (j = 0; j < n; j++)
                if ((mask & (1u << j)) && slots[j] == recipient)
                    found = TRUE;
            if (!found)
                score -= 10000;
        }
        // Abilità penalizzante (Truant, Slow Start...) senza chi la spegne o la toglie.
        if ((sTPGraph.roles[slot] & VGC_ROLE_HINDER_ABILITY) && ab != ABILITY_MYCELIUM_MIGHT
         && !MaskHasAbility(enemy, slots, n, mask, ABILITY_NEUTRALIZING_GAS)
         && !MaskHasRole(slots, n, mask, VGC_ROLE_ABILITY_CHANGER))
            score -= 5000;
        // Commander funziona solo con Dondozo (regola del gioco): se la squadra li ha entrambi, insieme o niente.
        if (ab == ABILITY_COMMANDER && MaskHasSpecies(enemy, slots, n, teamMask, SPECIES_DONDOZO)
         && !MaskHasSpecies(enemy, slots, n, mask, SPECIES_DONDOZO))
            score -= 5000;
        if (AI_VgcSameSpecies(SPECIES_DONDOZO, GetMonData(&enemy[slot], MON_DATA_SPECIES))
         && MaskHasAbility(enemy, slots, n, teamMask, ABILITY_COMMANDER) && !MaskHasAbility(enemy, slots, n, mask, ABILITY_COMMANDER))
            score -= 5000;
        // Il Gas Reagente spegne le abilità dei compagni che servono (Commander, Hospitality).
        if (ab == ABILITY_COMMANDER && MaskHasAbility(enemy, slots, n, mask, ABILITY_NEUTRALIZING_GAS))
            score -= 5000;
        if (ab == ABILITY_HOSPITALITY && MaskHasAbility(enemy, slots, n, mask, ABILITY_NEUTRALIZING_GAS))
            score -= 60;
        // Sole e attaccanti d'Acqua (o pioggia e attaccanti di Fuoco) si danneggiano a vicenda.
        if ((ab == ABILITY_DROUGHT || ab == ABILITY_ORICHALCUM_PULSE))
            for (j = 0; j < n; j++)
                if ((mask & (1u << j)) && MonReliesOnType(&enemy[slots[j]], TYPE_WATER))
                    score -= 3000;
        if (ab == ABILITY_DRIZZLE)
            for (j = 0; j < n; j++)
                if ((mask & (1u << j)) && MonReliesOnType(&enemy[slots[j]], TYPE_FIRE))
                    score -= 3000;
        // Trick Room e chi è veloce grazie al meteo non stanno insieme.
        if ((sTPGraph.roles[slot] & VGC_ROLE_TRICK_ROOM))
            for (j = 0; j < n; j++)
                if ((mask & (1u << j)) && IsWeatherSpeedAbility(GetMonAbility(&enemy[slots[j]])))
                    score -= 3000;
        // Un attaccante lento senza chi gli piazza Trick Room (se in squadra c'è).
        if ((sTPGraph.roles[slot] & VGC_ROLE_SLOW) && (sTPGraph.roles[slot] & (VGC_ROLE_ATK_SWEEPER | VGC_ROLE_SPA_SWEEPER))
         && MaskHasRole(slots, n, teamMask & ~(1u << i), VGC_ROLE_TRICK_ROOM)
         && !MaskHasRole(slots, n, mask & ~(1u << i), VGC_ROLE_TRICK_ROOM))
            score -= 2000;
        if (GetItemHoldEffect(GetMonData(&enemy[slot], MON_DATA_HELD_ITEM)) == HOLD_EFFECT_MEGA_STONE)
            megas++;
        // Sinergie di coppia del grafo.
        for (j = i + 1; j < n; j++)
            if (mask & (1u << j))
                score += sTPGraph.leadPair[slot][slots[j]] / 2;
    }
    if (megas >= 2)
        score -= 40; // una sola Mega per lotta
    return score;
}

// Coerenza della coppia di lead (indici di party `a` e `b`).
static s32 GraphLeadScore(struct Pokemon *enemy, u32 a, u32 b)
{
    s32 s = sTPGraph.leadPair[a][b] * 4;
    enum Ability abA = GetMonAbility(&enemy[a]), abB = GetMonAbility(&enemy[b]);
    bool32 donA = AI_VgcSameSpecies(SPECIES_DONDOZO, GetMonData(&enemy[a], MON_DATA_SPECIES));
    bool32 donB = AI_VgcSameSpecies(SPECIES_DONDOZO, GetMonData(&enemy[b], MON_DATA_SPECIES));
    u32 i;

    // Commander: Tatsugiri entra in Dondozo solo se partono insieme.
    if ((abA == ABILITY_COMMANDER && !donB) || (abB == ABILITY_COMMANDER && !donA))
        s -= 10000;
    // Abilità penalizzante in lead senza chi la spegne o la toglie accanto.
    if ((sTPGraph.roles[a] & VGC_ROLE_HINDER_ABILITY) && abA != ABILITY_MYCELIUM_MIGHT
     && abB != ABILITY_NEUTRALIZING_GAS && !(sTPGraph.roles[b] & VGC_ROLE_ABILITY_CHANGER))
        s -= 10000;
    if ((sTPGraph.roles[b] & VGC_ROLE_HINDER_ABILITY) && abB != ABILITY_MYCELIUM_MIGHT
     && abA != ABILITY_NEUTRALIZING_GAS && !(sTPGraph.roles[a] & VGC_ROLE_ABILITY_CHANGER))
        s -= 10000;
    // Chi sfrutta il meteo parte accanto a chi lo porta.
    if (((sTPGraph.roles[a] & VGC_ROLE_WEATHER_ABUSER) && !(sTPGraph.roles[b] & VGC_ROLE_WEATHER_SETTER))
     || ((sTPGraph.roles[b] & VGC_ROLE_WEATHER_ABUSER) && !(sTPGraph.roles[a] & VGC_ROLE_WEATHER_SETTER)))
        s -= 400;
    // Mosse ad area che colpiscono anche il compagno (Earthquake...): male se il compagno non è immune.
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        enum Move ma = GetMonData(&enemy[a], MON_DATA_MOVE1 + i), mb = GetMonData(&enemy[b], MON_DATA_MOVE1 + i);
        if (ma != MOVE_NONE && GetMoveTarget(ma) == TARGET_FOES_AND_ALLY && !IsBattleMoveStatus(ma)
         && TypeVsMonPct(GetMoveType(ma), &enemy[b]) != 0)
            s -= 80;
        if (mb != MOVE_NONE && GetMoveTarget(mb) == TARGET_FOES_AND_ALLY && !IsBattleMoveStatus(mb)
         && TypeVsMonPct(GetMoveType(mb), &enemy[a]) != 0)
            s -= 80;
    }
    return s;
}

static bool32 PairIs(u16 a, u16 b, u16 x, u16 y)
{
    return (AI_VgcSameSpecies(x, a) && AI_VgcSameSpecies(y, b)) || (AI_VgcSameSpecies(x, b) && AI_VgcSameSpecies(y, a));
}

// ---------------------------------------------------------------------
// Scouting della lead (vedi include/battle_ai_scout.h)
// ---------------------------------------------------------------------
// L'IA vede solo le specie del giocatore e ne stima i ruoli probabili; dei propri Pokémon conosce
// tutto. Con questo: (1) stima le lead più probabili del giocatore e le sue core, (2) valuta ogni
// propria lead contro ognuna di quelle (controllo velocità, meteo, campi, Fake Out, Provocazione,
// sonno, Prepotenza, redirect), (3) sceglie con una strategia mista per non essere leggibile.
#define PRED_LEADS 6

static EWRAM_DATA u16 sMegaDesignate = SPECIES_NONE; // chi megaevolve in questa gara (deciso in preview)

// Minacce esplicite (contesto della squadra): per ogni nostro Pokémon portato, quanto lo minacciano (0..100) gli
// avversari con i set del contesto, pesati per quanto è probabile che partano in lead. Due squadre (IA contro IA).
static EWRAM_DATA u16 sThreatSpecies[2][PARTY_SIZE] = {0};
static EWRAM_DATA u8 sThreatVal[2][PARTY_SIZE] = {0};
static EWRAM_DATA u8 sThreatNext = 0;

u32 TeamPreview_ThreatPrior(u16 species)
{
    u32 k, i, e;
    for (k = 0; k < 2; k++)
    {
        e = (sThreatNext + 1 + k) % 2; // prima la più recente
        for (i = 0; i < PARTY_SIZE; i++)
            if (species != SPECIES_NONE && sThreatSpecies[e][i] == species)
                return sThreatVal[e][i];
    }
    return 0;
}

u16 TeamPreview_GetMegaDesignateSpecies(void)
{
    return sMegaDesignate;
}

static u8 RoleMax(u8 a, u8 b)
{
    return a > b ? a : b;
}

// Profilo di un Pokémon dell'IA: è suo, quindi mosse e abilità vere.
static void EnemyProfile(struct Pokemon *mon, struct ScoutProfile *out)
{
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    u16 item = GetMonData(mon, MON_DATA_HELD_ITEM);
    enum Ability ability = GetMonAbility(mon);
    u32 i, r;

    Scout_GetSpeciesProfile(species, out);
    for (r = 0; r < SCOUT_ROLE_COUNT; r++)
        out->role[r] = 0;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        enum Move move = GetMonData(mon, MON_DATA_MOVE1 + i);
        for (r = 0; r < SCOUT_ROLE_COUNT; r++)
        {
            if (Scout_MoveHasRole(move, r))
                out->role[r] = 100;
        }
    }

    out->weather = SCOUT_WEATHER_NONE;
    out->weatherP = 100;
    switch (ability)
    {
    case ABILITY_DROUGHT:
    case ABILITY_ORICHALCUM_PULSE:
    case ABILITY_DESOLATE_LAND:
        out->weather = SCOUT_WEATHER_SUN;
        break;
    case ABILITY_DRIZZLE:
    case ABILITY_PRIMORDIAL_SEA:
        out->weather = SCOUT_WEATHER_RAIN;
        break;
    case ABILITY_SAND_STREAM:
    case ABILITY_SAND_SPIT:
        out->weather = SCOUT_WEATHER_SAND;
        break;
    case ABILITY_SNOW_WARNING:
        out->weather = SCOUT_WEATHER_SNOW;
        break;
    default:
        out->weatherP = 0;
        break;
    }
    // Megapietre che portano il meteo (la Mega attiva l'abilità al turno 1).
    if (out->weather == SCOUT_WEATHER_NONE && item == ITEM_CHARIZARDITE_Y)
    {
        out->weather = SCOUT_WEATHER_SUN;
        out->weatherP = 100;
    }
    else if (out->weather == SCOUT_WEATHER_NONE && item == ITEM_TYRANITARITE)
    {
        out->weather = SCOUT_WEATHER_SAND;
        out->weatherP = 100;
    }

    out->terrainP = 100;
    switch (ability)
    {
    case ABILITY_GRASSY_SURGE:
        out->terrain = SCOUT_TERRAIN_GRASSY;
        break;
    case ABILITY_ELECTRIC_SURGE:
    case ABILITY_HADRON_ENGINE:
        out->terrain = SCOUT_TERRAIN_ELECTRIC;
        break;
    case ABILITY_PSYCHIC_SURGE:
        out->terrain = SCOUT_TERRAIN_PSYCHIC;
        break;
    case ABILITY_MISTY_SURGE:
        out->terrain = SCOUT_TERRAIN_MISTY;
        break;
    default:
        out->terrain = SCOUT_TERRAIN_NONE;
        out->terrainP = 0;
        break;
    }
    out->intimidateP = (ability == ABILITY_INTIMIDATE) ? 100 : 0;
}

// Profilo di un Pokémon del giocatore: specie + (nel Bo3) ciò che l'IA ha visto nelle gare prima.
static void PlayerProfile(struct Pokemon *mon, struct ScoutProfile *out)
{
    const struct SeriesMonMemory *mem = SeriesFindMemory(GetMonData(mon, MON_DATA_PERSONALITY));
    u32 i, r, known = 0;
    bool8 hit[SCOUT_ROLE_COUNT] = {0};

    // Ruoli nella sua squadra (contesto: setter del Trick Room di una squadra TR, chi sfrutta il meteo...).
    Scout_GetProfileInTeam(PlayerContext(), GetMonData(mon, MON_DATA_SPECIES), out);
    {
        u16 mega = Scout_MegaTargetSpecies(GetMonData(mon, MON_DATA_SPECIES));
        // Una sola Mega per squadra: la probabilità è quella del contesto (la Mega più probabile della squadra).
        u32 megaP = min(Scout_MegaChanceInTeam(PlayerContext(), GetMonData(mon, MON_DATA_SPECIES)), 100u);
        if (mega != SPECIES_NONE && megaP > 0)
        {
            struct ScoutProfile mp;
            Scout_GetSpeciesProfile(mega, &mp);
            if (mp.weather != SCOUT_WEATHER_NONE && mp.weatherP * megaP / 100 > out->weatherP)
            {
                out->weather = mp.weather;
                out->weatherP = mp.weatherP * megaP / 100;
            }
            if (mp.intimidateP * megaP / 100 > out->intimidateP)
                out->intimidateP = mp.intimidateP * megaP / 100;
            if (mp.baseSpeed > out->baseSpeed)
                out->baseSpeed = (out->baseSpeed + mp.baseSpeed) / 2;
        }
    }
    if (mem == NULL)
        return;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (mem->moves[i] == MOVE_NONE)
            continue;
        known++;
        for (r = 0; r < SCOUT_ROLE_COUNT; r++)
        {
            if (Scout_MoveHasRole(mem->moves[i], r))
                hit[r] = TRUE;
        }
    }
    for (r = 0; r < SCOUT_ROLE_COUNT; r++)
    {
        if (hit[r])
            out->role[r] = 100;
        else
            out->role[r] = out->role[r] * (MAX_MON_MOVES - min(known, MAX_MON_MOVES)) / MAX_MON_MOVES;
    }
    if (mem->ability != ABILITY_NONE)
    {
        out->intimidateP = (mem->ability == ABILITY_INTIMIDATE) ? 100 : 0;
        if (mem->ability == ABILITY_DROUGHT || mem->ability == ABILITY_DRIZZLE || mem->ability == ABILITY_SAND_STREAM
         || mem->ability == ABILITY_SNOW_WARNING || mem->ability == ABILITY_ORICHALCUM_PULSE)
            out->weatherP = 100;
        else
            out->weatherP = 0;
    }
}

// Quanto è probabile / sensata la coppia come lead del giocatore (core e ruoli da lead).
static s32 PlayerLeadScore(const struct ScoutProfile *a, const struct ScoutProfile *b, struct Pokemon *ma, struct Pokemon *mb)
{
    const struct ScoutProfile *p[2] = { a, b };
    s32 s = 0;
    u32 i;

    for (i = 0; i < 2; i++)
    {
        const struct ScoutProfile *x = p[i], *y = p[1 - i];
        // Ruoli da lead.
        s += (x->role[SCOUT_FAKE_OUT] * 30 + x->intimidateP * 20 + x->role[SCOUT_TRICK_ROOM] * 25
            + x->role[SCOUT_TAILWIND] * 25 + x->weatherP * 25 + x->terrainP * 15 + x->role[SCOUT_REDIRECT] * 20
            + x->role[SCOUT_TAUNT] * 10 + x->role[SCOUT_SLEEP] * 10) / 100;
        // Core: setter + chi ne approfitta.
        if (y->baseSpeed <= 55)
            s += x->role[SCOUT_TRICK_ROOM] * 40 / 100;
        if (y->baseSpeed >= 60 && y->baseSpeed <= 110)
            s += x->role[SCOUT_TAILWIND] * 25 / 100;
        if (x->weather != SCOUT_WEATHER_NONE && x->weather == y->weatherAbuse)
            s += x->weatherP * 45 / 100;
        s += x->role[SCOUT_FAKE_OUT] * RoleMax(y->role[SCOUT_TRICK_ROOM], y->role[SCOUT_TAILWIND]) * 30 / 10000;
        s += x->role[SCOUT_REDIRECT] * y->role[SCOUT_SETUP] * 30 / 10000;
    }
    // Commander: Dondozo + Tatsugiri partono insieme.
    if (PairIs(GetMonData(ma, MON_DATA_SPECIES), GetMonData(mb, MON_DATA_SPECIES), SPECIES_DONDOZO, SPECIES_TATSUGIRI))
        s += 60;
    // Contesto della squadra: coppie che hanno senso insieme (setter + chi ne approfitta, Fake Out + setter,
    // redirect + setup, meteo + chi lo sfrutta), lenti senza Trick Room accanto meno probabili.
    s += Scout_ContextLeadScore(PlayerContext(), GetMonData(ma, MON_DATA_SPECIES), GetMonData(mb, MON_DATA_SPECIES));
    // Bo3: chi ha già fatto da lead tende a rifarlo.
    {
        const struct SeriesMonMemory *m1 = SeriesFindMemory(GetMonData(ma, MON_DATA_PERSONALITY));
        const struct SeriesMonMemory *m2 = SeriesFindMemory(GetMonData(mb, MON_DATA_PERSONALITY));
        if (m1 != NULL)
            s += 40 * min(m1->timesLead, 2);
        if (m2 != NULL)
            s += 40 * min(m2->timesLead, 2);
    }
    return s;
}

static bool32 ProfMonIsType(struct Pokemon *mon, enum Type type)
{
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    return gSpeciesInfo[species].types[0] == type || gSpeciesInfo[species].types[1] == type;
}

static bool32 MonIgnoresIntimidate(struct Pokemon *mon)
{
    enum Ability ab = GetMonAbility(mon);
    enum HoldEffect he = GetItemHoldEffect(GetMonData(mon, MON_DATA_HELD_ITEM));
    return ab == ABILITY_DEFIANT || ab == ABILITY_COMPETITIVE || ab == ABILITY_CLEAR_BODY || ab == ABILITY_WHITE_SMOKE
        || ab == ABILITY_FULL_METAL_BODY || ab == ABILITY_MIRROR_ARMOR || ab == ABILITY_HYPER_CUTTER || ab == ABILITY_INNER_FOCUS
        || ab == ABILITY_OWN_TEMPO || ab == ABILITY_OBLIVIOUS || ab == ABILITY_SCRAPPY || ab == ABILITY_GUARD_DOG
        || ab == ABILITY_GUTS || he == HOLD_EFFECT_CLEAR_AMULET;
}

static bool32 MonImmuneToSleep(struct Pokemon *mon)
{
    enum Ability ab = GetMonAbility(mon);
    return ProfMonIsType(mon, TYPE_GRASS) || ab == ABILITY_INSOMNIA || ab == ABILITY_VITAL_SPIRIT || ab == ABILITY_SWEET_VEIL
        || ab == ABILITY_GOOD_AS_GOLD || ab == ABILITY_MAGIC_BOUNCE || ab == ABILITY_PURIFYING_SALT || ab == ABILITY_OVERCOAT
        || GetItemHoldEffect(GetMonData(mon, MON_DATA_HELD_ITEM)) == HOLD_EFFECT_SAFETY_GOGGLES;
}

// Il setter di controllo velocità resiste a Fake Out? (Spettro, Forza Interiore, Campo Psichico del
// partner, Anticipo del partner, o un nostro Fake Out da scambiare).
static bool32 SetterSafeFromFakeOut(struct Pokemon *setter, struct Pokemon *partner, const struct ScoutProfile *pp)
{
    return ProfMonIsType(setter, TYPE_GHOST) || GetMonAbility(setter) == ABILITY_INNER_FOCUS
        || GetMonAbility(partner) == ABILITY_PSYCHIC_SURGE || MonKnowsMove(partner, MOVE_QUICK_GUARD)
        || pp->role[SCOUT_FAKE_OUT] == 100;
}

static bool32 SetterSafeFromTaunt(struct Pokemon *setter)
{
    enum Ability ab = GetMonAbility(setter);
    return ab == ABILITY_OBLIVIOUS || ab == ABILITY_MAGIC_BOUNCE
        || GetItemHoldEffect(GetMonData(setter, MON_DATA_HELD_ITEM)) == HOLD_EFFECT_MENTAL_HERB;
}

static bool32 MonHasSpreadAttack(struct Pokemon *mon)
{
    u32 i;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        enum Move m = GetMonData(mon, MON_DATA_MOVE1 + i);
        if (m != MOVE_NONE && !IsBattleMoveStatus(m) && IsSpreadMove(GetMoveTarget(m)))
            return TRUE;
    }
    return FALSE;
}

// Valore della nostra lead contro una lead probabile del giocatore. `matchup` = duelli 1v1 calcolati
// dal chiamante. Positivo = buono per l'IA.
static s32 LeadVsLead(struct Pokemon *mea, struct Pokemon *meb, const struct ScoutProfile *ea, const struct ScoutProfile *eb,
                      const struct ScoutProfile *pa, const struct ScoutProfile *pb, s32 matchup)
{
    s32 v = matchup;
    s32 plTR = RoleMax(pa->role[SCOUT_TRICK_ROOM], pb->role[SCOUT_TRICK_ROOM]);
    s32 plTW = RoleMax(pa->role[SCOUT_TAILWIND], pb->role[SCOUT_TAILWIND]);
    s32 plFO = RoleMax(pa->role[SCOUT_FAKE_OUT], pb->role[SCOUT_FAKE_OUT]);
    s32 plTaunt = RoleMax(pa->role[SCOUT_TAUNT], pb->role[SCOUT_TAUNT]);
    s32 plRedir = RoleMax(pa->role[SCOUT_REDIRECT], pb->role[SCOUT_REDIRECT]);
    s32 plSleep = RoleMax(pa->role[SCOUT_SLEEP], pb->role[SCOUT_SLEEP]);
    s32 plIntim = RoleMax(pa->intimidateP, pb->intimidateP);
    s32 plSetter = max(plTR, plTW);
    bool32 aiTR = ea->role[SCOUT_TRICK_ROOM] || eb->role[SCOUT_TRICK_ROOM];
    bool32 aiTW = ea->role[SCOUT_TAILWIND] || eb->role[SCOUT_TAILWIND];
    bool32 aiFO = ea->role[SCOUT_FAKE_OUT] || eb->role[SCOUT_FAKE_OUT];
    bool32 aiTaunt = ea->role[SCOUT_TAUNT] || eb->role[SCOUT_TAUNT];
    bool32 aiSoft = ea->role[SCOUT_SOFT_SPEED] || eb->role[SCOUT_SOFT_SPEED];
    bool32 aiRedir = ea->role[SCOUT_REDIRECT] || eb->role[SCOUT_REDIRECT];
    u32 aiSpd = (ea->baseSpeed + eb->baseSpeed) / 2, plSpd = (pa->baseSpeed + pb->baseSpeed) / 2;
    bool32 aIsSetter = ea->role[SCOUT_TRICK_ROOM] || ea->role[SCOUT_TAILWIND];
    struct Pokemon *setter = aIsSetter ? mea : meb;
    struct Pokemon *setterPartner = aIsSetter ? meb : mea;
    const struct ScoutProfile *setterPartnerProf = aIsSetter ? eb : ea;

    // --- Il nostro Trick Room ---
    if (aiTR)
    {
        if (aiSpd < plSpd)
            v += 25; // ci serve davvero
        else
            v -= 15;
        if (!SetterSafeFromFakeOut(setter, setterPartner, setterPartnerProf))
            v -= plFO * 40 / 100;
        if (!SetterSafeFromTaunt(setter))
            v -= plTaunt * 35 / 100;
        if (aiRedir)
            v += 10; // il redirect protegge il setter
    }
    // --- Il nostro Tailwind ---
    if (aiTW)
    {
        if (aiSpd <= plSpd && aiSpd * 2 > plSpd)
            v += 20;
        v -= plTR * 25 / 100; // la loro Distorsione ribalta il vento
        if (!SetterSafeFromTaunt(setter))
            v -= plTaunt * 15 / 100;
    }
    // --- Il loro Trick Room: ci danneggia se siamo il lato veloce ---
    if (plTR >= 30 && aiSpd > plSpd)
    {
        if (aiFO)
            v += plTR * 30 / 100;
        else if (aiTaunt)
            v += plTR * 25 / 100;
        else if (aiTR)
            v += plTR * 10 / 100; // possiamo annullarla, ma costa un turno
        else
            v -= plTR * 40 / 100;
    }
    // --- Il loro Tailwind ---
    if (plTW >= 30 && !aiTR)
    {
        if (aiTW)
            v += 5;
        else if (aiSoft || aiFO || aiTaunt)
            v += plTW * 10 / 100;
        else if (aiSpd >= plSpd)
            v -= plTW * 25 / 100; // ci superano col vento
    }
    // --- Fake Out / Provocazione nostri sui loro setter ---
    if (aiFO)
        v += plSetter * 25 / 100;
    if (aiTaunt)
        v += plSetter * 20 / 100;
    // --- Guerra del meteo: vince chi lo imposta per ultimo (il più lento) ---
    {
        const struct ScoutProfile *aw = (ea->weather != SCOUT_WEATHER_NONE) ? ea : ((eb->weather != SCOUT_WEATHER_NONE) ? eb : NULL);
        const struct ScoutProfile *pw = (pa->weatherP >= pb->weatherP) ? pa : pb;
        if (pw->weather != SCOUT_WEATHER_NONE && pw->weatherP >= 40)
        {
            if (GetMonAbility(mea) == ABILITY_CLOUD_NINE || GetMonAbility(meb) == ABILITY_CLOUD_NINE
             || GetMonAbility(mea) == ABILITY_AIR_LOCK || GetMonAbility(meb) == ABILITY_AIR_LOCK)
                v += pw->weatherP * 15 / 100;
            else if (aw != NULL && aw->weather != pw->weather)
                v += (aw->baseSpeed < pw->baseSpeed ? 25 : -25) * (s32)pw->weatherP / 100;
        }
    }
    // --- Campi ---
    {
        const struct ScoutProfile *pt = (pa->terrainP >= pb->terrainP) ? pa : pb;
        if (pt->terrain == SCOUT_TERRAIN_PSYCHIC && aiFO)
            v -= pt->terrainP * 20 / 100; // Campo Psichico blocca il nostro Fake Out
        if (ea->terrain == SCOUT_TERRAIN_PSYCHIC || eb->terrain == SCOUT_TERRAIN_PSYCHIC)
            v += plFO * 15 / 100;         // il nostro Campo Psichico blocca il loro
        if (ea->terrain == SCOUT_TERRAIN_ELECTRIC || eb->terrain == SCOUT_TERRAIN_ELECTRIC
         || ea->terrain == SCOUT_TERRAIN_MISTY || eb->terrain == SCOUT_TERRAIN_MISTY)
            v += plSleep * 15 / 100;      // niente sonno a terra
    }
    // --- La nostra Prepotenza e i nostri cali (Ventogelato...) contro le abilità possibili della specie ---
    if (GetMonAbility(mea) == ABILITY_INTIMIDATE || GetMonAbility(meb) == ABILITY_INTIMIDATE || aiSoft)
    {
        const struct ScoutProfile *pp[2] = { pa, pb };
        u32 q;
        for (q = 0; q < 2; q++)
        {
            u16 sp = pp[q]->species;
            if (SpeciesMayHaveAbility(sp, ABILITY_DEFIANT) || SpeciesMayHaveAbility(sp, ABILITY_COMPETITIVE)
             || SpeciesMayHaveAbility(sp, ABILITY_CLEAR_BODY) || SpeciesMayHaveAbility(sp, ABILITY_WHITE_SMOKE)
             || SpeciesMayHaveAbility(sp, ABILITY_FULL_METAL_BODY) || SpeciesMayHaveAbility(sp, ABILITY_MIRROR_ARMOR)
             || SpeciesMayHaveAbility(sp, ABILITY_GUARD_DOG))
                v -= 12; // li rafforziamo o rimbalza
            else if (SpeciesMayHaveAbility(sp, ABILITY_INNER_FOCUS) || SpeciesMayHaveAbility(sp, ABILITY_OWN_TEMPO)
                  || SpeciesMayHaveAbility(sp, ABILITY_OBLIVIOUS) || SpeciesMayHaveAbility(sp, ABILITY_SCRAPPY))
                v -= 4;  // immune a Prepotenza
            else if (pp[q]->physical)
                v += 8;
        }
    }
    // --- Prepotenza contro i nostri attaccanti fisici ---
    if (ea->physical && !MonIgnoresIntimidate(mea))
        v -= plIntim * 12 / 100;
    if (eb->physical && !MonIgnoresIntimidate(meb))
        v -= plIntim * 12 / 100;
    // --- Sonno ---
    if (!MonImmuneToSleep(mea))
        v -= plSleep * 10 / 100;
    if (!MonImmuneToSleep(meb))
        v -= plSleep * 10 / 100;
    // --- Redirect: si aggira con mosse ad area ---
    if (plRedir >= 30)
    {
        if (MonHasSpreadAttack(mea) || MonHasSpreadAttack(meb))
            v += 5;
        else
            v -= plRedir * 15 / 100;
    }
    return v;
}

void TeamPreview_AiSelectEnemyTeam(u32 picks, bool32 isDouble, u8 *outOrder)
{
    struct Pokemon *enemy = gEnemyParty, *player = gPlayerParty;
    u64 aiFlags = GetTrainerAIFlagsFromId(TRAINER_BATTLE_PARAM.opponentA);
    bool32 omniscient = (aiFlags & AI_FLAG_OMNISCIENT) != 0;
    u32 i, j, mask, bestMask = 0, nEnemy = 0, nPlayer = 0;
    u32 enemySlots[PARTY_SIZE], playerSlots[PARTY_SIZE];
    s32 weight[PARTY_SIZE], bestScore = INT32_MIN;
    s32 depth[PARTY_SIZE];
    u32 forcedMask = 0;
    bool32 foeHasFakeOut = FALSE;
    u32 chosen[PARTY_SIZE], nChosen = 0, order = 0;
    struct ScoutProfile enProf[PARTY_SIZE], plProf[PARTY_SIZE];
    u32 predJ[PRED_LEADS], predK[PRED_LEADS], nPred = 0;
    s32 predW[PRED_LEADS];
    u32 mainThreat[2], nMain = 0;
    bool32 planTrViable = FALSE, planTwViable = FALSE;
    u16 trainerId = TRAINER_BATTLE_PARAM.opponentA;
    const struct ScoutTeamContext *ctx;

    AI_VgcBuildTeamGraph(enemy, &sTPGraph);
    // Contesto della squadra del giocatore: calcolato una volta dalle sue 6 specie, vale da qui a tutta la lotta.
    ctx = Scout_SetTeamContextFromParty(player);
    if (gScoutContextPreviewOff)
        ctx = NULL;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (IsMonUsable(&enemy[i])) enemySlots[nEnemy++] = i;
        if (IsMonUsable(&player[i]))
        {
            struct ScoutProfile prof;
            playerSlots[nPlayer++] = i;
            // Le mosse del giocatore non si vedono: Fake Out probabile dal profilo della specie nella sua squadra.
            Scout_GetProfileInTeam(PlayerContext(), GetMonData(&player[i], MON_DATA_SPECIES), &prof);
            if (prof.role[SCOUT_FAKE_OUT] >= 50)
                foeHasFakeOut = TRUE;
        }
    }
    if (picks > nEnemy)
        picks = nEnemy;
    if (nEnemy == 0)
        return;

    // Libro dei piani (M9): piano scelto a caso tra quelli buoni per la nostra squadra e la classe dell'avversario
    // (strategia mista). Bo3: le lead che hanno perso una gara della serie non si ripropongono dal libro.
    if (SeriesIsBestOfThree())
    {
        u32 g;
        for (g = 0; g < 3; g++)
        {
            const u8 *h = sSeries.history[g].enemyLead;
            if (sSeries.history[g].valid && !sSeries.history[g].aiWon && h[0] < PARTY_SIZE && h[1] < PARTY_SIZE)
                AI_VgcBookExcludeLeads(GetMonData(&enemy[h[0]], MON_DATA_SPECIES), GetMonData(&enemy[h[1]], MON_DATA_SPECIES));
        }
    }
    AI_VgcBookBegin(enemy, enemySlots, nEnemy, player, playerSlots, nPlayer);

    // Piano di squadra: Trick Room conviene se il giocatore è più veloce della nostra media,
    // Tailwind se è più veloce ma lo supereremmo raddoppiando.
    {
        u32 sum = 0, fastFoes = 0, twFoes = 0;
        for (i = 0; i < nEnemy; i++)
            sum += GetMonData(&enemy[enemySlots[i]], MON_DATA_SPEED);
        sum /= nEnemy;
        for (j = 0; j < nPlayer; j++)
        {
            u32 spd = PreviewStat(&player[playerSlots[j]], MON_DATA_SPEED);
            if (spd > sum)
                fastFoes++;
            if (spd > sum && spd < sum * 2)
                twFoes++;
        }
        planTrViable = fastFoes * 2 >= nPlayer;      // almeno metà dei loro è più veloce di noi
        planTwViable = twFoes >= 1 && fastFoes >= 1;
    }

    // Ace: l'ultimo del party originale va sempre portato (e tenuto in fondo).
    if (aiFlags & (AI_FLAG_ACE_POKEMON | AI_FLAG_DOUBLE_ACE_POKEMON))
        forcedMask |= 1u << (nEnemy - 1);
    if ((aiFlags & AI_FLAG_DOUBLE_ACE_POKEMON) && nEnemy >= 2 && picks >= 2)
        forcedMask |= 1u << (nEnemy - 2);

    // Tabella matchup + peso "minaccia" di ogni Pokémon del giocatore.
    for (j = 0; j < nPlayer; j++)
    {
        s32 threat = 0;
        for (i = 0; i < nEnemy; i++)
        {
            sMatch[i][j] = Matchup(&enemy[enemySlots[i]], &player[playerSlots[j]], omniscient);
            if (sMatch[i][j] < 0)
                threat -= sMatch[i][j];
        }
        weight[j] = 100 + threat / (s32)nEnemy;
        {
            // Chi abbiamo portato (e messo in lead) nelle gare precedenti pesa di più.
            const struct SeriesMonMemory *mem = SeriesFindMemory(GetMonData(&player[playerSlots[j]], MON_DATA_PERSONALITY));
            if (mem != NULL)
                weight[j] += 40 * min(mem->timesBrought, 2) + 30 * min(mem->timesLead, 2) + 45 * min(mem->kos, 2);
            else if (j < 2 && sSeriesMemoryCount == 0) // i giocatori spesso tengono i lead in cima
                weight[j] += 25;
        }
    }
    for (i = 0; i < nEnemy; i++)
    {
        depth[i] = 0;
        for (j = 0; j < nPlayer; j++)
            depth[i] += sMatch[i][j];
        depth[i] /= (s32)max(nPlayer, 1);
    }

    // Scouting: profili (suoi veri, del giocatore stimati dalla specie) e lead probabili del giocatore.
    sMegaDesignate = SPECIES_NONE;
    for (i = 0; i < nEnemy; i++)
        EnemyProfile(&enemy[enemySlots[i]], &enProf[i]);
    for (j = 0; j < nPlayer; j++)
    {
        u32 key;
        PlayerProfile(&player[playerSlots[j]], &plProf[j]);
        // I pezzi chiave delle core (controllo velocità, meteo, Fake Out, redirect) vanno coperti di più.
        key = max(max(plProf[j].role[SCOUT_TRICK_ROOM], plProf[j].role[SCOUT_TAILWIND]),
                  max(plProf[j].weatherP, max(plProf[j].role[SCOUT_FAKE_OUT], plProf[j].role[SCOUT_REDIRECT])));
        weight[j] += key * 30 / 100;
    }
    if (isDouble && nPlayer >= 2)
    {
        s32 predS[PRED_LEADS];
        u32 k, p;
        for (j = 0; j < nPlayer; j++)
        {
            for (k = j + 1; k < nPlayer; k++)
            {
                s32 ls = PlayerLeadScore(&plProf[j], &plProf[k], &player[playerSlots[j]], &player[playerSlots[k]]);
                if (nPred < PRED_LEADS)
                    p = nPred++;
                else if (ls > predS[PRED_LEADS - 1])
                    p = PRED_LEADS - 1;
                else
                    continue;
                while (p > 0 && predS[p - 1] < ls)
                {
                    predS[p] = predS[p - 1];
                    predJ[p] = predJ[p - 1];
                    predK[p] = predK[p - 1];
                    p--;
                }
                predS[p] = ls;
                predJ[p] = j;
                predK[p] = k;
            }
        }
        // Peso di ogni lead prevista rispetto all'ultima della lista.
        for (p = 0; p < nPred; p++)
            predW[p] = predS[p] - predS[nPred - 1] + 20;
    }

    // Minacce principali (contesto): i due Pokémon del giocatore che pesano di più (duelli persi con i set della sua
    // squadra, pezzi chiave del suo piano). Un gruppo senza nessuna risposta a una di loro paga.
    if (ctx != NULL)
    {
        for (j = 0; j < nPlayer; j++)
        {
            if (nMain < 2)
                mainThreat[nMain++] = j;
            else if (weight[j] > weight[mainThreat[1]])
                mainThreat[1] = j;
            if (nMain == 2 && weight[mainThreat[1]] > weight[mainThreat[0]])
            {
                u32 tmp = mainThreat[0];
                mainThreat[0] = mainThreat[1];
                mainThreat[1] = tmp;
            }
        }
    }

    // 1) Miglior gruppo: ogni nemico del giocatore deve avere una risposta.
    for (mask = 1; mask < (1u << nEnemy); mask++)
    {
        s32 score = 0;
        u32 t;
        if (PopCount(mask) != picks || (mask & forcedMask) != forcedMask)
            continue;
        for (j = 0; j < nPlayer; j++)
        {
            s32 best = INT16_MIN;
            for (i = 0; i < nEnemy; i++)
                if (mask & (1u << i))
                    best = max(best, sMatch[i][j]);
            score += 2 * best * weight[j] / 100;
        }
        for (i = 0; i < nEnemy; i++)
            if (mask & (1u << i))
                score += depth[i] + AI_VgcPlanBringBonus(trainerId, GetMonData(&enemy[enemySlots[i]], MON_DATA_SPECIES), planTrViable, planTwViable);

        // Valutazione sinergia 2v2 e coerenza di squadra per TUTTI gli allenatori nei doppi:
        if (isDouble)
        {
            u32 i1, i2;
            u32 trSetters = 0, twSetters = 0, slowSweepers = 0, supportCount = 0;

            for (i1 = 0; i1 < nEnemy; i1++)
            {
                if (!(mask & (1u << i1)))
                    continue;

                struct Pokemon *m1 = &enemy[enemySlots[i1]];
                bool32 isTr = MonKnowsEffect(m1, EFFECT_TRICK_ROOM);
                bool32 isTw = MonKnowsEffect(m1, EFFECT_TAILWIND);

                if (isTr)
                    trSetters++;
                if (isTw)
                    twSetters++;
                if (BaseSpeed(m1) <= 50 && !isTr)
                    slowSweepers++;
                if (MonIsSupport(m1))
                    supportCount++;

                // Sinergia a coppie nei 4 scelti
                for (i2 = i1 + 1; i2 < nEnemy; i2++)
                {
                    if (mask & (1u << i2))
                    {
                        struct Pokemon *m2 = &enemy[enemySlots[i2]];
                        score += PairSynergy(m1, m2);
                    }
                }
            }

            // Coerenza del controllo velocità per TUTTI nei doppi:
            if (slowSweepers >= 2 && trSetters == 0)
                score -= 600 * slowSweepers; // Mai portare attaccanti lenti senza setter
            else if (slowSweepers >= 1 && trSetters == 0)
                score -= 300;
            else if (slowSweepers >= 1 && trSetters >= 1)
                score += 200; // Squadra coerente con Trick Room

            if (trSetters >= 3)
                score -= 400; // Troppi setter passivi
            if (trSetters > 0 && twSetters > 0)
                score -= 150; // Conflitto di velocità

            // Compensazione per Pokémon di supporto nei doppi:
            score += 150 * min(supportCount, 2);

            // Coerenza meteo / terreno per TUTTI nei doppi:
            for (i1 = 0; i1 < nEnemy; i1++)
            {
                if (!(mask & (1u << i1)))
                    continue;
                struct Pokemon *m1 = &enemy[enemySlots[i1]];
                enum Ability ab1 = GetMonAbility(m1);

                if (ab1 == ABILITY_SWIFT_SWIM || ab1 == ABILITY_CHLOROPHYLL
                 || ab1 == ABILITY_SAND_RUSH || ab1 == ABILITY_SLUSH_RUSH)
                {
                    bool32 hasSetter = FALSE;
                    for (i2 = 0; i2 < nEnemy; i2++)
                    {
                        if (mask & (1u << i2))
                        {
                            enum Ability ab2 = MonWeatherAbility(&enemy[enemySlots[i2]]);
                            if (ab1 == ABILITY_SWIFT_SWIM && (ab2 == ABILITY_DRIZZLE || ab2 == ABILITY_PRIMORDIAL_SEA))
                                hasSetter = TRUE;
                            else if (ab1 == ABILITY_CHLOROPHYLL && (ab2 == ABILITY_DROUGHT || ab2 == ABILITY_ORICHALCUM_PULSE))
                                hasSetter = TRUE;
                            else if (ab1 == ABILITY_SAND_RUSH && ab2 == ABILITY_SAND_STREAM)
                                hasSetter = TRUE;
                            else if (ab1 == ABILITY_SLUSH_RUSH && ab2 == ABILITY_SNOW_WARNING)
                                hasSetter = TRUE;
                        }
                    }
                    if (!hasSetter)
                        score -= 500; // Non portare chi dipende dal meteo senza il suo setter
                    else
                        score += 150;
                }
            }
        }

        // Risposte al piano di gioco dell'avversario letto dalla sua squadra (contesto): Trick Room, Tailwind, meteo...
        if (isDouble)
            score += ContextAnswerScore(enemy, enemySlots, nEnemy, mask, ctx);
        // Minacce principali senza nessuna risposta nel gruppo.
        for (t = 0; t < nMain; t++)
        {
            s32 best = INT16_MIN;
            for (i = 0; i < nEnemy; i++)
                if (mask & (1u << i))
                    best = max(best, (s32)sMatch[i][mainThreat[t]]);
            if (best <= 0)
                score -= (120 - best) * (s32)(gScoutContextAnswerPct ? gScoutContextAnswerPct : 100) / 100;
        }

        // Dipendenze e conflitti del gruppo ricavati dalle proprietà (grafo della squadra): Baton Passer col suo
        // ricevente, Truant col Gas Reagente, Commander con Dondozo, meteo contro attaccanti, Trick Room contro
        // meteo veloce, sinergie di coppia.
        score += GraphGroupScore(enemy, enemySlots, nEnemy, mask);

        // Troppi Pokémon deboli allo stesso tipo = male.
        for (t = TYPE_NORMAL; t < NUMBER_OF_MON_TYPES; t++)
        {
            u32 weak = 0;
            if (t == TYPE_MYSTERY || t == TYPE_STELLAR)
                continue;
            for (i = 0; i < nEnemy; i++)
                if ((mask & (1u << i)) && TypeVsMonPct(t, &enemy[enemySlots[i]]) >= 200)
                    weak++;
            if (weak >= 3)
                score -= 40 * (weak - 2);
        }
        // Gare precedenti della serie: lo stesso gruppo di 4 che ha perso vale meno, quello che ha vinto un po' di più.
        if (SeriesIsBestOfThree())
        {
            u32 g, origMask = 0;
            for (i = 0; i < nEnemy; i++)
                if (mask & (1u << i))
                    origMask |= 1u << enemySlots[i];
            for (g = 0; g < 3; g++)
            {
                if (sSeries.history[g].valid && sSeries.history[g].enemyMask == origMask)
                    score += sSeries.history[g].aiWon ? 30 : -70;
            }
        }
        // Libro dei piani (gruppo del piano scelto) ed esplorazione del self-play.
        score += AI_VgcBookGroupBonus(enemy, enemySlots, nEnemy, mask);
#if TESTING
        {
            // Diagnostica (harness di self-play VGC): punteggio di ogni gruppo di 4, maschera sugli slot originali.
            extern s32 Test_MgbaPrintf(const char *fmt, ...);
            u32 slotMask = 0;
            for (i = 0; i < nEnemy; i++)
                if (mask & (1u << i))
                    slotMask |= 1u << enemySlots[i];
            Test_MgbaPrintf("VGCTP gruppo %d %d", slotMask, score);
        }
#endif
        if (score > bestScore)
        {
            bestScore = score;
            bestMask = mask;
        }
    }

    for (i = 0; i < nEnemy; i++)
        if (bestMask & (1u << i))
            chosen[nChosen++] = i;

    // Minacce esplicite sui nostri portati (prior del pianificatore al turno 1): duello perso contro un Pokémon del
    // giocatore, pesato di più se quel Pokémon è in una delle sue lead probabili.
    {
        u32 e = sThreatNext, p, c;
        s32 wsum = 0;
        sThreatNext = (sThreatNext + 1) % 2;
        memset(sThreatSpecies[e], 0, sizeof(sThreatSpecies[e]));
        memset(sThreatVal[e], 0, sizeof(sThreatVal[e]));
        for (p = 0; p < nPred; p++)
            wsum += predW[p];
        for (c = 0; c < nChosen && ctx != NULL; c++)
        {
            s32 thr = 0;
            for (j = 0; j < nPlayer; j++)
            {
                s32 v = -sMatch[chosen[c]][j], lw = 0;
                if (v <= 0)
                    continue;
                for (p = 0; p < nPred; p++)
                    if (predJ[p] == j || predK[p] == j)
                        lw += predW[p];
                v = v * (50 + 50 * lw / max(wsum, 1)) / 100;
                thr = max(thr, v);
            }
            sThreatSpecies[e][c] = GetMonData(&enemy[enemySlots[chosen[c]]], MON_DATA_SPECIES);
            sThreatVal[e][c] = min(thr, 100);
        }
    }

    // 2) Lead (1 in singola, 2 in doppia).
    {
        u32 leadCount = (isDouble && nChosen >= 2) ? 2 : 1;
        u32 leadA = 0, leadB = 0;
        u32 a, b;
        s32 candS[3] = {0}, candWorst[3] = {0};
        u32 candA[3] = {0}, candB[3] = {0}, nCand = 0;

        for (a = 0; a < nChosen; a++)
        {
            for (b = (leadCount == 2 ? a + 1 : a); b < nChosen; b++)
            {
                s32 s = 0, worst = 0;
                u32 ea = chosen[a], eb = chosen[b];
                // L'ace non va in lead se c'è alternativa.
                if ((forcedMask & (1u << ea)) && nChosen > leadCount) s -= 200;
                if (leadCount == 2 && (forcedMask & (1u << eb)) && nChosen > leadCount) s -= 200;
                for (j = 0; j < nPlayer; j++)
                {
                    s32 m = sMatch[ea][j];
                    if (leadCount == 2)
                        m = max(m, sMatch[eb][j]);
                    s += m * weight[j] / 100;
                }
                s += LeadRoleBonus(&enemy[enemySlots[ea]], isDouble, foeHasFakeOut);
                if (leadCount == 2)
                {
                    s += LeadRoleBonus(&enemy[enemySlots[eb]], isDouble, foeHasFakeOut);
                    s += PairSynergy(&enemy[enemySlots[ea]], &enemy[enemySlots[eb]]);
                    // Piano di squadra: coppie di lead preferite, chi sta dietro non leada.
                    s += AI_VgcPlanLeadBonus(trainerId, GetMonData(&enemy[enemySlots[ea]], MON_DATA_SPECIES),
                                             GetMonData(&enemy[enemySlots[eb]], MON_DATA_SPECIES), planTrViable, planTwViable);
                    // Libro dei piani dal self-play (lead del piano scelto) ed esplorazione.
                    s += AI_VgcBookLeadBonus(GetMonData(&enemy[enemySlots[ea]], MON_DATA_SPECIES),
                                             GetMonData(&enemy[enemySlots[eb]], MON_DATA_SPECIES));

                    {
                        struct Pokemon *monA = &enemy[enemySlots[ea]];
                        struct Pokemon *monB = &enemy[enemySlots[eb]];
                        bool32 aIsTr = MonKnowsEffect(monA, EFFECT_TRICK_ROOM);
                        bool32 bIsTr = MonKnowsEffect(monB, EFFECT_TRICK_ROOM);
                        bool32 aIsSlow = BaseSpeed(monA) <= 50;
                        bool32 bIsSlow = BaseSpeed(monB) <= 50;
                        bool32 aIsSupp = MonIsSupport(monA);
                        bool32 bIsSupp = MonIsSupport(monB);
                        bool32 squadHasTr = FALSE;
                        u32 cIdx;

                        // Compensazione supporto in 2v2:
                        // I mon di supporto (TR, Fake Out, Redirection, Helping Hand) hanno sMatch 1v1 basso,
                        // ma in lead abilitano il compagno:
                        if (aIsSupp || bIsSupp)
                            s += 150;

                        // 1) Logica universale Distorsione in lead per TUTTI:
                        for (cIdx = 0; cIdx < nChosen; cIdx++)
                        {
                            if (MonKnowsEffect(&enemy[enemySlots[chosen[cIdx]]], EFFECT_TRICK_ROOM))
                            {
                                squadHasTr = TRUE;
                                break;
                            }
                        }

                        if (squadHasTr)
                        {
                            if (!aIsTr && !bIsTr)
                            {
                                // Squadra con Distorsione: mai aprire con attaccante lento senza Distorsione attiva!
                                if (aIsSlow || bIsSlow)
                                    s -= AI_VgcPlanIsTrickRoomTeam(trainerId) ? 5000 : 1200;
                                else
                                    s -= 300;
                            }
                            else if (aIsTr && bIsTr)
                            {
                                // Due setter di Distorsione insieme in apertura = zero pressione offensiva
                                s -= 3000;
                            }
                            else
                            {
                                struct Pokemon *partner = aIsTr ? monB : monA;

                                if (MonKnowsEffect(partner, EFFECT_FOLLOW_ME))
                                    s += 500; // Segui Me / Polverabbia garantisce TR
                                else if (HasFakeOut(partner))
                                    s += 400; // Bruciapelo assicura il primo turno
                                else if (MonKnowsMove(partner, MOVE_QUICK_GUARD) || MonKnowsEffect(partner, EFFECT_PROTECT))
                                    s += 200;
                                else if (!MonIsSupport(partner))
                                    s += 250; // Pressione offensiva immediata
                            }
                        }

                        // 2) Logica universale Meteo in lead per TUTTI:
                        {
                            enum Ability abA = GetMonAbility(monA);
                            enum Ability abB = GetMonAbility(monB);
                            bool32 aAbuser = (abA == ABILITY_SWIFT_SWIM || abA == ABILITY_CHLOROPHYLL || abA == ABILITY_SAND_RUSH || abA == ABILITY_SLUSH_RUSH);
                            bool32 bAbuser = (abB == ABILITY_SWIFT_SWIM || abB == ABILITY_CHLOROPHYLL || abB == ABILITY_SAND_RUSH || abB == ABILITY_SLUSH_RUSH);

                            if (aAbuser && !BenefitsFromWeather(monA, MonWeatherAbility(monB)))
                                s -= 3000; // Mai leadare abuser senza setter
                            if (bAbuser && !BenefitsFromWeather(monB, MonWeatherAbility(monA)))
                                s -= 3000;

                            if ((aAbuser && BenefitsFromWeather(monA, MonWeatherAbility(monB)))
                             || (bAbuser && BenefitsFromWeather(monB, MonWeatherAbility(monA))))
                                s += 400; // Setter + Abuser in lead
                        }

                        // 3) Logica universale Redirection + Setup in lead:
                        if ((MonKnowsEffect(monA, EFFECT_FOLLOW_ME) && MonKnowsSetupMove(monB))
                         || (MonKnowsEffect(monB, EFFECT_FOLLOW_ME) && MonKnowsSetupMove(monA)))
                            s += 400;

                        // 4) Due Pokémon puramente di supporto insieme:
                        if (MonIsPureSupport(monA) && MonIsPureSupport(monB) && !aIsTr && !bIsTr)
                            s -= 2000;
                    }

                    // Coerenza della coppia di lead dal grafo della squadra (sinergie, Commander, abilità
                    // penalizzanti, meteo, mosse ad area sul compagno).
                    s += GraphLeadScore(enemy, enemySlots[ea], enemySlots[eb]);
                    // Risposta immediata al piano dell'avversario (contesto), a metà peso rispetto al gruppo.
                    s += ContextAnswerScore(enemy, enemySlots, nEnemy, (1u << ea) | (1u << eb), ctx) / 2;
                }
                // Scouting: questa lead contro le lead probabili del giocatore. Media pesata (70%) +
                // caso peggiore (30%): si evita la lead che una contro-lead del giocatore smonta.
                if (leadCount == 2 && nPred > 0)
                {
                    s32 ev = 0, wsum = 0;
                    u32 p;
                    worst = INT32_MAX;
                    for (p = 0; p < nPred; p++)
                    {
                        u32 pj = predJ[p], pk = predK[p];
                        s32 mu = max(sMatch[ea][pj], sMatch[eb][pj]) + max(sMatch[ea][pk], sMatch[eb][pk]);
                        s32 v = LeadVsLead(&enemy[enemySlots[ea]], &enemy[enemySlots[eb]], &enProf[ea], &enProf[eb],
                                           &plProf[pj], &plProf[pk], mu);
                        ev += v * predW[p];
                        wsum += predW[p];
                        if (v < worst)
                            worst = v;
                    }
                    ev /= max(wsum, 1);
                    s += (7 * ev + 3 * worst) / 10;
                }
                // Gare precedenti: non si rifà il lead che ha perso; quello che ha vinto si ripropone.
                if (SeriesIsBestOfThree())
                {
                    u32 g, la = enemySlots[ea], lb = (leadCount == 2) ? enemySlots[eb] : 0xFF;
                    for (g = 0; g < 3; g++)
                    {
                        const u8 *h = sSeries.history[g].enemyLead;
                        bool32 same;
                        if (!sSeries.history[g].valid)
                            continue;
                        if (leadCount == 2)
                            same = (h[0] == la && h[1] == lb) || (h[0] == lb && h[1] == la);
                        else
                            same = (h[0] == la);
                        if (same)
                            s += sSeries.history[g].aiWon ? 40 : -150;
                    }
                }
#if TESTING
                {
                    // Diagnostica (harness di self-play VGC): valore di ogni coppia di lead (slot originali), caso peggiore.
                    extern s32 Test_MgbaPrintf(const char *fmt, ...);
                    Test_MgbaPrintf("VGCTP lead %d %d %d %d", enemySlots[ea], leadCount == 2 ? enemySlots[eb] : 255, s, worst);
                }
#endif
                // Le 3 lead migliori (per la scelta mista).
                {
                    u32 c;
                    if (nCand < 3)
                        c = nCand++;
                    else if (s > candS[2])
                        c = 2;
                    else
                        c = 3;
                    if (c < 3)
                    {
                        while (c > 0 && candS[c - 1] < s)
                        {
                            candS[c] = candS[c - 1];
                            candWorst[c] = candWorst[c - 1];
                            candA[c] = candA[c - 1];
                            candB[c] = candB[c - 1];
                            c--;
                        }
                        candS[c] = s;
                        candWorst[c] = worst;
                        candA[c] = a;
                        candB[c] = b;
                    }
                }
                if (leadCount == 1)
                    break;
            }
        }

        // Strategia mista: di solito la lead migliore, ma se le alternative sono vicine a volte una di
        // quelle (così il giocatore non può leggerla). Se la migliore ha un caso peggiore molto più
        // brutto della seconda (esiste una contro-lead che la smonta), la seconda diventa favorita.
        {
            u32 pick = 0;
            if (nCand >= 2 && candS[1] >= candS[0] - 40)
            {
                u32 w0 = 60, w1 = 28, w2 = (nCand >= 3 && candS[2] >= candS[0] - 40) ? 12 : 0, r;
                if (candWorst[0] + 40 < candWorst[1])
                {
                    w0 = 40;
                    w1 = 48;
                }
                r = Random() % (w0 + w1 + w2);
                pick = (r < w0) ? 0 : ((r < w0 + w1) ? 1 : 2);
            }
            if (nCand > 0)
            {
                leadA = candA[pick];
                leadB = candB[pick];
            }
        }

        outOrder[order++] = enemySlots[chosen[leadA]];
        if (leadCount == 2)
            outOrder[order++] = enemySlots[chosen[leadB]];

        // 3) Back: prima chi copre i Pokémon che i lead non gestiscono, ace per ultimo.
        while (order < nChosen)
        {
            s32 bestBack = INT32_MIN;
            u32 pick = 0, c;
            for (c = 0; c < nChosen; c++)
            {
                u32 e = chosen[c], k;
                bool32 used = FALSE;
                s32 s = 0;
                for (k = 0; k < order; k++)
                    if (outOrder[k] == enemySlots[e])
                        used = TRUE;
                if (used)
                    continue;
                for (j = 0; j < nPlayer; j++)
                {
                    s32 covered = INT16_MIN;
                    for (k = 0; k < order; k++)
                    {
                        u32 idx;
                        for (idx = 0; idx < nEnemy; idx++)
                            if (enemySlots[idx] == outOrder[k])
                                covered = max(covered, sMatch[idx][j]);
                    }
                    if (sMatch[e][j] > covered)
                        s += (sMatch[e][j] - covered) * weight[j] / 100;
                }
                if (forcedMask & (1u << e))
                    s -= 1000;
                if (s > bestBack)
                {
                    bestBack = s;
                    pick = e;
                }
            }
            outOrder[order++] = enemySlots[pick];
        }

        // Mega di questa gara: tra chi la può fare decide il matchup, non uno schema fisso.
        // Conta: essere in lead, servire come controllo velocità (Tailwind se il giocatore è più
        // veloce), il matchup contro la lead più probabile del giocatore, e un poco il piano.
        {
            s32 bestM = INT32_MIN;
            u32 aiSpdSum = 0, plSpdSum = 0, k2;
            for (k2 = 0; k2 < nChosen; k2++)
                aiSpdSum += enProf[chosen[k2]].baseSpeed;
            for (j = 0; j < nPlayer; j++)
                plSpdSum += plProf[j].baseSpeed;
            for (k2 = 0; k2 < order; k2++)
            {
                struct Pokemon *mon = &enemy[outOrder[k2]];
                u32 idx;
                s32 m;
                if (!HoldsMegaStone(mon))
                    continue;
                for (idx = 0; idx < nEnemy; idx++)
                {
                    if (enemySlots[idx] == outOrder[k2])
                        break;
                }
                if (idx >= nEnemy)
                    continue;
                m = depth[idx];
                if (k2 < leadCount)
                    m += 100;
                if (AI_VgcPlanIsWinCon(trainerId, GetMonData(mon, MON_DATA_SPECIES)))
                    m += 20;
                if (enProf[idx].role[SCOUT_TAILWIND] && plSpdSum * nChosen > aiSpdSum * max(nPlayer, 1))
                    m += 40;
                if (nPred > 0)
                    m += (sMatch[idx][predJ[0]] + sMatch[idx][predK[0]]) / 2;
                if (m > bestM)
                {
                    bestM = m;
                    sMegaDesignate = GetMonData(mon, MON_DATA_SPECIES);
                }
            }
        }
    }
    sTP.enemyPicks = order;
}

// ---------------------------------------------------------------------
// Ingresso / uscita
// ---------------------------------------------------------------------
bool32 TeamPreview_ShouldRun(void)
{
    u32 usable = CountUsable(gPlayerParty);

    if (!CUSTOM_TEAM_PREVIEW)
        return FALSE;
    if (CUSTOM_FLAG_NO_TEAM_PREVIEW != 0 && FlagGet(CUSTOM_FLAG_NO_TEAM_PREVIEW))
        return FALSE;
    if (!(gBattleTypeFlags & BATTLE_TYPE_TRAINER))
        return FALSE;
    if (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_FRONTIER | BATTLE_TYPE_MULTI
                          | BATTLE_TYPE_TWO_OPPONENTS | BATTLE_TYPE_INGAME_PARTNER | BATTLE_TYPE_SECRET_BASE
                          | BATTLE_TYPE_TRAINER_HILL | BATTLE_TYPE_EREADER_TRAINER | BATTLE_TYPE_FIRST_BATTLE
                          | BATTLE_TYPE_CATCH_TUTORIAL))
        return FALSE;
    if (B_FLAG_SKY_BATTLE != 0 && FlagGet(B_FLAG_SKY_BATTLE))
        return FALSE;
    if (usable < CUSTOM_TEAM_PREVIEW_MIN_PARTY)
        return FALSE;
    if (CountUsable(gEnemyParty) == 0)
        return FALSE;
    return TRUE;
}

void CB2_TeamPreview(void)
{
    u32 usable;

    // Serie nuova (o contro un altro allenatore): si riparte da zero e si chiede il formato.
    if (sSeries.game == 0 || sSeries.trainerId != TRAINER_BATTLE_PARAM.opponentA)
    {
        memset(&sSeries, 0, sizeof(sSeries));
        sSeriesMemoryCount = 0;
        AI_VgcResetMemory(); // anche la memoria di comportamento (Protect / cambi) riparte da zero
        sSeries.trainerId = TRAINER_BATTLE_PARAM.opponentA;
        ScalePartiesToBattleLevel(); // l'AI deve scegliere con le statistiche a Lv50
    }

    usable = CountUsable(gPlayerParty);
    memset(&sTP, 0, sizeof(sTP));
    sTP.required = min(CUSTOM_TEAM_PREVIEW_PICKS, usable);
    sTP.active = TRUE;
    if (sSeries.game == 0)
    {
        if (CUSTOM_BEST_OF_THREE && sPendingBestOf != 0)
        {
            // Formato già scelto parlando con l'allenatore.
            sSeries.game = 1;
            sSeries.bestOf = sPendingBestOf;
        }
        else if (CUSTOM_BEST_OF_THREE)
        {
            sTP.menu = MENU_FORMAT;
            sTP.menuCursor = 0;
        }
        else
        {
            sSeries.game = 1;
            sSeries.bestOf = 1;
        }
    }
    sPendingBestOf = 0;
    gMain.state = 0;
    SetMainCallback2(CB2_TeamPreviewInit);
}

static void VBlankCB_TeamPreview(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_TeamPreviewMain(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void CreateIcons(void)
{
    u32 i;
    LoadMonIconPalettes();
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *p = &gPlayerParty[i], *e = &gEnemyParty[i];
        sTP.iconIds[0][i] = MAX_SPRITES;
        sTP.iconIds[1][i] = MAX_SPRITES;
        if (GetMonData(p, MON_DATA_SPECIES) != SPECIES_NONE)
        {
            sTP.iconIds[0][i] = CreateMonIcon(GetMonData(p, MON_DATA_SPECIES_OR_EGG), SpriteCB_MonIcon,
                                              ICON_X, LIST_TOP_Y + ROW_H * i + 10, 1, GetMonData(p, MON_DATA_PERSONALITY));
            gSprites[sTP.iconIds[0][i]].oam.priority = 1;
        }
        if (GetMonData(e, MON_DATA_SPECIES) != SPECIES_NONE)
        {
            sTP.iconIds[1][i] = CreateMonIcon(GetMonData(e, MON_DATA_SPECIES), SpriteCB_MonIcon,
                                              120 + ICON_X, LIST_TOP_Y + ROW_H * i + 10, 1, GetMonData(e, MON_DATA_PERSONALITY));
            gSprites[sTP.iconIds[1][i]].oam.priority = 1;
            gSprites[sTP.iconIds[1][i]].callback = SpriteCallbackDummy; // fermi: lato avversario
        }
    }
}

static void UpdateIconAnims(void)
{
    u32 i;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (sTP.iconIds[0][i] == MAX_SPRITES)
            continue;
        // Solo il Pokémon sotto il cursore si muove.
        gSprites[sTP.iconIds[0][i]].callback = (i == sTP.cursor) ? SpriteCB_MonIcon : SpriteCallbackDummy;
    }
}

static void CB2_TeamPreviewInit(void)
{
    switch (gMain.state)
    {
    case 0:
        SetVBlankCallback(NULL);
        SetHBlankCallback(NULL);
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        gMain.state++;
        break;
    case 1:
        DmaClearLarge16(3, (void *)(VRAM), VRAM_SIZE, 0x1000);
        DmaClear32(3, OAM, OAM_SIZE);
        DmaClear16(3, PLTT, PLTT_SIZE);
        ScanlineEffect_Stop();
        ResetPaletteFade();
        ResetTasks();
        ResetSpriteData();
        FreeAllSpritePalettes();
        gMain.state++;
        break;
    case 2:
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sBgTemplates, ARRAY_COUNT(sBgTemplates));
        sTilemaps[0] = AllocZeroed(BG_SCREEN_SIZE);
        sTilemaps[1] = AllocZeroed(BG_SCREEN_SIZE);
        SetBgTilemapBuffer(0, sTilemaps[0]);
        SetBgTilemapBuffer(1, sTilemaps[1]);
        ChangeBgX(0, 0, BG_COORD_SET);
        ChangeBgY(0, 0, BG_COORD_SET);
        ChangeBgX(1, 0, BG_COORD_SET);
        ChangeBgY(1, 0, BG_COORD_SET);
        InitWindows(sWinTemplates);
        DeactivateAllTextPrinters();
        gMain.state++;
        break;
    case 3:
        LoadPalette(sPreviewPal, BG_PLTT_ID(0), sizeof(sPreviewPal));
        LoadUserWindowBorderGfx(WIN_MENU, FRAME_TILE, BG_PLTT_ID(FRAME_PAL));
        LoadPalette(GetOverworldTextboxPalettePtr(), BG_PLTT_ID(MENU_PAL), PLTT_SIZE_4BPP);
        gMain.state++;
        break;
    case 4:
        if (!sTP.aiDone)
        {
            TeamPreview_AiSelectEnemyTeam(CUSTOM_TEAM_PREVIEW_PICKS, (gBattleTypeFlags & BATTLE_TYPE_DOUBLE) != 0, sTP.enemyOrder);
            sTP.aiDone = TRUE;
        }
        CreateIcons();
        UpdateIconAnims();
        gMain.state++;
        break;
    case 5:
        PutWindowTilemap(WIN_HEADER);
        PutWindowTilemap(WIN_PLAYER);
        PutWindowTilemap(WIN_ENEMY);
        DrawAll();
        if (sTP.menu != MENU_NONE)
            DrawMenu();
        CopyBgTilemapBufferToVram(0);
        CopyBgTilemapBufferToVram(1);
        gMain.state++;
        break;
    default:
        ShowBg(0);
        ShowBg(1);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP | DISPCNT_BG0_ON | DISPCNT_BG1_ON);
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB_TeamPreview);
        SetMainCallback2(CB2_TeamPreviewMain);
        CreateTask(Task_TeamPreviewInput, 0);
        gMain.state = 0;
        break;
    }
}

static void FreeScreen(void)
{
    u32 i, side;
    for (side = 0; side < 2; side++)
    {
        for (i = 0; i < PARTY_SIZE; i++)
        {
            if (sTP.iconIds[side][i] != MAX_SPRITES)
            {
                FreeAndDestroyMonIconSprite(&gSprites[sTP.iconIds[side][i]]);
                sTP.iconIds[side][i] = MAX_SPRITES;
            }
        }
    }
    FreeMonIconPalettes();
    FreeAllWindowBuffers();
    for (i = 0; i < 2; i++)
    {
        UnsetBgTilemapBuffer(i);
        TRY_FREE_AND_SET_NULL(sTilemaps[i]);
    }
}

// ---------------------------------------------------------------------
// Disegno
// ---------------------------------------------------------------------
static void DrawHeader(void)
{
    u8 *ptr;
    u32 left = sTP.required - sTP.numPicks;

    FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(COL_HEADER));
    FillWindowPixelRect(WIN_HEADER, PIXEL_FILL(COL_P_LIGHT), 0, 15, 120, 1);
    FillWindowPixelRect(WIN_HEADER, PIXEL_FILL(COL_E_LIGHT), 120, 15, 120, 1);

    // Sinistra: "SCEGLI 4 POKéMON" + contatore allineato a destra.
    if (SeriesIsBestOfThree())
    {
        // "G2 1-0  SCEGLI 4": gara e punteggio della serie
        u8 *end;
        ConvertIntToDecimalStringN(gStringVar1, sSeries.game, STR_CONV_MODE_LEFT_ALIGN, 1);
        ConvertIntToDecimalStringN(gStringVar2, sSeries.playerWins, STR_CONV_MODE_LEFT_ALIGN, 1);
        ConvertIntToDecimalStringN(gStringVar3, sSeries.enemyWins, STR_CONV_MODE_LEFT_ALIGN, 1);
        end = StringExpandPlaceholders(gStringVar4, sText_SeriesHeader);
        ConvertIntToDecimalStringN(end, sTP.required, STR_CONV_MODE_LEFT_ALIGN, 1);
    }
    else
    {
        ConvertIntToDecimalStringN(gStringVar1, sTP.required, STR_CONV_MODE_LEFT_ALIGN, 1);
        StringExpandPlaceholders(gStringVar4, sText_Header);
    }
    AddTextPrinterParameterized3(WIN_HEADER, FONT_SMALL, 4, 2, sColWhite, TEXT_SKIP_DRAW, gStringVar4);
    ptr = ConvertIntToDecimalStringN(gStringVar2, sTP.numPicks, STR_CONV_MODE_LEFT_ALIGN, 1);
    ptr = StringCopy(ptr, sText_Slash);
    ConvertIntToDecimalStringN(ptr, sTP.required, STR_CONV_MODE_LEFT_ALIGN, 1);
    AddTextPrinterParameterized3(WIN_HEADER, FONT_SMALL, 116 - GetStringWidth(FONT_SMALL, gStringVar2, 0), 2,
                                 left == 0 ? sColGreen : sColYellow, TEXT_SKIP_DRAW, gStringVar2);

    // Destra: cosa puoi fare adesso.
    if (sTP.menu == MENU_FORMAT)
    {
        AddTextPrinterParameterized3(WIN_HEADER, GetFontIdToFit(sText_FormatHint, FONT_SMALL, 0, 112), 126, 2, sColYellow, TEXT_SKIP_DRAW, sText_FormatHint);
    }
    else if (left == 0)
    {
        AddTextPrinterParameterized3(WIN_HEADER, FONT_SMALL, 126, 2, sColGreen, TEXT_SKIP_DRAW, sText_Ready);
    }
    else if (sTP.warn)
    {
        ConvertIntToDecimalStringN(gStringVar1, left, STR_CONV_MODE_LEFT_ALIGN, 1);
        StringExpandPlaceholders(gStringVar4, sText_Missing);
        AddTextPrinterParameterized3(WIN_HEADER, FONT_SMALL, 126, 2, sColOrange, TEXT_SKIP_DRAW, gStringVar4);
    }
    else
    {
        AddTextPrinterParameterized3(WIN_HEADER, FONT_SMALL, 126, 2, sColWhite, TEXT_SKIP_DRAW, sText_Hint);
    }
    CopyWindowToVram(WIN_HEADER, COPYWIN_FULL);
}

// Scheda arrotondata stile slot del menu squadra.
static void DrawCard(u32 win, u32 rowY, u32 fill, u32 border, u32 light, u32 thick)
{
    u32 x = CARD_X, y = rowY + 1;

    FillWindowPixelRect(win, PIXEL_FILL(border), x, y, CARD_W, CARD_H);
    FillWindowPixelRect(win, PIXEL_FILL(fill), x + thick, y + thick, CARD_W - 2 * thick, CARD_H - 2 * thick);
    FillWindowPixelRect(win, PIXEL_FILL(light), x + thick, y + thick, CARD_W - 2 * thick, 1);
    FillWindowPixelRect(win, PIXEL_FILL(COL_BG), x, y, 1, 1);
    FillWindowPixelRect(win, PIXEL_FILL(COL_BG), x + CARD_W - 1, y, 1, 1);
    FillWindowPixelRect(win, PIXEL_FILL(COL_BG), x, y + CARD_H - 1, 1, 1);
    FillWindowPixelRect(win, PIXEL_FILL(COL_BG), x + CARD_W - 1, y + CARD_H - 1, 1, 1);
}

// Prime 3 lettere del nome del tipo (es. "Fir", "Wat").
static u8 *AppendTypeAbbr(u8 *ptr, enum Type type)
{
    const u8 *name = gTypesInfo[type].name;
    u32 i;

    for (i = 0; i < 3 && name[i] != EOS; i++)
        *ptr++ = name[i];
    *ptr = EOS;
    return ptr;
}

// Nome sulla prima riga; sulla seconda i tipi e, per i nostri, lo strumento tenuto.
static void DrawMonText(u32 win, struct Pokemon *mon, u32 y, const u8 *colors, bool32 showItem)
{
    u8 *ptr;
    enum Species species;
    enum Type type1, type2;

    if (GetMonData(mon, MON_DATA_IS_EGG))
    {
        AddTextPrinterParameterized3(win, FONT_NARROW, TEXT_X, y + 1, colors, TEXT_SKIP_DRAW, sText_Egg);
        return;
    }
    GetMonData(mon, MON_DATA_NICKNAME, gStringVar1);
    StringGet_Nickname(gStringVar1);
    AddTextPrinterParameterized3(win, FONT_NARROW, TEXT_X, y + 1, colors, TEXT_SKIP_DRAW, gStringVar1);

    if (showItem && GetMonData(mon, MON_DATA_HP) == 0)
        return; // al posto della seconda riga c'è "KO"

    species = GetMonData(mon, MON_DATA_SPECIES);
    type1 = GetSpeciesType(species, 0);
    type2 = GetSpeciesType(species, 1);
    ptr = AppendTypeAbbr(gStringVar2, type1);
    if (type2 != type1)
    {
        ptr = StringCopy(ptr, sText_Slash);
        ptr = AppendTypeAbbr(ptr, type2);
    }
    if (showItem)
    {
        enum Item item = GetMonData(mon, MON_DATA_HELD_ITEM);
        if (item != ITEM_NONE)
        {
            ptr = StringCopy(ptr, sText_Space);
            CopyItemName(item, ptr);
        }
    }
    AddTextPrinterParameterized3(win, GetFontIdToFit(gStringVar2, FONT_SMALL, 0, 82), TEXT_X, y + 11, colors, TEXT_SKIP_DRAW, gStringVar2);
}

static void DrawPlayerPanel(void)
{
    u32 i;
    FillWindowPixelBuffer(WIN_PLAYER, PIXEL_FILL(COL_BG));
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gPlayerParty[i];
        u32 y = ROW_H * i;
        const u8 *col = sColWhite;

        if (GetMonData(mon, MON_DATA_SPECIES) == SPECIES_NONE)
        {
            DrawCard(WIN_PLAYER, y, COL_BG, COL_P_BORDER, COL_BG, 1); // slot vuoto
            continue;
        }
        if (i == sTP.cursor)
            DrawCard(WIN_PLAYER, y, COL_SEL_FILL, COL_ORANGE, COL_P_LIGHT, 2);
        else
            DrawCard(WIN_PLAYER, y, COL_P_FILL, COL_P_BORDER, COL_P_LIGHT, 1);
        if (!IsMonUsable(mon))
            col = sColGrey;
        DrawMonText(WIN_PLAYER, mon, y, col, TRUE);
        if (!GetMonData(mon, MON_DATA_IS_EGG) && GetMonData(mon, MON_DATA_HP) == 0)
            AddTextPrinterParameterized3(WIN_PLAYER, FONT_SMALL, TEXT_X, y + 11, sColOrange, TEXT_SKIP_DRAW, sText_KO);
        if (sTP.pickOrder[i])
        {
            // Medaglietta gialla con l'ordine di scelta.
            FillWindowPixelRect(WIN_PLAYER, PIXEL_FILL(COL_SHADOW), 99, y + 4, 16, 16);
            FillWindowPixelRect(WIN_PLAYER, PIXEL_FILL(COL_YELLOW), 100, y + 5, 14, 14);
            ConvertIntToDecimalStringN(gStringVar3, sTP.pickOrder[i], STR_CONV_MODE_LEFT_ALIGN, 1);
            AddTextPrinterParameterized3(WIN_PLAYER, FONT_NORMAL, 104, y + 4, sColNumber, TEXT_SKIP_DRAW, gStringVar3);
        }
    }
    CopyWindowToVram(WIN_PLAYER, COPYWIN_FULL);
}

static void DrawEnemyPanel(void)
{
    u32 i;
    struct Pokemon *hover = &gPlayerParty[sTP.cursor];
    bool32 showEff = IsMonUsable(hover);

    FillWindowPixelBuffer(WIN_ENEMY, PIXEL_FILL(COL_BG));
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gEnemyParty[i];
        u32 y = ROW_H * i;

        if (GetMonData(mon, MON_DATA_SPECIES) == SPECIES_NONE)
        {
            DrawCard(WIN_ENEMY, y, COL_BG, COL_E_BORDER, COL_BG, 1); // slot vuoto
            continue;
        }
        DrawCard(WIN_ENEMY, y, COL_E_FILL, COL_E_BORDER, COL_E_LIGHT, 1);
        DrawMonText(WIN_ENEMY, mon, y, sColWhite, FALSE);

        if (showEff)
        {
            u32 eff = PreviewEffectiveness(hover, mon);
            const u8 *txt = NULL;
            const u8 *col = sColWhite;
            if (eff == 0)        { txt = sText_Immune;  col = sColWhite;  }
            else if (eff >= 400) { txt = sText_Iper;    col = sColYellow; }
            else if (eff >= 200) { txt = sText_Super;   col = sColYellow; }
            else if (eff <= 25)  { txt = sText_Resist4; col = sColCyan;   }
            else if (eff < 100)  { txt = sText_Resist;  col = sColCyan;   }
            if (txt != NULL)
                AddTextPrinterParameterized3(WIN_ENEMY, FONT_NORMAL, 114 - GetStringWidth(FONT_NORMAL, txt, 0), y + 4,
                                             col, TEXT_SKIP_DRAW, txt);
        }
        if (CUSTOM_TEAM_PREVIEW_REVEAL_AI)
        {
            u32 k;
            for (k = 0; k < sTP.enemyPicks; k++)
            {
                if (sTP.enemyOrder[k] == i)
                {
                    ConvertIntToDecimalStringN(gStringVar3, k + 1, STR_CONV_MODE_LEFT_ALIGN, 1);
                    AddTextPrinterParameterized3(WIN_ENEMY, FONT_SMALL, 74, y + 11, sColYellow, TEXT_SKIP_DRAW, gStringVar3);
                }
            }
        }
    }
    CopyWindowToVram(WIN_ENEMY, COPYWIN_FULL);
}

static void DrawAll(void)
{
    sTP.warn = FALSE;
    DrawHeader();
    DrawPlayerPanel();
    DrawEnemyPanel();
    UpdateIconAnims();
}

static u32 MenuItemCount(void)
{
    return (sTP.menu == MENU_ACTION) ? 3 : 2;
}

static void DrawMenu(void)
{
    const u8 *items[3];
    u32 i, n = MenuItemCount();

    if (sTP.menu == MENU_ACTION)
    {
        items[0] = sText_Info;
        items[1] = sTP.pickOrder[sTP.cursor] ? sText_Unpick : sText_Pick;
        items[2] = sText_Cancel;
    }
    else if (sTP.menu == MENU_FORMAT)
    {
        items[0] = sText_Bo1;
        items[1] = sText_Bo3;
    }
    else
    {
        items[0] = sText_Fight;
        items[1] = sText_Back;
    }
    // La cornice va prima del testo: DrawStdFrame... riempie la finestra di bianco.
    DrawStdFrameWithCustomTileAndPalette(WIN_MENU, FALSE, FRAME_TILE, FRAME_PAL);
    for (i = 0; i < n; i++)
    {
        if (i == sTP.menuCursor)
            AddTextPrinterParameterized3(WIN_MENU, FONT_NORMAL, 0, 16 * i, sColMenu, TEXT_SKIP_DRAW, sText_Cursor);
        AddTextPrinterParameterized3(WIN_MENU, FONT_NORMAL, 8, 16 * i, sColMenu, TEXT_SKIP_DRAW, items[i]);
    }
    CopyWindowToVram(WIN_MENU, COPYWIN_FULL);
    ScheduleBgCopyTilemapToVram(1);
}

static void OpenMenu(u32 menu, u32 cursor)
{
    sTP.menu = menu;
    sTP.menuCursor = cursor;
    DrawMenu();
}

static void CloseMenu(void)
{
    sTP.menu = MENU_NONE;
    ClearStdWindowAndFrameToTransparent(WIN_MENU, TRUE);
    ScheduleBgCopyTilemapToVram(1);
}

// ---------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------
static void Unpick(u32 slot)
{
    u32 i, num = sTP.pickOrder[slot];
    if (!num)
        return;
    for (i = 0; i < PARTY_SIZE; i++)
        if (sTP.pickOrder[i] > num)
            sTP.pickOrder[i]--;
    sTP.pickOrder[slot] = 0;
    sTP.numPicks--;
}

static bool32 TryPick(u32 slot)
{
    if (sTP.pickOrder[slot] || !IsMonUsable(&gPlayerParty[slot]) || sTP.numPicks >= sTP.required)
        return FALSE;
    sTP.pickOrder[slot] = ++sTP.numPicks;
    return TRUE;
}

// SELECT: rimette i Pokémon scelti nell'ultima lotta, nello stesso ordine
// (quelli che non ci sono più o non possono lottare vengono saltati).
static bool32 RepickLast(void)
{
    u32 i, k;

    if (sLastPickCount == 0)
        return FALSE;
    memset(sTP.pickOrder, 0, sizeof(sTP.pickOrder));
    sTP.numPicks = 0;
    for (k = 0; k < sLastPickCount; k++)
    {
        for (i = 0; i < PARTY_SIZE; i++)
        {
            if (GetMonData(&gPlayerParty[i], MON_DATA_SPECIES) != SPECIES_NONE
             && GetMonData(&gPlayerParty[i], MON_DATA_PERSONALITY) == sLastPickPersonality[k]
             && TryPick(i))
                break;
        }
    }
    return sTP.numPicks != 0;
}

static void MoveCursor(s32 delta)
{
    u32 count = gPlayerPartyCount;
    if (count == 0)
        return;
    sTP.cursor = (sTP.cursor + count + delta) % count;
    PlaySE(SE_SELECT);
    DrawAll();
}

static void Task_TeamPreviewInput(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    if (sTP.menu == MENU_NONE)
    {
        if (JOY_NEW(DPAD_UP))
            MoveCursor(-1);
        else if (JOY_NEW(DPAD_DOWN))
            MoveCursor(1);
        else if (JOY_NEW(A_BUTTON))
        {
            PlaySE(SE_SELECT);
            OpenMenu(MENU_ACTION, 1);
        }
        else if (JOY_NEW(B_BUTTON))
        {
            // B: togli l'ultimo scelto.
            u32 i;
            for (i = 0; i < PARTY_SIZE; i++)
            {
                if (sTP.numPicks && sTP.pickOrder[i] == sTP.numPicks)
                {
                    Unpick(i);
                    PlaySE(SE_SELECT);
                    DrawAll();
                    break;
                }
            }
        }
        else if (JOY_NEW(SELECT_BUTTON))
        {
            if (RepickLast())
            {
                PlaySE(SE_SELECT);
                DrawAll();
                if (sTP.numPicks == sTP.required)
                    OpenMenu(MENU_CONFIRM, 0);
            }
            else
            {
                PlaySE(SE_FAILURE);
                DrawAll();
            }
        }
        else if (JOY_NEW(START_BUTTON))
        {
            if (sTP.numPicks == sTP.required)
            {
                PlaySE(SE_SELECT);
                OpenMenu(MENU_CONFIRM, 0);
            }
            else
            {
                PlaySE(SE_FAILURE);
                sTP.warn = TRUE;
                DrawHeader();
            }
        }
        return;
    }

    if (JOY_NEW(DPAD_UP))
    {
        sTP.menuCursor = (sTP.menuCursor + MenuItemCount() - 1) % MenuItemCount();
        PlaySE(SE_SELECT);
        DrawMenu();
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        sTP.menuCursor = (sTP.menuCursor + 1) % MenuItemCount();
        PlaySE(SE_SELECT);
        DrawMenu();
    }
    else if (JOY_NEW(B_BUTTON))
    {
        if (sTP.menu == MENU_FORMAT)
            return; // il formato va scelto
        PlaySE(SE_SELECT);
        CloseMenu();
    }
    else if (JOY_NEW(A_BUTTON))
    {
        if (sTP.menu == MENU_FORMAT)
        {
            PlaySE(SE_SELECT);
            sSeries.game = 1;
            sSeries.bestOf = (sTP.menuCursor == 1) ? 3 : 1;
            CloseMenu();
            DrawAll();
            return;
        }
        if (sTP.menu == MENU_ACTION)
        {
            switch (sTP.menuCursor)
            {
            case 0: // Info
                PlaySE(SE_SELECT);
                sTP.menu = MENU_NONE;
                BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
                gTasks[taskId].func = Task_TeamPreviewExitToSummary;
                return;
            case 1: // Scegli / Togli
                if (sTP.pickOrder[sTP.cursor])
                {
                    Unpick(sTP.cursor);
                    PlaySE(SE_SELECT);
                }
                else if (TryPick(sTP.cursor))
                {
                    PlaySE(SE_SELECT);
                }
                else
                {
                    PlaySE(SE_FAILURE);
                    return;
                }
                CloseMenu();
                DrawAll();
                if (sTP.numPicks == sTP.required)
                    OpenMenu(MENU_CONFIRM, 0);
                return;
            default: // Annulla
                PlaySE(SE_SELECT);
                CloseMenu();
                return;
            }
        }
        else // MENU_CONFIRM
        {
            if (sTP.menuCursor == 0 && sTP.numPicks != sTP.required)
            {
                PlaySE(SE_FAILURE);
                CloseMenu();
                return;
            }
            PlaySE(SE_SELECT);
            if (sTP.menuCursor == 0)
            {
                CloseMenu();
                BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
                gTasks[taskId].func = Task_TeamPreviewExitToBattle;
            }
            else
            {
                CloseMenu();
            }
        }
    }
}

static void Task_TeamPreviewExitToSummary(u8 taskId)
{
    if (gPaletteFade.active)
        return;
    DestroyTask(taskId);
    FreeScreen();
    SetVBlankCallback(NULL);
    // LOCK_MOVES: niente rinomina / ricorda-mosse / riordino mosse da qui (escono verso altre schermate).
    ShowPokemonSummaryScreen(SUMMARY_MODE_LOCK_MOVES, gPlayerParty, sTP.cursor, gPlayerPartyCount - 1, CB2_ReturnFromSummary);
}

static void CB2_ReturnFromSummary(void)
{
    if (gLastViewedMonIndex < gPlayerPartyCount)
        sTP.cursor = gLastViewedMonIndex;
    gMain.state = 0;
    SetMainCallback2(CB2_TeamPreviewInit);
}

static void Task_TeamPreviewExitToBattle(u8 taskId)
{
    if (gPaletteFade.active)
        return;
    DestroyTask(taskId);
    FreeScreen();
    SetVBlankCallback(NULL);
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    ApplyPartySelections();
    sTP.active = FALSE;
    SetMainCallback2(CB2_InitBattle);
}

// ---------------------------------------------------------------------
// Applica / ripristina squadre
// ---------------------------------------------------------------------
static void ApplyPartySelections(void)
{
    u32 i, k;

    // Giocatore: salva tutto, poi tieni solo i scelti in ordine.
    for (i = 0; i < PARTY_SIZE; i++)
        sSavedParty[i] = gPlayerParty[i];
    sSavedPicks = sTP.numPicks;
    for (k = 1; k <= sTP.numPicks; k++)
        for (i = 0; i < PARTY_SIZE; i++)
            if (sTP.pickOrder[i] == k)
                sSavedOrder[k - 1] = i;
    sLastPickCount = sSavedPicks;
    for (k = 0; k < sSavedPicks; k++)
        sLastPickPersonality[k] = GetMonData(&sSavedParty[sSavedOrder[k]], MON_DATA_PERSONALITY);

    // Serie: serve la squadra avversaria intera per le gare successive, e l'AI prende nota
    // di chi abbiamo portato e messo in lead.
    sSeries.battleTypeFlags = gBattleTypeFlags;
    sSeries.savedCallback = gMain.savedCallback;
    for (i = 0; i < PARTY_SIZE; i++)
        sSeriesEnemyParty[i] = gEnemyParty[i];
    for (k = 0; k < sSavedPicks; k++)
    {
        struct SeriesMonMemory *mem = SeriesFindOrAddMemory(sLastPickPersonality[k]);
        if (mem == NULL)
            continue;
        if (mem->timesBrought < 255)
            mem->timesBrought++;
        if (k < ((gBattleTypeFlags & BATTLE_TYPE_DOUBLE) ? 2u : 1u) && mem->timesLead < 255)
            mem->timesLead++;
    }

    for (k = 0; k < PARTY_SIZE; k++)
    {
        if (k < sSavedPicks)
            gPlayerParty[k] = sSavedParty[sSavedOrder[k]];
        else
            ZeroMonData(&gPlayerParty[k]);
    }
    CalculatePlayerPartyCount();
    sPartyReduced = TRUE;

    // Avversario: ordine scelto dall'AI.
    if (sTP.enemyPicks)
    {
        struct Pokemon *tmp = Alloc(sizeof(struct Pokemon) * PARTY_SIZE);
        if (tmp != NULL)
        {
            for (i = 0; i < PARTY_SIZE; i++)
            {
                tmp[i] = gEnemyParty[i];
                sEnemyPreviewSpecies[i] = GetMonData(&tmp[i], MON_DATA_SPECIES);
                sEnemyPreviewBattleSlot[i] = 0xFF;
            }
            for (k = 0; k < sTP.enemyPicks; k++)
                sEnemyPreviewBattleSlot[sTP.enemyOrder[k]] = k;
            sEnemyPreviewValid = TRUE;
            // Serie: si annota il piano di questa gara (lead e gruppo portato).
            if (SeriesIsBestOfThree() && sSeries.game >= 1 && sSeries.game <= 3)
            {
                u32 g = sSeries.game - 1;
                sSeries.history[g].valid = TRUE;
                sSeries.history[g].aiWon = FALSE;
                sSeries.history[g].enemyLead[0] = sTP.enemyOrder[0];
                sSeries.history[g].enemyLead[1] = (sTP.enemyPicks >= 2) ? sTP.enemyOrder[1] : 0xFF;
                sSeries.history[g].enemyMask = 0;
                for (k = 0; k < sTP.enemyPicks; k++)
                    sSeries.history[g].enemyMask |= 1u << sTP.enemyOrder[k];
            }
            for (k = 0; k < PARTY_SIZE; k++)
            {
                if (k < sTP.enemyPicks)
                    gEnemyParty[k] = tmp[sTP.enemyOrder[k]];
                else
                    ZeroMonData(&gEnemyParty[k]);
            }
            Free(tmp);
            CalculateEnemyPartyCount();
        }
    }
}

// Pokémon avversario numero `index` dell'anteprima (ordine originale).
// battleSlot = slot nella squadra in lotta, PARTY_SIZE se non è stato portato.
// FALSE se questa lotta non ha avuto anteprima o lo slot è vuoto.
bool32 TeamPreview_GetEnemyPreviewMon(u32 index, u16 *species, u32 *battleSlot)
{
    if (!sEnemyPreviewValid || index >= PARTY_SIZE || sEnemyPreviewSpecies[index] == SPECIES_NONE)
        return FALSE;
    *species = sEnemyPreviewSpecies[index];
    *battleSlot = (sEnemyPreviewBattleSlot[index] < PARTY_SIZE) ? sEnemyPreviewBattleSlot[index] : PARTY_SIZE;
    return TRUE;
}

bool32 TeamPreview_HasEnemyPreview(void)
{
    return sEnemyPreviewValid;
}

// Chiamata mentre la lotta sta finendo, prima che i dati dell'AI vengano liberati:
// la squadra è ancora quella ridotta, quindi gli slot coincidono con quelli dell'AI.
void TeamPreview_SeriesCaptureAiKnowledge(void)
{
    u32 slot, i;

    if (!SeriesIsBestOfThree() || gAiPartyData == NULL || !sPartyReduced)
        return;
    for (slot = 0; slot < PARTY_SIZE; slot++)
    {
        struct Pokemon *mon = &gPlayerParty[slot];
        const struct AiPartyMon *known = &gAiPartyData->mons[B_TRAINER_PLAYER][slot];
        struct SeriesMonMemory *mem;

        if (GetMonData(mon, MON_DATA_SPECIES) == SPECIES_NONE || !known->wasSentInBattle)
            continue;
        mem = SeriesFindOrAddMemory(GetMonData(mon, MON_DATA_PERSONALITY));
        if (mem == NULL)
            continue;
        for (i = 0; i < MAX_MON_MOVES; i++)
            SeriesRememberMove(mem, known->moves[i]);
        if (known->ability != ABILITY_NONE)
            mem->ability = known->ability;
        Scout_BeliefExport(slot, &mem->belief); // belief dell'IA (M1): passa alla gara dopo
        if (known->heldEffect != HOLD_EFFECT_NONE)
        {
            u16 real = GetMonData(mon, MON_DATA_HELD_ITEM);
            mem->heldEffect = known->heldEffect;
            // L'oggetto preciso si ricorda solo se l'effetto visto/dedotto corrisponde davvero
            // (una deduzione sbagliata non deve rivelare lo strumento vero).
            mem->item = (GetItemHoldEffect(real) == known->heldEffect) ? real : ITEM_NONE;
        }
    }
}

// Chiamata all'inizio di ogni lotta, dopo che l'AI ha azzerato ciò che sa.
void TeamPreview_SeriesInjectAiKnowledge(void)
{
    u32 slot, i, j;

    if (!SeriesIsBestOfThree() || gAiPartyData == NULL)
        return;
    for (slot = 0; slot < PARTY_SIZE; slot++)
    {
        struct Pokemon *mon = &gPlayerParty[slot];
        struct AiPartyMon *known = &gAiPartyData->mons[B_TRAINER_PLAYER][slot];
        const struct SeriesMonMemory *mem;

        if (GetMonData(mon, MON_DATA_SPECIES) == SPECIES_NONE)
            continue;
        mem = SeriesFindMemory(GetMonData(mon, MON_DATA_PERSONALITY));
        if (mem == NULL)
            continue;
        // Le mosse vanno nello stesso slot in cui il Pokémon le ha davvero.
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            u16 actual = GetMonData(mon, MON_DATA_MOVE1 + i);
            if (actual == MOVE_NONE)
                continue;
            for (j = 0; j < MAX_MON_MOVES; j++)
            {
                if (mem->moves[j] == actual)
                    known->moves[i] = actual;
            }
        }
        if (mem->ability != ABILITY_NONE)
            known->ability = mem->ability;
        if (mem->heldEffect != HOLD_EFFECT_NONE)
        {
            known->heldEffect = mem->heldEffect;
            if (mem->item != ITEM_NONE && GetMonData(mon, MON_DATA_HELD_ITEM) == mem->item)
                known->item = mem->item;
        }
        if (mem->timesBrought != 0)
            known->species = GetMonData(mon, MON_DATA_SPECIES);
        Scout_BeliefImport(slot, &mem->belief);

        // Senza questo, la prima volta che il Pokémon entra in campo l'IA azzerava la sua storia
        // (lo considerava "mai visto") e le mosse ricordate dalla gara precedente andavano perse.
        known->wasSentInBattle = TRUE;
        known->species = GetMonData(mon, MON_DATA_SPECIES);
        known->level = GetMonData(mon, MON_DATA_LEVEL);
        {
            enum BattlerId b;
            for (b = 0; b < gBattlersCount; b++)
            {
                if (!IsOnPlayerSide(b) || gBattlerPartyIndexes[b] != slot)
                    continue;
                for (i = 0; i < MAX_MON_MOVES; i++)
                {
                    if (known->moves[i] != MOVE_NONE)
                        gBattleHistory->usedMoves[b][i] = known->moves[i];
                }
                if (known->ability != ABILITY_NONE)
                    gBattleHistory->abilities[b] = known->ability;
                if (known->heldEffect != HOLD_EFFECT_NONE)
                    gBattleHistory->itemEffects[b] = known->heldEffect;
            }
        }
    }
}

// Un Pokémon è andato KO: se era dell'AI e l'ha mandato KO un Pokémon del giocatore, se lo segna.
void TeamPreview_SeriesNoteFaint(enum BattlerId fainted)
{
    struct SeriesMonMemory *mem;
    enum BattlerId attacker = gBattlerAttacker;

    if (!SeriesIsBestOfThree() || !(gBattleTypeFlags & BATTLE_TYPE_TRAINER))
        return;
    if (IsOnPlayerSide(fainted) || attacker >= gBattlersCount || !IsOnPlayerSide(attacker))
        return;
    if (gBattleMons[attacker].hp == 0)
        return;
    mem = SeriesFindOrAddMemory(GetMonData(GetBattlerMon(attacker), MON_DATA_PERSONALITY));
    if (mem != NULL && mem->kos < 255)
        mem->kos++;
}

// A fine lotta. TRUE = la serie non è decisa: squadre rimesse a posto e nuova team preview.
bool32 TeamPreview_SeriesHandleBattleEnd(void)
{
    u32 i;

    if (!SeriesIsBestOfThree())
    {
        sSeries.game = 0;
        return FALSE;
    }
    if (gBattleOutcome == B_OUTCOME_WON)
        sSeries.playerWins++;
    else
        sSeries.enemyWins++;
    if (sSeries.game >= 1 && sSeries.game <= 3)
        sSeries.history[sSeries.game - 1].aiWon = (gBattleOutcome != B_OUTCOME_WON);

    if (sSeries.playerWins >= 2 || sSeries.enemyWins >= 2 || sSeries.game >= 3)
    {
        // Serie finita: l'esito dell'ultima gara è quello della serie, il resto lo fa il gioco.
        sSeries.game = 0;
        return FALSE;
    }

    sSeries.game++;
    HealPlayerParty();
    for (i = 0; i < PARTY_SIZE; i++)
        gEnemyParty[i] = sSeriesEnemyParty[i];
    CalculateEnemyPartyCount();
    gBattleTypeFlags = sSeries.battleTypeFlags;
    gMain.savedCallback = sSeries.savedCallback;
    PlayBattleBGM();
    SetMainCallback2(CB2_TeamPreview);
    return TRUE;
}

static void RestoreReducedParty(void)
{
    u32 k;

    if (!sPartyReduced)
        return;
    sPartyReduced = FALSE;

    // Dati aggiornati (HP, exp, evoluzioni, strumenti) tornano allo slot originale.
    for (k = 0; k < sSavedPicks; k++)
        sSavedParty[sSavedOrder[k]] = gPlayerParty[k];

    // Mon nuovi nati in lotta (es. Shedinja): slot libero o PC.
    for (k = sSavedPicks; k < PARTY_SIZE; k++)
    {
        u32 j;
        if (GetMonData(&gPlayerParty[k], MON_DATA_SPECIES) == SPECIES_NONE)
            continue;
        for (j = 0; j < PARTY_SIZE; j++)
        {
            if (GetMonData(&sSavedParty[j], MON_DATA_SPECIES) == SPECIES_NONE)
            {
                sSavedParty[j] = gPlayerParty[k];
                break;
            }
        }
        if (j == PARTY_SIZE)
            CopyMonToPC(&gPlayerParty[k]);
    }

    for (k = 0; k < PARTY_SIZE; k++)
        gPlayerParty[k] = sSavedParty[k];
    CompactPartySlots();
    CalculatePlayerPartyCount();
}

// Chiamata alla fine di ogni lotta contro un allenatore.
void TeamPreview_RestorePlayerParty(void)
{
    sEnemyPreviewValid = FALSE; // la lotta è finita
    sMegaDesignate = SPECIES_NONE;
    RestoreReducedParty();

    // Cura completa dopo una vittoria (dopo una sconfitta ci pensa già il ritorno al Centro).
    if (CUSTOM_HEAL_AFTER_BATTLE && !(gBattleTypeFlags & BATTLE_TYPE_FRONTIER)
        && (gBattleOutcome == B_OUTCOME_WON || IS_VGC_TRAINER(TRAINER_BATTLE_PARAM.opponentA)))
        HealPlayerParty();
}
