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
};

static EWRAM_DATA struct TeamPreviewState sTP = {0};
static EWRAM_DATA struct Pokemon sSavedParty[PARTY_SIZE] = {0};
static EWRAM_DATA u8 sSavedOrder[PARTY_SIZE] = {0};   // nuovo slot -> slot originale
static EWRAM_DATA u8 sSavedPicks = 0;
static EWRAM_DATA bool8 sPartyReduced = FALSE;
static EWRAM_DATA u16 *sTilemaps[2] = {NULL};

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
static const u8 sText_Header[]   = _("SCEGLI {STR_VAR_1}   ");
static const u8 sText_Hint[]     = _("A:menu B:togli START:via");
static const u8 sText_Enemy[]    = _("AVVERSARIO");
static const u8 sText_Info[]     = _("Info");
static const u8 sText_Pick[]     = _("Scegli");
static const u8 sText_Unpick[]   = _("Togli");
static const u8 sText_Cancel[]   = _("Annulla");
static const u8 sText_Fight[]    = _("Lotta!");
static const u8 sText_Back[]     = _("Indietro");
static const u8 sText_Lv[]       = _("Lv.");
static const u8 sText_KO[]       = _("KO");
static const u8 sText_Egg[]      = _("UOVO");
static const u8 sText_Cursor[]   = _("{RIGHT_ARROW}");
static const u8 sText_Iper[]     = _("{UP_ARROW}{UP_ARROW}");
static const u8 sText_Super[]    = _("{UP_ARROW}");
static const u8 sText_Resist[]   = _("{DOWN_ARROW}");
static const u8 sText_Resist4[]  = _("{DOWN_ARROW}{DOWN_ARROW}");
static const u8 sText_Immune[]   = _("X");
static const u8 sText_Slash[]    = _("/");

// ---------------------------------------------------------------------
// Grafica
// ---------------------------------------------------------------------
// 0 sfondo, 1 bianco, 2 testo, 3 ombra, 4 pannello noi, 5 pannello loro,
// 6 evidenziato, 7 rosso, 8 blu, 9 arancio, 10 grigio scuro, 11 grigio chiaro, 12 verde
static const u16 sPreviewPal[16] =
{
    RGB(4, 6, 10),  RGB(31, 31, 31), RGB(6, 6, 8),   RGB(18, 18, 20),
    RGB(20, 26, 31), RGB(31, 22, 22), RGB(31, 29, 14), RGB(28, 4, 4),
    RGB(4, 10, 28), RGB(30, 16, 2),  RGB(12, 12, 14), RGB(24, 24, 26),
    RGB(8, 20, 8),  RGB(0, 0, 0),    RGB(0, 0, 0),    RGB(0, 0, 0),
};

static const u8 sColHeader[]  = {0, 1, 10};
static const u8 sColPlayer[]  = {4, 2, 3};
static const u8 sColEnemy[]   = {5, 2, 3};
static const u8 sColHilite[]  = {6, 2, 3};
static const u8 sColGrey4[]   = {4, 10, 11};
static const u8 sColNumber[]  = {7, 1, 7};
static const u8 sColMenu[]    = {TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY};

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
    [WIN_MENU]   = { .bg = 1, .tilemapLeft = 16, .tilemapTop = 7, .width = 8,  .height = 6,  .paletteNum = MENU_PAL, .baseBlock = 601 },
    DUMMY_WIN_TEMPLATE,
};

// ---------------------------------------------------------------------
// Utilità mon
// ---------------------------------------------------------------------
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
    switch (GetMonAbility(def))
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
    u32 a = GetMonData(atk, physical ? MON_DATA_ATK : MON_DATA_SPATK);
    u32 d = GetMonData(def, physical ? MON_DATA_DEF : MON_DATA_SPDEF);
    u32 hp = GetMonData(def, MON_DATA_MAX_HP);
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
    bool32 physical = GetMonData(atk, MON_DATA_ATK) >= GetMonData(atk, MON_DATA_SPATK);
    u32 a = CalcPct(atk, def, 80, MonType(atk, 0), physical);
    u32 b = CalcPct(atk, def, 80, MonType(atk, 1), physical);
    return max(a, b);
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

static s32 PairSynergy(struct Pokemon *a, struct Pokemon *b)
{
    s32 s = 0;
    u32 t;

    // Trick Room + partner lento
    if (MonKnowsEffect(a, EFFECT_TRICK_ROOM) && BaseSpeed(b) <= 60) s += 40;
    if (MonKnowsEffect(b, EFFECT_TRICK_ROOM) && BaseSpeed(a) <= 60) s += 40;
    // Trick Room + Tailwind insieme = conflitto
    if ((MonKnowsEffect(a, EFFECT_TRICK_ROOM) && MonKnowsEffect(b, EFFECT_TAILWIND))
     || (MonKnowsEffect(b, EFFECT_TRICK_ROOM) && MonKnowsEffect(a, EFFECT_TAILWIND)))
        s -= 30;
    // Tailwind + attaccante
    if (MonKnowsEffect(a, EFFECT_TAILWIND) && BaseSpeed(b) >= 70) s += 20;
    if (MonKnowsEffect(b, EFFECT_TAILWIND) && BaseSpeed(a) >= 70) s += 20;
    // Meteo / terreno + chi ne beneficia
    if (BenefitsFromWeather(b, GetMonAbility(a))) s += 35;
    if (BenefitsFromWeather(a, GetMonAbility(b))) s += 35;
    // Fake Out / redirect + setup
    if ((HasFakeOut(a) || MonKnowsEffect(a, EFFECT_FOLLOW_ME))
     && (MonKnowsEffect(b, EFFECT_TRICK_ROOM) || MonKnowsEffect(b, EFFECT_TAILWIND)))
        s += 20;
    if ((HasFakeOut(b) || MonKnowsEffect(b, EFFECT_FOLLOW_ME))
     && (MonKnowsEffect(a, EFFECT_TRICK_ROOM) || MonKnowsEffect(a, EFFECT_TAILWIND)))
        s += 20;
    if (HasFakeOut(a) && HasFakeOut(b))
        s -= 10;
    // Debolezze in comune
    for (t = TYPE_NORMAL; t < NUMBER_OF_MON_TYPES; t++)
    {
        if (t == TYPE_MYSTERY || t == TYPE_STELLAR)
            continue;
        if (TypeVsMonPct(t, a) >= 200 && TypeVsMonPct(t, b) >= 200)
            s -= 12;
    }
    return s;
}

static s16 sMatch[PARTY_SIZE][PARTY_SIZE]; // [enemy][player]

// Punteggio 1v1: + buono per il nemico (AI), - buono per il giocatore.
static s32 Matchup(struct Pokemon *e, struct Pokemon *p, bool32 omniscient)
{
    u32 off = BestMovePct(e, p);
    u32 def = omniscient ? BestMovePct(p, e) : max(BestStabPct(p, e), BestMovePct(p, e) / 2);
    u32 hitsE = HitsToKo(off), hitsP = HitsToKo(def);
    bool32 eFaster = GetMonData(e, MON_DATA_SPEED) > GetMonData(p, MON_DATA_SPEED);
    s32 s = (s32)min(off, 150) - (s32)min(def, 150);

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

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (IsMonUsable(&enemy[i])) enemySlots[nEnemy++] = i;
        if (IsMonUsable(&player[i]))
        {
            playerSlots[nPlayer++] = i;
            if (HasFakeOut(&player[i]))
                foeHasFakeOut = TRUE;
        }
    }
    if (picks > nEnemy)
        picks = nEnemy;
    if (nEnemy == 0)
        return;

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
        if (j < 2) // i giocatori spesso tengono i lead in cima
            weight[j] += 25;
    }
    for (i = 0; i < nEnemy; i++)
    {
        depth[i] = 0;
        for (j = 0; j < nPlayer; j++)
            depth[i] += sMatch[i][j];
        depth[i] /= (s32)max(nPlayer, 1);
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
                score += depth[i];
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
        if (score > bestScore)
        {
            bestScore = score;
            bestMask = mask;
        }
    }

    for (i = 0; i < nEnemy; i++)
        if (bestMask & (1u << i))
            chosen[nChosen++] = i;

    // 2) Lead (1 in singola, 2 in doppia).
    {
        u32 leadCount = (isDouble && nChosen >= 2) ? 2 : 1;
        u32 leadA = 0, leadB = 0;
        s32 bestLead = INT32_MIN;
        u32 a, b;

        for (a = 0; a < nChosen; a++)
        {
            for (b = (leadCount == 2 ? a + 1 : a); b < nChosen; b++)
            {
                s32 s = 0;
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
                }
                s += Random() % 16; // un po' di varietà: lead meno leggibile
                if (s > bestLead)
                {
                    bestLead = s;
                    leadA = a;
                    leadB = b;
                }
                if (leadCount == 1)
                    break;
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
    u32 usable = CountUsable(gPlayerParty);

    memset(&sTP, 0, sizeof(sTP));
    sTP.required = min(CUSTOM_TEAM_PREVIEW_PICKS, usable);
    sTP.active = TRUE;
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
                                              14, LIST_TOP_Y + ROW_H * i + 10, 1, GetMonData(p, MON_DATA_PERSONALITY));
            gSprites[sTP.iconIds[0][i]].oam.priority = 1;
        }
        if (GetMonData(e, MON_DATA_SPECIES) != SPECIES_NONE)
        {
            sTP.iconIds[1][i] = CreateMonIcon(GetMonData(e, MON_DATA_SPECIES), SpriteCB_MonIcon,
                                              120 + 14, LIST_TOP_Y + ROW_H * i + 10, 1, GetMonData(e, MON_DATA_PERSONALITY));
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
    FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(0));
    ConvertIntToDecimalStringN(gStringVar1, sTP.required, STR_CONV_MODE_LEFT_ALIGN, 1);
    StringExpandPlaceholders(gStringVar4, sText_Header);
    ptr = gStringVar4 + StringLength(gStringVar4);
    ptr = ConvertIntToDecimalStringN(ptr, sTP.numPicks, STR_CONV_MODE_LEFT_ALIGN, 1);
    ptr = StringCopy(ptr, sText_Slash);
    ConvertIntToDecimalStringN(ptr, sTP.required, STR_CONV_MODE_LEFT_ALIGN, 1);
    AddTextPrinterParameterized3(WIN_HEADER, FONT_SMALL, 4, 2, sColHeader, TEXT_SKIP_DRAW, gStringVar4);
    AddTextPrinterParameterized3(WIN_HEADER, FONT_SMALL, 70, 2, sColHeader, TEXT_SKIP_DRAW, sText_Hint);
    CopyWindowToVram(WIN_HEADER, COPYWIN_FULL);
}

static void DrawMonText(u32 win, struct Pokemon *mon, u32 y, const u8 *colors)
{
    u8 *ptr;
    if (GetMonData(mon, MON_DATA_IS_EGG))
    {
        AddTextPrinterParameterized3(win, FONT_NARROW, 26, y, colors, TEXT_SKIP_DRAW, sText_Egg);
        return;
    }
    GetMonData(mon, MON_DATA_NICKNAME, gStringVar1);
    StringGet_Nickname(gStringVar1);
    AddTextPrinterParameterized3(win, FONT_NARROW, 26, y, colors, TEXT_SKIP_DRAW, gStringVar1);
    ptr = StringCopy(gStringVar2, sText_Lv);
    ConvertIntToDecimalStringN(ptr, GetMonData(mon, MON_DATA_LEVEL), STR_CONV_MODE_LEFT_ALIGN, 3);
    AddTextPrinterParameterized3(win, FONT_SMALL, 26, y + 12, colors, TEXT_SKIP_DRAW, gStringVar2);
}

static void DrawPlayerPanel(void)
{
    u32 i;
    FillWindowPixelBuffer(WIN_PLAYER, PIXEL_FILL(4));
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gPlayerParty[i];
        u32 y = ROW_H * i;
        const u8 *col = sColPlayer;

        if (GetMonData(mon, MON_DATA_SPECIES) == SPECIES_NONE)
            continue;
        if (i == sTP.cursor)
        {
            FillWindowPixelRect(WIN_PLAYER, PIXEL_FILL(6), 0, y, 120, ROW_H);
            col = sColHilite;
        }
        if (!IsMonUsable(mon))
            col = sColGrey4;
        DrawMonText(WIN_PLAYER, mon, y, col);
        if (!GetMonData(mon, MON_DATA_IS_EGG) && GetMonData(mon, MON_DATA_HP) == 0)
            AddTextPrinterParameterized3(WIN_PLAYER, FONT_SMALL, 70, y + 12, col, TEXT_SKIP_DRAW, sText_KO);
        if (sTP.pickOrder[i])
        {
            FillWindowPixelRect(WIN_PLAYER, PIXEL_FILL(7), 102, y + 4, 14, 16);
            ConvertIntToDecimalStringN(gStringVar3, sTP.pickOrder[i], STR_CONV_MODE_LEFT_ALIGN, 1);
            AddTextPrinterParameterized3(WIN_PLAYER, FONT_NORMAL, 106, y + 4, sColNumber, TEXT_SKIP_DRAW, gStringVar3);
        }
    }
    CopyWindowToVram(WIN_PLAYER, COPYWIN_FULL);
}

static void DrawEnemyPanel(void)
{
    u32 i;
    struct Pokemon *hover = &gPlayerParty[sTP.cursor];
    bool32 showEff = IsMonUsable(hover);

    FillWindowPixelBuffer(WIN_ENEMY, PIXEL_FILL(5));
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gEnemyParty[i];
        u32 y = ROW_H * i;

        if (GetMonData(mon, MON_DATA_SPECIES) == SPECIES_NONE)
            continue;
        DrawMonText(WIN_ENEMY, mon, y, sColEnemy);

        if (showEff)
        {
            u32 eff = PreviewEffectiveness(hover, mon);
            const u8 *txt = NULL;
            u8 col[3] = {5, 2, 3};
            if (eff == 0)        { txt = sText_Immune;  col[1] = 2;  }
            else if (eff >= 400) { txt = sText_Iper;    col[1] = 7;  }
            else if (eff >= 200) { txt = sText_Super;   col[1] = 9;  }
            else if (eff <= 25)  { txt = sText_Resist4; col[1] = 8;  }
            else if (eff < 100)  { txt = sText_Resist;  col[1] = 8;  }
            if (txt != NULL)
                AddTextPrinterParameterized3(WIN_ENEMY, FONT_NORMAL, 100, y + 4, col, TEXT_SKIP_DRAW, txt);
        }
        if (CUSTOM_TEAM_PREVIEW_REVEAL_AI)
        {
            u32 k;
            for (k = 0; k < sTP.enemyPicks; k++)
            {
                if (sTP.enemyOrder[k] == i)
                {
                    ConvertIntToDecimalStringN(gStringVar3, k + 1, STR_CONV_MODE_LEFT_ALIGN, 1);
                    AddTextPrinterParameterized3(WIN_ENEMY, FONT_SMALL, 70, y + 12, sColEnemy, TEXT_SKIP_DRAW, gStringVar3);
                }
            }
        }
    }
    AddTextPrinterParameterized3(WIN_ENEMY, FONT_SMALL, 70, 136, sColEnemy, TEXT_SKIP_DRAW, sText_Enemy);
    CopyWindowToVram(WIN_ENEMY, COPYWIN_FULL);
}

static void DrawAll(void)
{
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
    else
    {
        items[0] = sText_Fight;
        items[1] = sText_Back;
    }
    FillWindowPixelBuffer(WIN_MENU, PIXEL_FILL(1));
    for (i = 0; i < n; i++)
    {
        if (i == sTP.menuCursor)
            AddTextPrinterParameterized3(WIN_MENU, FONT_NORMAL, 0, 16 * i, sColMenu, TEXT_SKIP_DRAW, sText_Cursor);
        AddTextPrinterParameterized3(WIN_MENU, FONT_NORMAL, 10, 16 * i, sColMenu, TEXT_SKIP_DRAW, items[i]);
    }
    PutWindowTilemap(WIN_MENU);
    DrawStdFrameWithCustomTileAndPalette(WIN_MENU, FALSE, FRAME_TILE, FRAME_PAL);
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
        PlaySE(SE_SELECT);
        CloseMenu();
    }
    else if (JOY_NEW(A_BUTTON))
    {
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
    ShowPokemonSummaryScreen(SUMMARY_MODE_NORMAL, gPlayerParty, sTP.cursor, gPlayerPartyCount - 1, CB2_ReturnFromSummary);
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
                tmp[i] = gEnemyParty[i];
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

void TeamPreview_RestorePlayerParty(void)
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
